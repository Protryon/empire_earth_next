//! WONGUI's named integer constants (the WON SDK's LobbyTypes.h, EventTypes.h, GUITypes.h, ColorScheme.h, Cursor.h,
//! Font.h, ListBox.h, Scrollbar.h, SelectionColor.h and Browser.h enums), as plain (unscoped) C enums for
//! ee/wonapi/WONLobbyGUI.h: it names each type in namespace WONAPI with a typedef; the enumerators keep their C++
//! names (R6 WON GUI round 34).
#![allow(non_camel_case_types, clippy::enum_variant_names, clippy::upper_case_acronyms)]

/// SDK LobbyTypes: the lobby's game type (wire values 0..3)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiLobbyGameType {
    LobbyGameType_None = 0,
    LobbyGameType_Lan = 1,
    LobbyGameType_Internet = 2,
    LobbyGameType_DirectConnect = 3,
}

/// SDK EventTypes.h ControlIdType: the library's control ids from 100000, the game's from 200000 (the registrar
/// passes 100001 and 100002)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiControlIdType {
    ControlId_LibMin = 100000,
    ControlId_Ok = 100001,
    ControlId_Cancel = 100002,
    ControlId_TreeToggleOpen = 100003,
    ControlId_SplitterMin = 100004,
    /// ControlId_SplitterMin + 100
    ControlId_SplitterMax = 100104,
    ControlId_UserMin = 200000,
    ControlId_UserMin2 = 300000,
    ControlId_UserMin3 = 400000,
    ControlId_UserMin4 = 500000,
    ControlId_UserMin5 = 600000,
}

/// an arrow's direction (MSGraphicsUtil, the scroll buttons)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiDirection {
    Direction_Left = 0,
    Direction_Right = 1,
    Direction_Up = 2,
    Direction_Down = 3,
}

/// horizontal alignment
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiHorzAlign {
    HorzAlign_Left = 0,
    HorzAlign_Center = 1,
    HorzAlign_Right = 2,
}

/// vertical alignment
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiVertAlign {
    VertAlign_Top = 0,
    VertAlign_Center = 1,
    VertAlign_Bottom = 2,
}

/// a popup's flags (bits; the last four are the SDK's combinations)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiPopupFlags {
    PopupFlag_EndOnLeftMouseDown = 0x0001,
    PopupFlag_EndOnMiddleMouseDown = 0x0002,
    PopupFlag_EndOnRightMouseDown = 0x0004,
    PopupFlag_EndOnMouseMove = 0x0008,
    PopupFlag_EndOnKeyDown = 0x0010,
    PopupFlag_EndOnEscape = 0x0020,
    PopupFlag_EndOnFocusChange = 0x0040,
    PopupFlag_WantInput = 0x0080,
    PopupFlag_IsModal = 0x0100,
    PopupFlag_AdjustPosition = 0x0200,
    PopupFlag_EatMouseDownOnEnd = 0x0800,
    PopupFlag_Additional = 0x1000,
    PopupFlag_FireEventOnEnd = 0x2000,
    PopupFlag_DontWantMouse = 0x4000,
    PopupFlag_InOwnWindow = 0x10000,
    PopupFlag_NotInOwnWindow = 0x20000,
    /// the three mouse-down bits
    PopupFlag_EndOnMouseDown = 0x0007,
    /// the mouse-down, mouse-move and key-down bits
    PopupFlag_EndOnInput = 0x001f,
    PopupFlag_StandardPopup = 0x23e7,
    PopupFlag_StandardTip = 0x6247,
}

/// Cursor's standard cursors (the table GetStandardCursor reads)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiStandardCursor {
    StandardCursor_Arrow = 0,
    StandardCursor_Hand = 1,
    StandardCursor_IBeam = 2,
    StandardCursor_EastWest = 3,
    StandardCursor_NorthSouth = 4,
    StandardCursor_NorthSouthEastWest = 5,
    StandardCursor_Max = 6,
}

/// ColorScheme's 17 colours (0x326ce0 reads 17 DWORDs; ColorScheme::GetColorRef is 0x80000000 + the index)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiStandardColor {
    StandardColor_3DDarkShadow = 0,
    StandardColor_3DFace = 1,
    StandardColor_3DHilight = 2,
    StandardColor_3DShadow = 3,
    StandardColor_Scrollbar = 4,
    StandardColor_ButtonText = 5,
    StandardColor_GrayText = 6,
    StandardColor_Hilight = 7,
    StandardColor_HilightText = 8,
    StandardColor_ToolTipBack = 9,
    StandardColor_ToolTipText = 10,
    StandardColor_MenuBack = 11,
    StandardColor_MenuText = 12,
    StandardColor_Back = 13,
    StandardColor_Text = 14,
    StandardColor_Link = 15,
    StandardColor_LinkDown = 16,
    StandardColor_Max = 17,
}

/// a mouse button
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiMouseButton {
    MouseButton_Left = 0,
    MouseButton_Middle = 1,
    MouseButton_Right = 2,
}

/// SelectionColor's foreground mode
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiForegroundSelectionMode {
    ForegroundSelectionMode_UseOriginalColor = 0,
    ForegroundSelectionMode_InvertOriginalColor = 1,
    ForegroundSelectionMode_UseForegroundColor = 2,
}

/// when a scroller shows a scroll bar
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiScrollbarCondition {
    ScrollbarCondition_Disable = 0,
    ScrollbarCondition_Enable = 1,
    ScrollbarCondition_Conditional = 2,
}

/// when a scroll button shows
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiScrollButtonCondition {
    ScrollButtonCondition_Disable = 0,
    ScrollButtonCondition_Enable = 1,
    ScrollButtonCondition_Conditional = 2,
    ScrollButtonCondition_PartialConditional = 3,
}

/// a font's style bits
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiFontStyles {
    FontStyle_Plain = 0,
    FontStyle_Bold = 1,
    FontStyle_Italic = 2,
    FontStyle_Underline = 4,
}

/// a list's sort order
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiListSortType {
    ListSortType_Ascending = 0,
    ListSortType_Descending = 1,
    ListSortType_None = 2,
}

/// the font size a layout gives the page view (kept, not used)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiHTMLFontSize {
    HTMLFontSize_Smallest = 0,
    HTMLFontSize_Small = 1,
    HTMLFontSize_Medium = 2,
    HTMLFontSize_Large = 3,
    HTMLFontSize_Largest = 4,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn values_are_the_sdks() {
        assert_eq!(
            EeWonGuiControlIdType::ControlId_SplitterMax as i32,
            EeWonGuiControlIdType::ControlId_SplitterMin as i32 + 100
        );
        assert_eq!(EeWonGuiStandardColor::StandardColor_Max as i32, 17);
        let mouse_down = EeWonGuiPopupFlags::PopupFlag_EndOnLeftMouseDown as i32
            | EeWonGuiPopupFlags::PopupFlag_EndOnMiddleMouseDown as i32
            | EeWonGuiPopupFlags::PopupFlag_EndOnRightMouseDown as i32;
        assert_eq!(EeWonGuiPopupFlags::PopupFlag_EndOnMouseDown as i32, mouse_down);
        let input = mouse_down
            | EeWonGuiPopupFlags::PopupFlag_EndOnMouseMove as i32
            | EeWonGuiPopupFlags::PopupFlag_EndOnKeyDown as i32;
        assert_eq!(EeWonGuiPopupFlags::PopupFlag_EndOnInput as i32, input);
        assert_eq!(EeWonGuiFontStyles::FontStyle_Underline as i32, 4);
    }
}
