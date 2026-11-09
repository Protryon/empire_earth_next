// C:\EEX\Empire Earth\AI Thread\EECPPersonnel.h (RECONSTRUCTION; file name inferred; vptr, members from the ctor, the
// three virtuals). A computer player's personnel (RTTI EECPPersonnel, no base; vtable 0x43c258: dtor (??_G 0x86915),
// Load 0x87275, Restore 0x87d12, Save 0x88057; new 0x38, ctor 0x868b8 in EECPCombatManager's ctor 0x71d74;
// EECPCombatManager +0x54; EECPPersonnel.cpp 0x868b8..0x89574). Names are provisional.
// Requires <map>, <set>, ee/EECPUnitSelector.h.
#ifndef EE_EECPPERSONNEL_H
#define EE_EECPPERSONNEL_H

#include <map>
#include <set>
#include <vector>
#include "lle/U2DPoint.h"

struct EEPUnitSwap;                         // (ee/EEPhysics.h)
class EEUnit;
class EEComplexUnit;
class EEComputerPlayer;
class FSFile;
class EECPPersonnel;

// A personnel request (0x40 bytes, ctor 0x85ace(owner, id), referenced; the ids come from EECPPersonnel::mNextID).
// Only what EECPPersonnel.cpp calls; the class lives in the units before 0x868b8. (provisional name)
class EECPPersonnelRequest : public UReferenceCounter {
public:
    // EECPPersonnel.cpp's first functions 0x85ace..0x868b8 (vtable 0x43c234: ??_G 0x85b29, Restore 0x86624, Save
    // 0x86742; Load 0x862c9 is not virtual). Names are provisional
    EECPPersonnelRequest(EECPPersonnel* inPersonnel, long inID);   // 0x85ace
    virtual ~EECPPersonnelRequest();        // +0x00 0x85b45 (??_G 0x85b29)
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // +0x04 0x86624
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x08 0x86742
    long ReleaseOfType(long inClass, long inCount);  // 0x86020 (ret 8): drops up to inCount unfilled slots of inClass
    float UpdateFill();                     // 0x860c0: mF24 = filled / slots (returns it: the caller pops st0)
    void Clear();                           // 0x860fa: dereferences the units, clears the slots
    long GetTownID();                       // 0x8619f: mTownID from mTile's town
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x862c9
    bool Remove(EEComplexUnit* inUnit);     // 0x865a2 (ret 4)
    bool Replace(EEComplexUnit* inOld, EEComplexUnit* inNew);   // 0x86878 (ret 8)
    void ReleaseAll();                      // 0x85d51
    long GetPopulation();                   // 0x86144 (EECPDefenseCommander 0x799fa: its unit value) (provisional name)
    // (EECPExplorationManager): 0x41eb3 mUnits.size() == 0 (out of line); 0x8655c takes the last unit
    bool IsEmpty();                         // 0x41eb3 (provisional name)
    void PopSlot();                         // 0x8655c (provisional name)
    bool TryAssign(EEComplexUnit* inUnit);  // 0x85b64 (ret 4): fills the slot waiting for inUnit's id, else one of its class (provisional name)
    // 0x861de (ret 0xc): resets the request to one slot per class and per unit id (provisional name)
    void Setup(long inID, std::vector<long>& inClasses, std::vector<long>& inUnitIDs);
    void Cancel();                          // 0x85e94 (EECPResourceManager 0x8ecfd: a request that was not filled) (provisional name)
    unsigned long GetNumSlots() { return this->mUnits.size(); }  // inline (out-of-line COMDAT 0x86fae) (provisional name)

    // the ctor 0x85ace(inPersonnel, inID) stores these (provisional names)
    struct Unit {                           // 0x10 bytes (0x9f869 is the vector's size(), 0x7fffc its back())
        EEComplexUnit* mUnit;               // +0x00
        unsigned long mClass;               // +0x04 a gUnitClassTable index
        unsigned long mUnitID;              // (Save: Write(unsigned long); Load: 0 becomes -1)
        bool mB0C;                          // +0x0c (Restore sets it for a unit it found)
        Unit() : mUnit(NULL), mClass(0), mUnitID(0), mB0C(false) {}
        Unit(const Unit& inOther) : mUnit(inOther.mUnit), mClass(inOther.mClass), mUnitID(inOther.mUnitID), mB0C(inOther.mB0C) {}
        // user-declared: copy (0x8948b) / copy_backward copy member by member, the bool as a byte
        Unit& operator=(const Unit& inOther)
        {
            this->mUnit = inOther.mUnit;
            this->mClass = inOther.mClass;
            this->mUnitID = inOther.mUnitID;
            this->mB0C = inOther.mB0C;
            return *this;
        }
    };
    std::vector<Unit> mUnits;               // +0x08
    U2DPoint<long> mTile;                   // +0x18 (Load/Save through U2DPoint<long>'s 0x412e04/0x412e38; GetTownID)
    EECPPersonnel* mPersonnel;              // +0x20 its owner
    float mFill;                            // +0x24 filled / slots (EECPExplorationManager::Update hands the unit over at 1.0)
    unsigned short mContinent;              // +0x28 (Read/Write(unsigned short))
    long mTownID;                           // +0x2c the town id (GetTownID)
    unsigned long mStartTime;               // +0x30
    unsigned long mDeadline;                // +0x34 (-1; Restore of an old save: mL30 + 600000)
    long GetNumFilled() const { return this->mNumFilled; }   // inline: EECPPersonnelRequest::TryAssign 0x485b64
    long mNumFilled;                        // +0x38
    long GetID() const { return this->mID; }
    long mID;                               // +0x3c (EECPPersonnel::ReleaseRequest's argument)
};

class EECPPersonnel {
public:
    // inline (COMDAT 0x9bb76 after EECPUnitSelector.cpp's functions, its first user GatherCandidates): every unit to inGatherer
    void Gather(EEAIFOPAvailablePersonnelGatherer& ioGatherer)
    {
        for (std::multimap<long, EEUnit*>::iterator it = this->mUnits.begin(); it != this->mUnits.end(); ++it)
            ioGatherer.AddUnit(*it);
    }

    EECPPersonnel(EEComputerPlayer* inPlayer);   // 0x868b8 (ret 4)
    virtual ~EECPPersonnel();               // +0x00 (??_G 0x86915, dtor 0x86931: ClearAll, then the members)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x87275
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // +0x08 0x87d12
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x88057

    void ReleaseRequest(long inID);         // 0x86e3c (ret 4): erases the request, ReleaseAll, Dereference
    // 0x5f82b (ret 4; ICF-folded twin): mRequests.find(inID)'s request, 0 if none (provisional name)
    EECPPersonnelRequest* GetRequest(long inID);
    // 0x874db (ret 0xc): a new request for one of inClasses (EECPExplorationManager) (provisional name)
    EECPPersonnelRequest* NewContinentRequest(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inContinent);
    // 0x875ab (ret 0x10): the same for a town (its id, its hull's centre; EECPASGAir::Virtual08) (provisional name)
    EECPPersonnelRequest* NewTownRequest(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inTownID,
                                  const U2DPoint<long>& inCenter);
    void Update(unsigned long inTime);      // 0x86e81 (ret 4)
    void UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps);  // 0x87687 (ret 4)
    void ClearAll();                        // 0x87c5b: dereferences every unit and request, clears both maps
    void ReturnUnit(EEUnit* inUnit);        // 0x87fc0 (ret 4)
    void AddNewUnit(EEComplexUnit* inUnit);  // 0x881cc (ret 4)
    bool RemoveUnit(EEComplexUnit* inUnit);  // 0x88337 (ret 4; returns al: RemoveFromRequests's result)
    bool RemoveFromRequests(EEComplexUnit* inUnit);  // 0x87ccd (ret 4): every request's Remove (provisional name)
    void RequestProduction(long inUnitClass, long inTownID, unsigned short inContinent);  // 0x87f9e (ret 0xc): one more unit requested (provisional name)
    void SubmitRequest(EECPPersonnelRequest* inRequest);  // 0x86aee (ret 4): FillRequest (provisional name)
    void FillRequest(EECPPersonnelRequest* inRequest);  // 0x86b40 (ret 4): fills inRequest from the free units (provisional name)
    void ReleaseOfType(long inUnitClass, long inCount);  // 0x86afa (ret 8; EECPUnitManager 0x94cac: a dropped request) (provisional name)
    bool DraftUnit(EEComplexUnit* inUnit);  // 0x86959 (ret 4): drafts inUnit into an attack group, else remembers its id (provisional name)
    EECPPersonnelRequest* NewRequest();     // 0x86ffb: a referenced request with the next id (provisional name)
    bool OfferUnit(EEComplexUnit* inUnit);  // 0x87025 (ret 4): the first unfilled request that takes inUnit (provisional name)
    void KillUnitsForMilitary();            // 0x873f5 (Update's first call) (provisional name)
    void UnloadTransport(EEComplexUnit* inUnit);  // 0x88403 (ret 4; Update) (provisional name)
    void DraftDeferred();                   // 0x86a54 (clears mL1C; EEComputerPlayer::RefreshTownGroups 0x3cec9) (provisional name)
    void RetireObsoleteUnits();             // 0x8707b (EEComputerPlayer::EnterEpoch 0x3c9c5) (provisional name)

    // vptr                                 // +0x00
    // +0x04 the free units by unit type (referenced): a multimap (ReturnUnit/Load insert through 0x11005e, which
    // returns an iterator, not map's pair; RemoveUnit walks the equal keys from find(type))
    std::multimap<long, EEUnit*> mUnits;
    std::map<long, EECPPersonnelRequest*> mRequests;   // +0x10 request id -> request (referenced)
    std::set<unsigned long> mDeferredUnits;  // +0x1c (unit ids; 0x86959 inserts the unit's id, DraftDeferred clears it)
    EEComputerPlayer* mPlayer;              // +0x28
    unsigned long mNextID;                  // +0x2c the next request id (0x86ffb)
    unsigned long mNextUpdateTime;          // +0x30 (the next update time: jbe/ja)
    bool mDrafting;                         // +0x34
};

#endif
