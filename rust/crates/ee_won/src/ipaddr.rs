//! IPAddr (`include/ee/EEWonNet.h`, `src/Empire Earth/WONAPI/WONSocket/IPAddr.cpp`): WON's IPv4 address and port.
//! Parsing ("host", "host:port", with or without a name lookup), the text forms, the 6-byte wire form's order, and
//! this machine's addresses, on the platform layer's sockets (`ee_plat_sys`). The C++ `IPAddr` keeps its storage
//! (the `PlatSockAddrIn`, the remembered host string) and calls these.
//!
//! Addresses are kept as the platform layer keeps them: the IPv4 address and the port in network byte order.

use ee_plat_sys::constants::{PlatNetAdapter, PLAT_INADDR_NONE};
use ee_plat_sys::socket::imp as plat;
use std::ffi::CString;

/// An IPv4 address and port, both in network byte order (`PlatSockAddrIn`'s `mAddress` and `mPort`).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct Addr {
    pub address: u32,
    pub port: u16,
}

impl Addr {
    /// From a host-order address and port (`IPAddr(long, unsigned short)`).
    pub fn from_host(host: u32, port: u16) -> Addr {
        Addr { address: host.to_be(), port: port.to_be() }
    }

    /// The address in host order (`GetHost`, a `long` of the unsigned value).
    pub fn host(&self) -> u32 {
        u32::from_be(self.address)
    }

    /// The port in host order.
    pub fn host_port(&self) -> u16 {
        u16::from_be(self.port)
    }

    /// The 6-byte wire form: the port, then the address, as they lie in memory (network order).
    pub fn six_bytes(&self) -> [u8; 6] {
        let p = self.port.to_ne_bytes();
        let a = self.address.to_ne_bytes();
        [p[0], p[1], a[0], a[1], a[2], a[3]]
    }

    /// From the 6-byte wire form.
    pub fn from_six_bytes(b: &[u8; 6]) -> Addr {
        Addr { port: u16::from_ne_bytes([b[0], b[1]]), address: u32::from_ne_bytes([b[2], b[3], b[4], b[5]]) }
    }
}

/// The C string a `std::string`'s `c_str()` gives: the bytes up to the first NUL.
fn c_text(text: &[u8]) -> CString {
    let end = text.iter().position(|&b| b == 0).unwrap_or(text.len());
    CString::new(&text[..end]).expect("no NUL before end")
}

/// `inet_addr` (`PlatSocketParseIPv4`): the network-order address of "a.b.c.d", `PLAT_INADDR_NONE` when the text is
/// not one.
pub fn parse_ipv4(text: &[u8]) -> u32 {
    let c = c_text(text);
    // SAFETY: a NUL-terminated string
    unsafe { plat::PlatSocketParseIPv4(c.as_ptr()) }
}

/// The first IPv4 address of a host name, network order (`PlatSocketResolveIPv4`).
pub fn resolve_first(name: &[u8]) -> Option<u32> {
    let c = c_text(name);
    let mut address = 0u32;
    // SAFETY: a NUL-terminated name and room for one address
    let n = unsafe { plat::PlatSocketResolveIPv4(c.as_ptr(), &mut address, 1) };
    (n != 0).then_some(address)
}

/// The C runtime's `atoi` as the original (MSVC) has it: leading white space, an optional sign, digits; a value past
/// `int`'s range saturates.
pub fn atoi(text: &[u8]) -> i32 {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let mut negative = false;
    if i < text.len() && (text[i] == b'+' || text[i] == b'-') {
        negative = text[i] == b'-';
        i += 1;
    }
    let mut value: i64 = 0;
    while i < text.len() && text[i].is_ascii_digit() {
        value = (value * 10 + (text[i] - b'0') as i64).min(1 << 32);
        i += 1;
    }
    let value = if negative { -value } else { value };
    value.clamp(i32::MIN as i64, i32::MAX as i64) as i32
}

/// The port text after a colon as `IPAddr` reads it: `atoi`, cut to 16 bits.
pub fn port_of(text: &[u8]) -> u16 {
    atoi(text) as u16
}

/// "a dotted address or a host name" and a host-order port (`IPAddr::Fn2B2660`): the address, by a name lookup when
/// the text is not a dotted address; `None` when neither works.
pub fn resolve(host: &[u8], port: u16) -> Option<Addr> {
    let mut address = parse_ipv4(host);
    if address == PLAT_INADDR_NONE {
        address = resolve_first(host)?;
    }
    Some(Addr { address, port: port.to_be() })
}

/// Splits "host:port" at the first colon (`IPAddr::Fn2B2890`): (host, port), or `None` without a colon.
pub fn split_host_port(text: &[u8]) -> Option<(&[u8], u16)> {
    let colon = text.iter().position(|&b| b == b':')?;
    Some((&text[..colon], port_of(&text[colon + 1..])))
}

/// "host:port" (`IPAddr(const std::string&)`): resolved as [`resolve`]; `None` without a colon or when the host does
/// not resolve.
pub fn resolve_host_port(text: &[u8]) -> Option<Addr> {
    let (host, port) = split_host_port(text)?;
    resolve(host, port)
}

/// "host[:port]" with a default port (`SetWithDefaultPort`): with a colon as [`resolve_host_port`], else the whole
/// text is the host.
pub fn resolve_with_default_port(text: &[u8], default_port: u16) -> Option<Addr> {
    if text.contains(&b':') {
        resolve_host_port(text)
    } else {
        resolve(text, default_port)
    }
}

/// "a.b.c.d[:port]" without a name lookup (`SetWithDefaultPortNoDNS`): `None` when the address part is not a dotted
/// address (255.255.255.255 included: it is `inet_addr`'s failure value).
pub fn parse_no_dns(text: &[u8], default_port: u16) -> Option<Addr> {
    let (address, port) = match text.iter().position(|&b| b == b':') {
        Some(colon) => (parse_ipv4(&text[..colon]), port_of(&text[colon + 1..])),
        None => (parse_ipv4(text), default_port),
    };
    (address != PLAT_INADDR_NONE).then_some(Addr { address, port: port.to_be() })
}

/// The dotted form of a network-order address (`inet_ntoa`).
pub fn dotted(address: u32) -> String {
    let b = address.to_ne_bytes();
    format!("{}.{}.{}.{}", b[0], b[1], b[2], b[3])
}

/// `GetHostAndPortString`: "host:port" as `snprintf` into 50 bytes made it (at most 49 bytes; the host stops at a
/// NUL).
pub fn host_and_port(host: &[u8], port: u16) -> Vec<u8> {
    let end = host.iter().position(|&b| b == 0).unwrap_or(host.len());
    let mut out = host[..end].to_vec();
    out.extend_from_slice(format!(":{port}").as_bytes());
    out.truncate(49);
    out
}

/// `operator<` / `==`: the 6-byte wire forms compared as bytes (the port first).
pub fn compare_six(a: &Addr, b: &Addr) -> std::cmp::Ordering {
    a.six_bytes().cmp(&b.six_bytes())
}

/// `StringCompare`: the host-order addresses' difference as an `int`, else the ports'.
pub fn string_compare(a: &Addr, b: &Addr) -> i32 {
    let d = (a.host() as i32).wrapping_sub(b.host() as i32);
    if d != 0 {
        return d;
    }
    a.host_port() as i32 - b.host_port() as i32
}

/// This machine's name (`PlatSocketGetHostName`).
pub fn host_name() -> Option<Vec<u8>> {
    let mut name = [0u8; 1024];
    // SAFETY: a buffer of 1024 bytes
    if unsafe { plat::PlatSocketGetHostName(name.as_mut_ptr().cast(), name.len() as u32) } == 0 {
        return None;
    }
    let end = name.iter().position(|&b| b == 0).unwrap_or(name.len());
    Some(name[..end].to_vec())
}

/// The network adapters (description, network-order address), as many as `PlatSocketGetAdapters` gives (64 at
/// most).
pub fn adapters() -> Vec<(Vec<u8>, u32)> {
    let mut list = [PlatNetAdapter { mDescription: [0; 132], mAddress: 0, mNetmask: 0 }; 64];
    // SAFETY: room for 64
    let n = unsafe { plat::PlatSocketGetAdapters(list.as_mut_ptr(), 64) } as usize;
    list[..n.min(64)]
        .iter()
        .map(|a| {
            let end = a.mDescription.iter().position(|&b| b == 0).unwrap_or(a.mDescription.len());
            (a.mDescription[..end].to_vec(), a.mAddress)
        })
        .collect()
}

/// `LobbyGetNetAddresses`: this machine's addresses as (name, host-order address). With details, the adapter list
/// when it has any; else (or without details) the host name's addresses, unnamed.
pub fn local_addresses(details: bool) -> Vec<(Vec<u8>, u32)> {
    if details {
        let list = adapters();
        if !list.is_empty() {
            return list.into_iter().map(|(n, a)| (n, u32::from_be(a))).collect();
        }
    }
    let Some(name) = host_name() else { return Vec::new() };
    let c = c_text(&name);
    let mut addresses = [0u32; 64];
    // SAFETY: room for 64
    let n = unsafe { plat::PlatSocketResolveIPv4(c.as_ptr(), addresses.as_mut_ptr(), 64) } as usize;
    addresses[..n.min(64)].iter().map(|&a| (Vec::new(), u32::from_be(a))).collect()
}

/// `HasInternetConnection`: an adapter with an address that is neither 0.0.0.0 nor loopback (127/8). The original
/// asked wininet.
pub fn has_internet_connection() -> bool {
    adapters().iter().any(|&(_, a)| {
        let host = u32::from_be(a);
        host != 0 && (host >> 24) != 127
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cmp::Ordering;

    fn startup() {
        crate::socket::start();
    }

    #[test]
    fn atoi_like_msvc() {
        assert_eq!(atoi(b"1234"), 1234);
        assert_eq!(atoi(b"  +17x"), 17);
        assert_eq!(atoi(b"\t-5"), -5);
        assert_eq!(atoi(b"abc"), 0);
        assert_eq!(atoi(b""), 0);
        assert_eq!(atoi(b"99999999999999999999"), i32::MAX);
        assert_eq!(atoi(b"-99999999999999999999"), i32::MIN);
        assert_eq!(port_of(b"65536"), 0);
        assert_eq!(port_of(b"70000"), 4464);
        assert_eq!(port_of(b"-1"), 65535);
        assert_eq!(port_of(b"4294967296"), 65535);
    }

    #[test]
    fn parses_without_dns() {
        startup();
        let a = parse_no_dns(b"127.0.0.1:1234", 99).unwrap();
        assert_eq!((a.host(), a.host_port()), (0x7f00_0001, 1234));
        let a = parse_no_dns(b"10.1.2.3", 99).unwrap();
        assert_eq!((a.host(), a.host_port()), (0x0a01_0203, 99));
        assert!(parse_no_dns(b"not an address", 99).is_none());
        assert!(parse_no_dns(b"255.255.255.255:5", 99).is_none());
        // the port text after the first colon, whatever follows
        let a = parse_no_dns(b"1.2.3.4:80:90", 1).unwrap();
        assert_eq!(a.host_port(), 80);
        // the text stops at a NUL, as c_str() did
        let a = parse_no_dns(b"1.2.3.4\0junk", 7).unwrap();
        assert_eq!((a.host(), a.host_port()), (0x0102_0304, 7));
    }

    #[test]
    fn resolves_names_and_host_port() {
        startup();
        assert_eq!(resolve(b"127.0.0.1", 7).unwrap(), Addr::from_host(0x7f00_0001, 7));
        assert!(resolve(b"localhost", 7).is_some());
        assert_eq!(resolve_host_port(b"127.0.0.1:80").unwrap(), Addr::from_host(0x7f00_0001, 80));
        assert!(resolve_host_port(b"127.0.0.1").is_none());
        assert_eq!(resolve_with_default_port(b"127.0.0.1", 5).unwrap().host_port(), 5);
        assert_eq!(resolve_with_default_port(b"127.0.0.1:6", 5).unwrap().host_port(), 6);
        assert!(resolve(b"no-such-host.invalid", 1).is_none());
    }

    #[test]
    fn text_forms() {
        assert_eq!(dotted(Addr::from_host(0xc0a8_0102, 0).address), "192.168.1.2");
        assert_eq!(dotted(0xffff_ffff), "255.255.255.255");
        assert_eq!(host_and_port(b"127.0.0.1", 1234), b"127.0.0.1:1234");
        let long = [b'h'; 60];
        let s = host_and_port(&long, 80);
        assert_eq!(s.len(), 49);
        assert!(s.iter().all(|&b| b == b'h'));
        assert_eq!(host_and_port(&[b'h'; 44], 65535), [&[b'h'; 44][..], b":6553"].concat());
        assert_eq!(host_and_port(b"a\0b", 1), b"a:1");
    }

    #[test]
    fn wire_form_and_order() {
        let a = Addr::from_host(0x7f00_0001, 1234);
        assert_eq!(a.six_bytes(), [0x04, 0xd2, 127, 0, 0, 1]);
        assert_eq!(Addr::from_six_bytes(&a.six_bytes()), a);
        // the port decides first, as bytes
        let b = Addr::from_host(0x0100_0000, 1235);
        assert_eq!(compare_six(&a, &b), Ordering::Less);
        assert_eq!(compare_six(&a, &a), Ordering::Equal);
        assert_eq!(string_compare(&a, &b), 0x7f00_0001 - 0x0100_0000);
        assert_eq!(string_compare(&Addr::from_host(1, 9), &Addr::from_host(1, 2)), 7);
        // the int difference wraps
        assert_eq!(string_compare(&Addr::from_host(0xffff_ffff, 0), &Addr::from_host(0, 0)), -1);
        assert_eq!(string_compare(&Addr::from_host(0x8000_0000, 0), &Addr::from_host(1, 0)), i32::MAX);
    }

    #[test]
    fn local_addresses_list() {
        startup();
        assert!(host_name().is_some());
        // informational: depends on the machine, but the call works and names no adapter without an address
        let with = local_addresses(true);
        let without = local_addresses(false);
        assert!(without.iter().all(|(n, _)| n.is_empty()));
        let _ = (with, has_internet_connection());
    }
}
