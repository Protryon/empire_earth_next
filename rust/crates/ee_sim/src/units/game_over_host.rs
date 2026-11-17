//! `EEWorld::CheckGameOver`'s world in the game (game_over.rs): the players and their units through the world views,
//! the rest through `ee_units_game_over_host` (EEWorld.cpp). Absent from a test binary.

#![cfg(not(test))]

use crate::units::game_over::{check_game_over, GameOverHost, RecoverUnit};
use crate::units::player_views::land_building;
use crate::world::{PlayerView, UnitView, WorldRead, WorldView};
use std::os::raw::c_long;

/// the game options' `mIsScenarioGame`
pub const EE_UNITS_GAME_OVER_SCENARIO: i32 = 1;
/// the rules' `mGameOptions.mDefeatWithoutRecovery`
pub const EE_UNITS_GAME_OVER_DEFEAT_RULE: i32 = 2;
/// `mTeams.size()`; team a's `GetVictoryState()`; its `mPlayers.size()`; its `mPlayers[b]`
pub const EE_UNITS_GAME_OVER_TEAMS: i32 = 3;
pub const EE_UNITS_GAME_OVER_TEAM_STATE: i32 = 4;
pub const EE_UNITS_GAME_OVER_TEAM_SIZE: i32 = 5;
pub const EE_UNITS_GAME_OVER_TEAM_PLAYER: i32 = 6;
/// `mPlayers[a]->IsHuman()`
pub const EE_UNITS_GAME_OVER_IS_HUMAN: i32 = 7;
/// `RemovePlayerFromGame(b != 0, a)`
pub const EE_UNITS_GAME_OVER_REMOVE: i32 = 8;
/// `mPlayers[a]->SetGameResult(b)`
pub const EE_UNITS_GAME_OVER_SET_RESULT: i32 = 9;
/// player a can train a citizen: no citizen node, or `EEPlayer::CanAfford(node, citizen, 1)`
pub const EE_UNITS_GAME_OVER_CAN_TRAIN: i32 = 10;

extern "C" {
    fn ee_units_game_over_host(op: i32, a: i64, b: i64) -> i64;
}

struct Game<'t> {
    world: WorldView<'t>,
}

impl<'t> Game<'t> {
    fn player(&self, i: u64) -> Option<PlayerView<'t>> {
        self.world.player(i as usize)
    }
    fn host(&mut self, op: i32, a: i64, b: i64) -> i64 {
        // SAFETY: the C++ switch over the world's live objects
        unsafe { ee_units_game_over_host(op, a, b) }
    }
}

impl GameOverHost for Game<'_> {
    fn victory_enabled(&mut self) -> bool {
        self.world.victory_enabled()
    }
    fn num_players(&mut self) -> u64 {
        self.world.num_players()
    }
    fn scenario(&mut self) -> bool {
        self.host(EE_UNITS_GAME_OVER_SCENARIO, 0, 0) != 0
    }
    fn defeat_without_recovery(&mut self) -> bool {
        self.host(EE_UNITS_GAME_OVER_DEFEAT_RULE, 0, 0) != 0
    }
    fn teams(&mut self) -> usize {
        self.host(EE_UNITS_GAME_OVER_TEAMS, 0, 0) as usize
    }
    fn team_state(&mut self, team: usize) -> i64 {
        self.host(EE_UNITS_GAME_OVER_TEAM_STATE, team as i64, 0)
    }
    fn team_players(&mut self, team: usize) -> Vec<u64> {
        let n = self.host(EE_UNITS_GAME_OVER_TEAM_SIZE, team as i64, 0);
        (0..n).map(|j| self.host(EE_UNITS_GAME_OVER_TEAM_PLAYER, team as i64, j) as u64).collect()
    }
    fn exists(&mut self, player: u64) -> bool {
        self.player(player).is_some()
    }
    fn observer(&mut self, player: u64) -> bool {
        self.player(player).is_some_and(|p| p.observer())
    }
    fn end_of_game_count(&mut self, player: u64) -> i64 {
        self.player(player).map_or(0, |p| {
            let v: c_long = crate::world_field!(p.saved(), mEndOfGameCount);
            v as i64
        })
    }
    fn game_result(&mut self, player: u64) -> u64 {
        self.player(player).map_or(0, |p| {
            let v: std::os::raw::c_ulong = crate::world_field!(p.saved(), mGameResult);
            v as u64
        })
    }
    fn is_human(&mut self, player: u64) -> bool {
        self.host(EE_UNITS_GAME_OVER_IS_HUMAN, player as i64, 0) != 0
    }
    fn remove(&mut self, won: bool, player: u64) {
        self.host(EE_UNITS_GAME_OVER_REMOVE, player as i64, won as i64);
    }
    fn set_result(&mut self, player: u64, result: i64) {
        self.host(EE_UNITS_GAME_OVER_SET_RESULT, player as i64, result);
    }
    fn can_train(&mut self, player: u64) -> bool {
        self.host(EE_UNITS_GAME_OVER_CAN_TRAIN, player as i64, 0) != 0
    }
    fn complex_units(&mut self, player: u64) -> Vec<u64> {
        // `GetNextComplexUnit`'s walk: each unit's complex unit, in list order
        self.player(player).map_or_else(Vec::new, |p| {
            p.units().map(|u| u.complex_unit_handle()).filter(|&x| x != 0).collect()
        })
    }
    fn unit(&mut self, unit: u64) -> RecoverUnit {
        // SAFETY: one of the player's live complex units (listed just above; nothing ran in between but reads)
        let token = unsafe { WorldRead::begin() };
        // SAFETY: as above
        let Some(u) = (unsafe { UnitView::of(&token, unit) }) else { return RecoverUnit::default() };
        let class = u.class();
        RecoverUnit {
            hit_points: u.hit_points(),
            first_part: u.previous_part_handle() == 0,
            vital: class.is_some_and(|c| c.vital()),
            naval_citizen: class.is_some_and(|c| c.category() == 0x16),
            land_building: class.is_some_and(|c| land_building(&c)),
            unit_type: class.map_or(0, |c| c.unit_type()),
            built: u.is_built(),
        }
    }
}

/// `EEWorld::CheckGameOver` of the world `world` (`&gWorld`)
#[no_mangle]
pub extern "C" fn ee_units_check_game_over(world: u64) -> i64 {
    // SAFETY: the physics thread's end-of-game check holds the world as the C++ body did
    let token = unsafe { WorldRead::begin() };
    // SAFETY: `&gWorld`
    let Some(w) = (unsafe { WorldView::of(&token, world) }) else { return 0 };
    check_game_over(&mut Game { world: w })
}
