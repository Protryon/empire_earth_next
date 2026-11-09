//! PlatSocket on Winsock 2. A PlatSocket is the SOCKET, a PlatSockAddrIn has sockaddr_in's layout, and the readiness
//! event is WSAEventSelect on the PlatEvent's handle (a `PlatEvent*` is the event HANDLE). The calls are the Winsock
//! functions the TCP protocol made itself, so the test harness's socket guards see the same imports.

use crate::constants::*;
use crate::cstr::{cstr_bytes, strncpy};
use crate::thread::imp::PlatEvent;
use std::ffi::{c_char, c_void};
use std::mem::size_of;
use std::ptr::{null, null_mut};
use windows_sys::Win32::Foundation::{ERROR_BUFFER_OVERFLOW, ERROR_SUCCESS};
use windows_sys::Win32::NetworkManagement::IpHelper::{GetAdaptersInfo, IP_ADAPTER_INFO};
use windows_sys::Win32::Networking::WinSock::{
    __WSAFDIsSet, accept, bind, closesocket, connect, gethostbyaddr, gethostbyname, gethostname, getsockname,
    getsockopt, inet_addr, inet_ntoa, ioctlsocket, listen, recv, recvfrom, select, send, sendto, setsockopt, shutdown,
    socket, WSACleanup, WSAEventSelect, WSAGetLastError, WSAStartup, AF_INET, FD_ACCEPT, FD_READ, FD_SET, FIONBIO,
    INVALID_SOCKET, IN_ADDR, IPPROTO_TCP, LINGER, MSG_PEEK, SD_BOTH, SD_RECEIVE, SD_SEND, SOCKADDR, SOCKADDR_IN,
    SOCKET, SOCKET_ERROR, SOCK_DGRAM, SOCK_STREAM, SOL_SOCKET, SO_BROADCAST, SO_ERROR, SO_LINGER, SO_REUSEADDR,
    TCP_NODELAY, TIMEVAL, WSADATA, WSAEWOULDBLOCK, WSAHOST_NOT_FOUND,
};

const _: () = assert!(size_of::<PlatSockAddrIn>() == size_of::<SOCKADDR_IN>());
const _: () = assert!(PLAT_SOCKET_EVENT_READ == FD_READ && PLAT_SOCKET_EVENT_ACCEPT == FD_ACCEPT);
const _: () = assert!(PLAT_SOCK_EWOULDBLOCK == WSAEWOULDBLOCK as u32);
const _: () = assert!(PLAT_SOCK_HOST_NOT_FOUND == WSAHOST_NOT_FOUND as u32);
const _: () = assert!(
    PLAT_SOCKET_SHUT_RECEIVE == SD_RECEIVE && PLAT_SOCKET_SHUT_SEND == SD_SEND && PLAT_SOCKET_SHUT_BOTH == SD_BOTH
);

fn native(socket: PlatSocket) -> SOCKET {
    socket as SOCKET
}

fn address(addr: *const PlatSockAddrIn) -> *const SOCKADDR {
    addr.cast()
}

fn status(result: i32) -> i32 {
    (result == 0) as i32
}

const SOCKADDR_SIZE: i32 = size_of::<SOCKADDR_IN>() as i32;

#[no_mangle]
pub unsafe extern "C" fn PlatSocketStartup() -> i32 {
    let mut data: WSADATA = std::mem::zeroed();
    status(WSAStartup(0x0002, &mut data))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketCleanup() -> i32 {
    status(WSACleanup())
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketLastError() -> u32 {
    WSAGetLastError() as u32
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketOpen(kind: i32) -> PlatSocket {
    let kind = if kind == PLAT_SOCKET_STREAM { SOCK_STREAM } else { SOCK_DGRAM };
    let s = socket(AF_INET as i32, kind, 0);
    if s == INVALID_SOCKET {
        PLAT_SOCKET_INVALID
    } else {
        s as PlatSocket
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketClose(s: PlatSocket) -> i32 {
    status(closesocket(native(s)))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketBind(s: PlatSocket, addr: *const PlatSockAddrIn) -> i32 {
    status(bind(native(s), address(addr), SOCKADDR_SIZE))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketListen(s: PlatSocket, backlog: i32) -> i32 {
    status(listen(native(s), backlog))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketAccept(s: PlatSocket, peer: *mut PlatSockAddrIn) -> PlatSocket {
    let mut length = SOCKADDR_SIZE;
    let length_ptr: *mut i32 = if peer.is_null() { null_mut() } else { &mut length };
    let accepted = accept(native(s), peer.cast(), length_ptr);
    if accepted == INVALID_SOCKET {
        PLAT_SOCKET_INVALID
    } else {
        accepted as PlatSocket
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketConnect(s: PlatSocket, addr: *const PlatSockAddrIn) -> i32 {
    status(connect(native(s), address(addr), SOCKADDR_SIZE))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketGetName(s: PlatSocket, out: *mut PlatSockAddrIn) -> i32 {
    let mut length = SOCKADDR_SIZE;
    status(getsockname(native(s), out.cast(), &mut length))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetNonBlocking(s: PlatSocket, non_blocking: i32) -> i32 {
    let mut value: u32 = (non_blocking != 0) as u32;
    status(ioctlsocket(native(s), FIONBIO, &mut value))
}

unsafe fn set_bool_option(s: PlatSocket, level: i32, option: i32, on: i32) -> i32 {
    let value: i32 = (on != 0) as i32; // BOOL
    status(setsockopt(native(s), level, option, (&raw const value).cast(), size_of::<i32>() as i32))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetBroadcast(s: PlatSocket, broadcast: i32) -> i32 {
    set_bool_option(s, SOL_SOCKET, SO_BROADCAST, broadcast)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetLinger(s: PlatSocket, on: i32, seconds: i32) -> i32 {
    let value = LINGER { l_onoff: (on != 0) as u16, l_linger: seconds as u16 };
    status(setsockopt(native(s), SOL_SOCKET, SO_LINGER, (&raw const value).cast(), size_of::<LINGER>() as i32))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetReuseAddress(s: PlatSocket, reuse: i32) -> i32 {
    set_bool_option(s, SOL_SOCKET, SO_REUSEADDR, reuse)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSetNoDelay(s: PlatSocket, no_delay: i32) -> i32 {
    set_bool_option(s, IPPROTO_TCP, TCP_NODELAY, no_delay)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketShutdown(s: PlatSocket, how: i32) -> i32 {
    status(shutdown(native(s), how))
}

fn set_of(s: SOCKET, member: bool) -> FD_SET {
    let mut set: FD_SET = unsafe { std::mem::zeroed() };
    if member {
        set.fd_count = 1;
        set.fd_array[0] = s;
    }
    set
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketWait(s: PlatSocket, mask: u32, timeout_ms: u32) -> i32 {
    let handle = native(s);
    let mut read = set_of(handle, mask & PLAT_SOCKET_WAIT_READ != 0);
    let mut write = set_of(handle, mask & PLAT_SOCKET_WAIT_WRITE != 0);
    let mut except = set_of(handle, mask & PLAT_SOCKET_WAIT_EXCEPT != 0);
    let time = TIMEVAL { tv_sec: (timeout_ms / 1000) as i32, tv_usec: ((timeout_ms % 1000) * 1000) as i32 };
    let time_ptr: *const TIMEVAL = if timeout_ms == PLAT_WAIT_INFINITE { null() } else { &time };
    if select(0, &mut read, &mut write, &mut except, time_ptr) == SOCKET_ERROR {
        return -1;
    }
    let mut ready = 0u32;
    if __WSAFDIsSet(handle, &mut read) != 0 {
        ready |= PLAT_SOCKET_WAIT_READ;
    }
    if __WSAFDIsSet(handle, &mut write) != 0 {
        ready |= PLAT_SOCKET_WAIT_WRITE;
    }
    if __WSAFDIsSet(handle, &mut except) != 0 {
        ready |= PLAT_SOCKET_WAIT_EXCEPT;
    }
    ready as i32
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketGetError(s: PlatSocket) -> u32 {
    let mut error = 0i32;
    let mut length = size_of::<i32>() as i32;
    if getsockopt(native(s), SOL_SOCKET, SO_ERROR, (&raw mut error).cast(), &mut length) != 0 {
        return WSAGetLastError() as u32;
    }
    error as u32
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSend(s: PlatSocket, data: *const c_void, length: u32) -> i32 {
    send(native(s), data.cast(), length as i32, 0)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketRecv(s: PlatSocket, data: *mut c_void, length: u32) -> i32 {
    recv(native(s), data.cast(), length as i32, 0)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketPeek(s: PlatSocket, data: *mut c_void, length: u32) -> i32 {
    recv(native(s), data.cast(), length as i32, MSG_PEEK)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSendTo(
    s: PlatSocket,
    data: *const c_void,
    length: u32,
    to: *const PlatSockAddrIn,
) -> i32 {
    sendto(native(s), data.cast(), length as i32, 0, address(to), SOCKADDR_SIZE)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketRecvFrom(
    s: PlatSocket,
    data: *mut c_void,
    length: u32,
    from: *mut PlatSockAddrIn,
) -> i32 {
    let mut size = SOCKADDR_SIZE;
    let size_ptr: *mut i32 = if from.is_null() { null_mut() } else { &mut size };
    recvfrom(native(s), data.cast(), length as i32, 0, from.cast(), size_ptr)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketSelectEvent(s: PlatSocket, event: *mut PlatEvent, mask: u32) -> i32 {
    status(WSAEventSelect(native(s), event as isize, mask as i32))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketGetHostName(name: *mut c_char, size: u32) -> i32 {
    status(gethostname(name.cast(), size as i32))
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketResolveIPv4(name: *const c_char, addresses: *mut u32, max: u32) -> u32 {
    let host = gethostbyname(name.cast());
    if host.is_null() || (*host).h_addrtype != AF_INET as i16 {
        return 0;
    }
    let list = (*host).h_addr_list;
    let mut n = 0u32;
    while !(*list.add(n as usize)).is_null() {
        if n < max {
            std::ptr::copy_nonoverlapping(
                (*list.add(n as usize)).cast::<u8>(),
                addresses.add(n as usize).cast::<u8>(),
                4,
            );
        }
        n += 1;
    }
    n.min(max)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketReverseLookup(address: u32, name: *mut c_char, size: u32) -> i32 {
    let host = gethostbyaddr((&raw const address).cast(), 4, AF_INET as i32);
    if host.is_null() || size == 0 {
        return 0;
    }
    let out = std::slice::from_raw_parts_mut(name.cast::<u8>(), size as usize);
    strncpy(&mut out[..size as usize - 1], cstr_bytes((*host).h_name.cast()));
    out[size as usize - 1] = 0;
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketParseIPv4(text: *const c_char) -> u32 {
    inet_addr(text.cast())
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketFormatIPv4(address: u32, text: *mut c_char) {
    let value = IN_ADDR { S_un: windows_sys::Win32::Networking::WinSock::IN_ADDR_0 { S_addr: address } };
    let out = std::slice::from_raw_parts_mut(text.cast::<u8>(), 16);
    strncpy(&mut out[..15], cstr_bytes(inet_ntoa(value).cast()));
    out[15] = 0;
}

#[no_mangle]
pub unsafe extern "C" fn PlatSocketGetAdapters(adapters: *mut PlatNetAdapter, max: u32) -> u32 {
    let mut size: u32 = 0x1900;
    let mut buffer: Vec<u64> = vec![0; (size as usize).div_ceil(8)];
    let mut error = GetAdaptersInfo(buffer.as_mut_ptr().cast::<IP_ADAPTER_INFO>(), &mut size);
    if error == ERROR_BUFFER_OVERFLOW {
        buffer = vec![0; (size as usize).div_ceil(8)];
        error = GetAdaptersInfo(buffer.as_mut_ptr().cast::<IP_ADAPTER_INFO>(), &mut size);
    }
    let mut n = 0u32;
    if error == ERROR_SUCCESS {
        let mut adapter = buffer.as_ptr().cast::<IP_ADAPTER_INFO>();
        while !adapter.is_null() {
            if n < max {
                let out = &mut *adapters.add(n as usize);
                strncpy(&mut out.mDescription[..131], cstr_bytes((&raw const (*adapter).Description).cast()));
                out.mDescription[131] = 0;
                out.mAddress = inet_addr((&raw const (*adapter).IpAddressList.IpAddress.String).cast());
                out.mNetmask = inet_addr((&raw const (*adapter).IpAddressList.IpMask.String).cast());
            }
            n += 1;
            adapter = (*adapter).Next;
        }
    }
    n.min(max)
}
