//! The event list's compound host operations as one-statement C++ operations (the world lane, round 2: were
//! EEPPlayerEventList.cpp's `Host` cases and its constructor's and destructor's loops): the sight at an event and the
//! local view of it (whether the local player sees the player's sight is read through the world views), the alert
//! sound's play and the reference it keeps, the mini map's ping. The bodies (player_events.rs and its tests) keep
//! asking `EE_UNITS_EVENTS_SHOW` and the like; in the game [`split`] makes each the C++ statements it was, in order.

use crate::units::player_events::{
    EeUnitsPlayerEvent, EE_UNITS_EVENTS_HIDE, EE_UNITS_EVENTS_PING, EE_UNITS_EVENTS_SHOW, EE_UNITS_EVENTS_SOUND,
};

// ---- the one-statement operations (EEPPlayerEventList::Host's cases; the event's x/y the tile, kind the sound)
/// `AddSharedSightAt(tile)`, `RemoveSharedSightAt(tile)`
pub const EE_UNITS_EVENTS_X_SIGHT_ON: i32 = 20;
pub const EE_UNITS_EVENTS_X_SIGHT_OFF: i32 = 21;
/// `gMiniMap->CountCellOn(x, y)`, `CountCellOff`
pub const EE_UNITS_EVENTS_X_CELL_ON: i32 = 22;
pub const EE_UNITS_EVENTS_X_CELL_OFF: i32 = 23;
/// the terrain mesh's `ReferenceTile(x, y)`, `DereferenceTile`
pub const EE_UNITS_EVENTS_X_TILE_ON: i32 = 24;
pub const EE_UNITS_EVENTS_X_TILE_OFF: i32 = 25;
/// `mSounds[kind - 1] != NULL`; its sample played into `mPlayed` (`PlayAndReturnReferencedNode`)
pub const EE_UNITS_EVENTS_X_HAS_SOUND: i32 = 26;
pub const EE_UNITS_EVENTS_X_PLAY: i32 = 27;
/// `mSound != NULL`, `mSound->Dereference()`, `mSound = mPlayed`, `mPlayed != NULL`, `mPlayed->Reference()`
pub const EE_UNITS_EVENTS_X_HAS_CURRENT: i32 = 28;
pub const EE_UNITS_EVENTS_X_CURRENT_RELEASE: i32 = 29;
pub const EE_UNITS_EVENTS_X_CURRENT_SET: i32 = 30;
pub const EE_UNITS_EVENTS_X_HAS_PLAYED: i32 = 31;
pub const EE_UNITS_EVENTS_X_PLAYED_REFERENCE: i32 = 32;
/// the mini map's ping location (the event's source) and its flag on
pub const EE_UNITS_EVENTS_X_PING_X: i32 = 33;
pub const EE_UNITS_EVENTS_X_PING_Y: i32 = 34;
pub const EE_UNITS_EVENTS_X_PING_ON: i32 = 35;
/// the list's alert sound `kind - 1` (`mSounds[kind - 1] = EESoundObject::Get(kSounds[kind - 1])`), loaded,
/// unloaded (the constructor's and destructor's)
pub const EE_UNITS_EVENTS_X_SOUND_GET: i32 = 36;
pub const EE_UNITS_EVENTS_X_SOUND_LOAD: i32 = 37;
pub const EE_UNITS_EVENTS_X_SOUND_UNLOAD: i32 = 38;

/// The list's alert sounds (under attack, garrison, flare, two more).
pub const EE_UNITS_EVENTS_SOUNDS: i64 = 5;

/// One C++ operation with the event's values.
pub type Call<'a> = &'a mut dyn FnMut(i32, &EeUnitsPlayerEvent) -> i32;

fn with_kind(event: &EeUnitsPlayerEvent, kind: i64) -> EeUnitsPlayerEvent {
    let mut e = *event;
    e.kind = kind as _;
    e
}

/// The compound operation `op` on `event` (None: not one of these); `shared` whether the local player sees the
/// player's sight (it, or an ally sharing the line of sight both ways).
pub fn split(op: i32, event: &EeUnitsPlayerEvent, f: Call, shared: &mut dyn FnMut() -> bool) -> Option<i32> {
    match op {
        EE_UNITS_EVENTS_SHOW | EE_UNITS_EVENTS_HIDE => {
            let show = op == EE_UNITS_EVENTS_SHOW;
            let shared = shared();
            f(if show { EE_UNITS_EVENTS_X_SIGHT_ON } else { EE_UNITS_EVENTS_X_SIGHT_OFF }, event);
            if shared {
                f(if show { EE_UNITS_EVENTS_X_CELL_ON } else { EE_UNITS_EVENTS_X_CELL_OFF }, event);
                f(if show { EE_UNITS_EVENTS_X_TILE_ON } else { EE_UNITS_EVENTS_X_TILE_OFF }, event);
            }
            Some(0)
        }
        EE_UNITS_EVENTS_SOUND => {
            if f(EE_UNITS_EVENTS_X_HAS_SOUND, event) == 0 {
                return Some(0);
            }
            f(EE_UNITS_EVENTS_X_PLAY, event);
            if f(EE_UNITS_EVENTS_X_HAS_CURRENT, event) != 0 {
                f(EE_UNITS_EVENTS_X_CURRENT_RELEASE, event);
            }
            f(EE_UNITS_EVENTS_X_CURRENT_SET, event);
            if f(EE_UNITS_EVENTS_X_HAS_PLAYED, event) != 0 {
                f(EE_UNITS_EVENTS_X_PLAYED_REFERENCE, event);
            }
            if f(EE_UNITS_EVENTS_X_HAS_CURRENT, event) != 0 {
                f(EE_UNITS_EVENTS_X_CURRENT_RELEASE, event);
            }
            Some(0)
        }
        EE_UNITS_EVENTS_PING => {
            f(EE_UNITS_EVENTS_X_PING_X, event);
            f(EE_UNITS_EVENTS_X_PING_Y, event);
            f(EE_UNITS_EVENTS_X_PING_ON, event);
            Some(0)
        }
        _ => None,
    }
}

/// The constructor's sounds: each got and, when there is one, loaded.
pub fn load_sounds(f: Call) {
    let none = EeUnitsPlayerEvent::default();
    for i in 1..=EE_UNITS_EVENTS_SOUNDS {
        let e = with_kind(&none, i);
        f(EE_UNITS_EVENTS_X_SOUND_GET, &e);
        if f(EE_UNITS_EVENTS_X_HAS_SOUND, &e) != 0 {
            f(EE_UNITS_EVENTS_X_SOUND_LOAD, &e);
        }
    }
}

/// The destructor's: each sound there unloaded, then the one playing released.
pub fn unload_sounds(f: Call) {
    let none = EeUnitsPlayerEvent::default();
    for i in 1..=EE_UNITS_EVENTS_SOUNDS {
        let e = with_kind(&none, i);
        if f(EE_UNITS_EVENTS_X_HAS_SOUND, &e) != 0 {
            f(EE_UNITS_EVENTS_X_SOUND_UNLOAD, &e);
        }
    }
    if f(EE_UNITS_EVENTS_X_HAS_CURRENT, &none) != 0 {
        f(EE_UNITS_EVENTS_X_CURRENT_RELEASE, &none);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(op: i32, answers: &[(i32, i32)], shared: bool) -> Vec<i32> {
        let mut log = Vec::new();
        let mut queue = answers.to_vec();
        let mut f = |op: i32, _: &EeUnitsPlayerEvent| {
            log.push(op);
            queue.iter().position(|&(o, _)| o == op).map_or(0, |k| queue.remove(k).1)
        };
        assert_eq!(split(op, &EeUnitsPlayerEvent::default(), &mut f, &mut || shared), Some(0));
        log
    }

    #[test]
    fn the_sight_is_counted_locally_only_when_shared() {
        assert_eq!(run(EE_UNITS_EVENTS_SHOW, &[], false), vec![EE_UNITS_EVENTS_X_SIGHT_ON]);
        assert_eq!(
            run(EE_UNITS_EVENTS_HIDE, &[], true),
            vec![EE_UNITS_EVENTS_X_SIGHT_OFF, EE_UNITS_EVENTS_X_CELL_OFF, EE_UNITS_EVENTS_X_TILE_OFF]
        );
    }

    #[test]
    fn a_sound_replaces_the_one_playing_and_keeps_no_reference() {
        assert_eq!(run(EE_UNITS_EVENTS_SOUND, &[], false), vec![EE_UNITS_EVENTS_X_HAS_SOUND]);
        let log = run(
            EE_UNITS_EVENTS_SOUND,
            &[
                (EE_UNITS_EVENTS_X_HAS_SOUND, 1),
                (EE_UNITS_EVENTS_X_HAS_CURRENT, 1),
                (EE_UNITS_EVENTS_X_HAS_PLAYED, 1),
                (EE_UNITS_EVENTS_X_HAS_CURRENT, 1),
            ],
            false,
        );
        assert_eq!(
            log,
            vec![
                EE_UNITS_EVENTS_X_HAS_SOUND,
                EE_UNITS_EVENTS_X_PLAY,
                EE_UNITS_EVENTS_X_HAS_CURRENT,
                EE_UNITS_EVENTS_X_CURRENT_RELEASE,
                EE_UNITS_EVENTS_X_CURRENT_SET,
                EE_UNITS_EVENTS_X_HAS_PLAYED,
                EE_UNITS_EVENTS_X_PLAYED_REFERENCE,
                EE_UNITS_EVENTS_X_HAS_CURRENT,
                EE_UNITS_EVENTS_X_CURRENT_RELEASE,
            ]
        );
        assert_eq!(run(EE_UNITS_EVENTS_PING, &[], false).len(), 3);
        let mut e = EeUnitsPlayerEvent::default();
        assert_eq!(split(3, &e, &mut |_, _| 0, &mut || false), None);
        e.kind = 2;
        assert_eq!(with_kind(&e, 4).kind, 4);
    }

    #[test]
    fn the_sounds_load_and_unload_by_kind() {
        let mut kinds = Vec::new();
        load_sounds(&mut |op, e| {
            if op == EE_UNITS_EVENTS_X_SOUND_LOAD {
                kinds.push(e.kind as i64);
            }
            i32::from(e.kind != 3)
        });
        assert_eq!(kinds, vec![1, 2, 4, 5]);
        let mut log = Vec::new();
        unload_sounds(&mut |op, _| {
            log.push(op);
            1
        });
        assert_eq!(log.len(), 5 * 2 + 2);
    }
}
