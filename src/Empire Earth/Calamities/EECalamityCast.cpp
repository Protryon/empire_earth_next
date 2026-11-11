// New (port): the host of the calamity classes' Rust cast tests (ee/EECalamityUpdate.h; ee_tech calamity_cast.rs;
// docs/RUST_PLAN.md R5b.2): CanCastAt's caster, tile, sight and area effect questions, the target's flags, and the
// tiles, continents and units the classes' IsValidLocation read. The class's own IsValidLocation and IsValidTarget
// are virtual calls back into the class.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIKindIDs.h"

namespace {

struct CastHost {
    EECalamityClass* mClass;
    EEComplexUnit* mUnit;
    EEUnit* mTargetUnit;                    // the call's target
    EEUnit* mTarget;                        // the unit with its id (TargetLookup)
    long mUnitClassIndex;                   // the hurricane's and the volcano's unit class
    std::vector<EEUnit*> mUnits;
};

void CastCall(void* inContext, EeCalamityState*, const EeCalamityCall* inCall, EeCalamityReply* outReply)
{
    CastHost& theHost = *static_cast<CastHost*>(inContext);
    const EeCalamityCall& c = *inCall;
    EeCalamityReply& r = *outReply;
    r = EeCalamityReply();
    EEComplexUnit* theUnit = theHost.mUnit;
    unsigned long theX = (unsigned long)(long)c.index;
    unsigned long theY = (unsigned long)(long)c.index2;
    switch (c.op) {
    case EeCalamityOp::CasterPlayerIndex:
        r.value = (int32_t)::gWorld.GetPlayerIndex(theUnit->mPlayer);
        break;
    case EeCalamityOp::CasterHitPoints:
        r.value = (int32_t)theUnit->mHitPoints;
        break;
    case EeCalamityOp::CasterPower:
        r.value = (int32_t)theUnit->GetPower();
        break;
    case EeCalamityOp::TileExists:
        r.ok = ::gWorld.mMap->GetTile(theX, theY) != NULL;
        break;
    case EeCalamityOp::TileVisible:
        r.ok = ::gWorld.mMap->GetTile(theX, theY)->IsVisibleTo(theUnit->mPlayer);
        break;
    case EeCalamityOp::AreaEffect:
        r.ok = gAEManager.HasAreaEffect2(3, theX, theY, c.value);
        break;
    case EeCalamityOp::TargetLookup: {
        unsigned long theTargetID = -1;
        if (theHost.mTargetUnit != NULL)
            theTargetID = theHost.mTargetUnit->mUnitID;
        theHost.mTarget = ::gWorld.GetUnit(theTargetID);
        r.ok = theHost.mTarget != NULL;
        break;
    }
    case EeCalamityOp::ValidLocation:
        r.ok = static_cast<EELocationCalamityClass*>(theHost.mClass)->IsValidLocation(theUnit, theX, theY);
        break;
    case EeCalamityOp::ValidTarget:
        r.ok = static_cast<EEUnitCalamityClass*>(theHost.mClass)->IsValidTarget(theUnit, theHost.mTarget);
        break;
    case EeCalamityOp::TargetView: {
        EEUnit* theTarget = theHost.mTargetUnit;
        EEComplexUnit* theComplex = theTarget->mComplexUnit;
        r.flags = (theTarget->mTileNode != NULL ? EE_CALAMITY_TARGET_ON_MAP : 0) |
                  (theTarget == static_cast<EEUnit*>(theUnit) ? EE_CALAMITY_TARGET_IS_CASTER : 0) |
                  (theUnit != NULL && theTarget->mPlayer == theUnit->mPlayer ? EE_CALAMITY_TARGET_SAME_PLAYER : 0) |
                  (theUnit != NULL && theUnit->mPlayer->mEnemies[theTarget->mPlayer->mPlayerID] != 0 ? EE_CALAMITY_TARGET_ENEMY : 0) |
                  (gUnitSetManager.Contains(theTarget->mUnitClass->GetUnitType(), theHost.mClass->GetUnitSet()) ? EE_CALAMITY_TARGET_IN_SET : 0) |
                  (theTarget->IsFoundation() ? EE_CALAMITY_TARGET_FOUNDATION : 0) |
                  (theComplex != NULL && theComplex->mIsBuilt ? EE_CALAMITY_TARGET_BUILT : 0) |
                  (theComplex != NULL && theComplex->mIsDisabled ? EE_CALAMITY_TARGET_DISABLED : 0) |
                  (theTarget->mUnitClass->GetUnitType() == kUnitTypeVolcano ? EE_CALAMITY_TARGET_VOLCANO : 0);
        break;
    }
    case EeCalamityOp::AreaOnMap:
        r.ok = ::gWorld.mMap->IsAreaOnMap(theX, theY, (unsigned long)(uint32_t)c.value);
        break;
    case EeCalamityOp::ValidTile:
        r.ok = ::gWorld.mMap->mTerrainMesh.GetIsValidTile(theX, theY);
        break;
    case EeCalamityOp::SpaceMap:
        r.ok = ::gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
        break;
    case EeCalamityOp::CasterContinent:
        r.value = gContinentManager.GetContinent(theUnit, kAIPathNonSpace);
        break;
    case EeCalamityOp::TileContinent: {
        EETileCoordinate theTile;
        theTile.mX = theX;
        theTile.mY = theY;
        r.value = gContinentManager.GetContinent(theTile, kAIPathNonSpace);
        break;
    }
    case EeCalamityOp::TileBlocked:
        r.ok = ::gWorld.IsTileBlocked(theX, theY);
        break;
    case EeCalamityOp::TerrainType: {
        EEMapTile* theTile = ::gWorld.mMap->GetTile(theX, theY);
        r.ok = theTile != NULL;
        r.value = theTile != NULL ? (int32_t)theTile->mTerrainType : 0;
        break;
    }
    case EeCalamityOp::InnerSphere:
        r.x = gUnitClassTable[theHost.mUnitClassIndex]->GetInnerCollisionSphere();
        break;
    case EeCalamityOp::UnitsNear: {
        long theNear = EEGetUnitsNear(GE3DPoint(c.x, c.y, c.z), c.w, theHost.mUnits);
        r.value = (int32_t)theNear;
        r.id = (uint32_t)theHost.mUnits.size();
        break;
    }
    case EeCalamityOp::NearStorm: {
        EEUnit* theNear = theHost.mUnits[c.index];
        r.ok = theNear->mHitPoints > 0 && (theNear->mUnitClass->GetUnitType() == kUnitTypeHurricane ||
                                           theNear->mUnitClass->GetUnitType() == kUnitTypeAntiMatterStorm);
        break;
    }
    case EeCalamityOp::TileSize:
        r.value = (int32_t)gUnitClassTable[theHost.mUnitClassIndex]->mTileSize;
        break;
    case EeCalamityOp::EdgeTile:
        r.ok = ::gWorld.GetMap()->IsEdgeTile((long)c.index, (long)c.index2);
        break;
    case EeCalamityOp::TileUnblocked: {
        EEPlayer* thePlayer = theUnit != NULL ? theUnit->mPlayer : ::gWorld.mPlayers[0];
        r.ok = EEIsTileUnblocked(thePlayer, theHost.mUnitClassIndex, ::gWorld.GetMap()->GetTile(theX, theY));
        break;
    }
    case EeCalamityOp::CasterIsRadioMan:
        r.ok = theUnit->mUnitClass->mUnitType == kUnitTypeInf10RadioMan;
        break;
    case EeCalamityOp::TileFree:
        r.ok = EEIsTileFree(NULL, theX, theY);
        break;
    default:
        ESVERIFY_AT(false, 1);
        break;
    }
}

}   // namespace

EeCalamityCast EECalamityCastOf(EECalamityClass* inClass, EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    EeCalamityCast theCast = EeCalamityCast();
    theCast.mode = EeCalamityCastMode::Base;
    theCast.class_enum = (int32_t)inClass->mClassEnum;
    theCast.x = (uint32_t)inX;
    theCast.y = (uint32_t)inY;
    theCast.has_caster = inUnit != NULL;
    theCast.blocked_by_immunity = inClass->mIsBlockedByImmunity;
    return theCast;
}

long EECalamityCanCastAt(EECalamityClass* inClass, EeCalamityCastMode inMode, EEComplexUnit* inUnit, unsigned long inX,
                         unsigned long inY, bool inCheck, EEUnit* inTargetUnit)
{
    EeCalamityCast theCast = EECalamityCastOf(inClass, inUnit, inX, inY);
    theCast.mode = inMode;
    theCast.check = inCheck;
    theCast.cost = (int32_t)inClass->GetCost();
    CastHost theHost = {inClass, inUnit, inTargetUnit, NULL, 0, std::vector<EEUnit*>()};
    EeCalamityHostFfi theFfi;
    theFfi.ctx = &theHost;
    theFfi.call = &CastCall;
    return ee_calamity_can_cast_at(&theCast, &theFfi);
}

bool EECalamityValidTarget(EECalamityClass* inClass, EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    EeCalamityCast theCast = EECalamityCastOf(inClass, inUnit, 0, 0);
    CastHost theHost = {inClass, inUnit, inTargetUnit, inTargetUnit, 0, std::vector<EEUnit*>()};
    EeCalamityHostFfi theFfi;
    theFfi.ctx = &theHost;
    theFfi.call = &CastCall;
    return ee_calamity_valid_target(&theCast, &theFfi);
}

bool EECalamityValidLocation(EECalamityClass* inClass, const EeCalamityCast& inCast, EEComplexUnit* inUnit,
                             long inUnitClassIndex)
{
    CastHost theHost = {inClass, inUnit, NULL, NULL, inUnitClassIndex, std::vector<EEUnit*>()};
    EeCalamityHostFfi theFfi;
    theFfi.ctx = &theHost;
    theFfi.call = &CastCall;
    return ee_calamity_valid_location(&inCast, &theFfi);
}
