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
class PIMModule;

// sRandomMapGenerators' element (0x38 bytes: idiv 0x38; implicit ctor 0x286d4f and dtor 0x286d6e, out of line)
struct EERMGenerator {
    // inline: Initialize's std::sort expands it (the UWideString operator< import on +0x14 inside the sort COMDATs)
    bool operator<(const EERMGenerator& inOther) const { return mDisplayName < inOther.mDisplayName; }

    UWideString mName;                      // +0x00 the script name (EERMGetGeneratorName; compared with the map type)
    UWideString mDisplayName;               // +0x14 (the StringTableID string, else the name)
    UString mModuleName;                    // +0x28 the generator PIM's name ("UseMapGenerator")
};

// the generator PIM entry points (cdecl: `push edi; call [esi+0x41c]; pop ecx`)
typedef long (*EERMPIMFunction)(EERandomMapData* inMap);
typedef long (*EERMPIMSetValuesFunction)(EERandomMapData* inMap, const UString& inScript);

class EERandomMap {
public:
    EERandomMap(PIMModule* inModule, EEGameOptions* inOptions, std::vector<EEPlayer*>* inPlayers);   // 0x283c18
    virtual ~EERandomMap();                 // 0x284025

    // 0x28655a: up to 11 tries with fresh seeds until the players can reach each other (cdecl)
    static long CreateRandomMap(EEGameOptions* inOptions, std::vector<EEPlayer*>& inPlayers, unsigned long inPlayer,
                                const UWideString& inGenerator, unsigned long inSeed, EEMap* inMap,
                                GE3DPoint& outLocation);
    // 0x286804 ("EERandomMap::Initialize: %s random map generator detected."): the generator list from the PIMs and
    // Data\Random Map Scripts\*.rmv
    static long Initialize(const FSFileSpec& inDirectory);

    // members in address order (provisional names)
    void Fn284149();                        // 0x284149 (tiles near the water's edge; the coast list mV130)
    long Generate();                        // 0x28449e (the PIM steps with the load progress)
    void PlaceFeatures();                   // 0x2849f2 (the units of mFeatures and mWalls)
    void CreateRandomMap(unsigned long inPlayer, unsigned long inGenerator, unsigned long inSeed, EEMap* inMap,
                         GE3DPoint& outLocation);   // 0x285560
    void CreateTeams();                     // 0x2856e7
    void CreateValidMap();                  // 0x285933 ("EERandomMap::CreateValidMap %d valid tiles found")
    void PlaceWall(EEUnitClass* inClass, const GE2DPoint& inStart, const GE2DPoint& inEnd, EEPlayer* inPlayer);   // 0x2859e7
    long FilterHeightMap();                 // 0x285d89
    void SmoothHeights();                   // 0x285d9f
    void InitializeValues(unsigned long inSeed);   // 0x286d8d
    void CreateGrids();                     // 0x2871fa
    void MarkFeatures();                    // 0x287636
    void MarkLine(const GE2DPoint& inStart, const GE2DPoint& inEnd);   // 0x287a3f
    void PlaceTerrainUnit(unsigned long inX, unsigned long inY, EETerrainTable* inTerrain);   // 0x287c18
    void PaintTiles();                      // 0x287d37 ("EERandomMap::PaintTiles Processing completed in ...")
    void Fn288426();                        // 0x288426
    long SetUp(unsigned long inGenerator, EEMap* inMap, unsigned long inSeed);   // 0x288742
    void AddNeighbourHeight(unsigned long inX, unsigned long inY, float& ioSum, long& ioCount, float& ioMinimum,
                            float& ioMaximum);   // 0x2887bd
    void Fn288839();                        // 0x288839

    static FSFileSpec sGeneratorDirectory;  // 0x565588 (guard 0x565580 bit 1)
    static std::vector<EERMGenerator> sRandomMapGenerators;   // 0x565570 (guard bit 2)

    EERandomMapData mData;                  // +0x004 (the PIMs' state)
    EERMPIMFunction mCreateFlatMap;         // +0x400 "EERMPIMCreateFlatMap"
    EERMPIMFunction mCreateHeightMap;       // +0x404
    EERMPIMFunction mCreateNeutralLandWaterMap;   // +0x408
    EERMPIMFunction mCreatePlayerLandWaterMap;    // +0x40c
    EERMPIMFunction mCreateTrees;           // +0x410
    EERMPIMFunction mFilterHeightMapPIM;    // +0x414 "EERMPIMFilterHeightMap"
    EERMPIMFunction mGenerateNeutralPositions;    // +0x418
    EERMPIMFunction mGeneratePlayerPositions;     // +0x41c
    EERMPIMFunction mGenerateResources;     // +0x420
    EERMPIMFunction mGenerateShallows;      // +0x424
    EERMPIMSetValuesFunction mSetRandomMapValues; // +0x428
    PIMModule* mModule;                     // +0x42c (assigned in the ctor's body)
    EEMap* mMap;                            // +0x430 (Initialize)
    std::vector<EEPlayer*>* mGamePlayers;   // +0x434 (init list)
    EEGameOptions* mOptions;                // +0x438 (init list)
};

// file-scope helpers of EERandomMap.cpp (cdecl, called with &map->+4; provisional names)
bool EERMArePlayersConnected(EERandomMapData& inMap, EEMap* inGameMap);   // 0x284701

// other units' functions the random map calls (cdecl; provisional names)
extern bool& gUsingCarbon;                   // 0x5318e4 (= mU3D8 bit 0x10 while a map is generated)
class EEComplexUnit;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
void EEDropDiagonalCorners(std::vector<EETileCoordinate>& ioTiles);  // 0x2457a3 (the wall's tile line)
void EEDropRepeatedTiles(std::vector<EETileCoordinate>& ioTiles);  // 0x24da0c
void EEPickWallPieces(std::vector<EETileCoordinate>& inTiles, std::vector<long>& outCorners, EEMap* inMap);  // 0x24cca5
void EEShapeWall(EEComplexUnit* inUnit, long inCorner, long inRaisedCorners);      // 0x24e647
void EEMakeWallJunctions(std::vector<EEComplexUnit*>& inUnits, std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer,
                    EEMap* inMap);   // 0x243ae1

// the random map generator list (EERandomMap::sRandomMapGenerators; exe-view free functions, ee/EERandomMapGenerators.h)
#include "ee/EERandomMapGenerators.h"

#endif
