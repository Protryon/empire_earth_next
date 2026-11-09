use super::*;
use crate::capture::{constructed_as_preset, decode, replay, slots_eq, Record};

/// `EEFCircle`'s constructor values: the half arc (pi) and the minimum radius.
const CIRCLE_HALF_ARC: f32 = 3.14159265;
const CIRCLE_MIN_RADIUS: f32 = 0.5;

fn hex(text: &str) -> Vec<u8> {
    (0..text.len()).step_by(2).map(|i| u8::from_str_radix(&text[i..i + 2], 16).unwrap()).collect()
}

#[test]
fn the_recorded_cpp_calls_replay_bit_for_bit() {
    let mut kinds = Vec::new();
    for &(tag, text) in crate::golden::CALLS {
        let payload = hex(text);
        let call = decode(&Record { tag, payload: &payload }).expect("a formation tag").expect("a whole record");
        assert!(constructed_as_preset(&call), "type {}", call.kind);
        assert!(!call.slots.is_empty());
        assert!(slots_eq(&replay(&call), &call.slots), "type {}: {:?}", call.kind, replay(&call));
        kinds.push(call.kind);
    }
    assert_eq!(kinds, vec![0, 1, 2, 3, 4, 5, 6, 7, 7]);
}

#[test]
fn records_split_a_capture_and_refuse_truncation() {
    let mut data = Vec::new();
    data.extend_from_slice(&7u32.to_le_bytes());
    data.extend_from_slice(&2u32.to_le_bytes());
    data.extend_from_slice(&[1, 2]);
    let records = crate::capture::records(&data).unwrap();
    assert_eq!(records, vec![Record { tag: 7, payload: &[1, 2] }]);
    assert!(decode(&records[0]).is_none());
    assert!(crate::capture::records(&data[..9]).is_err());
    assert!(crate::capture::records(&data[..5]).is_err());
}

fn frame(yaw: f32) -> EeFormationFrame {
    EeFormationFrame { orientation: Rotation::yaw_pitch_roll(yaw, 0.0, 0.0).0, center: [50.0, 60.0, 2.0], heading: yaw }
}

fn line(kind: i32) -> (EeFormationLine, Vec<f32>) {
    let Some(Preset::Line(spacing, depth, count, rows)) = preset(kind) else { panic!("not a line") };
    (EeFormationLine { count, rows: rows + 1, spacing, depth }, line_offsets(spacing, count))
}

#[test]
fn line_offsets_alternate_out_from_the_centre() {
    assert_eq!(line_offsets(1.0, 5), vec![0.0, -1.0, 1.0, -2.0, 2.0]);
    assert_eq!(line_offsets(2.0, 4), vec![-1.0, 1.0, -3.0, 3.0]);
    assert_eq!(line_offsets(1.0, 10), vec![-0.5, 0.5, -1.5, 1.5, -2.5, 2.5, -3.5, 3.5, -4.5, 4.5]);
    assert_eq!(line_offsets(1.0, 1), vec![0.0]);
    assert!(line_offsets(1.0, 0).is_empty());
}

#[test]
fn um_tan_is_sine_over_cosine_and_infinite_at_the_poles() {
    assert_eq!(um_tan(0.0), 0.0);
    assert!((um_tan(0.785398185) - 1.0).abs() < 1e-3);
    // the table cosine is +0 at the pole: +inf just below pi/2, -inf at pi/2 itself (outside [0, pi/2))
    assert_eq!(um_tan(1.57075), f32::INFINITY);
    assert_eq!(um_tan(1.5707964), f32::NEG_INFINITY);
}

#[test]
fn rotation_by_yaw_turns_y_towards_minus_x() {
    let r = Rotation::yaw_pitch_roll(1.5707964, 0.0, 0.0);
    let v = r.apply([0.0, 1.0, 0.0]);
    assert!((v[0] + 1.0).abs() < 1e-6 && v[1].abs() < 1e-3 && v[2] == 0.0, "{v:?}");
    let mut twice = Rotation::IDENTITY;
    twice.rotate(0.5, 0.0, 0.0);
    twice.rotate(0.5, 0.0, 0.0);
    let once = Rotation::yaw_pitch_roll(1.0, 0.0, 0.0);
    for i in 0..3 {
        for j in 0..3 {
            assert!((twice.0[i][j] - once.0[i][j]).abs() < 1e-3);
        }
    }
}

#[test]
fn a_line_fills_rows_and_leaves_a_gap_row_after_five() {
    let (l, offsets) = line(0);
    let radii = vec![1.0f32; 30];
    let slots = line_place(&frame(0.0), &l, &offsets, &radii);
    assert_eq!(slots.len(), 30);
    // rows 0..4 at depth 0..4, then a skipped row: the sixth row stands at depth 6
    assert_eq!(slots[0].position, [50.0, 60.0, 2.0]);
    assert_eq!(slots[24].position[1], 60.0 - 4.0);
    assert_eq!(slots[25].position[1], 60.0 - 6.0);
    assert!(slots.iter().enumerate().all(|(i, s)| s.unit == i as u32 && s.heading == 0.0));
}

#[test]
fn a_staggered_line_shifts_its_even_rows() {
    let (l, offsets) = line(3);
    let slots = line_place(&frame(0.0), &l, &offsets, &[1.0; 10]);
    assert_eq!(slots[0].position[0], 50.0);
    assert_eq!(slots[5].position[0], 50.0 + 0.75);
}

#[test]
fn a_wedge_puts_a_leader_first_then_pairs() {
    let Some(Preset::Wedge(spacing, angle, count)) = preset(4) else { panic!() };
    let w = wedge_geometry(spacing, angle, count);
    assert_eq!(w.left_arm[0], -w.right_arm[0]);
    assert_eq!(w.left_arm[1], w.right_arm[1]);
    let slots = wedge_place(&frame(0.0), &w, &[1.0; 4]);
    assert_eq!(slots.iter().map(|s| s.unit).collect::<Vec<_>>(), vec![0, 1, 2, 3]);
    assert_eq!(slots[0].position, [50.0, 60.0, 2.0]);
    assert!(slots[1].position[0] < 50.0 && slots[2].position[0] > 50.0);
    assert!(slots[3].position[0] < slots[1].position[0]);
}

#[test]
fn a_wedge_of_more_than_count_starts_a_second_wedge_behind() {
    let w = wedge_geometry(1.0, 0.785398185, 3);
    let slots = wedge_place(&frame(0.0), &w, &[2.0; 4]);
    assert_eq!(slots.len(), 4);
    assert_eq!(slots[3].position[0], 50.0);
    assert_eq!(slots[3].position[1], 60.0 - 2.0 * w.row_depth);
}

#[test]
fn a_circle_places_the_last_unit_first_and_faces_by_mode() {
    let circle = EeFormationCircle { mode: 1, half_arc: CIRCLE_HALF_ARC, min_radius: CIRCLE_MIN_RADIUS };
    let slots = circle_place(&frame(0.0), &circle, &[1.0; 4]);
    assert_eq!(slots.iter().map(|s| s.unit).collect::<Vec<_>>(), vec![3, 2, 1, 0]);
    for s in &slots {
        let d = ((s.position[0] - 50.0).powi(2) + (s.position[1] - 60.0).powi(2)).sqrt();
        assert!((d - 0.63661975).abs() < 1e-4, "{d}");
        assert_eq!(s.heading, heading_between(s.position, [50.0, 60.0, 2.0]));
    }
    let lone = circle_place(&frame(0.7), &circle, &[3.0]);
    assert_eq!(lone, vec![EeFormationSlot { position: [50.0, 60.0, 2.0], heading: 0.0, unit: 0 }]);
    assert!(circle_place(&frame(0.7), &circle, &[]).is_empty());
}

#[test]
fn every_create_type_has_a_preset() {
    assert!((0..8).all(|k| preset(k).is_some()));
    assert_eq!(preset(8), None);
    assert_eq!(preset(-1), None);
}
