//! The game's free UI helpers (EEWonNetAvailableGameFormManager.cpp's, LLE and UI floor lane round 1): the
//! "<string id>::text" reference and `EETranslateString`, the scenario's briefing (`EEDisplayScenarioIntro`) and the
//! F9 screen shots (`EEPostF9Key`), over one-statement shims.

use ee_base::lle_enums::ISModifierState;

use crate::screens::game_cpp::{Cpp, EeScreensShimText};
use crate::screens::ui_ids::EEFormID;

/// `sscanf(text, "%lu")` on a 32-bit `unsigned long`: white space, a sign, digits (saturating); `None` without a digit.
fn scan_unsigned(text: &[u8]) -> Option<u32> {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | b'\r' | 0x0b | 0x0c) {
        i += 1;
    }
    let negative = text.get(i) == Some(&b'-');
    if matches!(text.get(i), Some(b'+' | b'-')) {
        i += 1;
    }
    let digits = text[i..].iter().take_while(|b| b.is_ascii_digit()).count();
    if digits == 0 {
        return None;
    }
    let mut value = 0u32;
    let mut overflow = false;
    for &b in &text[i..i + digits] {
        match value.checked_mul(10).and_then(|v| v.checked_add(u32::from(b - b'0'))) {
            Some(v) => value = v,
            None => overflow = true,
        }
    }
    Some(if overflow {
        u32::MAX
    } else if negative {
        value.wrapping_neg()
    } else {
        value
    })
}

/// `EEGetLanguageStringReference`: the string id of a "<string id>::text" reference (exactly two tokens between
/// colons, the first a number).
pub fn language_string_reference(text: &[u8]) -> Option<u32> {
    let tokens = ee_base::ustring::tokenize(text, b"::");
    if tokens.len() != 2 {
        return None;
    }
    scan_unsigned(&text[tokens[0].0 as usize..=tokens[0].1 as usize])
}

/// `EETranslateString` into the campaign editor's translated text: the reference's language string, else the text.
pub(crate) fn translate<C: Cpp + ?Sized>(c: &mut C, text: &[u8]) {
    // (the C++ string ends at the first 0 byte)
    let text = &text[..text.iter().position(|b| *b == 0).unwrap_or(text.len())];
    let id = language_string_reference(text);
    c.uh_translate(EeScreensShimText::of(text), id.unwrap_or(0), id.is_some());
}

/// `EEDisplayScenarioIntro`: opens the scenario intro (pausing the game) when a single-player scenario starts with
/// intro media; a campaign's media get the campaign folder's prefix.
pub(crate) fn display_scenario_intro<C: Cpp + ?Sized>(c: &mut C) {
    let intro = c.uh_form(EEFormID::kFormScenarioIntro as u32);
    if !c.gv_is_server() || !c.uh_comms_started() || c.uh_comms_multiplayer() || intro == 0 {
        return;
    }
    if c.uh_has_scenario() {
        c.uh_intro_media_copy();
        if c.uh_is_campaign() {
            c.uh_intro_media_prefix();
        }
    }
    if !c.uh_intro_has_media() {
        return;
    }
    c.uh_intro_set_loading_from_game(false);
    c.uh_intro_set_in_game(true);
    let server = c.server_dynamic();
    c.server_pause(server);
    let options = c.uh_form(EEFormID::kFormGameOptions as u32);
    let intro = c.uh_form(EEFormID::kFormScenarioIntro as u32);
    if c.cbh_current_form() == options {
        c.ev_open_form(intro);
    } else {
        c.uh_switch_to_form(intro);
    }
    c.post_pause_information(true);
}

/// `EEPostF9Key`: Alt, Control or Shift alone choose the in-game shots (4096 wide, 1024 wide, the screen's size
/// without the interface) on the game's and the editor's forms; otherwise the plain screen shot.
pub(crate) fn post_f9<C: Cpp + ?Sized>(c: &mut C) {
    let modifiers = c.mm_modifiers();
    let in_game = c.uh_in_game_form();
    if in_game && modifiers == ISModifierState::kModifierAlt as u32 {
        c.uh_post_screen_shot(0x1000, false);
    } else if in_game && modifiers == ISModifierState::kModifierControl as u32 {
        c.uh_post_screen_shot(0x400, false);
    } else if in_game && modifiers == ISModifierState::kModifierShift as u32 {
        c.uh_post_screen_shot(-1, false);
    } else {
        c.uh_post_screen_shot(-1, true);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn references() {
        assert_eq!(language_string_reference(b"1234::Hello"), Some(1234));
        assert_eq!(language_string_reference(b" 12x::Hello"), Some(12));
        assert_eq!(language_string_reference(b":::7:text::"), Some(7));
        assert_eq!(language_string_reference(b"Hello"), None);
        assert_eq!(language_string_reference(b"a::b"), None);
        assert_eq!(language_string_reference(b"1::b::c"), None);
        assert_eq!(language_string_reference(b""), None);
        assert_eq!(language_string_reference(b"-1::b"), Some(u32::MAX));
        assert_eq!(language_string_reference(b"99999999999::b"), Some(u32::MAX));
        assert_eq!(language_string_reference(b"+5::b"), Some(5));
        assert_eq!(language_string_reference(b"-::b"), None);
    }

    #[test]
    fn translates() {
        let mut f = Fake::default();
        translate(&mut f, b"32::x");
        translate(&mut f, b"plain");
        assert_eq!(f.calls(), "uh_translate(\"32::x\", 32, true)\nuh_translate(\"plain\", 0, false)");
    }

    #[test]
    fn the_intro() {
        // not the server: nothing
        let mut f = Fake::default();
        f.answer("uh_form", &[5]);
        display_scenario_intro(&mut f);
        assert_eq!(f.calls(), "uh_form(19)\ngv_is_server()");
        // a campaign's scenario from the options menu: opened over it
        let mut f = Fake::default();
        f.answer("uh_form", &[5, 9, 5]).answer("gv_is_server", &[1]).answer("uh_comms_started", &[1]);
        f.answer("uh_has_scenario", &[1]).answer("uh_is_campaign", &[1]).answer("uh_intro_has_media", &[1]);
        f.answer("server_dynamic", &[77]).answer("cbh_current_form", &[9]);
        display_scenario_intro(&mut f);
        assert_eq!(
            f.calls(),
            "uh_form(19)\ngv_is_server()\nuh_comms_started()\nuh_comms_multiplayer()\nuh_has_scenario()\n\
             uh_intro_media_copy()\nuh_is_campaign()\nuh_intro_media_prefix()\nuh_intro_has_media()\n\
             uh_intro_set_loading_from_game(false)\nuh_intro_set_in_game(true)\nserver_dynamic()\nserver_pause(77)\n\
             uh_form(28)\nuh_form(19)\ncbh_current_form()\nev_open_form(5)\npost_pause_information(true)"
        );
        // no scenario manager, media kept from before, another form current: switched to
        let mut f = Fake::default();
        f.answer("uh_form", &[5, 9, 5]).answer("gv_is_server", &[1]).answer("uh_comms_started", &[1]);
        f.answer("uh_intro_has_media", &[1]).answer("cbh_current_form", &[3]);
        display_scenario_intro(&mut f);
        assert!(f.calls().ends_with("cbh_current_form()\nuh_switch_to_form(5)\npost_pause_information(true)"));
        assert!(!f.calls().contains("media_copy"));
        // no media: nothing more; a multiplayer session: nothing
        let mut f = Fake::default();
        f.answer("uh_form", &[5]).answer("gv_is_server", &[1]).answer("uh_comms_started", &[1]);
        display_scenario_intro(&mut f);
        assert!(f.calls().ends_with("uh_has_scenario()\nuh_intro_has_media()"));
        let mut f = Fake::default();
        f.answer("uh_form", &[5]).answer("gv_is_server", &[1]).answer("uh_comms_started", &[1]);
        f.answer("uh_comms_multiplayer", &[1]);
        display_scenario_intro(&mut f);
        assert!(f.calls().ends_with("uh_comms_multiplayer()"));
    }

    #[test]
    fn the_f9_shots() {
        let shot = |modifiers: i64, in_game: i64| {
            let mut f = Fake::default();
            f.answer("mm_modifiers", &[modifiers]).answer("uh_in_game_form", &[in_game]);
            post_f9(&mut f);
            f.calls().lines().last().unwrap().to_string()
        };
        assert_eq!(shot(4, 1), "uh_post_screen_shot(4096, false)");
        assert_eq!(shot(2, 1), "uh_post_screen_shot(1024, false)");
        assert_eq!(shot(1, 1), "uh_post_screen_shot(-1, false)");
        assert_eq!(shot(0, 1), "uh_post_screen_shot(-1, true)");
        assert_eq!(shot(3, 1), "uh_post_screen_shot(-1, true)");
        assert_eq!(shot(4, 0), "uh_post_screen_shot(-1, true)");
    }
}
