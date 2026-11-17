//! The replay of `golden_modelobj.txt` (`won_modelobj_test --print`): each sequence's steps run on the Rust model
//! over stand-in shells and a host that answer as the test's; the trace must be the recorded one.

use super::*;
use std::collections::BTreeMap;
use std::fmt::Write as _;

#[repr(C)]
struct Ref {
    vt: isize,
    rc: i32,
    handle: i32,
    model: *mut EeWonModel,
    kind: i32,
    value: i32,
}

const TEXT: i32 = 100;
const CONN: i32 = 50;

struct CRec {
    name: i32,
    flags: isize,
    id: u16,
    user: isize,
    key: isize,
}
struct GRec {
    name: i32,
    id: u16,
    count: u16,
    flags: isize,
    captain: u16,
    max: u16,
    members: Vec<(u16, u8)>,
}

#[derive(Default)]
struct World {
    trace: Vec<String>,
    objs: BTreeMap<i32, *mut Ref>,
    next_obj: i32,
    next_text: i32,
    empty: *mut Ref,
    friends: Vec<i32>,
}

thread_local! {
    static W: std::cell::RefCell<World> = std::cell::RefCell::new(World::default());
}

fn line(s: String) {
    W.with(|w| w.borrow_mut().trace.push(s));
}
fn empty() -> isize {
    W.with(|w| w.borrow().empty as isize)
}

fn new_ref(kind: i32, value: i32) -> *mut Ref {
    let handle = W.with(|w| {
        let mut w = w.borrow_mut();
        let h = if kind == TEXT { w.next_text } else { w.next_obj };
        if kind == TEXT {
            w.next_text += 1;
        } else {
            w.next_obj += 1;
        }
        h
    });
    let r = Box::into_raw(Box::new(Ref { vt: 0, rc: 0, handle, model: std::ptr::null_mut(), kind, value }));
    if kind != TEXT {
        W.with(|w| w.borrow_mut().objs.insert(handle, r));
    }
    if kind != TEXT && kind != CONN {
        // SAFETY: a new shell
        let k: EeWonModelKind = unsafe { std::mem::transmute(kind as u32) };
        unsafe { (*r).model = ee_won_model_new(r as *mut c_void, k, host, empty() as *mut c_void) };
    }
    r
}

fn add_ref(r: *mut Ref) {
    // SAFETY: a live shell
    unsafe { (*r).rc += 1 };
}
fn release(r: *mut Ref) {
    // SAFETY: a live shell
    unsafe {
        (*r).rc -= 1;
        if (*r).rc <= 0 {
            let (kind, handle, model) = ((*r).kind, (*r).handle, (*r).model);
            line(format!("delete {}{handle}", if kind == TEXT { "t" } else { "h" }));
            if kind != TEXT {
                W.with(|w| w.borrow_mut().objs.remove(&handle));
            }
            if !model.is_null() {
                ee_won_model_free(model);
            }
            drop(Box::from_raw(r));
        }
    }
}
fn handle(p: isize) -> i32 {
    if p == 0 {
        -1
    } else {
        // SAFETY: a live shell
        unsafe { (*(p as *mut Ref)).handle }
    }
}
fn model_of(r: *mut Ref) -> *const EeWonModel {
    // SAFETY: a live shell
    unsafe { (*r).model }
}
fn call(r: *mut Ref, c: EeWonModelCall, a: isize, b: isize, cc: isize) -> isize {
    // SAFETY: the call's arguments
    unsafe { ee_won_model_call(model_of(r), c, a, b, cc, empty()) }
}
fn held(r: *mut Ref) -> isize {
    add_ref(r);
    r as isize
}

fn ignored(t: i32) -> bool {
    t % 3 == 0
}
fn blocked(k: c_ulong) -> bool {
    k % 4 == 1
}

unsafe extern "C" fn host(op: EeWonModelOp, a: isize, b: isize, c: isize) -> isize {
    use EeWonModelOp as O;
    let ra = a as *mut Ref;
    let mk = |k: EeWonModelKind| held(new_ref(k as i32, 0));
    // SAFETY (every arm): live shells and records (the model's contract)
    unsafe {
        match op {
            O::AddRef => add_ref(ra),
            O::Release => release(ra),
            O::NewClient => return mk(EeWonModelKind::Client),
            O::NewClientList => return mk(EeWonModelKind::ClientList),
            O::NewGroup => return mk(EeWonModelKind::Group),
            O::NewServer => return mk(EeWonModelKind::Server),
            O::NewGame => {
                let g = new_ref(EeWonModelKind::Game as i32, 0);
                ee_won_model_set((*g).model, EeWonModelField::GameType, a);
                return held(g);
            }
            O::NewPlayer => return mk(EeWonModelKind::Player),
            O::ClientReadRouting => {
                let r = &*(b as *const CRec);
                let v = [r.flags, r.id as isize, r.user, r.key];
                let t = new_ref(TEXT, r.name);
                add_ref(t);
                call(ra, EeWonModelCall::ClientSetRouting, t as isize, v.as_ptr() as isize, 0);
                release(t);
            }
            O::GroupReadRouting => {
                let r = &*(b as *const GRec);
                let v = [r.id as isize, r.count as isize, r.flags, r.captain as isize, r.max as isize];
                let t = new_ref(TEXT, r.name);
                add_ref(t);
                call(ra, EeWonModelCall::GroupSetRouting, t as isize, v.as_ptr() as isize, 0);
                release(t);
                if c != 0 {
                    let ids: Vec<u16> = r.members.iter().map(|m| m.0).collect();
                    let flags: Vec<u8> = r.members.iter().map(|m| m.1).collect();
                    call(ra, EeWonModelCall::GroupSetMembersList, c, 0, 0);
                    call(
                        ra,
                        EeWonModelCall::GroupSetMembers,
                        ids.as_ptr() as isize,
                        flags.as_ptr() as isize,
                        ids.len() as isize,
                    );
                }
            }
            O::NameIgnored => {
                line(format!("op ignored t{}", (*ra).handle));
                return ignored((*ra).value) as isize;
            }
            O::CdKeyBlocked => {
                line(format!("op blocked {}", a as c_ulong));
                return blocked(a as c_ulong) as isize;
            }
            O::FindFriend => {
                line(format!("op findfriend {}", a as c_ulong));
                let (friends, objs) = W.with(|w| (w.borrow().friends.clone(), w.borrow().objs.clone()));
                for f in friends {
                    if let Some(r) = objs.get(&f) {
                        if ee_won_model_get((**r).model, EeWonModelField::FriendUserId) as c_ulong == a as c_ulong {
                            return *r as isize;
                        }
                    }
                }
                return 0;
            }
            O::PlayerIsTeammate => {
                line(format!("op teammate h{} h{}", (*ra).handle, handle(b)));
                return (((*ra).handle + handle(b)) % 2 == 0) as isize;
            }
            O::GameCopyFromHook => line(format!("op copyhook h{} h{}", (*ra).handle, handle(b))),
            O::GameHostHook => line(format!("op hosthook h{} h{}", (*ra).handle, handle(b))),
            O::GamePlayerLeftHook => line(format!("op lefthook h{} h{} h{}", (*ra).handle, handle(b), handle(c))),
            O::GameJoinStatusHook => {
                line(format!("op joinhook h{}", (*ra).handle));
                return if (*ra).handle % 3 == 0 { 7 } else { 0 };
            }
            O::GameCanStartHook => {
                line(format!("op starthook h{}", (*ra).handle));
                return ((*ra).handle % 2 != 0) as isize;
            }
            O::ConnectionClose => line(format!("op close h{}", (*ra).handle)),
            O::TextOfUnits | O::GameReadSummaryBytes => line(format!("op {op:?}")),
        }
    }
    0
}

struct Step {
    what: i32,
    kind: i32,
    k: i32,
    call: i32,
    a: i64,
    b: i64,
    c: i64,
    arg_kind: i32,
    arg_k: i32,
    recs: Vec<usize>,
}
const NEW: i32 = 0;
const RELEASE: i32 = 1;
const CALL: i32 = 2;
const GET: i32 = 3;
const SET: i32 = 4;
const SET_TEXT: i32 = 5;
const SET_OBJ: i32 = 6;
const SET_ADDR: i32 = 7;
const ITER: i32 = 8;

fn nth(kind: i32, k: i32) -> i32 {
    W.with(|w| {
        let w = w.borrow();
        let mut n = 0;
        for (h, r) in &w.objs {
            // SAFETY: a live shell
            if unsafe { (**r).kind } == kind {
                if n == k {
                    return *h;
                }
                n += 1;
            }
        }
        -1
    })
}
fn obj(h: i32) -> *mut Ref {
    W.with(|w| w.borrow().objs[&h])
}

fn addr6(v: *const EeWonIpAddr) -> String {
    // SAFETY: a live value
    let v = unsafe { &*v };
    format!("a{}.{}", v.port(), v.host() as i32 as c_long)
}

use EeWonModelField as F;
fn fields(kind: i32) -> &'static [F] {
    match kind {
        0 => &[F::PlayerPing, F::PlayerReady],
        1 => &[
            F::ClientName,
            F::ClientId,
            F::ClientFlags,
            F::ClientWonUserId,
            F::ClientCdKeyId,
            F::ClientChatMemberFlags,
            F::ClientGameMemberFlags,
            F::ClientChatGroupId,
            F::ClientGameGroupId,
            F::ClientIgnored,
            F::ClientBlocked,
            F::ClientIsFriend,
            F::ClientGameCaptain,
            F::ClientChatCaptain,
            F::ClientInvited,
            F::ClientPlayer,
        ],
        3 => &[
            F::FriendName,
            F::FriendUserId,
            F::FriendAddress,
            F::FriendLocation,
            F::FriendLastSearch,
            F::FriendFlags,
            F::FriendLastLocate,
            F::FriendSearchStart,
            F::FriendAnonymous,
        ],
        4 => &[
            F::GroupName,
            F::GroupId,
            F::GroupCaptainId,
            F::GroupNumClients,
            F::GroupMaxClients,
            F::GroupFlags,
            F::GroupClients,
        ],
        5 => &[
            F::GameName,
            F::GamePassword,
            F::GameHasPassword,
            F::GameInviteOnly,
            F::GameAskToJoin,
            F::GameInProgress,
            F::GameInvited,
            F::GameIsHost,
            F::GameShowDetails,
            F::GameHaveDetails,
            F::GamePingReplied,
            F::GameLastSeen,
            F::GameFiltered,
            F::GameNumPlayers,
            F::GameOpenSlots,
            F::GameType,
            F::GameSkill,
            F::GameClients,
            F::GameAddress,
            F::GameCaptainId,
            F::GameGroupId,
            F::GamePing,
        ],
        6 => &[F::GameListType],
        7 => &[
            F::ServerName,
            F::ServerAddress,
            F::ServerNumChat,
            F::ServerNumClients,
            F::ServerLastPing,
            F::ServerQuerying,
            F::ServerClients,
            F::ServerConnection,
            F::ServerMyClientId,
        ],
        _ => &[],
    }
}
fn show(r: *mut Ref, f: F) -> String {
    // SAFETY: a live model
    let v = unsafe { ee_won_model_get(model_of(r), f) };
    match f {
        F::ClientName
        | F::FriendName
        | F::FriendLocation
        | F::GroupName
        | F::GameName
        | F::GamePassword
        | F::ServerName => {
            format!("t{}", handle(v))
        }
        F::ClientPlayer | F::GroupClients | F::GameClients | F::ServerClients | F::ServerConnection => {
            format!("h{}", handle(v))
        }
        F::FriendAddress | F::GameAddress | F::ServerAddress => addr6(v as *const EeWonIpAddr),
        _ => format!("{}", v as i64),
    }
}
fn dump() {
    let hs: Vec<i32> = W.with(|w| w.borrow().objs.keys().copied().collect());
    for h in hs {
        let r = obj(h);
        // SAFETY: a live shell
        let (kind, rc) = unsafe { ((*r).kind, (*r).rc) };
        let mut l = format!("state h{h} k{kind} rc{rc}");
        if kind < 9 {
            for f in fields(kind) {
                l += " ";
                l += &show(r, *f);
            }
            if kind == 2 || kind == 8 || kind == 6 || kind == 7 {
                l += " [";
                if kind == 2 {
                    for j in 0..call(r, EeWonModelCall::ListCount, 0, 0, 0) {
                        let _ = write!(l, " h{}", handle(call(r, EeWonModelCall::ListAt, j, 0, 0)));
                    }
                }
                if kind == 7 {
                    for j in 0..call(r, EeWonModelCall::ServerGroupCount, 0, 0, 0) {
                        let _ = write!(l, " h{}", handle(call(r, EeWonModelCall::ServerGroupAt, j, 0, 0)));
                    }
                }
                if kind == 8 {
                    for j in 0..call(r, EeWonModelCall::ServerListCount, 0, 0, 0) {
                        let a = call(r, EeWonModelCall::ServerListAddressAt, j, 0, 0) as *const EeWonIpAddr;
                        let _ = write!(l, " h{}@{}", handle(call(r, EeWonModelCall::ServerListAt, j, 0, 0)), addr6(a));
                    }
                }
                l += " ]";
            }
        }
        line(l);
    }
}

const NAMES: [&str; 6] = ["en", "EN", "fr", "De", "de", "it"];

fn run(steps: &[Step], crecs: &[CRec], grecs: &[GRec], addrs: &[EeWonIpAddr]) -> Vec<String> {
    W.with(|w| *w.borrow_mut() = World::default());
    let e = new_ref(TEXT, 0);
    // SAFETY: the immortal empty string
    unsafe { (*e).rc = 1_000_000 };
    W.with(|w| w.borrow_mut().empty = e);
    let mut roots: Vec<i32> = Vec::new();
    let mut cursors = [EeWonModelCursor::default(); 4];
    let mut cursor_of = [-1i32; 4];
    for (n, s) in steps.iter().enumerate() {
        if s.what == NEW {
            let r = new_ref(s.kind, if s.kind == TEXT { s.a as i32 } else { 0 });
            // SAFETY: a live shell
            unsafe { (*r).rc = 1 };
            // SAFETY: the shell's model
            unsafe {
                if s.kind == EeWonModelKind::GameList as i32 {
                    ee_won_model_set((*r).model, F::GameListType, s.a as isize);
                }
                if s.kind == EeWonModelKind::Friend as i32 {
                    let t = new_ref(TEXT, s.b as i32);
                    add_ref(t);
                    ee_won_model_set((*r).model, F::FriendName, t as isize);
                    ee_won_model_set((*r).model, F::FriendUserId, s.a as isize);
                    release(t);
                    W.with(|w| w.borrow_mut().friends.push((*r).handle));
                }
            }
            let h = handle(r as isize);
            roots.push(h);
            line(format!("new h{h} k{}", s.kind));
            continue;
        }
        if s.what == RELEASE {
            for h in roots.drain(..) {
                let live = W.with(|w| w.borrow().objs.get(&h).copied());
                if let Some(r) = live {
                    release(r);
                }
            }
            continue;
        }
        if s.what == GET && s.call < 0 {
            dump();
            continue;
        }
        let h = nth(s.kind, s.k);
        if h < 0 {
            line(format!("skip {n}"));
            continue;
        }
        let ah = if s.arg_kind >= 0 { nth(s.arg_kind, s.arg_k) } else { -1 };
        if s.arg_kind >= 0 && ah < 0 && s.arg_kind != CONN && s.what != SET_OBJ {
            line(format!("skip {n}"));
            continue;
        }
        if s.what == SET_OBJ && ah < 0 && s.call != F::ClientPlayer as i32 && s.call != F::GameClients as i32 {
            line(format!("skip {n}"));
            continue;
        }
        let ch = if s.call == EeWonModelCall::ServerAddClientToGroup as i32 && s.what == CALL {
            nth(EeWonModelKind::Client as i32, s.c as i32)
        } else {
            0
        };
        if ch < 0 {
            line(format!("skip {n}"));
            continue;
        }
        let o = obj(h);
        if s.what == CALL
            && (s.call == EeWonModelCall::GameAllReady as i32 || s.call == EeWonModelCall::GameCanStart as i32)
        {
            // SAFETY: a live model
            if unsafe { ee_won_model_get(model_of(o), F::GameClients) } == 0 {
                line(format!("skip {n}"));
                continue;
            }
        }
        let arg = if ah >= 0 { obj(ah) as isize } else { 0 };
        // SAFETY: a member step's call is a field
        let f = || -> F { unsafe { std::mem::transmute(s.call as u32) } };
        match s.what {
            SET => {
                // SAFETY: a number field
                unsafe { ee_won_model_set(model_of(o), f(), s.a as isize) };
                line(format!("set {}", s.call));
                continue;
            }
            SET_TEXT => {
                let t = new_ref(TEXT, s.a as i32);
                add_ref(t);
                // SAFETY: a text field
                unsafe { ee_won_model_set(model_of(o), f(), t as isize) };
                release(t);
                line(format!("settext {}", s.call));
                continue;
            }
            SET_OBJ => {
                if s.arg_kind == CONN && ah < 0 {
                    line(format!("skip {n}"));
                    continue;
                }
                // SAFETY: an object field
                unsafe { ee_won_model_set(model_of(o), f(), arg) };
                line(format!("setobj {} h{}", s.call, handle(arg)));
                continue;
            }
            SET_ADDR => {
                // SAFETY: an address field, a live value
                unsafe { ee_won_model_set(model_of(o), f(), &addrs[s.a as usize] as *const EeWonIpAddr as isize) };
                line(format!("setaddr {}", s.call));
                continue;
            }
            ITER => {
                let i = s.a as usize;
                if cursor_of[i] != h {
                    cursor_of[i] = h;
                    cursors[i] = EeWonModelCursor::default();
                }
                let p = &mut cursors[i] as *mut EeWonModelCursor as isize;
                let mut c: isize = 0;
                let (mut r, mut out) = (0isize, 0isize);
                match s.call {
                    0 => cursors[i] = EeWonModelCursor::default(),
                    1 => r = call(o, EeWonModelCall::ListIterHasMore, p, 0, 0),
                    2 => r = handle(call(o, EeWonModelCall::ListIterNext, p, 0, 0)) as isize,
                    3 => r = call(o, EeWonModelCall::ListSetHasMore, p, 0, 0),
                    _ => {
                        r = handle(call(o, EeWonModelCall::ListSetNext, p, &mut c as *mut isize as isize, 0)) as isize;
                        out = handle(c) as isize;
                    }
                }
                line(format!("iter {} {i} = {r} {out}", s.call));
                continue;
            }
            _ => {}
        }
        // SAFETY: a call of the list
        let mc: EeWonModelCall = unsafe { std::mem::transmute(s.call as u32) };
        let groups = mc == EeWonModelCall::ServerSyncGroups || mc == EeWonModelCall::ServerAddGroup;
        let mut ids: Vec<u16> = Vec::new();
        let mut infos: Vec<isize> = Vec::new();
        for &i in &s.recs {
            if groups {
                ids.push(grecs[i].id);
                infos.push(&grecs[i] as *const GRec as isize);
            } else {
                ids.push(crecs[i].id);
                infos.push(&crecs[i] as *const CRec as isize);
            }
        }
        let (pid, pinf) = (ids.as_ptr() as isize, infos.as_ptr() as isize);
        let ad = |i: i64| &addrs[i as usize] as *const EeWonIpAddr as isize;
        let mut old: isize = 0;
        let mut existed = false;
        let (mut ptr, mut hld) = (false, false);
        let mut out: isize = 0;
        use EeWonModelCall as C;
        let mut r = match mc {
            C::ListSync | C::ServerSyncClients => call(o, mc, pid, pinf, ids.len() as isize),
            C::ServerSyncGroups => call(o, mc, pid, pinf, (ids.len() as u64 | ((s.a & 1) as u64) << 32) as isize),
            C::ServerAddClient => {
                ptr = true;
                call(o, mc, &crecs[s.recs[0]] as *const CRec as isize, 0, 0)
            }
            C::ServerAddGroup => {
                ptr = true;
                call(o, mc, &grecs[s.recs[0]] as *const GRec as isize, 0, 0)
            }
            C::GroupSetCaptain => {
                ptr = true;
                let r = call(o, mc, s.a as isize, &mut old as *mut isize as isize, 0);
                out = handle(old) as isize;
                r
            }
            C::GameListGetOrCreateId => {
                ptr = true;
                let r = call(o, mc, s.a as isize, &mut existed as *mut bool as isize, 0);
                out = existed as isize;
                r
            }
            C::GameListGetOrCreateAddr => {
                ptr = true;
                let r = call(o, mc, ad(s.a), &mut existed as *mut bool as isize, 0);
                out = existed as isize;
                r
            }
            C::GameListGetAddr | C::ServerListFind | C::ServerListInsert => {
                ptr = true;
                call(o, mc, ad(s.a), 0, 0)
            }
            C::GameListRemoveAddr => {
                hld = true;
                call(o, mc, ad(s.a), 0, 0)
            }
            C::FriendSetLocation => {
                let t = new_ref(TEXT, s.b as i32);
                add_ref(t);
                let r = call(o, mc, ad(s.a), t as isize, s.c as isize);
                release(t);
                r
            }
            C::ServerHasAddress | C::ServerAddressAdd => {
                let name = NAMES[s.a as usize];
                call(o, mc, name.as_ptr() as isize, name.len() as isize, 0)
            }
            C::GameStoreSummary => {
                let (a, b, c) = (s.a, s.b, s.c);
                let mut sm = staging::EeWonGameSummary {
                    game_type: 0,
                    address: [0; 6],
                    product_id: 0,
                    in_progress: b & 1 != 0,
                    has_password: (b >> 1) & 1 != 0,
                    invite_only: (b >> 2) & 1 != 0,
                    ask_to_join: (b >> 3) & 1 != 0,
                    skill: ((b >> 4) & 7) as i32,
                    players: (c & 7) as i32,
                    open_slots: ((c >> 3) & 7) as i32,
                    ok: false,
                    read: a as u32,
                    rest: 0,
                };
                for i in 0..6 {
                    sm.address[i] = (c >> (6 + i)) as u8;
                }
                let t = if sm.read & 4 != 0 { new_ref(TEXT, (c % 50) as i32) } else { empty() as *mut Ref };
                add_ref(t);
                let r = call(o, mc, &sm as *const _ as isize, t as isize, 0);
                release(t);
                r
            }
            C::ServerAddClientToGroup => call(o, mc, obj(ch) as isize, s.b as isize, arg),
            C::ServerRemoveClientFromGroup => {
                hld = true;
                call(o, mc, s.a as isize, arg, 0)
            }
            _ => {
                let obj_arg = s.arg_kind >= 0;
                let r = call(
                    o,
                    mc,
                    if obj_arg { arg } else { s.a as isize },
                    if obj_arg { s.a as isize } else { s.b as isize },
                    s.c as isize,
                );
                match mc {
                    C::ListFind
                    | C::ListFindByWonId
                    | C::ListAt
                    | C::GroupCaptain
                    | C::GroupFind
                    | C::GameFindClient
                    | C::GameListGetId
                    | C::GameListNext
                    | C::ServerGetClient
                    | C::ServerFindByWonId
                    | C::ServerMyClient
                    | C::ServerGroup
                    | C::ServerGroupAt
                    | C::ServerListAt => ptr = true,
                    C::ListRemove
                    | C::GroupRemove
                    | C::GameListRemoveId
                    | C::ServerRemoveGroup
                    | C::ServerRemoveClient
                    | C::ServerRemoveClientFromGroup => hld = true,
                    _ => {}
                }
                r
            }
        };
        let mut fs = String::new();
        if ptr || hld {
            let x = handle(r) as isize;
            if hld && r != 0 {
                // a removed object's members (what the caller gets)
                // SAFETY: a live shell
                let kind = unsafe { (*(r as *mut Ref)).kind };
                for f in fields(kind) {
                    fs += " ";
                    fs += &show(r as *mut Ref, *f);
                }
                release(r as *mut Ref);
            }
            r = x;
        }
        line(format!("call {} {h} = {r} {out}{fs}", s.call));
    }
    let left: Vec<(i32, i32)> = W.with(|w| w.borrow().objs.iter().map(|(h, r)| (*h, unsafe { (**r).rc })).collect());
    for (h, rc) in left {
        line(format!("left h{h} rc{rc}"));
    }
    let texts = W.with(|w| w.borrow().next_text);
    line(format!("texts {texts}"));
    W.with(|w| std::mem::take(&mut w.borrow_mut().trace))
}

pub(super) fn replay() {
    let text = include_str!("golden_modelobj.txt");
    let mut crecs = Vec::new();
    let mut grecs = Vec::new();
    let mut addrs = Vec::new();
    for i in 0..6u32 {
        let mut a = EeWonIpAddr::default();
        a.set_host_order(0x0a00_0000 + i % 3, (80 + i / 3) as u16);
        addrs.push(a);
    }
    let mut seqs: Vec<(Vec<Step>, Vec<String>)> = Vec::new();
    for l in text.lines() {
        let w: Vec<&str> = l.split(' ').collect();
        let num = |i: usize| w[i].parse::<i64>().unwrap();
        match w[0] {
            "crec" => crecs.push(CRec {
                name: num(1) as i32,
                flags: num(2) as isize,
                id: num(3) as u16,
                user: num(4) as isize,
                key: num(5) as isize,
            }),
            "grec" => grecs.push(GRec {
                name: num(1) as i32,
                id: num(2) as u16,
                count: num(3) as u16,
                flags: num(4) as isize,
                captain: num(5) as u16,
                max: num(6) as u16,
                members: w[7..]
                    .iter()
                    .map(|m| {
                        let (x, y) = m.split_once(':').unwrap();
                        (x.parse().unwrap(), y.parse().unwrap())
                    })
                    .collect(),
            }),
            "seq" => seqs.push((Vec::new(), Vec::new())),
            "step" => seqs.last_mut().unwrap().0.push(Step {
                what: num(1) as i32,
                kind: num(2) as i32,
                k: num(3) as i32,
                call: num(4) as i32,
                a: num(5),
                b: num(6),
                c: num(7),
                arg_kind: num(8) as i32,
                arg_k: num(9) as i32,
                recs: w[10..].iter().map(|x| x.parse().unwrap()).collect(),
            }),
            _ => seqs.last_mut().unwrap().1.push(l.to_string()),
        }
    }
    assert!(seqs.len() >= 60, "{} sequences", seqs.len());
    for (n, (steps, want)) in seqs.iter().enumerate() {
        let got = run(steps, &crecs, &grecs, &addrs);
        for (i, (g, w)) in got.iter().zip(want).enumerate() {
            assert_eq!(g, w, "sequence {n} line {i}");
        }
        assert_eq!(got.len(), want.len(), "sequence {n}");
    }
}
