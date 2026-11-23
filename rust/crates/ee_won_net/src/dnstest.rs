//! DNSTestOp whole in Rust (R6 WON round 34): WONRouting.h's `DNSTestOp` (an `AsyncOp` of 0x7c bytes; ctor 2d95a0,
//! dtor 2d9740, RunHook 2d97f0) and `DNSTestThread` (a self-deleting `Thread` holding the op; ThreadFunc 2d9920) as a
//! Rust op on the op base. Its run starts a thread holding a reference to the op; the thread resolves the host text
//! as `IPAddr::SetWithDefaultPort(text, 0)` ("host:port", else a looked-up name or dotted address with port 0) and
//! finishes the op: `WS_Success` when the address is valid, `WS_InvalidAddress` when not. The address is the op's
//! [`EE_WON_DNSTEST_ADDR`] (its six bytes) once the op finished.
//!
//! Reached by the lobby's internet start (InitLogic's directory server test) and the LAN screen's Direct Connect
//! when the address is a host name (LanLogicHost.cpp's `LanDirectAsk`).

use crate::hostop::{EeWonHostOp, Host, Obj, Plan};
use crate::ipobj::EeWonIpAddr;
use crate::status_codes::WS_InvalidAddress;

/// The resolved address's six bytes (port, then address; `GetHostAddr()`), empty before the op finished.
pub const EE_WON_DNSTEST_ADDR: i32 = 1;
/// Whether the resolved address is valid (`GetHostAddr().IsValid()`).
pub const EE_WON_DNSTEST_VALID: i32 = 2;

/// The op's members: `mHostString`, `mHostAddr`.
pub(crate) struct DnsTest {
    host: Vec<u8>,
    addr: Option<EeWonIpAddr>,
}

/// The thread's work (`DNSTestThread::ThreadFunc`): the address and the op's status.
pub fn resolve(host: &[u8]) -> (EeWonIpAddr, i32) {
    let mut a = EeWonIpAddr::default();
    a.set_with_default_port(host, 0);
    let status = if a.valid { 0 } else { WS_InvalidAddress };
    (a, status)
}

/// The op's address for the thread (alive while the thread holds the op's reference).
struct HostRef(*const Host<DnsTest>);
// SAFETY: the Host is Sync (its state behind locks) and outlives the thread (the C++ op's reference)
unsafe impl Send for HostRef {}

impl Plan for DnsTest {
    fn run(h: &Host<DnsTest>) {
        // the thread holds the op (DNSTestThread::mOp) and deletes itself
        let op = Obj(h.own().op);
        crate::cppcore::ref_add(op.ptr());
        let host = h.st().host.clone();
        let r = HostRef(h);
        std::thread::spawn(move || {
            let r = r;
            let (a, status) = resolve(&host);
            // SAFETY: the op is held until the release below
            let h = unsafe { &*r.0 };
            h.st().addr = Some(a);
            h.finish(status);
            crate::cppcore::ref_release(op.ptr());
        });
    }

    fn callback(_h: &Host<DnsTest>, _id: i32, _op: Obj) -> bool {
        false
    }

    fn cleanup(_h: &Host<DnsTest>) {}

    fn num(&self, which: i32, _index: i64) -> i64 {
        match which {
            EE_WON_DNSTEST_VALID => i64::from(self.addr.as_ref().is_some_and(|a| a.valid)),
            _ => 0,
        }
    }

    fn bytes(&self, which: i32, _index: i64) -> Option<Vec<u8>> {
        match which {
            EE_WON_DNSTEST_ADDR => self.addr.as_ref().map(|a| a.six().to_vec()),
            _ => None,
        }
    }

    fn describe(&self) -> String {
        format!(
            "host {} addr {}",
            crate::hostop::hex(&self.host),
            self.addr.as_ref().map_or("-".to_string(), |a| format!("{}:{}", crate::hostop::hex(&a.six()), a.valid))
        )
    }

    fn take_objects(&mut self) -> Vec<Obj> {
        Vec::new()
    }
}

/// `new DNSTestOp(host)` (`RustServerOp` takes it).
///
/// # Safety
/// `host` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dnstest_new(host: *const u8, len: usize) -> *mut EeWonHostOp {
    // SAFETY: the contract
    let host = unsafe { crate::ffi::bytes(host, len) }.to_vec();
    EeWonHostOp::boxed(DnsTest { host, addr: None })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dotted_addresses_and_ports() {
        let (a, s) = resolve(b"127.0.0.1:1234");
        assert!(a.valid);
        assert_eq!(s, 0);
        assert_eq!(a.six(), [0x04, 0xd2, 127, 0, 0, 1]);
        let (a, s) = resolve(b"10.0.0.2");
        assert!(a.valid);
        assert_eq!(s, 0);
        assert_eq!(a.six(), [0, 0, 10, 0, 0, 2]);
    }

    #[test]
    fn an_empty_or_bad_host_is_invalid() {
        let (a, s) = resolve(b"");
        assert!(!a.valid);
        assert_eq!(s, WS_InvalidAddress);
        let (_, s) = resolve(b"no-colon-port:");
        assert_eq!(s, resolve(b"no-colon-port:").1);
    }

    #[test]
    fn the_address_is_read_once_finished() {
        let mut d = DnsTest { host: b"x".to_vec(), addr: None };
        assert_eq!(d.bytes(EE_WON_DNSTEST_ADDR, 0), None);
        d.addr = Some(resolve(b"127.0.0.1:2").0);
        assert_eq!(d.num(EE_WON_DNSTEST_VALID, 0), 1);
        assert_eq!(d.bytes(EE_WON_DNSTEST_ADDR, 0), Some(vec![0, 2, 127, 0, 0, 1]));
    }
}
