// EECivilization (RECONSTRUCTION; file name inferred; provisional class name). A civilization
// description handed to the civilization builder: two vectors of 2-byte ids (+0x00, +0x10: the ctor buys them with the 2-byte _Buy 0x18bdf), a name (+0x20), two epoch values and two
// flags. Ctor 0x12c57d, dtor 0x162677 (both out of line); not polymorphic. sizeof 0x40 (at least).
#ifndef EE_EECIVILIZATION_H
#define EE_EECIVILIZATION_H
#include "ee/EECivUpgrade.h"

#include <vector>

class UString;

class EECivilization {
public:
    EECivilization();                       // 0x12c57d
    // the save keys (EECivilization.cpp's guarded chains 0x12c3eb..0x12c57d, guard byte 0x529a88). EEPlayer holds an
    // EECivilization by value, so the original EEPlayer.h included this header and every includer saw these statics:
    // the $E/$S renumbering they cause is names.txt metadata (tools/rebind.py), not a source shape
    static const UString sNumPurchasedUpgrades;   // "EECivilization-NumPurchasedUpgrades" (guard bit 1)
    static const UString sPurchasedUpgrade;       // bit 2
    static const UString sAvailablePoints;        // bit 4
    static const UString sMaxPoints;              // bit 8
    static const UString sCivBuilderLastVisited;  // bit 0x10
    static const UString sIsDefaultCiv;           // bit 0x20
    ~EECivilization();                      // 0x162677
    // 0x12c5e9 (ret 0xc): the cost of inUpgrade (inList, else mPurchasedUpgrades, for the category count) into outCost; true when
    // affordable (or gGameOptions.mAllowCustomCivs is clear)
    bool CanAfford(EECivUpgrade inUpgrade, unsigned long* outCost, std::vector<EECivUpgrade>* inList);
    bool CanAffordAll(std::vector<EECivUpgrade>& inUpgrades);  // 0x12c648: every one affordable in turn
    // 0x12c847 (ret 0xc): inPurchase pays and records it; then applies it for inPlayer
    bool Buy(EECivUpgrade inUpgrade, long inPlayer, bool inPurchase);
    void SetPoints(long inPoints);          // 0x12ca99: sets mAvailablePoints
    void AddPoints(long inAmount);          // 0x12c73b: adds to both epoch fields and clears mB3C
    void SetPointsAndMax(long inEpoch);     // 0x12caa3 (sets +0x38 and +0x34)
    UWideString GetName() const { return this->mName; }  // (the diplomacy form reads it through a temporary, EEUIEDSOpen 0x1d4165)
    void SetName(const UWideString& inName);   // 0x12cab0: mName = inName, mUnchanged = false (provisional name)
    // 0x12c898: 0x12c847(id, inPlayer, inFlag) for every id of inUpgrades (EEMCivUpgradesPurchased; provisional name)
    void AddUpgrades(std::vector<EECivUpgrade>& inUpgrades, long inPlayer, bool inFlag);

    // (provisional names): 0x12c6bc/0x12c6fe/0x12c5d9/0x12c70f never touch this (forwarders to
    // gCivilizationManager; the civilization builder calls them through its civilization)
    long GetBaseCost(EECivUpgrade inUpgrade);     // 0x12c6bc
    long GetExtraCost(EECivUpgrade inUpgrade);    // 0x12c6fe
    long ComputeCost(long inBase, long inStep, long inCount);  // 0x12c5d9 (inA + inB * inC)
    long CountInCategory(EECivUpgrade inUpgrade, std::vector<EECivUpgrade>* inList);  // 0x12c70f
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x12c74c, ret12/status
    long Save(FSFile& outFile);             // 0x12c9ee (EEPlayer::Save; provisional name)
    bool GetNoPointsAdded();                // 0x12c6fa (mB3C; EEPlayer::RepairPointers; provisional name)
    long Reapply(class EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion);    // 0x12c916 (EEPlayer::RepairPointers)
    long ApplyDeferred(class EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion);  // 0x12c9a1 (EEWorld::Fn139749; provisional name)
    void ApplyLevel21ToWorld();             // 0x12c8ce (EEWorld::SetPopulationLimit; provisional name)
    bool GetUnchanged();                                           // 0x12c72c
    long GetAvailablePoints();                              // 0x12c730
    long GetMaxPoints();                                    // 0x12c734
    std::vector<EECivUpgrade>& GetPurchasedUpgrades();  // 0x12c738 (returns this)

    std::vector<EECivUpgrade> mPurchasedUpgrades;  // +0x00 (clear() is 0x2987d, an ICF-shared COMDAT)
    std::vector<EECivUpgrade> mDeferredUpgrades;  // +0x10
    UWideString mName;                      // +0x20
    unsigned long mAvailablePoints;         // +0x34 (Load/Save: FSFile Read/Write unsigned long, slots +0x5c/+0x94)
    unsigned long mMaxPoints;               // +0x38
    bool mNoPointsAdded;                    // +0x3c
    bool mUnchanged;                        // +0x3d
};

#endif
