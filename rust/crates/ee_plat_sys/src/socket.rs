//! PlatSocket (`include/platform/PlatSocket.h`): IPv4 sockets, the subset of Winsock the TCP protocol plug-in and the
//! WON library use, with errors in Winsock's numbering on every system.

#[cfg(unix)]
#[path = "socket_posix.rs"]
pub mod imp;
#[cfg(windows)]
#[path = "socket_win32.rs"]
pub mod imp;

#[cfg(test)]
mod tests {
    use super::imp::*;
    use crate::constants::*;
    use crate::test_util::{c, c_text};
    use crate::thread::imp::{PlatEventCreate, PlatEventDestroy, PlatEventReset, PlatEventWait};
    use std::ffi::c_void;
    use std::ptr::null_mut;
    use std::sync::Once;
    use std::time::{Duration, Instant};

    static STARTUP: Once = Once::new();

    /// Started once; the guard keeps the socket tests one at a time (startup_is_counted_with_cleanup's
    /// cleanup must not run under another socket test).
    fn startup() -> std::sync::MutexGuard<'static, ()> {
        static SERIAL: std::sync::Mutex<()> = std::sync::Mutex::new(());
        let guard = SERIAL.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
        STARTUP.call_once(|| unsafe {
            assert_eq!(PlatSocketStartup(), 1);
        });
        guard
    }

    fn loopback() -> PlatSockAddrIn {
        PlatSockAddrIn { mFamily: PLAT_AF_INET, mPort: 0, mAddress: 0x7F00_0001u32.to_be(), mZero: [0; 8] }
    }

    unsafe fn bound_listener() -> (PlatSocket, PlatSockAddrIn) {
        let listener = PlatSocketOpen(PLAT_SOCKET_STREAM);
        assert_ne!(listener, PLAT_SOCKET_INVALID);
        let any = loopback();
        assert_eq!(PlatSocketBind(listener, &any), 1);
        assert_eq!(PlatSocketListen(listener, 4), 1);
        let mut bound = PlatSockAddrIn::default();
        assert_eq!(PlatSocketGetName(listener, &mut bound), 1);
        assert_ne!(bound.mPort, 0);
        assert_eq!(bound.mFamily, PLAT_AF_INET);
        (listener, bound)
    }

    #[test]
    fn parse_and_format_ipv4() {
        let _serial = startup();
        unsafe {
            assert_eq!(PlatSocketParseIPv4(c("127.0.0.1").as_ptr()), 0x7F00_0001u32.to_be());
            assert_eq!(PlatSocketParseIPv4(c("not an address").as_ptr()), PLAT_INADDR_NONE);
            let mut text = [0u8; 16];
            PlatSocketFormatIPv4(0xC0A8_0102u32.to_be(), text.as_mut_ptr().cast());
            assert_eq!(c_text(&text), "192.168.1.2");
            PlatSocketFormatIPv4(0xFFFF_FFFFu32, text.as_mut_ptr().cast());
            assert_eq!(c_text(&text), "255.255.255.255");
        }
    }

    #[test]
    fn host_name_and_resolve() {
        let _serial = startup();
        unsafe {
            let mut name = [0u8; 256];
            assert_eq!(PlatSocketGetHostName(name.as_mut_ptr().cast(), 256), 1);
            assert_ne!(name[0], 0);
            let mut addresses = [0u32; 8];
            assert!(PlatSocketResolveIPv4(c("127.0.0.1").as_ptr(), addresses.as_mut_ptr(), 8) >= 1);
            assert_eq!(addresses[0], 0x7F00_0001u32.to_be());
            let mut adapters = [PlatNetAdapter { mDescription: [0; 132], mAddress: 0, mNetmask: 0 }; 4];
            let n = PlatSocketGetAdapters(adapters.as_mut_ptr(), 4);
            assert!(n <= 4);
        }
    }

    #[test]
    fn stream_exchange_with_readiness_events() {
        let _serial = startup();
        unsafe {
            let (listen, bound) = bound_listener();
            let accept_event = PlatEventCreate(1, 0);
            assert_eq!(PlatSocketSelectEvent(listen, accept_event.cast(), PLAT_SOCKET_EVENT_ACCEPT), 1);
            let client = PlatSocketOpen(PLAT_SOCKET_STREAM);
            assert_eq!(PlatSocketConnect(client, &bound), 1);
            assert_eq!(PlatEventWait(accept_event, 5000), 0);
            let mut peer = PlatSockAddrIn::default();
            let server = PlatSocketAccept(listen, &mut peer);
            assert_ne!(server, PLAT_SOCKET_INVALID);
            assert_eq!(peer.mAddress, bound.mAddress);
            let data_event = PlatEventCreate(1, 0);
            assert_eq!(PlatSocketSelectEvent(server, data_event.cast(), PLAT_SOCKET_EVENT_READ), 1);
            let mut buffer = [0u8; 64];
            assert_eq!(PlatSocketRecv(server, buffer.as_mut_ptr().cast(), 64), -1);
            assert_eq!(PlatSocketLastError(), PLAT_SOCK_EWOULDBLOCK); // non-blocking now
            assert_eq!(PlatEventWait(data_event, 0), PLAT_WAIT_TIMEOUT);
            assert_eq!(PlatSocketSend(client, b"ping!".as_ptr().cast(), 5), 5);
            assert_eq!(PlatEventWait(data_event, 5000), 0);
            PlatEventReset(data_event);
            assert_eq!(PlatSocketRecv(server, buffer.as_mut_ptr().cast(), 2), 2);
            assert_eq!(&buffer[..2], b"pi");
            assert_eq!(PlatEventWait(data_event, 5000), 0); // signalled again: data is left
            PlatEventReset(data_event);
            assert_eq!(PlatSocketRecv(server, buffer.as_mut_ptr().cast(), 64), 3);
            assert_eq!(&buffer[..3], b"ng!");
            assert_eq!(PlatEventWait(data_event, 100), PLAT_WAIT_TIMEOUT); // drained
            assert_eq!(PlatSocketSend(server, b"pong".as_ptr().cast(), 4), 4);
            assert_eq!(PlatSocketRecv(client, buffer.as_mut_ptr().cast(), 64), 4);
            assert_eq!(&buffer[..4], b"pong");
            assert_eq!(PlatSocketClose(client), 1);
            let mut end = -1;
            for _ in 0..400 {
                end = PlatSocketRecv(server, buffer.as_mut_ptr().cast(), 64);
                if end >= 0 {
                    break;
                }
                std::thread::sleep(Duration::from_millis(5));
            }
            assert_eq!(end, 0, "the peer's close reads as 0 bytes");
            assert_eq!(PlatSocketSelectEvent(server, null_mut(), 0), 1);
            assert_eq!(PlatSocketClose(server), 1);
            assert_eq!(PlatSocketClose(listen), 1);
            PlatEventDestroy(accept_event);
            PlatEventDestroy(data_event);
        }
    }

    #[test]
    fn datagrams_and_blocking_mode() {
        let _serial = startup();
        unsafe {
            let receiver = PlatSocketOpen(PLAT_SOCKET_DGRAM);
            let sender = PlatSocketOpen(PLAT_SOCKET_DGRAM);
            assert_ne!(receiver, PLAT_SOCKET_INVALID);
            let any = loopback();
            assert_eq!(PlatSocketBind(receiver, &any), 1);
            let mut bound = PlatSockAddrIn::default();
            assert_eq!(PlatSocketGetName(receiver, &mut bound), 1);
            assert_eq!(PlatSocketSetNonBlocking(receiver, 1), 1);
            let mut buffer = [0u8; 64];
            let mut from = PlatSockAddrIn::default();
            assert_eq!(PlatSocketRecvFrom(receiver, buffer.as_mut_ptr().cast(), 64, &mut from), -1);
            assert_eq!(PlatSocketLastError(), PLAT_SOCK_EWOULDBLOCK);
            assert_eq!(PlatSocketSetBroadcast(sender, 1), 1);
            assert_eq!(PlatSocketSendTo(sender, b"datagram".as_ptr().cast(), 8, &bound), 8);
            let mut got = -1;
            for _ in 0..400 {
                got = PlatSocketRecvFrom(receiver, buffer.as_mut_ptr().cast(), 64, &mut from);
                if got >= 0 {
                    break;
                }
                std::thread::sleep(Duration::from_millis(5));
            }
            assert_eq!(got, 8);
            assert_eq!(&buffer[..8], b"datagram");
            assert_eq!(from.mAddress, bound.mAddress);
            assert_eq!(from.mFamily, PLAT_AF_INET);
            PlatSocketClose(receiver);
            PlatSocketClose(sender);
        }
    }

    #[test]
    fn refused_connection_reports_econnrefused() {
        let _serial = startup();
        unsafe {
            let (listen, bound) = bound_listener();
            PlatSocketClose(listen);
            let refused = PlatSocketOpen(PLAT_SOCKET_STREAM);
            assert_eq!(PlatSocketConnect(refused, &bound), 0);
            assert_eq!(PlatSocketLastError(), PLAT_SOCK_ECONNREFUSED);
            PlatSocketClose(refused);
        }
    }

    #[test]
    fn wait_timeout_shutdown_and_options() {
        let _serial = startup();
        unsafe {
            let listen = PlatSocketOpen(PLAT_SOCKET_STREAM);
            assert_eq!(PlatSocketSetReuseAddress(listen, 1), 1);
            let any = loopback();
            assert_eq!(PlatSocketBind(listen, &any), 1);
            assert_eq!(PlatSocketListen(listen, 4), 1);
            let mut bound = PlatSockAddrIn::default();
            assert_eq!(PlatSocketGetName(listen, &mut bound), 1);
            assert_eq!(PlatSocketWait(listen, PLAT_SOCKET_WAIT_READ, 0), 0);
            let client = PlatSocketOpen(PLAT_SOCKET_STREAM);
            assert_eq!(PlatSocketSetNoDelay(client, 1), 1);
            assert_eq!(PlatSocketSetLinger(client, 1, 2), 1);
            assert_eq!(PlatSocketSetNonBlocking(client, 1), 1);
            let connected = PlatSocketConnect(client, &bound);
            assert!(connected == 1 || PlatSocketLastError() == PLAT_SOCK_EWOULDBLOCK);
            let mask = PLAT_SOCKET_WAIT_WRITE | PLAT_SOCKET_WAIT_EXCEPT;
            assert_eq!(PlatSocketWait(client, mask, 5000), PLAT_SOCKET_WAIT_WRITE as i32);
            assert_eq!(PlatSocketGetError(client), 0);
            assert_eq!(PlatSocketWait(listen, PLAT_SOCKET_WAIT_READ, 5000), PLAT_SOCKET_WAIT_READ as i32);
            let server = PlatSocketAccept(listen, null_mut());
            assert_ne!(server, PLAT_SOCKET_INVALID);
            let t0 = Instant::now();
            assert_eq!(PlatSocketWait(server, PLAT_SOCKET_WAIT_READ, 60), 0);
            assert!(t0.elapsed() >= Duration::from_millis(40));
            assert_eq!(PlatSocketSend(client, b"abc".as_ptr().cast(), 3), 3);
            let both = PLAT_SOCKET_WAIT_READ | PLAT_SOCKET_WAIT_WRITE;
            assert_eq!(PlatSocketWait(server, both, 5000), both as i32);
            let mut buffer = [0u8; 16];
            assert_eq!(PlatSocketPeek(server, buffer.as_mut_ptr().cast(), 16), 3);
            assert_eq!(PlatSocketRecv(server, buffer.as_mut_ptr().cast(), 16), 3); // the peek left it queued
            assert_eq!(PlatSocketShutdown(client, PLAT_SOCKET_SHUT_SEND), 1);
            assert_ne!(PlatSocketWait(server, PLAT_SOCKET_WAIT_READ, 5000) & PLAT_SOCKET_WAIT_READ as i32, 0);
            assert_eq!(PlatSocketRecv(server, buffer.as_mut_ptr().cast(), 16), 0);
            assert_eq!(PlatSocketSend(server, b"back".as_ptr().cast(), 4), 4); // the other direction still open
            assert_ne!(PlatSocketWait(client, PLAT_SOCKET_WAIT_READ, 5000) & PLAT_SOCKET_WAIT_READ as i32, 0);
            assert_eq!(PlatSocketRecv(client, buffer.as_mut_ptr().cast(), 16), 4);
            assert_eq!(PlatSocketShutdown(server, PLAT_SOCKET_SHUT_BOTH), 1);
            PlatSocketClose(client);
            PlatSocketClose(server);
            assert_eq!(PlatSocketClose(listen), 1);
        }
    }

    #[test]
    fn refused_nonblocking_connect_is_except_with_the_reason_pending() {
        let _serial = startup();
        unsafe {
            let (listen, bound) = bound_listener();
            PlatSocketClose(listen);
            let refused = PlatSocketOpen(PLAT_SOCKET_STREAM);
            assert_eq!(PlatSocketSetNonBlocking(refused, 1), 1);
            assert_eq!(PlatSocketConnect(refused, &bound), 0);
            if PlatSocketLastError() == PLAT_SOCK_EWOULDBLOCK {
                let ready = PlatSocketWait(refused, PLAT_SOCKET_WAIT_WRITE | PLAT_SOCKET_WAIT_EXCEPT, 10_000);
                assert_eq!(ready, PLAT_SOCKET_WAIT_EXCEPT as i32);
                assert_eq!(PlatSocketGetError(refused), PLAT_SOCK_ECONNREFUSED);
            } else {
                assert_eq!(PlatSocketLastError(), PLAT_SOCK_ECONNREFUSED);
            }
            PlatSocketClose(refused);
        }
    }

    #[test]
    fn a_blocking_wait_returns_when_data_arrives() {
        let _serial = startup();
        unsafe {
            let (listen, bound) = bound_listener();
            let client = PlatSocketOpen(PLAT_SOCKET_STREAM);
            assert_eq!(PlatSocketConnect(client, &bound), 1);
            let server = PlatSocketAccept(listen, null_mut());
            struct Shared(PlatSocket);
            unsafe impl Send for Shared {}
            let s = Shared(client);
            let sender = std::thread::spawn(move || {
                let s = s;
                std::thread::sleep(Duration::from_millis(40));
                PlatSocketSend(s.0, b"x".as_ptr().cast(), 1)
            });
            assert_eq!(PlatSocketWait(server, PLAT_SOCKET_WAIT_READ, PLAT_WAIT_INFINITE), PLAT_SOCKET_WAIT_READ as i32);
            assert_eq!(sender.join().unwrap(), 1);
            PlatSocketClose(client);
            PlatSocketClose(server);
            PlatSocketClose(listen);
        }
    }

    #[test]
    fn startup_is_counted_with_cleanup() {
        let _serial = startup();
        unsafe {
            assert_eq!(PlatSocketStartup(), 1);
            assert_eq!(PlatSocketCleanup(), 1);
            let bad: *mut c_void = null_mut();
            let _ = bad;
            // a descriptor that is no socket fails cleanly with a Winsock code. Not a second close of `s`: POSIX hands
            // the lowest free number to the next open, and SERIAL does not cover the file and thread tests, so a
            // file test's descriptor opened in between would be closed under it
            let s = PlatSocketOpen(PLAT_SOCKET_STREAM);
            assert_eq!(PlatSocketClose(s), 1);
            assert_eq!(PlatSocketClose(-1), 0);
            let e = PlatSocketLastError();
            assert!(e == PLAT_SOCK_ENOTSOCK || e == PLAT_SOCK_EBADF, "{e}");
        }
    }

    // Golden results of the C++ PlatSocketWait's mapping of poll flags (src/Platform/Posix/PlatSocket.cpp).
    #[cfg(unix)]
    #[test]
    fn poll_flags_map_to_wait_conditions() {
        use super::imp::ready_of;
        let (read, write, except) = (PLAT_SOCKET_WAIT_READ, PLAT_SOCKET_WAIT_WRITE, PLAT_SOCKET_WAIT_EXCEPT);
        let all = read | write | except;
        assert_eq!(ready_of(libc::POLLIN, all), read);
        assert_eq!(ready_of(libc::POLLOUT, all), write);
        assert_eq!(ready_of(libc::POLLIN | libc::POLLOUT, read), read); // restricted to the mask
                                                                        // a failed connect: poll adds POLLOUT, EXCEPT wins when asked for
        assert_eq!(ready_of(libc::POLLOUT | libc::POLLERR, write | except), except);
        // without EXCEPT the socket counts as writable (the send then fails with the reason)
        assert_eq!(ready_of(libc::POLLERR, write), write);
        // a reset or closed stream counts as readable
        assert_eq!(ready_of(libc::POLLHUP, read), read);
        assert_eq!(ready_of(libc::POLLERR, read | write), read | write);
    }

    #[cfg(unix)]
    #[test]
    fn errno_maps_to_winsock_numbers() {
        use super::imp::winsock_error;
        for (errno, wsa) in [
            (libc::EWOULDBLOCK, PLAT_SOCK_EWOULDBLOCK),
            (libc::EINPROGRESS, PLAT_SOCK_EWOULDBLOCK), // a non-blocking connect: Winsock says WSAEWOULDBLOCK
            (libc::ECONNREFUSED, PLAT_SOCK_ECONNREFUSED),
            (libc::EPIPE, PLAT_SOCK_ECONNRESET),
            (libc::ENOMEM, PLAT_SOCK_ENOBUFS),
            (libc::EPERM, PLAT_SOCK_EACCES),
            (libc::ENFILE, PLAT_SOCK_EMFILE),
            (libc::ENOTSOCK, PLAT_SOCK_ENOTSOCK),
            (libc::ETIMEDOUT, PLAT_SOCK_ETIMEDOUT),
            (libc::EAFNOSUPPORT, PLAT_SOCK_EAFNOSUPPORT),
            (9999, PLAT_SOCK_OTHER),
        ] {
            assert_eq!(winsock_error(errno), wsa, "errno {errno}");
        }
    }

    #[cfg(windows)]
    #[test]
    fn winsock_numbers_are_the_headers() {
        use windows_sys::Win32::Networking::WinSock::{
            WSAECONNREFUSED, WSAEDISCON, WSAENOTEMPTY, WSAESTALE, WSAEWOULDBLOCK, WSAHOST_NOT_FOUND,
        };
        assert_eq!(WSAEWOULDBLOCK as u32, PLAT_SOCK_EWOULDBLOCK);
        assert_eq!(WSAECONNREFUSED as u32, PLAT_SOCK_ECONNREFUSED);
        assert_eq!(WSAENOTEMPTY as u32, PLAT_SOCK_ENOTEMPTY);
        assert_eq!(WSAESTALE as u32, PLAT_SOCK_ESTALE);
        assert_eq!(WSAEDISCON as u32, 10101);
        assert_eq!(WSAHOST_NOT_FOUND as u32, PLAT_SOCK_HOST_NOT_FOUND);
    }
}
