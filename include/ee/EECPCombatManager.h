// C:\EEX\Empire Earth\AI Thread\EECPCombatManager.h (RECONSTRUCTION; file name inferred).
// A computer player's combat manager (RTTI EECPCombatManager, a root class; vtable 0x43c090, two slots: +0x00 the
// ??_G 0x71e8a, +0x04 0x740ee; EECPCombatManager.cpp 0x71c7f..0x77084; EEComputerPlayer::mCombatManager +0xbc0;
// sizeof 0x68). Layout from the ctor 0x71d74 / dtor 0x71ea6 and Load 0x7384c / Save 0x74d63.
// Names are provisional. Requires lle/LowLevelEngine.h, lle/FSFile.h.
#ifndef EE_EECPCOMBATMANAGER_H
#define EE_EECPCOMBATMANAGER_H

#include <map>
#include <vector>

#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"

struct EEPUnitSwap;                         // (ee/EEPhysics.h)
class EEPlayer;
class EEUnit;
class EEComplexUnit;
class EEComputerPlayer;
class EECPAttackCommander;
class EECPAttackSubGroup;
class EECPCombatStrategy;
class EEUnitClass;
class EECPDefenseCommander;
class EECPIntelligence;
class EECPPersonnel;
class EECPUnitSelector;

class EECPCombatManager {
public:
    EECPCombatManager(EEComputerPlayer* inPlayer);
    virtual ~EECPCombatManager();           // +0x00 (??_G 0x71e8a, dtor 0x71ea6)
    // +0x04 0x740ee (ret 0x20): attacks inTarget at inTile (provisional name; argument meanings inferred)
    virtual void Attack(EEPlayer* inTarget, const EETileLocation& inTile, bool inAllowNewGroup, bool inSendChat, bool inUseLand,
        bool inUseAir, bool inUseSea, bool inUseSpace);

    void AddAttackTask(unsigned long inUnitID, unsigned long inAreaSerial);  // 0x71f16 (ret 8): a task into mAttackTasks
    void AddDefendTask(unsigned long inUnitID, unsigned long inAreaSerial);  // 0x7203a (ret 8): a task into mDefendTasks
    bool OfferUnit(EEComplexUnit* inOld, EEComplexUnit* inUnit);
    void Update(unsigned long inTime);
    void DeleteGroups();
    void TargetPlayerNotice(EEPlayer* inPlayer, bool inFlag);
    void NoteThreat(EEComplexUnit* inUnit, bool inUseTarget);
    bool CoverLanding(EECPAttackSubGroup* inGroup);
    void AttackPlayer(EEPlayer* inPlayer);
    void DefendPlayer(EEPlayer* inPlayer);
    void CollectDefendingUnits(std::vector<EEComplexUnit*>& inUnits);  // 0x7305d (ret 4): the defending groups' units
    bool ChooseTask(EECPAGTask& outTask, bool inLand, bool inWater, bool inSpace, unsigned short inContinent);
    long GetUnitsAvailable();
    long GetUnitLimit();
    void ForgetTarget(unsigned long inSerial);
    void ForgetTargetUnit(unsigned long inUnitID);
    bool KeepsUnit(EEComplexUnit* inUnit);
    bool IsValidTask(EECPAGTask& inTask, EECPCombatStrategy& inStrategy, std::vector<long>& inList);
    bool IsValidTarget(unsigned long inSerial, unsigned long inUnitID, bool inEnemy);
    bool IsMilitaryUnit(EEComplexUnit* inUnit);
    bool IsMilitaryClass(EEUnitClass* inClass);  // 0x737c9 (ret 4; this unused)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    void EpochReached(long inEpoch);
    EEComplexUnit* ChooseTargetUnit(EEPlayer* inPlayer);  // 0x73fc6 (ret 4; this unused): the unit to attack
    void Refresh();
    void UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps);  // 0x74772 (ret 4; EEComputerPlayer::Virtual9C 0x3ced7 passes its swaps)
    void ReleaseFromAreaGroups(EEComplexUnit* inUnit);
    void DropLostTasks();
    bool DropAreaTask(unsigned long inSerial, bool inRelease);
    bool DropUnitTask(unsigned long inUnitID, bool inRelease);
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    void UnitAdded(EEUnit* inUnit);
    void UnitRemoved(EEUnit* inUnit);
    bool TakeFromAttack(EEUnit* inUnit, bool inReturnUnit);
    static bool IsMilitaryType(unsigned long inUnitType);
    static bool IsMilitaryTypeUnit(EEUnit* inUnit);
    void Tick(unsigned long inTime);
    void UpdateWonderRace(unsigned long inTime);
    void AttackUnit(EEComplexUnit* inTarget);
    void DefendUnit(EEComplexUnit* inTarget);
    void RespondToUnit(EEComplexUnit* inUnit);

    std::map<EETileLocation, long> mTileCounts;   // +0x04 (NoteThreat counts per region; Save: key, count)
    std::vector<EECPAGTask> mDefendTasks;   // +0x10
    std::vector<EECPAGTask> mAttackTasks;   // +0x20
    std::vector<unsigned long> mWonderAttackTimes;  // +0x30 (saved)
    EECPUnitSelector* mUnitSelector;        // +0x40 (new 8; EECPAttackGroup 0x57391 calls Select)
    EEComputerPlayer* mPlayer;              // +0x44
    EECPAttackCommander* GetAttackCommander() const { return this->mAttackCommander; }
    EECPAttackCommander* mAttackCommander;  // +0x48 (new 0x3c, ctor 0x5198e)
    EECPDefenseCommander* mDefenseCommander;   // +0x4c (new 0x54, ctor 0x77bce)
    // inline (EECPDefenseCommander 0x478301 loads the object chain into ecx before the push) (provisional name)
    EECPIntelligence* GetIntelligence() { return this->mIntelligence; }
    EECPIntelligence* mIntelligence;        // +0x50 (new 0x14, ctor 0x81cd0)
    EECPPersonnel* GetPersonnel() const { return this->mPersonnel; }
    EECPPersonnel* mPersonnel;              // +0x54 (new 0x38, ctor 0x868b8)
    // inline post-increment id dispensers (the AttackGroup/AttackSubGroup ctors 0x45473a/0x45adb2)
    unsigned long GetNextGroupID() { return this->mNextGroupID++; }
    unsigned long GetNextSubGroupID() { return this->mNextSubGroupID++; }
    unsigned long mNextGroupID;             // +0x58 (= 1; saved)
    unsigned long mNextSubGroupID;          // +0x5c (= 1; saved)
    bool mWasAttacked;                      // +0x60 (saved)
    bool mIsWonderRace;                     // +0x61 (saved)
    bool mImprovementsEnabled;              // +0x62 (saved)
    bool mB63;                              // +0x63
    bool mTracksThreats;                    // +0x64 (= true; NoteThreat runs only while set)
};

#endif
