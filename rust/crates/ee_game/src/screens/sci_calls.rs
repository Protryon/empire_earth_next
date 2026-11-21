//! The scenario introduction form's manager (`EEUISCIFormManager`, `UI Events/EEUIEScenarioIntroEvents.cpp`): its
//! bodies in Rust over one-statement C++ shims (UI lane round 9; the shims are `ee_screenscpp_sci_*` in that file).
//! The C++ members stay the class's (its callers' types), each one forward to the `ee_screens_sci_*` below. `m` is
//! the `EEUISCIFormManager*`, `spec` an `FSFileSpec*`.

use crate::screens::game_cpp::{Cpp, EeScreensShimOut as Out, EeScreensShimText as Text};
use crate::screens::main_form::{EE_SCREENS_MF_CTL_BRIEFING, EE_SCREENS_MF_CTL_BRIEFING_ALERT};

/// A media string's bytes with an extension (`theName = mMediaStrings.m...; theName += ext`).
fn name_with(f: impl FnOnce(Out), extension: &[u8]) -> Vec<u8> {
    let mut name = Vec::new();
    f(Out::of(&mut name));
    name.extend_from_slice(extension);
    name
}

/// `Clear`: no open files and every flag cleared.
pub(crate) fn clear<C: Cpp + ?Sized>(c: &mut C, m: usize) {
    c.sci_map_null(m);
    c.sci_file_null(m);
    c.sci_sound_null(m);
    c.sci_clear_loading(m);
    c.sci_clear_in_game(m);
    c.sci_clear_ba0(m);
    c.sci_clear_campaign(m);
    c.sci_clear_scroll(m);
}

/// `DeleteObjects`: the map, scenario and sound files deleted.
pub(crate) fn delete_objects<C: Cpp + ?Sized>(c: &mut C, m: usize) {
    c.sci_delete_map(m);
    c.sci_map_null(m);
    c.sci_delete_file(m);
    c.sci_file_null(m);
    c.sci_delete_sound(m);
    c.sci_sound_null(m);
}

/// `Get`: the scenario intro form's manager, made and attached on first use.
pub(crate) fn get<C: Cpp + ?Sized>(c: &mut C) -> usize {
    let form = c.sci_form();
    let manager = c.sci_form_manager(form);
    if manager != 0 {
        return manager;
    }
    let manager = c.sci_new();
    c.sci_form_set_manager(form, manager);
    manager
}

/// `OpenMap`: the scenario's map picture (.jpg, else .png) from Scenarios; whether one was found.
pub(crate) fn open_map<C: Cpp + ?Sized>(c: &mut C, m: usize) -> bool {
    if c.sci_map(m) != 0 {
        c.sci_delete_map(m);
    }
    c.sci_map_null(m);
    let jpeg = name_with(|o| c.sci_map_name(m, o), b".jpg");
    if c.sci_open_map(m, Text::of(&jpeg)) == 0 {
        return true;
    }
    let png = name_with(|o| c.sci_map_name(m, o), b".png");
    c.sci_open_map(m, Text::of(&png)) == 0
}

/// `OpenScenario`: the scenario file again by name from Scenarios or the data path; whether it was found (false
/// without one).
pub(crate) fn open_scenario<C: Cpp + ?Sized>(c: &mut C, m: usize) -> bool {
    if c.sci_file(m) == 0 {
        return false;
    }
    let name = name_with(|o| c.sci_file_name(m, o), b"");
    c.sci_delete_file(m);
    c.sci_file_null(m);
    c.sci_open_file(m, Text::of(&name)) == 0 || c.sci_find_file(m, Text::of(&name)) == 0
}

/// `OpenSound`: the scenario's sound (.mp3) from Sounds; whether it was found.
pub(crate) fn open_sound<C: Cpp + ?Sized>(c: &mut C, m: usize) -> bool {
    if c.sci_sound(m) != 0 {
        c.sci_delete_sound(m);
    }
    c.sci_sound_null(m);
    let name = name_with(|o| c.sci_sound_name(m, o), b".mp3");
    c.sci_open_sound(m, Text::of(&name)) == 0
}

/// `GetMap`, `GetScenario`, `GetSound`: the file opened and its spec copied into `spec`; whether it was found.
pub(crate) fn get_map<C: Cpp + ?Sized>(c: &mut C, m: usize, spec: usize) -> bool {
    let found = open_map(c, m);
    if found {
        c.sci_spec_from_map(m, spec);
    }
    found
}
pub(crate) fn get_scenario<C: Cpp + ?Sized>(c: &mut C, m: usize, spec: usize) -> bool {
    let found = open_scenario(c, m);
    if found {
        c.sci_spec_from_file(m, spec);
    }
    found
}
pub(crate) fn get_sound<C: Cpp + ?Sized>(c: &mut C, m: usize, spec: usize) -> bool {
    let found = open_sound(c, m);
    if found {
        c.sci_spec_from_sound(m, spec);
    }
    found
}

/// `HasMedia`: any briefing, hints, history, map or sound (the files opened only as far as needed).
pub(crate) fn has_media<C: Cpp + ?Sized>(c: &mut C, m: usize) -> bool {
    c.sci_has_instructions(m) || c.sci_has_hints(m) || c.sci_has_history(m) || open_map(c, m) || open_sound(c, m)
}

/// `SetBlinking`: the main game's briefing button blinking or not, the large briefing alert shown while it blinks.
pub(crate) fn set_blinking<C: Cpp + ?Sized>(c: &mut C, blinking: bool) {
    let form = c.sci_main_form();
    let button = c.sci_main_control(form, EE_SCREENS_MF_CTL_BRIEFING as u32);
    let other = c.sci_main_control(form, EE_SCREENS_MF_CTL_BRIEFING_ALERT as u32);
    c.sci_set_info_blinking(form, blinking);
    if button == 0 {
        return;
    }
    c.sci_button_blinking(button, blinking);
    if other == 0 {
        return;
    }
    if blinking {
        c.sci_button_shown(other);
    } else {
        c.sci_button_hidden(other);
    }
    c.sci_button_blinking(other, blinking);
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::game_cpp::Ffi;

    /// `EEUISCIFormManager::Clear`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_clear(m: usize) {
        super::clear(&mut Ffi, m)
    }
    /// `EEUISCIFormManager::DeleteObjects`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_delete_objects(m: usize) {
        super::delete_objects(&mut Ffi, m)
    }
    /// `EEUISCIFormManager::Get`: the manager (an `EEUISCIFormManager*`).
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_get() -> usize {
        super::get(&mut Ffi)
    }
    /// `EEUISCIFormManager::OpenMap`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_open_map(m: usize) -> bool {
        super::open_map(&mut Ffi, m)
    }
    /// `EEUISCIFormManager::OpenScenario`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_open_scenario(m: usize) -> bool {
        super::open_scenario(&mut Ffi, m)
    }
    /// `EEUISCIFormManager::OpenSound`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_open_sound(m: usize) -> bool {
        super::open_sound(&mut Ffi, m)
    }
    /// `EEUISCIFormManager::GetMap` (`spec` the `FSFileSpec*` out).
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_get_map(m: usize, spec: usize) -> bool {
        super::get_map(&mut Ffi, m, spec)
    }
    /// `EEUISCIFormManager::GetScenario`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_get_scenario(m: usize, spec: usize) -> bool {
        super::get_scenario(&mut Ffi, m, spec)
    }
    /// `EEUISCIFormManager::GetSound`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_get_sound(m: usize, spec: usize) -> bool {
        super::get_sound(&mut Ffi, m, spec)
    }
    /// `EEUISCIFormManager::HasMedia`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_has_media(m: usize) -> bool {
        super::has_media(&mut Ffi, m)
    }
    /// `EEUISCIFormManager::SetBlinking`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sci_set_blinking(blinking: bool) {
        super::set_blinking(&mut Ffi, blinking)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    const M: usize = 0x30;

    #[test]
    fn clear_and_delete_reset_every_file() {
        let mut f = Fake::default();
        clear(&mut f, M);
        delete_objects(&mut f, M);
        let calls = f.calls();
        assert!(calls.starts_with("sci_map_null(48)\nsci_file_null(48)\nsci_sound_null(48)\nsci_clear_loading(48)"));
        assert!(calls.ends_with("sci_delete_sound(48)\nsci_sound_null(48)"));
    }

    #[test]
    fn the_manager_is_made_once() {
        let mut f = Fake::default();
        f.answer("sci_form", &[0x10, 0x10]).answer("sci_form_manager", &[0, 0x40]).answer("sci_new", &[0x40]);
        assert_eq!(get(&mut f), 0x40);
        assert_eq!(get(&mut f), 0x40);
        assert_eq!(
            f.calls(),
            "sci_form()\nsci_form_manager(16)\nsci_new()\nsci_form_set_manager(16, 64)\nsci_form()\nsci_form_manager(16)"
        );
    }

    #[test]
    fn the_map_tries_jpeg_then_png() {
        let mut f = Fake::default();
        f.answer("sci_map", &[1]).text("sci_map_name", b"m").text("sci_map_name", b"m").answer("sci_open_map", &[1, 0]);
        assert!(open_map(&mut f, M));
        assert_eq!(
            f.calls(),
            "sci_map(48)\nsci_delete_map(48)\nsci_map_null(48)\nsci_map_name(48, out)\nsci_open_map(48, \"m.jpg\")\n\
             sci_map_name(48, out)\nsci_open_map(48, \"m.png\")"
        );
        let mut f = Fake::default();
        f.answer("sci_open_sound", &[1]);
        assert!(!get_sound(&mut f, M, 0x50));
        assert!(!f.calls().contains("sci_spec_from_sound"));
    }

    #[test]
    fn the_scenario_reopens_by_name_or_from_the_data_path() {
        let mut f = Fake::default();
        assert!(!open_scenario(&mut f, M));
        assert_eq!(f.calls(), "sci_file(48)");
        let mut f = Fake::default();
        f.answer("sci_file", &[1]).text("sci_file_name", b"s.scn").answer("sci_open_file", &[1]);
        assert!(get_scenario(&mut f, M, 0x50));
        assert!(f
            .calls()
            .ends_with("sci_open_file(48, \"s.scn\")\nsci_find_file(48, \"s.scn\")\nsci_spec_from_file(48, 80)"));
        let mut f = Fake::default();
        f.answer("sci_has_hints", &[1]);
        assert!(has_media(&mut f, M));
        assert_eq!(f.calls(), "sci_has_instructions(48)\nsci_has_hints(48)");
        let mut f = Fake::default();
        f.answer("sci_open_map", &[0]);
        assert!(get_map(&mut f, M, 0x50));
    }

    #[test]
    fn the_briefing_blinks_with_its_alert() {
        let mut f = Fake::default();
        f.answer("sci_main_form", &[0x60]).answer("sci_main_control", &[0x61, 0x62]);
        set_blinking(&mut f, true);
        assert!(f.calls().ends_with(
            "sci_set_info_blinking(96, true)\nsci_button_blinking(97, true)\nsci_button_shown(98)\nsci_button_blinking(98, true)"
        ));
        let mut f = Fake::default();
        f.answer("sci_main_control", &[0x61, 0x62]);
        set_blinking(&mut f, false);
        assert!(f.calls().contains("sci_button_hidden(98)"));
        let mut f = Fake::default();
        set_blinking(&mut f, false);
        assert!(f.calls().ends_with("sci_set_info_blinking(0, false)"));
    }
}
