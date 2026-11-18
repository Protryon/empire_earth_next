// C:\EEX\Empire Earth\Random Map\EERandomMap.h, the exe's view (RECONSTRUCTION; file name inferred). The game's
// random map: RTTI EERandomMap (vtable 0x44c014, one slot: ??_G 0x283dea), built by
// EERandomMap::CreateRandomMap 0x28655a for one generator PIM (a "pamr" PIMModule) and thrown away afterwards.
//
// Layout: vptr +0x000, the generator state +0x004..+0x400, the PIM entry points +0x400..+0x42c, then the module, the
// map, the game's players and the options. The ctor 0x283c18 calls the state's ctor 0x283e06 with `lea ecx,[esi+4]`
// before its vptr store, and every PIM call passes `lea edi,[esi+4]` with no null test, as does the dtor 0x284025
// (`add ecx,4; jmp`): the state is the first data member (a base class converts `this` with neg/sbb/and, tried).
// The state is the struct the generator PIMs receive, reconstructed from Default.dll
// in ee/EERandomMap.h. That header names it EERandomMap (Default.dll's names.txt binds by those mangled names), but
// in the exe the name belongs to this class, so the exe sees the struct as EERandomMapData (name inferred: no symbol of
// the struct survives; the PIMs are extern "C" and take a pointer). Same layout, one header: the rename is done at the
// include below, nothing is copied.
#ifndef EE_EERANDOMMAPCLASS_H
#define EE_EERANDOMMAPCLASS_H

#include <vector>

#define EERandomMap EERandomMapData
#include "ee/EERandomMap.h"
#undef EERandomMap

class EEGameOptions;
class EEMap;
class EEPlayer;
class EETerrainTable;
class EEUnitClass;
class GE3DPoint;
struct EeRmapGameState;
struct EeRmapGameHost;

// sRandomMapGenerators' element (0x38 bytes: idiv 0x38; implicit ctor 0x286d4f and dtor 0x286d6e, out of line)
struct EERMGenerator {
    // inline: Initialize's std::sort expands it (the UWideString operator< import on +0x14 inside the sort COMDATs)
    bool operator<(const EERMGenerator& inOther) const { return mDisplayName < inOther.mDisplayName; }

    UWideString mName;                      // +0x00 the script name (EERMGetGeneratorName; compared with the map type)
    UWideString mDisplayName;               // +0x14 (the StringTableID string, else the name)
    UString mModuleName;                    // +0x28 the generator PIM's name ("UseMapGenerator")
};

// the generator's entry points (Default.cpp: the "rmap" plug-in's exports, linked in; the plug-in registry went)
extern "C" {
long EERMPIMCreateFlatMap(EERandomMapData* ioMap);
long EERMPIMCreateHeightMap(EERandomMapData* ioMap);
long EERMPIMCreateNeutralLandWaterMap(EERandomMapData* ioMap);
long EERMPIMCreatePlayerLandWaterMap(EERandomMapData* ioMap);
long EERMPIMCreateTrees(EERandomMapData* ioMap);
long EERMPIMFilterHeightMap(EERandomMapData* ioMap);
long EERMPIMGenerateNeutralPositions(EERandomMapData* ioMap);
long EERMPIMGeneratePlayerPositions(EERandomMapData* ioMap);
long EERMPIMGenerateResources(EERandomMapData* ioMap);
long EERMPIMGenerateShallows(EERandomMapData* ioMap);
long EERMPIMSetRandomMapValues(EERandomMapData* ioMap, const UString& inScriptName);
}
const UString& EERMPIMGetName();            // the generator's name ("Default"; its PIMGetName)

class EERandomMap {
public:
    EERandomMap(EEGameOptions* inOptions, std::vector<EEPlayer*>* inPlayers);
    virtual ~EERandomMap();

    // 0x28655a: up to 11 tries with fresh seeds until the players can reach each other (cdecl)
    static long CreateRandomMap(EEGameOptions* inOptions, std::vector<EEPlayer*>& inPlayers, unsigned long inPlayer,
                                const UWideString& inGenerator, unsigned long inSeed, EEMap* inMap,
                                GE3DPoint& outLocation);
    // 0x286804 ("EERandomMap::Initialize: %s random map generator detected."): the generator list from the PIMs and
    // Data\Random Map Scripts\*.rmv
    static long Initialize(const FSFileSpec& inDirectory);

    // members in address order (provisional names). The game passes between the PIM steps went to Rust (ee_rmap
    // game.rs): Fn284149 (the water points), SmoothHeights, Fn288839 (the heights onto the mesh), PaintTiles with
    // PlaceTerrainUnit, MarkFeatures with MarkLine, PlaceFeatures, Fn288426 (the resource objects); Generate runs them
    // through RunGamePass.
    long Generate();                        // 0x28449e (the PIM steps with the load progress)
    void CreateRandomMap(unsigned long inPlayer, unsigned long inGenerator, unsigned long inSeed, EEMap* inMap,
                         GE3DPoint& outLocation);
    void CreateTeams();
    void CreateValidMap();                  // 0x285933 ("EERandomMap::CreateValidMap %d valid tiles found")
    void PlaceWall(EEUnitClass* inClass, const GE2DPoint& inStart, const GE2DPoint& inEnd, EEPlayer* inPlayer);
    long FilterHeightMap();
    void InitializeValues(unsigned long inSeed);
    void CreateGrids();
    long SetUp(unsigned long inGenerator, EEMap* inMap, unsigned long inSeed);
    void RunGamePass(void (*inPass)(EeRmapGameState* ioState, EeRmapGameHost* inHost));   // port: one Rust pass

    static std::vector<EERMGenerator> sRandomMapGenerators;   // 0x565570 (guard bit 2)

    EERandomMapData mData;                  // +0x004 (the PIMs' state)
    EEMap* mMap;                            // +0x430 (Initialize)
    std::vector<EEPlayer*>* mGamePlayers;   // +0x434 (init list)
    EEGameOptions* mOptions;                // +0x438 (init list)
};

// file-scope helpers of EERandomMap.cpp (cdecl, called with &map->+4; provisional names)
bool EERMArePlayersConnected(EERandomMapData& inMap, EEMap* inGameMap);

// other units' functions the random map calls (cdecl; provisional names)
extern bool& gUsingCarbon;                   // 0x5318e4 (= mU3D8 bit 0x10 while a map is generated)
class EEComplexUnit;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
void EEDropDiagonalCorners(std::vector<EETileCoordinate>& ioTiles);  // 0x2457a3 (the wall's tile line)
void EEDropRepeatedTiles(std::vector<EETileCoordinate>& ioTiles);
void EEPickWallPieces(std::vector<EETileCoordinate>& inTiles, std::vector<long>& outCorners, EEMap* inMap);
void EEShapeWall(EEComplexUnit* inUnit, long inCorner, long inRaisedCorners);
void EEMakeWallJunctions(std::vector<EEComplexUnit*>& inUnits, std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer,
                    EEMap* inMap);

// the random map generator list (EERandomMap::sRandomMapGenerators; exe-view free functions, ee/EERandomMapGenerators.h)
#include "ee/EERandomMapGenerators.h"

#endif
