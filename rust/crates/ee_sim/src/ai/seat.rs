//! The seat choice (`EECPEngine.cpp`; ee/EECPEngine.h): the lobby's choices (0 classic, 1 random, 2 + k the
//! manifest's entry k), a choice by name, EE_AI_ENGINE's list (TEST ONLY) and the resolve of random at the game's
//! start from the game's seed and the slot (32-bit arithmetic: the same on every peer and system).

/// The lobby's choices.
pub(crate) const CHOICE_CLASSIC: u64 = 0;
pub(crate) const CHOICE_RANDOM: u64 = 1;
pub(crate) const CHOICE_FIRST_AI: u64 = 2;

/// `EECPChoiceByName`: classic, random, next (the first AI, -1 when there is none) or an id of `ids` -> the choice,
/// -1 for none.
pub fn choice_by_name(name: &[u8], ids: &[&[u8]]) -> i64 {
    match name {
        b"classic" => CHOICE_CLASSIC as i64,
        b"random" => CHOICE_RANDOM as i64,
        b"next" => {
            if ids.is_empty() {
                -1
            } else {
                CHOICE_FIRST_AI as i64
            }
        }
        _ => ids.iter().position(|id| *id == name).map_or(-1, |k| CHOICE_FIRST_AI as i64 + k as i64),
    }
}

/// EE_AI_ENGINE's value as `EECPEngineOverrides` reads it: comma-separated names in turn -> the choices, and the
/// names that are none (each logged by the C++ as "EE_AI_ENGINE: unknown engine '<name>' ignored").
pub fn overrides(value: &[u8], ids: &[&[u8]]) -> (Vec<i64>, Vec<Vec<u8>>) {
    let mut choices = Vec::new();
    let mut unknown = Vec::new();
    if value.is_empty() {
        return (choices, unknown);
    }
    for name in value.split(|&c| c == b',') {
        let c = choice_by_name(name, ids);
        if c >= 0 {
            choices.push(c);
        } else {
            unknown.push(name.to_vec());
        }
    }
    (choices, unknown)
}

/// `EECPEngineResolve`: the choice a computer player of lobby slot `slot` plays with. `overrides` (EE_AI_ENGINE's,
/// empty: none) replaces the lobby's choice by player id from player 1; random picks uniformly among classic and the
/// AIs whose `random` flag is set, from a hash of the seed and the slot; a choice that names no AI is classic.
/// `random[k]` is entry k's flag (`random.len()` the manifest's size).
pub fn resolve(choice: u64, seed: u64, slot: u64, player_id: u64, overrides: &[i64], random: &[bool]) -> u64 {
    let mut choice = choice;
    if !overrides.is_empty() {
        let at = if player_id > 0 { player_id - 1 } else { 0 };
        choice = overrides[(at % overrides.len() as u64) as usize] as u64;
    }
    if choice == CHOICE_RANDOM {
        let mut pool = vec![CHOICE_CLASSIC];
        pool.extend(random.iter().enumerate().filter(|(_, &r)| r).map(|(k, _)| CHOICE_FIRST_AI + k as u64));
        let mut h = (seed as u32).wrapping_mul(2654435761) ^ (slot as u32).wrapping_add(1).wrapping_mul(0x9e3779b9);
        h ^= h >> 15;
        h = h.wrapping_mul(0x2c1b3c6d);
        h ^= h >> 12;
        return pool[((h >> 7) % pool.len() as u32) as usize];
    }
    if choice >= CHOICE_FIRST_AI && choice - CHOICE_FIRST_AI < random.len() as u64 {
        choice
    } else {
        CHOICE_CLASSIC
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const IDS: [&[u8]; 3] = [b"boom", b"rush", b"classic_lua"];

    #[test]
    fn names() {
        assert_eq!(choice_by_name(b"classic", &IDS), 0);
        assert_eq!(choice_by_name(b"random", &IDS), 1);
        assert_eq!(choice_by_name(b"next", &IDS), 2);
        assert_eq!(choice_by_name(b"next", &[]), -1);
        assert_eq!(choice_by_name(b"classic_lua", &IDS), 4);
        assert_eq!(choice_by_name(b"", &IDS), -1);
        let (c, u) = overrides(b"rush,,x,random", &IDS);
        assert_eq!(c, vec![3, 1]);
        assert_eq!(u, vec![b"".to_vec(), b"x".to_vec()]);
        assert_eq!(overrides(b"", &IDS), (vec![], vec![]));
    }

    #[test]
    fn resolve_picks() {
        let random = [true, false, false];
        assert_eq!(resolve(3, 1, 0, 2, &[], &random), 3);
        assert_eq!(resolve(9, 1, 0, 2, &[], &random), 0);
        assert_eq!(resolve(0, 1, 0, 2, &[4, 3], &random), 3);
        assert_eq!(resolve(0, 1, 0, 1, &[4, 3], &random), 4);
        // random: only classic and boom; both come up over the slots
        let picks: Vec<u64> = (0..16).map(|s| resolve(1, 12345, s, 2, &[], &random)).collect();
        assert!(picks.iter().all(|&p| p == 0 || p == 2));
        assert!(picks.contains(&0) && picks.contains(&2));
        assert!((0..16).all(|s| resolve(1, 7, s, 2, &[], &[]) == 0));
    }
}
