// C:\EEX\Empire Earth\Units\EEUnit.h (RECONSTRUCTION; file name inferred). A unit (RTTI EEUnit > RTSUnit >
// UReferenceCounter; vtable 0x447a20, sizeof 0x48, ctor 0x228cd2; cluster 0x228c56) and the complex unit most game
// code works with (RTTI EEComplexUnit > EEUnit; vtable 0x446ddc, sizeof 0x364, ctor 0x1f5999). Virtual slots come
// from the vtables; unknown ones are named by offset. Only what the decompiled units use;
// names are provisional. Requires lle/LowLevelEngine.h (lle/GETransformation.h), ee/EEAreaEffect.h.
#ifndef EE_EEUNIT_H
#define EE_EEUNIT_H

#include "ee/EEAreaEffect.h"            // EEComplexUnit holds an EEUnitAreaEffectList by value
#include "ee/EEEffectManager.h"         // EEComplexUnit holds an EEEffectHandle by value (+0x310)
#include <deque>
#include <algorithm>
#include <vector>
#include <utility>
#include "lle/UList.h"                  // EEComplexUnit+0xc4
#include "lle/U2DPoint.h"               // EEComplexUnit+0x1a4
#include "lle/GEColor.h"                // EEComplexUnit+0x1ac
#include "ee/EEUnitClassIDs.h"
#include "ee/EEResourceType.h"         // kResourceAny (CanTake)
class GETerrainMesh;
#include "lle/GEPhysicalModel.h"        // EEComplexUnit+0x130 (it uses GE3DLine/GEViewport by reference)

class EEComplexUnit;
class EEHerd;                               // ee/EEHerd.h (EEComplexUnit+0x200)
struct EEPUnitSwap;                         // ee/EEPhysics.h (ReplaceUnits)
template <class T> class UMemoryPool;   // lle/UMemoryPool.h (EEComplexUnit::sMemoryPool)
class EEGraphicEffect;
class SESMInstanceNode;                 // lle/SESMInstanceNode.h (EEComplexUnit::SetSound)
class EEGraphicObject;                      // ee/EEGraphicObject.h
typedef EEGraphicObject EEModel;            // (see below: WriteDetailsHook's)
class EEActiveGroup;
struct EECPASGFlags;                        // ee/EECPAttackSubGroup.h (EEUnitClass::GetTerrainFlags)
class EETechTreeNode;
class EEUnit;
typedef struct EeUnitsPatrolGroup EEPPatrolGroup;
class EETown;
#include "ee/EEUnitAction.h"             // EEComplexUnit::mCommand (+0x1e8; GetCommandType reads its mType inline)
class EEUnitBehavior;                      // ee/EEUnitBehavior.h (EEComplexUnit+0x208, EEUnitClass+0x118)
#include "ee/EEUnitVectorItem.h"        // the element of EEComplexUnit::mGoalQueues (inline GetQueueGoals, the destructor)
class GEModel;
#include "ee/EETileCoordinate.h"     // RTSUnit::GetTile returns one
class EEPlayer;
class EEUnitClass;
class EEUnitGoal;
class EEAIContinent;                        // ee/EEAIContinent.h (EEUnitClass::GetContainerContinent)
class EEPhysicsModel;                       // ee/EEPhysicsModel.h (EEUnitClass +0x110)
class EESoundObject;

#include "ee/EEUnitClassImprovement.h"
#include "ee/EEUnitKind.h"               // EEClassBody (EEUnitClass::IsBuilding, Virtual70)
#include "ee/EEUnitUpdate.h"             // EEUnitClassActionChanged (EEUnitClass::HandleActionChanged)
#include "rust/sim/units.h"              // EeUnitsUnitClassUpgradeFields (EEUnitClassUpgrade's base)
#include "rust/sim/cp.h"                 // ee_cp_distance_squared_to_unit (DistanceSquaredToUnit)
#include "rust/sim/astar.h"              // ee_astar_cm_footprint_continent (GetFootprintContinent)
#include "rust/sim/combat.h"             // EE_COMBAT_SHOT_* (the shots' steps)

// What +0x34 of a unit points to: the owner's RTTI EEUnitClassUpgrade for the unit's class (vtable 0x449ad8, sizeof
// 0x16c, ctor 0x23b6ae(EEUnitClass*); EEPlayer's Reset news one per gUnitClassTable entry into its vector at +0x884).
// Proven: every method called on it (0x23b82b, 0x23be6f..0x23c185) sits in EEUnitClassUpgrade.cpp
// 0x23b61a..0x23c332, and +0x04 is the embedded EEUnitClassImprovement (GetLevel on &mStats->+4). Layout
// gap-free from the ctor, the dtor 0x23b729 (deletes +0x68..+0x80 through slot +0x08 when mL154 > 0) and Init
// 0x23b891 (copies from mUnitClass; `layout.py EEUnitClassUpgrade`); member types by those stores, (provisional names)
// (the names users rely on are kept). EEUnitStats stays as a typedef for the old spelling.
// New (R5b.4, round 28): the plain members (the stats, their inverses, the counts and flags: names, types and order
// kept) are the Rust record EeUnitsUnitClassUpgradeFields (ee_units stats.rs), read in place by the Rust readers;
// Init's copies and SetMaxVelocity are Rust. The volatile busy flags stay here.
class EEButtonObject;
class FSFileSpec;
class EEUnitClassUpgrade : public EeUnitsUnitClassUpgradeLinks, public EeUnitsUnitClassUpgradeFields {
public:
    EEUnitClassUpgrade(EEUnitClass* inUnitClass);   // 0x23b6ae (then Init)
    virtual ~EEUnitClassUpgrade() { this->Media(EE_UNITS_UPGRADE_MEDIA_DESTROY, 0, NULL, NULL); }  // +0x00 dtor 0x23b729 (??_G 0x23b70d)
    virtual void CopyUpgradeLevels(EEUnitClassUpgrade* inOther) { this->mImprovement.CopyLevelsTo(inOther, inOther->GetImprovement()); }  // +0x04 0x23b7a4 (forwards to mImprovement)

    // the media bodies (ee_units upgrade_media.rs) over the eight media members (mButton .. mSelectionSounds[1],
    // contiguous), the busy flags and the record's counts
    uint64_t Media(int32_t inOp, long inArg, EEPlayer* inPlayer, void* inValue)
    {
        EeUnitsUpgradeMedia theMedia = {reinterpret_cast<void**>(&this->mButton), const_cast<bool*>(&this->mIsLoadingMedia),
                                        static_cast<EeUnitsUnitClassUpgradeFields*>(this), this, inPlayer};
        return ee_units_upgrade_media(&theMedia, inOp, inArg, inValue);
    }
    void SetName(UWideString inName) { this->mName = inName; }  // inline COMDAT 0x1bcb00
    void Init() { ee_units_uc_rest(EE_UNITS_UC_DO_R_UPGRADE_INIT, NULL, NULL, (uint64_t)(uintptr_t)this, 0, 0, NULL); }  // 0x23b891 (the fields below from mUnitClass)
    EESoundObject* GetSelectionSound(long inIndex) { return reinterpret_cast<EESoundObject*>(this->Media(EE_UNITS_UPGRADE_MEDIA_SELECTION_SOUND, inIndex, NULL, NULL)); }  // 0x23b82b (one of mSelectionSounds; EEMSavedGroupSelect passes -1 for EEPlayForLocalPlayer; provisional name)
    EESoundObject* GetAcknowledgeSound(long inIndex) { return reinterpret_cast<EESoundObject*>(this->Media(EE_UNITS_UPGRADE_MEDIA_ACKNOWLEDGE_SOUND, inIndex, NULL, NULL)); }  // 0x23b806 (one of mAcknowledgeSounds; EEPlayer::PlayAcknowledgement passes -1 for EEPlayForLocalPlayer; provisional name)
    // the per-class forwarders' targets (EEPlayer 0x163db9..0x163e71; provisional names)
    void LoadButtons(EEPlayer* inPlayer) { this->Media(EE_UNITS_UPGRADE_MEDIA_LOAD_BUTTONS, 0, inPlayer, NULL); }  // 0x23bc29 (EEPlayer::LoadUnitClassButtons; provisional name)
    void LoadMedia(EEPlayer* inPlayer, long inMediaFlags) { this->Media(EE_UNITS_UPGRADE_MEDIA_LOAD, inMediaFlags, inPlayer, NULL); }  // 0x23bcc3 (EEPlayer::SetUnitClassMedia; provisional name)
    void UnloadButtons(EEPlayer* inPlayer) { this->Media(EE_UNITS_UPGRADE_MEDIA_UNLOAD_BUTTONS, 0, inPlayer, NULL); }  // 0x23c1ef (EEPlayer::UnloadUnitClassButtons; provisional name)
    // EEUnitClassUpgrade.cpp (provisional names)
    EEUnitClassImprovement* GetImprovement() { return &this->mImprovement; }  // 0x23b827 (&mImprovement; defined after its first user Virtual04, so called)
    void DumpMediaCount(FSFileSpec& inFile);  // 0x23b7c1 (the media-count dump line)
    void ChangeLevel(long inImprovement, long inDelta) { this->mImprovement.ChangeLevel(this->mUnitClass, this, inImprovement, inDelta); }  // 0x23b87a (mImprovement.ChangeLevel)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_units_improvement(EE_UNITS_IMPROVEMENT_DO_UPGRADE_LOAD, this->mImprovement.mLevels, &this->mImprovement.mTotalCost, EEImprovementContext{&this->mImprovement, this->mUnitClass, this, &inFile}, &EEImprovementHost, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion); }   // 0x23bbf9 (mName, then mImprovement)
    void SetMaxVelocity(float inValue);    // 0x23c005 (EEUnitClassImprovement::ChangeLevel: improvement 14) (provisional name)
    void SetRange(float inValue);          // 0x23c101 (EEUnitClassImprovement::ChangeLevel: improvement 11) (provisional name)
    void UnloadMedia(EEPlayer* inPlayer) { this->Media(EE_UNITS_UPGRADE_MEDIA_UNLOAD, 0, inPlayer, NULL); }  // 0x23c24f (EEPlayer::UnloadUnitClassMedia; provisional name)
    EEUnitClass* GetUnitClass() { return this->mUnitClass; }  // 0x1f1f81 (mUnitClass; ICF-shared; EEScenarioManager::Prepare's trace) (provisional name)
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_units_improvement(EE_UNITS_IMPROVEMENT_DO_UPGRADE_SAVE, this->mImprovement.mLevels, &this->mImprovement.mTotalCost, EEImprovementContext{&this->mImprovement, this->mUnitClass, this, &outFile}, &EEImprovementHost, 0, 0); }   // 0x23be43 (EEPlayer::Save; provisional name)
    void SetAcknowledgeSound(EESoundObject* inSound, long inIndex) { this->Media(EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_ACKNOWLEDGE + inIndex, NULL, inSound); }  // 0x23be6f (mAcknowledgeSounds[inIndex])
    void SetCreationSound(EESoundObject* inSound) { this->Media(EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_CREATION, NULL, inSound); }  // 0x23bed9 (mCreationSound)
    void SetDeathSound(EESoundObject* inSound) { this->Media(EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_DEATH, NULL, inSound); }  // 0x23bf3d (mDeathSound)
    void SetModel(EEModel* inModel) { this->Media(EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_MODEL, NULL, inModel); }  // 0x23bfa1 (mModel)
    void SetButton(EEButtonObject* inButton) { this->Media(EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_BUTTON, NULL, inButton); }  // 0x23c09d (sets the button +0x64: Release on the old, Acquire on the new)
    void SetSelectionSound(EESoundObject* inSound, long inIndex) { this->Media(EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_SELECTION + inIndex, NULL, inSound); }  // 0x23c185 (mSelectionSounds[inIndex])
    // the improvement button's help text (EEUIBUpgrade 0x256c83; provisional names)
    void GetImprovementName(UWideString& outName, long inImprovement) { this->mImprovement.GetName(this->mUnitClass, inImprovement, outName); }
    void GetImprovementGainText(UWideString& outText, long inImprovement, long inLevel) { this->mImprovement.GetGainText(this, inLevel, inImprovement, outText); }
    // inline: EETechTreeNode::GetCost loads it first (fld; fimul; provisional name)
    float GetCostScale() const { return this->mCostScale; }
    // inline: EEUGAttackUnit::EvaluateCondition keeps it in a temporary across the GetLineOfSight call (0x21dce8)
    float GetRange() const { return this->mRange; }
    float GetScale() const { return this->mScale; }  // inline (EESGUnitSorter::Compare loads both operands)
    float GetAttack() const { return this->mAttack; }  // inline (likewise)
    long GetMaxHitPoints() const { return this->mMaxHitPoints; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    // inline (EEWheel::Update 0x23c957 loads them as values) (provisional names)
    float GetMaxVelocity() const { return this->mMaxVelocity; }
    float GetInverseMaxAngularVelocity() const { return this->mInverseMaxAngularVelocity; }
    float GetInverseMaxVelocity() const { return this->mInverseMaxVelocity; }
    float GetAcceleration() const { return this->mAcceleration; }  // inline (EEPMMissile::Accelerate 0x55b041 loads it as a value) (provisional name)
    float GetLineOfSight() const { return this->mLineOfSight; }  // inline (EEActiveGroup::SpreadAroundUnit loads it after GetRange as a value) (provisional name)

    EEUnitClassImprovement mImprovement;    // +0x04 (ctor 0x23a5bf)
    UWideString mName;                      // +0x4c (= "" in the ctor)
    // +0x60 mUnitClass, +0x64 mButton .. +0x7c mSelectionSounds[1]: the Rust record EeUnitsUnitClassUpgradeLinks (a base)
    // +0x84 mAreaEffectScale .. +0x168 mIsInitialized: the record EeUnitsUnitClassUpgradeFields (the base above)
    float GetAngularAcceleration() const { return this->mAngularAcceleration; }  // inline (EEActiveGroup::ComputeValues 0x5ed692 copies it through fld/fstp)
    float GetMaxAngularVelocity() const { return this->mMaxAngularVelocity; }  // inline (mFAC / mFB4 sites read both as values)
    float GetCameraFocus() const { return this->mCameraFocus; }  // inline (EECameraControl::GetUnitPoint 0x52844f: fld; fmulp) (provisional name)
    unsigned long GetMediaCount() const { return this->mMediaCount; }  // inline (EEPlayer::LoadAllClassMedia 0x167c16 compares it unsigned: jbe) (provisional name)
    bool GetIsInitialized() const { return this->mIsInitialized; }
    volatile bool mIsLoadingMedia;          // +0x169 (ctor: false; a busy flag polled with Sleep(1): volatile, 0x63bcc3)
    volatile bool mIsLoadingButtons;        // +0x16a (ctor: false; 0x23bc29 sets it while loading the media and the
                                            //  other loaders poll it with Sleep(1): `mov byte ptr`, re-read each pass)
};
typedef EEUnitClassUpgrade EEUnitStats;

// What EEUnit::GetModel returns (mStats +0x68): the unit class's EEGraphicObject (ee/EEGraphicObject.h; slots 0/4/8
// are EEMedia's, its +0x244 part is mModels[0]). EEModel was its provisional name here (merged into
// EEGraphicObject.h; the typedef is at the top of this header)

#include "ee/RTSUnit.h"                // RTSUnitLink, RTSUnit (the RTS layer's header since R5a.6)

// a tile Rust hands back packed (x in the low 32 bits, y in the high: FindAreaTile)
inline EETileCoordinate EEUnpackTile(int64_t inPacked) { return EETileCoordinate((long)(int32_t)(uint32_t)inPacked, (long)(int32_t)(uint32_t)((uint64_t)inPacked >> 32)); }

// New (R5b.4, round 26): the plain members (mResourceType, mHitPoints, mResourceAmount, mHiddenFlags: their names, types
// and order) are the Rust record EeUnitsUnitFields (ee_units unit_fields.rs), the second base after RTSUnit (still at
// offset 0). The unit store reads the record directly.
class EEUnit : public RTSUnit, public EeUnitsUnitFields {
public:
    unsigned long GetUnitID() const { return this->mUnitID; }
    // inline: the cloaking calamity loads both into registers before it pushes them (0xafd92)
    long GetTileX() const { return this->mTileX; }
    long GetTileY() const { return this->mTileY; }
    // inline (out-of-line COMDAT 0x8203 in EECPASGUMechProphet.cpp: [ret] = mTileX, [ret + 4] = mTileY, `ret 4`;
    // EEMBuildUnit::Execute 0xbbcc4) provisional name and spelling
    EETileCoordinate GetTile() const
    {
        // the constructed return value (COMDAT 0x408203 stores X before it loads Y)
        return EETileCoordinate(this->mTileX, this->mTileY);
    }
    EEComplexUnit* GetComplexUnit() const { return this->mComplexUnit; }  // inline (drop object 0xb0121)
    EEUnitClass* GetClass() const { return this->mUnitClass; }  // inline (named by EEUGAttackUnit::SetTarget's asserts)
    // inline: EEMLockUnlock loads +0x2c before its dynamic_cast pushes
    EEUnitClassUpgrade* GetStats() { return this->mStats; }  // inline COMDAT 0x1bcb1b
    EEUnitClass* GetUnitClass() const { return this->mUnitClass; }
    // inline (the logic bomb's Update: with the other accessors they make VC6 refuse its earlier nested
    // GE3DPoint::operator=, as the original does)
    long GetHitPoints() const { return this->mHitPoints; }
    EEPlayer* GetPlayer() const { return this->mPlayer; }
    RTSUnitLink* GetTileNode() const { return this->mTileNode; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    long GetResourceAmount() const { return this->mResourceAmount; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    unsigned long GetResourceType() const { return this->mResourceType; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    RTSTile* GetRTSTile() const { return this->mTile; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    // inline setters (`if (m != v) m = v`; EEUCResource::SimpleUnitCreated) (provisional names)
    void SetResourceType(unsigned long inValue) { if (this->mResourceType != inValue) this->mResourceType = inValue; }
    void SetResourceAmount(long inValue) { if (this->mResourceAmount != inValue) this->mResourceAmount = inValue; }
    // inline in the real header (out-of-line COMDAT 0x9d5e: mPlayer->mTechnologies->Get(mUnitClass's +0x1e4)); body not
    // reconstructed yet, so declared out of line here (the logic bomb calls it out of line) (provisional name)
    EETechTreeNode* GetTechTreeNode();
    float GetNearestFootprintTile(EEUnit* inUnit, long* outX, long* outY) { return ee_units_eu_nearest_footprint_tile(this, inUnit, outX, outY); }  // 0x229314; base-unit footprint distance
    float GetTileDistanceSquared(EEUnit* inOther) { return ee_units_atc_tile_distance_squared(this, inOther); }  // 0x14f455 (EEAirTrafficControl.cpp: the squared tile distance; provisional name)
    bool IsPathMarker();                    // 0x708c9 (EECPCivilizationManager.cpp; every caller out of line): unit type 0x104 or 0xdf (provisional name)

    // the RTSUnit slots EEUnit overrides (EEUnit.cpp)
    virtual ~EEUnit() { ee_units_eu_more(EE_UNITS_EUM_DO_DESTROY, this, NULL, 0, 0); }                      // +0x00 0x228d36 (calls Virtual30; ??_G 0x228d1a)
    virtual void Invalidate() { EEUnit::Remove(false); }              // +0x04 0x228cf6: EEUnit::Remove(false)
    // +0x08 0x228ef5: the stats, hit points (scaled to this unit's maximum) and mL40 of inUnit (an EEUnit)
    virtual void CopyFrom(RTSUnit* inUnit) { ee_units_eu_more(EE_UNITS_EUM_DO_COPY_FROM, this, inUnit, 0, 0); }
    virtual void SightOnHere() {}             // +0x0c (ICF-folded empty `ret` 0x1720c6)
    virtual void Describe(UWideString& ioString) { ee_units_eu_describe(this, &ioString, false); }   // +0x10 0x2293d4 ("res %d / %d:", "obj %d:", "hp %d:")
    virtual void DescribeLong(UWideString& ioString) { ee_units_eu_describe(this, &ioString, true); }  // +0x14 0x22946f ("Health: %d / %d (%.2f%%), newline)
    virtual void SightOffHere() {}            // +0x24 (ICF-folded empty `ret` 0x1720c6)
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion) { return RTSUnit::RepairPointers(inMajorVersion, inMinorVersion); }  // +0x28 0x2299d5: RTSUnit::RepairPointers (folded at 0x27cf38)
    virtual void Clear() { ee_units_eu_more(EE_UNITS_EUM_DO_CLEAR, this, NULL, 0, 0); }                   // +0x20 0x228d05: ResetEEUnit, Reset, mIsValid = false
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return EEUnitFile(true, this, 1, inMajorVersion, inMinorVersion, inFile); }   // +0x1c 0x2298fb
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return EEUnitFile(false, this, 1, inMajorVersion, inMinorVersion, outFile); }  // +0x2c 0x2299e5
    virtual void FreeNode() { RTSUnit::FreeNode(); }                // +0x30 0x2292c4 (RTSUnit::FreeNode)
    virtual void LastDereference() { ee_units_eu_more(EE_UNITS_EUM_DO_LAST_DEREFERENCE, this, NULL, 0, 0); }         // +0x34 0x2298e1 (Virtual30, the count = 0, delete this)
    // +0x38 0x228e7f: unloads the class media (now, or through gDataPreLoading when inArg and not shutting down),
    // hit points 0, the complex unit out of gWorld, RTSUnit::Invalidate (provisional name)
    virtual void Remove(bool inUnloadInBackground) { ee_units_eu_more(EE_UNITS_EUM_DO_REMOVE, this, NULL, inUnloadInBackground, 0); }
    virtual void SightOn(unsigned long, unsigned long) {}      // +0x3c
    virtual bool UpdateHidden() { return ee_units_update_hidden((uint64_t)(uintptr_t)this); }            // +0x40 (EERenderObject 0x10f44f tests al; both classes' body: ee_units hidden.rs)
    virtual void OutOfSynchDump(FILE*, unsigned long) {}  // +0x44 (the out-of-synch dump: EEPhysics 0x15234c)
    // +0x48 0x2292c9 (`ret 4`, returns bool; reads the argument's mStats +0x160 bit 14): EEMPathTo pushes a unit and
    // tests al; EEComplexUnit overrides it (0x1f9e81, ret 4). Name is provisional
    virtual bool IsPassableBy(EEComplexUnit* inUnit);
    virtual long GetChecksum(unsigned long) { return 0; }  // +0x4c
    // +0x50 0x229550: the unit's tile, or for a class bigger than one tile not on a continent with mCategory == inFlag,
    // the first tile within inRange of its square that is (inferred types)
    virtual EETileCoordinate FindAreaTile(bool inFlag, long inRange) { return EEUnpackTile(ee_units_eu_body(EE_UNITS_EU_DO_FIND_AREA_TILE, this, inFlag, inRange, 0)); }
    // +0x54: mStats's +0x68 (EEComplexUnit 0x1fa473: or a lookup when +0x351 is set; the logic bomb reads its
    // +0x244 part) (provisional name)
    virtual EEModel* GetModel() { return this->mStats->mModel; }
    virtual bool IsFoundation() { return false; }            // +0x58
    virtual void GetPosition(GE3DPoint& outPosition);         // +0x5c (the calamities read the caster's)
    virtual void Kill(bool inKilled, bool inKeepResearch, bool inOther) { ee_units_eu_body(EE_UNITS_EU_DO_KILL, this, inKilled | inKeepResearch << 1 | inOther << 2, 0, 0); }  // +0x60 0x22975a (bytes: cmp byte, or dl) (the teleport calamity: (1, 0, 0) when no place is found)
    // +0x64 0x22971e: loads the class media (now, or through gDataPreLoading when inArg) (provisional name)
    virtual void LoadMedia(bool inIsFoundation) { ee_units_eu_more(EE_UNITS_EUM_DO_LOAD_MEDIA, this, NULL, inIsFoundation, 0); }
    virtual void SightOff(unsigned long, unsigned long) {}     // +0x68
    // +0x6c 0x229973 (2 arguments: EEPlayer::FinishUnitsLoad 0x1690a5): after Load, the saved hit-point
    // fraction (Load memcpy's the float into mHitPoints) back to hit points; returns 0
    virtual long FinishLoad(long, long);
    // +0x74: damage from inAttacker for player inPlayerNumber (the contagion functors; provisional names)
    virtual void TakeHit(EEUnit* inAttacker, long inDamage, unsigned long inPlayerNumber, long inMode) { ee_units_eu_take_hit(this, inAttacker, inDamage, (uint32_t)inPlayerNumber, inMode); }
    virtual void SightMoved(unsigned long, unsigned long, unsigned long, unsigned long) {}  // +0x78

    EEUnit() { ee_units_eu_more(EE_UNITS_EUM_DO_CONSTRUCT, this, NULL, 0, 0); }                               // 0x228cd2: RTSUnit(), the vptr, Reset() twice over, then the count = 0
    void ResetEEUnit() { ee_units_eu_more(EE_UNITS_EUM_DO_RESET, this, NULL, 0, 0); }                     // 0x229706: +0x2c..+0x44 = 0 (provisional name)
    // 0x228d57: whether this unit can act on inUnit at (inX, inY) (the mining building's mine search) (provisional
    // name)
    // (inArg4/inArg5 read as bytes: bool)
    bool CanGather(EEUnit* inUnit, long inX, long inY, bool inIsVisible, bool inIgnoreAmount) { return ee_units_eu_can_gather(this, inUnit, inX, inY, inIsVisible, inIgnoreAmount) != 0; }
    void DepleteResource(long inAmount) { ee_units_eu_body(EE_UNITS_EU_DO_DEPLETE_RESOURCE, this, inAmount, 0, 0); }    // 0x229d73 (the mining building's mine, 1) (provisional name)
    void RegrowForest(EEComplexUnit* inUnit) { ee_units_eu_more(EE_UNITS_EUM_DO_REGROW_FOREST, this, inUnit, 0, 0); }  // 0x22a3fd (EEUGGather::Work when the resource's class mIsTree) (provisional name)
    void AddResourceAmount(long inValue) { this->mResourceAmount += inValue; }   // 0x228d4d: mL40 += inValue (provisional name)
    // 0x229278 / 0x2292ad (cdecl): a new EEUnit, then Init (provisional names)
    static EEUnit* Create(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer);
    static EEUnit* Create() { return new EEUnit; }
    void Init(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer) { ee_units_eu_more(EE_UNITS_EUM_DO_INIT_AT, this, inClass, (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32), (int64_t)(uintptr_t)inPlayer); }
    void Init(EEUnitClass* inClass, EEPlayer* inPlayer) { ee_units_eu_more(EE_UNITS_EUM_DO_INIT_COMPLEX, this, inClass, 0, (int64_t)(uintptr_t)inPlayer); }   // 0x2296b3 (mComplexUnit = this) (provisional name)
    void SetHitPoints(long inHitPoints) { ee_units_eu_body(EE_UNITS_EU_DO_SET_HIT_POINTS, this, inHitPoints, 0, 0); }
    void MoveTo(long inX, long inY, bool inUpdateTiles) { ee_units_eu_body(EE_UNITS_EU_DO_MOVE_TO, this, inX, inY, inUpdateTiles); }  // 0x229ac3 (Init's last call; moves the unit to a tile: regions, influence, mini map) (provisional name)
    // 0x22a4c3 (cdecl): unit types 0x24b, 0x24c, 0x2fb, 0x306, 0x307, 0x340..0x345, 0x348 (0x22a3fd skips them)
    // (provisional name)
    static bool IsCarbonRock(unsigned long inUnitType) { return inUnitType == 0x2fb || inUnitType == 0x306 || inUnitType == 0x307 || (inUnitType >= 0x340 && inUnitType <= 0x345) || inUnitType == 0x348 || inUnitType == 0x24b || inUnitType == 0x24c; }

    // mUnitClass (+0x2c), mComplexUnit (+0x30: the complex unit this unit belongs to) and mStats (+0x34): the record
    // EeUnitsUnitFields (ee_sim world unit_fields.rs), with
    // +0x38 mResourceType (saved), +0x3c mHitPoints (> 0: alive), +0x40 mResourceAmount (saved), +0x44 mHiddenFlags
    // (0x10: MoveTo moved it; 0x20 and the low 2 bits: Virtual40 0x228f90): EeUnitsUnitFields
};

// EEComplexUnit::mWaypoints's element: a waypoint of the unit's path (0x14 bytes: AddWaypoint builds one from a
// GE3DPoint, the path length so far and 0; GetPathStart/GetPathLength read back().mDistance - front().mF10)
// (provisional names)
struct EEComplexUnitWaypoint {
    GE3DPoint mLocation;                    // +0x00
    float mDistance;                        // +0x0c
    float mStartDistance;                   // +0x10
};
static_assert(sizeof(EEComplexUnitWaypoint) == sizeof(EeUnitsWaypoint), "a waypoint is ee_sim's EeUnitsWaypoint");

// EEComplexUnit::mWaypoints: the path, a Rust list (ee_sim::units waypoints.rs, which has the bodies over it) behind
// the deque members its readers use; an element reference is valid until the list changes. Its first word is the
// list's handle (the world layout's complex_waypoints).
class EEWaypointList {
public:
    EEWaypointList() : mList(ee_units_waypoints_new()) {}
    EEWaypointList(const EEWaypointList& inOther) : mList(ee_units_waypoints_new()) { ee_units_waypoints_assign(this->mList, inOther.mList); }
    ~EEWaypointList() { ee_units_waypoints_free(this->mList); }
    EEWaypointList& operator=(const EEWaypointList& inOther) { ee_units_waypoints_assign(this->mList, inOther.mList); return *this; }
    size_t size() const { return ee_units_waypoints_len(this->mList); }
    bool empty() const { return this->size() == 0; }
    const EEComplexUnitWaypoint& operator[](size_t inIndex) const { return *reinterpret_cast<const EEComplexUnitWaypoint*>(ee_units_waypoints_at(this->mList, inIndex)); }
    const EEComplexUnitWaypoint& back() const { return (*this)[this->size() - 1]; }
    void push_back(const EEComplexUnitWaypoint& inWaypoint) { ee_units_waypoints_push(this->mList, reinterpret_cast<const EeUnitsWaypoint*>(&inWaypoint)); }
    void clear() { ee_units_waypoints_clear(this->mList); }
    EeUnitsWaypoints* mList;
};

// EEComplexUnit::mIgnoredTargets's element (0x18 bytes; ctor 0x1f5d8b out of line right after its first user
// IgnoreTarget: six zero stores): an attacker record, found by its unit id (std::find 0x200eb6 with an unsigned long)
// (provisional names). The ctor's six zero stores are the members' initializers; its out-of-line memberwise copy
// 0x20018a (the deque erase family's) is the implicit one.
// (the struct: the Rust record EEComplexUnitAttacker, ee_sim world unit_fields.rs EeUnitsIgnoredTarget: the attacked
// unit's tile mTileX, mTileY, the attacker's mFromX, mFromY, mUnitID and mTime, gPhysics +0xd0 plus 30000; units round 11)

// A queued research/production order (EEComplexUnit::mBuildQueue; 0x1f5f97 pushes (technology, player flag)): the
// Rust record EEProductionEntry (ee_sim world unit_fields.rs EeUnitsProductionEntry: mTechnology, mFlag; units
// round 11). std::find(mBuildQueue.begin(), mBuildQueue.end(), inTechnology) in IsResearchQueued 0x937dd compares
// the first dword with the value (the find COMDAT 0x963ff).
inline bool operator==(const EEProductionEntry& inEntry, long inTechnology) { return inEntry.mTechnology == inTechnology; }

// New (R5b.4): EEComplexUnit::mPhysicalModel. GEPhysicalModel's layout (no member of its own); its location setters
// report the new location to the unit store (ee/EEUnitStore.h: the record's x, y, z). Not copied: it lives only in
// its unit. A write through a GEPhysicalModel reference would bypass it (none does: USCK compares the location)
class EEUnitStoreModel : public GEPhysicalModel {
public:
    EEUnitStoreModel() {}
    EEUnitStoreModel(const EEUnitStoreModel& inOther) = delete;
    EEUnitStoreModel& operator=(const EEUnitStoreModel& inOther) = delete;
    void SetLocation(const GETransformation& inLocation)
    {
        GEPhysicalModel::SetLocation(inLocation);
        EEUnitStoreMoved(this);
    }
    void SetLocationAndScale(const GETransformation& inLocation, float inScale)
    {
        GEPhysicalModel::SetLocationAndScale(inLocation, inScale);
        EEUnitStoreMoved(this);
    }
    void SetLocationXY(float inX, float inY)
    {
        GEPhysicalModel::SetLocationXY(inX, inY);
        EEUnitStoreMoved(this);
    }
    void SetLocationXYZ(const GE3DPoint& inPoint)
    {
        GEPhysicalModel::SetLocationXYZ(inPoint);
        EEUnitStoreMoved(this);
    }
};

// 0x241419 (cdecl): the model a unit of inClass shows for inPlayer (EEComplexUnit::GetModel when +0x351 is set;
// provisional name)
EEModel* EEGetPlayerClassGraphic(EEPlayer* inPlayer, EEUnitClass* inClass);

// New (R5b.4, round 26): the plain members from mHeight (+0x210) to mRoll (+0x360) and the port's
// mHasShiftQueuedBuild (their names, types and order; not mSound and the three effect handles, which stay here) are the
// Rust record EeUnitsComplexUnitFields (ee_units unit_fields.rs), the second base after EEUnit (RTSUnit still at offset
// 0). The unit store reads the record directly.
class EEComplexUnit : public EEUnit, public EeUnitsComplexUnitFields {
public:
    // the save keys and the pool (EEComplexUnit.cpp's `$E` chains 0x1f5805..0x1f5954, one guard byte 0x963210;
    // definition order as listed). Provisional names (from the keys)
    // 0x963198, bit 0x20 (ctor 0x1ffe45 stores UMemoryPool<EEComplexUnit>'s vtable; dtor 0x121790)
    static UMemoryPool<EEComplexUnit> sMemoryPool;
    // inline: the calamities load it into a register before their GetCost() call (EECalamityClass 0xadf12, cloaking
    // 0xafd57)
    long GetPower() const { return this->mPower; }
    EEUnitGoal* GetGoal() const { return this->mGoalState; }  // inline (EEMAddPatrolPoint)
    // inline: the load goals load +0x1ec before their dynamic_cast pushes (EEUGLoadContainer 0x226243) (provisional
    // name)
    EEUnitGoal* GetLoadGoal() const { return this->mActiveGoal; }
    // inline in the real header: the out-of-line COMDAT 0x5f27 sits in EECPASGUAirBomber.cpp, the
    // first of its 32 callers in link order. The squared map distance to inUnit, y difference first. Name is
    // provisional
    float DistanceSquaredTo(EEComplexUnit* inUnit) { return ee_units_cu_distance_squared(&this->mPhysicalModel.GetLocation().mMatrix, &inUnit->mPhysicalModel.GetLocation().mMatrix); }   // u12a_small.rs
    void SetPower(long inValue) { if (this->mPower != inValue) this->mPower = inValue; }  // inline (the cast-calamity goal's cost)

    virtual bool IsNextToTile(long inX, long inY) { return ee_units_cu_body(EE_UNITS_CU_DO_IS_NEXT_TO_TILE, this, inX, inY, 0) != 0; } // original1fab56 ret8/AL; sole footprint caller
    virtual bool StepAside(EEPlayer* inPlayer, EEUnitClass* inClass, long inX, long inY, bool inForce) { return ee_units_cu_step_aside(this, inPlayer, inClass, inX, inY, inForce) != 0; }               // +0x80
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew) { ee_units_cu_body(EE_UNITS_CU_DO_REPLACE_UNIT, this, (int64_t)(uintptr_t)inOld, (int64_t)(uintptr_t)inNew, 0); }  // +0x88 (ret 8: inNew replaces inOld as cargo/carrier)

    void StartAction(long inAction) { ee_units_cu_body(EE_UNITS_CU_DO_START_ACTION, this, inAction, 0, 0); }
    // the goal at the queue's front (the front kept or dropped with its auto waypoints); ee_units goals.rs set_goal
    void SetGoal(EEUnitGoal* inGoal, bool inKeepCurrent, long inArg3) { ee_units_unit_set_goal((uint64_t)(uintptr_t)this, (uint64_t)(uintptr_t)inGoal, inKeepCurrent, (int32_t)inArg3); }
    void PlayEffect(long inTechnology) { ee_units_cu_body(EE_UNITS_CU_DO_PLAY_EFFECT, this, inTechnology, 0, 0); }         // 0x1fea99 (the conscription calamity passes 0x138d) (provisional name)
    void ClearGoals(long inIndex) { ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_GOALS, this, inIndex, 0, 0); }            // 0x1f88fe (before a calamity hands out a new goal) (provisional name)
    void ToggleJITManufacturing() { ee_units_cu_body(EE_UNITS_CU_DO_TOGGLE_JIT, this, 0, 0, 0); }          // 0x1fd8f3 (EEMJITManufacturing, then UpdateJITManufacturing; provisional name)
    void UpdateJITManufacturing() { ee_units_cu_body(EE_UNITS_CU_DO_UPDATE_JIT, this, 0, 0, 0); }
    // 0x1fe7a8 (EESEARotateUnits passes its copy of +0x130 with a new yaw, then 1) (provisional name)
    void SetTransformation(const GETransformation& inTransformation, bool inUpdateTiles) { ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_TRANSFORMATION, this, &inTransformation, inUpdateTiles, 0, inTransformation.GetX(), inTransformation.GetY(), 0.0f); }   // the unit moved, then its model's transformation (u12a_small.rs)
    // inline (EECCDropObject.cpp emits the COMDAT 0xb01d2: its first user calls it out of line) (provisional name)
    void SetProjectileStart(const GE3DPoint& inPosition)
    {
        if (this->mProjectileStart != inPosition)
            this->mProjectileStart = inPosition;
    }
    void SetLauncherClass(EEUnitClass* inClass) { if (this->mLauncherClass != inClass) this->mLauncherClass = inClass; }
    // inline (COMDAT 0xb01ad, first emitted by EECCDropObject.cpp; keeps a reference) (provisional name)
    void SetOwner(EEComplexUnit* inOwner) { ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_OWNER, this, inOwner, 0, 0, 0.0f, 0.0f, 0.0f); }   // u12a_small.rs
    void SetHeight(float inHeight) { if (this->mHeight != inHeight) this->mHeight = inHeight; }  // inline (provisional name)
    void SetIsDeletable(bool inValue) { if (this->mIsDeletable != inValue) this->mIsDeletable = inValue; }  // inline (contagion functors) (provisional name)
    // inline (the fire functor damages a unit only when it is its own outermost carrier) (provisional name)
    // inline (provisional name; Update 0xb0a60, with GetPlayerNumber)
    bool IsCarried() const { return this->mPreviousPart != NULL || this->mNextPart != NULL; }
    EEComplexUnit* GetOutermostCarrier() { return ee_units_cu_outermost_carrier(this); }   // cu_inline.rs
    // 0x1f9df3: the effect of kind inKind in the unit's effect list (+0x100), false if none (provisional name)
    bool GetEffect(long inKind, EEGraphicEffect*& outEffect) { return ee_units_cu_body(EE_UNITS_CU_DO_GET_EFFECT, this, inKind, (int64_t)(uintptr_t)&outEffect, 0) != 0; }
    // the volcano moving a unit off its tiles (provisional names; types inferred)
    long NextWaypoint() { return ee_units_waypoints_next((uint64_t)(uintptr_t)this); }   // 0x1fbdd1 (0: the unit has more waypoints to try)
    long GetPathStart(GE3DPoint& outLocation, float& outLength)   // 0x1fa013 (0: outLocation is the next one)
        { return ee_units_waypoints_start(this->mWaypoints.mList, &outLocation.mX, &outLength); }
    void DropPath() { ee_units_waypoints_drop((uint64_t)(uintptr_t)this); }   // the waypoints and the collision count (waypoints.rs)
    // 0x1fa229 (EEUCSpaceBuilding::HasArrived: where the unit is going; 7 when it has no path: long) (provisional name)
    long GetNextWaypointPoint(GE3DPoint& outLocation) { return ee_units_waypoints_start(this->mWaypoints.mList, &outLocation.mX, NULL); }
    void UpdateWaterState(bool inArg) { ee_units_cu_body(EE_UNITS_CU_DO_UPDATE_WATER_STATE, this, inArg, 0, 0); }      // 0x1f7c47 (mWaterState from the class's +0x2d0 bits and the tile) (provisional name)
    void ApplyAreaEffects() { ee_units_cu_body(EE_UNITS_CU_DO_APPLY_AREA_EFFECTS, this, 0, 0, 0); }                // 0x1f9b72 (the area effects over the largest radius) (provisional name)
    // 0x1f9c9b (ret 0x20): up to inTries points around inTarget away from this unit until inScreen accepts one
    // (EECPAttackSubGroupUnit::Flee's flee) (provisional name)
    bool IsOnOtherContinentThan(EEComplexUnit* inUnit) { return ee_units_cu_body(EE_UNITS_CU_DO_IS_ON_OTHER_CONTINENT_THAN, this, (int64_t)(uintptr_t)inUnit, 0, 0) != 0; }
    // inline (EEUCLandTransport::BadlyHurt 0x217017: const operator[], `lea; mov [eax]`) (provisional name)
    EEComplexUnit* GetCargo(unsigned long inIndex) const { return this->mCargo[inIndex]; }
    void SetMine(EEUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_SET_MINE, this, (int64_t)(uintptr_t)inUnit, 0, 0); }           // 0x1fe1df (keeps a reference; the mining building's mine) (provisional name)
    void UpdateWheels() { ee_units_ucg_sea(EE_UNITS_UCG_DO_UPDATE_WHEELS, NULL, this, 0, 0, 0, NULL); }                      // 0x213c37: EEWheel::Update on each of mAreaEffectTemplate1 wheels at +0x1bc (provisional name)
    long LoadCargo(EEComplexUnit* inCargo, bool inIgnoreRoom, bool inArg3) { return ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, this, (int64_t)(uintptr_t)inCargo, inIgnoreRoom, inArg3); }  // 0x1f5da1 (the load goals: 7 = refused) (provisional name)
    bool CanTake(EEComplexUnit* inCargo, unsigned long inValue) { return this->mResourceType == kResourceAny || this->mResourceType == inValue ? true : false; }    // 0x1f6a1a (mU38 is 1 or inValue) (provisional name)
    bool GarrisonUnit(EEComplexUnit* inCargo) { return ee_units_cu_body(EE_UNITS_CU_DO_GARRISON_UNIT, this, (int64_t)(uintptr_t)inCargo, 0, 0) != 0; }  // 0x1f5ebf (the garrison goal) (provisional name)
    // inline in the real header (out-of-line COMDAT 0x1ed63b, first emitted by EEActiveGroup.cpp): the first goal queued
    // in mGoalQueues[inIndex], 0 if none; declared out of line here (provisional name)
    EEUnitGoal* GetQueuedGoal(unsigned long inIndex)   // the queue's first goal (NULL: none; ee_units goals.rs)
    {
        return (EEUnitGoal*)(uintptr_t)ee_units_unit_queued_goal((uint64_t)(uintptr_t)this, (uint32_t)inIndex);
    }
    EEGoalDeque& GetQueueGoals(unsigned long inIndex) { return this->mGoalQueues[inIndex].mGoals; }  // 0x1fa1cd (EEUICUnitIconBar 0x25ad01) (provisional name)
    // EEUGHealUnit's callees (now ee_combat goal_heal.rs's host operations, EEUGRustCombat.cpp) (provisional names; types inferred)
    void ReleaseHealEffect() { ee_units_cu_small(EE_UNITS_CU_S_DO_RELEASE_HEAL_EFFECT, this, NULL, 0); }               // 0x1f709c (the healee is let go)
    void SetAnimationFrame(unsigned long inFrame) { ee_units_cu_small(EE_UNITS_CU_S_DO_SET_ANIMATION_FRAME, this, NULL, (int64_t)inFrame); }  // 0x1f414d (ret 4): the state's model at frame inFrame (EEComplexDoppelganger.cpp's range) (provisional name)
    void FleeAfterCalamity(long inArg) { ee_units_cu_body(EE_UNITS_CU_DO_FLEE_AFTER_CALAMITY, this, inArg, 0, 0); }     // 0x1fbfdd (EEUnitClass::ActionChanged action 0x7ed passes 0) (provisional name)
    // EEUGAttackUnit.cpp's callees (provisional names; types inferred)
    bool CanReachUnit(EEUnit* inTarget) { return ee_units_cu_small(EE_UNITS_CU_S_DO_CAN_REACH_UNIT, this, inTarget, 0) != 0; }
    bool IsTargetIgnored(EEComplexUnit* inTarget, bool inCheckLocation) { return ee_units_cu_small(EE_UNITS_CU_S_DO_IS_TARGET_IGNORED, this, inTarget, inCheckLocation) != 0; }
    void IgnoreTarget(EEComplexUnit* inTarget) { ee_units_cu_small(EE_UNITS_CU_S_DO_IGNORE_TARGET, this, inTarget, 0); }
    bool IsInReach(EEUnit* inTarget) { return ee_units_cu_tiny(EE_UNITS_CU_T_DO_IS_IN_REACH, this, inTarget, 0, 0, 0.0f, 0.0f, 0.0f) != 0; }   // u12a_small.rs (ee_combat reads.rs in_reach; CBQR records the outcome)
    float GetLineOfSight() { return this->GetStats()->mLineOfSight + this->mSight; }                 // 0x1f9dcb (compared with mStats->mF9C)
    bool IsOnTileBorderingTarget(EEUnit* inTarget) { return ee_units_cu_small(EE_UNITS_CU_S_DO_IS_ON_TILE_BORDERING, this, inTarget, 0) != 0; }
    void AddCenterOffset(float& ioX, float& ioY) { ee_units_cu_more(EE_UNITS_CUM_DO_CENTER_OFFSET, this, 0, 0, 0, ioX, ioY, 0.0f, &ioX, &ioY); }  // 0x1f9fbb (EEUGLoadGarrison::ComputeRendezvous: the entrance point; provisional name)
    bool IsRepairable() { return this->mHitPoints > 0 && this->mHitPoints < this->GetStats()->mMaxHitPoints && this->mIsBuilt; }                    // 0x1fbd5f (can be healed)
    bool CanDelete() { return ee_units_cu_body(EE_UNITS_CU_DO_CAN_DELETE, this, 0, 0, 0) != 0; }                       // 0x1f9e2d (EEMKillUnit: the unit may be deleted; provisional name)
    // EEUGAttackGround.cpp's callees (provisional names; types inferred)
    bool CanReachTile(long inX, long inY) { return ee_units_cu_body(EE_UNITS_CU_DO_CAN_REACH_TILE, this, inX, inY, 0) != 0; }  // 0x1ff18a (can the unit act on that tile)
    void SetAnimation(long inPart, long inFrame, bool inIsAnimationPaused) { ee_units_cu_body(EE_UNITS_CU_DO_SET_ANIMATION, this, inPart, inFrame, inIsAnimationPaused); }
    void SetRandomReloadTimer(long inReloadTime) { ee_units_cu_body(EE_UNITS_CU_DO_SET_RANDOM_RELOAD_TIMER, this, inReloadTime, 0, 0); }
    // inline setters (`if (m != v) m = v`; EEUGAttackGround::Step)
    void SetCurrentPhysicsAction(unsigned long inValue) { if (this->mCurrentPhysicsAction != inValue) this->mCurrentPhysicsAction = inValue; }
    void SetRottingAwayTime(long inValue) { if (this->mRottingAwayTime != inValue) this->mRottingAwayTime = inValue; }
    void UpdateOrbitals() { ee_units_uc_rest(EE_UNITS_UC_DO_R_UPDATE_ORBITALS, NULL, this, 0, 0, 0, NULL); }                    // 0x20e086 (EEUCProjectile::Update, first) (provisional name)
    void SetOpacityChain(float inValue) { ee_units_uca_call(EE_UNITS_UCA_DO_OPACITY_CHAIN, NULL, this, 0, 0, 0, &inValue); }      // 0x216c22 (EEUCProjectile::Update: 1 - z/6 when below the ground) (provisional name)
    void SetInitialContactLocation(const GE3DPoint& inPoint) { if (this->mInitialContactLocation != inPoint) this->mInitialContactLocation = inPoint; }  // 0x215246 (EEUCLandable.cpp) (provisional name)
    void StartProduction() { ee_units_cu_body(EE_UNITS_CU_DO_START_PRODUCTION, this, 0, 0, 0); }
    void ContinueProduction() { if (this->mProductionTime == 0 && this->GetBuildQueueSize() != 0) this->StartProduction(); }  // 0x216d2d (EEUCLandCitizen::Update) (provisional name)
    void UpdateTurrets();                   // 0x20e14c (EEUCSpaceship::Update, after the physics model) (provisional name)
    void UpdateReloadTimer() { ee_units_ucp_call(EE_UNITS_UCP_DO_RELOAD_TIMER, NULL, this, 0, 0); }                 // 0x20e0b2 (EEUCSpaceship::Update) (provisional name)
    void UpdateTargetSearchTimer() { ee_units_ucp_call(EE_UNITS_UCP_DO_TARGET_SEARCH_TIMER, NULL, this, 0, 0); }           // 0x20e0db (EEUCSpaceship::Update) (provisional name)
    void UpdateHealSearchTimer() { ee_units_ucp_call(EE_UNITS_UCP_DO_HEAL_SEARCH_TIMER, NULL, this, 0, 0); }             // 0x20e104 (EEUCBasicPriest::Update: mHealSearchTimer down by 0.001 per ms) (provisional name)
    long UpdateRottingAwayTime() { return (long)ee_units_ucp_call(EE_UNITS_UCP_DO_ROTTING_TIME, NULL, this, 0, 0); }             // New (R5b.4 leaf 6): the classes' kActionUnitRotting countdown (returns the time)
    void UpdateResourcesRotting() { ee_units_ucp_call(EE_UNITS_UCP_DO_RESOURCES_ROTTING, NULL, this, 0, 0); }            // New (R5b.4 leaf 6): the classes' kActionResourcesRotting step
    void SetF220(float inValue) { if (this->mF220 != inValue) this->mF220 = inValue; }
    // inline (EEPMNaval::PushMoving 0x55c1cb: fld; fadd; fcom; je; fstp) (provisional names)
    void SetTargetSearchTimer(long inValue) { if (this->mTargetSearchTimer != inValue) this->mTargetSearchTimer = inValue; }
    void SetIsAnimationPaused(bool inValue) { if (this->mIsAnimationPaused != inValue) this->mIsAnimationPaused = inValue; }  // (EEUnitAI.cpp action 5000)
    void SetAnimationTime(unsigned long inValue) { if (this->mAnimationTime != inValue) this->mAnimationTime = inValue; }  // (EEUnitAI.cpp action 5000)
    // inline setters (EEHerd.cpp 0xa1b45/0xa286a/0xa31fd/0xa3d9c/0xa412e: cmp then store) (provisional name)
    void SetGrazeTime(unsigned long inValue) { if (this->mGrazeTime != inValue) this->mGrazeTime = inValue; }
    void SetGrowTime(unsigned long inValue) { if (this->mGrowTime != inValue) this->mGrowTime = inValue; }
    void SetHerd(EEHerd* inValue) { if (this->mHerd != inValue) this->mHerd = inValue; }  // (the unit's herd)
    void SetGrowth(float inScale) { ee_units_cu_body(EE_UNITS_CU_DO_SET_GROWTH, this, UFloatBits(inScale), 0, 0); }          // 0x1fe49a (EEHerd 0xa31fd: min(animal +0x34 + mGrowth, 1); 0xa3d9c) (provisional name)
    float GetVelocity() const { return this->mVelocity; }  // inline (EEPMMissile::Accelerate 0x55b041: fld; faddp) (provisional name)
    float GetGrowth() const { return this->mGrowth; }  // inline (EEHerd::GrowUnit 0xa31fd loads it as a value) (provisional name)
    // inline setters (EEUGBuildRepair::BeginBuilding)
    void SetWorkTimer(long inValue) { if (this->mWorkTimer != inValue) this->mWorkTimer = inValue; }
    void SetProductionTime(long inValue) { if (this->mProductionTime != inValue) this->mProductionTime = inValue; }
    void FaceTarget(EEUnit* inTarget) { ee_units_cu_more(EE_UNITS_CUM_DO_FACE_TARGET, this, (int64_t)(uintptr_t)inTarget, 0, 0, 0.0f, 0.0f, 0.0f, NULL, NULL); }      // 0x1feeb9 (the build/repair goal's command 0xfa2) (provisional name)
    void SetSight(float inSight) { this->mSight = inSight; }
    // inline (COMDAT 0xb021c, first emitted by EECCDropObject.cpp): +0x21c is kept at least mStats->mBC
    void SetVelocity(float inValue) { ee_units_cu_set_velocity(this, this, inValue); }   // cu_inline.rs
    // inline in the real header (out-of-line COMDAT 0x5e407; 54 callers): +0x1c8's value when set, else mStats->mFAC;
    // body not reconstructed yet, so declared out of line here provisional name (meteor storm)
    float GetMaxVelocity();
    // 0x15463e / 0x15465b / 0x154c95 / 0x154e7b / 0x154f39 (EEPMLand.cpp, ecx = the unit): the group's stat when
    // mActiveGroup is set (EEActiveGroup::GetMaxAngularVelocity ..), else mStats->mFB4 .. (provisional names)
    float GetMaxAngularVelocity() { return ee_units_pm_unit_rate(this, EE_UNITS_PM_RATE_MAX_ANGULAR_VELOCITY); }
    float GetAngularAcceleration() { return ee_units_pm_unit_rate(this, EE_UNITS_PM_RATE_ANGULAR_ACCELERATION); }
    float GetAcceleration() { return ee_units_pm_unit_rate(this, EE_UNITS_PM_RATE_ACCELERATION); }
    float GetInverseAcceleration() { return ee_units_pm_unit_rate(this, EE_UNITS_PM_RATE_INVERSE_ACCELERATION); }
    float GetInverseAngularAcceleration() { return ee_units_pm_unit_rate(this, EE_UNITS_PM_RATE_INVERSE_ANGULAR_ACCELERATION); }
    // 0x1fe76e (the meteor storm passes the meteor's start point, true) (provisional name)
    void SetLocation(const GE3DPoint& inLocation, bool inUpdateTiles) { ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_LOCATION, this, NULL, inUpdateTiles, 0, inLocation.GetX(), inLocation.GetY(), inLocation.GetZ()); }   // the unit moved, then its model's 3D location (u12a_small.rs)
    void SetFlightTime(long inValue) { if (this->mFlightTime != inValue) this->mFlightTime = inValue; }  // inline (meteor storm 0xb29ac)
    void SetOpenFraction(float inValue) { if (this->mOpenFraction != inValue) this->mOpenFraction = inValue; }  // inline (logic bomb 0xb251d)
    // inline (COMDAT 0xb3976, first emitted by EECCTeleport.cpp) (provisional name)
    void Set2DPosition(const U2DPoint<float>& inPoint) { if (this->mU0DC != inPoint) this->mU0DC = inPoint; }
    void TakeOffMap() { ee_units_cu_body(EE_UNITS_CU_DO_TAKE_OFF_MAP, this, 0, 0, 0); }                      // 0x1fdaf3 (the teleport calamity's hold) (provisional name)
    void SetBuilt(long inIsBuilt, long inUpdateSight, long inLoadButtons) { ee_units_cu_body(EE_UNITS_CU_DO_SET_BUILT, this, inIsBuilt, inUpdateSight, inLoadButtons); }  // 0x1fe230 (the hurricane's new unit: (1, 0, 1))
    // time warp (provisional names)
    void SetIsTinted(bool inValue) { if (this->mIsTinted != inValue) this->mIsTinted = inValue; }  // inline (the warped unit is away)
    void SetLocked(bool inValue) { if (this->mIsLocked != inValue) this->mIsLocked = inValue; }  // inline (EEMLockUnlock)
    void SetIsPacked(bool inValue) { if (this->mIsPacked != inValue) this->mIsPacked = inValue; }  // inline (EEMPackUnpack: packed)
    void GoFanatic() { ee_units_cu_body(EE_UNITS_CU_DO_GO_FANATIC, this, 0, 0, 0); }                       // 0x1fa906 (EEMGoFanatic; provisional name)
    void ClearAllGoalQueues() { ee_units_cu_small(EE_UNITS_CU_S_DO_CLEAR_ALL_GOAL_QUEUES, this, NULL, 0); }              // 0x1f6d44 (EEMScrambleAircraft)
    void QueueGoal(EEUnitGoal* inGoal, long inItem);  // 0x3d416: queues inGoal in mGoalQueues[inItem]
    // inline (COMDAT 0xd5c5 after its first user EECPASGUSpaceCarrier::Retarget)
    unsigned long GetNumCargoUnits() { return this->mCargo.size(); }
    bool HasCargo(EEComplexUnit* inCargo) { return std::find(this->mCargo.begin(), this->mCargo.end(), inCargo) != this->mCargo.end(); }  // 0x46fc0 (EEMImmediateCargoUnload; provisional name)
    bool UnloadCargoNow(EEComplexUnit* inCargo) { return ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_NOW, this, (int64_t)(uintptr_t)inCargo, 0, 0) != 0; }  // 0x1fd0fe (EEMImmediateCargoUnload; provisional name)
    // 0x229314: a free tile near inUnit for this unit (EEMPathTo; returns a distance; provisional name)

    // 0xc7486 (EEMessage.cpp's range; EEActiveGroup::SpreadAroundUnit/SpreadAroundResource call it): stats +0xd8 > 0 and the table
    // gDamageTable [inClass +0x1c8][class +0x1cc] > 0 for a class whose MarkInvalid holds (can attack it) (provisional name)
    bool CanAttackClass(EEUnitClass* inClass);
    EETown* FindNearestTown() { return reinterpret_cast<EETown*>(static_cast<uintptr_t>(ee_units_cu_body(EE_UNITS_CU_DO_FIND_NEAREST_TOWN, this, 0, 0, 0))); }              // 0x1fa856 (the owner's town (EEPlayer +0x7d0) nearest the unit that contains it: EEMPatrol, EECPUnitManager, EECPCombatManager) (provisional name)
    float DistanceSquaredToTileXY(long inTileX, long inTileY) { return ee_units_cu_distance_squared_to_tile(&this->mPhysicalModel.GetLocation().mMatrix, inTileX, inTileY); }  // 0x4211d (EEPPatrolGroup::GetNearest; provisional name; u12a_small.rs)
    float DistanceSquaredToUnit(EEUnit* inOther) { return ee_cp_distance_squared_to_unit((uint64_t)(uintptr_t)static_cast<EEUnit*>(this), (uint64_t)(uintptr_t)inOther); }  // 0x8bdb4 (ee_sim::cp place_world.rs) (provisional name)
    void QueueResearch(long inTechnology, bool inFront) { ee_units_cu_more(EE_UNITS_CUM_DO_QUEUE_RESEARCH, this, inTechnology, inFront, 0, 0.0f, 0.0f, 0.0f, NULL, NULL); }  // 0x1f5f97 (EEMResearchTechnology; provisional name)
    void QueueResearchUncaptured(long inTechnology, bool inFront) { ee_units_cu_body(EE_UNITS_CU_DO_QUEUE_RESEARCH, this, inTechnology, inFront, 0); }  // its body (TQRS records the capture's)
    // inline (out-of-line COMDAT 0x940eb, 26 callers)
    unsigned long GetBuildQueueSize() { return this->mBuildQueue.size(); }
    // inline (the per-class EnabledChanged/B8 switches and EEUCGate::IsPartLeftOut read the current command through it;
    // provisional name)
    unsigned long GetCommandType() const { return this->mCommand ? this->mCommand->mType : 0; }
    // inline (GetEffectHeight 0x1fa4dc loads mFirstFrame2 before the WriteDetailsHook call that indexes the models with it)
    unsigned long GetAnimation() const { return this->mAnimation; }
    void OrderRepair(EEUnit* inTarget) { ee_units_cu_body(EE_UNITS_CU_DO_ORDER_REPAIR, this, (int64_t)(uintptr_t)inTarget, 0, 0); }     // 0x1f60ea (EEMRepairUnit, EEMBuildUnit; provisional name)
    void AddBuildTarget(EEUnit* inTarget) { ee_units_cu_body(EE_UNITS_CU_DO_ADD_BUILD_TARGET, this, (int64_t)(uintptr_t)inTarget, 0, 0); }  // 0x1f61c8 (EEMRepairUnit, EEMBuildUnit; provisional name)
    void RemoveResearch(long inTechnology, long inRemoveAll) { ee_units_cu_body(EE_UNITS_CU_DO_REMOVE_RESEARCH, this, inTechnology, inRemoveAll, 0); }  // 0x1fd984 (EEMRemoveTechnology; provisional name)
    void SetHomeBase(EEComplexUnit* inBase) { ee_units_cu_body(EE_UNITS_CU_DO_SET_HOME_BASE, this, (int64_t)(uintptr_t)inBase, 0, 0); }  // 0x1fe9fb (EEMReturnToBase: the aircraft's new base; provisional name)
    // inline (COMDAT 0xb45b0, first emitted by EECCTimeWarp.cpp)
    void SetColor(const GEColor& inColor) { if (this->mColor != inColor) this->mColor = inColor; }
    float GetEffectHeight(bool inUseCurrentFrame) { return ee_units_cu_more(EE_UNITS_CUM_DO_EFFECT_HEIGHT, this, inUseCurrentFrame, 0, 0, 0.0f, 0.0f, 0.0f, NULL, NULL); }  // 0x1fa4b4 (a height: the effects sit at it, or half of it)
    void ClearBuildTargets() { ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_BUILD_TARGETS, this, 0, 0, 0); }
    unsigned long GetNumGoalQueues() { return this->mGoalQueues.size(); }   // inline in the real header (COMDAT 0x41da1)
    void ClearGoalQueue(unsigned long inIndex) { ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_GOAL_QUEUE, this, inIndex, 0, 0); }  // 0x1f6fbc (for each mGoalQueues item)
    void UnloadCargoAtTile(long inX, long inY, bool inArg) { ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_AT_TILE, this, inX, inY, inArg); }  // 0x1fcc91 (ret 0xc; EECPResourceManager::TransportIdle: the load goal's container tile) (provisional name)
    // 0x1fa661: +0x347 set and +0x358 is 0x1a or 0x1c (the logic bomb refuses such targets) (provisional name)
    bool IsProtected() { return this->mIsDisabled && (this->mDisabledReason == 0x1a || this->mDisabledReason == 0x1c); }
    void AIStep() { ee_units_cu_ai_step(this); }                          // 0x1ff2e8 (Step; provisional name)
    void Step();                            // inline below EEUnitClass (COMDAT 0x1540dc after its first user EEPhysics::UpdateUnits)
    void SetDisabled(bool inAway, long inReason) { ee_units_cu_body(EE_UNITS_CU_DO_SET_DISABLED, this, inAway, inReason, 0); }  // 0x1fe536 (logic bomb: (0, 0) on Stop, (1, 0x1c) on Update)
    void Halt() { ee_units_cu_body(EE_UNITS_CU_DO_HALT, this, 0, 0, 0); }                            // 0x1fef90 (logic bomb: a target that is not category 0x19)
    // the RTSUnit/EEUnit slots EEComplexUnit overrides (vtable 0x446ddc; EEComplexUnit.cpp 0x1f57d3..0x201d85 unless
    // noted). Signatures as in the base classes
    virtual ~EEComplexUnit() { this->FreeNode(); }               // +0x00 0x1f5ba7 (??_E 0x201335: the pool's chunks new[] the units)
    virtual void Invalidate() { ee_units_cu_body(EE_UNITS_CU_DO_INVALIDATE, this, 0, 0, 0); }              // +0x04 0x1f6acb
    virtual void CopyFrom(RTSUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_COPY_FROM, this, (int64_t)(uintptr_t)EEUnitCast<EEComplexUnit>(inUnit), (int64_t)(uintptr_t)inUnit, 0); }    // +0x08 0x1f713d (cu_life.rs)
    virtual void SightOnHere() { this->SightOn(this->GetTileX(), this->GetTileY()); }             // +0x0c 0x1f5b5b: Virtual3C(tile)
    virtual void Describe(UWideString& ioString) { ee_units_cu_describe(this, &ioString); }   // +0x10 0x1fa269
    virtual void DescribeLong(UWideString&) {}  // +0x14 (ICF-folded empty `ret 4` 0x157269)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return EEUnitFile(true, this, 2, inMajorVersion, inMinorVersion, inFile); }   // +0x1c 0x1f2452
    virtual void Clear() { ee_units_cu_small(EE_UNITS_CU_S_DO_CLEAR, this, NULL, 0); }                   // +0x20 0x1f5b6e
    virtual void SightOffHere() { this->SightOff(this->GetTileX(), this->GetTileY()); }            // +0x24 0x1f5b8e: MarkInvalid(tile)
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion) { return (long)ee_units_dop_repair_pointers(this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion); }  // +0x28 0x1f353f
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return EEUnitFile(false, this, 2, inMajorVersion, inMinorVersion, outFile); }  // +0x2c 0x1f417e
    virtual void FreeNode() { ee_units_cu_body(EE_UNITS_CU_DO_FREE_NODE, this, 0, 0, 0); }                // +0x30 0x1f8a43
    virtual void LastDereference() { ee_units_cu_small(EE_UNITS_CU_S_DO_LAST_DEREFERENCE, this, NULL, 0); }         // +0x34 0x1fb7fb (Virtual30, the count = 0, back to sMemoryPool)
    virtual void SightOn(unsigned long inX, unsigned long inY) { ee_units_sight((uint64_t)(uintptr_t)this, (uint32_t)inX, (uint32_t)inY, 1); }      // +0x3c 0x1f7ce7
    virtual void OutOfSynchDump(FILE* inFile, unsigned long inFlags) { ee_units_cu_out_of_synch_dump(this, inFile); }  // +0x44 0x1f908d
    virtual bool IsPassableBy(EEComplexUnit* inUnit) { return ee_units_cu_body(EE_UNITS_CU_DO_IS_PASSABLE_BY, this, (int64_t)(uintptr_t)inUnit, 0, 0) != 0; }  // +0x48 0x1f9e81
    virtual long GetChecksum(unsigned long inFlags) { return (long)ee_units_cu_body(EE_UNITS_CU_DO_GET_CHECKSUM, this, inFlags, 0, 0); }  // +0x4c 0x1fa07f
    virtual EETileCoordinate FindAreaTile(bool inFlag, long inRange) { return EEUnpackTile(ee_units_cu_body(EE_UNITS_CU_DO_FIND_AREA_TILE, this, inFlag, inRange, 0)); }  // +0x50 0x1fa385
    virtual EEModel* GetModel() { return !this->mIsFoundation ? this->GetStats()->mModel : EEGetPlayerClassGraphic(this->GetPlayer(), this->GetUnitClass()); }            // +0x54 0x1fa473 (a foundation: the player's foundation graphic)
    virtual bool IsFoundation() { return this->mIsFoundation; }            // +0x58 0x1f5b67: mIsFoundation
    virtual void GetPosition(GE3DPoint& outPosition) { this->mPhysicalModel.GetLocation().GetPosition(outPosition); }         // +0x5c 0x1fa493
    virtual void Kill(bool inKilled, bool inKeepResearch, bool inOther) { ee_units_cu_body(EE_UNITS_CU_DO_KILL, this, inKilled | inKeepResearch << 1 | inOther << 2, 0, 0); }        // +0x60 0x1fb383
    virtual void LoadMedia(bool inIsFoundation) { ee_units_cu_body(EE_UNITS_CU_DO_LOAD_MEDIA, this, inIsFoundation, 0, 0); }  // +0x64 0x1fb814
    virtual void SightOff(unsigned long inX, unsigned long inY) { ee_units_sight((uint64_t)(uintptr_t)this, (uint32_t)inX, (uint32_t)inY, 0); }     // +0x68 0x1fd336
    virtual long FinishLoad(long inMajorVersion, long inMinorVersion) { return (long)ee_units_dop_more(EE_UNITS_DOPX_DO_FINISH_LOAD, this, inMajorVersion, inMinorVersion); }    // +0x6c 0x1f3395
    virtual void SightMoved(unsigned long inOldX, unsigned long inOldY, unsigned long inNewX, unsigned long inNewY) { ee_units_cu_sight_moved(this, (long)inOldX, (long)inOldY, (long)inNewX, (long)inNewY); }  // +0x78 0x1ff834

    EEComplexUnit() { ee_units_eu_more(EE_UNITS_EUM_DO_CONSTRUCT_COMPLEX, static_cast<EEUnit*>(this), NULL, 0, 0); }   // eu_more.rs
    // 0x1fac37 (ret 8; first argument sizes the goal-queue vector; second randomizes heading): clears the containers
    // and members (provisional name)
    void ResetEEComplexUnit(unsigned long inGoalQueueCount, bool inRandomHeading) { ee_units_cu_body(EE_UNITS_CU_DO_RESET, this, inGoalQueueCount, inRandomHeading, 0); }
    // 0x1fb18f: Init (provisional name; types inferred)
    void Init(EEUnitClass* inClass, const GETransformation& inPlacement,
                         EEPlayer* inPlayer, unsigned long inGoalQueueCount) { ee_units_cu_init(this, inClass, &inPlacement, inPlayer, (uint32_t)inGoalQueueCount); }
    // the pool (cdecl; provisional names): a new unit, reset (0x1f7b7c) or Init'ed (0x1f7bd2); the pool sizes
    // (0x1f7ba6)
    static EEComplexUnit* Create() { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_cu_tiny(EE_UNITS_CU_T_DO_CREATE, NULL, NULL, 0, 0, 0.0f, 0.0f, 0.0f))); }
    static EEComplexUnit* Create(EEUnitClass* inClass, const GETransformation& inTransformation, EEPlayer* inPlayer, unsigned long inGoalQueueCount) { return static_cast<EEComplexUnit*>(ee_units_cu_create_init(inClass, &inTransformation, inPlayer, inGoalQueueCount)); }
    static bool SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks) { return ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_POOL_SIZE, NULL, NULL, inChunkSize, inMaxChunks, 0.0f, 0.0f, 0.0f) != 0; }
    void ClearGarrison() { ee_units_cu_clear_garrison(this); }                   // 0x1f8bd8: mL28C = mL288 = 0 (provisional name)
    bool HasEffect(long inKind) { return ee_units_cu_tiny(EE_UNITS_CU_T_DO_HAS_EFFECT, this, NULL, inKind, 0, 0.0f, 0.0f, 0.0f) != 0; }            // 0x1f9ddb: GetEffect into a dummy (provisional name)
    bool IsCloaked() { return this->mCloaked > 0; }                       // 0x1fa96c: mCloaked != 0 (provisional name)
    // 0x21ba6f: the squared distance from the position (+0x13c/+0x14c/+0x15c) to inPoint (the attack-ground test;
    // provisional name)
    float DistanceSquared3D(const GE3DPoint& inPoint) { return ee_units_uc_rest_f(EE_UNITS_UC_DO_R_DISTANCE_3D, NULL, this, 0, inPoint.mX, inPoint.mY, inPoint.mZ); }  
    void UpdateFanaticismColor() { ee_units_cu_body(EE_UNITS_CU_DO_UPDATE_FANATICISM_COLOR, this, 0, 0, 0); }                        // 0x1fa98c (the cloak colour; function-static GEColors) (provisional name)
    // 0x1fdf31: the goal = the first queued one (returned: DropFinishedGoals) (provisional name)
    EEUnitGoal* RefreshGoals() { return (EEUnitGoal*)(uintptr_t)ee_units_unit_refresh_goals((uint64_t)(uintptr_t)this); }
    void SetPatrolGroup(EEPPatrolGroup* inGroup) { ee_units_cu_body(EE_UNITS_CU_DO_SET_PATROL_GROUP, this, (int64_t)(uintptr_t)inGroup, 0, 0); }
    // defined in EEPlayer.cpp's range after their first user EEPlayer::DisableCamouflagePower (out of line everywhere: recursive)
    void SetCloakableChain(bool inValue) { ee_units_cloak_chain((uint64_t)(uintptr_t)static_cast<RTSUnit*>(this), 0, inValue); }   // 0x16c6a1: mIsCloakable along the mAIKind chain; false also clears mIsCloaked (ee_units player_rest.rs)
    void SetCloakedChain(bool inValue) { ee_units_cloak_chain((uint64_t)(uintptr_t)static_cast<RTSUnit*>(this), 1, inValue); }     // 0x16c6db: mIsCloaked along the mAIKind chain
    // inline: mIsCloaked only counts while mIsCloakable is set (SetCloakableChain clears both). EEUGCastCalamity::EvaluateCondition 0x621133 reads it
    // as `cmp byte [+0x345],0; je; mov al,[+0x346]; test al,al` (the result in al after the caller tests mIsCloakable itself);
    // the same ternary is spelled out at ~20 sites (provisional name)
    bool GetIsCloaked() const { return this->mIsCloakable ? this->mIsCloaked : false; }
    bool AddGarrison(long inAmount, long inGarrisonPopulation) { return ee_units_cu_body(EE_UNITS_CU_DO_ADD_GARRISON, this, inAmount, inGarrisonPopulation, 0) != 0; }  // 0x1f5f55: mL288 += inAmount within the class's mL250 (provisional name)
    EEComplexUnit* FindPassenger(unsigned long inUnitID) { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_cu_body(EE_UNITS_CU_DO_FIND_PASSENGER, this, inUnitID, 0, 0))); }  // 0x1f9f85: the cargo unit with that id (provisional name)
    void ReleaseCargo(EEComplexUnit* inCargo) { ee_units_cu_body(EE_UNITS_CU_DO_RELEASE_CARGO, this, (int64_t)(uintptr_t)inCargo, 0, 0); }  // 0x1fcb8c, actual cargo removal member/ret4
    EEComplexUnit* GetConditionTarget() { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_cu_body(EE_UNITS_CU_DO_CONDITION_TARGET, this, 0, 0, 0))); }    // 0x1ff58b: actual referenced complex attacker or null
    EEComplexUnit* UnloadCargoAt(unsigned long inIndex, const GETransformation& inLocation) { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>( ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_AT, this, inIndex, (int64_t)(uintptr_t)&inLocation, 0))); }
    EEComplexUnit* UnloadCargoByID(unsigned long inUnitID, const GETransformation& inLocation) { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_BY_ID, this, inUnitID, (int64_t)(uintptr_t)&inLocation, 0))); }  // 0x1fd2ba: UnloadCargoAt on the cargo unit with that id (provisional name)
    void SetLocationXY(float inX, float inY, bool inUpdateTiles) { ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_LOCATION_XY, this, NULL, inUpdateTiles, 0, inX, inY, 0.0f); }  // 0x1fe73b: MoveToLocation, then the 2D location (provisional name)
    // the referenced links (`if (m) m->Dereference(); m = in; in->Reference()` shapes; provisional names)
    // inline (ReleaseHeldUnits 0x1f6d6a reaches the link through a reference: lea edi, [eax + 0x1e4]) (provisional
    // name)
    void SetHolder(EEComplexUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_SET_HOLDER, this, (int64_t)(uintptr_t)inUnit, 0, 0); }  // 0x1f5ca2: mRegeneration
    void SetBlockingUnit(RTSUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_SET_BLOCKING_UNIT, this, (int64_t)(uintptr_t)inUnit, 0, 0); }  // 0x1fe202: mL20C
    void SetLastResource(RTSUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_SET_LAST_RESOURCE, this, (int64_t)(uintptr_t)inUnit, 0, 0); }  // 0x1fe5e3: mLastResource
    void ReleaseSound() { ee_units_cu_small(EE_UNITS_CU_S_DO_RELEASE_SOUND, this, NULL, 0); }                    // 0x1f6d1a: releases the sound instance mL25C
    void SetSound(SESMInstanceNode* inSound) { ee_units_cu_small(EE_UNITS_CU_S_DO_SET_SOUND, this, inSound, 0); }   // 0x1f7b4c: mL25C (after ReleaseSound)
    // mU0C4, the units linked to this one (each keeps a reference; provisional names)
    void AddContact(EEComplexUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_ADD_CONTACT, this, (int64_t)(uintptr_t)inUnit, 0, 0); }  // 0x1f6226: adds inUnit unless listed
    void RemoveContactUnit(EEComplexUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_REMOVE_CONTACT_UNIT, this, (int64_t)(uintptr_t)inUnit, 0, 0); }  // 0x1fd2ff: removes inUnit
    void ClearContacts() { ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_CONTACTS, this, 0, 0, 0); }                   // 0x1f6f65: unlinks this from each listed unit, then clears the list
    void MakeAreaEffects() { ee_units_cu_body(EE_UNITS_CU_DO_MAKE_AREA_EFFECTS, this, 0, 0, 0); }                 // 0x1ffcb2 (EEWorld::CreatePlayers on a new game; provisional name)
    // 0x1ff928: model replacement; caller 0x1caba9 passes old/new GraphicObject records, ret 8.
    void SwapGraphic(EEGraphicObject* inOldModel, EEGraphicObject* inNewModel) { ee_units_cu_body(EE_UNITS_CU_DO_SWAP_GRAPHIC, this, (int64_t)(uintptr_t)inOldModel, (int64_t)(uintptr_t)inNewModel, 0); }
    void ReplaceUnits(const std::vector<EEPUnitSwap>& inReplacements) { ee_units_cu_body(EE_UNITS_CU_DO_REPLACE_UNITS, this, (int64_t)(uintptr_t)&inReplacements, 0, 0); } // original1fc6e8 ret4, actual8-byte pair traversal
    EEUnit* PopBuildTarget() { return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(ee_units_cu_body(EE_UNITS_CU_DO_POP_BUILD_TARGET, this, 0, 0, 0))); }               // 0x1fbf16: pops mCitizenBuildQueue up to its first valid unit (provisional name)
    // 0x269137 (in EEUIFMGMainGroup.cpp after its first user Update, called out of line: defined there):
    // mCitizenBuildQueue[inIndex] (provisional name)
    EEUnit* GetCitizenBuildTarget(unsigned long inIndex) { return this->mCitizenBuildQueue[inIndex]; }
    bool IsResearchQueued(long inTechnology) { return std::find(this->mBuildQueue.begin(), this->mBuildQueue.end(), inTechnology) != this->mBuildQueue.end(); }  // 0x937dd (the main group's technology buttons) (provisional name)
    // (provisional names; types inferred)
    void HoldUnit(EEComplexUnit* inUnit) { ee_units_cu_small(EE_UNITS_CU_S_DO_HOLD_UNIT, this, inUnit, 0); }   // 0x1f5c42: links inUnit into mHeldUnits (kept sorted by mGrowth)
    void RemoveAffectingAreaEffects() { ee_units_cu_body(EE_UNITS_CU_DO_REMOVE_AFFECTING, this, 0, 0, 0); }      // 0x1f6de2: mAffectingAreaEffects's effects let go of the unit (EEAEManager 0x27b6b7)
    void ReleaseFireEffect() { ee_units_cu_body(EE_UNITS_CU_DO_RELEASE_FIRE_EFFECT, this, 0, 0, 0); }               // 0x1f70bf: the fire effect off (or 0x23d5c3 for type 0x321)
    void ResizeTurretAngles(unsigned long inCount) { ee_units_cu_more(EE_UNITS_CUM_DO_RESIZE_TURRETS, this, (int64_t)inCount, 0, 0, 0.0f, 0.0f, 0.0f, NULL, NULL); }  // 0x1f7bfa: mTabButtons = inCount zeroed floats
    void PartFinished() { ee_units_cu_small(EE_UNITS_CU_S_DO_PART_FINISHED, this, NULL, 0); }                    // 0x1f9d91: Virtual3C(tile), the mini map for other players' units
    // 0x1fcb0e (drops the auto-waypoint location goals; ee_units goals.rs)
    void DropFinishedGoals(bool inStartAll) { ee_units_unit_drop_finished_goals((uint64_t)(uintptr_t)this, inStartAll); }
    // 0x1fbf66: drops the queued goals of mGoalQueues[inIndex] that refuse the unit
    void DropUnsuitedGoals(unsigned long inIndex) { ee_units_unit_drop_unsuited_goals((uint64_t)(uintptr_t)this, (uint32_t)inIndex); }
    void ReplaceResearch(long inTechnology, long inNewTechnology) { ee_units_cu_body(EE_UNITS_CU_DO_REPLACE_RESEARCH, this, inTechnology, inNewTechnology, 0); }  // 0x1ffbc2 (0x244deb: each live unit of a technology's classes) (provisional name)
    bool ReleaseHeld(EEComplexUnit* inUnit) { return ee_units_cu_body(EE_UNITS_CU_DO_RELEASE_HELD, this, (int64_t)(uintptr_t)inUnit, 0, 0) != 0; }  // 0x1fca95: removes inUnit from mHeldUnits
    void RefreshActiveGoal() { ee_units_unit_refresh_active_goal((uint64_t)(uintptr_t)this); }
    // inGoal for each of the (3, or 4 with civilization power 0x34) goal queues: the first as is, copies for the rest
    void QueueSquadronGoals(EEUnitGoal* inGoal, bool inCloneGoal) { ee_units_cu_body(EE_UNITS_CU_DO_QUEUE_SQUADRON_GOALS, this, (int64_t)(uintptr_t)inGoal, inCloneGoal, 0); }  // 0x1fca21 (queued behind a goal already there) (provisional name)
    void SetSquadronGoals(EEUnitGoal* inGoal, bool inCloneGoal) { ee_units_cu_body(EE_UNITS_CU_DO_SET_SQUADRON_GOALS, this, (int64_t)(uintptr_t)inGoal, inCloneGoal, 0); }  // 0x1fe42b (replacing them) (provisional name)
    void SetGatheredResource(EEUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_SET_GATHERED_RESOURCE, this, (int64_t)(uintptr_t)inUnit, 0, 0); }  // 0x1fe372: mShadowScale (referenced; gWorld is told) (provisional name)
    void FnE123D() {}                         // an empty member (ICF-folded `ret` 0xe123d; EEE2DMarchingIcons::Virtual5C) (provisional name)
    void LinkPart(EEComplexUnit* inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_LINK_PART, this, (int64_t)(uintptr_t)inUnit, 0, 0); }   // 0x1fe7d9: mPreviousPart = inUnit, which links back through its mAIKind (provisional name)
    void SetReloadTimer(long inValue) { if (this->mReloadTimer != inValue) this->mReloadTimer = inValue; }  // inline (SetRandomReloadTimer) (provisional name)
    // inline setters (EEUnitClass::UnitCreated 0x23511e) (provisional name)
    void SetHealSearchTimer(float inValue) { if (this->mHealSearchTimer != inValue) this->mHealSearchTimer = inValue; }
    void SetLastHitPoints(long inValue) { if (this->mLastHitPoints != inValue) this->mLastHitPoints = inValue; }
    void SetAmmunition(unsigned long inValue) { if (this->mAmmunition != inValue) this->mAmmunition = inValue; }
    void SetBehavior(EEUnitBehavior* inValue) { if (this->mBehavior != inValue) this->mBehavior = inValue; }
    void SetTrailEffectTimer(long inValue) { if (this->mTrailEffectTimer != inValue) this->mTrailEffectTimer = inValue; }  // (EEUnitClass::UpdateTrail)
    void SetRegenerationTimer(long inValue) { if (this->mRegenerationTimer != inValue) this->mRegenerationTimer = inValue; }  // (EEUnitClass::Regenerate)
    void SetExitingUnitID(long inValue) { if (this->mExitingUnitID != inValue) this->mExitingUnitID = inValue; }  // (EEUCBuilding::SelectUnitToRelease)
    void SetFacing(unsigned long inValue) { if (this->mFacing != inValue) this->mFacing = inValue; }  // (EEUCBuilding::UnitCreated)
    void SetGateDirection(unsigned long inValue) { if (this->mGateDirection != inValue) this->mGateDirection = inValue; }  // (the gate's direction: EEUCGate.cpp)
    // 0x2394b0 (out of line in EEUnitClass.cpp after its first user UpdateRipples's neighbour): mActiveGroup's GetInverseMaxVelocity,
    // else mStats->mFD0 (provisional name)
    float GetInverseMaxVelocity() { return ee_units_uc_rest_f(EE_UNITS_UC_DO_R_INVERSE_MAX_VELOCITY, NULL, this, 0, 0.0f, 0.0f, 0.0f); }  
    void SetIsCommandable(bool inValue) { if (this->mIsCommandable != inValue) this->mIsCommandable = inValue; }  // (EEUnitClass::UnitDied)
    void SetB342(bool inValue) { if (this->mIsClearanceGranted != inValue) this->mIsClearanceGranted = inValue; }  // (EEAirTrafficControl::ClearAirportQueue: cmp then store through the address)
    // (provisional names; types inferred)
    void ReleaseConversionEffect() { ee_units_cu_body(EE_UNITS_CU_DO_RELEASE_CONVERSION_EFFECT, this, 0, 0, 0); }         // 0x1f6f99: the mL2C0 effect off
    void KillCargo() { ee_units_cu_body(EE_UNITS_CU_DO_KILL_CARGO, this, 0, 0, 0); }                       // 0x1f8af6: unloads every cargo unit (Virtual60(0, 0, 0)), statistics
    static void ReleasePool() { ee_units_cu_tiny(EE_UNITS_CU_T_DO_RELEASE_POOL, NULL, NULL, 0, 0, 0.0f, 0.0f, 0.0f); }              // 0x1f8be7 (cdecl; 0x144295): the pool's stats, then Release
    // 0x1f8c2d RefreshPath (0x22e47a): Rust (ee_units waypoints.rs refresh_path; its callers are Rust)
    void SetScaledModel(GEModel* inModel) { ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_SCALED_MODEL, this, inModel, 0, 0, 0.0f, 0.0f, 0.0f); }  // 0x1fe405 (0xf127d): the model, scaled by the class's mL28C
    void ClearCollisionState() { ee_units_cu_clear_collision_state(this); }             // 0x138e29 (TakeOffMap calls it first) (provisional name)
    void UpdateCamouflage(long inAction) { ee_units_cu_body(EE_UNITS_CU_DO_UPDATE_CAMOUFLAGE, this, inAction, 0, 0); }   // 0x1ff6b4 (before mCommand changes: StartAction, AIStep) (provisional name)
    void UpdateAreaEffects(unsigned long inValue) { this->mAreaEffects.MoveAndUpdate(GE2DPoint(this->mPhysicalModel.GetLocation().GetX(), this->mPhysicalModel.GetLocation().GetY()), (float)inValue); }  // 0x1ff7f8 (EEAEManager 0x27b693): the area effects at the unit's 2D location (provisional name)
    void ReplaceCargo(EEComplexUnit* inOld, EEComplexUnit* inNew) { ee_units_cu_body(EE_UNITS_CU_DO_REPLACE_CARGO, this, (int64_t)(uintptr_t)inOld, (int64_t)(uintptr_t)inNew, 0); }  // 0x1fecd8: inNew takes inOld's cargo place (provisional name)
    bool HasSwimAnimation();                // 0x1566e5 (out of line, EEPhysicsModel's range; MoveToLocation) (provisional name)
    bool IsSwimming();                      // 0x156681 (likewise: the model part 2 / 0xd switch) (provisional name)
    void FindContacts() { ee_units_cu_body(EE_UNITS_CU_DO_FIND_CONTACTS, this, 0, 0, 0); }                    // 0x1f6817: links the units on the tiles around the unit (AddContact both ways)
    void SetNextFrame(long inValue) { if (this->mNextFrame != inValue) this->mNextFrame = inValue; }  // inline (SetAnimation: cmp, lea, store)
    // 0x1fbc3b (ret 0xc): the waypoint on tile (inX, inY), if any
    bool FindWaypointAtTile(long inX, long inY, const EEComplexUnitWaypoint*& outWaypoint)   // waypoints.rs
        { return ee_units_waypoints_at_tile((uint64_t)(uintptr_t)this, inX, inY, reinterpret_cast<const EeUnitsWaypoint**>(&outWaypoint)); }
    void PlaceOnGround(const GE3DPoint& inLocation) { ee_units_cu_more(EE_UNITS_CUM_DO_PLACE_ON_GROUND, this, 0, 0, 0, inLocation.GetX(), inLocation.GetY(), inLocation.GetZ(), NULL, NULL); }  // 0x1fc90c (EEPhysicsModel::Move): onto the ground at inLocation

    // Layout (no gaps): the ctor 0x1f5999 (member ctors), its init calls 0x23da8e/0x229706/
    // 0x1fac37, the dtor 0x1f5ba7 (member dtors) and the slots' Load/Save (FSFile Read/Write overloads give the
    // scalar types). Container element types inferred unless a named COMDAT gives them.
    std::vector<EEProductionEntry> mBuildQueue;  // +0x048 the production queue (8-byte elements: `_Buy` 0x14823b)
    std::deque<EEUnit*> mCitizenBuildQueue;  // +0x058 (POD elements; dtor 0x2003a4)
    std::vector<EEComplexUnit*> mCargo;            // +0x06c the cargo (EEUGUnloadTransport 0x2288ea: _Ptrit<EEUnit*>::operator+)
    EEWaypointList mWaypoints;              // +0x07c the path (a Rust list)
    std::vector<EEUnitVectorItem> mGoalQueues;  // +0x090 (0x28-byte polymorphic elements: `_Buy` 0x10918a multiplies by 40, dtor 0x776bb)
    std::deque<EEComplexUnitAttacker> mIgnoredTargets;  // +0x0a0 the attackers (IgnoreTarget)
    std::vector<EEComplexUnit*> mHeldUnits;  // +0x0b4
    UList<EEComplexUnit*> mContacts;                // +0x0c4 (4-byte data: the node's next at +8; dtor 0x200d66)
    std::vector<long> mUnitVariables;       // +0x0cc
    U2DPoint<float> mU0DC;                  // +0x0dc (Set2DPosition; the teleport floors it) U2DPoint<float>: Save calls COMDAT 0x5f53fc = U2DPoint<float>::Save, twin of 0x412e38 (float write slot 0x7c), not the GE2DPoint::Save import
    EEUnitAreaEffectList mAreaEffects;      // +0x0e4
    EEUnitAreaEffectList mAffectingAreaEffects;  // +0x0f8
    GE3DPoint mProjectileStart;             // +0x10c provisional name (SetPosition; saved)
    GE3DPoint mInitialContactLocation;      // +0x118 (three fldz; not saved)
    GE3DPoint mLastMapPosition;             // +0x124 (saved)
    // +0x130..+0x17c: the ctor runs GEPhysicalModel's inline ctor (GETransformation(), mModel = 0, mScale = 1,
    // mFrame = 0), the dtor calls ~GEPhysicalModel. Its position is mPhysicalModel.GetLocation() (the area effects'
    // source location; switching every reader to the accessor changed no row)
    // New (R5b.4): an EEUnitStoreModel (GEPhysicalModel's layout; its location setters report to the unit store)
    EEUnitStoreModel& GetPhysicalModel() { return this->mPhysicalModel; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    EEUnitStoreModel mPhysicalModel;        // +0x130
    GEOrientation mAngularVelocity;         // +0x17c (saved)
    GEOrientation mOrientation;             // +0x188 (saved)
    std::vector<U2DPoint<long> > mBoundedPathTiles;  // +0x194 (filled from EEGetUnitTiles's vector 0x2373bd; assigned with operator= 0x1a4f4 at 0x419ccb)
    GEColor mColor;                         // +0x1ac (1, 1, 1; saved)
    // +0x1b8..+0x20c and +0x25c, the pointer members (mOrbitals, mWheels, mLauncherClass, mActiveGroup,
    // mPreviousPart, mNextPart, mHomeBase, mContainer, mLastAttacker, mOwner, mHolder, mCommand, mActiveGoal,
    // mGoalState, mLastResource, mGatheredResource, mMine, mHerd, mTurretAngles, mBehavior, mBlockingUnit, mSound):
    // the record EeUnitsComplexUnitFields (ee_sim world unit_fields.rs; the views read them there)
    EEPPatrolGroup* GetPatrolGroup() const { return this->mPatrolGroup; }
    EEPPatrolGroup* mPatrolGroup;           // +0x1c4 (RTTI EEPPatrolGroup, ee/EEPPatrolGroup.h; a C++ member: its Rust type is the
                                            // units module's, which the world records' header cannot include)
    // mCollisionTile (+0x1a4) and the effect handles mConversionEffect (+0x2c0), mHealEffect (+0x2c4) and mFireEffect
    // (+0x310): the record's (units round 12)
    EEActiveGroup* GetActiveGroup() const { return this->mActiveGroup; }
    EEComplexUnit* GetContainer() const { return this->mContainer; }  // inline (0x61c806 budget) r37_big B2
    EEUnitAction* GetCommand() const { return this->mCommand; }
    EEUnitGoal* GetGoalState() const { return this->mGoalState; }
    EEUnitBehavior* GetBehavior() const { return this->mBehavior; }
    float GetCollisionResolutionCount() const { return this->mCollisionResolutionCount; }
    bool GetIsDisabled() const { return this->mIsDisabled; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    bool GetIsBuilt() const { return this->mIsBuilt; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    float GetRoll() const { return this->mRoll; }
    // Port (not in the original, not saved): a human shift-queued a build or repair (EEMBuildUnit, EEMRepairUnit),
    // so the citizen idles at the end of its build queue instead of taking a smart task (EEUGBuildRepair::Step).
    // OrderRepair and ClearBuildTargets clear it, and so does going idle with an empty build queue.
};

// 0x16d69e (cdecl): the statistics series of inUnit, 0 for none (EEComplexUnit::GarrisonUnit passes it to
// EEPlayerStatistics::Lower with 1) (provisional name)
inline long EEAddDeadUnitStatistic(EEComplexUnit* inUnit) { return ee_units_dead_unit_statistic((uint64_t)(uintptr_t)static_cast<RTSUnit*>(inUnit)); }
// 0x246c87 (cdecl): the complex unit acting on inUnit, 0 if none (SetGatheredResource) (provisional name)
EEComplexUnit* EEGetWaitingGatherer(EEUnit* inUnit);
// 0x23d5c3 (cdecl; EEComplexUnit::ReleaseFireEffect for unit type 0x321): destroys the unit's cloaking effect, if it has
// one, and clears its handle (uu_cloak.rs) (provisional name)
inline void EERemoveCloakEffect(EEUnit* inUnit) { ee_units_ck_body(EE_UNITS_CK_DO_REMOVE_CLOAK_EFFECT, (uint64_t)(uintptr_t)inUnit, 0, 0, 0); }
// 0x23d570 (cdecl; EEUIFMGMainGroup::SetUnit for a selected unit of type 0x321): attaches the cloaking graphic effect
// (0x11c) to the unit and keeps its handle in the complex unit's mCloaked (provisional name)
inline void EEAddCloakEffect(EEUnit* inUnit) { ee_units_ck_body(EE_UNITS_CK_DO_ADD_CLOAK_EFFECT, (uint64_t)(uintptr_t)inUnit, 0, 0, 0); }
// 0x23d5fb (cdecl): the planet (land continent) under a unit or nearest it within 6 tiles; 1 on a non-space map, 0
// when none (the new unit's +0x35c: EEUnitClass::CreateUnitAt for walls/farms) (provisional name)
inline unsigned short EEGetLandContinentOf(EEComplexUnit* inUnit) { return (unsigned short)ee_units_ck_body(EE_UNITS_CK_DO_LAND_CONTINENT_OF, (uint64_t)(uintptr_t)inUnit, 0, 0, 0); }

// The weapon kinds of a unit class (EEUnitClass::GetWeapon; only the bound is named, by EEUGAttackUnit's assert)
enum EEUnitClassWeapon { kWeaponTotal = 6, keeucwTotal = kWeaponTotal /* spelled this way in the original's assert strings */ };

// 0x5636ac..0x564448 (871; loops end at 0x964448): every unit class by index (provisional name); constant-index reads
// like [827] (0x241419) are elements
extern std::vector<EEUnitClass*> gUnitClassTable;
// The number of unit class slots: one per unit type, sized from dbobjects by EEUnitClass::CreateUnitClasses (the
// original's fixed 0x367 = 871 slots; vanilla AoC fills 848). Every "for each unit class" loop and every unit type
// range check uses this.
inline unsigned long EEUnitClassCount() { return (unsigned long)gUnitClassTable.size(); }
// "no unit class" in the fields that used the old table size 0x367 as that value (EEPlayer::mHackUnitClass,
// EEMHackANewUnit's "the sender's chosen class"), and "any unit type" for EEALasso::SelectUnits (was 0x366)
const unsigned long kNoUnitClass = 0xffffffffUL;
const unsigned long kAnyUnitType = 0xffffffffUL;
// 0x531d58: the EEPM* physics model singletons (EEPMLand.cpp; the EEUC* ctors pass one to EEUnitClass) (provisional
// name)
extern EEPhysicsModel* gPhysicsModels[12];
// Unit action names use the real UString value in EEUnitAction::sActionNames (0x563668).

// 0x22c7e0 (cdecl): the key map of a unit class's +0x08 name (EEUnitClass ctor: mAIScript) (provisional name)
// 0x563a6c is gUnitClassTable[0xf0] (inside the table, not after it): the wall class the ghost of unit type 0x42 adds
// three of (EEGhostUnit::Create); there is no separate gWallUnitClass extern (the readers index the table)

bool EEIsLineBlocked(long inX1, long inY1, long inX2, long inY2, EEUnit** outArg5, long* outX, long* outY);

// 0x12a52e (cdecl): whether inUnit is seen by the current player (the land priest's sounds) (provisional name)
bool EEIsSeen(EEUnit* inUnit);

// 0x20b687 (cdecl): whether tile (inX, inY) takes a unit (special location; first argument 0) (provisional name)
bool EEIsTileFree(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);   // inUnit: the unload goal passes its transport

// 0x20ccb8 (cdecl; the conscription calamity's second call with 0x138d) (provisional name)
void EEPlayUnitEffect(EEComplexUnit* inUnit, long inEffect);

// EEActiveGroup (the group object at EEComplexUnit +0x1c8) is defined in ee/EEPlayer.h after its base EEGroup
// (RTTI EEActiveGroup > EEGroup, ctor 0x1ebc86)

// 0x24e518 (cdecl; 17 callers, the meteor storm after it has placed its meteor) provisional name and argument types
void EEUpdateUnitTile(GETransformation* inTransformation, EEComplexUnit* inUnit, long* outX, long* outY);

// A unit type (RTTI EEUnitClass > UReferenceCounter; vtable 0x447b10; EEUnitClass.cpp around 0x2358b1). Names
// are provisional.
// a shot's ActionChanged or Update (inUpdate; inFunc its CBSO record): ee_combat shots.rs over EEUnitClassShots.cpp's host
void EECombatShotStep(int32_t inKind, bool inUpdate, uint32_t inFunc, EEUnitClass* inClass, EEComplexUnit* inUnit);
struct EEUnitClassRecord;                   // a dbunits.dat record (the ctor copies it field by field; +0x374 a count)

// The class's improvement table (EEUnitClass::mImprovements) is the upgrade table's Rust record EeDataUpgradeFields
// (rust/data/tables.h; EEUCUpgradeTable's base): per improvement attribute a factor (mScale the cost factor, mOffset
// the step factor; attribute 12 has no cost) and the maximum level (units round 11: the C++ views of it,
// EEUnitClassImprovementTable and its entry, are gone).

// The plain members (mAirTrafficType, mCommands and the block mCruiseAltitude +0x11c .. mIsParatrooperBomber +0x326:
// numbers, flags and their arrays, names and types kept) are the Rust record EeUnitsUnitClassFields (ee_units
// unit_class_fields.rs), the second base after UReferenceCounter; the ctor's copies from the db row are its fill.
class EEUnitClass : public UReferenceCounter, public EeUnitsUnitClassFields, public EeUnitsUnitClassLinks {
public:
    // an object's handle for the Rust calls of the inline members (its address)
    static uint64_t H(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
    // 0x22f317 (out of line; every EEUC* ctor calls it with 1, its record and its class's global) (provisional names)
    EEUnitClass(long inIsComplex, EEUnitClassRecord* inRecord, EEPhysicsModel* inPhysicsModel) { ee_units_uc_class(EE_UNITS_UC_DO_CONSTRUCT, this, inRecord, inIsComplex, inPhysicsModel); }
    // 0x2364c0: the value for inKey in the map at +0x60 (0 when there is no map or no entry); the bombers ask for
    // 0x7e0 (provisional name)
    EEUnitAction* FindAction(long inKey) { return reinterpret_cast<EEUnitAction*>(static_cast<uintptr_t>( ee_units_script_find(reinterpret_cast<uintptr_t>(this->mAIScript), (int32_t)inKey))); }
    void UnitRemoved() { ee_units_uc_rest(EE_UNITS_UC_DO_R_UNIT_REMOVED, this, NULL, 0, 0, 0, NULL); }  // 0x236a83 (EEUnit::Remove, first) (provisional name)
    void LoadClassMedia() { ee_units_uc_call(EE_UNITS_UC_DO_LOAD_CLASS_MEDIA, this, NULL, 0, 0, 0, NULL, NULL); }  // 0x2354ae (EEPlayer::LoadAllClassMedia, per class with loaded media; provisional name)
    void UnloadClassMedia() { ee_units_uc_call(EE_UNITS_UC_DO_UNLOAD_CLASS_MEDIA, this, NULL, 0, 0, 0, NULL, NULL); }  // 0x2382fd (EEPlayer::UnloadAllClassMedia, per mSoundLoadedClasses class; provisional name)
    virtual ~EEUnitClass() { ee_units_uc_call(EE_UNITS_UC_DO_DESTROY, this, NULL, 0, 0, 0, NULL, NULL); }  // +0x00 0x2305ee (dereferences the effects and classes)
    void GroundLocation(GE3DPoint& ioLocation) { ee_units_uck_dispatch(EE_UNITS_BODY_GROUND_LOCATION, this, NULL, 0, 0, 0, &ioLocation.mX); }  // +0x04 (the time warp's return, flag 4)
    // +0x08 (base 0x2907a3: empty): the tile under inLocation along the camera line (EEUCNavalCitizen 0x218490)
    void LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY) { ee_units_uck_dispatch(EE_UNITS_BODY_LOCATION_TO_TILE, this, NULL, H(&inLocation), (int64_t)H(&outX), (int64_t)H(&outY), NULL); }
    void CargoRemoved(EEComplexUnit* inUnit, EEComplexUnit* inCargo) { ee_units_uck_dispatch(EE_UNITS_BODY_CARGO_REMOVED, this, inUnit, H(inCargo), 0, 0, NULL); }  // +0x14 (EEUCFortress 0x2101da: ret 8, the cargo)
    // +0x18 (EEComplexUnit::LoadCargo pushes (this, cargo, its third argument) after loading a cargo)
    void CargoAdded(EEComplexUnit* inUnit, EEComplexUnit* inCargo, bool inArg) { ee_units_uck_dispatch(EE_UNITS_BODY_CARGO_ADDED, this, inUnit, H(inCargo), inArg, 0, NULL); }  // bool: 0x6101b5 `cmp byte`
    void CargoUnloaded(EEComplexUnit* inUnit, EEComplexUnit* inCargo) { ee_units_uck_dispatch(EE_UNITS_BODY_CARGO_UNLOADED, this, inUnit, H(inCargo), 0, 0, NULL); }  // +0x1c (EEUCFortress: the same body 0x2101da)
    void GateReplaced(EEUnit* inGate, EEUnit* inUnit) { ee_units_uck_dispatch(EE_UNITS_BODY_GATE_REPLACED, this, NULL, H(inGate), (int64_t)H(inUnit), 0, NULL); }  // +0x20 (base 0xac9c8: ret 8; EEUCGate 0x2104ef)
    // +0x24 the volcano ClearArea 0xb46bc pushes (unit, &location), stores a float (compile-required; row 62.6).
    // inline (COMDAT 0x216d77 right after EEUCLandPriest.cpp's ctor, with +0x68/+0x6c: the place of inline in-class
    // virtuals; every unit class's vtable points there) inferred body
    float GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation) { return ee_units_ucn_height_at(H(this), H(inUnit), inLocation.mX, inLocation.mY, inLocation.mZ); }
    // +0x2c: the volume/pan/pitch of inSound at inUnit (EEUGUnloadTransport::Unload 0x228c29)
    void GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch) { ee_units_uck_sound(this, inUnit, inSound, outVolume, outPan, outPitch); }
    // +0x30: a new unit of this class at (inX, inY) for player inPlayer (the drop-object calamity) (provisional name)
    // (inCreator: EEUnit*, the meteor storm passes its EECalamity::mCasterUnit)
    EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator) { return (EEUnit*)(uintptr_t)ee_units_uck_dispatch(EE_UNITS_BODY_CREATE_UNIT, this, NULL, H(inCreator), (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32), (int64_t)inPlayer * 2 + inIsFoundation, NULL); }
    // +0x34: a new unit of this class at inTransformation for player inPlayer (EECreateCalamityUnit 0xaeb83, the
    // hurricane and volcano calamities) (provisional name)
    EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator) { return (EEUnit*)(uintptr_t)ee_units_uck_dispatch(EE_UNITS_BODY_CREATE_UNIT_AT, this, NULL, H(inCreator), (int64_t)H(&inTransformation), (int64_t)inPlayer * 2 + inIsFoundation, NULL); }
    // +0x38/+0x3c: a new unit of this class for inPlayer on inMap at a tile / at a point, facing inHeading (the random
    // map's PlaceWall 0x2859e7, PlaceTerrainUnit 0x287c18, 0x288426 / PlaceFeatures 0x2849f2) (provisional names)
    EEUnit* CreateUnitAtTile(EEPlayer* inPlayer, long inX, long inY, EEMap* inMap, float inHeading) { return (EEUnit*)ee_units_uc_create_at(this, inPlayer, (float)(unsigned long)inX + 0.5f, (float)(unsigned long)inY + 0.5f, (uint32_t)inX, (uint32_t)inY, inMap, inHeading); }
    EEUnit* CreateUnitAtPoint(EEPlayer* inPlayer, float inX, float inY, EEMap* inMap, float inHeading) { return (EEUnit*)ee_units_uc_create_at(this, inPlayer, inX, inY, UMFloorUnsigned(inX), UMFloorUnsigned(inY), inMap, inHeading); }   // uc_create.rs
    void Flee(EEComplexUnit* inUnit) { ee_units_mv_flee(H(this), H(inUnit)); }  // original ret4, all37 actual class vtables
    // +0x44 (the footprint tiles; EEUGBuildRepair 0x21e3a6); the 4th argument is a bool (0x234965 `cmp byte`)
    void GetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners, bool inAppend) { ee_units_ucn_surrounding_tiles(H(this), H(inUnit), H(&outTiles), (int32_t)inCorners, inAppend); }
    bool IsBuilding() { return EEClassBody(this, EE_UNITS_BODY_IS_BUILDING) == 1; }   // +0x48
    bool IsLandBuilding() { return ee_units_uck_dispatch(EE_UNITS_BODY_IS_LAND_BUILDING, this, NULL, 0, 0, 0, NULL) != 0; }  // +0x54
    bool IsPartLeftOut(EEComplexUnit* inUnit) { return ee_units_uck_dispatch(EE_UNITS_BODY_IS_PART_LEFT_OUT, this, inUnit, 0, 0, 0, NULL) != 0; }  // +0x58 (EEUCGate 0x211370)
    bool FliesOver() { return ee_units_uck_dispatch(EE_UNITS_BODY_FLIES_OVER, this, NULL, 0, 0, 0, NULL) != 0; }  // +0x5c
    bool IsPickable() { return ee_units_uck_dispatch(EE_UNITS_BODY_IS_PICKABLE, this, NULL, 0, 0, 0, NULL) != 0; }  // +0x60
    bool IsSelectable(EEUnit* inUnit) { return ee_units_uck_dispatch(EE_UNITS_BODY_IS_SELECTABLE, this, NULL, H(inUnit), 0, 0, NULL) != 0; }  // +0x64 (a unit: EEPlayer::SelectUnit 0x16a53b)
    bool CanBeAttacked() { return !this->mIsFlare; }  // +0x68 inline (COMDAT 0x216d81; the Hero unit tests al)
    long GetPopulationCost() { return this->mWeight; }  // +0x6c (mL258)
    void ActionChanged(EEComplexUnit* inUnit) { ee_units_uck_call(EE_UNITS_UCK_DO_ACTION_CHANGED, this, inUnit, 0, 0, 0, 0.0f); }  // +0x74 (the cloaking calamity calls it on each cloaked unit)
    bool CanMoveOnTerrain(EEUnit* inUnit, long inTerrainType) { return ee_units_cls_can_move_on_terrain(this, inUnit != NULL ? inUnit->mStats : NULL, (int32_t)inTerrainType) != 0; }  // 0x234ad8 (the missile's +0xe4: true = stays) (provisional name)
    void RouteDockWaypoint(EEComplexUnit* inUnit) { ee_units_ucn_route_dock_waypoint(H(this), H(inUnit)); }  // 0x213db5 (the airplane on its carrier after its move: EEPMAirplane::OrientOnCarrier) (provisional name)
    bool FacesTarget(EEComplexUnit* inUnit, EEUnit* inTarget, bool inUseTolerance) { return ee_units_ucn_faces_target(H(this), H(inUnit), H(inTarget), inUseTolerance); }  // 0x233a14 (EEUGHealUnit)
    // 0x238087: the projectile launch from the weapon tag point (FireShot) (provisional name)
    void LaunchFromTurret(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& inLocation) { ee_units_uc_call(EE_UNITS_UC_DO_LAUNCH_FROM_TURRET, this, inUnit, H(inTarget), 0, 0, (const float(*)[3])&inLocation.mX, NULL); }
    void FireShot(EEComplexUnit* inUnit) { ee_units_ucn_fire_shot(H(this), H(inUnit)); }   // 0x231312 (EEUGAttackGround::Step; provisional name)
    bool FacesPoint(EEComplexUnit* inUnit, const GE3DPoint& inLocation, bool inUseTolerance) { return ee_units_ucn_faces_point(H(this), H(inUnit), inLocation.mX, inLocation.mY, inUseTolerance); }  // 0x233a72 (EEUGAttackGround::EvaluateCondition)
    // the building classes' VirtualB8 helpers (EEUCSpaceBuilding 0x21a1cb; provisional names)
    void RefreshSound(EEComplexUnit* inUnit) { ee_units_uc_body(EE_UNITS_UC_DO_REFRESH_SOUND, this, inUnit); }
    void Regenerate(EEComplexUnit* inUnit) { ee_units_uc_call(EE_UNITS_UC_DO_REGENERATE, this, inUnit, 0, 0, 0, NULL, NULL); }  // 0x239d8a (units whose stats +0x15c is positive)
    bool IsDropSite() { return ee_units_uc_is(H(this), EE_UNITS_UC_IS_DROP_SITE); }                      // 0x4249 (inline in the real header: an out-of-line COMDAT; EEUGBuildRepair::FindNextWork) (provisional name)
    bool IsCrusader() { return ee_units_uc_is(H(this), EE_UNITS_UC_IS_CRUSADER); }                      // 0x5b96d (ICF-shared with EECPAttackSubGroup::Fn5B96D): mUnitType 0x2e6..0x2e9 (0x24722d) (provisional name)
    bool IsTransport() { return ee_units_uc_is(H(this), EE_UNITS_UC_IS_TRANSPORT); }                     // 0x4292 (categories 0x10..: a transport class; EEMImmediateCargoUnload)
    bool IsAirBase() { return ee_units_uc_is(H(this), EE_UNITS_UC_IS_AIR_BASE); }                       // 0x4229 (+0x28 set and +0x1b8 2, 0x15 or 0x20: the icon bar's aircraft carrier test) (provisional name)
    // 0x1fdf06 (inline in the real header? its COMDAT follows EEComplexUnit::StartProduction; EEUnitClass 0x231b55
    // calls it too): mAirTrafficType set, not WriteDetailsHook, category not 0x15/0x20 (provisional name)
    bool IsAircraft() { return this->mAirTrafficType != 0 && !this->IsLandBuilding() && this->mCategory != kUnitCategoryNavalBuilding && this->mCategory != kUnitCategorySpaceBuilding; }
    // inline (COMDAT 0x221b9, first emitted by EEAIContinentManager.cpp; every caller passes it to an EEDebugTrace
    // and calls it out of line) (provisional name)
    const UString& GetName() { return this->mName; }
    // inline (named by EEUGAttackUnit::SetTarget's assert: 0 <= GetWeapon() < keeucwTotal)
    unsigned long GetWeapon() const { return this->mWeapon; }  // unsigned: the assert compiles to one `cmp 6; jb`
    // inline: the garrison goal's ComputeUpgrade loads both before its tech-tree lookups (0x226bfa) (provisional names)
    long GetTechnology() const { return this->mTechnology; }
    bool IsPriest() { return ee_units_uc_is(H(this), EE_UNITS_UC_IS_PRIEST); }                        // 0x6ef9 (EEUGAttackUnit's ctor; provisional name)
    long GetNumResearchTechnologies() { return this->mResearchTechnologies.size(); }      // 0x196dff (EECPDefenseCommander 0x78dc8: mResearchTechnologies's count) (provisional name)
    // EEUnitClass.cpp members (provisional names)
    void AddWaypointAt(EEComplexUnit* inUnit, float inX, float inY, float inZ, bool inSnapToMap) { ee_units_mv_add_waypoint_at(H(inUnit), inX, inY, inZ, inSnapToMap); }
    float GetElevationFactor(EEComplexUnit* inUnit, EEComplexUnit* inTarget) { return ee_units_uc_rest_f(EE_UNITS_UC_DO_R_ELEVATION_FACTOR, this, inUnit, H(inTarget), 0.0f, 0.0f, 0.0f); }  // 0x23187a (a height factor against inTarget)
    bool CanEnter(EEUnitClass* inClass) { return ee_units_cls_can_enter(this, inClass) != 0; }
    // 0x564448: the unit types registered in gUnitClassTable (RegisterUnitClass push_backs them; the vector right after
    // the table; guarded dtor: a class static) (provisional name)
    static std::vector<unsigned long> sUnitTypes;
    static std::vector<unsigned long> GetUnitTypes() { return EEUnitClass::sUnitTypes; }  // by value (EESETSETSetFavoriteUnit 0x18d608 reads the copy's size from the frame)
    static void ReleaseUnitClasses() { ee_units_uc_rest(EE_UNITS_UC_DO_R_RELEASE_CLASSES, NULL, NULL, 0, 0, 0, NULL); }  // 0x2333fb (ReleaseUnitClass for every unit type)
    static void RegisterUnitClass(EEUnitClass* inClass) { ee_units_uc_rest(EE_UNITS_UC_DO_R_REGISTER_CLASS, NULL, NULL, H(inClass), 0, 0, NULL); }  // 0x236a9c (registers inClass in gUnitClassTable and sUnitTypes)
    // 0x2358b1: every unit class from dbobjects.dat (one EEUC* per record by its category), then their links and
    // unit-set flags; sUnitTypes sorted by name (Empire Earth.cpp 0x1424a8) (provisional name)
    static void CreateUnitClasses() { ee_units_uc_class(EE_UNITS_UC_DO_CREATE_UNIT_CLASSES, NULL, NULL, 0, NULL); }
    bool ProduceUnit(EEComplexUnit* inUnit, EEUnitClass* inClass, bool inArg) { return ee_units_uc_produce(this, inUnit, inClass, inArg); }  // original thiscall ret12
    void CreateDeathUnit(EEComplexUnit* inUnit) { ee_units_uc_body(EE_UNITS_UC_DO_CREATE_DEATH_UNIT, this, inUnit); }  // original class update ret4
    long GetPassableFlags(EEUnit* inUnit) { return ee_units_cls_passable_flags(this, inUnit != NULL ? inUnit->mStats : NULL); }  // 0x2349b1 (the CanMoveOnTerrain terrain bits)
    bool FindTargetNear(EEComplexUnit* inUnit, float inRange, EEUnit** outTarget) { return ee_units_uc_rest(EE_UNITS_UC_DO_R_FIND_TARGET_NEAR, this, inUnit, 0, UFloatBits(inRange), (int64_t)H(outTarget), NULL) != 0; }  // 0x234a05 (a target near the unit)
    long GetNumGoalQueues() { return ee_units_cls_num_goal_queues(this); }
    static void GetClassesInSet(unsigned long inSet, std::vector<EEUnitClass*>& outClasses) { ee_units_uc_call(EE_UNITS_UC_DO_CLASSES_IN_SET, NULL, NULL, H(&outClasses), inSet, 0, NULL, NULL); }
    void RegeneratePower(EEComplexUnit* inUnit) { ee_units_st_power(H(this), H(inUnit)); }  // 0x239ac7 (the unit regenerates mPower every 5 s)
    // 0x23094b: the terrain height for a new unit of this class at tile (inX, inY): the footprint's average (types
    // 0x2ba/0x34) or its levelled minimum (the mesh and the map tiles set to it) (provisional name)
    void LevelGround(GETerrainMesh* inMesh, unsigned long inX, unsigned long inY, float* outZ) { ee_units_lv_level_ground(H(this), H(inMesh), inX, inY, outZ); }
    void MoveToTileCentre(EEComplexUnit* inUnit) { ee_units_uc_call(EE_UNITS_UC_DO_MOVE_TO_TILE_CENTRE, this, inUnit, 0, 0, 0, NULL, NULL); }  // 0x23658b (to its tile centre, or a step forward; 0x22db0f) (provisional name)
    void CreateRunways(EEComplexUnit* inUnit, EEPlayer* inPlayer, bool inIsFoundation);  // 0x232e4a (CreateUnitAt for unit type 0x42; bool: CreateUnitAt passes its bool through) (provisional name)
    void ReplantFarm(EEComplexUnit* inUnit, EEPlayer* inPlayer) { ee_units_uc_call(EE_UNITS_UC_DO_REPLANT_FARM, this, inUnit, H(inPlayer), 0, 0, NULL, NULL); }  // 0x232c9f (EEMReplantFarms, farm classes)
    // EEUCProjectile.cpp's callees (provisional names)
    void HandleActionChanged(EEComplexUnit* inUnit) { EEUnitClassActionChanged(EE_UNITS_ACTION_BASE, this, inUnit); }  // 0x22db0f (the default +0x74 command handling)
    EEUnit* FindImpactUnit(EEComplexUnit* inUnit) { return (EEUnit*)(uintptr_t)ee_units_ucn_impact_unit(H(inUnit)); }  // 0x233410 (a target for inUnit, or 0)
    void CreateRemainsUnit(EEComplexUnit* inUnit) { ee_units_uc_call(EE_UNITS_UC_DO_CREATE_REMAINS_UNIT, this, inUnit, 0, 0, 0, NULL, NULL); }  // 0x237e79 (the class's +0x108 unit where inUnit ends)
    void UpdateAnimal(EEComplexUnit* inUnit) { ee_units_uc_body(EE_UNITS_UC_DO_UPDATE_ANIMAL, this, inUnit); }  // 0x239644 (EEUCLand::Update for a herd animal of a player) (provisional name)
    void UpdateCamouflageTimer(EEComplexUnit* inUnit) { ee_units_st_camouflage(H(this), H(inUnit)); }  // 0x23978b (EEUCSpaceship::Update)
    void TurnTurrets(EEComplexUnit* inUnit) { ee_units_uc_call(EE_UNITS_UC_DO_TURN_TURRETS, this, inUnit, 0, 0, 0, NULL, NULL); }  // 0x238948 (EEComplexUnit::UpdateTurrets: a unit whose model has parts) (provisional name)
    void UpdateBuilding(EEComplexUnit* inUnit) { ee_units_uc_body(EE_UNITS_UC_DO_UPDATE_BUILDING, this, inUnit); }  // 0x2394cd (EEUCBuilding::Update) (provisional name)
    // 0x234b05 (EEPickTileNear): the continent of the last of the unit's Virtual44 tiles that is on a kind-1 continent, a
    // non-9 terrain tile and inTarget's continent (when given); else the last continent looked at (provisional name)
    EEAIContinent* GetContainerContinent(EEComplexUnit* inUnit, EEComplexUnit* inTarget) { return (EEAIContinent*)(uintptr_t)ee_units_uc_rest(EE_UNITS_UC_DO_R_CONTAINER_CONTINENT, this, inUnit, H(inTarget), 0, 0, NULL); }
    void LaunchStoredMissile(EEComplexUnit* inUnit) { ee_units_ucs_call(EE_UNITS_UCS_DO_LAUNCH_STORED, this, inUnit, 0, NULL); }  // 0x20e50c (in EEUCBomb.cpp's range; FireShot: the carrier drops its first bomb) (provisional name)
    void LaunchShot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& inLocation, EEComplexUnit* inCargo) { ee_units_uc_call(EE_UNITS_UC_DO_LAUNCH_SHOT, this, inUnit, H(inTarget), (int64_t)H(inCargo), 0, (const float(*)[3])&inLocation.mX, NULL); }  // 0x2340a7 (LaunchStoredMissile) (provisional name)
    void FireOnAttackFrame(EEComplexUnit* inUnit, long inArg, long inFrame) { ee_units_uc_call(EE_UNITS_UC_DO_FIRE_ON_ATTACK_FRAME, this, inUnit, 0, inArg, inFrame, NULL, NULL); }  // 0x2386cf (the spaceship's attack commands)
    void UpdateDone(EEComplexUnit*) {}   // 0x157269 (empty, `ret 4`; ICF-folded; the last call of EEUCSpaceship::Update)
    // +0x78..+0xdc: arguments by `ret N` (inferred types)
    void AddToRegion(EEUnit* inUnit) { ee_units_uck_dispatch(EE_UNITS_BODY_ADD_TO_REGION, this, NULL, H(inUnit), 0, 0, NULL); }  // +0x80 0x2351cf (adds the unit to its map region)
    void AddToTile(EEUnit* inUnit) { ee_units_uck_dispatch(EE_UNITS_BODY_ADD_TO_TILE, this, NULL, H(inUnit), 0, 0, NULL); }  // +0x84 0x23523c (reads the unit's tile; EEUCResource adds it to its tiles)
    bool HasArrived(EEComplexUnit* inUnit, float inValue) { return ee_units_ucn_has_arrived(H(this), H(inUnit), inValue); }  // +0x88
    void BadlyHurt(EEUnit* inUnit) { ee_units_uck_dispatch(EE_UNITS_BODY_BADLY_HURT, this, NULL, H(inUnit), 0, 0, NULL); }  // +0x8c (EEUCLandTransport: its cargo)
    // +0x90 0x2369b9; the splash damage 0x230fd2 passes (the hit complex unit, the attacker, the damage)
    void UnitDamaged(EEComplexUnit* inUnit, EEComplexUnit* inAttacker, long inDamage) { ee_units_st_unit_damaged(H(this), H(inUnit)); }
    void RemoveFromRegion(EEUnit* inUnit) { ee_units_uck_dispatch(EE_UNITS_BODY_REMOVE_FROM_REGION, this, NULL, H(inUnit), 0, 0, NULL); }  // +0x94 0x236ac6 (removes the unit from its map region)
    void RemoveFromTile(EEUnit* inUnit) { ee_units_uck_dispatch(EE_UNITS_BODY_REMOVE_FROM_TILE, this, NULL, H(inUnit), 0, 0, NULL); }  // +0x98 0x236b55 (the unit leaves its tile)
    void FleeAttacker(EEComplexUnit* inUnit) { ee_units_mv_flee_attacker(H(this), H(inUnit)); }  // +0x9c
    long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent) { return (long)ee_units_uck_call(EE_UNITS_UCK_DO_EVALUATE_CONDITION, this, inUnit, 0, (int64_t)inEvent, 0, 0.0f); }  // +0xa0
    void LoadButtons(EEPlayer* inPlayer) { ee_units_uc_call(EE_UNITS_UC_DO_LOAD_BUTTONS, this, NULL, H(inPlayer), 0, 0, NULL, NULL); }  // +0xa4 0x235527 (loads the class buttons and calamity media for inPlayer)
    void LoadMedia(EEPlayer* inPlayer) { ee_units_uck_dispatch(EE_UNITS_BODY_LOAD_MEDIA, this, NULL, H(inPlayer), 0, 0, NULL); }  // +0xa8
    bool LaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation) { return ee_units_uck_dispatch(EE_UNITS_BODY_LAUNCH_PROJECTILE, this, inUnit, H(&inLocation), 0, 0, NULL) != 0; }  // +0xac
    void UnloadButtons(EEPlayer* inPlayer) { ee_units_uc_call(EE_UNITS_UC_DO_UNLOAD_BUTTONS, this, NULL, H(inPlayer), 0, 0, NULL, NULL); }  // +0xb0
    void UnloadMedia(EEPlayer* inPlayer) { ee_units_uck_dispatch(EE_UNITS_BODY_UNLOAD_MEDIA, this, NULL, H(inPlayer), 0, 0, NULL); }  // +0xb4
    void Update(EEComplexUnit* inUnit) { ee_units_uck_call(EE_UNITS_UCK_DO_UPDATE, this, inUnit, 0, 0, 0, 0.0f); }  // +0xb8 (EEUCLandTransport: its wheels)
    void UpdateEffects(EEComplexUnit* inUnit) { ee_units_uck_call(EE_UNITS_UCK_DO_UPDATE_EFFECTS, this, inUnit, 0, 0, 0, 0.0f); }  // +0xbc (the EEUC* bodies read a complex unit: EEUCProjectile 0x219ae9)
    long Animate(EEComplexUnit* inUnit) { return (long)ee_units_uc_call(EE_UNITS_UC_DO_ANIMATE, this, inUnit, 0, 0, 0, NULL, NULL); }  // +0xc0 0x238ce7 (returns a value: EEUCSpaceship::Update keeps it)
    void UnitDied(EEComplexUnit* inUnit) { ee_units_uc_call(EE_UNITS_UC_DO_UNIT_DIED, this, inUnit, 0, 0, 0, NULL, NULL); }  // +0xc4 0x233f0e (the unit dies: its death class, effect, sound)
    void FindSafeLocation(EEComplexUnit* inUnit) { ee_units_uc_call(EE_UNITS_UC_DO_FIND_SAFE_LOCATION, this, inUnit, 0, 0, 0, NULL, NULL); }  // +0xc8 0x234000 (a computer player's unit moves off)
    void NextWaypoint(EEComplexUnit* inUnit) { ee_units_mv_next_waypoint(H(this), H(inUnit)); }  // +0xcc
    void Heal(EEComplexUnit* inUnit) { ee_units_uc_body(EE_UNITS_UC_DO_HEAL, this, inUnit); }  // +0xd0
    void PathToGoal(EEComplexUnit* inUnit) { ee_units_mv_path_to_goal(H(this), H(inUnit)); }  // +0xd4
    void FindPath(EEComplexUnit* inUnit) { ee_units_mv_find_path(H(this), H(inUnit)); }  // +0xd8
    void Attacked(EEComplexUnit* inUnit) { ee_units_ucn_attacked(H(this), H(inUnit)); }    // +0xdc
    // The kinds' own virtuals (the EEUC* families' vtable slots until R5b.4 round 32), dispatchers by kind like
    // the ones above (EEUnitClassKinds.cpp); a kind without a body answers the default
    void CallParatroopPlane(EEComplexUnit* inUnit) { ee_units_uck_dispatch(EE_UNITS_BODY_CALL_PARATROOP_PLANE, this, inUnit, 0, 0, 0, NULL); }  // (EEUCLand)
    bool CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY) { return ee_units_ucn_can_unload_at(H(this), H(inTransport), (int32_t)inX, (int32_t)inY); }  // (EEUCTransport)
    bool CheckGroundImpact(EEComplexUnit* inUnit) { return ee_units_uck_dispatch(EE_UNITS_BODY_CHECK_GROUND_IMPACT, this, inUnit, 0, 0, 0, NULL) != 0; }  // (EEUCMissile)
    bool CheckWaterImpact(EEComplexUnit* inUnit) { return ee_units_uck_dispatch(EE_UNITS_BODY_CHECK_WATER_IMPACT, this, inUnit, 0, 0, 0, NULL) != 0; }  // (EEUCMissile)
    void FilterLoadable(std::vector<EEComplexUnit*>* ioUnits, EEComplexUnit* inTransport, std::vector<EEComplexUnit*>* outRejected) { ee_units_ucn_filter_loadable(H(this), H(ioUnits), H(inTransport), H(outRejected)); }  // (EEUCTransport)
    bool FindBoardingLocation(EEComplexUnit* inUnit, EEComplexUnit* inTransport, GE3DPoint& inLocation, GE3DPoint& outLocation) { return ee_units_ucn_boarding_location(H(this), H(inUnit), H(inTransport), &inLocation.mX, &outLocation.mX); }  // (EEUCTransport)
    void FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport, U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile) { ee_units_ucn_rendezvous_tiles(H(this), H(inUnits), H(inTransport), (long(*)[2])outTransportTile, (long(*)[2])outUnitTile); }  // (EEUCTransport)
    void FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination, U2DPoint<long>* outUnloadTile, U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile) { ee_units_ucn_unload_tiles(H(this), H(inTransport), (const long(*)[2])inDestination, (long(*)[2])outUnloadTile, (long(*)[2])outFirstCargoTile, (long(*)[2])outTile); }  // (EEUCAirplane)
    long GetUnloadFailedTextID() { return (long)ee_units_uck_dispatch(EE_UNITS_BODY_GET_UNLOAD_FAILED_TEXT_ID, this, NULL, 0, 0, 0, NULL); }  // (EEUCTransport)
    bool HasRoom(EEComplexUnit* inUnit, EEComplexUnit* inCargo) { return ee_units_ucn_has_room(H(this), H(inUnit), H(inCargo)); }  // (EEUCTransport)
    bool HasSunk(EEComplexUnit* inUnit) { return ee_units_uck_dispatch(EE_UNITS_BODY_HAS_SUNK, this, inUnit, 0, 0, 0, NULL) != 0; }  // (EEUCNavalBuilding)
    bool SelectUnitToRelease(EEComplexUnit* inUnit) { return ee_units_ucn_select_to_release(H(this), H(inUnit)); }  // (EEUCBuilding)
    void UnloadAt(EEComplexUnit* inTransport, long inX, long inY) { ee_units_uck_dispatch(EE_UNITS_BODY_UNLOAD_AT, this, inTransport, 0, inX, inY, NULL); }  // (EEUCTransport)
    // The kinds' bodies (the EEUC* classes' members until R5b.4 round 32: EEUnitClassAir, -Buildings,
    // -Transports, -Shots, -People and -Ground.cpp); the overrides' run behind the dispatchers
    bool AirAnimalReturnToAirport(EEComplexUnit* inUnit) { return ee_units_uca_call(EE_UNITS_UCA_DO_AIR_ANIMAL_RETURN, this, inUnit, 0, 0, 0, NULL) != 0; }
    void FlyOffMap(EEComplexUnit* inUnit) { ee_units_uca_call(EE_UNITS_UCA_DO_FLY_OFF_MAP, this, inUnit, 0, 0, 0, NULL); }
    void UpdateCitizen(EEComplexUnit* inUnit) { ee_units_ucp_call(EE_UNITS_UCP_DO_UPDATE_CITIZEN, this, inUnit, 0, 0); }
    EEUnit* FindNextResource(EEComplexUnit* inUnit, EEUnit* inResource, unsigned long inKind, std::vector<EEUnit*>* ioUnits) { return (EEUnit*)(uintptr_t)ee_units_uc_rest(EE_UNITS_UC_DO_R_FIND_NEXT_RESOURCE, this, inUnit, H(inResource), (int64_t)inKind, (int64_t)H(ioUnits), NULL); }
    EEUnit* FindStorage(EEComplexUnit* inUnit, bool inAnyStorage) { return (EEUnit*)(uintptr_t)ee_units_ucn_find_storage(H(this), H(inUnit), inAnyStorage); }
    bool HasGatherPoint(EEComplexUnit* inUnit, EEUnit* inResource) { return ee_units_ucn_has_gather_point(H(this), H(inUnit), H(inResource)); }
    EEUnit* FindResourceOfKind(EEComplexUnit* inUnit, EEUnit* inCenter, EEUnit* inResource, long inRange, unsigned long inKind, std::vector<EEUnit*>* ioUnits, bool inFarms) { return (EEUnit*)(uintptr_t)ee_units_ucn_resource_of_kind(H(this), (uint32_t)this->mCategory, H(inUnit), H(inCenter), H(inResource), (int32_t)inRange, (uint32_t)inKind, H(ioUnits), inFarms); }
    void BasicPriestUpdateConversion(EEComplexUnit* inUnit) { ee_units_ucp_call(EE_UNITS_UCP_DO_UPDATE_CONVERSION, this, inUnit, 0, 0); }
    void BasicPriestHideConversionEffects(EEComplexUnit* inUnit) { ee_units_ucp_call(EE_UNITS_UCP_DO_HIDE_CONVERSION, this, inUnit, 0, 0); }
    EEUnit* FindNearestTargetBuilding(EEUnit* inUnit) { return (EEUnit*)(uintptr_t)ee_units_ucn_nearest_target_building(H(this), H(inUnit)); }
    bool EruptVolcano(EEComplexUnit* inUnit) { return ee_units_uc_rest(EE_UNITS_UC_DO_R_ERUPT_VOLCANO, this, inUnit, 0, 0, 0, NULL) != 0; }
    void ReleaseUnit(EEComplexUnit* inUnit) { ee_units_ucb_call(EE_UNITS_UCB_DO_RELEASE_UNIT, this, inUnit, 0, 0, 0, NULL); }
    void UpdateArmed(EEComplexUnit* inUnit) { ee_units_ucb_call(EE_UNITS_UCB_DO_UPDATE_ARMED, this, inUnit, 0, 0, 0, NULL); }
    EEComplexUnit* GetSecondPart(EEComplexUnit* inGate) { return (EEComplexUnit*)(uintptr_t)ee_units_uc_rest(EE_UNITS_UC_DO_R_SECOND_PART, this, NULL, H(inGate), 0, 0, NULL); }
    EEComplexUnit* GetFourthPart(EEComplexUnit* inGate) { return (EEComplexUnit*)(uintptr_t)ee_units_uc_rest(EE_UNITS_UC_DO_R_FOURTH_PART, this, NULL, H(inGate), 0, 0, NULL); }
    EEComplexUnit* GetPart(EEComplexUnit* inGate, long inIndex) { return (EEComplexUnit*)(uintptr_t)ee_units_ucn_gate_part(H(this), H(inGate), inIndex); }
    bool AreEnemiesNear(EEComplexUnit* inUnit) { return ee_units_ucn_gate_near(H(this), H(inUnit), false); }
    bool IsPasserNear(EEComplexUnit* inUnit) { return ee_units_ucn_gate_near(H(this), H(inUnit), true); }
    unsigned long GetNextFlightState(EEComplexUnit* inUnit) { return ee_units_ucn_next_flight_state(H(this), H(inUnit)); }
    bool IsBombOnTarget(EEComplexUnit* inUnit) { return ee_units_ucn_bomb_on_target(H(this), H(inUnit)); }
    bool IsAwayFromGoal(EEComplexUnit* inUnit) { return ee_units_ucn_away_from_goal(H(this), H(inUnit)); }
    void MissileActionChanged(EEComplexUnit* inUnit) { EECombatShotStep(EE_COMBAT_SHOT_MISSILE, false, EE_COMBAT_SO_MISSILE_ACTION, this, inUnit); }
    long MissileEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);
    void MissileUpdate(EEComplexUnit* inUnit) { EECombatShotStep(EE_COMBAT_SHOT_MISSILE, true, EE_COMBAT_SO_MISSILE_UPDATE, this, inUnit); }
    void UpdateResourceUnit(EEComplexUnit* inUnit) { ee_units_ucg_call(EE_UNITS_UCG_DO_UPDATE_RESOURCE_UNIT, this, inUnit, 0, 0.0f); }
    void ProjectileActionChanged(EEComplexUnit* inUnit) { EECombatShotStep(EE_COMBAT_SHOT_PROJECTILE, false, EE_COMBAT_SO_PROJECTILE_ACTION, this, inUnit); }
    void ProjectileUpdate(EEComplexUnit* inUnit) { EECombatShotStep(EE_COMBAT_SHOT_PROJECTILE, true, EE_COMBAT_SO_PROJECTILE_UPDATE, this, inUnit); }
    void AirGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch);
    void TorpedoUpdate(EEComplexUnit* inUnit) { EECombatShotStep(EE_COMBAT_SHOT_TORPEDO, true, EE_COMBAT_SO_TORPEDO_UPDATE, this, inUnit); }
    bool CanLoad(EEComplexUnit* inTransport, EEComplexUnit* inUnit) { return ee_units_ucn_can_load(H(this), H(inTransport), H(inUnit)); }
    bool CanUnloadNear(EEComplexUnit* inTransport, EEComplexUnit* inCargo, U2DPoint<long>* ioUnloadTile, U2DPoint<long>* ioFirstCargoTile, U2DPoint<long>* inDestination) { return ee_units_ucn_can_unload_near(H(this), H(inTransport), H(inCargo), (const long(*)[2])ioUnloadTile, (const long(*)[2])ioFirstCargoTile, (const long(*)[2])inDestination); }
    bool IsSidePart(EEComplexUnit* inGate) { return inGate->mNextPart == NULL && inGate->mPreviousPart != NULL || this->IsMiddlePart(inGate); }    // the gate's (EEAIAStarBoundedPathGoalModifiers.cpp)
    bool IsMiddlePart(EEComplexUnit* inGate) { return inGate->mNextPart != NULL && inGate->mNextPart->mNextPart == NULL && inGate->mPreviousPart != NULL; }   // second to last with a part before it
    // the base bodies of the dispatched virtuals
    void BaseGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch) { ee_units_uc_rest_sound(EE_UNITS_UC_SOUND_BASE, this, inUnit, inSound, outVolume, outPan, outPitch); }
    void BaseSimpleUnitCreated(EEUnit* inUnit) { inUnit->SetHitPoints(inUnit->mStats->mMaxHitPoints); }

    // inline (EEContagionFunctor::CanInfect 0xb66ff loads both before the virtual call)
    unsigned long GetCategory() const { return this->mCategory; }
    long GetUnitType() const { return this->mUnitType; }  // inline (conscription 0xafed3: loaded before GetUnitSet())
    long GetTileSize() const { return this->mTileSize; }  // inline (the teleport loads it before the other pushes) (provisional name)
    float GetCruiseAltitude() const { return this->mCruiseAltitude; }  // inline (the airplanes' scale by height loads it as a value: fld; fdivp; EEPMAirplane 0x55726c) (provisional name)
    float GetShadowScale() const { return this->mShadowScale; }  // inline (EERenderObject copies it with fld/fstp) (provisional name)
    float GetF170() const { return this->mL170; }  // inline (likewise) (provisional name)
    float GetInnerCollisionSphere() const { return this->mInnerCollisionSphere; }  // inline (EEComplexUnit::SetGrowth copies it with fld/fstp) (provisional name)
    // inline: its COMDAT 0x1cf0a2 follows its first user EETechTreeNode::GetCost (`push 1; pop eax` of an ||
    // chain) (provisional name)
    bool HasRallyPoint() { return (this->IsLandBuilding() && this->mCanSetRallyPoint) || ((this->mCategory == kUnitCategoryNavalBuilding || this->mCategory == kUnitCategorySpaceBuilding) && this->mAirTrafficType != 0) || this->mUnitType == kUnitTypeCpMissileBase; }
    // 0x234084: plays the class's +0x94 effect on inUnit (the meteor storm's new meteor) (provisional name)
    void PlayCreationEffect(EEUnit* inUnit);
    // 0x211747: the last unit of inUnit's +0x1d0 chain; 0x211723: that unit's +0x1cc (the logic bomb; ecx unused)
    // (provisional names)
    EEComplexUnit* GetLastLinked(EEComplexUnit* inUnit) { return (EEComplexUnit*)(uintptr_t)ee_units_uc_rest(EE_UNITS_UC_DO_R_LAST_LINKED, this, NULL, H(inUnit), 0, 0, NULL); }
    EEComplexUnit* GetLastLinkedCarrier(EEComplexUnit* inUnit) { return this->GetLastLinked(inUnit)->mPreviousPart; }
    // the 29 graphic effects (+0x64..+0xd4, mAttachedEffect to mBloodStainEffect) as one array, in member order: the
    // dtor's, LoadClassMedia's and UnloadClassMedia's walks (EEUnitClass.cpp checks the layout)

    // Layout (no gaps; sizeof 0x32c: every subclass EEUC* is 0x32c): the ctor 0x22f317
    // (EEUnitClass(bool, record, ...): it copies a database record field by field) and the dtor 0x2305ee. Types by
    // the stores' width (float: fld/fstp); member names are provisional.
    UString mAIScriptName;                  // +0x008 the ctor's member ctor, then UString::Copy
    UString mName;                          // +0x018 the ctor's member ctor, then UString::Copy
    std::vector<long> mResearchTechnologies;  // +0x02c 4-byte elements: the ctor's `_Buy(0)`
    // +0x060 mAIScript .. +0x118 mBehavior (the effects, media, related classes, physics model, improvements,
    // behaviour): the Rust record EeUnitsUnitClassLinks (the third base above)
    EEUnitBehavior* GetBehavior() const { return this->mBehavior; }
    // inline (EEE3DDamageVent::SetUp 0xeabb5 loads it as a value before the effect's height)
    // (provisional name)
    float GetScale() const { return this->mScale; }
    float GetMaxVelocity() const { return this->mMaxVelocity; }  // inline (read as a value)
    float GetRange() const { return this->mRange; }  // inline (EESMObject 0x5c8871)
    float GetLineOfSight() const { return this->mLineOfSight; }
    bool GetIsTree() const { return this->mIsTree; }  // inline (EESMEffect 0x5c2b3e)
    bool GetCanSetRallyPoint() const { return this->mCanSetRallyPoint; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    bool GetIsTower() const { return this->mIsTower; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    bool GetIsMedic() const { return this->mIsMedic; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    long GetAnimalID() const { return this->mAnimalID; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    unsigned long GetFamily() const { return this->mFamily; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    long GetAirTrafficType() const { return this->mAirTrafficType; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    long GetAttack() const { return this->mAttack; }  // inline (EESMEffect 0x5c2507's free sites)
    bool GetIsProjectile() const { return this->mIsProjectile; }
    unsigned char GetDomains() const { return this->mDomains; }
    float GetAcceleration() const { return this->mAcceleration; }  // inline (read as a value)
    float GetMaxAngularVelocity() const { return this->mMaxAngularVelocity; }  // inline (read as a value)
    float GetAngularAcceleration() const { return this->mAngularAcceleration; }  // inline (EEPMNaval::PitchMode 0x55c146 loads it as a value: fld; fdivp) (provisional name)
    // inline (the heal goal's Virtual64 0x22569f loads it as an FP value after the ratio; provisional name)
    float GetHealThreshold() const { return this->mHealThreshold; }
    bool GetIsWall() const { return this->mIsWall; }
    volatile bool mIsMediaLoaded;           // +0x328 the ctor stores it (volatile: VirtualB4 0x638432 clears it with `mov byte`, not `and`)
};

// EEComplexUnit::Step (inline; needs EEUnitClass): EEPhysics::UpdateUnits calls it for each unit of gWorld's +0xadc
// deque (provisional name)
inline void EEComplexUnit::Step()
{
    if (!this->mIsDisabled)
        this->AIStep();
    this->mUnitClass->Update(this);
}

#endif
