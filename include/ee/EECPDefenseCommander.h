// C:\EEX\Empire Earth\AI Thread\EECPDefenseCommander.h (RECONSTRUCTION; file name inferred). A computer player's
// defense commander (RTTI EECPDefenseCommander, no base; vtable 0x43c0b4: the destructor only, ??_G 0x77c64; new 0x54,
// ctor 0x77bce in EECPCombatManager's ctor 0x71d74; EECPCombatManager +0x4c). EECPDefenseCommander.cpp
// 0x77718..0x7ed03: it sends military aid (EECPMilitaryAidPackage, one per town it defends, keyed by the town id) to
// allied towns under attack, gathers its own citizens against
// raiders, and keeps the naval/space/carrier/AMB counts in line. Names are provisional (by address where the
// role is unclear). Requires <vector>, <map>, lle/U2DPoint.h.
#ifndef EE_EECPDEFENSECOMMANDER_H
#define EE_EECPDEFENSECOMMANDER_H

#include <vector>
#include <map>
#include "ee/EEUnitHandle.h"

struct EEPUnitSwap;                         // (ee/EEPhysics.h)
class EEAIFOGatherUnits;
class EEUnit;
class EEComplexUnit;
class EEComputerPlayer;
class EECPAttackCommander;
class EECPMilitaryAidPackage;
class EEPlayer;
class EETown;
class EECPAttackSubGroup;
class EECPAGTask;
class FSFile;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
// One threatened point of a town (the value of ComputeTownThreat's map, keyed by the town's hull point nearest the army): 0x1c
// bytes, ctor 0x799a9 (all zero), user copy ctor 0x799c2 (the point default-constructed, then every field copied).
// Provisional names. (moved here from EECPDefenseCommander.cpp: EECPMilitaryAidPackage.cpp needs it complete)
struct EECPDCTownThreat {
    EECPDCTownThreat() : mL08(0), mAttacker(0), mOwner(0), mEnemy(0), mOwn(0) {}
    EECPDCTownThreat(const EECPDCTownThreat& inOther)
    {
        this->mLocation = inOther.mLocation;
        this->mL08 = inOther.mL08;
        this->mAttacker = inOther.mAttacker;
        this->mOwner = inOther.mOwner;
        this->mEnemy = inOther.mEnemy;
        this->mOwn = inOther.mOwn;
    }

    U2DPoint<long> mLocation;               // +0x00
    long mL08;                              // +0x08
    long mAttacker;                         // +0x0c the main attacker's player id
    long mOwner;                            // +0x10 the town owner's player id
    long mEnemy;                            // +0x14 the enemy strength there
    long mOwn;                              // +0x18 the own strength there
};

class EECPDefenseCommander {
public:
    EECPDefenseCommander(EEComputerPlayer* inPlayer);   // 0x77bce
    virtual ~EECPDefenseCommander();        // +0x00 0x77c80 (??_G 0x77c64)

    void CreateDefendGroup();               // 0x77ca8: a defend task at the next task player's capital
    // 0x77d32 (ret 0xc): sends (or tops up) the aid package for inPlayer's town inTownID
    bool SendAid(EEPlayer* inPlayer, long inTownID, bool inCounterAttackOnly);
    float GetTownThreat(EETown* inTown, long& outPlayerID);  // 0x77e4b (ret 8): the town's threat (0x79382 without the map)
    void RefreshIntelligence();             // 0x77e8e: refreshes the intelligence file of every player
    // 0x77efd (ret 8): the best of inGroups sent to inPackage's town
    bool SendBestSubGroup(std::vector<EECPAttackSubGroup*>& inGroups, EECPMilitaryAidPackage* inPackage);
    void BuildUp();                         // 0x78148: the death-match build-up
    void PlanAid(unsigned long inTime);     // 0x783e7 (ret 4)
    void AidAllies();                       // 0x785cb: aid for the allies' threatened towns
    void CheckRaids();                      // 0x78866: the citizens against raiders, every five minutes
    void AidOwnTowns();                     // 0x7897c: aid for the own towns
    // 0x789fb (ret 0x10): the town's threat map turned into its three worst threats
    void GetWorstThreats(EEPlayer* inPlayer, long inTownID, std::map<U2DPoint<long>, EECPDCTownThreat>& ioMap,
                 std::vector<EECPDCTownThreat>& outList);
    bool CounterAttack(EECPMilitaryAidPackage* inPackage);  // 0x78b34 (ret 4)
    void FreePackages();                    // 0x78d7b: deletes every package, then clears the map
    void Retaliate(EEPlayer* inPlayer);     // 0x78dc8 (ret 4)
    // 0x792ec (ret 8): the defending groups' units that inGatherer accepts, appended to ioUnits
    void CollectDefendingUnits(EEAIFOGatherUnits& inGatherer, std::vector<EEUnitHandle<EEUnit> >& ioUnits);   // (EECPUnitSelectionGroup::mUnits)
    // 0x79382 (ret 0x10): the threat to inTown (0..1) from the enemy armies near it, the main attacker's id
    float ComputeTownThreat(EETown* inTown, long& outPlayerID, std::map<U2DPoint<long>, EECPDCTownThreat>* ioMap, bool inFill);
    long GetUnitAllowance();                // 0x799fa: how many units the defense may still take
    // 0x79bd1 (ret 0xc): the most threatened town of inPlayer (in inArea's continent), into ioPackage
    long FindThreatenedTown(EEPlayer* inPlayer, EECPMilitaryAidPackage* ioPackage, unsigned short inArea);
    void Update(unsigned long inTime);      // 0x79e41 (ret 4; EECPCombatManager calls it once inTime >= mU38)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x79eb8
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x7a142
    // 0x7a461 (ret 0xc; EECPAttackGroup 0x54fde/0x550f2 push (sub group, &task, word flag); was declared (EEPlayer*),
    // ret 4) (provisional name)
    void AssignSubGroupTask(EECPAttackSubGroup* inSubGroup, EECPAGTask& outTask, bool inHasContinent);
    EECPMilitaryAidPackage* GetPackage(long inTownID);   // 0x7b088 (ret 4)
    void SetNextAidTime(unsigned long inTime);   // 0x7b0b2 (ret 4): mU3C
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x7b115
    bool AssignSubGroups(EECPMilitaryAidPackage* inPackage, bool inCounterAttackOnly);  // 0x7b2d3 (ret 8)
    unsigned long UpdatePackages(unsigned long inTime);  // 0x7b442 (ret 4): drops finished packages, the next delay
    unsigned long CheckNaval(unsigned long inTime);  // 0x7b502 (ret 4): the naval units
    unsigned long CheckSpace(unsigned long inTime);  // 0x7b923 (ret 4): the space units
    unsigned long CheckSpecials(unsigned long inTime);  // 0x7bc5b (ret 4): the AMBs and the calamity powers
    // ICF-folded members (the kept copies are other classes' twins: EEComplexUnit::FnE123D, EECCAreaEffect::Reset,
    // EECCVolcano::UpdateVolcano)
    void Refresh();                         // 0xe123d
    void RespondToUnit(EEComplexUnit* inUnit);  // 0x157269 (ret 4)
    // 0x157269 too (empty, ICF-folded; EECPCombatManager::UnitsSwapped passes the unit swaps) (provisional name)
    void UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps);
    void TargetPlayerNotice(EEPlayer* inPlayer, bool inFlag);  // 0x163d6f (ret 8)

    std::map<long, EECPMilitaryAidPackage*> mPackages;   // +0x04 (_Init 0x617b6) by town id
    std::vector<unsigned long> mLoadedSubGroupIDs;  // +0x10 (Load: sub group ids; Restore flags those sub groups)
    std::vector<EECPMilitaryAidPackage*> mLoadedPackages;   // +0x20 (Load reads them, Restore moves them into the map)
    EEComputerPlayer* mPlayer;              // +0x30 (the ctor's argument)
    EECPAttackCommander* mAttackCommander;  // +0x34 (EECPCombatManager's ctor stores it)
    unsigned long mNextUpdateTime;          // +0x38 ((player id + 5) * 1000: the next update time)
    unsigned long mNextAidTime;             // +0x3c (mU38 + random * 60000; halved by EECPCombatManager 0x472ec3)
    unsigned long mNextNavalTime;           // +0x40 (CheckNaval's next time)
    unsigned long mNextRaidCheckTime;       // +0x44 (CheckRaids's next time)
    unsigned long mNextSpaceTime;           // +0x48 (CheckSpace's next time)
    unsigned long mNextSpecialTime;         // +0x4c (CheckSpecials's next time)
    bool mEnabled;                          // +0x50 (Update runs only while set; saved)
};

#endif
