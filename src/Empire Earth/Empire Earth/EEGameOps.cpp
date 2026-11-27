// New (port, game core lane round 7): the C++ calls and members of the game's bodies that moved to Rust (ee_game
// gamecpp: the calamity classes' world steps, ...), one case each (EeGameCpp). p is the object, a the integer and
// pointer arguments, f the float arguments; a pointer answers as an integer, a float through ee_gamecpp_f. The GE
// value types cross as their Rust mirrors' addresses (GE3DPoint, GETransformation, GERectangle, U2DPoint<float>).
#include <windows.h>
#include <vector>
#include <algorithm>
#include <math.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESoundManager.h"
#include "lle/USingleton.h"
#include "lle/GEModel.h"
#include "lle/GEViewport.h"

#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EEAEManager.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECombatGoals.h"
#include "ee/EEGraphics.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEGameVariant.h"
#include "ee/EEMouse.h"
#include "ee/EEUnitPicker.h"
#include "ee/EESaveGameFiles.h"

// the constants the Rust passes (ee_game gamecpp calamity.rs `k`, `cap`)
static_assert(kActionAttackEnemyUnit == 2001 && kActionCalamityPreCast == 2030 && kActionSpecialLocation == 2032 && kActionIdle == 5000 && kActionOpen == 15000 && kMovementAir == 1 && kMovementSea == 4 && kUnitTypeCapitol == 0x29 && kUnitCategoryGate == 0x19 && kGoalCastCalamity == 11 && kTechConscriptionSpell == 1920 && kEffect2DLightning == 0x8 && kEffect3DDisease == 0x23 && kEffect3DTracerBeam == 0x40 && kCalamityStartSound == 2, "gamecpp calamity.rs");

// the world bodies' cases (EEWorld.cpp)
int64_t EEWorldOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEWorldOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define UN ((EEUnit*)p)
#define CX ((EEComplexUnit*)p)
#define UC ((EEUnitClass*)p)
#define PL ((EEPlayer*)p)
#define CL ((EECalamity*)p)
#define KL ((EECalamityClass*)p)
#define HO ((EECalamityUpdateHost*)p)
#define MG ((EECalamityManager*)p)
#define PT(i) (*(GE3DPoint*)a[i])
#define TR(i) (*(GETransformation*)a[i])
#define HANDLE(i) ((EEEffectHandle*)a[i])

extern "C" int64_t ee_gamecpp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the units
    case O::UnRts: return P64(static_cast<EeUnitsRtsUnitFields*>(UN));
    case O::UnFields: return P64(static_cast<EeUnitsUnitFields*>(UN));
    case O::CxFields: return P64(static_cast<EeUnitsComplexUnitFields*>(CX));
    case O::UnComplex: return P64(UN->mComplexUnit);
    case O::UnGetComplex: return P64(UN->GetComplexUnit());
    case O::UnPlayer: return P64(UN->mPlayer);
    case O::UnClass: return P64(UN->mUnitClass);
    case O::UnTileNode: return P64(UN->mTileNode);
    case O::UnPosition: UN->GetPosition(PT(0));
        break;
    case O::UnGetModel: return P64(UN->GetModel());
    case O::UnKill: UN->Kill(a[0] != 0, a[1] != 0, a[2] != 0);
        break;
    case O::UnTakeHit: UN->TakeHit((EEUnit*)a[0], (long)a[1], (unsigned long)a[2], (long)a[3]);
        break;
    case O::UnTechNode: return P64(UN->GetTechTreeNode());
    case O::UnCastComplex: return P64(EEUnitCast<EEComplexUnit>(UN));
    case O::CxLocation: TR(0) = CX->mPhysicalModel.GetLocation();
        break;
    case O::CxSetTransformation: CX->SetTransformation(TR(0), a[1] != 0);
        break;
    case O::CxClearBuildTargets: CX->ClearBuildTargets();
        break;
    case O::CxNumGoalQueues: return CX->GetNumGoalQueues();
    case O::CxClearGoalQueue: CX->ClearGoalQueue((unsigned long)a[0]);
        break;
    case O::CxClearGoals: CX->ClearGoals((long)a[0]);
        break;
    case O::CxSetDisabled: CX->SetDisabled(a[0] != 0, (long)a[1]);
        break;
    case O::CxStartAction: CX->StartAction((long)a[0]);
        break;
    case O::CxHalt: CX->Halt();
        break;
    case O::CxSetBuilt: CX->SetBuilt((long)a[0], (long)a[1], (long)a[2]);
        break;
    case O::CxSetIsTinted: CX->SetIsTinted(a[0] != 0);
        break;
    case O::CxTakeOffMap: CX->TakeOffMap();
        break;
    case O::CxSetHitPoints: CX->SetHitPoints((long)a[0]);
        break;
    case O::CxSetOpenFraction: CX->SetOpenFraction(f[0]);
        break;
    case O::CxGetEffect: return CX->GetEffect((long)a[0], *(EEGraphicEffect**)a[1]);
    case O::CxIsCarried: return CX->IsCarried();
    case O::CxOutermostCarrier: return P64(CX->GetOutermostCarrier());
    case O::CxWaypointCount: return (int64_t)CX->mWaypoints.size();
    case O::CxNextWaypoint: return CX->NextWaypoint();
    case O::CxGetPathStart: return CX->GetPathStart(PT(0), *(float*)a[1]);
    case O::CxDropPath: CX->DropPath();
        break;
    case O::CxSet2DPosition: CX->Set2DPosition(*(const U2DPoint<float>*)a[0]);
        break;
    case O::CxU0DC: return P64(&CX->mU0DC);
    case O::CxActiveGroup: return P64(CX->mActiveGroup);
    case O::GroupTell: ((EEActiveGroup*)p)->TellGroup((EEComplexUnit*)a[0], PT(1));
        break;
    case O::CxSetLauncherClass: CX->SetLauncherClass((EEUnitClass*)a[0]);
        break;
    case O::CxSetOwner: CX->SetOwner((EEComplexUnit*)a[0]);
        break;
    case O::CxSetGoal: CX->SetGoal((EEUnitGoal*)a[0], a[1] != 0, (long)a[2]);
        break;
    case O::CxSetVelocity: CX->SetVelocity(f[0]);
        break;
    case O::CxSetProjectileStart: CX->SetProjectileStart(PT(0));
        break;
    case O::CxSetLocation: CX->SetLocation(PT(0), a[1] != 0);
        break;
    case O::CxSetHeight: CX->SetHeight(f[0]);
        break;
    case O::CxSetFlightTime: CX->SetFlightTime((long)a[0]);
        break;
    case O::CxSetOrientation: CX->mOrientation = GEOrientation(f[0], f[1], f[2]);
        break;
    case O::CxPlayEffect: CX->PlayEffect((long)a[0]);
        break;
    case O::PlayUnitEffect: EEPlayUnitEffect(CX, (long)a[0]);
        break;
    case O::CxSightOffHere: CX->SightOffHere();
        break;
    case O::CxSightOnHere: CX->SightOnHere();
        break;
    case O::CxSetSight: CX->SetSight(f[0]);
        break;
    case O::CxIsProtected: return CX->IsProtected();
    case O::CxSetIsDeletable: CX->SetIsDeletable(a[0] != 0);
        break;
    case O::CxAreaEffectsAdd: CX->mAreaEffects.Add((EEAreaEffect*)a[0]);
        break;
    case O::CxAreaEffectsRemove: CX->mAreaEffects.Remove(a[0]);
        break;
    case O::CxGoalState: return P64(CX->mGoalState);
    case O::GoalKind: return ((EEUnitGoal*)p)->mKind;
    case O::UpdateUnitTile: EEUpdateUnitTile(&TR(0), CX, (long*)a[1], (long*)a[2]);
        break;
    case O::CombatFindDrop: return P64(EECombatFindDropLocation(CX, (EEUnit*)a[0], PT(1)));
    case O::NewAttackGround: return P64(EEUGRust::NewAttackGround(PT(0), a[1] != 0));
    case O::NewAttackUnit: return P64(EEUGRust::NewAttackUnit(CX, (EEUnit*)a[0], a[1] != 0, a[2] != 0));
    case O::NewCastCalamity: return P64(EEUGRust::NewCastCalamity(CX, (EEUnit*)a[0], (long)a[1], (long)a[2], (long)a[3], a[4] != 0));
    case O::NewLocation: return P64(EEUGRust::NewLocation(&PT(0), a[1] != 0, a[2] != 0, a[3] != 0, f[0], a[4] != 0, (unsigned char)a[5]));
    // the unit classes
    case O::UcFields: return P64(static_cast<EeUnitsUnitClassFields*>(UC));
    case O::UcAt: return P64(gUnitClassTable[a[0]]);
    case O::UcActionChanged: UC->ActionChanged((EEComplexUnit*)a[0]);
        break;
    case O::UcIsLandBuilding: return UC->IsLandBuilding();
    case O::UcGroundLocation: UC->GroundLocation(PT(0));
        break;
    case O::UcCreateUnit: return P64(UC->CreateUnit((long)a[0], (long)a[1], (long)a[2], a[3] != 0, (EEUnit*)a[4]));
    case O::UcPlayCreationEffect: UC->PlayCreationEffect((EEComplexUnit*)a[0]);
        break;
    case O::UcLastLinkedCarrier: return P64(UC->GetLastLinkedCarrier((EEComplexUnit*)a[0]));
    case O::UcLastLinked: return P64(UC->GetLastLinked((EEComplexUnit*)a[0]));
    case O::UcMoraleEffect: return P64(UC->mMoraleEffect);
    // the players
    case O::PlRts: return P64(static_cast<EeUnitsRtsPlayerFields*>(PL));
    case O::PlSaved: return P64(static_cast<EeUnitsPlayerSaved*>(PL));
    case O::PlFields: return P64(static_cast<EeUnitsPlayerFields*>(PL));
    case O::PlCanAfford: return PL->CanAfford((EETechnology*)a[0], (long)a[1], (long)a[2]);
    case O::PlPay: PL->Pay((EETechnology*)a[0], (long)a[1], f[0]);
        break;
    case O::PlAddWonder: PL->AddWonder((EEComplexUnit*)a[0], a[1] != 0);
        break;
    case O::PlRemoveWonder: PL->RemoveWonder((EEComplexUnit*)a[0]);
        break;
    case O::PlDeselect: PL->DeselectUnit((EEUnit*)a[0]);
        break;
    case O::PlOffMapInfo: return P64(PL->GetOffMapInfo((EEComplexUnit*)a[0]));
    case O::OffMapSetReason: ((EEWUnitsOffMapInfo*)p)->mReason = (long)a[0];
        break;
    // the world, the map, the physics
    case O::WorldAddr: return P64(&::gWorld);
    case O::WorldPlayer: return P64(::gWorld.mPlayers[a[0]]);
    case O::WorldCurrentPlayer: return P64(::gWorld.GetCurrentPlayer());
    case O::MapTile: return P64(::gWorld.mMap->GetTile((long)a[0], (long)a[1]));
    case O::TileUnits: return P64(((EEMapTile*)p)->mUnits);
    case O::MeshValidTile: return ::gWorld.mMap->mTerrainMesh.GetIsValidTile((long)a[0], (long)a[1]);
    case O::ContagionHelper: return P64(::gWorld.GetContagionManager()->GetContagionHelper((long)a[0]));
    case O::ContagionCreate: return ::gWorld.GetContagionManager()->CreateContagionHelper((EEContagionFunctor*)p, (EECalamity*)a[0], (EEUnit*)a[1]);
    case O::ContagionDestroyVerify: ESVERIFY_AT(::gWorld.GetContagionManager()->DestroyContagionHelper((long)a[0]), a[1]);
        break;
    case O::AeHas: return gAEManager.HasAreaEffect((long)a[0], (long)a[1], (long)a[2], (long)a[3]);
    case O::AeCreate: return P64(gAEManager.CreateAreaEffect((unsigned long)a[0], (EEComplexUnit*)a[1]));
    case O::RefDereference: ((UReferenceCounter*)(EEAreaEffect*)p)->Dereference();
        break;
    case O::UnitSetContains: return gUnitSetManager.Contains((long)a[0], (unsigned long)a[1]);
    case O::ContinentOfUnit: return gContinentManager.GetContinent(UN, kAIPathNonSpace);
    case O::ViewportInverseApply: gViewport.GetCameraInverse().Apply(PT(0), PT(1));
        break;
    case O::Verify: ESVERIFY_AT(a[0] != 0, a[1]);
        break;
    // the effects and the sounds
    case O::EffUnit: return gWorld.mEffectManager->CreateUnitEffect(UN, (EEGraphicEffect*)a[0], HANDLE(1), a[2] != 0, a[3], a[4]);
    case O::EffBetweenUnits: return gWorld.mEffectManager->CreateEffectBetweenUnits(UN, (EEUnit*)a[0], (EEGraphicEffect*)a[1], HANDLE(2));
    case O::EffAt: return gWorld.mEffectManager->CreateEffect(UN, PT(0), (EEGraphicEffect*)a[1], HANDLE(2), a[3]);
    case O::EffPlaced: return gWorld.mEffectManager->CreatePlacedEffect(PT(0), (EEGraphicEffect*)a[1], HANDLE(2));
    case O::EffBetween: return gWorld.mEffectManager->CreateEffectBetween(PT(0), PT(1), (EEGraphicEffect*)a[2], HANDLE(3));
    case O::EffDestroy: return gWorld.mEffectManager->DestroyEffect(EEEffectHandle{(uint32_t)a[0]});
    case O::EffectKind: return ((EEGraphicEffect*)p)->mKind;
    case O::SoPlayNode: return P64(gSoundManager->PlayAndReturnReferencedNode(((EESoundObject*)p)->GetSampleID(), f[0], f[1], f[2], ((EESoundObject*)p)->GetLooping(), NULL));
    case O::SoPlay: return gSoundManager->Play(((EESoundObject*)p)->GetSampleID(), f[0], f[1], f[2], ((EESoundObject*)p)->GetLooping(), NULL);
    case O::NodeDereference: ((SESMInstanceNode*)p)->Dereference();
        break;
    case O::SoCalamityParams: EEComputeSoundParameters(f[0], PT(0), *(float*)a[1], *(float*)a[2], *(float*)a[3], kCalamitySoundFalloff);
        break;
    case O::ModelPart0: return P64(((EEModel*)p)->mModels[0]);
    // the calamities
    case O::CalFields: return P64(static_cast<EeCalamityFields*>(CL));
    case O::CalRect: return P64(&CL->mRectangle);
    case O::CalTargetLoc: return P64(&CL->mTargetLocation);
    case O::CalShakeLoc: return P64(&CL->mShakeLocation);
    case O::CalTile: return P64(&CL->mTilePlaceHolder1);
    case O::CalTarget: return P64(CL->mTargetUnit);
    case O::CalCaster: return P64(CL->mCasterUnit);
    case O::CalClass: return P64(CL->mClass);
    case O::CalEffectHandle: return P64(&CL->mEffectHandle.mValue);
    case O::CalSound: return P64(CL->mSound);
    case O::CalSecondary: return P64(CL->mSecondarySound);
    case O::KlFields: return P64(static_cast<EeCalamityClassFields*>(KL));
    case O::KlEffect1: return P64(KL->mGraphicEffect1);
    case O::KlEffect2: return P64(KL->mGraphicEffect2);
    case O::KlEffect3: return P64(KL->mGraphicEffect3);
    case O::KlEffect4: return P64(KL->mGraphicEffect4);
    case O::KlSound: return P64(KL->mSound);
    case O::KlSecondary: return P64(KL->mSecondarySound);
    case O::KlFunctor: return P64(KL->mFunctor);
    case O::KlContagionFunctor: return P64(KL->GetContagionFunctor());
    case O::KlUnitSet: return KL->GetUnitSet();
    case O::KlCost: return KL->GetCost();
    case O::KlUpdateSound: KL->UpdateSound((EECalamity*)a[0], (EESoundObject*)a[1], f[0], f[1], a[2] != 0);
        break;
    case O::KlPlaySound: KL->PlaySound((EECalamity*)a[0]);
        break;
    case O::HostClass: return P64(HO->mClass);
    case O::HostCalamity: return P64(HO->mCalamity);
    case O::HostSound: return P64(HO->mSounds[a[0]]);
    case O::HostUnits: return P64(&HO->mUnits);
    case O::HostUnitAt: return P64(HO->mUnits[a[0]]);
    case O::HostUnitCount: return (int64_t)HO->mUnits.size();
    case O::HostPending: return P64(HO->mPending);
    case O::HostSetPending: HO->mPending = (void*)a[0];
        break;
    case O::HostTiles: return P64(&HO->mTiles);
    case O::HostScratch: return P64(&HO->mScratch);
    case O::VecTilesClear: ((std::vector<EETileCoordinate>*)p)->clear();
        break;
    case O::VecUnitsClear: ((std::vector<EEUnit*>*)p)->clear();
        break;
    case O::SortTilesNear: std::sort(((std::vector<EETileCoordinate>*)p)->begin(), ((std::vector<EETileCoordinate>*)p)->end(), EETileDistanceLess((long)a[0], (long)a[1]));
        break;
    case O::CxIsColor: return CX->mColor == GEColor(f[0], f[1], f[2]);
    case O::CxSetColor: CX->SetColor(GEColor(f[0], f[1], f[2]));
        break;
    // the calamity objects (calamity_objects.rs)
    case O::CalNew: return P64(new EECalamity(KL, (EEUnit*)a[0]));
    case O::CalDelete: delete CL;
        break;
    case O::CalSetTargetRaw: CL->mTargetUnit = (EEUnit*)a[0];
        break;
    case O::CalSetCasterRaw: CL->mCasterUnit = (EEUnit*)a[0];
        break;
    case O::CalSetSoundRaw: CL->mSound = (SESMInstanceNode*)a[0];
        break;
    case O::CalSetSecondaryRaw: CL->mSecondarySound = (SESMInstanceNode*)a[0];
        break;
    case O::CalSetClassRaw: CL->mClass = (EECalamityClass*)a[0];
        break;
    case O::CalEffectGenerationZero: *(CL->mEffectGeneration.begin() + a[0]) = 0;
        break;
    case O::KlNew: return P64(new EECalamityClass);
    case O::KlInitialize: KL->Initialize((EECalamityData*)a[0]);
        break;
    case O::KlLoadMedia: KL->LoadMedia((EEPlayer*)a[0], a[1] != 0);
        break;
    case O::KlUnloadMedia: KL->UnloadMedia((EEPlayer*)a[0]);
        break;
    case O::KlSetEffect1: KL->mGraphicEffect1 = (EEGraphicEffect*)a[0];
        break;
    case O::KlSetEffect2: KL->mGraphicEffect2 = (EEGraphicEffect*)a[0];
        break;
    case O::KlSetEffect3: KL->mGraphicEffect3 = (EEGraphicEffect*)a[0];
        break;
    case O::KlSetEffect4: KL->mGraphicEffect4 = (EEGraphicEffect*)a[0];
        break;
    case O::KlSetSound: KL->mSound = (EESoundObject*)a[0];
        break;
    case O::KlSetSecondary: KL->mSecondarySound = (EESoundObject*)a[0];
        break;
    case O::KlSetFunctor: KL->mFunctor = (EEContagionFunctor*)a[0];
        break;
    case O::KlDeleteFunctor: delete KL->mFunctor;
        break;
    case O::MgrClassCount: return (int64_t)MG->mCalamityClasses.size();
    case O::MgrClassAt: return P64(MG->mCalamityClasses[a[0]]);
    case O::MgrClassSet: MG->mCalamityClasses[a[0]] = (EECalamityClass*)a[1];
        break;
    case O::MgrClassesResize: MG->mCalamityClasses.resize((size_t)a[0], 0);
        break;
    case O::MgrCalamityCount: return (int64_t)MG->mCalamities.size();
    case O::MgrCalamityAt: return P64(MG->mCalamities[a[0]]);
    case O::MgrCalamityPush: MG->mCalamities.push_back((EECalamity*)a[0]);
        break;
    case O::MgrCalamityErase: MG->mCalamities.erase(MG->mCalamities.begin() + a[0]);
        break;
    case O::MgrDeleteCalamities: return DeleteVectorContents(MG->mCalamities);
    case O::MgrDeleteClasses: return DeleteVectorContents(MG->mCalamityClasses);
    case O::VecCalamityCount: return (int64_t)((std::vector<EECalamity*>*)p)->size();
    case O::VecCalamityAt: return P64((*(std::vector<EECalamity*>*)p)[a[0]]);
    case O::VecUnitsNew: return P64(new std::vector<EEUnit*>);
    case O::VecUnitsDelete: delete (std::vector<EEUnit*>*)p;
        break;
    case O::VecUnitsCount: return (int64_t)((std::vector<EEUnit*>*)p)->size();
    case O::VecUnitsAt: return P64((*(std::vector<EEUnit*>*)p)[a[0]]);
    case O::FnNew: return P64(new EEContagionFunctor(KL, (uint32_t)a[0]));
    case O::FnFields: return P64(static_cast<EeContagionFunctorFields*>((EEContagionFunctor*)p));
    case O::FnEffect1: return P64(((EEContagionFunctor*)p)->mEffect);
    case O::FnEffect2: return P64(((EEContagionFunctor*)p)->mEffect2);
    case O::FnEffect3: return P64(((EEContagionFunctor*)p)->mEffect3);
    case O::FnSetEffect1: ((EEContagionFunctor*)p)->mEffect = (EEGraphicEffect*)a[0];
        break;
    case O::FnSetEffect2: ((EEContagionFunctor*)p)->mEffect2 = (EEGraphicEffect*)a[0];
        break;
    case O::FnSetEffect3: ((EEContagionFunctor*)p)->mEffect3 = (EEGraphicEffect*)a[0];
        break;
    case O::FnCanInfect: return ((EEContagionFunctor*)p)->CanInfect((EEUnit*)a[0]);
    case O::CxFireEffect: return CX->mFireEffect.mValue;
    case O::CxSetFireEffect: CX->mFireEffect.mValue = (uint32_t)a[0];
        break;
    case O::UcLeprosyEffect: return P64(UC->mLeprosyEffect);
    case O::UcPlagueEffect: return P64(UC->mPlagueEffect);
    case O::UcPulseCannonEffect: return P64(UC->mPulseCannonEffect);
    case O::UcBattleCryEffect: return P64(UC->mBattleCryEffect);
    case O::UcCreateUnitAt: return P64(UC->CreateUnitAt(TR(0), (long)a[1], 0, NULL));
    case O::UnMaxHitPointsI32: return (int32_t)UN->mStats->mMaxHitPoints;
    case O::EffGet: return P64(gWorld.mEffectManager->GetEffect(EEEffectHandle{(uint32_t)a[0]}));
    case O::EffKindOf: return ((EEEffect*)p)->GetKind();
    case O::EffFinishDamaged: ((EEEffect*)p)->mRecord->damaged.finished = true;   // the damage controller (0x3e, on EEE2DRust)
        break;
    case O::GfxEffectGet: return P64(EEGraphicEffect::Get((long)a[0]));
    case O::GfxEffectReference: ((EEGraphicEffect*)p)->Reference();
        break;
    case O::GfxEffectDereference: ((EEGraphicEffect*)p)->Dereference();
        break;
    case O::GfxEffectLoadPtr: ((EEGraphicEffect*)p)->Load(a[0] != 0);
        break;
    case O::GfxEffectUnloadPtr: ((EEGraphicEffect*)p)->Unload();
        break;
    case O::GfxEffectDuration: return (uint32_t)((EEGraphicEffect*)p)->mDuration;
    case O::GfxEffectTime1: return (uint32_t)((EEGraphicEffect*)p)->mTime1;
    case O::SoundObjectGet: return P64(EESoundObject::Get((long)a[0]));
    case O::SoundObjectLoadPtr: ((EESoundObject*)p)->LoadMedia();
        break;
    case O::SoundObjectUnloadPtr: ((EESoundObject*)p)->UnloadMedia();
        break;
    case O::SoundUnloadSample: gSoundManager->Unload(((EESoundObject*)p)->GetSampleID());
        break;
    case O::SoundStop: gSoundManager->Stop((SESMInstanceNode*)p, a[0] != 0);
        break;
    case O::NodeReference: ((SESMInstanceNode*)p)->Reference();
        break;
    case O::SoParams: EEComputeSoundParameters(f[0], PT(0), *(float*)a[1], *(float*)a[2], *(float*)a[3], f[1]);
        break;
    case O::SoSetParams: gSoundManager->SetSoundParameters((SESMInstanceNode*)p, f[0], f[1], f[2]);
        break;
    case O::SoPlayOnce: return gSoundManager->Play(((EESoundObject*)p)->GetSampleID(), f[0], f[1], f[2], false, NULL);
    case O::PlLoadClassMedia: PL->LoadUnitClassMedia((long)a[0]);
        break;
    case O::PlSetClassMedia: PL->SetUnitClassMedia((long)a[0], (long)a[1]);
        break;
    case O::PickUnitAtMouse: return P64(gUnitPicker.PickUnit(gMouse->mPosition, 0));
    case O::WorldUnit: return P64(::gWorld.GetUnit((unsigned long)a[0]));
    case O::WorldTileBlocked: return ::gWorld.IsTileBlocked((unsigned long)a[0], (unsigned long)a[1]);
    case O::TileVisibleTo: return ((EEMapTile*)p)->IsVisibleTo((EEPlayer*)a[0]);
    case O::TileTerrainType: return ((EEMapTile*)p)->mTerrainType;
    case O::TileFree: return EEIsTileFree(NULL, (unsigned long)a[0], (unsigned long)a[1]);
    case O::AeHas2: return gAEManager.HasAreaEffect2((long)a[0], (long)a[1], (long)a[2], (long)a[3]);
    case O::MapAreaOnMap: return ::gWorld.mMap->IsAreaOnMap((unsigned long)a[0], (unsigned long)a[1], (unsigned long)a[2]);
    case O::MapIsEdgeTile: return ::gWorld.GetMap()->IsEdgeTile((long)a[0], (long)a[1]);
    case O::MeshIsSpaceMap: return ::gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case O::ContinentOfTile: return gContinentManager.GetContinent(EETileCoordinate((long)a[0], (long)a[1]), kAIPathNonSpace);
    case O::FileOpenRead: ((FSFileSpec*)p)->Open(kFileAccessRead);
        break;
    case O::FileClose: ((FSFileSpec*)p)->Close();
        break;
    case O::FileReadULong: return ((FSFileSpec*)p)->Read(*(unsigned long*)a[0]);
    case O::FileReadBytes: return ((FSFileSpec*)p)->Read((unsigned char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::FilePuts: fputs((const char*)a[0], (FILE*)p);
        break;
    // the calamity capture's (calamity_capture.rs, TEST ONLY)
    case O::SoundObjectIndex: return std::find(gSoundObjects.begin(), gSoundObjects.end(), (EESoundObject*)p) - gSoundObjects.begin();
    case O::CalamityManager: return P64(EECalamityManager::GetInstance());
    case O::MemFilePosition: return ((FSMemoryFile*)p)->GetPosition(*(long*)a[0]);
    case O::MemFileData: return P64(((FSMemoryFile*)p)->GetData());
    case O::MemFileNewDefault: return P64(new FSMemoryFile);
    case O::SaveMinorVersion: return kSaveGameMinorVersion;
    case O::CalSave: return CL->Save(0, (unsigned long)a[0], *(FSMemoryFile*)a[1]);
    case O::CalLoad: return CL->Load(0, (unsigned long)a[0], *(FSMemoryFile*)a[1]);
    case O::CalNewEmpty: return P64(new EECalamity);
    case O::CalEffectGenerationCount: return (int64_t)CL->mEffectGeneration.size();
    case O::CalEffectGenerationAt: return CL->mEffectGeneration[a[0]];
    case O::ContagionRust: return P64(::gWorld.GetContagionManager()->mRust);
    case O::KlUpdate: return KL->Update((EECalamity*)a[0], f[0]);
    case O::KlStart: return KL->Start((EECalamity*)a[0]);
    case O::KlStop: KL->Stop((EECalamity*)a[0]);
        break;
    case O::KlCanCastAt: return KL->CanCastAt((EEComplexUnit*)a[0], *(GE3DPoint*)a[1], (unsigned long)a[2], (unsigned long)a[3], a[4] != 0, (EEUnit*)a[5]);
    case O::EffectManagerPresent: return gWorld.mEffectManager != NULL;
    case O::EffCount: return (int64_t)gWorld.mEffectManager->Count();
    case O::EffDestroyedCount: return (int64_t)gWorld.mEffectManager->DestroyedCount();
    case O::HelperUpdate: EEContagionHelperUpdate((EEContagionHelper*)p, f[0]);
        break;
    default: return EEWorldOp(inOp, p, a, f);   // the world bodies' (EEWorld.cpp)
    }
    return 0;
}

extern "C" double ee_gamecpp_f(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::CxEffectHeight: return CX->GetEffectHeight(a[0] != 0);
    case O::CxMaxVelocity: return CX->GetMaxVelocity();
    case O::UcHeightAt: return UC->GetHeightAt((EEComplexUnit*)a[0], PT(1));
    case O::TileHeight: return ((EEMapTile*)p)->mHeight;
    case O::MeshZ: return ::gWorld.mMap->mTerrainMesh.GetMeshZ(f[0], f[1]);
    case O::MeshGetZ: return ::gWorld.mMap->mTerrainMesh.GetZ((long)a[0], (long)a[1]);
    case O::PhysGetRandom: return gPhysics->GetRandom();
    case O::PhysGenerate: return gPhysics->mRandom.Generate();
    case O::PhysRandomRange: return gPhysics->GetRandom(f[0], f[1]);
    case O::AeRadius: return ((EEAreaEffect*)p)->GetRadius();
    case O::SoVolume: return ((EESoundObject*)p)->GetVolume();
    case O::GeModelRadius: return ((GEModel*)p)->GetBoundingRadius((unsigned long)a[0]);
    case O::GfxEffectSize: return ((EEGraphicEffect*)p)->GetSize();
    case O::UcInnerSphere: return UC->GetInnerCollisionSphere();
    case O::UnStatsAttack: return UN->mStats->GetAttack();
    case O::VariantQuakeFactor: return EEGameVariant::GetGameVariant((long)a[0])->GetEarthquakeDamageFactor();
    default: return EEWorldOpF(inOp, p, a, f);
    }
    return 0.0;
}
