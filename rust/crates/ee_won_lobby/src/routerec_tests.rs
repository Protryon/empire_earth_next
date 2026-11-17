//! The routing records read into the model ([`route_record_op`]): `golden_routerec.txt`, recorded by
//! `won_routemodel_test` over the original statements (RoutingOp.cpp's copies, RoutingLogic.cpp's cases and
//! LobbyModelHost.cpp's reads), replayed here on the Rust routing ops the recorded messages build. The world is the
//! driver's: stand-in shells numbered as made, texts by their UTF-16 units, the same host answers and lines.

use super::*;
use ee_won_net::routeop::RouteOp;
use std::cell::RefCell as TestCell;
use std::collections::BTreeMap;

#[repr(C)]
struct Shell {
    handle: usize,
    rc: i32,
    pad: i32,
    model: *mut EeWonModel,
}

#[derive(Default)]
struct World {
    next: usize,
    lines: Vec<String>,
    /// the live shells by handle, and each text's units
    live: BTreeMap<usize, isize>,
    texts: BTreeMap<usize, String>,
    empty: isize,
}

thread_local! {
    static W: TestCell<World> = TestCell::new(World::default());
}

fn line(s: String) {
    W.with(|w| w.borrow_mut().lines.push(s));
}
fn empty() -> isize {
    W.with(|w| w.borrow().empty)
}
// SAFETY (the shell helpers): live shells of this world
fn sh(p: isize) -> &'static mut Shell {
    unsafe { &mut *(p as *mut Shell) }
}
fn new_shell(kind: Option<EeWonModelKind>, text: Option<String>) -> isize {
    let handle = W.with(|w| {
        let mut w = w.borrow_mut();
        let h = w.next;
        w.next += 1;
        h
    });
    let s = Box::into_raw(Box::new(Shell { handle, rc: 0, pad: 0, model: std::ptr::null_mut() })) as isize;
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.live.insert(handle, s);
        if let Some(t) = text {
            w.texts.insert(handle, t);
        }
    });
    if let Some(k) = kind {
        // SAFETY: a new shell
        sh(s).model = unsafe { ee_won_model_new(s as *mut c_void, k, host, empty() as *mut c_void) };
    }
    s
}
fn held(kind: EeWonModelKind, name: &str) -> isize {
    let s = new_shell(Some(kind), None);
    sh(s).rc += 1;
    line(format!("new {name} h{}", sh(s).handle));
    s
}
fn text(units: String) -> isize {
    let t = new_shell(None, Some(units.clone()));
    sh(t).rc += 1;
    line(format!("text h{} {units}", sh(t).handle));
    t
}
fn rel(p: isize) {
    if p == 0 || p == empty() {
        return;
    }
    let s = sh(p);
    s.rc -= 1;
    if s.rc > 0 {
        return;
    }
    line(format!("delete h{}", s.handle));
    let (handle, model) = (s.handle, s.model);
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.live.remove(&handle);
        w.texts.remove(&handle);
    });
    if !model.is_null() {
        // SAFETY: the shell's model, freed once
        unsafe { ee_won_model_free(model) };
    }
    // SAFETY: the shell's box
    drop(unsafe { Box::from_raw(p as *mut Shell) });
}
fn h(p: isize) -> String {
    if p == 0 {
        "0".into()
    } else if p == empty() {
        "e".into()
    } else {
        format!("h{}", sh(p).handle)
    }
}
fn text_of(p: isize) -> String {
    if p == 0 {
        return "0".into();
    }
    if p == empty() {
        return "e".into();
    }
    W.with(|w| w.borrow().texts.get(&sh(p).handle).cloned().unwrap_or_default())
}
fn units(u: &[u16]) -> String {
    if u.is_empty() {
        return "-".into();
    }
    u.iter().map(|x| format!("{x:04x}")).collect()
}
fn hex(b: &[u8]) -> String {
    if b.is_empty() {
        return "-".into();
    }
    b.iter().map(|x| format!("{x:02x}")).collect()
}
fn summary(game: isize, data: &[u8]) -> isize {
    let answer = (data.len() % 2) as isize;
    line(format!("summary {} {} -> {answer}", h(game), hex(data)));
    answer
}

unsafe extern "C" fn host(op: EeWonModelOp, a: isize, b: isize, c: isize) -> isize {
    use EeWonModelOp as O;
    match op {
        O::AddRef => {
            if a != empty() {
                sh(a).rc += 1;
            }
        }
        O::Release => rel(a),
        O::NewClient => return held(EeWonModelKind::Client, "client"),
        O::NewClientList => return held(EeWonModelKind::ClientList, "list"),
        O::NewGroup => return held(EeWonModelKind::Group, "group"),
        O::NameIgnored => {
            let answer = ((text_of(a).len() / 4) % 2) as isize;
            line(format!("ignored? {} -> {answer}", h(a)));
            return answer;
        }
        O::CdKeyBlocked => {
            let k = a as c_ulong;
            let answer = k.is_multiple_of(3) as isize;
            line(format!("blocked? {k} -> {answer}"));
            return answer;
        }
        O::TextOfUnits => {
            // SAFETY: the op's units
            let u = unsafe { std::slice::from_raw_parts(a as *const u16, b as usize) };
            return text(units(u));
        }
        O::GameReadSummaryBytes => {
            // SAFETY: the op's bytes
            let d = unsafe { std::slice::from_raw_parts(b as *const u8, c as usize) };
            return summary(a, d);
        }
        _ => line(format!("model op {}", op as u32)),
    }
    0
}

fn get(s: isize, f: EeWonModelField) -> isize {
    // SAFETY: a live model
    unsafe { ee_won_model_get(sh(s).model, f) }
}
fn call(s: isize, k: EeWonModelCall, a: isize) -> isize {
    // SAFETY: a live model, the call's argument
    unsafe { ee_won_model_call(sh(s).model, k, a, 0, 0, empty()) }
}

fn dump(server: isize) {
    use EeWonModelField as F;
    let clients = get(server, F::ServerClients);
    let n = call(clients, EeWonModelCall::ListCount, 0);
    line(format!("D clients {n}"));
    for i in 0..n {
        let c = call(clients, EeWonModelCall::ListAt, i);
        let u = |f| get(c, f) as c_ulong;
        line(format!(
            "D c {} {} id={} name={} flags={} won={} cd={} ign={} blk={} chat={} game={} cm={} gm={} cc={} gc={}",
            get(c, F::ClientId),
            h(c),
            get(c, F::ClientId),
            text_of(get(c, F::ClientName)),
            u(F::ClientFlags),
            u(F::ClientWonUserId),
            u(F::ClientCdKeyId),
            get(c, F::ClientIgnored),
            get(c, F::ClientBlocked),
            get(c, F::ClientChatGroupId),
            get(c, F::ClientGameGroupId),
            get(c, F::ClientChatMemberFlags),
            get(c, F::ClientGameMemberFlags),
            get(c, F::ClientChatCaptain),
            get(c, F::ClientGameCaptain)
        ));
    }
    let g = call(server, EeWonModelCall::ServerGroupCount, 0);
    line(format!("D groups {g}"));
    for i in 0..g {
        let r = call(server, EeWonModelCall::ServerGroupAt, i);
        let m = get(r, F::GroupClients);
        let k = call(m, EeWonModelCall::ListCount, 0);
        let members: String = (0..k).map(|j| format!(" {}", h(call(m, EeWonModelCall::ListAt, j)))).collect();
        line(format!(
            "D g {} id={} name={} num={} flags={} cap={} max={} members{k}:{members}",
            h(r),
            get(r, F::GroupId),
            text_of(get(r, F::GroupName)),
            get(r, F::GroupNumClients) as c_ulong,
            get(r, F::GroupFlags) as c_ulong,
            get(r, F::GroupCaptainId),
            get(r, F::GroupMaxClients) as c_ulong
        ));
    }
    line(format!("D pop chat={} total={}", get(server, F::ServerNumChat), get(server, F::ServerNumClients)));
    let counts: String =
        W.with(|w| w.borrow().live.iter().map(|(hd, s)| format!(" h{hd}={}", sh(*s).rc)).collect::<String>());
    line(format!("D rc{counts}"));
}

fn bytes_of(hexs: &str) -> Vec<u8> {
    if hexs == "-" {
        return Vec::new();
    }
    (0..hexs.len() / 2).map(|i| u8::from_str_radix(&hexs[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn step(server: isize, game: isize, kind: i32, op: &RouteOp, flag: isize) {
    use crate::routelogic::EeWonRouteLogicOp as O;
    const OP: isize = 0x0700;
    let ro = |p: isize| if p == OP { Some(op) } else { None };
    let no_list = |_: isize| None;
    let run = |o: O, a: isize, b: isize, c: isize| route_record_op_in(o, a, b, c, &ro, &no_list).expect("answered");
    match kind {
        0 => line(format!("=> {}", h(run(O::ServerAddGroup, server, OP, 0)))),
        1 => line(format!("=> {}", h(run(O::ServerAddClient, server, OP, 0)))),
        2 => {
            run(O::ServerSyncClients, server, OP, 0);
        }
        3 => {
            run(O::ServerSyncGroups, server, OP, flag);
        }
        4 => {
            let mut count: isize = 0;
            let list = run(O::RDataObjects, OP, &mut count as *mut isize as isize, 0);
            line(format!("objects {count}"));
            for i in 0..count {
                line(format!("link {}", run(O::DataObjectLink, list, i, 0)));
                line(format!("=> {}", run(O::GameReadDataObject, game, list, i)));
            }
            run(O::ListFree, list, 1, 0);
        }
        5 => line(format!("=> {}", run(O::GameReadSummary, game, OP, 0))),
        _ => {
            for b in &op.bad_users {
                line(format!(
                    "bad name={} id={} exp={} mod={} modname={} comment={}",
                    units(&b.name),
                    b.won_id,
                    b.expiration,
                    b.moderator_id,
                    units(&b.moderator_name),
                    units(&b.comment)
                ));
            }
        }
    }
    dump(server);
}

/// One recorded sequence replayed: its lines as this world makes them.
fn replay(golden: &[&str]) -> Vec<String> {
    W.with(|w| *w.borrow_mut() = World { next: 0, ..World::default() });
    let e = new_shell(None, Some("e".into()));
    sh(e).rc = 1_000_000;
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.empty = e;
        w.live.remove(&0);
        w.next = 1;
    });
    line(golden[0].to_string());
    let server = held(EeWonModelKind::Server, "server");
    let game = held(EeWonModelKind::Game, "game");
    for l in golden.iter().filter(|l| l.starts_with("step ")) {
        line(l.to_string());
        let f: Vec<&str> = l.split(' ').collect();
        let (kind, ty, flag) = (f[1].parse().unwrap(), f[2].parse().unwrap(), f[3].parse().unwrap());
        let msg = bytes_of(f[4]);
        let mut op = RouteOp::new(ty);
        op.reply(msg[2], &msg, 3, true);
        step(server, game, kind, &op, flag);
    }
    rel(game);
    rel(server);
    let left = W.with(|w| w.borrow().live.len());
    line(format!("left {left}"));
    W.with(|w| std::mem::take(&mut w.borrow_mut().lines))
}

#[test]
fn the_routing_records_read_as_the_original_read_them() {
    let golden = include_str!("golden_routerec.txt");
    let lines: Vec<&str> = golden.lines().collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    assert!(starts.len() >= 40, "the golden's sequences");
    starts.push(lines.len());
    for w in starts.windows(2) {
        let want = &lines[w[0]..w[1]];
        let got = replay(want);
        for (i, (a, b)) in want.iter().zip(got.iter()).enumerate() {
            assert_eq!(a, b, "{} line {i}", want[0]);
        }
        assert_eq!(want.len(), got.len(), "{}", want[0]);
    }
}
