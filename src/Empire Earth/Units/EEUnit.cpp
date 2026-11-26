// C:\EEX\Empire Earth\Units\EEUnit.cpp (file name inferred: no __FILE__; the keys "EEUnit-HitPoints"/"EEUnit-Data").
// The unit base: 0x228c4c..0x22a509. Unit flags: the exe's module set. Data: the keys 0x963638/0x963620 (one guard
// byte 0x963630), the vtable 0x447a20.

#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EECombatFormulas.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDoppelgangerManager.h"
#include "lle/SESoundManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EESoundObject.h"
#include "ee/EEInfluenceTable.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPNextUnits.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIForest.h"

// Creates a simple unit of inClass for inPlayer at tile (inX, inY)
EEUnit* EEUnit::Create(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer)
{
    EEUnit* theUnit = new EEUnit;
    theUnit->Init(inClass, inX, inY, inPlayer);
    return theUnit;
}

// Returns the centre of the unit's tile at ground height
void EEUnit::GetPosition(GE3DPoint& outPosition) { outPosition = GE3DPoint(this->mTileX + 0.5f, this->mTileY + 0.5f, this->mTile->mHeight); }

// Turns the loaded health fraction into hit points (at least 1 if the fraction was not 0; port, R5b.4 round 27:
// ee_units unit_load.rs finish_hit_points over the record); returns 0
long EEUnit::FinishLoad(long, long)
{
    if (this->mUnitClass != NULL)
        ee_units_unit_finish_hit_points(this, this->mStats->mMaxHitPoints);
    return 0;
}

// TakeHit's locals (the modifiers' input, the area damage, the damage) and the hit formula's answer
static long sHitInput;
static long sHitAreaDamage;
static long sHitDamage;
// Kill's death sound parameters (its locals)
static float sDeathSound[3];   // volume, pan, pitch
// Describe's and DescribeLong's text (their theString)
static UWideString sEuText;

template <class T> static T* EEEuObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// The C++ of EEUnit::TakeHit's Rust body (ee_units eu_hit.rs; EE_UNITS_EU_*): one statement of the original each, in
// its order; inA an object, inB and inC values
extern "C" int64_t ee_units_eu_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB, int64_t inC)
{
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    EEUnit* theOther = inA != 0 ? EEEuObject<EEUnit>(inA) : theUnit;
    EEComplexUnit* theComplex = theUnit->mComplexUnit;
    EEPlayer* thePlayer = dynamic_cast<EEPlayer*>(theUnit->mPlayer);
    const int64_t* theArgs = EEEuObject<const int64_t>((uint64_t)inB);
    switch (inOp) {
    case EE_UNITS_EU_PART_TAKE_HIT: EEEuObject<EEComplexUnit>(inA)->TakeHit(EEEuObject<EEUnit>((uint64_t)theArgs[0]), (long)theArgs[1], (unsigned long)theArgs[2], (long)theArgs[3]); return 0;
    case EE_UNITS_EU_SPARKS: gWorld.mEffectManager->CreateUnitEffect(theUnit, theUnit->mUnitClass->mImpactSparksEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_BLOOD: gWorld.mEffectManager->CreateUnitEffect(theUnit, theUnit->mUnitClass->mImpactBloodEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_SET_INPUT: sHitInput = (long)inB; return 0;
    case EE_UNITS_EU_SET_AREA_DAMAGE: sHitAreaDamage = (long)inB; return 0;
    case EE_UNITS_EU_APPLY_MODIFIERS: theComplex->mAffectingAreaEffects.ApplyModifiers(1, sHitInput, sHitAreaDamage, theComplex); return 0;
    case EE_UNITS_EU_SET_DAMAGE: sHitDamage = (long)inB; return 0;
    case EE_UNITS_EU_APPLY_MORALE: theComplex->ApplyMorale(sHitAreaDamage, sHitDamage); return 0;
    case EE_UNITS_EU_DAMAGE: return sHitDamage;
    case EE_UNITS_EU_HAS_UNDER_ATTACK: return theComplex->mUnitClass->FindAction(kActionUnderAttack) != NULL;
    case EE_UNITS_EU_ACTION_CHANGED: theComplex->mUnitClass->ActionChanged(theComplex); return 0;
    case EE_UNITS_EU_SHOOTS_TRACERS: return theOther->mUnitClass->mShotEffect != NULL && theOther->mUnitClass->mShotEffect->mKind == kEffect3DTracers;
    case EE_UNITS_EU_BADLY_HURT: theUnit->mUnitClass->BadlyHurt(theUnit); return 0;
    case EE_UNITS_EU_DAMAGE_TAKEN: thePlayer->DamageTaken((unsigned long)inB, (long)inC, theUnit); return 0;
    case EE_UNITS_EU_HAS_UNITS_STATE: return gWorld.mPlayers[inB] != NULL && gWorld.mPlayers[inB]->mComputerPlayer != NULL && gWorld.mPlayers[inB]->mComputerPlayer->mUnitsState != NULL;
    case EE_UNITS_EU_ON_DEALT: EECPUnitsOnDealt(gWorld.mPlayers[inB]->mComputerPlayer, (long)inC, theUnit); return 0;
    case EE_UNITS_EU_ADD_ATTACK_EVENT: thePlayer->mEvents.AddPlayerEvent(EETileLocation(theUnit->mTileX, theUnit->mTileY), EETileLocation((long)inB, (long)inC), 1, thePlayer, -1, false); return 0;
    case EE_UNITS_EU_DAMAGE_EFFECT_FORMULA: return EECombatDamageEffect(EeCombatDamageEffectIn{(int32_t)theUnit->mHitPoints, (int32_t)inB, (int32_t)theUnit->mStats->mMaxHitPoints, EEEuObject<EEGraphicEffect>(inA)->GetParam2()});
    case EE_UNITS_EU_EFFECT_ON_SELF: gWorld.mEffectManager->CreateUnitEffect(theUnit, EEEuObject<EEGraphicEffect>(inA), NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_GATE_EFFECT_LINKED: gWorld.mEffectManager->CreateUnitEffect(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->mUnitClass)->GetLastLinked(theUnit->mComplexUnit), theUnit->mUnitClass->mDamageEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_GATE_EFFECT_CARRIER: gWorld.mEffectManager->CreateUnitEffect(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->mUnitClass)->GetLastLinkedCarrier(theUnit->mComplexUnit), theUnit->mUnitClass->mDamageEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_STATS_RESET: ::gWorld.mPlayers[inB]->mSavedGroups.Reset((long)inC, 1); return 0;
    case EE_UNITS_EU_KILL: theUnit->Kill(true, false, false); return 0;
    case EE_UNITS_EU_ICBM_EXPLOSION: gWorld.mEffectManager->CreateUnitEffect(theUnit, EEGraphicEffect::Get(kGraphicEffectAircraftExplosion), NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_MINIMAP_PING: gMiniMap->Ping(GE2DPoint((float)theUnit->mTileX, (float)theUnit->mTileY)); return 0;
    case EE_UNITS_EU_SELECTED: return theUnit->mPlayer->mSelectedGroup->Contains(theComplex);
    case EE_UNITS_EU_POST_424: gUserInterface->PostMessage424Once(); return 0;
    // eu_body.rs's
    case EE_UNITS_EU_ADD_OFF_MAP: theUnit->mPlayer->AddOffMapUnit(theUnit); return 0;
    case EE_UNITS_EU_REMOVE_OFF_MAP: theUnit->mPlayer->RemoveOffMapUnit(theUnit); return 0;
    case EE_UNITS_EU_IS_FOUNDATION: return theUnit->IsFoundation();
    case EE_UNITS_EU_MINIMAP_MOVE: gMiniMap->MoveUnit(theUnit, theUnit->mTileX, theUnit->mTileY, (long)inB, (long)inC); return 0;
    case EE_UNITS_EU_SIGHT_MOVED: theUnit->SightMoved(theUnit->mTileX, theUnit->mTileY, (long)inB, (long)inC); return 0;
    case EE_UNITS_EU_REMOVE_FROM_REGION: theUnit->mUnitClass->RemoveFromRegion(theUnit); return 0;
    case EE_UNITS_EU_INFLUENCE_REMOVE: gWorld.mInfluenceTable->RemoveUnit(theUnit, theUnit->mTileX, theUnit->mTileY); return 0;
    case EE_UNITS_EU_REMOVE_FROM_TILE: theUnit->mUnitClass->RemoveFromTile(theUnit); return 0;
    case EE_UNITS_EU_UPDATE_HIDDEN: theUnit->UpdateHidden(); return 0;
    case EE_UNITS_EU_ADD_TO_TILE: theUnit->mUnitClass->AddToTile(theUnit); return 0;
    case EE_UNITS_EU_ADD_TO_REGION: theUnit->mUnitClass->AddToRegion(theUnit); return 0;
    case EE_UNITS_EU_INFLUENCE_ADD: gWorld.mInfluenceTable->AddUnit(theUnit, (long)inB, (long)inC); return 0;
    case EE_UNITS_EU_ACTIVATE_COMPLEX: gWorld.ActivateUnit(theComplex); return 0;
    case EE_UNITS_EU_RESORT: gWorld.ResortUnit(theUnit); return 0;
    case EE_UNITS_EU_SIGHT_OFF_HERE: theUnit->SightOff(theUnit->mTileX, theUnit->mTileY); return 0;
    case EE_UNITS_EU_HAS_DOPPELGANGER: return gDoppelgangerManager.GetDoppelganger(EEEuObject<EETile>(inA), theUnit, theUnit->mPlayer->mPlayerID) != NULL;
    case EE_UNITS_EU_DOPPELGANGER_TILE_SHOWN: gDoppelgangerManager.TileShown(EEEuObject<EETile>(inA), theUnit->mTileX, theUnit->mTileY, theUnit->mPlayer); return 0;
    case EE_UNITS_EU_DEATH_EFFECT: gWorld.mEffectManager->CreateUnitEffect(theUnit, theUnit->mUnitClass->mDeathEffect, NULL, 0, 0, 0); return 0;
    case EE_UNITS_EU_DEATH_SOUND_PARAMETERS: theUnit->mUnitClass->GetSoundParameters(theUnit, theUnit->mStats->mDeathSound, &sDeathSound[0], &sDeathSound[1], &sDeathSound[2]); return 0;
    case EE_UNITS_EU_DEATH_SOUND_PLAY:
        gSoundManager->Play(theUnit->mStats->mDeathSound->mSampleID, sDeathSound[0], sDeathSound[1], sDeathSound[2], theUnit->mStats->mDeathSound->mLooping, NULL);
        return 0;
    case EE_UNITS_EU_SIGHT_ON_HERE: theUnit->SightOn(theUnit->mTileX, theUnit->mTileY); return 0;
    case EE_UNITS_EU_CAMERA_UNIT_IS_THIS: return EEUnitHandleResolved<EEUnit>(ee_app_camera_unit((uint32_t)inB)) == theUnit;
    case EE_UNITS_EU_CLEAR_CAMERA_UNIT: gWorld.mCameraControl.ClearUnitA(); return 0;
    case EE_UNITS_EU_CLEAR_CAMERA_UNIT_B: gWorld.mCameraControl.ClearUnitB(); return 0;
    case EE_UNITS_EU_UNIT_TAKEN: theUnit->mPlayer->UnitTaken(theUnit, inB != 0); return 0;
    case EE_UNITS_EU_TILE_VISIBLE: return gWorld.mMap->GetTile((long)inB, (long)inC)->IsVisibleTo(theUnit->GetPlayer());
    case EE_UNITS_EU_CONTINENT_AT: return (uint64_t)(uintptr_t)gContinentManager.GetContinentAt(theUnit);
    case EE_UNITS_EU_CONTINENT_CATEGORY: return EEEuObject<EEAIContinent>(inA)->GetCategory();
    case EE_UNITS_EU_MAP_SIZE: return inB == 0 ? (int64_t)gMapWidth : (int64_t)gMapHeight;
    case EE_UNITS_EU_CONTINENT_ID: return gContinentManager.GetContinentID((long)inB, (long)inC);
    case EE_UNITS_EU_CONTINENT_BY_ID: return (uint64_t)(uintptr_t)gContinentManager.GetContinentByID((unsigned short)inB);
    case EE_UNITS_EU_TILE_DISTANCE_SQUARED: return UFloatBits(theUnit->GetTileDistanceSquared(theOther));
    case EE_UNITS_EU_KILL_UNIT: theUnit->mPlayer->KillUnit(theUnit, false); return 0;
    default: return 0;
    }
}

// The statements of eu_more.rs's bodies (EE_UNITS_EUM_*; no preamble: Init and ResetEEUnit run on a unit whose
// members are not set yet)
extern "C" int64_t ee_units_eum_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB, int64_t inC)
{
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    switch (inOp) {
    case EE_UNITS_EUM_UNIT_REMOVED: theUnit->mUnitClass->UnitRemoved(); return 0;
    case EE_UNITS_EUM_SHUTDOWN: return UShutdownInProgress();
    case EE_UNITS_EUM_CLEARING: return gWorld.mClearing;
    case EE_UNITS_EUM_POST_UNLOAD: gDataPreLoading->PostUnloadUnitClass(theUnit->mPlayer->mPlayerID, theUnit->mUnitClass->mUnitType); return 0;
    case EE_UNITS_EUM_UNLOAD_MEDIA: theUnit->mPlayer->UnloadUnitClassMedia(theUnit->mUnitClass->mUnitType); return 0;
    case EE_UNITS_EUM_DEACTIVATE: gWorld.DeactivateUnit(theUnit->mComplexUnit); return 0;
    case EE_UNITS_EUM_RTS_INVALIDATE: theUnit->RTSUnit::Invalidate(); return 0;
    case EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS: return EEEuObject<EEUnit>(inA)->GetPlayer()->mUnitClassUpgrades[EEEuObject<EEUnit>(inA)->GetUnitClass()->mUnitType]->mMaxHitPoints;
    case EE_UNITS_EUM_RTS_COPY_FROM: theUnit->RTSUnit::CopyFrom(EEEuObject<RTSUnit>(inA)); return 0;
    case EE_UNITS_EUM_INITIALIZE: theUnit->Initialize((long)inB); return 0;
    case EE_UNITS_EUM_REFERENCE_COUNT: return theUnit->mReferenceCount;
    case EE_UNITS_EUM_REFERENCE_ONE: theUnit->mReferenceCount = 1; return 0;
    case EE_UNITS_EUM_SET_STATS: theUnit->mStats = theUnit->mPlayer->mUnitClassUpgrades[theUnit->mUnitClass->mUnitType]; return 0;
    case EE_UNITS_EUM_MOVE_TO_TILE: theUnit->MoveTo((long)inB, (long)inC, 0); return 0;
    case EE_UNITS_EUM_CARBON_ROCK: return EEUnit::IsCarbonRock(theUnit->mUnitClass->mUnitType);
    case EE_UNITS_EUM_FOREST: return EEObj(gContinentManager.GetForestAt(theUnit));
    case EE_UNITS_EUM_FOREST_DUE: return EEEuObject<EEAIForest>(inA)->GetNextRenewal() <= gPhysics->mGameTime;
    case EE_UNITS_EUM_FOREST_TILES: return EEEuObject<EEAIForest>(inA)->GetNumTiles();
    case EE_UNITS_EUM_RANDOM: return gPhysics->GetRandom((long)inB, (long)inC);
    case EE_UNITS_EUM_REGROW: EEEuObject<EEAIForest>(inA)->RegrowTrees(U2DPoint<long>(theUnit->mTileX, theUnit->mTileY), (long)inB, (long)inC); return 0;
    case EE_UNITS_EUM_SET_NEXT_RENEWAL: EEEuObject<EEAIForest>(inA)->SetNextRenewal((long)inB); return 0;
    case EE_UNITS_EUM_SET_CLASS_MEDIA: theUnit->mPlayer->SetUnitClassMedia(theUnit->mUnitClass->mUnitType, 0x7f); return 0;
    case EE_UNITS_EUM_POST_LOAD: gDataPreLoading->PostLoadUnitClass(theUnit->mPlayer->mPlayerID, theUnit->mUnitClass->mUnitType); return 0;
    case EE_UNITS_EUM_DEREFERENCE_LAST_ATTACKER: static_cast<EEComplexUnit*>(theUnit)->mLastAttacker->Dereference(); return 0;
    case EE_UNITS_EUM_REFERENCE_OF: EEEuObject<EEComplexUnit>(inA)->Reference(); return 0;
    case EE_UNITS_EUM_CX_RESET_COMPLEX: static_cast<EEComplexUnit*>(theUnit)->ResetEEComplexUnit(true, false); return 0;
    case EE_UNITS_EUM_RESET_EE_UNIT: theUnit->ResetEEUnit(); return 0;
    case EE_UNITS_EUM_RTS_RESET: theUnit->Reset(); return 0;
    case EE_UNITS_EUM_FREE_NODE: theUnit->FreeNode(); return 0;
    case EE_UNITS_EUM_ZERO_REFERENCES: theUnit->mReferenceCount = 0; return 0;
    case EE_UNITS_EUM_DELETE: delete theUnit; return 0;
    // eu_describe.rs's (sEuText: the originals' theString)
    case EE_UNITS_EUM_TEXT_NEW: sEuText = UWideString(); return 0;
    case EE_UNITS_EUM_TEXT_RTS_DESCRIBE: theUnit->RTSUnit::Describe(sEuText); return 0;
    case EE_UNITS_EUM_TEXT_APPEND: *EEEuObject<UWideString>(inA) += sEuText; return 0;
    case EE_UNITS_EUM_TEXT_RESOURCE: sprintf(sEuText, "res %ld / %ld:", theUnit->mResourceType, theUnit->mResourceAmount); return 0;
    case EE_UNITS_EUM_TEXT_OBJECT: sprintf(sEuText, "obj %ld:", theUnit->mUnitClass->mUnitType); return 0;
    case EE_UNITS_EUM_TEXT_HIT_POINTS: sprintf(sEuText, "hp %ld:", theUnit->mHitPoints); return 0;
    case EE_UNITS_EUM_TEXT_SIMPLE: sprintf(sEuText, "\n======] SIMPLE UNIT [======\n"); return 0;
    case EE_UNITS_EUM_TEXT_RTS_DESCRIBE_LONG: theUnit->RTSUnit::DescribeLong(sEuText); return 0;
    case EE_UNITS_EUM_TEXT_CLASS: sprintf(sEuText, "Object Type and Name: %ld, %s\n", theUnit->mUnitClass->mUnitType, (const char*)theUnit->mUnitClass->mName); return 0;
    case EE_UNITS_EUM_TEXT_RESOURCE_LONG: sprintf(sEuText, "Resource Type/Amount:%ld / %ld\n", theUnit->mResourceType, theUnit->mResourceAmount); return 0;
    case EE_UNITS_EUM_TEXT_HEALTH: sprintf(sEuText, "Health: %ld / %ld (%.2f%%)\n", theUnit->mHitPoints, theUnit->mStats->mMaxHitPoints, (float)theUnit->mHitPoints / (float)theUnit->mStats->mMaxHitPoints); return 0;
    default: return 0;
    }
}

// Returns whether inUnit may pass the unit: non-blocking units, dead non-resources, and wood for forest-crossing units
bool EEUnit::IsPassableBy(EEComplexUnit* inUnit)
{
    return !this->mUnitClass->mBlocksTile || (this->mHitPoints == 0 && this->mUnitClass->mCategory != kUnitCategoryResource) || (this->mUnitClass->mResourceType == kResourceWood && inUnit != NULL && (bool)((inUnit->mStats->mTerrainMask >> 14) & 1));
}
