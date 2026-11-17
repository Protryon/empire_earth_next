//! `MultiPingOp` (WONServer/MultiPingOp.cpp) on the Rust op base (hostop.rs): the servers to ping, the batch in
//! flight by address, the finished list, the limits, the datagram socket and the batch's timeout, and every step:
//! `RunHook` (asynchronous: a repeating datagram receive and the batches; blocking: the batches in turn),
//! `PrepareBatch`, `FinishBatch`, `PingBatchAsync`, `PingBatchBlock`, `ResponseTimeLeft`, the receive and timeout
//! callbacks, `HandleResponsePrv`, `CleanupHook`'s order by ping time and `GetServerContext`'s addresses. The
//! request, the reply's test and the rules are server.rs's.
//!
//! Behaviour: `golden_hostop.txt`'s `M` sequences, recorded over the original's statements before the C++ class was
//! deleted. Nothing in the game makes one (no C++ user since the port).
//!
//! The original's undefined corners, defined: `PingBatchBlock`'s status before any send or receive (indeterminate)
//! is `WS_Success`; a successful receive with no bytes (a NULL dereference) is an invalid reply; a batch entry gone
//! while its batch is sent (an invalidated iterator) is skipped.

use std::collections::{BTreeMap, VecDeque};

use crate::hostop::{
    self, EeWonHostOp, Host, Obj, Plan, SetValue, EE_WON_HOST_OP_RECV_BYTES_FROM, EE_WON_HOST_OP_SEND_BYTES_TO,
    FOREVER, ST_PENDING, ST_SUCCESS,
};
use crate::server;

/// The repeating receive's completion id.
const ID_RECV: i32 = 1;
/// The batch timeout's completion id.
const ID_TIMEOUT: i32 = 2;

// Results (`ee_won_hostop_num` / `_bytes`), `index` the finished list's element:
/// the finished list's length
pub const EE_WON_MULTIPING_COUNT: i32 = 1;
/// an element's address (six bytes)
pub const EE_WON_MULTIPING_ADDR: i32 = 2;
/// an element's ping time (-1: no reply)
pub const EE_WON_MULTIPING_TIME: i32 = 3;
/// an element's failures
pub const EE_WON_MULTIPING_FAILURES: i32 = 4;
/// an element's directory entity (`DirEntity*`, borrowed; NULL: none)
pub const EE_WON_MULTIPING_ENTITY: i32 = 5;
/// whether `GetServerContext(true)` keeps the element (`GetServerContext(false)`: [`EE_WON_MULTIPING_KEEPS_ALL`])
pub const EE_WON_MULTIPING_KEEPS: i32 = 6;
pub const EE_WON_MULTIPING_KEEPS_ALL: i32 = 7;
// Setters (`ee_won_hostop_set`):
/// `SetBatchSize`, `SetMaxFailures`, `SetMaxPingTime` (the number)
pub const EE_WON_MULTIPING_BATCH_SIZE: i32 = 10;
pub const EE_WON_MULTIPING_MAX_FAILURES: i32 = 11;
pub const EE_WON_MULTIPING_MAX_PING_TIME: i32 = 12;
/// `AddServer(addr)` / `AddServers`' element (the address, the entity or NULL)
pub const EE_WON_MULTIPING_ADD: i32 = 13;
/// `ClearServers`
pub const EE_WON_MULTIPING_CLEAR: i32 = 14;

/// `MultiPingStruct`.
#[derive(Debug)]
struct Ping {
    addr: [u8; 6],
    ping_time: i32,
    failures: i32,
    start: u32,
    id: i32,
    entity: Obj,
}

/// The op's members.
pub(crate) struct MultiPing {
    list: VecDeque<Ping>,
    map: BTreeMap<[u8; 6], Ping>,
    finished: Vec<Ping>,
    batch_size: u16,
    max_failures: u8,
    max_ping_time: u32,
    max_reply: u16,
    order_by_time: bool,
    socket: Obj,
    timer: Obj,
    /// entities of the structs dropped (a map entry overwritten, a list cleared), released after the lock
    dropped: Vec<Obj>,
}

impl MultiPing {
    /// The constructor's limits.
    pub(crate) fn new() -> MultiPing {
        MultiPing {
            list: VecDeque::new(),
            map: BTreeMap::new(),
            finished: Vec::new(),
            batch_size: 20,
            max_failures: 3,
            max_ping_time: 3000,
            max_reply: 2048,
            order_by_time: true,
            socket: Obj::none(),
            timer: Obj::none(),
            dropped: Vec::new(),
        }
    }

    fn drop_pings(&mut self, pings: impl IntoIterator<Item = Ping>) {
        for p in pings {
            if !p.entity.is_none() {
                self.dropped.push(p.entity);
            }
        }
    }

    /// `PrepareBatch`
    fn prepare_batch(&mut self) {
        let old = std::mem::take(&mut self.map);
        self.drop_pings(old.into_values());
        let mut count = 0;
        while count < self.batch_size {
            let Some(p) = self.list.pop_front() else { break };
            if let Some(over) = self.map.insert(p.addr, p) {
                self.drop_pings([over]);
            }
            count += 1;
        }
    }

    /// `FinishBatch`
    fn finish_batch(&mut self) {
        let map = std::mem::take(&mut self.map);
        for (_, mut p) in map {
            p.failures += 1;
            if server::multi_ping_gives_up(p.failures, self.max_failures) {
                self.finished.push(p);
            } else {
                self.list.push_back(p);
            }
        }
    }
}

/// Releases the dropped structs' entities.
fn flush(h: &Host<MultiPing>) {
    let d = std::mem::take(&mut h.st().dropped);
    for o in d {
        h.release(o);
    }
}

/// `GetRequest(struct)` for the batch entry at `addr`: its ping id and start tick set; None when it is gone.
fn get_request(h: &Host<MultiPing>, addr: [u8; 6]) -> Option<Vec<u8>> {
    let id = h.rand();
    let start = h.tick();
    let mut s = h.st();
    let p = s.map.get_mut(&addr)?;
    p.id = id;
    p.start = start;
    Some(server::multi_ping_request(id))
}

/// `HandleResponsePrv(status, response, addr)`.
fn handle_response(h: &Host<MultiPing>, status: i32, response: Option<&[u8]>, addr: [u8; 6]) -> bool {
    let rule = server::multi_ping_recv_status(status);
    if rule == 0 {
        return false;
    }
    if rule == 2 {
        // HandleResponse: the reply's test, then the ping time
        let valid = {
            let s = h.st();
            s.map.get(&addr).map(|p| server::multi_ping_reply(response.unwrap_or(&[]), p.id))
        };
        if valid == Some(true) {
            let now = h.tick();
            let mut s = h.st();
            if let Some(mut p) = s.map.remove(&addr) {
                p.ping_time = now.wrapping_sub(p.start) as i32;
                s.finished.push(p);
            }
        }
    }
    true
}

/// `PingBatchAsync`
fn ping_batch_async(h: &Host<MultiPing>) {
    let timer = std::mem::take(&mut h.st().timer);
    if !timer.is_none() {
        h.kill(timer, true);
        h.release(timer);
    }
    let (keys, socket) = {
        let mut s = h.st();
        s.prepare_batch();
        (s.map.keys().copied().collect::<Vec<_>>(), s.socket)
    };
    flush(h);
    for addr in keys {
        let Some(ping) = get_request(h, addr) else { continue };
        let op = h.new_op(EE_WON_HOST_OP_SEND_BYTES_TO, Obj::none(), &ping, addr, 0);
        h.queue(socket, op, FOREVER);
        h.release(op);
    }
    let timer = h.new_timer(ID_TIMEOUT);
    h.replace(|s| &mut s.timer, timer);
    let wait = h.st().max_ping_time;
    h.run_async(timer, wait);
}

/// `ResponseTimeLeft(start)`
fn response_time_left(h: &Host<MultiPing>, start: u32) -> u32 {
    let now = h.tick();
    let max = h.st().max_ping_time;
    if server::multi_ping_response_time_left(now, start, max, FOREVER) == 0 {
        return 0;
    }
    let left = h.time_left();
    server::multi_ping_response_time_left(now, start, max, left)
}

/// `PingBatchBlock`
fn ping_batch_block(h: &Host<MultiPing>) -> i32 {
    let (keys, socket, max_reply) = {
        let mut s = h.st();
        s.prepare_batch();
        (s.map.keys().copied().collect::<Vec<_>>(), s.socket, s.max_reply)
    };
    flush(h);
    let mut status = ST_SUCCESS;
    let mut i = 0;
    while h.time_left() > 0 && i < keys.len() {
        let addr = keys[i];
        i += 1;
        let Some(ping) = get_request(h, addr) else { continue };
        let wait = h.time_left();
        status = h.send_bytes_to(socket, &ping, addr, wait);
        if status != ST_SUCCESS {
            return status;
        }
    }
    let start = h.tick();
    while response_time_left(h, start) > 0 && !h.st().map.is_empty() {
        let wait = response_time_left(h, start);
        let (st, reply, addr) = h.recv_bytes_from(socket, max_reply, wait);
        status = st;
        if !handle_response(h, status, reply.as_deref(), addr) {
            return status;
        }
    }
    status
}

impl Plan for MultiPing {
    fn run(h: &Host<MultiPing>) {
        h.srand();
        let socket = h.new_socket(true);
        h.replace(|s| &mut s.socket, socket);
        h.bind(socket, 0, false);
        {
            let mut s = h.st();
            let f = std::mem::take(&mut s.finished);
            s.drop_pings(f);
        }
        flush(h);
        if h.is_async() {
            let size = h.st().max_reply;
            let recv = h.new_op(EE_WON_HOST_OP_RECV_BYTES_FROM, Obj::none(), &[], [0; 6], u32::from(size));
            h.set_repeat(socket, recv, ID_RECV);
            h.release(recv);
            ping_batch_async(h);
            return;
        }
        let mut status;
        while h.time_left() > 0 && !h.st().list.is_empty() {
            status = ping_batch_block(h);
            h.st().finish_batch();
            if !server::multi_ping_batch_continues(status) {
                h.finish(status);
                return;
            }
        }
        h.finish(ST_SUCCESS);
    }

    fn callback(h: &Host<MultiPing>, id: i32, op: Obj) -> bool {
        match id {
            ID_RECV => {
                // RecvCallback
                if h.status() != ST_PENDING || h.op_socket(op) != h.st().socket {
                    return true;
                }
                let status = h.op_status(op);
                let bytes = h.op_bytes(op);
                let addr = h.op_addr(op);
                if !handle_response(h, status, bytes.as_deref(), addr) {
                    let st = h.op_status(op);
                    h.finish(st);
                }
                let next = {
                    let s = h.st();
                    server::multi_ping_next(s.map.len(), s.list.len())
                };
                if next == 1 {
                    h.finish(ST_SUCCESS);
                } else if next == 2 {
                    ping_batch_async(h);
                }
                true
            }
            ID_TIMEOUT => {
                // TimeoutCallback
                if h.status() != ST_PENDING || op != h.st().timer {
                    return true;
                }
                let timer = std::mem::take(&mut h.st().timer);
                h.release(timer);
                h.st().finish_batch();
                ping_batch_async(h);
                true
            }
            _ => false,
        }
    }

    fn cleanup(h: &Host<MultiPing>) {
        let socket = std::mem::take(&mut h.st().socket);
        if !socket.is_none() {
            h.close(socket);
            h.release(socket);
        }
        let timer = std::mem::take(&mut h.st().timer);
        if !timer.is_none() {
            h.kill(timer, true);
            h.release(timer);
        }
        let mut s = h.st();
        s.finish_batch();
        let rest: Vec<Ping> = s.list.drain(..).collect();
        s.finished.extend(rest);
        if !s.order_by_time {
            return;
        }
        let times: Vec<i32> = s.finished.iter().map(|p| p.ping_time).collect();
        let order = server::multi_ping_order(&times);
        let mut old: Vec<Option<Ping>> = std::mem::take(&mut s.finished).into_iter().map(Some).collect();
        s.finished = order.into_iter().filter_map(|i| old[i].take()).collect();
    }

    fn num(&self, which: i32, index: i64) -> i64 {
        if which == EE_WON_MULTIPING_COUNT {
            return self.finished.len() as i64;
        }
        let Some(p) = usize::try_from(index).ok().and_then(|i| self.finished.get(i)) else { return 0 };
        match which {
            EE_WON_MULTIPING_TIME => i64::from(p.ping_time),
            EE_WON_MULTIPING_FAILURES => i64::from(p.failures),
            EE_WON_MULTIPING_ENTITY => p.entity.0 as i64,
            EE_WON_MULTIPING_KEEPS => i64::from(server::multi_ping_keeps(p.ping_time, true)),
            EE_WON_MULTIPING_KEEPS_ALL => i64::from(server::multi_ping_keeps(p.ping_time, false)),
            _ => 0,
        }
    }

    fn bytes(&self, which: i32, index: i64) -> Option<Vec<u8>> {
        let p = usize::try_from(index).ok().and_then(|i| self.finished.get(i))?;
        (which == EE_WON_MULTIPING_ADDR).then(|| p.addr.to_vec())
    }

    fn set(h: &Host<MultiPing>, which: i32, v: &SetValue) {
        match which {
            EE_WON_MULTIPING_BATCH_SIZE => h.st().batch_size = v.num as u16,
            EE_WON_MULTIPING_MAX_FAILURES => h.st().max_failures = v.num as u8,
            EE_WON_MULTIPING_MAX_PING_TIME => h.st().max_ping_time = v.num as u32,
            EE_WON_MULTIPING_ADD => {
                h.add_ref(v.obj);
                h.st().list.push_back(Ping {
                    addr: v.addr,
                    ping_time: -1,
                    failures: 0,
                    start: 0,
                    id: 0,
                    entity: v.obj,
                });
            }
            EE_WON_MULTIPING_CLEAR => {
                let mut s = h.st();
                let l: Vec<Ping> = s.list.drain(..).collect();
                s.drop_pings(l);
            }
            _ => {}
        }
        flush(h);
    }

    fn describe(&self) -> String {
        let one = |p: &Ping| format!("{}:{}:{}:{}:{}", hostop::hex(&p.addr), p.ping_time, p.failures, p.id, p.start);
        let list: Vec<String> = self.list.iter().map(one).collect();
        let map: Vec<String> = self.map.values().map(one).collect();
        let fin: Vec<String> = self.finished.iter().map(one).collect();
        format!(
            "list {} map {} fin {} bs {} mf {} mp {} sock {} timer {}",
            list.join(","),
            map.join(","),
            fin.join(","),
            self.batch_size,
            self.max_failures,
            self.max_ping_time,
            self.socket.0,
            self.timer.0
        )
    }

    fn take_objects(&mut self) -> Vec<Obj> {
        let mut v = std::mem::take(&mut self.dropped);
        let list: Vec<Ping> = self.list.drain(..).collect();
        let map: Vec<Ping> = std::mem::take(&mut self.map).into_values().collect();
        let fin = std::mem::take(&mut self.finished);
        for p in list.into_iter().chain(map).chain(fin) {
            if !p.entity.is_none() {
                v.push(p.entity);
            }
        }
        for o in [&mut self.socket, &mut self.timer] {
            let x = std::mem::take(o);
            if !x.is_none() {
                v.push(x);
            }
        }
        v
    }
}

/// `new MultiPingOp`: the Rust op a `RustServerOp` runs.
#[no_mangle]
pub extern "C" fn ee_won_multiping_new() -> *mut EeWonHostOp {
    EeWonHostOp::boxed(MultiPing::new())
}
