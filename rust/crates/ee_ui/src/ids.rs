//! The toolkit's enumerations (UI lane round 13): once C++ enums in the toolkit's headers (`UIBackgroundType`,
//! `UIButtonState`, `UILayoutType`, `UITiling`, `UICheckState`, `UIControlTriggerID`, `UIControlType`, `UIDisplayMode`, `UIDbControlType`, `UIFormEventType`),
//! now Rust's. cbindgen emits each as an unscoped C++ `enum X : uint32_t` with the same names and values
//! (`cbindgen:enum-class=false`), so the C++ that names them is unchanged; the Rust tables that listed the values
//! (`tableobj.rs`, `controls_table.rs`, `formobj.rs`) read them from here.

/// UI database background type (UIBackgroundTable::mBackgroundType): CreateBackgrounds creates the class named here; others go to CreateUserDefinedBackground
/// (the C++ `enum UIBackgroundType`, UIBackgroundTable.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UIBackgroundType {
    /// CreateBackgrounds case: new UIBitmapBackground
    kBackgroundBitmap = 0,
    /// CreateBackgrounds case: new UITiledBackground
    kBackgroundTiled = 1,
    /// CreateBackgrounds case: new UIBFull
    kBackgroundFull = 2,
    /// CreateBackgrounds case: new UITiledBackground (kTilingLeftRight)
    kBackgroundLRTiled = 3,
    /// CreateBackgrounds case: new UITiledBackground (kTilingTopBottom)
    kBackgroundTBTiled = 4,
    /// CreateBackgrounds case: new UITiledBackground (kTilingStretched)
    kBackgroundStretchedTiled = 5,
    /// CreateBackgrounds case: EEUIManager::CreateUserDefinedBackground: new EEUIBGenericBackground
    kBackgroundGeneric = 101,
}

/// UIButton::mState: UIButton::GetCurrentBitmap picks the bitmap named here
/// (the C++ `enum UIButtonState`, UIButtonState.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UIButtonState {
    /// GetCurrentBitmap -> mNormalBitmap (provisional name)
    kButtonStateNormal = 0,
    /// GetCurrentBitmap -> mFocusedBitmap (MouseMove/TabbedTo) (provisional name)
    kButtonStateFocused = 1,
    /// GetCurrentBitmap -> mPressedBitmap (ButtonDown) (provisional name)
    kButtonStatePressed = 2,
}

/// UIControl trigger-event ids (FireTriggerEvent / SetTriggerEvent); provisional names from the lle firing sites
/// (the C++ `enum UIControlTriggerID`, UIControl.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UIControlTriggerID {
    /// UISlider::ChangeRange fires it (db: Game Settings Music Volume etc.) (provisional name)
    kControlTriggerRangeChanged = 0x3e8,
    /// UICheckBox::SetChecked(true), UIRadioButton::Check; UITriStateCheckBox::SetCheckState fires it for every state (db: Check Box ON) (provisional name)
    kControlTriggerChecked = 0x3e9,
    /// UICheckBox::SetChecked(false), UIRadioButton::UnCheck (db: Check Box OFF, UnCheck Box) (provisional name)
    kControlTriggerUnchecked = 0x3ea,
    /// UIListBox::ChangeSelectionIndex, UIComboBox::ChangeSelection (db: Change Focused File, Selected) (provisional name)
    kControlTriggerSelectionChanged = 0x3eb,
    /// UIListBox::MouseLButtonDoublePressed (db: Load Game By DbClick) (provisional name)
    kControlTriggerDoubleClicked = 0x3ec,
    /// UIComboBox::OpenListBox (provisional name)
    kControlTriggerListOpened = 0x3ed,
    /// UIComboBox::CloseListBox (provisional name)
    kControlTriggerListClosed = 0x3ee,
    /// UIButton::MouseLButtonDown / UpdateKeyboard key press, UIListBox::ButtonDown (next to ButtonDown()) (provisional name)
    kControlTriggerButtonDown = 0x3ef,
    /// UIButton::MouseLButtonUp inside the rectangle / key release, UIListBox::ButtonUp (next to ButtonUp(); db: Load Game, Cancel Save) (provisional name)
    kControlTriggerButtonUp = 0x3f0,
    /// UIEditBox::ProcessEnterKey, UIListBox::UpdateKeyboard Enter (db: Save Game by Enter) (provisional name)
    kControlTriggerEnterPressed = 0x3f1,
    /// UIForm::SetMouseOverControl on the new control (db: SI Map Mouse Over) (provisional name)
    kControlTriggerMouseOver = 0x3f2,
    /// UIForm::SetMouseOverControl on the old control (db: SI Map Mouse Out) (provisional name)
    kControlTriggerMouseOut = 0x3f3,
    /// UITLScrolling::CopyData end poll / StopScrolling (db: SRM Chat Dialog Finished) (provisional name)
    kControlTriggerScrollingFinished = 0x3f4,
    /// UIDualButton::MouseRButtonDown (provisional name)
    kControlTriggerRightButtonDown = 0x3f5,
    /// UIDualButton::MouseRButtonUp inside the rectangle (provisional name)
    kControlTriggerRightButtonUp = 0x3f6,
}

/// UIControl::GetType: the type each control class passes to the UIControl ctor (or its GetType override)
/// (the C++ `enum UIControlType`, UIControl.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UIControlType {
    /// the game's own controls (EEUIMiniMap, EEUICPortrait, EEUICPing, ...) pass 0 (provisional name)
    kControlTypeCustom = 0,
    /// UIButton ctor (provisional name)
    kControlTypeButton = 1,
    /// UICheckBox::GetType (provisional name)
    kControlTypeCheckBox = 2,
    /// UIComboBox ctor (provisional name)
    kControlTypeComboBox = 3,
    /// UIEditBox ctor (provisional name)
    kControlTypeEditBox = 4,
    /// UIListBox ctor (provisional name)
    kControlTypeListBox = 5,
    /// UIRadioButton::GetType (provisional name)
    kControlTypeRadioButton = 6,
    /// UIScrollBar ctor (provisional name)
    kControlTypeScrollBar = 7,
    /// UISlider ctor (provisional name)
    kControlTypeSlider = 8,
    /// UIStatusBar ctor (provisional name)
    kControlTypeStatusBar = 9,
    /// UIBitmap ctor (UIMovie passes 10 too) (provisional name)
    kControlTypeBitmap = 10,
    /// UITextLabel ctor (provisional name)
    kControlTypeTextLabel = 11,
    /// UITriStateCheckBox::GetType (provisional name)
    kControlTypeTriStateCheckBox = 12,
    /// UIGraph ctor (provisional name)
    kControlTypeGraph = 13,
}

/// UIControl::mDisplayMode (+0x08, 1 at construction): SetDisplayMode(0) hides the control (HideControl, dirty rectangle, tab moves on); setrilo ui_framework.h "0 hidden, 1 shown".
/// (the C++ `enum UIDisplayMode`, UIControl.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UIDisplayMode {
    /// setrilo ui_framework.h displayMode_ "0 hidden"; UIControl::SetDisplayMode LLE 0x10065af5 hides the control on 0
    kDisplayModeHidden = 0,
    /// setrilo ui_framework.h displayMode_ "1 shown"; the UIControl ctor sets 1
    kDisplayModeShown = 1,
}

/// UI database control type (UIControlsTable::CreateControl; >= 1000: EEUIManager::CreateUserDefinedControl)
/// (the C++ `enum UIDbControlType`, UIControlsTable.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UIDbControlType {
    /// CreateBitmap (provisional name)
    kDbControlBitmap = 0,
    /// CreateButton: new UIButton (provisional name)
    kDbControlButton = 1,
    /// CreateCheckBox (provisional name)
    kDbControlCheckBox = 2,
    /// CreateComboBox: UIComboBox text box type 0 (an edit box) (provisional name)
    kDbControlEditableComboBox = 3,
    /// CreateComboBox: UIComboBox text box type 1 (a text label; db GS Video Card Combo Box) (provisional name)
    kDbControlComboBox = 4,
    /// CreateEditBox: UIEditBox numbers only + multi-line; text set from the number (provisional name)
    kDbControlNumberMultiLineEditBox = 5,
    /// CreateEditBox: UIEditBox numbers only, single line (provisional name)
    kDbControlNumberEditBox = 6,
    /// CreateEditBox: UIEditBox multi-line (db CE Campaign Descrip. Edit Box) (provisional name)
    kDbControlMultiLineEditBox = 7,
    /// CreateEditBox: UIEditBox single line (db Chat Text Edit Box) (provisional name)
    kDbControlEditBox = 8,
    /// CreateListBox: UILBString (provisional name)
    kDbControlListBox = 9,
    /// CreateListBox: UILBString with inAutoSelect (select on mouse over) (provisional name)
    kDbControlAutoSelectListBox = 10,
    /// CreateRadioButton (provisional name)
    kDbControlRadioButton = 11,
    /// CreateRadioButton with inIsPushButton (db: GS Video Tab, MPS Join By IP Tab) (provisional name)
    kDbControlRadioTab = 12,
    /// CreateStatusBar (provisional name)
    kDbControlStatusBar = 13,
    /// CreateScrollBar style 0 (provisional name)
    kDbControlHorizontalScrollBar = 14,
    /// CreateTextLabel: label type 0 (single line) (provisional name)
    kDbControlTextLabel = 15,
    /// CreateTextLabel: label type 1 (multi-line with scroll bar) (provisional name)
    kDbControlMultiLineTextLabel = 16,
    /// CreateSlider style 0 (provisional name)
    kDbControlHorizontalSlider = 0x11,
    /// CreateSlider style 1 (provisional name)
    kDbControlVerticalSlider = 0x12,
    /// CreateScrollBar style 1 (provisional name)
    kDbControlVerticalScrollBar = 0x13,
    /// CreateTextLabel single line, text = the number in the string-table field (db CB Civ Pts Left (#)) (provisional name)
    kDbControlNumberTextLabel = 0x14,
    /// CreateTextLabel multi-line, number text (provisional name)
    kDbControlNumberMultiLineTextLabel = 0x15,
    /// CreateFileListBox (provisional name)
    kDbControlFileListBox = 0x16,
    /// UIForm::Open takes it as mHelpTextLabel; single line (db Main Screen Tool Tips) (provisional name)
    kDbControlHelpTextLabel = 0x18,
    /// UIForm::Open help text label, multi-line (db GS Help Roll Over Text Display) (provisional name)
    kDbControlMultiLineHelpTextLabel = 0x19,
    /// CreateScrollingTextLabel: UITLScrolling (provisional name)
    kDbControlScrollingTextLabel = 0x1a,
    /// CreateButton: new UIBlinkingButton (provisional name)
    kDbControlBlinkingButton = 0x1b,
    /// CreateButton: new UIScalableButton (provisional name)
    kDbControlScalableButton = 0x1c,
    /// no control (the original's CreateMovie: UIMovie; movies left the game) (provisional name)
    kDbControlMovie = 0x1d,
    /// CreateGraph: new UILineGraph (provisional name)
    kDbControlLineGraph = 0x1e,
    /// CreateButton: new UIDualButton (provisional name)
    kDbControlDualButton = 0x1f,
    /// CreateFileComboBox: UIFileComboBox (provisional name)
    kDbControlFileComboBox = 0x20,
    /// CreateUserDefinedControl: new EEUIFMGStatusButton (RTTI)
    kDbControlStatusButton = 0x3e8,
    /// CreateUserDefinedControl: new EEUIMiniMap (RTTI)
    kDbControlMiniMap = 0x3e9,
    /// CreateUserDefinedControl: new EEUIFMGErrorLog (RTTI)
    kDbControlErrorLog = 0x3eb,
    /// CreateUserDefinedControl: new EEUIFMGAttribute (RTTI)
    kDbControlAttribute = 0x3ed,
    /// CreateUserDefinedControl: new EEUIFMGStatusBar (RTTI)
    kDbControlMainGameStatusBar = 0x3ee,
    /// CreateUserDefinedControl: new EEUICSelectUnits (RTTI)
    kDbControlSelectUnits = 0x3ef,
    /// CreateUserDefinedControl: new EEUICCommandUnits (RTTI)
    kDbControlCommandUnits = 0x3f0,
    /// CreateUserDefinedControl: new EEUICPortrait (RTTI)
    kDbControlPortrait = 0x3f1,
    /// CreateUserDefinedControl: new EEUICChatLine (RTTI)
    kDbControlChatLine = 0x3f2,
    /// CreateUserDefinedControl: new EECBUIListBox (RTTI)
    kDbControlCBListBox = 0x3f3,
    /// CreateUserDefinedControl: new EEUICWonNetStatBox (RTTI)
    kDbControlWonNetStatBox = 0x3f5,
    /// CreateUserDefinedControl: new EEUICUnitIconBar (RTTI)
    kDbControlUnitIconBar = 0x3f6,
    /// CreateUserDefinedControl: new EEUIBSavedGroup (RTTI)
    kDbControlSavedGroup = 0x3f7,
    /// CreateUserDefinedControl: new EEUIPGSLineGraph (RTTI)
    kDbControlPGSLineGraph = 0x3f8,
    /// CreateUserDefinedControl: new EEUICPing (RTTI)
    kDbControlPing = 0x3f9,
    /// CreateUserDefinedControl: new EEUICWonNetRoomsBox (RTTI)
    kDbControlWonNetRoomsBox = 0x3fa,
    /// CreateUserDefinedControl: new EEUICWonNetGamesBox (RTTI)
    kDbControlWonNetGamesBox = 0x3fb,
}

/// UIForm::SetFormEvent selector (UIEventData::mEventType, db form-event type)
/// (the C++ `enum UIFormEventType`, UIForm.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UIFormEventType {
    /// UIForm::SetFormEvent case 0 calls SetFormOpenEvent (exported)
    kUIEventFormOpen = 0,
    /// UIForm::SetFormEvent case 1 calls SetFormCloseEvent (exported)
    kUIEventFormClose = 1,
    /// UIForm::SetFormEvent case 2 calls SetMouseLeftButtonPressedEvent (exported)
    kUIEventMouseLeftButtonPressed = 2,
    /// UIForm::SetFormEvent case 3 calls SetMouseLeftButtonUnpressedEvent (exported)
    kUIEventMouseLeftButtonUnpressed = 3,
    /// UIForm::SetFormEvent case 4 calls SetMouseLeftButtonDoublePressedEvent (exported)
    kUIEventMouseLeftButtonDoublePressed = 4,
    /// UIForm::SetFormEvent case 5 calls SetMouseMiddleButtonPressedEvent (exported)
    kUIEventMouseMiddleButtonPressed = 5,
    /// UIForm::SetFormEvent case 6 calls SetMouseMiddleButtonUnpressedEvent (exported)
    kUIEventMouseMiddleButtonUnpressed = 6,
    /// UIForm::SetFormEvent case 7 calls SetMouseMiddleButtonDoublePressedEvent (exported)
    kUIEventMouseMiddleButtonDoublePressed = 7,
    /// UIForm::SetFormEvent case 8 calls SetMouseRightButtonPressedEvent (exported)
    kUIEventMouseRightButtonPressed = 8,
    /// UIForm::SetFormEvent case 9 calls SetMouseRightButtonUnpressedEvent (exported)
    kUIEventMouseRightButtonUnpressed = 9,
    /// UIForm::SetFormEvent case 10 calls SetMouseRightButtonDoublePressedEvent (exported)
    kUIEventMouseRightButtonDoublePressed = 10,
    /// UIForm::SetFormEvent case 11 calls SetMouseMoveEvent (exported)
    kUIEventMouseMove = 11,
    /// UIForm::SetFormEvent case 12 calls SetMouseWheelMoveEvent (exported)
    kUIEventMouseWheelMove = 12,
}

/// UILayoutUtility edge offset types (GetHorizontalPoint/GetVerticalPoint switch); the database layouts store them
/// (the C++ `enum UILayoutType`, UILayoutUtility.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UILayoutType {
    /// UILayoutUtility::GetHorizontalPoint: left/top + offset (provisional name)
    kLayoutFromStart = 0,
    /// UILayoutUtility::GetHorizontalPoint: centre + offset (provisional name)
    kLayoutAfterCenter = 1,
    /// UILayoutUtility::GetHorizontalPoint: centre - offset (provisional name)
    kLayoutBeforeCenter = 2,
    /// UILayoutUtility::GetHorizontalPoint: right/bottom - offset (provisional name)
    kLayoutFromEnd = 3,
    /// UILayoutUtility::GetHorizontalPoint: left/top + size x offset (fraction) (provisional name)
    kLayoutProportional = 4,
    /// UILayoutUtility::GetHorizontalPoint: the left/top edge point + offset (a width/height) (provisional name)
    kLayoutFromStartEdge = 5,
}

/// UITiledBackground's tiling (bgobj.rs mTiling)
/// (the C++ `enum UITiling`, UITiledBackground.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UITiling {
    /// UITiledBackground
    kTilingBoth = 0,
    /// was UILRTiledBackground
    kTilingLeftRight = 1,
    /// was UITBTiledBackground
    kTilingTopBottom = 2,
    /// was UIStretchedTiledBackground (inAcross x inDown tiles)
    kTilingStretched = 3,
}

/// UITriStateCheckBox::SetCheckState: the UIManager check bitmaps each state shows
/// (the C++ `enum UICheckState`, UITriStateCheckBox.h)
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UICheckState {
    /// SetCheckState case -> UIManager::GetCheckOff/GetCheckDownOff
    kCheckStateOff = 0,
    /// SetCheckState case -> UIManager::GetCheckOn/GetCheckDownOn
    kCheckStateOn = 1,
    /// SetCheckState case -> UIManager::GetCheckNot/GetCheckDownNot
    kCheckStateNot = 2,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_values_are_the_databases_and_the_originals() {
        assert_eq!(UIBackgroundType::kBackgroundGeneric as u32, 101);
        assert_eq!(UIButtonState::kButtonStatePressed as u32, 2);
        assert_eq!(UIControlTriggerID::kControlTriggerRightButtonUp as u32, 0x3f6);
        assert_eq!(UIControlType::kControlTypeGraph as u32, 13);
        assert_eq!(UIDisplayMode::kDisplayModeShown as u32, 1);
        assert_eq!(UIDbControlType::kDbControlFileListBox as u32, 22);
        assert_eq!(UIFormEventType::kUIEventMouseWheelMove as u32, 12);
        assert_eq!(UITiling::kTilingStretched as u32, 3);
        assert_eq!(UICheckState::kCheckStateNot as u32, 2);
        // layout.rs' exported constants are the enum's values
        use crate::layout as l;
        assert_eq!(UILayoutType::kLayoutFromStart as core::ffi::c_ulong, l::FROM_START);
        assert_eq!(UILayoutType::kLayoutAfterCenter as core::ffi::c_ulong, l::AFTER_CENTER);
        assert_eq!(UILayoutType::kLayoutBeforeCenter as core::ffi::c_ulong, l::BEFORE_CENTER);
        assert_eq!(UILayoutType::kLayoutFromEnd as core::ffi::c_ulong, l::FROM_END);
        assert_eq!(UILayoutType::kLayoutProportional as core::ffi::c_ulong, l::PROPORTIONAL);
        assert_eq!(UILayoutType::kLayoutFromStartEdge as core::ffi::c_ulong, l::FROM_START_EDGE);
    }
}
