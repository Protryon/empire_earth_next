// EEUnitClassKinds.h (new, R5b.4 round 32): the free functions the EEUC* headers declared after their classes.
// The classes themselves are kinds of EEUnitClass (ee/EEUnit.h; the kind tests are ee/EEUnitKind.h).
#ifndef EE_EEUNITCLASSKINDS_H
#define EE_EEUNITCLASSKINDS_H

#include <vector>

#include "ee/EEUnit.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EEUnitKind.h"

class EEComplexUnit;
class EEUnit;
class EESoundObject;
class EEPhysicsModel;
template <class T> class U2DPoint;

// cdecl helpers (provisional names)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer);  // 0x20b6ff (a carrier's aircraft: the best fighter researched)
EEComplexUnit* EEConvertUnit(EEComplexUnit* inUnit, long inPlayer, bool inIsConversion);  // 0x24e7da (the unit itself, passed on to EEMakeUnitCopy/EEAddDeadUnitStatistic)
// EEUCBuilding.cpp's helpers and its callees (cdecl; provisional names)
long EEGetDockFacing(unsigned long inX, unsigned long inY);  // 0x20ea89 (the side with the most water: a dock's facing)
long EEGetFacingQuarter(long inX, long inY);  // 0x20ebda (the map quarter 0..3 the tile faces)
long EEGetUnitFacingQuarter(EEComplexUnit* inUnit); // 0x20ed11 (EEGetFacingQuarter at the unit's tile)
void EEGetBlockRing(long inArg, long inX, long inY, long inWidth, long inHeight, std::vector<EETileCoordinate>& outTiles,
                bool inArg7);           // 0x246a42 (the tiles of a rectangle)
float EEGetSpaceDockYaw(long inX, long inY, long inSize);  // 0x23d008 (a yaw for the footprint at (inX, inY))
void EEAddToFootprintTiles(EEUnit* inUnit);  // 0x24caef (NotifyRootVisibility)
void EERemoveFromFootprintTiles(EEUnit* inUnit);  // 0x24d946 (GetMouseChild)
// cdecl callees (provisional names)
void EEAddBuildingRegion(EEUnit* inUnit);  // 0x24c8bc (Virtual80)
void EERemoveBuildingRegion(EEUnit* inUnit);  // 0x24d779 (ComputeLoadDistance)
extern unsigned short gFortressUnloadCost;   // 0x4c8fa4 (100)
// 0x531d58: the value EEUCLandPriest's ctor passes on (provisional name; EEUCNavalPriest's is 0x531d68)
extern EEPhysicsModel* gPhysicsModels[12];     // 0x531d58 (EEPMLand.cpp: pointers to the EEPM* singletons; [0] is the land one)
// cdecl helpers of the flying classes (provisional names; types inferred)
void EEGetTilesNearPoint(const GE3DPoint& inPoint, float inDistance, std::vector<EETileCoordinate>& outTiles);  // 0x24b212 (EEMap.cpp's range)
// 0x24939d: EEFindDropLocation (ee/EEUnit.h)
float EEGetHitDistanceSquared(EEComplexUnit* inUnit, EEUnit* inTarget, float inArg3, float inArg4);  // 0x2494cb
// 0x242110 (cdecl): the space version of the sound volume/pan/pitch (EEUCSpaceship::GetSoundParameters) (provisional
// name)
void EEGetDopplerPitch(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch);
// 0x23e530 (cdecl; whether a unit of type inUnitType of inPlayer is blocked on inTile): ee/EEUnitUtilities.h
// 0x249727 (cdecl): the tiles of the line (inX1, inY1)..(inX2, inY2) into outTiles (provisional name)
void EEGetTileLine(long inX1, long inY1, long inX2, long inY2, std::vector<U2DPoint<long> >* outTiles, bool inEnds, bool inThin);  // bools: 0x649727 tests them with `cmp byte`
// the kinds' helpers EEUnitClassKinds.cpp's dispatchers call (provisional names)
void EEAdjustNavalPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inArg);  // the naval kinds' paths
EEComplexUnit* EEGetFirstPart(EEComplexUnit* inUnit);  // a gate's first part (EEUnitClassBuildings.cpp)

#endif
