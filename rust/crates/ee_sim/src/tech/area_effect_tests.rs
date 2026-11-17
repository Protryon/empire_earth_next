use super::*;

/// A host that logs every call and answers by op from a table (unset ops answer the default).
#[derive(Default)]
struct Host {
    log: Vec<EeAreaEffectCall>,
    answers: Vec<(EeAreaEffectOp, EeAreaEffectReply)>,
    continents: Vec<((i32, i32), i32)>,
}

impl Host {
    fn answer(mut self, op: EeAreaEffectOp, ok: bool, value: i32, x: f32) -> Self {
        self.answers.push((op, EeAreaEffectReply { ok, value, x, y: 0.0 }));
        self
    }
    fn position(mut self, x: f32, y: f32) -> Self {
        self.answers.push((EeAreaEffectOp::TargetPosition, EeAreaEffectReply { ok: true, value: 0, x, y }));
        self
    }
    fn continent(mut self, tile: (i32, i32), value: i32) -> Self {
        self.continents.push((tile, value));
        self
    }
    fn ops(&self) -> Vec<EeAreaEffectOp> {
        self.log.iter().map(|c| c.op).collect()
    }
}

impl AreaEffectHost for Host {
    fn call(&mut self, call: &EeAreaEffectCall) -> EeAreaEffectReply {
        self.log.push(*call);
        if call.op == EeAreaEffectOp::Continent {
            let v = self.continents.iter().find(|(t, _)| *t == (call.a, call.b)).map_or(0, |(_, v)| *v);
            return EeAreaEffectReply { ok: true, value: v, ..Default::default() };
        }
        self.answers.iter().rev().find(|(op, _)| *op == call.op).map(|(_, r)| *r).unwrap_or_default()
    }
}

fn template(kind: c_long) -> EeAreaEffectTemplateFields {
    EeAreaEffectTemplateFields { mKind: kind, mRadius: 4.0, mUnitSet: 4, mLevel: 3, mMaxLevel: 6, ..template_reset() }
}

fn effect(kind: c_long) -> EeAreaEffectFields {
    init(kind, 6, 2)
}

#[test]
fn the_template_constructor_and_init_fill_every_number() {
    let t = template_reset();
    assert_eq!((t.mShape, t.mMode, t.mRadius, t.mUnitSet, t.mLevel), (1, 2, 1.0, -1, -1));
    assert_eq!((t.mColorRed, t.mColorAlpha, t.mRed, t.mB64), (1.0, 0.15, -1.0, false));
    let mut d = EeAreaEffectData {
        mID: 9,
        mKind: 5,
        mShape: 2,
        mUnitSet: 7,
        mMode: 1,
        mMaxLevel: 4,
        mLevel: 2,
        mUnusedValue1: 11,
        mUnusedValue3: 13,
        mRadius: 6.5,
        mRed: 0.25,
        mGreen: 0.5,
        mBlue: 0.75,
        mAlpha: 0.3,
        mUnusedFloat: 1.5,
        mUnusedFlag2: true,
        ..EeAreaEffectData::default()
    };
    d.mName[0] = b'x' as c_char;
    let mut t = template_reset();
    template_init(&mut t, &d);
    assert_eq!((t.mKind, t.mID, t.mShape, t.mUnitSet, t.mMode), (5, 9, 2, 7, 1));
    assert_eq!((t.mMaxLevel, t.mLevel, t.mL44, t.mL48, t.mL4C), (4, 2, 11, 0, 13));
    assert_eq!((t.mRadius, t.mF60, t.mB65, t.mB64), (6.5, 1.5, true, false));
    assert_eq!((t.mColorRed, t.mColorGreen, t.mColorBlue, t.mColorAlpha), (0.25, 0.5, 0.75, 0.3));
}

#[test]
fn the_kinds_and_the_morale_group() {
    assert!(!known_kind(0) && known_kind(1) && known_kind(8) && !known_kind(9));
    let e = init(EE_AREA_EFFECT_MORALE_FROM_BUILDINGS, 6, 2);
    assert_eq!((e.mMax, e.mCount, e.mDirty), (8, 0, true));
    assert_eq!(init(EE_AREA_EFFECT_HEALING, 6, 2), EeAreaEffectFields { mKind: 1, ..Default::default() });
    assert!(same_group(2, 5) && same_group(6, 2) && same_group(1, 1) && !same_group(1, 2) && !same_group(7, 8));
    assert!(is_immunity(&effect(3)) && is_immunity(&effect(4)) && !is_immunity(&effect(1)));
}

#[test]
fn the_player_modes() {
    let mut t = template(1);
    let mut h = Host::default().answer(EeAreaEffectOp::Relation, true, EE_AREA_EFFECT_ALLIED, 0.0);
    t.mMode = 2;
    assert!(affects_player(&mut h, &t, 1, 2));
    t.mMode = 1;
    assert!(!affects_player(&mut h, &t, 1, 2) && affects_player(&mut h, &t, 2, 2));
    t.mMode = 3;
    assert!(!affects_player(&mut h, &t, 1, 2));
    let mut war = Host::default().answer(EeAreaEffectOp::Relation, true, EE_AREA_EFFECT_AT_WAR, 0.0);
    assert!(affects_player(&mut war, &t, 1, 2));
    let mut same =
        Host::default().answer(EeAreaEffectOp::Relation, true, EE_AREA_EFFECT_AT_WAR | EE_AREA_EFFECT_SAME, 0.0);
    assert!(!affects_player(&mut same, &t, 1, 1));
    t.mMode = 4;
    assert!(affects_player(&mut h, &t, 1, 2));
    // reversed: 1 the others, 2 the enemies, 3 the friends
    t.mMode = 1;
    assert!(affects_player_reversed(&mut h, &t, 1, 2) && !affects_player_reversed(&mut h, &t, 2, 2));
    t.mMode = 2;
    assert!(affects_player_reversed(&mut war, &t, 1, 2) && !affects_player_reversed(&mut h, &t, 1, 2));
    t.mMode = 3;
    assert!(affects_player_reversed(&mut same, &t, 1, 1) && !affects_player_reversed(&mut war, &t, 1, 2));
    t.mMode = 4;
    assert!(!affects_player_reversed(&mut h, &t, 1, 2));
}

#[test]
fn the_area_tests_and_the_continents() {
    let mut t = template(1);
    let c = Ge2DPoint { x: 10.5, y: 10.5 };
    let mut h = Host::default();
    // a square of 4 * 1.5 = 6
    assert!(is_within(&mut h, &t, c, Ge2DPoint { x: 16.5, y: 4.5 }, 1.5, false));
    assert!(!is_within(&mut h, &t, c, Ge2DPoint { x: 16.6, y: 10.5 }, 1.5, false));
    assert!(h.log.is_empty(), "unit set 4: no continent test");
    t.mShape = 2;
    assert!(!is_within(&mut h, &t, c, Ge2DPoint { x: 15.0, y: 15.0 }, 1.5, false));
    assert!(is_within(&mut h, &t, c, Ge2DPoint { x: 14.0, y: 14.0 }, 1.5, false));
    t.mShape = 3;
    assert!(!is_within(&mut h, &t, c, c, 1.0, false));
    t.mRadius = 0.0;
    assert!(is_within(&mut h, &t, c, Ge2DPoint { x: 99.0, y: 99.0 }, 1.0, false));
    // other continents: false, unless a carried dock is on that planet
    let mut t = template(1);
    t.mUnitSet = 3;
    let mut h = Host::default().continent((12, 9), 5).continent((10, 10), 6);
    assert!(!is_within(&mut h, &t, c, Ge2DPoint { x: 12.9, y: 9.2 }, 1.0, true));
    assert_eq!(h.ops(), [EeAreaEffectOp::Continent, EeAreaEffectOp::Continent, EeAreaEffectOp::TargetCarrierPlanet]);
    assert_eq!((h.log[0].a, h.log[0].b, h.log[1].a, h.log[1].b), (12, 9, 10, 10));
    let mut h = Host::default().continent((12, 9), 5).continent((10, 10), 6).answer(
        EeAreaEffectOp::TargetCarrierPlanet,
        true,
        5,
        0.0,
    );
    assert!(is_within(&mut h, &t, c, Ge2DPoint { x: 12.9, y: 9.2 }, 1.0, true));
    let mut h = Host::default().continent((10, 10), 6);
    assert!(is_within(&mut h, &t, c, Ge2DPoint { x: 12.9, y: 9.2 }, 1.0, true), "continent 0: no second test");
    assert_eq!(h.log.len(), 1);
}

#[test]
fn strength_level_and_the_modifiers_by_kind() {
    let t = template(1);
    let mut h = Host::default()
        .answer(EeAreaEffectOp::OwnerHitPoints, true, 5, 0.0)
        .answer(EeAreaEffectOp::OwnerLeadership, true, 0, 4.5)
        .answer(EeAreaEffectOp::OwnerGarrisonBonus, true, 0, 1.25);
    assert_eq!(strength(&mut h, &effect(1), &t), 3);
    assert_eq!(strength(&mut h, &effect(2), &t), 3);
    assert_eq!(strength(&mut h, &effect(3), &t), 0);
    assert_eq!(strength(&mut h, &effect(8), &t), 6);
    assert_eq!(strength(&mut h, &effect(6), &t), 4, "lrintf(4.5) = 4");
    assert_eq!(strength(&mut h, &effect(7), &t), 12);
    let mut m = effect(5);
    m.mCount = 11;
    assert_eq!(level(&mut h, &m, &t), 8);
    m.mCount = 5;
    assert_eq!(strength(&mut h, &m, &t), 5);
    // damage less a tenth per level, the gather amount by the rate, the cloaking's strength
    assert_eq!(modify(&mut h, &m, &t, 1, 1000), Some(500));
    assert_eq!(modify(&mut h, &m, &t, 2, 1000), None);
    assert_eq!(modify(&mut h, &effect(7), &t, 2, 1000), Some(1250));
    assert_eq!(modify(&mut h, &effect(8), &t, 3, 1000), Some(6));
    assert_eq!(modify(&mut h, &effect(1), &t, 1, 1000), None);
    m.mCount = 30;
    m.mMax = 30;
    assert_eq!(modify(&mut h, &m, &t, 1, 37), Some(0), "the level capped at 10");
    m.mCount = -3;
    assert_eq!(modify(&mut h, &m, &t, 1, 37), Some(37), "and at 0");
    let mut dead = Host::default().answer(EeAreaEffectOp::OwnerLeadership, true, 0, 4.5);
    assert_eq!(level(&mut dead, &effect(6), &t), 0);
    assert_eq!(dead.ops(), [EeAreaEffectOp::OwnerHitPoints]);
    let mut big = Host::default().answer(EeAreaEffectOp::OwnerGarrisonBonus, true, 0, 30.0);
    assert_eq!(strength(&mut big, &effect(7), &t), 100);
    let mut low = Host::default().answer(EeAreaEffectOp::OwnerGarrisonBonus, true, 0, -2.0);
    assert_eq!(modify(&mut low, &effect(7), &t, 2, 1000), Some(0));
}

#[test]
fn covers_by_kind_in_the_originals_order() {
    let mut t = template(2);
    t.mUnitSet = 9;
    let point = Ge2DPoint { x: 10.0, y: 10.0 };
    let all = || {
        Host::default()
            .answer(EeAreaEffectOp::TargetInSet, true, 0, 0.0)
            .answer(EeAreaEffectOp::Relation, true, EE_AREA_EFFECT_ALLIED, 0.0)
            .answer(EeAreaEffectOp::TargetBuilt, true, 0, 0.0)
            .answer(EeAreaEffectOp::TargetRepairable, true, 0, 0.0)
            .answer(EeAreaEffectOp::OwnerScale, true, 0, 2.0)
            .position(17.0, 10.0)
    };
    use EeAreaEffectOp::*;
    let mut h = all();
    assert!(covers(&mut h, &effect(2), &t, 1, 2, point));
    assert_eq!(h.ops(), [OwnerScale, TargetInSet, Relation, TargetBuilt, TargetPosition, Continent]);
    assert_eq!(h.log[1].a, 9);
    let mut h = all();
    assert!(covers(&mut h, &effect(1), &t, 1, 2, point));
    assert_eq!(h.ops()[..2], [TargetRepairable, OwnerScale]);
    let mut h = all().answer(TargetRepairable, false, 0, 0.0);
    assert!(!covers(&mut h, &effect(1), &t, 1, 2, point));
    assert_eq!(h.ops(), [TargetRepairable]);
    // the cloaking: no built test, the tech node test, the scale after the position
    let mut h = all().answer(TargetBuilt, false, 0, 0.0);
    assert!(covers(&mut h, &effect(8), &t, 1, 2, point));
    assert_eq!(h.ops(), [TargetInSet, TargetTechKind4, Relation, TargetPosition, OwnerScale, Continent]);
    let mut h = all().answer(TargetTechKind4, true, 0, 0.0);
    assert!(!covers(&mut h, &effect(8), &t, 1, 2, point));
    // outside the doubled square, not built
    let mut h = all().position(22.5, 10.0);
    assert!(!covers(&mut h, &effect(2), &t, 1, 2, point));
    let mut h = all().answer(TargetBuilt, false, 0, 0.0);
    assert!(!covers(&mut h, &effect(5), &t, 1, 2, point));
    // radius 0: everyone in the set
    t.mRadius = 0.0;
    let mut h = all();
    assert!(covers(&mut h, &effect(3), &t, 1, 2, point));
    assert!(!h.ops().contains(&TargetPosition));
}

#[test]
fn the_world_steps_by_kind() {
    use EeAreaEffectOp::*;
    let t = template(1);
    let mut h = Host::default();
    applied_to(&mut h, &effect(8));
    removed_from(&mut h, &effect(8));
    applied_to(&mut h, &effect(1));
    removed_from(&mut h, &effect(5));
    assert_eq!(h.ops(), [TargetCloak, TargetUncloak]);
    // healing: level 3 per second of 2000 ms = 6, capped at the maximum, at least 1
    let mut h = Host::default().answer(TargetHitPoints, true, 50, 0.0).answer(TargetMaxHitPoints, true, 100, 0.0);
    tick(&mut h, &effect(1), &t, 2000.0);
    assert_eq!(h.ops(), [TargetHitPoints, InCinematic, TargetMaxHitPoints, TargetSetHitPoints]);
    assert_eq!(h.log[3].a, 56);
    let mut h = Host::default().answer(TargetHitPoints, true, 98, 0.0).answer(TargetMaxHitPoints, true, 100, 0.0);
    tick(&mut h, &effect(1), &t, 100.0);
    assert_eq!(h.log[3].a, 99, "at least 1");
    tick(&mut h, &effect(1), &t, 9000.0);
    assert_eq!(h.log[7].a, 100);
    let mut h = Host::default().answer(TargetHitPoints, true, 0, 0.0);
    tick(&mut h, &effect(1), &t, 2000.0);
    assert_eq!(h.ops(), [TargetHitPoints]);
    let mut h = Host::default().answer(TargetHitPoints, true, 5, 0.0).answer(InCinematic, true, 0, 0.0);
    tick(&mut h, &effect(1), &t, 2000.0);
    assert_eq!(h.ops(), [TargetHitPoints, InCinematic]);
    let mut h = Host::default();
    tick(&mut h, &effect(2), &t, 2000.0);
    assert!(h.log.is_empty());
}

#[test]
fn the_building_count() {
    use EeAreaEffectOp::*;
    let mut t = template(5);
    t.mRadius = 3.0;
    let mut e = effect(5);
    // the tile's middle: the rectangle one larger to the right and the bottom
    let mut h = Host::default().answer(OwnerScale, true, 0, 1.0).answer(CountBuildings, true, 12, 0.0);
    recount(&mut h, &mut e, &t, Ge2DPoint { x: 20.5, y: 30.5 });
    let c = h.log[1];
    assert_eq!((c.a, c.b, c.c, c.d, c.e), (17, 27, 23, 33, 1));
    assert_eq!((e.mCount, e.mDirty), (8, false), "capped at the maximum");
    // not flagged: nothing counted (the radius still read)
    let mut h = Host::default();
    recount(&mut h, &mut e, &t, Ge2DPoint { x: 20.5, y: 30.5 });
    assert_eq!(h.ops(), [OwnerScale]);
    mark_dirty(&mut e);
    assert!(e.mDirty);
    // the tile's corner, a circle: the range squared from the tile
    t.mShape = 2;
    let mut h = Host::default().answer(OwnerScale, true, 0, 1.5).answer(CountBuildings, true, 3, 0.0);
    recount(&mut h, &mut e, &t, Ge2DPoint { x: 20.0, y: 30.9 });
    let c = h.log[2];
    assert_eq!((c.a, c.b, c.c, c.d, c.e, c.f, c.g, c.x), (16, 26, 23, 33, 2, 20, 30, 20.25));
    assert_eq!(e.mCount, 3);
    // a negative cap counts none
    let mut n = effect(5);
    n.mMax = -1;
    let mut h = Host::default().answer(CountBuildings, true, 3, 0.0);
    recount(&mut h, &mut n, &template(5), Ge2DPoint { x: 1.0, y: 1.0 });
    assert_eq!(n.mCount, 0);
    let mut h = Host::default();
    let mut other = effect(2);
    recount(&mut h, &mut other, &t, Ge2DPoint { x: 1.0, y: 1.0 });
    mark_dirty(&mut other);
    assert!(h.log.is_empty() && !other.mDirty);
}

#[test]
fn the_checksum_sums_the_bits_as_long() {
    let mut e = effect(1);
    e.mPlayer = 2;
    let x = 1.5f32.to_bits() as i32 as c_long;
    let y = (-2.0f32).to_bits() as i32 as c_long;
    assert_eq!(checksum(&e, Ge2DPoint { x: 1.5, y: -2.0 }, 7), x.wrapping_add(y).wrapping_add(9));
    assert_eq!(scale(&mut Host::default()), 1.0);
    assert_eq!(radius(&mut Host::default().answer(EeAreaEffectOp::OwnerScale, true, 0, 2.5), &template(1)), 10.0);
}
