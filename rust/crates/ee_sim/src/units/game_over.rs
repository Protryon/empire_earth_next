//! `EEWorld::CheckGameOver` and `EEWorldCanRecover` (the object-storage plan's lane P): the end-of-game decision
//! over the players' and units' fields, in the C++'s order (its gotos as the flow below). The players are read
//! through a [`GameOverHost`] (the game's reads them through the world views, game_over_host in
//! tile_sight_host.rs); the calls that change the game (RemovePlayerFromGame, SetGameResult), the virtual
//! `IsHuman`, the teams' victory conditions, the rules and the citizen's `CanAfford` stay C++ host calls.

/// What the decision reads and calls (players by index into `gWorld.mPlayers`, units by address).
pub trait GameOverHost {
    /// `mVictoryEnabled`, `mNumPlayers`, the game options' `mIsScenarioGame`, the rules' `mDefeatWithoutRecovery`
    fn victory_enabled(&mut self) -> bool;
    fn num_players(&mut self) -> u64;
    fn scenario(&mut self) -> bool;
    fn defeat_without_recovery(&mut self) -> bool;
    /// `mTeams.size()`, team i's `GetVictoryState()`, its `mPlayers`
    fn teams(&mut self) -> usize;
    fn team_state(&mut self, team: usize) -> i64;
    fn team_players(&mut self, team: usize) -> Vec<u64>;
    /// whether `mPlayers[i]` is set; its `mObserverMode`, `mEndOfGameCount`, `mGameResult`, `IsHuman()`
    fn exists(&mut self, player: u64) -> bool;
    fn observer(&mut self, player: u64) -> bool;
    fn end_of_game_count(&mut self, player: u64) -> i64;
    fn game_result(&mut self, player: u64) -> u64;
    fn is_human(&mut self, player: u64) -> bool;
    /// `RemovePlayerFromGame(won, i)`, then `mPlayers[i]->SetGameResult(result)`
    fn remove(&mut self, won: bool, player: u64);
    fn set_result(&mut self, player: u64, result: i64);
    /// `EEWorldCanRecover`'s parts: whether the player can train a citizen (no citizen node, or `CanAfford`), its
    /// complex units in list order (`GetNextComplexUnit`) and each one's hit points, previous part, class vitality,
    /// category, land building, type and built flag
    fn can_train(&mut self, player: u64) -> bool;
    fn complex_units(&mut self, player: u64) -> Vec<u64>;
    fn unit(&mut self, unit: u64) -> RecoverUnit;
}

/// A complex unit as `EEWorldCanRecover` reads it
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RecoverUnit {
    pub hit_points: i64,
    pub first_part: bool,
    pub vital: bool,
    pub naval_citizen: bool,
    pub land_building: bool,
    pub unit_type: u64,
    pub built: bool,
}

/// `kUnitTypeTownCenter`, `kUnitTypeCapitol`
pub const TOWN_CENTER: u64 = 0x23;
pub const CAPITOL: u64 = 0x29;

/// `EEWorldCanRecover`: a living unit that is not a building, or a finished town centre or capitol and the means to
/// train a citizen there (fishing boats do not count)
pub fn can_recover<H: GameOverHost + ?Sized>(h: &mut H, player: u64) -> bool {
    let can_train = h.can_train(player);
    for u in h.complex_units(player) {
        let r = h.unit(u);
        #[allow(clippy::neg_cmp_op_on_partial_ord)]
        if !(r.hit_points > 0) || !r.first_part || !r.vital || r.naval_citizen {
            continue;
        }
        if !r.land_building {
            return true;
        }
        if can_train && r.built && (r.unit_type == TOWN_CENTER || r.unit_type == CAPITOL) {
            return true;
        }
    }
    false
}

/// `EEWorld::CheckGameOver`: 2 when someone won, 1 when no human (or only one player) is left, else 0
pub fn check_game_over<H: GameOverHost + ?Sized>(h: &mut H) -> i64 {
    let mut results: Vec<(u64, i64)> = Vec::new();
    let mut result: i64 = 0;
    let mut humans = 0;
    // the C++'s labels: `alone` and `results`, or `done`
    enum Next {
        Alone,
        Results,
        Done,
    }
    let next = 'decide: {
        if h.victory_enabled() {
            let mut i = 0;
            while i < h.teams() {
                let state = h.team_state(i);
                if state != 0 {
                    result = 0;
                    for p in h.team_players(i) {
                        results.push((p, state));
                    }
                    result = 2;
                }
                i += 1;
            }
            if result != 0 {
                break 'decide Next::Results;
            }
        }
        if h.num_players() < 2 {
            break 'decide Next::Alone;
        }
        let mut i = 1;
        while i < h.num_players() {
            if h.exists(i) {
                if !h.observer(i) {
                    if h.victory_enabled()
                        && (h.end_of_game_count(i) <= 0
                            || (h.defeat_without_recovery()
                                && !h.scenario()
                                && h.game_result(i) == 0
                                && !can_recover(h, i)))
                    {
                        h.remove(false, i);
                        h.set_result(i, 2);
                    }
                    if h.is_human(i) {
                        humans += 1;
                    }
                }
                if h.game_result(i) == 1 {
                    result = 2;
                }
            }
            i += 1;
        }
        if result == 0 {
            if humans == 0 {
                break 'decide Next::Alone;
            }
            let teams = h.teams();
            if !h.victory_enabled() || teams > 1 || teams == 0 {
                break 'decide Next::Done;
            }
            result = 2;
            for p in h.team_players(0) {
                results.push((p, 1));
            }
        }
        Next::Results
    };
    match next {
        Next::Done => return result,
        Next::Alone => {
            result = 1;
            let mut i = 1;
            while i < h.num_players() {
                if h.exists(i) && h.game_result(i) == 1 {
                    result = 2;
                    break;
                }
                i += 1;
            }
        }
        Next::Results => {}
    }
    for &(p, r) in &results {
        if h.exists(p) && h.game_result(p) == 0 {
            h.remove(r == 1, p);
            h.set_result(p, r);
        }
    }
    let mut i = 1;
    while i < h.num_players() {
        if h.exists(i) && h.game_result(i) == 0 {
            h.remove(result == 1, i);
            h.set_result(i, result);
        }
        i += 1;
    }
    result
}

#[cfg(test)]
#[path = "game_over_tests.rs"]
mod tests;
