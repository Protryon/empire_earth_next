//! `EEUnit::Describe` and `DescribeLong` (the units' lane, round 9; were EEUnit.cpp): the debug texts. Every C++
//! statement of the originals is one operation of the unit's host ([`EumHost::eum`], `ee_units_eu_host`), in the
//! original's order; the text is the host's (`theString`, a `UWideString` the statements fill and append to
//! `text`).

use crate::units::eu_more::{EumHost, EE_UNITS_EUM_HAS_COMPLEX};

// ---- the statements (`op`; the answer). `a` is the text appended to.
/// `theString = UWideString()` (the originals' fresh local)
pub const EE_UNITS_EUM_TEXT_NEW: i32 = 4000;
/// `RTSUnit::Describe(theString)`
pub const EE_UNITS_EUM_TEXT_RTS_DESCRIBE: i32 = 4001;
/// `text a += theString`
pub const EE_UNITS_EUM_TEXT_APPEND: i32 = 4002;
/// `sprintf(theString, "res %ld / %ld:", mResourceType, mResourceAmount)`
pub const EE_UNITS_EUM_TEXT_RESOURCE: i32 = 4003;
/// `sprintf(theString, "obj %ld:", mUnitClass->mUnitType)`
pub const EE_UNITS_EUM_TEXT_OBJECT: i32 = 4004;
/// `sprintf(theString, "hp %ld:", mHitPoints)`
pub const EE_UNITS_EUM_TEXT_HIT_POINTS: i32 = 4005;
/// `sprintf(theString, "\n======] SIMPLE UNIT [======\n")`
pub const EE_UNITS_EUM_TEXT_SIMPLE: i32 = 4006;
/// `RTSUnit::DescribeLong(theString)`
pub const EE_UNITS_EUM_TEXT_RTS_DESCRIBE_LONG: i32 = 4007;
/// `sprintf(theString, "Object Type and Name: %ld, %s\n", the class's type and name)`
pub const EE_UNITS_EUM_TEXT_CLASS: i32 = 4008;
/// `mResourceType != kResourceNone`
pub const EE_UNITS_EUM_HAS_RESOURCE: i32 = 4009;
/// `sprintf(theString, "Resource Type/Amount:%ld / %ld\n", mResourceType, mResourceAmount)`
pub const EE_UNITS_EUM_TEXT_RESOURCE_LONG: i32 = 4010;
/// `sprintf(theString, "Health: %ld / %ld (%.2f%%)\n", ...)`
pub const EE_UNITS_EUM_TEXT_HEALTH: i32 = 4011;

/// `Describe(text)`: the base text, the resource type and amount, the class type and the hit points.
pub fn describe<H: EumHost + ?Sized>(h: &mut H, unit: u64, text: u64) {
    let mut q = |op: i32| h.eum(op, unit, text, 0, 0);
    q(EE_UNITS_EUM_TEXT_NEW);
    q(EE_UNITS_EUM_TEXT_RTS_DESCRIBE);
    q(EE_UNITS_EUM_TEXT_APPEND);
    for op in [EE_UNITS_EUM_TEXT_RESOURCE, EE_UNITS_EUM_TEXT_OBJECT, EE_UNITS_EUM_TEXT_HIT_POINTS] {
        q(op);
        q(EE_UNITS_EUM_TEXT_APPEND);
    }
}

/// `DescribeLong(text)`: a simple unit's header, the base long text, a simple unit's class, the resource (if any) and
/// the health.
pub fn describe_long<H: EumHost + ?Sized>(h: &mut H, unit: u64, text: u64) {
    let mut q = |op: i32| h.eum(op, unit, text, 0, 0);
    q(EE_UNITS_EUM_TEXT_NEW);
    if q(EE_UNITS_EUM_HAS_COMPLEX) == 0 {
        q(EE_UNITS_EUM_TEXT_SIMPLE);
        q(EE_UNITS_EUM_TEXT_APPEND);
    }
    q(EE_UNITS_EUM_TEXT_RTS_DESCRIBE_LONG);
    q(EE_UNITS_EUM_TEXT_APPEND);
    if q(EE_UNITS_EUM_HAS_COMPLEX) == 0 {
        q(EE_UNITS_EUM_TEXT_CLASS);
        q(EE_UNITS_EUM_TEXT_APPEND);
    }
    if q(EE_UNITS_EUM_HAS_RESOURCE) != 0 {
        q(EE_UNITS_EUM_TEXT_RESOURCE_LONG);
        q(EE_UNITS_EUM_TEXT_APPEND);
    }
    q(EE_UNITS_EUM_TEXT_HEALTH);
    q(EE_UNITS_EUM_TEXT_APPEND);
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::eu_more::game::Game;
    use std::os::raw::c_void;

    /// `EEUnit::Describe(text)` (`long` false) or `DescribeLong(text)` on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEUnit`, `text` a live `UWideString`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_eu_describe(unit: *mut c_void, text: *mut c_void, long: bool) {
        let (u, t) = (unit as usize as u64, text as usize as u64);
        if long {
            describe_long(&mut Game, u, t)
        } else {
            describe(&mut Game, u, t)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// records the statements; a simple unit with a resource unless told otherwise
    struct Rec {
        log: Vec<i32>,
        complex: i64,
        resource: i64,
    }

    impl EumHost for Rec {
        fn eum(&mut self, op: i32, unit: u64, a: u64, _b: i64, _c: i64) -> i64 {
            assert_eq!((unit, a), (1, 9));
            self.log.push(op);
            match op {
                EE_UNITS_EUM_HAS_COMPLEX => self.complex,
                EE_UNITS_EUM_HAS_RESOURCE => self.resource,
                _ => 0,
            }
        }
    }

    const APPEND: i32 = EE_UNITS_EUM_TEXT_APPEND;

    #[test]
    fn describe_appends_base_resource_object_and_hit_points() {
        let mut r = Rec { log: vec![], complex: 0, resource: 0 };
        describe(&mut r, 1, 9);
        assert_eq!(
            r.log,
            [
                EE_UNITS_EUM_TEXT_NEW,
                EE_UNITS_EUM_TEXT_RTS_DESCRIBE,
                APPEND,
                EE_UNITS_EUM_TEXT_RESOURCE,
                APPEND,
                EE_UNITS_EUM_TEXT_OBJECT,
                APPEND,
                EE_UNITS_EUM_TEXT_HIT_POINTS,
                APPEND
            ]
        );
    }

    #[test]
    fn describe_long_heads_and_names_a_simple_unit() {
        let mut r = Rec { log: vec![], complex: 0, resource: 1 };
        describe_long(&mut r, 1, 9);
        assert_eq!(
            r.log,
            [
                EE_UNITS_EUM_TEXT_NEW,
                EE_UNITS_EUM_HAS_COMPLEX,
                EE_UNITS_EUM_TEXT_SIMPLE,
                APPEND,
                EE_UNITS_EUM_TEXT_RTS_DESCRIBE_LONG,
                APPEND,
                EE_UNITS_EUM_HAS_COMPLEX,
                EE_UNITS_EUM_TEXT_CLASS,
                APPEND,
                EE_UNITS_EUM_HAS_RESOURCE,
                EE_UNITS_EUM_TEXT_RESOURCE_LONG,
                APPEND,
                EE_UNITS_EUM_TEXT_HEALTH,
                APPEND
            ]
        );
        let mut r = Rec { log: vec![], complex: 1, resource: 0 };
        describe_long(&mut r, 1, 9);
        assert_eq!(
            r.log,
            [
                EE_UNITS_EUM_TEXT_NEW,
                EE_UNITS_EUM_HAS_COMPLEX,
                EE_UNITS_EUM_TEXT_RTS_DESCRIBE_LONG,
                APPEND,
                EE_UNITS_EUM_HAS_COMPLEX,
                EE_UNITS_EUM_HAS_RESOURCE,
                EE_UNITS_EUM_TEXT_HEALTH,
                APPEND
            ]
        );
    }
}
