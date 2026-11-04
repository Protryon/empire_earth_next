// C:\EEX\Empire Earth\Random Map\Random Map Generators\Default\Default.cpp (file name inferred: the plugin
// entry points; units link alphabetically, and this one sits between the CMU unit and EEClumps)
// Unit flags (hypothesis, docs/CALIBRATION.md): -O1 -G6 -MD -GR
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"
#include "lle/U2DFractal.h"
#include "ee/EERandomMap.h"
#include "CMU Map Generator.h"
#include "HMUFractal.h"
#include "Unit10.h"
#include "EEClumps.h"
#include "EELandClumps.h"
#include "EEIslandClumps.h"

// Compiler-generated for this unit (names in analysis/default/names.txt):
// SYNTHETIC: DEFAULT 0x100076f6
// SYNTHETIC: DEFAULT 0x10007700
// SYNTHETIC: DEFAULT 0x10007711
// SYNTHETIC: DEFAULT 0x1000771d
// SYNTHETIC: DEFAULT 0x10007728
// SYNTHETIC: DEFAULT 0x10007732
// SYNTHETIC: DEFAULT 0x10007743
// SYNTHETIC: DEFAULT 0x1000774f
// SYNTHETIC: DEFAULT 0x1000775a
// SYNTHETIC: DEFAULT 0x10007764
// SYNTHETIC: DEFAULT 0x10007775
// SYNTHETIC: DEFAULT 0x10007781
// SYNTHETIC: DEFAULT 0x1000778c
// SYNTHETIC: DEFAULT 0x10007796
// SYNTHETIC: DEFAULT 0x100077a7
// SYNTHETIC: DEFAULT 0x100077b3
// SYNTHETIC: DEFAULT 0x100077be
// SYNTHETIC: DEFAULT 0x100077c8
// SYNTHETIC: DEFAULT 0x100077d9
// SYNTHETIC: DEFAULT 0x100077e5
// SYNTHETIC: DEFAULT 0x100077f0
// SYNTHETIC: DEFAULT 0x100077fa
// SYNTHETIC: DEFAULT 0x1000780b
// SYNTHETIC: DEFAULT 0x10007817
// SYNTHETIC: DEFAULT 0x10007822
// SYNTHETIC: DEFAULT 0x1000782c
// SYNTHETIC: DEFAULT 0x1000783d
// SYNTHETIC: DEFAULT 0x10007849
// SYNTHETIC: DEFAULT 0x10007854
// SYNTHETIC: DEFAULT 0x1000785e
// SYNTHETIC: DEFAULT 0x1000786f
// SYNTHETIC: DEFAULT 0x1000787b
// SYNTHETIC: DEFAULT 0x100079e4
// SYNTHETIC: DEFAULT 0x100079ee

static UString gName("Default");
static unsigned long gType = 'rmap';

// FUNCTION: DEFAULT 0x1000788c
// Returns the plug-in's name, "Default"
static const UString* PIMGetName()
{
    return &gName;
}

// FUNCTION: DEFAULT 0x10007892
// Returns the plug-in type 'rmap' (random map generator)
static unsigned long PIMGetType()
{
    return gType;
}

// FUNCTION: DEFAULT 0x10007898
// Reports the plug-in as always usable
static bool PIMIsValid()
{
    return true;
}

// FUNCTION: DEFAULT 0x1000789b
// Grows the flat start areas of players and neutrals (CMUCreateFlatMap)
extern "C" long EERMPIMCreateFlatMap(EERandomMap* ioMap)
{
    return CMUCreateFlatMap(ioMap);
}

// FUNCTION: DEFAULT 0x100078a6
// Builds the map's fractal height map (CMUCreateHeightMap)
extern "C" long EERMPIMCreateHeightMap(EERandomMap* ioMap)
{
    return CMUCreateHeightMap(ioMap);
}

// FUNCTION: DEFAULT 0x100078b1
// Does nothing for neutral land and water; returns success
extern "C" long EERMPIMCreateNeutralLandWaterMap(EERandomMap* ioMap)
{
    return 0;
}

EEClumps* CreatePlayerClumps(EERandomMap* ioMap, std::vector<unsigned long>& inSizes, unsigned long inPlayer,
                             unsigned long inCount, float inSpread, unsigned long inTotal);

// FUNCTION: DEFAULT 0x100078b4
// Grows every player's and neutral's land mass, using CreatePlayerClumps to make each grower
extern "C" long EERMPIMCreatePlayerLandWaterMap(EERandomMap* ioMap)
{
    return CMUCreateLandWaterMap(ioMap, CreatePlayerClumps);
}

// FUNCTION: DEFAULT 0x100078c5
// Creates the land grower for one player: island clumps (kept a river apart from other players)
// when the map has a river width and the player is not a moon, else plain land clumps
EEClumps* CreatePlayerClumps(EERandomMap* ioMap, std::vector<unsigned long>& inSizes, unsigned long inPlayer,
                             unsigned long inCount, float inSpread, unsigned long inTotal)
{
    unsigned long theTargetSize = inTotal / inCount;
    if (!ioMap->mPlayers[inPlayer].mIsMoon && ioMap->mRiverWidth != 0)
        return new EEIslandClumps(ioMap, &ioMap->mClaimedTiles, &inSizes, inCount, inSpread, theTargetSize, inPlayer,
                                  ioMap->mPlayers[inPlayer].mIsNeutral);
    return new EELandClumps(ioMap, &ioMap->mClaimedTiles, &inSizes, inCount, inSpread, theTargetSize, inPlayer,
                            ioMap->mPlayers[inPlayer].mIsNeutral);
}

// FUNCTION: DEFAULT 0x10007977
// Plants the forests around players and neutrals (CMUCreateTrees)
extern "C" long EERMPIMCreateTrees(EERandomMap* ioMap)
{
    return CMUCreateTrees(ioMap);
}

// FUNCTION: DEFAULT 0x10007982
// Smooths the height map and snaps coast lines to sea level (CMUFilterHeightMap)
extern "C" long EERMPIMFilterHeightMap(EERandomMap* ioMap)
{
    return CMUFilterHeightMap(ioMap);
}

// FUNCTION: DEFAULT 0x1000798d
// Places the neutral land masses and moons (the river width goes in as the unused count argument)
extern "C" long EERMPIMGenerateNeutralPositions(EERandomMap* ioMap)
{
    return CMUGenerateNeutralPositions(ioMap, ioMap->mRiverWidth);
}

// FUNCTION: DEFAULT 0x100079b3
// Places the players' start positions on the player ring
extern "C" long EERMPIMGeneratePlayerPositions(EERandomMap* ioMap)
{
    return CMUGeneratePlayerPositions(ioMap);
}

// FUNCTION: DEFAULT 0x100079be
// Places resources, herds and the script's spaced objects (CMUGenerateResources)
extern "C" long EERMPIMGenerateResources(EERandomMap* ioMap)
{
    return CMUGenerateResources(ioMap);
}

// FUNCTION: DEFAULT 0x100079c9
// Raises shallow fords between the players' land masses (CMUGenerateShallows)
extern "C" long EERMPIMGenerateShallows(EERandomMap* ioMap)
{
    return CMUGenerateShallows(ioMap);
}

// FUNCTION: DEFAULT 0x100079d4
// Reads the named .rmv script into the map settings (SetRandomMapValues)
extern "C" long EERMPIMSetRandomMapValues(EERandomMap* ioMap, const UString& inScriptName)
{
    return SetRandomMapValues(ioMap, inScriptName);
}

#include "lle/PIMModule.h"

// The plug-in's entry points (its DLL's export table before; registered in PIMModule.cpp)
static const PIMEntry sEntries[] = {
    PIM_ENTRY(PIMGetName),
    PIM_ENTRY(PIMGetType),
    PIM_ENTRY(PIMIsValid),
    PIM_ENTRY(EERMPIMCreateFlatMap),
    PIM_ENTRY(EERMPIMCreateHeightMap),
    PIM_ENTRY(EERMPIMCreateNeutralLandWaterMap),
    PIM_ENTRY(EERMPIMCreatePlayerLandWaterMap),
    PIM_ENTRY(EERMPIMCreateTrees),
    PIM_ENTRY(EERMPIMFilterHeightMap),
    PIM_ENTRY(EERMPIMGenerateNeutralPositions),
    PIM_ENTRY(EERMPIMGeneratePlayerPositions),
    PIM_ENTRY(EERMPIMGenerateResources),
    PIM_ENTRY(EERMPIMGenerateShallows),
    PIM_ENTRY(EERMPIMSetRandomMapValues),
    { NULL, NULL },
};
extern const PIMPlugin gPIMPluginDefaultRandomMap = { "Default.dll", sEntries };
