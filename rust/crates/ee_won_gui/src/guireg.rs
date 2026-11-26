//! The GUI component types' registration (R6 WON round 33): `WONGUIConfig_Init`'s table, the names with their
//! config and component factories in the original's order, then the Ok and Cancel control ids. Each config is of
//! its entry's kind, made by compcfg.rs over `CfgNewOfKind`; a shell, a skin template's copy and the WON tab control
//! are made there too (R6 WON lobby and network round 39); a C++ class's component is the host's factory
//! (ComponentConfigHost.cpp, WONGUIConfig.cpp until that round).

use crate::cfgkind::EeWonCfgKind;
use crate::compcfg::{add_allocator, register_control_id, Make};
use crate::shell::EeWonShellKind;
use crate::skinobj::EeWonSkinGet;

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

/// Each entry's config kind and component (C++ classes: the host's factory; the shells, skin copies and the WON tab
/// control on the game's skin made here, R6 WON lobby and network round 39), in the entries' order.
const ENTRIES: [(EeWonGuiReg, EeWonCfgKind, Make); 66] = [
    (EeWonGuiReg::Component, EeWonCfgKind::Component, CPP),
    (EeWonGuiReg::LayoutPoint, EeWonCfgKind::Component, CPP),
    (EeWonGuiReg::Container, EeWonCfgKind::Container, CPP),
    (EeWonGuiReg::Dialog, EeWonCfgKind::Dialog, CPP),
    (EeWonGuiReg::ImageButton, EeWonCfgKind::ImageButton, CPP),
    (EeWonGuiReg::Label, EeWonCfgKind::Label, CPP),
    (EeWonGuiReg::ListArea, EeWonCfgKind::ListArea, CPP),
    (EeWonGuiReg::MultiListArea, EeWonCfgKind::MultiListArea, CPP),
    (EeWonGuiReg::MultiListHeader, EeWonCfgKind::MultiListHeader, CPP),
    (EeWonGuiReg::InputBox, EeWonCfgKind::InputBox, CPP),
    (EeWonGuiReg::Splitter, EeWonCfgKind::Splitter, CPP),
    (EeWonGuiReg::TextArea, EeWonCfgKind::TextArea, CPP),
    (EeWonGuiReg::TabCtrl, EeWonCfgKind::TabCtrl, CPP),
    (EeWonGuiReg::TreeArea, EeWonCfgKind::TreeArea, CPP),
    (EeWonGuiReg::RectangleComponent, EeWonCfgKind::Rectangle, CPP),
    (EeWonGuiReg::RectangleComponent3D, EeWonCfgKind::Rectangle3D, CPP),
    (EeWonGuiReg::ImageComponent, EeWonCfgKind::ImageComponent, CPP),
    (EeWonGuiReg::ScreenContainer, EeWonCfgKind::ScreenContainer, CPP),
    (EeWonGuiReg::MSButton, EeWonCfgKind::MsButton, Make::Shell(EeWonShellKind::MsButton, [0, 0, 0])),
    (EeWonGuiReg::MSCheckbox, EeWonCfgKind::MsButton, Make::Shell(EeWonShellKind::MsCheckbox, [0, 0, 0])),
    (EeWonGuiReg::MSRadioButton, EeWonCfgKind::MsButton, Make::Shell(EeWonShellKind::MsRadioButton, [0, 0, 0])),
    (EeWonGuiReg::MSLabel, EeWonCfgKind::MsButton, Make::Shell(EeWonShellKind::MsLabel, [0, 0, 0])),
    (EeWonGuiReg::MSInputBox, EeWonCfgKind::InputBox, Make::Shell(EeWonShellKind::MsInputBox, [0, 0, 0])),
    (EeWonGuiReg::MSComboBox, EeWonCfgKind::MsComboBox, Make::Shell(EeWonShellKind::MsComboBox, [0, 0, 0])),
    (EeWonGuiReg::MSArrowButton, EeWonCfgKind::MsArrowButton, Make::Shell(EeWonShellKind::MsArrowButton, [2, 0, 0])),
    (EeWonGuiReg::MSCloseButton, EeWonCfgKind::Button, Make::Shell(EeWonShellKind::MsCloseButton, [0, 0, 0])),
    (EeWonGuiReg::MSTabButton, EeWonCfgKind::MsTabButton, Make::Shell(EeWonShellKind::MsTabButton, [0, 1, 3])),
    (EeWonGuiReg::MSHeaderButton, EeWonCfgKind::MsButton, Make::Shell(EeWonShellKind::MsHeaderButton, [0, 0, 0])),
    (EeWonGuiReg::MSScrollbar, EeWonCfgKind::MsScrollbar, Make::Shell(EeWonShellKind::MsScrollbar, [1, 0, 0])),
    (EeWonGuiReg::MSScrollContainer, EeWonCfgKind::MsScrollContainer, Make::Shell(EeWonShellKind::MsScrollContainer, [0, 0, 0])),
    (EeWonGuiReg::MSSeperator, EeWonCfgKind::Component, Make::Shell(EeWonShellKind::MsSeperator, [6, 0, 0])),
    (EeWonGuiReg::MS3DFrame, EeWonCfgKind::Ms3DFrame, Make::Shell(EeWonShellKind::Ms3DFrame, [3, 0, 0])),
    (EeWonGuiReg::MSEtchedFrame, EeWonCfgKind::Component, Make::Shell(EeWonShellKind::MsEtchedFrame, [0, 0, 0])),
    (EeWonGuiReg::MSEtchedFrameGroup, EeWonCfgKind::MsEtchedFrameGroup, Make::Shell(EeWonShellKind::MsEtchedFrameGroup, [0, 0, 0])),
    (EeWonGuiReg::MSDlgBack, EeWonCfgKind::Component, Make::Shell(EeWonShellKind::MsDlgBack, [0, 0, 0])),
    (EeWonGuiReg::MSTabCtrl, EeWonCfgKind::MsTabCtrl, Make::Shell(EeWonShellKind::MsTabCtrl, [0, 0, 0])),
    (EeWonGuiReg::MSListCtrl, EeWonCfgKind::MsListCtrl, Make::Shell(EeWonShellKind::MsListCtrl, [0, 0, 0])),
    (EeWonGuiReg::MSHeaderControl, EeWonCfgKind::MsHeaderControl, Make::Shell(EeWonShellKind::MsHeaderControl, [0, 0, 0])),
    (EeWonGuiReg::Image, EeWonCfgKind::Image, Make::Fn(0)),
    (EeWonGuiReg::StretchImage, EeWonCfgKind::StretchImage, Make::Fn(0)),
    (EeWonGuiReg::StretchCenterImage, EeWonCfgKind::StretchCenterImage, Make::Fn(0)),
    (EeWonGuiReg::ImageFilter, EeWonCfgKind::ImageFilter, Make::Fn(0)),
    (EeWonGuiReg::Font, EeWonCfgKind::Font, Make::Fn(0)),
    (EeWonGuiReg::WONSkin, EeWonCfgKind::WonSkin, CPP),
    (EeWonGuiReg::WONButtonSkin, EeWonCfgKind::WonButton, CPP),
    (EeWonGuiReg::WONScrollbarSkin, EeWonCfgKind::WonScrollbar, Make::Shell(EeWonShellKind::WonScrollbar, [1, 0, 0])),
    (EeWonGuiReg::WONHeaderButtonSkin, EeWonCfgKind::WonButton, Make::Shell(EeWonShellKind::WonHeaderButton, [0, 0, 0])),
    (EeWonGuiReg::WONSimpleButtonSkin, EeWonCfgKind::WonSimpleButton, Make::Shell(EeWonShellKind::WonSimpleButton, [0, 0, 0])),
    (EeWonGuiReg::WONScrollContainerSkin, EeWonCfgKind::WonScrollContainer, Make::Shell(EeWonShellKind::WonScrollContainer, [0, 0, 0])),
    (EeWonGuiReg::WONListCtrlSkin, EeWonCfgKind::WonListCtrl, Make::Shell(EeWonShellKind::WonListCtrl, [0, 0, 0])),
    (EeWonGuiReg::WONTabCtrlSkin, EeWonCfgKind::WonTabCtrl, Make::SkinTab),
    (EeWonGuiReg::WONComboBoxSkin, EeWonCfgKind::WonComboBox, Make::Shell(EeWonShellKind::WonComboBox, [0, 0, 0])),
    (EeWonGuiReg::WONButton, EeWonCfgKind::WonButton, Make::Skin(EeWonSkinGet::PushButton)),
    (EeWonGuiReg::WONCheckbox, EeWonCfgKind::WonButton, Make::Skin(EeWonSkinGet::Checkbox)),
    (EeWonGuiReg::WONRadioButton, EeWonCfgKind::WonButton, Make::Skin(EeWonSkinGet::RadioButton)),
    (EeWonGuiReg::WONTabButton, EeWonCfgKind::WonButton, Make::Skin(EeWonSkinGet::TabButton)),
    (EeWonGuiReg::WONHeaderButton, EeWonCfgKind::WonButton, Make::Skin(EeWonSkinGet::HeaderButton)),
    (EeWonGuiReg::WONCloseButton, EeWonCfgKind::WonButton, Make::Skin(EeWonSkinGet::CloseButton)),
    (EeWonGuiReg::WONPopupBack, EeWonCfgKind::WonFrameComponent, Make::Skin(EeWonSkinGet::PopupBack)),
    (EeWonGuiReg::WONDialogBack, EeWonCfgKind::WonFrameComponent, Make::Skin(EeWonSkinGet::DialogBack)),
    (EeWonGuiReg::WONInputBox, EeWonCfgKind::InputBox, Make::Skin(EeWonSkinGet::InputBox)),
    (EeWonGuiReg::WONComboBox, EeWonCfgKind::WonComboBox, Make::Skin(EeWonSkinGet::ComboBox)),
    (EeWonGuiReg::WONTabCtrl, EeWonCfgKind::WonTabCtrl, Make::Skin(EeWonSkinGet::TabCtrl)),
    (EeWonGuiReg::WONScrollContainer, EeWonCfgKind::WonScrollContainer, Make::Skin(EeWonSkinGet::ScrollContainer)),
    (EeWonGuiReg::WONListCtrl, EeWonCfgKind::WonListCtrl, Make::Skin(EeWonSkinGet::ListCtrl)),
    (EeWonGuiReg::WONFrameComponent, EeWonCfgKind::WonFrameComponent, Make::Shell(EeWonShellKind::WonFrameComponent, [0, 0, 0])),
];
/// An entry whose component is a C++ class: the host's factory.
const CPP: Make = Make::Fn(usize::MAX);

/// The host: the component factory of an entry whose component is a C++ class, as a function pointer.
pub type EeWonGuiRegHostFn = Option<unsafe extern "C" fn(entry: EeWonGuiReg) -> isize>;

/// The control ids `WONGUIConfig_Init` registers last (WONLobbyGUI.h `ControlId_Ok`, `ControlId_Cancel`).
const CONTROL_IDS: [(&str, i32); 2] = [("Ok", 100001), ("Cancel", 100002)];

/// `WONGUIConfig_Init`: every entry's allocator added (the first of a name stays), then the control ids.
///
/// # Safety
/// `host` answers every `EeWonGuiReg` with function pointers the configs' allocation calls.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_config_init(host: EeWonGuiRegHostFn) {
    let Some(host) = host else { return };
    for (name, (e, kind, comp)) in NAMES.iter().zip(ENTRIES) {
        // SAFETY: the caller's host
        let comp = if comp == CPP { Make::Fn(unsafe { host(e) } as usize) } else { comp };
        add_allocator(name.as_bytes(), Make::Kind(kind), comp);
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
        for (i, (e, _, _)) in ENTRIES.iter().enumerate() {
            assert_eq!(*e as usize, i);
        }
        assert_eq!(format!("{:?}", EeWonGuiReg::MS3DFrame), NAMES[EeWonGuiReg::MS3DFrame as usize]);
        for (i, n) in NAMES.iter().enumerate() {
            // SAFETY: in range (above)
            let e = unsafe { std::mem::transmute::<u32, EeWonGuiReg>(i as u32) };
            assert_eq!(&format!("{e:?}"), n);
        }
    }
}
