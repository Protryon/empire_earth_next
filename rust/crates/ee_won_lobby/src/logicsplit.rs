//! The lobby logics' host cases composed in Rust (R6 WON lobby and network round 35): an op DialogLogic's Rust object
//! (dlglogic.rs) sends to its C++ host (WONLobby/DialogLogic.cpp) that was a run of C++ statements is answered here
//! in the game, each statement one case of the host (a C++ local is a host static, set where the local was made and
//! emptied where it died). Reached from dlglogic.rs's `op` while the network core is bound (the game); the drivers'
//! scripted hosts never reach it, so won_dlglogic_test sees the ops whole. won_logic35_test runs the deleted cases
//! (as they were at 207fb805f) against these over one stand-in world, then skips each new op in turn.
//!
//! - WmPopups: the default window manager's popups flag (-1 without a manager).
//! - Bounds: a component's bounds into the caller's four ints.
//! - NewWindow / SetUpPopup: a popup window made (over the host's `SmartPtr<Window>` local) and set up.
//! - StatusSetStatus: a WON status's text (the host's `GUIString` local) on the status dialog.
//! - OkCancelSetText: the OK/Cancel or Yes/No button texts (the host's two-pointer local) on the OK/Cancel dialog.

use crate::dlglogic::EeWonDlgLogicOp as Op;
use crate::resource::EeWonLobbyRes as R;

/// The calls out: a DialogLogic op to the host the logic was made with.
pub(crate) type Out<'a> = &'a dyn Fn(Op, isize, isize, isize, isize) -> isize;

/// [`dlg_op`] in the game only: while the network core is bound (the drivers' scripted hosts never bind it).
pub(crate) fn game_dlg_op(op: Op, a: isize, b: isize, c: isize, d: isize, out: Out) -> Option<isize> {
    if !ee_won_net::cppnet::bound() {
        return None;
    }
    dlg_op(op, a, b, c, d, out)
}

/// The op answered here (`None`: the host's).
pub(crate) fn dlg_op(op: Op, a: isize, b: isize, c: isize, d: isize, out: Out) -> Option<isize> {
    let on = |op: Op, x: isize| out(op, x, 0, 0, 0);
    let r = match op {
        // `if (GetDefaultWindowManager() == NULL) return -1; return GetDefaultWindowManager()->GetPopups..() ? 1 : 0`
        Op::WmPopups => {
            if on(Op::WmDefault, 0) == 0 {
                -1
            } else {
                on(Op::WmPopupsOf, on(Op::WmDefault, 0))
            }
        }
        // `const WONRectangle& aBounds = a->GetBounds()`, its x, y, width and height into `(int*)b`
        Op::Bounds => {
            let fields = [Op::BoundsX, Op::BoundsY, Op::BoundsWidth, Op::BoundsHeight];
            for (i, f) in fields.into_iter().enumerate() {
                let v = on(f, a) as i32;
                // SAFETY: the caller's four ints (dlglogic.rs's rectangles)
                unsafe { *(b as *mut i32).add(i) = v };
            }
            0
        }
        // `SmartPtr<Window> aNew(CreateNativeWindow()); aNew->SetOneComponentWindow(true); aNew->AddRef();`, the
        // window returned with the Rust's reference, then the local's reference released
        Op::NewWindow => {
            on(Op::NewWindowMake, 0);
            on(Op::NewWindowOneComponent, 0);
            let w = on(Op::NewWindowGet, 0);
            on(Op::AddRefWindow, w);
            on(Op::NewWindowDrop, 0);
            w
        }
        Op::SetUpPopup => {
            out(Op::WindowSetParent, a, b, 0, 0);
            on(Op::WindowSetPopupStyle, a);
            on(Op::WindowCreatePopup, a);
            on(Op::WindowListenRoot, a);
            on(Op::WindowRootSetScheme, a);
            0
        }
        // `GUIString aStatus = GetWONStatusString(b); a->SetStatus(aStatus, false)`
        Op::StatusSetStatus => {
            on(Op::StatusTextOf, b);
            on(Op::StatusSetHeldText, a);
            on(Op::StatusTextDrop, 0);
            0
        }
        // `const GUIString* aButtons[2] = {&(Yes or OK), &(No or Cancel)}; a->SetText(b, c, aButtons)`
        Op::OkCancelSetText => {
            let (first, second) = if d != 0 {
                (R::LobbyGlobal_Yes_String, R::LobbyGlobal_No_String)
            } else {
                (R::LobbyGlobal_OK_String, R::LobbyGlobal_Cancel_String)
            };
            out(Op::ButtonTextSet, 0, first as isize, 0, 0);
            out(Op::ButtonTextSet, 1, second as isize, 0, 0);
            out(Op::OkCancelSetHeldText, a, b, c, 0);
            0
        }
        _ => return None,
    };
    Some(r)
}

/// One op as the game answers it, for the driver (won_logic35_test): the composition over `world`'s one-statement
/// cases, or `world`'s own case.
///
/// # Safety
/// `world` answers the ops as DialogLogic.cpp's host; `Bounds`'s `b` holds four ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_logicsplit_dlg_op(
    world: crate::dlglogic::EeWonDlgLogicHostFn,
    logic: *mut core::ffi::c_void,
    op: crate::dlglogic::EeWonDlgLogicOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    let Some(f) = world else { return 0 };
    // SAFETY: the caller's world
    let out = |op: Op, a: isize, b: isize, c: isize, d: isize| unsafe { f(logic, op, a, b, c, d) };
    dlg_op(op, a, b, c, d, &out).unwrap_or_else(|| out(op, a, b, c, d))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    thread_local! {
        static LOG: RefCell<Vec<(Op, isize, isize, isize)>> = const { RefCell::new(Vec::new()) };
    }

    fn world(op: Op, a: isize, b: isize, c: isize, _d: isize) -> isize {
        LOG.with(|l| l.borrow_mut().push((op, a, b, c)));
        match op {
            Op::WmDefault => 77,
            Op::WmPopupsOf => 1,
            Op::BoundsX => 1,
            Op::BoundsY => 2,
            Op::BoundsWidth => 30,
            Op::BoundsHeight => 40,
            Op::NewWindowGet => 9,
            _ => 0,
        }
    }

    fn run(op: Op, a: isize, b: isize, c: isize, d: isize) -> (Option<isize>, Vec<(Op, isize, isize, isize)>) {
        LOG.with(|l| l.borrow_mut().clear());
        let r = dlg_op(op, a, b, c, d, &world);
        (r, LOG.with(|l| l.borrow().clone()))
    }

    #[test]
    fn the_popups_flag_and_the_bounds() {
        let (r, log) = run(Op::WmPopups, 0, 0, 0, 0);
        assert_eq!(r, Some(1));
        assert_eq!(log, vec![(Op::WmDefault, 0, 0, 0), (Op::WmDefault, 0, 0, 0), (Op::WmPopupsOf, 77, 0, 0)]);
        let mut rect = [0i32; 4];
        let (r, _) = run(Op::Bounds, 5, rect.as_mut_ptr() as isize, 0, 0);
        assert_eq!(r, Some(0));
        assert_eq!(rect, [1, 2, 30, 40]);
    }

    #[test]
    fn the_window_is_handed_out_with_a_reference() {
        let (r, log) = run(Op::NewWindow, 0, 0, 0, 0);
        assert_eq!(r, Some(9));
        let ops: Vec<Op> = log.iter().map(|e| e.0).collect();
        assert_eq!(
            ops,
            vec![Op::NewWindowMake, Op::NewWindowOneComponent, Op::NewWindowGet, Op::AddRefWindow, Op::NewWindowDrop]
        );
        assert_eq!(log[3].1, 9);
    }

    #[test]
    fn the_buttons_follow_the_yes_no_flag() {
        let (_, log) = run(Op::OkCancelSetText, 3, 4, 5, 1);
        assert_eq!(log[0], (Op::ButtonTextSet, 0, R::LobbyGlobal_Yes_String as isize, 0));
        assert_eq!(log[1], (Op::ButtonTextSet, 1, R::LobbyGlobal_No_String as isize, 0));
        assert_eq!(log[2], (Op::OkCancelSetHeldText, 3, 4, 5));
        let (_, log) = run(Op::OkCancelSetText, 3, 4, 5, 0);
        assert_eq!(log[0].2, R::LobbyGlobal_OK_String as isize);
        assert_eq!(log[1].2, R::LobbyGlobal_Cancel_String as isize);
        assert_eq!(run(Op::Width, 0, 0, 0, 0).0, None);
    }
}
