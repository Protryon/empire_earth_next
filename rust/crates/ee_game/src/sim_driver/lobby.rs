//! The lobby's choices (TEST ONLY): which combo gets which line, in the harness's order (tools/harness/ai_game.py),
//! and a line found by name.

use std::os::raw::c_long;

use super::options::{c_atol, Options};

/// The player type boxes (slot k: 30 + k), the team boxes, the starting citizens' boxes, the engine boxes
/// (ee/EEUIManager.h kLobbyEngineCombo + slot).
pub const PLAYER_TYPE: c_long = 30;
pub const TEAM: c_long = 62;
pub const CITIZENS: c_long = 126;
pub const ENGINE: c_long = 275;
pub const VISIBLE_SLOTS: c_long = 8;

/// The text trimmed of blanks and tabs, in lower case (ASCII).
pub fn lower(text: &str) -> String {
    text.trim_matches(|c| c == ' ' || c == '\t').to_ascii_lowercase()
}

/// A line by name (ai_game.py resolve): "#k" an index, else the exact line, else the one line it starts, else the
/// one line containing it. `Err` the failure's text.
pub fn resolve(value: &str, lines: &[String], what: &str) -> Result<usize, String> {
    if let Some(rest) = value.strip_prefix('#') {
        let k = c_atol(rest);
        if k < 0 || k as usize >= lines.len() {
            return Err(format!("{what}: index {value} outside the combo's {} lines", lines.len()));
        }
        return Ok(k as usize);
    }
    let key = lower(value);
    let lowered: Vec<String> = lines.iter().map(|l| lower(l)).collect();
    if let Some(k) = lowered.iter().position(|l| *l == key) {
        return Ok(k);
    }
    let mut hits: Vec<usize> = (0..lines.len()).filter(|&k| lowered[k].starts_with(&key)).collect();
    if hits.is_empty() {
        hits = (0..lines.len()).filter(|&k| lowered[k].contains(&key)).collect();
    }
    if hits.len() != 1 {
        return Err(format!("{what}: '{value}' matches {} of the lines {}", hits.len(), lines.join("|")));
    }
    Ok(hits[0])
}

/// The engine names of `--engine` (a comma list; "" none).
pub fn engines(list: &str) -> Vec<String> {
    if list.is_empty() {
        return Vec::new();
    }
    list.split(',').map(str::to_string).collect()
}

/// The lobby's (control, value) list, applied in order, absolutely (ai_game.py): the defaults, the options' map
/// size, map type, difficulty and `--lobby` boxes over them, no team and 5 starting citizens for each playing slot,
/// then each computer seat's engine box (`choice` gives the box's line of an engine name, `Err` the failure).
pub fn lobby_options(
    o: &Options,
    choice: &mut dyn FnMut(&str) -> Result<c_long, String>,
) -> Result<Vec<(c_long, String)>, String> {
    let mut options: Vec<(c_long, String)> = [
        (175, "Random Map"),
        (177, "Small"),
        (179, "Continental"),
        (181, "Standard - High"),
        (183, "Prehistoric Age"),
        (185, "Space Age"),
        (187, "1200"),
        (189, "Off"),
        (205, "Standard"),
        (207, "Easy"),
        (209, "Very Fast"),
    ]
    .iter()
    .map(|&(c, v)| (c, v.to_string()))
    .collect();
    fn set(options: &mut Vec<(c_long, String)>, control: c_long, value: &str) {
        match options.iter_mut().find(|o| o.0 == control) {
            Some(o) => o.1 = value.to_string(),
            None => options.push((control, value.to_string())),
        }
    }
    if !o.map_size.is_empty() {
        set(&mut options, 177, &o.map_size);
    }
    if !o.map_type.is_empty() {
        set(&mut options, 179, &o.map_type);
    }
    if !o.difficulty.is_empty() {
        set(&mut options, 207, &o.difficulty);
    }
    for (c, v) in &o.lobby {
        set(&mut options, *c, v);
    }
    let n = o.computers;
    for s in 0..=n {
        let team = options.iter().any(|x| x.0 == TEAM + s);
        let citizens = options.iter().any(|x| x.0 == CITIZENS + s);
        if !team {
            options.push((TEAM + s, "-".into()));
        }
        if !citizens {
            options.push((CITIZENS + s, "5".into()));
        }
    }
    let engines = engines(&o.engine);
    for s in 1..=n {
        if engines.is_empty() {
            break;
        }
        if options.iter().any(|x| x.0 == ENGINE + s) {
            continue;
        }
        let name = &engines[(s - 1) as usize % engines.len()];
        let k = choice(name)?;
        options.push((ENGINE + s, format!("#{k}")));
    }
    Ok(options)
}
