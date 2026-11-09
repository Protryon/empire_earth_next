//! The algorithms of WON's async ops (`src/Empire Earth/WONAPI/WONCommon/AsyncOp.cpp`,
//! `WONSocket/SocketOps.cpp`, `RecvCRMsgOp.cpp`): an op's time left, the message length prefix, the receive chunk,
//! and the loops that send a buffer and receive a CR-LF line on a [`Socket`]. The C++ ops keep their objects, their
//! state and the virtual run / continue / finish steps, and call these.

use crate::socket::{self, Socket, WAIT_READ, WAIT_WRITE};
use crate::status::*;
use std::ffi::c_void;

/// `OP_MODE_ASYNC`.
/// cbindgen:ignore
pub const MODE_ASYNC: i32 = 1;
/// `OP_MODE_BLOCK`.
/// cbindgen:ignore
pub const MODE_BLOCK: i32 = 2;

/// WON's millisecond tick (`WONTickCount`): the platform clock cut to 32 bits.
pub fn tick() -> u32 {
    ee_plat_core::time::PlatTimeMilliseconds64() as u32
}

/// An op's run timing (`AsyncOp`'s `mRunMode`, `mStartTime`, `mTimeout`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct OpTiming {
    pub run_mode: i32,
    pub start: u32,
    pub timeout: u32,
}

impl OpTiming {
    /// `AsyncOp::TimeLeft` at `now`: 0 for an async run (it waits on nothing) or once the timeout passed (a wrapping
    /// 32-bit elapsed time).
    pub fn time_left(&self, now: u32) -> u32 {
        if self.run_mode == MODE_ASYNC {
            return 0;
        }
        real_time_left(self.start, self.timeout, now)
    }
}

/// `AsyncOp::RealTimeLeft` and `AsyncSocket::TimeLeft`: the time left of `timeout` since `start`, whatever the
/// mode (the original compared the signed elapsed time as unsigned).
pub fn real_time_left(start: u32, timeout: u32, now: u32) -> u32 {
    timeout.saturating_sub(now.wrapping_sub(start))
}

/// What a received length prefix says (`RecvMsgOp::StartRecvBody`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LengthStep {
    /// A 2-byte 0xffff: a 4-byte prefix follows.
    Widen,
    /// The body's length (the message length less the prefix).
    Body(u32),
    /// `WS_RecvMsg_InvalidMessageLength`: not above the prefix's size, 256000 or more, or a prefix of another size.
    Invalid,
}

/// The length a prefix of 1, 2 or 4 little-endian bytes gives; the length counts the prefix.
pub fn length_step(prefix: &[u8]) -> LengthStep {
    let length = match prefix.len() {
        1 => prefix[0] as u32,
        2 => u16::from_le_bytes([prefix[0], prefix[1]]) as u32,
        4 => u32::from_le_bytes([prefix[0], prefix[1], prefix[2], prefix[3]]),
        _ => return LengthStep::Invalid,
    };
    let field = prefix.len() as u32;
    if length > field && length < 256_000 {
        if length == 65535 && field == 2 {
            return LengthStep::Widen;
        }
        return LengthStep::Body(length - field);
    }
    LengthStep::Invalid
}

/// `RecvMsgOp::Start`'s test of the socket's length field size.
pub fn valid_length_field(size: u32) -> bool {
    matches!(size, 1 | 2 | 4)
}

/// `RecvBytesOp::ContinueRecv`'s read size into its own 1024-byte buffer (a file, skipped bytes, or no length): 1024,
/// or what is left of the length, never negative.
pub fn chunk_length(length: u32, bytes_read: u32) -> i32 {
    if length == 0 {
        return 1024;
    }
    let left = length.wrapping_sub(bytes_read) as i32;
    left.clamp(0, 1024)
}

/// The socket an op works on: its core and the C++ lock (`AsyncSocket::mDataCrit`'s platform mutex), taken around
/// each call on the socket as the C++ `AsyncSocket` methods take it.
pub struct LockedSocket {
    pub core: *mut Socket,
    pub lock: *mut c_void,
}

impl LockedSocket {
    fn with<R>(&self, f: impl FnOnce(&mut Socket) -> R) -> R {
        let lock = self.lock.cast();
        // SAFETY: the C++ AsyncSocket's mutex and core, alive while the op holds the socket (the op's SmartPtr);
        // the mutex is recursive, so a caller already holding it is fine
        unsafe {
            ee_plat_sys::thread::imp::PlatMutexLock(lock);
            let r = f(&mut *self.core);
            ee_plat_sys::thread::imp::PlatMutexUnlock(lock);
            r
        }
    }

    /// `WaitForRead` / `WaitForWrite`: the handle read under the lock, the wait without it.
    fn wait(&self, mask: u32, timeout: u32) {
        if timeout == 0 {
            return;
        }
        let handle = self.with(|s| s.handle);
        socket::wait(handle, mask, timeout);
    }
}

/// `SendBytesOp::Continue`: sends `data[*pos..]` as the socket takes it, waiting for room for the op's time left;
/// `*pos` follows what went. `WS_Success` when all went, `WS_TimedOut` when the time ran out (or, for an async op,
/// at once when the socket is full: the socket thread continues it), else the send's error.
pub fn send_all(socket: &LockedSocket, data: &[u8], pos: &mut u32, timing: OpTiming) -> Status {
    while (*pos as usize) < data.len() {
        socket.wait(WAIT_WRITE, timing.time_left(tick()));
        let mut sent = 0;
        let status = socket.with(|s| s.send(&data[*pos as usize..], &mut sent));
        if sent > 0 {
            *pos += sent as u32;
        }
        if status != WS_TIMED_OUT {
            return status;
        }
        if timing.time_left(tick()) == 0 {
            return WS_TIMED_OUT;
        }
    }
    WS_SUCCESS
}

/// `RecvCRMsgOp::Continue`: reads a byte at a time, each one given to `append`, until a CR-LF ends the line.
/// `*last` is the byte before (the line so far's last byte, -1 for none), kept across calls. `WS_Success` at the
/// LF, `WS_TimedOut` when the time ran out, else the receive's error.
pub fn recv_line(socket: &LockedSocket, last: &mut i32, timing: OpTiming, mut append: impl FnMut(u8)) -> Status {
    loop {
        socket.wait(WAIT_READ, timing.time_left(tick()));
        let mut byte = [0u8; 1];
        let mut n = 0;
        let status = socket.with(|s| s.recv(&mut byte, &mut n));
        if status == WS_SUCCESS {
            append(byte[0]);
            let before = *last;
            *last = byte[0] as i32;
            if byte[0] == 10 && before == 13 {
                return WS_SUCCESS;
            }
        } else if status == WS_TIMED_OUT {
            if timing.time_left(tick()) == 0 {
                return WS_TIMED_OUT;
            }
        } else {
            return status;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ipaddr::Addr;
    use crate::socket::{Socket, KIND_STREAM};
    use ee_plat_sys::thread::imp::{PlatMutexCreate, PlatMutexDestroy};

    #[test]
    fn time_left() {
        let t = OpTiming { run_mode: MODE_BLOCK, start: 1000, timeout: 500 };
        assert_eq!(t.time_left(1000), 500);
        assert_eq!(t.time_left(1499), 1);
        assert_eq!(t.time_left(1500), 0);
        assert_eq!(t.time_left(900), 0); // "elapsed" wrapped past the timeout
        assert_eq!(OpTiming { run_mode: MODE_ASYNC, ..t }.time_left(1000), 0);
        // the tick wraps
        let w = OpTiming { run_mode: MODE_BLOCK, start: u32::MAX - 10, timeout: 100 };
        assert_eq!(w.time_left(20), 69);
        assert_eq!(real_time_left(0, u32::MAX, 5), u32::MAX - 5);
        assert_eq!(real_time_left(0, 0, 0), 0);
    }

    #[test]
    fn length_prefixes() {
        assert_eq!(length_step(&[9, 0, 0, 0]), LengthStep::Body(5));
        assert_eq!(length_step(&[5, 0]), LengthStep::Body(3));
        assert_eq!(length_step(&[3]), LengthStep::Body(2));
        assert_eq!(length_step(&[0xff, 0xff]), LengthStep::Widen);
        assert_eq!(length_step(&[0xff, 0xff, 0, 0]), LengthStep::Body(65531));
        assert_eq!(length_step(&[4, 0, 0, 0]), LengthStep::Invalid);
        assert_eq!(length_step(&[1]), LengthStep::Invalid);
        assert_eq!(length_step(&[0x00, 0xe8, 0x03, 0]), LengthStep::Invalid); // 256000
        assert_eq!(length_step(&[0xff, 0xe7, 0x03, 0]), LengthStep::Body(255_995));
        assert_eq!(length_step(&[0xff, 0xff, 0xff, 0xff]), LengthStep::Invalid);
        assert_eq!(length_step(&[1, 2, 3]), LengthStep::Invalid);
        assert_eq!(length_step(&[]), LengthStep::Invalid);
        assert!(valid_length_field(1) && valid_length_field(2) && valid_length_field(4));
        assert!(!valid_length_field(0) && !valid_length_field(3) && !valid_length_field(8));
    }

    #[test]
    fn chunks() {
        assert_eq!(chunk_length(0, 5000), 1024);
        assert_eq!(chunk_length(5000, 0), 1024);
        assert_eq!(chunk_length(5000, 4500), 500);
        assert_eq!(chunk_length(5000, 5000), 0);
        assert_eq!(chunk_length(5000, 6000), 0);
    }

    struct Pair {
        lock: *mut ee_plat_sys::thread::imp::PlatMutex,
        client: Box<Socket>,
        server: Box<Socket>,
    }

    fn pair() -> Pair {
        socket::start();
        let mut listener = Socket::new(KIND_STREAM);
        assert_eq!(listener.open_handle(), WS_SUCCESS);
        assert_eq!(listener.bind(Addr::from_host(0x7f00_0001, 0), true), WS_SUCCESS);
        let port = listener.local_port();
        assert_eq!(listener.listen(1), WS_SUCCESS);
        let mut client = Box::new(Socket::new(KIND_STREAM));
        assert_eq!(client.open_handle(), WS_SUCCESS);
        client.connect(Addr::from_host(0x7f00_0001, port));
        assert_eq!(socket::wait_for_connect(client.handle, 2000), WS_SUCCESS);
        socket::wait(listener.handle, WAIT_READ, 2000);
        let (h, _) = listener.accept().unwrap();
        let mut server = Box::new(Socket::new(KIND_STREAM));
        server.adopt(h);
        listener.close_handle();
        // SAFETY: plain call
        Pair { lock: unsafe { PlatMutexCreate() }, client, server }
    }

    #[test]
    fn send_all_and_lines() {
        let mut p = pair();
        let blocking = |timeout| OpTiming { run_mode: MODE_BLOCK, start: tick(), timeout };
        let client = LockedSocket { core: &mut *p.client, lock: p.lock.cast() };
        let server = LockedSocket { core: &mut *p.server, lock: p.lock.cast() };
        let mut pos = 0;
        assert_eq!(send_all(&client, b"a\nb\r\nrest\r\n", &mut pos, blocking(1000)), WS_SUCCESS);
        assert_eq!(pos, 11);
        let mut line = Vec::new();
        let mut last = -1;
        assert_eq!(recv_line(&server, &mut last, blocking(1000), |b| line.push(b)), WS_SUCCESS);
        assert_eq!(line, b"a\nb\r\n");
        // a CR-LF split between calls still ends the line: the last byte carries over
        line.clear();
        last = 13;
        let mut tail = Vec::new();
        assert_eq!(recv_line(&server, &mut last, blocking(1000), |b| tail.push(b)), WS_SUCCESS);
        assert_eq!(tail, b"rest\r\n");
        // nothing more: the time runs out
        let t0 = std::time::Instant::now();
        assert_eq!(recv_line(&server, &mut last, blocking(120), |b| line.push(b)), WS_TIMED_OUT);
        assert!(t0.elapsed().as_millis() >= 100);
        // an async op does not wait: would-block at once
        assert_eq!(
            recv_line(&server, &mut last, OpTiming { run_mode: MODE_ASYNC, start: tick(), timeout: 5000 }, |_| ()),
            WS_TIMED_OUT
        );

        // a large buffer to a reader thread
        let data: Vec<u8> = (0..3_000_000u32).map(|i| (i * 7 + (i >> 11)) as u8).collect();
        struct Send(*mut Socket, *mut c_void);
        // SAFETY: the reader alone uses the server core, under the shared lock
        unsafe impl std::marker::Send for Send {}
        let s = Send(&mut *p.server, p.lock.cast());
        let n = data.len();
        let reader = std::thread::spawn(move || {
            let s = s;
            let server = LockedSocket { core: s.0, lock: s.1 };
            let mut got = Vec::with_capacity(n);
            let mut buffer = vec![0u8; 65536];
            let end = std::time::Instant::now() + std::time::Duration::from_secs(10);
            while got.len() < n && std::time::Instant::now() < end {
                server.wait(WAIT_READ, 100);
                let mut k = 0;
                let st = server.with(|x| x.recv(&mut buffer, &mut k));
                if st == WS_SUCCESS || st == WS_TIMED_OUT {
                    got.extend_from_slice(&buffer[..k.max(0) as usize]);
                } else {
                    break;
                }
            }
            got
        });
        let mut pos = 0;
        assert_eq!(send_all(&client, &data, &mut pos, blocking(10000)), WS_SUCCESS);
        assert_eq!(pos as usize, data.len());
        assert_eq!(reader.join().unwrap(), data);

        // a closed peer ends a line read with Shutdown
        // SAFETY: the reader is done
        unsafe { (*client.core).close_handle() };
        assert_eq!(recv_line(&server, &mut last, blocking(1000), |_| ()), WS_ASYNC_SOCKET_SHUTDOWN);
        let mut pos = 0;
        assert_eq!(send_all(&client, b"x", &mut pos, blocking(100)), WS_ASYNC_SOCKET_INVALID_SOCKET);
        p.server.close_handle();
        // SAFETY: created above
        unsafe { PlatMutexDestroy(p.lock) };
    }
}
