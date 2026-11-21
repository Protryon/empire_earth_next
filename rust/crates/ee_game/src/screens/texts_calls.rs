//! The game screens' texts that were the original's `ee_screenscpp_text` and `ee_screenscpp_put_text`
//! (`UI Events/EEScreens.cpp`) in Rust over one-statement C++ shims (UI lane round 9; the shims are
//! `ee_screenscpp_tt_*` and `_tp_*` in `EEScreens.cpp`). `game_texts.rs` has round 7's; the dispatchers' adapter
//! (`menus_ffi.rs` `Form`) asks [`text`] and [`put`] after it: with these the C++ hosts are gone.

use core::ffi::c_void;

use ee_base::codec::ffi::ee_ue_decrypt_ustring;

use crate::screens::campaign_editor::{
    EE_SCREENS_CE_ASK_EDITOR, EE_SCREENS_CE_ASK_MAIN, EE_SCREENS_TEXT_CE_APPEND, EE_SCREENS_TEXT_CE_BG_EQUALS,
    EE_SCREENS_TEXT_CE_QUESTION, EE_SCREENS_TEXT_CE_TRANSLATE,
};
use crate::screens::chat::{EE_SCREENS_TEXT_CH_CHAT, EE_SCREENS_TEXT_CL_APPEND};
use crate::screens::civ_choice::{
    EE_SCREENS_TEXT_PDCS_CIV_FILE, EE_SCREENS_TEXT_PDCS_CIV_NAME, EE_SCREENS_TEXT_PDCS_LABEL,
};
use crate::screens::custom_campaign::{
    EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN, EE_SCREENS_TEXT_CAMPAIGN_FILES, EE_SCREENS_TEXT_MESSAGE_NONE_FOUND,
};
use crate::screens::game_calls::string;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut as Out, EeScreensShimText as Text};
use crate::screens::game_texts::parts;
use crate::screens::lobby_form::{
    EE_SCREENS_TEXT_LE_BOOT_QUESTION, EE_SCREENS_TEXT_LE_CHAT, EE_SCREENS_TEXT_LE_ENGINE,
    EE_SCREENS_TEXT_LE_GENERATOR_DISPLAY, EE_SCREENS_TEXT_LE_GENERATOR_NAME, EE_SCREENS_TEXT_LE_MAP_TYPE,
    EE_SCREENS_TEXT_LE_SEND_MAP_TYPE,
};
use crate::screens::main_game::EE_SCREENS_TEXT_MG_CHAT_LOG;
use crate::screens::main_game_options::{
    EE_SCREENS_GO_RESIGN_YES, EE_SCREENS_GO_RESTART_YES, EE_SCREENS_GO_RETURN_TO_EDITOR_YES,
    EE_SCREENS_TEXT_GO_QUESTION,
};
use crate::screens::menus::{
    EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION, EE_SCREENS_TEXT_CAMPAIGN_FIRST, EE_SCREENS_TEXT_CAMPAIGN_NAME,
    EE_SCREENS_TEXT_MESSAGE, EE_SCREENS_TEXT_PLAYER,
};
use crate::screens::post_game::{
    EE_SCREENS_TEXT_PGS_BAR, EE_SCREENS_TEXT_PGS_CAMPAIGN_LOAD, EE_SCREENS_TEXT_PGS_CAMPAIGN_NAME,
    EE_SCREENS_TEXT_PGS_SERVER,
};
use crate::screens::save_load::{
    EE_SCREENS_TEXT_LOAD_SAVED_NAME, EE_SCREENS_TEXT_LOAD_SCENARIO_NAME, EE_SCREENS_TEXT_QUESTION_DELETE,
    EE_SCREENS_TEXT_SAVE_ACTION, EE_SCREENS_TEXT_SAVE_MESSAGE, EE_SCREENS_TEXT_SAVE_QUESTION,
};
use crate::screens::scenario_screens::{
    EE_SCREENS_TEXT_CS_COMPLETED, EE_SCREENS_TEXT_CS_PROGRESS, EE_SCREENS_TEXT_CS_SCENARIOS,
    EE_SCREENS_TEXT_CS_SCENARIO_NAME, EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN,
};
use crate::screens::settings_form::{
    EE_SCREENS_TEXT_ADAPTERS, EE_SCREENS_TEXT_AUDIO_DEVICE, EE_SCREENS_TEXT_OUTPUT_DEVICES, EE_SCREENS_TEXT_QUESTION,
    EE_SCREENS_TEXT_RASTERIZER_SETTING,
};
use crate::screens::splash::{EE_SCREENS_TEXT_SS_COPYRIGHT, EE_SCREENS_TEXT_SS_TITLE};

/// The loading screen's encrypted title and copyright lines, and the stats server's report file (0x563108).
const TT_TITLE: &[u8] = b"B7B2265DB7B2265D2C11B5F7AC114576B5A95DCC84F90596C4116DB6D6B115C626B327BEB7B274D5";
const TT_COPYRIGHT: &[u8] =
    b"B7B227FDB7B227FD1CC12D1714F15D2EFC610D9EB419BD94242B2506FCD147BEA49B253E24C9BD96AC19859696B327BEB7B25FB5";
const TT_REPORT_FILE: &[u8] = b"B7B226CDB7B226CD2C112D1E2C99B50E9C2355F6C48226BEB7B2086D";
/// The engine choices (`ee/EECPEngine.h`) and the lobby's string of the first ("Classic", then "Random").
const TT_ENGINE_CLASSIC: u32 = 0;
const TT_ENGINE_RANDOM: u32 = 1;
const TT_ENGINE_CLASSIC_STRING: u32 = 33170;

/// A decrypted line (`UEDecryptUString` into a `UString`: up to its first 0).
fn decrypted(text: &[u8]) -> Vec<u8> {
    unsafe extern "C" fn sink(context: *mut c_void, bytes: *const u8, length: usize) {
        if length != 0 {
            // SAFETY: the codec's sink contract (`bytes` valid for `length`); `context` is `out` below
            unsafe { (*(context as *mut Vec<u8>)).extend_from_slice(core::slice::from_raw_parts(bytes, length)) };
        }
    }
    let mut out = Vec::new();
    // SAFETY: `text` is a live slice; the sink is called once, during the call
    unsafe { ee_ue_decrypt_ustring(text.as_ptr(), text.len(), sink, &mut out as *mut Vec<u8> as *mut c_void) };
    if let Some(n) = out.iter().position(|b| *b == 0) {
        out.truncate(n);
    }
    out
}

/// A shim's out text.
fn out_text(f: impl FnOnce(Out)) -> Vec<u8> {
    let mut t = Vec::new();
    f(Out::of(&mut t));
    t
}

/// A list's texts, each ended by a 0 byte (`count` items of `item`).
fn list<C: Cpp + ?Sized>(c: &mut C, count: u32, item: impl Fn(&mut C, u32, Out)) -> Vec<u8> {
    let mut out = Vec::new();
    for i in 0..count {
        item(c, i, Out::of(&mut out));
        out.push(0);
    }
    out
}

/// The text `which` (its argument `arg`): `Some(None)` for a missing one, `None` for a text that is not here.
pub(crate) fn text<C: Cpp + ?Sized>(c: &mut C, _form: usize, which: u32, arg: u32) -> Option<Option<Vec<u8>>> {
    Some(Some(match which {
        // the loading screen's (splash.rs): the decrypted lines
        EE_SCREENS_TEXT_SS_TITLE => decrypted(TT_TITLE),
        EE_SCREENS_TEXT_SS_COPYRIGHT => decrypted(TT_COPYRIGHT),
        // the post-game summary's (post_game.rs): the stats server's host and port from the report file in the
        // install directory ("host\0port"; missing without the file or a session)
        EE_SCREENS_TEXT_PGS_SERVER => {
            let file = decrypted(TT_REPORT_FILE);
            let spec = c.spec_new();
            c.spec_set_volume_startup(spec);
            c.tt_spec_installed_directory(spec);
            c.spec_set_file(spec, Text::of(&file));
            if c.tt_spec_fopen_read(spec) != 0 || !c.gv_comms() {
                c.spec_delete(spec);
                return Some(None);
            }
            c.tt_spec_scan_server(spec);
            c.spec_close(spec);
            c.spec_delete(spec);
            let mut out = out_text(|o| c.tt_server_host(o));
            out.push(0);
            c.tt_server_port(Out::of(&mut out));
            out
        }
        // the civilization choice form's (civ_choice.rs)
        EE_SCREENS_TEXT_PDCS_CIV_NAME => string(c.tt_pdcs_civ_name_id(arg)),
        EE_SCREENS_TEXT_PDCS_CIV_FILE => out_text(|o| c.tt_pdcs_civ_file(arg, o)),
        // the custom campaign form's (custom_campaign.rs): the files of data\Campaigns with the campaign extension
        EE_SCREENS_TEXT_CAMPAIGN_FILES => {
            c.tt_cf_files_clear();
            c.tt_cf_directory_reset();
            c.tt_cf_path_new();
            c.tt_cf_directory_volume();
            c.tt_cf_directory_campaigns();
            c.tt_cf_path_add();
            c.tt_cf_list_campaigns();
            c.tt_cf_path_dereference();
            c.tt_cf_snapshot();
            let n = c.tt_lines_count();
            list(c, n, |c, i, o| c.tt_line(i, o))
        }
        // the settings' (settings_form.rs): the network adapters, the audio output devices
        EE_SCREENS_TEXT_ADAPTERS => {
            c.tt_list_clear();
            c.tt_adapters_protocol_clear();
            c.tt_adapters_protocol();
            if c.tt_adapters_has_protocol() {
                c.tt_adapters_enumerate();
            }
            let n = c.tt_list_count();
            list(c, n, |c, i, o| c.tt_list_item(i, o))
        }
        EE_SCREENS_TEXT_OUTPUT_DEVICES => {
            let n = c.tt_output_devices().min(16);
            list(c, n, |c, i, o| c.tt_output_device(i, o))
        }
        // the campaign scenario select's (scenario_screens.rs): the scenarios, the default user's completed ones
        EE_SCREENS_TEXT_CS_SCENARIOS => {
            c.tt_cs_scenarios();
            let n = c.tt_list_count();
            list(c, n, |c, i, o| c.tt_list_item(i, o))
        }
        EE_SCREENS_TEXT_CS_COMPLETED => {
            c.tt_list_clear();
            c.tt_cs_progress_new();
            if c.tt_cs_progress_load() == 0 {
                c.tt_cs_progress_completed();
            }
            c.tt_cs_progress_delete();
            let n = c.tt_list_count();
            list(c, n, |c, i, o| c.tt_list_item(i, o))
        }
        EE_SCREENS_TEXT_CS_PROGRESS => {
            c.tt_cs_progress_format();
            c.tt_cs_progress_print(arg >> 16, arg & 0xffff);
            out_text(|o| c.tt_text_out(o))
        }
        // the lobby's (lobby_form.rs)
        EE_SCREENS_TEXT_LE_GENERATOR_DISPLAY => out_text(|o| c.tt_generator_display(arg, o)),
        EE_SCREENS_TEXT_LE_GENERATOR_NAME => out_text(|o| c.tt_generator_name(arg, o)),
        EE_SCREENS_TEXT_LE_ENGINE => {
            // an engine choice: the strings Classic and Random, an AI's manifest name
            let ai = if arg == TT_ENGINE_CLASSIC { 0 } else { c.tt_engine_ai(arg) };
            if ai != 0 {
                out_text(|o| c.tt_engine_ai_name(ai, o))
            } else {
                string(TT_ENGINE_CLASSIC_STRING + (arg == TT_ENGINE_RANDOM) as u32)
            }
        }
        _ => return None,
    }))
}

/// The text `text` into `which` (its argument `arg`): its answer, `None` for a text that is not here.
pub(crate) fn put<C: Cpp + ?Sized>(c: &mut C, form: usize, which: u32, arg: u32, text: &[u8]) -> Option<u32> {
    let t = Text::of(text);
    Some(match which {
        EE_SCREENS_TEXT_PLAYER => {
            c.tp_player(t);
            0
        }
        EE_SCREENS_TEXT_RASTERIZER_SETTING => {
            c.tp_rasterizer_setting(t);
            0
        }
        EE_SCREENS_TEXT_AUDIO_DEVICE => {
            c.tp_audio_device(t);
            0
        }
        // (its number: the checksum the built-in campaign must have)
        EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN => c.tp_campaign_built_in(t),
        EE_SCREENS_TEXT_MESSAGE_NONE_FOUND => {
            c.tp_message_none_found(t);
            0
        }
        EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN => {
            c.tp_message_close_campaign(t);
            0
        }
        EE_SCREENS_TEXT_CS_SCENARIO_NAME => {
            c.tp_cs_scenario_name(t);
            0
        }
        EE_SCREENS_TEXT_QUESTION => {
            c.tp_question(form, t);
            0
        }
        EE_SCREENS_TEXT_MESSAGE => {
            c.tp_message(t, arg != 0);
            0
        }
        EE_SCREENS_TEXT_MG_CHAT_LOG => {
            c.tp_mg_chat_log(t);
            0
        }
        EE_SCREENS_TEXT_CH_CHAT => {
            c.tp_ch_chat(t);
            0
        }
        EE_SCREENS_TEXT_CL_APPEND => {
            c.tp_cl_append(t);
            0
        }
        EE_SCREENS_TEXT_CAMPAIGN_NAME => {
            c.tp_campaign_name(t);
            0
        }
        EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION => {
            c.tp_campaign_description(t);
            0
        }
        EE_SCREENS_TEXT_CAMPAIGN_FIRST => {
            c.tp_campaign_first(t);
            0
        }
        // the load and save screens' (save_load.rs)
        EE_SCREENS_TEXT_LOAD_SAVED_NAME => {
            c.tp_load_saved_name(Text::of(parts(text)[0]));
            0
        }
        EE_SCREENS_TEXT_LOAD_SCENARIO_NAME => {
            c.tp_load_scenario_name(Text::of(parts(text)[0]));
            0
        }
        EE_SCREENS_TEXT_QUESTION_DELETE => {
            c.tp_question_delete(form, arg, t);
            0
        }
        EE_SCREENS_TEXT_SAVE_MESSAGE => {
            c.tp_save_message(t);
            0
        }
        EE_SCREENS_TEXT_SAVE_ACTION => {
            let p = parts(text);
            c.tp_save_action(Text::of(p[0]), Text::of(p[1]));
            0
        }
        EE_SCREENS_TEXT_SAVE_QUESTION => {
            // yes: a scenario's save posted (arg), else a game's sent
            c.tp_save_question((arg != 0) as u32, t);
            0
        }
        // the lobby's (lobby_form.rs)
        EE_SCREENS_TEXT_LE_MAP_TYPE => {
            c.tp_le_map_type(t);
            0
        }
        EE_SCREENS_TEXT_LE_SEND_MAP_TYPE => {
            c.tp_le_send_map_type(arg, t);
            0
        }
        EE_SCREENS_TEXT_LE_CHAT => {
            c.tp_le_chat(t);
            0
        }
        EE_SCREENS_TEXT_LE_BOOT_QUESTION => {
            c.tp_le_boot_question(t);
            0
        }
        // the in-game options menu's (main_game_options.rs): on the form's manager (the game's without a form)
        EE_SCREENS_TEXT_GO_QUESTION => {
            let manager = if form != 0 { c.form_manager(form) } else { c.tp_ui_manager() };
            let yes = match arg {
                EE_SCREENS_GO_RESIGN_YES => 0,
                EE_SCREENS_GO_RESTART_YES => 1,
                EE_SCREENS_GO_RETURN_TO_EDITOR_YES => 2,
                _ => 3,
            };
            c.tp_go_question(manager, yes, t);
            0
        }
        // the post-game summary's (post_game.rs)
        EE_SCREENS_TEXT_PGS_BAR => {
            c.tp_pgs_bar(form, arg, t);
            0
        }
        EE_SCREENS_TEXT_PGS_CAMPAIGN_LOAD => {
            // the campaign select form made, its manager kept for Exit, the campaign loaded
            c.tp_csel_form();
            c.tp_pgs_campaign_get();
            c.tp_pgs_file_name(t);
            c.tp_pgs_error_clear();
            c.tp_pgs_campaign_load()
        }
        EE_SCREENS_TEXT_PGS_CAMPAIGN_NAME => {
            c.tp_pgs_campaign_name(t);
            0
        }
        // the civilization choice form's (civ_choice.rs)
        EE_SCREENS_TEXT_PDCS_LABEL => {
            c.tp_pdcs_label(t);
            0
        }
        // the campaign editor's (campaign_editor.rs): the main screen's and the editor's questions on the form's
        // manager, the others on the game's
        EE_SCREENS_TEXT_CE_QUESTION => {
            let manager = if arg == EE_SCREENS_CE_ASK_MAIN || arg == EE_SCREENS_CE_ASK_EDITOR {
                c.form_manager(form)
            } else {
                c.tp_ui_manager()
            };
            c.tp_ce_question(manager, form, arg % 5, t);
            0
        }
        EE_SCREENS_TEXT_CE_APPEND => {
            // the texts, each ended by a 0 byte (a last one without its 0 is not appended)
            c.tp_ce_lines_clear();
            let mut start = 0;
            for (i, b) in text.iter().enumerate() {
                if *b == 0 {
                    c.tp_ce_lines_add(Text::of(&text[start..i]));
                    start = i + 1;
                }
            }
            c.tp_ce_lines_append(form, arg);
            0
        }
        EE_SCREENS_TEXT_CE_BG_EQUALS => c.tp_ce_bg_equals(form, arg, t) as u32,
        EE_SCREENS_TEXT_CE_TRANSLATE => {
            c.tp_ce_translate(t);
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_loading_screens_lines_decrypt() {
        let mut f = Fake::default();
        let title = text(&mut f, 0, EE_SCREENS_TEXT_SS_TITLE, 0).unwrap().unwrap();
        assert!(!title.is_empty() && !title.contains(&0));
        assert!(f.log.is_empty());
        assert!(!decrypted(TT_REPORT_FILE).is_empty());
    }

    #[test]
    fn the_stats_server_is_missing_without_its_file_or_a_session() {
        let mut f = Fake::default();
        f.answer("spec_new", &[9]).answer("tt_spec_fopen_read", &[1]);
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_PGS_SERVER, 0), Some(None));
        assert!(f.calls().ends_with("tt_spec_fopen_read(9)\nspec_delete(9)"));
        let mut f = Fake::default();
        f.answer("spec_new", &[9]).answer("gv_comms", &[1]);
        f.text("tt_server_host", b"h").text("tt_server_port", b"80");
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_PGS_SERVER, 0), Some(Some(b"h\x0080".to_vec())));
        assert!(f.calls().contains("tt_spec_scan_server(9)\nspec_close(9)\nspec_delete(9)"));
    }

    #[test]
    fn the_lists_end_each_item_with_a_zero() {
        let mut f = Fake::default();
        f.answer("tt_adapters_has_protocol", &[1]).answer("tt_list_count", &[2]);
        f.text("tt_list_item", b"a").text("tt_list_item", b"bc");
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_ADAPTERS, 0), Some(Some(b"a\0bc\0".to_vec())));
        assert!(f.calls().contains("tt_adapters_protocol()\ntt_adapters_has_protocol()\ntt_adapters_enumerate()"));
        let mut f = Fake::default();
        f.answer("tt_output_devices", &[20]);
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_OUTPUT_DEVICES, 0).unwrap().unwrap().len(), 16);
        let mut f = Fake::default();
        f.answer("tt_lines_count", &[1]).text("tt_line", b"x.eec");
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_CAMPAIGN_FILES, 0), Some(Some(b"x.eec\0".to_vec())));
        let mut f = Fake::default();
        f.answer("tt_cs_progress_load", &[1]);
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_CS_COMPLETED, 0), Some(Some(vec![])));
        assert!(!f.calls().contains("tt_cs_progress_completed"));
        let mut f = Fake::default();
        text(&mut f, 0, EE_SCREENS_TEXT_CS_SCENARIOS, 0);
        text(&mut f, 0, EE_SCREENS_TEXT_CS_PROGRESS, 0x0002_0005);
        assert!(f.calls().contains("tt_cs_progress_print(2, 5)\ntt_text_out(out)"));
    }

    #[test]
    fn an_engine_is_its_ai_name_or_the_classic_and_random_strings() {
        let mut f = Fake::default();
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_LE_ENGINE, 0), Some(Some(string(TT_ENGINE_CLASSIC_STRING))));
        assert!(f.log.is_empty());
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_LE_ENGINE, 1), Some(Some(string(TT_ENGINE_CLASSIC_STRING + 1))));
        f.answer("tt_engine_ai", &[0x20]).text("tt_engine_ai_name", b"Bob");
        assert_eq!(text(&mut f, 0, EE_SCREENS_TEXT_LE_ENGINE, 3), Some(Some(b"Bob".to_vec())));
        for which in [
            EE_SCREENS_TEXT_PDCS_CIV_NAME,
            EE_SCREENS_TEXT_PDCS_CIV_FILE,
            EE_SCREENS_TEXT_LE_GENERATOR_DISPLAY,
            EE_SCREENS_TEXT_LE_GENERATOR_NAME,
        ] {
            assert!(text(&mut f, 0, which, 0).is_some());
        }
        assert_eq!(text(&mut f, 0, 0xffff_fff0, 0), None);
    }

    #[test]
    fn the_questions_pick_their_manager_and_answer() {
        let mut f = Fake::default();
        f.answer("form_manager", &[0x30]).answer("tp_ui_manager", &[0x40]);
        put(&mut f, 0x10, EE_SCREENS_TEXT_GO_QUESTION, EE_SCREENS_GO_RESTART_YES, b"q");
        put(&mut f, 0, EE_SCREENS_TEXT_GO_QUESTION, 99, b"q");
        put(&mut f, 0x10, EE_SCREENS_TEXT_CE_QUESTION, 7, b"q");
        assert_eq!(
            f.calls(),
            "form_manager(16)\ntp_go_question(48, 1, \"q\")\ntp_ui_manager()\ntp_go_question(64, 3, \"q\")\n\
             tp_ui_manager()\ntp_ce_question(0, 16, 2, \"q\")"
        );
    }

    #[test]
    fn the_puts_split_their_parts_and_lines() {
        let mut f = Fake::default();
        put(&mut f, 0x10, EE_SCREENS_TEXT_SAVE_ACTION, 0, b"a\0b");
        put(&mut f, 0x10, EE_SCREENS_TEXT_CE_APPEND, 3, b"x\0yz\0w");
        put(&mut f, 0x10, EE_SCREENS_TEXT_SAVE_QUESTION, 5, b"s");
        assert_eq!(
            f.calls(),
            "tp_save_action(\"a\", \"b\")\ntp_ce_lines_clear()\ntp_ce_lines_add(\"x\")\ntp_ce_lines_add(\"yz\")\n\
             tp_ce_lines_append(16, 3)\ntp_save_question(1, \"s\")"
        );
        let mut f = Fake::default();
        f.answer("tp_pgs_campaign_load", &[1]);
        assert_eq!(put(&mut f, 0, EE_SCREENS_TEXT_PGS_CAMPAIGN_LOAD, 0, b"c"), Some(1));
        for which in [
            EE_SCREENS_TEXT_PLAYER,
            EE_SCREENS_TEXT_RASTERIZER_SETTING,
            EE_SCREENS_TEXT_AUDIO_DEVICE,
            EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN,
            EE_SCREENS_TEXT_MESSAGE_NONE_FOUND,
            EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN,
            EE_SCREENS_TEXT_CS_SCENARIO_NAME,
            EE_SCREENS_TEXT_QUESTION,
            EE_SCREENS_TEXT_MESSAGE,
            EE_SCREENS_TEXT_MG_CHAT_LOG,
            EE_SCREENS_TEXT_CH_CHAT,
            EE_SCREENS_TEXT_CL_APPEND,
            EE_SCREENS_TEXT_CAMPAIGN_NAME,
            EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION,
            EE_SCREENS_TEXT_CAMPAIGN_FIRST,
            EE_SCREENS_TEXT_LOAD_SAVED_NAME,
            EE_SCREENS_TEXT_LOAD_SCENARIO_NAME,
            EE_SCREENS_TEXT_QUESTION_DELETE,
            EE_SCREENS_TEXT_SAVE_MESSAGE,
            EE_SCREENS_TEXT_LE_MAP_TYPE,
            EE_SCREENS_TEXT_LE_SEND_MAP_TYPE,
            EE_SCREENS_TEXT_LE_CHAT,
            EE_SCREENS_TEXT_LE_BOOT_QUESTION,
            EE_SCREENS_TEXT_PGS_BAR,
            EE_SCREENS_TEXT_PGS_CAMPAIGN_NAME,
            EE_SCREENS_TEXT_PDCS_LABEL,
            EE_SCREENS_TEXT_CE_BG_EQUALS,
            EE_SCREENS_TEXT_CE_TRANSLATE,
        ] {
            let mut f = Fake::default();
            assert!(put(&mut f, 0, which, 0, b"t").is_some(), "{which}");
            assert_eq!(f.log.len(), 1, "{which}");
        }
        assert_eq!(put(&mut Fake::default(), 0, 0xffff_fff0, 0, b""), None);
    }
}
