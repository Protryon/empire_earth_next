// The computer player's engine per computer seat (ee/EECPEngine.h; docs/AI_LUA_PLAN.md §5 S0).
// New in this port: no original address.
#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

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
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPTrace.h"
#include "ee/EECPEngine.h"

long EECPEngine(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL && inPlayer->mEngine == kEECPEngineNext ? kEECPEngineNext : kEECPEngineClassic;
}

const char* EECPEngineName(long inEngine)
{
    switch (inEngine) {
    case kEECPEngineNext:
        return "next";
    case kEECPEngineRandom:
        return "random";
    default:
        return "classic";
    }
}

// EE_AI_ENGINE (TEST ONLY): the engines in turn by player id from player 1 (empty: unset)
static const std::vector<long>& EECPEngineOverrides()
{
    static std::vector<long> sEngines;
    static bool sRead = false;
    if (sRead)
        return sEngines;
    sRead = true;
    const char* theValue = getenv("EE_AI_ENGINE");
    if (theValue == NULL || theValue[0] == '\0')
        return sEngines;
    std::string theList(theValue);
    size_t theStart = 0;
    while (theStart <= theList.size()) {
        size_t theEnd = theList.find(',', theStart);
        if (theEnd == std::string::npos)
            theEnd = theList.size();
        std::string theName = theList.substr(theStart, theEnd - theStart);
        long k;
        for (k = 0; k < kEECPEngineChoiceCount; k++)
            if (theName == EECPEngineName(k))
                break;
        if (k < kEECPEngineChoiceCount)
            sEngines.push_back(k);
        else
            ESDebugOut("EE-AI EE_AI_ENGINE: unknown engine '%s' ignored\n", theName.c_str());
        theStart = theEnd + 1;
    }
    return sEngines;
}

long EECPEngineResolve(unsigned long inChoice, unsigned long inSeed, unsigned long inSlot, unsigned long inPlayerID)
{
    const std::vector<long>& theOverrides = EECPEngineOverrides();
    if (!theOverrides.empty())
        inChoice = (unsigned long)theOverrides[(inPlayerID > 0 ? inPlayerID - 1 : 0) % theOverrides.size()];
    if (inChoice == kEECPEngineRandom) {
        // a hash of the seed and the slot (32-bit arithmetic: the same on every peer and platform); not the game's
        // generators, so the other draws (the random start epoch, the map) stay as they were
        uint32_t h = (uint32_t)inSeed * 2654435761u ^ ((uint32_t)inSlot + 1u) * 0x9e3779b9u;
        h ^= h >> 15;
        h *= 0x2c1b3c6du;
        h ^= h >> 12;
        return (h >> 7) & 1 ? kEECPEngineNext : kEECPEngineClassic;
    }
    return inChoice == kEECPEngineNext ? kEECPEngineNext : kEECPEngineClassic;
}

// S0: the stub. The next engine makes no decisions yet: no gathering assignments, building, training, research or
// attacks (the classic managers do not run for it); the units' own reflexes (the unit goals, auto attack) still
// run. Only the 30-second stats line of the trace, so a run shows the idle town.
void EECPNextUpdate(EEComputerPlayer* inPlayer, unsigned long inTime)
{
    EECPTraceStats(inPlayer, inTime);
}
