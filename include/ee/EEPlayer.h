// C:\EEX\Empire Earth\Player\EEPlayer.h (RECONSTRUCTION; file name inferred). A player (RTTI EEPlayer > RTSPlayer;
// vtable 0x4429e0), indexed in gWorld.mPlayers. Also declares the selected-group classes it holds. Only what the
// decompiled units read; names are provisional. Requires lle/LowLevelEngine.h.
#ifndef EE_EEPLAYER_H
#define EE_EEPLAYER_H

#include <deque>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include "lle/GEColor.h"
#include "ee/EECivilization.h"          // held by value (+0x468)
#include "ee/EEResourceType.h"

class EEPlayer;
void EEGameLogIncome(EEPlayer* inPlayer, long inResource, long inAmount);   // ee/EEGameLog.h

class EEComplexUnit;
class EEAIArea;                             // ee/EEAIArea.h (GetNearestArea)
class EEFormation;
class EEGroupPathRequest;                   // ee/EEGroupPathRequest.h
struct EEGroupCluster;                      // ee/EEGroupCluster.h
struct EECPASGFlags;                        // ee/EECPAttackSubGroup.h
class EEUnitGoal;
class EEPPatrolGroup;
class EEPPlayerEvent;
class SESMInstanceNode;                     // lle/SESMInstanceNode.h (EEPPlayerEventList::mSound)
class EEComputerPlayer;
class EEHumanPlayer;
class EEUnit;
class NEMessage;
class RTSUnit;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
template <class T> class U2DPoint;
typedef U2DPoint<long> EETileLocation;   // ee/EETileLocation.h
class RTSTile;
class EETown;                               // (ee/EETown.h)
class EETile;
class EEUnitClass;
class EEUnitClassUpgrade;                   // (ee/EEUnit.h; was `struct EEUnitStats`)
struct EEPUnitSwap;                         // (ee/EEPhysics.h)
class EEWUnitsOffMapInfo;                   // (ee/EEWUnitsOffMapInfo.h)
class EEMedia;
class EESoundObject;                        // (EEUnitClassUpgrade's sounds: SetClassAcknowledgeSound..SetClassSelectionSound)
class EEGraphicObject;
class EEHerd;                               // ee/EEHerd.h (mHerds's elements)
class EEUnitSet;                            // ee/EEUnitSetManager.h (mHealedUnitSet)
class EEPPatrolGroup;
template <class T> class UReferenceContainer;
class EEButtonObject;
class EEMap;
class UIHotKey;
template <class T> class U2DPoint;           // AddEvent / GetMouseChild take a U2DPoint<long>

// A player's event list (RTTI EEPPlayerEventList, vtable 0x442b54; ctor 0x16e5b9; sizeof 0x3c; provisional names)
class EEPPlayerEventList {
public:
    EEPPlayerEventList();                   // 0x16e5b9
    // 0x16e702 (EEMSendFlare: (tile, 3, the sender, -1, true)) (provisional name)
    void AddEvent(const U2DPoint<long>& inTile, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew);
    // 0x16e72e: an event at inTile seen from inFrom (EEUCBasicPriest::UpdateConversion: the conversion, type 1); both points are
    // passed straight to EEPPlayerEvent's ctor (U2DPoint<long>, was pair<long, int>) (provisional name)
    void AddPlayerEvent(const U2DPoint<long>& inTile, const U2DPoint<long>& inFrom, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew);
    virtual ~EEPPlayerEventList();
    // 0x16e94e: the next event (mNextIndex cycles over mEvents; EEAPlayerEvent::Virtual4 0x4352 reads its tile)
    // (provisional name)
    EEPPlayerEvent* GetNextEvent();
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x16e9a3, ret12/status
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x16eda4 (EEPlayer::Save; provisional name)
    long RepairEvents(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion);  // 0x16ed0c (EEPlayer::RepairPointers)
    void TickEvents(EEPlayer* inPlayer, float inElapsed);  // 0x16ee6d (EEPlayer::UpdatePlayer: gPhysics->mF98; provisional name)
    void ShowEvents(EEPlayer* inPlayer);    // 0x16e90a (EEPlayer::SetSharedLOS, after the units; provisional name)
    void HideEvents(EEPlayer* inPlayer);    // 0x16ecb8 (EEPlayer::SetSharedLOS, before; provisional name)
    void Clear(EEPlayer* inPlayer);         // 0x16e8bc (EEPlayer::Reset, the dtor, 0x16828c; provisional name)
    // EEPPlayerEventList.cpp's own members (provisional names)
    void ReleaseSound();                    // 0x16e689: dereferences mSound, clears it
    // 0x16e6ad: new EEPPlayerEvent, reactivated for inPlayer, pushed at the front
    EEPPlayerEvent* MakeEvent(const U2DPoint<long>& inTile, const U2DPoint<long>& inFrom, long inType, EEPlayer* inPlayer,
                                float inDuration, long inTextID);
    void TrimEvents(EEPlayer* inPlayer);    // 0x16e85c: drops expired events from the back while more than 15
    void LoadSounds();                      // 0x16ea2a: sounds 0x18d, 0x86, 8, 0x7d0, 0x7d0 (counted uses)
    void Announce(EEPlayer* inPlayer, EEPPlayerEvent* inEvent);   // 0x16eaa8: the local player's sound + mini-map ping
    void SetSound(SESMInstanceNode* inSound);   // 0x16ebb1 (referenced)
    void Ping(EEPPlayerEvent* inEvent);     // 0x16ebcb: the mini-map ping at the event's source
    void ShowText(EEPPlayerEvent* inEvent); // 0x16ec18: the event's string (mL1C) into the chat log
    // 0x16ec61: erases and deletes one event (deactivating it first when inDeactivate)
    void RemoveEvent(std::deque<EEPPlayerEvent*>::iterator inWhere, EEPlayer* inPlayer, bool inDeactivate);
    void StopPing(EEPlayer* inPlayer);      // 0x16ee0b: the local player's mini-map ping off
    void UnloadSounds();                    // 0x16ee2c

    std::deque<EEPPlayerEvent*> mEvents;    // +0x04 (ee/EEPPlayerEvent.h)
    float mPingTimeLeft;                    // +0x18 (= 0; the ping time left: 10000, 20000 for type 3)
    EEPPlayerEvent* mCurrent;               // +0x1c (compared with and set to events: 0x16e81f, 0x16e994)
    SESMInstanceNode* mSound;               // +0x20 (referenced: 0x16ebb1 inc [+4]; 0x16e689 Dereference)
    EESoundObject* mGarrisonSound;          // +0x24 (sound 0x86: type 2)
    EESoundObject* mSound28;                // +0x28 (sound 0x7d0)
    EESoundObject* mFlareSound;             // +0x2c (sound 8: type 3)
    EESoundObject* mUnderAttackSound;       // +0x30 (sound 0x18d: type 1)
    EESoundObject* mSound34;                // +0x34 (sound 0x7d0)
    unsigned long mNextIndex;               // +0x38 (GetNextEvent: unsigned compare with 15)
};

// A group of units (RTTI EEGroup > UReferenceCounter; EEGroup.cpp around 0x205ee4)
class EEGroup : public UReferenceCounter {
public:
    EEGroup(EEPlayer* inPlayer);            // 0x20601b
    virtual ~EEGroup();                     // +0x00 0x2060a2 (??_G 0x206086)
    virtual void Add(EEComplexUnit* inUnit);   // +0x04 0x206174 (`ret 4`; EEMSelectTheseUnits adds through it)
    // +0x08 0x206280: cancels and deletes the path request of path inPathID (mU08's key)
    virtual void Remove(long inPathID);
    // +0x0c (0x20637e for EESelectedGroup): inUnit is in the group (the hot keys' unit selection)
    virtual bool Contains(EEComplexUnit* inUnit);
    virtual bool HasPathRequest(EEComplexUnit* inUnit);  // +0x10 0x20648c: FindPathRequest(inUnit) != 0
    virtual EEComplexUnit* GetUnit(long inIndex);   // +0x14 0x165bfe: mUnits[inIndex]
    virtual unsigned long GetNumUnits();    // +0x18 0x165c0d: mUnits.size()
    // +0x1c 0x2064f4 / +0x30 0x206e61: FSFile Read/Write slots (+0x5c/+0x94) on the third argument; +0x2c 0x206dd9:
    // the units by id back to pointers (EESavedGroup/EESelectedGroup forward to them)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x1c 0x2064f4
    virtual void UpdateKindFlags();         // +0x20 0x206c41
    virtual void Clear();                   // +0x24 0x206c73 (provisional name; EEMSelectTheseUnits clears first)
    // +0x28..+0x3c (vtable 0x446f20; arguments by `ret N`; +0x34 is pure in EEGroup's vtable) (provisional name)
    virtual void RemoveUnit(EEComplexUnit* inUnit);  // +0x28 0x206d15 (removes inUnit by index)
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);  // +0x2c 0x206dd9
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x30 0x206e61
    virtual void Validate() = 0;            // +0x34 (pure)
    virtual void MoveAlong(EEGroupPathRequest* inRequest);   // +0x38 0x206680 (a found path: the units follow it)
    virtual void MoveAlongFoundPaths();     // +0x3c 0x20754e

    // 0x2cfc (out of line; slot 0x18 then slot 0x14(0)): the group's first unit, 0 when empty (provisional name;
    // EEUIEHKCreateGroup)
    EEComplexUnit* GetFirstUnit();
    // inline const accessor (Save 0x206e61 reads mUnits[i] through the const operator[]: lea; mov [eax]) (provisional
    // name)
    EEComplexUnit* GetUnitAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    void SetFormation(EEFormation* inFormation);   // 0x206ef2 (keeps a reference)
    // 0x2061ee: the rectangle around the group's units (EEAMiniMap::SendGroup; provisional name)
    void GetBounds(GERectangle& outBounds);
    // 0x206462: how many units are of class kind inKind (their class +0x1b8; provisional name)
    long CountUnitsOfKind(long inKind);
    void Assign(EEGroup* inGroup);          // 0x208ef3 (EEMSavedGroupCreate: a saved group takes the active group; provisional name)
    // 0x2063a1: this group's units become inGroup's (EESavedGroup::Assign, EESelectedGroup::SelectSavedGroup)
    // (provisional name)
    void CopyUnits(EEGroup* inGroup);
    EEGroup& CopyGroup(EEGroup* inGroup);     // 0x206121: the units and maps of inGroup (EESelectedGroup::CopySelection) (provisional name)
    // EEGroup.cpp (provisional names)
    EECPASGFlags GetFlags();                // 0x2062cb: the flags all units share (their class's slot +0x28)
    EEComplexUnit* FindUnitOfCategory(long inCategory);   // 0x206405 (class +0x1b8)
    EEComplexUnit* FindUnitWithFlags(unsigned long inFlags);   // 0x20642e (class +0x1e0 & inFlags, mL38 first)
    EEGroupPathRequest* FindPathRequest(EEComplexUnit* inUnit);   // 0x20649e (the request holding inUnit's id)
    // 0x206581: a path request for inCluster's units from inFrom to inTo (EEActiveGroup::RequestPaths)
    void RequestPath(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, EEGroupCluster* inCluster);
    void CancelPathRequest(long inPathID);  // 0x206cc9 (EEActiveGroup 0x1efa05)
    void RemoveAt(long inIndex);            // 0x206d40
    // 0x206f1a (cdecl; EECPAttackSubGroup::PickSplitUnits, 0x45e8d2, EEActiveGroup::RequestPaths): inUnits split into
    // clusters (ee/EEGroupCluster.h) by continent / boats / distance inDistance (squared first when inSquare)
    static void MakeClusters(std::vector<EEComplexUnit*>& inUnits, std::vector<EEGroupCluster*>& outClusters,
                             long inDistance, bool inSquare);
    // 0x1f07a7 (a template COMDAT among EEActiveGroup.cpp's std::sort instantiations, `ret 4` with the sorter by value):
    // std::sort(mUnits, inSorter) (EESelectedGroup::SortBy 0x209b15 is the same shape: probably one EEGroup member
    // in the original)
    template <class T> void SortBy(T inSorter) { std::sort(this->mUnits.begin(), this->mUnits.end(), inSorter); }
    // save keys (EEGroup.cpp's `$E` chains 0x205eda / 0x205f1d, guard byte 0x963270 bits 1, 2)
    static const UString sNumSelectedUnitsStringID;   // 0x963278 "EEGroup-NumSelectedUnitsStringID"
    static const UString sUnitIDStringID;             // 0x963260 "EEGroup-UnitIDStringID"

    // Layout from the ctor EEGroup(EEPlayer*) 0x20601b (sizeof 0x3c: EEActiveGroup's own members start at +0x3c;
    // dtor 0x2060a2). Names and map types inferred
    // inline (COMDAT 0xe42b9 right after its first user EEE2DMarchingIcons::Step): whether mU14 holds inKey (the
    // marching icons pass unit types) (provisional name)
    bool HasUnitType(long inKey) { return this->mUnitTypeCounts.find(inKey) != this->mUnitTypeCounts.end() ? true : false; }
    // inline (COMDAT 0x4042b8 after its first user EEAMouseClick::Execute): mU14's value for inKey, 0 when absent
    long GetUnitTypeCount(long inKey) { std::map<long, long>::iterator i = this->mUnitTypeCounts.find(inKey); return i != this->mUnitTypeCounts.end() ? i->second : 0; }

    std::map<long, EEGroupPathRequest*> mPathRequests;  // +0x08 the path requests by path id (dtor 0x2075e8)
    std::map<long, long> mUnitTypeCounts;   // +0x14 (dtor 0x95e70)
    EEComplexUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EEComplexUnit*> mUnits;     // +0x20 (GetUnit/GetNumUnits)
    EEPlayer* mPlayer;                      // +0x30 (the ctor argument)
    EEFormation* GetFormation() const { return this->mFormation; }
    EEFormation* mFormation;                // +0x34 (= EEFormation::Create(0))
    long mKindFlags;                        // +0x38 (= 0; Virtual20 clears it)
};

// The player's current selection (RTTI EESelectedGroup > EEGroup; vtable 0x442ac0; sizeof 0x48): EEPlayer's Reset news
// three of them (0x165813: +0x9ac, +0x9b0, +0x9b4) and stores +0x3c, +0x44, +0x40 after the base ctor
class EESelectedGroup : public EEGroup {
public:
    EESelectedGroup& CopySelection(EESelectedGroup* inGroup);  // 0x20915b (returns *this: probably the user operator=) (with the player's active group; reads its +0x3c/+0x44; provisional name)
    void SelectSavedGroup(EEGroup* inGroup, bool inRememberGroup);  // 0x20920c (EEMSavedGroupSelect: a saved group, true)
    // EESelectedGroup.cpp 0x209008..0x20a3c0 (provisional names)
    virtual ~EESelectedGroup();             // +0x00 0x209144 (??_G 0x165c20, EEPlayer.cpp)
    virtual void Add(EEComplexUnit* inUnit);   // +0x04 0x2091d1: Virtual40(inUnit, true)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x1c 0x20930f
    virtual void Clear();                   // +0x24 0x209340
    virtual void RemoveUnit(EEComplexUnit* inUnit);  // +0x28 0x20935e
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);  // +0x2c 0x2093a3
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x30 0x2093df
    virtual void Validate();                // +0x34 0x209469
    virtual void AddIfAbsent(EEComplexUnit* inUnit, bool inNeedsSort);  // +0x40 0x2091df (new slot)
    void AddUnit(EEComplexUnit* inUnit, bool inNeedsSort);  // 0x209185
    void SortUnits();                       // 0x20942f (std::sort by EESGUnitSorter when mB44 is set)
    // 0x209b15 (a template COMDAT among EESelectedGroup.cpp's std::sort instantiations): std::sort(mUnits, inSorter);
    // (provisional name; its instantiator includes <algorithm>; VC6 binds std::sort at the instantiation)
    template <class T> void SortBy(T inSorter) { std::sort(this->mUnits.begin(), this->mUnits.end(), inSorter); }
    void ShowSelectionAreas();              // 0x209500
    void RefreshSelectionAreas(EEComplexUnit* inUnit);  // 0x2097b3
    unsigned short GetContinent();          // 0x20924f

    // inline (EEPlayer::Reset 0x165813 news three: EEGroup(inPlayer) out of line, then the vptr
    // and +0x3c, +0x44, +0x40; the vtable and ??_G 0x165c20 are emitted in EEPlayer.cpp)
    EESelectedGroup(EEPlayer* inPlayer) : EEGroup(inPlayer) { this->mSavedGroup = NULL; this->mNeedsSort = false; this->mChangeTime = 0; }
    EEGroup* mSavedGroup;                   // +0x3c (= 0; EEMChangeFormation gives it the formation too)
    long mChangeTime;                       // +0x40 (= 0)
    bool mNeedsSort;                        // +0x44 (= false)
};

class EECalamityClass;
class EETechTreeNode;

// A group the commands act on (RTTI EEActiveGroup > EEGroup; vtable 0x446cf8; ctor 0x1ebc86(EEPlayer*); sizeof 0xb8:
// `new 0xb8` in EEMessage.cpp's Executes). The unit-command messages build one per formation slot (1..5) from the
// selected group and push it into EEPlayer +0x854. Names are provisional.
class EEActiveGroup : public EEGroup {
public:
    EEActiveGroup(EEPlayer* inPlayer);      // 0x1ebc86
    virtual ~EEActiveGroup();               // +0x00 0x1ebd27 (??_G 0x1ebd0b)
    virtual void Add(EEComplexUnit* inUnit);   // +0x04 0x1ebcfd: Virtual48(inUnit, false)
    virtual void Clear();                   // +0x24 0x1eeb6d (the dtor calls it first)
    // its own slots (vtable 0x446cf8) provisional names; types inferred
    // +0x40 0x1ebdfc (Virtual44: mUnits): inUnits' clusters (EEGroup::MakeClusters) each request a path from their
    // first unit's tile to the goal's; true when there were clusters
    virtual bool RequestPaths(std::vector<EEComplexUnit*>& inUnits);
    virtual void RequestAllPaths();         // +0x44 0x1ebdf2
    virtual void AddMember(EEComplexUnit* inUnit, bool inRemoveFromOld);  // +0x48 0x1ebd73 (EEMRepairUnit: each unit sent to repair)
    virtual void ComputeValues();           // +0x4c 0x1ed692 (the ctor's last call)
    // 0x1ed9d4: the selected group's units for one slot (IsInFormationSlot); inSort 0: SortUnits first, 1: sorted afterwards
    // by EEAGIncreasingVelocityUnitSorter
    void Fill(EESelectedGroup* inGroup, long inSort, long inSlot);
    void Finish();                          // 0x1edaf3: ClearGoal, into the player's mFinishedGroups (referenced), mBB4 = true
    bool IsInFormationSlot(EEComplexUnit* inUnit, long inSlot);  // 0x1edb21: inUnit belongs to formation slot inSlot (0..5)
    void KeepAttackers();                   // 0x1eec0b (SpreadAroundUnit: keeps the units that can attack)
    bool IsReserved(const GE3DPoint& inLocation);  // 0x1efaf0: a mU3C location at (or near) inLocation
    bool IsCellTaken(const GE3DPoint& inLocation);  // 0x1efba8: a mU3C location in inLocation's unit region
    void LayOutFormation();                 // 0x1ebf85 (Plan for commands 8000 and 0x2ee1: the formation's placement)
    void SpreadForUnload(long inTileX, long inTileY);  // 0x1ef5ce (EEMessage 0xc8615: as SpreadAroundTile, the transports stay)
    // 0x1ef86c (EEMessage 0xbc48f: a calamity's casters for inTarget; the others attack it)
    void SplitCalamityCasters(EECalamityClass* inCalamity, EETechTreeNode* inNode, EEUnit* inTarget);
    virtual void RemoveUnit(EEComplexUnit* inUnit);  // +0x28 0x1efa05 (EEGroup's, then the active group and mU3C)
    virtual void Validate();                // +0x34 0x1efcd7 (drops the dead units)
    // 0x1edc93: mGoal = inGoal, +0xb5 = inFlag, +0xb6 = (kind 1 and its +0x33)
    void SetGoal(EEUnitGoal* inGoal, bool inFlag);
    void Plan();                            // 0x1edcc0 (after SetGoal)
    void KeepNonCitizens();                 // 0x1eebb9
    // 0x1edbb2..0x1edc72: a unit's stat, or the group's when mBB6 is set (SetGoal: a kind-1 goal with its +0x33)
    // (provisional names)
    float GetMaxVelocity(EEComplexUnit* inUnit);  // 0x1edbb2: mF70 / mStats->mFAC
    float GetAcceleration(EEComplexUnit* inUnit);  // 0x1edbd0: mF7C / mStats->mFA8
    float GetMaxAngularVelocity(EEComplexUnit* inUnit);  // 0x1edbee: mScroll / mStats->mFB4
    float GetAngularAcceleration(EEComplexUnit* inUnit);  // 0x1edc0f: mF94 / mStats->mFB0
    float GetInverseMaxVelocity(EEComplexUnit* inUnit);  // 0x1edc30: mFA0 / mStats->mFD0
    float GetInverseAcceleration(EEComplexUnit* inUnit);  // 0x1edc51: mFA4 / mStats->mFC4
    float GetInverseAngularAcceleration(EEComplexUnit* inUnit);  // 0x1edc72: mFAC / mStats->mParam8
    void KeepGroundAttackers();             // 0x1eef30
    void ResetUnitsPaths();                 // 0x1edac5 (EEMPathTo)
    void KeepCitizens();                    // 0x1eef6c (EEMBuildWall)
    void SpreadAroundResource(EEUnit* inTarget);  // 0x1eefbe (EEMGatherResource)
    void SpreadAroundTile(long inTileX, long inTileY);  // 0x1ef2d4 (EEMDropoffResource: the drop-off's tile)
    void SpreadAroundUnit(EEUnit* inTarget, long inTileX, long inTileY);  // 0x1eecb2 (EEMAttackUnit: the target and its tile)
    void KeepMobileUnits();                 // 0x1ef56d
    void TellGroup(EEComplexUnit* inUnit, const GE3DPoint& inLocation);  // 0x1efca8 (the teleport calamity)
    // inline (out-of-line COMDAT 0xbb02a after its first user EEMAddPatrolPoint::Execute; 25 callers, none expands
    // it): defined in ee/EEActiveGroup.h, where EEUnitGoal is complete (its delete calls the virtual dtor)
    void ClearGoal();

    std::map<unsigned int, GE3DPoint> mDestinations;  // +0x3c (operator[] 0x1efd6c: 12-byte values; 0x20-byte nodes; dtor 0x1efdac: _Init ICF-folded with set<UString>'s; types inferred)
    std::vector<GE3DPoint> mU48;            // +0x48 (12-byte elements: _Buy 0x150afb)
    std::vector<GE3DPoint> mU58;            // +0x58
    EEUnitGoal* mGoal;                      // +0x68 (SetGoal; deleted through its virtual dtor)
    long mAction;                           // +0x6c (= 5000; the messages store a goal id: 0x1f4d, 0x7d2, ...)
    float mSpeed;                           // +0x70 +0x70..+0xac: Virtual4C 0x1ed692: per stat (units' mStats +0xac, +0xa8, +0xb4, +0xb0) the mean, max, min
    float mMaxSpeed;                        // +0x74
    float mMinSpeed;                        // +0x78
    float mAcceleration;                    // +0x7c
    float mMaxAcceleration;                 // +0x80
    float mMinAcceleration;                 // +0x84
    float mTurnSpeed;                       // +0x88
    float mMaxTurnSpeed;                    // +0x8c
    float mMinTurnSpeed;                    // +0x90
    float mTurnAcceleration;                // +0x94
    float mMaxTurnAcceleration;             // +0x98
    float mMinTurnAcceleration;             // +0x9c
    float mInverseSpeed;                    // +0xa0 +0xa0..+0xac: 1 / the means (0 when not > 0)
    float mInverseAcceleration;             // +0xa4
    float mInverseTurnSpeed;                // +0xa8
    float mInverseTurnAcceleration;         // +0xac
    float mCapFactor;                       // +0xb0 (world table entry 0x31; the means are capped at the min times it)
    bool mIsFinished;                       // +0xb4
    bool mIsQueued;                         // +0xb5
    bool mMovesTogether;                    // +0xb6
    bool mIsSpecialAttack;                  // +0xb7
};

// A player's tech tree (RTTI EETechTree > RTSTechTree, vtable 0x446114, sizeof 0x554, ctor 0x1cbcc2: the player's
// Reset news it into +0x9cc) and its nodes (RTTI EETechTreeNode > RTSTechTreeNode, vtable 0x446150, sizeof 0x24,
// ctor 0x1cedce). Were EETechnologyTable/EETechnology (the typedefs keep that spelling compiling; names.txt 18a4 now
// carries the RTTI names). Slots named by offset. Layouts and member functions decoded
// from src/Empire Earth/Tech Tree/ (EETechTree.cpp 0x1cba2f, EETechTreeEpoch.cpp 0x1cdf25, EETechTreeNode.cpp
// 0x1cedce); names are provisional.
class EETechTree;

// What a tech tree node's +0x08 points to (provisional name; the logic bomb tests +0x48 == 4). sizeof 0x84
// (the nodes' Load news 0x84 bytes; the implicit ctor 0x1ce39e builds the map and the vector). Most fields are copied
// from the 0x10c-byte database record (EETechTreeNode::Load 0x1cf370).
struct RTSTechTreeNodeData {
    std::map<long, long*> mClassCosts;      // +0x00 per unit class a new long[7] of costs (Virtual0C; the node's
                                            //  operator= copies them with memcpy 0x1c)
    std::vector<unsigned long> mUnitClasses;    // +0x0c (Contains 0x1cf6e5; Load reads each with Read(unsigned long&))
    long mCosts[7];                         // +0x1c the costs by resource (Load zeroes all seven, then copies five)
    long mCostResource;                     // +0x38
    long mRequiredStatistic;                // +0x3c
    long mDefaultUnitClass;                 // +0x40 (0x2a995: past the end of mUnitClasses)
    // inline: the cast-calamity goal loads it before the other pushes (0x220b72) (provisional name)
    unsigned long GetCalamity() const { return this->mCalamity; }
    unsigned long mCalamity;                // +0x44 (a calamity's EECalamityEnum: the cast-calamity goal; unsigned: StartCalamity 0x66662d `jbe`)
    long mKind;                             // +0x48 the kind (3, 4, 5; the logic bomb's 4)
    long mID;                               // +0x4c the node's id (an epoch: its position; else >= 1000, EETechTreeEpochOf)
    long mResearchedEvent;                  // +0x50 EEEvent id run when it is researched (Virtual14/1C load/unload it)
    long mObsoleteEvent;                    // +0x54 EEEvent id run when it becomes obsolete
    long mBuildTime;                        // +0x58 (Virtual10)
    long mPrerequisite1;                    // +0x5c the four prerequisites (ids; SetPrerequisite's flags 2/4/8/0x10)
    long mPrerequisite2;                    // +0x60
    long mPrerequisite3;                    // +0x64
    long mPrerequisite4;                    // +0x68
    long mUnitClass;                        // +0x6c the gUnitClassTable index of what it builds (EEMBuildWall)
    long mRequiredStatisticValue;           // +0x70
    long mNameID;                           // +0x74
    long mHotKey;                           // +0x78
    bool mMakesUnit;                        // +0x7c (EEMResearchTechnology)
    bool mCountsTowardEpoch;                // +0x7d
    bool mShowInEditor;                     // +0x7e
    bool mObsoletedByDefault;               // +0x7f (Load: sets the node's mScenEditorObsoleted)
    bool mAvailableWithEpoch;               // +0x80
    bool mResearchedWithEpoch;              // +0x81
};

// RTTI RTSTechTreeNode (vtable 0x446174). Entirely inline: its ??_G 0x1cfed2 (user-declared empty dtor: the vptr
// store) and the two accessors 0x1cfeca/0x1cfece close EETechTreeNode.cpp, the only unit that uses its vtable (the
// EETechTreeNode dtor's last store).
class RTSTechTreeNode {
public:
    // inline (EETechTreeNode's ctor 0x1cedce: these stores come first, its own init list next, the one vptr store last)
    RTSTechTreeNode() : mIsResearched(false), mIsObsolete(false), mIsAvailable(false), mData(NULL) {}
    virtual ~RTSTechTreeNode() {}           // +0x00 (??_G 0x1cfed2)
    virtual bool GetIsAvailable() { return this->mIsAvailable; }  // +0x04 0x1cfeca
    virtual bool GetIsObsolete() { return this->mIsObsolete; }  // +0x08 0x1cfece

    // members (EETechTreeNode's ctor 0x1cedce zeroes them all before its one vptr store; RTSTechTreeNode's own slots
    // read +0x05/+0x06; the split between the two classes is inferred)
    bool mIsResearched;                     // +0x04 researched (Virtual18 sets it)
    bool mIsObsolete;                       // +0x05 obsolete (SetObsolete 0x1cfbfe)
    bool mIsAvailable;                      // +0x06 available (SetPrerequisite 0x1cf995 once all five flags are set)
    RTSTechTreeNodeData* mData;             // +0x08
};

class EETechTreeNode : public RTSTechTreeNode {
public:
    EETechTreeNode();                       // 0x1cedce
    virtual ~EETechTreeNode();              // +0x00 0x1cee25 (??_G 0x1cee09)
    virtual bool GetIsAvailable();          // +0x04 0x1cf686
    virtual bool GetIsObsolete();           // +0x08 0x1cf742
    // the cost of resource inResource for unit class inUnitClass, scaled (EETechTree::SumCosts passes 2..6, 1.0f)
    virtual long GetCost(long inResource, long inUnitClass, float inScale);     // +0x0c 0x1cef26
    virtual long GetBuildTime();            // +0x10 0x1cee02 (mU08->mU58)
    virtual void LoadMedia(EEPlayer* inPlayer);   // +0x14 0x1cf882 (loads the media; EESELoadThread Handle406 0x1723e3)
    virtual long ResearchNode(EEComplexUnit* inUnit);  // +0x18 0x1cfa53 (inUnit: the researching unit) (researches it: 0, 2 or 9)
    virtual void UnloadMedia(EEPlayer* inPlayer);  // +0x1c 0x1cfcc9 (unloads the media; Handle407 0x172b00)

    void ClearClassCosts();                 // 0x1ceeb3 (delete[]s mU08->mClassCosts' arrays, then clear())
    long Unload();                          // 0x1ceefc (deletes mU08; 0)
    void GetCostString(UWideString& outString, long inUnitClass, float inScale);   // 0x1cf0e2
    long Load(FSFile& inFile, EETechTree* inTree);  // 0x1cf370 (one 0x10c-byte database record)
    bool Contains(long inUnitClass);        // 0x1cf6e5 (std::find in mU08->mUnitClasses)
    bool NeedsUnitMedia();                  // 0x1cf75b
    bool DependsOn(long inID);              // 0x1cf79a (recursive over the prerequisites)
    void SetPrerequisite(long inID);        // 0x1cf995
    void MakeAvailable();                   // 0x1cfa3c
    void SetScenEditorObsoleted(bool inValue, bool inRecurse);  // 0x1cfae9
    void SetObsolete(bool inValue, bool inKeepMedia);  // 0x1cfbfe
    void SetButton(unsigned long inButton); // 0x1cfc72
    // 0x268dc6 (in EEUIFMGMainGroup.cpp after its first user ShowCommands, called out of line: defined there): the hot key
    // mU08->mHotKey, 0 out of range (provisional name)
    UIHotKey* GetHotKey();
    EETechTreeNode& operator=(const EETechTreeNode& inOther);   // 0x1cfde2
    // 0x2a995 (a COMDAT in the AI range: inline there?): mU08->mUnitClasses[inIndex], mU08->mU40 past the end
    long GetUnitClass(unsigned long inIndex);
    // 0x8a42e (a COMDAT first emitted by EECPResearchManager.cpp, every call out of line): mU08->mUnitClasses.size();
    // (provisional name)
    unsigned long GetNumUnitClasses() { return this->mData->mUnitClasses.size(); }

    EETechTree* mTree;                      // +0x0c (GetPlayer 0x1ccb1f)
    EEButtonObject* mButton;                // +0x10 (EEButtonObject::Get of the record's +0xb0)
    long mEpoch;                            // +0x14 the epoch it belongs to
    long mObsoleteEpoch;                    // +0x18 (= 14) the epoch it becomes obsolete
    long mResearchDelay;                    // +0x1c (Virtual18 counts it down)
    unsigned char mPrerequisiteFlags;       // +0x20 the prerequisite flags (1: the epoch, 2/4/8/0x10: mU5C..mSelectedObjects)
    bool mIsAllowed;                        // +0x21 (= true)
    bool GetScenEditorObsoleted() const { return this->mScenEditorObsoleted; }
    bool mScenEditorObsoleted;              // +0x22
    bool mIsUnitMediaLoaded;                // +0x23 the media are loaded (sizeof 0x24)
};
typedef EETechTreeNode EETechnology;

// RTTI RTSTechTree (vtable 0x446120). Entirely inline: EETechTree's ctor stores mU04 and the vptr first; ??_G 0x1cdeef
// and SetPlayerIndex 0x1cdee5 close EETechTree.cpp.
class RTSTechTree {
public:
    RTSTechTree() : mPlayerIndex(0) {}
    virtual ~RTSTechTree() {}               // +0x00 (??_G 0x1cdeef)
    virtual void SetPlayerIndex(long inValue) { this->mPlayerIndex = inValue; }  // +0x04 0x1cdee5 (the player's index; provisional name)

    long mPlayerIndex;                      // +0x04 (EETechTree's ctor zeroes it before its vptr store)
};

// One epoch of a tech tree (RTTI EETechTreeEpoch > EETechTreeNode > RTSTechTreeNode, vtable 0x44612c; sizeof 0x54:
// EETechTree's ctor builds 15 of them in a loop of 0x54-byte steps; ctor 0x1cdf68 calls EETechTreeNode's 0x1cedce,
// then its own members; provisional names). The epoch is itself the node of its id, and holds the
// other nodes of its ids.
class EETechTreeEpoch : public EETechTreeNode {
public:
    EETechTreeEpoch();                      // 0x1cdf68
    virtual ~EETechTreeEpoch();             // +0x00 0x1cdfcf (??_G 0x1cdfb3)
    virtual bool GetIsAvailable();          // +0x04 0x1ce474
    virtual bool GetIsObsolete();           // +0x08 0x1ce494
    virtual long GetCost(long inResource, long inUnitClass, float inScale);     // +0x0c 0x1ce038
    virtual long GetBuildTime();            // +0x10 0x1ce0eb
    virtual void LoadMedia(EEPlayer* inPlayer);   // +0x14 0x1ce5b5
    virtual long ResearchNode(EEComplexUnit* inUnit);  // +0x18 0x1ce862
    virtual void UnloadMedia(EEPlayer* inPlayer);  // +0x1c 0x1cea53

    void DeleteNodes();                     // 0x1cdff6
    long GetCurrentSOL();                   // 0x1ce0e7
    long GetRequiredSOL() const { return this->mRequiredSOL; }  // inline (AttemptEpochAdvance 0x6f406 loads it before the GetCurrentSOL call) (provisional name)
    long Load(FSFile& inFile, EETechTree* inTree);  // 0x1ce10e (the epoch's record, then its nodes)
    long Unload();                          // 0x1ce3c9
    void GetNodeIDs(std::vector<long>& outIDs);     // 0x1ce439
    void AddSOL();                          // 0x1ce498
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x1ce4f2 (saved game)
    void CheckUnitMedia(EEPlayer* inPlayer);  // 0x1ce53f
    void LoadMediaOfKind(long inKind);      // 0x1ce624
    void MakeObsolete(long inID);           // 0x1ce675
    void ObsoleteOutdatedNodes();           // 0x1ce6e9
    void SetPrerequisite(long inID, long inPrerequisite);  // 0x1ce738
    void Unlock();                          // 0x1ce766
    void UpdateAvailability();              // 0x1ce7db
    void ResearchAll(bool inAllNodes);      // 0x1ce875
    void ResearchKind(long inKind, bool inEpochOnly); // 0x1ce8c5
    void ResearchSelf();                    // 0x1ce918
    long Research(long inID, EEComplexUnit* inUnit);   // 0x1ce930
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x1cea3e
    void UnloadMediaOfKind(long inKind);    // 0x1cea9a
    EETechTreeNode* Get(long inID);         // 0x1ceaeb (EETechTree::Get's lookup in one epoch; provisional name)
    EETechTreeEpoch& operator=(const EETechTreeEpoch& inOther);  // 0x1ceb26

    // the save key (EETechTreeEpoch.cpp's `$E` chain 0x1cdf25; guard byte 0x562dd8). Unguarded
    // a class static only renumbers an includer's `$E` symbols
    static const UString sEpochsCurrentSOL;

    std::map<long, EETechTreeNode*> mNodes; // +0x24 by id (0x18-byte nodes: _Init 0x617b6)
    std::vector<long> mLoadedUnitClasses;   // +0x30 unit classes whose media Virtual14 loaded (_Buy 0xe7d2e)
    long mRequiredSOL;                      // +0x40
    long mFirstNodeID;                      // +0x44
    long mCurrentSOL;                       // +0x48
    long mNextEpoch;                        // +0x4c
    bool mIsCountingSOL;                    // +0x50
    long mTitleID;                          // the title string (epochs.yaml; 0 in the original's data: 30145 + epoch)
};

#include "ee/EETechTreeTechID.h"   // EETechTreeTechID (own header: ee/EETechTreeTechID.h)

// A tree's epochs, one per epoch of the tech tree file (the original: a member array of 15). The epochs stay where
// they are made (a vector of owners), and mEpochs[i] reads as before.
class EETechTreeEpochList {
public:
    EETechTreeEpoch& operator[](long inIndex) { return *this->mItems[inIndex]; }
    const EETechTreeEpoch& operator[](long inIndex) const { return *this->mItems[inIndex]; }
    long size() const { return (long)this->mItems.size(); }
    void Resize(long inCount)
    {
        while (this->size() < inCount)
            this->mItems.emplace_back(new EETechTreeEpoch);
    }

private:
    std::vector<std::unique_ptr<EETechTreeEpoch>> mItems;
};

class EETechTree : public RTSTechTree {
public:
    EETechTree();                           // 0x1cbcc2
    virtual ~EETechTree();                  // +0x00 0x1cbd99 (??_G 0x1cbd7d)
    // inline in the real header (the COMDAT 0x18a4 sits in EEABuild.cpp, its first user; every caller calls it
    // out of line). An epoch's id is its position; a node is found in its epoch (EETechTreeEpochOf).
    EETechTreeNode* Get(long inID)
    {
        long theEpoch = EETechTreeEpochOf(inID);
        if (theEpoch >= 0 && theEpoch < this->mEpochs.size())
            return this->mEpochs[theEpoch].Get(inID);
        return NULL;
    }

    // inline (editor P7: refused Get calls are hoisted and ->mUnitClass is pushed from [eax+0x6c] at 0x5aa1c7,
    // 0x5aa0d1, 0x59721b): the tech's node data. Provisional name.
    RTSTechTreeNodeData* GetData(long inID) { return this->Get(inID)->mData; }
    bool GetTechIDState(long inTechID, bool& outIsResearched, bool& outIsObsolete, bool& outScenEditorObsoleted);   // 0x1cbc87 (the asserts name it)
    long Load(FSFileSpec& inFile);          // 0x1cbdf0 (the database: every epoch block of the file)
    static void BuildUnitClassInfo();       // 0x1cbe4a (gTechTree's events by unit class)
    void UnloadAll();                       // 0x1cc282
    void ApplySavedResearch();              // 0x1cc29a
    void ApplySavedScenEditorObsoleted();   // 0x1cc330
    void GetNodeIDs(std::vector<long>& outIDs);     // 0x1cc4af
    void GetEpochNodeIDs(long inEpoch, std::vector<long>& outIDs);   // 0x1cc4cc
    void GetChecksum(ESChecksum& ioChecksum);       // 0x1cc4ec
    void SumCosts(long inID, long inUnitClass, long* ioCosts);   // 0x1cc5db
    static bool GetUnitClassMediaMask(EETechTree* inTree, long inUnitClass, unsigned long* outMask);  // 0x1cc710
    static bool IsLatestEffect(EETechTree* inTree, long inID, long inUnitClass, long inKind);  // 0x1cc9e8
    EEPlayer* GetPlayer();                  // 0x1ccb1f
    void AdvanceEpoch();                    // 0x1ccb34
    void ApplySavedState(bool inApplyObsoleted, bool inApplyResearch);  // 0x1ccb4a
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x1ccbc9 (saved game)
    void OnResearched(long inID, EEComplexUnit* inUnit);  // 0x1ccdac (body: inUnit->Virtual70(inID); both callers pass NULL)
    void MakeObsolete(long inID);           // 0x1ccdea
    void ObsoleteOutdatedNodes();           // 0x1cce23
    void SetPrerequisite(long inID, long inPrerequisite);  // 0x1cce4c
    void AddObsoleteID(long inID);          // 0x1cce89
    void AddResearchedID(long inID);        // 0x1cce9c
    void AddScenEditorObsoletedID(long inID);  // 0x1cceaf
    void Research(long inID, EEComplexUnit* inUnit);   // 0x1ccec2 (inUnit: the researching unit, passed on to EEEvent::Trigger)
    void ResearchToEpoch(long inEpoch);     // 0x1ccf5d
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x1cd046
    void SetEpoch(long inEpoch);            // 0x1cd2a7
    void SetEpochRange(long inStart, long inEnd);   // 0x1cd2ed
    void ResetEpochRange(long inStart, long inEnd);  // 0x1cd30a
    void SetNodeAvailable(long inID, bool inValue);  // 0x1cd33b
    void SetPlayer(EEPlayer* inPlayer);     // 0x1cd358
    void UpdateInterface(bool inEpochChanged);  // 0x1cd374
    EETechTree& operator=(const EETechTree& inOther);   // 0x1cd3cb
    // 0x267eaa: the epoch the statistics group shows (inline, defined in ee/EEUIFMGGStatistics.h: refused at its
    // one call site, the COMDAT follows EEUIFMGGStatistics::Select; provisional name)
    long GetShownEpoch() const;

    // the save keys (EETechTree.cpp's `$E` chains 0x1cba2f..0x1cbc47; guard byte 0x562dc0). Unguarded
    // a class static only renumbers an includer's `$E` symbols
    static const UString sNumEpochs;        // "EETechTree-NumEpochs"
    static const UString sNumTechIDs;       // "EETechTree-NumTechIDs"
    static const UString sTechNodeID;       // "EETechTree-TechNodeID"
    static const UString sNodeIsResearched; // "EETechTree-NodeIsResearched"
    static const UString sNodeIsObsolete;   // "EETechTree-NodeIsObsolete"
    static const UString sCurrentEpoch;     // "EETechTree-CurrentEpoch"
    static const UString sStartEpoch;       // "EETechTree-StartEpoch"
    static const UString sEndEpoch;         // "EETechTree-EndEpoch"

    // Layout (from the ctor 0x1cbcc2; provisional names)
    EETechTreeEpochList mEpochs;            // +0x008 one per epoch (the original: 15, the ids below 15)
    std::vector<long> mResearchedIDs;       // +0x4f4
    std::vector<long> mObsoleteIDs;         // +0x504
    std::vector<long> mScenEditorObsoletedIDs;  // +0x514
    std::vector<EETileCoordinate> mSavedSOLs;  // +0x524 (8-byte elements: _Buy 0x14823b; the epochs' saved {id, mU48})
    EEPlayer* mPlayer;                      // +0x534 (GetPlayer: else gWorld.mPlayers[mU04])
    long GetCurrentEpoch() const { return this->mCurrentEpoch; }
    long mCurrentEpoch;                     // +0x538 the current epoch (EEMusicManager 0x132022 passes it to
                                            // EEMusicObject's pick)
    long mStartEpoch;                       // +0x53c
    long mEndEpoch;                         // +0x540 (= 14)
    long mEpochsBeforeWonders;              // +0x544 (= 1)
    long mMediaID;                          // +0x548
    long mEpochNameID;                      // +0x54c (= 30145: 0x75c1 + the epoch, a string id)
    bool mIsLoaded;                         // +0x550
};
typedef EETechTree EETechnologyTable;
extern EETechTree gTechTree;                // 0x52fd98 (Empire Earth.cpp: the database's tree)

// One point of a player's statistics series (provisional names; 8 bytes). Not EETileCoordinate (trivial ctor)
// nor U2DPoint<long> (operator= 0x4d28b copies +0 first): the inline default ctor zeroes both (0x16d4c6 `and [ebp-8],0;
// and [ebp-4],0`; resize's default argument pushes 0, 0) and operator= 0x16d389 is out of line in
// EEPlayerStatistics.cpp (before its first user) and copies +4 first. Both unsigned long: Load/Save use FSFile's
// unsigned long Read/Write (+0x5c/+0x94) for each field; +0 compares unsigned (jb/jae/jbe).
struct EEStatisticsPoint {
    EEStatisticsPoint() : mValue(0), mTime(0) {}
    EEStatisticsPoint& operator=(const EEStatisticsPoint& inPoint);   // 0x16d389
    unsigned long mValue;                   // +0x00 (Set's inValue; the series' mU10 keeps the maximum)
    unsigned long mTime;                    // +0x04 (Set: gPhysics->mLD0; the post-game graph's x)
};

// One statistics series of a player (provisional name; sizeof 0x18; 31 of them in EEPlayerStatistics): the
// player-statistics ctor builds each with the vector's `_Buy(0)` and then 0x16dd12 (+0x10 = 0, +0x14 = false).
struct EEStatisticsSeries {
    // inline: the ctor 0x16d453 and CompactEpochHistory's local build the vector (`_Buy(0)`) and call Reset
    EEStatisticsSeries() { this->Reset(); }
    EEStatisticsSeries& operator=(const EEStatisticsSeries& inSeries);   // 0x16d39c
    // inline, refused (COMDAT 0x16dd12 in EEPlayerStatistics.cpp's template tail)
    void Reset() { this->mPoints.clear(); this->mPoints.resize(1); this->mMaxValue = 0; this->mKeepHistory = false; }

    std::vector<EEStatisticsPoint> mPoints;  // +0x00 the points (Reset leaves one zero point)
    unsigned long mMaxValue;                // +0x10 the largest value pushed (GetValue)
    bool mKeepHistory;                      // +0x14 false: Set replaces the last point (pop_back) before push_back
};

// What the player holds at +0x4a8: RTTI EEPlayerStatistics (vtable 0x442080; ctor 0x16d453, ??_G 0x13d3e3, dtor
// 0x16d498, Load 0x16d76c, Save 0x16d86b; sizeof 0x304). Was EEPlayerSavedGroups (renamed to the RTTI name;
// names.txt 16d507/16d545 edited in place; the old spelling stays as a typedef).
class EEPlayerStatistics {
public:
    EEPlayerStatistics();                   // 0x16d453
    virtual ~EEPlayerStatistics();          // +0x00 (??_G 0x13d3e3)
    static const UString sKey;              // "EEPlayerStatistics-" (chain 0x16d346..0x16d36d, guard 0x531eb0)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x16d76c
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x08 0x16d86b
    // 0x16d507: the hot-key group events call it with (0x14, true) before they send their message (provisional name)
    void Reset(long inSeries, long inAmount);
    void Lower(long inSeries, long inAmount);      // 0x16d4c6 (EEPlayer::RemoveWonder: the logic bomb's unit leaves; provisional name) // the second argument is added as a dword
    void Set(long inIndex, long inValue);   // 0x16d545 (EEMReportStatistics::Execute; provisional name)
    unsigned long GetValue(unsigned long inSeries);       // 0x16d5fd: mSeries[n].mU10 (post-game summary; provisional name)
    unsigned long GetLastValue(unsigned long inSeries);   // 0x16d60b: the last point's first dword, else 0 (provisional name)
    unsigned long CountIncreases(unsigned long inSeries); // 0x16d63a: points whose x rises (provisional name)
    void Clear();                           // 0x16d718 (the name cleared, the series reset; EEPlayer::Reset; provisional name)
    // 0x16d409: Clear, mU004, mU300, then the 31 series (EEWorld::RecordStatistics; EEStatisticsRecord::operator=
    // 0x13e451 calls it first)
    EEPlayerStatistics& operator=(const EEPlayerStatistics& inStatistics);
    void CompactEpochHistory();             // 0x16d95a (EEWorld::RecordStatistics; provisional name)
    // inline (COMDAT 0x16ce29 among EEPlayer.cpp's template tail: `ret 0x14`, a UWideString by value; provisional name)
    void SetName(UWideString inName) { this->mName = inName; }
    // inline, by value: the post-game summary copies a whole series (vector copy + both fields) into a temporary,
    // uses its points and destroys it (0x1e4826, 0x1e4a0b). Defined in ee/EEStatisticsRecord.h: an in-class body
    // instantiates vector<EEStatisticsPoint>'s copy ctor in every includer (provisional name)
    EEStatisticsSeries GetSeries(unsigned long inSeries);

    UWideString mName;                      // +0x004 (saved)
    EEStatisticsSeries mSeries[31];         // +0x018 (a loop of 31 in the ctor)
    unsigned long mEndTime;                 // +0x300 (saved)
};
typedef EEPlayerStatistics EEPlayerSavedGroups;

// RTSPlayer's unit hash: the units whose id & 0x7f is the bucket's index, chained through RTSUnit::mNext (0x16f433
// looks a unit up by id; provisional names)
struct RTSUnitBucket {
    // inline: RTSPlayer's ctors 0x16f095/0x16f12e zero the 128 buckets with VC6's down-counting array-ctor loop
    RTSUnitBucket() : mFirst(NULL), mCount(0) {}
    EEUnit* mFirst;                         // +0x00
    unsigned long mCount;                   // +0x04
};

// C:\EEX\RTS Engine\Player\RTSPlayer.h (RECONSTRUCTION; file name inferred; vtable 0x442b5c). Virtual slots
// from the vtable, named by offset.
class RTSPlayer {
public:
    RTSPlayer();                            // 0x16f095
    RTSPlayer(const UWideString& inName, unsigned long inPlayerID);   // 0x16f12e (EEPlayer's ctor; mSaveVisMap = inPlayerID != 0)
    virtual ~RTSPlayer();                   // +0x00 (??_G 0x16f112, dtor 0x16f1cf)
    // +0x04 0x16f1f3: Virtual28(each mPendingUnits unit, inArg1), then Virtual0C() if inArg2 (both tested as bytes;
    // EESELoadThread passes 0, 0; was (long, long))
    virtual void AddPendingUnits(bool inKeepID, bool inRelease);
    virtual void RecountPopulation() = 0;   // +0x08 (pure)
    virtual void ReleasePendingUnits();     // +0x0c
    virtual void GetChecksum(ESChecksum& ioChecksum);   // +0x10 0x16f394 (EEPPlayerHelper)
    virtual bool IsActive() = 0;            // +0x14 (pure in the vtable; RTSWorld::UpdatePlayerPopulationLimit 0x147f0d tests `al == 1`)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x18 0x16f539
    virtual void Clear();                   // +0x1c
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // +0x20 0x16f6d3 (ret 8, returns 0; was Virtual20())
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x24 0x16f6f8
    // +0x28 0x16f233: adds a unit (a new id unless inKeepID), then InsertUnit (was Virtual28())
    virtual void AddUnit(RTSUnit* inUnit, bool inKeepID);
    // +0x2c..+0x3c pure in the vtable; signatures from RTSPlayer's own calls (0x16f27f, Load 0x16f539, 0x16f7b1;
    // were all `void ()`)
    virtual void UnitRemoved(RTSUnit* inUnit) = 0;  // +0x2c (a unit leaves: after RemoveFromList)
    virtual long LoadUnit(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) = 0;    // +0x30 (loads one unit)
    virtual void RemovalsDone() = 0;        // +0x34 (after the mU004 removals)
    virtual void RemovingUnit(RTSUnit* inUnit) = 0;  // +0x38 (a unit leaves: first)
    virtual long SaveUnit(RTSUnit* inUnit, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) = 0;    // +0x3c (saves one unit)

    RTSUnit* GetUnit(unsigned long inUnitID);   // 0x16f433: by id through the buckets (RTSWorld::GetUnit 0x14804f)
    // inline: EEMLoadMainScreen passes it through a byte temporary, like NEMessage::GetOriginatingSlot
    unsigned char GetSlot() const { return this->mSlot; }
    void RemoveKilledUnits();               // 0x16f27f: walks mU004, fixes mFirstUnit/mLastUnit, the buckets (provisional name)
    bool GetObserverMode() const { return this->mObserverMode; }  // (the diplomacy form reads it through a temporary, EEUIEDSOpen 0x1d4165)
    unsigned long GetPlayerID() { return this->mPlayerID; }  // inline (EEDataPreLoading::LoadGame pushes the whole dword)
    // RTSPlayer.cpp's other non-virtual members (provisional names)
    RTSUnit* GetPendingUnit(unsigned long inIndex);  // 0x16f40f (0 past the end)
    // inline (refused: out of line through the ICF-folded COMDAT 0x138e4b; EEWorld::ReplacePlayer; provisional name)
    unsigned long GetNumPendingUnits() const { return this->mPendingUnits.size(); }
    // inline (COMDAT 0x131872 in EEGameState.cpp, its first user Create; EEWorld 0x139264 calls the same copy;
    // the 0.6f constant is loaded once, then three fmul st(1))
    void SetColor(const GEColor& inColor)
    {
        this->mColor = inColor;
        this->mShadeColor.mRed = inColor.mRed * 0.6f;
        this->mShadeColor.mGreen = inColor.mGreen * 0.6f;
        this->mShadeColor.mBlue = inColor.mBlue * 0.6f;
    }
    void Reset();                           // 0x16f464 (the ctors call it)
    void InsertUnit(RTSUnit* inUnit);       // 0x16f49e (the unit list and the id buckets; Virtual28's)
    void KillUnit(RTSUnit* inUnit, bool inForce);  // 0x16f6ae (marks inUnit->mB28, pushes it)
    long SaveUnits(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x16f7b1 (Save's)

    // the save keys (RTSPlayer.cpp's six `$E` chains; one guard byte 0x531f18; unguarded). Names from
    // the keys (provisional names).
    static const UString sSaveVisMap;       // "RTSPlayer-SaveVisMap"
    static const UString sUniqueID;         // "RTSPlayer-UniqueID"
    static const UString sPlayerNumber;     // "RTSPlayer-PlayerNumber"
    static const UString sUnitCount;        // "RTSPlayer-UnitCount"
    static const UString sHashBucketLength; // "RTSPlayer-HashBucketLength"
    static const UString sObserverMode;     // "RTSPlayer-ObserverMode"

    // Layout (no gaps; sizeof 0x468): the ctor 0x16f12e (vectors, the name, the bucket loop of 0x80, two GEColors
    // of 1.0), its Reset 0x16f464 and Load/Save. Names are provisional unless stated.
    // units (0x16f27f/0x16f35e Dereference the elements, KillUnit pushes an RTSUnit*; were vector<long>; the
    // _Buy/_Tidy/clear calls are ICF-folded vector<long>/<EECalamity*> copies)
    std::vector<RTSUnit*> mKilledUnits;     // +0x004
    std::vector<RTSUnit*> mPendingUnits;    // +0x014
    UWideString mName;                      // +0x024 (the chat log compares senders with it)
    RTSUnitBucket mUnitBuckets[128];        // +0x038
    const GEColor& GetColor() const { return this->mColor; }
    GEColor mColor;                         // +0x438 (1, 1, 1; saved)
    GEColor mShadeColor;                    // +0x444 (1, 1, 1)
    EEUnit* mFirstUnit;                     // +0x450 its units, linked through EEUnit::mNext
    EEUnit* mLastUnit;                      // +0x454 (provisional name; the unit-removal code 0x16f27f fixes both ends)
    unsigned long mNextUniqueID;            // +0x458 (saved)
    unsigned long mPlayerID;                // +0x45c (index into another player's mEnemies; saved)
    unsigned long mUnitCount;               // +0x460 (saved)
    unsigned char mSlot;                    // +0x464 (= 0xff; EEWorld::GetPlayerBySlot compares it)
    bool GetSeesAll() const { return this->mSeesAll; }
    bool mSeesAll;                          // +0x465 toggled by EEMMakeWorldVisible (0xc212b; provisional name)
    bool mSaveVisMap;                       // +0x466 (saved; checked when a unit is removed)
    bool mObserverMode;                     // +0x467 (chat: everybody / observer; provisional name)
};

class EEPlayer : public RTSPlayer {
public:
    // out of line in EEPlayer.cpp (EEHumanPlayer's ctors call them; its ??_G calls the dtor)
    EEPlayer();                             // 0x162048
    EEPlayer(const UWideString& inName, unsigned long inPlayerID);   // 0x161da5 (RTSPlayer(inName, inPlayerID) 0x16f12e first)
    virtual ~EEPlayer();                    // +0x00 0x162297
    // the FSRestorable-style slots EEPlayer overrides (EEHumanPlayer's wrappers call them)
    virtual void Clear();                   // +0x1c 0x16828c (same RTSPlayer slot)
    virtual long LoadUnit(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x30 0x167ca4
    virtual void RemovalsDone();            // +0x34 0x16854e (same RTSPlayer slot)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x18 0x165cfa
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // +0x20 0x1690ca
    bool AdoptTownPatrol(EETown* inTown);   // port: a loaded town takes back its saved patrol group (0.253 saves)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x24 0x169c46
    virtual void AddPendingUnits(bool inKeepID, bool inReleasePending);  // +0x04 0x16275a (mPendingGroups into mRunningGroups, then RTSPlayer's)
    virtual void RecountPopulation();       // +0x08 0x16379e (counts the units into mPopulationCount/mEndOfGameCount/mNumVitalBuildings)
    virtual void ReleasePendingUnits();     // +0x0c 0x163f7e
    virtual void GetChecksum(ESChecksum& ioChecksum);   // +0x10 0x164c4c (RTSPlayer's, the resources, the tech tree)
    virtual bool IsActive();                // +0x14 0x164e0c (!mObserverMode || mGameResult)
    // +0x28 0x162ba8: a unit joins the player (RTSPlayer's add plus the counts, tech nodes and statistics)
    virtual void AddUnit(RTSUnit* inUnit, bool inKeepID);
    virtual void UnitRemoved(RTSUnit* inUnit);  // +0x2c 0x164241 (a complex unit leaves: ClearGoalQueue per mGoalQueues item) (clears mPendingGroups, then RTSPlayer's)
    virtual void RemovingUnit(RTSUnit* inUnit);  // +0x38 0x168deb (an EEUnit leaves its tile; out of its region and the influence table while linked)
    // The class static save keys (EEPlayer.cpp's init chains 0x161825..0x161c55; guard bytes 0x531e98 / 0x531e10;
    // unguarded: a class static only renumbers an includer's `$E` symbols, never its bytes).
    // Member names from the keys (provisional names).
    static const UString sClassObjectType;      // "EEPlayer-ClassObjectType"
    static const UString sUnitIsComplex;        // "EEPlayer-UnitIsComplex"
    static const UString sNumResources;         // "EEPlayer-NumResources"
    static const UString sResource;             // "EEPlayer-Resource"
    static const UString sPopLimit;             // "EEPlayer-PopLimit"
    static const UString sPatrolGroups;         // "EEPlayer-PatrolGroups"
    static const UString sHaveTechTree;         // "EEPlayer-HaveTechTree"
    static const UString sDiplomaticStance;     // "EEPlayer-DiplomaticStance"
    static const UString sStatistics;           // "EEPlayer-Statistics"
    static const UString sPopulationCount;      // "EEPlayer-PopulationCount"
    static const UString sEndOfGameCount;       // "EEPlayer-EndOfGameCount"
    static const UString sObserverMode;         // "EEPlayer-ObserverMode"
    static const UString sGeneric;              // "EEPlayer-Generic"
    static const UString sSharedLOSFlag;        // "EEPlayer-SharedLOSFlag"
    static const UString sSendChatMessage;      // "EEPlayer-SendChatMessage"
    static const UString sSendChatMessageType;  // "EEPlayer-SendChatMessageType"
public:
    virtual void RefundImprovement(EEUnitClass* inClass, long inImprovement, float inFraction);  // +0x40 0x162915 (ret 0xc)
    // +0x44 (EEMDisband: the unit's technology, its type, 0.75 of its hit-point fraction: a refund) (provisional name)
    virtual void RefundTechnology(EETechnology* inTechnology, long inUnitType, float inFraction);
    // +0x48..+0xa8: inline in the real header (their bodies 0xa5486/0xa549a/0xa54a1/0xa54a7 sit in
    // EEHumanPlayer.cpp, the first unit in link order that emits EEPlayer's vtable without overriding them)
    // +0x48 0xa5486; port: counted into the game log (ee/EEGameLog.h)
    virtual void AddResource(long inIndex, long inAmount) { this->mResourceCaps[inIndex] += inAmount; EEGameLogIncome(this, inIndex, inAmount); }
    virtual void DetachUnitFromAI(EEComplexUnit* inUnit);  // +0x4c 0x16367b (sets the unit's mIsDetachedFromAI)
    // +0x50 (EEComplexUnit::QueueResearch compares GetHeroCount(1, 1) with mHeroLimit: a count) (provisional name)
    virtual long GetHeroCount(bool inByClass, bool inRequests);    // (bools: 0x164223 reads bytes; EEComputerPlayer 0x2e248 stores them as bytes)
    // +0x54 0x163a25 (ret 0x18; can the player build inTechnology: EEABuild 0x1760 passes (id, 0x20, 1, 1, 3, 0); +0x14
    // is read as a byte, +0x1c receives the reason through UResourceFile::GetString; provisional names)
    virtual bool CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason);
    virtual void NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag);  // +0x58 0x163d6f (EESMEffect 0x1be94e passes (another player, false); 0x55cee tests a byte; ret 8: EEComputerPlayer 0x2dca6 forwards both)
    virtual void DeleteHerds();             // +0x5c
    virtual void Virtual60();               // +0x60
    virtual void ClearTowns();              // +0x64
    virtual void UnitCompleted(EEComplexUnit* inUnit);  // +0x68 0x164003 (ret 4: EEComputerPlayer 0x2e1e9 forwards it)
    virtual long ScaleCost(long inCost) { return inCost; }  // +0x6c 0xa549a
    // +0x70 0x164bea (ret 0xc): reads inUnit's mStats/mUnitClass and inOther's class, then 0x16542e (provisional name)
    virtual bool CanAffordRepair(EEUnit* inUnit, EEUnit* inOther, unsigned long inValue);  // (bool: EEUGBuildRepair::EvaluateCondition tests al)
    EEUnit* GetNearestConstructionSite(EEComplexUnit* inUnit);  // 0x164820 (the next unit inUnit should build; EEUGBuildRepair::FindNextWork) (provisional name)
    virtual bool IsHuman();                 // +0x74 (EEPlayer::SwapClassButtonMedia tests `al == 1`)
    // inline accessors the diplomacy form expands (EEUIEDSOpen 0x1d4165 reads each through a temporary;
    // provisional names)
    unsigned long GetGameResult() const { return this->mGameResult; }
    float GetTributeFee() const { return this->mTributeFee; }  // (RefundTributes 0x1d5bfc: fld of the value, then fimul)
    long GetEnemy(unsigned long inPlayer) const { return this->mEnemies[inPlayer]; }
    // inline: inPlayer is this player or not at war with it (EECPCombatManager::IsValidTarget 0x473684 compares the
    // result as a byte: `cmp al, bl` for `== false`) (provisional name)
    bool IsAlly(EEPlayer* inPlayer) { return inPlayer == this || this->mEnemies[inPlayer->mPlayerID] == 0; }
    long GetResource(long inResource) const { return this->mResourceCaps[inResource]; }
    bool GetSendChatMessage(unsigned long inPlayer) const { return this->mSendChatMessage[inPlayer]; }
    virtual long GetExtraPopulation(long inUnitClass) { return 1; }  // +0x78 0xa54a1 (`push 1; pop eax; ret 4`: a long, not a bool)
    virtual long GetPopulationLimit(long inArg);  // +0x7c 0x16528b (ret 4: mB24 + mB20 + a global; EECPCombatStrategy 0x771be)
    // +0x80 (conscription 0xafe5f) (provisional name)
    // (0x16566a: arg 3 is a count, multiplied with each cost: `imul` in CanAffordScaled)
    virtual bool CanAfford(EETechnology* inTechnology, long inUnitType, long inCount);
    // +0x84 0x16569c (ret 0xc; EEComputerPlayer's 0x2f16d passes its unit third)
    virtual void DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit);
    virtual void EpochMediaLoaded(long inEpoch);  // +0x88 (0x157269 `ret 4`, empty; EETechTreeNode 0x1cf989 passes a node's +0x4c)
    virtual void PlayAcknowledgement(EEComplexUnit* inUnit);  // +0x8c 0x167f10 (a unit of the group the messages act on)
    virtual void RefreshTownGroups();       // +0x90
    virtual void UpdatePlayer();            // +0x94
    virtual void FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile);  // +0x98 (EEMSendFlare; provisional name)
    virtual void UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps);  // +0x9c 0x167f70 (ret 4; EEPhysics 0x1534d1 passes &mSound2Index7)
    virtual long FinishUnitsLoad(long inMajorVersion, long inMinorVersion);  // +0xa0 0x1690a5 (each unit's slot +0x6c; returns 0)
    virtual void EpochReached(long inEpoch);  // +0xa4 0x1695e7 (ret 4; 0..14 when EnabledChanged(); the tech tree passes it)
    virtual void SetCombatState(unsigned long inCombatState) { this->mCombatState = inCombatState; }  // +0xa8 0xa54a7
    virtual void UnitStranded(EEComplexUnit* inUnit);  // +0xac (ret 4: EEComputerPlayer 0x3ed48 references the unit)
    virtual void PayImprovement(EEUnitClass* inClass, long inImprovement);  // +0xb0 0x16b3f7 (ret 8; QueueImprovement passes the class)
    // +0xb4 (conscription 0xafe7e) (provisional name)
    virtual void Pay(EETechnology* inTechnology, long inUnitType, float inFraction);
    virtual void TakeResource(long inArg1, long inArg2);  // +0xb8 0x16b08a (two arguments by `ret 8`)
    virtual void SuggestRecomputeAllAreaEffects();  // +0xbc
    virtual void UnitPlaced(EEComplexUnit* inUnit);  // +0xc0 0x16b88f (ret 4: EEComputerPlayer 0x3ef5d forwards it)
    virtual void UnitTaken(RTSUnit* inUnit, bool inSwapped);  // +0xc4 (2 arguments: EEUnit::DepleteResource pushes (this, 0); types inferred)
    virtual void UnitGoalDone(EEComplexUnit* inUnit);            // +0xc8
    virtual void EpochMediaUnloaded(long inEpoch);  // +0xcc (0x157269 `ret 4`, empty; EETechTreeNode 0x1cfcc9 passes a node's +0x4c)
    virtual bool AvoidsEnemies(EEComplexUnit* inUnit);  // +0xd0 (ret 4, al: EEComputerPlayer 0x3f73e)
    virtual void WonderStarted(EEComplexUnit* inUnit);  // +0xd4 (ret 4: EEComputerPlayer 0x3f7ba)
    virtual bool AdvanceImprovement(unsigned long inUnitType);  // +0xd8 0x16964a (ret 4; ComputeLoadDistance passes mQueuedImprovements's key, tests al)

    // 0x16ae11: with inUnit and inFlag set, calls slot 0x24 of +0x9ac; the hot-key events call it with (0, true)
    // (provisional name)
    void UpdateSelection(EEUnit* inUnit, bool inFlag);
    // per unit class (+0x888, one per unit class; the meteor storm's Virtual28/Virtual34 with its meteor class)
    // (provisional names)
    void LoadUnitClassMedia(long inUnitClass);              // 0x16c776
    void GetHerdsOfType(std::vector<EEHerd*>& outHerds, long inUnitType);  // 0x164e78 (the herds of inUnitType's animal; EESMObject 0x1c8e37; provisional name)
    void ApplyMapRestrictions();            // 0x16c249 (the space/sea-map techs off, or the unit-selection hot keys for them; 0x13a3f9; provisional name)
    void DisableCamouflagePower();          // 0x16c5f9 (civ power 0x31: each complex unit in its unit set loses mIsCloakable/mIsCloaked, mCamouflageTimer/mU32C; VirtualXX 0x16b987; provisional name)
    void CheckButtonsPreloaded(long inUnitClass);  // 0x16c6fa (EEUnitClass::CreateUnitAt before a new unit; provisional name)
    void SetUnitClassMedia(long inUnitClass, long inMediaFlags);  // 0x167c7d
    void UnloadUnitClassMedia(long inUnitClass);            // 0x16b61c
    // logic bomb (provisional names): Stop passes the target's complex unit and false, Update the unit
    void AddWonder(EEComplexUnit* inUnit, bool inIsLoading);  // 0x1636cd
    void RemoveWonder(EEComplexUnit* inUnit);               // 0x168ff2
    void DeselectUnit(EEUnit* inUnit);                      // 0x168eb9 (the hurricane's Stop, its unit)
    // 0x164e22: the last town of the vector at +0x7cc whose area holds inUnit's tile, or 0 (the prophet analyzers'
    // 0xaa8b; provisional name)
    EETown* GetTownOf(EEUnit* inUnit);
    unsigned long GetNumTowns() const { return this->mTowns.size(); }   // inline COMDAT 0x4098f0 (emitted after its first out-of-line user, EECPASGUProphet.cpp 0x409831); /Os post-check undoes it where size() expands inside, it stays where size() is refused (provisional name)
    void LoadUnitClassButtons(long inUnitClass);            // 0x167c5a (the hurricane's Virtual28)
    void UnloadUnitClassButtons(long inUnitClass);          // 0x16b5f9 (the hurricane's Virtual34)
    void ApplyImprovement(long inUnitType, long inImprovement, bool inFlag);  // 0x16b65a (ret 0xc; arg 3 read as a byte)
    // 0x1655f6: inUnit's entry in the player's +0x7c0 vector, 0 if none (the teleport sets its +0x14 to 4)
    // (provisional name)
    EEWUnitsOffMapInfo* GetOffMapInfo(EEComplexUnit* inUnit);  // (mUnitsOffMap's entry; was a gap struct EEPlayerUnitInfo)
    void SelectUnits(std::vector<long>& inUnitIDs);   // 0x16a658 (EEMSelectUnits::Execute; provisional name)
    // selection by the EEM* messages (EEMessage.cpp Executes; provisional names)
    void AddUnitToSelectedGroup(unsigned long inUnitID, bool inFlag);   // 0x1629e4
    void AddUnitsToSelectedGroup(std::vector<long>& inUnitIDs);   // 0x162a5d
    void IncrementEpic();                   // 0x165808
    void RemoveUnitFromSelectedGroup(long inUnitID);   // 0x1688ed
    void SelectUnit(unsigned long inUnitID, bool inArg1, bool inNewGroup);  // 0x16a53b
    void UnselectAllUnits();                // 0x16bc55
    void ReloadTechTree(bool inFlag);       // 0x1697ff (EESELoadThread Handle403; provisional name)
    void LoadAllClassMedia();               // 0x167bff (Handle406, before its loop; provisional name)
    void UnloadAllClassMedia();             // 0x16b5b6 (Handle407, before its loop: per +0x8d8 entry, then clear; provisional name)
    void ClearSavedGroup(long inGroup);     // 0x163f95 (EEMSavedGroupClear)
    void AddAndRemoveUnitsFromSelectedGroup(std::vector<long>& inUnitIDs);   // 0x162816 (provisional name)
    EEPPatrolGroup* GetPatrolGroup(unsigned long inID);   // 0x16519e (mPatrolGroups's group with that id; EEUGPatrol::RepairPointers)
    void AddPatrolGroup(EEPPatrolGroup* inGroup);   // 0x16279f: referenced, pushed into mPatrolGroups (provisional name)
    long CountQueuedImprovements(unsigned long inUnitType, long inArg) const;  // 0x164f4a (EEMRemoveImprovement; const: the const vector reads and the dead const-operator[] lea; provisional name)
    void RemoveQueuedImprovement(long inUnitType, long inImprovement, bool inFlag);  // 0x16870e (EEMRemoveImprovement)
    void SetHackUnitClass(long inUnitClass);  // 0x16ae52 (EEMLoadUnitData; provisional name)
    void SetGameResult(long inState);       // 0x16af5c (EEMResign passes 2; provisional name)
    // 0x16384d (EEMImproveUnit). QueueImprovement passes 0, so the original takes a UWideString*; kept a reference until
    // EEMessage.cpp's caller passes &theText
    bool CanBuy(long inUnitType, long inImprovement, UWideString* outText);     // (QueueImprovement passes 0: a pointer)
    void QueueImprovement(unsigned long inUnitType, unsigned long inImprovement, bool inFront, bool inAlreadyPaid);  // 0x162694 (EEMImproveUnit; args 3/4 tested as bytes)
    bool CanImprove(long inUnitType, long inImprovement, UWideString* outReason);  // 0x163c6d (ret 0xc)
    void Reset();                           // 0x165813 (both ctors call it after the vptr store; the arrays and groups; hides RTSPlayer::Reset)
    const UWideString& GetName();           // 0x167eda (&mName, out of line; Reset's; provisional name)
    bool AddUnitToTown(EEComplexUnit* inUnit);
    void UpdateFreeUnitPower(); // original Player unit-class update, no arguments
    void PulseSight(bool inByID, float inSight, long inID); // original169b26, three arguments/ret12 // 0x163138 (ret 4; RepairPointers: each unit's complex unit; provisional name)
    void RecountHeroesAndTeleporters();     // 0x16c427 (RepairPointers' last call; provisional name)
    void UpdateAirTraffic();                // 0x16b63f (each mAirTraffic's Update; ComputeLoadDistance)
    unsigned long GetTeamIndex();           // 0x165561 (the index of the team holding this player, 0 if none; provisional name)
    // +0x3c 0x16a4b5 (ret 0x10): one unit's type, whether it is a unit (not 0xdf) and complex, then its Save
    virtual long SaveUnit(RTSUnit* inUnit, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    void KillHerdUnits(EEHerd* inHerd, bool inKilled);  // 0x165c3c (ret 8: each unit of a herd in state 0xb: Virtual60(inArg, 0, 0))
    void EraseQueuedImprovements(unsigned long inUnitType);  // 0x163f36 (mQueuedImprovements's entry erased; 0x12ba6d)
    void MoveQueuedImprovements(unsigned long inFrom, unsigned long inTo);  // 0x16419b (mQueuedImprovements's entry moved)
    // inline (out-of-line COMDATs after their first users in EEUICPortrait.cpp: 0x258cb8 / 0x2594e8): the improvement
    // being researched for inUnitType (0x10: none) and its progress (0.0 when absent; read as a float by 0x2594e8:
    // mQueuedImprovements's first is really a float) (provisional names)
    long GetResearchedImprovement(unsigned long inUnitType)
    {
        if (this->mQueuedImprovements.find(inUnitType) != this->mQueuedImprovements.end())
            return this->mQueuedImprovements.find(inUnitType)->second.second[0];
        return 0x10;
    }
    float GetImprovementProgress(unsigned long inUnitType)
    {
        if (this->mQueuedImprovements.find(inUnitType) != this->mQueuedImprovements.end())
            return (*this->mQueuedImprovements.find(inUnitType)).second.first;
        return 0.0f;
    }
    void RestartTowns();                    // 0x1680e4 (each town's UpdateNearWater; EEWorld::StartGame)
    void RegisterTowns();                   // 0x16513d (the towns re-registered with gContinentManager)
    void RegisterHerds();                   // 0x1650dc (player 0 before a scenario starts; provisional name)
    bool AddUnitToHerd(EEComplexUnit* inUnit);  // 0x162e22 (AddUnit result returned in AL)
    void AssignHerds();                     // 0x164453 (player 0, after RegisterHerds; provisional name)
    void AssignTowns();                     // 0x16451a (the other players, after RegisterTowns; provisional name)
    EETown* GetTownByID(long inID);         // 0x1655b5 (the town whose mL74 is inID)
    // 0x82b30 (ret 4; the argument unread): the town with the largest mL78, the earliest on ties (EECPIntelligence
    // 0x816a6; it sits among that unit's COMDATs: an inline or template member in the real header) (provisional name)
    EETown* GetLargestTown(bool* outArg);
    // 0x164cb0 (ret 8): the town nearest inTile, its closest point into outTile (EECPDefenseCommander) (provisional
    // name)
    EETown* GetNearestTown(const EETileLocation& inTile, EETileLocation& outTile);
    // template COMDAT 0x7d4f3 (EECPDefenseCommander.cpp's range): the towns on continent inContinent (0: all), the
    // last first (provisional name)
    template <class V> void GetTowns(const unsigned short& inContinent, V& outTowns)
    {
        for (long i = this->mTowns.size() - 1; i >= 0; i--)
            if (this->GetTownList()[i]->mContinent == inContinent || inContinent == 0)
                outTowns.push_back(this->mTowns[i]);
    }
    // const view (0x47d4f3, 0x47897c read the towns through the const operator[]) (provisional name)
    const std::vector<EETown*>& GetTownList() const { return this->mTowns; }
    // template COMDAT 0xa5313 (EEHerd.cpp's range; ret 8): the herds of mHerds (+0x7dc, EEHerd* elements) inPredicate
    // holds for, the last first (provisional name)
    template <class P, class V> void GetHerds(P inPredicate, V& outHerds)
    {
        for (long i = this->mHerds.size() - 1; i >= 0; i--)
            if (inPredicate(this->mHerds[i]))
                outHerds.push_back(this->mHerds[i]);
    }
    bool CanAffordSingleResource(EETechTreeNode* inNode, long inUnitClass, long inCount);  // 0x1652a0 (CanAfford's second test)
    // inline (out-of-line COMDAT 0xc350f after its first user EEMPayTribute::Execute): the tribute statistics, inDirection
    // 0 paid / 1 received (provisional names)
    void AddTribute(long inDirection, long inResource, long inAmount)
    {
        this->mTributes[inDirection * 5 + inResource - 2] += inAmount;
        this->mSavedGroups.Reset((inDirection != 0) + 0x11, inAmount);
    }
    void SetSelectedViewedUnit(RTSUnit* inUnit);  // 0x16adee (with mViewedUnit, after a selection message; provisional name)
    // 0x11c10d (out of line; EEMap.cpp's range): the same player, or neither is at war with the other (EEPPlayerEvent;
    // provisional name)
    bool IsAlliedWith(EEPlayer* inPlayer);
    // EEPPlayerEvent's (de)activation at a tile (provisional names)
    void RemoveSharedSightAt(const EETileLocation& inLocation);  // 0x164309
    void ExecuteSelectionMessage(NEMessage* inMessage);  // 0x1646fc (EEServer::PrepareLocalInput: a fast-selection message; provisional name)
    void AddSharedSightAt(const EETileLocation& inLocation);  // 0x1681e8
    // a tile's player references (the doppelganger manager follows them; provisional names)
    void RemoveSharedSight(RTSTile* inTile);  // 0x164287 (also for the allies that share the line of sight)
    void RemoveOwnSight(RTSTile* inTile);   // 0x164327
    void RemoveOwnSightAt(const EETileLocation& inLocation);  // 0x16434c
    void AddSightSharer(EEPlayer* inPlayer);  // 0x1627b9 (pushes inPlayer into mSightSharers between two walks of the units)
    void RemoveSightSharer(EEPlayer* inPlayer);  // 0x1684ab (the mirror: inPlayer out of mSightSharers; RemoveWonderPower, 0x16ab41)
    void RemoveTownID(long inID);           // 0x163605 (erases mTownsByID's entry inID)
    bool CanProduceUnitType(long inUnitType, long inArg, UWideString* outReason);  // 0x1639e8 (WriteDetailsHook for the class's technology)
    void AddUnitToRepatrol(EEComplexUnit* inUnit);  // 0x162b35 (one of its own units into mUnitsToRepatrol, referenced)
    void ClearUnitsToRepatrol();            // 0x163fbb (mUnitsToRepatrol dereferenced and cleared)
    void AddOffMapUnit(RTSUnit* inUnit);    // 0x162dab (a new EEWUnitsOffMapInfo for inUnit into mUnitsOffMap)
    void AddStrandedUnit(EEUnit* inUnit);   // 0x163043 (into mStrandedUnits once, referenced)
    bool TrySelectUnit(EEUnit* inUnit, bool inFlag);  // 0x1630ac (selects inUnit; false when another player's unit meets a selection)
    void AddConstructionSite(EEComplexUnit* inUnit);  // 0x16368f (into mConstructionSites unless it has it)
    void AddWonderPower(EEComplexUnit* inUnit);  // 0x164891 (the first unit of its type; AddWonder)
    EEComplexUnit* GetNextComplexUnit(EEUnit* inUnit);  // 0x167ede (the next unit with a complex unit after inUnit, from the first when 0)
    void AddSharedSight(EETile* inTile, long inX, long inY);  // 0x168120 (AddOwnSight, then the allies sharing the line of sight)
    void AddOwnSight(EETile* inTile, long inX, long inY);  // 0x16820e (counts this player in at the tile)
    void AddOwnSightAt(const EETileLocation& inLocation);  // 0x168266
    void RemoveConstructionSite(EEComplexUnit* inUnit);  // 0x168fdf (out of mConstructionSites)
    void RemoveAlly(EEPlayer* inPlayer);    // 0x1683da (inPlayer out of mAllies)
    void RemovePatrolGroup(EEPPatrolGroup* inGroup); // 0x168889 (out of mPatrolGroups, dereferenced)
    void RemoveOffMapUnit(RTSUnit* inUnit);  // 0x168e30 (its EEWUnitsOffMapInfo out of mUnitsOffMap, deleted)
    void ResetTechTree();                   // 0x1697b4 (the player's tech tree from gTechTree)
    void SetAllResources(long inValue);     // 0x16ab1b (mResourceCaps[2..6] = inValue)
    void ClearFootprint(EEComplexUnit* inUnit);  // 0x16a70a (thiscall: `ret 4`, MarkInvalid passes ecx = this; this unused)
    void SetDiplomaticStance(EEPlayer* inOther, long inStance, EEMap* inMap);  // 0x16ab41 (the diplomatic stance towards inOther; EEWorld::SetDiplomaticStance; provisional name)
    void RefreshTerrainVisibility();        // 0x16aec0 (EEWorld::RestartUnits on player 0; provisional name)
    void DumpMedia(FSFileSpec& inFile);     // 0x16436a (EEWorld::DumpMedia; provisional name)
    void RestorePatrols();                  // 0x16bc6e (EEWorld::StartGame after RestartTowns; provisional name)
    void SetCivilizationPoints(long inEpoch);  // 0x16ab2f (mCivilization.SetEpoch)
    void SetEpoch(long inEpoch);            // 0x16addc (the tech tree's SetEpoch)
    void UnloadAllMedia();                  // 0x16b572 (unloads and clears mLoadedMedia)
    void SetSharedLOS(bool inFlag, EEMap* inArg); // 0x16ae5f (ret 8; sets mSharedLOS around the units' slots +0x24/+0x0c)
    bool HasLoadedUnitID(unsigned long inUnitID);   // 0x16bbf8 (mLoadedUnitIDs has it)
    void RemoveUnitFromHerd(EEComplexUnit* inUnit);  // 0x168d33: remove a real ComplexUnit from herd ownership
    void RemoveUnitFromTown(EEComplexUnit* inUnit);  // 0x168f03: remove a real ComplexUnit from town ownership
    void RefreshUpgrades();                // 0x169990: refresh player upgrades, original this/no args
    void EnableCamouflagePower();          // 0x16c548: civilization powers, original this/no args
    void ResetWonderTimers(EEPlayer* inPlayer);  // 0x16c1e1 (both players' mWonders units: mCloaked = the time)
    void NoticeForeignUnit(EEUnit* inUnit);  // 0x16442b (mCombatState = 1 for a unit of a player other than 0)
    EEGroup* GetActiveGroupOf(EEComplexUnit* inUnit);  // 0x164b4f (the group of mRunningGroups, else mPendingGroups, holding inUnit)
    long GetCitizenPassability();           // 0x1650a3 (flags 1/2/4/8 from unit class 0x20's upgrade +0x160)
    EEAIArea* GetNearestArea(const EETileCoordinate& inTile, bool inSameContinent);  // 0x164781 (ret 8; EECPResourceManager::FindResourceTarget takes its slot +0x18) (provisional name)
    long GetTotalPopulation(bool inFirst, bool inSecond);  // 0x1651e0 (mPopulationCount plus the units' AddedToParent: mPendingUnits, then those in a 0x1d container)
    void RemoveWonderPower(EEComplexUnit* inUnit);  // 0x16890e (the last unit of its type left; RemoveWonder)
    bool CanAffordTechnology(long inTechnology, float inScale, UWideString* outReason);  // 0x163d1f (CanAffordScaled for the technology; string 0x273a when not)
    bool CanAffordScaled(EETechTreeNode* inNode, long inUnitClass, float inScale, long inCount);  // 0x16542e (ret 0x10; arg 4 an `imul` count)
    long GetImprovementLevel(unsigned long inUnitType, long inImprovement) const;  // 0x164ed7 (an improvement's level; Virtual40; const: as CountQueuedImprovements)
    // 0x164fac: whether the improvement is at its maximum level (the class's mImprovements->mMaxLevel against
    // GetImprovementLevel; EEUIBUpgrade; provisional name)
    bool IsImprovementMaxed(unsigned long inUnitType, long inImprovement);
    void KillStrandedUnits();               // 0x165c8c (mStrandedUnits's units back into the world)
    void AddLoadedMedia(EEMedia* inMedia);  // 0x162740 (pushes into mLoadedMedia unless 0; provisional name)
    void DelayWonderTimers(long inAmount);  // 0x162b08 (adds to every unit's mCloaked in mWonders; provisional name)
    bool RemoveLoadedMedia(EEMedia* inMedia);  // 0x16843f (UnloadPlayerEffects unloads the media when true; provisional name)
    void LoadPlayerEffects();               // 0x16c7f2 (provisional name)
    void UnloadPlayerEffects();             // 0x16c829
    long GetNumHeroes(bool inFirst, bool inSecond);   // 0x164223 (mNumHeroes and/or mNumHeroesInProduction; provisional name)
    long GetWonderTime(long inIndex) const;  // 0x165644 (mWonders's unit inIndex's mCloaked; provisional name)
    unsigned long GetNumWonderUnits() const;   // 0x16565f: mWonders->GetSize() (provisional name)
    // per unit class (mUnitClassUpgrades[inUnitClass] when inUnitClass < EEUnitClassCount(); provisional names)
    EEUnitClassUpgrade* GetUpgrade(long inUnitClass) { return this->mUnitClassUpgrades[inUnitClass]; }  // inline COMDAT 0x1bcae9
    void SwapClassButtonMedia(unsigned long inUnitClass, EEButtonObject* inFirst, EEButtonObject* inSecond);  // 0x163d72
    void SetClassAcknowledgeSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex);  // 0x163db9
    void SetClassCreationSound(unsigned long inUnitClass, EESoundObject* inSound);  // 0x163de1
    void SetClassDeathSound(unsigned long inUnitClass, EESoundObject* inSound);  // 0x163e05
    void SetClassModel(unsigned long inUnitClass, EEGraphicObject* inModel);  // 0x163e29 (EEModel)
    void SetClassButton(unsigned long inUnitClass, EEButtonObject* inButton);  // 0x163e4d (the unit class's button; EEEvent SetEventButton)
    void SetClassSelectionSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex);  // 0x163e71

    // Layout (no gaps; sizeof 0xb80: EEHumanPlayer adds a UWideString at +0xb80, sizeof 0xb94):
    // the ctor 0x161da5 (member ctors), its Reset 0x165813 (the array loops: 20, 4, 17, 10, 10, 17, 17 elements) and
    // Load/Save. Container element types and every unnamed member are provisional.
    EECivilization mCivilization;           // +0x468 (ctor 0x12c57d; provisional name)
    EEPlayerStatistics mSavedGroups;        // +0x4a8 (the player's statistics; provisional name)
    UString mCivilizationFile;              // +0x7ac (saved)
    std::vector<EEWUnitsOffMapInfo*> mUnitsOffMap; // +0x7bc (AddOffMapUnit pushes a new one per unit taken off the map; push_back 0x16c8b5)
    std::vector<EETown*> mTowns;            // +0x7cc (the player's towns: EECPILocation::Update 0x80a57 walks them)
    std::vector<EEHerd*> mHerds;            // +0x7dc
    std::vector<EEUnit*> mStrandedUnits;    // +0x7ec (AddStrandedUnit pushes a referenced unit unless std::find has it)
    std::vector<EEMedia*> mLoadedMedia;     // +0x7fc (AddLoadedMedia pushes the loaded media: push_back 0x16c8e1)
    std::map<long, long> mWonderTypeCounts;  // +0x80c (node 0x18: an 8-byte value; dtor 0x95e70)
    std::map<long, EETown*> mTownsByID;  // +0x818 (dtor 0x16c938; find 0x16c94c)
    std::vector<UString> mMediaWarnings;    // +0x824 (the ctor's _Buy 0x88ba6 / the dtor's _Tidy 0x665e4 are vector<UString>'s)
    std::vector<EEPlayer*> mSightSharers;   // +0x834 (0x1627b9 push_backs `this`: vector<EEPlayer*>'s push_back 0x44e8f)
    // inline (out-of-line COMDAT 0x152525 after its first user EEPhysics::GetOutOfSynchChecksum) (provisional name)
    unsigned long GetNumRunningGroups() { return this->mRunningGroups.size(); }
    std::vector<EEGroup*> mRunningGroups;   // +0x844 (Virtual04 0x16275a copies mPendingGroups into it)
    std::vector<EEGroup*> mPendingGroups;   // +0x854 (the command messages push their new EEActiveGroups)
    std::vector<EEGroup*> mFinishedGroups;  // +0x864 (EEActiveGroup::Finish pushes itself)
    std::vector<EEPPatrolGroup*> mPatrolGroups;  // +0x874 (AddPatrolGroup: push_back 0x16c986)
    EEUnitClassUpgrade* GetUnitClassUpgradesAt(unsigned long inIndex) const { return this->mUnitClassUpgrades[inIndex]; }
    std::vector<EEUnitClassUpgrade*> mUnitClassUpgrades; // +0x884 (resized to EEUnitClassCount() by Reset: one per unit class; the forwarders
                                            // 0x163db9.. index it
    EEPlayer* GetAlliesAt(unsigned long inIndex) const { return this->mAllies[inIndex]; }
    std::vector<EEPlayer*> mAllies;         // +0x894 (EEPPlayerHelper push_backs into it; EEMSendFlare copies it with
                                            // vector<EEPlayer*>::operator= 0x1119ec and flares each)
    std::vector<EEComplexUnit*> mUnitsToRepatrol;  // +0x8a4 (AddUnitToRepatrol pushes a referenced unit unless std::find has it; ClearUnitsToRepatrol dereferences and clears)
    UReferenceContainer<EEComplexUnit*>* mConstructionSites;  // +0x8b4 (0x16368f: Contains 0x27c820, then its +4 vector)
    std::vector<long> mConstructionSiteIDs;  // +0x8b8
    std::vector<unsigned long> mLoadedUnitIDs;    // +0x8c8 (HasLoadedUnitID: std::find; element type inferred)
    std::vector<long> mSoundLoadedClasses;  // +0x8d8
    std::vector<long> mWonderPowers;        // +0x8e8
    UReferenceContainer<EEComplexUnit*>* mWonders;       // +0x8f8 (Reset news one: vtable 0x4429d8)
    std::vector<long> mWonderIDs;           // +0x8fc
    // +0x90c (node 0x28: per unit type (unsigned: `jb` in find 0x16ccf6) a long and the improvements queued
    // (CountQueuedImprovements counts them from index 1; copy 0x16d323); types inferred; ctor 0x16d077)
    std::map<unsigned long, std::pair<float, std::vector<unsigned long> > > mQueuedImprovements;  // (float: Save Write slot 0x7c, ComputeLoadDistance; FSSaveVector<unsigned long>)
    EEPPlayerEventList mEvents;             // +0x918 (RTTI EEPPlayerEventList, ctor 0x16e5b9; provisional name)
    EEGroup* mGroups[20];                   // +0x954 the saved (hot-key) groups, indexed by a byte (EEMSavedGroupClear
                                            // 0xc5117); Reset calls each one's slot +0x24
    EEComputerPlayer* mComputerPlayer;      // +0x9a4 non-zero for a computer player (EEArtificialIntelligence::
                                            // CollectComputerPlayers 0x2cd0e; EEPPlayerHelper)
    EEHumanPlayer* mHumanPlayer;            // +0x9a8 dynamic_cast<EEHumanPlayer*>(this) in both EEHumanPlayer ctors
                                            // (0xa543d, 0xa54fe)
    EESelectedGroup* mActiveGroup;          // +0x9ac (an EESelectedGroup: Reset 0x165813) (slot +0x18 = count, +0x24, +0x14; provisional name)
    EESelectedGroup* GetSelectedGroup() const { return this->mSelectedGroup; }
    EESelectedGroup* mSelectedGroup;        // +0x9b0
    EESelectedGroup* mL9B4;                 // +0x9b4 (Reset news a third EESelectedGroup)
    EEUnitSet* mHealedUnitSet;              // +0x9b8 (ComputeLoadDistance: Contains(unit type) heals those units)
    class EEAirTrafficControl* mAirTraffic[4];   // +0x9bc (Reset: each a new 0x20-byte object, ctor 0x14e87c(this)); indexed by an airport class's mAirTrafficType - 1 (EEMClearAirportCommands 0xbd588)
    // inline: the garrison goal's ComputeUpgrade (0x226c08) loads its argument before `this` (provisional name)
    EETechTree* GetTechTree() const { return this->mTechnologies; }
    EETechnologyTable* GetTechnologies() const { return this->mTechnologies; }
    EETechnologyTable* mTechnologies;       // +0x9cc (provisional name; conscription 0xafe3b; Reset: new 0x554 bytes)
    RTSUnit* mViewedUnit;                   // +0x9d0 (Reset dereferences it)
    RTSUnit* mSelectedViewedUnit;           // +0x9d4 (a referenced unit: SetSelectedViewedUnit swaps it)
    unsigned long mChatType;                // +0x9d8 0..3 (the chat form's radio buttons 3, 2, 4, 5; saved)
    unsigned long mEnemies[17];             // +0x9dc non-zero: at war with that player (calamity targets; unsigned: Save writes it with Write(const unsigned long&))
    unsigned long mCombatState;             // +0xa20 (saved)
    unsigned long mHackUnitClass;           // +0xa24 (kNoUnitClass: none (the original: 0x367); a gUnitClassTable index, compared unsigned: EEMLoadUnitData)
    unsigned long mGameResult;              // +0xa28 (saved)
    float mConversionFactor;                // +0xa2c (= 1)
    float mTributeFee;                      // +0xa30 (the tribute fee: the diplomacy form loads it with fld)
    float mHighGroundBonus;                 // +0xa34 (saved)
    float mHealTimer;                       // +0xa38 (saved)
    unsigned long mScenarioVariables[10];   // +0xa3c (zeroed together; saved)
    long mMaxStartingCitizens;              // +0xa64
    long mDamageFrom[17];                   // +0xa68 (a loop of 17 with the bytes at +0xb30)
    long mNumAttacks;                       // +0xaac
    unsigned long mTributes[10];            // +0xab0 (zeroed together; unsigned: Save)
    long mLastAcknowledgementTime;          // +0xad8
    long mLastAcknowledgedGroup;            // +0xadc
    long GetHeroLimit() const { return this->mHeroLimit; }
    long mHeroLimit;                        // +0xae0
    long GetMiningLimit() const { return this->mMiningLimit; }  // inline (EEUGBuildRepair::GatherNearBuilding 0x61ed5e: loaded into a temp before the GetResourceUsers call)
    long mMiningLimit;                      // +0xae4
    unsigned long mFreeUnitCounter;         // +0xae8 (saved)
    unsigned long mFreeUnitInterval;        // +0xaec (saved)
    long mLAF0;                             // +0xaf0
    // +0xaf4: indexed by a resource (EEMAddRemoveResources 0xbb28c, 0x12bbf6, 0x702c4); Reset zeroes
    // [2..6] with five stosd. Provisional name and count
    long mResourceCaps[7];
    long mPopLimit;                         // +0xb10 (saved)
    long mPopulationCount;                  // +0xb14 (saved)
    long mEndOfGameCount;                   // +0xb18 (saved)
    long mNumCapitols;                      // +0xb1c (saved)
    long mShareBonus;                       // +0xb20
    long mPopulationLimitBonus;             // +0xb24
    long mNumHeroes;                        // +0xb28
    long mNumHeroesInProduction;            // +0xb2c
    bool mSendChatMessage[17];              // +0xb30 (the loop of 17)
    bool mAreaConversion;                   // +0xb41
    bool mBB42;                             // +0xb42
    bool mSharedLOS;                        // +0xb43 (saved)
    bool mBB44;                             // +0xb44
    bool mInstantBuild;                     // +0xb45
    bool mHealsUnits;                       // +0xb46
    bool mBehindFirewall;                   // +0xb47
    bool mDisconnected;                     // +0xb48 (saved)
    bool mCivilizationLocked;               // +0xb49 (saved)
    bool mCivilizationChosen;               // +0xb4a (saved)
    bool mCivUpgradesPurchased;             // +0xb4b (saved)
    bool mEarthquakeShaken;                 // +0xb4c (EECalamityManager::Update clears it for players 1..)
    bool mShiftMode;                        // +0xb4d EEMEnterShiftMode/EEMExitShiftMode (provisional name)
    bool mJITManufacturing;                 // +0xb4e
    // +0xb50..+0xb74 untouched by the ctor, Reset and Load/Save. +0xb50: indexed by a resource 2..6 as
    // mResourceTrade[resource - 2] (EEMExchangeResources::Execute 0xbe3b1: `[player + resource*4 + 0xb48]`;
    // name and count inferred: resources 2..6 as mResourceCaps' Reset)
    long mResourceTrade[5];
    long mInsuranceRefunds[5];              // +0xb64..+0xb74 (Reset zeroes them, Save writes them in one loop)
    long mNumVitalBuildings;                // +0xb78 (saved)
    long mNumTeleporters;                   // +0xb7c
};

#endif
