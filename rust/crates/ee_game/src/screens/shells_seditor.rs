//! The scenario editor lane's control kinds over the generic shells (round 1): the player tab's diplomacy buttons
//! (`EESEUIBDiplomacy`, once `Scenario Editor/EESERBUnitTypes.cpp`) and the form's Undo and Redo buttons
//! (`EEUIFSEActionButton`, once `EEUIFScenarioEditor.cpp`), each a C++ class deleted whole.
//!
//! Numbers (the lane's ranges, so the UI lane's do not collide): kinds 0x70.., kind ops 0x70.., host ops 0xa0...

use core::cell::Cell;
use core::ffi::CStr;

use crate::screens::shells::*;

/// `EESEUIBDiplomacy`: a diplomacy grid button (allies or enemies; the constructor's state enemies).
pub const EE_SCREENS_SK_DIPLOMACY: u32 = 0x70;
/// `EEUIFSEActionButton`: the Undo or Redo button (args: the button's `EEAction*`, released with the button).
pub const EE_SCREENS_SK_ACTION_BUTTON: u32 = 0x71;

/// The diplomacy button's state (`mState`: 0 allies, 1 enemies) / the state set to a, its bitmaps and help text
/// shown (`UpdateBitmaps`).
pub const EE_SCREENS_SKOP_DIPLOMACY_STATE: u32 = 0x70;
pub const EE_SCREENS_SKOP_DIPLOMACY_SET: u32 = 0x71;

/// The diplomacy button's bitmaps and help text for the state a, then the form's help text when b
/// (`EESEDiplomacyShow`).
pub const EE_SCREENS_SH_SE_DIPLOMACY: u32 = 0xa0;
/// The action a (an `EEAction*`) posted to the user interface / released (`Dereference`).
pub const EE_SCREENS_SH_SE_ACTION_POST: u32 = 0xa1;
pub const EE_SCREENS_SH_SE_ACTION_RELEASE: u32 = 0xa2;

/// `kButtonStateNormal`, `kButtonStateFocused`, `kButtonStatePressed` (lle/UIButtonState.h): the diplomacy states
/// are the first two.
const STATE_FOCUSED: u32 = 1;
const STATE_PRESSED: u32 = 2;

const fn bit(v: u32) -> u64 {
    1 << v
}

/// The kind `kind` made with the constructor's arguments `args`; `None` for a kind not of this lane.
pub fn make(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    Some(match kind {
        EE_SCREENS_SK_DIPLOMACY => Box::new(Diplomacy { state: Cell::new(STATE_FOCUSED) }),
        EE_SCREENS_SK_ACTION_BUTTON => Box::new(ActionButton { action: args.first().copied().unwrap_or(0) }),
        _ => return None,
    })
}

/// `EESEUIBDiplomacy` (vtable 0x443298, sizeof 0x108): `ButtonUp` (0x176baf) toggles the state, shows its bitmaps
/// and help text and the form's help text, then the base's.
struct Diplomacy {
    state: Cell<u32>,
}

impl Kind for Diplomacy {
    fn name(&self) -> &'static CStr {
        c"EESEUIBDiplomacy"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_BUTTON_UP)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_BUTTON_UP {
            let mut s = self.state.get().wrapping_add(1);
            if s == STATE_PRESSED {
                s = 0;
            }
            self.state.set(s);
            c.call(EE_SCREENS_SH_SE_DIPLOMACY, i64::from(s), 1);
        }
        c.base(v, a, b, f)
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_DIPLOMACY_STATE => i64::from(self.state.get()),
            EE_SCREENS_SKOP_DIPLOMACY_SET => {
                self.state.set(a as u32);
                c.call(EE_SCREENS_SH_SE_DIPLOMACY, i64::from(a as u32), 0);
                0
            }
            _ => 0,
        }
    }
}

/// `EEUIFSEActionButton` (the original EEUIFSEUndo 0x1a4dbe and EEUIFSERedo 0x1a4cbb): `ButtonUp` posts the action
/// (the UI thread's undo or redo), then the base's; the destructor releases it.
struct ActionButton {
    action: i64,
}

impl Kind for ActionButton {
    fn name(&self) -> &'static CStr {
        c"EEUIFSEActionButton"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_BUTTON_UP)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_BUTTON_UP {
            c.call(EE_SCREENS_SH_SE_ACTION_POST, self.action, 0);
        }
        c.base(v, a, b, f)
    }
    fn free(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_SE_ACTION_RELEASE, self.action, 0);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::RefCell;

    struct Rec(RefCell<Vec<(u32, i64, i64)>>);

    impl ShellHost for Rec {
        fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, _f: &mut [f32; 4]) -> i64 {
            self.0.borrow_mut().push((op, a, b));
            0
        }
    }

    #[test]
    fn the_diplomacy_button_toggles_between_allies_and_enemies() {
        let h = Rec(RefCell::new(Vec::new()));
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        let k = make(EE_SCREENS_SK_DIPLOMACY, &[]).unwrap();
        assert_eq!(k.name().to_str().unwrap(), "EESEUIBDiplomacy");
        assert_eq!(k.op(&c, EE_SCREENS_SKOP_DIPLOMACY_STATE, 0, 0, &mut [0.0; 4]), 1);
        k.virt(&c, EE_SCREENS_SV_BUTTON_UP, 0, 0, &mut [0.0; 4]);
        assert_eq!(k.op(&c, EE_SCREENS_SKOP_DIPLOMACY_STATE, 0, 0, &mut [0.0; 4]), 0);
        k.virt(&c, EE_SCREENS_SV_BUTTON_UP, 0, 0, &mut [0.0; 4]);
        assert_eq!(k.op(&c, EE_SCREENS_SKOP_DIPLOMACY_STATE, 0, 0, &mut [0.0; 4]), 1);
        k.op(&c, EE_SCREENS_SKOP_DIPLOMACY_SET, 0, 0, &mut [0.0; 4]);
        assert_eq!(
            *h.0.borrow(),
            vec![
                (EE_SCREENS_SH_SE_DIPLOMACY, 0, 1),
                (EE_SCREENS_SH_BASE + EE_SCREENS_SV_BUTTON_UP, 0, 0),
                (EE_SCREENS_SH_SE_DIPLOMACY, 1, 1),
                (EE_SCREENS_SH_BASE + EE_SCREENS_SV_BUTTON_UP, 0, 0),
                (EE_SCREENS_SH_SE_DIPLOMACY, 0, 0),
            ]
        );
    }

    #[test]
    fn the_action_button_posts_then_presses_and_releases_its_action() {
        let h = Rec(RefCell::new(Vec::new()));
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        let k = make(EE_SCREENS_SK_ACTION_BUTTON, &[0x1234]).unwrap();
        assert_eq!(k.name().to_str().unwrap(), "EEUIFSEActionButton");
        assert_eq!(k.overrides(), 1 << EE_SCREENS_SV_BUTTON_UP);
        k.virt(&c, EE_SCREENS_SV_BUTTON_UP, 0, 0, &mut [0.0; 4]);
        k.free(&c);
        assert_eq!(
            *h.0.borrow(),
            vec![
                (EE_SCREENS_SH_SE_ACTION_POST, 0x1234, 0),
                (EE_SCREENS_SH_BASE + EE_SCREENS_SV_BUTTON_UP, 0, 0),
                (EE_SCREENS_SH_SE_ACTION_RELEASE, 0x1234, 0),
            ]
        );
        assert!(make(0x7f, &[]).is_none());
    }
}
