use super::*;
use crate::lobbyctl::pack;
use std::cell::RefCell;
use std::collections::BTreeMap;

// A small world: items with their base fields, fixed fonts, images and model answers, and a log of every op that
// changes something or draws.
#[derive(Default, Clone, Copy)]
struct Item {
    selected: isize,
    width: isize,
    height: isize,
    color: isize,
    font: isize,
    image: isize,
}

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    static ITEMS: RefCell<BTreeMap<isize, Item>> = const { RefCell::new(BTreeMap::new()) };
    static NEXT: Cell<isize> = const { Cell::new(0x5000) };
    static MODEL: RefCell<BTreeMap<(isize, u32), isize>> = const { RefCell::new(BTreeMap::new()) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}

fn take() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

fn item(i: isize) -> Item {
    ITEMS.with(|m| m.borrow().get(&i).copied().unwrap_or_default())
}

fn edit(i: isize, f: impl FnOnce(&mut Item)) {
    ITEMS.with(|m| f(m.borrow_mut().entry(i).or_default()));
}

unsafe extern "C" fn world(_: *mut c_void, op: Op, a: isize, b: isize, c: isize) -> isize {
    let d = 0;
    match op {
        Op::RowShellNew => {
            let i = NEXT.with(|n| {
                n.set(n.get() + 0x10);
                n.get()
            });
            edit(i, |it| it.color = -1);
            log(format!("{} {i:#x}", if b != 0 { "NewTextRow" } else { "NewRow" }));
            i
        }
        Op::ItemGet => {
            let it = item(a);
            match b {
                b if b == F::Selected as isize => it.selected,
                b if b == F::Width as isize => it.width,
                b if b == F::Height as isize => it.height,
                _ => it.color,
            }
        }
        Op::ItemSet => {
            log(format!("set {a:#x} {b} {c}"));
            edit(a, |it| match b {
                b if b == F::Selected as isize => it.selected = c,
                b if b == F::Width as isize => it.width = c,
                b if b == F::Height as isize => it.height = c,
                _ => it.color = c,
            });
            0
        }
        Op::ItemFont => item(a).font,
        Op::ItemSetFont => {
            log(format!("font {a:#x} {b:#x}"));
            edit(a, |it| it.font = b);
            0
        }
        Op::ItemImage => item(a).image,
        Op::ItemSetImageRes => {
            log(format!("image {a:#x} {b}"));
            edit(a, |it| it.image = if b < 0 { 0 } else { 0x9000 + b });
            0
        }
        Op::FontHeight => a & 0xf,
        Op::ImageWidth => 20,
        Op::ImageHeight => 6,
        Op::ItemTextWidth => 40,
        Op::ListFont => 0x7003,
        Op::FontMod => 0x7108,
        Op::ListTextColor => 0x123,
        Op::ListPaintWidth => 90,
        Op::ListTotalWidth => 70,
        Op::ListFlagSet => 0,
        Op::ListPaintColumn => MODEL.with(|m| m.borrow().get(&(a, 999)).copied().unwrap_or(-1)),
        Op::GfxSelBack => 0xbb,
        Op::GfxSelFore => 0xf0 + c,
        Op::GfxStandardColor => 0x14,
        Op::ResImage => 0x9000 + a,
        Op::ItemTextCompare | Op::TextCompareNoCase => a - b,
        Op::RowTextCompare => 7,
        Op::MGet | Op::MCall => MODEL.with(|m| m.borrow().get(&(a, b as u32)).copied().unwrap_or(0)),
        Op::GameAddrText => {
            log(format!("addr text {a:#x}"));
            0x7400 + a
        }
        Op::AddrClone => {
            log(format!("clone {a:#x}"));
            a + 1
        }
        _ => {
            log(format!("{op:?} {a:#x} {b:#x} {c:#x} {d}"));
            0
        }
    }
}

struct World;
impl Drop for World {
    fn drop(&mut self) {
        ee_won_lobby_rows_host_world(None);
    }
}

fn world_on() -> World {
    ee_won_lobby_rows_host_world(Some(world));
    take();
    World
}

fn model(obj: isize, f: u32, v: isize) {
    MODEL.with(|m| m.borrow_mut().insert((obj, f), v));
}

fn call(item: isize, c: EeWonRowCall, list: isize, a: isize, b: isize) -> isize {
    let row = rowobj::ee_won_row_of(item as *mut c_void);
    assert!(!row.is_null());
    // SAFETY: a live row of the world
    unsafe { rowobj::ee_won_row_call(row, c, item as *mut c_void, list as *mut c_void, a, b, 0) }
}

fn free(item: isize) {
    // SAFETY: a live row, not used again
    unsafe { rowobj::ee_won_row_free(rowobj::ee_won_row_of(item as *mut c_void)) };
}

#[test]
fn a_loading_server_row_says_querying_and_frees_its_address() {
    let _w = world_on();
    model(0x3400, MF::ServerAddress as u32, 0x7800);
    let i = ctl_op(LOp::RoomItemServer, 0x3400, 1, 0).unwrap();
    assert_eq!(
        take(),
        vec![
            format!("NewTextRow {i:#x}"),
            "clone 0x7800".to_string(),
            format!("ItemSetTextRes {i:#x} {:#x} 0x0 0", R::RoomCtrl_QueryingServer_String as isize),
            format!("image {i:#x} -1"),
        ]
    );
    assert_eq!(ctl_op(LOp::RoomItemAddr, i, 0, 0), Some(0x7801));
    assert_eq!(ctl_op(LOp::RoomItemGroupId, i, 0, 0), Some(0xffff));
    // a loading row measures with the list's plain font
    call(i, EeWonRowCall::CalcDimensions, 0x6000, 0, 0);
    assert_eq!(
        take(),
        vec![
            format!("font {i:#x} 0x7003"),
            format!("set {i:#x} 7 291"),
            format!("set {i:#x} 2 42"),
            format!("set {i:#x} 0 3")
        ]
    );
    free(i);
    assert_eq!(take(), vec!["AddrFree 0x7801 0x0 0x0 0".to_string()]);
}

#[test]
fn a_room_row_counts_its_users_and_shows_the_lock() {
    let _w = world_on();
    model(0x3400, MF::ServerAddress as u32, 0x7800);
    model(0x3500, MF::GroupId as u32, 0x10007);
    model(0x3500, MF::GroupNumClients as u32, 3);
    model(0x3500, MF::GroupName as u32, 0x7100);
    model(0x3500, MC::GroupKind as u32, 5);
    let i = ctl_op(LOp::RoomItemGroup, 0x3500, 0x3400, 0).unwrap();
    let ops = take();
    assert_eq!(ops[2], format!("ItemSetTextCount {i:#x} 0x7100 {:#x} 0", pack(3, 1)));
    assert_eq!(ops[3], format!("image {i:#x} {}", R::RoomCtrl_Password_Image as isize));
    assert_eq!(ctl_op(LOp::RoomItemGroupId, i, 0, 0), Some(7));
    // a room row measures with the plain font and adds the lock's size
    call(i, EeWonRowCall::CalcDimensions, 0x6000, 0, 0);
    assert_eq!(item(i).width, 42 + 22);
    assert_eq!(item(i).height, 6);
    take();
    edit(i, |it| it.selected = 1);
    call(i, EeWonRowCall::Paint, 0x6000, 0x4000, 0);
    assert_eq!(
        take(),
        vec![
            "GfxSetColor 0x4000 0xbb 0x0 0".to_string(),
            format!("GfxFillRect 0x4000 0x0 {:#x} 0", pack(64, 6)),
            format!("GfxDrawImage 0x4000 {:#x} {:#x} 0", item(i).image, pack(2, 0)),
            format!("GfxSetColor 0x4000 {:#x} 0x0 0", 0xf0 + 0x123),
            "GfxSetFont 0x4000 0x7003 0x0 0".to_string(),
            format!("ItemTextDraw {i:#x} 0x4000 {:#x} 0", pack(24, 1)),
        ]
    );
    // the server row with no users: the name alone, a bold font
    model(0x3401, MF::ServerAddress as u32, 0x7900);
    model(0x3401, MF::ServerName as u32, 0x7200);
    let s = ctl_op(LOp::RoomItemServer, 0x3401, 0, 0).unwrap();
    assert!(take().contains(&format!("ItemSetTextCount {s:#x} 0x7200 0x0 0")));
    call(s, EeWonRowCall::CalcDimensions, 0x6000, 0, 0);
    assert!(take().contains(&format!("font {s:#x} 0x7108")));
    assert_eq!(call(i, EeWonRowCall::Compare, 0x6000, 0, s), i - s);
    free(i);
    free(s);
}

#[test]
fn friend_and_user_rows_are_separators_until_set() {
    let _w = world_on();
    let f = ctl_op(LOp::FriendRowNew, 0, 0, 0).unwrap();
    let g = ctl_op(LOp::FriendRowNew, 0, 0, 0).unwrap();
    assert_eq!(ctl_op(LOp::ItemGet, f, F::FriendSeparator as isize, 0), Some(1));
    ctl_op(LOp::ItemSet, g, F::FriendSortGroup as isize, 0x102);
    assert_eq!(ctl_op(LOp::ItemGet, g, F::FriendSortGroup as isize, 0), Some(2));
    assert_eq!(ctl_op(LOp::ItemGet, g, F::Height as isize, 0), None);
    assert_eq!(ctl_op(LOp::ItemGet, g, F::UserSortKey as isize, 0), None, "not a user row");
    take();
    call(f, EeWonRowCall::CalcDimensions, 0x6000, 0, 0);
    assert_eq!(take(), vec![format!("set {f:#x} 2 1"), format!("set {f:#x} 0 8")]);
    call(f, EeWonRowCall::Paint, 0x6000, 0x4000, 0);
    assert_eq!(
        take(),
        vec![
            "GfxSetSysColor 0x4000 0xe 0x0 0".to_string(),
            format!("GfxDrawLine 0x4000 {:#x} {:#x} 0", pack(0, 2), pack(90, 2)),
        ]
    );
    // the other's group first, then the text
    let fr = rowobj::ee_won_row_of(f as *mut c_void) as isize;
    assert_eq!(call(g, EeWonRowCall::Compare, 0x6000, fr, f), -2);
    ctl_op(LOp::ItemSetFriend, f, 0x3300, 0);
    assert_eq!(take(), vec!["AddRef 0x3300 0x0 0x0 0".to_string()]);
    assert_eq!(ctl_op(LOp::ItemFriend, f, 0, 0), Some(0x3300));
    let gr = rowobj::ee_won_row_of(g as *mut c_void) as isize;
    assert_eq!(call(f, EeWonRowCall::Compare, 0x6000, gr, g), 2);
    call(f, EeWonRowCall::Paint, 0x6000, 0x4000, 0);
    assert_eq!(take(), vec![format!("RowTextPaint {f:#x} 0x4000 0x6000 0")]);
    free(f);
    assert_eq!(take(), vec!["Release 0x3300 0x0 0x0 0".to_string()]);
    let u = ctl_op(LOp::UserRowNew, 0, 0, 0).unwrap();
    ctl_op(LOp::ItemSet, u, F::UserShowSeparator as isize, 0);
    take();
    call(u, EeWonRowCall::CalcDimensions, 0x6000, 0, 0);
    assert_eq!(take(), vec![format!("set {u:#x} 2 1"), format!("set {u:#x} 0 1")]);
    free(g);
    free(u);
}

#[test]
fn a_player_row_paints_its_columns() {
    let _w = world_on();
    let p = ctl_op(LOp::ItemNew, 0x3200, 0, 0).unwrap();
    assert_eq!(
        take(),
        vec![format!("NewRow {p:#x}"), "AddRef 0x3200 0x0 0x0 0".into(), format!("AddRef {p:#x} 0x0 0x0 0")]
    );
    ctl_op(LOp::ItemSetImage, p, Icon::Captain as isize, 0);
    let img = 0x9000 + R::UserListCtrl_Captain_Image as isize;
    assert_eq!(take(), vec![format!("AddRef {img:#x} 0x0 0x0 0")]);
    call(p, EeWonRowCall::CalcDimensions, 0x6000, 0, 0);
    assert_eq!(item(p).height, 6, "the icon is taller than the font");
    take();
    model(0x6000, 999, 3);
    model(0x3200, MC::ClientPing as u32, 0x1_0005);
    call(p, EeWonRowCall::Paint, 0x6000, 0x4000, 0);
    assert_eq!(
        take(),
        vec![
            "GfxSetColor 0x4000 0x14 0x0 0".to_string(),
            "GfxSetFont 0x4000 0x7003 0x0 0".to_string(),
            "GfxDrawPing 0x4000 0x5 0x4 0".to_string(),
        ]
    );
    model(0x3200, MC::ClientIsCaptain as u32, 1);
    let q = ctl_op(LOp::ItemNew, 0x3210, 0, 0).unwrap();
    let pr = rowobj::ee_won_row_of(p as *mut c_void) as isize;
    assert_eq!(call(q, EeWonRowCall::Compare, 0x6000, pr, p), 1, "the captain first");
    assert_eq!(ctl_op(LOp::ItemClient, q, 0, 0), Some(0x3210));
    assert_eq!(ctl_op(LOp::ItemClientIs, p, Test::IsCaptain as isize, 0), Some(1));
    assert_eq!(ctl_op(LOp::ItemClientIs, q, Test::IsCaptain as isize, 0), Some(0));
    take();
    free(p);
    assert_eq!(
        take(),
        vec![
            "Release 0x7003 0x0 0x0 0".to_string(),
            format!("Release {img:#x} 0x0 0x0 0"),
            "Release 0x3200 0x0 0x0 0".into()
        ]
    );
    free(q);
}

#[test]
fn other_ops_are_not_rows() {
    let _w = world_on();
    assert_eq!(ctl_op(LOp::Find, 0, 0, 0), None);
    assert_eq!(ctl_op(LOp::ItemFriend, 0x1234, 0, 0), None);
    assert_eq!(ee_won_lobby_rows_ctl_op(LOp::Find, 0, 0, 0), isize::MIN);
}

#[test]
fn a_game_list_row_keeps_its_game_text_icon_and_height() {
    let _w = world_on();
    let r = ctl_op(LOp::RowNew, 0, 0, 0).unwrap();
    assert_eq!(take(), vec![format!("NewRow {r:#x}"), format!("AddRef {r:#x} 0x0 0x0 0")]);
    assert_eq!(ctl_op(LOp::RowGame, r, 0, 0), Some(0));
    ctl_op(LOp::RowSetGame, r, 0x3000, 0);
    ctl_op(LOp::RowSetAddrText, r, 0, 0);
    ctl_op(LOp::RowSetAddrText, r, 0, 0);
    assert_eq!(
        take(),
        vec![
            "AddRef 0x3000 0x0 0x0 0".to_string(),
            "addr text 0x3000".into(),
            "addr text 0x3000".into(),
            "Release 0xa400 0x0 0x0 0".into(),
        ]
    );
    assert_eq!(ctl_op(LOp::RowAddrText, r, 0, 0), Some(0xa400));
    ctl_op(LOp::ItemSet, r, F::SummaryHeight as isize, 0x1ff);
    ctl_op(LOp::ItemSet, r, F::IconId as isize, -3);
    assert_eq!(ctl_op(LOp::ItemGet, r, F::SummaryHeight as isize, 0), Some(0xff));
    assert_eq!(ctl_op(LOp::ItemGet, r, F::IconId as isize, 0), Some(-3));
    // ServerDetailsItem's own hooks (no EE game): no details
    assert_eq!(ctl_op(LOp::RowDetailsHeight, r, 0x6000, 0), Some(0));
    assert_eq!(ctl_op(LOp::RowPaintDetails, r, 0x4000, 0x6000), Some(0));
    assert_eq!(ctl_op(LOp::RowGame, 0x1234, 0, 0), None);
    free(r);
    assert_eq!(take(), vec!["Release 0xa400 0x0 0x0 0".to_string(), "Release 0x3000 0x0 0x0 0".into()]);
}
