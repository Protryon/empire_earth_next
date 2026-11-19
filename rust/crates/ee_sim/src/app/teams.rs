//! The world's teams (`EEWorld::mTeams`' players; EETeam's and EEWorld's team bodies): a team is its players' world
//! indices in order. The victory condition each C++ `EETeam` holds keeps only its team's address, so the C++ mirror
//! (`EEWorld::mTeams`, which the AI thread and the server read) is rebuilt from these lists after every change.
//!
//! The stances are read through `enemy(a, b)`: player `a`'s `mEnemies[b]` is not 0 (indices and player ids are
//! the world's own: `GetPlayerID()` of player i is i).
//!
//! Determinism: vectors in the C++'s order; nothing hashes or sorts.

/// The teams, in `mTeams`' order.
pub type Teams = Vec<Vec<u32>>;

/// `EETeam::AddPlayer`: pushed unless present.
pub fn add_player(team: &mut Vec<u32>, player: u32) {
    if !team.contains(&player) {
        team.push(player);
    }
}

/// `EETeam::Merge`: each of `other`'s players added.
pub fn merge(team: &mut Vec<u32>, other: &[u32]) {
    for &p in other {
        add_player(team, p);
    }
}

/// `EETeam::RemovePlayer`: a copy of the team without the player's first entry.
pub fn without(team: &[u32], player: u32) -> Vec<u32> {
    let mut out = team.to_vec();
    if let Some(at) = out.iter().position(|&p| p == player) {
        out.remove(at);
    }
    out
}

/// `EEWorld::AddTeam`: the team, after merging in every existing team none of whose players is at war with one of
/// its players either way (the existing teams that merge leave their places), pushed at the end. `present(i)`:
/// `mPlayers[i]` is not NULL.
pub fn add_team(teams: &mut Teams, players: &[u32], present: &dyn Fn(u32) -> bool, enemy: &dyn Fn(u32, u32) -> bool) {
    let mut team = players.to_vec();
    let mut i = 0;
    while i < teams.len() {
        let mut disjoint = true;
        'members: for &member in &teams[i] {
            if !present(member) {
                continue;
            }
            for &p in &team {
                if (present(p) && enemy(p, member)) || enemy(member, p) {
                    disjoint = false;
                    break 'members;
                }
            }
        }
        if disjoint {
            let other = teams.remove(i);
            merge(&mut team, &other);
        } else {
            i += 1;
        }
    }
    teams.push(team);
}

/// `EEWorld::AddAlliance`: the two players' team, after merging in every existing team whose players are allied both
/// ways with both of them, pushed at the end.
pub fn add_alliance(teams: &mut Teams, a: u32, b: u32, enemy: &dyn Fn(u32, u32) -> bool) {
    let mut team = Vec::new();
    add_player(&mut team, a);
    add_player(&mut team, b);
    let mut i = 0;
    while i < teams.len() {
        let allied = teams[i].iter().all(|&p| !(enemy(a, p) || enemy(p, a) || enemy(b, p) || enemy(p, b)));
        if allied {
            let other = teams.remove(i);
            merge(&mut team, &other);
        } else {
            i += 1;
        }
    }
    teams.push(team);
}

/// `EEWorld::BreakAlliance`: every team holding both players leaves; each is added again without the one and
/// without the other (`AddTeam`), in order.
pub fn break_alliance(
    teams: &mut Teams,
    a: u32,
    b: u32,
    present: &dyn Fn(u32) -> bool,
    enemy: &dyn Fn(u32, u32) -> bool,
) {
    let mut split = Vec::new();
    let mut i = 0;
    while i < teams.len() {
        if teams[i].contains(&a) && teams[i].contains(&b) {
            let t = teams.remove(i);
            split.push(without(&t, a));
            split.push(without(&t, b));
        } else {
            i += 1;
        }
    }
    for t in split {
        add_team(teams, &t, present, enemy);
    }
}

/// `EEWorld::RemovePlayerFromTeams`: every team holding the player leaves and is added again without it.
pub fn remove_player(teams: &mut Teams, player: u32, present: &dyn Fn(u32) -> bool, enemy: &dyn Fn(u32, u32) -> bool) {
    let mut split = Vec::new();
    let mut i = 0;
    while i < teams.len() {
        if teams[i].contains(&player) {
            let t = teams.remove(i);
            split.push(without(&t, player));
        } else {
            i += 1;
        }
    }
    for t in split {
        add_team(teams, &t, present, enemy);
    }
}

/// `EEWorld::RebuildTeams`' teams: one team per playing (not observing) player from 1, then an alliance for each
/// pair of playing players allied one way (`i < j`, `mPlayers[i]`'s stance toward j), in order.
pub fn rebuild(count: u32, observer: &dyn Fn(u32) -> bool, enemy: &dyn Fn(u32, u32) -> bool) -> Teams {
    let mut teams: Teams = (1..count).filter(|&i| !observer(i)).map(|i| vec![i]).collect();
    for i in 1..count {
        if observer(i) {
            continue;
        }
        for j in i + 1..count {
            if !observer(j) && !enemy(i, j) {
                add_alliance(&mut teams, i, j, enemy);
            }
        }
    }
    teams
}
