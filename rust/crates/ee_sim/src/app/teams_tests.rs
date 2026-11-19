//! The teams' tests: the merges as the C++ made them.

use super::teams::*;

fn stances(war: &'static [(u32, u32)]) -> impl Fn(u32, u32) -> bool {
    move |a, b| war.contains(&(a, b))
}

#[test]
fn a_rebuild_allies_the_players_at_peace() {
    // 1 and 2 at peace, 3 at war with 1 (one way), 4 observes
    let enemy = stances(&[(1, 3), (3, 2), (2, 3)]);
    let t = rebuild(5, &|i| i == 4, &enemy);
    assert_eq!(t, vec![vec![3], vec![1, 2]]);
    let t = rebuild(4, &|_| false, &|_, _| true);
    assert_eq!(t, vec![vec![1], vec![2], vec![3]]);
}

#[test]
fn a_team_merges_the_teams_without_war_either_way() {
    let enemy = stances(&[(3, 1)]);
    let mut t: Teams = vec![vec![2], vec![3], vec![4]];
    add_team(&mut t, &[1, 1], &|i| i != 4, &enemy);
    // 2 merges; 3 is at war with 1; 4 is no player (NULL): its team has no member to test and merges
    assert_eq!(t, vec![vec![3], vec![1, 1, 2, 4]]);
}

#[test]
fn breaking_an_alliance_splits_its_teams() {
    let peace = |_: u32, _: u32| false;
    let mut t: Teams = vec![vec![1, 2, 3], vec![4]];
    // at war now: the two halves are added again (and merge back where at peace)
    let enemy = stances(&[(1, 2), (2, 1)]);
    break_alliance(&mut t, 1, 2, &|_| true, &enemy);
    assert_eq!(t, vec![vec![2, 3, 4], vec![1, 3]]);
    let mut t: Teams = vec![vec![1, 2], vec![3]];
    remove_player(&mut t, 2, &|_| true, &peace);
    assert_eq!(t, vec![vec![1, 3]]);
    let mut t: Teams = vec![vec![5]];
    add_alliance(&mut t, 1, 2, &stances(&[(5, 1)]));
    assert_eq!(t, vec![vec![5], vec![1, 2]]);
    assert_eq!(without(&[1, 2, 1], 1), vec![2, 1]);
}
