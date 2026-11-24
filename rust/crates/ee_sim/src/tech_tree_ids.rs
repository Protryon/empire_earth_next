//! The tech tree's named ids (ee/EETechTreeTechID.h: the vanilla epochs' positions, the techs the code names, the
//! prerequisite bits), as plain (unscoped) C enums for the C++ headers; each comment gives the evidence for its
//! name, "(provisional name)" marks an inferred one. A composed value's parts are in its comment (`[= a | b]`).
#![allow(non_camel_case_types, clippy::enum_variant_names)]

/// The tech ids (provisional name). The epoch constants are the vanilla epochs' positions (epochs.yaml order); the
/// epoch count is EEEpochCount(), from the data (the original fixed it at 15: kTechEpochAll / keettidEpochAll).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EETechTreeTechID {
    /// dbtechtree.dat epoch 0 record 'Epoch Paleo' (id +0x68 = 0); its mNameID 9065 'Epoch Prehistoric' (Language.rc); epoch title 30145 'Prehistoric Age'
    kTechEpochPrehistoric = 0,
    /// dbtechtree.dat epoch 1 record 'Epoch Stone' (id +0x68 = 1); its mNameID 9168 'Epoch Stone' (Language.rc); epoch title 30146 'Stone Age'
    kTechEpochStone = 1,
    /// dbtechtree.dat epoch 2 record 'Epoch Copper' (id +0x68 = 2); its mNameID 9167 'Epoch Copper' (Language.rc); epoch title 30147 'Copper Age'
    kTechEpochCopper = 2,
    /// dbtechtree.dat epoch 3 record 'Epoch Bronze' (id +0x68 = 3); its mNameID 9166 'Epoch Bronze' (Language.rc); epoch title 30148 'Bronze Age'
    kTechEpochBronze = 3,
    /// dbtechtree.dat epoch 4 record 'Epoch Dark' (id +0x68 = 4); its mNameID 9165 'Epoch Dark' (Language.rc); epoch title 30149 'Dark Age'
    kTechEpochDark = 4,
    /// dbtechtree.dat epoch 5 record 'Epoch Middle' (id +0x68 = 5); its mNameID 9164 'Epoch Middle' (Language.rc); epoch title 30150 'Middle Ages'
    kTechEpochMiddle = 5,
    /// dbtechtree.dat epoch 6 record 'Epoch Renaissance' (id +0x68 = 6); its mNameID 9163 'Epoch Renaissance' (Language.rc); epoch title 30151 'Renaissance'
    kTechEpochRenaissance = 6,
    /// dbtechtree.dat epoch 7 record 'Epoch Imperial' (id +0x68 = 7); its mNameID 9153 'Epoch Imperial' (Language.rc); epoch title 30152 'Imperial Age'
    kTechEpochImperial = 7,
    /// dbtechtree.dat epoch 8 record 'Epoch Industrial' (id +0x68 = 8); its mNameID 9161 'Epoch Industrial' (Language.rc); epoch title 30153 'Industrial Age'
    kTechEpochIndustrial = 8,
    /// dbtechtree.dat epoch 9 record 'Epoch Atomic - WW1' (id +0x68 = 9); its mNameID 9231 'Epoch Atomic - WW1' (Language.rc); epoch title 30154 'Atomic Age - WW1'
    kTechEpochAtomicWW1 = 9,
    /// dbtechtree.dat epoch 10 record 'Epoch Atomic - WW2' (id +0x68 = 10); its mNameID 9232 'Epoch Atomic - WW2' (Language.rc); epoch title 30155 'Atomic Age - WW2'
    kTechEpochAtomicWW2 = 10,
    /// dbtechtree.dat epoch 11 record 'Epoch Atomic - Modern' (id +0x68 = 11); its mNameID 9174 'Epoch Atomic - Modern' (Language.rc); epoch title 30156 'Atomic Age - Modern'
    kTechEpochAtomicModern = 11,
    /// dbtechtree.dat epoch 12 record 'Epoch Digital' (id +0x68 = 12); its mNameID 9172 'Epoch Digital' (Language.rc); epoch title 30157 'Digital Age'
    kTechEpochDigital = 12,
    /// dbtechtree.dat epoch 13 record 'Epoch Nano' (id +0x68 = 13); its mNameID 9198 'Epoch Nano' (Language.rc); epoch title 30158 'Nano Age'
    kTechEpochNano = 13,
    /// dbtechtree.dat epoch 14 record 'Epoch Space' (id +0x68 = 14); its mNameID 9300 'Epoch -Space' (Language.rc); epoch title 30159 'Space Age'
    kTechEpochSpace = 14,
    /// dbtechtree.dat id 1022 (record +0x68; epoch 0) name 'zzzHurricane'
    kTechHurricane = 1022,
    /// dbtechtree.dat id 1025 (record +0x68; epoch 0) name 'b  Town Center'
    kTechTownCenter = 1025,
    /// dbtechtree.dat id 1028 (record +0x68; epoch 0) name 'b  Settlement'
    kTechSettlement = 1028,
    /// dbtechtree.dat id 1053 (record +0x68; epoch 0) name 'b Capitol'
    kTechCapitol = 1053,
    /// dbtechtree.dat id 1074 (record +0x68; epoch 0) name 'Citizen'
    kTechCitizen = 1074,
    /// dbtechtree.dat id 1089 (record +0x68; epoch 0) name 'Meteor Storm'
    kTechMeteorStorm = 1089,
    /// dbtechtree.dat id 1916 (record +0x68; epoch 0) name 'Teleport'
    kTechTeleport = 1916,
    /// dbtechtree.dat id 1920 (record +0x68; epoch 0) name 'Conscription Spell'
    kTechConscriptionSpell = 1920,
    /// dbtechtree.dat id 2023 (record +0x68; epoch 1) name 'b  Dock'
    kTechDock = 2023,
    /// dbtechtree.dat id 3047 (record +0x68; epoch 2) name 'w Lighthouse of Alexandria'
    kTechLighthouseOfAlexandria = 3047,
    /// dbtechtree.dat id 3059 (record +0x68; epoch 2) name 'Simple Bowman(Flaming)  Research'
    kTechSimpleBowmanFlamingResearch = 3059,
    /// dbtechtree.dat id 3060 (record +0x68; epoch 2) name 'Simple Bowman Research'
    kTechSimpleBowmanResearch = 3060,
    /// dbtechtree.dat id 4080 (record +0x68; epoch 3) name 'ShortSword Upgrade(Crusaders)'
    kTechShortSwordUpgradeCrusaders = 4080,
    /// dbtechtree.dat id 4081 (record +0x68; epoch 3) name 'ShortSword Upgrade'
    kTechShortSwordUpgrade = 4081,
    /// dbtechtree.dat id 5080 (record +0x68; epoch 4) name 'Composite Bow Upgrade'
    kTechCompositeBowUpgrade = 5080,
    /// dbtechtree.dat id 5081 (record +0x68; epoch 4) name 'Composite Bow (Flaming) Upgrade'
    kTechCompositeBowFlamingUpgrade = 5081,
    /// dbtechtree.dat id 6078 (record +0x68; epoch 5) name 'Long Bow (Flaming)  Upgrade'
    kTechLongBowFlamingUpgrade = 6078,
    /// dbtechtree.dat id 6079 (record +0x68; epoch 5) name 'LongSword Upgrade (Crusaders)'
    kTechLongSwordUpgradeCrusaders = 6079,
    /// dbtechtree.dat id 6080 (record +0x68; epoch 5) name 'Long Bow Upgrade'
    kTechLongBowUpgrade = 6080,
    /// dbtechtree.dat id 6081 (record +0x68; epoch 5) name 'LongSword Upgrade'
    kTechLongSwordUpgrade = 6081,
    /// dbtechtree.dat id 6087 (record +0x68; epoch 5) name 'Knight Upgrade'
    kTechKnightUpgrade = 6087,
    /// dbtechtree.dat id 6094 (record +0x68; epoch 5) name 'Knight Upgrade (Crusaders)'
    kTechKnightUpgradeCrusaders = 6094,
    /// dbtechtree.dat id 7020 (record +0x68; epoch 6) name 'Conquistadors Spell'
    kTechConquistadorsSpell = 7020,
    /// dbtechtree.dat id 10007 (record +0x68; epoch 9) name 'b  Navy Yard'
    kTechNavyYard = 10007,
    /// dbtechtree.dat id 10008 (record +0x68; epoch 9) name 'b  Airport'
    kTechAirport = 10008,
    /// dbtechtree.dat id 13012 (record +0x68; epoch 12) name 'Inf13 - Sentinel'
    kTechInf13Sentinel = 13012,
    /// dbtechtree.dat id 14101 (record +0x68; epoch 13) name 'Inf14 - Guardian'
    kTechInf14Guardian = 14101,
    /// dbtechtree.dat id 15000 (record +0x68; epoch 14) name 'b Space Dock'
    kTechSpaceDock = 15000,
    /// dbtechtree.dat id 15009 (record +0x68; epoch 14) name 'b Space Turret'
    kTechSpaceTurret = 15009,
    /// dbtechtree.dat id 15013 (record +0x68; epoch 14) name 'b Wall - Space'
    kTechWallSpace = 15013,
    /// dbtechtree.dat id 15014 (record +0x68; epoch 14) name 'b Gate - Space'
    kTechGateSpace = 15014,
    /// dbtechtree.dat id 15020 (record +0x68; epoch 14) name 'w Future Research Sentinel'
    kTechFutureResearchSentinel = 15020,
    /// dbtechtree.dat id 15027 (record +0x68; epoch 14) name 'Inf15 - Watchman'
    kTechInf15Watchman = 15027,
    /// dbtechtree.dat id 15029 (record +0x68; epoch 14) name 'w Orbital Space Station'
    kTechOrbitalSpaceStation = 15029,
    /// dbtechtree.dat id 15036 (record +0x68; epoch 14) name 'Robotic Farm Upgrade'
    kTechRoboticFarmUpgrade = 15036,
    /// the 'no technology' id: setrilo ai_build.h/ai_research.h '100000 without a class'; EETechTreeUnitClassInfo, EEComplexUnit::mCurrentResearch, EEUGBuildRepair default to it; EETechTree::SumCosts 0x5cc5db walks ids below it (provisional name)
    kTechNone = 100000,
}

/// EETechTreeNode::mPrerequisiteFlags (+0x20): the prerequisites met so far; EETechTreeNode::SetPrerequisite 0x5cf995 sets one bit per prerequisite, the node is available at kPrerequisiteAll; Load sets the bits of empty prerequisites
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EETechPrerequisiteFlag {
    /// SetPrerequisite: an id 0..14 (an epoch root) (provisional name)
    kPrerequisiteEpoch = 1,
    /// SetPrerequisite / Load: mData->mPrerequisite1 (provisional name)
    kPrerequisitePrerequisite1 = 2,
    /// SetPrerequisite / Load: mData->mPrerequisite2 (provisional name)
    kPrerequisitePrerequisite2 = 4,
    /// SetPrerequisite / Load: mData->mPrerequisite3 (provisional name)
    kPrerequisitePrerequisite3 = 8,
    /// SetPrerequisite / Load: mData->mPrerequisite4 (provisional name)
    kPrerequisitePrerequisite4 = 0x10,
    /// every bit: SetPrerequisite makes the node available (provisional name) [= kPrerequisiteEpoch | kPrerequisitePrerequisite1 | kPrerequisitePrerequisite2 | kPrerequisitePrerequisite3 | kPrerequisitePrerequisite4]
    kPrerequisiteAll = 0x1f,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_ids_keep_their_numbers() {
        assert_eq!(EETechTreeTechID::kTechEpochSpace as i32, 14);
        assert_eq!(EETechTreeTechID::kTechNone as i32, 100000);
        let all = EETechPrerequisiteFlag::kPrerequisiteEpoch as i32
            | EETechPrerequisiteFlag::kPrerequisitePrerequisite1 as i32
            | EETechPrerequisiteFlag::kPrerequisitePrerequisite2 as i32
            | EETechPrerequisiteFlag::kPrerequisitePrerequisite3 as i32
            | EETechPrerequisiteFlag::kPrerequisitePrerequisite4 as i32;
        assert_eq!(EETechPrerequisiteFlag::kPrerequisiteAll as i32, all);
    }
}
