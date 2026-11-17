use super::*;
use crate::tech::calamity_class::EE_CALAMITY_FUNCTOR_NONE;

fn class() -> EeCalamityClassFields {
    EeCalamityClassFields {
        mUnitSet: 12,
        mIsBlockedByImmunity: true,
        mFunctorFlag: true,
        mDamage: 7,
        mDamageFactor: 3,
        mBurn: 25,
        mSpreadTime: 1500,
        mRadius: 2.5,
        mChance: 0.25,
        ..EeCalamityClassFields::default()
    }
}

#[test]
fn each_kind_keeps_what_its_constructor_stored() {
    let c = class();
    let fire = init(&c, EE_CALAMITY_FUNCTOR_FIRE);
    assert_eq!((fire.mBurn, fire.mDamage, fire.mSpreadTime, fire.mRadius, fire.mChance), (25, 7, 1500, 2.5, 0.25));
    assert_eq!((fire.mUnitSet, fire.mMode, fire.mUpdates, fire.mFlag), (12, 2, 0, false));
    let plague = init(&c, EE_CALAMITY_FUNCTOR_PLAGUE);
    assert_eq!((plague.mBurn, plague.mDamage, plague.mSpreadTime, plague.mRadius), (0, 7, 1500, 2.5));
    let leprosy = init(&c, EE_CALAMITY_FUNCTOR_LEPROSY);
    assert_eq!((leprosy.mDamage, leprosy.mSpreadTime, leprosy.mRadius, leprosy.mChance), (7, 1500, 0.0, 0.0));
    let area = init(&c, EE_CALAMITY_FUNCTOR_AREA);
    assert_eq!((area.mDamage, area.mSpreadTime, area.mFlag), (3, 1500, false));
    // the mind control and the extra damage pass no damage and no update time to their area effect base
    let extra = init(&c, EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE);
    assert_eq!((extra.mDamage, extra.mSpreadTime, extra.mFlag, extra.mUnitSet), (0, 0, true, 12));
    let mind = init(&c, EE_CALAMITY_FUNCTOR_MIND_CONTROL);
    assert_eq!((mind.mDamage, mind.mSpreadTime, mind.mFlag), (0, 0, false));
    let open = init(&EeCalamityClassFields { mIsBlockedByImmunity: false, ..c }, EE_CALAMITY_FUNCTOR_AREA);
    assert_eq!(open.mMode, 3);
}

#[test]
fn the_getters_are_each_kinds_override_or_the_bases_default() {
    let c = class();
    let fire = params(&init(&c, EE_CALAMITY_FUNCTOR_FIRE), false);
    assert_eq!(
        fire,
        EeContagionParams { spread_time: 1500, chance: 0.25, radius: 2.5, flags: EE_CONTAGION_ALLIES_ONLY }
    );
    // the plague tries every second and infects enemies too
    let plague = params(&init(&c, EE_CALAMITY_FUNCTOR_PLAGUE), true);
    assert_eq!(
        plague,
        EeContagionParams { spread_time: 1000, chance: 0.25, radius: 2.5, flags: EE_CONTAGION_BLOCKED_BY_IMMUNITY }
    );
    for kind in [
        EE_CALAMITY_FUNCTOR_LEPROSY,
        EE_CALAMITY_FUNCTOR_AREA,
        EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE,
        EE_CALAMITY_FUNCTOR_MIND_CONTROL,
    ] {
        let p = params(&init(&c, kind), false);
        assert_eq!(
            p,
            EeContagionParams {
                spread_time: 0,
                chance: 0.0,
                radius: 0.0,
                flags: EE_CONTAGION_SKIPS_SPREAD | EE_CONTAGION_ALLIES_ONLY
            },
            "kind {kind}"
        );
    }
    assert_eq!(params(&init(&c, EE_CALAMITY_FUNCTOR_NONE), false).flags, EE_CONTAGION_ALLIES_ONLY);
}

#[test]
fn the_discrete_time_converts_as_the_members_type() {
    let mut f = init(&class(), EE_CALAMITY_FUNCTOR_AREA);
    assert_eq!(discrete_time(&f), 1500.0);
    f.mSpreadTime = (-2i32) as c_ulong;
    assert_eq!(discrete_time(&f), -2.0);
    f.mKind = EE_CALAMITY_FUNCTOR_FIRE;
    assert_eq!(discrete_time(&f), 4_294_967_294u32 as f32);
}

fn hit(damage: i32) -> EeContagionFunctorHit {
    EeContagionFunctorHit { hits: true, damage }
}

#[test]
fn apply_damages_by_kind_only_after_a_discrete_update() {
    let c = class();
    let mut fire = init(&c, EE_CALAMITY_FUNCTOR_FIRE);
    assert_eq!(apply_damage(&fire, 100, 1.0), EeContagionFunctorHit::default());
    fire.mUpdates = 3;
    assert_eq!(apply_damage(&fire, 100, 1.0), hit(21));
    // the fire and the plague hit even for nothing
    fire.mDamage = 0;
    assert_eq!(apply_damage(&fire, 100, 1.0), hit(0));
    let mut plague = init(&c, EE_CALAMITY_FUNCTOR_PLAGUE);
    plague.mUpdates = 2;
    assert_eq!(apply_damage(&plague, 1, 1.0), hit(14));
    let mut leprosy = init(&c, EE_CALAMITY_FUNCTOR_LEPROSY);
    leprosy.mUpdates = 2;
    assert_eq!(apply_damage(&leprosy, 100, 1.0), hit(14));
    assert_eq!(apply_damage(&leprosy, 10, 1.0), hit(9));
    assert_eq!(apply_damage(&leprosy, 2, 1.0), EeContagionFunctorHit::default());
    let mut area = init(&c, EE_CALAMITY_FUNCTOR_AREA);
    area.mUpdates = 2;
    assert_eq!(apply_damage(&area, 100, 1.5), hit(9));
    assert_eq!(apply_damage(&area, 100, 0.1), EeContagionFunctorHit::default());
    for kind in [EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE, EE_CALAMITY_FUNCTOR_MIND_CONTROL] {
        let mut f = init(&c, kind);
        f.mUpdates = 5;
        assert_eq!(apply_damage(&f, 100, 1.0), EeContagionFunctorHit::default());
    }
}
