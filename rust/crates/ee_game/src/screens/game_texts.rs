//! The screens' texts (`ee_screenscpp_text`, `ee_screenscpp_put_text`, `UI Events/EEScreens.cpp`) in Rust (UI lane
//! round 7): each case that was more than one C++ statement is here, over the one-statement shims of [`Cpp`]
//! (`game_cpp.rs`), as `game_calls.rs` does for the calls. The dispatchers' adapter (`menus_ffi.rs` `Form`) asks
//! [`text`] and [`put`] before the C++ host; a text that is not here goes on to it.

use core::ffi::{c_long, c_ulong};

use crate::screens::chat::EE_SCREENS_TEXT_CH_CHEAT;
use crate::screens::civ_choice::{EE_SCREENS_TEXT_PDCS_LINE, EE_SCREENS_TEXT_PDCS_LOAD};
use crate::screens::debug_sound::EE_SCREENS_TEXT_DSU_LABEL;
use crate::screens::diplomacy::EE_SCREENS_TEXT_DS_BUTTON;
use crate::screens::game_calls::{new_local_server, string};
use crate::screens::game_cpp::{
    Cpp, EeScreensShimLongOut as OutL, EeScreensShimOut as Out, EeScreensShimPtrOut as OutPtr,
    EeScreensShimText as Text, EeScreensShimULongOut as OutUL,
};
use crate::screens::lobby_form::{EE_SCREENS_TEXT_LE_CHECK_LABEL, EE_SCREENS_TEXT_LE_OPEN_HEADER};
use crate::screens::main_game::EE_SCREENS_TEXT_MG_QUICK_LOAD;
use crate::screens::main_game_options::EE_SCREENS_TEXT_GO_BUTTON;
use crate::screens::menus::{
    EE_SCREENS_TEXT_CAMPAIGN_LOAD, EE_SCREENS_TEXT_LABEL_LINES, EE_SCREENS_TEXT_SERVER, EE_SCREENS_TEXT_STRING,
    EE_SCREENS_TEXT_STRING_CHECKED,
};
use crate::screens::post_game::EE_SCREENS_TEXT_PGS_SEND;
use crate::screens::save_load::{
    EE_SCREENS_TEXT_FILE_EXISTS, EE_SCREENS_TEXT_FILE_LIST, EE_SCREENS_TEXT_LOAD_POST, EE_SCREENS_TEXT_LOAD_SAVED_GAME,
    EE_SCREENS_TEXT_SCENARIO_INTRO,
};
use crate::screens::scenario_screens::{
    EE_SCREENS_TEXT_CS_ARCHIVE, EE_SCREENS_TEXT_CS_SCENARIO_HEADER, EE_SCREENS_TEXT_CS_START,
};
use crate::screens::settings_form::{
    EE_SCREENS_TEXT_DISPLAY_RASTERIZER, EE_SCREENS_TEXT_DISPLAY_SCREEN, EE_SCREENS_TEXT_RASTERIZER_NAME,
};

/// A put's texts joined by 0 bytes (`Parts`: at least three, the missing ones empty).
pub(crate) fn parts(text: &[u8]) -> Vec<&[u8]> {
    let mut p: Vec<&[u8]> = text.split(|b| *b == 0).collect();
    while p.len() < 3 {
        p.push(b"");
    }
    p
}

/// A shim's out text.
fn out_text(f: impl FnOnce(Out)) -> Vec<u8> {
    let mut t = Vec::new();
    f(Out::of(&mut t));
    t
}

/// The text `which` (its argument `arg`): `Some(None)` for a missing one, `None` for a text that is not here.
pub(crate) fn text<C: Cpp + ?Sized>(c: &mut C, form: usize, which: u32, arg: u32) -> Option<Option<Vec<u8>>> {
    Some(match which {
        EE_SCREENS_TEXT_STRING => Some(string(arg)),
        EE_SCREENS_TEXT_STRING_CHECKED => crate::screens::hud_direct::resource_string(arg as i64).map(|s| s.to_vec()),
        // the civilization choice form's (civ_choice.rs): its list's selected line
        EE_SCREENS_TEXT_PDCS_LINE => {
            let mut selection: c_ulong = 0;
            if c.pdcs_selection(OutUL::of(&mut selection)) != 0 {
                return Some(None);
            }
            Some(out_text(|o| c.pdcs_line(selection as u32, o)))
        }
        // the settings' (settings_form.rs): the display devices' and the rasterizers' names ("" past the lists)
        EE_SCREENS_TEXT_DISPLAY_RASTERIZER => Some(if arg < c.display_rasterizer_count() {
            out_text(|o| c.display_rasterizer_name(arg, o))
        } else {
            vec![]
        }),
        EE_SCREENS_TEXT_DISPLAY_SCREEN => {
            let (r, s) = (arg >> 16, arg & 0xffff);
            Some(if r < c.display_rasterizer_count() && s < c.display_screen_count(r) {
                out_text(|o| c.display_screen_name(r, s, o))
            } else {
                vec![]
            })
        }
        EE_SCREENS_TEXT_RASTERIZER_NAME => {
            Some(if arg < c.rasterizer_count() { out_text(|o| c.rasterizer_name(arg, o)) } else { vec![] })
        }
        // the lobby's (lobby_form.rs): a check box's label (missing without one)
        EE_SCREENS_TEXT_LE_CHECK_LABEL => {
            let check = c.form_control(form, arg);
            if check == 0 || c.check_box_label(check) == 0 {
                return Some(None);
            }
            let label = c.check_box_label(check);
            Some(out_text(|o| c.label_text(label, o)))
        }
        _ => return None,
    })
}

/// The texts `text` into `which` (its argument `arg`): its answer, `None` for a text that is not here.
pub(crate) fn put<C: Cpp + ?Sized>(c: &mut C, form: usize, which: u32, arg: u32, text: &[u8]) -> Option<u32> {
    let t = Text::of(text);
    Some(match which {
        EE_SCREENS_TEXT_CS_ARCHIVE => {
            // the campaign's file on the data path; a valid one's archive added to it
            let p = parts(text);
            let archive = c.archive_new();
            let mut file = 0usize;
            let found = c.data_find_file(Text::of(p[0]), OutPtr::of(&mut file));
            let added = found == 0 && arg != 0;
            if added {
                c.archive_set(archive, file);
                c.data_add_archive(archive);
                c.set_data_paths();
            }
            if file != 0 {
                c.spec_delete(file);
            }
            c.archive_delete(archive);
            added as u32
        }
        EE_SCREENS_TEXT_CS_SCENARIO_HEADER | EE_SCREENS_TEXT_CS_START => {
            // a campaign scenario's header: for Start, its media strings (the campaign's prefix on the sound and map)
            // and file into the intro form's manager (2; 1: the header does not load, 0: no file)
            let header = c.header_new();
            let prefix = out_text(|o| c.scenario_prefix(o));
            let extension = out_text(|o| c.scenario_extension(o));
            let name = [&b"Scenarios\\"[..], &prefix, cut(text), &extension].concat();
            let mut file = 0usize;
            let r = if c.data_find_file(Text::of(&name), OutPtr::of(&mut file)) != 0 {
                if file != 0 {
                    c.spec_delete(file);
                }
                0
            } else if c.header_load(header, file) != 0 {
                c.spec_delete(file);
                (which == EE_SCREENS_TEXT_CS_START) as u32
            } else if which == EE_SCREENS_TEXT_CS_SCENARIO_HEADER {
                c.spec_delete(file);
                1
            } else {
                c.campaign_update_checksum();
                c.intro_media_from_header(header);
                c.intro_prefix_media();
                c.intro_set_file(file);
                c.intro_set_flag(2, true);
                2
            };
            c.header_delete(header);
            r
        }
        EE_SCREENS_TEXT_CH_CHEAT => {
            let (mut code, mut parameter): (c_long, c_long) = (0, 0);
            c.parse_cheat(t, OutL::of(&mut code), OutL::of(&mut parameter));
            if code != 0 {
                c.send_cheat(code as i64, parameter as i64);
            }
            0
        }
        EE_SCREENS_TEXT_DS_BUTTON | EE_SCREENS_TEXT_GO_BUTTON => {
            // a button's text in its own font
            let font = c.button_font(form, arg);
            c.button_set_text(form, arg, t, font);
            0
        }
        EE_SCREENS_TEXT_MG_QUICK_LOAD => {
            // the last quick save's (the auto save's when arg) load; the text shown when its file is not found
            let name = if arg != 0 { out_text(|o| c.last_auto_save(o)) } else { out_text(|o| c.last_quick_save(o)) };
            let mut file = c.spec_new();
            if c.open_save_game_file(Text::of(&name), Text::of(b"Saved Games\\"), OutPtr::of(&mut file)) == 0 {
                c.spec_set_file(file, Text::of(&name));
                c.post_load_saved_game(file);
            } else {
                c.display_message(t, true);
            }
            if file != 0 {
                c.spec_delete(file);
            }
            0
        }
        EE_SCREENS_TEXT_LABEL_LINES => {
            let label = c.manager_form_control(arg >> 16, arg & 0xffff);
            c.label_delete_array(label);
            c.label_append_text(label, t);
            0
        }
        EE_SCREENS_TEXT_CAMPAIGN_LOAD => {
            let extension = out_text(|o| c.campaign_extension(o));
            let file = [&b"Campaigns\\"[..], cut(text), &extension].concat();
            c.campaign_error_reset();
            c.campaign_load(Text::of(&file)) as u32
        }
        EE_SCREENS_TEXT_SERVER => {
            // the random map's local server (the old communications deleted) and the human's connection request
            new_local_server(c);
            c.send_connection_request(t);
            0
        }
        EE_SCREENS_TEXT_FILE_LIST => {
            // a file list from a directory below the startup one with an extension, the name selected when listed
            let p = parts(text);
            let list = c.manager_form_control(arg >> 16, arg & 0xffff);
            if list == 0 {
                return Some(0);
            }
            let directory = c.spec_new();
            let path = c.path_new();
            c.file_list_add_extension(list, Text::of(p[0]));
            c.spec_set_volume_startup(directory);
            c.spec_set_directory_startup(directory, Text::of(p[1]));
            c.path_add_directory(path, directory);
            c.file_list_set_paths(list, path);
            c.file_list_create(list);
            let mut index: c_ulong = 0;
            if c.file_list_find(list, Text::of(p[2]), OutUL::of(&mut index)) {
                c.list_select(list, index as u32);
                c.list_scroll_to(list, index as u32);
            }
            c.path_dereference(path);
            c.spec_delete(directory);
            0
        }
        EE_SCREENS_TEXT_LOAD_POST => {
            let p = parts(text);
            let mut file = 0usize;
            if c.open_save_game_file(Text::of(p[0]), Text::of(p[1]), OutPtr::of(&mut file)) != 0 {
                return Some(0);
            }
            c.post_load_saved_game(file);
            if file != 0 {
                c.spec_delete(file);
            }
            1
        }
        EE_SCREENS_TEXT_FILE_EXISTS => {
            let p = parts(text);
            let spec = c.spec_new();
            c.spec_set_file(spec, Text::of(p[0]));
            c.spec_set_directory_startup(spec, Text::of(p[1]));
            let r = if c.spec_open_read(spec) != 0 {
                0
            } else {
                c.spec_close(spec);
                1
            };
            c.spec_delete(spec);
            r
        }
        EE_SCREENS_TEXT_LOAD_SAVED_GAME => load_saved_game(c, &parts(text)),
        EE_SCREENS_TEXT_SCENARIO_INTRO => {
            // a scenario's header into the scenario intro's form manager (2; the caller opens the form), the scenario
            // the last loaded one; 1: the file is not a scenario, 0: not found
            let p = parts(text);
            let header = c.header_new();
            let mut file = 0usize;
            let r = if c.open_save_game_file(Text::of(p[0]), Text::of(p[1]), OutPtr::of(&mut file)) != 0 {
                0
            } else {
                c.header_set_major(header);
                c.header_set_minor(header);
                if c.header_load(header, file) != 0 {
                    if file != 0 {
                        c.spec_delete(file);
                    }
                    1
                } else {
                    c.intro_media_from_header(header);
                    c.intro_set_file(file);
                    c.intro_set_flag(2, false);
                    c.load_scenario_name_from_spec(file);
                    2
                }
            };
            c.header_delete(header);
            r
        }
        EE_SCREENS_TEXT_LE_OPEN_HEADER => {
            // the load screen's: the header kept for the transfers, the options and the slots
            let p = parts(text);
            c.lobby_header_delete();
            c.lobby_header_new();
            c.lobby_file_clear();
            c.lobby_load(Text::of(p[0]), Text::of(p[1]), arg)
        }
        EE_SCREENS_TEXT_DSU_LABEL => {
            let label = c.debug_sound_label(arg);
            if label != 0 {
                c.label_set_text(label, t);
            }
            0
        }
        EE_SCREENS_TEXT_PGS_SEND => {
            // "host\0payload" to the stats server
            let host = text.iter().position(|b| *b == 0).unwrap_or(text.len());
            let payload = if host < text.len() { &text[host + 1..] } else { &text[text.len()..] };
            c.pgs_send(Text::of(&text[..host]), arg, Text::of(payload));
            0
        }
        EE_SCREENS_TEXT_PDCS_LOAD => {
            c.pdcs_name_reset();
            c.pdcs_upgrades_clear();
            c.pdcs_load(t)
        }
        _ => return None,
    })
}

/// The text up to its first 0 byte (the C++ cases' `UWideString(const char*)` of the whole text).
fn cut(text: &[u8]) -> &[u8] {
    &text[..text.iter().position(|b| *b == 0).unwrap_or(text.len())]
}

/// The single player load: the saved game validated, the lobby options and a new local server set up for it, its
/// loading started; an invalid file shows the reason (the file is freed on every path).
fn load_saved_game<C: Cpp + ?Sized>(c: &mut C, p: &[&[u8]]) -> u32 {
    let mut file = 0usize;
    if c.open_save_game_file(Text::of(p[0]), Text::of(p[1]), OutPtr::of(&mut file)) == 0 && c.spec_open_read(file) == 0
    {
        c.shim_message_reset();
        if !c.validate_save_game(file) {
            c.display_shim_message();
            c.spec_delete(file);
            return 0;
        }
        c.lobby_options_write_lock();
        c.lobby_options_set_defaults();
        c.lobby_options_set_load_type(2);
        c.lobby_options_map_type_from_spec(file);
        c.lobby_options_write_unlock();
        c.load_saved_game_name_from_spec(file);
        c.spec_delete(file);
        c.game_state_single();
        new_local_server(c);
        c.set_game_state_load_type(2);
        c.server_stop_ping();
        c.server_set_load_state(true);
        c.send_scenario_start_loading();
        return 1;
    }
    if file != 0 {
        c.spec_delete(file);
    }
    0
}

#[cfg(test)]
#[path = "game_texts_tests.rs"]
mod tests;
