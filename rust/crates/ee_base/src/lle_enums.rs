//! The LLE's named integer constants, as plain (unscoped) C enums for its C++ headers; each comment gives the
//! evidence for its name, "(provisional name)" marks an inferred one.
#![allow(non_camel_case_types, clippy::enum_variant_names, clippy::upper_case_acronyms)]

/// ISKeyboard entity index: the DirectInput DIK_* scan code; 0xe0..0xe2 the either-side ISExtendedKeys; 0x7fff no key
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ISKeyCode {
    /// DirectInput 7 dinput.h DIK_ESCAPE = 0x01
    kKeyEscape = 0x01,
    /// DirectInput 7 dinput.h DIK_1 = 0x02
    kKey1 = 0x02,
    /// DirectInput 7 dinput.h DIK_2 = 0x03
    kKey2 = 0x03,
    /// DirectInput 7 dinput.h DIK_3 = 0x04
    kKey3 = 0x04,
    /// DirectInput 7 dinput.h DIK_4 = 0x05
    kKey4 = 0x05,
    /// DirectInput 7 dinput.h DIK_5 = 0x06
    kKey5 = 0x06,
    /// DirectInput 7 dinput.h DIK_6 = 0x07
    kKey6 = 0x07,
    /// DirectInput 7 dinput.h DIK_7 = 0x08
    kKey7 = 0x08,
    /// DirectInput 7 dinput.h DIK_8 = 0x09
    kKey8 = 0x09,
    /// DirectInput 7 dinput.h DIK_BACK = 0x0e
    kKeyBack = 0x0e,
    /// DirectInput 7 dinput.h DIK_TAB = 0x0f
    kKeyTab = 0x0f,
    /// DirectInput 7 dinput.h DIK_Q = 0x10
    kKeyQ = 0x10,
    /// DirectInput 7 dinput.h DIK_W = 0x11
    kKeyW = 0x11,
    /// DirectInput 7 dinput.h DIK_R = 0x13
    kKeyR = 0x13,
    /// DirectInput 7 dinput.h DIK_T = 0x14
    kKeyT = 0x14,
    /// DirectInput 7 dinput.h DIK_Y = 0x15
    kKeyY = 0x15,
    /// DirectInput 7 dinput.h DIK_U = 0x16
    kKeyU = 0x16,
    /// DirectInput 7 dinput.h DIK_I = 0x17
    kKeyI = 0x17,
    /// DirectInput 7 dinput.h DIK_LBRACKET = 0x1a
    kKeyLBracket = 0x1a,
    /// DirectInput 7 dinput.h DIK_RBRACKET = 0x1b
    kKeyRBracket = 0x1b,
    /// DirectInput 7 dinput.h DIK_RETURN = 0x1c
    kKeyReturn = 0x1c,
    /// DirectInput 7 dinput.h DIK_A = 0x1e
    kKeyA = 0x1e,
    /// DirectInput 7 dinput.h DIK_S = 0x1f
    kKeyS = 0x1f,
    /// DirectInput 7 dinput.h DIK_D = 0x20
    kKeyD = 0x20,
    /// DirectInput 7 dinput.h DIK_F = 0x21
    kKeyF = 0x21,
    /// DirectInput 7 dinput.h DIK_G = 0x22
    kKeyG = 0x22,
    /// DirectInput 7 dinput.h DIK_H = 0x23
    kKeyH = 0x23,
    /// DirectInput 7 dinput.h DIK_L = 0x26
    kKeyL = 0x26,
    /// DirectInput 7 dinput.h DIK_SEMICOLON = 0x27
    kKeySemicolon = 0x27,
    /// DirectInput 7 dinput.h DIK_GRAVE = 0x29
    kKeyGrave = 0x29,
    /// DirectInput 7 dinput.h DIK_BACKSLASH = 0x2b
    kKeyBackslash = 0x2b,
    /// DirectInput 7 dinput.h DIK_Z = 0x2c
    kKeyZ = 0x2c,
    /// DirectInput 7 dinput.h DIK_X = 0x2d
    kKeyX = 0x2d,
    /// DirectInput 7 dinput.h DIK_C = 0x2e
    kKeyC = 0x2e,
    /// DirectInput 7 dinput.h DIK_V = 0x2f
    kKeyV = 0x2f,
    /// DirectInput 7 dinput.h DIK_B = 0x30
    kKeyB = 0x30,
    /// DirectInput 7 dinput.h DIK_N = 0x31
    kKeyN = 0x31,
    /// DirectInput 7 dinput.h DIK_M = 0x32
    kKeyM = 0x32,
    /// DirectInput 7 dinput.h DIK_COMMA = 0x33
    kKeyComma = 0x33,
    /// DirectInput 7 dinput.h DIK_PERIOD = 0x34
    kKeyPeriod = 0x34,
    /// DirectInput 7 dinput.h DIK_SLASH = 0x35
    kKeySlash = 0x35,
    /// DirectInput 7 dinput.h DIK_SPACE = 0x39
    kKeySpace = 0x39,
    /// DirectInput 7 dinput.h DIK_F1 = 0x3b
    kKeyF1 = 0x3b,
    /// DirectInput 7 dinput.h DIK_F2 = 0x3c
    kKeyF2 = 0x3c,
    /// DirectInput 7 dinput.h DIK_F3 = 0x3d
    kKeyF3 = 0x3d,
    /// DirectInput 7 dinput.h DIK_F4 = 0x3e
    kKeyF4 = 0x3e,
    /// DirectInput 7 dinput.h DIK_F5 = 0x3f
    kKeyF5 = 0x3f,
    /// DirectInput 7 dinput.h DIK_F9 = 0x43
    kKeyF9 = 0x43,
    /// DirectInput 7 dinput.h DIK_F10 = 0x44
    kKeyF10 = 0x44,
    /// DirectInput 7 dinput.h DIK_NUMPAD7 = 0x47
    kKeyNumpad7 = 0x47,
    /// DirectInput 7 dinput.h DIK_NUMPAD8 = 0x48
    kKeyNumpad8 = 0x48,
    /// DirectInput 7 dinput.h DIK_SUBTRACT = 0x4a
    kKeySubtract = 0x4a,
    /// DirectInput 7 dinput.h DIK_NUMPAD4 = 0x4b
    kKeyNumpad4 = 0x4b,
    /// DirectInput 7 dinput.h DIK_NUMPAD6 = 0x4d
    kKeyNumpad6 = 0x4d,
    /// DirectInput 7 dinput.h DIK_ADD = 0x4e
    kKeyAdd = 0x4e,
    /// DirectInput 7 dinput.h DIK_NUMPAD1 = 0x4f
    kKeyNumpad1 = 0x4f,
    /// DirectInput 7 dinput.h DIK_NUMPAD2 = 0x50
    kKeyNumpad2 = 0x50,
    /// DirectInput 7 dinput.h DIK_DECIMAL = 0x53
    kKeyDecimal = 0x53,
    /// DirectInput 7 dinput.h DIK_F11 = 0x57
    kKeyF11 = 0x57,
    /// DirectInput 7 dinput.h DIK_NUMPADENTER = 0x9c
    kKeyNumpadEnter = 0x9c,
    /// DirectInput 7 dinput.h DIK_HOME = 0xc7
    kKeyHome = 0xc7,
    /// DirectInput 7 dinput.h DIK_UP = 0xc8
    kKeyUp = 0xc8,
    /// DirectInput 7 dinput.h DIK_PRIOR = 0xc9
    kKeyPrior = 0xc9,
    /// DirectInput 7 dinput.h DIK_LEFT = 0xcb
    kKeyLeft = 0xcb,
    /// DirectInput 7 dinput.h DIK_RIGHT = 0xcd
    kKeyRight = 0xcd,
    /// DirectInput 7 dinput.h DIK_END = 0xcf
    kKeyEnd = 0xcf,
    /// DirectInput 7 dinput.h DIK_DOWN = 0xd0
    kKeyDown = 0xd0,
    /// DirectInput 7 dinput.h DIK_INSERT = 0xd2
    kKeyInsert = 0xd2,
    /// DirectInput 7 dinput.h DIK_DELETE = 0xd3
    kKeyDelete = 0xd3,
    /// ISKeyboard ctor: mKeys[0xe0] = ISExtendedKey(DIK_LCONTROL, DIK_RCONTROL); ComputeModifierState bit 2 (provisional name)
    kKeyControl = 0xe0,
    /// ISKeyboard ctor: mKeys[0xe1] = ISExtendedKey(DIK_LSHIFT, DIK_RSHIFT); ComputeModifierState bit 1; UIForm sShiftKey (provisional name)
    kKeyShift = 0xe1,
    /// ISKeyboard ctor: mKeys[0xe2] = ISExtendedKey(DIK_LMENU, DIK_RMENU); ComputeModifierState bit 4 (provisional name)
    kKeyAlt = 0xe2,
    /// UIForm::AddHotKey sets a modifier only when < 0x7fff; UIHotKey::CreateHotKeys: 0x7fff = no modifier (provisional name)
    kKeyNone = 0x7fff,
}

/// ISKeyboard::mModifierState bits (ISKeyboard::ComputeModifierState)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ISModifierState {
    /// ComputeModifierState: DIK_LSHIFT/DIK_RSHIFT/0xe1
    kModifierShift = 1,
    /// ComputeModifierState: DIK_LCONTROL/DIK_RCONTROL/0xe0
    kModifierControl = 2,
    /// ComputeModifierState: DIK_LMENU/DIK_RMENU/0xe2
    kModifierAlt = 4,
}

/// ISMouse entity index (GetEntity): the names are Rust's
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ISMouseEntity {
    /// "X Axis" (DIMOFS_X)
    kMouseXAxis = 0,
    /// "Y Axis" (DIMOFS_Y)
    kMouseYAxis = 1,
    /// "Z Axis" (DIMOFS_Z, the wheel)
    kMouseZAxis = 2,
    /// "Left button"
    kMouseLeftButton = 3,
    /// "Right button"
    kMouseRightButton = 4,
    /// "Middle button"
    kMouseMiddleButton = 5,
    /// "Other button"
    kMouseOtherButton = 6,
}

/// FSFile::SetPosition inOrigin: FSFileSpec::SetPosition maps it to the SetFilePointer move method
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FSFileOrigin {
    /// FSFileSpec::SetPosition -> FILE_BEGIN
    kFileOriginBegin = 0,
    /// FSFileSpec::SetPosition -> FILE_END
    kFileOriginEnd = 1,
    /// FSFileSpec::SetPosition -> FILE_CURRENT
    kFileOriginCurrent = 2,
}

/// UMSetFloatingPointPrecision / UMGetFloatingPointPrecision: the float.h _PC_* precision-control value (_controlfp(x, _MCW_PC), UMath.cpp)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UMFloatingPointPrecision {
    /// float.h _PC_64
    kFPPrecision64 = 0,
    /// float.h _PC_53
    kFPPrecision53 = 0x10000,
    /// float.h _PC_24
    kFPPrecision24 = 0x20000,
}

/// UGetOSType (Utilities.cpp 0x100837eb) results from GetVersion. Names are provisional.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UOSType {
    /// UGetOSType 0x100837eb returns 1 for Win32 platform, version 4.0..4.9 (95) (provisional name)
    kOSWindows95 = 1,
    /// UGetOSType 0x100837eb returns 2 for Win32 platform, 4.10+ (98/ME) (provisional name)
    kOSWindows98 = 2,
    /// UGetOSType 0x100837eb returns 3 for NT platform, major >= 5 (2000/XP) (provisional name)
    kOSWindowsNT5 = 3,
    /// UGetOSType 0x100837eb returns 4 for NT major < 5, or Win32s/major < 4 (provisional name)
    kOSWindowsNT4 = 4,
}

/// SEConductor::TransitionTo inCommand: the transition pattern (DirectMusic embellishment) the conductor plays
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SETransitionCommand {
    /// DmEmbellishment_GROOVE (default)
    kMusicTransitionGroove = 0,
    /// DmEmbellishment_FILL
    kMusicTransitionFill = 1,
    /// DmEmbellishment_BREAK
    kMusicTransitionBreak = 2,
    /// DmEmbellishment_INTRO
    kMusicTransitionIntro = 3,
    /// DmEmbellishment_END
    kMusicTransitionEnd = 4,
    /// end only (the engine has no end-and-intro)
    kMusicTransitionEndAndIntro = 5,
}

/// SEConductor::TransitionTo inFlags: the boundary (DirectMusic's composer flags; the engine transitions on a measure)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SETransitionFlags {
    kTransitionFlagMeasure = 0,
    kTransitionFlagModulate = 1,
    kTransitionFlagLong = 2,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_values_are_the_originals() {
        for (value, original) in [(ISKeyCode::kKeyA as u32, 0x1e), (ISKeyCode::kKeyNone as u32, 0x7fff), (ISModifierState::kModifierAlt as u32, 4), (ISMouseEntity::kMouseOtherButton as u32, 6), (FSFileOrigin::kFileOriginCurrent as u32, 2), (UMFloatingPointPrecision::kFPPrecision24 as u32, 0x20000), (UOSType::kOSWindowsNT4 as u32, 4), (SETransitionCommand::kMusicTransitionEndAndIntro as u32, 5), (SETransitionFlags::kTransitionFlagLong as u32, 2)] {
            assert_eq!(value, original);
        }
        assert_eq!(std::mem::size_of::<ISKeyCode>(), 4);
    }
}
