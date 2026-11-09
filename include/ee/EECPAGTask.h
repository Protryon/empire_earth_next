// C:\EEX\Empire Earth\AI Thread\EECPAGTask.h (RECONSTRUCTION; file name inferred). A task of a computer
// player's attack group (RTTI EECPAGTask, no base; vtable 0x43a6c0: ??_G, Load, Restore, Save; sizeof 0x34;
// EECPAGTask.cpp 0x4162f..0x4192c). The unit and the player are saved as ids and swizzled back by Restore. Names
// are provisional. Requires ee/EECPILocation.h, lle/FSFile.h.
#ifndef EE_EECPAGTASK_H
#define EE_EECPAGTASK_H

class EEComplexUnit;
class EEPlayer;

class EECPAGTask {
public:
    EECPAGTask();                           // 0x4162f
    EECPAGTask(const EECPAGTask& inTask);   // 0x41686
    virtual ~EECPAGTask() { this->ReleaseUnit(); }  // +0x00 (inline: ??_G 0x4165d expands it)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x04 0x416f5
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);                // +0x08 0x41791
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x0c 0x4180a

    long GetPlayerID();                     // 0x416e3 (-1 without a player)
    EEPlayer* GetPlayer() const { return this->mPlayer; }  // inline: the groups load it before the other pushes (0x456b77)
    void ReleaseUnit();                     // 0x417f5
    EECPAGTask& operator=(const EECPAGTask& inTask);   // 0x418c9
    void SetUnit(EEComplexUnit* inUnit);    // 0x41912 (referenced)

    EECPILocation mLocation;                // +0x04
    EEComplexUnit* GetUnit() const { return this->mUnit; }
    EEComplexUnit* mUnit;                   // +0x1c (referenced; saved as its unit id)
    EEPlayer* mPlayer;                      // +0x20 (saved as its player id)
    float mAggression;                      // +0x24
    long mType;                             // +0x28
    long mMode;                             // +0x2c
    bool IsActive() const { return this->mIsActive; }   // inline: EECPAttackGroup::AssignAirTask 0x454f62 (scheduling of the mType store), 0x45502e
    bool GetIsActive() const { return this->mIsActive; }
    bool mIsActive;                         // +0x30
    bool mB31;                              // +0x31
};

// Attack group task modes (EECPAGTask::mMode): who issued the task. Names are provisional (by behaviour).
enum EECPAGTaskMode {
    kAGModeNone = 0,   // EECPAGTask ctor (provisional name)
    kAGModeDefense = 1,  // EECPDefenseCommander (CreateDefendGroup, AssignSubGroupTask, Restore) (provisional name)
    kAGModeCombat = 2,  // EECPCombatManager tasks (AddAttackTask/AddDefendTask/Attack/DefendUnit), counter attacks, military aid packages, scenario groups; ShouldRefreshTask tests it (provisional name)
    kAGModeWonder = 3,  // EECPCombatManager::AttackUnit 0x4754e7 (an enemy wonder) and the own-wonder escort in EECPCombatManager::Update (provisional name)
};

// Attack group task types (EECPAGTask::mType); EECPAttackSubGroup::CheckTaskArrival 0x460a27 maps each to its arrival
// state. 3/4/5 (subclass arrival virtuals) and 9 are never stored in AoC (unnamed). Names are provisional (by
// behaviour).
enum EECPAGTaskType {
    kAGTaskNone = 0,           // EECPAGTask ctor; no task (provisional name)
    kAGTaskLocation = 1,       // a location or area task (AddAttackTask 0x471f16 / AddDefendTask 0x47203a with an area or an own unit); arrival -> 6002 hold (provisional name)
    kAGTaskTerritory = 2,      // an enemy territory (EESMEffect::CreateAIMilitaryGroup attack without a unit; AssignLand/Air/Sea/SpaceTask); arrival -> 10000 (provisional name)
    kAGTaskUnit = 7,           // a task unit (AddAttackTask / AddDefendTask with another player's unit; the own-wonder escort in EECPCombatManager::Update); arrival when it moved -> 6006 follow (provisional name)
    kAGTaskReturn = 8,         // stored by the transports' SetState and AssignLand/Air/Sea/SpaceTask; condition 3009 -> 6007 return to the rally point (provisional name)
    kAGTaskAttackLocation = 10,  // EECPCombatManager::Attack 0x4740ee; handled like type 1 (provisional name)
    kAGTaskDefendWonder = 11,  // EECPCombatManager::DefendUnit 0x47590f (a wonder); handled like type 1 (provisional name)
    kAGTaskAttackWonder = 12,  // EECPCombatManager::AttackUnit 0x4754e7 (an enemy wonder); handled like type 7 (provisional name)
};

#endif
