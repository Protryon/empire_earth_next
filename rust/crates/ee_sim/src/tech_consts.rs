//! The civilization powers' ids (ee/EECivPowerIDs.h) and the calamity captures' event ids (ee/EETechCapture.h), as
//! plain (unscoped) C enums for the C++ headers; each comment gives the evidence for its name, "(provisional name)"
//! marks an inferred one. (The tech ids are tech_tree_ids.rs.)
#![allow(non_camel_case_types, non_upper_case_globals, clippy::enum_variant_names, clippy::upper_case_acronyms)]

/// Civilization power ids (`EECivPower::mID`, dbcivpowers.dat record +0x68; `EECivPowersManager::GetPower` 0x6831d0 /
/// GetPowerRecord index the records by id - 0x26; HasPower tests the civilization attribute index); names from the
/// record names (+0x00).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EECivPowerID {
    /// dbcivpowers.dat record 0 id (+0x68) 38 "Flaming Arrows"
    kCivPowerFlamingArrows = 0x26,
    /// dbcivpowers.dat record 1 id (+0x68) 39 "Slavery"
    kCivPowerSlavery = 0x27,
    /// dbcivpowers.dat record 2 id (+0x68) 40 "Priest Tower"
    kCivPowerPriestTower = 0x28,
    /// dbcivpowers.dat record 3 id (+0x68) 41 "Crusaders"
    kCivPowerCrusaders = 0x29,
    /// dbcivpowers.dat record 4 id (+0x68) 42 "Fanaticism"
    kCivPowerFanaticism = 0x2a,
    /// dbcivpowers.dat record 5 id (+0x68) 43 "Emissaries"
    kCivPowerEmissaries = 0x2b,
    /// dbcivpowers.dat record 6 id (+0x68) 44 "Exploration"
    kCivPowerExploration = 0x2c,
    /// dbcivpowers.dat record 7 id (+0x68) 45 "Insurance"
    kCivPowerInsurance = 0x2d,
    /// dbcivpowers.dat record 8 id (+0x68) 46 "Metallurgy"
    kCivPowerMetallurgy = 0x2e,
    /// dbcivpowers.dat record 9 id (+0x68) 47 "Advanced Mining"
    kCivPowerAdvancedMining = 0x2f,
    /// dbcivpowers.dat record 10 id (+0x68) 48 "Pathfinding"
    kCivPowerPathfinding = 0x30,
    /// dbcivpowers.dat record 11 id (+0x68) 49 "Camouflage"
    kCivPowerCamouflage = 0x31,
    /// dbcivpowers.dat record 12 id (+0x68) 50 "Bundeswehr"
    kCivPowerBundeswehr = 0x32,
    /// dbcivpowers.dat record 13 id (+0x68) 51 "SAS"
    kCivPowerSAS = 0x33,
    /// dbcivpowers.dat record 14 id (+0x68) 52 "Paratroopers"
    kCivPowerParatroopers = 0x34,
    /// dbcivpowers.dat record 15 id (+0x68) 53 "Market"
    kCivPowerMarket = 0x35,
    /// dbcivpowers.dat record 16 id (+0x68) 54 "Cyber Ninja"
    kCivPowerCyberNinja = 0x36,
    /// dbcivpowers.dat record 17 id (+0x68) 55 "Missile Base"
    kCivPowerMissileBase = 0x37,
    /// dbcivpowers.dat record 18 id (+0x68) 56 "Cloaking"
    kCivPowerCloaking = 0x38,
    /// dbcivpowers.dat record 19 id (+0x68) 57 "Adaptation"
    kCivPowerAdaptation = 0x39,
    /// dbcivpowers.dat record 20 id (+0x68) 58 "Expansionism"
    kCivPowerExpansionism = 0x3a,
    /// dbcivpowers.dat record 21 id (+0x68) 59 "Conquistadors"
    kCivPowerConquistadors = 0x3b,
    /// dbcivpowers.dat record 22 id (+0x68) 60 "Just In Time Manufacturing"
    kCivPowerJustInTimeManufacturing = 0x3c,
}

/// TEST ONLY (the simulation capture, ee/EETechCapture.h): the CALT record's world calls of a calamity's Start or
/// Stop, in order (`EECalamityCaptureStartEvent`).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EECalamityStartOp {
    /// an effect the start creates (its location's x, y as floats; z as the int)
    kCalamityStartEffect = 1,
    /// a sound started (volume, pan as floats)
    kCalamityStartSound = 2,
    /// a contagion helper created (id, whether it exists)
    kCalamityStartHelper = 3,
    /// a contagion helper destroyed (id)
    kCalamityStopHelper = 4,
    /// the calamity's effect destroyed (handle)
    kCalamityStartDestroyEffect = 5,
    /// StopSound / StopSecondarySound (0 / 1)
    kCalamityStartStopSound = 6,
    /// the target unit killed (id)
    kCalamityStartKill = 7,
    /// a class's own world step (its number)
    kCalamityStartStep = 8,
}

/// TEST ONLY (the simulation capture, ee/EETechCapture.h): the CONT record's world calls of one contagion helper's
/// Update, in order (`EECalamityCaptureContagionEvent`).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEContagionCaptureOp {
    /// Spread (the elapsed time, the chance)
    kContagionSpread = 1,
    /// the functor's UpdateUnits instead of a spread
    kContagionUpdateUnits = 2,
    /// a near unit (id, the draw or -1; the distance squared or -1)
    kContagionCandidate = 3,
    /// a new unit (id; 1 added, 2 immune, 3 held or no complex)
    kContagionNew = 4,
    /// Apply on a living unit (id)
    kContagionApply = 5,
    /// a dead unit removed and cured (id)
    kContagionRemove = 6,
    /// a functor's TakeHit (unit id, damage)
    kContagionDamage = 7,
    /// a functor's Infect (unit id)
    kContagionInfect = 8,
    /// a functor's Cure (unit id)
    kContagionCure = 9,
    /// the functor's Update (its update count)
    kContagionFunctorUpdate = 10,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_power_ids_are_the_records_ids() {
        assert_eq!(EECivPowerID::kCivPowerFlamingArrows as i32, 0x26);
        assert_eq!(EECivPowerID::kCivPowerJustInTimeManufacturing as i32, 0x3c);
        assert_eq!(EECivPowerID::kCivPowerMetallurgy as i32 - EECivPowerID::kCivPowerFlamingArrows as i32, 8);
    }

    #[test]
    fn the_capture_events_count_from_one() {
        assert_eq!(EECalamityStartOp::kCalamityStartEffect as i32, 1);
        assert_eq!(EECalamityStartOp::kCalamityStartStep as i32, 8);
        assert_eq!(EEContagionCaptureOp::kContagionNew as i32, 4);
        assert_eq!(EEContagionCaptureOp::kContagionFunctorUpdate as i32, 10);
    }
}
