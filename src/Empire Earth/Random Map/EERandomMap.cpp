// C:\EEX\Empire Earth\Random Map\EERandomMap.cpp (__FILE__ at 0x28655a). The game's random map: the generator list
// (EERandomMap::Initialize over Data\Random Map Scripts\*.rmv) and CreateRandomMap. The random map itself is Rust
// (ee_sim rmap object.rs, world lane round 1): the generator state, a try's set-up, the generator's steps, the game's
// passes, PlaceWall and the retries; ee_rmap_cpp below answers its calls into the map, the players, the tables and the
// game, one statement each. Header: ee/EERandomMapClass.h.
#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETerrainMesh.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGameOptions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"                // gWorld.mUsingCarbon
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERules.h"
#include "rust/sim/rmap.h"


// The world a random map is made in (ee_rmap_cpp's p)
struct EERMWorld {
    EEMap* mMap;
    std::vector<EEPlayer*>* mPlayers;
    EEGameOptions* mOptions;
};

// The generator list's script search (Initialize)
struct EERMFind {
    EERMFind() { mDirectory.SetVolume(gStartupVolume); mDirectory.SetDirectory(gStartupDirectory + "Data\\Random Map Scripts\\"); }
    FSFileSpec mDirectory;
    FSFileSpec mFile;
};

// A text into the Rust side's sink
static void EERMText(int64_t inSink, const UString& inText)
{
    ee_rmap_script_bytes_push((EeRmapScriptBytes*)inSink, (const uint8_t*)(const char*)inText, inText.GetLength());
}

// A string-table string into the Rust side's sink -> GetString's result
static long EERMResourceString(int64_t inSink, unsigned long inID)
{
    UWideString theText;
    long theResult = gResource->GetString(inID, theText);
    ee_rmap_script_bytes_push((EeRmapScriptBytes*)inSink, (const uint8_t*)(const char*)theText, theText.GetTCHARLength());
    return theResult;
}

#define P64(x) ((int64_t)(intptr_t)(x))
#define W ((EERMWorld*)p)
#define TILE ((EEMapTile*)p)
#define PL ((EEPlayer*)p)
#define UC ((EEUnitClass*)p)
#define TT ((EETerrainTable*)p)
#define CX ((EEComplexUnit*)p)

extern "C" double ee_rmap_cpp_f(EeRmapCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// The random map's calls (ee_sim rmap object.rs EeRmapCpp): p the world or the object named, a the integers, f the floats
extern "C" int64_t ee_rmap_cpp(EeRmapCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeRmapCpp O;
    switch (inOp) {
    case O::MapTileCount: return W->mMap->GetTileCount();
    case O::MapIsEdgeTile: return W->mMap->IsEdgeTile((long)a[0], (long)a[1]);
    case O::MapPaintTerrain: W->mMap->PaintTerrain((long)a[0], (long)a[1], (long)a[2]);
        break;
    case O::MapSetCliffTexture: W->mMap->SetCliffTexture((long)a[0]);
        break;
    case O::MapUpdateCliffs: W->mMap->UpdateCliffs(0, W->mMap->mSize, 0, W->mMap->mSize);
        break;
    case O::MapSetCharacteristics: W->mMap->mMapCharacteristics = (unsigned long)a[0];
        break;
    case O::MapRaisedCorners: return W->mMap->GetNumRaisedCorners((unsigned long)a[0], (unsigned long)a[1]);
    case O::MeshSetZ: W->mMap->mTerrainMesh.SetZ((unsigned long)a[0], (unsigned long)a[1], f[0]);
        break;
    case O::MeshValidPoint: return W->mMap->mTerrainMesh.GetIsValidPoint((unsigned long)a[0], (unsigned long)a[1]);
    case O::MeshValidTile: return W->mMap->mTerrainMesh.GetIsValidTile((unsigned long)a[0], (unsigned long)a[1]);
    case O::MeshWaterColorSet: W->mMap->mTerrainMesh.SetWaterColorSet((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::MeshVertexNormal: W->mMap->mTerrainMesh.ComputeVertexNormal((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::MeshSetIsWater: W->mMap->mTerrainMesh.SetIsWater((unsigned long)a[0], (unsigned long)a[1], a[2] != 0);
        break;
    case O::MeshShorelines: W->mMap->mTerrainMesh.ComputeShorelines((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::TileSetHeight: TILE->mHeight = f[0];
        break;
    case O::PlayerCount: return (int64_t)W->mPlayers->size();
    case O::Player: return P64((*W->mPlayers)[(size_t)a[0]]);
    case O::PlayerMaxCitizens: return PL->mMaxStartingCitizens;
    case O::StartEpoch: return W->mOptions->mEpochStart;
    case O::RuleMapSizeCount: return (int64_t)EEGetRules().mGameOptions.mMapSizes.size();
    case O::RuleMapSizeTiles: return EEGetRules().mGameOptions.mMapSizes[(size_t)a[0]].mTiles;
    case O::RuleMapSizeScript: return EEGetRules().mGameOptions.mMapSizes[(size_t)a[0]].mScriptSize;
    case O::ClassName: EERMText(a[0], UC->mName);
        break;
    case O::ClassIsLandCitizen: return EEUnitClassOfKind<EE_CLASS_KIND_LAND_CITIZEN>(UC) != NULL;
    case O::ClassCreateAtPoint: return P64(UC->CreateUnitAtPoint((EEPlayer*)a[0], f[0], f[1], (EEMap*)a[1], f[2]));
    case O::ClassCreateAtTile: return P64(UC->CreateUnitAtTile((EEPlayer*)a[0], (long)a[1], (long)a[2], (EEMap*)a[3], f[0]));
    case O::TerrainCount: return EETerrainTable::GetNumTables();
    case O::Terrain: return P64(EETerrainTable::Get((unsigned long)a[0]));
    case O::TerrainName: EERMText(a[0], TT->mName);
        break;
    case O::TerrainUnitCount: return TT->GetNumUnitClasses();
    case O::TerrainAmbientChance: return TT->mAmbientUnitChance;
    case O::TerrainUnitClass: return TT->GetUnitClass((unsigned long)a[0]);
    case O::CliffCount: return EETerrainTable::GetNumCliffTerrains();
    case O::Cliff: return P64(EETerrainTable::sCliffTable[a[0]]);
    case O::CliffName: EERMText(a[0], ((EECliffTerrainTable*)p)->mName);
        break;
    case O::CxSetBuilt: CX->SetBuilt((long)a[0], (long)a[1], (long)a[2]);
        break;
    case O::CxSetPaidFor: CX->mIsPaidFor = true;
        break;
    case O::CxAnimation: return CX->GetAnimation();
    case O::ScriptDirectory: EERMText(a[0], gStartupVolume + gStartupDirectory + "Data\\Random Map Scripts\\");
        break;
    case O::SetUsingCarbon: gWorld.mUsingCarbon = a[0] != 0;
        break;
    case O::DebugOut: ESDebugOut("%s", (const char*)a[0]);
        break;
    case O::FindNew: return P64(new EERMFind);
    case O::FindDelete: delete (EERMFind*)p;
        break;
    case O::FindFirst: return ((EERMFind*)p)->mDirectory.GetFirstFile("*.rmv", ((EERMFind*)p)->mFile);
    case O::FindNext: return ((EERMFind*)p)->mDirectory.GetNextFile(((EERMFind*)p)->mFile);
    case O::FindName: EERMText(a[0], ((EERMFind*)p)->mFile.GetFile());
        break;
    case O::FindPath: EERMText(a[0], ((EERMFind*)p)->mFile.GetVolume() + ((EERMFind*)p)->mFile.GetDirectory() + ((EERMFind*)p)->mFile.GetFile());
        break;
    case O::ResourceString: return EERMResourceString(a[0], (unsigned long)a[1]);
    case O::Verify: ESVERIFY_AT(a[0] != 0, (long)a[1]);
        break;
    default: return (int64_t)ee_rmap_cpp_f(inOp, p, a, f);
    }
    return 0;
}

extern "C" double ee_rmap_cpp_f(EeRmapCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeRmapCpp O;
    switch (inOp) {
    case O::MapTileUnit: return W->mMap->GetF1955F4();
    case O::MeshZ: return W->mMap->mTerrainMesh.GetZ((unsigned long)a[0], (unsigned long)a[1]);
    default: ESVERIFY_AT(false, 1);
    }
    return 0.0;
}

#undef W
#undef TILE
#undef PL
#undef UC
#undef TT
#undef CX

// Finds the named generator and generates the map, retrying with a new seed (11 tries at most) until the players
// are connected (ee_sim rmap object.rs); kErrorNoMoreFiles when its script names a generator other than the linked
// one (Default)
long EERandomMap::CreateRandomMap(EEGameOptions* inOptions, std::vector<EEPlayer*>& inPlayers, unsigned long inPlayer,
                                  const UWideString& inGenerator, unsigned long inSeed, EEMap* inMap,
                                  GE3DPoint& outLocation)
{
    EERMWorld theWorld = {inMap, &inPlayers, inOptions};
    return ee_rmap_create_random_map(&theWorld, inMap, (uint32_t)inPlayer, inGenerator, (uint32_t)inSeed, &outLocation.mX);
}

// The generator list is ee_sim rmap object.rs' (initialize_generators): the names by value
unsigned long EERMGetNumberOfGenerators()
{
    return ee_rmap_generator_count();
}

UWideString EERMGetGeneratorDisplayName(unsigned long inIndex)
{
    return UWideString(ee_rmap_generator_text(inIndex, true));
}

// the display name of the generator with the given name, or the name itself when none matches
UWideString EERMGetGeneratorDisplayName(const UWideString& inName)
{
    return UWideString(ee_rmap_generator_display(inName));
}

UWideString EERMGetGeneratorName(unsigned long inIndex)
{
    return UWideString(ee_rmap_generator_text(inIndex, false));
}

// a generator's index by name; 3 when there is none
long EERMFindGenerator(const UWideString& inName, unsigned long& outIndex)
{
    return ee_rmap_find_generator(inName, &outIndex);
}

// Lists the random-map generators: each .rmv script naming the linked generator (Default), with its string-table
// display name, sorted; asserts that there is one (ee_sim rmap object.rs initialize_generators)
long EERandomMap::Initialize(const FSFileSpec& inDirectory)
{
    ee_rmap_initialize_generators();
    return 0;
}

// Drops the registered random-map generators (at Empire Earth.cpp's shutdown); returns 0
// (cdecl; provisional name)
long EEClearRandomMapGenerators()
{
    return ee_rmap_generators_clear();
}
