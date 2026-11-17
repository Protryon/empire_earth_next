use super::*;
use std::cell::RefCell as TestCell;

/// A stand-in C++ shell: the vtable slot, the count, the model at `SHELL_MODEL`.
#[repr(C)]
struct Shell {
    vt: usize,
    rc: i32,
    pad: i32,
    model: *mut EeWonModel,
}

thread_local! {
    static LOG: TestCell<Vec<String>> = const { TestCell::new(Vec::new()) };
    static EMPTY: TestCell<isize> = const { TestCell::new(0) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}

fn shell(kind: Option<EeWonModelKind>) -> isize {
    let s = Box::into_raw(Box::new(Shell { vt: 0, rc: 0, pad: 0, model: std::ptr::null_mut() }));
    if let Some(k) = kind {
        // SAFETY: a new shell
        unsafe { (*s).model = ee_won_model_new(s as *mut c_void, k, host, empty() as *mut c_void) };
    }
    s as isize
}

fn empty() -> isize {
    EMPTY.with(|e| {
        if *e.borrow() == 0 {
            let s = shell(None);
            // SAFETY: a live shell; the empty string lives for the test
            unsafe { (*(s as *mut Shell)).rc = 1000 };
            *e.borrow_mut() = s;
        }
        *e.borrow()
    })
}

fn rc(p: isize) -> i32 {
    // SAFETY: a live shell
    unsafe { (*(p as *mut Shell)).rc }
}

unsafe extern "C" fn host(op: EeWonModelOp, a: isize, b: isize, _c: isize) -> isize {
    use EeWonModelOp as O;
    let held = |k| {
        let s = shell(Some(k));
        // SAFETY: a live shell
        unsafe { (*(s as *mut Shell)).rc = 1 };
        s
    };
    match op {
        // SAFETY: live shells (the model's contract)
        O::AddRef => unsafe { (*(a as *mut Shell)).rc += 1 },
        O::Release => {
            // SAFETY: as AddRef; a shell at 0 is deleted as the C++'s Release deletes
            unsafe {
                let s = a as *mut Shell;
                (*s).rc -= 1;
                if (*s).rc <= 0 {
                    log(format!("delete {}", (*s).vt));
                    ee_won_model_free((*s).model);
                    drop(Box::from_raw(s));
                }
            }
        }
        O::NewClient => return held(EeWonModelKind::Client),
        O::NewClientList => return held(EeWonModelKind::ClientList),
        O::NewGroup => return held(EeWonModelKind::Group),
        O::NewServer => return held(EeWonModelKind::Server),
        O::NewGame => {
            let g = held(EeWonModelKind::Game);
            // SAFETY: a live model
            unsafe { ee_won_model_set(model(g), EeWonModelField::GameType, a) };
            return g;
        }
        O::NewPlayer => return held(EeWonModelKind::Player),
        O::ClientReadRouting => {
            // the stand-in record is the id
            let v = [0isize, b, b * 10, b * 100];
            // SAFETY: a live model
            unsafe {
                ee_won_model_call(model(a), EeWonModelCall::ClientSetRouting, empty(), v.as_ptr() as isize, 0, 0)
            };
        }
        O::PlayerIsTeammate => return 1,
        O::GameJoinStatusHook => return 9,
        O::GameCanStartHook => return 1,
        _ => log(format!("op {op:?} {a} {b}")),
    }
    0
}

fn model(s: isize) -> *const EeWonModel {
    // SAFETY: a live shell
    unsafe { (*(s as *mut Shell)).model }
}
fn call(s: isize, c: EeWonModelCall, a: isize, b: isize, cc: isize) -> isize {
    // SAFETY: a live model, the call's arguments
    unsafe { ee_won_model_call(model(s), c, a, b, cc, empty()) }
}
fn get(s: isize, f: EeWonModelField) -> isize {
    // SAFETY: a live model
    unsafe { ee_won_model_get(model(s), f) }
}
fn set(s: isize, f: EeWonModelField, v: isize) {
    // SAFETY: a live model
    unsafe { ee_won_model_set(model(s), f, v) }
}
fn held(kind: EeWonModelKind) -> isize {
    let s = shell(Some(kind));
    // SAFETY: a live shell
    unsafe { (*(s as *mut Shell)).rc = 1 };
    s
}

#[test]
fn a_new_client_is_in_no_room() {
    let c = held(EeWonModelKind::Client);
    assert_eq!(get(c, EeWonModelField::ClientId), 0xffff);
    assert_eq!(call(c, EeWonModelCall::ClientInChat, 0, 0, 0), 0);
    assert_eq!(call(c, EeWonModelCall::ClientPing, 0, 0, 0), 0xfffe);
    call(c, EeWonModelCall::ClientSetGroupId, 7, 1, 0);
    call(c, EeWonModelCall::ClientSetCaptain, 1, 1, 0);
    assert_eq!(call(c, EeWonModelCall::ClientInGame, 0, 0, 0), 1);
    call(c, EeWonModelCall::ClientClearGroup, 7, 1, 0);
    assert_eq!(call(c, EeWonModelCall::ClientIsCaptain, 1, 0, 0), 0);
    assert_eq!(get(c, EeWonModelField::ClientGameGroupId), 0xffff);
}

#[test]
fn the_lists_hold_one_reference_each() {
    let l = held(EeWonModelKind::ClientList);
    let c = held(EeWonModelKind::Client);
    set(c, EeWonModelField::ClientId, 5);
    call(l, EeWonModelCall::ListAdd, c, 0, 0);
    assert_eq!(rc(c), 2);
    assert_eq!(call(l, EeWonModelCall::ListFind, 5, 0, 0), c);
    let r = call(l, EeWonModelCall::ListRemove, 5, 0, 0);
    assert_eq!(r, c);
    assert_eq!(rc(c), 2, "the removed client is held for the caller");
    assert_eq!(call(l, EeWonModelCall::ListCount, 0, 0, 0), 0);
}

#[test]
fn sync_creates_refreshes_and_drops() {
    let l = held(EeWonModelKind::ClientList);
    call(l, EeWonModelCall::ListSync, [1u16, 3, 5].as_ptr() as isize, [1isize, 3, 5].as_ptr() as isize, 3);
    assert_eq!(call(l, EeWonModelCall::ListCount, 0, 0, 0), 3);
    call(l, EeWonModelCall::ListSync, [2u16, 3].as_ptr() as isize, [2isize, 3].as_ptr() as isize, 2);
    assert_eq!(call(l, EeWonModelCall::ListCount, 0, 0, 0), 2);
    let first = call(l, EeWonModelCall::ListAt, 0, 0, 0);
    assert_eq!(get(first, EeWonModelField::ClientId), 2);
    assert_eq!(get(first, EeWonModelField::ClientWonUserId), 20);
    assert_eq!(rc(first), 1);
}

#[test]
fn the_cursor_walks_past_the_last_client_returned() {
    let l = held(EeWonModelKind::ClientList);
    call(l, EeWonModelCall::ListSync, [1u16, 4].as_ptr() as isize, [1isize, 4].as_ptr() as isize, 2);
    let mut cur = EeWonModelCursor::default();
    let p = &mut cur as *mut EeWonModelCursor as isize;
    let a = call(l, EeWonModelCall::ListIterNext, p, 0, 0);
    assert_eq!(get(a, EeWonModelField::ClientId), 1);
    // a client inserted before the cursor's place is not visited
    call(l, EeWonModelCall::ListSync, [0u16, 1, 4].as_ptr() as isize, [0isize, 1, 4].as_ptr() as isize, 3);
    let b = call(l, EeWonModelCall::ListIterNext, p, 0, 0);
    assert_eq!(get(b, EeWonModelField::ClientId), 4);
    assert_eq!(call(l, EeWonModelCall::ListIterHasMore, p, 0, 0), 0);
}

#[test]
fn a_group_moves_its_captaincy() {
    let g = held(EeWonModelKind::Group);
    let a = held(EeWonModelKind::Client);
    let b = held(EeWonModelKind::Client);
    set(a, EeWonModelField::ClientId, 1);
    set(b, EeWonModelField::ClientId, 2);
    set(g, EeWonModelField::GroupCaptainId, 1);
    call(g, EeWonModelCall::GroupAdd, a, 0, 0);
    call(g, EeWonModelCall::GroupAdd, b, 0, 0);
    assert_eq!(call(a, EeWonModelCall::ClientIsCaptain, 1, 0, 0), 1, "flags 0: a game room");
    let mut old = 0isize;
    let new = call(g, EeWonModelCall::GroupSetCaptain, 2, &mut old as *mut isize as isize, 0);
    assert_eq!((new, old), (b, a));
    assert_eq!(call(a, EeWonModelCall::ClientIsCaptain, 1, 0, 0), 0);
    assert_eq!(get(g, EeWonModelField::GroupNumClients), 2);
    let r = call(g, EeWonModelCall::GroupRemove, 1, 0, 0);
    assert_eq!(r, a);
    assert_eq!(get(a, EeWonModelField::ClientGameGroupId), 0xffff);
}

#[test]
fn a_game_list_by_address_drops_stale_games() {
    let l = held(EeWonModelKind::GameList);
    set(l, EeWonModelField::GameListType, 1);
    let mut addr = EeWonIpAddr::default();
    addr.set_host_order(0x0a000001, 80);
    let mut existed = true;
    let g = call(
        l,
        EeWonModelCall::GameListGetOrCreateAddr,
        &addr as *const _ as isize,
        &mut existed as *mut bool as isize,
        0,
    );
    assert!(!existed);
    assert_eq!(rc(g), 1);
    assert_eq!(get(g, EeWonModelField::GameType), 1);
    set(g, EeWonModelField::GameLastSeen, 100);
    set(g, EeWonModelField::GamePing, 5);
    assert_eq!(call(l, EeWonModelCall::GameListRemoveStale, 30, 120, 0), 0);
    assert_eq!(call(l, EeWonModelCall::GameListHasMore, 0, 0, 0), 1);
    assert_eq!(call(l, EeWonModelCall::GameListNext, 0, 0, 0), g);
    assert_eq!(call(l, EeWonModelCall::GameListHasMore, 0, 0, 0), 0);
    assert_eq!(call(l, EeWonModelCall::GameListRemoveStale, 30, 131, 0), 1);
}

#[test]
fn a_game_asks_its_hook_last() {
    let g = held(EeWonModelKind::Game);
    assert_eq!(call(g, EeWonModelCall::GameJoinStatus, 0, 0, 0), 9);
    set(g, EeWonModelField::GameNumPlayers, 2);
    set(g, EeWonModelField::GameOpenSlots, 2);
    assert_eq!(call(g, EeWonModelCall::GameJoinStatus, 0, 0, 0), 2, "full");
}

#[test]
fn freeing_gives_the_references_back() {
    let g = held(EeWonModelKind::Group);
    let list = get(g, EeWonModelField::GroupClients);
    let c = held(EeWonModelKind::Client);
    call(g, EeWonModelCall::GroupAdd, c, 0, 0);
    assert_eq!(rc(c), 2);
    // SAFETY: the group's last reference
    unsafe { host(EeWonModelOp::Release, g, 0, 0) };
    let _ = list;
    assert_eq!(rc(c), 1, "the group's list, freed with it, released the client");
}

#[test]
fn the_golden_replays() {
    super::golden::replay();
}
