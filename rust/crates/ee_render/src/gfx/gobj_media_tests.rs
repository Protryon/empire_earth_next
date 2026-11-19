use super::*;

fn part(
    frame_count: c_ulong,
    shares: bool,
    start: c_ulong,
    last: c_ulong,
    model: Option<&[u8]>,
) -> GraphicFramePart<'_> {
    GraphicFramePart { frame_count, shares, start, last, model }
}

#[test]
fn frame_maps_keep_the_first_value_in_frame_order() {
    let none = part(0, false, 0, 0, None);
    let mut parts = vec![none; PARTS];
    // two parts on one model: their entries share a map, the first insert of a frame wins
    parts[0] = part(6, true, 0, 5, Some(b"a"));
    parts[1] = part(5, true, 4, 8, Some(b"a"));
    // a part on the shared model (no name), one on another model, one too short, one not sharing
    parts[2] = part(5, true, 10, 14, None);
    parts[3] = part(5, true, 0, 4, Some(b"b"));
    parts[4] = part(4, true, 0, 3, Some(b"c"));
    parts[5] = part(9, false, 0, 8, Some(b"d"));
    let (frames, slots) = frame_maps(&parts);
    assert_eq!(frames.len(), PARTS + 1);
    assert_eq!(&slots[..6], &[0, 0, 14, 1, -1, -1]);
    let entries = |slot: usize| frames[slot].iter().map(|e| (e.frame, e.offset)).collect::<Vec<_>>();
    // part 0: an even count, frames 1 and 3 -1, the last odd frame (5) gives its previous one (4) 1; part 1 (an odd
    // count from 4): 5 and 7 -1
    assert_eq!(entries(0), vec![(1, -1), (3, -1), (4, 1), (5, -1), (7, -1)]);
    assert_eq!(entries(1), vec![(1, -1), (3, -1)]);
    assert_eq!(entries(14), vec![(11, -1), (13, -1)]);
    assert!(frames[2].is_empty());
}

#[test]
fn mip_levels_are_the_sum_below_the_maximum() {
    assert_eq!(mip_levels_to_drop(1, 1, 3), 2);
    assert_eq!(mip_levels_to_drop(2, 1, 3), 3);
    assert_eq!(mip_levels_to_drop(5, 0, 3), 3);
    // a negative record value wraps as the C++'s unsigned sum
    assert_eq!(mip_levels_to_drop(-1, 0, 3), 3);
    assert_eq!(mip_levels_to_drop(-1, 1, 3), 0);
}

#[test]
fn part_kinds_by_name() {
    assert_eq!(part_kind(b"turret_main"), PartKind::Turret);
    assert_eq!(part_kind(b"Turret_main"), PartKind::Other);
    assert_eq!(part_kind(b"orbital_cw_90_x"), PartKind::Orbital);
    assert_eq!(part_kind(b"ORBITAL_CCW_45_x"), PartKind::Orbital);
    assert_eq!(part_kind(b"wheel_1"), PartKind::Other);
}

struct Models;

fn translation(x: f32, y: f32, z: f32) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
    t
}

impl PartModels for Models {
    fn name(&self, model: usize) -> Vec<u8> {
        match model {
            1 => b"body".to_vec(),
            2 => b"turret_a".to_vec(),
            3 => b"orbital_cw_90_ring".to_vec(),
            4 => b"wheel_front".to_vec(),
            _ => b"turret_b".to_vec(),
        }
    }
    fn internal0(&self, model: usize) -> GeTransformation {
        translation(model as f32, 0.0, 1.0)
    }
    fn sub_models(&self, model: usize) -> Vec<usize> {
        match model {
            1 => vec![2, 3, 4],
            2 => vec![5],
            _ => vec![],
        }
    }
}

#[derive(Default)]
struct Found(Vec<String>);

impl PartSink for Found {
    fn turret(&mut self, position: Ge3DPoint, at: &GeTransformation, model: usize) {
        assert_eq!((at.x(), at.y(), at.z()), (position.x, position.y, position.z));
        self.0.push(format!("turret {model} at {} {} {}", position.x, position.y, position.z));
    }
    fn orbital(&mut self, model: usize) {
        self.0.push(format!("orbital {model}"));
    }
    fn maybe_wheel(&mut self, model: usize) {
        self.0.push(format!("other {model}"));
    }
}

#[test]
fn the_part_scan_walks_depth_first_under_the_combined_transformations() {
    let mut found = Found::default();
    collect_parts(&Models, &mut found, 1, &translation(10.0, 0.0, 0.0));
    assert_eq!(
        found.0,
        vec![
            "other 1".to_string(),
            "turret 2 at 13 0 2".to_string(),
            "turret 5 at 18 0 3".to_string(),
            "orbital 3".to_string(),
            "other 4".to_string(),
        ]
    );
    let mut none = Found::default();
    collect_parts(&Models, &mut none, 0, &GeTransformation::IDENTITY);
    assert!(none.0.is_empty());
}
