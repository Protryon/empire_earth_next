//! The campaign editor model's host in Rust over one-statement C++ shims (UI lane round 8; the shims are
//! `ee_screenscpp_cem_*` in `UI Events/EEScreens.cpp`, on the edited `EECampaign`, the kept string and lines and
//! Reset's files): the original's `ee_screenscpp_ce_campaign` (`EE_SCREENS_CEM_*`, each op the original model's
//! calls) and `ee_screenscpp_ce_campaign_text`.

use core::ffi::c_ulong;

use crate::screens::campaign_editor::*;
use crate::screens::campaign_model::*;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut, EeScreensShimText, EeScreensShimULongOut};

/// One model op (`text` the op's text); 0 for an op the host does not know.
pub(crate) fn call(c: &mut impl Cpp, op: u32, a: u32, text: &[u8]) -> u32 {
    let t = EeScreensShimText::of(text);
    match op {
        EE_SCREENS_CEM_STRING => {
            c.cem_kept_clear();
            return c.cem_string(a);
        }
        EE_SCREENS_CEM_SET_FIELD => match a {
            EE_SCREENS_CE_FIELD_NAME => c.cem_set_name(t),
            EE_SCREENS_CE_FIELD_AUTHOR => c.cem_set_author(t),
            EE_SCREENS_CE_FIELD_EMAIL => c.cem_set_email(t),
            EE_SCREENS_CE_FIELD_BACKGROUND => c.cem_set_background(t),
            EE_SCREENS_CE_FIELD_CIVILIZATION => c.cem_set_civilization(t),
            EE_SCREENS_CE_FIELD_DESCRIPTION => c.cem_set_description(t),
            _ => c.cem_set_password(t),
        },
        EE_SCREENS_CEM_SET_SCENARIOS => {
            // the lines each end with a 0 (a last line without one is dropped, as the original's)
            c.cem_scenarios_clear();
            let mut start = 0;
            for (i, &b) in text.iter().enumerate() {
                if b == 0 {
                    c.cem_scenarios_add(EeScreensShimText::of(&text[start..i]));
                    start = i + 1;
                }
            }
            c.cem_set_scenarios();
        }
        EE_SCREENS_CEM_LOAD => {
            let mut file = b"Campaigns\\".to_vec();
            file.extend_from_slice(text);
            c.cem_kept_clear();
            return c.cem_load(EeScreensShimText::of(&file));
        }
        EE_SCREENS_CEM_SAVE => {
            c.cem_kept_clear();
            return c.cem_save();
        }
        EE_SCREENS_CEM_LIST => {
            c.cem_lines_clear();
            c.cem_kept_clear();
            return c.cem_list();
        }
        EE_SCREENS_CEM_DELETE => {
            c.cem_delete_reset();
            c.cem_delete_volume();
            c.cem_delete_directory();
            c.cem_delete_file(t);
            return c.cem_delete() as u32;
        }
        EE_SCREENS_CEM_RESET_BEGIN => {
            c.cem_files_clear();
            c.cem_spec_clear();
            c.cem_file_clear();
            c.cem_path_new(0);
            c.cem_path_new(1);
            c.cem_set_directory();
        }
        EE_SCREENS_CEM_LIST_FILES => {
            c.cem_directory_volume();
            if a != 0 {
                c.cem_directory_scenarios();
            } else {
                c.cem_directory_campaigns();
            }
            c.cem_path_add(if a != 0 { 1 } else { 0 });
            if a != 0 {
                c.cem_list_scenario_files();
            } else {
                c.cem_list_campaign_files();
            }
        }
        EE_SCREENS_CEM_BUILT_IN => return c.cem_built_in(t),
        EE_SCREENS_CEM_CHECK => return check(c, a, t) as u32,
        EE_SCREENS_CEM_RESET_END => {
            c.cem_path_dereference(0);
            c.cem_path_dereference(1);
            c.cem_spec_delete();
            c.cem_file_delete();
        }
        _ => {}
    }
    0
}

/// `EE_SCREENS_CEM_CHECK`: a campaign scenario's archived copy summed against the file's checksum; a difference
/// shown (unless `quiet`); whether it was.
fn check(c: &mut impl Cpp, quiet: u32, t: EeScreensShimText) -> bool {
    let mut warned = false;
    if c.cem_find_campaign() != 0 {
        return false;
    }
    let archive = c.cem_archive_new();
    if c.cem_archive_open(archive) == 0 && c.cem_archive_find(archive, t) == 0 {
        if c.cem_file_open() == 0 {
            let mut length: c_ulong = 0;
            if c.cem_file_length(EeScreensShimULongOut::of(&mut length)) == 0 {
                let mut sum: c_ulong = 0;
                for _ in 0..length {
                    c.cem_file_read();
                    sum = sum.wrapping_add(c.cem_file_byte() as c_ulong);
                }
                if quiet == 0 && sum as u64 != c.cem_file_checksum(t) {
                    c.cem_differs_format();
                    c.cem_differs_print(t);
                    c.cem_differs_show();
                    warned = true;
                }
            }
        }
        c.cem_file_close();
        c.cem_file_delete();
        c.cem_file_clear();
    }
    c.cem_archive_close(archive);
    c.cem_archive_delete(archive);
    warned
}

/// One model text (`EE_SCREENS_CEM_TEXT_*`): a field, the lists (each line ended with a 0), the kept string, an
/// extension.
pub(crate) fn text(c: &mut impl Cpp, which: u32, arg: u32) -> Vec<u8> {
    let mut out = Vec::new();
    let o = EeScreensShimOut::of(&mut out);
    match which {
        EE_SCREENS_CEM_TEXT_FIELD => match arg {
            EE_SCREENS_CE_FIELD_NAME => c.cem_name(o),
            EE_SCREENS_CE_FIELD_AUTHOR => c.cem_author(o),
            EE_SCREENS_CE_FIELD_EMAIL => c.cem_email(o),
            EE_SCREENS_CE_FIELD_BACKGROUND => c.cem_background(o),
            EE_SCREENS_CE_FIELD_CIVILIZATION => c.cem_civilization(o),
            EE_SCREENS_CE_FIELD_DESCRIPTION => c.cem_description(o),
            _ => c.cem_password(o),
        },
        EE_SCREENS_CEM_TEXT_SCENARIOS => {
            for i in 0..c.cem_scenario_count() {
                c.cem_scenario(i, o);
                // SAFETY: `out` lives; the shim's append is over
                unsafe { (*(o.0 as *mut Vec<u8>)).push(0) };
            }
        }
        EE_SCREENS_CEM_TEXT_KEPT => c.cem_kept(o),
        EE_SCREENS_CEM_TEXT_LINES => {
            for i in 0..c.cem_line_count() {
                c.cem_line(i, o);
                unsafe { (*(o.0 as *mut Vec<u8>)).push(0) };
            }
        }
        EE_SCREENS_CEM_TEXT_FILES => {
            c.cem_files_snapshot();
            for i in 0..c.cem_file_count() {
                c.cem_file_name(i, o);
                unsafe { (*(o.0 as *mut Vec<u8>)).push(0) };
            }
        }
        EE_SCREENS_CEM_TEXT_EXTENSION => {
            if arg != 0 {
                c.cem_scenario_extension(o)
            } else {
                c.cem_campaign_extension(o)
            }
        }
        _ => {}
    }
    out
}

#[cfg(test)]
#[path = "campaign_model_calls_tests.rs"]
mod tests;
