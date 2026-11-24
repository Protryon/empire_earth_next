// C:\EEX\Empire Earth\Random Map\EERandomMap.h, the exe's view (RECONSTRUCTION; file name inferred). The game's
// random map: RTTI EERandomMap (vtable 0x44c014), built by EERandomMap::CreateRandomMap 0x28655a for one generator
// PIM and thrown away afterwards. The random map is Rust now (ee_sim rmap object.rs, world lane round 1: the generator
// state EERandomMapData, a try's set-up and steps, the game's passes, PlaceWall and the retries); what the C++ keeps
// is the generator list and the two entry points.
#ifndef EE_EERANDOMMAPCLASS_H
#define EE_EERANDOMMAPCLASS_H

#include <vector>

class EEGameOptions;
class EEMap;
class EEPlayer;
class GE3DPoint;

class EERandomMap {
public:
    // 0x28655a: up to 11 tries with fresh seeds until the players can reach each other (cdecl)
    static long CreateRandomMap(EEGameOptions* inOptions, std::vector<EEPlayer*>& inPlayers, unsigned long inPlayer,
                                const UWideString& inGenerator, unsigned long inSeed, EEMap* inMap,
                                GE3DPoint& outLocation);
    // 0x286804 ("EERandomMap::Initialize: %s random map generator detected."): the generator list from
    // Data\Random Map Scripts\*.rmv
    static long Initialize(const FSFileSpec& inDirectory);
    // sRandomMapGenerators (0x565570: 0x38-byte entries of the name, the display name and the module) is ee_sim rmap
    // object.rs' generator list (ee/EERandomMapGenerators.h)
};

// other units' functions the random map calls (cdecl; provisional names)
extern bool& gUsingCarbon;                   // 0x5318e4 (= mU3D8 bit 0x10 while a map is generated)
class EEComplexUnit;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
void EEPickWallPieces(std::vector<EETileCoordinate>& inTiles, std::vector<long>& outCorners, EEMap* inMap);
void EEMakeWallJunctions(std::vector<EEComplexUnit*>& inUnits, std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer,
                    EEMap* inMap);

// the random map generator list (EERandomMap::sRandomMapGenerators; exe-view free functions, ee/EERandomMapGenerators.h)
#include "ee/EERandomMapGenerators.h"

#endif
