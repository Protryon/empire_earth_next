//! TEST ONLY: the capture's PLST record, a player's state after its tick (was EEPlayer.cpp's `EEPlayerCaptureState`;
//! tools/sim_capture.py): the player's id and the game time, FNV-1a digests of its statistics, events, queued
//! improvements and plain members, then its resources and counts. Read through the world views and the Rust objects
//! the player holds (the statistics, the events, the queue).

use std::os::raw::{c_long, c_ulong};

use crate::app::capture;
use crate::units::player::ee_units_queued_digest;
use crate::units::player_events::{ee_units_events_digest, EeUnitsPlayerEvents};
use crate::units::player_stats::EeUnitsPlayerStats;
use crate::world::view::PlayerView;
use crate::world::WorldRead;
use crate::world_field;

/// The FNV-1a of the C++ digest (`EEPlayerDigest`).
pub struct Digest(pub u64);

impl Default for Digest {
    fn default() -> Self {
        Digest(0xcbf29ce484222325)
    }
}

impl Digest {
    pub fn u32(&mut self, v: u32) {
        for b in v.to_le_bytes() {
            self.0 = (self.0 ^ b as u64).wrapping_mul(0x100000001b3);
        }
    }
    pub fn f32(&mut self, v: f32) {
        self.u32(v.to_bits());
    }
}

/// The statistics' digest: the end time, then every series but the local player's reports (0x14, 0x15, kept by the
/// wall clock): its maximum, history flag, point count and points.
pub fn statistics_digest(stats: &EeUnitsPlayerStats, end_time: c_ulong) -> u64 {
    let mut d = Digest::default();
    d.u32(end_time as u32);
    for (s, series) in stats.series.iter().enumerate().take(31) {
        if s == 0x14 || s == 0x15 {
            continue;
        }
        d.u32(series.max as u32);
        d.u32(series.keep as u32);
        d.u32(series.points.len() as u32);
        for p in &series.points {
            d.u32(p.value as u32);
            d.u32(p.time as u32);
        }
    }
    d.0
}

fn put(out: &mut Vec<u8>, v: u32) {
    out.extend_from_slice(&v.to_le_bytes());
}

fn put_hash(out: &mut Vec<u8>, h: u64) {
    put(out, h as u32);
    put(out, (h >> 32) as u32);
}

/// The plain members' digest (the C++'s order).
fn fields_digest(p: &PlayerView) -> u64 {
    let f = p.fields();
    let mut d = Digest::default();
    let hack: c_ulong = world_field!(f, mHackUnitClass);
    d.u32(hack as u32);
    d.f32(world_field!(f, mConversionFactor));
    d.f32(world_field!(f, mTributeFee));
    let starting: c_long = world_field!(f, mMaxStartingCitizens);
    d.u32(starting as u32);
    let damage: [c_long; 17] = world_field!(f, mDamageFrom);
    for v in damage {
        d.u32(v as u32);
    }
    let longs: [c_long; 9] = [
        world_field!(f, mNumAttacks),
        world_field!(f, mHeroLimit),
        world_field!(f, mMiningLimit),
        world_field!(f, mLAF0),
        world_field!(f, mShareBonus),
        world_field!(f, mPopulationLimitBonus),
        world_field!(f, mNumHeroes),
        world_field!(f, mNumHeroesInProduction),
        world_field!(f, mNumTeleporters),
    ];
    for v in longs {
        d.u32(v as u32);
    }
    let bools: [bool; 9] = [
        world_field!(f, mAreaConversion),
        world_field!(f, mBB42),
        world_field!(f, mBB44),
        world_field!(f, mInstantBuild),
        world_field!(f, mHealsUnits),
        world_field!(f, mBehindFirewall),
        world_field!(f, mEarthquakeShaken),
        world_field!(f, mShiftMode),
        world_field!(f, mJITManufacturing),
    ];
    for v in bools {
        d.u32(v as u32);
    }
    d.0
}

/// PLST for the player `player` (an `EEPlayer*`): `game_time` the physics' game time (0 without physics), `stats`
/// and `end_time` its statistics (`mSavedGroups`), `events` its event list's Rust object.
///
/// # Safety
/// The caller holds the player (its physics tick); the pointers are its live members.
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_player_state(
    player: u64,
    game_time: u32,
    stats: *const EeUnitsPlayerStats,
    end_time: c_ulong,
    events: *const EeUnitsPlayerEvents,
) {
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller holds the player
    let token = unsafe { WorldRead::begin() };
    let Some(p) = (unsafe { PlayerView::of(&token, player) }) else { return };
    let mut r = Vec::new();
    let id: c_ulong = world_field!(p.rts(), mPlayerID);
    put(&mut r, id as u32);
    put(&mut r, game_time);
    // SAFETY (each): the player's live Rust objects
    put_hash(&mut r, statistics_digest(unsafe { &*stats }, end_time));
    put_hash(&mut r, unsafe { ee_units_events_digest(events) });
    let queued = world_field!(p.fields(), mQueued);
    put_hash(&mut r, unsafe { ee_units_queued_digest(queued) });
    put_hash(&mut r, fields_digest(&p));
    let s = p.saved();
    let caps: [c_long; 7] = world_field!(s, mResourceCaps);
    for v in &caps[2..7] {
        put(&mut r, *v as u32);
    }
    let counts: [c_long; 5] = [
        world_field!(s, mPopulationCount),
        world_field!(s, mEndOfGameCount),
        world_field!(s, mNumVitalBuildings),
        world_field!(s, mNumCapitols),
        world_field!(s, mPopLimit),
    ];
    for v in counts {
        put(&mut r, v as u32);
    }
    capture::commit(u32::from_le_bytes(*b"PLST"), &r);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn digests() {
        let mut d = Digest::default();
        d.u32(0);
        let mut e = Digest(0xcbf29ce484222325);
        for _ in 0..4 {
            e.0 = e.0.wrapping_mul(0x100000001b3);
        }
        assert_eq!(d.0, e.0);
        let stats = EeUnitsPlayerStats::default();
        let a = statistics_digest(&stats, 0);
        assert_ne!(a, statistics_digest(&stats, 1));
        let mut other = stats.clone();
        other.series[0x14].max = 99;
        assert_eq!(a, statistics_digest(&other, 0));
        other.series[0].max = 99;
        assert_ne!(a, statistics_digest(&other, 0));
    }
}
