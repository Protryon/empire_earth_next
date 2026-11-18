//! `EEUIFMainGame`'s and its main panel group's (`EEUIFMGMainGroup`) plain members as Rust records the C++ classes derive from (the UI thread lane, round 1:
//! `class EEUIFMainGame : public UIForm, public EeScreensMainFormFields`, ee/EEUIFMainGame.h). The members keep
//! their C++ names, types and order (the original's +0x520 `mKeyboardModes` .. +0x5f3 `mIsButtonBlinking`), so the
//! C++ that reads them by name is unchanged. The form's host's field ops (`EE_SCREENS_MF_OP_FIELD`, `SET_FIELD`)
//! and the modes' setters (`MOUSE_TEST`, `KEYBOARD_TEST`) read and write the record here, on the call's form; the
//! record's place in the class is registered by the constructor (`ee_screens_main_form_fields`).

#![allow(non_snake_case)]

use core::ffi::c_void;
use std::os::raw::{c_long, c_ulong};
use std::sync::atomic::{AtomicUsize, Ordering};

use crate::screens::main_form::*;

/// `EEUIFMainGame`'s plain members (ee/EEUIFMainGame.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensMainFormFields {
    /// the keyboard and mouse modes of each interface state (`EE_SCREENS_MF_MODE_*`; 2: the cinematic's keys)
    pub mKeyboardModes: [c_long; 22],
    pub mMouseModes: [c_long; 22],
    /// the interface state (5: gate, 6: wall, 0x11: no scrolling)
    pub mUIState: c_long,
    /// the background media (-1: none)
    pub mMediaID: c_long,
    /// the enabled entity group and the index into the panel groups
    pub mEntityGroup: c_long,
    pub mCurrentGroup: c_ulong,
    /// the poll's elapsed time
    pub mElapsed: c_ulong,
    pub mHotKeyMode: c_ulong,
    pub mClockMode: c_long,
    pub mEntityGroupSet: bool,
    /// the error log's strings changed
    pub mStringsChanged: bool,
    pub mIsCursorLocked: bool,
    pub mRefreshGroup: bool,
    pub mB5F0: bool,
    pub mB5F1: bool,
    pub mInfoBlinking: bool,
    pub mIsButtonBlinking: bool,
}

/// The record's offset in `EEUIFMainGame` (the C++ registers it; one class, one offset).
static OFFSET: AtomicUsize = AtomicUsize::new(usize::MAX);

/// `EEUIFMainGame`'s constructor: the form `form` and its record `fields` (the record's offset is the class's).
#[no_mangle]
pub extern "C" fn ee_screens_main_form_fields(form: *mut c_void, fields: *mut EeScreensMainFormFields) {
    OFFSET.store(fields as usize - form as usize, Ordering::Relaxed);
}

/// The record of the form `form` (an `EEUIFMainGame*`); None before the offset is registered or for NULL.
pub(crate) fn record(form: *mut c_void) -> Option<*mut EeScreensMainFormFields> {
    let offset = OFFSET.load(Ordering::Relaxed);
    (!form.is_null() && offset != usize::MAX).then(|| (form as usize + offset) as *mut EeScreensMainFormFields)
}

/// `EE_SCREENS_MF_OP_FIELD` (`set` None) or `SET_FIELD` (`set` the value) of the field `id` on the record: the
/// C++'s `member = (decltype(member))value; return (int64_t)member;`. None for a field the record does not hold
/// (the gate's direction, the wall's state, the tooltip shadow, the build's tech: the C++'s).
pub(crate) fn field(r: &mut EeScreensMainFormFields, id: i64, set: Option<i64>) -> Option<i64> {
    macro_rules! long {
        ($m:ident) => {{
            if let Some(v) = set {
                r.$m = v as c_long;
            }
            r.$m as i64
        }};
    }
    macro_rules! ulong {
        ($m:ident) => {{
            if let Some(v) = set {
                r.$m = v as c_ulong;
            }
            r.$m as i64
        }};
    }
    macro_rules! flag {
        ($m:ident) => {{
            if let Some(v) = set {
                r.$m = v != 0;
            }
            r.$m as i64
        }};
    }
    Some(match id {
        EE_SCREENS_MF_FIELD_STRINGS_CHANGED => flag!(mStringsChanged),
        EE_SCREENS_MF_FIELD_ELAPSED => ulong!(mElapsed),
        EE_SCREENS_MF_FIELD_REFRESH_GROUP => flag!(mRefreshGroup),
        EE_SCREENS_MF_FIELD_UI_STATE => long!(mUIState),
        EE_SCREENS_MF_FIELD_CLOCK_MODE => long!(mClockMode),
        EE_SCREENS_MF_FIELD_CURSOR_LOCKED => flag!(mIsCursorLocked),
        EE_SCREENS_MF_FIELD_INFO_BLINKING => flag!(mInfoBlinking),
        EE_SCREENS_MF_FIELD_BUTTON_BLINKING => flag!(mIsButtonBlinking),
        EE_SCREENS_MF_FIELD_ENTITY_GROUP => long!(mEntityGroup),
        EE_SCREENS_MF_FIELD_CURRENT_GROUP => ulong!(mCurrentGroup),
        EE_SCREENS_MF_FIELD_MEDIA_ID => long!(mMediaID),
        EE_SCREENS_MF_FIELD_ENTITY_GROUP_SET => flag!(mEntityGroupSet),
        EE_SCREENS_MF_FIELD_B5F0 => flag!(mB5F0),
        EE_SCREENS_MF_FIELD_HOT_KEY_MODE => ulong!(mHotKeyMode),
        _ => return None,
    })
}

/// The form host's ops on the record: the fields, and the modes' setters (`mMouseModes[a] = b`,
/// `mKeyboardModes[a] = b`); None for the host's others.
pub(crate) fn op(r: &mut EeScreensMainFormFields, op: u32, a: i64, b: i64) -> Option<i64> {
    match op {
        EE_SCREENS_MF_OP_FIELD => field(r, a, None),
        EE_SCREENS_MF_OP_SET_FIELD => field(r, a, Some(b)),
        EE_SCREENS_MF_OP_MOUSE_TEST => {
            r.mMouseModes[a as usize] = b as c_long;
            Some(0)
        }
        EE_SCREENS_MF_OP_KEYBOARD_TEST => {
            r.mKeyboardModes[a as usize] = b as c_long;
            Some(0)
        }
        _ => None,
    }
}

/// `EEUIFMGMainGroup`'s plain members (ee/EEUIFMGMainGroup.h, the original's +0x0c `mMode` .. +0x25 `mReset`), a
/// Rust record the C++ class derives from as `EeScreensMainFormFields` is the form's.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensMainGroupFields {
    /// the mode: 0, 3, 4 (the market's unit type 0x321); 1, 2 tested
    pub mMode: c_long,
    /// the shown unit's hit points, id and the previous id
    pub mHitPoints: c_long,
    pub mUnitID: c_ulong,
    pub mPreviousID: c_ulong,
    pub mL1C: c_long,
    /// the unit class's `mL294`
    pub mL20: c_long,
    /// a unit is shown; the group was reset (`Reset`)
    pub mHasUnit: bool,
    pub mReset: bool,
}

/// The record's offset in `EEUIFMGMainGroup`.
static GROUP_OFFSET: AtomicUsize = AtomicUsize::new(usize::MAX);

/// `EEUIFMGMainGroup`'s constructor: the group `group` and its record `fields`.
#[no_mangle]
pub extern "C" fn ee_screens_main_group_fields(group: *mut c_void, fields: *mut EeScreensMainGroupFields) {
    GROUP_OFFSET.store(fields as usize - group as usize, Ordering::Relaxed);
}

/// The record of the group `group` (an `EEUIFMGMainGroup*`).
pub(crate) fn group_record(group: *mut c_void) -> Option<*mut EeScreensMainGroupFields> {
    let offset = GROUP_OFFSET.load(Ordering::Relaxed);
    (!group.is_null() && offset != usize::MAX).then(|| (group as usize + offset) as *mut EeScreensMainGroupFields)
}

/// The main group host's `EE_SCREENS_MGR_OP_FIELD` / `SET_FIELD` on the record (the C++'s
/// `member = (decltype(member))value; return (int64_t)member;`); None for its other ops.
pub(crate) fn group_op(r: &mut EeScreensMainGroupFields, op: u32, a: i64, b: i64) -> Option<i64> {
    use crate::screens::main_group::*;
    let set = match op {
        EE_SCREENS_MGR_OP_FIELD => None,
        EE_SCREENS_MGR_OP_SET_FIELD => Some(b),
        _ => return None,
    };
    macro_rules! put {
        ($m:ident, $t:ty) => {{
            if let Some(v) = set {
                r.$m = v as $t;
            }
            r.$m as i64
        }};
    }
    Some(match a {
        EE_SCREENS_MGR_FIELD_MODE => put!(mMode, c_long),
        EE_SCREENS_MGR_FIELD_HIT_POINTS => put!(mHitPoints, c_long),
        EE_SCREENS_MGR_FIELD_UNIT_ID => put!(mUnitID, c_ulong),
        EE_SCREENS_MGR_FIELD_PREVIOUS_ID => put!(mPreviousID, c_ulong),
        EE_SCREENS_MGR_FIELD_L1C => put!(mL1C, c_long),
        EE_SCREENS_MGR_FIELD_L20 => put!(mL20, c_long),
        EE_SCREENS_MGR_FIELD_HAS_UNIT => {
            if let Some(v) = set {
                r.mHasUnit = v != 0;
            }
            r.mHasUnit as i64
        }
        EE_SCREENS_MGR_FIELD_RESET => {
            if let Some(v) = set {
                r.mReset = v != 0;
            }
            r.mReset as i64
        }
        // (the C++ answered 0 for another field)
        _ => 0,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fields_keep_the_cpp_conversions() {
        let mut r = EeScreensMainFormFields::default();
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_SET_FIELD, EE_SCREENS_MF_FIELD_UI_STATE, 6), Some(6));
        assert_eq!(r.mUIState, 6);
        // a bool stores != 0 and answers 0 or 1
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_SET_FIELD, EE_SCREENS_MF_FIELD_CURSOR_LOCKED, 5), Some(1));
        assert!(r.mIsCursorLocked);
        // an unsigned long answers zero-extended, a long sign-extended
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_SET_FIELD, EE_SCREENS_MF_FIELD_ELAPSED, -1), Some(c_ulong::MAX as i64));
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_SET_FIELD, EE_SCREENS_MF_FIELD_MEDIA_ID, -1), Some(-1));
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_FIELD, EE_SCREENS_MF_FIELD_MEDIA_ID, 0), Some(-1));
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_FIELD, EE_SCREENS_MF_FIELD_HOT_KEY_MODE, 0), Some(0));
        // the gate's direction and the others stay the C++'s
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_FIELD, EE_SCREENS_MF_FIELD_GATE_DIRECTION, 0), None);
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_FIELD, EE_SCREENS_MF_FIELD_TOOLTIP_SHADOW, 0), None);
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_MOUSE_TEST, 21, 4), Some(0));
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_KEYBOARD_TEST, 0, 2), Some(0));
        assert_eq!((r.mMouseModes[21], r.mKeyboardModes[0]), (4, 2));
        assert_eq!(op(&mut r, EE_SCREENS_MF_OP_POLL, 0, 0), None);
    }

    #[test]
    fn the_record_follows_the_registered_offset() {
        assert!(record(core::ptr::null_mut()).is_none());
        ee_screens_main_form_fields(0x2000 as *mut c_void, 0x2040 as *mut EeScreensMainFormFields);
        assert_eq!(record(0x1000 as *mut c_void), Some(0x1040 as *mut EeScreensMainFormFields));
        assert!(group_record(core::ptr::null_mut()).is_none());
        ee_screens_main_group_fields(0x3000 as *mut c_void, 0x3008 as *mut EeScreensMainGroupFields);
        assert_eq!(group_record(0x10 as *mut c_void), Some(0x18 as *mut EeScreensMainGroupFields));
    }

    #[test]
    fn the_group_fields() {
        use crate::screens::main_group::*;
        let mut r = EeScreensMainGroupFields::default();
        assert_eq!(
            group_op(&mut r, EE_SCREENS_MGR_OP_SET_FIELD, EE_SCREENS_MGR_FIELD_UNIT_ID, -1),
            Some(c_ulong::MAX as i64)
        );
        assert_eq!(group_op(&mut r, EE_SCREENS_MGR_OP_SET_FIELD, EE_SCREENS_MGR_FIELD_HIT_POINTS, -3), Some(-3));
        assert_eq!(group_op(&mut r, EE_SCREENS_MGR_OP_SET_FIELD, EE_SCREENS_MGR_FIELD_RESET, 9), Some(1));
        assert_eq!(group_op(&mut r, EE_SCREENS_MGR_OP_FIELD, EE_SCREENS_MGR_FIELD_HAS_UNIT, 0), Some(0));
        assert_eq!(group_op(&mut r, EE_SCREENS_MGR_OP_FIELD, 99, 0), Some(0));
        assert_eq!(group_op(&mut r, EE_SCREENS_MGR_OP_UI_STATE, 0, 0), None);
        assert!(r.mReset && r.mHitPoints == -3);
    }
}
