// Platform layer: the switches of the headless simulation (EE-SIM.exe, src/Empire Earth/Empire Earth/EESim.cpp).
// TEST ONLY. The simulation runs the game's own code with its front end stubbed: no visible window, no rendering, no
// sound, the menus driven in-process, and the physics thread producing its own communications ticks back to back
// instead of waiting for the wall clock. Everything here is off (zero) in EE-AOC.exe; the game's units test the
// flags at the few places the simulation differs.
#ifndef PLATFORM_PLATSIM_H
#define PLATFORM_PLATSIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// nonzero in EE-SIM.exe (set before the engine starts):
// - the game window is created hidden (DX7Screen::CreateAWindow);
// - the graphics thread renders nothing (RTSGraphics::Main) and the mixer opens the null audio device;
// - once the local server's game runs, its send thread sends no communications ticks of its own; the physics thread
//   sends the next one (NESynchronousServer::SimCommunicationsTick) as soon as it has run the last, without waiting
//   for its timer (RTSPhysics::Main), while gPlatSimRun is set;
// - message boxes go to stderr.
extern int gPlatSimMode;
// the physics thread runs ticks only while this is set (the driver sets it once the game is ready; clearing it holds
// the game between communications ticks)
extern volatile int gPlatSimRun;
// called on the physics thread after every communications tick's physics ticks, under the world's write lock
extern void (*gPlatSimAfterTick)(void);
// the start seed: EEMStartLoading::Execute takes it instead of the lobby's rand() when gPlatSimSeedSet (the harness's
// seed pin, game_session.py --seed)
extern int gPlatSimSeedSet;
extern uint32_t gPlatSimSeed;
// the idle local human is never taken out of the game nor defeated (the harness's --no-human-defeat):
// EEWorld::RemovePlayerFromGame(false, local player) and EEPlayer::SetGameResult(2) on the local player do nothing
extern int gPlatSimNoHumanDefeat;
// added to the TCP protocol's fixed ports 33334/33335 (0 in the game), so simulations do not share them
extern int gPlatSimPortOffset;
extern volatile long gPlatSimNoHumanDefeatHits;

// The harness clock (TEST ONLY; ee_plat_core sim_clock). With a factor set, the clocks of platform/PlatTime.h
// (PlatTimeMilliseconds, PlatTimeMilliseconds64, PlatCounterTicks) run num/den times real time, and every timed wait
// of the platform layer (PlatSleep, the system timers' intervals, the finite timeouts of PlatEventWait,
// PlatEventWaitAny and PlatThreadWait) lasts den/num of its real time. The clocks go on from their values at the
// change (no jump). Wall-clock time is never scaled. The test fixture sets it at start-up from EE_TEST_TIME_FACTOR
// (EETestFixture.cpp; tools/harness/fixture_game.py --time-factor), in place of the time shim's import hooks.
// -> nonzero on success; 0 for a factor outside 1/1000..4096 (nothing changes then).
int PlatSimSetTimeFactor(uint32_t inNum, uint32_t inDen);
// the factor in force (1/1 when none was set)
void PlatSimGetTimeFactor(uint32_t* outNum, uint32_t* outDen);

// Where PlatDebugOutput's lines go: a handler instead of the debugger / stderr (NULL: the default)
typedef void (*PlatDebugHandler)(const char* inText);
void PlatDebugSetHandler(PlatDebugHandler inHandler);
PlatDebugHandler PlatDebugGetHandler(void);

#ifdef __cplusplus
}
#endif

#endif
