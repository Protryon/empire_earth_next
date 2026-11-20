//! The control hosts' cases whose statements are Rust (R6 WONLobby round 29): an op the Rust objects send to the
//! C++ host (LobbyCtrlHost.cpp, LobbyNetCtrlHost.cpp) that was a run of C++ statements with decisions between them is
//! answered here in the game, its decisions in Rust and each C++ statement a one-statement case of the host. Reached
//! from lobbyctl.rs's game mode and rows.rs's game host before the C++ host; the drivers' scripted hosts never reach
//! it, so the controls' and rows' goldens see the ops as before. won_hostops_test runs the deleted cases (as they
//! were) against these over one stand-in world.

use crate::lobbyctl::{pack, EeWonLobbyCtlOp as Op};
use crate::resource::EeWonLobbyRes as Res;
use std::sync::atomic::{AtomicU32, Ordering};

/// The calls out: the op through the same dispatch the moved op came by.
pub(crate) type Out<'a> = &'a dyn Fn(Op, isize, isize, isize) -> isize;

/// `DrawLobbyPing`'s ping ceiling (0x8df4cc): 5000 until the lobby config's is read, while it is still 5000.
static PING_CEILING: AtomicU32 = AtomicU32::new(5000);

/// The op answered here (`None`: the C++ host's).
pub(crate) fn ctl_op(op: Op, a: isize, b: isize, c: isize, out: Out) -> Option<isize> {
    match op {
        Op::GfxDrawPing => {
            draw_ping(a, b as u16, c as i32, out);
            Some(0)
        }
        // ServerListCtrl's rows: `" (%d)"` of the count after the name when shown (rows.rs)
        Op::ItemSetTextCount => {
            out(Op::ItemSetText, a, b, 0);
            if c >> 32 != 0 {
                with_text(out, &format!(" ({})", c as i32), |t| out(Op::ItemTextAppend, a, t, 0));
            }
            Some(0)
        }
        // ServerListCtrl's summaries: the counts `"%d"` into the resource's format
        Op::LabelSetPingSummary => {
            let res = Res::ServerListCtrl_ListPingSummary_String as isize;
            with_text(out, &format!("{}", b as i32), |t| out(Op::LabelSetFormatRes, a, res, t));
            Some(0)
        }
        Op::LabelSetListSummary => {
            let res = Res::ServerListCtrl_ListSummary_String as isize;
            with_text(out, &format!("{}", b as i32), |shown| {
                with_text(out, &format!("{}", c as i32), |total| {
                    let texts = [shown, total];
                    out(Op::LabelSetFormat2Res, a, res, texts.as_ptr() as isize)
                })
            });
            Some(0)
        }
        // the filter combo's add/remove row
        Op::FilterClear => {
            out(Op::ItemSetTextRes, a, Res::ServerListCtrl_AddRemove_String as isize, 0);
            out(Op::FilterSetIsAdd, a, 1, 0);
            out(Op::FilterClearText, a, 0, 0);
            out(Op::FilterSetSelPos, a, 0, 0);
            Some(0)
        }
        Op::FilterSet => {
            let is_add = b >> 32 != 0;
            out(Op::FilterSetSelPos, a, b as i32 as isize, 0);
            out(Op::FilterSetIsAdd, a, is_add as isize, 0);
            out(Op::FilterSetText, a, c, 0);
            let label = if is_add { Res::ServerListCtrl_Add_String } else { Res::ServerListCtrl_Remove_String };
            out(Op::ItemSetTextRes, a, label as isize, 0);
            out(Op::ItemTextAppend, a, c, 0);
            with_text(out, "\")", |t| out(Op::ItemTextAppend, a, t, 0));
            Some(0)
        }
        Op::ComboAddFilterValues => {
            let n = out(Op::FilterCount, b, 0, 0) as usize;
            for i in 0..n {
                out(Op::ComboAddFilterValue, a, b, i as isize);
            }
            Some(0)
        }
        // the room tree's joins and creations: the row's address and room id (rows.rs's)
        Op::DeliverJoinRoom => {
            let addr = crate::rows::ctl_op(Op::RoomItemAddr, a, 0, 0).unwrap_or(isize::MIN);
            let id = crate::rows::ctl_op(Op::RoomItemGroupId, a, 0, 0).unwrap_or(isize::MIN);
            out(Op::DeliverJoinRoomAt, addr, id, 0);
            Some(0)
        }
        Op::DeliverCreateRoom => {
            if a != 0 {
                out(Op::DeliverCreateRoomAt, a, 0, 0);
            } else {
                out(Op::DeliverCreateRoomAny, 0, 0, 0);
            }
            Some(0)
        }
        Op::ConfigIsPublic => {
            let config = out(Op::LobbyConfigGet, 0, 0, 0);
            Some((config != 0 && out(Op::ConfigPublic, config, 0, 0) != 0) as isize)
        }
        // a combo's, a tree's or a list's event item
        Op::EventItem => {
            let item = match out(Op::EvType, a, 0, 0) as i32 {
                COMBO_SEL_CHANGED => Op::EvComboItem,
                TREE_ITEM_OPENED => Op::EvTreeItem,
                _ => Op::EvListItem,
            };
            Some(out(item, a, 0, 0))
        }
        // the numbers as `"%d"`
        Op::LabelSetNumber => {
            with_text(out, &format!("{}", b as i32), |t| out(Op::LabelSetData, a, t, 0));
            Some(0)
        }
        Op::ComboAddNumber => {
            with_text(out, &format!("{}", b as i32), |t| out(Op::ComboAddString, a, t, 0));
            Some(0)
        }
        Op::LabelSetPercent => {
            with_text(out, &format!("{}", b as i32), |t| out(Op::LabelSetPercentText, a, t, 0));
            Some(0)
        }
        // EELobbyPlayerOptionsCtrl: the saved games directory's `.ees` files in the file dialog, run (-1 without it)
        Op::ChooseSavedGame => {
            let dialog = out(Op::FileDialogGet, 0, 0, 0);
            if dialog == 0 {
                return Some(-1);
            }
            out(Op::AddRef, dialog, 0, 0);
            out(Op::FileDialogListSaves, dialog, 0, 0);
            let result = out(Op::DoDialog, dialog, 0, 0);
            out(Op::Release, dialog, 0, 0);
            Some(result as i32 as isize)
        }
        _ => None,
    }
}

// WONGUI's `WindowEventType` places (ee/wonapi/WONEventTypes.h)
const TREE_ITEM_OPENED: i32 = 26;
const COMBO_SEL_CHANGED: i32 = 27;

/// `f` over a temporary text of the bytes `s` (a `GUIStringData` with one reference, given back after).
fn with_text(out: Out, s: &str, f: impl FnOnce(isize) -> isize) -> isize {
    let t = out(Op::TextNew, s.as_ptr() as isize, s.len() as isize, 0);
    let r = f(t);
    out(Op::Release, t, 0, 0);
    r
}

/// MOTDCtrl.cpp's `DrawLobbyPing(g, ping, height, 0, 0)` (the game list's and the game room's ping bars; the
/// alignment and width are always 0 and its text always empty, so the text branches never drew): the searching or
/// failed picture centred on the height, or 1 to 10 green bars scaled by the ceiling.
fn draw_ping(g: isize, ping: u16, height: i32, out: Out) {
    if ping == 0xfffe {
        return;
    }
    if PING_CEILING.load(Ordering::Relaxed) == 5000 {
        let config = out(Op::LobbyConfigGet, 0, 0, 0);
        if config != 0 {
            PING_CEILING.store(out(Op::ConfigPingCeiling, config, 0, 0) as u16 as u32, Ordering::Relaxed);
        }
    }
    let mut bars = 0i32;
    let image = match ping {
        0xfffd | 0xfffc => out(Op::ResImage, Res::LobbyGlobal_Searching_Image as isize, 0, 0),
        0xffff => out(Op::ResImage, Res::ServerListCtrl_PingFailed_Image as isize, 0, 0),
        _ => {
            // `10 - int((unsigned long)ping * 10 / ceiling)`; a zero ceiling (the original divided by it) shows 10
            let ceiling = u64::from(PING_CEILING.load(Ordering::Relaxed));
            bars = 10 - (u64::from(ping) * 10).checked_div(ceiling).unwrap_or(0) as i32;
            bars = bars.clamp(1, 10);
            0
        }
    };
    if image != 0 {
        let y = ((height - out(Op::ImageHeight, image, 0, 0) as i32) / 2).max(0);
        out(Op::GfxDrawImage, g, image, pack(0, y));
    } else {
        out(Op::GfxSetColor, g, 0x10d810, 0);
        let mut x = 0i32;
        while bars > 0 {
            out(Op::GfxFillRect, g, pack(x, 1), pack(2, height - 2));
            x += 3;
            bars -= 1;
        }
    }
}

/// The drivers' entry: `ctl_op` with the host `host` for the calls out, made with the control `ctl` (`INTPTR_MIN`:
/// not an op answered here).
///
/// # Safety
/// `host` answers the ops (a driver's world).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_hostops_ctl_op(
    ctl: *mut std::ffi::c_void,
    op: crate::lobbyctl::EeWonLobbyCtlOp,
    a: isize,
    b: isize,
    c: isize,
    host: crate::lobbyctl::EeWonLobbyCtlHostFn,
) -> isize {
    let out = |op: Op, a: isize, b: isize, c: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(ctl, op, a, b, c) },
        None => 0,
    };
    ctl_op(op, a, b, c, &out).unwrap_or(isize::MIN)
}

/// The drivers' reset of the remembered state (the ping ceiling back to 5000).
#[no_mangle]
pub extern "C" fn ee_won_lobby_hostops_reset() {
    PING_CEILING.store(5000, Ordering::Relaxed);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(op: Op, a: isize, b: isize, c: isize, answers: &dyn Fn(Op, isize) -> isize) -> Vec<String> {
        let log = RefCell::new(Vec::new());
        let out = |op: Op, a: isize, b: isize, c: isize| {
            log.borrow_mut().push(format!("{op:?} {a:#x} {b:#x} {c:#x}"));
            answers(op, a)
        };
        ctl_op(op, a, b, c, &out);
        log.into_inner()
    }

    #[test]
    fn the_ping_bars() {
        ee_won_lobby_hostops_reset();
        // no config: 5000 stays; ping 2500 is 5 bars of the height less 2
        let ops = run(Op::GfxDrawPing, 0x40, 2500, 12, &|_, _| 0);
        assert_eq!(ops[0], "LobbyConfigGet 0x0 0x0 0x0");
        assert_eq!(ops[1], "GfxSetColor 0x40 0x10d810 0x0");
        assert_eq!(ops.len(), 2 + 5);
        assert_eq!(ops[6], format!("GfxFillRect 0x40 {:#x} {:#x}", pack(12, 1), pack(2, 10)));
        // a config's ceiling is taken once
        let ops = run(Op::GfxDrawPing, 0x40, 100, 12, &|op, _| if op == Op::LobbyConfigGet { 0x77 } else { 1000 });
        assert_eq!(ops[1], "ConfigPingCeiling 0x77 0x0 0x0");
        assert_eq!(ops.len(), 3 + 9);
        // ceiling 1000: 10 - 20 is 1 bar; no config read
        let ops = run(Op::GfxDrawPing, 0x40, 2000, 12, &|_, _| 7);
        assert_eq!(ops.len(), 1 + 1);
        // the pictures, centred
        let ops = run(Op::GfxDrawPing, 0x40, 0xffff, 12, &|op, _| if op == Op::ImageHeight { 4 } else { 0x99 });
        assert_eq!(ops[0], format!("ResImage {:#x} 0x0 0x0", Res::ServerListCtrl_PingFailed_Image as isize));
        assert_eq!(ops[2], format!("GfxDrawImage 0x40 0x99 {:#x}", pack(0, 4)));
        assert!(run(Op::GfxDrawPing, 0x40, 0xfffe, 12, &|_, _| 1).is_empty());
        ee_won_lobby_hostops_reset();
    }
}
