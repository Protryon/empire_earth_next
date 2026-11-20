//! order_log.rs: the lines from records and worlds made up here.

use super::*;

/// a record by member index
struct Fake(Vec<(u32, Vec<u8>)>);

impl OrderRecord for Fake {
    fn bytes(&self, index: u32, size: usize) -> Option<&[u8]> {
        self.0.iter().find(|(i, b)| *i == index && b.len() == size).map(|(_, b)| b.as_slice())
    }
}

fn i(index: u32, v: i32) -> (u32, Vec<u8>) {
    (index, v.to_le_bytes().to_vec())
}
fn f(index: u32, v: f32) -> (u32, Vec<u8>) {
    (index, v.to_le_bytes().to_vec())
}
fn b(index: u32, v: bool) -> (u32, Vec<u8>) {
    (index, vec![u8::from(v)])
}
fn pt(index: u32, x: f32, y: f32) -> (u32, Vec<u8>) {
    let mut v = x.to_le_bytes().to_vec();
    v.extend_from_slice(&y.to_le_bytes());
    (index, v)
}

#[derive(Default)]
struct World {
    group: Vec<Option<(i64, u64)>>,
    makes: Option<i64>,
}

impl OrderWorld for World {
    fn class_name(&mut self, t: i64) -> String {
        format!("c{t}")
    }
    fn tech_name(&mut self, tech: i64) -> String {
        format!("t{tech}")
    }
    fn target(&mut self, id: i64) -> Option<(i64, u64, i64, i64)> {
        (id == 7).then_some((12, 2, 30, 40))
    }
    fn makes_unit(&mut self, _tech: i64) -> Option<i64> {
        self.makes
    }
    fn group_units(&mut self) -> Vec<Option<(i64, u64)>> {
        self.group.clone()
    }
}

#[test]
fn a_move_names_its_group_and_point() {
    let mut w = World { group: vec![Some((5, 100)), None, Some((3, 101)), Some((5, 102))], makes: None };
    let m = Fake(vec![f(2, 12.9), f(3, -3.5), b(4, true), b(6, false)]);
    let line = order_line(MSG_PATH_TO, &m, &mut w, 5000, 1).unwrap();
    assert_eq!(line, "EE-GAME 5000 p1 order verb=move units=4 classes=c3:1,c5:2 ids=100,101,102 at=12,-3 forced=1\n");
}

#[test]
fn an_attack_on_a_known_and_an_unknown_target() {
    let mut w = World::default();
    let line = order_line(MSG_ATTACK_UNIT, &Fake(vec![i(1, 7), b(2, true), b(3, true)]), &mut w, 1, 3).unwrap();
    assert_eq!(line, "EE-GAME 1 p3 order verb=convert units=0 classes=- ids=- target=7:c12:p2@30,40 queued=1\n");
    let line = order_line(MSG_REPAIR_UNIT, &Fake(vec![i(1, 0)]), &mut w, 1, 3).unwrap();
    assert!(line.ends_with(" target=0:-\n"), "{line}");
}

#[test]
fn research_of_a_unit_is_training_and_not_the_group() {
    let mut w = World { group: vec![Some((1, 9))], makes: Some(44) };
    let m = Fake(vec![i(1, 7), i(2, 600), b(3, true)]);
    let line = order_line(MSG_RESEARCH_TECHNOLOGY, &m, &mut w, 2, 1).unwrap();
    assert_eq!(line, "EE-GAME 2 p1 order verb=train target=7:c12:p2@30,40 class=c44 times=5\n");
    w.makes = None;
    let line =
        order_line(MSG_BUILD_WALL, &Fake(vec![pt(1, 1.0, 2.0), pt(2, 3.0, 4.0), i(3, 9)]), &mut w, 2, 1).unwrap();
    assert_eq!(line, "EE-GAME 2 p1 order verb=wall units=1 classes=c1:1 ids=9 tech=t9 at=1,2 at=3,4\n");
}

#[test]
fn other_messages_are_not_logged() {
    assert!(order_line(0x01, &Fake(vec![]), &mut World::default(), 0, 1).is_none());
}

#[test]
fn ids_stop_after_24() {
    let mut w = World { group: (0..26).map(|k| Some((1, k))).collect(), makes: None };
    let s = units(&mut w);
    assert!(s.starts_with(" units=26 classes=c1:26 ids=0,1,"), "{s}");
    assert!(s.ends_with(",22,23,..."), "{s}");
}

#[test]
fn a_selection_is_logged_when_it_changes() {
    let mut last = Vec::new();
    let mut w = World { group: vec![Some((2, 5))], makes: None };
    assert_eq!(select_line(&mut last, 1, &mut w, 9).unwrap(), "EE-GAME 9 p1 select units=1 classes=c2:1 ids=5\n");
    assert!(select_line(&mut last, 1, &mut w, 10).is_none());
    w.group.clear();
    assert!(select_line(&mut last, 1, &mut w, 11).is_some());
    assert!(select_line(&mut last, 64, &mut w, 11).is_none());
}

#[test]
fn the_wire_reads_by_the_record_layout() {
    // a PathTo record's members as the codec lays them out
    let ty = MSG_PATH_TO;
    let mut rec = [0u8; EE_NETMSG_RECORD_BYTES];
    let x = record_member(ty, 2);
    assert_eq!(x.size, 4);
    rec[x.offset as usize..x.offset as usize + 4].copy_from_slice(&7.5f32.to_le_bytes());
    let w = Wire { ty, record: &rec };
    assert_eq!(w.f32(2), 7.5);
    assert!(w.bytes(2, 1).is_none());
}
