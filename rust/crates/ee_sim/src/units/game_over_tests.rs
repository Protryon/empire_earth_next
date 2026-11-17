use super::*;

#[derive(Default, Clone)]
struct P {
    exists: bool,
    observer: bool,
    end_count: i64,
    result: u64,
    human: bool,
    can_train: bool,
    units: Vec<RecoverUnit>,
}

#[derive(Default)]
struct Mock {
    victory: bool,
    scenario: bool,
    defeat: bool,
    players: Vec<P>,
    teams: Vec<(i64, Vec<u64>)>,
    log: Vec<String>,
}

impl GameOverHost for Mock {
    fn victory_enabled(&mut self) -> bool {
        self.victory
    }
    fn num_players(&mut self) -> u64 {
        self.players.len() as u64
    }
    fn scenario(&mut self) -> bool {
        self.scenario
    }
    fn defeat_without_recovery(&mut self) -> bool {
        self.defeat
    }
    fn teams(&mut self) -> usize {
        self.teams.len()
    }
    fn team_state(&mut self, team: usize) -> i64 {
        self.teams[team].0
    }
    fn team_players(&mut self, team: usize) -> Vec<u64> {
        self.teams[team].1.clone()
    }
    fn exists(&mut self, p: u64) -> bool {
        self.players.get(p as usize).is_some_and(|p| p.exists)
    }
    fn observer(&mut self, p: u64) -> bool {
        self.players[p as usize].observer
    }
    fn end_of_game_count(&mut self, p: u64) -> i64 {
        self.players[p as usize].end_count
    }
    fn game_result(&mut self, p: u64) -> u64 {
        self.players[p as usize].result
    }
    fn is_human(&mut self, p: u64) -> bool {
        self.players[p as usize].human
    }
    fn remove(&mut self, won: bool, p: u64) {
        self.log.push(format!("remove {won} {p}"));
        self.players[p as usize].observer = true;
    }
    fn set_result(&mut self, p: u64, r: i64) {
        self.log.push(format!("result {p} {r}"));
        self.players[p as usize].result = r as u64;
    }
    fn can_train(&mut self, p: u64) -> bool {
        self.log.push(format!("can_train {p}"));
        self.players[p as usize].can_train
    }
    fn complex_units(&mut self, p: u64) -> Vec<u64> {
        (0..self.players[p as usize].units.len() as u64).map(|i| p * 100 + i).collect()
    }
    fn unit(&mut self, u: u64) -> RecoverUnit {
        self.players[(u / 100) as usize].units[(u % 100) as usize]
    }
}

fn player(end_count: i64, human: bool) -> P {
    P { exists: true, end_count, human, ..Default::default() }
}

#[test]
fn a_player_out_of_units_is_defeated_and_the_last_team_wins() {
    let mut m = Mock { victory: true, ..Default::default() };
    m.players = vec![player(1, false), player(3, true), player(0, false)];
    m.teams = vec![(0, vec![1])];
    assert_eq!(check_game_over(&mut m), 2);
    assert_eq!(m.log, vec!["remove false 2", "result 2 2", "remove true 1", "result 1 1"]);
}

#[test]
fn no_human_left_ends_alone_and_victory_off_waits() {
    let mut m = Mock { victory: true, ..Default::default() };
    m.players = vec![player(1, false), player(3, false), player(3, false)];
    m.teams = vec![(0, vec![1]), (0, vec![2])];
    assert_eq!(check_game_over(&mut m), 1);
    assert_eq!(m.log, vec!["remove true 1", "result 1 1", "remove true 2", "result 2 1"]);
    let mut m = Mock { victory: false, ..Default::default() };
    m.players = vec![player(1, false), player(0, true), player(3, false)];
    assert_eq!(check_game_over(&mut m), 0);
    assert!(m.log.is_empty());
}

#[test]
fn a_team_victory_decides_first() {
    let mut m = Mock { victory: true, ..Default::default() };
    m.players = vec![player(1, false), player(3, true), player(3, false)];
    m.teams = vec![(1, vec![1]), (2, vec![2])];
    assert_eq!(check_game_over(&mut m), 2);
    assert_eq!(m.log, vec!["remove true 1", "result 1 1", "remove false 2", "result 2 2"]);
}

#[test]
fn a_player_that_cannot_recover_is_out_only_with_the_rule() {
    let building = RecoverUnit {
        hit_points: 5,
        first_part: true,
        vital: true,
        land_building: true,
        unit_type: TOWN_CENTER,
        built: true,
        ..Default::default()
    };
    let mut m = Mock { victory: true, defeat: true, ..Default::default() };
    m.players = vec![player(1, false), player(3, true), player(3, false), player(3, false)];
    m.players[1].units = vec![RecoverUnit { land_building: false, ..building }];
    m.players[2].units = vec![building];
    m.players[2].can_train = false;
    m.players[3].units = vec![building];
    m.players[3].can_train = true;
    m.teams = vec![(0, vec![1]), (0, vec![3])];
    assert_eq!(check_game_over(&mut m), 0);
    assert_eq!(m.log, vec!["can_train 1", "can_train 2", "remove false 2", "result 2 2", "can_train 3"]);
    // a soldier always recovers; a dead or naval citizen does not count
    let soldier = RecoverUnit { hit_points: 1, first_part: true, vital: true, ..Default::default() };
    let mut m = Mock { victory: true, defeat: true, ..Default::default() };
    m.players = vec![player(1, false), player(3, true)];
    m.players[1].units = vec![RecoverUnit { naval_citizen: true, ..soldier }, RecoverUnit { hit_points: 0, ..soldier }];
    assert!(!can_recover(&mut m, 1));
    m.players[1].units.push(soldier);
    assert!(can_recover(&mut m, 1));
}
