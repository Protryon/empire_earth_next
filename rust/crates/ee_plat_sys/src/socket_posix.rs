//! PlatSocket on BSD sockets. Errors are errno mapped to Winsock's numbering (per thread). Readiness events emulate
//! WSAEventSelect with one poll thread, started by the first PlatSocketSelectEvent: each registered socket is "armed"
//! until the poll finds it ready, then its event is set and it is disarmed until the next receive or accept on it
//! re-arms it (Winsock's re-enabling rule: a recv that leaves data behind signals again). A self-pipe wakes the poll
//! when the set changes. Sends never raise SIGPIPE (MSG_NOSIGNAL / SO_NOSIGPIPE).

#![allow(clippy::unnecessary_cast)]

use crate::constants::*;
use crate::cstr::cstr_bytes;
use crate::last_error::errno;
use crate::thread::imp::{PlatEvent, PlatEventSet};
use std::cell::Cell;
use std::collections::BTreeMap;
use std::ffi::{c_char, c_int, c_void};
use std::mem::size_of;
use std::ptr::null_mut;
use std::sync::Mutex;

#[cfg(target_os = "linux")]
const SEND_FLAGS: c_int = libc::MSG_NOSIGNAL;
#[cfg(not(target_os = "linux"))]
const SEND_FLAGS: c_int = 0;

thread_local! {
    static SOCKET_ERROR: Cell<u32> = const { Cell::new(0) };
}

fn set_socket_error(error: u32) {
    SOCKET_ERROR.with(|e| e.set(error));
}

/// Winsock's number for an errno value.
pub fn winsock_error(errno: i32) -> u32 {
    #[rustfmt::skip]
    const TABLE: &[(i32, u32)] = &[
        (libc::EINTR, PLAT_SOCK_EINTR),
        (libc::EBADF, PLAT_SOCK_EBADF),
        (libc::EACCES, PLAT_SOCK_EACCES),
        (libc::EPERM, PLAT_SOCK_EACCES),
        (libc::EFAULT, PLAT_SOCK_EFAULT),
        (libc::EINVAL, PLAT_SOCK_EINVAL),
        (libc::EMFILE, PLAT_SOCK_EMFILE),
        (libc::ENFILE, PLAT_SOCK_EMFILE),
        (libc::EWOULDBLOCK, PLAT_SOCK_EWOULDBLOCK),
        (libc::EINPROGRESS, PLAT_SOCK_EWOULDBLOCK), // a non-blocking connect: Winsock says WSAEWOULDBLOCK
        (libc::EALREADY, PLAT_SOCK_EALREADY),
        (libc::ENOTSOCK, PLAT_SOCK_ENOTSOCK),
        (libc::EMSGSIZE, PLAT_SOCK_EMSGSIZE),
        (libc::EOPNOTSUPP, PLAT_SOCK_EOPNOTSUPP),
        (libc::EADDRINUSE, PLAT_SOCK_EADDRINUSE),
        (libc::EADDRNOTAVAIL, PLAT_SOCK_EADDRNOTAVAIL),
        (libc::ENETDOWN, PLAT_SOCK_ENETDOWN),
        (libc::ENETUNREACH, PLAT_SOCK_ENETUNREACH),
        (libc::ECONNABORTED, PLAT_SOCK_ECONNABORTED),
        (libc::ECONNRESET, PLAT_SOCK_ECONNRESET),
        (libc::EPIPE, PLAT_SOCK_ECONNRESET),
        (libc::ENOBUFS, PLAT_SOCK_ENOBUFS),
        (libc::ENOMEM, PLAT_SOCK_ENOBUFS),
        (libc::EISCONN, PLAT_SOCK_EISCONN),
        (libc::ENOTCONN, PLAT_SOCK_ENOTCONN),
        (libc::ETIMEDOUT, PLAT_SOCK_ETIMEDOUT),
        (libc::ECONNREFUSED, PLAT_SOCK_ECONNREFUSED),
        (libc::EHOSTUNREACH, PLAT_SOCK_EHOSTUNREACH),
        (libc::EDESTADDRREQ, PLAT_SOCK_EDESTADDRREQ),
        (libc::EPROTOTYPE, PLAT_SOCK_EPROTOTYPE),
        (libc::ENOPROTOOPT, PLAT_SOCK_ENOPROTOOPT),
        (libc::EPROTONOSUPPORT, PLAT_SOCK_EPROTONOSUPPORT),
        (libc::ESOCKTNOSUPPORT, PLAT_SOCK_ESOCKTNOSUPPORT),
        (libc::EPFNOSUPPORT, PLAT_SOCK_EPFNOSUPPORT),
        (libc::EAFNOSUPPORT, PLAT_SOCK_EAFNOSUPPORT),
        (libc::ENETRESET, PLAT_SOCK_ENETRESET),
        (libc::ESHUTDOWN, PLAT_SOCK_ESHUTDOWN),
        (libc::ETOOMANYREFS, PLAT_SOCK_ETOOMANYREFS),
        (libc::ELOOP, PLAT_SOCK_ELOOP),
        (libc::ENAMETOOLONG, PLAT_SOCK_ENAMETOOLONG),
        (libc::EHOSTDOWN, PLAT_SOCK_EHOSTDOWN),
        (libc::ENOTEMPTY, PLAT_SOCK_ENOTEMPTY),
        (libc::EUSERS, PLAT_SOCK_EUSERS),
        (libc::EDQUOT, PLAT_SOCK_EDQUOT),
        (libc::ESTALE, PLAT_SOCK_ESTALE),
        (libc::EREMOTE, PLAT_SOCK_EREMOTE),
    ];
    TABLE.iter().find(|(e, _)| *e == errno).map_or(PLAT_SOCK_OTHER, |(_, w)| *w)
}

/// Sets the socket error from errno -> 0.
fn fail_errno() -> i32 {
    set_socket_error(winsock_error(errno()));
    0
}

fn fd(socket: PlatSocket) -> c_int {
    socket as c_int
}

fn sockaddr_in_zeroed() -> libc::sockaddr_in {
    unsafe { std::mem::zeroed() }
}

unsafe fn to_native(address: *const PlatSockAddrIn) -> libc::sockaddr_in {
    let mut out = sockaddr_in_zeroed();
    #[cfg(target_vendor = "apple")]
    {
        out.sin_len = size_of::<libc::sockaddr_in>() as u8;
    }
    out.sin_family = libc::AF_INET as libc::sa_family_t;
    out.sin_port = (*address).mPort;
    out.sin_addr.s_addr = (*address).mAddress;
    out
}

unsafe fn from_native(address: &libc::sockaddr_in, out: *mut PlatSockAddrIn) {
    if !out.is_null() {
        *out = PlatSockAddrIn {
            mFamily: PLAT_AF_INET,
            mPort: address.sin_port,
            mAddress: address.sin_addr.s_addr,
            mZero: [0; 8],
        };
    }
}

const SOCKADDR_SIZE: libc::socklen_t = size_of::<libc::sockaddr_in>() as libc::socklen_t;

// ---- readiness events

#[derive(Clone, Copy)]
struct Registration {
    event: usize, // *mut PlatEvent, as an integer: the map is Send
    armed: bool,
}

struct PollState {
    registrations: BTreeMap<c_int, Registration>,
    wake_pipe: [c_int; 2],
    started: bool,
}

// never dropped: a detached poll thread keeps reading it while exit() runs
static POLL: Mutex<PollState> =
    Mutex::new(PollState { registrations: BTreeMap::new(), wake_pipe: [-1, -1], started: false });

fn wake(state: &PollState) {
    if state.wake_pipe[1] >= 0 {
        let c = 0u8;
        unsafe { libc::write(state.wake_pipe[1], (&raw const c).cast(), 1) };
    }
}

fn poll_thread() {
    let wake_read = POLL.lock().unwrap().wake_pipe[0];
    let mut fds: Vec<libc::pollfd> = Vec::new();
    loop {
        fds.clear();
        fds.push(libc::pollfd { fd: wake_read, events: libc::POLLIN, revents: 0 });
        {
            let state = POLL.lock().unwrap();
            for (&socket, registration) in &state.registrations {
                if registration.armed {
                    fds.push(libc::pollfd { fd: socket, events: libc::POLLIN, revents: 0 });
                }
            }
        }
        if unsafe { libc::poll(fds.as_mut_ptr(), fds.len() as libc::nfds_t, -1) } < 0 {
            continue;
        }
        if fds[0].revents & libc::POLLIN != 0 {
            let mut buffer = [0u8; 64];
            while unsafe { libc::read(wake_read, buffer.as_mut_ptr().cast(), buffer.len()) } > 0 {}
        }
        let mut state = POLL.lock().unwrap();
        for entry in &fds[1..] {
            if entry.revents & (libc::POLLIN | libc::POLLHUP | libc::POLLERR) == 0 {
                continue;
            }
            match state.registrations.get_mut(&entry.fd) {
                Some(registration) if registration.armed => {
                    registration.armed = false;
                    unsafe { PlatEventSet(registration.event as *mut PlatEvent) };
                }
                _ => {} // closed or re-registered meanwhile
            }
        }
    }
}

fn start_poll_thread(state: &mut PollState) -> bool {
    if state.started {
        return true;
    }
    let mut pipe = [-1 as c_int; 2];
    if unsafe { libc::pipe(pipe.as_mut_ptr()) } != 0 {
        return false;
    }
    for descriptor in pipe {
        unsafe {
            libc::fcntl(descriptor, libc::F_SETFL, libc::fcntl(descriptor, libc::F_GETFL) | libc::O_NONBLOCK);
            libc::fcntl(descriptor, libc::F_SETFD, libc::FD_CLOEXEC);
        }
    }
    state.wake_pipe = pipe;
    state.started = std::thread::Builder::new().spawn(poll_thread).is_ok();
    state.started
}

/// A receive or accept on the socket: arm its event again.
fn rearm(socket: PlatSocket) {
    let mut state = POLL.lock().unwrap();
    let mut woken = false;
    if let Some(registration) = state.registrations.get_mut(&fd(socket)) {
        if !registration.armed {
            registration.armed = true;
            woken = true;
        }
    }
    if woken {
        wake(&state);
    }
}

#[cfg(target_vendor = "apple")]
fn set_no_sigpipe(socket: c_int) {
    let on: c_int = 1;
    unsafe {
        libc::setsockopt(
            socket,
            libc::SOL_SOCKET,
            libc::SO_NOSIGPIPE,
            (&raw const on).cast(),
            size_of::<c_int>() as libc::socklen_t,
        )
    };
}
#[cfg(not(target_vendor = "apple"))]
fn set_no_sigpipe(_socket: c_int) {}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketStartup() -> i32 {
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketCleanup() -> i32 {
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketLastError() -> u32 {
    SOCKET_ERROR.with(Cell::get)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketOpen(kind: i32) -> PlatSocket {
    let socket =
        libc::socket(libc::AF_INET, if kind == PLAT_SOCKET_STREAM { libc::SOCK_STREAM } else { libc::SOCK_DGRAM }, 0);
    if socket < 0 {
        fail_errno();
        return PLAT_SOCKET_INVALID;
    }
    libc::fcntl(socket, libc::F_SETFD, libc::FD_CLOEXEC);
    set_no_sigpipe(socket);
    socket as PlatSocket
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketClose(socket: PlatSocket) -> i32 {
    {
        let mut state = POLL.lock().unwrap();
        if state.registrations.remove(&fd(socket)).is_some() {
            wake(&state);
        }
    }
    if libc::close(fd(socket)) == 0 {
        1
    } else {
        fail_errno()
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketBind(socket: PlatSocket, address: *const PlatSockAddrIn) -> i32 {
    let native = to_native(address);
    if libc::bind(fd(socket), (&raw const native).cast(), SOCKADDR_SIZE) == 0 {
        1
    } else {
        fail_errno()
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketListen(socket: PlatSocket, backlog: i32) -> i32 {
    let backlog = if backlog == PLAT_SOCKET_MAX_BACKLOG { libc::SOMAXCONN } else { backlog };
    if libc::listen(fd(socket), backlog) == 0 {
        1
    } else {
        fail_errno()
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketAccept(socket: PlatSocket, peer: *mut PlatSockAddrIn) -> PlatSocket {
    let mut native = sockaddr_in_zeroed();
    let mut length = SOCKADDR_SIZE;
    let accepted = libc::accept(fd(socket), (&raw mut native).cast(), &mut length);
    let error = errno();
    rearm(socket);
    if accepted < 0 {
        set_socket_error(winsock_error(error));
        return PLAT_SOCKET_INVALID;
    }
    libc::fcntl(accepted, libc::F_SETFD, libc::FD_CLOEXEC);
    set_no_sigpipe(accepted);
    // Winsock: the accepted socket takes the listening socket's blocking mode (and its event selection, which the
    // game replaces right away; not carried over here)
    let flags = libc::fcntl(fd(socket), libc::F_GETFL);
    if flags >= 0 && flags & libc::O_NONBLOCK != 0 {
        libc::fcntl(accepted, libc::F_SETFL, libc::fcntl(accepted, libc::F_GETFL) | libc::O_NONBLOCK);
    }
    from_native(&native, peer);
    accepted as PlatSocket
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketConnect(socket: PlatSocket, address: *const PlatSockAddrIn) -> i32 {
    let native = to_native(address);
    loop {
        if libc::connect(fd(socket), (&raw const native).cast(), SOCKADDR_SIZE) == 0 {
            return 1;
        }
        if errno() != libc::EINTR {
            return fail_errno();
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketGetName(socket: PlatSocket, out: *mut PlatSockAddrIn) -> i32 {
    let mut native = sockaddr_in_zeroed();
    let mut length = SOCKADDR_SIZE;
    if libc::getsockname(fd(socket), (&raw mut native).cast(), &mut length) != 0 {
        return fail_errno();
    }
    from_native(&native, out);
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetNonBlocking(socket: PlatSocket, non_blocking: i32) -> i32 {
    let flags = libc::fcntl(fd(socket), libc::F_GETFL);
    if flags < 0 {
        return fail_errno();
    }
    let flags = if non_blocking != 0 { flags | libc::O_NONBLOCK } else { flags & !libc::O_NONBLOCK };
    if libc::fcntl(fd(socket), libc::F_SETFL, flags) == 0 {
        1
    } else {
        fail_errno()
    }
}

unsafe fn set_int_option(socket: PlatSocket, level: c_int, option: c_int, on: i32) -> i32 {
    let value: c_int = (on != 0) as c_int;
    if libc::setsockopt(fd(socket), level, option, (&raw const value).cast(), size_of::<c_int>() as libc::socklen_t)
        == 0
    {
        1
    } else {
        fail_errno()
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetBroadcast(socket: PlatSocket, broadcast: i32) -> i32 {
    set_int_option(socket, libc::SOL_SOCKET, libc::SO_BROADCAST, broadcast)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetLinger(socket: PlatSocket, on: i32, seconds: i32) -> i32 {
    let value = libc::linger { l_onoff: (on != 0) as c_int, l_linger: seconds };
    if libc::setsockopt(
        fd(socket),
        libc::SOL_SOCKET,
        libc::SO_LINGER,
        (&raw const value).cast(),
        size_of::<libc::linger>() as libc::socklen_t,
    ) == 0
    {
        1
    } else {
        fail_errno()
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetReuseAddress(socket: PlatSocket, reuse: i32) -> i32 {
    set_int_option(socket, libc::SOL_SOCKET, libc::SO_REUSEADDR, reuse)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetNoDelay(socket: PlatSocket, no_delay: i32) -> i32 {
    set_int_option(socket, libc::IPPROTO_TCP, libc::TCP_NODELAY, no_delay)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketShutdown(socket: PlatSocket, how: i32) -> i32 {
    let how = match how {
        PLAT_SOCKET_SHUT_RECEIVE => libc::SHUT_RD,
        PLAT_SOCKET_SHUT_SEND => libc::SHUT_WR,
        _ => libc::SHUT_RDWR,
    };
    if libc::shutdown(fd(socket), how) == 0 {
        1
    } else {
        fail_errno()
    }
}

/// The ready conditions for a `poll` result, restricted to `mask`. A reset or closed stream counts as readable (the
/// receive then tells which). A failed connect is except only on Winsock, while poll adds POLLOUT: when EXCEPT is
/// asked for it wins over WRITE; otherwise the socket counts as writable (the send then fails with the reason).
pub fn ready_of(revents: i16, mask: u32) -> u32 {
    let mut ready = 0u32;
    if revents & libc::POLLIN != 0 {
        ready |= PLAT_SOCKET_WAIT_READ;
    }
    if revents & libc::POLLOUT != 0 {
        ready |= PLAT_SOCKET_WAIT_WRITE;
    }
    if revents & (libc::POLLERR | libc::POLLHUP) != 0 {
        ready |= PLAT_SOCKET_WAIT_READ;
        if mask & PLAT_SOCKET_WAIT_EXCEPT != 0 {
            ready = (ready & !PLAT_SOCKET_WAIT_WRITE) | PLAT_SOCKET_WAIT_EXCEPT;
        } else {
            ready |= PLAT_SOCKET_WAIT_WRITE;
        }
    }
    ready & mask
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketWait(socket: PlatSocket, mask: u32, timeout_ms: u32) -> i32 {
    let mut pollfd = libc::pollfd { fd: fd(socket), events: 0, revents: 0 };
    if mask & PLAT_SOCKET_WAIT_READ != 0 {
        pollfd.events |= libc::POLLIN;
    }
    if mask & PLAT_SOCKET_WAIT_WRITE != 0 {
        pollfd.events |= libc::POLLOUT;
    }
    // POLLERR / POLLHUP are always reported; with no condition asked for poll still waits out the timeout (select
    // with an empty set does the same)
    let mut start: libc::timespec = std::mem::zeroed();
    libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut start);
    let result = loop {
        let mut timeout: c_int = -1;
        if timeout_ms != PLAT_WAIT_INFINITE {
            let mut now: libc::timespec = std::mem::zeroed();
            libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut now);
            let elapsed = (now.tv_sec as i64 - start.tv_sec as i64) * 1000
                + (now.tv_nsec as i64 - start.tv_nsec as i64) / 1_000_000;
            let left = timeout_ms as i64 - elapsed;
            timeout = if left <= 0 {
                0
            } else if left > 0x7fff_ffff {
                0x7fff_ffff
            } else {
                left as c_int
            };
        }
        let polled = libc::poll(&mut pollfd, 1, timeout);
        if polled >= 0 || errno() != libc::EINTR {
            break polled;
        }
    };
    if result < 0 {
        fail_errno();
        return -1;
    }
    if result == 0 {
        return 0;
    }
    if pollfd.revents & libc::POLLNVAL != 0 {
        set_socket_error(PLAT_SOCK_ENOTSOCK);
        return -1;
    }
    ready_of(pollfd.revents, mask) as i32
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketGetError(socket: PlatSocket) -> u32 {
    let mut error: c_int = 0;
    let mut length = size_of::<c_int>() as libc::socklen_t;
    if libc::getsockopt(fd(socket), libc::SOL_SOCKET, libc::SO_ERROR, (&raw mut error).cast(), &mut length) != 0 {
        error = errno();
    }
    if error == 0 {
        0
    } else {
        winsock_error(error)
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSend(socket: PlatSocket, data: *const c_void, length: u32) -> i32 {
    loop {
        let sent = libc::send(fd(socket), data, length as usize, SEND_FLAGS);
        if sent >= 0 {
            return sent as i32;
        }
        if errno() != libc::EINTR {
            fail_errno();
            return -1;
        }
    }
}

/// recv / recvfrom retried across EINTR; re-arms the readiness event (Winsock re-enables FD_READ after a peek too:
/// the data is still there).
unsafe fn receive(socket: PlatSocket, call: impl Fn() -> isize) -> i32 {
    let read = loop {
        let read = call();
        if read >= 0 || errno() != libc::EINTR {
            break read;
        }
    };
    let error = errno();
    rearm(socket);
    if read < 0 {
        set_socket_error(winsock_error(error));
        return -1;
    }
    read as i32
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketRecv(socket: PlatSocket, data: *mut c_void, length: u32) -> i32 {
    receive(socket, || libc::recv(fd(socket), data, length as usize, 0))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketPeek(socket: PlatSocket, data: *mut c_void, length: u32) -> i32 {
    receive(socket, || libc::recv(fd(socket), data, length as usize, libc::MSG_PEEK))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSendTo(
    socket: PlatSocket,
    data: *const c_void,
    length: u32,
    to: *const PlatSockAddrIn,
) -> i32 {
    let native = to_native(to);
    loop {
        let sent =
            libc::sendto(fd(socket), data, length as usize, SEND_FLAGS, (&raw const native).cast(), SOCKADDR_SIZE);
        if sent >= 0 {
            return sent as i32;
        }
        if errno() != libc::EINTR {
            fail_errno();
            return -1;
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketRecvFrom(
    socket: PlatSocket,
    data: *mut c_void,
    length: u32,
    from: *mut PlatSockAddrIn,
) -> i32 {
    let mut native = sockaddr_in_zeroed();
    let native_ptr: *mut libc::sockaddr_in = &mut native;
    let mut size = SOCKADDR_SIZE;
    let size_ptr: *mut libc::socklen_t = &mut size;
    let read = receive(socket, || libc::recvfrom(fd(socket), data, length as usize, 0, native_ptr.cast(), size_ptr));
    if read >= 0 {
        from_native(&native, from);
    }
    read
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSelectEvent(socket: PlatSocket, event: *mut PlatEvent, mask: u32) -> i32 {
    let mut state = POLL.lock().unwrap();
    if mask == 0 || event.is_null() {
        state.registrations.remove(&fd(socket));
        wake(&state);
        return 1;
    }
    if !start_poll_thread(&mut state) {
        set_socket_error(PLAT_SOCK_ENOBUFS);
        return 0;
    }
    // WSAEventSelect makes the socket non-blocking
    let flags = libc::fcntl(fd(socket), libc::F_GETFL);
    if flags < 0 || libc::fcntl(fd(socket), libc::F_SETFL, flags | libc::O_NONBLOCK) != 0 {
        return fail_errno();
    }
    state.registrations.insert(fd(socket), Registration { event: event as usize, armed: true });
    wake(&state);
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketGetHostName(name: *mut c_char, size: u32) -> i32 {
    if libc::gethostname(name, size as usize) == 0 {
        1
    } else {
        fail_errno()
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketResolveIPv4(name: *const c_char, addresses: *mut u32, max: u32) -> u32 {
    let mut hints: libc::addrinfo = std::mem::zeroed();
    hints.ai_family = libc::AF_INET;
    hints.ai_socktype = libc::SOCK_STREAM; // one entry per address
    let mut list: *mut libc::addrinfo = null_mut();
    if libc::getaddrinfo(name, std::ptr::null(), &hints, &mut list) != 0 || list.is_null() {
        set_socket_error(PLAT_SOCK_HOST_NOT_FOUND);
        return 0;
    }
    let mut n = 0u32;
    let mut p = list;
    while !p.is_null() && n < max {
        if (*p).ai_family == libc::AF_INET {
            let address = (*(*p).ai_addr.cast::<libc::sockaddr_in>()).sin_addr.s_addr;
            let seen = (0..n as usize).any(|i| *addresses.add(i) == address);
            if !seen {
                *addresses.add(n as usize) = address;
                n += 1;
            }
        }
        p = (*p).ai_next;
    }
    libc::freeaddrinfo(list);
    if n == 0 {
        set_socket_error(PLAT_SOCK_HOST_NOT_FOUND);
    }
    n
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketReverseLookup(address: u32, name: *mut c_char, size: u32) -> i32 {
    let mut native = sockaddr_in_zeroed();
    #[cfg(target_vendor = "apple")]
    {
        native.sin_len = size_of::<libc::sockaddr_in>() as u8;
    }
    native.sin_family = libc::AF_INET as libc::sa_family_t;
    native.sin_addr.s_addr = address;
    if libc::getnameinfo((&raw const native).cast(), SOCKADDR_SIZE, name, size as _, null_mut(), 0, libc::NI_NAMEREQD)
        != 0
    {
        set_socket_error(PLAT_SOCK_HOST_NOT_FOUND);
        return 0;
    }
    1
}

extern "C" {
    // inet_addr: also accepts the short forms ("127.1") and octal/hex parts, as Winsock does
    fn inet_addr(text: *const c_char) -> u32;
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketParseIPv4(text: *const c_char) -> u32 {
    inet_addr(text)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketFormatIPv4(address: u32, text: *mut c_char) {
    let b = address.to_ne_bytes();
    let formatted = format!("{}.{}.{}.{}", b[0], b[1], b[2], b[3]);
    std::ptr::copy_nonoverlapping(formatted.as_ptr().cast(), text, formatted.len());
    *text.add(formatted.len()) = 0;
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketGetAdapters(adapters: *mut PlatNetAdapter, max: u32) -> u32 {
    let mut list: *mut libc::ifaddrs = null_mut();
    if libc::getifaddrs(&mut list) != 0 {
        fail_errno();
        return 0;
    }
    let mut n = 0u32;
    let mut p = list;
    while !p.is_null() && n < max {
        let flags = (*p).ifa_flags as c_int;
        let address = (*p).ifa_addr;
        if !address.is_null()
            && (*address).sa_family as c_int == libc::AF_INET
            && flags & libc::IFF_LOOPBACK == 0
            && flags & libc::IFF_UP != 0
        {
            // GetAdaptersInfo lists no loopback adapter
            let out = &mut *adapters.add(n as usize);
            *out = PlatNetAdapter { mDescription: [0; 132], mAddress: 0, mNetmask: 0 };
            let name = cstr_bytes((*p).ifa_name);
            let length = name.len().min(131);
            out.mDescription[..length].copy_from_slice(&name[..length]);
            out.mAddress = (*address.cast::<libc::sockaddr_in>()).sin_addr.s_addr;
            let netmask = (*p).ifa_netmask;
            if !netmask.is_null() {
                out.mNetmask = (*netmask.cast::<libc::sockaddr_in>()).sin_addr.s_addr;
            }
            n += 1;
        }
        p = (*p).ifa_next;
    }
    libc::freeifaddrs(list);
    n
}
