// Platform layer: the headless simulation's switches (include/platform/PlatSim.h). TEST ONLY; all off in EE-AOC.exe.
#include <atomic>

#include "platform/PlatSim.h"

extern "C" {

int gPlatSimMode = 0;
volatile int gPlatSimRun = 0;
void (*gPlatSimAfterTick)(void) = 0;
int gPlatSimSeedSet = 0;
uint32_t gPlatSimSeed = 0;
int gPlatSimNoHumanDefeat = 0;
int gPlatSimPortOffset = 0;
volatile long gPlatSimNoHumanDefeatHits = 0;

}

static std::atomic<PlatDebugHandler> sDebugHandler{nullptr};
static std::atomic<long> sLobbyPosts{0}, sLobbyApplies{0};

extern "C" void PlatSimLobbyPosted(void)
{
    if (gPlatSimMode)
        sLobbyPosts++;
}

extern "C" void PlatSimLobbyApplied(void)
{
    if (gPlatSimMode)
        sLobbyApplies++;
}

extern "C" long PlatSimLobbyPosts(void)
{
    return sLobbyPosts.load();
}

extern "C" long PlatSimLobbyApplies(void)
{
    return sLobbyApplies.load();
}

extern "C" void PlatDebugSetHandler(PlatDebugHandler inHandler)
{
    sDebugHandler.store(inHandler);
}

extern "C" PlatDebugHandler PlatDebugGetHandler(void)
{
    return sDebugHandler.load();
}
