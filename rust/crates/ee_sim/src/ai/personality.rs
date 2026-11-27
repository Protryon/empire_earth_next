//! The computer player's personality and its switches (were EECPTrace.cpp's, the AI Thread cluster's round 8):
//! the player's personality (aipersonalities.yaml, or EE_AI_PERSONALITY's, TEST ONLY), the rules' switches with
//! their EE_AI_EXP experiments (TEST ONLY) on or off, the difficulty's cheats narrowed by EE_AI_CHEATS, and the
//! first read of the environment (`EECPTraceInit`: EE_AI_TRACE and the switches of `switches`).
//!
//! The rules are the game's (`EEGetRules()`'s Rust owner, bound by the civilization manager: `tech::civ::rules`).
//! Every accessor reads the environment first, as each C++ accessor called EECPTraceInit.

use std::ffi::{c_char, c_long, CStr};
use std::sync::atomic::{AtomicBool, Ordering};

use ee_data::generated::rules_gen::{AIPersonalityRule, AIPersonalityRules};

use super::switches;

static TRACE_READ: AtomicBool = AtomicBool::new(false);

#[cfg(not(test))]
extern "C" {
    /// `EECPTraceInit()` (EECPTrace.cpp: `ee_ai_trace_init` over the C++'s trace flags)
    fn ee_ai_host_trace_init();
    /// `gWorld.mLobbyOptions.mDifficulty` (EECPTrace.cpp)
    fn ee_ai_host_difficulty() -> u32;
}

#[cfg(test)]
unsafe fn ee_ai_host_trace_init() {
    // SAFETY: no flags to write
    unsafe { ee_ai_trace_init(std::ptr::null_mut(), std::ptr::null_mut()) }
}

#[cfg(test)]
unsafe fn ee_ai_host_difficulty() -> u32 {
    1
}

/// `EECPTraceInit`: the environment read once.
fn init() {
    if !TRACE_READ.load(Ordering::Acquire) {
        // SAFETY: the C++ flags' owner, on the simulation thread
        unsafe { ee_ai_host_trace_init() }
    }
}

/// A personality's position in the rules (-1: none): EE_AI_PERSONALITY's names.
fn find(rules: &AIPersonalityRules, name: &str) -> c_long {
    rules.personalities.iter().position(|p| p.id == name.as_bytes()).map_or(-1, |k| k as c_long)
}

/// `EECPTraceInit`: EE_AI_TRACE ("" and "0": off) into `on` and `hash` (each may be null), and the TEST ONLY
/// switches; once (later calls do nothing).
///
/// # Safety
/// `on` and `hash` are null or writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_trace_init(on: *mut bool, hash: *mut bool) {
    if TRACE_READ.swap(true, Ordering::AcqRel) {
        return;
    }
    let trace = std::env::var("EE_AI_TRACE").ok();
    let trace_on = trace.as_deref().is_some_and(|t| !t.is_empty() && t != "0");
    // SAFETY: the caller's contract
    unsafe {
        if let Some(on) = on.as_mut() {
            *on = trace_on;
        }
        // the sim hash only with EE_AI_TRACE (the game log turns the trace on alone)
        if let Some(hash) = hash.as_mut() {
            *hash = trace_on;
        }
    }
    extern "C" fn find_c(name: *const c_char) -> c_long {
        // SAFETY: switches' NUL-terminated name
        let name = unsafe { CStr::from_ptr(name) }.to_str().unwrap_or("");
        find(&crate::tech::civ::rules().ai_personalities, name)
    }
    extern "C" fn out_c(line: *const c_char) {
        // SAFETY: a NUL-terminated line
        #[cfg(not(test))]
        unsafe {
            super::next_game::debug_out(line)
        }
        #[cfg(test)]
        let _ = line;
    }
    // SAFETY: both callbacks take NUL-terminated strings
    unsafe { switches::ee_ai_switches_init(find_c, out_c) };
}

/// `EEAIPersonalityRules::ForPlayer`: the assignment's personality for player `id` (n takes `assign[(n - 1) %
/// size]`; the first for a bad position; "balanced"'s defaults without any).
pub fn for_player(rules: &AIPersonalityRules, id: u64) -> &AIPersonalityRule {
    static NONE: std::sync::OnceLock<AIPersonalityRule> = std::sync::OnceLock::new();
    if rules.personalities.is_empty() {
        return NONE.get_or_init(|| AIPersonalityRule { id: b"balanced".to_vec(), ..AIPersonalityRule::default() });
    }
    let mut k: i64 = 0;
    if !rules.assign.is_empty() {
        k = rules.assign[(id.saturating_sub(1) % rules.assign.len() as u64) as usize] as i64;
    }
    &rules.personalities[if k >= 0 && (k as usize) < rules.personalities.len() { k as usize } else { 0 }]
}

/// The position of the player's personality in `rules` (None: the "balanced" defaults of no list).
fn position(rules: &AIPersonalityRules, id: u64) -> Option<usize> {
    let k = switches::ee_ai_switch_personality(id);
    if k >= 0 && (k as usize) < rules.personalities.len() {
        return Some(k as usize);
    }
    let p = for_player(rules, id);
    rules.personalities.iter().position(|q| std::ptr::eq(q, p))
}

/// `EECPPersonality`: the player's personality.
pub fn rule(id: u64) -> &'static AIPersonalityRule {
    init();
    let rules = &crate::tech::civ::rules().ai_personalities;
    match position(rules, id) {
        Some(k) => &rules.personalities[k],
        None => for_player(rules, id),
    }
}

/// `EECPPersonality`'s position in the rules' list (the C++'s `mPersonalities`); -1: none (no list).
#[no_mangle]
pub extern "C" fn ee_ai_personality_index(id: u64) -> c_long {
    init();
    position(&crate::tech::civ::rules().ai_personalities, id).map_or(-1, |k| k as c_long)
}

/// `EECPExperiment`
pub fn experiment(id: u64, name: &str) -> bool {
    init();
    switches::experiment_of(id, name)
}

/// `EECPExperiment(player, name)` for the C++.
///
/// # Safety
/// `name` is a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_experiment(id: u64, name: *const c_char) -> bool {
    // SAFETY: the caller's contract
    experiment(id, unsafe { CStr::from_ptr(name) }.to_str().unwrap_or(""))
}

/// `EECPExperimentNames`: ",a,b," or "-".
#[no_mangle]
pub extern "C" fn ee_ai_experiment_names(id: u64) -> *const c_char {
    init();
    switches::ee_ai_switch_experiment_names(id)
}

/// on when `on` names an experiment, else the rule unless `off` does
fn rule_on(id: u64, rule: bool, on: &str, off: &str) -> bool {
    if experiment(id, on) {
        return true;
    }
    rule && !experiment(id, off)
}

/// military.militia; TEST ONLY: EE_AI_EXP nomilitia
pub fn militia_on(id: u64) -> bool {
    rule(id).militia && !experiment(id, "nomilitia")
}
/// economy.coast_fishing; nofish
pub fn coast_fishing_on(id: u64) -> bool {
    rule(id).coast_fishing && !experiment(id, "nofish")
}
/// military.militia_towers; nomtower
pub fn militia_towers_on(id: u64) -> bool {
    rule(id).militia_towers && !experiment(id, "nomtower")
}
/// military.prehistoric_army (0: none); prearmy4 (the owner's "maybe 4 clubmen before stone"), noprearmy
pub fn prehistoric_army(id: u64) -> c_long {
    if experiment(id, "prearmy4") {
        return 4;
    }
    if experiment(id, "noprearmy") {
        0
    } else {
        rule(id).prehistoric_army as c_long
    }
}
/// economy.forage_patches; foodeco, nofoodeco
pub fn forage_patches_on(id: u64) -> bool {
    rule_on(id, rule(id).forage_patches, "foodeco", "nofoodeco")
}
/// economy.claim_needed; goldfix, nogoldfix
pub fn claim_needed_on(id: u64) -> bool {
    rule_on(id, rule(id).claim_needed, "goldfix", "nogoldfix")
}
/// military.naval_flee; navalflee, nonavalflee
pub fn naval_flee_on(id: u64) -> bool {
    rule_on(id, rule(id).naval_flee, "navalflee", "nonavalflee")
}
/// economy.early_town_centers (0: the original's timers); noupgrade, nonoupgrade
pub fn early_town_centers(id: u64) -> c_long {
    if experiment(id, "noupgrade") {
        return 2;
    }
    if experiment(id, "nonoupgrade") {
        0
    } else {
        rule(id).early_town_centers as c_long
    }
}
/// economy.early_trainers (0: no cap); fewbarracks, manybarracks
pub fn early_trainers(id: u64) -> c_long {
    if experiment(id, "fewbarracks") {
        return 2;
    }
    if experiment(id, "manybarracks") {
        0
    } else {
        rule(id).early_trainers as c_long
    }
}
/// economy.patch_expand; patchexpand, nopatchexpand
pub fn patch_expand_on(id: u64) -> bool {
    rule_on(id, rule(id).patch_expand, "patchexpand", "nopatchexpand")
}
/// epoch_pressure.safe_bonus; safedash, nosafedash
pub fn safe_bonus(id: u64) -> f32 {
    if experiment(id, "safedash") {
        return 0.25;
    }
    if experiment(id, "nosafedash") {
        0.0
    } else {
        rule(id).safe_bonus
    }
}
/// not military.stone_attacks; stonepeace, nostonepeace
pub fn stone_peace_on(id: u64) -> bool {
    rule_on(id, !rule(id).stone_attacks, "stonepeace", "nostonepeace")
}
/// not military.prehistoric_attacks; prepeace, noprepeace
pub fn prehistoric_peace_on(id: u64) -> bool {
    rule_on(id, !rule(id).prehistoric_attacks, "prepeace", "noprepeace")
}
/// `kEECPClaimRange` (the strategy's claim range is the Lua verbs', data/ai/lib/verbs.lua)
pub const CLAIM_RANGE: c_long = 25;
/// economy.forage_patches with the claim range; oldpatches keeps forage_patches without it
pub fn claim_range_on(id: u64) -> bool {
    forage_patches_on(id) && !experiment(id, "oldpatches")
}
/// epoch_pressure.stone_span in minutes (0: the timer); stone22, stone30, nostone
pub fn stone_span(id: u64) -> f32 {
    if experiment(id, "stone22") {
        return 22.0;
    }
    if experiment(id, "stone30") {
        return 30.0;
    }
    if experiment(id, "nostone") {
        0.0
    } else {
        rule(id).stone_span
    }
}
/// economy.demand_split; TEST ONLY: EE_AI_DEMAND overrides it
pub fn demand_split_on(id: u64) -> bool {
    init();
    let demand = switches::ee_ai_switch_demand(id);
    if demand < 0 {
        rule(id).demand_split
    } else {
        demand != 0
    }
}

/// `EECPCheats`: the game's difficulty's cheats (gameoptions.yaml ai_cheats) narrowed by EE_AI_CHEATS (player
/// `id`'s set; 0: no player, player 1's).
pub fn cheats(id: u64) -> c_long {
    init();
    let mask = switches::ee_ai_switch_cheats(if id == 0 { 1 } else { id });
    // SAFETY: the lobby's options
    let difficulty = unsafe { ee_ai_host_difficulty() } as usize;
    let list = &crate::tech::civ::rules().game_options.difficulties;
    list.get(difficulty).map_or(0, |d| d.ai_cheats as c_long) & mask
}

/// `EECPCheatOn`: whether that cheat stays on
pub fn cheat_on(id: u64, cheat: c_long) -> bool {
    cheats(id) & cheat != 0
}

/// `EECPCheatNames`: the cheats that are on ("cost,reveal,..." or "none")
pub fn cheat_names(id: u64) -> String {
    switches::cheat_names(cheats(id))
}

/// `EECPCheatNames` for the C++: the text, valid until the next call (the C++'s static buffer).
#[no_mangle]
pub extern "C" fn ee_ai_cheat_names_text(id: u64) -> *const c_char {
    static TEXT: std::sync::Mutex<Option<std::ffi::CString>> = std::sync::Mutex::new(None);
    let text = std::ffi::CString::new(cheat_names(id)).unwrap_or_default();
    let mut slot = TEXT.lock().unwrap_or_else(|e| e.into_inner());
    slot.insert(text).as_ptr()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rules(n: usize, assign: Vec<i32>) -> AIPersonalityRules {
        AIPersonalityRules {
            personalities: (0..n)
                .map(|k| AIPersonalityRule { id: format!("p{k}").into_bytes(), ..AIPersonalityRule::default() })
                .collect(),
            assign,
        }
    }

    #[test]
    fn players_take_the_assignment_in_turn() {
        let r = rules(3, vec![2, 0]);
        assert_eq!(for_player(&r, 1).id, b"p2");
        assert_eq!(for_player(&r, 2).id, b"p0");
        assert_eq!(for_player(&r, 3).id, b"p2");
        // player 0 takes the first entry; a bad position the first personality
        assert_eq!(for_player(&r, 0).id, b"p2");
        assert_eq!(for_player(&rules(2, vec![5]), 4).id, b"p0");
        assert_eq!(for_player(&rules(2, vec![]), 4).id, b"p0");
        assert_eq!(for_player(&rules(0, vec![]), 4).id, b"balanced");
        assert_eq!(position(&r, 2), Some(0));
        assert_eq!(position(&rules(0, vec![]), 2), None);
    }

    #[test]
    fn names_find_their_position() {
        let r = rules(3, vec![0]);
        assert_eq!(find(&r, "p1"), 1);
        assert_eq!(find(&r, "nope"), -1);
    }

    #[test]
    fn the_switches_follow_the_default_rules_without_experiments() {
        // the defaults (no EE_AI_EXP in a test run): each switch is its rule
        let p = rule(2);
        assert_eq!(militia_on(2), p.militia);
        assert_eq!(stone_peace_on(2), !p.stone_attacks);
        assert_eq!(prehistoric_peace_on(2), !p.prehistoric_attacks);
        assert_eq!(claim_range_on(2), forage_patches_on(2));
        assert_eq!(stone_span(2), p.stone_span);
        assert_eq!(early_trainers(2), p.early_trainers as c_long);
        assert!(!experiment(2, "nomilitia"));
        assert_eq!(cheats(2) & !switches::CHEAT_ALL, 0);
    }
}
