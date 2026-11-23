//! The EE lobby glue host's multi-statement cases composed in Rust (R6 WON round 32): an op eeglue.rs, eegamefn.rs
//! or eewindow.rs sends that was a run of C++ statements in EELobbyGlueHost.cpp is answered here in the game, each
//! C++ statement one case of the same host (`EeWonEeGlueOp` from 500), in the original's order. Only while the
//! host is the game's (`ee_won_eeglue_set_game_host`): the drivers' scripted hosts (won_eeglue_test,
//! won_eewindow_test) still see the ops whole.
//!
//! The C++ locals the statements shared (the resource table, the player colour, the saved game's length, the
//! address list) are the host's statics, set and cleared by ops where the local was made and went out of scope.

use crate::eeglue::EeWonEeGlueOp as Op;

/// The calls out: the host's op with its six arguments.
pub(crate) type Out<'a> = &'a dyn Fn(Op, isize, isize, isize, isize, isize, isize) -> isize;

/// The op `op` with the arguments `v` answered here (`None`: the host's own case).
pub(crate) fn game_op(op: Op, v: [isize; 6], out: Out) -> Option<isize> {
    let o = |op: Op, a: isize, b: isize| out(op, a, b, 0, 0, 0, 0);
    let [a, b, c, ..] = v;
    let r = match op {
        // `GUIString aText = GlueText(b, c); return g->mFont->GetStringWidth(aText, 0, aText.length());`
        Op::TextWidth => {
            let t = o(Op::TextHeld, b, c);
            let w = o(Op::GfxTextWidthHeld, a, t);
            o(Op::Release, t, 0);
            w
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
        _ => return None,
    };
    Some(r)
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
}
