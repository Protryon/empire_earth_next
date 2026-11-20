use super::*;
use EeWonCfgKind as K;

#[test]
fn chains_follow_the_classes() {
    // ComponentConfig: its own keys and table
    assert_eq!(v24_steps(K::Component), vec![STEP_OWN]);
    // ListAreaConfig: ListArea, ScrollArea, Container, Component
    assert_eq!(
        v24_steps(K::ListArea),
        vec![
            EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION,
            EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION,
            EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION,
            STEP_OWN
        ]
    );
    // WONHeaderControlConfig asked its base first
    assert_eq!(
        v24_steps(K::WonHeaderControl),
        vec![
            EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION,
            EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION,
            STEP_OWN,
            EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY
        ]
    );
    // the image configs end at ImageConfig's table, which has no base call
    assert_eq!(
        v24_steps(K::StretchImage),
        vec![
            EE_WON_CFG_KEYS_STRETCHIMAGECONFIG_VIRTUAL24_INSTRUCTION,
            EE_WON_CFG_KEYS_IMAGECONFIG_VIRTUAL24_ININSTRUCTION
        ]
    );
    assert_eq!(v24_steps(K::WonSkin), vec![EE_WON_CFG_KEYS_WONSKINCONFIG_VIRTUAL24_THEINSTRUCTION]);
    // every chain is short enough for the C++'s buffer, every table a table
    for k in KINDS {
        let s = v24_steps(k);
        assert!(!s.is_empty() && s.len() <= 8, "{k:?}");
        assert!(s.iter().all(|&t| t == STEP_OWN || t >= 0));
    }
}

#[test]
fn kinds_derive_as_the_classes_did() {
    assert!(is_a(K::MsListCtrl, K::ScrollbarScroller));
    assert!(is_a(K::MsListCtrl, K::Container));
    assert!(is_a(K::ScreenContainer, K::Container));
    assert!(!is_a(K::Button, K::Container));
    assert!(!is_a(K::Font, K::Container));
    assert!(is_a(K::ImageFilter, K::Image));
    for k in KINDS {
        assert!(is_a(k, K::Component));
    }
    assert_eq!(KINDS.iter().enumerate().filter(|(i, k)| **k as usize != *i).count(), 0);
}

#[test]
fn predefined_children_walk_the_bases() {
    assert_eq!(predefined(K::ComboBox, b"LISTAREA"), Some((K::ListArea, EeWonCfgPart::ComboList)));
    assert_eq!(predefined(K::WonComboBox, b"INPUTBOX"), Some((K::InputBox, EeWonCfgPart::ComboEdit)));
    assert_eq!(predefined(K::MsScrollbar, b"THUMB"), Some((K::Component, EeWonCfgPart::Thumb)));
    assert_eq!(predefined(K::MsListCtrl, b"HEADER"), Some((K::MsHeaderControl, EeWonCfgPart::Header)));
    assert_eq!(predefined(K::MsListCtrl, b"VERTSCROLLBAR"), Some((K::MsScrollbar, EeWonCfgPart::VertBar)));
    assert_eq!(predefined(K::WonListCtrl, b"HORZSCROLLBAR"), Some((K::WonScrollbar, EeWonCfgPart::HorzBar)));
    assert_eq!(predefined(K::MsTabCtrl, b"CONTAINERLAYOUT"), Some((K::Component, EeWonCfgPart::TabLayout)));
    assert_eq!(predefined(K::Browser, b"HTMLVIEW"), Some((K::ScrollArea, EeWonCfgPart::HtmlView)));
    assert_eq!(predefined(K::MsEtchedFrameGroup, b"LABEL"), Some((K::Label, EeWonCfgPart::EtchedLabel)));
    // exact names; the plain scrollbar scroller and the container have none
    assert_eq!(predefined(K::MsListCtrl, b"header"), None);
    assert_eq!(predefined(K::ScrollbarScroller, b"HORZSCROLLBAR"), None);
    assert_eq!(predefined(K::Container, b"LISTAREA"), None);
    assert_eq!(predefined(K::Component, b""), None);
}

#[test]
fn frame_paddings() {
    assert_eq!(named_borders(K::WonFrameComponent, b"LEFTPAD"), Some(&[0][..]));
    assert_eq!(named_borders(K::WonFrameComponent, b"HORZPAD"), Some(&[2, 0][..]));
    assert_eq!(named_borders(K::WonFrameComponent, b"VERTPAD"), Some(&[3, 1][..]));
    assert_eq!(named_borders(K::WonFrameComponent, b"WIDTH"), None);
    assert_eq!(named_borders(K::Component, b"LEFTPAD"), None);
}
