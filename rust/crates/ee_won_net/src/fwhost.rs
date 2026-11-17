//! `DetectFirewallOp` on the Rust op base (round 10): fwop.rs's op (its members and every step, as round 6 moved
//! them) as a [`HostRequest`] of the request exchange (reqhost.rs), so the C++ class is a `RustServerOp` with its
//! setters and no hook table. The listening socket and the datagram receive ops are host calls, one per statement
//! of round 6's hooks ([`HostEnv`]); the op's own members (the request, the reply, the server error, its mode and
//! time left) are the exchange's ([`FwSide`]). What can call back into the op (`Finish`, `Track` with `RunAsync`
//! of the receive) runs after the op's lock is let go: [`Own`] and `HostRequest::datagram`'s receive.
//!
//! The behaviour is `golden_fwop.txt` again: `fwop::tests::golden_through_the_base` replays it with every object
//! statement a host call answered by the replay's stand-in. Reached by nothing in the game but an Internet login.

use crate::fwop::{Callback, Check, Cut, Datagram, Env, FirewallOp, Obj as FObj};
use crate::hostop::{
    EeWonHostOp, Obj, SetValue, EE_WON_HOST_OP_RECV_BYTES_FROM, EE_WON_HOST_RUN, EE_WON_HOST_SOCKET_LISTEN,
    EE_WON_HOST_SOCKET_PORT,
};
use crate::reqhost::{set_last_server_error, Calls, HostRequest, Own, ReqPlan};
use crate::reqop::Checked;
use crate::server::st;

/// `SetListenPort(num)`: `ee_won_hostop_set`
pub const EE_WON_FWOP_LISTEN_PORT: i32 = 1;
/// `SetDoListen(num)`: `ee_won_hostop_set`
pub const EE_WON_FWOP_DO_LISTEN: i32 = 2;
/// `SetWaitForConnect(num)`: `ee_won_hostop_set`
pub const EE_WON_FWOP_WAIT_FOR_CONNECT: i32 = 3;
/// whether the server gave our public address (`ee_won_hostop_num`)
pub const EE_WON_FWOP_HAS_PUBLIC: i32 = 4;
/// the public address's host, set with port 0 (`ee_won_hostop_num`)
pub const EE_WON_FWOP_PUBLIC_HOST: i32 = 5;

/// `ServerRequestOp::RecvServerDatagram`'s receive id.
const RECV_ID: i32 = 1000;

fn o(p: FObj) -> Obj {
    Obj::of(p)
}

/// The op's own side of its environment: the base op's mode and time left and the members the exchange keeps.
pub(crate) trait FwSide {
    fn time_left(&mut self, c: &Calls) -> u32;
    fn mode(&mut self, c: &Calls) -> i32;
    fn set_request(&mut self, c: &Calls, message: &[u8]);
    fn response(&mut self, c: &Calls) -> Vec<u8>;
    fn invalid_header(&mut self, c: &Calls) -> i32;
    fn set_public(&mut self, host: i32);
}

/// The real side.
pub(crate) struct ExSide;

impl FwSide for ExSide {
    fn time_left(&mut self, c: &Calls) -> u32 {
        c.h.time_left()
    }
    fn mode(&mut self, c: &Calls) -> i32 {
        // OP_MODE_ASYNC or blocking: the op tests only the first
        if c.h.is_async() {
            1
        } else {
            2
        }
    }
    fn set_request(&mut self, c: &Calls, message: &[u8]) {
        c.h.st().ex.request = Some(message.to_vec());
    }
    fn response(&mut self, c: &Calls) -> Vec<u8> {
        c.h.st().ex.response.clone().unwrap_or_default()
    }
    fn invalid_header(&mut self, c: &Calls) -> i32 {
        set_last_server_error(c.h, st::INVALID_REPLY_HEADER);
        st::TRY_NEXT_SERVER
    }
    fn set_public(&mut self, _host: i32) {}
}

/// fwop.rs's environment over the host.
pub(crate) struct HostEnv<'a, 'b, S> {
    pub(crate) c: &'a Calls<'b>,
    pub(crate) side: S,
}

impl<S: FwSide> Env for HostEnv<'_, '_, S> {
    fn release(&mut self, p: FObj) {
        self.c.h.release(o(p))
    }
    fn new_socket(&mut self, udp: bool) -> FObj {
        self.c.h.new_socket(udp).ptr()
    }
    fn bind(&mut self, socket: FObj, port: u16) -> i32 {
        self.c.h.bind(o(socket), port, false)
    }
    fn listen(&mut self, socket: FObj) -> i32 {
        self.c.h.call(EE_WON_HOST_SOCKET_LISTEN, [o(socket), Obj::none(), Obj::none()], [0, 0], &[], [0; 6]).num as i32
    }
    fn local_port(&mut self, socket: FObj) -> u16 {
        self.c.h.call(EE_WON_HOST_SOCKET_PORT, [o(socket), Obj::none(), Obj::none()], [0, 0], &[], [0; 6]).num as u16
    }
    fn close(&mut self, socket: FObj) {
        self.c.h.close(o(socket))
    }
    fn new_recv(&mut self, socket: FObj) -> FObj {
        self.c.h.new_op(EE_WON_HOST_OP_RECV_BYTES_FROM, o(socket), &[], [0; 6], 255).ptr()
    }
    fn recv_run(&mut self, recv: FObj, wait: u32) -> i32 {
        let r = self.c.h.call(EE_WON_HOST_RUN, [o(recv), Obj::none(), Obj::none()], [2, i64::from(wait)], &[], [0; 6]);
        r.num as i32
    }
    fn recv_from_current(&mut self, recv: FObj) -> bool {
        // `GetAddr().GetHost() == GetCurAddr().GetHost()`: the six bytes' last four
        let a = self.c.h.op_addr(o(recv));
        let cur = self.c.h.st().ex.cur_addr;
        a[2..] == cur[2..]
    }
    fn time_left(&mut self) -> u32 {
        self.side.time_left(self.c)
    }
    fn mode(&mut self) -> i32 {
        self.side.mode(self.c)
    }
    fn set_request(&mut self, message: &[u8]) {
        self.side.set_request(self.c, message)
    }
    fn response(&mut self) -> Vec<u8> {
        self.side.response(self.c)
    }
    fn invalid_header(&mut self) -> i32 {
        self.side.invalid_header(self.c)
    }
    fn set_public(&mut self, host: i32) {
        self.side.set_public(host)
    }
}

/// `DetectFirewallOp` on the base.
pub(crate) struct FwReq(pub(crate) FirewallOp);

// SAFETY: the op's pointers are C++ `RefCount*`s reached only through the host calls, under the op's lock
unsafe impl Send for FwReq {}

fn env<'a, 'b>(c: &'a Calls<'b>) -> HostEnv<'a, 'b, ExSide> {
    HostEnv { c, side: ExSide }
}

impl HostRequest for FwReq {
    /// `Init`: 2-byte lengths
    fn length_field_size(&self) -> u8 {
        2
    }
    fn use_auth2(&self) -> bool {
        true
    }
    /// `Init`: no receive limit
    fn max_times(&self) -> [u32; 3] {
        [20000, 20000, 0xffff_ffff]
    }
    fn run_hook(&mut self, c: &Calls) -> Option<i32> {
        self.0.run_hook(&mut env(c))
    }
    fn next_request(&mut self, c: &Calls, _datagram: bool) -> (i32, Option<Vec<u8>>) {
        let lfs = c.h.lfs();
        (self.0.get_next_request(&mut env(c), lfs), None)
    }
    fn check_response(&mut self, c: &Calls, _response: &[u8]) -> Checked {
        match self.0.check_response(&mut env(c)) {
            Err(Cut) => Checked::Cut,
            Ok(Check::Status(s)) => Checked::Status(s),
            Ok(Check::Datagram) => Checked::Datagram,
        }
    }
    fn reset(&mut self, _c: &Calls) {}
    fn cleanup(&mut self, c: &Calls, after: bool) {
        if after {
            self.0.cleanup_hook(&mut env(c));
        }
    }
    fn callback(&mut self, c: &Calls, id: i32, op: Obj) -> Option<Own> {
        if id != RECV_ID {
            return None;
        }
        let status = c.h.op_status(op);
        match self.0.callback_hook(&mut env(c), id, status) {
            Callback::Base => None,
            Callback::Datagram => Some(Own::Datagram),
            Callback::Finish(s) => Some(Own::Finish(s)),
        }
    }
    fn datagram(&mut self, c: &Calls) -> Result<i32, (Obj, u32)> {
        match self.0.recv_datagram(&mut env(c)) {
            Datagram::Status(s) => Ok(s),
            Datagram::Async(wait) => Err((o(self.0.recv), wait)),
        }
    }
    fn describe(&self) -> String {
        let f = &self.0;
        format!(
            "port {} wait {} listen {} waitc {} udp {} pub {}",
            f.listen_port,
            f.max_wait,
            u8::from(f.do_listen),
            u8::from(f.wait_for_connect),
            u8::from(f.use_udp),
            f.public.map_or("-".to_string(), |h| h.to_string())
        )
    }
    fn num(&self, which: i32, _index: i64) -> i64 {
        match which {
            EE_WON_FWOP_HAS_PUBLIC => i64::from(self.0.public.is_some()),
            EE_WON_FWOP_PUBLIC_HOST => i64::from(self.0.public.unwrap_or(0)),
            _ => 0,
        }
    }
    fn bytes(&self, _which: i32, _index: i64) -> Option<Vec<u8>> {
        None
    }
    fn set(&mut self, _c: &Calls, which: i32, value: &SetValue) {
        match which {
            EE_WON_FWOP_LISTEN_PORT => self.0.listen_port = value.num as u16,
            EE_WON_FWOP_DO_LISTEN => self.0.do_listen = value.num != 0,
            EE_WON_FWOP_WAIT_FOR_CONNECT => self.0.wait_for_connect = value.num != 0,
            _ => {}
        }
    }
    fn take_objects(&mut self) -> Vec<Obj> {
        let f = &mut self.0;
        [&mut f.recv, &mut f.socket]
            .into_iter()
            .map(|p| o(std::mem::replace(p, std::ptr::null_mut())))
            .filter(|p| !p.is_none())
            .collect()
    }
}

/// A new `DetectFirewallOp`: its server context (`EE_WON_REQHOST_CONTEXT`) is set after the bind.
#[no_mangle]
pub extern "C" fn ee_won_hostop_firewall() -> *mut EeWonHostOp {
    EeWonHostOp::boxed(ReqPlan::new(Box::new(FwReq(FirewallOp::new()))))
}
