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
#include <vector>
#include <utility>
#include "lle/UList.h"                  // EEComplexUnit+0xc4
#include "lle/U2DPoint.h"               // EEComplexUnit+0x1a4
#include "lle/GEColor.h"                // EEComplexUnit+0x1ac
#include "ee/EEUnitClassIDs.h"
class GE3DLine;
template <class T> class U2DPoint;      // (EEComplexUnit::DistanceSquaredToTile)
class GEViewport;
class GETerrainMesh;
#include "lle/GEPhysicalModel.h"        // EEComplexUnit+0x130 (it uses GE3DLine/GEViewport by reference)

class EEComplexUnit;
class EEHerd;                               // ee/EEHerd.h (EEComplexUnit+0x200)
struct EEPUnitSwap;                         // ee/EEPhysics.h (ReplaceUnits)
template <class T> class UMemoryPool;   // lle/UMemoryPool.h (EEComplexUnit::sMemoryPool)
class EEGraphicEffect;
class EEMedia;
class SESMInstanceNode;                 // lle/SESMInstanceNode.h (EEComplexUnit::SetSound)
class EEOrbital;                            // ee/EEGraphicObject.h (EEComplexUnit+0x1b8)
class EEWheel;                              // ee/EEWheel.h (EEComplexUnit+0x1bc)
class EEGraphicObject;                      // ee/EEGraphicObject.h
typedef EEGraphicObject EEModel;            // (see below: WriteDetailsHook's)
class EEActiveGroup;
struct EECPASGFlags;                        // ee/EECPAttackSubGroup.h (EEUnitClass::GetTerrainFlags)
class EEFleeFromLocationScreenBase;         // ee/EEFleeFromLocationScreenBase.h (EEComplexUnit::FindFleePoint)
struct EEPlayerUnitInfo;
class EETechTreeNode;
class EEUnit;
class EEPPatrolGroup;
class EETown;
#include "ee/EEUnitAction.h"             // EEComplexUnit::mCommand (+0x1e8; GetCommandType reads its mType inline)
class EEUnitBehavior;                      // ee/EEUnitBehavior.h (EEComplexUnit+0x208, EEUnitClass+0x118)
class EEUnitVectorItem;                     // the 0x28-byte polymorphic elements of EEComplexUnit+0x90 (ee/EEUnitVectorItem.h)
class GEModel;
class EETagPointList;                       // ee/EEGraphicObject.h (EEModel::FnD84EC)
#include "ee/EETileCoordinate.h"     // RTSUnit::GetTile returns one
class EEPlayer;
class EEUnitClass;
class EEUnitGoal;
class EEAIContinent;                        // ee/EEAIContinent.h (EEUnitClass::GetContainerContinent)
class EEPhysicsModel;                       // ee/EEPhysicsModel.h (EEUnitClass +0x110)
class EESoundObject;

#include "ee/EEUnitClassImprovement.h"

// What +0x34 of a unit points to: the owner's RTTI EEUnitClassUpgrade for the unit's class (vtable 0x449ad8, sizeof
// 0x16c, ctor 0x23b6ae(EEUnitClass*); EEPlayer's Reset news one per gUnitClassTable entry into its vector at +0x884).
// Proven: every method called on it (0x23b82b, 0x23be6f..0x23c185) sits in EEUnitClassUpgrade.cpp
// 0x23b61a..0x23c332, and +0x04 is the embedded EEUnitClassImprovement (GetLevel on &mStats->+4). Layout
// gap-free from the ctor, the dtor 0x23b729 (deletes +0x68..+0x80 through slot +0x08 when mL154 > 0) and Init
// 0x23b891 (copies from mUnitClass; `layout.py EEUnitClassUpgrade`); member types by those stores, (provisional names)
// (the names users rely on are kept). EEUnitStats stays as a typedef for the old spelling.
class EEButtonObject;
class FSFileSpec;
class EEUnitClassUpgrade {
public:
    EEUnitClassUpgrade(EEUnitClass* inUnitClass);   // 0x23b6ae (then Init)
    virtual ~EEUnitClassUpgrade();          // +0x00 dtor 0x23b729 (??_G 0x23b70d)
    virtual void CopyUpgradeLevels(EEUnitClassUpgrade* inOther);  // +0x04 0x23b7a4 (forwards to mImprovement)

    void SetName(UWideString inName) { this->mName = inName; }  // inline COMDAT 0x1bcb00
    void Init();                            // 0x23b891 (the fields below from mUnitClass)
    EESoundObject* GetSelectionSound(long inIndex);  // 0x23b82b (one of mSelectionSounds; EEMSavedGroupSelect passes -1 for EEPlayForLocalPlayer; provisional name)
    EESoundObject* GetAcknowledgeSound(long inIndex);  // 0x23b806 (one of mAcknowledgeSounds; EEPlayer::PlayAcknowledgement passes -1 for EEPlayForLocalPlayer; provisional name)
    // the per-class forwarders' targets (EEPlayer 0x163db9..0x163e71; provisional names)
    void LoadButtons(EEPlayer* inPlayer);   // 0x23bc29 (EEPlayer::LoadUnitClassButtons; provisional name)
    void LoadMedia(EEPlayer* inPlayer, long inMediaFlags);  // 0x23bcc3 (EEPlayer::SetUnitClassMedia; provisional name)
    void UnloadButtons(EEPlayer* inPlayer);  // 0x23c1ef (EEPlayer::UnloadUnitClassButtons; provisional name)
    // EEUnitClassUpgrade.cpp (provisional names)
    EEUnitClassImprovement* GetImprovement();   // 0x23b827 (&mImprovement; defined after its first user Virtual04, so called)
    void DumpMediaCount(FSFileSpec& inFile);  // 0x23b7c1 (the media-count dump line)
    void ChangeLevel(long inImprovement, long inDelta);  // 0x23b87a (mImprovement.ChangeLevel)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x23bbf9 (mName, then mImprovement)
    void SetMaxVelocity(float inValue);    // 0x23c005 (EEUnitClassImprovement::ChangeLevel: improvement 14) (provisional name)
    void SetRange(float inValue);          // 0x23c101 (EEUnitClassImprovement::ChangeLevel: improvement 11) (provisional name)
    void UnloadMedia(EEPlayer* inPlayer);   // 0x23c24f (EEPlayer::UnloadUnitClassMedia; provisional name)
    EEUnitClass* GetUnitClass();            // 0x1f1f81 (mUnitClass; ICF-shared; EEScenarioManager::Prepare's trace) (provisional name)
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x23be43 (EEPlayer::Save; provisional name)
    void SetAcknowledgeSound(EESoundObject* inSound, long inIndex);  // 0x23be6f (mAcknowledgeSounds[inIndex])
    void SetCreationSound(EESoundObject* inSound);  // 0x23bed9 (mCreationSound)
    void SetDeathSound(EESoundObject* inSound);  // 0x23bf3d (mDeathSound)
    void SetModel(EEModel* inModel);        // 0x23bfa1 (mModel)
    void SetButton(EEButtonObject* inButton);   // 0x23c09d (sets the button +0x64: Release on the old, Acquire on the new)
    void SetSelectionSound(EESoundObject* inSound, long inIndex);  // 0x23c185 (mSelectionSounds[inIndex])
    // the improvement button's help text (EEUIBUpgrade 0x256c83; provisional names)
    void GetImprovementName(UWideString& outName, long inImprovement);  // 0x23b864 (ret 8)
    void GetImprovementGainText(UWideString& outText, long inImprovement, long inLevel);  // 0x23b84c (ret 0xc)
    // inline: EETechTreeNode::GetCost loads it first (fld; fimul; provisional name)
    float GetCostScale() const { return this->mCostScale; }
    // inline: EEUGAttackUnit::EvaluateCondition keeps it in a temporary across the GetLineOfSight call (0x21dce8)
    float GetRange() const { return this->mRange; }
    float GetScale() const { return this->mScale; }  // inline (EESGUnitSorter::Compare loads both operands)
    float GetAttack() const { return this->mAttack; }  // inline (likewise)
    float GetLeadership() const { return this->mLeadership; }  // inline (EESMObject 0x5c8871)
    long GetMaxHitPoints() const { return this->mMaxHitPoints; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    float GetAreaEffect() const { return this->mAreaEffect; }  // inline (0x66cbe1)
    // inline (EEWheel::Update 0x23c957 loads them as values) (provisional names)
    float GetMaxVelocity() const { return this->mMaxVelocity; }
    float GetInverseMaxAngularVelocity() const { return this->mInverseMaxAngularVelocity; }
    float GetInverseMaxVelocity() const { return this->mInverseMaxVelocity; }
    float GetAcceleration() const { return this->mAcceleration; }  // inline (EEPMMissile::Accelerate 0x55b041 loads it as a value) (provisional name)
    float GetLineOfSight() const { return this->mLineOfSight; }  // inline (EEActiveGroup::SpreadAroundUnit loads it after GetRange as a value) (provisional name)

    EEUnitClassImprovement mImprovement;    // +0x04 (ctor 0x23a5bf)
    UWideString mName;                      // +0x4c (= "" in the ctor)
    EEUnitClass* mUnitClass;                // +0x60 (ctor argument; Init reads its +0xd8.. fields)
    EEButtonObject* mButton;                // +0x64 (Init: mUnitClass +0xdc) the unit's portrait button (EEUIBSavedGroup::SetUnit 0x2560e1 calls EEButtonObject::SetFaceBitmap on it)
    EEModel* mModel;                        // +0x68 (EEUnit::GetModel's; the doppelgangers call its slots +4/+8)
    EESoundObject* mCreationSound;          // +0x6c (a sound: EEUCBasicPriest 0x20d6c9 plays its sample; from mUnitClass +0xe0)
    EESoundObject* mDeathSound;             // +0x70 (EEUnitClass::UnitDied plays it where the unit dies)
    EESoundObject* mAcknowledgeSounds[2];   // +0x74 (the dtor's loop of two; GetAcknowledgeSound hands one to EEPlayForLocalPlayer, which plays it)
    EESoundObject* mSelectionSounds[2];     // +0x7c (likewise GetSelectionSound)
    float mAreaEffectScale;                 // +0x84
    float mConversionResistance;            // +0x88
    float mConversionRate;                  // +0x8c
    float mCostScale;                       // +0x90
    float mBuildTimeScale;                  // +0x94
    float mScale;                           // +0x98 (Init: mUnitClass +0x130)
    float mRange;                           // +0x9c (EEUGAttackGround::EvaluateCondition: > 1)
    float mAreaEffect;                      // +0xa0
    float mLineOfSight;                     // +0xa4
    float mAcceleration;                    // +0xa8
    float mMaxVelocity;                     // +0xac (> 0: the hot keys' unit selection takes the unit)
    float GetAngularAcceleration() const { return this->mAngularAcceleration; }  // inline (EEActiveGroup::ComputeValues 0x5ed692 copies it through fld/fstp)
    float mAngularAcceleration;             // +0xb0
    float GetMaxAngularVelocity() const { return this->mMaxAngularVelocity; }  // inline (mFAC / mFB4 sites read both as values)
    float GetCameraFocus() const { return this->mCameraFocus; }  // inline (EECameraControl::GetUnitPoint 0x52844f: fld; fmulp) (provisional name)
    float mMaxAngularVelocity;              // +0xb4
    float mTurretMaxAngularVelocity;        // +0xb8
    float mBC;                              // +0xbc (EEComplexUnit::SetVelocity keeps +0x21c at least this)
    float mLaunchSpeed;                     // +0xc0 (EEPMProjectile::Step0 0x15d106: fld, compared with 0.001f)
    float mInverseAcceleration;             // +0xc4
    float mInverseAngularAcceleration;      // +0xc8
    float mInverseMaxAngularVelocity;       // +0xcc
    float mInverseMaxVelocity;              // +0xd0
    float mInverseTurretMaxAngularVelocity;  // +0xd4
    float mAttack;                          // +0xd8
    float mLeadership;                      // +0xdc
    float GetArmor(long inKind) const { return this->mArmor[inKind]; }  // inline (0x63a8a8, 0x63afc1) (provisional name)
    float mArmor[6];                        // +0xe0 per weapon kind (EEPhysicsModel::Push 0x15607f indexes it by the attacker class's mWeapon) (provisional name)
    float mCameraFocus;                      // +0xf8
    long mAreaEffectTemplates[4];          // +0xfc..108, indexed independently by Init and SelectedGroup
    long mWonderUnitSet;                    // +0x10c
    long mGatherTimes[7];                   // +0x110 (Init's loop copies mUnitClass +0x21c..+0x238)
    long mPowerRegeneration;                // +0x12c
    long mFlightTime;                       // +0x130
    long mWorkRate;                         // +0x134
    long mReloadTime;                       // +0x138 (EEUGAttackGround::Step passes it to SetRandomReloadTimer)
    long mMaxObjectCarry;                   // +0x13c
    long mRepairRate;                       // +0x140
    long mMaxHitPoints;                     // +0x144 provisional name (aircraft selection)
    long mPower;                            // +0x148
    long mCarryAmount;                      // +0x14c
    long mHealingPoints;                    // +0x150
    long mMediaCount;                       // +0x154 (EEPlayer::LoadUnitClassMedia: loaded once; the dtor's guard; signed: EEUnitClassUpgrade's `> 0` tests are jle)
    unsigned long GetMediaCount() const { return this->mMediaCount; }  // inline (EEPlayer::LoadAllClassMedia 0x167c16 compares it unsigned: jbe) (provisional name)
    long mButtonLoadCount;                  // +0x158 (EEPlayer::SwapClassButtonMedia: > 0)
    long mRegeneration;                     // +0x15c
    unsigned long GetTerrainMask() const { return this->mTerrainMask; }
    unsigned long mTerrainMask;             // +0x160 (unsigned: `>> 14` is shr in 0x23198b/0x2292c9/0x23b891)
    bool mCanFireOverWalls;                 // +0x164 (EEUGAttackGround::Step)
    bool mBuildsOnElevation;                // +0x165
    bool mConvertsBuildings;                // +0x166
    bool mConvertsPriests;                  // +0x167
    bool GetIsInitialized() const { return this->mIsInitialized; }
    bool mIsInitialized;                    // +0x168
    volatile bool mIsLoadingMedia;          // +0x169 (ctor: false; a busy flag polled with Sleep(1): volatile, 0x63bcc3)
    volatile bool mIsLoadingButtons;        // +0x16a (ctor: false; 0x23bc29 sets it while loading the media and the
                                            //  other loaders poll it with Sleep(1): `mov byte ptr`, re-read each pass)
};
typedef EEUnitClassUpgrade EEUnitStats;

// What EEUnit::GetModel returns (mStats +0x68): the unit class's EEGraphicObject (ee/EEGraphicObject.h; slots 0/4/8
// are EEMedia's, its +0x244 part is mModels[0]). EEModel was its provisional name here (merged into
// EEGraphicObject.h; the typedef is at the top of this header)

#include "ee/RTSUnit.h"                // RTSUnitLink, RTSUnit (the RTS layer's header since R5a.6)

class EEUnit : public RTSUnit {
public:
    // the save keys (EEUnit.cpp's `$E` chains 0x228c4c/0x228c8f, one guard byte 0x963630 bits 1/2)
    static const UString sData;             // "EEUnit-Data" (0x963620) (provisional name)
    static const UString sHitPoints;        // "EEUnit-HitPoints" (0x963638) (provisional name)
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
    float GetNearestFootprintTile(EEUnit* inUnit, long* outX, long* outY);  // 0x229314; base-unit footprint distance
    float GetTileDistanceSquared(EEUnit* inOther);  // 0x14f455 (EEAirTrafficControl.cpp: the squared tile distance; provisional name)
    bool IsPathMarker();                    // 0x708c9 (EECPCivilizationManager.cpp; every caller out of line): unit type 0x104 or 0xdf (provisional name)
    bool Fn27C0B4();                        // 0x27c0b4 (ICF-folded `return false`; EERenderObject 0x10b556 tests it for class 0xdf) (provisional name)

    // the RTSUnit slots EEUnit overrides (EEUnit.cpp)
    virtual ~EEUnit();                      // +0x00 0x228d36 (calls Virtual30; ??_G 0x228d1a)
    virtual void Invalidate();              // +0x04 0x228cf6: EEUnit::Remove(false)
    // +0x08 0x228ef5: the stats, hit points (scaled to this unit's maximum) and mL40 of inUnit (an EEUnit)
    virtual void CopyFrom(RTSUnit* inUnit);
    virtual void SightOnHere();             // +0x0c (ICF-folded empty `ret` 0x1720c6)
    virtual void Describe(UWideString& ioString);   // +0x10 0x2293d4 ("res %d / %d:", "obj %d:", "hp %d:")
    virtual void DescribeLong(UWideString& ioString);  // +0x14 0x22946f ("Health: %d / %d (%.2f%%), newline)
    virtual void SightOffHere();            // +0x24 (ICF-folded empty `ret` 0x1720c6)
    virtual long RepairPointers(unsigned long, unsigned long);  // +0x28 0x2299d5: RTSUnit::RepairPointers (folded at 0x27cf38)
    virtual void Clear();                   // +0x20 0x228d05: ResetEEUnit, Reset, mIsValid = false
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x1c 0x2298fb
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x2c 0x2299e5
    virtual void FreeNode();                // +0x30 0x2292c4 (RTSUnit::FreeNode)
    virtual void LastDereference();         // +0x34 0x2298e1 (Virtual30, the count = 0, delete this)
    // +0x38 0x228e7f: unloads the class media (now, or through gDataPreLoading when inArg and not shutting down),
    // hit points 0, the complex unit out of gWorld, RTSUnit::Invalidate (provisional name)
    virtual void Remove(bool inUnloadInBackground);
    virtual void SightOn(unsigned long, unsigned long);      // +0x3c
    virtual bool UpdateHidden();            // +0x40 (EERenderObject 0x10f44f tests al)
    virtual void OutOfSynchDump(FILE* inFile, unsigned long inFlags);  // +0x44 (the out-of-synch dump: EEPhysics 0x15234c)
    // +0x48 0x2292c9 (`ret 4`, returns bool; reads the argument's mStats +0x160 bit 14): EEMPathTo pushes a unit and
    // tests al; EEComplexUnit overrides it (0x1f9e81, ret 4). Name is provisional
    virtual bool IsPassableBy(EEComplexUnit* inUnit);
    virtual long GetChecksum(unsigned long);  // +0x4c
    // +0x50 0x229550: the unit's tile, or for a class bigger than one tile not on a continent with mCategory == inFlag,
    // the first tile within inRange of its square that is (inferred types)
    virtual EETileCoordinate FindAreaTile(bool inFlag, long inRange);
    // +0x54: mStats's +0x68 (EEComplexUnit 0x1fa473: or a lookup when +0x351 is set; the logic bomb reads its
    // +0x244 part) (provisional name)
    virtual EEModel* GetModel();
    virtual bool IsFoundation();            // +0x58
    virtual void GetPosition(GE3DPoint& outPosition);         // +0x5c (the calamities read the caster's)
    virtual void Kill(bool inKilled, bool inKeepResearch, bool inOther);  // +0x60 0x22975a (bytes: cmp byte, or dl) (the teleport calamity: (1, 0, 0) when no place is found)
    // +0x64 0x22971e: loads the class media (now, or through gDataPreLoading when inArg) (provisional name)
    virtual void LoadMedia(bool inIsFoundation);
    virtual void SightOff(unsigned long, unsigned long);     // +0x68
    // +0x6c 0x229973 (2 arguments: EEPlayer::FinishUnitsLoad 0x1690a5): after Load, the saved hit-point
    // fraction (Load memcpy's the float into mHitPoints) back to hit points; returns 0
    virtual long FinishLoad(long, long);
    virtual void ResearchCompleted(unsigned long);  // +0x70
    // +0x74: damage from inAttacker for player inPlayerNumber (the contagion functors; provisional names)
    virtual void TakeHit(EEUnit* inAttacker, long inDamage, unsigned long inPlayerNumber, long inMode);
    virtual void SightMoved(unsigned long, unsigned long, unsigned long, unsigned long);  // +0x78

    EEUnit();                               // 0x228cd2: RTSUnit(), the vptr, Reset() twice over, then the count = 0
    void ResetEEUnit();                     // 0x229706: +0x2c..+0x44 = 0 (provisional name)
    // 0x228d57: whether this unit can act on inUnit at (inX, inY) (the mining building's mine search) (provisional
    // name)
    // (inArg4/inArg5 read as bytes: bool)
    bool CanGather(EEUnit* inUnit, long inX, long inY, bool inIsVisible, bool inIgnoreAmount);
    void DepleteResource(long inAmount);    // 0x229d73 (the mining building's mine, 1) (provisional name)
    void RegrowForest(EEComplexUnit* inUnit);  // 0x22a3fd (EEUGGather::Work when the resource's class mIsTree) (provisional name)
    void AddResourceAmount(long inValue);   // 0x228d4d: mL40 += inValue (provisional name)
    // 0x229278 / 0x2292ad (cdecl): a new EEUnit, then Init (provisional names)
    static EEUnit* Create(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer);
    static EEUnit* Create();
    void Init(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer);   // 0x22965c (provisional name)
    void Init(EEUnitClass* inClass, EEPlayer* inPlayer);   // 0x2296b3 (mComplexUnit = this) (provisional name)
    void SetHitPoints(long inHitPoints);    // 0x229a56 (provisional name)
    void MoveTo(long inX, long inY, bool inUpdateTiles);  // 0x229ac3 (Init's last call; moves the unit to a tile: regions, influence, mini map) (provisional name)
    // 0x22a4c3 (cdecl): unit types 0x24b, 0x24c, 0x2fb, 0x306, 0x307, 0x340..0x345, 0x348 (0x22a3fd skips them)
    // (provisional name)
    static bool IsCarbonRock(unsigned long inUnitType);

    EEUnitStoreField<EEUnitClass*, kUnitFieldUnitClass> mUnitClass;   // +0x2c (the unit store's)
    // +0x30: the complex unit this unit belongs to (the calamities read its hit points, transformation +0x130 and
    // goals through it: 0xafe2c, 0xb1a5d) (provisional name)
    EEUnitStoreField<EEComplexUnit*, kUnitFieldComplex> mComplexUnit;   // (the unit store's)
    EEUnitClassUpgrade* mStats;             // +0x34 (provisional name)
    unsigned long mResourceType;            // +0x38 (saved)
    EEUnitStoreField<long, kUnitFieldHitPoints> mHitPoints;   // +0x3c (the unit store's) (> 0: alive) (provisional name)
    EEUnitStoreField<long, kUnitFieldResourceAmount> mResourceAmount;   // +0x40 (saved) (the unit store's)
    unsigned char mHiddenFlags;             // +0x44 flags (0x10: MoveTo moved it; 0x20 and the low 2 bits: Virtual40 0x228f90)
};

// EEComplexUnit::mWaypoints's element: a waypoint of the unit's path (0x14 bytes: AddWaypoint builds one from a
// GE3DPoint, the path length so far and 0; GetPathStart/GetPathLength read back().mDistance - front().mF10)
// (provisional names)
struct EEComplexUnitWaypoint {
    GE3DPoint mLocation;                    // +0x00
    float mDistance;                        // +0x0c
    float mStartDistance;                   // +0x10
};

// EEComplexUnit::mIgnoredTargets's element (0x18 bytes; ctor 0x1f5d8b out of line right after its first user
// IgnoreTarget: six zero stores): an attacker record, found by its unit id (std::find 0x200eb6 with an unsigned long)
// (provisional names)
struct EEComplexUnitAttacker {
    EEComplexUnitAttacker();
    EEComplexUnitAttacker& operator=(const EEComplexUnitAttacker& inOther);   // 0x20018a (memberwise, out of line: the deque erase family 0x2001b5/0x200ef2/0x200f35 copies through it; ICF with EESEAPlaceWallEntry's)
    bool operator==(unsigned long inUnitID) const { return this->mUnitID == inUnitID; }
    long mTileX;                            // +0x00 (the attacked unit's tile)
    long mTileY;                            // +0x04
    long mFromX;                            // +0x08 (the attacker's tile)
    long mFromY;                            // +0x0c
    unsigned long mUnitID;                  // +0x10
    unsigned long mTime;                    // +0x14 (gPhysics +0xd0 plus 30000)
};

// A queued research/production order (EEComplexUnit::mBuildQueue; 0x1f5f97 pushes (technology, player flag)). Not a tile:
// a POD, its entries are built field by field with no zeroing. Name is provisional.
struct EEProductionEntry {
    // inline (std::find(mBuildQueue.begin(), mBuildQueue.end(), inTechnology) in EEComplexUnit::IsResearchQueued
    // 0x937dd: the find COMDAT 0x963ff compares the first dword with the value)
    bool operator==(long inTechnology) const { return this->mTechnology == inTechnology; }
    long mTechnology;
    long mFlag;
};

class EEComplexUnit : public EEUnit {
public:
    // the save keys and the pool (EEComplexUnit.cpp's `$E` chains 0x1f5805..0x1f5954, one guard byte 0x963210;
    // definition order as listed). Provisional names (from the keys)
    static const UString sData;             // "EEComplexUnit-Data" (0x963218, guard bit 1)
    static const UString sCloakableOpacity; // "EEComplexUnit-CloakableOpacity" (0x963200, bit 2)
    static const UString sIsCloaked;        // "EEComplexUnit-IsCloaked" (0x9631f0, bit 4)
    static const UString sDrawAreaEffects;  // "EEComplexUnit-DrawAreaEffects" (0x9631e0, bit 8)
    static const UString sAreaEffectsTick;  // "EEComplexUnit-AreaEffectsTick" (0x9631d0, bit 0x10)
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
    float DistanceSquaredTo(EEComplexUnit* inUnit)
    {
        float theDY = this->mPhysicalModel.GetLocation().GetY() - inUnit->mPhysicalModel.GetLocation().GetY();
        float theDX = this->mPhysicalModel.GetLocation().GetX() - inUnit->mPhysicalModel.GetLocation().GetX();
        return theDX * theDX + theDY * theDY;
    }
    void SetPower(long inValue) { if (this->mPower != inValue) this->mPower = inValue; }  // inline (the cast-calamity goal's cost)
    // inline in the real header (out-of-line COMDAT 0x21b819): the squared distance to the centre of inTile (the load
    // goals' Virtual64) declared out of line here (provisional name)
    // inline (COMDAT 0x61b819 after its first user EEUCTransport::CanLoad; every caller calls it: the /Os post-check
    // undoes it, and the no-/Og functor 0x63ddc1 keeps its 8 locals per site, frame 0x54) r37_big B1
    float DistanceSquaredToTile(const U2DPoint<long>& inTile)
    {
        return (this->mPhysicalModel.GetLocation().GetX() - inTile.GetX()) * (this->mPhysicalModel.GetLocation().GetX() - inTile.GetX())
               + (this->mPhysicalModel.GetLocation().GetY() - inTile.GetY()) * (this->mPhysicalModel.GetLocation().GetY() - inTile.GetY());
    }

    virtual bool IsNextToTile(long inX, long inY); // original1fab56 ret8/AL; sole footprint caller
    virtual bool StepAside(EEPlayer* inPlayer, EEUnitClass* inClass, long inX, long inY, bool inFlag);               // +0x80
    virtual void SetFoundation(bool);       // +0x84 (sets +0x351)
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew);  // +0x88 (ret 8: inNew replaces inOld as cargo/carrier)

    void StartAction(long inAction);        // 0x1fdfa5 (provisional name)
    void SetGoal(EEUnitGoal* inGoal, bool inKeepCurrent, long inArg3);  // 0x1fe863 (provisional name)
    void PlayEffect(long inEffect);         // 0x1fea99 (the conscription calamity passes 0x138d) (provisional name)
    void ClearGoals(long inArg);            // 0x1f88fe (before a calamity hands out a new goal) (provisional name)
    void ToggleJITManufacturing();          // 0x1fd8f3 (EEMJITManufacturing, then UpdateJITManufacturing; provisional name)
    void UpdateJITManufacturing();          // 0x1f6e19 (provisional name)
    // 0x1fe7a8 (EESEARotateUnits passes its copy of +0x130 with a new yaw, then 1) (provisional name)
    void SetTransformation(const GETransformation& inTransformation, bool inUpdateTiles);
    // inline (EECCDropObject.cpp emits the COMDAT 0xb01d2: its first user calls it out of line) (provisional name)
    void SetProjectileStart(const GE3DPoint& inPosition)
    {
        if (this->mProjectileStart != inPosition)
            this->mProjectileStart = inPosition;
    }
    void SetLauncherClass(EEUnitClass* inClass) { if (this->mLauncherClass != inClass) this->mLauncherClass = inClass; }
    // inline (COMDAT 0xb01ad, first emitted by EECCDropObject.cpp; keeps a reference) (provisional name)
    void SetOwner(EEComplexUnit* inOwner)
    {
        if (this->mOwner != inOwner) {
            if (this->mOwner != NULL)
                this->mOwner->Dereference();
            this->mOwner = inOwner;
            inOwner->Reference();
        }
    }
    void SetHeight(float inHeight) { if (this->mHeight != inHeight) this->mHeight = inHeight; }  // inline (provisional name)
    void SetIsDeletable(bool inValue) { if (this->mIsDeletable != inValue) this->mIsDeletable = inValue; }  // inline (contagion functors) (provisional name)
    // inline (the fire functor damages a unit only when it is its own outermost carrier) (provisional name)
    // inline (provisional name; Update 0xb0a60, with GetPlayerNumber)
    bool IsCarried() const { return this->mPreviousPart != NULL || this->mNextPart != NULL; }
    EEComplexUnit* GetOutermostCarrier()
    {
        // through a pointer to the link: the fire functor re-tests *theCarrier after the first test (0xb6ce8)
        EEComplexUnit* theResult = this;
        if (this->mPreviousPart != NULL) {
            EEComplexUnit** theCarrier = &this->mPreviousPart;
            while (*theCarrier != NULL) {
                theResult = *theCarrier;
                theCarrier = &theResult->mPreviousPart;
            }
        }
        return theResult;
    }
    // 0x1f9df3: the effect of kind inKind in the unit's effect list (+0x100), false if none (provisional name)
    bool GetEffect(long inKind, EEGraphicEffect*& outEffect);
    // the volcano moving a unit off its tiles (provisional names; types inferred)
    long NextWaypoint();                    // 0x1fbdd1 (0: the unit has more waypoints to try)
    long GetPathStart(GE3DPoint& outLocation, float& outLength);  // 0x1fa013 (0: outLocation is the next one)
    void DropPath();                        // 0x1f7112
    // 0x1fa229 (EEUCSpaceBuilding::HasArrived: where the unit is going; 7 when it has no path: long) (provisional name)
    long GetNextWaypointPoint(GE3DPoint& outLocation);
    long GetPathLength(float& outDistance);  // 0x1fa1e1: the path's remaining length (7: no path) (provisional name)
    bool GetCatchableContagions(EEComplexUnit* inUnit, unsigned long& outFlags);  // 0x1f6a31 (calamity flags of inUnit this one takes)
    void UpdateWaterState(bool inArg);      // 0x1f7c47 (mWaterState from the class's +0x2d0 bits and the tile) (provisional name)
    void ApplyAreaEffects();                // 0x1f9b72 (the area effects over the largest radius) (provisional name)
    // 0x1f9c9b (ret 0x20): up to inTries points around inTarget away from this unit until inScreen accepts one
    // (EECPAttackSubGroupUnit::Flee's flee) (provisional name)
    bool FindFleePoint(GE3DPoint* inTarget, GE3DPoint* outPoint, long inTries, float inArg4, float inArg5, float inArg6,
                  float inArg7, EEFleeFromLocationScreenBase* inScreen);
    bool IsOnOtherContinentThan(EEComplexUnit* inUnit);  // 0x1fa696 (provisional name)
    bool IsOnOtherContinent(long inTileX, long inTileY);  // 0x1fa6f9 (another continent than the unit's) (provisional name)
    unsigned short GetFootprintContinent(long inKind);  // 0x1fa7f6 (the first continent among the unit's tiles) (provisional name)
    // inline (EEUCLandTransport::BadlyHurt 0x217017: const operator[], `lea; mov [eax]`) (provisional name)
    EEComplexUnit* GetCargo(unsigned long inIndex) const { return this->mCargo[inIndex]; }
    void SetMine(EEUnit* inUnit);           // 0x1fe1df (keeps a reference; the mining building's mine) (provisional name)
    void UpdateWheels();                    // 0x213c37: EEWheel::Update on each of mAreaEffectTemplate1 wheels at +0x1bc (provisional name)
    long LoadCargo(EEComplexUnit* inCargo, bool inIgnoreRoom, bool inArg3);  // 0x1f5da1 (the load goals: 7 = refused) (provisional name)
    bool CanTake(EEComplexUnit* inCargo, unsigned long inValue);    // 0x1f6a1a (mU38 is 1 or inValue) (provisional name)
    bool GarrisonUnit(EEComplexUnit* inCargo);  // 0x1f5ebf (the garrison goal) (provisional name)
    // inline in the real header (out-of-line COMDAT 0x1ed63b, first emitted by EEActiveGroup.cpp): the first goal queued
    // in mGoalQueues[inIndex], 0 if none; declared out of line here (provisional name)
    EEUnitGoal* GetQueuedGoal(unsigned long inIndex);
    std::deque<EEUnitGoal*>& GetQueueGoals(unsigned long inIndex);  // 0x1fa1cd: mGoalQueues[inIndex].mGoals (out of line; EEUICUnitIconBar 0x25ad01) (provisional name)
    // 0x1ebdcc (defined in EEActiveGroup.cpp after Virtual48, out of line): mActiveGroup = inGroup, referenced
    // (provisional name)
    void SetActiveGroup(EEActiveGroup* inGroup);
    // EEUGHealUnit.cpp's callees (provisional names; types inferred)
    void ReleaseHealEffect();               // 0x1f709c (the healee is let go)
    void SetAnimationFrame(unsigned long inFrame);  // 0x1f414d (ret 4): the state's model at frame inFrame (EEComplexDoppelganger.cpp's range) (provisional name)
    void FleeAfterCalamity(long inArg);     // 0x1fbfdd (EEUnitClass::ActionChanged action 0x7ed passes 0) (provisional name)
    // EEUGAttackUnit.cpp's callees (provisional names; types inferred)
    bool CanReachUnit(EEUnit* inTarget);    // 0x1fa785
    bool IsTargetIgnored(EEComplexUnit* inTarget, bool inCheckLocation);  // 0x1fa563
    void IgnoreTarget(EEComplexUnit* inTarget); // 0x1f5cc7
    bool IsInReach(EEUnit* inTarget);       // 0x1fefff
    float GetLineOfSight();                 // 0x1f9dcb (compared with mStats->mF9C)
    bool IsOnTileBorderingTarget(EEUnit* inTarget);  // 0x1ff400
    void AddCenterOffset(float& ioX, float& ioY);  // 0x1f9fbb (EEUGLoadGarrison::ComputeRendezvous: the entrance point; provisional name)
    bool IsRepairable();                    // 0x1fbd5f (can be healed)
    bool CanDelete();                       // 0x1f9e2d (EEMKillUnit: the unit may be deleted; provisional name)
    // EEUGAttackGround.cpp's callees (provisional names; types inferred)
    bool CanReachTile(long inTileX, long inTileY);  // 0x1ff18a (can the unit act on that tile)
    void SetAnimation(long inArg1, long inArg2, bool inIsAnimationPaused);  // 0x1fe02a
    void SetRandomReloadTimer(long inReloadTime);       // 0x1fea22
    // inline setters (`if (m != v) m = v`; EEUGAttackGround::Step)
    void SetCurrentPhysicsAction(unsigned long inValue) { if (this->mCurrentPhysicsAction != inValue) this->mCurrentPhysicsAction = inValue; }
    // inline (EEUCPlanetSpaceship::ActionChanged 0x218e34, EEUCProjectile::ActionChanged 0x219388) (provisional name)
    void SetCurrentPhysicsOrder(unsigned long inValue) { if (this->mCurrentPhysicsOrder != inValue) this->mCurrentPhysicsOrder = inValue; }
    void SetHasArrived(bool inValue) { if (this->mHasArrived != inValue) this->mHasArrived = inValue; }
    void SetRottingAwayTime(long inValue) { if (this->mRottingAwayTime != inValue) this->mRottingAwayTime = inValue; }
    void UpdateOrbitals();                  // 0x20e086 (EEUCProjectile::Update, first) (provisional name)
    void SetOpacityChain(float inValue);    // 0x216c22 (EEUCProjectile::Update: 1 - z/6 when below the ground) (provisional name)
    void SetInitialContactLocation(const GE3DPoint& inPoint);  // 0x215246 (mInitialContactLocation = inPoint when it differs; EEUCLandable.cpp) (provisional name)
    void StartProduction();                 // 0x1fdc58 (provisional name)
    void ContinueProduction();              // 0x216d2d (EEUCLandCitizen::Update: StartProduction when mL280 == 0 and mBuildQueue is not empty) (provisional name)
    void UpdateTurrets();                   // 0x20e14c (EEUCSpaceship::Update, after the physics model) (provisional name)
    void UpdateReloadTimer();               // 0x20e0b2 (EEUCSpaceship::Update) (provisional name)
    void UpdateTargetSearchTimer();         // 0x20e0db (EEUCSpaceship::Update) (provisional name)
    void UpdateHealSearchTimer();           // 0x20e104 (EEUCBasicPriest::Update: mHealSearchTimer down by 0.001 per ms) (provisional name)
    float GetF220() const { return this->mF220; }  // inline (EEPMHelicopter::ApplyBob 0x558ce3 loads it as a value: fld; faddp) (provisional name)
    void SetF220(float inValue) { if (this->mF220 != inValue) this->mF220 = inValue; }
    // inline (EEPMNaval::PushMoving 0x55c1cb: fld; fadd; fcom; je; fstp) (provisional names)
    void SetPushVelocityX(float inValue) { if (this->mPushVelocityX != inValue) this->mPushVelocityX = inValue; }
    void SetPushVelocityY(float inValue) { if (this->mPushVelocityY != inValue) this->mPushVelocityY = inValue; }
    void SetTargetSearchTimer(long inValue) { if (this->mTargetSearchTimer != inValue) this->mTargetSearchTimer = inValue; }
    void SetIsAnimationPaused(bool inValue) { if (this->mIsAnimationPaused != inValue) this->mIsAnimationPaused = inValue; }  // (EEUnitAI.cpp action 5000)
    void SetAnimationTime(unsigned long inValue) { if (this->mAnimationTime != inValue) this->mAnimationTime = inValue; }  // (EEUnitAI.cpp action 5000)
    // inline setters (EEHerd.cpp 0xa1b45/0xa286a/0xa31fd/0xa3d9c/0xa412e: cmp then store) (provisional name)
    void SetGrazeTime(unsigned long inValue) { if (this->mGrazeTime != inValue) this->mGrazeTime = inValue; }
    void SetGrowTime(unsigned long inValue) { if (this->mGrowTime != inValue) this->mGrowTime = inValue; }
    void SetHerd(EEHerd* inValue) { if (this->mHerd != inValue) this->mHerd = inValue; }  // (the unit's herd)
    void SetGrowth(float inValue);          // 0x1fe49a (EEHerd 0xa31fd: min(animal +0x34 + mGrowth, 1); 0xa3d9c) (provisional name)
    float GetVelocity() const { return this->mVelocity; }  // inline (EEPMMissile::Accelerate 0x55b041: fld; faddp) (provisional name)
    float GetGrowth() const { return this->mGrowth; }  // inline (EEHerd::GrowUnit 0xa31fd loads it as a value) (provisional name)
    // inline setters (EEUGBuildRepair::BeginBuilding)
    void SetWorkTimer(long inValue) { if (this->mWorkTimer != inValue) this->mWorkTimer = inValue; }
    void SetProductionTime(long inValue) { if (this->mProductionTime != inValue) this->mProductionTime = inValue; }
    void FaceTarget(EEUnit* inTarget);      // 0x1feeb9 (the build/repair goal's command 0xfa2) (provisional name)
    float GetSight() const { return this->mSight; }  // inline (line of sight 0xb1fa5) (provisional name)
    void SetSight(float inSight) { this->mSight = inSight; }
    // inline (COMDAT 0xb021c, first emitted by EECCDropObject.cpp): +0x21c is kept at least mStats->mBC
    void SetVelocity(float inValue)
    {
        if (inValue != this->mVelocity)
            this->mVelocity = inValue;
        this->mF21C = max(inValue, this->mStats->mBC);
    }
    // inline in the real header (out-of-line COMDAT 0x5e407; 54 callers): +0x1c8's value when set, else mStats->mFAC;
    // body not reconstructed yet, so declared out of line here provisional name (meteor storm)
    float GetMaxVelocity();
    // 0x15463e / 0x15465b / 0x154c95 / 0x154e7b / 0x154f39 (EEPMLand.cpp, ecx = the unit): the group's stat when
    // mActiveGroup is set (EEActiveGroup::GetMaxAngularVelocity ..), else mStats->mFB4 .. (provisional names)
    float GetMaxAngularVelocity();
    float GetAngularAcceleration();
    float GetAcceleration();
    float GetInverseAcceleration();
    float GetInverseAngularAcceleration();
    // 0x1fe76e (the meteor storm passes the meteor's start point, true) (provisional name)
    void SetLocation(const GE3DPoint& inLocation, bool inUpdateTiles);
    void SetFlightTime(long inValue) { if (this->mFlightTime != inValue) this->mFlightTime = inValue; }  // inline (meteor storm 0xb29ac)
    void SetOpenFraction(float inValue) { if (this->mOpenFraction != inValue) this->mOpenFraction = inValue; }  // inline (logic bomb 0xb251d)
    float GetOpenFraction() const { return this->mOpenFraction; }  // inline (EEE3DEnergyWall::Queue loads it first)
    // inline (COMDAT 0xb3976, first emitted by EECCTeleport.cpp) (provisional name)
    void Set2DPosition(const U2DPoint<float>& inPoint) { if (this->mU0DC != inPoint) this->mU0DC = inPoint; }
    void TakeOffMap();                      // 0x1fdaf3 (the teleport calamity's hold) (provisional name)
    void SetBuilt(long inIsBuilt, long inUpdateSight, long inLoadButtons);  // 0x1fe230 (the hurricane's new unit: (1, 0, 1))
    // time warp (provisional names)
    void SetIsTinted(bool inValue) { if (this->mIsTinted != inValue) this->mIsTinted = inValue; }  // inline (the warped unit is away)
    void SetLocked(bool inValue) { if (this->mIsLocked != inValue) this->mIsLocked = inValue; }  // inline (EEMLockUnlock)
    void SetIsPacked(bool inValue) { if (this->mIsPacked != inValue) this->mIsPacked = inValue; }  // inline (EEMPackUnpack: packed)
    void SetIsFiringHigh(bool inValue) { if (this->mIsFiringHigh != inValue) this->mIsFiringHigh = inValue; }  // inline (SetGoal: cmp, lea, and)
    void GoFanatic();                       // 0x1fa906 (EEMGoFanatic; provisional name)
    void ClearAllGoalQueues();              // 0x1f6d44 (EEMScrambleAircraft)
    void QueueGoal(EEUnitGoal* inGoal, long inItem);  // 0x3d416: queues inGoal in mGoalQueues[inItem]
    // inline (COMDAT 0xd5c5 after its first user EECPASGUSpaceCarrier::Retarget)
    unsigned long GetNumCargoUnits() { return this->mCargo.size(); }
    unsigned long GetNumWaypoints() { return this->mWaypoints.size(); }  // EECPASGLand::HasIdleUnitToLoad 0x447c96 (provisional name)
    bool HasCargo(EEComplexUnit* inCargo);  // 0x46fc0 (EEMImmediateCargoUnload; provisional name)
    bool UnloadCargoNow(EEComplexUnit* inCargo);  // 0x1fd0fe (EEMImmediateCargoUnload; provisional name)
    // 0x229314: a free tile near inUnit for this unit (EEMPathTo; returns a distance; provisional name)

    // 0xc7486 (EEMessage.cpp's range; EEActiveGroup::SpreadAroundUnit/SpreadAroundResource call it): stats +0xd8 > 0 and the table
    // gDamageTable [inClass +0x1c8][class +0x1cc] > 0 for a class whose MarkInvalid holds (can attack it) (provisional name)
    bool CanAttackClass(EEUnitClass* inClass);
    EETown* FindNearestTown();              // 0x1fa856 (the owner's town (EEPlayer +0x7d0) nearest the unit that contains it: EEMPatrol, EECPUnitManager, EECPCombatManager) (provisional name)
    float DistanceSquaredToTileXY(long inTileX, long inTileY);  // 0x4211d (squared distance from the transformation (+0x13c/+0x14c) to a tile; EEPPatrolGroup::GetNearest; provisional name)
    float DistanceSquaredToUnit(EEUnit* inOther);  // 0x8bdb4 (EECPResourceCenter.cpp: squared distance to another unit) (provisional name)
    // 0x20c607 (out-of-line COMDAT in EEUCBasicCitizen's range): squared distance from +0x13c/+0x14c to inPoint, y first
    // (EEUGGather::IsStorageInReach) (provisional name)
    // inline (COMDAT 0x60c607 in EEUCBasicCitizen.cpp; undone at every caller; the no-/Og functor 0x63de0f keeps its
    // 8 locals per site, frame 0x54) r37_big B1
    float DistanceSquared2D(const GE2DPoint& inPoint)
    {
        return (this->mPhysicalModel.GetLocation().GetX() - inPoint.GetX()) * (this->mPhysicalModel.GetLocation().GetX() - inPoint.GetX())
               + (this->mPhysicalModel.GetLocation().GetY() - inPoint.GetY()) * (this->mPhysicalModel.GetLocation().GetY() - inPoint.GetY());
    }
    void QueueResearch(long inTechnology, bool inFront);  // 0x1f5f97 (EEMResearchTechnology; provisional name)
    void QueueResearchUncaptured(long inTechnology, bool inFront);  // its body (ee/EETechCapture.h's TQRS)
    // inline (out-of-line COMDAT 0x940eb, 26 callers)
    unsigned long GetBuildQueueSize() { return this->mBuildQueue.size(); }
    // inline (the per-class EnabledChanged/B8 switches and EEUCGate::IsPartLeftOut read the current command through it;
    // provisional name)
    unsigned long GetCommandType() const { return this->mCommand ? this->mCommand->mType : 0; }
    // inline (GetEffectHeight 0x1fa4dc loads mFirstFrame2 before the WriteDetailsHook call that indexes the models with it)
    unsigned long GetAnimation() const { return this->mAnimation; }
    void OrderRepair(EEUnit* inTarget);     // 0x1f60ea (EEMRepairUnit, EEMBuildUnit; provisional name)
    void AddBuildTarget(EEUnit* inTarget);  // 0x1f61c8 (EEMRepairUnit, EEMBuildUnit; provisional name)
    void RemoveResearch(long inTechnology, long inRemoveAll);  // 0x1fd984 (EEMRemoveTechnology; provisional name)
    void SetHomeBase(EEComplexUnit* inBase);  // 0x1fe9fb (EEMReturnToBase: the aircraft's new base; provisional name)
    // inline (COMDAT 0xb45b0, first emitted by EECCTimeWarp.cpp)
    void SetColor(const GEColor& inColor) { if (this->mColor != inColor) this->mColor = inColor; }
    float GetEffectHeight(bool inUseCurrentFrame);  // 0x1fa4b4 (a height: the effects sit at it, or half of it)
    void ClearBuildTargets();               // 0x1f6f2a
    // inline in the real header (COMDAT 0x41da1: mGoalQueues.size()); body not reconstructed yet (EEUnitVectorItem is
    // incomplete here), so declared out of line
    unsigned long GetNumGoalQueues();
    void ClearGoalQueue(unsigned long inIndex);  // 0x1f6fbc (for each mGoalQueues item)
    void UnloadCargoAtTile(long inX, long inY, bool inArg);  // 0x1fcc91 (ret 0xc; EECPResourceManager::TransportIdle: the load goal's container tile) (provisional name)
    // 0x1fa661: +0x347 set and +0x358 is 0x1a or 0x1c (the logic bomb refuses such targets) (provisional name)
    void PayInsurance(float inValue); // original602B callee, actual class technology update
    bool IsProtected();
    void AIStep();                          // 0x1ff2e8 (Step; provisional name)
    void Step();                            // inline below EEUnitClass (COMDAT 0x1540dc after its first user EEPhysics::UpdateUnits)
    void SetDisabled(bool inArg1, long inArg2);  // 0x1fe536 (logic bomb: (0, 0) on Stop, (1, 0x1c) on Update)
    void Halt();                            // 0x1fef90 (logic bomb: a target that is not category 0x19)
    // the RTSUnit/EEUnit slots EEComplexUnit overrides (vtable 0x446ddc; EEComplexUnit.cpp 0x1f57d3..0x201d85 unless
    // noted). Signatures as in the base classes
    virtual ~EEComplexUnit();               // +0x00 0x1f5ba7 (??_E 0x201335: the pool's chunks new[] the units)
    virtual void Invalidate();              // +0x04 0x1f6acb
    virtual void CopyFrom(RTSUnit* inUnit);    // +0x08 0x1f713d
    virtual void SightOnHere();             // +0x0c 0x1f5b5b: Virtual3C(tile)
    virtual void Describe(UWideString& ioString);   // +0x10 0x1fa269
    virtual void DescribeLong(UWideString& ioString);  // +0x14 (ICF-folded empty `ret 4` 0x157269)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x1c 0x1f2452
    virtual void Clear();                   // +0x20 0x1f5b6e
    virtual void SightOffHere();            // +0x24 0x1f5b8e: MarkInvalid(tile)
    virtual long RepairPointers(unsigned long, unsigned long);  // +0x28 0x1f353f
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x2c 0x1f417e
    virtual void FreeNode();                // +0x30 0x1f8a43
    virtual void LastDereference();         // +0x34 0x1fb7fb (Virtual30, the count = 0, back to sMemoryPool)
    virtual void SightOn(unsigned long, unsigned long);      // +0x3c 0x1f7ce7
    virtual bool UpdateHidden();            // +0x40 0x1f8357
    virtual void OutOfSynchDump(FILE* inFile, unsigned long inFlags);  // +0x44 0x1f908d
    virtual bool IsPassableBy(EEComplexUnit* inUnit);  // +0x48 0x1f9e81
    virtual long GetChecksum(unsigned long);  // +0x4c 0x1fa07f
    virtual EETileCoordinate FindAreaTile(bool inFlag, long inRange);  // +0x50 0x1fa385
    virtual EEModel* GetModel();            // +0x54 0x1fa473
    virtual bool IsFoundation();            // +0x58 0x1f5b67: mIsFoundation
    virtual void GetPosition(GE3DPoint& outPosition);         // +0x5c 0x1fa493
    virtual void Kill(bool inArg1, bool inArg2, bool inArg3);        // +0x60 0x1fb383
    virtual void LoadMedia(bool inIsFoundation);  // +0x64 0x1fb814
    virtual void SightOff(unsigned long, unsigned long);     // +0x68 0x1fd336
    virtual long FinishLoad(long, long);    // +0x6c 0x1f3395
    virtual void ResearchCompleted(unsigned long);  // +0x70 (ICF-folded empty `ret 4` 0x157269)
    virtual void SightMoved(unsigned long, unsigned long, unsigned long, unsigned long);  // +0x78 0x1ff834

    EEComplexUnit();                        // 0x1f5999
    // 0x1fac37 (ret 8; first argument sizes the goal-queue vector; second randomizes heading): clears the containers
    // and members (provisional name)
    void ResetEEComplexUnit(unsigned long inGoalQueueCount, bool inArg2);
    // 0x1fb18f: Init (provisional name; types inferred)
    void Init(EEUnitClass* inClass, const GETransformation& inTransformation, EEPlayer* inPlayer, unsigned long inGoalQueueCount);
    // the pool (cdecl; provisional names): a new unit, reset (0x1f7b7c) or Init'ed (0x1f7bd2); the pool sizes
    // (0x1f7ba6)
    static EEComplexUnit* Create();
    static EEComplexUnit* Create(EEUnitClass* inClass, const GETransformation& inTransformation, EEPlayer* inPlayer, unsigned long inGoalQueueCount);
    static bool SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks);
    bool GetB34F();                         // 0x1f7b45 (out of line) (provisional name)
    bool HasCalamityFlag(unsigned long inFlag);   // 0x1f7b69 (provisional name)
    void ClearGarrison();                   // 0x1f8bd8: mL28C = mL288 = 0 (provisional name)
    bool HasEffect(long inKind);            // 0x1f9ddb: GetEffect into a dummy (provisional name)
    bool IsGathering();                     // 0x1fa67f: the goal's kind is 6 (provisional name)
    bool IsCloaked();                       // 0x1fa96c: mCloaked != 0 (provisional name)
    // 0x21ba6f: the squared distance from the position (+0x13c/+0x14c/+0x15c) to inPoint (the attack-ground test;
    // provisional name)
    float DistanceSquared3D(const GE3DPoint& inPoint);
    void ClearTint();                       // 0x1fa979 (provisional name)
    void UpdateFanaticismColor();                        // 0x1fa98c (the cloak colour; function-static GEColors) (provisional name)
    EEComplexUnit* FindWaypointMarker(long inArg1, long inArg2);  // 0x1f9c12 (provisional name)
    void RemoveWaypointMarker(long inX, long inY);        // 0x1fdc39 (provisional name)
    EEUnitGoal* RefreshGoals();             // 0x1fdf31: the goal = the first queued one (returned: DropFinishedGoals) (provisional name)
    void SetPatrolGroup(EEPPatrolGroup* inGroup);   // 0x1fe83f (provisional name)
    // defined in EEPlayer.cpp's range after their first user EEPlayer::DisableCamouflagePower (out of line everywhere: recursive)
    void SetCloakableChain(bool inValue);   // 0x16c6a1: mIsCloakable along the mAIKind chain; false also clears mIsCloaked (provisional name)
    void SetCloakedChain(bool inValue);     // 0x16c6db: mIsCloaked along the mAIKind chain (tail recursion -> loop) (provisional name)
    // inline: mIsCloaked only counts while mIsCloakable is set (SetCloakableChain clears both). EEUGCastCalamity::EvaluateCondition 0x621133 reads it
    // as `cmp byte [+0x345],0; je; mov al,[+0x346]; test al,al` (the result in al after the caller tests mIsCloakable itself);
    // the same ternary is spelled out at ~20 sites (provisional name)
    bool GetIsCloaked() const { return this->mIsCloakable ? this->mIsCloaked : false; }
    bool AddGarrison(long inAmount, long inGarrisonPopulation);  // 0x1f5f55: mL288 += inAmount within the class's mL250 (provisional name)
    EEComplexUnit* FindPassenger(unsigned long inUnitID);  // 0x1f9f85: the cargo unit with that id (provisional name)
    void SetGroupPathGoals(std::vector<GE3DPoint>& inPath);  // 0x1fc0a8, actual12-byte point vector; constness inferred
    void ReleaseCargo(EEComplexUnit* inCargo);  // 0x1fcb8c, actual cargo removal member/ret4
    EEComplexUnit* GetConditionTarget();    // 0x1ff58b: actual referenced complex attacker or null
    bool ShouldMoveToNextAutoWaypoint();   // 0x1feb61: actual path/goal predicate, no args/AL
    EEComplexUnit* UnloadCargoAt(unsigned long inIndex, const GETransformation& inLocation);  // 0x1fcf16 (provisional name)
    EEComplexUnit* UnloadCargoByID(unsigned long inUnitID, const GETransformation& inLocation);  // 0x1fd2ba: UnloadCargoAt on the cargo unit with that id (provisional name)
    void MoveToLocation(float inX, float inY, bool inUpdateTiles);  // 0x1fe611 (before the unit moves to (inX, inY)) (provisional name)
    void SetLocationXY(float inX, float inY, bool inUpdateTiles);  // 0x1fe73b: MoveToLocation, then the 2D location (provisional name)
    // the referenced links (`if (m) m->Dereference(); m = in; in->Reference()` shapes; provisional names)
    // inline (ReleaseHeldUnits 0x1f6d6a reaches the link through a reference: lea edi, [eax + 0x1e4]) (provisional
    // name)
    void ClearHolder() { if (this->mHolder) this->mHolder->Dereference(); this->mHolder = NULL; }
    void SetHolder(EEComplexUnit* inUnit);  // 0x1f5ca2: mRegeneration
    void SetBlockingUnit(RTSUnit* inUnit);  // 0x1fe202: mL20C
    void SetLastResource(RTSUnit* inUnit);  // 0x1fe5e3: mLastResource
    void ReleaseSound();                    // 0x1f6d1a: releases the sound instance mL25C
    void SetSound(SESMInstanceNode* inSound);   // 0x1f7b4c: mL25C (after ReleaseSound)
    // mU0C4, the units linked to this one (each keeps a reference; provisional names)
    void AddContact(EEComplexUnit* inUnit);  // 0x1f6226: adds inUnit unless listed
    void RemoveContactUnit(EEComplexUnit* inUnit);  // 0x1fd2ff: removes inUnit
    void ClearContacts();                   // 0x1f6f65: unlinks this from each listed unit, then clears the list
    void MakeAreaEffects();                 // 0x1ffcb2 (EEWorld::CreatePlayers on a new game; provisional name)
    // 0x1ff928: model replacement; caller 0x1caba9 passes old/new GraphicObject records, ret 8.
    void SwapGraphic(EEGraphicObject* inOldModel, EEGraphicObject* inNewModel);
    void ReplaceUnits(const std::vector<EEPUnitSwap>& inReplacements); // original1fc6e8 ret4, actual8-byte pair traversal
    void ReleaseHeldUnits();                // 0x1f6d6a: releases mHeldUnits (clearing their mRegeneration links to this) (provisional name)
    EEUnit* PopBuildTarget();               // 0x1fbf16: pops mCitizenBuildQueue up to its first valid unit (provisional name)
    // 0x269137 (in EEUIFMGMainGroup.cpp after its first user Update, called out of line: defined there):
    // mCitizenBuildQueue[inIndex] (provisional name)
    EEUnit* GetCitizenBuildTarget(unsigned long inIndex);
    void PopProductionQueue();              // 0x93dbb: mBuildQueue.erase(mBuildQueue.begin()) (EECPUnitManager.cpp's range) (provisional name)
    bool IsResearchQueued(long inTechnology);  // 0x937dd (the main group's technology buttons: the tech tree node's mU4C) (provisional name)
    // (provisional names; types inferred)
    void HoldUnit(EEComplexUnit* inUnit);   // 0x1f5c42: links inUnit into mHeldUnits (kept sorted by mGrowth)
    void RemoveAffectingAreaEffects();      // 0x1f6de2: mAffectingAreaEffects's effects let go of the unit (EEAEManager 0x27b6b7)
    void ReleaseFireEffect();               // 0x1f70bf: the fire effect off (or 0x23d5c3 for type 0x321)
    void ResizeTurretAngles(unsigned long inCount);  // 0x1f7bfa: mTabButtons = inCount zeroed floats
    void PartFinished();                    // 0x1f9d91: Virtual3C(tile), the mini map for other players' units
    void DropFinishedGoals(bool inStartAll);  // 0x1fcb0e (drops the auto-waypoint location goals)
    void ApplyMorale(long inValue, long& outValue);  // 0x1fbd81: inValue less 0.8 of it with positive morale, plus 0.6 with negative
    long GetSecondWaypoint(GE3DPoint& outLocation);  // 0x1fbecf: the second waypoint (7: none) (provisional name)
    void DropUnsuitedGoals(unsigned long inIndex);  // 0x1fbf66: drops the queued goals of mGoalQueues[inIndex] that refuse the unit
    void ReplaceResearch(long inTechnology, long inNewTechnology);  // 0x1ffbc2 (0x244deb: each live unit of a technology's classes) (provisional name)
    bool ReleaseHeld(EEComplexUnit* inUnit);  // 0x1fca95: removes inUnit from mHeldUnits
    void CancelQueuedProduction();          // 0x1fce47: refunds the queued research (mBuildQueue)
    void RefreshActiveGoal();               // 0x1fdf43
    // inGoal for each of the (3, or 4 with civilization power 0x34) goal queues: the first as is, copies for the rest
    void QueueSquadronGoals(EEUnitGoal* inGoal, bool inCloneGoal);  // 0x1fca21 (queued behind a goal already there) (provisional name)
    void SetSquadronGoals(EEUnitGoal* inGoal, bool inCloneGoal);  // 0x1fe42b (replacing them) (provisional name)
    void SetGatheredResource(EEUnit* inUnit);  // 0x1fe372: mShadowScale (referenced; gWorld is told) (provisional name)
    void FnE123D();                         // an empty member (ICF-folded `ret` 0xe123d; EEE2DMarchingIcons::Virtual5C) (provisional name)
    void LinkPart(EEComplexUnit* inUnit);   // 0x1fe7d9: mPreviousPart = inUnit, which links back through its mAIKind (provisional name)
    // 0x1fed98: turns the unit to face (inX, inY) (inZ unread; ret 0xc) provisional name; types inferred
    void FaceToward(GE3DPoint inPoint);     // 0x1fed98 (point by value; ret12)
    void SetReloadTimer(long inValue) { if (this->mReloadTimer != inValue) this->mReloadTimer = inValue; }  // inline (SetRandomReloadTimer) (provisional name)
    // inline setters (EEUnitClass::UnitCreated 0x23511e) (provisional name)
    void SetHealSearchTimer(float inValue) { if (this->mHealSearchTimer != inValue) this->mHealSearchTimer = inValue; }
    void SetLastHitPoints(long inValue) { if (this->mLastHitPoints != inValue) this->mLastHitPoints = inValue; }
    void SetAmmunition(unsigned long inValue) { if (this->mAmmunition != inValue) this->mAmmunition = inValue; }
    void SetBehavior(EEUnitBehavior* inValue) { if (this->mBehavior != inValue) this->mBehavior = inValue; }
    void SetAIState(long inValue) { if (this->mAIState != inValue) this->mAIState = inValue; }  // inline (EECPExplorationManager 0x7f352: cmp then store)
    void SetPowerRegenTimer(long inValue) { if (this->mPowerRegenTimer != inValue) this->mPowerRegenTimer = inValue; }  // (EEUnitClass::RegeneratePower)
    void SetTrailEffectTimer(long inValue) { if (this->mTrailEffectTimer != inValue) this->mTrailEffectTimer = inValue; }  // (EEUnitClass::UpdateTrail)
    void SetRegenerationTimer(long inValue) { if (this->mRegenerationTimer != inValue) this->mRegenerationTimer = inValue; }  // (EEUnitClass::Regenerate)
    void SetExitingUnitID(long inValue) { if (this->mExitingUnitID != inValue) this->mExitingUnitID = inValue; }  // (EEUCBuilding::SelectUnitToRelease)
    void SetFacing(unsigned long inValue) { if (this->mFacing != inValue) this->mFacing = inValue; }  // (EEUCBuilding::UnitCreated)
    void SetGateDirection(unsigned long inValue) { if (this->mGateDirection != inValue) this->mGateDirection = inValue; }  // (the gate's direction: EEUCGate.cpp)
    // 0x2394b0 (out of line in EEUnitClass.cpp after its first user UpdateRipples's neighbour): mActiveGroup's GetInverseMaxVelocity,
    // else mStats->mFD0 (provisional name)
    float GetInverseMaxVelocity();
    void SetCamouflaged(bool inValue);      // 0x239aad: bit 31 of mCamouflageTimer (EEUnitClass 0x23978b; in EEUnitClass.cpp) (provisional name)
    void SetIsCommandable(bool inValue) { if (this->mIsCommandable != inValue) this->mIsCommandable = inValue; }  // (EEUnitClass::UnitDied)
    void SetB342(bool inValue) { if (this->mIsClearanceGranted != inValue) this->mIsClearanceGranted = inValue; }  // (EEAirTrafficControl::ClearAirportQueue: cmp then store through the address)
    void AddWaypoint(const GE3DPoint& inLocation, bool inSnapToMap);  // 0x1f625b (EEUnitClass::AddWaypointAt) (provisional name)
    // (provisional names; types inferred)
    void ReleaseConversionEffect();         // 0x1f6f99: the mL2C0 effect off
    void KillCargo();                       // 0x1f8af6: unloads every cargo unit (Virtual60(0, 0, 0)), statistics
    static void ReleasePool();              // 0x1f8be7 (cdecl; 0x144295): the pool's stats, then Release
    void RefreshPath();                     // 0x1f8c2d (0x22e47a): the group's path request and the path manager
    void SetScaledModel(GEModel* inModel);  // 0x1fe405 (0xf127d): the model, scaled by the class's mL28C
    void ClearCollisionState();             // 0x138e29 (out of line, another unit's range; TakeOffMap calls it first) (provisional name)
    void UpdateCamouflage(long inAction);   // 0x1ff6b4 (before mCommand changes: StartAction, AIStep) (provisional name)
    void UpdateAreaEffects(unsigned long inValue);  // 0x1ff7f8 (EEAEManager 0x27b693): the area effects at the unit's 2D location (provisional name)
    void ReplaceCargo(EEComplexUnit* inOld, EEComplexUnit* inNew);  // 0x1fecd8: inNew takes inOld's cargo place (provisional name)
    void FaceTowardSquare(long inX, long inY, long inSize);  // 0x1fedfb: faces the centre of the inSize square at (inX, inY)
    bool HasSwimAnimation();                // 0x1566e5 (out of line, EEPhysicsModel's range; MoveToLocation) (provisional name)
    bool IsSwimming();                      // 0x156681 (likewise: the model part 2 / 0xd switch) (provisional name)
    void FindContacts();                    // 0x1f6817: links the units on the tiles around the unit (AddContact both ways)
    void SetNextFrame(long inValue) { if (this->mNextFrame != inValue) this->mNextFrame = inValue; }  // inline (SetAnimation: cmp, lea, store)
    // 0x1fbc3b (ret 0xc): the waypoint on tile (inX, inY), if any
    bool FindWaypointAtTile(long inX, long inY, EEComplexUnitWaypoint*& outWaypoint);
    void SnapLastWaypoint();                // 0x1fc507 (EEPathManager 0xa84f6): re-adds the last waypoint at the goal's location
    // 0x1fc61b (EEPathManager 0xa83da): the class's Virtual0C on ioPath, then its points as waypoints, last first
    void TakePath(std::vector<GE3DPoint>& ioPath, bool inSmooth);
    void PlaceOnGround(const GE3DPoint& inLocation);  // 0x1fc90c (EEPhysicsModel::Move): onto the ground at inLocation

    // Layout (no gaps): the ctor 0x1f5999 (member ctors), its init calls 0x23da8e/0x229706/
    // 0x1fac37, the dtor 0x1f5ba7 (member dtors) and the slots' Load/Save (FSFile Read/Write overloads give the
    // scalar types). Container element types inferred unless a named COMDAT gives them.
    std::vector<EEProductionEntry> mBuildQueue;  // +0x048 the production queue (8-byte elements: `_Buy` 0x14823b)
    std::deque<EEUnit*> mCitizenBuildQueue;  // +0x058 (POD elements; dtor 0x2003a4)
    std::vector<EEComplexUnit*> mCargo;            // +0x06c the cargo (EEUGUnloadTransport 0x2288ea: _Ptrit<EEUnit*>::operator+)
    std::deque<EEComplexUnitWaypoint> mWaypoints;  // +0x07c the path
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
    GEPhysicalModel& GetPhysicalModel() { return this->mPhysicalModel; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    GEPhysicalModel mPhysicalModel;         // +0x130
    void SetAngularVelocity(const GEOrientation& inV) { this->mAngularVelocity = inV; }
    GEOrientation mAngularVelocity;         // +0x17c (saved)
    GEOrientation mOrientation;             // +0x188 (saved)
    std::vector<U2DPoint<long> > mBoundedPathTiles;  // +0x194 (filled from EEGetUnitTiles's vector 0x2373bd; assigned with operator= 0x1a4f4 at 0x419ccb)
    U2DPoint<long> mCollisionTile;          // +0x1a4 (the ctor's dead lea: U2DPoint's inline ctor; Virtual10 prints it as the tile)
    GEColor mColor;                         // +0x1ac (1, 1, 1; saved)
    EEOrbital** mOrbitals;                  // +0x1b8 (+0x1b8..+0x20c: zeroed by 0x1fac37; mAreaEffectTemplate0 copies of the model's orbitals: Virtual64/Virtual30)
    EEWheel** mWheels;                      // +0x1bc (mAreaEffectTemplate1 copies of the model's wheels: Virtual64/Virtual30)
    EEUnitClass* mLauncherClass;            // +0x1c0 provisional name (drop object: the dropped class)
    EEPPatrolGroup* GetPatrolGroup() const { return this->mPatrolGroup; }
    EEPPatrolGroup* mPatrolGroup;           // +0x1c4 (RTTI EEPPatrolGroup, ee/EEPPatrolGroup.h)
    EEActiveGroup* GetActiveGroup() const { return this->mActiveGroup; }
    EEActiveGroup* mActiveGroup;            // +0x1c8 provisional name (0x5e407 and the teleport calamity call into it)
    // +0x1cc (non-zero: not selectable by the aircraft hot key; the unit carrying this one: the fire/plague functors
    // walk the chain to the outermost carrier; provisional name)
    EEComplexUnit* GetPreviousPart() const { return this->mPreviousPart; }
    EEComplexUnit* mPreviousPart;
    EEComplexUnit* GetNextPart() const { return this->mNextPart; }
    EEComplexUnit* mNextPart;               // +0x1d0 the next unit of a linked group (EEDoppelgangerManager 0x20278c walks it)
    EEComplexUnit* mHomeBase;               // +0x1d4
    EEComplexUnit* GetContainer() const { return this->mContainer; }  // inline (0x61c806 budget) r37_big B2
    EEComplexUnit* mContainer;              // +0x1d8 provisional name (aircraft in an airport: its position is the holder's)
    EEComplexUnit* mLastAttacker;                 // +0x1dc (released by Virtual04)
    void SetLastAttacker(EEComplexUnit* inUnit);  // 0x22a3d8 (EnabledChanged 0x229de2; set-if-different with Reference) (provisional name)
    EEComplexUnit* mOwner;                        // +0x1e0 provisional name (SetOwner; referenced)
    EEComplexUnit* GetHolder() const { return this->mHolder; }
    EEComplexUnit* mHolder;                 // +0x1e4 (Virtual04: mRegeneration->ReleaseHeld(this))
    EEUnitAction* GetCommand() const { return this->mCommand; }
    EEUnitAction* mCommand;                 // +0x1e8 the unit's current script action (ee/EEUnitAction.h)
    // +0x1ec a goal (EECPASGUHero::CheckCondition 0x64fd tests its mKind == 0xb; EECPASGUProphet::CanStillCast
    // dynamic_casts it to EEUnitGoal) (provisional name)
    EEUnitGoal* mActiveGoal;
    // +0x1f0 the unit's goal (EEMAddPatrolPoint dynamic_casts it to EEUGPatrol, src TD EEUnitGoal). Its
    // mKind 0xb = not to be cloaked (EECCCloaking). Name is provisional
    EEUnitGoal* GetGoalState() const { return this->mGoalState; }
    EEUnitStoreField<EEUnitGoal*, kUnitFieldGoal> mGoalState;   // (the unit store's)
    RTSUnit* mLastResource;                 // +0x1f4
    EEUnit* mGatheredResource;              // +0x1f8 (SetGatheredResource: referenced, its +0x2b tested)
    EEUnit* mMine;                          // +0x1fc (a unit: the mining building tests its +0x2b/+0x40; provisional name)
    EEHerd* mHerd;                          // +0x200 the unit's herd (EEHerd AddUnit 0xa1b45 sets it, RemoveUnit 0xa3749 clears it)
    float* mTurretAngles;                   // +0x204 (ResizeTurretAngles: delete[], new float[n] zeroed)
    EEUnitBehavior* GetBehavior() const { return this->mBehavior; }
    EEUnitBehavior* mBehavior;              // +0x208 the unit's behaviour (EEUnitAI 0x22d9b5 reads its mB09; EEUIFMGMainGroup compares it with EEUnitBehavior::sTable[i])
    RTSUnit* mBlockingUnit;                 // +0x20c
    float mHeight;                          // +0x210 provisional name (SetHeight; saved)
    float mInitialContactHeading;           // +0x214 (saved)
    float mOpacity;                         // +0x218 (saved)
    float mF21C;                            // +0x21c (SetVelocity; saved)
    float mF220;                            // +0x220 (saved; 2*pi by 0x1fac37)
    float mLaunchYaw;                       // +0x224 (saved)
    float mOpenFraction;                    // +0x228 (saved)
    float mHealSearchTimer;                 // +0x22c (saved)
    float GetPushVelocityX() const { return this->mPushVelocityX; }  // inline value getters
    float GetPushVelocityY() const { return this->mPushVelocityY; }
    float GetCollisionResolutionCount() const { return this->mCollisionResolutionCount; }
    float mPushVelocityX;                   // +0x230
    float mPushVelocityY;                   // +0x234
    float mVelocity;                        // +0x238 (SetVelocity; saved)
    float mAnimationFraction;               // +0x23c
    float mGrowth;                          // +0x240 (saved)
    float GetCollisionRadius() const { return this->mCollisionRadius; }
    float mCollisionRadius;                 // +0x244
    float mCollisionResolutionCount;        // +0x248 (saved)
    // inline value getter (the Physics Virtual0C family: `fld [F98]; fld [ecx]; faddp`, ecx = &mF24C kept from the
    // `!= 0` test; a member read gives `fadd [ecx]`)
    float GetF24C() const { return this->mF24C; }
    float mF24C;                            // +0x24c
    float mSight;                           // +0x250 provisional name (the line-of-sight calamity adds to it; saved)
    unsigned long mCalamityFlags;           // +0x254 the EECalamityClass::mClassFlags of the contagions on it (provisional name)
    unsigned long mNextTriggerTime;         // +0x258 (unsigned: AIStep compares it with gPhysics->mLD0 by jb)
    SESMInstanceNode* mSound;               // +0x25c
    long mCurrentResearch;                  // +0x260 (= 100000; saved)
    unsigned long mAnimation;               // +0x264 (= 7; saved)
    unsigned long mCurrentPhysicsAction;    // +0x268 (= 6; saved)
    unsigned long mCurrentPhysicsOrder;     // +0x26c (= 5; saved)
    unsigned long mFacing;                  // +0x270 (saved)
    unsigned long mGateDirection;           // +0x274 (saved)
    long mNumBuilders;                      // +0x278 (saved)
    long mCrewHitPoints;                    // +0x27c (saved)
    long mProductionTime;                   // +0x280 (saved)
    long mTrailEffectTimer;                 // +0x284 (saved)
    long mGarrisonCount;                    // +0x288
    long mGarrisonPopulation;               // +0x28c (saved)
    long mFlightTime;                       // +0x290
    long mRegenerationTimer;                // +0x294 (saved)
    long mExitingUnitID;                    // +0x298 (saved)
    EEUnitStoreField<unsigned long, kUnitFieldFormationSlot> mFormationSlot;   // +0x29c (the unit store's)
    long mLastHitPoints;                    // +0x2a0
    long mReloadTimer;                      // +0x2a4 (saved)
    long mCargoRoom;                        // +0x2a8 (saved)
    long mRottingAwayTime;                  // +0x2ac (saved)
    long mTargetSearchTimer;                // +0x2b0 (saved)
    unsigned long mNumAreaEffects;          // +0x2b4 (saved)
    long mWakeTimer;                        // +0x2b8 (saved)
    long mWorkTimer;                        // +0x2bc (saved)
    EEEffectHandle mConversionEffect;       // +0x2c0 (saved; -1: none, ReleaseConversionEffect)
    EEEffectHandle mHealEffect;             // +0x2c4 (saved; -1: none, ReleaseHealEffect)
    long mPositiveMorale;                   // +0x2c8 provisional name (the morale calamity counts itself in; saved)
    long mNegativeMorale;                   // +0x2cc provisional name (saved)
    long mPowerRegenTimer;                  // +0x2d0 (saved)
    long mPower;                            // +0x2d4 (calamities compare it with EECalamityClass::GetCost)
    long mNumPathRequests;                  // +0x2d8
    long mIsCollidable;                     // +0x2dc
    long GetFormationRank() const { return this->mFormationRank; }
    long mFormationRank;                    // +0x2e0 (saved)
    long mWaterState;                       // +0x2e4 (saved)
    unsigned long mAmmunition;              // +0x2e8 (saved)
    unsigned long mWeaponTagPoint;          // +0x2ec (saved)
    unsigned long mNumOrbitals;             // +0x2f0 (the orbital count)
    unsigned long mNumWheels;               // +0x2f4 (the wheel count)
    long mNumTurretAngles;                  // +0x2f8
    unsigned long mU2FC;                    // +0x2fc (saved)
    unsigned long mAnimationTime;           // +0x300 (saved)
    unsigned long mConversionStartTime;     // +0x304 (saved)
    unsigned long mTimesUpdated;            // +0x308 (saved)
    long mNextFrame;                        // +0x30c
    EEEffectHandle mFireEffect;             // +0x310 provisional name (the fire functor's effect on the unit; saved)
    unsigned long GetGrazeTime() const { return this->mGrazeTime; }
    unsigned long mGrazeTime;               // +0x314 (saved)
    unsigned long mGrowTime;                // +0x318 (saved)
    unsigned long mNumAttackers;            // +0x31c (saved)
    unsigned long mCloaked;                        // +0x320 provisional name (the cloaking calamity sets it; saved as unsigned long)
    unsigned long mPushImmuneUntil;         // +0x324 (a game time: mLD0 + 5000; compared unsigned)
    unsigned long mCamouflageTimer;         // +0x328 (saved)
    unsigned long mU32C;                    // +0x32c (saved)
    long GetAIState() const { return this->mAIState; }
    long mAIState;                          // +0x330
    long mL334;                             // +0x334
    long mL338;                             // +0x338
    unsigned short mCollisionState;         // +0x33c (saved)
    unsigned short GetExperience() const { return this->mExperience; }
    unsigned short mExperience;             // +0x33e (saved)
    bool mIsDeletable;                      // +0x340 (the contagion functors set it from EEUnitClass::mIsDeletable; provisional name)
    bool GetIsCommandable() const { return this->mIsCommandable; }  // inline (0x61c806 budget) r37_big B2
    bool GetIsDisabled() const { return this->mIsDisabled; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    bool GetIsBuilt() const { return this->mIsBuilt; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    EEUnitStoreField<bool, kUnitFieldCommandable> mIsCommandable;   // +0x341 (the unit store's) (set: selectable by the aircraft hot key; provisional name)
    bool mIsClearanceGranted;               // +0x342 (+0x340..+0x351 saved)
    bool mIsAnimationPaused;                // +0x343
    bool mIsPaidFor;                        // +0x344
    bool mIsCloakable;                      // +0x345
    bool mIsCloaked;                        // +0x346
    bool mIsDisabled;                       // +0x347 (set: not selectable by the aircraft hot key; provisional name)
    bool mIsLocked;                         // +0x348
    bool mIsTinted;                         // +0x349
    bool mIsPathDone;                       // +0x34a
    bool mHasArrived;                       // +0x34b
    EEUnitStoreField<bool, kUnitFieldBuilt> mIsBuilt;   // +0x34c (the unit store's) (drop object: a valid target)
    bool GetIsDetachedFromAI() const { return this->mIsDetachedFromAI; }
    bool mIsDetachedFromAI;                 // +0x34d
    bool mIsFiringHigh;                     // +0x34e
    bool mB34F;                             // +0x34f
    bool GetIsRegrowing() const { return this->mIsRegrowing; }
    bool mIsRegrowing;                      // +0x350
    bool mIsFoundation;                     // +0x351 (NotifyRootVisibility sets it)
    bool mCanBeHidden;                      // +0x352
    bool mIsCitizen;                        // +0x353
    bool mIsPacked;                         // +0x354
    bool mIsSpecialAttack;                  // +0x355 (special attack sets it; saved)
    unsigned short mU356;                   // +0x356 (untouched by the ctor and Load/Save)
    unsigned long mDisabledReason;          // +0x358 (saved)
    unsigned short mPlanetID;               // +0x35c (saved)
    unsigned short mU35E;                   // +0x35e (untouched)
    float GetRoll() const { return this->mRoll; }
    float mRoll;                            // +0x360
    // Port (not in the original, not saved): a human shift-queued a build or repair (EEMBuildUnit, EEMRepairUnit),
    // so the citizen idles at the end of its build queue instead of taking a smart task (EEUGBuildRepair::Step).
    // OrderRepair and ClearBuildTargets clear it, and so does going idle with an empty build queue.
    bool mHasShiftQueuedBuild;
};

// 0x241419 (cdecl): the model a unit of inClass shows for inPlayer (EEComplexUnit::GetModel when +0x351 is set;
// provisional name)
EEModel* EEGetPlayerClassGraphic(EEPlayer* inPlayer, EEUnitClass* inClass);
// 0x16d69e (cdecl): the statistics series of inUnit, 0 for none (EEComplexUnit::GarrisonUnit passes it to
// EEPlayerStatistics::Lower with 1) (provisional name)
long EEAddDeadUnitStatistic(EEComplexUnit* inUnit);
// 0x246c87 (cdecl): the complex unit acting on inUnit, 0 if none (SetGatheredResource) (provisional name)
EEComplexUnit* EEGetWaitingGatherer(EEUnit* inUnit);
// 0x24572b (cdecl): the single flags of inFlags into outFlags (EEComplexUnit::GetCatchableContagions) (provisional
// name)
void EESplitCalamityFlags(unsigned long inFlags, std::vector<unsigned long>& outFlags);
// 0x24b5b3 (cdecl): the tiles inUnit stands on (EEComplexUnit::GetFootprintContinent) (provisional name)
void EEGetFootprintTiles(EEUnit* inUnit, std::vector<EETileCoordinate>& outTiles);
// 0x23d5c3 (cdecl; EEComplexUnit::ReleaseFireEffect for unit type 0x321) (provisional name)
void EERemoveCloakEffect(EEUnit* inUnit);
// 0x23d570 (cdecl; EEUIFMGMainGroup::SetUnit for a selected unit of type 0x321) (provisional name)
void EEAddCloakEffect(EEUnit* inUnit);
// 0x23d5fb (cdecl): the new unit's +0x35c (EEUnitClass::CreateUnitAt for walls/farms) (provisional name)
unsigned short EEGetLandContinentOf(EEComplexUnit* inUnit);

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
// 0x4e872c: EEUCUpgradeTable::sTable's _First (the unit class ctor indexes it with its record +0x238) (provisional
// alias)
struct EEUnitClassImprovementTable;
extern EEUnitClassImprovementTable** gUnitClassImprovementTables;
// Unit action names use the real UString value in EEUnitAction::sActionNames (0x563668).







// 0x22c7e0 (cdecl): the key map of a unit class's +0x08 name (EEUnitClass ctor: mAIScript) (provisional name)
// 0x563a6c is gUnitClassTable[0xf0] (inside the table, not after it): the wall class the ghost of unit type 0x42 adds
// three of (EEGhostUnit::Create); there is no separate gWallUnitClass extern (the readers index the table)
// 0x23ebb2 (cdecl): the wall pieces of class inClass from inStart to inEnd for inPlayer (EEMBuildWall) and 0x23e630
// (cdecl): hands them to inGroup. Provisional names and argument meanings
class EEActiveGroup;
// (the pieces are the planned wall units: 0x23ebb2 push_backs EEUnit*, 0x23e630 reads them as units; inArg5 is a bool:
// `cmp byte ptr [ebp+0x20], 0` and passed whole to CreateUnit's bool)
void EEPlanWall(GE2DPoint inStart, GE2DPoint inEnd, EEUnitClass* inClass, EEPlayer* inPlayer, bool inAsFoundation, bool inSkipPlayerChecks,
                std::vector<EEUnit*>& outPieces);
void EEBuildWall(EEActiveGroup* inGroup, std::vector<EEUnit*>& inPieces);

// 0x24116b / 0x241504 / 0x24dd2e (cdecl): EEUGLoadContainer::GetDestination's fallback (the tiles around inContainer
// for inUnit, then a free location among them) and IsLoadable's tile path search (0: reachable). Provisional names;
// argument meanings inferred
void EEGetTilesAroundUnit(EEComplexUnit* inUnit, EEComplexUnit* inContainer, long inRange, long inCorners, bool inArg5,
                std::vector<EETileCoordinate>& outTiles);
bool EEPickTileNear(std::vector<EETileCoordinate>& inTiles, EEComplexUnit* inUnit, EEComplexUnit* inContainer, bool inIgnoreBlockedLine,
                GE3DPoint& outLocation);
bool EEIsLineBlocked(long inX1, long inY1, long inX2, long inY2, EEUnit** outArg5, long* outX, long* outY);

// 0x12a52e (cdecl): whether inUnit is seen by the current player (the land priest's sounds) (provisional name)
bool EEIsSeen(EEUnit* inUnit);

// 0x245284 (cdecl): the garrison goal's call when a unit with +0x40 enters inContainer (provisional name)
void EEDepositLoad(EEComplexUnit* inUnit, EEComplexUnit* inContainer);

// 0x24939d (cdecl): where a unit dropped next to inTarget lands (provisional name and argument meanings)
void EEFindDropLocation(EEComplexUnit* inUnit, EEUnit** ioTarget, GE3DPoint* outLocation, EEUnit* inTarget);

// 0x20b687 (cdecl): whether tile (inX, inY) takes a unit (special location; first argument 0) (provisional name)
bool EEIsTileFree(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);   // inUnit: the unload goal passes its transport

// 0x20ccb8 (cdecl; the conscription calamity's second call with 0x138d) (provisional name)
void EEPlayUnitEffect(EEComplexUnit* inUnit, long inEffect);

// 0x243561 (cdecl): a free place for inUnit near (inX, inY); true and outLocation/outTransformation when found (the
// teleport calamity) provisional name and argument meanings
bool EEFindPlacement(GE3DPoint* inPoint, EEComplexUnit* inUnit, EEUnitClass* inClass, unsigned long inX,
                     unsigned long inY, long inSize, long inRings, bool inCorners, bool inIncludeCenter, bool inFacePoint,
                     bool inArg11, GETransformation* outTransformation, EEComplexUnit* inTarget);  // the body's types

// EEActiveGroup (the group object at EEComplexUnit +0x1c8) is defined in ee/EEPlayer.h after its base EEGroup
// (RTTI EEActiveGroup > EEGroup, ctor 0x1ebc86)

// 0x24e518 (cdecl; 17 callers, the meteor storm after it has placed its meteor) provisional name and argument types
void EEUpdateUnitTile(GETransformation* inTransformation, EEComplexUnit* inUnit, long* outX, long* outY);

// A unit type (RTTI EEUnitClass > UReferenceCounter; vtable 0x447b10; EEUnitClass.cpp around 0x2358b1). Names
// are provisional.
struct EEUnitClassRecord;                   // a dbunits.dat record (the ctor copies it field by field; +0x374 a count)

// The class's improvement table (EEUnitClass::mImprovements): per improvement attribute a pair of
// floats (EEUIBUpgrade 0x256bd3/0x256c83: `fld [p + attr*8 + 8]` (attr 12 has none) and `[p + attr*8 + 4]` times the
// level), and the maximum level at +0x84 (EEPlayer 0x164fac: signed `jg` against GetImprovementLevel). Names are
// provisional.
struct EEUnitClassImprovementEntry {
    float mCostFactor;
    float mStepFactor;
};

// It is EEUCUpgradeTable read through another type (EEUnitClass::EEUnitClass reinterpret_casts it), so its first member
// stands for that class's vtable pointer: pointer-sized (a 4-byte `long` here put every entry 4 bytes off on 64-bit
// Windows, and the maximum level read a step factor, 0: no improvement could be bought; docs/AI_RESEARCH.md §8.35).
// mMaxLevel is `long` as EEUCUpgradeTable's is (8 bytes on LP64, both).
struct EEUnitClassImprovementTable {
    long GetMaxLevel() const { return this->mMaxLevel; }  // (provisional name; inline-budget evidence: 0x563c6d)
    void* mU00;                             // +0x00 (EEUCUpgradeTable's vtable pointer)
    EEUnitClassImprovementEntry mEntries[16];   // +0x04
    long mMaxLevel;                         // +0x84
};


class EEUnitClass : public UReferenceCounter {
public:
    // 0x22f317 (out of line; every EEUC* ctor calls it with 1, its record and its class's global) (provisional names)
    EEUnitClass(long inIsComplex, EEUnitClassRecord* inRecord, EEPhysicsModel* inPhysicsModel);
    // 0x2364c0: the value for inKey in the map at +0x60 (0 when there is no map or no entry); the bombers ask for
    // 0x7e0 (provisional name)
    EEUnitAction* FindAction(long inKey);
    void UnitRemoved();                     // 0x236a83 (EEUnit::Remove, first) (provisional name)
    void LoadClassMedia();                  // 0x2354ae (EEPlayer::LoadAllClassMedia, per class with loaded media; provisional name)
    void UnloadClassMedia();                // 0x2382fd (EEPlayer::UnloadAllClassMedia, per mSoundLoadedClasses class; provisional name)
    virtual ~EEUnitClass();                 // +0x00 0x2305ee (dereferences the effects and classes)
    virtual void GroundLocation(GE3DPoint& ioLocation);  // +0x04 (the time warp's return, flag 4)
    // +0x08 (base 0x2907a3: empty): the tile under inLocation along the camera line (EEUCNavalCitizen 0x218490)
    virtual void LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY);
    // +0x0c (base 0x2907a3: empty): adjusts a path for the class (EEUCSeaAnimal 0x219d87: random depths) (provisional
    // name)
    virtual void AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth);
    virtual bool CanAttackMove();           // +0x10
    virtual void CargoRemoved(EEComplexUnit* inUnit, EEComplexUnit* inCargo);  // +0x14 (EEUCFortress 0x2101da: ret 8, the cargo)
    // +0x18 (EEComplexUnit::LoadCargo pushes (this, cargo, its third argument) after loading a cargo)
    virtual void CargoAdded(EEComplexUnit* inUnit, EEComplexUnit* inCargo, bool inArg);   // bool: 0x6101b5 `cmp byte`
    virtual void CargoUnloaded(EEComplexUnit* inUnit, EEComplexUnit* inCargo);  // +0x1c (EEUCFortress: the same body 0x2101da)
    virtual void GateReplaced(EEUnit* inGate, EEUnit* inUnit);  // +0x20 (base 0xac9c8: ret 8; EEUCGate 0x2104ef)
    // +0x24 the volcano ClearArea 0xb46bc pushes (unit, &location), stores a float (compile-required; row 62.6).
    // inline (COMDAT 0x216d77 right after EEUCLandPriest.cpp's ctor, with +0x68/+0x6c: the place of inline in-class
    // virtuals; every unit class's vtable points there) inferred body
    virtual float GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation) { return inLocation.GetZ(); }
    // +0x28: the flags of inUnit (EEGroup::GetFlags 0x2062cb Ands them) (provisional name)
    virtual void GetTerrainFlags(EEComplexUnit* inUnit, EECPASGFlags& outFlags);
    // +0x2c: the volume/pan/pitch of inSound at inUnit (EEUGUnloadTransport::Unload 0x228c29)
    virtual void GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch);
    // +0x30: a new unit of this class at (inX, inY) for player inPlayer (the drop-object calamity) (provisional name)
    // (inCreator: EEUnit*, the meteor storm passes its EECalamity::mCasterUnit)
    virtual EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator);
    // +0x34: a new unit of this class at inTransformation for player inPlayer (EECreateCalamityUnit 0xaeb83, the
    // hurricane and volcano calamities) (provisional name)
    virtual EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator);
    // +0x38/+0x3c: a new unit of this class for inPlayer on inMap at a tile / at a point, facing inHeading (the random
    // map's PlaceWall 0x2859e7, PlaceTerrainUnit 0x287c18, 0x288426 / PlaceFeatures 0x2849f2) (provisional names)
    virtual EEUnit* CreateUnitAtTile(EEPlayer* inPlayer, long inX, long inY, EEMap* inMap, float inHeading);
    virtual EEUnit* CreateUnitAtPoint(EEPlayer* inPlayer, float inX, float inY, EEMap* inMap, float inHeading);
    virtual void Flee(EEComplexUnit* inUnit);      // original ret4, all37 actual class vtables
    // +0x44 (the footprint tiles; EEUGBuildRepair 0x21e3a6); the 4th argument is a bool (0x234965 `cmp byte`)
    virtual void GetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long, bool);
    virtual bool IsBuilding();              // +0x48
    virtual bool Virtual4C(unsigned long);  // +0x4c
    virtual void Virtual50(unsigned long);  // +0x50
    virtual bool IsLandBuilding();          // +0x54
    virtual bool IsPartLeftOut(EEComplexUnit* inUnit);  // +0x58 (EEUCGate 0x211370)
    virtual bool FliesOver();               // +0x5c
    virtual bool IsPickable();              // +0x60
    virtual bool IsSelectable(EEUnit* inUnit); // +0x64 (a unit: EEPlayer::SelectUnit 0x16a53b)
    virtual bool CanBeAttacked() { return !this->mIsFlare; }  // +0x68 inline (COMDAT 0x216d81; the Hero unit tests al)
    virtual long GetPopulationCost();       // +0x6c (mL258)
    virtual long Virtual70();               // +0x70
    virtual void ActionChanged(EEComplexUnit* inUnit);  // +0x74 (the cloaking calamity calls it on each cloaked unit)
    bool CanMoveOnTerrain(EEUnit* inUnit, long inTerrainType);  // 0x234ad8 (the missile's +0xe4: true = stays) (provisional name)
    void RouteDockWaypoint(EEComplexUnit* inUnit);  // 0x213db5 (the airplane on its carrier after its move: EEPMAirplane::OrientOnCarrier) (provisional name)
    bool IsAffectedByCalamity(unsigned long inFlag);  // 0x2317d7 (EEComplexUnit::GetCatchableContagions: the class takes that calamity flag) (provisional name)
    bool FacesTarget(EEComplexUnit* inUnit, EEUnit* inTarget, bool inUseTolerance);  // 0x233a14 (EEUGHealUnit)
    // 0x230c55: the splash damage of inUnit's attack around it (inRadius, inDamage, the to-hit kind and weapon column),
    // every unit hit once, inTarget first (provisional name)
    void ApplyAreaDamage(EEComplexUnit* inUnit, float inRadius, float inDamage, long inKind, long inColumn, EEUnit* inTarget);
    // 0x238087: the projectile launch from the weapon tag point (FireShot) (provisional name)
    void LaunchFromTurret(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& inLocation);
    void ApplyHit(EEComplexUnit* inUnit, EEUnit* inTarget);   // 0x230fd2: inUnit's hit on inTarget plus its splash
    // The damage of one hit, the one function of the three forms (ee/EERules.h EEDamageForm; the constants are
    // combat.yaml's): inUnit hits inTarget with inAttack, attack kind inAmmo (dbfamily column) and weapon inWeapon
    // (dbweapontohit row); the elevation rule is this class's. inFriendlyFactor: splash damage on the own player's
    // units. False when the hit does not get through (outAmount untouched), else outAmount (new, not in the original:
    // ApplyHit, ApplyAreaDamage and EEPhysicsModel's trample each had a copy).
    bool ComputeDamage(int inForm, EEComplexUnit* inUnit, EEUnit* inTarget, float inAttack, long inAmmo, long inWeapon,
                       float inFriendlyFactor, long& outAmount);
    void FireShot(EEComplexUnit* inUnit);   // 0x231312 (EEUGAttackGround::Step; provisional name)
    bool FacesPoint(EEComplexUnit* inUnit, const GE3DPoint& inLocation, bool inUseTolerance);  // 0x233a72 (EEUGAttackGround::EvaluateCondition)
    // the building classes' VirtualB8 helpers (EEUCSpaceBuilding 0x21a1cb; provisional names)
    void RefreshSound(EEComplexUnit* inUnit);  // 0x239b47
    void CreateRipples(EEComplexUnit* inUnit);  // 0x234779 (void, ret4; transform effects)
    void HealGarrison(EEComplexUnit* inUnit);  // 0x239c74 (void, ret4; passenger healing)
    void UpdateCloakDetection(EEComplexUnit* inUnit); // original void thiscall, ret4
    bool TurretsFacePoint(EEComplexUnit* inUnit, const GE3DPoint& inPoint, bool inFlag); // original AL result, ret12
    void Regenerate(EEComplexUnit* inUnit);  // 0x239d8a (units whose stats +0x15c is positive)
    bool SharesResearch(long inUnitType);   // 0x234fbd (EEMResearchTechnology: another type that may research; provisional name)
    bool IsDropSite();                      // 0x4249 (inline in the real header: an out-of-line COMDAT; EEUGBuildRepair::FindNextWork) (provisional name)
    bool IsCrusader();                      // 0x5b96d (ICF-shared with EECPAttackSubGroup::Fn5B96D): mUnitType 0x2e6..0x2e9 (0x24722d) (provisional name)
    bool IsTransport();                     // 0x4292 (categories 0x10..: a transport class; EEMImmediateCargoUnload)
    bool IsAirBase();                       // 0x4229 (+0x28 set and +0x1b8 2, 0x15 or 0x20: the icon bar's aircraft carrier test) (provisional name)
    // 0x1fdf06 (inline in the real header? its COMDAT follows EEComplexUnit::StartProduction; EEUnitClass 0x231b55
    // calls it too): mAirTrafficType set, not WriteDetailsHook, category not 0x15/0x20 (provisional name)
    bool IsAircraft();
    // inline (COMDAT 0x221b9, first emitted by EEAIContinentManager.cpp; every caller passes it to an EEDebugTrace
    // and calls it out of line) (provisional name)
    const UString& GetName() { return this->mName; }
    // inline (named by EEUGAttackUnit::SetTarget's assert: 0 <= GetWeapon() < keeucwTotal)
    unsigned long GetWeapon() const { return this->mWeapon; }  // unsigned: the assert compiles to one `cmp 6; jb`
    // inline: the garrison goal's ComputeUpgrade loads both before its tech-tree lookups (0x226bfa) (provisional names)
    long GetTechnology() const { return this->mTechnology; }
    long GetGarrisonTechnology() const { return this->mGarrisonTechnology; }
    bool IsPriest();                        // 0x6ef9 (EEUGAttackUnit's ctor; provisional name)
    long GetNumResearchTechnologies() { return this->mResearchTechnologies.size(); }      // 0x196dff (EECPDefenseCommander 0x78dc8: mResearchTechnologies's count) (provisional name)
    // 0x7028: gUnitClassTable[inUnitType]'s +0x1d0 is non-zero and one of mCounterKind1/mCounterKind2/mL1DC
    // (EECPAttackSubGroupUnit::GetCounterFactor 0x633ad; provisional name)
    bool IsCounteredBy(long inUnitType);
    // EEUnitClass.cpp members (provisional names)
    void AddWaypointAt(EEComplexUnit* inUnit, float inX, float inY, float inZ, bool inSnapToMap);  // 0x23086c
    float GetElevationFactor(EEComplexUnit* inUnit, EEComplexUnit* inTarget);  // 0x23187a (a height factor against inTarget)
    bool CanEnter(EEUnitClass* inClass);    // 0x231723
    // 0x564448: the unit types registered in gUnitClassTable (RegisterUnitClass push_backs them; the vector right after
    // the table; guarded dtor: a class static) (provisional name)
    static std::vector<unsigned long> sUnitTypes;
    static std::vector<unsigned long> GetUnitTypes() { return EEUnitClass::sUnitTypes; }  // by value (EESETSETSetFavoriteUnit 0x18d608 reads the copy's size from the frame)
    static void ReleaseUnitClasses();       // 0x2333fb (ReleaseUnitClass for every unit type)
    static void ReleaseUnitClass(unsigned long inUnitType);  // 0x236b33 (gUnitClassTable[inUnitType] released)
    static void RegisterUnitClass(EEUnitClass* inClass);  // 0x236a9c (registers inClass in gUnitClassTable and sUnitTypes)
    // 0x2358b1: every unit class from dbobjects.dat (one EEUC* per record by its category), then their links and
    // unit-set flags; sUnitTypes sorted by name (Empire Earth.cpp 0x1424a8) (provisional name)
    static void CreateUnitClasses();
    // 0x23302d..0x233306 (cdecl): the terrain under a farm-like unit levelled to its tile's height, four orientations
    static void LevelRunwayPosX(EEComplexUnit* inUnit);  // 0x23302d
    static void LevelRunwayPosY(EEComplexUnit* inUnit);  // 0x233122
    static void LevelRunwayNegY(EEComplexUnit* inUnit);  // 0x233210
    static void LevelRunwayNegX(EEComplexUnit* inUnit);  // 0x233306
    bool ProduceUnit(EEComplexUnit* inUnit, EEUnitClass* inClass, bool inArg); // original thiscall ret12
    void CreateDeathUnit(EEComplexUnit* inUnit); // original class update ret4
    long GetPassableFlags(EEUnit* inUnit);  // 0x2349b1 (the CanMoveOnTerrain terrain bits)
    bool FindTargetNear(EEComplexUnit* inUnit, float inRange, EEUnit** outTarget);  // 0x234a05 (a target near the unit)
    long GetNumGoalQueues();                // 0x234ea2
    static void GetClassesInSet(unsigned long inSet, std::vector<EEUnitClass*>& outClasses);  // 0x234ee7
    bool SharesRallyPoint(unsigned long inUnitType);  // 0x234f34
    void RegeneratePower(EEComplexUnit* inUnit);  // 0x239ac7 (the unit regenerates mPower every 5 s)
    // 0x23094b: the terrain height for a new unit of this class at tile (inX, inY): the footprint's average (types
    // 0x2ba/0x34) or its levelled minimum (the mesh and the map tiles set to it) (provisional name)
    void LevelGround(GETerrainMesh* inMesh, unsigned long inX, unsigned long inY, float* outZ);
    void UpdateTrail(EEComplexUnit* inUnit);  // 0x2393a9 (the unit's +0x284 timer; the EEUC* VirtualB8s) (provisional name)
    void TurnTurretsBack(EEComplexUnit* inUnit);  // 0x2364f3 (the unit's mTabButtons values back towards 0; TurnTurrets) (provisional name)
    void MoveToTileCentre(EEComplexUnit* inUnit);  // 0x23658b (to its tile centre, or a step forward; 0x22db0f) (provisional name)
    bool AttackNearby(EEComplexUnit* inUnit, EEUnitGoal* inGoal);  // 0x22d95f (ret 8; this unread): an attack goal on a target (EEUnitAI.cpp) (provisional name)
    void CreateRunways(EEComplexUnit* inUnit, EEPlayer* inPlayer, bool inIsFoundation);  // 0x232e4a (CreateUnitAt for unit type 0x42; bool: CreateUnitAt passes its bool through) (provisional name)
    void ReplantFarm(EEComplexUnit* inUnit, EEPlayer* inPlayer);  // 0x232c9f (EEMReplantFarms, farm classes)
    // EEUCProjectile.cpp's callees (provisional names)
    void HandleActionChanged(EEComplexUnit* inUnit);  // 0x22db0f (the default +0x74 command handling)
    EEUnit* FindImpactUnit(EEComplexUnit* inUnit);  // 0x233410 (a target for inUnit, or 0)
    void CreateRemainsUnit(EEComplexUnit* inUnit);  // 0x237e79 (the class's +0x108 unit where inUnit ends)
    void UpdateRipples(EEComplexUnit* inUnit);  // 0x2393ff (like UpdateTrail, the +0x80 media)
    void UpdateAnimal(EEComplexUnit* inUnit);  // 0x239644 (EEUCLand::Update for a herd animal of a player) (provisional name)
    void UpdateCamouflageTimer(EEComplexUnit* inUnit);  // 0x23978b (EEUCSpaceship::Update)
    void TurnTurrets(EEComplexUnit* inUnit);  // 0x238948 (EEComplexUnit::UpdateTurrets: a unit whose model has parts) (provisional name)
    void UpdateBuilding(EEComplexUnit* inUnit);  // 0x2394cd (EEUCBuilding::Update) (provisional name)
    // 0x234b05 (EEPickTileNear): the continent of the last of the unit's Virtual44 tiles that is on a kind-1 continent, a
    // non-9 terrain tile and inTarget's continent (when given); else the last continent looked at (provisional name)
    EEAIContinent* GetContainerContinent(EEComplexUnit* inUnit, EEComplexUnit* inTarget);
    void LaunchStoredMissile(EEComplexUnit* inUnit);  // 0x20e50c (in EEUCBomb.cpp's range; FireShot: the carrier drops its first bomb) (provisional name)
    void LaunchShot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& inLocation, EEComplexUnit* inCargo);  // 0x2340a7 (LaunchStoredMissile) (provisional name)
    void FireOnAttackFrame(EEComplexUnit* inUnit, long inArg, long inFrame);  // 0x2386cf (the spaceship's attack commands)
    void UpdateDone(EEComplexUnit* inUnit);   // 0x157269 (empty, `ret 4`; ICF-folded; the last call of EEUCSpaceship::Update)
    // 0x23525a: moves inUnit from inFrom towards inTo (EEUCPlanetSpaceship::ActionChanged's landing) (provisional name)
    void PathToward(EEComplexUnit* inUnit, GE3DPoint& inFrom, GE3DPoint& inTo, bool inSnapToMap);
    // +0x78..+0xdc: arguments by `ret N` (inferred types)
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 (EEUCMissile: the unit's launch)
    virtual void SimpleUnitCreated(EEUnit* inUnit);  // +0x7c 0x2351ba (the unit's hit points to its maximum)
    virtual void AddToRegion(EEUnit* inUnit);  // +0x80 0x2351cf (adds the unit to its map region)
    virtual void AddToTile(EEUnit* inUnit);   // +0x84 0x23523c (reads the unit's tile; EEUCResource adds it to its tiles)
    virtual bool HasArrived(EEComplexUnit* inUnit, float inValue);  // +0x88
    virtual void BadlyHurt(EEUnit* inUnit);   // +0x8c (EEUCLandTransport: its cargo)
    // +0x90 0x2369b9; the splash damage 0x230fd2 passes (the hit complex unit, the attacker, the damage)
    virtual void UnitDamaged(EEComplexUnit* inUnit, EEComplexUnit* inAttacker, long inDamage);
    virtual void RemoveFromRegion(EEUnit* inUnit);  // +0x94 0x236ac6 (removes the unit from its map region)
    virtual void RemoveFromTile(EEUnit* inUnit);  // +0x98 0x236b55 (the unit leaves its tile)
    virtual void FleeAttacker(EEComplexUnit* inUnit);  // +0x9c
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0
    virtual void LoadButtons(EEPlayer* inPlayer);  // +0xa4 0x235527 (loads the class buttons and calamity media for inPlayer)
    virtual void LoadMedia(EEPlayer* inPlayer);   // +0xa8
    virtual bool LaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation);  // +0xac
    virtual void UnloadButtons(EEPlayer* inPlayer);  // +0xb0
    virtual void UnloadMedia(EEPlayer* inPlayer);  // +0xb4
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 (EEUCLandTransport: its wheels)
    virtual void UpdateEffects(EEComplexUnit* inUnit);  // +0xbc (the EEUC* bodies read a complex unit: EEUCProjectile 0x219ae9)
    virtual long Animate(EEComplexUnit* inUnit);     // +0xc0 0x238ce7 (returns a value: EEUCSpaceship::Update keeps it)
    virtual void UnitDied(EEComplexUnit* inUnit);    // +0xc4 0x233f0e (the unit dies: its death class, effect, sound)
    virtual void FindSafeLocation(EEComplexUnit* inUnit);  // +0xc8 0x234000 (a computer player's unit moves off)
    virtual void NextWaypoint(EEComplexUnit* inUnit);  // +0xcc
    virtual void Heal(EEComplexUnit* inUnit);        // +0xd0
    virtual void PathToGoal(EEComplexUnit* inUnit);  // +0xd4
    virtual void FindPath(EEComplexUnit* inUnit);    // +0xd8
    virtual void Attacked(EEComplexUnit* inUnit);    // +0xdc

    // inline (EEContagionFunctor::CanInfect 0xb66ff loads both before the virtual call)
    unsigned long GetCategory() const { return this->mCategory; }
    bool IsAirCategory() const { return this->mCategory == kUnitCategorySubmergedNaval || this->mCategory == kUnitCategorySeaAnimal; }  // inline (0x5f6817) (provisional name)
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
    EEComplexUnit* GetLastLinked(EEComplexUnit* inUnit);
    EEComplexUnit* GetLastLinkedCarrier(EEComplexUnit* inUnit);

    // Layout (no gaps; sizeof 0x32c: every subclass EEUC* is 0x32c): the ctor 0x22f317
    // (EEUnitClass(bool, record, ...): it copies a database record field by field) and the dtor 0x2305ee. Types by
    // the stores' width (float: fld/fstp); member names are provisional.
    UString mAIScriptName;                  // +0x008 the ctor's member ctor, then UString::Copy
    UString mName;                          // +0x018 the ctor's member ctor, then UString::Copy
    long mAirTrafficType;                   // +0x028 the ctor stores it
    std::vector<long> mResearchTechnologies;  // +0x02c 4-byte elements: the ctor's `_Buy(0)`
    unsigned long mCommands[9];             // +0x03c the command buttons (30: formation); 9 separate ctor stores
    std::map<long, EEUnitAction*>* mAIScript;  // +0x060 the class's AI script (EEUnitAction::GetScript of its name; FindAction finds an action in it)
    EEGraphicEffect* mAttachedEffect;   // +0x064 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mDeathEffect;          // +0x068 provisional name (VirtualC4: played on the dying unit)
    EEGraphicEffect* mDamageEffect;     // +0x06c (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mHealEffect;       // +0x070 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mFireTrailEffect;  // +0x074 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mTrailEffect;      // +0x078 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mMuzzleFlashEffect;  // +0x07c (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mRippleEffect;     // +0x080 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mSplashEffect;     // +0x084 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mImpactSparksEffect;  // +0x088 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mImpactBloodEffect;  // +0x08c (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mEffect090;        // +0x090 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mCreationEffect;       // +0x094 provisional name (PlayCreationEffect 0x234084)
    EEGraphicEffect* mEffect098;        // +0x098 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mEffect09C;        // +0x09c (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mEffect0A0;        // +0x0a0 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mEffect0A4;        // +0x0a4 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mEffect0A8;        // +0x0a8 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mSpecialAttackEffect;  // +0x0ac (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mEffect0B0;        // +0x0b0 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mShotEffect;       // +0x0b4 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mLeprosyEffect;        // +0x0b8 provisional name (EELeprosyFunctor plays it on an infected unit)
    EEGraphicEffect* mPlagueEffect;         // +0x0bc provisional name (EEPlagueFunctor)
    EEGraphicEffect* mPulseCannonEffect;  // +0x0c0 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mMoraleEffect;         // +0x0c4 provisional name (the morale calamity plays it on the unit)
    EEGraphicEffect* mBattleCryEffect;  // +0x0c8 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mShellHoleEffect;  // +0x0cc (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mEffect0D0;        // +0x0d0 (dereferenced by the dtor 0x2305ee: an effect)
    EEGraphicEffect* mBloodStainEffect;  // +0x0d4 (dereferenced by the dtor 0x2305ee: an effect)
    // +0x0d8..+0x0f4: the class media (LoadClassMedia loads each through EEMedia +0x04, UnloadClassMedia unloads
    // through +0x08, two loops of two; the button through EEButtonObject::Acquire/Release) (provisional names)
    EEModel* mModel;                        // +0x0d8 (EEUnitClassUpgrade::Init hands it to SetModel: its mModel)
    EEButtonObject* mButton;                // +0x0dc
    EESoundObject* mCreationSound;          // +0x0e0 (EESoundObject::Get in the ctor)
    EESoundObject* mAcknowledgeSounds[2];   // +0x0e4 (LoadClassMedia/UnloadClassMedia loop over both)
    EESoundObject* mSelectionSounds[2];     // +0x0ec (likewise)
    EESoundObject* mDeathSound;             // +0x0f4
    EEUnitClass* mProjectileClass;           // +0x0f8 (dereferenced and cleared by the dtor 0x2305ee; a unit class)
    EEUnitClass* mScaffoldingClass;          // +0x0fc (dereferenced and cleared by the dtor 0x2305ee; a unit class)
    EEUnitClass* mDeathClass;                // +0x100 (dereferenced and cleared by the dtor 0x2305ee; VirtualC4 creates a unit of it where the unit dies)
    EEUnitClass* mDeathSpawnClass;           // +0x104 (dereferenced and cleared by the dtor 0x2305ee; a unit class)
    EEUnitClass* mRemainsClass;              // +0x108 (dereferenced and cleared by the dtor 0x2305ee; a unit class)
    EEUnitClass* mPackedClass;              // +0x10c the ctor stores it (the class it packs/unpacks into: EEMPackUnpack)
    EEPhysicsModel* mPhysicsModel;          // +0x110 (the class's physics model: EEUCSpaceBuilding/NavalBuilding::VirtualB8 call its 0x156897) (provisional name)
    EEUnitClassImprovementTable* mImprovements;   // +0x114 the ctor stores it
    EEUnitBehavior* GetBehavior() const { return this->mBehavior; }
    EEUnitBehavior* mBehavior;              // +0x118 the ctor stores it (EEUnitBehavior::sTable[record +0x390]; the units' default behaviour)
    float mCruiseAltitude;                  // +0x11c the ctor stores it (a float: the space building's z, EEUCSpaceBuilding 0x219fe3)
    float mMinRange;                        // +0x120 the ctor stores it (a float: 0x243e0a fld/fmul; the record's +0x80 copied with mov)
    float mRange;                           // +0x124 the ctor stores it (a float: the ctor compares it with mLineOfSight)
    float mAreaEffect;                       // +0x128 the ctor stores it
    float mAreaEffectScale;                 // +0x12c the ctor stores it
    // inline (EEE3DDamageVent::SetUp 0xeabb5 loads it as a value before the effect's height)
    // (provisional name)
    float GetScale() const { return this->mScale; }
    float mScale;                           // +0x130 the ctor stores it
    float GetMaxVelocity() const { return this->mMaxVelocity; }  // inline (read as a value)
    float GetRange() const { return this->mRange; }  // inline (EESMObject 0x5c8871)
    float GetAreaEffect() const { return this->mAreaEffect; }  // inline (0x66cbe1)
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
    float GetTurretMaxAngularVelocity() const { return this->mTurretMaxAngularVelocity; }  // inline (read as a value)
    float mMaxVelocity;                     // +0x134 the ctor stores it (a float: EETown 0xac2b6 compares it with 0.0f)
    float mF138;                            // +0x138 the ctor stores it
    float mAcceleration;                    // +0x13c the ctor stores it (the projectile models' gravity factor: EEPMProjectile 0x15d259)
    float mAngularAcceleration;             // +0x140 (> 0: the editor may rotate the unit; EESEARotateUnits 0x1aa794)
    float GetAngularAcceleration() const { return this->mAngularAcceleration; }  // inline (EEPMNaval::PitchMode 0x55c146 loads it as a value: fld; fdivp) (provisional name)
    float mMaxAngularVelocity;              // +0x144 the ctor stores it (a float: EEGhostUnit::Create applies the rotation only when non-zero)
    float mCostScale;                       // +0x148 the ctor stores it
    float mTurretMaxAngularVelocity;        // +0x14c actual turret angular velocity, original FLD/FMUL
    float mBuildTimeScale;                  // +0x150 the ctor stores it
    float mGarrisonBonus;                   // +0x154 the ctor stores it (fld: EEAEEconomyFromUnits::GetRate 0x27ac9e)
    float GetMaxRoll() const { return this->mMaxRoll; }  // inline value getter (0x55da2c / 0x55937a)
    float mMaxRoll;                         // +0x158 the ctor stores it (a float: EEPMAirplane::ComputeRoll 0x55da2c)
    float GetFacingOffset() const { return this->mFacingOffset; }
    float mFacingOffset;                    // +0x15c the ctor stores it (a float: EEPhysicsModel::TurnTowardAngle 0x554738 reads it)
    float mFiringArc;                       // +0x160 the ctor stores 0x3eb2b8c3; 0x148291 is called on it
    float mLineOfSight;                     // +0x164 the ctor stores it
    float mSelectionSize;                   // +0x168 the ctor stores it (a float: EEE3DCommandConfirmation::SetUp fld/fmul)
    float mShadowScale;                     // +0x16c the ctor stores it (a float: EERenderObject fld's it)
    float mL170;                            // +0x170 the ctor stores it (a float, as mL16C)
    float mOuterCollisionSphere;            // +0x174 the ctor stores it (a float: EEMChangeSelectedUnitCollisionSphere fadds it)
    float mInnerCollisionSphere;            // +0x178 the ctor stores it (a float: the hurricane fld's it)
    float mLaunchPitch;                     // +0x17c the ctor stores it
    long mL180;                             // +0x180 the ctor stores it
    float GetConversionResistance() const { return this->mConversionResistance; }  // inline (EEUnitClassUpgrade::Init 0x63b891 copies with fld/fstp)
    float GetConversionRate() const { return this->mConversionRate; }
    float mConversionResistance;            // +0x184 the ctor stores it (a float: EEUnitClassUpgrade::Init 0x63ba8f fld's it)
    float mConversionRate;                  // +0x188 the ctor stores it
    float mSlopeSpeedPenalty;                            // +0x18c the ctor stores it (a float: EEPhysicsModel::SpeedMode 0x554cb2 reads it)
    float mBobHeight;                       // +0x190 the ctor stores it (the helicopter's bob height: EEPMHelicopter 0x158ce3)
    float GetBobHeight() const { return this->mBobHeight; }  // inline (EEPMHelicopter::ApplyBob 0x558ce3 loads it as a value: fld; fmul st(2)) (provisional name)
    float mBobPitch;                        // +0x194 the ctor stores it
    float GetBobPitch() const { return this->mBobPitch; }  // inline (EEPMNaval::ApplyBob 0x55b668 loads it as a value before the bob: fld [eax+0x194]) (provisional name)
    float mFriendlyDamageFactor;            // +0x198 the ctor stores it (a float: EEUnitClass 0x630db1/0x630ef9 fld it)
    float mF19C;                            // +0x19c the ctor stores it
    // inline (the heal goal's Virtual64 0x22569f loads it as an FP value after the ratio; provisional name)
    float GetHealThreshold() const { return this->mHealThreshold; }
    float mHealThreshold;                   // +0x1a0 the ctor stores it (a float: the heal goal fld's it as the health fraction to heal below)
    float mHealRange;                       // +0x1a4 the ctor stores it (the heal range: squared against a distance)
    float mHealDelay;                       // +0x1a8 the ctor stores it (a float: SetCursor compares it with the unit's +0x22c)
    float mLoadRange;                       // +0x1ac the ctor stores it (a float: the load goals add it to their distance)
    float mUnloadRange;                     // +0x1b0 the ctor stores it (a float: the unload goal squares it as its range)
    long mIsComplex;                        // +0x1b4 the ctor stores it
    unsigned long mCategory;                       // +0x1b8 provisional name (3, 0x22: aircraft; 6, 0x16: the hot keys' idle workers)
    unsigned long mUnitType;                // +0x1bc (unsigned: EESGUnitSorter::Compare 0x209018 ends in setb) EEUnitSetManager::Contains; cloaking checks 0x29
    long mCombatReaction;                   // +0x1c0 the ctor stores it
    long mWeapon;                           // +0x1c4 the ctor stores it
    unsigned long mFamily;                  // +0x1c8 (provisional name; aircraft hot keys: with +0x1cc the aircraft kind; unsigned: EESEUnitLayerLess 0x199e45 jbe/jae)
    long mAmmoType;                         // +0x1cc (provisional name)
    unsigned long GetAIKind() const { return this->mAIKind; }
    unsigned long mAIKind;                         // +0x1d0 the ctor stores it
    unsigned long mCounterKind1;            // +0x1d4 the ctor stores it (unsigned: 0x49b34e's map<long>::find takes a converted temp)
    unsigned long mCounterKind2;            // +0x1d8 the ctor stores it
    unsigned long mCounterKind3;            // +0x1dc the ctor stores it
    long mKindFlags;                        // +0x1e0 the ctor stores it
    long mTechnology;                       // +0x1e4 the ctor stores it
    long mGarrisonTechnology;               // +0x1e8 the ctor stores it
    long mWaterKind;                        // +0x1ec the ctor stores it
    long mArmor[6];                         // +0x1f0 the base armor per weapon kind (EECBUpgradeArmor 0x28239e indexes it)
    long mL208;                             // +0x208 the ctor stores it
    long GetEpoch() const { return this->mEpoch; }
    long mEpoch;                            // +0x20c the ctor stores it
    long mReloadTime;                       // +0x210 the ctor stores it
    long mAttack;                           // +0x214 the ctor stores it
    long mWorkRate;                         // +0x218 the ctor stores it
    long mGatherTimes[7];                   // +0x21c..+0x234 (the ctor zeroes it in a loop, rep stosd; EEUnitClassUpgrade::Init copies it)
    long mFlightTime;                       // +0x238 the ctor stores it
    long mLeadership;                       // +0x23c the ctor stores it
    long mMaxObjectCarry;                   // +0x240 the ctor stores it
    long GetInfluence() const { return this->mInfluence; }
    long mInfluence;                        // +0x244 the ctor stores it
    long mRottingTime;                      // +0x248 the ctor stores it
    long GetTransportCapacity() const { return this->mTransportCapacity; }
    long mTransportCapacity;                // +0x24c the ctor stores it
    long mGarrisonCapacity;                 // +0x250 the ctor stores it
    long mRepairRate;                       // +0x254 the ctor stores it
    long mWeight;                           // +0x258 the ctor stores it
    long mHitPoints;                        // +0x25c the ctor stores it
    long mNameStringID;                     // +0x260 the ctor stores it
    long mTargetSearchInterval;             // +0x264 the ctor stores it
    long mResourceType;                     // +0x268 the ctor stores it
    long mResourceAmount;                   // +0x26c the ctor stores it
    long mCarryAmount;                      // +0x270 the ctor stores it
    long mHealingPoints;                    // +0x274 the ctor stores it
    long mRegeneration;                     // +0x278 the ctor stores it
    long mPowerRegeneration;                // +0x27c the ctor stores it
    long mPower;                            // +0x280 the ctor stores it
    long mL284;                             // +0x284 the ctor stores it
    long mMissileDelayTime;                 // +0x288 the ctor stores it
    long mTileSize;                         // +0x28c the ctor stores it (EEFindPlacement's 6th argument; read through GetTileSize)
    long mCloakRadius;                      // +0x290 the ctor stores it
    long mL294;                             // +0x294 the ctor stores it
    long mOverlapUnitSet;                   // +0x298 the ctor stores it
    long mAmmunition;                       // +0x29c the ctor stores it
    long mNumUnits;                         // +0x2a0 the ctor stores it
    long mLastUnitGoneTime;                 // +0x2a4 the ctor stores it
    long mMiniMapColor;                     // +0x2a8 the ctor stores it
    long GetMiniMapColor() const { return this->mMiniMapColor; }  // inline (EEMiniMap 0x51e53b loads it before the colour pushes) (provisional name)
    long mInitialGarrisonCount;             // +0x2ac the ctor stores it
    long mWonderBonus;                      // +0x2b0 the ctor stores it
    long mWonderUnitSet;                    // +0x2b4 the ctor stores it
    long mMinConversionTime;                // +0x2b8 the ctor stores it
    // +0x2bc..+0x2c8 the area-effect template ids: one array (EEUnitClassUpgrade::Init 0x63b891 copies them in a loop;
    // the ctor stores the four one by one) (provisional name)
    long mAreaEffectTemplates[4];
    //
    long mAnimalID;                         // +0x2cc the ctor stores it
    unsigned char mDomains;                 // +0x2d0 the ctor stores it (the time warp tests bits 1 and 4)
    bool mB2D1;                             // +0x2d1 the ctor stores it
    bool mTerrainTypes[17];                 // +0x2d2..+0x2e2 (the ctor memsets 0x11 bytes) the ctor stores each; per terrain type, whether the class may stand on it (CanMoveOnTerrain, EERandomMap 0x2849f2) (provisional name)
    bool mIsAffectedByGravity;              // +0x2e3 the ctor stores it
    bool mUsesAmmunition;                   // +0x2e4 the ctor stores it
    bool mShootsHigh;                       // +0x2e5 the ctor stores it
    bool mUpgradesAreaEffect;               // +0x2e6 the ctor stores it
    bool mUpgradesRepair;                   // +0x2e7 the ctor stores it
    bool mUpgradesShockArmor;               // +0x2e8 the ctor stores it
    bool mUpgradesPierceArmor;              // +0x2e9 the ctor stores it
    bool mUpgradesArrowArmor;               // +0x2ea the ctor stores it
    bool mUpgradesLaserArmor;               // +0x2eb the ctor stores it
    bool mUpgradesGunArmor;                 // +0x2ec the ctor stores it
    bool mUpgradesMissileArmor;             // +0x2ed the ctor stores it
    bool mUpgradesRange;                    // +0x2ee the ctor stores it
    bool mPitchesWithTerrain;               // +0x2ef the ctor stores it
    bool mRollsWithTerrain;                 // +0x2f0 the ctor stores it
    bool mFiresBroadside;                   // +0x2f1 the ctor stores it
    bool mCanAttackGround;                  // +0x2f2 the ctor stores it
    bool mCanFireOverWalls;                 // +0x2f3 the ctor stores it
    bool mIsScenarioSelectable;             // +0x2f4 the ctor stores it
    bool mIsEnabled;                        // +0x2f5 the ctor stores it
    bool mFacesCamera;                             // +0x2f6 the ctor stores it
    bool mIsDecoration;                     // +0x2f7 the ctor stores it
    bool mFiresWhileMoving;                 // +0x2f8 the ctor stores it
    bool mCanFly;                           // +0x2f9 the ctor stores it
    bool mB2FA;                             // +0x2fa the ctor stores it
    bool mBuildsOnElevation;                // +0x2fb the ctor stores it
    bool GetBlocksTile() const { return this->mBlocksTile; }
    bool mBlocksTile;                       // +0x2fc the ctor stores it
    bool GetCanEnterAirTransports() const { return this->mCanEnterAirTransports; }
    bool mCanEnterAirTransports;            // +0x2fd the ctor stores it
    bool mCanEnterLandTransports;           // +0x2fe the ctor stores it
    bool GetCanEnterSeaTransports() const { return this->mCanEnterSeaTransports; }
    bool mCanEnterSeaTransports;            // +0x2ff the ctor stores it
    bool GetCanEnterSpaceTransports() const { return this->mCanEnterSpaceTransports; }
    bool mCanEnterSpaceTransports;          // +0x300 the ctor stores it
    bool mCanEnterFortresses;               // +0x301 the ctor stores it
    bool mCanGarrison;                      // +0x302 the ctor stores it
    bool GetIsCollidable() const { return this->mIsCollidable; }
    bool mIsCollidable;                     // +0x303 the ctor stores it
    bool mCanSetRallyPoint;                 // +0x304 the ctor stores it
    bool mB305;                             // +0x305 the ctor stores it
    bool mIsGaiaPlaceable;                  // +0x306 the ctor stores it
    bool mConvertsBuildings;                // +0x307 the ctor stores it
    bool mConvertsPriests;                  // +0x308 the ctor stores it
    bool mIsPlayerPlaceable;                // +0x309 the ctor stores it
    bool mIsVital;                          // +0x30a the ctor stores it
    bool mIsCloakable;                      // +0x30b the ctor stores it
    bool mIsExpansionUnit;                             // +0x30c the ctor stores it
    bool mCanEnterAirports;                 // +0x30d the ctor stores it
    bool mCanEnterCarriers;                 // +0x30e the ctor stores it
    bool mCanEnterNavyYards;                // +0x30f the ctor stores it
    bool GetIsWall() const { return this->mIsWall; }
    bool mIsWall;                           // +0x310 the ctor stores it
    bool mIsDeletable;                      // +0x311 the contagion functors copy it to EEComplexUnit::mIsDeletable; (provisional name)
    bool mIsInactive;                       // +0x312 the ctor stores it
    bool GetIsAttackUnit() const { return this->mIsAttackUnit; }
    bool mIsAttackUnit;                     // +0x313 the ctor stores it
    bool GetIsDefenseUnit() const { return this->mIsDefenseUnit; }
    bool mIsDefenseUnit;                    // +0x314 the ctor stores it
    bool mCanCrossForest;                             // +0x315 the ctor stores it
    bool mIsObstacle;                       // +0x316 the ctor stores it
    bool mIsTree;                           // +0x317 the ctor stores it
    bool mIsMelee;                          // +0x318 the ctor stores it
    bool mCanDisband;                       // +0x319 the ctor stores it
    bool mIsHuman;                          // +0x31a the ctor stores it
    bool mIsHorse;                          // +0x31b the ctor stores it
    bool mIsTower;                          // +0x31c the ctor stores it
    bool GetHasCalamityPowers() const { return this->mHasCalamityPowers; }
    bool mHasCalamityPowers;                // +0x31d the ctor stores it
    bool mIsProjectile;                     // +0x31e the ctor stores it
    bool mCanChangeBehavior;                // +0x31f the ctor stores it
    bool mIsCyber;                          // +0x320 the ctor stores it
    bool mCanPatrol;                        // +0x321 the ctor stores it
    bool mIsMedic;                          // +0x322 (untouched by the ctor)
    bool mSkipsLineOfFireCheck;             // +0x323 the ctor stores it
    bool mIsFlare;                          // +0x324 (untouched by the ctor; MarkInvalid; the doppelganger manager)
    bool mIsBoobyTrap;                      // +0x325 (untouched by the ctor; the doppelganger manager)
    bool mIsParatrooperBomber;              // +0x326 (untouched by the ctor)
    bool mHasSpecialAttack;                 // +0x327 the ctor stores it
    volatile bool mIsMediaLoaded;           // +0x328 the ctor stores it (volatile: VirtualB4 0x638432 clears it with `mov byte`, not `and`)
    bool mB329;                             // +0x329 (untouched by the ctor)
    short mS32A;                            // +0x32a (untouched by the ctor)
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
