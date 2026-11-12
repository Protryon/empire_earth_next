//! The custom campaign form whole (form 34, opened by the play campaign screen's Custom Campaign): the campaign files
//! of `data\Campaigns` that are not built in, the chosen one's description, and OK into its scenario choice; the
//! original `EEUIEChooseCustomCampaignEvents.cpp`'s events and calls over a host ([`CustomCampaignHost`]) whose
//! methods are each one toolkit or game call (`UI Events/EEScreens.cpp`). The form keeps no state of its own (the
//! campaign form manager's fields are the game's, behind the host).

use crate::menus::{
    EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN, EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION, EE_SCREENS_TEXT_CAMPAIGN_FIRST,
    EE_SCREENS_TEXT_CAMPAIGN_NAME,
};
use ee_plat_core::utf8;

// ---- the events (EEScreenEvent::mEvent)

/// The form's open event (set by the play campaign screen), its close event (nothing), the campaign list's
/// selection, OK, Cancel, and the no-campaigns message's callback (no event object).
pub const EE_SCREENS_CC_OPEN: u32 = 0x2201;
pub const EE_SCREENS_CC_CLOSE: u32 = 0x2202;
pub const EE_SCREENS_CC_SELECT: u32 = 0x2203;
pub const EE_SCREENS_CC_OK: u32 = 0x2204;
pub const EE_SCREENS_CC_CANCEL: u32 = 0x2205;
pub const EE_SCREENS_CC_NONE_FOUND: u32 = 0x2206;

// ---- the host's ops (ee_screenscpp_call; menus.rs and settings_form.rs have the shared ones)

pub const EE_SCREENS_OP_FORM_LIST_CLEAR: u32 = 0x160; // (form, control) DeleteAllEntries
pub const EE_SCREENS_OP_FORM_LIST_COUNT: u32 = 0x161; // (form, control) -> GetLineCount
pub const EE_SCREENS_OP_FORM_LIST_SELECT: u32 = 0x162; // (form, control, line) ChangeSelectionIndex(line, true, false)
pub const EE_SCREENS_OP_FORM_LIST_SELECTION: u32 = 0x163; // (form, control) -> GetCurrentSelection's selection
/// Texts (ee_screenscpp_text / _put_text).
pub const EE_SCREENS_TEXT_FORM_LIST_DATA: u32 = 0x30; // (form << 24 | control << 16 | line) a list box's line
pub const EE_SCREENS_TEXT_FORM_LIST_APPEND: u32 = 0x31; // put (form << 16 | control) a list box's AppendLine
pub const EE_SCREENS_TEXT_CAMPAIGN_FILES: u32 = 0x32; // the files "*" + extension of data\Campaigns, each ended by 0
pub const EE_SCREENS_TEXT_CAMPAIGN_EXTENSION: u32 = 0x33; // gCampaignExtension
pub const EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN: u32 = 0x34; // put (a name) -> 1 for a built-in campaign
pub const EE_SCREENS_TEXT_CAMPAIGN_ERROR: u32 = 0x35; // the last campaign load's error text
pub const EE_SCREENS_TEXT_MESSAGE_NONE_FOUND: u32 = 0x36; // put: DisplayMessage(text, true, NULL, none found, true)

// the forms, controls, triggers and strings (ee/EEUIManager.h, lle/UIControl.h, ee/EEStringIDs.h)
const CC_FORM: u32 = 34;
const CC_FORM_SCENARIOS: u32 = 33;
const CC_DESCRIPTION: u32 = 1;
const CC_LIST: u32 = 2;
const CC_CANCEL_BUTTON: u32 = 3;
const CC_OK_BUTTON: u32 = 4;
const CC_TRIGGER_SELECTION_CHANGED: u32 = 0x3eb;
const CC_TRIGGER_BUTTON_UP: u32 = 0x3f0;
const CC_IDS_NO_CUSTOM_CAMPAIGNS: u32 = 7175;
const CC_IDS_PLEASE_WAIT: u32 = 31842;

/// The toolkit's and the game's calls the custom campaign form makes. Texts are the game's UTF-8 bytes.
pub trait CustomCampaignHost {
    /// Control `control` of form `form`: `SetTriggerEvent(trigger, event)`.
    fn set_trigger(&mut self, form: u32, control: u32, trigger: u32, event: u32);
    fn set_open_event(&mut self, form: u32, event: u32);
    fn set_close_event(&mut self, form: u32, event: u32);
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to three arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32;
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    /// A text into the game; the host's answer (a built-in campaign, a campaign that loads: 1).
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32;
    /// A resource string (whatever `GetString` left when it fails), and one that may be missing.
    fn string(&mut self, id: u32) -> Vec<u8>;
    fn string_checked(&mut self, id: u32) -> Option<Vec<u8>>;
}

/// The form's event for a control event id: none (the form hangs its own on its controls when it opens).
pub fn is_custom_campaign_event(event: u32) -> bool {
    (EE_SCREENS_CC_OPEN..=EE_SCREENS_CC_NONE_FOUND).contains(&event)
}

/// `GetIndexedSubstring(first, last)` of a `UWideString`: the characters `first..=last` (`PlatMbsNInc`'s steps).
fn substring(text: &[u8], first: usize, last: usize) -> &[u8] {
    let start = utf8::bytes_of_chars(text, first);
    let rest = &text[start..];
    &rest[..utf8::bytes_of_chars(rest, last + 1 - first)]
}

/// A campaign file's list line: its name without the campaign extension (compared lower-cased, `PlatMbsLwr`), a
/// name of four characters or fewer as it is.
pub fn campaign_name<'a>(file: &'a [u8], extension: &[u8]) -> &'a [u8] {
    let length = utf8::char_len(file);
    if length > 4 {
        let mut tail = substring(file, length - 4, length - 1).to_vec();
        utf8::map_in_place(&mut tail, utf8::fold);
        if tail == extension {
            return substring(file, 0, length - 5);
        }
    }
    file
}

fn list_text<H: CustomCampaignHost>(host: &mut H, line: u32) -> Vec<u8> {
    host.text(EE_SCREENS_TEXT_FORM_LIST_DATA, CC_FORM << 24 | CC_LIST << 16 | (line & 0xffff))
}

/// One of the form's events; false for another code (nothing called).
pub fn dispatch<H: CustomCampaignHost>(host: &mut H, event: u32) -> bool {
    match event {
        EE_SCREENS_CC_OPEN => {
            // the campaign files that are not built in, the first selected (none: a message that closes the form)
            host.set_trigger(CC_FORM, CC_LIST, CC_TRIGGER_SELECTION_CHANGED, EE_SCREENS_CC_SELECT);
            host.set_trigger(CC_FORM, CC_OK_BUTTON, CC_TRIGGER_BUTTON_UP, EE_SCREENS_CC_OK);
            host.set_trigger(CC_FORM, CC_CANCEL_BUTTON, CC_TRIGGER_BUTTON_UP, EE_SCREENS_CC_CANCEL);
            host.call(EE_SCREENS_OP_FORM_LIST_CLEAR, CC_FORM, CC_LIST, 0);
            let files = host.text(EE_SCREENS_TEXT_CAMPAIGN_FILES, 0);
            let extension = host.text(EE_SCREENS_TEXT_CAMPAIGN_EXTENSION, 0);
            for file in files.split(|&b| b == 0).take(files.iter().filter(|&&b| b == 0).count()) {
                let name = campaign_name(file, &extension).to_vec();
                if host.put_text(EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN, 0, &name) == 0 {
                    host.put_text(EE_SCREENS_TEXT_FORM_LIST_APPEND, CC_FORM << 16 | CC_LIST, &name);
                }
            }
            if host.call(EE_SCREENS_OP_FORM_LIST_COUNT, CC_FORM, CC_LIST, 0) == 0 {
                let text = host.string(CC_IDS_NO_CUSTOM_CAMPAIGNS);
                host.put_text(EE_SCREENS_TEXT_MESSAGE_NONE_FOUND, 0, &text);
            } else {
                host.call(EE_SCREENS_OP_FORM_LIST_SELECT, CC_FORM, CC_LIST, 0);
            }
        }
        EE_SCREENS_CC_SELECT => {
            // the selected campaign's description (or its load error) into the campaign form manager
            let selection = host.call(EE_SCREENS_OP_FORM_LIST_SELECTION, CC_FORM, CC_LIST, 0);
            let name = list_text(host, selection);
            let text = if host.put_text(crate::menus::EE_SCREENS_TEXT_CAMPAIGN_LOAD, 0, &name) != 0 {
                let description = host.text(crate::menus::EE_SCREENS_TEXT_CAMPAIGN_LOADED, 0);
                let first = host.text(crate::menus::EE_SCREENS_TEXT_CAMPAIGN_CIV, 0);
                host.put_text(EE_SCREENS_TEXT_CAMPAIGN_NAME, 0, &name);
                host.put_text(EE_SCREENS_TEXT_CAMPAIGN_FIRST, 0, &first);
                host.put_text(EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION, 0, &description);
                description
            } else {
                let error = host.text(EE_SCREENS_TEXT_CAMPAIGN_ERROR, 0);
                for which in [
                    EE_SCREENS_TEXT_CAMPAIGN_NAME,
                    EE_SCREENS_TEXT_CAMPAIGN_FIRST,
                    EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION,
                ] {
                    host.put_text(which, 0, b"");
                }
                error
            };
            host.put_text(crate::menus::EE_SCREENS_TEXT_LABEL_LINES, CC_FORM << 16 | CC_DESCRIPTION, &text);
        }
        EE_SCREENS_CC_OK => {
            // the chosen campaign's scenario choice (its open scenarios only), this form closed behind Please wait
            host.call(EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN, 0, 0, 0);
            host.set_open_event(CC_FORM_SCENARIOS, crate::scenario_screens::EE_SCREENS_CS_OPEN);
            host.set_close_event(CC_FORM_SCENARIOS, crate::scenario_screens::EE_SCREENS_CS_CLOSE);
            if let Some(text) = host.string_checked(CC_IDS_PLEASE_WAIT) {
                host.call(crate::settings_form::EE_SCREENS_OP_CLOSE_FORM, 0, 0, 0);
                host.put_text(crate::menus::EE_SCREENS_TEXT_MESSAGE, 0, &text);
            }
            host.call(crate::menus::EE_SCREENS_OP_OPEN_FORM, CC_FORM_SCENARIOS, 0, 0);
        }
        EE_SCREENS_CC_CANCEL | EE_SCREENS_CC_NONE_FOUND => {
            host.call(crate::settings_form::EE_SCREENS_OP_CLOSE_FORM, 0, 0, 0);
        }
        EE_SCREENS_CC_CLOSE => {}
        _ => return false,
    }
    true
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn campaign_names() {
        assert_eq!(campaign_name(b"Mine.ssa", b".ssa"), b"Mine");
        assert_eq!(campaign_name(b"Mine.SSA", b".ssa"), b"Mine");
        assert_eq!(campaign_name(b"Mine.ssb", b".ssa"), b"Mine.ssb");
        assert_eq!(campaign_name(b".ssa", b".ssa"), b".ssa"); // four characters: kept
        assert_eq!(campaign_name(b"x.ssa", b".ssa"), b"x");
        assert_eq!(campaign_name("Zo\u{eb}.ssa".as_bytes(), b".ssa"), "Zo\u{eb}".as_bytes());
        assert_eq!(campaign_name("\u{e9}.SSA".as_bytes(), b".ssa"), "\u{e9}".as_bytes());
        // the extension's characters, not bytes: an accented one is lower-cased in place and stays different
        assert_eq!(campaign_name("ab.\u{c9}SA".as_bytes(), b".ssa"), "ab.\u{c9}SA".as_bytes());
    }

    #[test]
    fn substrings_are_characters() {
        assert_eq!(substring("a\u{eb}bc".as_bytes(), 1, 2), "\u{eb}b".as_bytes());
        assert_eq!(substring(b"abc", 0, 5), b"abc");
        assert_eq!(substring(b"abc", 3, 3), b"");
    }

    #[test]
    fn events() {
        assert!(is_custom_campaign_event(EE_SCREENS_CC_OPEN) && is_custom_campaign_event(EE_SCREENS_CC_NONE_FOUND));
        assert!(!is_custom_campaign_event(0x2200) && !is_custom_campaign_event(0x2207));
    }
}
