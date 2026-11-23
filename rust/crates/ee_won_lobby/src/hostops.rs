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

/// [`ctl_op`] in the game only: while the network core is bound (the game binds it with the lobby's WONAPICore,
/// before any control exists; the model-mode tests and the drivers' worlds never do).
pub(crate) fn game_ctl_op(op: Op, a: isize, b: isize, c: isize, out: Out) -> Option<isize> {
    if !ee_won_net::cppnet::bound() {
        return None;
    }
    ctl_op(op, a, b, c, out)
}

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
        _ => split_op(op, a, b, c, out),
    }
}

/// The R6 WON round 35 cases: LobbyNetCtrlHost.cpp's compound cases, each C++ statement one op of the host (the
/// cases' locals the host's statics, set where the local was made and emptied where it went out of scope).
fn split_op(op: Op, a: isize, b: isize, c: isize, out: Out) -> Option<isize> {
    use crate::lobbyctl::EeWonLobbyCtlEvField as EvF;
    use crate::lobbyevt::EeWonLobbyEvtField as EF;
    let r = match op {
        // a row's members (the friend and user rows' other fields are rows.rs's: the host answered them 0)
        Op::ItemGet => out(item_op(b, false)?, a, 0, 0),
        Op::ItemSet => {
            out(item_op(b, true)?, a, c, 0);
            0
        }
        Op::EvGet if b == EvF::TlaLink as isize => out(Op::EvTlaLink, a, 0, 0),
        // a chat link's members (an unknown one reads 0 and writes nothing)
        Op::LinkGet => out(link_op(b, false)?, a, 0, 0),
        Op::LinkSet => {
            out(link_op(b, true)?, a, c, 0);
            0
        }
        // a new text with one reference
        Op::TextNew => {
            let t = out(Op::TextAlloc, a, b, 0);
            out(Op::AddRef, t, 0, 0);
            t
        }
        // a text's bytes into the buffer `b` of `c` bytes (as many as fit); their count
        Op::TextBytes => {
            out(Op::BytesOfText, a, 0, 0);
            bytes_into(b, c, out)
        }
        // an HTTP document's path (the system's for `b` 0, else the game's) into the 1024 bytes at `c`
        Op::HttpDocPath => {
            out(if b == 0 { Op::BytesOfHttpDocSys } else { Op::BytesOfHttpDocGame }, a, 0, 0);
            bytes_into(c, 1024, out)
        }
        // the held text `a` (its reference the local's) with the text `b`, or the bytes `b` of `c`, after it
        Op::TextAppend | Op::TextAppendBytes => {
            out(Op::TextTake, a, 0, 0);
            out(Op::Release, a, 0, 0);
            if op == Op::TextAppend {
                out(Op::TextAppendText, b, 0, 0);
            } else {
                out(Op::TextAppendRaw, b, c, 0);
            }
            text_out(out)
        }
        // the prefix, the command, then the quoted name (the target's without one)
        Op::ParserCommandText => {
            out(Op::TextStartEmpty, 0, 0, 0);
            out(Op::TextAppendParserPrefix, a, 0, 0);
            out(Op::TextAppendParserCommand, a, b, 0);
            out(Op::TextAppendQuoteOpen, 0, 0, 0);
            if c != 0 {
                out(Op::TextAppendText, c, 0, 0);
            } else {
                out(Op::TextAppendParserTarget, a, 0, 0);
            }
            out(Op::TextAppendQuoteClose, 0, 0, 0);
            text_out(out)
        }
        // the format's strings, links and fonts into the arrays, the texts and links given when counted
        Op::ChatAddFormatted => {
            // SAFETY: the caller's format (chatctl.rs's, alive for the op)
            let f = unsafe { &*(a as *const crate::chatctl::EeWonLobbyCtlFormat) };
            for (i, &s) in f.strings.iter().enumerate() {
                out(Op::FmtSetString, i as isize, s, 0);
            }
            for (i, &l) in f.links.iter().enumerate() {
                out(Op::FmtSetLink, i as isize, l, 0);
            }
            for (i, &font) in f.fonts.iter().enumerate() {
                out(Op::FmtSetFont, i as isize, font, 0);
            }
            let texts = if f.num_strings != 0 { out(Op::FmtTexts, 0, 0, 0) } else { 0 };
            let links = if f.num_links != 0 { out(Op::FmtLinks, 0, 0, 0) } else { 0 };
            out(Op::FmtAppend, a, texts, links);
            for i in 0..f.strings.len() {
                out(Op::FmtSetString, i as isize, 0, 0);
            }
            0
        }
        // the account panel's fields (the inputs and the check box at `a`) in a CreateAccount event, delivered
        Op::DeliverCreateAccount => {
            // SAFETY: the caller's seven fields (netctl.rs's)
            let f = unsafe { std::slice::from_raw_parts(a as *const isize, 7) };
            let number = |input: isize| out(Op::TextToInt, out(Op::InputText, input, 0, 0), 0, 0);
            out(Op::EvCreateAccountNew, 0, 0, 0);
            out(Op::EvSetWide, EF::UserName as isize, f[0], 0);
            out(Op::EvSetWide, EF::Password as isize, f[1], 0);
            out(Op::EvSetNarrow, EF::Email as isize, f[2], 0);
            out(Op::EvSetNum, EF::Day as isize, number(f[3]) as u8 as isize, 0);
            out(Op::EvSetNum, EF::Month as isize, number(f[4]) as u8 as isize, 0);
            out(Op::EvSetNum, EF::Year as isize, number(f[5]) as u16 as isize, 0);
            out(Op::EvSetNum, EF::Newsletter as isize, (out(Op::ButtonIsChecked, f[6], 0, 0) != 0) as isize, 0);
            out(Op::EvDeliverHeld, 0, 0, 0);
            out(Op::EvDrop, 0, 0, 0);
            0
        }
        // the EE glue's column call `a` on the row `b`; the keeps' text `c` as a `GUIString*`
        Op::EeColumn => {
            use crate::eeglue::EeWonEeGlueCall as GC;
            let call = glue_call(a)?;
            if call == GC::EpochKeep || call == GC::MapTypeKeep {
                out(Op::ColumnTextSet, c, 0, 0);
                let text = out(Op::ColumnText, 0, 0, 0);
                // SAFETY: the glue's call over the row and the host's text (its contract)
                let r = unsafe { crate::eeglue::ee_won_eeglue_call(call, b, text, 0) };
                out(Op::ColumnTextSet, 0, 0, 0);
                r
            } else {
                // SAFETY: as above, the text the caller's
                unsafe { crate::eeglue::ee_won_eeglue_call(call, b, c, 0) }
            }
        }
        _ => return None,
    };
    Some(r)
}

/// ItemGet's and ItemSet's field ops (`None`: not the host's field).
fn item_op(field: isize, set: bool) -> Option<Op> {
    use crate::lobbyctl::EeWonLobbyCtlItemField as IF;
    let ops = [
        (IF::Height, Op::ItemGetHeight, Op::ItemSetHeight),
        (IF::Top, Op::ItemGetTop, Op::ItemSetTop),
        (IF::Width, Op::ItemGetWidth, Op::ItemSetWidth),
        (IF::Selected, Op::ItemGetSelected, Op::ItemSetSelected),
        (IF::Selectable, Op::ItemGetSelectable, Op::ItemSetSelectable),
        (IF::Color, Op::ItemGetColor, Op::ItemSetColor),
    ];
    ops.iter().find(|o| o.0 as isize == field).map(|o| if set { o.2 } else { o.1 })
}

/// LinkGet's and LinkSet's member ops (`None`: the original's `default`).
fn link_op(field: isize, set: bool) -> Option<Op> {
    use crate::chatctl::EeWonLobbyCtlLinkField as L;
    let ops = [
        (L::Kind, Some(Op::LinkGetKind), None),
        (L::Client, Some(Op::LinkGetClient), None),
        (L::Friend, Some(Op::LinkGetFriend), None),
        (L::Color, None, Some(Op::LinkSetColor)),
        (L::Enabled, None, Some(Op::LinkSetEnabled)),
        (L::Next, Some(Op::LinkGetNext), Some(Op::LinkSetNext)),
        (L::Previous, Some(Op::LinkGetPrevious), Some(Op::LinkSetPrevious)),
    ];
    let o = ops.iter().find(|o| o.0 as isize == field)?;
    if set {
        o.2
    } else {
        o.1
    }
}

/// The EE glue's call numbered `v` (srvlistctl.rs's column calls), `None` for another number.
fn glue_call(v: isize) -> Option<crate::eeglue::EeWonEeGlueCall> {
    use crate::eeglue::EeWonEeGlueCall as GC;
    [GC::EpochCompare, GC::MapTypeCompare, GC::EpochKeep, GC::MapTypeKeep, GC::EpochPaint, GC::MapTypePaint]
        .into_iter()
        .find(|c| *c as isize == v)
}

/// The held bytes (`sEELobbyBytes`) into `buffer` of `size` bytes (as many as fit), then dropped; their count.
fn bytes_into(buffer: isize, size: isize, out: Out) -> isize {
    let n = out(Op::BytesLen, 0, 0, 0);
    let copied = if (n as usize) < (size as usize) { n } else { size };
    out(Op::BytesCopy, buffer, copied, 0);
    out(Op::BytesDrop, 0, 0, 0);
    n
}

/// The built text (`sEELobbyText`) with one reference, the local dropped.
fn text_out(out: Out) -> isize {
    let t = out(Op::TextHeldOut, 0, 0, 0);
    out(Op::TextDrop, 0, 0, 0);
    t
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

    #[test]
    fn the_split_cases_run_their_statements_in_order() {
        use crate::lobbyctl::EeWonLobbyCtlItemField as IF;
        assert_eq!(run(Op::ItemGet, 0x10, IF::Top as isize, 0, &|_, _| 0), vec!["ItemGetTop 0x10 0x0 0x0"]);
        assert_eq!(run(Op::ItemSet, 0x10, IF::Color as isize, 9, &|_, _| 0), vec!["ItemSetColor 0x10 0x9 0x0"]);
        assert_eq!(ctl_op(Op::ItemGet, 0x10, IF::UserSortKey as isize, 0, &|_, _, _, _| 0), None);
        assert_eq!(run(Op::TextNew, 0x20, 3, 0, &|_, _| 0x55), vec!["TextAlloc 0x20 0x3 0x0", "AddRef 0x55 0x0 0x0"]);
        // the bytes: as many as fit, the count all of them
        let ops = run(Op::TextBytes, 0x30, 0x900, 4, &|op, _| if op == Op::BytesLen { 6 } else { 0 });
        assert_eq!(
            ops,
            vec![
                "BytesOfText 0x30 0x0 0x0",
                "BytesLen 0x0 0x0 0x0",
                "BytesCopy 0x900 0x4 0x0",
                "BytesDrop 0x0 0x0 0x0"
            ]
        );
        let ops = run(Op::HttpDocPath, 0x30, 1, 0x900, &|op, _| if op == Op::BytesLen { 6 } else { 0 });
        assert_eq!(ops[0], "BytesOfHttpDocGame 0x30 0x0 0x0");
        assert_eq!(ops[2], "BytesCopy 0x900 0x6 0x0");
        let ops = run(Op::TextAppendBytes, 0x40, 0x50, 2, &|_, _| 0x60);
        assert_eq!(
            ops,
            vec![
                "TextTake 0x40 0x0 0x0",
                "Release 0x40 0x0 0x0",
                "TextAppendRaw 0x50 0x2 0x0",
                "TextHeldOut 0x0 0x0 0x0",
                "TextDrop 0x0 0x0 0x0"
            ]
        );
        let ops = run(Op::ParserCommandText, 0x70, 3, 0, &|_, _| 0);
        assert_eq!(ops[4], "TextAppendParserTarget 0x70 0x0 0x0");
        assert_eq!(run(Op::ParserCommandText, 0x70, 3, 0x80, &|_, _| 0)[4], "TextAppendText 0x80 0x0 0x0");
        let fields: [isize; 7] = [1, 2, 3, 4, 5, 6, 7];
        let ops = run(Op::DeliverCreateAccount, fields.as_ptr() as isize, 0, 0, &|op, _| {
            if op == Op::TextToInt {
                300
            } else {
                1
            }
        });
        assert_eq!(ops.iter().filter(|o| o.starts_with("EvSetNum")).count(), 4);
        assert!(ops.contains(&format!(
            "EvSetNum {:#x} {:#x} 0x0",
            crate::lobbyevt::EeWonLobbyEvtField::Day as isize,
            300u16 as u8
        )));
        assert_eq!(ops.last().unwrap(), "EvDrop 0x0 0x0 0x0");
    }
}
