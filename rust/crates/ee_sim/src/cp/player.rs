//! `EEComputerPlayer`'s plain bodies over its record: the per-difficulty scales (`mDifficulty` 0 easy, 1 medium,
//! 2 hard), their scenario setters, `ScaleCost`, `SetNextAttackTime` and the field part of `RestoreManagers`.
//!
//! The arithmetic is the C++'s in f32 (`long * 0.01f`), the roundings `UMFloatToLong` (`lrintf`), the times and the
//! minutes wrapping 32-bit words.

#![allow(non_snake_case)]
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use ee_base::random::float_to_long;

use super::fields::EeCpFields;

pub const EASY: c_ulong = 0;
pub const MEDIUM: c_ulong = 1;
pub const HARD: c_ulong = 2;

impl EeCpFields {
    /// `GetAttackDelayModifier` 0x2eafc: easy or hard's percent / 100, else 1.
    pub fn attack_delay_modifier(&self) -> f32 {
        match self.mDifficulty {
            EASY => self.mAttackDelayModifierEasy as f32 * 0.01,
            HARD => self.mAttackDelayModifierHard as f32 * 0.01,
            _ => 1.0,
        }
    }
    /// `GetAttackFrequencyModifier` 0x2eb26.
    pub fn attack_frequency_modifier(&self) -> f32 {
        match self.mDifficulty {
            EASY => self.mAttackFrequencyModifierEasy as f32 * 0.01,
            HARD => self.mAttackFrequencyModifierHard as f32 * 0.01,
            _ => 1.0,
        }
    }
    /// `GetUnitCostModifier` 0x2eb7c: the difficulty's percent / 100 (medium's a float).
    pub fn unit_cost_modifier(&self) -> f32 {
        match self.mDifficulty {
            EASY => self.mUnitCostReductionEasy as f32 * 0.01,
            HARD => self.mUnitCostReductionHard as f32 * 0.01,
            _ => self.mUnitCostMedium * 0.01,
        }
    }
    /// `GetDMAttackFrequencyModifier` 0x2eba8: the difficulty's, else 1.
    pub fn dm_attack_frequency_modifier(&self) -> f32 {
        match self.mDifficulty {
            EASY => self.mDMAttackFrequencyModifierEasy,
            MEDIUM => self.mDMAttackFrequencyModifierMedium,
            HARD => self.mDMAttackFrequencyModifierHard,
            _ => 1.0,
        }
    }
    /// `GetDMInitialAttackModifier` 0x2ebd9.
    pub fn dm_initial_attack_modifier(&self) -> f32 {
        match self.mDifficulty {
            EASY => self.mDMInitialAttackModifierEasy,
            MEDIUM => self.mDMInitialAttackModifierMedium,
            HARD => self.mDMInitialAttackModifierHard,
            _ => 1.0,
        }
    }
    /// `GetPopLimitAdjustment` 0x2ec74: the difficulty's percent / 100 (medium's for any other).
    pub fn pop_limit_adjustment(&self) -> f32 {
        match self.mDifficulty {
            EASY => self.mPopLimitAdjustmentEasy as f32 * 0.01,
            HARD => self.mPopLimitAdjustmentHard as f32 * 0.01,
            _ => self.mPopLimitAdjustmentMedium as f32 * 0.01,
        }
    }
    /// `GetIslandExpansionTimer` 0x2ed01: the difficulty's, else 60 s.
    pub fn island_expansion_timer(&self) -> c_ulong {
        match self.mDifficulty {
            EASY => self.mIslandExpansionTimerEasy as c_ulong,
            MEDIUM => self.mIslandExpansionTimerMedium as c_ulong,
            HARD => self.mIslandExpansionTimerHard as c_ulong,
            _ => 60000,
        }
    }
    /// `ScaleCost` 0x2eace: scaled by the unit cost modifier while cheating, never below 0.
    pub fn scale_cost(&self, cost: c_long) -> c_long {
        let mut r = cost;
        if self.mIsCheating {
            r = float_to_long(self.unit_cost_modifier() * cost as f32) as c_long;
        }
        r.max(0)
    }
    /// `SetDMAttackFrequencyModifier` 0x3ec9d: the difficulty's slot.
    pub fn set_dm_attack_frequency_modifier(&mut self, v: f32) {
        match self.mDifficulty {
            EASY => self.mDMAttackFrequencyModifierEasy = v,
            MEDIUM => self.mDMAttackFrequencyModifierMedium = v,
            HARD => self.mDMAttackFrequencyModifierHard = v,
            _ => {}
        }
    }
    /// `SetDMInitialAttackModifier` 0x3ecd6.
    pub fn set_dm_initial_attack_modifier(&mut self, v: f32) {
        match self.mDifficulty {
            EASY => self.mDMInitialAttackModifierEasy = v,
            MEDIUM => self.mDMInitialAttackModifierMedium = v,
            HARD => self.mDMInitialAttackModifierHard = v,
            _ => {}
        }
    }
    /// `SetIslandExpansionTimer` 0x3ed0f.
    pub fn set_island_expansion_timer(&mut self, v: c_ulong) {
        match self.mDifficulty {
            EASY => self.mIslandExpansionTimerEasy = v as c_long,
            MEDIUM => self.mIslandExpansionTimerMedium = v as c_long,
            HARD => self.mIslandExpansionTimerHard = v as c_long,
            _ => {}
        }
    }

    /// `SetNextAttackTime` 0x3d4cd: a random number of minutes (`random(min, max)`, the game's generator: the
    /// initial delay before the first attack, the frequency after), plus the epoch span's share, scaled by the
    /// deathmatch or difficulty modifier, clamped to the bounds, added to `time`; the later ones mark the attack
    /// started.
    pub fn set_next_attack_time(
        &mut self,
        time: c_ulong,
        epochs: c_long,
        deathmatch: bool,
        random: &mut dyn FnMut(c_long, c_long) -> c_long,
    ) {
        let (mut minutes, low, high);
        if !self.mIsStarted {
            minutes = random(self.mInitialAttackDelayMin, self.mInitialAttackDelayMax) as i32;
            minutes = minutes.wrapping_add((self.mInitialAttackDelayPerEpoch as i32).wrapping_mul(epochs as i32));
            let scale = if deathmatch { self.dm_initial_attack_modifier() } else { self.attack_delay_modifier() };
            minutes = float_to_long(scale * minutes as f32);
            low = self.mAttackDelayFloor as i32;
            high = self.mAttackDelayCeiling as i32;
        } else {
            minutes = random(self.mAttackFrequencyMin, self.mAttackFrequencyMax) as i32;
            minutes = minutes.wrapping_add(float_to_long(self.mAttackFrequencyPerEpoch * epochs as f32));
            let scale = if deathmatch { self.dm_attack_frequency_modifier() } else { self.attack_frequency_modifier() };
            minutes = float_to_long(scale * minutes as f32);
            low = self.mAttackFrequencyFloor as i32;
            high = self.mAttackFrequencyCeiling as i32;
        }
        if minutes < low {
            minutes = low;
        }
        if minutes > high {
            minutes = high;
        }
        self.mNextAttackTime = (minutes.wrapping_mul(60000) as u32).wrapping_add(time as u32) as c_ulong;
        if self.mIsStarted {
            self.mAttackStarted = true;
        }
    }

    /// `RestoreManagers`' rebase of the saved relative times to the game clock (after the placement's restore,
    /// `rebase_expansion` once the centres' restores succeeded too).
    pub fn rebase_attack_time(&mut self, game_time: c_ulong) {
        self.mNextAttackTime = (self.mNextAttackTime as u32).wrapping_add(game_time as u32) as c_ulong;
    }
    pub fn rebase_expansion_time(&mut self, game_time: c_ulong) {
        self.mNextExpansionTime = (self.mNextExpansionTime as u32).wrapping_add(game_time as u32) as c_ulong;
    }

    /// `RestoreManagers`' fixes after the restores (`ok`: they succeeded): the attack bounds in order, an old
    /// save's squared drop-off distance (above 25) to its root, a regroup asked for.
    pub fn after_restore(&mut self, ok: bool) {
        self.mAttackFrequencyMin = self.mAttackFrequencyMin.min(self.mAttackFrequencyMax);
        self.mAttackFrequencyMax = self.mAttackFrequencyMin.max(self.mAttackFrequencyMax);
        self.mAttackFrequencyFloor = self.mAttackFrequencyFloor.min(self.mAttackFrequencyCeiling);
        self.mAttackFrequencyCeiling = self.mAttackFrequencyFloor.max(self.mAttackFrequencyCeiling);
        if ok && self.mDropOffDistance > 25 {
            self.mDropOffDistance = float_to_long((self.mDropOffDistance as f64).sqrt() as f32) as c_long;
        }
        self.mRegroupPending = true;
    }

    /// `RestoreManagers`' last step: a save before 0.229 had one expansion timer (never reached: `Load` refuses it).
    pub fn old_expansion_timers(&mut self, major: u32, minor: u32) {
        if major == 0 && minor < 0xe5 {
            self.mExpansionTimerMedium = self.mExpansionTimerEasy;
            self.mExpansionTimerHard = self.mExpansionTimerEasy;
        }
    }
}

#[cfg(test)]
#[path = "player_tests.rs"]
mod tests;
