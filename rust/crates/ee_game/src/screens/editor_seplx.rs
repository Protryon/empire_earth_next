//! The player tab's host ops that were C++ bodies in `Scenario Editor/EESERBUnitTypes.cpp` (the scenario editor
//! lane's round 4): the civilization combo's fill, pick and apply (`SeplCivFill`, `SeplCivSelect`, `SeplCivApply`),
//! the epochs' combo fill, the diplomacy buttons' bitmaps (`SeplDiplomacyLoad`, `SeplDiplomacyRelease`) and their
//! state shown (`EESEDiplomacyShow`); composed here of single C++ statements (`EE_SCREENS_SEPX_*` from 200,
//! `ee_screenscpp_sepx`) and the toolkit's Rust (`EE_SCREENS_SEPX_R_*`, answered by [`direct`]) in the C++'s order.

// (the casts of the C `unsigned long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use core::ffi::c_ulong;

use crate::screens::editor_players::*;
use crate::screens::editor_sepl::Sepx;
use crate::screens::hud_direct::{differs, resource_string};

// ---- the statements (p0 the op's control; a text in: p1 its bytes, i2 their count; a text out: p1 a buffer of i3
// bytes, the answer its whole length)
/// `gCivilizationManager.GetNumCivilizations()`, civilization i0's `mNameID`, its `mFileName` (out).
pub const EE_SCREENS_SEPX_CIV_COUNT: u32 = 200;
pub const EE_SCREENS_SEPX_CIV_NAME_ID: u32 = 201;
pub const EE_SCREENS_SEPX_CIV_FILE: u32 = 202;
/// The custom civilization files (`gCivilizationDataPath->ListFiles("*" + gSECivilizationExtension)` into the host's
/// set): cleared, listed, counted, file i0's name without its extension (`GetIndexedSubstring(0, length - 5)`, out).
pub const EE_SCREENS_SEPX_FILES_CLEAR: u32 = 203;
pub const EE_SCREENS_SEPX_FILES_LIST: u32 = 204;
pub const EE_SCREENS_SEPX_FILES_COUNT: u32 = 205;
pub const EE_SCREENS_SEPX_FILE_STEM: u32 = 206;
/// Player i0's `mCivilizationFile` (out); its `mCivilization.mPurchasedUpgrades.clear()`, `mAvailablePoints =
/// mMaxPoints`; `mCivilizationFile = the text`; `EEApplyCivilizationFile(mCivilizationFile, it)`.
pub const EE_SCREENS_SEPX_PLAYER_CIV: u32 = 207;
pub const EE_SCREENS_SEPX_PLAYER_CIV_CLEAR: u32 = 208;
pub const EE_SCREENS_SEPX_PLAYER_CIV_POINTS: u32 = 209;
pub const EE_SCREENS_SEPX_PLAYER_SET_CIV: u32 = 210;
pub const EE_SCREENS_SEPX_PLAYER_APPLY_CIV: u32 = 211;
/// The host's lines: cleared, the text pushed, p0's `AppendStringArray(them)`.
pub const EE_SCREENS_SEPX_LINES_CLEAR: u32 = 212;
pub const EE_SCREENS_SEPX_LINES_PUSH: u32 = 213;
pub const EE_SCREENS_SEPX_COMBO_APPEND_LINES: u32 = 214;
/// The diplomacy bitmaps `sSeplDiplomacy[i0][i1]`: the editor p0's button texture's `GetWidth()`; `new GEBitmap`;
/// `SetTexture(the texture, the editor's screen)`; `SetTextureCoordinates((f0, f2), (f1, f2), (f0, f3), (f1, f3))`;
/// `SetXYScale(f0, f0)`; `Dereference()`.
pub const EE_SCREENS_SEPX_DIPLO_WIDTH: u32 = 215;
pub const EE_SCREENS_SEPX_DIPLO_NEW: u32 = 216;
pub const EE_SCREENS_SEPX_DIPLO_TEXTURE: u32 = 217;
pub const EE_SCREENS_SEPX_DIPLO_COORDS: u32 = 218;
pub const EE_SCREENS_SEPX_DIPLO_SCALE: u32 = 219;
pub const EE_SCREENS_SEPX_DIPLO_RELEASE: u32 = 220;
/// The button p0's `SetPressedBitmap(sSeplDiplomacy[2][i0])`, `SetFocusedBitmap([1][i0])`, `SetNormalBitmap([0][i0])`,
/// `GetForm()->UpdateHelpString()`.
pub const EE_SCREENS_SEPX_DIPLO_PRESSED: u32 = 221;
pub const EE_SCREENS_SEPX_DIPLO_FOCUSED: u32 = 222;
pub const EE_SCREENS_SEPX_DIPLO_NORMAL: u32 = 223;
pub const EE_SCREENS_SEPX_DIPLO_UPDATE_HELP: u32 = 224;
/// The maker's text and lines (the C++'s `SeplText`, `SeplComboLines`, `SeplMaxPlayerLines`, `SeplEpochLines`, built
/// here before a making statement, [`prestep`]): the text the next statements take (`sSepxText`); the host's lines
/// swapped with the pending ones (`sSeplLines`), the pending ones cleared, a pending line pushed; the unit sets'
/// `gUnitSetManager.GetSize()`, set i0 shown in the editor (`GetSet(i0) != NULL && ShowInEditor()`), its `GetName()`
/// (out).
pub const EE_SCREENS_SEPX_SET_TEXT: u32 = 225;
pub const EE_SCREENS_SEPX_LINES_TAKE_PENDING: u32 = 226;
pub const EE_SCREENS_SEPX_PENDING_CLEAR: u32 = 227;
pub const EE_SCREENS_SEPX_PENDING_PUSH: u32 = 228;
pub const EE_SCREENS_SEPX_SETS_COUNT: u32 = 229;
pub const EE_SCREENS_SEPX_SET_SHOWN: u32 = 230;
pub const EE_SCREENS_SEPX_SET_NAME: u32 = 231;
/// The brush group's: `gSeplScreenFont` (its handle in i0, i1), `gSeplScreenFont = p0`, the radio p0's
/// `EEUIShell::Op(EE_SCREENS_SKOP_BRUSH_SIZE)`.
pub const EE_SCREENS_SEPX_SCREEN_FONT: u32 = 232;
pub const EE_SCREENS_SEPX_SET_SCREEN_FONT: u32 = 233;
pub const EE_SCREENS_SEPX_BRUSH_SIZE: u32 = 234;

/// The toolkit's (answered by [`direct`]; p0 the combo box or button): `DeleteAllEntries()`, `AppendLine(the text)`,
/// `SetCurrentSelection(i0)`, `GetCurrentSelection`, the list's line count, its line i0 (out), the text label's line 0
/// (out), its `SetText(the text)`; the control's `SetStringTableID(i0)`.
pub const EE_SCREENS_SEPX_R_FIRST: u32 = 300;
pub const EE_SCREENS_SEPX_R_COMBO_CLEAR: u32 = 300;
pub const EE_SCREENS_SEPX_R_COMBO_APPEND: u32 = 301;
pub const EE_SCREENS_SEPX_R_COMBO_SELECT: u32 = 302;
pub const EE_SCREENS_SEPX_R_COMBO_SELECTION: u32 = 303;
pub const EE_SCREENS_SEPX_R_COMBO_LINES: u32 = 304;
pub const EE_SCREENS_SEPX_R_COMBO_LINE: u32 = 305;
pub const EE_SCREENS_SEPX_R_COMBO_LABEL: u32 = 306;
pub const EE_SCREENS_SEPX_R_COMBO_SET_LABEL: u32 = 307;
pub const EE_SCREENS_SEPX_R_HELP_ID: u32 = 308;

/// The strings (ee/EEStringIDs.h; EESERBUnitTypes.cpp asserts them).
pub const EE_SCREENS_SEPX_IDS_NONE: i64 = 13001;
pub const EE_SCREENS_SEPX_IDS_UNKNOWN_CIVILIZATION: i64 = 30464;
pub const EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ALLIES: i32 = 8600;
pub const EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ENEMIES: i32 = 8601;
pub const EE_SCREENS_SEPX_IDS_SET: i64 = 30481;
pub const EE_SCREENS_SEPX_IDS_UNIT_FAMILY_FIRST: i64 = 31951;

fn st(x: &mut Sepx, op: u32, p0: usize, a: [i32; 4]) -> i32 {
    let mut a = a;
    x(op, [p0, 0, 0], &mut a, &mut [0.0; 4])
}

fn st_f(x: &mut Sepx, op: u32, p0: usize, a: [i32; 4], f: [f32; 4]) -> i32 {
    let (mut a, mut f) = (a, f);
    x(op, [p0, 0, 0], &mut a, &mut f)
}

/// A statement taking a text (p1 its bytes, i2 their count).
fn st_text(x: &mut Sepx, op: u32, p0: usize, a0: i32, text: &[u8]) -> i32 {
    x(op, [p0, text.as_ptr() as usize, 0], &mut [a0, 0, text.len() as i32, 0], &mut [0.0; 4])
}

/// A statement answering a text into a buffer (p1, i3 its size; the answer the whole length).
fn read_text(x: &mut Sepx, op: u32, p0: usize, a0: i32) -> Vec<u8> {
    let mut buffer = vec![0u8; 256];
    loop {
        let n = x(op, [p0, buffer.as_mut_ptr() as usize, 0], &mut [a0, 0, 0, buffer.len() as i32], &mut [0.0; 4]).max(0)
            as usize;
        if n <= buffer.len() {
            buffer.truncate(n);
            return buffer;
        }
        buffer = vec![0u8; n];
    }
}

/// `gResource->GetString(id, out)`: the string, or `out` left as it was when there is none.
fn get_string(id: i64, out: &mut Vec<u8>) {
    if let Some(s) = resource_string(id) {
        *out = s.to_vec();
    }
}

/// `SeplCivFill`: none, the built-in civilizations, the custom files, the first picked.
pub fn civ_fill(x: &mut Sepx, combo: usize) {
    let mut text = Vec::new();
    st(x, EE_SCREENS_SEPX_R_COMBO_CLEAR, combo, [0; 4]);
    get_string(EE_SCREENS_SEPX_IDS_NONE, &mut text);
    st_text(x, EE_SCREENS_SEPX_R_COMBO_APPEND, combo, 0, &text);
    let mut i = 0;
    while i < st(x, EE_SCREENS_SEPX_CIV_COUNT, 0, [0; 4]) as u32 {
        get_string(st(x, EE_SCREENS_SEPX_CIV_NAME_ID, 0, [i as i32, 0, 0, 0]) as i64, &mut text);
        st_text(x, EE_SCREENS_SEPX_R_COMBO_APPEND, combo, 0, &text);
        i += 1;
    }
    st(x, EE_SCREENS_SEPX_FILES_CLEAR, 0, [0; 4]);
    st(x, EE_SCREENS_SEPX_FILES_LIST, 0, [0; 4]);
    for k in 0..st(x, EE_SCREENS_SEPX_FILES_COUNT, 0, [0; 4]) {
        let name = read_text(x, EE_SCREENS_SEPX_FILE_STEM, 0, k);
        st_text(x, EE_SCREENS_SEPX_R_COMBO_APPEND, combo, 0, &name);
    }
    st(x, EE_SCREENS_SEPX_R_COMBO_SELECT, combo, [0; 4]);
}

/// `SeplCivSelect`: the player's civilization picked: none, a built-in, a custom file's line, else none titled
/// "unknown".
pub fn civ_select(x: &mut Sepx, combo: usize, player: i32) {
    let file = read_text(x, EE_SCREENS_SEPX_PLAYER_CIV, 0, player);
    if file.is_empty() {
        st(x, EE_SCREENS_SEPX_R_COMBO_SELECT, combo, [0; 4]);
        return;
    }
    let count = st(x, EE_SCREENS_SEPX_CIV_COUNT, 0, [0; 4]) as u32;
    let mut i = 0;
    while i < count {
        if read_text(x, EE_SCREENS_SEPX_CIV_FILE, 0, i as i32) == file {
            break;
        }
        i += 1;
    }
    if i != st(x, EE_SCREENS_SEPX_CIV_COUNT, 0, [0; 4]) as u32 {
        st(x, EE_SCREENS_SEPX_R_COMBO_SELECT, combo, [(i + 1) as i32, 0, 0, 0]);
        return;
    }
    let mut text = Vec::new();
    while i < st(x, EE_SCREENS_SEPX_R_COMBO_LINES, combo, [0; 4]) as u32 {
        text = read_text(x, EE_SCREENS_SEPX_R_COMBO_LINE, combo, i as i32);
        if !differs(&text, &file) {
            st(x, EE_SCREENS_SEPX_R_COMBO_SELECT, combo, [i as i32, 0, 0, 0]);
            return;
        }
        i += 1;
    }
    st(x, EE_SCREENS_SEPX_R_COMBO_SELECT, combo, [0; 4]);
    get_string(EE_SCREENS_SEPX_IDS_UNKNOWN_CIVILIZATION, &mut text);
    st_text(x, EE_SCREENS_SEPX_R_COMBO_SET_LABEL, combo, 0, &text);
}

/// `SeplCivApply`: the chosen civilization to the player; whether it changed.
pub fn civ_apply(x: &mut Sepx, combo: usize, player: i32) -> bool {
    let selection = st(x, EE_SCREENS_SEPX_R_COMBO_SELECTION, combo, [0; 4]) as u32;
    let mut text = Vec::new();
    get_string(EE_SCREENS_SEPX_IDS_UNKNOWN_CIVILIZATION, &mut text);
    let civilization = if selection > 0 && selection <= st(x, EE_SCREENS_SEPX_CIV_COUNT, 0, [0; 4]) as u32 {
        read_text(x, EE_SCREENS_SEPX_CIV_FILE, 0, (selection - 1) as i32)
    } else {
        read_text(x, EE_SCREENS_SEPX_R_COMBO_LINE, combo, selection as i32)
    };
    if selection == 0 {
        if !read_text(x, EE_SCREENS_SEPX_PLAYER_CIV, 0, player).is_empty()
            && differs(&read_text(x, EE_SCREENS_SEPX_R_COMBO_LABEL, combo, 0), &text)
        {
            st(x, EE_SCREENS_SEPX_PLAYER_CIV_CLEAR, 0, [player, 0, 0, 0]);
            st(x, EE_SCREENS_SEPX_PLAYER_CIV_POINTS, 0, [player, 0, 0, 0]);
            st_text(x, EE_SCREENS_SEPX_PLAYER_SET_CIV, 0, player, b"");
            return true;
        }
    } else if civilization != read_text(x, EE_SCREENS_SEPX_PLAYER_CIV, 0, player) {
        st_text(x, EE_SCREENS_SEPX_PLAYER_SET_CIV, 0, player, &civilization);
        st(x, EE_SCREENS_SEPX_PLAYER_APPLY_CIV, 0, [player, 0, 0, 0]);
        return true;
    }
    false
}

/// The epochs' combo: the titles from epoch `first` on, replacing its lines.
pub fn fill_epochs(x: &mut Sepx, combo: usize, first: i32) {
    let mut title = Vec::new();
    st(x, EE_SCREENS_SEPX_LINES_CLEAR, 0, [0; 4]);
    let mut e = first as u32;
    while (e as i64) < ee_sim::tech::tech_ids::ee_tech_epoch_count() as i64 {
        get_string(ee_sim::tech::tech_ids::ee_tech_epoch_title_id(e as i32) as i64, &mut title);
        st_text(x, EE_SCREENS_SEPX_LINES_PUSH, 0, 0, &title);
        e += 1;
    }
    st(x, EE_SCREENS_SEPX_R_COMBO_CLEAR, combo, [0; 4]);
    st(x, EE_SCREENS_SEPX_COMBO_APPEND_LINES, combo, [0; 4]);
}

/// `SeplDiplomacyLoad`: the normal, focused and pressed bitmaps for allies (0) and enemies (1) cut from the editor's
/// button texture.
pub fn diplomacy_load(x: &mut Sepx, editor: usize) {
    let width = st(x, EE_SCREENS_SEPX_DIPLO_WIDTH, editor, [0; 4]) as u32 as f32;
    let top = 230.0f32 / width;
    let bottom = 253.0f32 / width;
    let scale = 23.0f32 / width;
    for i in 0..2i32 {
        let x0 = i as f32 * 46.0f32;
        let left = x0 / width;
        let right = (x0 + 23.0f32) / width;
        let pressed_right = (x0 + 46.0f32) / width;
        // normal, pressed, focused (the original's order): the pressed cell is the one right of the normal
        let cells = [(left, right), (right, pressed_right), (left, right)];
        let slots = [0, 2, 1];
        for k in 0..3 {
            let a = [slots[k], i, 0, 0];
            st(x, EE_SCREENS_SEPX_DIPLO_NEW, editor, a);
            st(x, EE_SCREENS_SEPX_DIPLO_TEXTURE, editor, a);
            st_f(x, EE_SCREENS_SEPX_DIPLO_COORDS, editor, a, [cells[k].0, cells[k].1, top, bottom]);
            st_f(x, EE_SCREENS_SEPX_DIPLO_SCALE, editor, a, [scale, 0.0, 0.0, 0.0]);
        }
    }
}

/// `SeplDiplomacyRelease`.
pub fn diplomacy_release(x: &mut Sepx) {
    for i in 0..2 {
        for slot in [0, 2, 1] {
            st(x, EE_SCREENS_SEPX_DIPLO_RELEASE, 0, [slot, i, 0, 0]);
        }
    }
}

/// `EESEDiplomacyShow`: a diplomacy button's bitmaps and help text for its state, then the form's help text when
/// `help`.
pub fn diplomacy_show(x: &mut Sepx, button: usize, state: c_ulong, help: bool) {
    let a = [state as i32, 0, 0, 0];
    st(x, EE_SCREENS_SEPX_DIPLO_PRESSED, button, a);
    st(x, EE_SCREENS_SEPX_DIPLO_FOCUSED, button, a);
    st(x, EE_SCREENS_SEPX_DIPLO_NORMAL, button, a);
    if state == 0 {
        st(x, EE_SCREENS_SEPX_R_HELP_ID, button, [EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ALLIES, 0, 0, 0]);
    } else if state == 1 {
        st(x, EE_SCREENS_SEPX_R_HELP_ID, button, [EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ENEMIES, 0, 0, 0]);
    }
    if help {
        st(x, EE_SCREENS_SEPX_DIPLO_UPDATE_HELP, button, [0; 4]);
    }
}

/// Whether the making or setting statement `op` takes the maker's text or lines ([`prestep`]).
pub fn needs_prestep(op: u32) -> bool {
    use crate::screens::editor_sepl::*;
    matches!(
        op,
        EE_SCREENS_SEPX_MAKE_LABEL
            | EE_SCREENS_SEPX_MAKE_CHECK
            | EE_SCREENS_SEPX_MAKE_UNIT_TYPE
            | EE_SCREENS_SEPX_MAKE_RADIO
            | EE_SCREENS_SEPX_MAKE_TRISTATE
            | EE_SCREENS_SEPX_EDIT_TEXT
            | EE_SCREENS_SEPX_BUTTON_TEXT
            | EE_SCREENS_SEPX_MAKE_COMBO_EPOCHS
            | EE_SCREENS_SEPX_MAKE_MAX_PLAYER
            | EE_SCREENS_SEPX_MAKE_SHELL_COMBO
            | EE_SCREENS_SEPX_MAKE_TRIGGER_COMBO
            | EE_SCREENS_SEPX_MAKE_SCENARIO_COMBO
            | EE_SCREENS_SEPX_MAKE_PLAIN_COMBO
    )
}

/// The language string `id`, empty when there is none (`GetString` into a fresh string).
fn string_or_empty(id: i64) -> Vec<u8> {
    resource_string(id).unwrap_or_default().to_vec()
}

/// Before the maker's statement `op` on the record `c`: the text it takes (`SeplText` of the record's kind, string
/// and number) or the lines its combo box is made with (the epochs' titles, the counts 2..16 in the record's text,
/// the unit families and the editor's unit sets, the pending lines, the record's strings).
pub fn prestep(x: &mut Sepx, op: u32, c: &EeScreensSeplCtl) {
    use crate::screens::editor_direct::sepl_text;
    use crate::screens::editor_map::EE_SCREENS_SEPL_COMBO_LINES_PENDING;
    use crate::screens::editor_object::{
        EE_SCREENS_SEOB_FAMILIES, EE_SCREENS_SEPL_COMBO_LINES_FAMILIES, EE_SCREENS_SEPL_COMBO_LINES_STRINGS,
    };
    use crate::screens::editor_selt::{c_format, EeScreensSeltArg};
    use crate::screens::editor_sepl::*;
    let push = |x: &mut Sepx, line: &[u8]| {
        st_text(x, EE_SCREENS_SEPX_LINES_PUSH, 0, 0, line);
    };
    match op {
        EE_SCREENS_SEPX_MAKE_COMBO_EPOCHS => {
            st(x, EE_SCREENS_SEPX_LINES_CLEAR, 0, [0; 4]);
            for e in 0..ee_sim::tech::tech_ids::ee_tech_epoch_count().max(0) {
                push(x, &string_or_empty(ee_sim::tech::tech_ids::ee_tech_epoch_title_id(e) as i64));
            }
        }
        EE_SCREENS_SEPX_MAKE_MAX_PLAYER => {
            st(x, EE_SCREENS_SEPX_LINES_CLEAR, 0, [0; 4]);
            for n in 2..=16 {
                push(x, &sepl_text(c.text, c.string, n));
            }
        }
        EE_SCREENS_SEPX_MAKE_SHELL_COMBO
        | EE_SCREENS_SEPX_MAKE_TRIGGER_COMBO
        | EE_SCREENS_SEPX_MAKE_SCENARIO_COMBO
        | EE_SCREENS_SEPX_MAKE_PLAIN_COMBO => {
            st(x, EE_SCREENS_SEPX_LINES_CLEAR, 0, [0; 4]);
            if c.extra[1] == EE_SCREENS_SEPL_COMBO_LINES_FAMILIES {
                for k in 0..EE_SCREENS_SEOB_FAMILIES as i64 {
                    push(x, &string_or_empty(k + EE_SCREENS_SEPX_IDS_UNIT_FAMILY_FIRST));
                }
                let format = string_or_empty(EE_SCREENS_SEPX_IDS_SET);
                for j in 0..st(x, EE_SCREENS_SEPX_SETS_COUNT, 0, [0; 4]) {
                    if st(x, EE_SCREENS_SEPX_SET_SHOWN, 0, [j, 0, 0, 0]) == 0 {
                        continue;
                    }
                    let name = read_text(x, EE_SCREENS_SEPX_SET_NAME, 0, j);
                    push(x, &c_format(&format, &[EeScreensSeltArg::Text(&name)]));
                }
            } else if c.extra[1] == EE_SCREENS_SEPL_COMBO_LINES_PENDING {
                st(x, EE_SCREENS_SEPX_LINES_TAKE_PENDING, 0, [0; 4]);
                st(x, EE_SCREENS_SEPX_PENDING_CLEAR, 0, [0; 4]);
            } else if c.extra[1] == EE_SCREENS_SEPL_COMBO_LINES_STRINGS {
                for k in 0..c.number.max(0) as i64 {
                    push(x, &string_or_empty(c.string as i64 + k));
                }
            }
        }
        _ => {
            st_text(x, EE_SCREENS_SEPX_SET_TEXT, 0, 0, &sepl_text(c.text, c.string, c.number as core::ffi::c_long));
        }
    }
}

/// The player tab's ops composed here, or `None` (editor_sepl.rs's fallthrough).
pub fn compose(x: &mut Sepx, op: u32, p: [usize; 3], i: &mut [i32; 4], _f: &mut [f32; 4]) -> Option<i32> {
    Some(match op {
        EE_SCREENS_SEPL_OP_CIV_FILL => {
            civ_fill(x, p[0]);
            0
        }
        EE_SCREENS_SEPL_OP_CIV_SELECT => {
            civ_select(x, p[0], i[0]);
            0
        }
        EE_SCREENS_SEPL_OP_CIV_APPLY => civ_apply(x, p[0], i[0]) as i32,
        EE_SCREENS_SEPL_OP_FILL_EPOCHS => {
            fill_epochs(x, p[0], i[0]);
            0
        }
        EE_SCREENS_SEPL_OP_LOAD_BITMAPS => {
            diplomacy_load(x, p[0]);
            0
        }
        EE_SCREENS_SEPL_OP_RELEASE_BITMAPS => {
            diplomacy_release(x);
            0
        }
        crate::screens::editor_map::EE_SCREENS_SEPL_OP_LINE_PUSH => {
            let line = crate::screens::editor_direct::sepl_text(i[0] as u32, i[1] as u32, i[2] as core::ffi::c_long);
            st_text(x, EE_SCREENS_SEPX_PENDING_PUSH, 0, 0, &line);
            0
        }
        _ => return None,
    })
}

/// The toolkit's statements (`EE_SCREENS_SEPX_R_*`) on the controls' Rust objects; `None` for the C++'s.
#[cfg(not(test))]
pub fn direct(op: u32, p: [usize; 3], i: &mut [i32; 4]) -> Option<i32> {
    use crate::screens::editor_direct::{combo_append, combo_line, combo_lines, combo_text};
    use crate::screens::hud_direct::label_set_text;
    use ee_ui::comboobj::EeUiComboCall;
    use ee_ui::direct::{self as d, Cpp};
    let c = p[0] as Cpp;
    // SAFETY: a text in: p1 holds i2 bytes (the statement's contract)
    let text_in = || unsafe { core::slice::from_raw_parts(p[1] as *const u8, i[2].max(0) as usize) }.to_vec();
    let out = |text: Vec<u8>, i: &[i32; 4]| -> i32 {
        let n = text.len().min(i[3].max(0) as usize);
        // SAFETY: p1 holds i3 bytes (the statement's contract)
        unsafe { core::ptr::copy_nonoverlapping(text.as_ptr(), p[1] as *mut u8, n) };
        text.len() as i32
    };
    Some(match op {
        EE_SCREENS_SEPX_R_COMBO_CLEAR => {
            d::combo_call(c, EeUiComboCall::DeleteAllEntries, 0, 0);
            0
        }
        EE_SCREENS_SEPX_R_COMBO_APPEND => {
            combo_append(c, &text_in());
            0
        }
        EE_SCREENS_SEPX_R_COMBO_SELECT => {
            d::combo_call(c, EeUiComboCall::SetCurrentSelection, i[0] as c_ulong as isize, 0);
            0
        }
        EE_SCREENS_SEPX_R_COMBO_SELECTION => d::combo_call(c, EeUiComboCall::Selection, 0, 0).unwrap_or(-1) as i32,
        EE_SCREENS_SEPX_R_COMBO_LINES => combo_lines(c) as i32,
        EE_SCREENS_SEPX_R_COMBO_LINE => out(combo_line(c, i[0] as c_ulong as isize), i),
        EE_SCREENS_SEPX_R_COMBO_LABEL => out(combo_text(c), i),
        EE_SCREENS_SEPX_R_COMBO_SET_LABEL => {
            let label = d::combo_call(c, EeUiComboCall::Part, 4, 0).unwrap_or(0) as Cpp;
            label_set_text(label, &text_in());
            0
        }
        EE_SCREENS_SEPX_R_HELP_ID => {
            d::with_fields(c, |r| r.mStringTableID = i[0] as c_ulong);
            0
        }
        _ => return None,
    })
}

/// `EESEDiplomacyShow` (the shell's `gEEUIShellDiplomacyShow`, ee/EEUIShell.h): `button`'s bitmaps and help for
/// `state`.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_screens_sepl_diplomacy_show(button: *mut core::ffi::c_void, state: c_ulong, help: bool) {
    diplomacy_show(&mut sepx_statement, button as usize, state, help);
}

/// `EEUIFSEBrushGroup`'s members (ee/EEUIFSEBrushGroup.h, the layout asserted by EEUIEOpenCivBuilder.cpp): the
/// highlight square (`mRectangle`'s bottom, left, right, top), the five radios, the title, the brush size, the
/// highlight's and the centring's flags.
#[repr(C)]
pub struct EeScreensSeBrushGroup {
    pub bottom: f32,
    pub left: f32,
    pub right: f32,
    pub top: f32,
    pub buttons: [usize; 5],
    pub title: usize,
    pub brush_size: c_ulong,
    pub highlight_shown: bool,
    pub centered: bool,
}

/// `EEUIFSEBrushGroup::EEUIFSEBrushGroup`'s body: the title and the five radios made (`build`, editor_brush.rs) in
/// the font `font`, the next control index advanced, the first radio's brush size taken.
pub fn brush_group_new(
    x: &mut Sepx,
    g: &mut EeScreensSeBrushGroup,
    font: usize,
    build: impl FnOnce(&mut u32) -> [usize; 6],
    index: &mut c_ulong,
) {
    g.highlight_shown = false;
    let mut a = [0; 4];
    x(EE_SCREENS_SEPX_SCREEN_FONT, [0; 3], &mut a, &mut [0.0; 4]);
    let saved = (a[0] as u32 as usize) | ((a[1] as u32 as usize) << 32);
    st(x, EE_SCREENS_SEPX_SET_SCREEN_FONT, font, [0; 4]);
    let mut next = *index as u32;
    let controls = build(&mut next);
    st(x, EE_SCREENS_SEPX_SET_SCREEN_FONT, saved, [0; 4]);
    *index = next as c_ulong;
    g.title = controls[0];
    g.buttons.copy_from_slice(&controls[1..6]);
    g.brush_size = st(x, EE_SCREENS_SEPX_BRUSH_SIZE, g.buttons[0], [0; 4]) as u32 as c_ulong;
    g.centered = false;
}

/// `EEUIFSEBrushGroup::SetHighlight`: the square around the map position, one tile larger to the top left when
/// centred (editor_brush.rs).
pub fn brush_group_highlight(g: &mut EeScreensSeBrushGroup, px: f32, py: f32, centered: bool) {
    let r = crate::screens::editor_brush::highlight(px, py, g.brush_size as u32, centered);
    g.left = r[0];
    g.top = r[1];
    g.right = r[2];
    g.bottom = r[3];
    g.centered = centered;
}

/// `EEUIFSEBrushGroup`'s destructor: its five buttons, then its title deleted (`delete` of each, the C++'s order).
pub fn brush_group_free(x: &mut Sepx, g: &EeScreensSeBrushGroup) {
    for b in g.buttons.into_iter().chain([g.title]) {
        x(EE_SCREENS_SEPL_OP_DELETE, [b, 0, 0], &mut [0; 4], &mut [0.0; 4]);
    }
}

/// The brush group's destructor body.
///
/// # Safety
/// `group` is the C++ group being destroyed.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sebr_group_free(group: *const EeScreensSeBrushGroup) {
    let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]| -> i32 {
        let p = p.map(|v| v as *mut core::ffi::c_void);
        // SAFETY: the group's live controls
        unsafe { crate::screens::editor_direct::sepl(op, p.as_ptr(), i, f) }
    };
    // SAFETY: the contract
    brush_group_free(&mut x, unsafe { &*group });
}

/// The brush group's constructor body (`index` the form's next control index, advanced).
///
/// # Safety
/// `group` is the C++ group under construction, `index` its caller's index, `form` and `font` live.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sebr_group_new(
    group: *mut EeScreensSeBrushGroup,
    index: *mut c_ulong,
    x: f32,
    y: f32,
    form: *mut core::ffi::c_void,
    font: *mut core::ffi::c_void,
) {
    // SAFETY: the contract
    let (g, index) = unsafe { (&mut *group, &mut *index) };
    let build = |next: &mut u32| {
        let mut out = [0usize; 6];
        // SAFETY: six handles out, the index in and out
        unsafe {
            crate::screens::editor_terrain_ffi::ee_screens_sebr_build(group.cast(), form, x, y, next, out.as_mut_ptr())
        };
        out
    };
    brush_group_new(&mut sepx_statement, g, font as usize, build, index);
}

/// `EEUIFSEBrushGroup::SetHighlight`.
///
/// # Safety
/// `group` is a live C++ group.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sebr_group_highlight(
    group: *mut EeScreensSeBrushGroup,
    x: f32,
    y: f32,
    centered: bool,
) {
    // SAFETY: the contract
    brush_group_highlight(unsafe { &mut *group }, x, y, centered);
}

/// One of the C++'s statements (`ee_screenscpp_sepx`), or the toolkit's (`_R_`).
#[cfg(not(test))]
fn sepx_statement(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
    /// cbindgen:ignore
    mod sys {
        use core::ffi::c_void;

        extern "C" {
            pub fn ee_screenscpp_sepx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        }
    }
    if let Some(r) = direct(op, p, i) {
        return r;
    }
    let p = p.map(|v| v as *mut core::ffi::c_void);
    // SAFETY: the statement's live handles
    unsafe { sys::ee_screenscpp_sepx(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr()) }
}

#[cfg(test)]
#[path = "editor_seplx_tests.rs"]
mod tests;
