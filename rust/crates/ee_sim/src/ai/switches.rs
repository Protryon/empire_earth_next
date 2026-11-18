//! The computer players' TEST ONLY switches (EECPTrace.cpp's EECPTraceInit before; lane A, round 2): the
//! environment's per-player lists, read once and taken in turn by player id (player 1 the first entry):
//!
//! - `EE_AI_PERSONALITY`: "a,b,...": personalities by their rules id (unknown ids are left out, with a line);
//! - `EE_AI_DEMAND`: "1/0/...": overrides economy.demand_split;
//! - `EE_AI_CHEATS`: "a,b" (one set for every player) or "set/set/...": the cheats it lets through ("all", "none");
//! - `EE_AI_EXP`: the named experiments, "a,b" or "set/set/..." ("a+b" joins too).
//!
//! The values stay as the C++ kept them; the C++'s accessors (EECPExperiment, EECPPersonality, EECPCheats,
//! EECPDemandSplitOn) ask here.

use std::ffi::{c_char, c_long, CStr, CString};
use std::sync::OnceLock;

/// The cheats' names in their bits' order (gameoptions.yaml ai_cheats; EERules.cpp kEEAICheatNames)
pub const CHEAT_NAMES: [&str; 7] = ["cost", "reveal", "epochs", "extra", "popband", "catchup", "retire"];
/// `kAICheatAll`
pub const CHEAT_ALL: c_long = 127;

/// The parsed lists.
#[derive(Debug, Default, PartialEq)]
pub struct Switches {
    /// personalities' positions in the rules
    pub personalities: Vec<c_long>,
    pub demand: Vec<bool>,
    pub cheats: Vec<c_long>,
    /// each player's set as ",a,b," (NUL-terminated for the C++)
    pub experiments: Vec<CString>,
}

/// `EEAICheatBit` (0: not a cheat's name)
pub fn cheat_bit(name: &str) -> c_long {
    CHEAT_NAMES.iter().position(|n| *n == name).map_or(0, |i| 1 << i)
}

/// The pieces of `list` between `sep`s (an empty list is one empty piece, as the C++'s loop took it)
fn pieces(list: &str, sep: char) -> impl Iterator<Item = &str> {
    list.split(sep)
}

/// The lists from the environment's values (None or "": not set); `find` gives a personality's position (-1:
/// unknown), `warn` takes a line for an unknown name
pub fn parse(
    personality: Option<&str>,
    demand: Option<&str>,
    cheats: Option<&str>,
    experiments: Option<&str>,
    find: &mut dyn FnMut(&str) -> c_long,
    warn: &mut dyn FnMut(String),
) -> Switches {
    let mut s = Switches::default();
    if let Some(list) = personality.filter(|v| !v.is_empty()) {
        for name in pieces(list, ',') {
            let k = find(name);
            if k >= 0 {
                s.personalities.push(k);
            } else {
                warn(format!("EE-AI EE_AI_PERSONALITY: unknown personality '{name}' ignored\n"));
            }
        }
    }
    if let Some(list) = demand.filter(|v| !v.is_empty()) {
        s.demand = pieces(list, '/').map(|v| !v.is_empty() && v != "0").collect();
    }
    if let Some(sets) = cheats.filter(|v| !v.is_empty()) {
        for set in pieces(sets, '/') {
            let mut bits: c_long = 0;
            for name in pieces(set, ',') {
                let bit = if name == "all" { CHEAT_ALL } else { cheat_bit(name) };
                if bit == 0 && name != "none" && !name.is_empty() {
                    warn(format!("EE-AI EE_AI_CHEATS: unknown cheat '{name}' ignored\n"));
                }
                bits |= bit;
            }
            s.cheats.push(bits);
        }
    }
    if let Some(sets) = experiments.filter(|v| !v.is_empty()) {
        for set in pieces(sets, '/') {
            let joined = format!(",{},", set.replace('+', ","));
            s.experiments.push(CString::new(joined).unwrap_or_default());
        }
    }
    s
}

/// The entry of a list for the player `id` (player 1 the first; player 0 too)
pub fn pick<T>(list: &[T], id: u64) -> Option<&T> {
    if list.is_empty() {
        return None;
    }
    let i = id.saturating_sub(1) as usize % list.len();
    list.get(i)
}

/// Whether the player's experiments name `name`
pub fn experiment(s: &Switches, id: u64, name: &str) -> bool {
    pick(&s.experiments, id).is_some_and(|set| set.to_str().is_ok_and(|t| t.contains(&format!(",{name},"))))
}

static SWITCHES: OnceLock<Switches> = OnceLock::new();

fn switches() -> &'static Switches {
    SWITCHES.get_or_init(Switches::default)
}

fn env(name: &str) -> Option<String> {
    std::env::var(name).ok()
}

/// Reads the switches from the environment (once; later calls do nothing). `find` gives a personality's position
/// in the rules (-1: unknown), `out` writes a line to the game's debug output.
///
/// # Safety
/// `find` and `out` are callable with a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_switches_init(
    find: unsafe extern "C" fn(*const c_char) -> c_long,
    out: unsafe extern "C" fn(*const c_char),
) {
    SWITCHES.get_or_init(|| {
        let mut find = |name: &str| {
            let c = CString::new(name).unwrap_or_default();
            // SAFETY: the caller's contract
            unsafe { find(c.as_ptr()) }
        };
        let mut warn = |line: String| {
            let c = CString::new(line).unwrap_or_default();
            // SAFETY: the caller's contract
            unsafe { out(c.as_ptr()) }
        };
        parse(
            env("EE_AI_PERSONALITY").as_deref(),
            env("EE_AI_DEMAND").as_deref(),
            env("EE_AI_CHEATS").as_deref(),
            env("EE_AI_EXP").as_deref(),
            &mut find,
            &mut warn,
        )
    });
}

/// `EECPExperiment`: the player `id`'s experiments name `name`.
///
/// # Safety
/// `name` is a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_switch_experiment(id: u64, name: *const c_char) -> bool {
    let name = CStr::from_ptr(name).to_str().unwrap_or("");
    experiment(switches(), id, name)
}

/// `EECPExperimentNames`: the player's set as ",a,b," ("-": none or empty); the text lives as long as the game.
#[no_mangle]
pub extern "C" fn ee_ai_switch_experiment_names(id: u64) -> *const c_char {
    match pick(&switches().experiments, id) {
        Some(set) if set.as_bytes().len() > 2 => set.as_ptr(),
        _ => c"-".as_ptr(),
    }
}

/// `EE_AI_DEMAND`'s value for the player: 1, 0, or -1 when not set.
#[no_mangle]
pub extern "C" fn ee_ai_switch_demand(id: u64) -> i32 {
    pick(&switches().demand, id).map_or(-1, |&v| v as i32)
}

/// `EE_AI_PERSONALITY`'s position for the player (-1: not set).
#[no_mangle]
pub extern "C" fn ee_ai_switch_personality(id: u64) -> c_long {
    pick(&switches().personalities, id).copied().unwrap_or(-1)
}

/// `EE_AI_CHEATS`'s set for the player (`kAICheatAll` when not set).
#[no_mangle]
pub extern "C" fn ee_ai_switch_cheats(id: u64) -> c_long {
    pick(&switches().cheats, id).copied().unwrap_or(CHEAT_ALL)
}

/// `EECPCheatNames`: the cheats of `bits` by name, "a,b" ("none": none) into `out` (room for `cap` bytes,
/// NUL-terminated) -> the length.
///
/// # Safety
/// `out` has room for `cap` bytes (at least 1).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_switch_cheat_names(bits: c_long, out: *mut c_char, cap: usize) -> usize {
    let text = cheat_names(bits);
    let n = text.len().min(cap.saturating_sub(1));
    std::ptr::copy_nonoverlapping(text.as_ptr(), out.cast(), n);
    *out.add(n) = 0;
    n
}

/// The cheats of `bits` by name
pub fn cheat_names(bits: c_long) -> String {
    let names: Vec<&str> = (0..CHEAT_NAMES.len()).filter(|i| bits & (1 << i) != 0).map(|i| CHEAT_NAMES[i]).collect();
    if names.is_empty() {
        "none".to_string()
    } else {
        names.join(",")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(p: Option<&str>, d: Option<&str>, c: Option<&str>, e: Option<&str>) -> (Switches, Vec<String>) {
        let mut warned = Vec::new();
        let mut find = |n: &str| match n {
            "classic" => 0,
            "tall" => 2,
            _ => -1,
        };
        let s = parse(p, d, c, e, &mut find, &mut |l| warned.push(l));
        (s, warned)
    }

    #[test]
    fn unset_and_empty() {
        let (s, w) = run(None, Some(""), None, Some(""));
        assert_eq!(s, Switches::default());
        assert!(w.is_empty());
        assert_eq!(pick(&s.cheats, 3), None);
        assert!(!experiment(&s, 1, "x"));
    }

    #[test]
    fn personalities_in_turn() {
        let (s, w) = run(Some("tall,nope,classic"), None, None, None);
        assert_eq!(s.personalities, vec![2, 0]);
        assert_eq!(w, vec!["EE-AI EE_AI_PERSONALITY: unknown personality 'nope' ignored\n".to_string()]);
        assert_eq!(
            (pick(&s.personalities, 1), pick(&s.personalities, 2), pick(&s.personalities, 3)),
            (Some(&2), Some(&0), Some(&2))
        );
        assert_eq!(pick(&s.personalities, 0), Some(&2)); // player 0 takes the first
                                                         // a trailing comma is an empty, unknown name
        let (s, w) = run(Some("classic,"), None, None, None);
        assert_eq!((s.personalities.len(), w.len()), (1, 1));
    }

    #[test]
    fn demand_and_cheats() {
        let (s, _) = run(None, Some("1/0//2"), None, None);
        assert_eq!(s.demand, vec![true, false, false, true]);
        let (s, w) = run(None, None, Some("cost,reveal/none/all/bogus,retire/"), None);
        assert_eq!(s.cheats, vec![3, 0, 127, 64, 0]);
        assert_eq!(w, vec!["EE-AI EE_AI_CHEATS: unknown cheat 'bogus' ignored\n".to_string()]);
        assert_eq!(cheat_bit("popband"), 16);
        assert_eq!(cheat_names(0), "none");
        assert_eq!(cheat_names(1 | 4 | 64), "cost,epochs,retire");
    }

    #[test]
    fn experiments() {
        let (s, _) = run(None, None, None, Some("a+b/-/c"));
        assert_eq!(s.experiments[0].to_str().unwrap(), ",a,b,");
        assert!(experiment(&s, 1, "a") && experiment(&s, 1, "b") && !experiment(&s, 1, "c"));
        assert!(!experiment(&s, 2, "-x") && experiment(&s, 2, "-"));
        assert!(experiment(&s, 3, "c") && experiment(&s, 4, "a"));
        assert_eq!(s.experiments[1].to_str().unwrap(), ",-,");
    }
}
