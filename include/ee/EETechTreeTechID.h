// ee/EETechTreeTechID.h (RECONSTRUCTION): the tech tree ids, moved out of ee/EEPlayer.h (which includes it) so that
// units without ee/EEPlayer.h can name them. Each comment gives the evidence for its name; (provisional name) marks an
// inferred one.
#ifndef EE_EETECHTREETECHID_H
#define EE_EETECHTREETECHID_H

#include <vector>

// The tech ids (provisional name). The epoch constants are the vanilla epochs' positions (epochs.yaml order); the
// epoch count is EEEpochCount(), from the data (the original fixed it at 15: kTechEpochAll / keettidEpochAll).
enum EETechTreeTechID {
    kTechEpochPrehistoric = 0,              // dbtechtree.dat epoch 0 record 'Epoch Paleo' (id +0x68 = 0); its mNameID 9065 'Epoch Prehistoric' (Language.rc); epoch title 30145 'Prehistoric Age'
    kTechEpochStone = 1,                    // dbtechtree.dat epoch 1 record 'Epoch Stone' (id +0x68 = 1); its mNameID 9168 'Epoch Stone' (Language.rc); epoch title 30146 'Stone Age'
    kTechEpochCopper = 2,                   // dbtechtree.dat epoch 2 record 'Epoch Copper' (id +0x68 = 2); its mNameID 9167 'Epoch Copper' (Language.rc); epoch title 30147 'Copper Age'
    kTechEpochBronze = 3,                   // dbtechtree.dat epoch 3 record 'Epoch Bronze' (id +0x68 = 3); its mNameID 9166 'Epoch Bronze' (Language.rc); epoch title 30148 'Bronze Age'
    kTechEpochDark = 4,                     // dbtechtree.dat epoch 4 record 'Epoch Dark' (id +0x68 = 4); its mNameID 9165 'Epoch Dark' (Language.rc); epoch title 30149 'Dark Age'
    kTechEpochMiddle = 5,                   // dbtechtree.dat epoch 5 record 'Epoch Middle' (id +0x68 = 5); its mNameID 9164 'Epoch Middle' (Language.rc); epoch title 30150 'Middle Ages'
    kTechEpochRenaissance = 6,              // dbtechtree.dat epoch 6 record 'Epoch Renaissance' (id +0x68 = 6); its mNameID 9163 'Epoch Renaissance' (Language.rc); epoch title 30151 'Renaissance'
    kTechEpochImperial = 7,                 // dbtechtree.dat epoch 7 record 'Epoch Imperial' (id +0x68 = 7); its mNameID 9153 'Epoch Imperial' (Language.rc); epoch title 30152 'Imperial Age'
    kTechEpochIndustrial = 8,               // dbtechtree.dat epoch 8 record 'Epoch Industrial' (id +0x68 = 8); its mNameID 9161 'Epoch Industrial' (Language.rc); epoch title 30153 'Industrial Age'
    kTechEpochAtomicWW1 = 9,                // dbtechtree.dat epoch 9 record 'Epoch Atomic - WW1' (id +0x68 = 9); its mNameID 9231 'Epoch Atomic - WW1' (Language.rc); epoch title 30154 'Atomic Age - WW1'
    kTechEpochAtomicWW2 = 10,               // dbtechtree.dat epoch 10 record 'Epoch Atomic - WW2' (id +0x68 = 10); its mNameID 9232 'Epoch Atomic - WW2' (Language.rc); epoch title 30155 'Atomic Age - WW2'
    kTechEpochAtomicModern = 11,            // dbtechtree.dat epoch 11 record 'Epoch Atomic - Modern' (id +0x68 = 11); its mNameID 9174 'Epoch Atomic - Modern' (Language.rc); epoch title 30156 'Atomic Age - Modern'
    kTechEpochDigital = 12,                 // dbtechtree.dat epoch 12 record 'Epoch Digital' (id +0x68 = 12); its mNameID 9172 'Epoch Digital' (Language.rc); epoch title 30157 'Digital Age'
    kTechEpochNano = 13,                    // dbtechtree.dat epoch 13 record 'Epoch Nano' (id +0x68 = 13); its mNameID 9198 'Epoch Nano' (Language.rc); epoch title 30158 'Nano Age'
    kTechEpochSpace = 14,                   // dbtechtree.dat epoch 14 record 'Epoch Space' (id +0x68 = 14); its mNameID 9300 'Epoch -Space' (Language.rc); epoch title 30159 'Space Age'
    kTechHurricane = 1022,                  // dbtechtree.dat id 1022 (record +0x68; epoch 0) name 'zzzHurricane'
    kTechTownCenter = 1025,                 // dbtechtree.dat id 1025 (record +0x68; epoch 0) name 'b  Town Center'
    kTechSettlement = 1028,                 // dbtechtree.dat id 1028 (record +0x68; epoch 0) name 'b  Settlement'
    kTechCapitol = 1053,                    // dbtechtree.dat id 1053 (record +0x68; epoch 0) name 'b Capitol'
    kTechCitizen = 1074,                    // dbtechtree.dat id 1074 (record +0x68; epoch 0) name 'Citizen'
    kTechMeteorStorm = 1089,                // dbtechtree.dat id 1089 (record +0x68; epoch 0) name 'Meteor Storm'
    kTechTeleport = 1916,                   // dbtechtree.dat id 1916 (record +0x68; epoch 0) name 'Teleport'
    kTechConscriptionSpell = 1920,          // dbtechtree.dat id 1920 (record +0x68; epoch 0) name 'Conscription Spell'
    kTechDock = 2023,                       // dbtechtree.dat id 2023 (record +0x68; epoch 1) name 'b  Dock'
    kTechLighthouseOfAlexandria = 3047,     // dbtechtree.dat id 3047 (record +0x68; epoch 2) name 'w Lighthouse of Alexandria'
    kTechSimpleBowmanFlamingResearch = 3059,  // dbtechtree.dat id 3059 (record +0x68; epoch 2) name 'Simple Bowman(Flaming)  Research'
    kTechSimpleBowmanResearch = 3060,       // dbtechtree.dat id 3060 (record +0x68; epoch 2) name 'Simple Bowman Research'
    kTechShortSwordUpgradeCrusaders = 4080,  // dbtechtree.dat id 4080 (record +0x68; epoch 3) name 'ShortSword Upgrade(Crusaders)'
    kTechShortSwordUpgrade = 4081,          // dbtechtree.dat id 4081 (record +0x68; epoch 3) name 'ShortSword Upgrade'
    kTechCompositeBowUpgrade = 5080,        // dbtechtree.dat id 5080 (record +0x68; epoch 4) name 'Composite Bow Upgrade'
    kTechCompositeBowFlamingUpgrade = 5081,  // dbtechtree.dat id 5081 (record +0x68; epoch 4) name 'Composite Bow (Flaming) Upgrade'
    kTechLongBowFlamingUpgrade = 6078,      // dbtechtree.dat id 6078 (record +0x68; epoch 5) name 'Long Bow (Flaming)  Upgrade'
    kTechLongSwordUpgradeCrusaders = 6079,  // dbtechtree.dat id 6079 (record +0x68; epoch 5) name 'LongSword Upgrade (Crusaders)'
    kTechLongBowUpgrade = 6080,             // dbtechtree.dat id 6080 (record +0x68; epoch 5) name 'Long Bow Upgrade'
    kTechLongSwordUpgrade = 6081,           // dbtechtree.dat id 6081 (record +0x68; epoch 5) name 'LongSword Upgrade'
    kTechKnightUpgrade = 6087,              // dbtechtree.dat id 6087 (record +0x68; epoch 5) name 'Knight Upgrade'
    kTechKnightUpgradeCrusaders = 6094,     // dbtechtree.dat id 6094 (record +0x68; epoch 5) name 'Knight Upgrade (Crusaders)'
    kTechConquistadorsSpell = 7020,         // dbtechtree.dat id 7020 (record +0x68; epoch 6) name 'Conquistadors Spell'
    kTechNavyYard = 10007,                  // dbtechtree.dat id 10007 (record +0x68; epoch 9) name 'b  Navy Yard'
    kTechAirport = 10008,                   // dbtechtree.dat id 10008 (record +0x68; epoch 9) name 'b  Airport'
    kTechInf13Sentinel = 13012,             // dbtechtree.dat id 13012 (record +0x68; epoch 12) name 'Inf13 - Sentinel'
    kTechInf14Guardian = 14101,             // dbtechtree.dat id 14101 (record +0x68; epoch 13) name 'Inf14 - Guardian'
    kTechSpaceDock = 15000,                 // dbtechtree.dat id 15000 (record +0x68; epoch 14) name 'b Space Dock'
    kTechSpaceTurret = 15009,               // dbtechtree.dat id 15009 (record +0x68; epoch 14) name 'b Space Turret'
    kTechWallSpace = 15013,                 // dbtechtree.dat id 15013 (record +0x68; epoch 14) name 'b Wall - Space'
    kTechGateSpace = 15014,                 // dbtechtree.dat id 15014 (record +0x68; epoch 14) name 'b Gate - Space'
    kTechFutureResearchSentinel = 15020,    // dbtechtree.dat id 15020 (record +0x68; epoch 14) name 'w Future Research Sentinel'
    kTechInf15Watchman = 15027,             // dbtechtree.dat id 15027 (record +0x68; epoch 14) name 'Inf15 - Watchman'
    kTechOrbitalSpaceStation = 15029,       // dbtechtree.dat id 15029 (record +0x68; epoch 14) name 'w Orbital Space Station'
    kTechRoboticFarmUpgrade = 15036,        // dbtechtree.dat id 15036 (record +0x68; epoch 14) name 'Robotic Farm Upgrade'
    kTechNone = 100000,                     // the 'no technology' id: setrilo ai_build.h/ai_research.h '100000 without a class'; EETechTreeUnitClassInfo, EEComplexUnit::mCurrentResearch, EEUGBuildRepair default to it; EETechTree::SumCosts 0x5cc5db walks ids below it (provisional name)
};

// EETechTreeNode::mPrerequisiteFlags (+0x20): the prerequisites met so far; EETechTreeNode::SetPrerequisite 0x5cf995 sets one bit per prerequisite, the node is available at kPrerequisiteAll; Load sets the bits of empty prerequisites
enum EETechPrerequisiteFlag {
    kPrerequisiteEpoch = 1,           // SetPrerequisite: an id 0..14 (an epoch root) (provisional name)
    kPrerequisitePrerequisite1 = 2,   // SetPrerequisite / Load: mData->mPrerequisite1 (provisional name)
    kPrerequisitePrerequisite2 = 4,   // SetPrerequisite / Load: mData->mPrerequisite2 (provisional name)
    kPrerequisitePrerequisite3 = 8,   // SetPrerequisite / Load: mData->mPrerequisite3 (provisional name)
    kPrerequisitePrerequisite4 = 0x10,  // SetPrerequisite / Load: mData->mPrerequisite4 (provisional name)
    kPrerequisiteAll = kPrerequisiteEpoch | kPrerequisitePrerequisite1 | kPrerequisitePrerequisite2 | kPrerequisitePrerequisite3 | kPrerequisitePrerequisite4,          // every bit: SetPrerequisite makes the node available (provisional name)
};

// The epochs (epochs.yaml; the dbtechtree file's epoch blocks): their count is set when gTechTree loads (15, the
// original's, until then). An epoch's tech id is its position 0..count-1; node ids are 1000 and up (kTechFirstNodeID),
// and a node's epoch is the block it was loaded in (EETechTreeEpochOf), not id / 1000 - 1 as in the original, so an
// epoch inserted in the middle keeps every other tech's id.
extern long gEpochCount;
inline long EEEpochCount() { return gEpochCount; }
inline bool EEIsEpochID(long inID) { return inID >= 0 && inID < gEpochCount; }
const long kTechFirstNodeID = 1000;
long EETechTreeEpochOf(long inID);          // the epoch of a tech id (an epoch's own id; a node's block); -1: none
long EEEpochTitleID(long inEpoch);          // the epoch's title string ("Bronze Age"; epochs.yaml mTitleID); for
                                            // EEEpochCount(), the game options' "Random Epoch"
// one element per epoch (sized when it is made: after the database is loaded)
template <class T> class EEEpochArray : public std::vector<T> {
public:
    EEEpochArray() : std::vector<T>(gEpochCount) {}
};

#endif // EE_EETECHTREETECHID_H
