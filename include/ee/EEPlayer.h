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
#include "rust/sim/records.h"
#include "rust/sim/units.h"             // EeUnitsPlayerSaved (EEPlayer's saved numbers)
#include "rust/game/rts.h"              // RTSPlayer's bodies (ee_game rts units.rs)
#include "rust/game/tech_tree.h"        // the tech tree classes' bodies (ee_game tech_tree)
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
// the Rust player's view of a player (ee_units player.rs; EEPlayer.cpp), and its unit bookkeeping and wonders
// (ee_units player_units.rs ee_units_player_units)
struct EEPlayerRefs : EeUnitsPlayerRefs {
    EEPlayerRefs(const EEPlayer* inPlayer);
    operator const EeUnitsPlayerRefs*() const { return this; }
};
long EEPlayerUnits(const EEPlayer* inPlayer, uint32_t inWhat, void* inUnit = NULL, bool inFlag = false, bool inFlag2 = false);

class EEComplexUnit;
class EEAIArea;                             // ee/EEAIArea.h (GetNearestArea)
class EEFormation;
class GERectangle;
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
typedef U2DPoint<long> EETileLocation;   // ee/EETileLocation.h
class RTSTile;
class EETown;                               // (ee/EETown.h)
class EETile;
class EEUnitClass;
class EEUnitClassUpgrade;                   // (ee/EEUnit.h; was `struct EEUnitStats`)
struct EEPUnitSwap;                         // (ee/EEPhysics.h)
typedef EeUnitsOffMapInfo EEWUnitsOffMapInfo;   // (ee/EEWUnitsOffMapInfo.h)
class EEMedia;
class EEGraphicObject;
class EEHerd;                               // ee/EEHerd.h (mHerds's elements)
class EEUnitSet;                            // ee/EEUnitSetManager.h (mHealedUnitSet)
class EEButtonObject;
class EEMap;
class UIHotKey;

// A player's event list (RTTI EEPPlayerEventList, vtable 0x442b54): the alerts at a map location (1 under attack, 2 a
// garrison's message, 3 a flare).
// port (the player lane, round 1): the events (EEPPlayerEvent's members) and every decision on them are a Rust object
// (mList: ee_units player_events.rs); the C++ keeps the alert sounds and answers the world's parts (Host)
class EEPPlayerEventList {
public:
    // the alert sounds got and loaded (player_events_split.rs); unloaded, the one playing released
    EEPPlayerEventList() : mList(ee_units_events_new()), mSound(NULL), mPlayed(NULL) { ee_units_events_sounds(this, &EEPPlayerEventList::Host, true); }
    virtual ~EEPPlayerEventList() { ee_units_events_destroy(this->mList, this, &EEPPlayerEventList::Host); }
    // an event at inTile (EEMSendFlare: (tile, 3, the sender, -1, true)); AddPlayerEvent's with inFrom, the location
    // whose sight an attack shares (EEUCBasicPriest::UpdateConversion: the conversion, type 1)
    void AddEvent(const U2DPoint<long>& inTile, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew);
    void AddPlayerEvent(const U2DPoint<long>& inTile, const U2DPoint<long>& inFrom, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew);
    bool GetNextEventSource(U2DPoint<long>& outSource);   // the next event's source (EEAPlayerEvent); false without one
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
        { return ee_units_events_file(this->mList, &inFile, &EESaveFilePrimitive, false); }
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
        { return ee_units_events_file(this->mList, &outFile, &EESaveFilePrimitive, true); }
    long RepairEvents(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion)
    {
        this->Call(inPlayer, EE_UNITS_EVENTS_REPAIR, 0.0f);
        return 0;
    }
    void TickEvents(EEPlayer* inPlayer, float inElapsed) { this->Call(inPlayer, EE_UNITS_EVENTS_TICK, inElapsed); }
    void ShowEvents(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_SHOW_ALL, 0.0f); }   // around a sight change
    void HideEvents(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_HIDE_ALL, 0.0f); }
    void Clear(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_CLEAR, 0.0f); }   // (NULL: no world call)
    void Call(EEPlayer* inPlayer, uint32_t inWhat, float inElapsed) { ee_units_events_call(this->mList, this, inPlayer, &EEPPlayerEventList::Host, inWhat, inElapsed); }
    static int32_t Host(void* inList, void* inPlayer, int32_t inOp, const EeUnitsPlayerEvent* inEvent);

    EeUnitsPlayerEvents* mList;             // the events (Rust)
    SESMInstanceNode* mSound;               // the alert playing (referenced)
    SESMInstanceNode* mPlayed;              // the alert just played (EE_UNITS_EVENTS_X_PLAY: player_events_split.rs)
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
    void Add(EEComplexUnit* inUnit) { ee_units_group_do(this, 0, inUnit); }
    bool Contains(EEComplexUnit* inUnit) { return ee_units_group_contains(this, inUnit); }
    EEComplexUnit* GetUnit(long inIndex) { return static_cast<EEComplexUnit*>(ee_units_group_unit(this, inIndex)); }
    unsigned long GetNumUnits() { return ee_units_group_len(this); }
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
        { return ee_units_group_file(this, false, &inFile, &EESaveFilePrimitive); }
    void Clear() { ee_units_group_do(this, 2, NULL); }
    void RemoveUnit(EEComplexUnit* inUnit) { ee_units_group_do(this, 1, inUnit); }
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
        { return ee_units_group_file(this, true, &outFile, &EESaveFilePrimitive); }
    EEComplexUnit* GetFirstUnit() { return this->GetUnit(0); }   // NULL when empty
    EEFormation* GetFormation() const { return static_cast<EEFormation*>(this->mFormation); }
    EEPlayer* GetPlayer() const { return static_cast<EEPlayer*>(this->mPlayer); }
    // the rectangle around the units on the map (GERectangle: bottom, left, right, top; untouched for none)
    void GetBounds(GERectangle& outBounds) { ee_units_group_bounds(this, reinterpret_cast<float(*)[4]>(&outBounds)); }
    void Assign(EEGroup* inGroup) { ee_units_group_with(this, 3, inGroup); }   // a hot-key group takes a selection
    EECPASGFlags GetFlags();                // the terrain flags every unit shares (EEGroup.cpp)
    EEComplexUnit* FindUnitOfCategory(long inCategory)
        { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_group_get(this, 3, inCategory))); }
    EEComplexUnit* FindUnitWithFlags(unsigned long inFlags)
        { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_group_get(this, 4, (int64_t)inFlags))); }
    bool HasUnitType(long inKey) { return ee_units_group_get(this, 1, inKey) != 0; }
    // (GetUnitPath: Rust's, RefreshPath in waypoints.rs)
};

// The player's current selection (RTTI EESelectedGroup > EEGroup): EEPlayer's Reset makes two
class EESelectedGroup : public EEGroup {
public:
    EESelectedGroup& CopySelection(EESelectedGroup* inGroup) { ee_units_group_with(this, 2, inGroup); return *this; }
    void AddUnit(EEComplexUnit* inUnit, bool inNeedsSort) { ee_units_group_do(this, inNeedsSort ? 11 : 12, inUnit); }
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
    // the selected group's units for one slot; inSort 0: SortUnits first, 1: sorted slowest first afterwards
    void Finish() { ee_units_group_do(this, 15, NULL); }   // the goal dropped, into the player's finished groups
    bool IsReserved(const GE3DPoint& inLocation) { return ee_units_ag_reserved(H(this), (const float(*)[3])&inLocation.mX, 0) != 0; }  // 0x1efaf0: a mU3C location at (or near) inLocation
    // 0x1ef86c (EEMessage 0xbc48f: a calamity's casters for inTarget; the others attack it)
    void SplitCalamityCasters(EECalamityClass* inCalamity, EETechTreeNode* inNode, EEUnit* inTarget) { ee_units_ag_split_calamity_casters(H(this), H(inCalamity), H(inNode), H(inTarget)); }
    // the goal and whether it is queued; the group moves together for a location goal asking for it
    void SetGoal(EEUnitGoal* inGoal, bool inFlag) { ee_units_group_set_goal(this, inGoal, inFlag); }
    void Plan() { ee_units_ag_plan(H(this)); }  // 0x1edcc0 (after SetGoal)
    // a unit's stat, or the group's when it moves together (EEActiveGroup.cpp)
    float GetMaxVelocity(EEComplexUnit* inUnit);
    float GetInverseMaxVelocity(EEComplexUnit* inUnit);
    void SpreadAroundTile(long inTileX, long inTileY) { ee_units_ag_spread_around_tile(H(this), (int32_t)inTileX, (int32_t)inTileY, 0); }  // 0x1ef2d4 (EEMDropoffResource: the drop-off's tile)
    void SpreadAroundUnit(EEUnit* inTarget, long inTileX, long inTileY) { ee_units_ag_spread_around_unit(H(this), H(inTarget), (int32_t)inTileX, (int32_t)inTileY); }  // 0x1eecb2 (EEMAttackUnit: the target and its tile)
    void KeepMobileUnits() { ee_units_ag_keep_mobile(H(this)); }
    // the unit's reserved destination (the teleport calamity)
    void TellGroup(EEComplexUnit* inUnit, const GE3DPoint& inLocation) { ee_units_group_tell(this, inUnit, (const float(*)[3])&inLocation.mX); }
};

// A player's tech tree (RTTI EETechTree > RTSTechTree, vtable 0x446114, sizeof 0x554, ctor 0x1cbcc2: the player's
// Reset news it into +0x9cc) and its nodes (RTTI EETechTreeNode > RTSTechTreeNode, vtable 0x446150, sizeof 0x24,
// ctor 0x1cedce; EETechnology stays a typedef). Decoded from src/Empire Earth/Tech Tree/ (EETechTree.cpp 0x1cba2f,
// EETechTreeEpoch.cpp 0x1cdf25, EETechTreeNode.cpp 0x1cedce); names are provisional.
// port (player/tech lane, round 3): the tree, its epochs and its nodes are their Rust records (ee_sim tech nodes.rs;
// made and freed by Rust: tree_objects.rs, ee_game tech_tree objects.rs) and the classes add no member, so a pointer
// to one is its record; the original's virtuals (an epoch's GetIsObsolete, GetCost, GetBuildTime, LoadMedia,
// UnloadMedia and destructor) dispatch in Rust on whether the node is an epoch (its mLists).
class EETechTree;
extern EETechTree gTechTree;                // 0x52fd98 (Empire Earth.cpp: the database's tree)
// the host of every walk (ctx: the tree; ee_game tech_tree host.rs)
inline int32_t EETechHost(void* inTree, int32_t inOp, EeTechHostIo* io) { return ee_gamecpp_tech_host(inTree, inOp, io); }

// What a node's mData points to (sizeof 0x84; most fields copied from the 0x10c-byte database record): the Rust
// record EeTechNodeDataFields (ee_sim tech nodes.rs: the costs, kind, id, events, prerequisites, unit classes and
// their cost tables)
typedef EeTechNodeDataFields RTSTechTreeNodeData;

// A tech tree node (an epoch is one too): EeTechNodeFields' flags, epochs, prerequisites, research delay, an epoch's
// counters, mData, mTree, mButton and an epoch's mLists. The bodies are Rust over EETechHost
class EETechTreeNode : public EeTechNodeFields {
public:
    EETechTreeNode() = delete;   // (Rust makes the nodes)
    // whether the node can be researched now (wonders only after enough epochs or in the last epoch)
    bool GetIsAvailable() { return this->Walk(EeTechNodeOp::IsAvailable) != 0; }   // +0x04 0x1cf686
    // +0x0c 0x1cef26: the cost of a resource for a unit class (its own costs when it has some), scaled for
    // unit-making nodes by wonder epoch, cost set, upgrade and just-in-time manufacturing; an epoch's by the epochs
    // advanced (TCST, TEST ONLY)
    long GetCost(long inResource, long inUnitClass, float inScale) { return ee_gamecpp_tech_obj_cost(this, inResource, inUnitClass, inScale); }
    // +0x14 0x1cf882, +0x1c 0x1cfcc9: the unit class and its buttons, and the media its research event uses
    void LoadMedia(EEPlayer* inPlayer) { ee_gamecpp_tech_obj_media(this, inPlayer, true); }
    void UnloadMedia(EEPlayer* inPlayer) { ee_gamecpp_tech_obj_media(this, inPlayer, false); }
    // +0x18 0x1cfa53 (inUnit: the researching unit): fires its event; 2 when already researched (repeatable kind 3
    // excepted), 9 when not possible; counts down a research delay (TNRS, TEST ONLY)
    long ResearchNode(EEComplexUnit* inUnit) { return this->Walk(EeTechNodeOp::Research, 0, false, inUnit); }
    bool Contains(long inUnitClass) { return ee_tech_node_classes_contains(this->mData, inUnitClass); }   // 0x1cf6e5
    bool DependsOn(long inID) { return this->Walk(EeTechNodeOp::DependsOn, inID) != 0; }   // 0x1cf79a (recursive)
    void MakeAvailable() { this->Walk(EeTechNodeOp::MakeAvailable); }   // with every prerequisite met
    // the scenario editor's flag; recursively obsoletes the nodes depending on it, or un-obsoletes its prerequisites
    void SetScenEditorObsoleted(bool inValue, bool inRecurse) { this->Walk(EeTechNodeOp::SetEditorObsoleted, inValue ? 1 : 0, inRecurse); }
    // obsolete (the obsolete event, unloading its unit media unless kept) or the flag cleared
    void SetObsolete(bool inValue, bool inKeepMedia) { this->Walk(EeTechNodeOp::SetObsolete, inValue ? 1 : 0, inKeepMedia); }
    void SetButton(unsigned long inButton) { ee_gamecpp_tech_node_set_button(this, inButton); }   // swapped into its unit classes
    UIHotKey* GetHotKey() { return (UIHotKey*)ee_gamecpp_tech_node_hot_key(this); }   // 0x268dc6: mData->mHotKey's
    long GetUnitClass(unsigned long inIndex) { return ee_gamecpp_tech_obj_unit_class(this, inIndex); }   // the default past the end
    unsigned long GetNumUnitClasses() { return ee_tech_node_classes_count(this->mData); }
    int32_t Walk(EeTechNodeOp inOp, long inA = 0, bool inB = false, EEComplexUnit* inUnit = NULL);   // ee_tech_walk_node
};
typedef EETechTreeNode EETechnology;

// One epoch of a tech tree (RTTI EETechTreeEpoch > EETechTreeNode, vtable 0x44612c, sizeof 0x54): itself the node of
// its id, holding the other nodes of its ids (mLists)
class EETechTreeEpoch : public EETechTreeNode {
public:
    EETechTreeEpoch() = delete;
    // the media of the epoch's nodes of one kind loaded (ee_sim tech node_media.rs), or unloaded
    void LoadMediaOfKind(long inKind) { this->EpochWalk(EeTechEpochOp::MediaOfKind, inKind, true, (EEComplexUnit*)ee_gamecpp_tech_tree_player(this->mTree)); }
    void UnloadMediaOfKind(long inKind) { this->EpochWalk(EeTechEpochOp::MediaOfKind, inKind, false, (EEComplexUnit*)ee_gamecpp_tech_tree_player(this->mTree)); }
    // the epoch's nodes of one kind researched (only those researched with the epoch when asked)
    void ResearchKind(long inKind, bool inEpochOnly) { this->EpochWalk(EeTechEpochOp::ResearchKind, inKind, inEpochOnly); }
    int32_t EpochWalk(EeTechEpochOp inOp, long inA = 0, bool inB = false, EEComplexUnit* inUnit = NULL);   // ee_tech_walk_epoch
};

#include "ee/EETechTreeTechID.h"   // EETechTreeTechID (own header: ee/EETechTreeTechID.h)

// EeTechTreeFields: mCurrentEpoch (+0x538; EEMusicManager 0x132022 passes it to EEMusicObject's pick), mStartEpoch,
// mEndEpoch (= 14), mEpochsBeforeWonders (= 1), mMediaID, mEpochNameID (30145 + the epoch), mIsLoaded, mPlayerIndex,
// mPlayer (+0x534), the saved game's research lists (mSaved) and the epochs (mEpochs: the original's member array of
// 15). The original's base RTSTechTree is folded in. The bodies are Rust over EETechHost
class EETechTree : public EeTechTreeFields {
public:
    EETechTree() { ee_gamecpp_tech_tree_init(this); }   // every epoch, the prehistoric one current
    ~EETechTree() { ee_gamecpp_tech_tree_free(this); }   // 0x1cbd99: the saved game's lists, the epochs
    EETechTree(const EETechTree&) = delete;
    void SetPlayerIndex(long inValue) { this->mPlayerIndex = inValue; }   // the player's index (provisional name)
    // an epoch's id is its position; a node is found in its epoch (EETechTreeEpochOf)
    EETechTreeNode* Get(long inID) { return (EETechTreeNode*)ee_gamecpp_tech_obj_get(this, inID); }
    RTSTechTreeNodeData* GetData(long inID) { return this->Get(inID)->mData; }   // inline (editor P7)
    // 0x1cbdf0 (the database): every epoch block of the file, the epoch count, the epoch range reset to all epochs
    long Load(FSFileSpec& inFile) { return ee_gamecpp_tech_load_file(this, &inFile); }
    static void BuildUnitClassInfo() { gTechTree.Walk(EeTechTreeOp::BuildUnitClassInfo); }   // 0x1cbe4a (gTechTree's events by unit class)
    // the researched/obsolete state of every tech and epoch, weighted by position (ee_tech tree_checksum)
    void GetChecksum(ESChecksum& ioChecksum);
    // whether a tech's model or sound effect for a unit class is the latest one the tree still uses
    static bool IsLatestEffect(EETechTree* inTree, long inID, long inUnitClass, long inKind) { return ee_tech_is_latest_effect(inTree, &EETechHost, inTree, (int32_t)inID, (int32_t)inUnitClass, (int32_t)inKind); }
    EEPlayer* GetPlayer() { return (EEPlayer*)ee_gamecpp_tech_tree_player(this); }   // by index when no player pointer is set
    // a loaded tree's state reapplied: editor-obsoleted techs, the epoch range, research, each epoch's counters, then
    // the interface
    void ApplySavedState(bool inApplyObsoleted, bool inApplyResearch) { this->Walk(EeTechTreeOp::ApplySavedState, inApplyObsoleted ? 1 : 0, inApplyResearch ? 1 : 0); }
    // 0x1ccbc9 (saved game): the research, obsolete and editor-obsoleted ids, the epochs and each epoch's state
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_tech_tree_save(this, &inFile, true, inMajorVersion, inMinorVersion); }
    void MakeObsolete(long inID) { this->Walk(EeTechTreeOp::MakeObsolete, inID); }
    void SetPrerequisite(long inID, long inPrerequisite) { this->Walk(EeTechTreeOp::SetPrerequisite, inID, inPrerequisite); }
    // 0x1ccec2 (inUnit: the researching unit, passed on to EEEvent::Trigger) (TTRS, TEST ONLY)
    void Research(long inID, EEComplexUnit* inUnit) { this->Walk(EeTechTreeOp::Research, inID, 0, inUnit); }
    long Save(unsigned long, unsigned long, FSFile& outFile) { return ee_gamecpp_tech_tree_save(this, &outFile, false, 0, 0); }
    // forward to an epoch (never back), recorded in statistics series 0xc, researching up to it
    void SetEpoch(long inEpoch) { this->Walk(EeTechTreeOp::SetEpoch, inEpoch); }
    void ResetEpochRange(long inStart, long inEnd) { this->Walk(EeTechTreeOp::ResetEpochRange, inStart, inEnd); }   // the current epoch its start, no research
    void SetNodeAvailable(long inID, bool inValue) { this->Walk(EeTechTreeOp::SetNodeAvailable, inID, inValue ? 1 : 0); }   // unless it has a research delay
    void SetPlayer(EEPlayer* inPlayer) { ee_gamecpp_tech_set_player(this, inPlayer); }   // and, for a player, its index
    // for the local player's tree, the new epoch's name shown and the main game form's group panel refreshed
    void UpdateInterface(bool inEpochChanged) { ee_gamecpp_tech_update_interface(this, inEpochChanged); }
    // another tree's epochs, epoch range and player; the copy is not marked loaded
    EETechTree& operator=(const EETechTree& inOther) { return *(EETechTree*)ee_gamecpp_tech_tree_assign(this, &inOther); }
    int32_t Walk(EeTechTreeOp inOp, long inA = 0, long inB = 0, EEComplexUnit* inUnit = NULL) { return ee_tech_walk_tree(this, &EETechHost, inOp, this, (int32_t)inA, (int32_t)inB, inUnit); }
};
inline int32_t EETechTreeNode::Walk(EeTechNodeOp inOp, long inA, bool inB, EEComplexUnit* inUnit)
{
    return ee_tech_walk_node(this->mTree, &EETechHost, inOp, this->mTree, EeTechNodeRef{this, this, this->mData}, (int32_t)inA, inB, inUnit);
}
inline int32_t EETechTreeEpoch::EpochWalk(EeTechEpochOp inOp, long inA, bool inB, EEComplexUnit* inUnit)
{
    return ee_tech_walk_epoch(this->mTree, &EETechHost, inOp, this->mTree, EeTechNodeRef{this, this, this->mData}, (int32_t)inA, inB, inUnit);
}

// One statistics series of a player, a copy GetSeries makes (the series themselves are Rust's)
struct EEStatisticsSeries {
    EEStatisticsSeries() : mPoints(1, EeUnitsStatPoint{0, 0}), mMaxValue(0), mKeepHistory(false) {}
    std::vector<EeUnitsStatPoint> mPoints;  // the points (EEStatisticsPoint: the value, the game time; ee_units player_stats.rs)
    unsigned long mMaxValue;                // the largest value pushed (GetValue)
    bool mKeepHistory;                      // false: Set replaces the last point
};

// What the player holds at +0x4a8: RTTI EEPlayerStatistics (vtable 0x442080). Was EEPlayerSavedGroups.
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
    // the name, then Rust's (the end time, each series' maximum and points)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
    {
        long theResult = this->mName.Load(inFile);
        return theResult != 0 ? theResult : ee_units_stats_load(this->mStats, &inFile, &EESaveFilePrimitive, &this->mEndTime);
    }
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
    {
        long theResult = this->mName.Save(outFile);
        return theResult != 0 ? theResult : ee_units_stats_save(this->mStats, &outFile, &EESaveFilePrimitive, this->mEndTime);
    }
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
    void SetName(UWideString inName) { this->mName = inName; }
    EEStatisticsSeries GetSeries(unsigned long inSeries)   // a copy (the post-game summary, the scores)
    {
        EEStatisticsSeries theSeries;
        theSeries.mPoints.resize(ee_units_stats_value(this->mStats, inSeries, 3));
        if (!theSeries.mPoints.empty())
            ee_units_stats_points(this->mStats, inSeries, theSeries.mPoints.data(), theSeries.mPoints.size());
        theSeries.mMaxValue = ee_units_stats_value(this->mStats, inSeries, 0);
        theSeries.mKeepHistory = ee_units_stats_value(this->mStats, inSeries, 4) != 0;
        return theSeries;
    }
    void Record(uint32_t inOp, long inSeries, long inValue);   // Set's gate and the game time, then Rust's op

    UWideString mName;                      // (saved)
    EeUnitsPlayerStats* mStats;             // the 31 series (Rust)
    unsigned long mEndTime;                 // (saved)
};

// port (R5b.4): the saved numbers (mScenarioVariables, mEnemies, mTributes, mResourceCaps, mPopLimit, ... the 24
// fields EEPlayer::Load and Save read and write as plain values) are the Rust record EeUnitsPlayerSaved
// (ee_units player_save.rs) this class derives from, with their C++ names and types; the offsets below omit them.
// port (the player lane, round 1): the other plain members (mHackUnitClass .. mJITManufacturing, mNumTeleporters) are
// the Rust record EeUnitsPlayerFields (ee_units player.rs; their names and types kept), with the queued improvements
// (mQueued, Rust's); the economy's and the improvements' bodies are Rust over EEPlayer.cpp's host
// The player lane, round 2: the original's base RTSPlayer (C:\EEX\RTS Engine\Player\RTSPlayer.h, vtable 0x442b5c; its
// plain members the Rust record EeUnitsRtsPlayerFields, its unit list's bodies ee_game rts units.rs) is folded in:
// EEPlayer is the only player kind. RTSPlayer names it for the engine's files.
class EEPlayer : public EeUnitsRtsPlayerFields, public EeUnitsPlayerSaved, public EeUnitsPlayerFields {
public:
    // (EEHumanPlayer's ctors call them; its ??_G calls the dtor): the base player's reset (0x16f464; named: every
    // player but player 0 keeps a visibility map), then all state reset, the tech tree bound to a named player, the
    // wonder and construction-site lists made (ee_units player_body.rs construct); the destructor clears and releases
    // the groups, media, tech tree, air traffic, upgrades, towns, herds, off-map records, patrol groups and lists, then
    // its list's records leave the unit store (ee/EEUnitStore.h)
    EEPlayer() { ee_rts_player_reset(this); ee_units_player_body(this, EE_UNITS_PLAYER_DO_CONSTRUCT, 0, 0, 0, 0.0f); }
    EEPlayer(const UWideString& inName, unsigned long inPlayerID)   // 0x161da5 (0x16f12e first)
    {
        ee_rts_player_reset(this);
        this->mName = inName;
        this->mPlayerID = inPlayerID;
        this->mSaveVisMap = inPlayerID != 0;
        ee_units_player_body(this, EE_UNITS_PLAYER_DO_CONSTRUCT, 0, 1, (int64_t)inPlayerID, 0.0f);
    }
    virtual ~EEPlayer()   // +0x00 0x162297
    {
        ee_units_player_body(this, EE_UNITS_PLAYER_DO_DESTROY, 0, 0, 0, 0.0f);
        ee_units_store_clear_player_object((uint64_t)(uintptr_t)this);
    }
    // the FSRestorable-style slots EEPlayer overrides (EEHumanPlayer's wrappers call them)
    virtual void Clear() { ee_units_player_body(this, EE_UNITS_PLAYER_DO_CLEAR, 0, 0, 0, 0.0f); }                   // +0x1c 0x16828c (same RTSPlayer slot)
    virtual long LoadUnit(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x30 0x167ca4
    virtual void RemovalsDone() { ee_units_groups_player(this, 3); }            // +0x34 0x16854e (same RTSPlayer slot)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x18 0x165cfa
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond) { return (long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_REPAIR_POINTERS, inFirst, (int64_t)inSecond, 0, 0.0f); }   // +0x20 0x1690ca
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x24 0x169c46
    virtual void AddPendingUnits(bool inKeepID, bool inReleasePending) { ee_rts_eeplayer_add_pending_units(this, this->mGroupLists, inKeepID, inReleasePending); }  // +0x04 0x16275a (mPendingGroups into mRunningGroups, then RTSPlayer's)
    virtual void RecountPopulation() { EEPlayerUnits(this, 4); }       // +0x08 0x16379e (counts the units into mPopulationCount/mEndOfGameCount/mNumVitalBuildings)
    virtual void ReleasePendingUnits() { ee_rts_eeplayer_release_pending_units(this, this->mGroupLists); }     // +0x0c 0x163f7e (the pending groups cleared first)
    virtual void GetChecksum(ESChecksum& ioChecksum) { ee_units_player_rest(this, EE_UNITS_PLAYER_REST_CHECKSUM, (uint64_t)(uintptr_t)&ioChecksum, 0, 0); }   // +0x10 0x164c4c (RTSPlayer's, the resources, the tech tree)
    virtual bool IsActive() { return !this->mObserverMode || this->mGameResult != 0; }                // +0x14 0x164e0c (!mObserverMode || mGameResult)
    // +0x28 0x162ba8: a unit joins the player (RTSPlayer's add plus the counts, tech nodes and statistics)
    virtual void AddUnit(RTSUnit* inUnit, bool inKeepID) { EEPlayerUnits(this, 0, inUnit, inKeepID); }
    virtual void UnitRemoved(RTSUnit* inUnit) { ee_units_player_rest(this, EE_UNITS_PLAYER_REST_UNIT_REMOVED, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0); }  // +0x2c 0x164241 (a complex unit leaves: ClearGoalQueue per mGoalQueues item) (clears mPendingGroups, then RTSPlayer's)
    virtual void RemovingUnit(RTSUnit* inUnit) { ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REMOVING_UNIT, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0); }  // +0x38 0x168deb (an EEUnit leaves its tile; out of its region and the influence table while linked)
    // +0x44 (EEMDisband: the unit's technology, its type, 0.75 of its hit-point fraction: a refund) (provisional name)
    virtual void RefundTechnology(EETechnology* inTechnology, long inUnitType, float inFraction) { ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_REFUND_TECHNOLOGY, inTechnology, inUnitType, 0, 0, 0, 0, inFraction, NULL); }
    // +0x48..+0xa8: inline in the real header (their bodies 0xa5486/0xa549a/0xa54a1/0xa54a7 sit in
    // EEHumanPlayer.cpp, the first unit in link order that emits EEPlayer's vtable without overriding them)
    // +0x48 0xa5486; port: counted into the game log (ee/EEGameLog.h)
    virtual void AddResource(long inIndex, long inAmount) { this->mResourceCaps[inIndex] += inAmount; EEGameLogIncome(this, inIndex, inAmount); }
    virtual void DetachUnitFromAI(EEComplexUnit* inUnit);  // +0x4c 0x16367b (sets the unit's mIsDetachedFromAI)
    // +0x50 (EEComplexUnit::QueueResearch compares GetHeroCount(1, 1) with mHeroLimit: a count) (provisional name)
    virtual long GetHeroCount(bool inByClass, bool inRequests) { return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_NUM_HEROES, NULL, inByClass, inRequests, 0, 0, 0, 0.0f, NULL); }    // (bools: 0x164223 reads bytes; EEComputerPlayer 0x2e248 stores them as bytes)
    // +0x54 0x163a25 (ret 0x18; can the player build inTechnology: EEABuild 0x1760 passes (id, 0x20, 1, 1, 3, 0); +0x14
    // is read as a byte, +0x1c receives the reason through UResourceFile::GetString; provisional names)
    virtual bool CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason) { return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_PRODUCE, NULL, inTechnology, inUnitClass, inCount, inCheckCost, inWonderMode, 0.0f, outReason) != 0; }
    virtual void NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag) {}  // +0x58 0x163d6f (EESMEffect 0x1be94e passes (another player, false); 0x55cee tests a byte; ret 8: EEComputerPlayer 0x2dca6 forwards both)
    virtual void Virtual60() {}               // +0x60
    virtual void UnitCompleted(EEComplexUnit* inUnit) { EEPlayerUnits(this, 3, inUnit); }  // +0x68 0x164003 (ret 4: EEComputerPlayer 0x2e1e9 forwards it)
    virtual long ScaleCost(long inCost) { return inCost; }  // +0x6c 0xa549a
    // +0x70 0x164bea (ret 0xc): reads inUnit's mStats/mUnitClass and inOther's class, then 0x16542e (provisional name)
    virtual bool CanAffordRepair(EEUnit* inUnit, EEUnit* inOther, unsigned long inValue) { return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_AFFORD_REPAIR, inUnit, (int64_t)reinterpret_cast<uintptr_t>(inOther), inValue, 0, 0, 0, 0.0f, NULL) != 0; }  // (bool: EEUGBuildRepair::EvaluateCondition tests al)
    EEUnit* GetNearestConstructionSite(EEComplexUnit* inUnit) { return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(ee_units_player_body(this, EE_UNITS_PLAYER_DO_NEAREST_SITE, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0, 0.0f))); }  // 0x164820 (the next unit inUnit should build; EEUGBuildRepair::FindNextWork) (provisional name)
    virtual bool IsHuman() { return false; }                 // +0x74 (EEPlayer::SwapClassButtonMedia tests `al == 1`)
    // inline accessors the diplomacy form expands (EEUIEDSOpen 0x1d4165 reads each through a temporary;
    // provisional names)
    unsigned long GetGameResult() const { return this->mGameResult; }
    long GetEnemy(unsigned long inPlayer) const { return this->mEnemies[inPlayer]; }
    // inline: inPlayer is this player or not at war with it (EECPCombatManager::IsValidTarget 0x473684 compares the
    // result as a byte: `cmp al, bl` for `== false`) (provisional name)
    bool IsAlly(EEPlayer* inPlayer) { return inPlayer == this || this->mEnemies[inPlayer->mPlayerID] == 0; }
    virtual long GetExtraPopulation(long inUnitClass) { return 1; }  // +0x78 0xa54a1 (`push 1; pop eax; ret 4`: a long, not a bool)
    virtual long GetPopulationLimit(long inArg);  // +0x7c 0x16528b (ret 4: mB24 + mB20 + a global; EECPCombatStrategy 0x771be)
    // +0x80 (conscription 0xafe5f) (provisional name)
    // (0x16566a: arg 3 is a count, multiplied with each cost: `imul` in CanAffordScaled)
    virtual bool CanAfford(EETechnology* inTechnology, long inUnitType, long inCount) { return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_AFFORD, inTechnology, inUnitType, inCount, 0, 0, 0, 0.0f, NULL) != 0; }
    // +0x84 0x16569c (ret 0xc; EEComputerPlayer's 0x2f16d passes its unit third)
    virtual void DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit) { ee_units_player_body(this, EE_UNITS_PLAYER_DO_DAMAGE, 0, inPlayer, inAmount, 0.0f); }
    virtual void EpochMediaLoaded(long inEpoch) {}  // +0x88 (0x157269 `ret 4`, empty; EETechTreeNode 0x1cf989 passes a node's +0x4c)
    virtual void PlayAcknowledgement(EEComplexUnit* inUnit) { ee_units_selection(this, 9, inUnit, 0, NULL, 0, false, false); }  // +0x8c 0x167f10 (a unit of the group the messages act on)
    virtual void RefreshTownGroups() { ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REFRESH_TOWN_GROUPS, 0, 0, 0); }       // +0x90
    virtual void UpdatePlayer();            // +0x94
    virtual void FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile) {}  // +0x98 (EEMSendFlare; provisional name)
    virtual void UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps);  // +0x9c 0x167f70 (ret 4; EEPhysics 0x1534d1 passes &mSound2Index7)
    virtual long FinishUnitsLoad(long inMajorVersion, long inMinorVersion) { return (long)ee_units_player_rest(this, EE_UNITS_PLAYER_REST_FINISH_UNITS_LOAD, 0, inMajorVersion, inMinorVersion); }  // +0xa0 0x1690a5 (each unit's slot +0x6c; returns 0)
    // port: the original let every computer player catch up to a human's epoch here (EEComputerPlayer::CatchUpEpoch);
    // the timer went with the C++ classic's decisions: a Lua AI reads the epochs itself
    virtual void EpochReached(long inEpoch) {}  // +0xa4 0x1695e7 (ret 4; 0..14 when EnabledChanged(); the tech tree passes it)
    virtual void SetCombatState(unsigned long inCombatState) { this->mCombatState = inCombatState; }  // +0xa8 0xa54a7
    virtual void UnitStranded(EEComplexUnit* inUnit) {}  // +0xac (ret 4: EEComputerPlayer 0x3ed48 references the unit)
    virtual void PayImprovement(EEUnitClass* inClass, long inImprovement) { ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_PAY_IMPROVEMENT, inClass, inImprovement, 0, 0, 0, 0, 0.0f, NULL); }  // +0xb0 0x16b3f7 (ret 8; QueueImprovement passes the class)
    // +0xb4 (conscription 0xafe7e) (provisional name)
    virtual void Pay(EETechnology* inTechnology, long inUnitType, float inFraction) { ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_PAY, inTechnology, inUnitType, 0, 0, 0, 0, inFraction, NULL); }
    virtual void TakeResource(long inArg1, long inArg2) { this->mResourceCaps[inArg1] -= inArg2; }  // +0xb8 0x16b08a (two arguments by `ret 8`)
    virtual void SuggestRecomputeAllAreaEffects();  // +0xbc
    virtual void UnitPlaced(EEComplexUnit* inUnit) { EEPlayerUnits(this, 1, inUnit); }  // +0xc0 0x16b88f (ret 4: EEComputerPlayer 0x3ef5d forwards it)
    virtual void UnitTaken(RTSUnit* inUnit, bool inSwapped) { EEPlayerUnits(this, 2, inUnit, inSwapped); }  // +0xc4 (2 arguments: EEUnit::DepleteResource pushes (this, 0); types inferred)
    virtual void UnitGoalDone(EEComplexUnit* inUnit) { ee_units_player_rest(this, EE_UNITS_PLAYER_REST_UNIT_GOAL_DONE, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0); }            // +0xc8
    virtual void EpochMediaUnloaded(long inEpoch) {}  // +0xcc (0x157269 `ret 4`, empty; EETechTreeNode 0x1cfcc9 passes a node's +0x4c)
    virtual bool AvoidsEnemies(EEComplexUnit* inUnit) { return false; }  // +0xd0 (ret 4, al: EEComputerPlayer 0x3f73e)
    virtual void WonderStarted(EEComplexUnit* inUnit) {}  // +0xd4 (ret 4: EEComputerPlayer 0x3f7ba)
    virtual bool AdvanceImprovement(unsigned long inUnitType) { return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_ADVANCE_IMPROVEMENT, NULL, (int32_t)inUnitType, 0, 0, 0, 0, 0.0f, NULL) != 0; }  // +0xd8 0x16964a (ret 4; ComputeLoadDistance passes mQueuedImprovements's key, tests al)

    // 0x16ae11: with inUnit and inFlag set, calls slot 0x24 of +0x9ac; the hot-key events call it with (0, true)
    // (provisional name)
    void SetUnitClassMedia(long inUnitClass, long inMediaFlags);
    void UnloadUnitClassMedia(long inUnitClass);
    // logic bomb (provisional names): Stop passes the target's complex unit and false, Update the unit
    void AddWonder(EEComplexUnit* inUnit, bool inIsLoading) { EEPlayerUnits(this, 7, inUnit, inIsLoading); }
    void RemoveWonder(EEComplexUnit* inUnit) { EEPlayerUnits(this, 8, inUnit); }
    void DeselectUnit(EEUnit* inUnit) { ee_units_selection(this, 7, inUnit, 0, NULL, 0, false, false); }                      // 0x168eb9 (the hurricane's Stop, its unit)
    void EnableCamouflagePower() { ee_units_player_body(this, EE_UNITS_PLAYER_DO_CAMOUFLAGE, 0, 1, 0, 0.0f); }           // 0x16c548 (the civilization manager's civ power)
    // 0x164e22: the last town of the vector at +0x7cc whose area holds inUnit's tile, or 0 (the prophet analyzers'
    // 0xaa8b; provisional name)
    unsigned long GetNumTowns() const { return (unsigned long)ee_units_lists_len(this->mLists, EE_UNITS_LISTS_TOWNS); }   // inline COMDAT 0x4098f0 (mTowns.size(); provisional name)
    EETown* GetTown(unsigned long inIndex) const { return reinterpret_cast<EETown*>(static_cast<uintptr_t>(ee_units_lists_at(this->mLists, EE_UNITS_LISTS_TOWNS, inIndex))); }   // mTowns[inIndex]
    void LoadUnitClassButtons(long inUnitClass);            // 0x167c5a (the hurricane's Virtual28)
    void UnloadUnitClassButtons(long inUnitClass);          // 0x16b5f9 (the hurricane's Virtual34)
    void ApplyImprovement(long inUnitType, long inImprovement, bool inFlag) { ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_APPLY_IMPROVEMENT, NULL, inUnitType, inImprovement, inFlag, 0, 0, 0.0f, NULL); }  // 0x16b65a (ret 0xc; arg 3 read as a byte)
    // 0x1655f6: inUnit's entry in the player's +0x7c0 vector, 0 if none (the teleport sets its +0x14 to 4)
    // (provisional name)
    EEWUnitsOffMapInfo* GetOffMapInfo(EEComplexUnit* inUnit) { return ee_units_off_map_info(this->mLists, (int64_t)(uintptr_t)(void*)inUnit); }  // (the lists' record of inUnit, 0 none)
    // selection by the EEM* messages (EEMessage.cpp Executes; provisional names)
    void IncrementEpic() { this->mTechnologies->Walk(EeTechTreeOp::AdvanceEpoch); }
    void UnselectAllUnits() { ee_units_player_body(this, EE_UNITS_PLAYER_DO_UNSELECT_ALL, 0, 0, 0, 0.0f); }
    EEPPatrolGroup* GetPatrolGroup(unsigned long inID) { return reinterpret_cast<EEPPatrolGroup*>(static_cast<uintptr_t>(ee_units_player_body(this, EE_UNITS_PLAYER_DO_PATROL_GROUP, inID, 0, 0, 0.0f))); }   // 0x16519e (mPatrolGroups's group with that id; EEUGPatrol::RepairPointers)
    void AddPatrolGroup(EEPPatrolGroup* inGroup) { ee_units_lists_add(this->mLists, EE_UNITS_LISTS_PATROLS, (uint64_t)(uintptr_t)inGroup); }   // 0x16279f: referenced, pushed into mPatrolGroups (provisional name)
    long CountQueuedImprovements(unsigned long inUnitType, long inArg) const { return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_COUNT_QUEUED, NULL, (int32_t)inUnitType, inArg, 0, 0, 0, 0.0f, NULL); }  // 0x164f4a (EEMRemoveImprovement; const: the const vector reads and the dead const-operator[] lea; provisional name)
    void RemoveQueuedImprovement(long inUnitType, long inImprovement, bool inFlag) { ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_REMOVE_QUEUED, NULL, inUnitType, inImprovement, inFlag, 0, 0, 0.0f, NULL); }  // 0x16870e (EEMRemoveImprovement)
    void SetHackUnitClass(long inUnitClass) { this->mHackUnitClass = inUnitClass; }  // 0x16ae52 (EEMLoadUnitData; provisional name)
    // 0x16af5c (EEMResign passes 2; provisional name); out of line: the harness hooks it (tools/build.py HARNESS_HOOKS)
    void SetGameResult(long inState);
    // 0x16384d (EEMImproveUnit). QueueImprovement passes 0, so the original takes a UWideString*; kept a reference until
    // EEMessage.cpp's caller passes &theText
    bool CanBuy(long inUnitType, long inImprovement, UWideString* outText) { return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_BUY, NULL, inUnitType, inImprovement, 0, 0, 0, 0.0f, outText) != 0; }     // (QueueImprovement passes 0: a pointer)
    void QueueImprovement(unsigned long inUnitType, unsigned long inImprovement, bool inFront, bool inAlreadyPaid) { ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_QUEUE_IMPROVEMENT, NULL, (int32_t)inUnitType, (int32_t)inImprovement, inFront, inAlreadyPaid, 0, 0.0f, NULL); }  // 0x162694 (EEMImproveUnit; args 3/4 tested as bytes)
    bool CanImprove(long inUnitType, long inImprovement, UWideString* outReason) { return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_IMPROVE, NULL, inUnitType, inImprovement, 0, 0, 0, 0.0f, outReason) != 0; }
    // 0x165813 (both ctors call it after the vptr store; hides RTSPlayer::Reset): new selection and control groups,
    // upgrades, tech tree and air traffic, zeroed counters and default settings, then the player effects
    const UWideString& GetName() { return this->mName; }           // 0x167eda (&mName, out of line; Reset's; provisional name)
    void UpdateFreeUnitPower() { EEPlayerUnits(this, 12); } // original Player unit-class update, no arguments
    void PulseSight(bool inByID, float inSight, long inID) { ee_units_player_body(this, EE_UNITS_PLAYER_DO_PULSE_SIGHT, (uint64_t)(int64_t)inID, inByID, 0, inSight); } // original169b26, three arguments/ret12 // 0x163138 (ret 4; RepairPointers: each unit's complex unit; provisional name)
    unsigned long GetTeamIndex() { return (unsigned long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_TEAM_INDEX, 0, 0, 0, 0.0f); }           // 0x165561 (the index of the team holding this player, 0 if none; provisional name)
    // +0x3c 0x16a4b5 (ret 0x10): one unit's type, whether it is a unit (not 0xdf) and complex, then its Save
    virtual long SaveUnit(RTSUnit* inUnit, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    void EraseQueuedImprovements(unsigned long inUnitType) { ee_units_queued_op(this->mQueued, 0, inUnitType, 0); }  // 0x163f36 (mQueuedImprovements's entry erased; 0x12ba6d)
    void MoveQueuedImprovements(unsigned long inFrom, unsigned long inTo) { ee_units_queued_op(this->mQueued, 1, inFrom, inTo); }  // 0x16419b (mQueuedImprovements's entry moved)
    // inline (out-of-line COMDATs after their first users in EEUICPortrait.cpp: 0x258cb8 / 0x2594e8): the improvement
    // being researched for inUnitType (0x10: none) and its progress (0.0 when absent; read as a float by 0x2594e8:
    // mQueuedImprovements's first is really a float) (provisional names)
    long GetResearchedImprovement(unsigned long inUnitType) { return (long)ee_units_queued_op(this->mQueued, 2, inUnitType, 0); }
    float GetImprovementProgress(unsigned long inUnitType) { return ee_units_queued_timer(this->mQueued, inUnitType); }
    void RestartTowns();                    // 0x1680e4 (each town's UpdateNearWater; EEWorld::StartGame)
    void AssignHerds() { ee_units_player_areas(EEPlayerRefs(this), 5, NULL); }                     // 0x164453 (player 0, after RegisterHerds; provisional name)
    void AssignTowns() { ee_units_player_areas(EEPlayerRefs(this), 4, NULL); }                     // 0x16451a (the other players, after RegisterTowns; provisional name)
    // inline (out-of-line COMDAT 0xc350f after its first user EEMPayTribute::Execute): the tribute statistics, inDirection
    // 0 paid / 1 received (provisional names)
    void AddTribute(long inDirection, long inResource, long inAmount)
    {
        this->mTributes[inDirection * 5 + inResource - 2] += inAmount;
        this->mSavedGroups.Reset((inDirection != 0) + 0x11, inAmount);
    }
    void SetSelectedViewedUnit(RTSUnit* inUnit) { ee_units_player_body(this, EE_UNITS_PLAYER_DO_SET_SELECTED_VIEWED, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0, 0.0f); }  // 0x16adee (with mViewedUnit, after a selection message; provisional name)
    // 0x11c10d (out of line; EEMap.cpp's range): the same player, or neither is at war with the other (EEPPlayerEvent;
    // provisional name)
    bool IsAlliedWith(EEPlayer* inPlayer);
    // EEPPlayerEvent's (de)activation at a tile (provisional names)
    void RemoveSharedSightAt(const EETileLocation& inLocation);
    void ExecuteSelectionMessage(NEMessage* inMessage) { ee_units_player_body(this, EE_UNITS_PLAYER_DO_EXECUTE_SELECTION, (uint64_t)(uintptr_t)inMessage, 0, 0, 0.0f); }  // 0x1646fc (EEServer::PrepareLocalInput: a fast-selection message; provisional name)
    void AddSharedSightAt(const EETileLocation& inLocation);
    // a tile's player references (the doppelganger manager follows them; provisional names)
    void RemoveSharedSight(RTSTile* inTile);  // 0x164287 (also for the allies that share the line of sight)
    void RemoveOwnSight(RTSTile* inTile);
    void RemoveOwnSightAt(const EETileLocation& inLocation);
    void AddSightSharer(EEPlayer* inPlayer) { ee_units_player_diplomacy(EEPlayerRefs(this), 1, inPlayer, 0, false); }  // 0x1627b9 (pushes inPlayer into mSightSharers between two walks of the units)
    void RemoveSightSharer(EEPlayer* inPlayer) { ee_units_player_diplomacy(EEPlayerRefs(this), 2, inPlayer, 0, false); }  // 0x1684ab (the mirror: inPlayer out of mSightSharers; RemoveWonderPower, 0x16ab41)
    void AddOffMapUnit(RTSUnit* inUnit);    // 0x162dab (a new EEWUnitsOffMapInfo for inUnit into mUnitsOffMap)
    void AddStrandedUnit(EEUnit* inUnit) { ee_units_lists_add(this->mLists, EE_UNITS_LISTS_STRANDED, (int64_t)(uintptr_t)(void*)inUnit); }   // 0x163043 (into mStrandedUnits once, referenced)
    void AddConstructionSite(EEComplexUnit* inUnit)   // 0x16368f (into mConstructionSites unless it has it)
    {
        if (!ee_units_lists_has(this->mLists, EE_UNITS_LISTS_SITES, (uint64_t)(uintptr_t)(void*)inUnit))
            ee_units_lists_add(this->mLists, EE_UNITS_LISTS_SITES, (uint64_t)(uintptr_t)(void*)inUnit);
    }
    void AddSharedSight(EETile* inTile, long inX, long inY);  // 0x168120 (AddOwnSight, then the allies sharing the line of sight)
    void AddOwnSight(EETile* inTile, long inX, long inY);  // 0x16820e (counts this player in at the tile)
    void AddOwnSightAt(const EETileLocation& inLocation);
    void RemoveOffMapUnit(RTSUnit* inUnit) { ee_units_off_map(this->mLists, 1, (int64_t)(uintptr_t)(void*)inUnit, 0, 0, 0); }  // 0x168e30 (its EEWUnitsOffMapInfo out of mUnitsOffMap, deleted)
    void SetDiplomaticStance(EEPlayer* inOther, long inStance, EEMap* inMap) { ee_units_player_diplomacy(EEPlayerRefs(this), 0, inOther, (unsigned long)inStance, inMap != NULL); }  // 0x16ab41 (the diplomatic stance towards inOther; EEWorld::SetDiplomaticStance; provisional name)
    void RefreshTerrainVisibility() { ee_units_player_body(this, EE_UNITS_PLAYER_DO_REFRESH_TERRAIN, 0, 0, 0, 0.0f); }        // 0x16aec0 (EEWorld::RestartUnits on player 0; provisional name)
    void SetCivilizationPoints(long inEpoch) { this->mCivilization.SetPointsAndMax(inEpoch); }  // 0x16ab2f (mCivilization.SetEpoch)
    void SetEpoch(long inEpoch) { this->mTechnologies->SetEpoch(inEpoch); }            // 0x16addc (the tech tree's SetEpoch)
    void SetSharedLOS(bool inFlag, EEMap* inArg) { ee_units_player_diplomacy(EEPlayerRefs(this), 3, NULL, 0, inFlag); } // 0x16ae5f (ret 8; sets mSharedLOS around the units' slots +0x24/+0x0c)
    bool HasLoadedUnitID(unsigned long inUnitID) { return ee_units_lists_has(this->mLists, EE_UNITS_LISTS_LOADED_IDS, inUnitID); }   // 0x16bbf8 (mLoadedUnitIDs has it)
    void NoticeForeignUnit(EEUnit* inUnit);  // 0x16442b (mCombatState = 1 for a unit of a player other than 0)
    long GetCitizenPassability() { return (long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_CITIZEN_PASSABILITY, 0, 0, 0, 0.0f); }           // 0x1650a3 (flags 1/2/4/8 from unit class 0x20's upgrade +0x160)
    EEAIArea* GetNearestArea(const EETileCoordinate& inTile, bool inSameContinent);  // 0x164781 (ret 8; EECPResourceManager::FindResourceTarget takes its slot +0x18) (provisional name)
    long GetTotalPopulation(bool inFirst, bool inSecond) { return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_TOTAL_POPULATION, NULL, inFirst, inSecond, 0, 0, 0, 0.0f, NULL); }  // 0x1651e0 (mPopulationCount plus the units' AddedToParent: mPendingUnits, then those in a 0x1d container)
    long GetImprovementLevel(unsigned long inUnitType, long inImprovement) const { return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_IMPROVEMENT_LEVEL, NULL, (int32_t)inUnitType, inImprovement, 0, 0, 0, 0.0f, NULL); }  // 0x164ed7 (an improvement's level; Virtual40; const: as CountQueuedImprovements)
    // 0x164fac: whether the improvement is at its maximum level (the class's mImprovements->mMaxLevel against
    // GetImprovementLevel; EEUIBUpgrade; provisional name)
    bool IsImprovementMaxed(unsigned long inUnitType, long inImprovement) { return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_IS_IMPROVEMENT_MAXED, NULL, (int32_t)inUnitType, inImprovement, 0, 0, 0, 0.0f, NULL) != 0; }
    void AddLoadedMedia(EEMedia* inMedia) { ee_units_lists_add(this->mLists, EE_UNITS_LISTS_MEDIA, (uint64_t)(uintptr_t)inMedia); }  // 0x162740 (pushes into mLoadedMedia unless 0; provisional name)
    long GetWonderTime(long inIndex) const { return (long)ee_units_player_rest(const_cast<EEPlayer*>(this), EE_UNITS_PLAYER_REST_WONDER_TIME, 0, inIndex, 0); }  // 0x165644 (mWonders's unit inIndex's mCloaked; provisional name)
    unsigned long GetNumWonderUnits() const { return (unsigned long)ee_units_lists_len(this->mLists, EE_UNITS_LISTS_WONDERS); }   // 0x16565f: mWonders->GetSize() (provisional name)
    // per unit class (mUnitClassUpgrades[inUnitClass] when inUnitClass < EEUnitClassCount(); provisional names)
    EEUnitClassUpgrade* GetUpgrade(long inUnitClass) { return this->mUnitClassUpgrades[inUnitClass]; }  // inline COMDAT 0x1bcae9
    void SwapClassButtonMedia(unsigned long inUnitClass, EEButtonObject* inFirst, EEButtonObject* inSecond) { ee_units_player_body(this, EE_UNITS_PLAYER_DO_SWAP_BUTTONS, (uint64_t)(uintptr_t)inFirst, (int64_t)inUnitClass, (int64_t)(uintptr_t)inSecond, 0.0f); }
    void SetClassAcknowledgeSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex);
    void SetClassCreationSound(unsigned long inUnitClass, EESoundObject* inSound);
    void SetClassDeathSound(unsigned long inUnitClass, EESoundObject* inSound);
    void SetClassModel(unsigned long inUnitClass, EEGraphicObject* inModel);  // 0x163e29 (EEModel)
    void SetClassButton(unsigned long inUnitClass, EEButtonObject* inButton);  // 0x163e4d (the unit class's button; EEEvent SetEventButton)
    void SetClassSelectionSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex);

    RTSUnit* GetUnit(unsigned long inUnitID) { return static_cast<RTSUnit*>(ee_rts_player_unit(this, inUnitID)); }   // 0x16f433: by id through the buckets (RTSWorld::GetUnit 0x14804f)
    // inline: EEMLoadMainScreen passes it through a byte temporary, like NEMessage::GetOriginatingSlot
    unsigned char GetSlot() const { return this->mSlot; }
    unsigned long GetPlayerID() { return this->mPlayerID; }  // inline (EEDataPreLoading::LoadGame pushes the whole dword)
    // inline (refused: out of line through the ICF-folded COMDAT 0x138e4b; EEWorld::ReplacePlayer; provisional name)
    unsigned long GetNumPendingUnits() const { return this->mPendingUnits.size(); }
    // inline (COMDAT 0x131872 in EEGameState.cpp, its first user Create; EEWorld 0x139264 calls the same copy;
    // the 0.6f constant is loaded once, then three fmul st(1))
    void SetColor(const GEColor& inColor)   // (the color and its shade at 0.6: ee_units player_rest.rs)
    {
        ee_units_player_set_color(reinterpret_cast<float(*)[3]>(&this->mColor), reinterpret_cast<float(*)[3]>(&this->mShadeColor), reinterpret_cast<const float(*)[3]>(&inColor));
    }
    void KillUnit(RTSUnit* inUnit, bool inForce) { ee_rts_player_kill_unit(this, inUnit, inForce); }  // 0x16f6ae (marks inUnit->mB28, pushes it)
    // Layout (no gaps; sizeof 0x468): the ctor 0x16f12e (vectors, the name, the bucket loop of 0x80, two GEColors
    // of 1.0), its Reset 0x16f464 and Load/Save. Names are provisional unless stated.
    // units (0x16f27f/0x16f35e Dereference the elements, KillUnit pushes an RTSUnit*; were vector<long>; the
    // _Buy/_Tidy/clear calls are ICF-folded vector<long>/<EECalamity*> copies)
    std::vector<RTSUnit*> mKilledUnits;     // +0x004
    std::vector<RTSUnit*> mPendingUnits;    // +0x014
    UWideString mName;                      // +0x024 (the chat log compares senders with it)
    // +0x038 the unit hash: the units whose id & 0x7f is the bucket's index, chained through RTSUnit::mNext (the first
    // unit, the count: ee_game rts units.rs; zeroed as the original's ctors did)
    EeRtsUnitBucket mUnitBuckets[128] = {};
    const GEColor& GetColor() const { return this->mColor; }
    GEColor mColor;                         // +0x438 (1, 1, 1; saved)
    GEColor mShadeColor;                    // +0x444 (1, 1, 1)
    EEUnit* mFirstUnit;                     // +0x450 its units, linked through EEUnit::mNext
    EEUnit* mLastUnit;                      // +0x454 (provisional name; the unit-removal code 0x16f27f fixes both ends)
    // the record (EeUnitsRtsPlayerFields): mNextUniqueID (+0x458, saved), mPlayerID (+0x45c: the index into another
    // player's mEnemies; saved), mUnitCount (+0x460, saved), mSlot (+0x464, = 0xff; EEWorld::GetPlayerBySlot compares
    // it), mSeesAll (+0x465: toggled by EEMMakeWorldVisible), mSaveVisMap (+0x466: saved; checked when a unit is
    // removed), mObserverMode (+0x467: chat everybody / observer)

    // Layout (no gaps; sizeof 0xb80: EEHumanPlayer adds a UWideString at +0xb80, sizeof 0xb94):
    // the ctor 0x161da5 (member ctors), its Reset 0x165813 (the array loops: 20, 4, 17, 10, 10, 17, 17 elements) and
    // Load/Save. Container element types and every unnamed member are provisional.
    EECivilization mCivilization;           // +0x468 (ctor 0x12c57d; provisional name)
    EEPlayerStatistics mSavedGroups;        // +0x4a8 (the player's statistics; provisional name)
    UString mCivilizationFile;              // +0x7ac (saved)
    // port (the game core lane, round 5): the private lists (the stranded units, the units to repatrol, the loaded
    // and saved ids, the media-loaded classes, the loaded media, the off-map records) are a Rust object
    // (ee_units player_lists.rs; made by the constructors, freed by the destructor). The towns by id (written,
    // never read) and the third selected group (made, never used) are gone. The player/tech lane, round 2: the
    // other containers too (mTowns +0x7cc, mHerds +0x7dc, mMediaWarnings +0x824, mSightSharers +0x834,
    // mPatrolGroups +0x874, mAllies +0x894, mConstructionSites +0x8b4, mWonders +0x8f8: EE_UNITS_LISTS_*)
    EeUnitsPlayerLists* mLists;
    // port (the groups lane, round 2): the running (+0x844: AddPendingUnits copies the pending ones into it), pending
    // (+0x854: the command messages push their new EEActiveGroups) and finished groups (+0x864: EEActiveGroup::Finish
    // pushes itself) are a Rust object (ee_units group_obj.rs EeUnitsPlayerGroups; made by the constructors, freed by
    // the destructor); the C++ reads them through these
    EeUnitsPlayerGroups* mGroupLists;
    unsigned long GetNumRunningGroups() { return ee_units_player_groups_len(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_RUNNING); }
    EEGroup* GetRunningGroup(unsigned long inIndex) { return static_cast<EEGroup*>(ee_units_player_groups_at(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_RUNNING, inIndex)); }
    unsigned long GetNumPendingGroups() { return ee_units_player_groups_len(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_PENDING); }
    void AddPendingGroup(EEGroup* inGroup) { ee_units_player_groups_push(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_PENDING, inGroup); }
    std::vector<EEUnitClassUpgrade*> mUnitClassUpgrades; // +0x884 (resized to EEUnitClassCount() by Reset: one per unit class; the forwarders
                                            // 0x163db9.. index it
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
    EETechTree* mTechnologies;       // +0x9cc (provisional name; conscription 0xafe3b; Reset: new 0x554 bytes)
    RTSUnit* mViewedUnit;                   // +0x9d0 (Reset dereferences it)
    RTSUnit* mSelectedViewedUnit;           // +0x9d4 (a referenced unit: SetSelectedViewedUnit swaps it)
};
typedef EEPlayer RTSPlayer;

// EEWorld::ApplyShareBonus (false) and RemoveShareBonus (true): the share bonus of every player by its wonder powers and
// stances (ee_units player_units.rs; EEPlayer.cpp). The player lane, round 2: EEPlayer's mWonderTypeCounts and
// mWonderPowers are its Rust wonder state (mWonderState)
void EEPlayerShareBonus(bool inRemove);
#endif
