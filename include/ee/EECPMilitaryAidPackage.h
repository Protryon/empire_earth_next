// C:\EEX\Empire Earth\AI Thread\EECPMilitaryAidPackage.h (RECONSTRUCTION; file name inferred). A computer player's
// military aid package: the attack groups it sends to one threatened town
// (RTTI EECPMilitaryAidPackage, no base; vtable 0x43c0c8: the destructor only, ??_G 0x785a1; sizeof 0x3c: new 0x3c in
// EECPDefenseCommander's Load 0x79eb8; ctor 0x84d8d(inPlayer): a vector<long> +0x04, zeroes +0x14..+0x38, the player
// at +0x1c, +0x34 from the player's +0xed8.. by +0xbd8). The defense commander keeps them in its map +0x04 by town id;
// EECPCombatManager 0x472ec3/0x472ff5 halve +0x34. Its members live in the units before 0x868b8 (0x84d8d..0x85a97).
// The dtor is inline (??_G 0x785a1 and EECPDefenseCommander's stack copies expand it: vptr, ReleaseSubGroups, the vector).
// Names are provisional. Requires <vector>, lle/U2DPoint.h.
#ifndef EE_EECPMILITARYAIDPACKAGE_H
#define EE_EECPMILITARYAIDPACKAGE_H

class EEComputerPlayer;
class EEPlayer;
class EETown;
class FSFile;
struct EECPDCTownThreat;                    // ee/EECPDefenseCommander.h

class EECPMilitaryAidPackage {
public:
    EECPMilitaryAidPackage(EEComputerPlayer* inPlayer);   // 0x84d8d
    virtual ~EECPMilitaryAidPackage() { this->ReleaseSubGroups(); }  // +0x00 (??_G 0x785a1)

    void ReleaseSubGroups();                // 0x8572c (the dtor's call; EECPDefenseCommander::FreePackages)
    // the state machine (mL2C 0..3; SetState 0x85131 runs the entry action) (provisional names)
    void Gather();                          // 0x84e42 (state 1: sends sub groups)
    void Fn85006();                         // 0x85006 (state 3)
    void Recall();                          // 0x85084 (state 0: the sub groups go home)
    void SetState(long inState);            // 0x85131 (ret 4; EECPDefenseCommander 0x77e4b..)
    void Deploy(std::vector<EECPDCTownThreat>& inThreats);    // 0x853e3 (ret 4; Gather passes GetWorstThreats's list)
    void DropDeadSubGroups();               // 0x8566a: drops the ids of dead sub groups
    void Fn8588A();                         // 0x8588a (state 2)
    unsigned long UpdateGathering(EETown* inTown);  // 0x85892 (ret 4; state 1's update)
    unsigned long Fn85922(EETown* inTown);  // 0x85922 (ret 4; state 3's update)
    unsigned long UpdateRecalled(EETown* inTown);  // 0x85981 (ret 4; state 0's update)
    unsigned long Fn859D7(EETown* inTown);  // 0x859d7 (ret 4; state 2's update)
    unsigned long Update(unsigned long inTime);    // 0x85163 (ret 4): the next time it wants an update (0: none)
    bool IsFinished();                      // 0x85105: finished
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x851ec
    // 0x8500e (ret 4, hidden result): the first tile of the target town (mTownID of mTarget), also stored in mLocation
    // ((0, 0) when the town is gone or empty) (provisional name)
    U2DPoint<long> GetLocation();
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x856e4
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x85789
    EECPMilitaryAidPackage& operator=(const EECPMilitaryAidPackage& inOther);   // 0x85a36 (ret 4)
    unsigned long GetNumUnits() const { return this->mSubGroupIDs.size(); }  // inline (ICF-folded into 0x91f11) (provisional name)

    std::vector<unsigned long> mSubGroupIDs;  // +0x04 (_Buy(0)) the sub group ids sent
    U2DPoint<long> mLocation;               // +0x14 the town's hull centre
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x1c (the ctor's argument)
    EEPlayer* mTarget;                      // +0x20 the town's owner
    float GetThreat() const { return this->mThreat; }
    long GetTownID() const { return this->mTownID; }
    float mThreat;                          // +0x24
    long mTownID;                           // +0x28 (EETown::mL74; the defense commander's map key)
    long mState;                            // +0x2c
    unsigned long mNextUpdateTime;          // +0x30 (EECPDefenseCommander 0x7b442: no update before it)
    unsigned long mNextSendTime;            // +0x34 (shr: unsigned; 0x7b442: kept until then)
    unsigned long mPlayerID;                // +0x38 the attacker's player id (Load/Save: Read/Write(unsigned long))
};

// Military aid package states (EECPMilitaryAidPackage::SetState 0x485131 / Update 0x485163).
enum EECPMilitaryAidState {
    kAidStateRecalled = 0,  // SetState 0x485131 case 0 calls Recall (setrilo AidPackageRecall: the sub groups go home); Update case 0 -> UpdateRecalled
    kAidStateGathering = 1,  // SetState case 1 calls Gather (setrilo AidPackageGather: sends sub groups); Update case 1 -> UpdateGathering
};

#endif
