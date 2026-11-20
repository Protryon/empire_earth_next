use super::*;

fn unit(id: u32, unit_type: u64, tile: (i64, i64)) -> OwnUnit {
    OwnUnit { id, class: Some(OwnClass { unit_type, ..OwnClass::default() }), tile, ..OwnUnit::default() }
}

fn collect(p: &PlayerFacts, w: &WorldFacts, on_map: impl Fn(i64, i64) -> bool) -> Vec<(SweepCase, Vec<u32>)> {
    let mut out = Vec::new();
    player_cases(p, w, on_map, |c, u| out.push((*c, u.to_vec())));
    out
}

#[test]
fn an_empty_player_runs_three_selections_from_the_centre() {
    let p = PlayerFacts { id: 3, build_techs: [-1; 3], ..PlayerFacts::default() };
    let w = WorldFacts { resource: None, map_size: 9 };
    let cases = collect(&p, &w, |_, _| true);
    // no path point: the fourth selection is left out
    assert!(cases.iter().all(|(c, u)| c.selection < 3 && u.is_empty()));
    let first = &cases[0].0;
    assert_eq!((first.body, first.x, first.y), (SWEEP_ATTACK_GROUND, 4.5, 4.5));
    // the hack cases close each selection: the player's id as the target, the four points
    let last = &cases.last().unwrap().0;
    assert_eq!((last.body, last.target, last.technology, last.x), (SWEEP_HACK_A_NEW_UNIT, 3, 0x7fff, 8.5));
    // Build Unit: only the technology with no node (four cases: plain and queued, the first point and the centre
    // for the first technology only)
    let build: Vec<_> = cases.iter().filter(|(c, u)| c.body == SWEEP_BUILD_UNIT && u.is_empty()).collect();
    assert_eq!(build.len(), 3 * 2);
    assert!(build.iter().all(|(c, _)| c.technology == 0x7e00));
}

#[test]
fn the_first_unit_on_the_map_gives_the_first_point() {
    let p = PlayerFacts {
        own: vec![unit(0x101, 7, (2, 3)), unit(0x102, UNIT_TYPE_PATH_POINT, (5, 5))],
        next_first: Some(0x201),
        ..PlayerFacts::default()
    };
    let w = WorldFacts { resource: Some((0x9, Some(0x10))), map_size: 20 };
    let cases = collect(&p, &w, |x, y| x < 4 && y < 4);
    // the selections: own, reversed, none, the path point first
    let sels: Vec<u32> = cases.iter().map(|(c, _)| c.selection).collect();
    assert!(sels.windows(2).all(|s| s[0] <= s[1]) && sels.contains(&3));
    let (c, u) = &cases[0];
    assert_eq!((c.x, c.y), (2.5, 3.5));
    assert_eq!(u, &vec![0x101, 0x102]);
    // the reversed selection's first unit (the path point, at 5, 5) is off this map: the centre
    let rev = cases.iter().find(|(c, _)| c.selection == 1).unwrap();
    assert_eq!((rev.0.x, rev.0.y), (10.5, 10.5));
    // Patrol: -1, its first unit, the next player's, its path point, no unit
    let patrol: Vec<i32> =
        cases.iter().filter(|(c, _)| c.selection == 0 && c.body == SWEEP_PATROL).map(|(c, _)| c.target).collect();
    assert_eq!(patrol, vec![-1, 0x101, 0x201, 0x102, SWEEP_NO_UNIT]);
    // Repair skips the path point; Attack Unit converts only its first target and adds the resource
    let attack: Vec<(i32, bool, bool)> = cases
        .iter()
        .filter(|(c, _)| c.selection == 0 && c.body == SWEEP_ATTACK_UNIT)
        .map(|(c, _)| (c.target, c.queued, c.convert))
        .collect();
    assert_eq!(
        attack[..5],
        [
            (0x101, false, false),
            (0x101, false, true),
            (0x201, false, false),
            (SWEEP_NO_UNIT, false, false),
            (9, false, false)
        ]
    );
    // Unload's points are whole
    let unload = cases.iter().find(|(c, _)| c.body == SWEEP_UNLOAD_TRANSPORT).unwrap();
    assert_eq!((unload.0.x, unload.0.y), (2.0, 3.0));
    // Hack A New Unit: no class, the first unit's, the resource's, past the table
    let hack: Vec<i32> = cases
        .iter()
        .filter(|(c, _)| c.selection == 0 && c.body == SWEEP_HACK_A_NEW_UNIT && c.x == 0.5)
        .map(|(c, _)| c.technology)
        .collect();
    assert_eq!(hack, vec![-1, 7, 0x10, 0x7fff]);
}

#[test]
fn rally_goals_run_from_the_buildings_found() {
    let mut farm = unit(0x103, UNIT_TYPE_FARM, (1, 1));
    farm.class.as_mut().unwrap().land_building = true;
    farm.damaged = true;
    let p = PlayerFacts { own: vec![unit(0x101, UNIT_TYPE_AIRPORT, (1, 1)), farm], ..PlayerFacts::default() };
    let w = WorldFacts { resource: None, map_size: 8 };
    let cases = collect(&p, &w, |_, _| true);
    let rally: Vec<(i32, i32, bool)> = cases
        .iter()
        .filter(|(c, _)| c.selection == 0 && c.body == SWEEP_SET_RALLY_GOAL)
        .map(|(c, _)| (c.caster, c.target, c.queued))
        .collect();
    // buildings: its first unit, its airport (the same), its land building; targets: -1, targets[2] (here the id
    // no unit has), the farm (farm and damaged), no unit, then the building itself
    let targets = [-1, SWEEP_NO_UNIT, 0x103, 0x103, SWEEP_NO_UNIT];
    assert_eq!(rally.len(), 3 * (targets.len() + 1) * 2);
    assert_eq!(rally[0], (0x101, -1, false));
    assert_eq!(rally[(targets.len() + 1) * 2 - 1], (0x101, 0x101, true));
    assert_eq!(rally.last().unwrap(), &(0x103, 0x103, true));
}

#[test]
fn a_record_holds_its_bodys_fields() {
    let c = SweepCase {
        body: SWEEP_PATH_TO,
        selection: 2,
        x: 1.5,
        y: 2.5,
        target: -1,
        flags: 0xf,
        facing: 0.75,
        ..SweepCase::default()
    };
    let r = record(&c, 4, &[7, 8], &[1, 2, 3], 0);
    let words: Vec<u32> = r.chunks(4).map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]])).collect();
    assert_eq!(
        words,
        vec![8, 4, 2, 1.5f32.to_bits(), 2.5f32.to_bits(), u32::MAX, 0xf, 0.75f32.to_bits(), 2, 7, 8, 3, 1, 2, 3, 0]
    );
    let b = SweepCase { body: SWEEP_BUILD_UNIT, technology: 5, queued: true, ..SweepCase::default() };
    let r = record(&b, 0, &[], &[], 1);
    assert_eq!(&r[24..28], &(5u32 | 0x8000_0000).to_le_bytes());
    assert_eq!(r.len(), 4 * 10);
}
