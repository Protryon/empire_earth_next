//! WONGUI's component config classes as kinds (R6 WONLobby round 26).
//!
//! The original had one C++ class per configured widget type (ButtonConfig, ContainerConfig, DialogConfig, ..., the
//! Windows skin's MSButtonConfig .. MSListCtrlConfig, the WON skin's WONButtonConfig .. WONSkinConfig: 45 classes).
//! Each held its component a second time under its own type (`SetComponent`), and overrode up to four hooks:
//! `Virtual24` (its instructions: its table of ee_won_gui's cfgtab.rs, then or before its base class's),
//! `GetPredefinedComponent` (the named parts of its component a resource file may configure: a combo box's list and
//! edit box, a scrollbar's thumb and arrows, ...), `GetNamedValue` (the WON frame's paddings) and `PreParse` (the WON
//! skin's default skin). This module is those classes: [`EeWonCfgKind`] names the class, [`class`] its base, its
//! table and how it called its base; the hooks are walks of that hierarchy, so a kind answers as its class's chain of
//! overrides did. The C++ end (include/ee/wonapi/WONCfgTable.inl, WONCompCfg.inl) is one `ComponentConfig` (and one
//! `ContainerConfig`, which keeps the layout state) with a kind; the typed pointers were all the component itself.
//!
//! The classes with members of their own stay C++ classes and have kinds too: FontConfig, ImageConfig and its three
//! image subclasses, PercentLayoutConfig, ColorSchemeConfig.
//!
//! The lobby's control configs (ChatCtrlConfig .. CreditsDialogConfig) are kinds too since R6 WON lobby/net round 40
//! ([`LOBBY_KINDS`]): their instructions are ee_won_lobby's reader (lobbycfgrd.rs), which asks its base kind's walk
//! for the keys it does not hold; `ee_won_cfg_v24` answers 3 for them (the C++ end asks the lobby's reader).

use crate::guiconf::*;

/// A component config's original class.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum EeWonCfgKind {
    Component = 0,
    Container,
    Dialog,
    Button,
    ImageButton,
    Label,
    InputBox,
    ScrollArea,
    ListArea,
    MultiListArea,
    TreeArea,
    TextArea,
    MultiListHeader,
    Scroller,
    ScrollbarScroller,
    Scrollbar,
    TabCtrl,
    ComboBox,
    Splitter,
    Browser,
    Rectangle,
    Rectangle3D,
    ImageComponent,
    ScreenContainer,
    MsButton,
    MsArrowButton,
    MsTabButton,
    Ms3DFrame,
    MsComboBox,
    MsScrollbar,
    MsEtchedFrameGroup,
    MsTabCtrl,
    MsScrollContainer,
    MsHeaderControl,
    MsListCtrl,
    WonSimpleButton,
    WonButton,
    WonScrollbar,
    WonScrollContainer,
    WonHeaderControl,
    WonListCtrl,
    WonComboBox,
    WonTabCtrl,
    WonFrameComponent,
    WonSkin,
    Font,
    Image,
    StretchImage,
    StretchCenterImage,
    ImageFilter,
    PercentLayout,
    ColorScheme,
    /// ChatCtrlConfig (a container's)
    LobbyChat,
    /// RoomLabelCtrlConfig (a container's)
    LobbyRoomLabel,
    /// UserListCtrlConfig (a container's)
    LobbyUserList,
    /// ServerListCtrlConfig (a container's; its header and column count in the config's [`crate::compcfg`] slots)
    LobbyServerList,
    /// GameBrowserCtrlConfig (a container's)
    LobbyGameBrowser,
    /// GameUserListCtrlConfig (a container's)
    LobbyGameUserList,
    /// CrossPromotionButtonConfig (an image button's)
    LobbyCrossPromotion,
    /// ImageTextButtonConfig (an image button's)
    LobbyImageText,
    /// CreditsDialogConfig (a dialog's)
    LobbyCredits,
}

/// The lobby controls' kinds (their reader is ee_won_lobby's lobbycfgrd.rs).
pub const LOBBY_KINDS: [EeWonCfgKind; 9] = {
    use EeWonCfgKind::*;
    [
        LobbyChat,
        LobbyRoomLabel,
        LobbyUserList,
        LobbyServerList,
        LobbyGameBrowser,
        LobbyGameUserList,
        LobbyCrossPromotion,
        LobbyImageText,
        LobbyCredits,
    ]
};

/// Every kind, in order (for the drivers and the tests).
pub const KINDS: [EeWonCfgKind; 52] = {
    use EeWonCfgKind::*;
    [
        Component,
        Container,
        Dialog,
        Button,
        ImageButton,
        Label,
        InputBox,
        ScrollArea,
        ListArea,
        MultiListArea,
        TreeArea,
        TextArea,
        MultiListHeader,
        Scroller,
        ScrollbarScroller,
        Scrollbar,
        TabCtrl,
        ComboBox,
        Splitter,
        Browser,
        Rectangle,
        Rectangle3D,
        ImageComponent,
        ScreenContainer,
        MsButton,
        MsArrowButton,
        MsTabButton,
        Ms3DFrame,
        MsComboBox,
        MsScrollbar,
        MsEtchedFrameGroup,
        MsTabCtrl,
        MsScrollContainer,
        MsHeaderControl,
        MsListCtrl,
        WonSimpleButton,
        WonButton,
        WonScrollbar,
        WonScrollContainer,
        WonHeaderControl,
        WonListCtrl,
        WonComboBox,
        WonTabCtrl,
        WonFrameComponent,
        WonSkin,
        Font,
        Image,
        StretchImage,
        StretchCenterImage,
        ImageFilter,
        PercentLayout,
        ColorScheme,
    ]
};

/// A part of the component that a predefined child names (the C++ host reads it from the component).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgPart {
    /// a combo box's `mListArea`
    ComboList = 0,
    /// a combo box's `mEdit`
    ComboEdit,
    /// a scrollbar's `GetThumb()`
    Thumb,
    /// a scrollbar's `GetUpArrow()`
    UpArrow,
    /// a scrollbar's `GetDownArrow()`
    DownArrow,
    /// a scrollbar's `GetScrollRect()`
    ScrollRect,
    /// a tab control's `mContainerLayout`
    TabLayout,
    /// a page view's `GetHTMLView()`
    HtmlView,
    /// a scrollbar scroller's `GetScrollbar(false)`
    HorzBar,
    /// a scrollbar scroller's `GetScrollbar(true)`
    VertBar,
    /// a list control kind's header (`WONShell::Op(c, GetHeader)`)
    Header,
    /// a scrollbar scroller's `GetScrollArea()`
    ScrollAreaOf,
    /// an etched frame group's label (`WONShell::Op(c, GetLabel)`)
    EtchedLabel,
}

/// How a class's `Virtual24` called its base's.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Order {
    /// ComponentConfig's own: MODIFY, MAPCOMPONENT, then its table
    Own,
    /// its table, then the base's (most of them)
    TableThenBase,
    /// the base's, then its table (WONHeaderControlConfig)
    BaseThenTable,
    /// its table alone (WONSkinConfig, FontConfig, ImageConfig, PercentLayoutConfig, ColorSchemeConfig)
    TableOnly,
    /// the lobby's reader (lobbycfgrd.rs), which asks its base's walk itself
    Lobby,
}

/// One class: its base, its cfgtab table and how its `Virtual24` used them, and its own predefined children (a
/// name, the child config's kind and the component part).
pub struct Class {
    pub base: Option<EeWonCfgKind>,
    pub table: i32,
    pub order: Order,
    pub predefined: &'static [(Pre, EeWonCfgKind, EeWonCfgPart)],
}

/// A predefined child's name: an index of a guiconf key table (the readers that compared through one) or the text
/// (those that compared `name == "..."`); both compare exactly.
#[derive(Clone, Copy, Debug)]
pub enum Pre {
    Key(i32, i32),
    Text(&'static [u8]),
}

/// The class `k` was.
pub fn class(k: EeWonCfgKind) -> Class {
    use EeWonCfgKind as K;
    use EeWonCfgPart as P;
    use Order::*;
    let c = |base: K, table: i32| Class { base: Some(base), table, order: TableThenBase, predefined: &[] };
    let alone = |table: i32| Class { base: Some(K::Component), table, order: TableOnly, predefined: &[] };
    let with = |base: K, table: i32, predefined: &'static [(Pre, K, P)]| Class {
        base: Some(base),
        table,
        order: TableThenBase,
        predefined,
    };
    match k {
        K::Component => {
            Class { base: None, table: EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY, order: Own, predefined: &[] }
        }
        K::Container => c(K::Component, EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION),
        K::Dialog => c(K::Container, EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION),
        K::Button => c(K::Component, EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION),
        K::ImageButton => c(K::Component, EE_WON_CFG_KEYS_IMAGEBUTTONCONFIG_VIRTUAL24_INSTRUCTION),
        K::Label => c(K::Component, EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION),
        K::InputBox => c(K::Component, EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION),
        K::ScrollArea => c(K::Container, EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION),
        K::ListArea => c(K::ScrollArea, EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION),
        K::MultiListArea => c(K::ListArea, EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION),
        K::TreeArea => c(K::ListArea, EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION),
        K::TextArea => c(K::ScrollArea, EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY),
        K::MultiListHeader => c(K::Container, EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION),
        K::Scroller => c(K::Container, EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION),
        K::ScrollbarScroller => c(K::Scroller, EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION),
        K::Scrollbar => with(
            K::Container,
            EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION,
            &[
                (Pre::Key(EE_WON_CFG_KEYS_SCROLLBARCONFIG_GETPREDEFINEDCOMPONENT_NAME, 0), K::Component, P::Thumb),
                (Pre::Key(EE_WON_CFG_KEYS_SCROLLBARCONFIG_GETPREDEFINEDCOMPONENT_NAME, 1), K::Button, P::UpArrow),
                (Pre::Key(EE_WON_CFG_KEYS_SCROLLBARCONFIG_GETPREDEFINEDCOMPONENT_NAME, 2), K::Button, P::DownArrow),
                (Pre::Key(EE_WON_CFG_KEYS_SCROLLBARCONFIG_GETPREDEFINEDCOMPONENT_NAME, 3), K::Button, P::ScrollRect),
            ],
        ),
        K::TabCtrl => with(
            K::Container,
            EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY,
            &[(Pre::Key(EE_WON_CFG_KEYS_TABCTRLCONFIG_GETPREDEFINEDCOMPONENT_NAME, 0), K::Component, P::TabLayout)],
        ),
        K::ComboBox => with(
            K::Container,
            EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY,
            &[
                (Pre::Key(EE_WON_CFG_KEYS_COMBOBOXCONFIG_GETPREDEFINEDCOMPONENT_NAME, 0), K::ListArea, P::ComboList),
                (Pre::Key(EE_WON_CFG_KEYS_COMBOBOXCONFIG_GETPREDEFINEDCOMPONENT_NAME, 1), K::InputBox, P::ComboEdit),
            ],
        ),
        K::Splitter => c(K::Container, EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION),
        K::Browser => with(
            K::Container,
            EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_VIRTUAL24_INSTRUCTION,
            &[(
                Pre::Key(EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_GETPREDEFINEDCOMPONENT_NAME, 0),
                K::ScrollArea,
                P::HtmlView,
            )],
        ),
        K::Rectangle => c(K::Component, EE_WON_CFG_KEYS_RECTANGLECOMPONENTCONFIG_VIRTUAL24_KEY),
        K::Rectangle3D => c(K::Component, EE_WON_CFG_KEYS_RECTANGLECOMPONENT3DCONFIG_VIRTUAL24_KEY),
        K::ImageComponent => c(K::Component, EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY),
        K::ScreenContainer => c(K::Container, EE_WON_CFG_KEYS_SCREENCONTAINERCONFIG_VIRTUAL24_KEY),
        K::MsButton => c(K::Button, EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY),
        K::MsArrowButton => c(K::Button, EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY),
        K::MsTabButton => c(K::MsButton, EE_WON_CFG_KEYS_MSTABBUTTONCONFIG_VIRTUAL24_KEY),
        K::Ms3DFrame => c(K::Component, EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY),
        K::MsComboBox => c(K::ComboBox, EE_WON_CFG_KEYS_MSCOMBOBOXCONFIG_VIRTUAL24_KEY),
        K::MsScrollbar => c(K::Scrollbar, EE_WON_CFG_KEYS_MSSCROLLBARCONFIG_VIRTUAL24_KEY),
        K::MsEtchedFrameGroup => with(
            K::Container,
            EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY,
            &[(Pre::Text(b"LABEL"), K::Label, P::EtchedLabel)],
        ),
        K::MsTabCtrl => c(K::TabCtrl, EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY),
        K::MsScrollContainer => with(
            K::ScrollbarScroller,
            EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY,
            &[
                (Pre::Text(b"HORZSCROLLBAR"), K::MsScrollbar, P::HorzBar),
                (Pre::Text(b"VERTSCROLLBAR"), K::MsScrollbar, P::VertBar),
            ],
        ),
        K::MsHeaderControl => c(K::MultiListHeader, EE_WON_CFG_KEYS_MSHEADERCONTROLCONFIG_VIRTUAL24_KEY),
        K::MsListCtrl => with(
            K::MsScrollContainer,
            EE_WON_CFG_KEYS_MSLISTCTRLCONFIG_VIRTUAL24_KEY,
            &[
                (Pre::Text(b"HEADER"), K::MsHeaderControl, P::Header),
                (Pre::Text(b"MULTILISTAREA"), K::MultiListArea, P::ScrollAreaOf),
            ],
        ),
        K::WonSimpleButton => c(K::Button, EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION),
        K::WonButton => c(K::Button, EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION),
        K::WonScrollbar => c(K::Scrollbar, EE_WON_CFG_KEYS_WONSCROLLBARCONFIG_VIRTUAL24_KEY),
        K::WonScrollContainer => with(
            K::ScrollbarScroller,
            EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY,
            &[
                (Pre::Text(b"HORZSCROLLBAR"), K::WonScrollbar, P::HorzBar),
                (Pre::Text(b"VERTSCROLLBAR"), K::WonScrollbar, P::VertBar),
            ],
        ),
        K::WonHeaderControl => Class {
            base: Some(K::MultiListHeader),
            table: EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY,
            order: BaseThenTable,
            predefined: &[],
        },
        K::WonListCtrl => with(
            K::WonScrollContainer,
            EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY,
            &[
                (Pre::Text(b"HEADER"), K::WonHeaderControl, P::Header),
                (Pre::Text(b"MULTILISTAREA"), K::MultiListArea, P::ScrollAreaOf),
            ],
        ),
        K::WonComboBox => c(K::ComboBox, EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY),
        K::WonTabCtrl => c(K::TabCtrl, EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY),
        K::WonFrameComponent => c(K::Component, EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY),
        K::WonSkin => alone(EE_WON_CFG_KEYS_WONSKINCONFIG_VIRTUAL24_THEINSTRUCTION),
        K::Font => alone(EE_WON_CFG_KEYS_FONTCONFIG_VIRTUAL24_INSTRUCTION),
        K::Image => alone(EE_WON_CFG_KEYS_IMAGECONFIG_VIRTUAL24_ININSTRUCTION),
        K::StretchImage => c(K::Image, EE_WON_CFG_KEYS_STRETCHIMAGECONFIG_VIRTUAL24_INSTRUCTION),
        K::StretchCenterImage => c(K::Image, EE_WON_CFG_KEYS_STRETCHCENTERIMAGECONFIG_VIRTUAL24_INSTRUCTION),
        K::ImageFilter => c(K::Image, EE_WON_CFG_KEYS_IMAGEFILTERCONFIG_VIRTUAL24_ININSTRUCTION),
        K::PercentLayout => alone(EE_WON_CFG_KEYS_PERCENTLAYOUTCONFIG_VIRTUAL24_INSTRUCTION),
        K::ColorScheme => alone(EE_WON_CFG_KEYS_COLORSCHEMECONFIG_VIRTUAL24_INSTRUCTION),
        K::LobbyChat
        | K::LobbyRoomLabel
        | K::LobbyUserList
        | K::LobbyServerList
        | K::LobbyGameBrowser
        | K::LobbyGameUserList => Class { base: Some(K::Container), table: -1, order: Lobby, predefined: &[] },
        K::LobbyCrossPromotion | K::LobbyImageText => {
            Class { base: Some(K::ImageButton), table: -1, order: Lobby, predefined: &[] }
        }
        K::LobbyCredits => Class { base: Some(K::Dialog), table: -1, order: Lobby, predefined: &[] },
    }
}

/// `Own` in a [`v24_steps`] list: ComponentConfig's own MODIFY, MAPCOMPONENT, else its table.
pub const STEP_OWN: i32 = -1;

/// The steps of `k`'s `Virtual24`, in order: a table of cfgtab.rs (run with the key) or [`STEP_OWN`]. The first
/// that handles the key ends the call (the classes' `||`).
pub fn v24_steps(k: EeWonCfgKind) -> Vec<i32> {
    let c = class(k);
    let base = || c.base.map(v24_steps).unwrap_or_default();
    match c.order {
        Order::Own => vec![STEP_OWN],
        Order::TableOnly => vec![c.table],
        Order::TableThenBase => [vec![c.table], base()].concat(),
        Order::BaseThenTable => [base(), vec![c.table]].concat(),
        Order::Lobby => base(),
    }
}

/// Whether `k` is `ancestor` or derives from it.
pub fn is_a(mut k: EeWonCfgKind, ancestor: EeWonCfgKind) -> bool {
    loop {
        if k == ancestor {
            return true;
        }
        match class(k).base {
            Some(b) => k = b,
            None => return false,
        }
    }
}

/// `GetPredefinedComponent(name)` of a `k` config: the first of its class's predefined children, then its base's,
/// ... that the name names (exactly): the new child config's kind and the component's part. ComponentConfig had none.
pub fn predefined(mut k: EeWonCfgKind, name: &[u8]) -> Option<(EeWonCfgKind, EeWonCfgPart)> {
    loop {
        let c = class(k);
        for &(pre, kind, part) in c.predefined {
            let hit = match pre {
                Pre::Key(table, index) => key_index(table, name) == index,
                Pre::Text(t) => t == name,
            };
            if hit {
                return Some((kind, part));
            }
        }
        k = c.base?;
    }
}

/// `GetNamedValue(name)`'s own names of a `k` config: the WON frame's LEFTPAD, RIGHTPAD, TOPPAD, BOTTOMPAD, HORZPAD
/// and VERTPAD, each the sum of these frame borders (0 left, 1 top, 2 right, 3 bottom, read in this order); None for
/// any other kind or name (the base config's own then answer).
pub fn named_borders(k: EeWonCfgKind, name: &[u8]) -> Option<&'static [i32]> {
    if k != EeWonCfgKind::WonFrameComponent {
        return None;
    }
    match key_index(EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_GETNAMEDVALUE_KEY, name) {
        0 => Some(&[0]),
        1 => Some(&[2]),
        2 => Some(&[1]),
        3 => Some(&[3]),
        4 => Some(&[2, 0]),
        5 => Some(&[3, 1]),
        _ => None,
    }
}

// ---- the C ABI

unsafe fn text<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the caller's `n` bytes
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// `k`'s `Virtual24` steps into `out` (at most `cap`; [`STEP_OWN`] -1): their number.
///
/// # Safety
/// `out` holds `cap` ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_kind_steps(k: EeWonCfgKind, out: *mut i32, cap: i32) -> i32 {
    let s = v24_steps(k);
    for (i, &v) in s.iter().take(cap.max(0) as usize).enumerate() {
        // SAFETY: the contract
        unsafe { *out.add(i) = v };
    }
    s.len() as i32
}

/// Whether kind `k` is `ancestor` or derives from it (the C++'s `dynamic_cast` of the original classes).
#[no_mangle]
pub extern "C" fn ee_won_cfg_kind_is(k: EeWonCfgKind, ancestor: EeWonCfgKind) -> bool {
    is_a(k, ancestor)
}

/// The predefined child `name` (`len` bytes) of a `k` config: true with its config's kind and the part.
///
/// # Safety
/// `name` holds `len` bytes; the outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_kind_predefined(
    k: EeWonCfgKind,
    name: *const u8,
    len: usize,
    out_kind: *mut EeWonCfgKind,
    out_part: *mut EeWonCfgPart,
) -> bool {
    // SAFETY: the contract
    match predefined(k, unsafe { text(name, len) }) {
        Some((kind, part)) => {
            // SAFETY: the contract
            unsafe {
                *out_kind = kind;
                *out_part = part;
            }
            true
        }
        None => false,
    }
}

/// The frame borders (at most 2, into `out`) a `k` config's named value `name` sums: their number, 0 when the kind
/// has no such name.
///
/// # Safety
/// As `ee_won_cfg_kind_predefined`; `out` holds 2 ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_kind_named(k: EeWonCfgKind, name: *const u8, len: usize, out: *mut i32) -> i32 {
    // SAFETY: the contract
    match named_borders(k, unsafe { text(name, len) }) {
        Some(b) => {
            for (i, &v) in b.iter().enumerate() {
                // SAFETY: the contract (two at most)
                unsafe { *out.add(i) = v };
            }
            b.len() as i32
        }
        None => 0,
    }
}

#[cfg(test)]
#[path = "cfgkind_tests.rs"]
mod tests;
