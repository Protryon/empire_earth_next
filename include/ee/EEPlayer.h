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
#include "rust/sim/tech.h"             // EeUnitsPlayerSaved (EEPlayer's saved numbers)
#include "rust/sim/units.h"
#include "rust/sim/tech.h"              // EeTechNodeFields, EeTechTreeFields (the tech tree's records)
#include "rust/sim/units.h"
class FSFile;
// New (R5b.4, round 28): the Rust saves' file primitives (ee_units player_save.rs's READ_ULONG .. WRITE_BOOL and the
// unsigned short's two, shared by the player's, the physics', the map's and the units' hosts; EEPlayer.cpp); -1 for
// another operation
int32_t EESavePrimitive(FSFile& inFile, int32_t inOp, EeUnitsPlayerSaveIo* io);
// the same as a Rust host callback on an FSFile* context (the statistics', events' and queued improvements' saves)
int32_t EESaveFilePrimitive(void* inFile, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io);
inline bool EESaveIsPrimitive(int32_t inOp)
{
    return (inOp >= EE_UNITS_PLAYER_SAVE_READ_ULONG && inOp <= EE_UNITS_PLAYER_SAVE_WRITE_BOOL) ||
           inOp == EE_UNITS_PLAYER_SAVE_READ_USHORT || inOp == EE_UNITS_PLAYER_SAVE_WRITE_USHORT;
}

class EEPlayer;
void EEGameLogIncome(EEPlayer* inPlayer, long inResource, long inAmount);   // ee/EEGameLog.h

class EEComplexUnit;
class EEAIArea;                             // ee/EEAIArea.h (GetNearestArea)
class EEFormation;
class GERectangle;
class EEAutoPathRequest;                    // ee/EEAutoPathRequest.h (a group path request's path)
struct EECPASGFlags;                        // ee/EECPAttackSubGroup.h
class EEUnitGoal;
typedef struct EeUnitsPatrolGroup EEPPatrolGroup;
class SESMInstanceNode;                     // lle/SESMInstanceNode.h (EEPPlayerEventList::mSound)
class EESoundObject;
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
typedef EeUnitsOffMapInfo EEWUnitsOffMapInfo;   // (ee/EEWUnitsOffMapInfo.h)
class EEMedia;
class EESoundObject;                        // (EEUnitClassUpgrade's sounds: SetClassAcknowledgeSound..SetClassSelectionSound)
class EEGraphicObject;
class EEHerd;                               // ee/EEHerd.h (mHerds's elements)
class EEUnitSet;                            // ee/EEUnitSetManager.h (mHealedUnitSet)
typedef struct EeUnitsPatrolGroup EEPPatrolGroup;
template <class T> class UReferenceContainer;
class EEButtonObject;
class EEMap;
class UIHotKey;
template <class T> class U2DPoint;           // AddEvent / GetMouseChild take a U2DPoint<long>

// A player's event list (RTTI EEPPlayerEventList, vtable 0x442b54): the alerts at a map location (1 under attack, 2 a
// garrison's message, 3 a flare).
// port (the player lane, round 1): the events (EEPPlayerEvent's members) and every decision on them are a Rust object
// (mList: ee_units player_events.rs); the C++ keeps the alert sounds and answers the world's parts (Host)
class EEPPlayerEventList {
public:
    EEPPlayerEventList();                   // the alert sounds loaded
    virtual ~EEPPlayerEventList();
    // an event at inTile (EEMSendFlare: (tile, 3, the sender, -1, true)); AddPlayerEvent's with inFrom, the location
    // whose sight an attack shares (EEUCBasicPriest::UpdateConversion: the conversion, type 1)
    void AddEvent(const U2DPoint<long>& inTile, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew);
    void AddPlayerEvent(const U2DPoint<long>& inTile, const U2DPoint<long>& inFrom, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew);
    bool GetNextEventSource(U2DPoint<long>& outSource);   // the next event's source (EEAPlayerEvent); false without one
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    long RepairEvents(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion)
    {
        this->Call(inPlayer, EE_UNITS_EVENTS_REPAIR, 0.0f);
        return 0;
    }
    void TickEvents(EEPlayer* inPlayer, float inElapsed) { this->Call(inPlayer, EE_UNITS_EVENTS_TICK, inElapsed); }
    void ShowEvents(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_SHOW_ALL, 0.0f); }   // around a sight change
    void HideEvents(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_HIDE_ALL, 0.0f); }
    void Clear(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_CLEAR, 0.0f); }   // (NULL: no world call)
    void Call(EEPlayer* inPlayer, uint32_t inWhat, float inElapsed);
    static int32_t Host(void* inList, void* inPlayer, int32_t inOp, const EeUnitsPlayerEvent* inEvent);

    EeUnitsPlayerEvents* mList;             // the events (Rust)
    SESMInstanceNode* mSound;               // the alert playing (referenced)
    EESoundObject* mSounds[5];              // under attack, garrison, flare, 0x7d0 twice (loaded with the list)
};

// The unit groups (port, the groups lane round 1): Rust objects (ee_units group_obj.rs; were EEGroup.cpp,
// EESavedGroup.cpp, EESelectedGroup.cpp, EEActiveGroup.cpp 0x1ebc47..0x20a3c0). EEGroup and its kinds (RTTI EEGroup >
// UReferenceCounter; EESavedGroup, EESelectedGroup, EEActiveGroup > EEGroup) name the Rust object: its head
// EeUnitsGroup holds what the C++ reads in place (mPlayer, mFormation, mKindFlags, a selection's mSavedGroup,
// mChangeTime, mNeedsSort, a hot-key group's mIndex, an active group's mGoal and mFields); the classes add no data
// and each member is one call (the original's virtuals dispatch on the kind in Rust). Made by New (one reference),
// freed by the last Dereference. The C++ the bodies call is ee_units_group_host (EEGroup.cpp).
class EEGroup : public EeUnitsGroup {
public:
    EEGroup() = delete;
    EEGroup(const EEGroup&) = delete;
    ~EEGroup() = delete;
    void Reference() { ee_units_group_reference(this); }
    void Dereference() { ee_units_group_dereference(this); }
    void Add(EEComplexUnit* inUnit) { ee_units_group_do(this, 0, inUnit); }
    void Remove(long inPathID) { ee_units_group_cancel_path(this, inPathID); }   // the path request's cancel
    bool Contains(EEComplexUnit* inUnit) { return ee_units_group_contains(this, inUnit); }
    EEComplexUnit* GetUnit(long inIndex) { return static_cast<EEComplexUnit*>(ee_units_group_unit(this, inIndex)); }
    unsigned long GetNumUnits() { return ee_units_group_len(this); }
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
        { return ee_units_group_file(this, false, &inFile, &EESaveFilePrimitive); }
    void UpdateKindFlags() { ee_units_group_do(this, 4, NULL); }
    void Clear() { ee_units_group_do(this, 2, NULL); }
    void RemoveUnit(EEComplexUnit* inUnit) { ee_units_group_do(this, 1, inUnit); }
    long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion) { return (long)ee_units_group_get(this, 7, 0); }
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
        { return ee_units_group_file(this, true, &outFile, &EESaveFilePrimitive); }
    void Validate() { ee_units_group_do(this, 3, NULL); }
    void MoveAlongFoundPaths() { ee_units_group_do(this, 5, NULL); }
    EEComplexUnit* GetFirstUnit() { return this->GetUnit(0); }   // NULL when empty
    void SetFormation(EEFormation* inFormation) { ee_units_group_set_formation(this, inFormation); }
    EEFormation* GetFormation() const { return static_cast<EEFormation*>(this->mFormation); }
    EEPlayer* GetPlayer() const { return static_cast<EEPlayer*>(this->mPlayer); }
    // the rectangle around the units on the map (GERectangle: bottom, left, right, top; untouched for none)
    void GetBounds(GERectangle& outBounds) { ee_units_group_bounds(this, reinterpret_cast<float(*)[4]>(&outBounds)); }
    long CountUnitsOfKind(long inKind) { return (long)ee_units_group_get(this, 0, inKind); }
    void Assign(EEGroup* inGroup) { ee_units_group_with(this, 3, inGroup); }   // a hot-key group takes a selection
    void CopyUnits(EEGroup* inGroup) { ee_units_group_with(this, 0, inGroup); }
    EEGroup& CopyGroup(EEGroup* inGroup) { ee_units_group_with(this, 1, inGroup); return *this; }
    EECPASGFlags GetFlags();                // the terrain flags every unit shares (EEGroup.cpp)
    EEComplexUnit* FindUnitOfCategory(long inCategory)
        { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_group_get(this, 3, inCategory))); }
    EEComplexUnit* FindUnitWithFlags(unsigned long inFlags)
        { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_group_get(this, 4, (int64_t)inFlags))); }
    void CancelPathRequest(long inPathID) { ee_units_group_cancel_path(this, inPathID); }
    bool HasUnitType(long inKey) { return ee_units_group_get(this, 1, inKey) != 0; }
    long GetUnitTypeCount(long inKey) { return (long)ee_units_group_get(this, 2, inKey); }
    // the path of the path request carrying unit id inUnitID (EEComplexUnit::RefreshPath), NULL for none
    EEAutoPathRequest* GetUnitPath(unsigned long inUnitID)
        { return reinterpret_cast<EEAutoPathRequest*>(static_cast<uintptr_t>(ee_units_group_get(this, 6, (int64_t)inUnitID))); }
};

// The player's current selection (RTTI EESelectedGroup > EEGroup): EEPlayer's Reset makes two
class EESelectedGroup : public EEGroup {
public:
    static EESelectedGroup* New(EEPlayer* inPlayer)
        { return static_cast<EESelectedGroup*>(static_cast<EEGroup*>(ee_units_group_new(1, inPlayer, 0))); }
    EESelectedGroup& CopySelection(EESelectedGroup* inGroup) { ee_units_group_with(this, 2, inGroup); return *this; }
    // a saved group's units, remembering the group when asked (EEMSavedGroupSelect: true)
    void SelectSavedGroup(EEGroup* inGroup, bool inRememberGroup) { ee_units_group_with(this, inRememberGroup ? 4 : 5, inGroup); }
    void AddIfAbsent(EEComplexUnit* inUnit, bool inNeedsSort) { ee_units_group_do(this, inNeedsSort ? 9 : 10, inUnit); }
    void AddUnit(EEComplexUnit* inUnit, bool inNeedsSort) { ee_units_group_do(this, inNeedsSort ? 11 : 12, inUnit); }
    void SortUnits() { ee_units_group_do(this, 6, NULL); }
    void ShowSelectionAreas() { ee_units_group_do(this, 7, NULL); }
    void RefreshSelectionAreas(EEComplexUnit* inUnit) { ee_units_group_do(this, 8, inUnit); }
    unsigned short GetContinent() { return (unsigned short)ee_units_group_get(this, 5, 0); }
    EEGroup* GetSavedGroup() const { return static_cast<EEGroup*>(this->mSavedGroup); }
};

class EECalamityClass;
class EETechTreeNode;

// A group the commands act on (RTTI EEActiveGroup > EEGroup): the unit-command messages make one per formation slot
// (1..5) from the selected group and push it into EEPlayer's mPendingGroups. Its plain record (the action, the 17
// speeds, the four flags) is mFields (ee_units ag_fields.rs); its bodies are ag.rs's and group_obj.rs's.
class EEActiveGroup : public EEGroup {
public:
    static uint64_t H(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }   // the inline members' handles
    static EEActiveGroup* New(EEPlayer* inPlayer)
        { return static_cast<EEActiveGroup*>(static_cast<EEGroup*>(ee_units_group_new(2, inPlayer, 0))); }
    // inUnits' clusters each request a path from their first unit's tile to the goal's; true when there were clusters
    bool RequestPaths(std::vector<EEComplexUnit*>& inUnits)
        { return ee_units_group_request_paths(this, reinterpret_cast<void* const*>(inUnits.data()), inUnits.size()); }
    void RequestAllPaths() { ee_units_group_do(this, 21, NULL); }
    void AddMember(EEComplexUnit* inUnit, bool inRemoveFromOld) { ee_units_group_do(this, inRemoveFromOld ? 13 : 14, inUnit); }
    void ComputeValues() { ee_units_ag_compute_values(H(this)); }
    // the selected group's units for one slot; inSort 0: SortUnits first, 1: sorted slowest first afterwards
    void Fill(EESelectedGroup* inGroup, long inSort, long inSlot) { ee_units_group_fill(this, inGroup, inSort, inSlot); }
    void Finish() { ee_units_group_do(this, 15, NULL); }   // the goal dropped, into the player's finished groups
    void KeepAttackers() { ee_units_ag_keep_attackers(H(this)); }  // 0x1eec0b (SpreadAroundUnit: keeps the units that can attack)
    bool IsReserved(const GE3DPoint& inLocation) { return ee_units_ag_reserved(H(this), (const float(*)[3])&inLocation.mX, 0) != 0; }  // 0x1efaf0: a mU3C location at (or near) inLocation
    bool IsCellTaken(const GE3DPoint& inLocation) { return ee_units_ag_reserved(H(this), (const float(*)[3])&inLocation.mX, 1) != 0; }  // 0x1efba8: a mU3C location in inLocation's unit region
    void SpreadForUnload(long inTileX, long inTileY) { ee_units_ag_spread_around_tile(H(this), (int32_t)inTileX, (int32_t)inTileY, 1); }  // 0x1ef5ce (EEMessage 0xc8615: as SpreadAroundTile, the transports stay)
    // 0x1ef86c (EEMessage 0xbc48f: a calamity's casters for inTarget; the others attack it)
    void SplitCalamityCasters(EECalamityClass* inCalamity, EETechTreeNode* inNode, EEUnit* inTarget) { ee_units_ag_split_calamity_casters(H(this), H(inCalamity), H(inNode), H(inTarget)); }
    // the goal and whether it is queued; the group moves together for a location goal asking for it
    void SetGoal(EEUnitGoal* inGoal, bool inFlag) { ee_units_group_set_goal(this, inGoal, inFlag); }
    void Plan() { ee_units_ag_plan(H(this)); }  // 0x1edcc0 (after SetGoal)
    void KeepNonCitizens() { ee_units_group_do(this, 17, NULL); }
    // a unit's stat, or the group's when it moves together (EEActiveGroup.cpp)
    float GetMaxVelocity(EEComplexUnit* inUnit);
    float GetAcceleration(EEComplexUnit* inUnit);
    float GetMaxAngularVelocity(EEComplexUnit* inUnit);
    float GetAngularAcceleration(EEComplexUnit* inUnit);
    float GetInverseMaxVelocity(EEComplexUnit* inUnit);
    float GetInverseAcceleration(EEComplexUnit* inUnit);
    float GetInverseAngularAcceleration(EEComplexUnit* inUnit);
    void KeepGroundAttackers() { ee_units_group_do(this, 18, NULL); }
    void ResetUnitsPaths() { ee_units_group_do(this, 20, NULL); }   // EEMPathTo
    void KeepCitizens() { ee_units_group_do(this, 19, NULL); }      // EEMBuildWall
    void SpreadAroundResource(EEUnit* inTarget) { ee_units_ag_spread_around_resource(H(this), H(inTarget)); }  // 0x1eefbe (EEMGatherResource)
    void SpreadAroundTile(long inTileX, long inTileY) { ee_units_ag_spread_around_tile(H(this), (int32_t)inTileX, (int32_t)inTileY, 0); }  // 0x1ef2d4 (EEMDropoffResource: the drop-off's tile)
    void SpreadAroundUnit(EEUnit* inTarget, long inTileX, long inTileY) { ee_units_ag_spread_around_unit(H(this), H(inTarget), (int32_t)inTileX, (int32_t)inTileY); }  // 0x1eecb2 (EEMAttackUnit: the target and its tile)
    void KeepMobileUnits() { ee_units_ag_keep_mobile(H(this)); }
    // the unit's reserved destination (the teleport calamity)
    void TellGroup(EEComplexUnit* inUnit, const GE3DPoint& inLocation) { ee_units_group_tell(this, inUnit, (const float(*)[3])&inLocation.mX); }
    void ClearGoal() { ee_units_group_do(this, 16, NULL); }
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
// port (R5b.4, round 29): the plain members mCosts .. mResearchedWithEpoch are the Rust record EeTechNodeDataFields
// (ee_tech nodes.rs; names, C++ types and order kept), filled from the file's record by ee_tech_fill_from_record:
// mCosts[7] (by resource), mCostResource, mRequiredStatistic, mDefaultUnitClass (past the end of mUnitClasses),
// mCalamity (an EECalamityEnum: the cast-calamity goal; unsigned), mKind (3 repeatable, 4 a wonder, 5 never
// researched), mID (an epoch: its position; else >= 1000), mResearchedEvent, mObsoleteEvent (EEEvent ids), mBuildTime,
// mPrerequisite1..4 (ids), mUnitClass (what it builds), mRequiredStatisticValue, mNameID, mHotKey, mMakesUnit,
// mCountsTowardEpoch, mShowInEditor, mObsoletedByDefault, mAvailableWithEpoch, mResearchedWithEpoch
struct RTSTechTreeNodeData : public EeTechNodeDataFields {
    std::map<long, long*> mClassCosts;      // +0x00 per unit class a new long[7] of costs (the node's operator= shares them)
    std::vector<unsigned long> mUnitClasses;    // +0x0c (Contains 0x1cf6e5; Load reads each with Read(unsigned long&))
    // inline: the cast-calamity goal loads it before the other pushes (0x220b72) (provisional name)
    unsigned long GetCalamity() const { return this->mCalamity; }
};

// RTTI RTSTechTreeNode (vtable 0x446174). Entirely inline: its ??_G 0x1cfed2 (user-declared empty dtor: the vptr
// store) and the two accessors 0x1cfeca/0x1cfece close EETechTreeNode.cpp, the only unit that uses its vtable (the
// EETechTreeNode dtor's last store).
// port (R5b.4, round 29): the plain members of RTSTechTreeNode, EETechTreeNode and EETechTreeEpoch are the Rust record
// EeTechNodeFields (ee_tech nodes.rs; names and C++ types kept): mIsResearched (+0x04), mIsObsolete (+0x05: the flag),
// mIsAvailable (+0x06), mPrerequisiteFlags (+0x20: 1 the epoch, 2/4/8/0x10 the four prerequisites), mIsAllowed (+0x21),
// mScenEditorObsoleted (+0x22), mIsUnitMediaLoaded (+0x23), mEpoch (+0x14), mObsoleteEpoch (+0x18, = 14),
// mResearchDelay (+0x1c: ResearchNode counts it down); an epoch's mIsCountingSOL (+0x50), mRequiredSOL (+0x40),
// mFirstNodeID (+0x44), mCurrentSOL (+0x48: the techs researched toward it), mNextEpoch (+0x4c), mTitleID. The
// bodies are Rust over EETechHost (ee/EETechRust.h)
class RTSTechTreeNode : public EeTechNodeFields {
public:
    // inline (EETechTreeNode's ctor 0x1cedce: these stores come first, its own init list next, the one vptr store last)
    RTSTechTreeNode() : mData(NULL) { ee_tech_node_fields_init(this); }
    virtual ~RTSTechTreeNode() {}           // +0x00 (??_G 0x1cfed2)
    virtual bool GetIsAvailable() { return this->mIsAvailable; }  // +0x04 0x1cfeca
    virtual bool GetIsObsolete() { return this->mIsObsolete; }  // +0x08 0x1cfece

    RTSTechTreeNodeData* mData;             // +0x08
};

class EETechTreeNode : public RTSTechTreeNode {
public:
    EETechTreeNode();
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
    void GetCostString(UWideString& outString, long inUnitClass, float inScale);
    long Load(FSFile& inFile, EETechTree* inTree);  // 0x1cf370 (one 0x10c-byte database record)
    bool Contains(long inUnitClass);        // 0x1cf6e5 (std::find in mU08->mUnitClasses)
    bool NeedsUnitMedia();
    bool NeedsUnitMediaUncaptured();        // (TNWK, TEST ONLY: NeedsUnitMedia records its answer)
    bool DependsOn(long inID);              // 0x1cf79a (recursive over the prerequisites)
    void SetPrerequisite(long inID);
    void MakeAvailable();
    void SetScenEditorObsoleted(bool inValue, bool inRecurse);
    void SetObsolete(bool inValue, bool inKeepMedia);
    void SetButton(unsigned long inButton);
    // GetCost's body (the virtual adds the simulation capture's record: ee/EETechCapture.h)
    long GetCostUncaptured(long inResource, long inUnitClass, float inScale);
    // 0x268dc6 (in EEUIFMGMainGroup.cpp after its first user ShowCommands, called out of line: defined there): the hot key
    // mU08->mHotKey, 0 out of range (provisional name)
    UIHotKey* GetHotKey();
    EETechTreeNode& operator=(const EETechTreeNode& inOther);
    // 0x2a995 (a COMDAT in the AI range: inline there?): mU08->mUnitClasses[inIndex], mU08->mU40 past the end
    long GetUnitClass(unsigned long inIndex);
    // 0x8a42e (a COMDAT first emitted by EECPResearchManager.cpp, every call out of line): mU08->mUnitClasses.size();
    // (provisional name)
    unsigned long GetNumUnitClasses() { return this->mData->mUnitClasses.size(); }
    bool GetScenEditorObsoleted() const { return this->mScenEditorObsoleted; }

    EETechTree* mTree;                      // +0x0c (GetPlayer 0x1ccb1f)
    EEButtonObject* mButton;                // +0x10 (EEButtonObject::Get of the record's +0xb0)
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
    EETechTreeEpoch();
    virtual ~EETechTreeEpoch();             // +0x00 0x1cdfcf (??_G 0x1cdfb3)
    virtual bool GetIsAvailable();          // +0x04 0x1ce474
    virtual bool GetIsObsolete();           // +0x08 0x1ce494
    virtual long GetCost(long inResource, long inUnitClass, float inScale);     // +0x0c 0x1ce038
    virtual long GetBuildTime();            // +0x10 0x1ce0eb
    virtual void LoadMedia(EEPlayer* inPlayer);   // +0x14 0x1ce5b5
    virtual long ResearchNode(EEComplexUnit* inUnit);  // +0x18 0x1ce862
    virtual void UnloadMedia(EEPlayer* inPlayer);  // +0x1c 0x1cea53

    void DeleteNodes();
    long GetCurrentSOL();
    long GetRequiredSOL() const { return this->mRequiredSOL; }  // inline (AttemptEpochAdvance 0x6f406 loads it before the GetCurrentSOL call) (provisional name)
    long Load(FSFile& inFile, EETechTree* inTree);  // 0x1ce10e (the epoch's record, then its nodes)
    long Unload();
    void GetNodeIDs(std::vector<long>& outIDs);
    void AddSOL();
    void LoadMediaOfKind(long inKind);
    void UpdateAvailability();
    void ResearchKind(long inKind, bool inEpochOnly);
    long Research(long inID, EEComplexUnit* inUnit);
    long GetCostUncaptured(long inResource, long inUnitClass, float inScale);   // GetCost's body (ee/EETechCapture.h)
    void UnloadMediaOfKind(long inKind);
    // 0x1ceaeb (EETechTree::Get's lookup in one epoch; provisional name): the epoch for its own id, else one of its
    // nodes, else NULL (inline: EETechTree::Get's every caller makes it)
    EETechTreeNode* Get(long inID)
    {
        if (inID == this->mData->mID)
            return this;
        std::map<long, EETechTreeNode*>::iterator theNode = this->mNodes.find(inID);
        return theNode != this->mNodes.end() ? theNode->second : NULL;
    }
    EETechTreeEpoch& operator=(const EETechTreeEpoch& inOther);

    std::map<long, EETechTreeNode*> mNodes; // +0x24 by id (0x18-byte nodes: _Init 0x617b6)
    std::vector<long> mLoadedUnitClasses;   // +0x30 unit classes whose media Virtual14 loaded (_Buy 0xe7d2e)
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

// port (R5b.4, round 29): the plain members are the Rust record EeTechTreeFields (ee_tech nodes.rs): mCurrentEpoch
// (+0x538; EEMusicManager 0x132022 passes it to EEMusicObject's pick), mStartEpoch (+0x53c), mEndEpoch (+0x540, = 14),
// mEpochsBeforeWonders (+0x544, = 1), mMediaID (+0x548), mEpochNameID (+0x54c: 30145 + the epoch, a string id),
// mIsLoaded (+0x550); the saved game's research lists (+0x4f4 .. +0x524) are Rust's (mSaved). The bodies are Rust
// over EETechHost (ee/EETechRust.h)
class EETechTree : public RTSTechTree, public EeTechTreeFields {
public:
    EETechTree();
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
    long Load(FSFileSpec& inFile);          // 0x1cbdf0 (the database: every epoch block of the file)
    static void BuildUnitClassInfo();       // 0x1cbe4a (gTechTree's events by unit class)
    void UnloadAll();
    void GetNodeIDs(std::vector<long>& outIDs);
    void GetEpochNodeIDs(long inEpoch, std::vector<long>& outIDs);
    void GetChecksum(ESChecksum& ioChecksum);
    void SumCosts(long inID, long inUnitClass, long* ioCosts);
    static bool IsLatestEffect(EETechTree* inTree, long inID, long inUnitClass, long inKind);
    EEPlayer* GetPlayer();
    void AdvanceEpoch();
    void ApplySavedState(bool inApplyObsoleted, bool inApplyResearch);
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x1ccbc9 (saved game)
    void MakeObsolete(long inID);
    void SetPrerequisite(long inID, long inPrerequisite);
    void Research(long inID, EEComplexUnit* inUnit);   // 0x1ccec2 (inUnit: the researching unit, passed on to EEEvent::Trigger)
    void SumCostsUncaptured(long inID, long inUnitClass, long* ioCosts);   // SumCosts' body (ee/EETechCapture.h)
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    void SetEpoch(long inEpoch);
    void SetEpochRange(long inStart, long inEnd);
    void ResetEpochRange(long inStart, long inEnd);
    void SetNodeAvailable(long inID, bool inValue);
    void SetPlayer(EEPlayer* inPlayer);
    void UpdateInterface(bool inEpochChanged);
    EETechTree& operator=(const EETechTree& inOther);
    // 0x267eaa: the epoch the statistics group shows (inline in the original's EEUIFMGGStatistics.h, the COMDAT after
    // its Select; defined in EEUIFMGMainGroup.cpp with the panel groups' host; provisional name)
    long GetShownEpoch() const;

    // Layout (from the ctor 0x1cbcc2; provisional names)
    EETechTreeEpochList mEpochs;            // +0x008 one per epoch (the original: 15, the ids below 15)
    EEPlayer* mPlayer;                      // +0x534 (GetPlayer: else gWorld.mPlayers[mU04])
    long GetCurrentEpoch() const { return this->mCurrentEpoch; }
};
typedef EETechTree EETechnologyTable;
extern EETechTree gTechTree;                // 0x52fd98 (Empire Earth.cpp: the database's tree)

// One point of a player's statistics series (8 bytes on Windows; the layout of ee_units' EeUnitsStatPoint, which
// GetSeries copies into)
struct EEStatisticsPoint {
    EEStatisticsPoint() : mValue(0), mTime(0) {}
    unsigned long mValue;                   // +0x00 (Set's value; the series keeps the maximum)
    unsigned long mTime;                    // +0x04 (the game time: the post-game graph's x)
};

// One statistics series of a player, a copy GetSeries makes (the series themselves are Rust's)
struct EEStatisticsSeries {
    EEStatisticsSeries() : mPoints(1), mMaxValue(0), mKeepHistory(false) {}
    std::vector<EEStatisticsPoint> mPoints;  // the points
    unsigned long mMaxValue;                // the largest value pushed (GetValue)
    bool mKeepHistory;                      // false: Set replaces the last point
};

// What the player holds at +0x4a8: RTTI EEPlayerStatistics (vtable 0x442080). Was EEPlayerSavedGroups (the old
// spelling stays as a typedef).
// port (the player lane, round 1): the 31 series are a Rust object (mStats: ee_units player_stats.rs) with every
// body (Set's point, Reset/Lower, the readers, the copy, CompactEpochHistory, Load/Save); the name and the end time
// stay C++ members
class EEPlayerStatistics {
public:
    EEPlayerStatistics() : mStats(ee_units_stats_new()), mEndTime(0) {}
    EEPlayerStatistics(const EEPlayerStatistics& inOther) : mName(inOther.mName), mStats(ee_units_stats_new()), mEndTime(inOther.mEndTime)
    {
        ee_units_stats_copy(this->mStats, inOther.mStats);
    }
    virtual ~EEPlayerStatistics() { ee_units_stats_free(this->mStats); }
    static const UString sKey;              // "EEPlayerStatistics-"
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    // the series raised (the hot-key group events: (0x14, true)) or lowered by an amount, or set to a value
    void Reset(long inSeries, long inAmount) { this->Record(1, inSeries, inAmount); }
    void Lower(long inSeries, long inAmount) { this->Record(2, inSeries, inAmount); }
    void Set(long inIndex, long inValue) { this->Record(0, inIndex, inValue); }
    // the series' largest value, last value (0 without one), the points whose next one is larger
    unsigned long GetValue(unsigned long inSeries) { return ee_units_stats_value(this->mStats, inSeries, 0); }
    unsigned long GetLastValue(unsigned long inSeries) { return ee_units_stats_value(this->mStats, inSeries, 1); }
    unsigned long CountIncreases(unsigned long inSeries) { return ee_units_stats_value(this->mStats, inSeries, 2); }
    void Clear() { this->mName = UWideString(); this->mEndTime = 0; ee_units_stats_clear(this->mStats); }
    EEPlayerStatistics& operator=(const EEPlayerStatistics& inStatistics)
    {
        this->mName = inStatistics.mName;
        this->mEndTime = inStatistics.mEndTime;
        ee_units_stats_copy(this->mStats, inStatistics.mStats);
        return *this;
    }
    void CompactEpochHistory() { ee_units_stats_compact(this->mStats); }   // EEWorld::RecordStatistics
    void SetName(UWideString inName) { this->mName = inName; }
    EEStatisticsSeries GetSeries(unsigned long inSeries);   // a copy (the post-game summary, the scores)
    void Record(uint32_t inOp, long inSeries, long inValue);   // Set's gate and the game time, then Rust's op

    UWideString mName;                      // (saved)
    EeUnitsPlayerStats* mStats;             // the 31 series (Rust)
    unsigned long mEndTime;                 // (saved)
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
// port (R5b.4, round 29): the plain members mNextUniqueID .. mObserverMode are the Rust record EeUnitsRtsPlayerFields
// (ee_units rts_player_save.rs; names, types and order kept), Load and Save Rust over it
class RTSPlayer : public EeUnitsRtsPlayerFields {
public:
    RTSPlayer();
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
    // the record (EeUnitsRtsPlayerFields): mNextUniqueID (+0x458, saved), mPlayerID (+0x45c: the index into another
    // player's mEnemies; saved), mUnitCount (+0x460, saved), mSlot (+0x464, = 0xff; EEWorld::GetPlayerBySlot compares
    // it), mSeesAll (+0x465: toggled by EEMMakeWorldVisible), mSaveVisMap (+0x466: saved; checked when a unit is
    // removed), mObserverMode (+0x467: chat everybody / observer)
    bool GetSeesAll() const { return this->mSeesAll; }
};

// port (R5b.4): the saved numbers (mScenarioVariables, mEnemies, mTributes, mResourceCaps, mPopLimit, ... the 24
// fields EEPlayer::Load and Save read and write as plain values) are the Rust record EeUnitsPlayerSaved
// (ee_units player_save.rs) this class derives from, with their C++ names and types; the offsets below omit them.
// port (the player lane, round 1): the other plain members (mHackUnitClass .. mJITManufacturing, mNumTeleporters) are
// the Rust record EeUnitsPlayerFields (ee_units player.rs; their names and types kept), with the queued improvements
// (mQueued, Rust's); the economy's and the improvements' bodies are Rust over EEPlayer.cpp's host
class EEPlayer : public RTSPlayer, public EeUnitsPlayerSaved, public EeUnitsPlayerFields {
public:
    // out of line in EEPlayer.cpp (EEHumanPlayer's ctors call them; its ??_G calls the dtor)
    EEPlayer();
    EEPlayer(const UWideString& inName, unsigned long inPlayerID);   // 0x161da5 (RTSPlayer(inName, inPlayerID) 0x16f12e first)
    virtual ~EEPlayer();                    // +0x00 0x162297
    // the FSRestorable-style slots EEPlayer overrides (EEHumanPlayer's wrappers call them)
    virtual void Clear();                   // +0x1c 0x16828c (same RTSPlayer slot)
    virtual long LoadUnit(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x30 0x167ca4
    virtual void RemovalsDone();            // +0x34 0x16854e (same RTSPlayer slot)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x18 0x165cfa
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // +0x20 0x1690ca
    bool AdoptTownPatrol(EETown* inTown);   // port: a loaded town takes back its saved patrol group (0.254 saves)
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
public:
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
    virtual void Virtual60();               // +0x60
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
    void LoadUnitClassMedia(long inUnitClass);
    void GetHerdsOfType(std::vector<EEHerd*>& outHerds, long inUnitType);  // 0x164e78 (the herds of inUnitType's animal; EESMObject 0x1c8e37; provisional name)
    void ApplyMapRestrictions();            // 0x16c249 (the space/sea-map techs off, or the unit-selection hot keys for them; 0x13a3f9; provisional name)
    void DisableCamouflagePower();          // 0x16c5f9 (civ power 0x31: each complex unit in its unit set loses mIsCloakable/mIsCloaked, mCamouflageTimer/mU32C; VirtualXX 0x16b987; provisional name)
    void CheckButtonsPreloaded(long inUnitClass);  // 0x16c6fa (EEUnitClass::CreateUnitAt before a new unit; provisional name)
    void SetUnitClassMedia(long inUnitClass, long inMediaFlags);
    void UnloadUnitClassMedia(long inUnitClass);
    // logic bomb (provisional names): Stop passes the target's complex unit and false, Update the unit
    void AddWonder(EEComplexUnit* inUnit, bool inIsLoading);
    void RemoveWonder(EEComplexUnit* inUnit);
    void DeselectUnit(EEUnit* inUnit);                      // 0x168eb9 (the hurricane's Stop, its unit)
    // 0x164e22: the last town of the vector at +0x7cc whose area holds inUnit's tile, or 0 (the prophet analyzers'
    // 0xaa8b; provisional name)
    unsigned long GetNumTowns() const { return this->mTowns.size(); }   // inline COMDAT 0x4098f0 (emitted after its first out-of-line user, EECPASGUProphet.cpp 0x409831); /Os post-check undoes it where size() expands inside, it stays where size() is refused (provisional name)
    void LoadUnitClassButtons(long inUnitClass);            // 0x167c5a (the hurricane's Virtual28)
    void UnloadUnitClassButtons(long inUnitClass);          // 0x16b5f9 (the hurricane's Virtual34)
    void ApplyImprovement(long inUnitType, long inImprovement, bool inFlag);  // 0x16b65a (ret 0xc; arg 3 read as a byte)
    // 0x1655f6: inUnit's entry in the player's +0x7c0 vector, 0 if none (the teleport sets its +0x14 to 4)
    // (provisional name)
    EEWUnitsOffMapInfo* GetOffMapInfo(EEComplexUnit* inUnit);  // (the lists' record of inUnit, 0 none)
    void SelectUnits(std::vector<long>& inUnitIDs);   // 0x16a658 (EEMSelectUnits::Execute; provisional name)
    // selection by the EEM* messages (EEMessage.cpp Executes; provisional names)
    void AddUnitToSelectedGroup(unsigned long inUnitID, bool inFlag);
    void AddUnitsToSelectedGroup(std::vector<long>& inUnitIDs);
    void IncrementEpic();
    void RemoveUnitFromSelectedGroup(long inUnitID);
    void SelectUnit(unsigned long inUnitID, bool inArg1, bool inNewGroup);
    void UnselectAllUnits();
    void ReloadTechTree(bool inFlag);       // 0x1697ff (EESELoadThread Handle403; provisional name)
    void LoadAllClassMedia();               // 0x167bff (Handle406, before its loop; provisional name)
    void UnloadAllClassMedia();             // 0x16b5b6 (Handle407, before its loop: per +0x8d8 entry, then clear; provisional name)
    void ClearSavedGroup(long inGroup);     // 0x163f95 (EEMSavedGroupClear)
    void AddAndRemoveUnitsFromSelectedGroup(std::vector<long>& inUnitIDs);
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
    bool CanImprove(long inUnitType, long inImprovement, UWideString* outReason);
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
    long GetResearchedImprovement(unsigned long inUnitType) { return (long)ee_units_queued_op(this->mQueued, 2, inUnitType, 0); }
    float GetImprovementProgress(unsigned long inUnitType) { return ee_units_queued_timer(this->mQueued, inUnitType); }
    void RestartTowns();                    // 0x1680e4 (each town's UpdateNearWater; EEWorld::StartGame)
    void RegisterTowns();                   // 0x16513d (the towns re-registered with gContinentManager)
    void RegisterHerds();                   // 0x1650dc (player 0 before a scenario starts; provisional name)
    bool AddUnitToHerd(EEComplexUnit* inUnit);  // 0x162e22 (AddUnit result returned in AL)
    void AssignHerds();                     // 0x164453 (player 0, after RegisterHerds; provisional name)
    void AssignTowns();                     // 0x16451a (the other players, after RegisterTowns; provisional name)
    // template COMDAT 0xa5313 (EEHerd.cpp's range; ret 8): the herds of mHerds (+0x7dc, EEHerd* elements) inPredicate
    // holds for, the last first (provisional name)
    template <class P, class V> void GetHerds(P inPredicate, V& outHerds)
    {
        for (long i = this->mHerds.size() - 1; i >= 0; i--)
            if (inPredicate(this->mHerds[i]))
                outHerds.push_back(this->mHerds[i]);
    }
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
    void RemoveSharedSightAt(const EETileLocation& inLocation);
    void ExecuteSelectionMessage(NEMessage* inMessage);  // 0x1646fc (EEServer::PrepareLocalInput: a fast-selection message; provisional name)
    void AddSharedSightAt(const EETileLocation& inLocation);
    // a tile's player references (the doppelganger manager follows them; provisional names)
    void RemoveSharedSight(RTSTile* inTile);  // 0x164287 (also for the allies that share the line of sight)
    void RemoveOwnSight(RTSTile* inTile);
    void RemoveOwnSightAt(const EETileLocation& inLocation);
    void AddSightSharer(EEPlayer* inPlayer);  // 0x1627b9 (pushes inPlayer into mSightSharers between two walks of the units)
    void RemoveSightSharer(EEPlayer* inPlayer);  // 0x1684ab (the mirror: inPlayer out of mSightSharers; RemoveWonderPower, 0x16ab41)
    void AddUnitToRepatrol(EEComplexUnit* inUnit);  // 0x162b35 (one of its own units into mUnitsToRepatrol, referenced)
    void ClearUnitsToRepatrol();            // 0x163fbb (mUnitsToRepatrol dereferenced and cleared)
    void AddOffMapUnit(RTSUnit* inUnit);    // 0x162dab (a new EEWUnitsOffMapInfo for inUnit into mUnitsOffMap)
    void AddStrandedUnit(EEUnit* inUnit);   // 0x163043 (into mStrandedUnits once, referenced)
    bool TrySelectUnit(EEUnit* inUnit, bool inFlag);  // 0x1630ac (selects inUnit; false when another player's unit meets a selection)
    void AddConstructionSite(EEComplexUnit* inUnit);  // 0x16368f (into mConstructionSites unless it has it)
    EEComplexUnit* GetNextComplexUnit(EEUnit* inUnit);  // 0x167ede (the next unit with a complex unit after inUnit, from the first when 0)
    void AddSharedSight(EETile* inTile, long inX, long inY);  // 0x168120 (AddOwnSight, then the allies sharing the line of sight)
    void AddOwnSight(EETile* inTile, long inX, long inY);  // 0x16820e (counts this player in at the tile)
    void AddOwnSightAt(const EETileLocation& inLocation);
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
    EEGroup* GetActiveGroupOf(EEComplexUnit* inUnit) { return static_cast<EEGroup*>(ee_units_group_active_of(this, inUnit)); }  // 0x164b4f (the group of mRunningGroups, else mPendingGroups, holding inUnit)
    long GetCitizenPassability();           // 0x1650a3 (flags 1/2/4/8 from unit class 0x20's upgrade +0x160)
    EEAIArea* GetNearestArea(const EETileCoordinate& inTile, bool inSameContinent);  // 0x164781 (ret 8; EECPResourceManager::FindResourceTarget takes its slot +0x18) (provisional name)
    long GetTotalPopulation(bool inFirst, bool inSecond);  // 0x1651e0 (mPopulationCount plus the units' AddedToParent: mPendingUnits, then those in a 0x1d container)
    bool CanAffordScaled(EETechTreeNode* inNode, long inUnitClass, float inScale, long inCount);  // 0x16542e (ret 0x10; arg 4 an `imul` count)
    long GetImprovementLevel(unsigned long inUnitType, long inImprovement) const;  // 0x164ed7 (an improvement's level; Virtual40; const: as CountQueuedImprovements)
    // 0x164fac: whether the improvement is at its maximum level (the class's mImprovements->mMaxLevel against
    // GetImprovementLevel; EEUIBUpgrade; provisional name)
    bool IsImprovementMaxed(unsigned long inUnitType, long inImprovement);
    void KillStrandedUnits();               // 0x165c8c (mStrandedUnits's units back into the world)
    void AddLoadedMedia(EEMedia* inMedia);  // 0x162740 (pushes into mLoadedMedia unless 0; provisional name)
    void DelayWonderTimers(long inAmount);  // 0x162b08 (adds to every unit's mCloaked in mWonders; provisional name)
    bool RemoveLoadedMedia(EEMedia* inMedia);  // 0x16843f (UnloadPlayerEffects unloads the media when true; provisional name)
    void LoadPlayerEffects();
    void UnloadPlayerEffects();
    long GetNumHeroes(bool inFirst, bool inSecond);   // 0x164223 (mNumHeroes and/or mNumHeroesInProduction; provisional name)
    long GetWonderTime(long inIndex) const;  // 0x165644 (mWonders's unit inIndex's mCloaked; provisional name)
    unsigned long GetNumWonderUnits() const;   // 0x16565f: mWonders->GetSize() (provisional name)
    // per unit class (mUnitClassUpgrades[inUnitClass] when inUnitClass < EEUnitClassCount(); provisional names)
    EEUnitClassUpgrade* GetUpgrade(long inUnitClass) { return this->mUnitClassUpgrades[inUnitClass]; }  // inline COMDAT 0x1bcae9
    void SwapClassButtonMedia(unsigned long inUnitClass, EEButtonObject* inFirst, EEButtonObject* inSecond);
    void SetClassAcknowledgeSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex);
    void SetClassCreationSound(unsigned long inUnitClass, EESoundObject* inSound);
    void SetClassDeathSound(unsigned long inUnitClass, EESoundObject* inSound);
    void SetClassModel(unsigned long inUnitClass, EEGraphicObject* inModel);  // 0x163e29 (EEModel)
    void SetClassButton(unsigned long inUnitClass, EEButtonObject* inButton);  // 0x163e4d (the unit class's button; EEEvent SetEventButton)
    void SetClassSelectionSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex);

    // Layout (no gaps; sizeof 0xb80: EEHumanPlayer adds a UWideString at +0xb80, sizeof 0xb94):
    // the ctor 0x161da5 (member ctors), its Reset 0x165813 (the array loops: 20, 4, 17, 10, 10, 17, 17 elements) and
    // Load/Save. Container element types and every unnamed member are provisional.
    EECivilization mCivilization;           // +0x468 (ctor 0x12c57d; provisional name)
    EEPlayerStatistics mSavedGroups;        // +0x4a8 (the player's statistics; provisional name)
    UString mCivilizationFile;              // +0x7ac (saved)
    std::vector<EETown*> mTowns;            // +0x7cc (the player's towns: EECPILocation::Update 0x80a57 walks them)
    std::vector<EEHerd*> mHerds;            // +0x7dc
    // port (the game core lane, round 5): the private lists (the stranded units, the units to repatrol, the loaded
    // and saved ids, the media-loaded classes, the loaded media, the off-map records) are a Rust object
    // (ee_units player_lists.rs; made by the constructors, freed by the destructor). The towns by id (written,
    // never read) and the third selected group (made, never used) are gone
    EeUnitsPlayerLists* mLists;
    std::vector<UString> mMediaWarnings;    // +0x824 (the ctor's _Buy 0x88ba6 / the dtor's _Tidy 0x665e4 are vector<UString>'s)
    std::vector<EEPlayer*> mSightSharers;   // +0x834 (0x1627b9 push_backs `this`: vector<EEPlayer*>'s push_back 0x44e8f)
    // port (the groups lane, round 2): the running (+0x844: AddPendingUnits copies the pending ones into it), pending
    // (+0x854: the command messages push their new EEActiveGroups) and finished groups (+0x864: EEActiveGroup::Finish
    // pushes itself) are a Rust object (ee_units group_obj.rs EeUnitsPlayerGroups; made by the constructors, freed by
    // the destructor); the C++ reads them through these
    EeUnitsPlayerGroups* mGroupLists;
    unsigned long GetNumRunningGroups() { return ee_units_player_groups_len(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_RUNNING); }
    EEGroup* GetRunningGroup(unsigned long inIndex) { return static_cast<EEGroup*>(ee_units_player_groups_at(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_RUNNING, inIndex)); }
    unsigned long GetNumPendingGroups() { return ee_units_player_groups_len(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_PENDING); }
    void AddPendingGroup(EEGroup* inGroup) { ee_units_player_groups_push(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_PENDING, inGroup); }
    std::vector<EEPPatrolGroup*> mPatrolGroups;  // +0x874 (AddPatrolGroup: push_back 0x16c986)
    std::vector<EEUnitClassUpgrade*> mUnitClassUpgrades; // +0x884 (resized to EEUnitClassCount() by Reset: one per unit class; the forwarders
                                            // 0x163db9.. index it
    std::vector<EEPlayer*> mAllies;         // +0x894 (EEPPlayerHelper push_backs into it; EEMSendFlare copies it with
                                            // vector<EEPlayer*>::operator= 0x1119ec and flares each)
    UReferenceContainer<EEComplexUnit*>* mConstructionSites;  // +0x8b4 (0x16368f: Contains 0x27c820, then its +4 vector)
    UReferenceContainer<EEComplexUnit*>* mWonders;       // +0x8f8 (Reset news one: vtable 0x4429d8)
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
    EEUnitSet* mHealedUnitSet;              // +0x9b8 (ComputeLoadDistance: Contains(unit type) heals those units)
    struct EeUnitsAtc* mAirTraffic[4];   // +0x9bc (Reset: each EEATCNew(), Rust's state: ee/EEAirTrafficControl.h); indexed by an airport class's mAirTrafficType - 1 (EEMClearAirportCommands 0xbd588)
    // inline: the garrison goal's ComputeUpgrade (0x226c08) loads its argument before `this` (provisional name)
    EETechTree* GetTechTree() const { return this->mTechnologies; }
    EETechnologyTable* mTechnologies;       // +0x9cc (provisional name; conscription 0xafe3b; Reset: new 0x554 bytes)
    RTSUnit* mViewedUnit;                   // +0x9d0 (Reset dereferences it)
    RTSUnit* mSelectedViewedUnit;           // +0x9d4 (a referenced unit: SetSelectedViewedUnit swaps it)
    long GetMiningLimit() const { return this->mMiningLimit; }  // inline (EEUGBuildRepair::GatherNearBuilding)
};


// EEWorld::ApplyShareBonus (false) and RemoveShareBonus (true): the share bonus of every player by its wonder powers and
// stances (ee_units player_units.rs; EEPlayer.cpp). The player lane, round 2: EEPlayer's mWonderTypeCounts and
// mWonderPowers are its Rust wonder state (mWonderState)
void EEPlayerShareBonus(bool inRemove);
#endif
