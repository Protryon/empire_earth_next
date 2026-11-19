// C:\EEX\Empire Earth\AI Thread\EECPResourceCenter.cpp (file name inferred: no __FILE__; the class-static key
// "EECPResourceCenter-Data"). A computer player's resource center: 0x8bbbf..0x8e702 (the key chain first; the vector /
// sort / find_if COMDATs close the unit).
//
// AI lane B, round 2, and the AI Thread cluster, round 2: the centres are Rust's whole (ee_sim::cp lists.rs: the
// members a Rust record, the forwarders with their PLCE records; resource_center.rs: the bodies). What stays here:
// EEComplexUnit::DistanceSquaredToUnit (the original's, defined in this unit) and ee_cp_center_host, the units and
// area effects the Rust bodies ask.
#include <windows.h>
#include <stdint.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/USingleton.h"

#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EECPPlacement.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EECPPlaceCapture.h"
#include "rust/sim/cp.h"

static uint64_t EECPRCHandle(const void* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

template <class T> static T* EECPRCObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>((uintptr_t)inHandle);
}

// Returns the squared distance from this unit's position to another unit's position (its tile if not a complex unit)
float EEComplexUnit::DistanceSquaredToUnit(EEUnit* inOther)
{
    float theDeltaY;
    float theDeltaX;
    EEComplexUnit* theOther = inOther->mComplexUnit;
    if (theOther != NULL) {
        theDeltaY = this->mPhysicalModel.GetLocation().GetY() - theOther->mPhysicalModel.GetLocation().GetY();
        theDeltaX = this->mPhysicalModel.GetLocation().GetX() - theOther->mPhysicalModel.GetLocation().GetX();
    } else {
        theDeltaY = this->mPhysicalModel.GetLocation().GetY() - inOther->GetTileY();
        theDeltaX = this->mPhysicalModel.GetLocation().GetX() - inOther->GetTileX();
    }
    return theDeltaX * theDeltaX + theDeltaY * theDeltaY;
}

// The owner's living, unfinished houses (unit type 0x25, +0x34c clear) whose tile lies in mRect (0x8da4d, out of
// line) (provisional name; layout inferred: 0x14 bytes on GetMoraleFill's frame)
struct EECPRCBuildingInRectangle {
    EECPRCBuildingInRectangle(EEPlayer* inPlayer, const U2DRectangle<long>& inRect) : mPlayer(inPlayer), mRect(inRect) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && inUnit->GetUnitClass()->mUnitType == kUnitTypeHouse && !inUnit->mIsBuilt &&
               inUnit->mHitPoints > 0 && this->mRect.Contains(U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY()));
    }

    EEPlayer* mPlayer;                      // +0x00
    U2DRectangle<long> mRect;               // +0x04
};

// The owner's living houses (unit type 0x25) whose tile lies in mRect, finished or not (0x8d6e0, out of line;
// EECPRCBuildingInRectangle without the +0x34c test) (provisional name; layout inferred: 0x14 bytes on FindBuildSite's
// frame)
struct EECPRCLiveBuildingInRectangle {
    EECPRCLiveBuildingInRectangle(EEPlayer* inPlayer, const U2DRectangle<long>& inRect) : mPlayer(inPlayer), mRect(inRect) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && inUnit->GetUnitClass()->mUnitType == kUnitTypeHouse && inUnit->mHitPoints > 0 &&
               this->mRect.Contains(U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY()));
    }

    EEPlayer* mPlayer;                      // +0x00
    U2DRectangle<long> mRect;               // +0x04
};

// the unit's first morale-from-buildings effect (NULL: none)
static EEAreaEffect* EECPRCMoraleEffect(EEComplexUnit* inUnit)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = inUnit->mAreaEffects.mItems.begin();
         theIterator != inUnit->mAreaEffects.mItems.end(); ++theIterator) {
        if ((*theIterator)->mKind == kAreaEffectMoraleFromBuildings)
            return *theIterator;
    }
    return NULL;
}

// The centres' world (ee_sim::cp::place_game's EE_CP_CENTER_* operations; `inA`, `inB` units): the references, the
// area effects
extern "C" int64_t ee_cp_center_host(const EeCpPlaceCtx* inContext, int32_t inOp, uint64_t inA, uint64_t inB,
    EeCpPlaceIo* io)
{
    EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(EECPRCObject<EEPlayer>(inContext->player));
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_CENTER_UNIT_REFERENCE:
        EECPRCObject<EEUnit>(inA)->Reference();
        return 0;
    case EE_CP_CENTER_UNIT_DEREFERENCE:
        EECPRCObject<EEUnit>(inA)->Dereference();
        return 0;
    case EE_CP_CENTER_WORLD_COMPLEX:
        return (int64_t)EECPRCHandle(static_cast<EEUnit*>(dynamic_cast<EEComplexUnit*>(gWorld.GetUnit((uint32_t)v[0]))));
    case EE_CP_CENTER_DISTANCE_TO_UNIT:
        io->f[0] = static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>(inA))->DistanceSquaredToUnit(EECPRCObject<EEUnit>(inB));
        return 0;
    case EE_CP_CENTER_HAS_AREA_EFFECT:
        return gAEManager.HasAreaEffect((long)v[0], (long)v[1], (long)v[2], (long)v[3]) ? 1 : 0;
    case EE_CP_CENTER_MORALE_RADIUS: {
        EEAreaEffect* theEffect = EECPRCMoraleEffect(static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>(inA)));
        if (theEffect == NULL)
            return 0;
        io->f[0] = theEffect->GetRadius();
        return 1;
    }
    case EE_CP_CENTER_MORALE_RECOUNT: {
        EEAreaEffect* theEffect = EECPRCMoraleEffect(static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>(inA)));
        if (theEffect == NULL)
            return 0;
        theEffect->MarkDirty();
        theEffect->Recount(0);
        io->v[0] = theEffect->GetLevel();
        io->v[1] = theEffect->mTemplate->mMaxLevel;
        io->f[0] = theEffect->GetRadius();
        return 1;
    }
    case EE_CP_CENTER_EFFECT_TEMPLATES: {
        EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[(long)v[0]];
        for (unsigned long i = 0; i < 4; i++)
            io->v[i] = (int64_t)(uint32_t)theUpgrade->mAreaEffectTemplates[i];
        return 0;
    }
    case EE_CP_CENTER_TEMPLATE_RADIUS:
        io->f[0] = gAEManager.mTemplates[(unsigned long)v[0]]->mRadius;
        return 0;
    case EE_CP_PLACE_UNITS_IN_RECT: {
        U2DRectangle<long> theRect;
        theRect.mLeft = (long)v[0];
        theRect.mTop = (long)v[1];
        theRect.mRight = (long)v[2];
        theRect.mBottom = (long)v[3];
        std::vector<EEComplexUnit*> theUnits;
        for (size_t i = 0; i < io->input_len; i++)
            theUnits.push_back(static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>((uint64_t)io->input[2 * i])));
        if (v[4] == 1)
            EEGetUnitsInRectangle(theRect, theUnits, EECPRCLiveBuildingInRectangle(thePlayer, theRect), true);
        else
            EEGetUnitsInRectangle(theRect, theUnits, EECPRCBuildingInRectangle(thePlayer, theRect), true);
        for (size_t i = 0; i < theUnits.size(); i++)
            ee_cp_place_push(io->list, (int64_t)EECPRCHandle(static_cast<EEUnit*>(theUnits[i])), 0);
        return 0;
    }
    }
    return 0;
}
