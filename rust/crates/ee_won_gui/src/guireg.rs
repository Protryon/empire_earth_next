//! The GUI component types' registration (R6 WON round 33): `WONGUIConfig_Init`'s table, the names with their
//! config and component factories in the original's order, then the Ok and Cancel control ids. The factories are
//! C++ functions (templates over the C++ classes): the C++ host answers each entry's pair (WONGUIConfig.cpp).

use crate::compcfg::{add_allocator, register_control_id};

/// One registration of `WONGUIConfig_Init`, in its order (the base components, the Windows skin's, the image and
/// font configs, the WON skin's); the C++ host gives its config and component factories.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[allow(clippy::upper_case_acronyms)]
pub enum EeWonGuiReg {
    /// "Component"
    Component = 0,
    /// "LayoutPoint"
    LayoutPoint,
    /// "Container"
    Container,
    /// "Dialog"
    Dialog,
    /// "ImageButton"
    ImageButton,
    /// "Label"
    Label,
    /// "ListArea"
    ListArea,
    /// "MultiListArea"
    MultiListArea,
    /// "MultiListHeader"
    MultiListHeader,
    /// "InputBox"
    InputBox,
    /// "Splitter"
    Splitter,
    /// "TextArea"
    TextArea,
    /// "TabCtrl"
    TabCtrl,
    /// "TreeArea"
    TreeArea,
    /// "RectangleComponent"
    RectangleComponent,
    /// "RectangleComponent3D"
    RectangleComponent3D,
    /// "ImageComponent"
    ImageComponent,
    /// "ScreenContainer"
    ScreenContainer,
    /// "MSButton"
    MSButton,
    /// "MSCheckbox"
    MSCheckbox,
    /// "MSRadioButton"
    MSRadioButton,
    /// "MSLabel"
    MSLabel,
    /// "MSInputBox"
    MSInputBox,
    /// "MSComboBox"
    MSComboBox,
    /// "MSArrowButton"
    MSArrowButton,
    /// "MSCloseButton"
    MSCloseButton,
    /// "MSTabButton"
    MSTabButton,
    /// "MSHeaderButton"
    MSHeaderButton,
    /// "MSScrollbar"
    MSScrollbar,
    /// "MSScrollContainer"
    MSScrollContainer,
    /// "MSSeperator"
    MSSeperator,
    /// "MS3DFrame"
    MS3DFrame,
    /// "MSEtchedFrame"
    MSEtchedFrame,
    /// "MSEtchedFrameGroup"
    MSEtchedFrameGroup,
    /// "MSDlgBack"
    MSDlgBack,
    /// "MSTabCtrl"
    MSTabCtrl,
    /// "MSListCtrl"
    MSListCtrl,
    /// "MSHeaderControl"
    MSHeaderControl,
    /// "Image"
    Image,
    /// "StretchImage"
    StretchImage,
    /// "StretchCenterImage"
    StretchCenterImage,
    /// "ImageFilter"
    ImageFilter,
    /// "Font"
    Font,
    /// "WONSkin"
    WONSkin,
    /// "WONButtonSkin"
    WONButtonSkin,
    /// "WONScrollbarSkin"
    WONScrollbarSkin,
    /// "WONHeaderButtonSkin"
    WONHeaderButtonSkin,
    /// "WONSimpleButtonSkin"
    WONSimpleButtonSkin,
    /// "WONScrollContainerSkin"
    WONScrollContainerSkin,
    /// "WONListCtrlSkin"
    WONListCtrlSkin,
    /// "WONTabCtrlSkin"
    WONTabCtrlSkin,
    /// "WONComboBoxSkin"
    WONComboBoxSkin,
    /// "WONButton"
    WONButton,
    /// "WONCheckbox"
    WONCheckbox,
    /// "WONRadioButton"
    WONRadioButton,
    /// "WONTabButton"
    WONTabButton,
    /// "WONHeaderButton"
    WONHeaderButton,
    /// "WONCloseButton"
    WONCloseButton,
    /// "WONPopupBack"
    WONPopupBack,
    /// "WONDialogBack"
    WONDialogBack,
    /// "WONInputBox"
    WONInputBox,
    /// "WONComboBox"
    WONComboBox,
    /// "WONTabCtrl"
    WONTabCtrl,
    /// "WONScrollContainer"
    WONScrollContainer,
    /// "WONListCtrl"
    WONListCtrl,
    /// "WONFrameComponent"
    WONFrameComponent,
}

/// The names, in `EeWonGuiReg`'s order.
const NAMES: [&str; 66] = [
    "Component",
    "LayoutPoint",
    "Container",
    "Dialog",
    "ImageButton",
    "Label",
    "ListArea",
    "MultiListArea",
    "MultiListHeader",
    "InputBox",
    "Splitter",
    "TextArea",
    "TabCtrl",
    "TreeArea",
    "RectangleComponent",
    "RectangleComponent3D",
    "ImageComponent",
    "ScreenContainer",
    "MSButton",
    "MSCheckbox",
    "MSRadioButton",
    "MSLabel",
    "MSInputBox",
    "MSComboBox",
    "MSArrowButton",
    "MSCloseButton",
    "MSTabButton",
    "MSHeaderButton",
    "MSScrollbar",
    "MSScrollContainer",
    "MSSeperator",
    "MS3DFrame",
    "MSEtchedFrame",
    "MSEtchedFrameGroup",
    "MSDlgBack",
    "MSTabCtrl",
    "MSListCtrl",
    "MSHeaderControl",
    "Image",
    "StretchImage",
    "StretchCenterImage",
    "ImageFilter",
    "Font",
    "WONSkin",
    "WONButtonSkin",
    "WONScrollbarSkin",
    "WONHeaderButtonSkin",
    "WONSimpleButtonSkin",
    "WONScrollContainerSkin",
    "WONListCtrlSkin",
    "WONTabCtrlSkin",
    "WONComboBoxSkin",
    "WONButton",
    "WONCheckbox",
    "WONRadioButton",
    "WONTabButton",
    "WONHeaderButton",
    "WONCloseButton",
    "WONPopupBack",
    "WONDialogBack",
    "WONInputBox",
    "WONComboBox",
    "WONTabCtrl",
    "WONScrollContainer",
    "WONListCtrl",
    "WONFrameComponent",
];

/// The host: the entry's config factory (`config` true) or component factory, as a function pointer.
pub type EeWonGuiRegHostFn = Option<unsafe extern "C" fn(entry: EeWonGuiReg, config: bool) -> isize>;

/// The control ids `WONGUIConfig_Init` registers last (WONLobbyGUI.h `ControlId_Ok`, `ControlId_Cancel`).
const CONTROL_IDS: [(&str, i32); 2] = [("Ok", 100001), ("Cancel", 100002)];

/// `WONGUIConfig_Init`: every entry's allocator added (the first of a name stays), then the control ids.
///
/// # Safety
/// `host` answers every `EeWonGuiReg` with function pointers the configs' allocation calls.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_config_init(host: EeWonGuiRegHostFn) {
    let Some(host) = host else { return };
    for (i, name) in NAMES.iter().enumerate() {
        // SAFETY: a value of the enum (0.. in order)
        let e = unsafe { std::mem::transmute::<u32, EeWonGuiReg>(i as u32) };
        // SAFETY: the caller's host
        let (config, comp) = unsafe { (host(e, true), host(e, false)) };
        add_allocator(name.as_bytes(), config as usize, comp as usize);
    }
    for (name, id) in CONTROL_IDS {
        register_control_id(name.as_bytes(), id);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_names_follow_the_entries() {
        assert_eq!(NAMES.len(), EeWonGuiReg::WONFrameComponent as usize + 1);
        assert_eq!(format!("{:?}", EeWonGuiReg::MS3DFrame), NAMES[EeWonGuiReg::MS3DFrame as usize]);
        for (i, n) in NAMES.iter().enumerate() {
            // SAFETY: in range (above)
            let e = unsafe { std::mem::transmute::<u32, EeWonGuiReg>(i as u32) };
            assert_eq!(&format!("{e:?}"), n);
        }
    }
}
