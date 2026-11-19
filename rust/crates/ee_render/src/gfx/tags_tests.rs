//! tags.rs' tests over fake models: the part scan's lists, points and colour groups as the C++ built them, the
//! points' geometry through the parent chain, the pick, the scale and the release order of the references.

use super::*;
use std::cell::RefCell;
use std::collections::BTreeMap;

#[derive(Clone, Default)]
struct FakeModel {
    tags: Vec<(Vec<u8>, Vec<Ge3DPoint>)>,
    internal: Vec<GeTransformation>,
    parent: usize,
    subs: Vec<usize>,
    frames: c_ulong,
    top_z: f32,
    materials: Vec<Vec<u8>>,
}

#[derive(Default)]
struct Fakes {
    models: BTreeMap<usize, FakeModel>,
    refs: RefCell<BTreeMap<usize, i32>>,
    log: RefCell<Vec<String>>,
}

impl TagModels for Fakes {
    fn tag_count(&self, m: usize) -> usize {
        self.models[&m].tags.len()
    }
    fn tag_name(&self, m: usize, i: usize) -> Vec<u8> {
        self.models[&m].tags[i].0.clone()
    }
    fn tag_point(&self, m: usize, i: usize, frame: usize) -> Ge3DPoint {
        self.models[&m].tags[i].1[frame]
    }
    fn internal(&self, m: usize, frame: usize) -> GeTransformation {
        self.models[&m].internal[frame]
    }
    fn parent(&self, m: usize) -> usize {
        self.models[&m].parent
    }
    fn sub_models(&self, m: usize) -> Vec<usize> {
        self.models[&m].subs.clone()
    }
    fn frame_count(&self, m: usize) -> c_ulong {
        self.models[&m].frames
    }
    fn bounding_box(&self, m: usize) -> GeBoundingBox {
        GeBoundingBox { top_right_back: Ge3DPoint { x: 9.0, y: 9.0, z: self.models[&m].top_z }, ..Default::default() }
    }
    fn material_names(&self, m: usize) -> Vec<Vec<u8>> {
        self.models[&m].materials.clone()
    }
    fn set_material_color(&self, m: usize, i: usize, color: [f32; 3]) {
        self.log.borrow_mut().push(format!("color {m:x} {i} {:?}", color));
    }
    fn reference(&self, m: usize) {
        *self.refs.borrow_mut().entry(m).or_default() += 1;
    }
    fn dereference(&self, m: usize) {
        *self.refs.borrow_mut().entry(m).or_default() -= 1;
        self.log.borrow_mut().push(format!("deref {m:x}"));
    }
}

fn p(x: f32, y: f32, z: f32) -> Ge3DPoint {
    Ge3DPoint { x, y, z }
}

fn moved(x: f32, y: f32, z: f32) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
    t
}

fn tag(name: &str, points: &[Ge3DPoint]) -> (Vec<u8>, Vec<Ge3DPoint>) {
    (name.as_bytes().to_vec(), points.to_vec())
}

/// A root model 0x100 (two frames) with a sub-model 0x200 moved by (1, 2, 3).
fn world() -> Fakes {
    let mut f = Fakes::default();
    f.models.insert(
        0x100,
        FakeModel {
            tags: vec![
                tag("weapon_mount1", &[p(1.0, 0.0, 0.0), p(1.5, 0.0, 0.0)]),
                tag("attack", &[p(0.0, 0.0, 2.0), p(0.0, 0.0, 4.0)]),
                tag("2dfire", &[p(0.5, 0.5, 0.5), p(0.5, 0.5, 0.5)]),
            ],
            internal: vec![GeTransformation::IDENTITY, moved(0.0, 0.0, 1.0)],
            subs: vec![0x200],
            frames: 2,
            top_z: 3.0,
            materials: vec![b"body".to_vec(), b"player color".to_vec(), b"player color".to_vec()],
            ..Default::default()
        },
    );
    f.models.insert(
        0x200,
        FakeModel {
            tags: vec![
                tag("status bar", &[p(0.0, 0.0, 5.0), p(0.0, 0.0, 6.0)]),
                tag("weapon_mount2", &[p(2.0, 0.0, 0.0), p(2.0, 0.0, 0.0)]),
            ],
            internal: vec![moved(1.0, 2.0, 3.0), moved(1.0, 2.0, 3.0)],
            parent: 0x100,
            frames: 2,
            materials: vec![b"player color".to_vec()],
            ..Default::default()
        },
    );
    f
}

fn kind_of(name: &[u8]) -> c_long {
    ee_data::tables::tag_point_kinds(name)[0]
}

#[test]
fn the_scan_files_each_point_under_its_root_and_kind() {
    let f = world();
    let mut tags = EeGfxGraphicTags::default();
    let mut out = ScanPoints::default();
    tags.collect(&f, 0, 0x100, &GeTransformation::IDENTITY, 0x100, 1, &mut out);
    // both models' weapon mounts in part 0's list, in scan order (the root's first)
    let w = tags.weapon(0).expect("the weapon list");
    assert_eq!(w.len, 2);
    assert_eq!((w.point(0).mModel as usize, w.point(0).mIndex), (0x100, 0));
    assert_eq!((w.point(1).mModel as usize, w.point(1).mIndex), (0x200, 1));
    // every kind's list is under the root model, also the sub-model's points
    let fire = kind_of(b"2dfire");
    assert_eq!(tags.find(0x100, fire).expect("fire").len, 1);
    assert!(tags.find(0x200, fire).is_none());
    // a weapon mount is also of the weapon_mount kinds, the sub-model's under the root
    assert_eq!(tags.find(0x100, kind_of(b"weapon_mount")).expect("mounts").len, 2);
    // the attack point and its top at frame 1; the status bar of the sub-model under its transformation
    assert_eq!(out.default_point, p(0.0, 0.0, 2.0));
    assert_eq!(out.top, p(0.0, 0.0, 4.0));
    assert_eq!(out.bar, p(1.0, 2.0, 8.0));
    assert_eq!(tags.pick(0), Some(&p(0.0, 0.0, 2.0)));
    assert_eq!(tags.pick(7), Some(&p(0.0, 0.0, 2.0)));
    // one reference per point
    let mut points = tags.weapon(0).map_or(0, |l| l.len);
    for lists in tags.kinds.values() {
        points += lists.values().map(|l| l.len).sum::<usize>();
    }
    assert_eq!(f.refs.borrow().values().sum::<i32>(), points as i32);
}

#[test]
fn a_root_without_a_status_bar_takes_its_box_top_and_never_lowers_the_bar() {
    let mut f = world();
    f.models.get_mut(&0x100).unwrap().subs.clear();
    let mut tags = EeGfxGraphicTags::default();
    let mut out = ScanPoints { bar: p(7.0, 7.0, 10.0), ..Default::default() };
    tags.collect(&f, 0, 0x100, &moved(0.0, 0.0, 1.0), 0x100, 5, &mut out);
    // the box's top 3, raised by the parent's 1: below the bar's 10, which stays
    assert_eq!(out.bar, p(7.0, 7.0, 10.0));
    // last5 (5) is not below the frame count (2): no top point
    assert_eq!(out.top, Ge3DPoint::default());
    let mut out = ScanPoints::default();
    let mut tags = EeGfxGraphicTags::default();
    tags.collect(&f, 0, 0x100, &moved(0.0, 0.0, 1.0), 0x100, 5, &mut out);
    assert_eq!(out.bar.z, 4.0);
    assert_eq!(out.bar.x, 0.0);
}

#[test]
fn later_parts_skip_the_kinds_of_part_0s_model_but_keep_their_mounts() {
    let f = world();
    let mut tags = EeGfxGraphicTags::default();
    let mut out = ScanPoints::default();
    tags.collect(&f, 3, 0x100, &GeTransformation::IDENTITY, 0x100, 1, &mut out);
    assert!(tags.kinds.values().all(|l| l.is_empty()));
    assert_eq!(tags.weapon(3).expect("part 3's mounts").len, 2);
    assert!(tags.weapon(0).is_none());
}

#[test]
fn the_colour_groups_take_each_models_player_colour_materials() {
    let f = world();
    let mut tags = EeGfxGraphicTags::default();
    let mut out = ScanPoints::default();
    tags.collect(&f, 2, 0x100, &GeTransformation::IDENTITY, 0x100, 1, &mut out);
    tags.set_color(&f, 2, [0.25, 0.5, 1.0]);
    tags.set_color(&f, 0, [1.0, 1.0, 1.0]);
    assert_eq!(
        *f.log.borrow(),
        vec!["color 100 1 [0.25, 0.5, 1.0]", "color 100 2 [0.25, 0.5, 1.0]", "color 200 0 [0.25, 0.5, 1.0]"]
    );
}

#[test]
fn a_point_goes_through_its_part_its_model_chain_and_the_location() {
    let f = world();
    let point = EeGfxTagPoint { mModel: 0x200 as *mut c_void, mIndex: 1 };
    // frame 1: (2, 0, 0) by the sub-model (1, 2, 3), then the root's (0, 0, 1)
    assert_eq!(offset(&f, &point, 1, None), p(3.0, 2.0, 4.0));
    let mut turn = GeTransformation::IDENTITY;
    turn.m[0][0] = 0.0;
    turn.m[0][1] = -1.0;
    turn.m[1][0] = 1.0;
    turn.m[1][1] = 0.0;
    // the part turns (2, 0, 0) to (0, 2, 0) first
    assert_eq!(offset(&f, &point, 1, Some(&turn)), p(1.0, 4.0, 4.0));
    assert_eq!(point_at(&f, &point, 0, &moved(10.0, 0.0, 0.0), None), p(13.0, 2.0, 3.0));
}

#[test]
fn the_scale_multiplies_the_rotation_only() {
    let mut t = moved(4.0, 5.0, 6.0);
    t.m[0][1] = 2.0;
    scale(&mut t, 0.5);
    assert_eq!(t.m[0], [0.5, 1.0, 0.0, 4.0]);
    assert_eq!(t.m[1], [0.0, 0.5, 0.0, 5.0]);
    assert_eq!(t.m[2], [0.0, 0.0, 0.5, 6.0]);
    assert_eq!(t.m[3], [0.0, 0.0, 0.0, 1.0]);
}

#[test]
fn the_release_dereferences_the_part_lists_then_the_kinds_in_key_order() {
    let f = world();
    let mut tags = EeGfxGraphicTags::default();
    let mut out = ScanPoints::default();
    tags.collect(&f, 0, 0x100, &GeTransformation::IDENTITY, 0x100, 1, &mut out);
    let held: i32 = f.refs.borrow().values().sum();
    assert!(held >= 2);
    tags.clear_weapon(&f, 0);
    assert_eq!(f.log.borrow()[..2], ["deref 100".to_string(), "deref 200".to_string()]);
    assert!(tags.weapon(0).is_none());
    tags.clear(&f);
    assert!(f.refs.borrow().values().all(|&n| n == 0));
    assert_eq!(tags.pick(3), None);
    assert!(tags.find(0x100, kind_of(b"2dfire")).is_none());
}

#[test]
fn the_list_is_the_c_vector_layout() {
    let f = world();
    let mut list = EeGfxTagList::new();
    for i in 0..5 {
        list.add(&f, 0x100, i);
    }
    assert_eq!(list.len, 5);
    assert!(list.cap >= 5);
    // SAFETY: the list's own points
    let third = unsafe { &**list.points.add(2) };
    assert_eq!(third.mIndex, 2);
    list.clear(&f);
    assert_eq!(list.len, 0);
    assert_eq!(f.refs.borrow()[&0x100], 0);
}

#[test]
fn no_store_answers_none() {
    // SAFETY: NULL stores
    unsafe {
        assert!(ee_gfx_tags_find(std::ptr::null(), 0x100 as *const c_void, 0).is_null());
        assert!(ee_gfx_tags_pick(std::ptr::null(), 0).is_null());
        ee_gfx_tags_free(std::ptr::null_mut());
    }
}
