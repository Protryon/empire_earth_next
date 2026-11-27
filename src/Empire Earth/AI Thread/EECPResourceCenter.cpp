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
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "rust/sim/cp.h"

static uint64_t EECPRCHandle(const void* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

template <class T> static T* EECPRCObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>((uintptr_t)inHandle);
}

// The centres' world (ee_sim::cp::place_game's EE_CP_CENTER_* operations; `inA`, `inB` units): the references, the
// area effects
extern "C" int64_t ee_cp_center_host(const EeCpPlaceCtx* inContext, int32_t inOp, uint64_t inA, uint64_t inB,
    EeCpPlaceIo* io)
{
    EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(EECPRCObject<EEPlayer>(inContext->player));
    EEAreaEffect* theEffect = EECPRCObject<EEAreaEffect>(inB);
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_CENTER_UNIT_REFERENCE: EECPRCObject<EEUnit>(inA)->Reference();
        return 0;
    case EE_CP_CENTER_UNIT_DEREFERENCE: EECPRCObject<EEUnit>(inA)->Dereference();
        return 0;
    case EE_CP_CENTER_WORLD_COMPLEX:
        return (int64_t)EECPRCHandle(static_cast<EEUnit*>(dynamic_cast<EEComplexUnit*>(gWorld.GetUnit((uint32_t)v[0]))));
    case EE_CP_CENTER_HAS_AREA_EFFECT:
        return gAEManager.HasAreaEffect((long)v[0], (long)v[1], (long)v[2], (long)v[3]) ? 1 : 0;
    case EE_CP_CENTER_TEMPLATE_RADIUS: io->f[0] = gAEManager.mTemplates[(unsigned long)v[0]]->mRadius;
        return 0;
    case EE_CP_CENTER_EFFECT_COUNT:
        return (int64_t)static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>(inA))->mAreaEffects.mItems.size();
    case EE_CP_CENTER_EFFECT_AT:
        return (int64_t)EECPRCHandle(static_cast<EEComplexUnit*>(EECPRCObject<EEUnit>(inA))->mAreaEffects.mItems[(size_t)v[0]]);
    case EE_CP_CENTER_EFFECT_KIND:
        return theEffect->mKind;
    case EE_CP_CENTER_EFFECT_RADIUS: io->f[0] = theEffect->GetRadius();
        return 0;
    case EE_CP_CENTER_EFFECT_DIRTY: theEffect->MarkDirty();
        return 0;
    case EE_CP_CENTER_EFFECT_RECOUNT: theEffect->Recount(0);
        return 0;
    case EE_CP_CENTER_EFFECT_LEVEL:
        return theEffect->GetLevel();
    case EE_CP_CENTER_EFFECT_MAX_LEVEL:
        return theEffect->mTemplate->mMaxLevel;
    case EE_CP_CENTER_UPGRADE:
        return (int64_t)EECPRCHandle(thePlayer->mUnitClassUpgrades[(long)v[0]]);
    }
    return 0;
}
