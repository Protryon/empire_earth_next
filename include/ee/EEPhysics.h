// C:\EEX\Empire Earth\Physics Thread\EEPhysics.h (RECONSTRUCTION; file name inferred) and
// the engine-side base RTSPhysics (C:\EEX\RTS Engine\Physics\RTSPhysics.h). The physics (simulation) thread: RTTI
// EEPhysics > RTSPhysics (vtables 0x442304 / 0x442988). The game's instance is gPhysics (0x5318f0, 465 readers; new
// 0xfc bytes at 0x14392b). Member names from the ctors' asserts ("this->mThreadCreated" ...); the rest are provisional.
// Virtual slots named by offset (arguments by `ret N`). Requires lle/LowLevelEngine.h (URandom, TS*), <vector>.
#ifndef EE_EEPHYSICS_H
#define EE_EEPHYSICS_H

#include <vector>
#include <deque>
#include "rust/sim/units.h"             // EeUnitsPhysicsSaved (EEPhysics' saved block)
#include "rust/game/rts.h"              // EeRtsPhysicsFields (RTSPhysics' plain members)
#include "rust/game/physics.h"

class FSFile;
class RTSGraphics;
class EEGraphics;
class EEUnit;
class EEComplexUnit;
class NEMessage;
class TMTimer;
class TSCriticalSection;
class TSEvent;
class TSThread;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;

// a queued unit replacement (0x1534d1 swaps them in the contagion manager and the effects; provisional names)
struct EEPUnitSwap {
    EEComplexUnit* mOld;                    // +0x00 (GetUnitSwapKind reads mIsCitizen of both)
    EEComplexUnit* mNew;                    // +0x04
};

// LLE round 8: the bodies are Rust (ee_game rts physics.rs); the plain members are its record EeRtsPhysicsFields, a
// base (mL28, mTimerRunning, mSpeed, mTimerInterval, mTicksPerSecond, mTicksRun, mTicksOwed, mDefaultTicksPerSecond,
// mStopThread, mSlowPlayback; the Rust loop reads the volatile ones with volatile accesses).
class RTSPhysics : public EeRtsPhysicsFields {   // RTSPhysics.cpp, ctor 0x161041 (asserts lines 0x31..0x52)
public:
    // 0x161041 (EEPhysics passes its graphics and timer, then 1, 0xf; argument names are provisional)
    RTSPhysics(RTSGraphics* inGraphics, TMTimer* inTimer, long inDefaultTicksPerSecond, long inThreadPriority) : mRandom(1) { ee_rts_physics_construct(this, inGraphics, inTimer, inDefaultTicksPerSecond, inThreadPriority); }
    // the saved physics state (EEPMultiplayerHelper's Load/Save pass their arguments on)
    // +0x00 0x16130d
    virtual long Load(unsigned long, unsigned long, FSFile& inFile) { return ee_rts_physics_load(this, &inFile); }
    virtual void QueueServerMessage(NEMessage* inMessage) = 0;  // +0x04 (EEPhysics queues it in mU04)
    // +0x08 0x1616a4
    virtual long Save(unsigned long, unsigned long, FSFile& outFile) { return ee_rts_physics_save(this, &outFile); }
    virtual void UpdateTiming() = 0;        // +0x0c
    // +0x10 (??_G 0x161204)
    virtual ~RTSPhysics() { ee_rts_physics_destruct(this); }
    virtual void Lock() = 0;                // +0x14
    // the thread's messages (Main passes Receive's three results; both overrides return nothing: `ret 0xc`)
    // +0x18 0x161609
    virtual void HandleMessage(unsigned long inMessage, void*, void*) { ee_rts_physics_handle_message(this, inMessage); }
    virtual void Idle() = 0;                // +0x1c
    virtual void Tick() = 0;                // +0x20
    virtual void Unlock() = 0;              // +0x28

    // Not inline: every GetRandom() site calls it (0x437ab: `add ecx, 0x18; jmp __imp_Generate`, the ICF representative
    // of an identical AI-unit function), even the 0x10a-byte earthquake ShakeCamera; the inline-looking draws are
    // `gPhysics->mRandom.Generate()` written directly (the meteor storm's Update keeps it in a local: exact; Leprosy's
    // ShowEffects has one of each). Its own body belongs to RTSPhysics.cpp. Names are provisional
    float GetRandom() { return this->mRandom.Generate(); }
    // 0x1612b9: inMinimum + (inMaximum - inMinimum) * a draw. Not inline: it sits in RTSPhysics.cpp although its first
    // caller is 0x66ff (an inline COMDAT would follow its first user); the meteor storm calls it
    float GetRandom(float inMinimum, float inMaximum) { return this->mRandom.Generate() * (inMaximum - inMinimum) + inMinimum; }
    // 0x1612d3: an integer in [inMinimum, inMaximum] from the same draw (the group messages pick a unit)
    long GetRandom(long inMinimum, long inMaximum) { return ee_rts_physics_random_range(this, inMinimum, inMaximum); }
    // 0x161774 (EEMRecordFrames passes mF50; provisional name)
    void SetSpeed(float inSpeed) { ee_rts_physics_set_speed(this, inSpeed); }
    // inline: EEMRecordFrames stores +0x69 after it has pushed mF50 from the same `this` (provisional name)
    void SetSlowPlayback(bool inValue) { this->mSlowPlayback = inValue; this->SetSpeed(this->mSpeed); }

    // +0x04 (EEPhysics::QueueServerMessage pushes, ClearServerMessages pops and dereferences; push_back 0xa96fd, pop_back 0xa96d4, back()
    // through the out-of-line iterator operator* 0x3f7cc with the 1024-element blocks: a deque, whose _Mysize is the
    // +0x14 the loop tests
    std::deque<NEMessage*> mServerMessages;
    URandom mRandom;                        // +0x18 (seed 1; provisional name)
    RTSGraphics* mGraphics;                 // +0x30 (the ctor's first argument; Main sets its +0x24 event)
    TSThread* mThread;                      // +0x34 (CreateThread(this->mThread, RTSPhysics::Main, this); the game
                                            // posts 0x502/0x503 through it: EEMToggleDayNight/EEMSnapToDaylight)
    TSEvent* mThreadCreated;                // +0x38
    TSEvent* mStartThreadMainLoop;          // +0x3c
    TSEvent* mThreadDone;                   // +0x40
    TSEvent* mTimerTick;                    // +0x44
    TMTimer* mTimer;                        // +0x48 (a ctor argument; provisional name)
    TSCriticalSection* mServerMessageQueueLock;   // +0x4c
};

// New (R5b.4, round 28): the plain block +0x080 .. +0x0e8 (the clocks, tick counters, timing averages and the civ
// timer's flags; names, types and order kept) is the Rust record EeUnitsPhysicsSaved (ee_units physics_save.rs), the
// second base; Load and Save are Rust over it.
class EEPhysics : public RTSPhysics, public EeUnitsPhysicsSaved {       // sizeof 0xfc; EEPhysics.cpp (__FILE__ 0x151cb4), ctor 0x151c5a
public:
    EEPhysics(EEGraphics* inGraphics, TMTimer* inTimer);   // 0x151c5a (new 0xfc at 0x14392b)
    // +0x00 0x152a4d
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_physics_load(this, &inFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion); }
    // +0x04 0x152f35 (mU04.push_back)
    virtual void QueueServerMessage(NEMessage* inMessage) { this->mServerMessages.push_back(inMessage); }
    // +0x08 0x153950
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_physics_save(this, &outFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion); }
    // +0x0c 0x153bd8
    virtual void UpdateTiming() { ee_physics_update_timing(this); }
    virtual ~EEPhysics();                   // +0x10 (??_G 0x151cd4)
    virtual void Lock();                    // +0x14 0x152f29
    // +0x18 0x152f45
    virtual void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2) { ee_physics_handle_message(this, (uint32_t)inMessage, inParameter1, inParameter2); }
    // +0x1c 0x15305a
    virtual void Idle() { ee_physics_idle(this); }
    // +0x20 0x153082
    virtual void Tick();                    // +0x20 0x153082 (out of line: the test profile's harness hook)
    virtual void Unlock();                  // +0x28 0x153c4d
    // 0x151e4d (EEDataPreLoading::LoadGame, under gWorld's lock; provisional name)
    void ClearServerMessages() { ee_physics_clear_server_messages(this); }
    // 0x151f3a: the game is over (every player's result, the campaign progress, the main game form's blinking
    // button, the teams); the cheat codes 0x12bb7b/0x12bbad and Virtual20 call it (provisional name)
    void EndGame(bool inIsVictory) { ee_physics_end_game(this, inIsVictory); }

    // (provisional names)
    // 0x151efe (runs Virtual20 while mUB8 is set; DrainInput first)
    void RunOwedTicks() { ee_physics_run_owed_ticks(this); }
    // 0x15268d: inTime (0: mLD0) as whole minutes and the seconds left over
    void GetMinutesAndSeconds(unsigned long* outMinutes, unsigned long* outSeconds, unsigned long inTime) { ee_physics_minutes_and_seconds(this, outMinutes, outSeconds, inTime); }
    // 0x152909 (the ctor and message 0x509: the counters, the queues)
    void Init() { ee_physics_init(this); }
    // 0x153c23 (both referenced; processed by 0x1534d1)
    void QueueUnitSwap(EEComplexUnit* inOld, EEComplexUnit* inNew) { ee_physics_queue_unit_swap(this, inOld, inNew); }
    // 0x153836 (Virtual20's; provisional name)
    void ReportStatistics();                // 0x153836 (out of line: the test profile's harness hook)
    // the thread messages (provisional names)
    long PostShowDialogMessage() { return ee_physics_post(this, 0x508); }
    long PostInit() { return ee_physics_post(this, 0x509); }
    // inline (COMDAT 0x761b5, first emitted by EECPCombatManager.cpp): the queued unit swaps (mSound2Index7.size())
    // (provisional name)
    // 0x1526d2 (ret 4): the unit's queued swap kind (0: none; 1, 5, 7) (provisional name)
    long GetUnitSwapKind(EEComplexUnit* inUnit) { return ee_physics_unit_swap_kind(this, inUnit); }
    // (provisional names): 0x1520cf the out-of-synch checksum (inDump: also the "OOS <date>.log" dump)
    // 0x15253e the elapsed game time as text; 0x153430 one queued swap (camera, contagion, scenario, effects);
    // 0x153ca1 the civ timers; 0x153dff the queued load/save messages; 0x15401e the collision period (returns the
    // count of gWorld's +0xadc deque); 0x1540fb the day/night and wind
    unsigned long GetOutOfSynchChecksum(bool inDump, unsigned long inFlags) { return ee_physics_out_of_synch_checksum(this, inDump, inFlags); }
    UWideString GetTimeString();

    std::vector<NEMessage*> mLoadSaveRequests;  // +0x06c (queued under mSaveGameLock, referenced; Init dereferences them)
    TSCriticalSection* mSaveGameLock;       // +0x07c
    // +0x080 mCurrentTick .. +0x0e8 mResourceTimerPending: the record EeUnitsPhysicsSaved (the base above)
    float GetTickSeconds() const { return this->mTickSeconds; }  // inline (EEWheel::Update 0x23c99e loads it as a value) (provisional name)
    float GetGameClock() const { return this->mGameClock; }
    float GetTickTime() const { return this->mTickTime; }
    long GetGameTime() const { return this->mGameTime; }
    std::vector<EEPUnitSwap> mUnitSwaps;    // +0x0ec (QueueUnitSwap; the `_Buy` is vector<EETileCoordinate>'s ICF twin)
    // New members (R5a.5): the physics totals that were the globals 0x531cf0 / 0x531cf4 (gU531CF0 / gU531CF4)
    long mInputMessagesProcessed;           // the user-input messages executed (Init clears it; part of the out-of-synch checksum)
    unsigned long mLastOutOfSynchChecksum;  // GetOutOfSynchChecksum's last result (written, never read)
};

extern EEPhysics* gPhysics;
extern UString gPhysicsThreadName;          // 0x52a270 (RTSPhysics's thread name)
// port, TEST ONLY: the unit EEPhysics::UpdateUnits is stepping (NULL outside the loop); EE-SIM's --rng-trace names it
extern EEComplexUnit* gPhysicsSteppingUnit;
// port, TEST ONLY: called by the headless simulation's loop (RTSPhysics::Main) after every physics tick, under the
// world's write lock (NULL in the game); EE-SIM's --dump-tick
extern void (*gPhysicsAfterTickHook)(void);

#endif
