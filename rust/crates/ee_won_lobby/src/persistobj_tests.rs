use super::*;
use std::cell::RefCell;

// A stand-in host: friends are numbered records with reference counts, the resources fixed texts, the clock a
// counter. (The files are the driver's: tests/won/won_persistobj_test.cpp.)
#[derive(Default)]
struct World {
    friends: Vec<(Vec<u32>, i64, i32)>,
    res: Vec<(isize, Vec<u32>)>,
    muted: Vec<bool>,
    append: bool,
    now: isize,
}
thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World::default());
}
unsafe extern "C" fn host(_: *mut c_void, op: EeWonPersistOp, a: isize, b: isize, c: isize) -> isize {
    WORLD.with(|w| {
        let mut w = w.borrow_mut();
        match op {
            EeWonPersistOp::FriendNew => {
                let name = unsafe { std::slice::from_raw_parts(a as *const u32, b as usize) }.to_vec();
                w.friends.push((name, c as i64, 1));
                w.friends.len() as isize
            }
            EeWonPersistOp::FriendRef => {
                w.friends[a as usize - 1].2 += if b == 1 { 1 } else { -1 };
                0
            }
            EeWonPersistOp::FriendName => {
                let f = &w.friends[a as usize - 1].0;
                unsafe { *(b as *mut usize) = f.len() };
                f.as_ptr() as isize
            }
            EeWonPersistOp::ResText => {
                let t = &w.res.iter().find(|(r, _)| *r == a).unwrap().1;
                unsafe { *(b as *mut usize) = t.len() };
                t.as_ptr() as isize
            }
            EeWonPersistOp::MuteClick => {
                w.muted.push(a != 0);
                0
            }
            EeWonPersistOp::AppendLanNumber => isize::from(w.append),
            EeWonPersistOp::Now => {
                w.now += 1;
                w.now
            }
        }
    })
}
fn u(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}
fn make(append: bool) -> Box<EeWonPersist> {
    WORLD.with(|w| {
        let mut w = w.borrow_mut();
        *w = World::default();
        w.append = append;
        w.res = vec![
            (EeWonLobbyRes::LobbyOptions_DefaultName_String as isize, u("Player")),
            (EeWonLobbyRes::LobbyGlobal_QuickPlay_String as isize, u("Quick Play")),
            (EeWonLobbyRes::LobbyGlobal_Lan_String as isize, u("LAN")),
        ];
    });
    EeWonPersist::new(
        EeWonPersistHost { ctx: std::ptr::null_mut(), op: Some(host) },
        4,
        b"\x5f\x65\xfa\x22\x01\x02\x03\x04",
    )
}
fn call(p: &mut EeWonPersist, c: EeWonPersistCall, t1: &str, t2: &str, a: i64, b: i64) -> isize {
    let mut n = 0;
    p.call(c, u(t1), u(t2), a, b, &mut n)
}
fn text(p: &mut EeWonPersist, c: EeWonPersistCall, t1: &str, a: i64) -> String {
    let mut n = 0;
    let r = p.call(c, u(t1), Vec::new(), a, 0, &mut n);
    let s = unsafe { std::slice::from_raw_parts(r as *const u32, n) };
    s.iter().map(|&c| char::from_u32(c).unwrap()).collect()
}
use EeWonPersistCall as C;

#[test]
fn defaults_and_no_data() {
    let mut p = make(false);
    assert_eq!(call(&mut p, C::LanBroadcastPort, "", "", 0, 0), 8585);
    assert_eq!(text(&mut p, C::Text, "", EeWonPrefsText::LanName as i64), "Player");
    assert_eq!(call(&mut p, C::Flag, "", "", F::UserDirty as i64, 0), 0);
    assert_eq!(call(&mut p, C::Flag, "", "", F::SavedLobbyMusic as i64, 0), 1);
    let mut n = 7;
    assert_eq!(no_data(C::ConnectionSpeed, &mut n), 50);
    assert_eq!(n, 0);
    assert_eq!(no_data(C::LanBroadcastPort, &mut n), 8585);
    assert_eq!(no_data(C::FriendCount, &mut n), 0);
    // the default name with the address's number
    let q = make(true);
    let name: String = q.default_lan_name().iter().map(|&c| char::from_u32(c).unwrap()).collect();
    assert!(name.starts_with("Player") && name.len() > 6, "{name}");
}

#[test]
fn ignored_names_ignore_case_and_set_the_dirty_flag_to_the_change() {
    let mut p = make(false);
    call(&mut p, C::SetIgnored, "Bob", "", 1, 0);
    assert_eq!(call(&mut p, C::Flag, "", "", F::UserDirty as i64, 0), 1);
    call(&mut p, C::SetIgnored, "BOB", "", 1, 0);
    // already there: the dirty flag is set to false
    assert_eq!(call(&mut p, C::Flag, "", "", F::UserDirty as i64, 0), 0);
    assert_eq!(call(&mut p, C::IgnoredCount, "", "", 0, 0), 1);
    assert_eq!(text(&mut p, C::IgnoredAt, "", 0), "Bob");
    assert_eq!(call(&mut p, C::IsIgnored, "bOb", "", 0, 0), 1);
    call(&mut p, C::SetIgnored, "bob", "", 0, 0);
    assert_eq!(call(&mut p, C::IgnoredCount, "", "", 0, 0), 0);
    assert_eq!(call(&mut p, C::Flag, "", "", F::UserDirty as i64, 0), 1);
}

#[test]
fn friends_hold_one_reference_each() {
    let mut p = make(false);
    let f = call(&mut p, C::AddFriend, "Ann", "", 30, 0);
    assert_eq!(call(&mut p, C::AddFriend, "Other", "", 30, 0), f);
    call(&mut p, C::AddFriend, "Zed", "", 7, 0);
    assert_eq!(call(&mut p, C::FriendCount, "", "", 0, 0), 2);
    // by id
    assert_eq!(call(&mut p, C::FriendAt, "", "", 1, 0), f);
    assert_eq!(call(&mut p, C::FindFriend, "", "", 30, 0), f);
    assert_eq!(call(&mut p, C::FindFriend, "", "", 31, 0), 0);
    call(&mut p, C::RemoveFriend, "", "", 30, 0);
    WORLD.with(|w| assert_eq!(w.borrow().friends[0].2, 0));
    call(&mut p, C::RemoveFriend, "", "", 30, 0);
    assert_eq!(call(&mut p, C::Flag, "", "", F::UserDirty as i64, 0), 0);
    drop(p);
    WORLD.with(|w| assert!(w.borrow().friends.iter().all(|f| f.2 == 0)));
}

#[test]
fn lan_names_keep_the_most_recent() {
    let mut p = make(false);
    call(&mut p, C::SetLanName, "Ann", "", 0, 0);
    call(&mut p, C::SetLanName, "Bea", "", 0, 0);
    // the current name again: nothing
    call(&mut p, C::SetLanName, "Bea", "", 0, 0);
    assert_eq!(call(&mut p, C::LanNameCount, "", "", 0, 0), 2);
    assert_eq!(text(&mut p, C::Text, "", EeWonPrefsText::LanName as i64), "Bea");
    call(&mut p, C::RemoveLanName, "", "", 0, 0);
    assert_eq!(text(&mut p, C::Text, "", EeWonPrefsText::LanName as i64), "Ann");
    call(&mut p, C::RemoveLanName, "", "", 0, 0);
    assert_eq!(text(&mut p, C::Text, "", EeWonPrefsText::LanName as i64), "Player");
    for i in 0..60 {
        call(&mut p, C::SetLanName, &format!("n{i}"), "", 0, 0);
    }
    assert_eq!(call(&mut p, C::LanNameCount, "", "", 0, 0), 50);
}

#[test]
fn filters_and_game_options() {
    let mut p = make(false);
    call(&mut p, C::AddGameOption, "Epoch", " Stone, \"Iron Age\" ,Atomic", 0, 0);
    // a column with values is skipped unless appending
    call(&mut p, C::AddGameOption, "EPOCH", "Space", 0, 0);
    call(&mut p, C::ResetFilters, "", "", 0, 0);
    assert_eq!(call(&mut p, C::FilterCount, "epoch", "", 0, 0), 3);
    assert_eq!(text(&mut p, C::FilterValueAt, "Epoch", 1), "Iron Age");
    call(&mut p, C::AddFilterValue, "Epoch", "Stone", 0, 0);
    assert_eq!(call(&mut p, C::FilterCount, "Epoch", "", 0, 0), 3);
    call(&mut p, C::RemoveFilterValue, "Epoch", "stone", 0, 0);
    // unique by Compare: case matters
    assert_eq!(call(&mut p, C::FilterCount, "Epoch", "", 0, 0), 3);
    call(&mut p, C::RemoveFilterValue, "Epoch", "Stone", 0, 0);
    assert_eq!(call(&mut p, C::FilterCount, "Epoch", "", 0, 0), 2);
    // the defaults are untouched
    call(&mut p, C::ResetFilters, "", "", 0, 0);
    assert_eq!(call(&mut p, C::FilterCount, "Epoch", "", 0, 0), 3);
    call(&mut p, C::AddGameOption, "Epoch", "Space", 1, 0);
    call(&mut p, C::ResetFilters, "", "", 0, 0);
    assert_eq!(text(&mut p, C::FilterValueAt, "Epoch", 3), "Space");
}

#[test]
fn passwords_round_trip_through_the_key() {
    let p = make(false);
    let pw = u("s\u{e9}cr\u{1f600}t");
    let e = p.encrypt(&pw);
    assert!(!e.is_empty());
    assert_eq!(p.decrypt(&e), pw);
    assert_eq!(p.decrypt(b"junk"), Vec::<u32>::new());
}

#[test]
fn direct_connects_by_address() {
    let mut p = make(false);
    let a = six_addr(&[0x1f, 0x90, 10, 0, 0, 2]);
    let b = six_addr(&[0x1f, 0x90, 10, 0, 0, 1]);
    let mut n = 0;
    p.call(C::AddDirect, u("two"), Vec::new(), &a as *const _ as i64, 0, &mut n);
    p.call(C::AddDirect, u("one"), Vec::new(), &b as *const _ as i64, 0, &mut n);
    assert_eq!(call(&mut p, C::DirectCount, "", "", 0, 0), 2);
    assert_eq!(text(&mut p, C::DirectNameAt, "", 0), "one");
    let first = p.call(C::DirectAddrAt, Vec::new(), Vec::new(), 0, 0, &mut n) as *const EeWonIpAddr;
    assert_eq!(unsafe { &*first }.addr().six_bytes(), [0x1f, 0x90, 10, 0, 0, 1]);
    p.call(C::RemoveDirect, Vec::new(), Vec::new(), &b as *const _ as i64, 0, &mut n);
    assert_eq!(call(&mut p, C::DirectCount, "", "", 0, 0), 1);
    assert_eq!(p.call(C::ReconnectId, Vec::new(), Vec::new(), &a as *const _ as i64, 0, &mut n), 0);
}

#[test]
fn users_by_name_and_the_current_user() {
    let mut p = make(false);
    // existing only: a new user is not kept
    assert_eq!(call(&mut p, C::AddUser, "Ann", "pw", 1, 0), 0);
    assert_eq!(call(&mut p, C::UserCount, "", "", 0, 0), 0);
    let ann = call(&mut p, C::AddUser, "Ann", "pw", 0, 0) as *mut EeWonPersistUser;
    let bob = call(&mut p, C::AddUser, "bob", "", 0, 0) as *mut EeWonPersistUser;
    assert_eq!(unsafe { (*ann).user_id }, 1);
    assert_eq!(unsafe { (*bob).user_id }, 2);
    assert_eq!(call(&mut p, C::FindUser, "ANN", "", 0, 0), ann as isize);
    assert_eq!(text(&mut p, C::UserNameAt, "", 1), "bob");
    assert_eq!(call(&mut p, C::Flag, "", "", F::GlobalDirty as i64, 0), 1);
    // again: the same record, its id kept, its time stamped
    let t = unsafe { (*ann).last_use };
    assert_eq!(call(&mut p, C::AddUser, "aNN", "x", 0, 0), ann as isize);
    assert!(unsafe { (*ann).last_use } > t);
    let mut n = 0;
    let pw = unsafe { ee_won_persist_user_text(ann, 1, &mut n) };
    assert_eq!(unsafe { std::slice::from_raw_parts(pw, n) }, &u("x")[..]);
    p.prefs.set_current_user(2);
    assert_eq!(call(&mut p, C::CurrentUser, "", "", 0, 0), bob as isize);
}
