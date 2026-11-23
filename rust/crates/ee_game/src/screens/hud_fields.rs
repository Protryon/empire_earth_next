//! The game UI classes' members as Rust records the C++ classes derive from (UI lane round 15), under their C++
//! names: the plain values as they were, the C++ objects as untyped pointers the C++ casts at its uses (the
//! pattern of `EeScreensMainFormFields`, main_form_fields.rs). Each record is the class's second base, made zero by
//! the class's constructor.

#![allow(non_snake_case)]

use core::cell::UnsafeCell;
use core::ffi::c_void;

use ee_base::math::types::GeRectangle;
use std::os::raw::{c_long, c_ulong};

/// `EEUIBUpgrade`'s members (ee/EEUIBUpgrade.h).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensUpgradeFields {
    /// the improvement (0x10: help string only, 0xc: no cost)
    pub mAttribute: c_long,
    /// a `UITextLabel*`
    pub mCostLabel: *mut c_void,
    /// `TMGetTime` of the last "cannot" chat line
    pub mLastMessageTime: c_ulong,
    /// the level last shown
    pub mLevel: c_long,
    pub mNumQueued: c_long,
    /// the button is on the scenario editor's form
    pub mScenarioEditor: bool,
    /// `GetDisabled()` as of the last UpdateCost
    pub mDisabled: bool,
}

/// `EEUIFMGStatusBar`'s members (ee/EEUIFMGStatusBar.h): its parts (a `UIBitmap*` icon, two `UITextLabel*` value
/// labels, a `UIStatusBar*`, two `GEBitmap*`s the destructor dereferences, the queued count's `GEText*`) and flags.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensStatusBarFields {
    pub mIcon: *mut c_void,
    pub mValue: *mut c_void,
    pub mValue2: *mut c_void,
    pub mBar: *mut c_void,
    pub mBitmap140: *mut c_void,
    pub mBitmap144: *mut c_void,
    pub mCount: *mut c_void,
    /// the icon is moved by (1, 1)
    pub mPressed: bool,
    pub mShowCount: bool,
    /// `mEnabled` as of the last CopyData (DrawPolygons)
    pub mCopiedEnabled: bool,
    pub mEnabled: bool,
}

/// `EEUIMiniMap`'s members but its origin and lines (ee/EEUIMiniMap.h): the `EEAMiniMap*` action, the view
/// edges drawn, the flare.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensMiniMapFields {
    pub mAction: *mut c_void,
    pub mDrawLine0: bool,
    pub mDrawLine1: bool,
    pub mDrawLine2: bool,
    pub mDrawLine3: bool,
    /// gMiniMap's flare is on (and the camera is not free)
    pub mShowFlare: bool,
    /// toggles the flare colour each CopyData
    pub mFlareBlink: bool,
}

/// `EEUISCIFormManager`'s flags (ee/EEUISCIFormManager.h).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensSciFormFields {
    /// (Clear)
    pub mBA0: bool,
    /// the in-game LoadScenario clears it; SIE Back closes the form
    pub mIsInGame: bool,
    /// and sets this one; SIE Start posts the saved game
    pub mIsLoadingFromGame: bool,
    /// the CSSE StartScenario sets it: a campaign scenario
    pub mIsCampaign: bool,
    /// SIE Open sets it, Briefing scrolls to the end and clears it
    pub mScrollBriefingToEnd: bool,
}

/// `EEUICPortrait`'s members but its unit (ee/EEUICPortrait.h): the level's `GEText*`, the face's `GEBitmap*`,
/// the unit class's `EEButtonObject*` (loaded while shown), the level, the bars and the level shown.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensPortraitFields {
    pub mText: *mut c_void,
    pub mPortrait: *mut c_void,
    pub mButton: *mut c_void,
    pub mLevel: c_ulong,
    pub mShowFirstBar: bool,
    pub mShowSecondBar: bool,
    pub mShowLevel: bool,
}

/// `EEUIFMGAttribute`'s members (ee/EEUIFMGAttribute.h): the level's `GEText*`, the level shown.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensAttributeFields {
    pub mLevelText: *mut c_void,
    /// `mL128 > 0` as of the last CopyData
    pub mShowLevel: bool,
}

/// `EEUIFMGStatusButton`'s C++ objects (ee/EEUIFMGStatusButton.h): the `EEButtonObject*`, the dither and icon
/// `GEBitmap*`s, the queue count's `GEText*`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensStatusButtonObjects {
    pub mButtonObject: *mut c_void,
    /// textures\Dither over a disabled button
    pub mDither: *mut c_void,
    pub mIcon: *mut c_void,
    pub mBuildQueue: *mut c_void,
}

/// `EEUIBGenericBackground`'s numbers (ee/EEUIBGenericBackground.h): the opacity, the scale, the frame flags.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensBackgroundFields {
    pub mOpacity: f32,
    /// EEUIFError::Open: the game window's width / 1024 and height / 768
    pub mScaleX: f32,
    pub mScaleY: f32,
    /// (3 from EEUIFError)
    pub mFrameFlags: c_ulong,
}

/// `EEUIFMGUnitHack`'s members (ee/EEUIFMGUnitHack.h): its `EEUIFMainGame*` and its Rust record.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensUnitHackFields {
    pub mForm: *mut c_void,
    pub mRecord: *mut c_void,
}

/// The box selection's and the right-drag command's C++ objects (ee/EEUICSelectUnits.h, EEUICCommandUnits.h):
/// the `EEAMouseClick*` (the click; the command's listener), the `EEALasso*`, the arrow's `GEModel*` and
/// `GETexture*`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensSelectUnitsObjects {
    pub mClick: *mut c_void,
    pub mLasso: *mut c_void,
}

/// The right-drag command's: its listener, the arrow's model and texture.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensCommandUnitsObjects {
    pub mListener: *mut c_void,
    pub mModel: *mut c_void,
    pub mTexture: *mut c_void,
}

/// One loaded icon of the status box (the original's `EEUIFMGStatusBox::sIconBitmaps[50]`, 0x14 bytes each): its
/// cell in the icon textures and its `GEBitmap*`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeScreensIconBitmap {
    pub mRectangle: GeRectangle,
    pub mBitmap: *mut c_void,
}

/// The status box's icons (`LoadIcons` fills them; the UI and graphics threads read them).
const ICON_COUNT: usize = 50;
struct Icons(UnsafeCell<[EeScreensIconBitmap; ICON_COUNT]>);
// SAFETY: the C++ reaches the icons through the pointer below, as it did its static array
unsafe impl Sync for Icons {}
// SAFETY: a zeroed rectangle and a null pointer are the icons' values before LoadIcons
static ICONS: Icons = Icons(UnsafeCell::new(unsafe { core::mem::zeroed() }));

/// The status box's icon `icon` (0..50).
#[no_mangle]
pub extern "C" fn ee_screens_icon_bitmap(icon: i64) -> *mut EeScreensIconBitmap {
    if !(0..ICON_COUNT as i64).contains(&icon) {
        std::process::abort();
    }
    // SAFETY: the index is in the array
    unsafe { (ICONS.0.get() as *mut EeScreensIconBitmap).add(icon as usize) }
}

/// The improvement button's call record's pointers (the C++ `EEUIBUpgradeCall` derives from it; its held text and
/// reason follow): the `EEUIBUpgrade*`, the out text (a `UWideString*`: the tooltip, the refusal's reason, the new
/// cost), the mouse location (a `const GE2DPoint*`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensUpgradeCallObjects {
    pub mButton: *mut c_void,
    pub mOut: *mut c_void,
    pub mLocation: *const c_void,
}

/// The UI manager's call record's pointers (the C++ `EEUIManagerCall` derives from it; StartScenarioEditor's two
/// names follow): the `EEUIManager*`, UpdateMouse's location (a `const GE2DPoint*`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensManagerCallObjects {
    pub mManager: *mut c_void,
    pub mMouse: *const c_void,
}

/// The status box's call record's pointers (the C++ `EEUIFMGStatusBoxCall` derives from it; the held text and
/// LoadIcons' description follow, hud_direct.rs `BoxCallHead`): the box (an `EEUIFMGStatusBox*`, its record
/// first), the unit (Fill, Refresh), the complex unit (UpdateQueuedCounts), LoadIcons' four `GETexture*`s.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensStatusBoxCallObjects {
    pub mBox: *mut c_void,
    pub mUnit: *mut c_void,
    pub mComplex: *mut c_void,
    pub mTextures: [*mut c_void; 4],
}
