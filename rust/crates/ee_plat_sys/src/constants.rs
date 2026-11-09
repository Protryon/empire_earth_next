//! The values of `include/platform/Plat*.h`, the contract the C++ callers compile against, and the records that
//! cross the C ABI with their layout pinned.

use std::mem::{offset_of, size_of};

/// `PLAT_MAX_PATH`: the longest path the game builds, terminator included.
pub const PLAT_MAX_PATH: usize = 260;

/// `PlatFileResult`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PlatFileResult {
    Ok = 0,
    Exists = 1,
    BadPath = 2,
    NoMore = 3,
    NativeError = 4,
}

pub const PLAT_ATTR_READONLY: u32 = 0x01;
pub const PLAT_ATTR_HIDDEN: u32 = 0x02;
pub const PLAT_ATTR_SYSTEM: u32 = 0x04;
pub const PLAT_ATTR_DIRECTORY: u32 = 0x10;
pub const PLAT_ATTR_ARCHIVE: u32 = 0x20;
pub const PLAT_ATTR_NORMAL: u32 = 0x80;
pub const PLAT_ATTR_INVALID: u32 = 0xFFFF_FFFF;

pub const PLAT_FILE_READ: u32 = 0x1;
pub const PLAT_FILE_WRITE: u32 = 0x2;

pub const PLAT_SEEK_BEGIN: i32 = 0;
pub const PLAT_SEEK_CURRENT: i32 = 1;
pub const PLAT_SEEK_END: i32 = 2;

/// `PlatFindData`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatFindData {
    pub mName: [u8; PLAT_MAX_PATH],
    pub mAttributes: u32,
}

const _: () = assert!(size_of::<PlatFindData>() == 264);
const _: () = assert!(offset_of!(PlatFindData, mAttributes) == 260);

pub const PLAT_WAIT_INFINITE: u32 = 0xFFFF_FFFF;
pub const PLAT_WAIT_TIMEOUT: i32 = -1;
pub const PLAT_WAIT_FAILED: i32 = -2;
pub const PLAT_WAIT_MAX: u32 = 64;

pub const PLAT_THREAD_STILL_ACTIVE: u32 = 259;

pub const PLAT_PRIORITY_IDLE: i32 = -15;
pub const PLAT_PRIORITY_LOWEST: i32 = -2;
pub const PLAT_PRIORITY_BELOW_NORMAL: i32 = -1;
pub const PLAT_PRIORITY_NORMAL: i32 = 0;
pub const PLAT_PRIORITY_ABOVE_NORMAL: i32 = 1;
pub const PLAT_PRIORITY_HIGHEST: i32 = 2;
pub const PLAT_PRIORITY_TIME_CRITICAL: i32 = 15;
pub const PLAT_PRIORITY_ERROR: i32 = 0x7FFF_FFFF;

/// `PlatSocket`: a SOCKET or a file descriptor.
pub type PlatSocket = isize;
pub const PLAT_SOCKET_INVALID: PlatSocket = -1;

// Winsock's numbering, on every system: the WON library's WSAE* names are defined as these numbers.
pub const PLAT_SOCK_EINTR: u32 = 10004;
pub const PLAT_SOCK_EBADF: u32 = 10009;
pub const PLAT_SOCK_EACCES: u32 = 10013;
pub const PLAT_SOCK_EFAULT: u32 = 10014;
pub const PLAT_SOCK_EINVAL: u32 = 10022;
pub const PLAT_SOCK_EMFILE: u32 = 10024;
pub const PLAT_SOCK_EWOULDBLOCK: u32 = 10035;
pub const PLAT_SOCK_EINPROGRESS: u32 = 10036;
pub const PLAT_SOCK_EALREADY: u32 = 10037;
pub const PLAT_SOCK_ENOTSOCK: u32 = 10038;
pub const PLAT_SOCK_EDESTADDRREQ: u32 = 10039;
pub const PLAT_SOCK_EMSGSIZE: u32 = 10040;
pub const PLAT_SOCK_EPROTOTYPE: u32 = 10041;
pub const PLAT_SOCK_ENOPROTOOPT: u32 = 10042;
pub const PLAT_SOCK_EPROTONOSUPPORT: u32 = 10043;
pub const PLAT_SOCK_ESOCKTNOSUPPORT: u32 = 10044;
pub const PLAT_SOCK_EOPNOTSUPP: u32 = 10045;
pub const PLAT_SOCK_EPFNOSUPPORT: u32 = 10046;
pub const PLAT_SOCK_EAFNOSUPPORT: u32 = 10047;
pub const PLAT_SOCK_EADDRINUSE: u32 = 10048;
pub const PLAT_SOCK_EADDRNOTAVAIL: u32 = 10049;
pub const PLAT_SOCK_ENETDOWN: u32 = 10050;
pub const PLAT_SOCK_ENETUNREACH: u32 = 10051;
pub const PLAT_SOCK_ENETRESET: u32 = 10052;
pub const PLAT_SOCK_ECONNABORTED: u32 = 10053;
pub const PLAT_SOCK_ECONNRESET: u32 = 10054;
pub const PLAT_SOCK_ENOBUFS: u32 = 10055;
pub const PLAT_SOCK_EISCONN: u32 = 10056;
pub const PLAT_SOCK_ENOTCONN: u32 = 10057;
pub const PLAT_SOCK_ESHUTDOWN: u32 = 10058;
pub const PLAT_SOCK_ETOOMANYREFS: u32 = 10059;
pub const PLAT_SOCK_ETIMEDOUT: u32 = 10060;
pub const PLAT_SOCK_ECONNREFUSED: u32 = 10061;
pub const PLAT_SOCK_ELOOP: u32 = 10062;
pub const PLAT_SOCK_ENAMETOOLONG: u32 = 10063;
pub const PLAT_SOCK_EHOSTDOWN: u32 = 10064;
pub const PLAT_SOCK_EHOSTUNREACH: u32 = 10065;
pub const PLAT_SOCK_ENOTEMPTY: u32 = 10066;
pub const PLAT_SOCK_EUSERS: u32 = 10068;
pub const PLAT_SOCK_EDQUOT: u32 = 10069;
pub const PLAT_SOCK_ESTALE: u32 = 10070;
pub const PLAT_SOCK_EREMOTE: u32 = 10071;
pub const PLAT_SOCK_OTHER: u32 = 10100;
pub const PLAT_SOCK_HOST_NOT_FOUND: u32 = 11001;

pub const PLAT_AF_INET: u16 = 2;
pub const PLAT_INADDR_ANY: u32 = 0;
pub const PLAT_INADDR_NONE: u32 = 0xFFFF_FFFF;

pub const PLAT_SOCKET_STREAM: i32 = 1;
pub const PLAT_SOCKET_DGRAM: i32 = 2;
pub const PLAT_SOCKET_MAX_BACKLOG: i32 = 0x7fff_ffff;

pub const PLAT_SOCKET_SHUT_RECEIVE: i32 = 0;
pub const PLAT_SOCKET_SHUT_SEND: i32 = 1;
pub const PLAT_SOCKET_SHUT_BOTH: i32 = 2;

pub const PLAT_SOCKET_WAIT_READ: u32 = 0x1;
pub const PLAT_SOCKET_WAIT_WRITE: u32 = 0x2;
pub const PLAT_SOCKET_WAIT_EXCEPT: u32 = 0x4;

pub const PLAT_SOCKET_EVENT_READ: u32 = 0x01;
pub const PLAT_SOCKET_EVENT_ACCEPT: u32 = 0x08;

/// `PlatSockAddrIn`, laid out as Windows' `sockaddr_in`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PlatSockAddrIn {
    pub mFamily: u16,
    pub mPort: u16,
    pub mAddress: u32,
    pub mZero: [u8; 8],
}

const _: () = assert!(size_of::<PlatSockAddrIn>() == 16);
const _: () = assert!(offset_of!(PlatSockAddrIn, mPort) == 2);
const _: () = assert!(offset_of!(PlatSockAddrIn, mAddress) == 4);

/// `PlatNetAdapter`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatNetAdapter {
    pub mDescription: [u8; 132],
    pub mAddress: u32,
    pub mNetmask: u32,
}

const _: () = assert!(size_of::<PlatNetAdapter>() == 140);
const _: () = assert!(offset_of!(PlatNetAdapter, mAddress) == 132);

#[cfg(test)]
mod tests {
    use super::*;

    // The header's numbers, written out: the C++ callers and the WON library compare against them.
    #[test]
    fn header_values() {
        assert_eq!(PLAT_MAX_PATH, 260);
        assert_eq!(PlatFileResult::Ok as i32, 0);
        assert_eq!(PlatFileResult::Exists as i32, 1);
        assert_eq!(PlatFileResult::BadPath as i32, 2);
        assert_eq!(PlatFileResult::NoMore as i32, 3);
        assert_eq!(PlatFileResult::NativeError as i32, 4);
        assert_eq!(PLAT_WAIT_TIMEOUT, -1);
        assert_eq!(PLAT_WAIT_FAILED, -2);
        assert_eq!(PLAT_THREAD_STILL_ACTIVE, 259);
        assert_eq!(PLAT_SOCK_EWOULDBLOCK, 10035);
        assert_eq!(PLAT_SOCK_ECONNREFUSED, 10061);
        assert_eq!(PLAT_SOCK_HOST_NOT_FOUND, 11001);
        assert_eq!(PLAT_SOCK_OTHER, 10100);
        assert_eq!(PLAT_SOCKET_EVENT_ACCEPT, 0x08);
    }
}
