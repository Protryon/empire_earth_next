// C:\EEX\Empire Earth\Empire Earth\EEEMStorm.cpp (name from RTTI EEEMStorm; no __FILE__). 0x12dba8..0x12ed4e: the
// storm keys and colours ($E chains), the storm route planners (Snow/Rain +4), GetTypeName, EEEMStorm (ctor .. Update)
// and EEAIBPGModifierStormShaper. EECommandLine.cpp ends at 0x12dba8, EEEnvironmentManager.cpp starts at 0x12ed4e.
// Unit flags: the exe's module set. (provisional names)
#include <windows.h>
#include <string.h>
#include <vector>
#include <map>
#include <deque>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIHill.h"
#include "ee/EETown.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEEffectManager.h"

// Constructs the storm's "Storm-Data" key at startup and destroys it at exit (wrapper, constructor, registrar,
// destructor)
UString EEEMStorm::sStormData("Storm-Data");
// Constructs the storm's "Environment-Data" key at startup and destroys it at exit (wrapper, constructor,
// registrar, destructor)
UString EEEMStorm::sEnvironmentData("Environment-Data");
// Constructs the black storm colour at startup and registers its destructor
GEColor gStormColor;                        // (read by Step: the environment manager's colour when a rain storm ends)

// (the storms' route planners and shaper, EEStormModifiers.h before: ee_sim::astar modifier_obj.rs)

// ------------------------------------------------------------------------- round 10, lane D: the storm

int64_t EECore10OpD7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EECameraControl.cpp (the environment manager's, the cheats', the civilization's and the wonder's: EEGameLog.cpp)

static_assert(sizeof(EEEffectHandle) == 4 && kStormRain == 1 && kStormSnow == 2 && kStormHurricane == 3 && kStormBlizzard == 4
                  && kGraphicEffectRain == 0x54 && kGraphicEffectSnow == 0xac
                  && sizeof(EECPASGFlags) == (sizeof(long) + 17 + sizeof(long) - 1) / sizeof(long) * sizeof(long)
                  && sizeof(U2DRectangle<long>) == 4 * sizeof(long) && sizeof(EEAIAStarBounds) == 4 * sizeof(long),
              "gamecpp weather.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define S ((EEEMStorm*)p)
#define PL ((EEPlayer*)p)
#define POINTS(i) (*(std::vector<U2DPoint<long> >*)a[i])

// the C++ calls and members of the storm's bodies in Rust (gamecpp weather.rs): p the storm unless noted, one
// statement each
int64_t EECore10OpD2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DsFields: return P64(static_cast<EeAppStormFields*>(S));
    case O::DsStorm: return P64(&S->mStorm);
    case O::DsRequest: return P64(&S->mRequest);
    case O::DsArea: return P64(&S->mArea);
    case O::DsStart: return P64(&S->mStart);
    case O::DsGoal: return P64(&S->mGoal);
    case O::DsSetName: S->mName = EEEMStorm::GetTypeName((long)a[0]);
        break;
    case O::DsRandomPlayer: return P64(gWorld.mPlayers[gPhysics->GetRandom(1L, gWorld.mNumPlayers - 1)]);
    case O::DsRandomTown: return P64(static_cast<EEAIArea*>(PL->GetTown(gPhysics->GetRandom(0L, (long)PL->GetNumTowns() - 1))));
    case O::DsLargestLand: return P64(static_cast<EEAIArea*>(gContinentManager.GetContinents()[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryLand)]));
    case O::DsLargestAny: return P64(static_cast<EEAIArea*>(gContinentManager.GetContinents()[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryNone)]));
    case O::DsLargestWater: return P64(static_cast<EEAIArea*>(gContinentManager.GetContinents()[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryWater)]));
    case O::DsRandomCliff: return P64(static_cast<EEAIArea*>(static_cast<EEAIContinent*>(gContinentManager.GetRandomContinent(kContinentCategoryCliff, gPhysics->mRandom))));
    case O::DsRandomHill: return P64(static_cast<EEAIArea*>(static_cast<EEAIHill*>(gContinentManager.GetRandomArea(kAIAreaHill, gPhysics->mRandom, 0))));
    case O::DsRandomPoint: *(U2DPoint<long>*)a[0] = ((EEAIArea*)p)->GetRandomPoint();
        break;
    case O::DsMapSize: return gWorld.GetMap()->mSize;
    case O::DsRandomLong: return gPhysics->GetRandom((long)a[0], (long)a[1]);
    case O::DsPointsNew: return P64(new std::vector<U2DPoint<long> >);
    case O::DsPointsDelete: delete (std::vector<U2DPoint<long> >*)p;
        break;
    case O::DsFindPath: gPathManager.FindPath(NULL, gWorld.GetMap(), S->mStart, S->mGoal, *(EEAIAStarBounds*)a[0], POINTS(1), 4, 0x6e, (EeAStarModifier*)a[2], &POINTS(3));
        break;
    case O::DsCreateWeather: gWorld.mEffectManager->CreateWeatherEffect(*(std::vector<U2DPoint<long> >*)p, (EEGraphicEffect*)a[0], (EEEffectHandle*)a[1]);
        break;
    case O::DsEffectReference: ((EEE3DRust*)p)->Reference();
        break;
    case O::DsEffectRelease: ((EEE3DRust*)p)->Release();
        break;
    case O::DsEffectBounds: S->mStorm->GetBounds(S->mArea);
        break;
    case O::DsEffectHandle: return ((EEE3DRust*)p)->GetHandle().mValue;
    case O::DsSustain: S->mStorm->mRecord->storm.sustained = a[0] != 0;
        break;
    case O::DsEndEffect: S->mStorm->mRecord->storm.elapsed = S->mStorm->mRecord->storm.duration;
        break;
    case O::DsSetIntensity: S->mStorm->mRecord->storm.intensity = f[0];
        break;
    case O::DsTileCount: return (int64_t)((EEE3DRust*)p)->GetTileCount();
    case O::DsCenter: *(U2DPoint<long>*)a[0] = ((EEE3DRust*)p)->GetCenterPoint();
        break;
    case O::DsCancelRequest: gPathManager.CancelAutoPathRequest((EEAutoPathRequest*)p);
        break;
    case O::DsRequestPath: return P64(gPathManager.RequestPath(0, S->mStart, S->mGoal, (EECPASGFlags*)a[1], 1, 500, (EeAStarModifier*)a[0], 0));
    case O::DsPathClear: S->mPath.clear();
        break;
    case O::DsPathPush: S->mPath.push_back(U2DPoint<long>((long)a[0], (long)a[1]));
        break;
    case O::DsPathLen: return (int64_t)S->mPath.size();
    case O::DsPathAt: *(U2DPoint<long>*)a[1] = S->mPath[a[0]];
        break;
    case O::DsPathResize: S->mPath.resize((size_t)a[0]);
        break;
    case O::DsPopPath: S->mPath.pop_back();
        break;
    case O::DsDistance: return S->mStorm->GetCenterPoint().DistanceToSquared(a[0] != 0 ? S->mGoal : S->mPath.back());
    case O::DsMove: S->mStorm->Move(a[0] != 0 ? S->mGoal : S->mPath.back(), S->mArea);
        break;
    case O::DsResetColor: S->mManager->mC18 = gStormColor;
        break;
    case O::DsPathPointLoad: return S->mPath[a[0]].Load(*(FSFile*)a[1]);
    case O::DsPathPointSave: return S->mPath[a[0]].Save(*(FSFile*)a[1]);
    case O::DsRectLoad: return ((U2DRectangle<long>*)p)->Load(*(FSFile*)a[0]);
    case O::DsRectSave: return ((U2DRectangle<long>*)p)->Save(*(FSFile*)a[0]);
    case O::DsPointLoad: return ((U2DPoint<long>*)p)->Load(*(FSFile*)a[0]);
    case O::DsPointSave: return ((U2DPoint<long>*)p)->Save(*(FSFile*)a[0]);
    default: return EECore10OpD7(inOp, p, a, f);
    }
    return 0;
}

