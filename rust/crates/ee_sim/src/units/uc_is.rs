//! The unit class's kind predicates (the units' lane, round 10): `EEUnitClass::IsDropSite`, `IsCrusader`,
//! `IsTransport`, `IsAirBase` and `IsPriest` over the class's record (their C++ bodies were in EEARust.cpp and
//! EECPEngineHelpers.cpp; ee/EEUnit.h calls [`ee_units_uc_is`] inline). The land-building test is the kind table's
//! (`IsLandBuilding`, uu_host.rs's class record), read only where the C++ called it.

use std::os::raw::c_long;

use crate::unit_ids::EEUnitCategory::*;
use crate::unit_ids::EEUnitTypeID::*;
use crate::units::unit_class_fields::EeUnitsUnitClassFields;

/// `IsDropSite()`: units can drop resources off at the class.
pub const EE_UNITS_UC_IS_DROP_SITE: i32 = 0;
/// `IsCrusader()`: one of the four crusader types.
pub const EE_UNITS_UC_IS_CRUSADER: i32 = 1;
/// `IsTransport()`: a land, helicopter, naval or space transport or a fortress.
pub const EE_UNITS_UC_IS_TRANSPORT: i32 = 2;
/// `IsAirBase()`: air traffic on a building, naval building or space building.
pub const EE_UNITS_UC_IS_AIR_BASE: i32 = 3;
/// `IsPriest()`: a land, naval, helicopter or building priest.
pub const EE_UNITS_UC_IS_PRIEST: i32 = 4;

/// The predicate `which` of the class record `c`; `land_building` its `IsLandBuilding()` (asked only by the drop
/// site's test, after its unit type test, as the C++'s `||` did).
#[allow(clippy::unnecessary_cast)] // c_ulong is 64 bits off Windows
pub fn is(which: i32, c: &EeUnitsUnitClassFields, land_building: impl FnOnce() -> bool) -> bool {
    let t = c.mUnitType as u32;
    let k = c.mCategory as u32;
    match which {
        EE_UNITS_UC_IS_DROP_SITE => {
            t == kUnitTypeMechZeusPrototype as u32
                || land_building()
                    && t != kUnitTypeFarm as u32
                    && t != kUnitTypeDock as u32
                    && t != kUnitTypeNavyYard as u32
                    && c.mResourceType > 0 as c_long
                    && c.mResourceType < 7
        }
        EE_UNITS_UC_IS_CRUSADER => [
            kUnitTypeInf04ShortSwordCrusader as u32,
            kUnitTypeInf06LongSwordCrusader as u32,
            kUnitTypeCav04BronzeSpearCavalryCrusader as u32,
            kUnitTypeCav06KnightCrusader as u32,
        ]
        .contains(&t),
        EE_UNITS_UC_IS_TRANSPORT => [
            kUnitCategoryLandTransport as u32,
            kUnitCategoryHelicopterTransport as u32,
            kUnitCategoryNavalTransport as u32,
            kUnitCategorySpaceTransport as u32,
            kUnitCategoryFortress as u32,
        ]
        .contains(&k),
        EE_UNITS_UC_IS_AIR_BASE => {
            c.mAirTrafficType != 0
                && [kUnitCategoryBuilding as u32, kUnitCategoryNavalBuilding as u32, kUnitCategorySpaceBuilding as u32]
                    .contains(&k)
        }
        EE_UNITS_UC_IS_PRIEST => [
            kUnitCategoryLandPriest as u32,
            kUnitCategoryNavalPriest as u32,
            kUnitCategoryHelicopterPriest as u32,
            kUnitCategoryBuildingPriest as u32,
        ]
        .contains(&k),
        _ => false,
    }
}

/// `EEUnitClass::Is*()` (`which` an `EE_UNITS_UC_IS_*`) of the live class `class`.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_uc_is(class: u64, which: i32) -> bool {
    use crate::units::uu::UuHost;
    let c = crate::units::uu_host::Game.class(class);
    if c.fields.is_null() {
        return false;
    }
    // SAFETY: a live class's record, read in place
    let fields = unsafe { &*c.fields };
    is(which, fields, || c.land_building != 0)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn class(unit_type: u32, category: u32) -> EeUnitsUnitClassFields {
        EeUnitsUnitClassFields { mUnitType: unit_type as _, mCategory: category as _, ..Default::default() }
    }

    #[test]
    fn the_drop_site_asks_the_land_test_after_the_zeus_and_takes_the_resource_kinds() {
        let zeus = class(kUnitTypeMechZeusPrototype as u32, 0);
        assert!(is(EE_UNITS_UC_IS_DROP_SITE, &zeus, || panic!("not asked")));
        let mut c = class(5, kUnitCategoryBuilding as u32);
        c.mResourceType = 3;
        assert!(is(EE_UNITS_UC_IS_DROP_SITE, &c, || true));
        assert!(!is(EE_UNITS_UC_IS_DROP_SITE, &c, || false));
        c.mResourceType = 7;
        assert!(!is(EE_UNITS_UC_IS_DROP_SITE, &c, || true));
        let mut farm = class(kUnitTypeFarm as u32, 0);
        farm.mResourceType = 2;
        assert!(!is(EE_UNITS_UC_IS_DROP_SITE, &farm, || true));
    }

    #[test]
    fn the_kind_tests() {
        assert!(is(EE_UNITS_UC_IS_CRUSADER, &class(kUnitTypeCav06KnightCrusader as u32, 0), || false));
        assert!(!is(EE_UNITS_UC_IS_CRUSADER, &class(0x2ea, 0), || false));
        assert!(is(EE_UNITS_UC_IS_TRANSPORT, &class(0, kUnitCategoryFortress as u32), || false));
        assert!(!is(EE_UNITS_UC_IS_TRANSPORT, &class(0, kUnitCategoryBuilding as u32), || false));
        assert!(is(EE_UNITS_UC_IS_PRIEST, &class(0, kUnitCategoryBuildingPriest as u32), || false));
        let mut base = class(0, kUnitCategoryNavalBuilding as u32);
        assert!(!is(EE_UNITS_UC_IS_AIR_BASE, &base, || false));
        base.mAirTrafficType = 2;
        assert!(is(EE_UNITS_UC_IS_AIR_BASE, &base, || false));
    }
}
