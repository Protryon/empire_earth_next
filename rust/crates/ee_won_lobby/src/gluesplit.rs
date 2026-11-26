//! The EE lobby glue host's multi-statement cases composed in Rust (R6 WON round 32): an op eeglue.rs, eegamefn.rs
//! or eewindow.rs sends that was a run of C++ statements in EELobbyGlueHost.cpp is answered here in the game, each
//! C++ statement one case of the same host (`EeWonEeGlueOp` from 500), in the original's order. Only while the
//! host is the game's (`ee_won_eeglue_set_game_host`): the drivers' scripted hosts (won_eeglue_test,
//! won_eewindow_test) still see the ops whole.
//!
//! The C++ locals the statements shared (the resource table, the player colour, the saved game's length, the
//! address list) are the host's statics, set and cleared by ops where the local was made and went out of scope.

use crate::eeglue::{EeWonEeGlueField as F, EeWonEeGlueOp as Op};

/// The calls out: the host's op with its six arguments.
pub(crate) type Out<'a> = &'a dyn Fn(Op, isize, isize, isize, isize, isize, isize) -> isize;

fn is_game() -> bool {
    ee_won_gui::compobj::is_game()
}

/// A lobby text (`GlueText(kind, value)`: eeglue.rs's `EeWonEeGlueText`) with one reference: a map type's name is
/// the host's (`MapTypeTextHeld`); in a driver's world the host's `TextHeld`.
fn text_held(kind: isize, value: isize, out: Out) -> isize {
    use crate::eeglue::EeWonEeGlueText as T;
    use crate::lobbytext as lt;
    if !is_game() {
        return out(Op::TextHeld, kind, value, 0, 0, 0, 0);
    }
    let empty = || lt::held(ee_won_gui::guistrobj::ee_won_gstr_empty());
    match kind {
        k if k == T::Resource as isize => {
            // SAFETY: the string's GUIString (one text pointer)
            lt::held(unsafe { *(crate::eeresource::ee_won_eeresource_string(value as i32 as usize) as *const *mut _) })
        }
        // `sprintf("%ld", (long)value)`
        k if k == T::Number as isize => {
            let s = (value as std::ffi::c_long).to_string();
            lt::held(lt::narrow_new(s.as_ptr() as isize, s.len()))
        }
        k if k == T::MapType as isize => out(Op::MapTypeTextHeld, value, 0, 0, 0, 0, 0),
        k if k == T::Data as isize && value != 0 => lt::held(value as *mut _),
        _ => empty(),
    }
}

/// The op `op` with the arguments `v` answered here (`None`: the host's own case).
pub(crate) fn game_op(op: Op, v: [isize; 6], out: Out) -> Option<isize> {
    let o = |op: Op, a: isize, b: isize| out(op, a, b, 0, 0, 0, 0);
    let [a, b, c, ..] = v;
    let r = match op {
        // `GUIString aText = GlueText(b, c); return g->mFont->GetStringWidth(aText, 0, aText.length());`
        Op::TextWidth => {
            let t = text_held(b, c, out);
            let w = o(Op::GfxTextWidthHeld, a, t);
            o(Op::Release, t, 0);
            w
        }
        // the lobby's texts (`GlueText(kind, value)`), in the game
        Op::TextHeld => {
            if !is_game() {
                return None;
            }
            text_held(a, b, out)
        }
        // `((Graphics*)a)->DrawString(GlueText(b, c), d, e)`
        Op::DrawText => {
            if !is_game() {
                return None;
            }
            let t = text_held(b, c, out);
            let p = t as *const ee_won_gui::guistrobj::EeWonGstr;
            // SAFETY: the game's Graphics (its record first); the text a GUIString (one pointer) for the call
            unsafe {
                ee_won_gui::gfxrun::ee_won_graphics_call(
                    a as *mut ee_won_gui::gfxrun::EeWonGraphics,
                    ee_won_gui::gfxobj::EeWonGfxCall::DrawString,
                    v[3] as i32,
                    v[4] as i32,
                    crate::lobbytext::len(t) as i32,
                    0,
                    0,
                    0,
                    0,
                    (&p as *const *const ee_won_gui::guistrobj::EeWonGstr).cast(),
                )
            };
            crate::lobbytext::release(t);
            0
        }
        // `GlueText(b, c).findNoCase(*(const GUIString*)d, 0)`
        Op::TextFind => {
            if !is_game() {
                return None;
            }
            let t = text_held(b, c, out);
            let r = crate::lobbytext::find_no_case(t, crate::lobbytext::of(v[3]));
            crate::lobbytext::release(t);
            r
        }
        // `GlueText(a, b).CompareNoCase(GlueText(c, d))`
        Op::TextCompareNoCase => {
            if !is_game() {
                return None;
            }
            let (ta, tb) = (text_held(a, b, out), text_held(c, v[3], out));
            let r = crate::lobbytext::compare_no_case(ta, tb);
            crate::lobbytext::release(ta);
            crate::lobbytext::release(tb);
            r
        }
        // `LobbyAddGameOption(a ? "MapType" : "Epoch", GlueText(b, c), true)`
        Op::AddGameOption => {
            if !is_game() {
                return None;
            }
            let name: &str = if a != 0 { "MapType" } else { "Epoch" };
            let column = crate::lobbytext::held(crate::lobbytext::narrow_new(name.as_ptr() as isize, name.len()));
            let t = text_held(b, c, out);
            crate::persistobj::game_call_texts(crate::persistobj::EeWonPersistCall::AddGameOption, column, t, 1);
            crate::lobbytext::release(t);
            crate::lobbytext::release(column);
            0
        }
        Op::SetFactories => {
            o(Op::FactoryGame, 0, 0);
            o(Op::FactoryPlayer, 0, 0);
            crate::rows::ee_won_lobby_rows_ee_details(); // the game list's rows paint the EE game's details
            o(Op::FactoryConfig, 0, 0);
            0
        }
        Op::AddResources => {
            for op in [
                Op::ResInit,
                Op::ResAllocGameOptions,
                Op::ResAllocPlayerOptions,
                Op::ResAllocColorButton,
                Op::ResAllocFileDialog,
                Op::ResAllocConnectDialog,
                Op::ResAddMusic,
                Op::ResAddLoader,
                Op::ResDone,
            ] {
                o(op, 0, 0);
            }
            0
        }
        // `EEGetPlayerColor(a)`'s channels into the `float[3]` `b`
        Op::PlayerColor => {
            o(Op::PlayerColorGet, a, 0);
            o(Op::PlayerColorRed, b, 0);
            o(Op::PlayerColorGreen, b + 4, 0);
            o(Op::PlayerColorBlue, b + 8, 0);
            0
        }
        // `aData->mLength`, and with `b` its units copied to the `uint32_t*` `b` (R6 WON round 33)
        // (the game's units read in place since R6 WON GUI round 40; a driver's world answers the two ops)
        Op::StringUnits if ee_won_gui::compobj::is_game() => {
            let u = crate::lobbytext::units(a);
            if b != 0 {
                // SAFETY: the caller's buffer of `mLength` units (the op's contract)
                unsafe { std::ptr::copy_nonoverlapping(u.as_ptr(), b as *mut u32, u.len()) };
            }
            u.len() as isize
        }
        Op::StringUnits => {
            let n = o(Op::StringLength, a, 0);
            if b != 0 {
                for i in 0..n.max(0) as u32 as isize {
                    // SAFETY: the caller's buffer of `mLength` units (the op's contract)
                    unsafe { *(b as *mut u32).offset(i) = o(Op::StringUnitAt, a, i) as u32 };
                }
            }
            n
        }
        Op::FindDialog => o(if b != 0 { Op::FindConnectDialog } else { Op::FindFileDialog }, a, 0),
        Op::SaveNew => {
            let s = o(Op::SaveAlloc, 0, 0);
            o(Op::SaveSetFile, s, a);
            s
        }
        Op::SaveLength => {
            o(Op::SaveLengthClear, 0, 0);
            o(Op::SaveGetLength, a, 0);
            o(Op::SaveLengthValue, 0, 0)
        }
        Op::SaveLoadHeader => {
            o(Op::SaveSetMajor, a, 0);
            o(Op::SaveSetMinor, a, 0);
            o(Op::SaveLoadFile, a, 0)
        }
        Op::OptionsBegin => {
            o(Op::OptionsWriteLock, 0, 0);
            o(Op::OptionsCopyLobby, 0, 0);
            o(Op::OptionsSetMultiplayer, 0, 0);
            o(Op::OptionsClearCheats, 0, 0);
            0
        }
        Op::AddressesGet => {
            o(Op::AddressListFill, 0, 0);
            let v = o(Op::AddressVectorNew, 0, 0);
            o(Op::AddressListClear, 0, 0);
            v
        }
        Op::DeleteCommunications => {
            o(Op::CommDelete, 0, 0);
            o(Op::CommClear, 0, 0);
            0
        }
        // the run loop's locks (EEUserInterface::ProcessActions' order)
        Op::LocksRelease => {
            o(Op::WorldWriteUnlock, 0, 0);
            o(Op::UiUnlockCopy, 0, 0);
            0
        }
        Op::LocksRetake => {
            o(Op::UiLockCopy, 0, 0);
            o(Op::WorldWriteLock, 0, 0);
            0
        }
        // the options' and players' fields: the field's own op (R6 WON round 35; an unknown field reads 0)
        Op::OptionsField if b == F::MapTypeIndex as isize => {
            o(Op::MapIndexClear, 0, 0);
            o(Op::OptGetMapTypeIndex, a, 0)
        }
        Op::OptionsField => opt_get(b).map_or(0, |op| o(op, a, 0)),
        Op::OptionsPlayer => player_get(c).map_or(0, |op| o(op, o(Op::OptionsPlayerAt, a, b), 0)),
        Op::OptionSet => opt_set(a).map_or(0, |op| o(op, b, 0)),
        Op::PlayerSet => player_set(a).map_or(0, |op| o(op, b, 0)),
        _ => return None,
    };
    Some(r)
}

/// The field ops (R6 WON round 35): a field's read of the options, its write of the game's options, a player's
/// read and the player being made's write (`None`: the original's `default` case, 0 and no statement).
fn opt_get(f: isize) -> Option<Op> {
    Some(match f {
        x if x == F::MapSize as isize => Op::OptGetMapSize,
        x if x == F::Resources as isize => Op::OptGetResources,
        x if x == F::EpochStart as isize => Op::OptGetEpochStart,
        x if x == F::EpochEnd as isize => Op::OptGetEpochEnd,
        x if x == F::Variant as isize => Op::OptGetVariant,
        x if x == F::Difficulty as isize => Op::OptGetDifficulty,
        x if x == F::GameSpeed as isize => Op::OptGetGameSpeed,
        x if x == F::UnitLimit as isize => Op::OptGetUnitLimit,
        x if x == F::WondersForVictory as isize => Op::OptGetWondersForVictory,
        x if x == F::RevealMap as isize => Op::OptGetRevealMap,
        x if x == F::AllowCustomCivs as isize => Op::OptGetAllowCustomCivs,
        x if x == F::LockTeams as isize => Op::OptGetLockTeams,
        x if x == F::LockSpeed as isize => Op::OptGetLockSpeed,
        x if x == F::Victory as isize => Op::OptGetVictory,
        _ => return None,
    })
}
fn opt_set(f: isize) -> Option<Op> {
    Some(match f {
        x if x == F::LoadType as isize => Op::OptSetLoadType,
        x if x == F::MapSize as isize => Op::OptSetMapSize,
        x if x == F::Resources as isize => Op::OptSetResources,
        x if x == F::EpochStart as isize => Op::OptSetEpochStart,
        x if x == F::EpochEnd as isize => Op::OptSetEpochEnd,
        x if x == F::Variant as isize => Op::OptSetVariant,
        x if x == F::Difficulty as isize => Op::OptSetDifficulty,
        x if x == F::GameSpeed as isize => Op::OptSetGameSpeed,
        x if x == F::UnitLimit as isize => Op::OptSetUnitLimit,
        x if x == F::WondersForVictory as isize => Op::OptSetWondersForVictory,
        x if x == F::RevealMap as isize => Op::OptSetRevealMap,
        x if x == F::AllowCustomCivs as isize => Op::OptSetAllowCustomCivs,
        x if x == F::LockTeams as isize => Op::OptSetLockTeams,
        x if x == F::LockSpeed as isize => Op::OptSetLockSpeed,
        x if x == F::Victory as isize => Op::OptSetVictory,
        _ => return None,
    })
}
fn player_get(f: isize) -> Option<Op> {
    Some(match f {
        x if x == F::Type as isize => Op::PlayerGetType,
        x if x == F::LoadedType as isize => Op::PlayerGetLoadedType,
        x if x == F::Slot as isize => Op::PlayerGetSlot,
        x if x == F::Color as isize => Op::PlayerGetColor,
        x if x == F::Team as isize => Op::PlayerGetTeam,
        x if x == F::StartingCitizens as isize => Op::PlayerGetStartingCitizens,
        x if x == F::IsLocal as isize => Op::PlayerGetIsLocal,
        x if x == F::IsReady as isize => Op::PlayerGetIsReady,
        x if x == F::B36 as isize => Op::PlayerGetB36,
        x if x == F::NameEmpty as isize => Op::PlayerGetNameEmpty,
        _ => return None,
    })
}
fn player_set(f: isize) -> Option<Op> {
    Some(match f {
        x if x == F::Type as isize => Op::PlayerSetType,
        x if x == F::LoadedType as isize => Op::PlayerSetLoadedType,
        x if x == F::Slot as isize => Op::PlayerSetSlot,
        x if x == F::Color as isize => Op::PlayerSetColor,
        x if x == F::Team as isize => Op::PlayerSetTeam,
        x if x == F::StartingCitizens as isize => Op::PlayerSetStartingCitizens,
        x if x == F::IsLocal as isize => Op::PlayerSetIsLocal,
        x if x == F::IsReady as isize => Op::PlayerSetIsReady,
        x if x == F::B36 as isize => Op::PlayerSetB36,
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(op: Op, v: [isize; 6], answer: &dyn Fn(Op) -> isize) -> (Option<isize>, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let out = |op: Op, a: isize, b: isize, _: isize, _: isize, _: isize, _: isize| {
            log.borrow_mut().push(format!("{op:?} {a} {b}"));
            answer(op)
        };
        let r = game_op(op, v, &out);
        (r, log.into_inner())
    }

    #[test]
    fn the_statements_run_in_the_original_order() {
        let (r, log) = run(Op::TextWidth, [7, 2, 9, 0, 0, 0], &|op| if op == Op::TextHeld { 40 } else { 13 });
        assert_eq!(
            (r, log),
            (Some(13), vec!["TextHeld 2 9".into(), "GfxTextWidthHeld 7 40".into(), "Release 40 0".into()])
        );
        let (r, log) = run(Op::SaveNew, [5, 0, 0, 0, 0, 0], &|op| if op == Op::SaveAlloc { 80 } else { 0 });
        assert_eq!((r, log), (Some(80), vec!["SaveAlloc 0 0".into(), "SaveSetFile 80 5".into()]));
        let (r, log) = run(Op::SaveLength, [80, 0, 0, 0, 0, 0], &|op| if op == Op::SaveLengthValue { 1234 } else { 0 });
        assert_eq!(
            (r, log),
            (Some(1234), vec!["SaveLengthClear 0 0".into(), "SaveGetLength 80 0".into(), "SaveLengthValue 0 0".into()])
        );
        let (r, log) = run(Op::SaveLoadHeader, [80, 0, 0, 0, 0, 0], &|op| (op == Op::SaveLoadFile) as isize);
        assert_eq!((r, log.len(), log[0].as_str()), (Some(1), 3, "SaveSetMajor 80 0"));
        let (_, log) = run(Op::PlayerColor, [3, 1000, 0, 0, 0, 0], &|_| 0);
        assert_eq!(
            log,
            vec!["PlayerColorGet 3 0", "PlayerColorRed 1000 0", "PlayerColorGreen 1004 0", "PlayerColorBlue 1008 0"]
        );
        let (_, log) = run(Op::LocksRelease, [0; 6], &|_| 0);
        assert_eq!(log, vec!["WorldWriteUnlock 0 0", "UiUnlockCopy 0 0"]);
        let (_, log) = run(Op::LocksRetake, [0; 6], &|_| 0);
        assert_eq!(log, vec!["UiLockCopy 0 0", "WorldWriteLock 0 0"]);
        let (_, log) = run(Op::AddResources, [0; 6], &|_| 0);
        assert_eq!((log.len(), log[0].as_str(), log[8].as_str()), (9, "ResInit 0 0", "ResDone 0 0"));
        let (r, log) = run(Op::FindDialog, [9, 1, 0, 0, 0, 0], &|_| 55);
        assert_eq!((r, log), (Some(55), vec!["FindConnectDialog 9 0".into()]));
        let (_, log) = run(Op::FindDialog, [9, 0, 0, 0, 0, 0], &|_| 55);
        assert_eq!(log, vec!["FindFileDialog 9 0"]);
        let (_, log) = run(Op::DeleteCommunications, [0; 6], &|_| 0);
        assert_eq!(log, vec!["CommDelete 0 0", "CommClear 0 0"]);
        assert_eq!(run(Op::MyGame, [0; 6], &|_| 0).0, None);
    }

    #[test]
    fn the_fields_are_one_op_each() {
        let (r, log) = run(Op::OptionsField, [70, F::MapSize as isize, 0, 0, 0, 0], &|_| 4);
        assert_eq!((r, log), (Some(4), vec!["OptGetMapSize 70 0".into()]));
        let (r, log) = run(Op::OptionsField, [70, F::MapTypeIndex as isize, 0, 0, 0, 0], &|_| 6);
        assert_eq!((r, log), (Some(6), vec!["MapIndexClear 0 0".into(), "OptGetMapTypeIndex 70 0".into()]));
        // the original's default cases: 0 and no statement
        assert_eq!(run(Op::OptionsField, [70, F::LoadType as isize, 0, 0, 0, 0], &|_| 6), (Some(0), vec![]));
        assert_eq!(run(Op::OptionSet, [F::MapTypeIndex as isize, 3, 0, 0, 0, 0], &|_| 6), (Some(0), vec![]));
        assert_eq!(run(Op::PlayerSet, [F::NameEmpty as isize, 3, 0, 0, 0, 0], &|_| 6), (Some(0), vec![]));
        assert_eq!(run(Op::OptionsPlayer, [70, 2, F::Victory as isize, 0, 0, 0], &|_| 6), (Some(0), vec![]));
        let (r, log) = run(Op::OptionsPlayer, [70, 2, F::Team as isize, 0, 0, 0], &|op| {
            if op == Op::OptionsPlayerAt {
                900
            } else {
                3
            }
        });
        assert_eq!((r, log), (Some(3), vec!["OptionsPlayerAt 70 2".into(), "PlayerGetTeam 900 0".into()]));
        let (_, log) = run(Op::OptionSet, [F::LoadType as isize, 5, 0, 0, 0, 0], &|_| 0);
        assert_eq!(log, vec!["OptSetLoadType 5 0"]);
        let (_, log) = run(Op::PlayerSet, [F::B36 as isize, 1, 0, 0, 0, 0], &|_| 0);
        assert_eq!(log, vec!["PlayerSetB36 1 0"]);
        // every field of the options reads through its own op
        for f in F::MapSize as isize..=F::Victory as isize {
            assert!(opt_get(f).is_some() && opt_set(f).is_some());
        }
        for f in F::Type as isize..=F::B36 as isize {
            assert!(player_get(f).is_some() && player_set(f).is_some());
        }
        assert!(player_get(F::NameEmpty as isize).is_some());
    }
}
