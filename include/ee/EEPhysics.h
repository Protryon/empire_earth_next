// C:\EEX\Empire Earth\Physics Thread\EEPhysics.h (RECONSTRUCTION; file name inferred) and
// the engine-side base RTSPhysics (C:\EEX\RTS Engine\Physics\RTSPhysics.h). The physics (simulation) thread: RTTI
// EEPhysics > RTSPhysics (vtables 0x442304 / 0x442988). The game's instance is gPhysics (0x5318f0, 465 readers; new
// 0xfc bytes at 0x14392b). Member names from the ctors' asserts ("this->mThreadCreated" ...); the rest are provisional.
// Virtual slots named by offset (arguments by `ret N`). Requires lle/LowLevelEngine.h (URandom, TS*), <vector>.
#ifndef EE_EEPHYSICS_H
#define EE_EEPHYSICS_H

#include <vector>
#include <deque>

class FSFile;
class RTSGraphics;
class EEGraphics;
class EEUnit;
class EEComplexUnit;
class EEPlayer;
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

class RTSPhysics {                          // sizeof 0x6c; RTSPhysics.cpp, ctor 0x161041 (asserts lines 0x31..0x52)
public:
    void Shutdown();                        // COMDAT 0x144488: mThread->Post(0x401), mThreadDone->Wait (inline: defined in Empire Earth.cpp, its only user)
    // 0x161041 (EEPhysics passes its graphics and timer, then 1, 0xf; argument names are provisional)
    RTSPhysics(RTSGraphics* inGraphics, TMTimer* inTimer, long inDefaultTicksPerSecond, long inArg4);
    // the saved physics state (EEPMultiplayerHelper's Load/Save pass their arguments on)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x00 0x16130d
    virtual void QueueServerMessage(NEMessage* inMessage) = 0;  // +0x04 (EEPhysics queues it in mU04)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x08 0x1616a4
    virtual void UpdateTiming() = 0;        // +0x0c
    virtual ~RTSPhysics();                  // +0x10 (??_G 0x161204)
    virtual void Lock() = 0;                // +0x14
    // the thread's messages (Main passes Receive's three results; both overrides return nothing: `ret 0xc`)
    virtual void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2);  // +0x18 0x161609
    virtual void Idle() = 0;                // +0x1c
    virtual void Tick() = 0;                // +0x20
    virtual void CreateGameTimer() = 0;     // +0x24
    virtual void Unlock() = 0;              // +0x28

    // Not inline: every GetRandom() site calls it (0x437ab: `add ecx, 0x18; jmp __imp_Generate`, the ICF representative
    // of an identical AI-unit function), even the 0x10a-byte earthquake ShakeCamera; the inline-looking draws are
    // `gPhysics->mRandom.Generate()` written directly (the meteor storm's Update keeps it in a local: exact; Leprosy's
    // ShowEffects has one of each). Its own body belongs to RTSPhysics.cpp. Names are provisional
    float GetRandom();
    // 0x1612b9: inMinimum + (inMaximum - inMinimum) * a draw. Not inline: it sits in RTSPhysics.cpp although its first
    // caller is 0x66ff (an inline COMDAT would follow its first user); the meteor storm calls it
    float GetRandom(float inMinimum, float inMaximum);
    // 0x1612d3: an integer in [inMinimum, inMaximum] from the same draw (the group messages pick a unit)
    long GetRandom(long inMinimum, long inMaximum);
    // 0x82d66 (ret 4): a random index into inValues, GetRandom(0, size - 1). A member template: each TU emits its own
    // COMDAT (the kept copy is EECPIntelligence's 0x82d66, ICF-folding the EEComplexUnit*/long/EEPlayer*
    // instantiations; callers EECPCivilizationManager 0x711ac, EECPDefenseCommander 0x78148, EEHerd) (provisional name)
    template <class T> long Fn82D66(std::vector<T>& inValues) { return this->GetRandom(0L, (long)inValues.size() - 1); }
    // template COMDAT 0xa52eb (EEHerd.cpp's range, ret 4): GetRandom(0, size - 1) of a vector of EEFormationSlot
    // (Fn82D66's twin for 0x14-byte elements) (provisional name)
    template <class V> long GetRandomIndex(V& inValues) { return this->GetRandom(0L, (long)inValues.size() - 1); }
    void SetSpeed(float inSpeed);           // 0x161774 (EEMRecordFrames passes mF50; provisional name)
    // inline: EEMRecordFrames stores +0x69 after it has pushed mF50 from the same `this` (provisional name)
    void SetSlowPlayback(bool inValue) { this->mSlowPlayback = inValue; this->SetSpeed(this->mSpeed); }
    static unsigned long Main(void* inPointer);   // 0x16140c (the thread; named by the ctor's CreateThread assert)

    // +0x04 (EEPhysics::QueueServerMessage pushes, ClearServerMessages pops and dereferences; push_back 0xa96fd, pop_back 0xa96d4, back()
    // through the out-of-line iterator operator* 0x3f7cc with the 1024-element blocks: a deque, whose _Mysize is the
    // +0x14 the loop tests
    std::deque<NEMessage*> mServerMessages;
    URandom mRandom;                        // +0x18 (seed 1; provisional name)
    unsigned long mL28;                     // +0x28 (= 0; unsigned: Load/Save use the unsigned long overloads)
    volatile bool mTimerRunning;            // +0x2c (= false; volatile: `mov byte` stores, Save copies it to a local)
    RTSGraphics* mGraphics;                 // +0x30 (the ctor's first argument; Main sets its +0x24 event)
    TSThread* mThread;                      // +0x34 (CreateThread(this->mThread, RTSPhysics::Main, this); the game
                                            // posts 0x502/0x503 through it: EEMToggleDayNight/EEMSnapToDaylight)
    TSEvent* mThreadCreated;                // +0x38
    TSEvent* mStartThreadMainLoop;          // +0x3c
    TSEvent* mThreadDone;                   // +0x40
    TSEvent* mTimerTick;                    // +0x44
    TMTimer* mTimer;                        // +0x48 (a ctor argument; provisional name)
    TSCriticalSection* mServerMessageQueueLock;   // +0x4c
    float mSpeed;                           // +0x50 (= 1)
    unsigned long mTimerInterval;           // +0x54 (= 1000: the timer interval)
    unsigned long mTicksPerSecond;          // +0x58 (= 1; Virtual0C converts it unsigned: fild qword)
    unsigned long mTicksRun;                // +0x5c (unsigned: Load/Save)
    volatile unsigned long mTicksOwed;      // +0x60 (volatile: Load's `mov dword [x], 0`, Main's load/inc/store; unsigned: jae/jbe)
    unsigned long mDefaultTicksPerSecond;   // +0x64 (a ctor argument; unsigned: Virtual18's `fild qword`)
    volatile bool mStopThread;              // +0x68 (volatile: Save/Load copy it through a local)
    bool mSlowPlayback;                     // +0x69 (= true before CreateThread)
};

class EEPhysics : public RTSPhysics {       // sizeof 0xfc; EEPhysics.cpp (__FILE__ 0x151cb4), ctor 0x151c5a
public:
    EEPhysics(EEGraphics* inGraphics, TMTimer* inTimer);   // 0x151c5a (new 0xfc at 0x14392b)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x00 0x152a4d
    virtual void QueueServerMessage(NEMessage* inMessage);  // +0x04 0x152f35 (mU04.push_back)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x08 0x153950
    virtual void UpdateTiming();            // +0x0c 0x153bd8
    virtual ~EEPhysics();                   // +0x10 (??_G 0x151cd4)
    virtual void Lock();                    // +0x14 0x152f29
    virtual void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2);  // +0x18 0x152f45
    virtual void Idle();                    // +0x1c 0x15305a
    virtual void Tick();                    // +0x20 0x153082
    virtual void CreateGameTimer();         // +0x24 0x15394b
    virtual void Unlock();                  // +0x28 0x153c4d
    void ClearServerMessages();             // 0x151e4d (EEDataPreLoading::LoadGame, under gWorld's lock; provisional name)
    void AutoSave();                        // 0x151d1f (Virtual18's message 0x50a: an EEMLoadOrSaveGame of "AutoSave <date>"; provisional name)
    // 0x151f3a: the game is over (every player's result, the campaign progress, the main game form's blinking
    // button, the teams); the cheat codes 0x12bb7b/0x12bbad and Virtual20 call it (provisional name)
    void EndGame(bool inIsVictory);

    void FlushWorld();                      // 0x151e9f (after EEMReplantFarms; provisional name)
    // (provisional names)
    void RunOwedTicks();                    // 0x151efe (runs Virtual20 while mUB8 is set; DrainInput first)
    // 0x15268d: inTime (0: mLD0) as whole minutes and the seconds left over
    void GetMinutesAndSeconds(unsigned long* outMinutes, unsigned long* outSeconds, unsigned long inTime);
    void Init();                            // 0x152909 (the ctor and message 0x509: the counters, the queues)
    void QueueMessage(NEMessage* inMessage);   // 0x153403 (under mSaveGameLock; referenced)
    void QueueUnitSwap(EEComplexUnit* inOld, EEComplexUnit* inNew);   // 0x153c23 (both referenced; processed by 0x1534d1)
    void UpdateAreaEffectsClock();          // 0x153c59 (mF9C += mF98; every 2000: gAEManager)
    void DrainInput();                      // 0x15355c
    void CheckShiftMode();                  // 0x153f47 (Virtual20's; provisional name)
    void ReportStatistics();                // 0x153836 (Virtual20's; provisional name)
    void ProcessUnitSwaps();                // 0x1534d1 (the queued unit swaps: every player's Virtual9C, then each swap)
    // the thread messages (provisional names)
    long PostAutoSave();                    // 0x153b8d
    long PostShowDialogMessage();           // 0x153ba6
    long PostInit();                        // 0x153bbf
    // inline (COMDAT 0x761b5, first emitted by EECPCombatManager.cpp): the queued unit swaps (mSound2Index7.size())
    // (provisional name)
    unsigned long GetNumUnitSwaps() const { return this->mUnitSwaps.size(); }
    long GetUnitSwapKind(EEComplexUnit* inUnit);  // 0x1526d2 (ret 4): the unit's queued swap kind (0: none; 1, 5, 7) (provisional name)
    // (provisional names): 0x1520cf the out-of-synch checksum (inDump: also the "OOS <date>.log" dump)
    // 0x15253e the elapsed game time as text; 0x153430 one queued swap (camera, contagion, scenario, effects);
    // 0x153ca1 the civ timers; 0x153dff the queued load/save messages; 0x15401e the collision period (returns the
    // count of gWorld's +0xadc deque); 0x1540fb the day/night and wind
    unsigned long GetOutOfSynchChecksum(bool inDump, unsigned long inFlags);
    UWideString GetTimeString();
    void SwapUnit(EEUnit* inOld, EEUnit* inNew);
    void UpdateCivTimer();
    void ServeLoadSaveRequests();
    unsigned long UpdateUnits();
    void UpdateWeather();

    std::vector<NEMessage*> mLoadSaveRequests;  // +0x06c (queued under mSaveGameLock, referenced; Init dereferences them)
    TSCriticalSection* mSaveGameLock;       // +0x07c
    unsigned long mCurrentTick;             // +0x080 (saved)
    float mF84;                             // +0x084 (saved; EEMEdictProposal clears it)
    float mTicksPerGameSecond;              // +0x088 (saved)
    float GetTickSeconds() const { return this->mTickSeconds; }  // inline (EEWheel::Update 0x23c99e loads it as a value) (provisional name)
    float mTickSeconds;                     // +0x08c (saved)
    float mTimeOfDayStep;                   // +0x090 (saved)
    // inline (EEE2DParticle::Create copies it into the particle through the FP stack) (provisional name)
    float GetGameClock() const { return this->mGameClock; }
    float mGameClock;                       // +0x094 (saved)
    // inline: the 3D effects age by it loaded as a value (EEE3DBlastWave::Step 0xe943f: fld, then fidiv the
    // duration) (provisional name)
    float GetTickTime() const { return this->mTickTime; }
    float mTickTime;                        // +0x098 (saved)
    float mAreaEffectsClock;                // +0x09c (saved)
    float mShiftModeClock;                  // +0x0a0
    long mTickTimeMs;                       // +0x0a4
    long mTickStartTime;                    // +0x0a8 (saved)
    long mPreviousTickStartTime;            // +0x0ac (saved)
    long mTickRandom;                       // +0x0b0
    unsigned long mNumTicksDone;            // +0x0b4 (saved)
    unsigned long mTicksToProcess;          // +0x0b8 (saved)
    unsigned long mTickGapSum;              // +0x0bc (saved)
    unsigned long mTickGapCount;            // +0x0c0 (saved)
    unsigned long mTickWorkSum;             // +0x0c4 (saved)
    unsigned long mTickWorkCount;           // +0x0c8 (saved)
    long mAutoSaveClock;                    // +0x0cc (untouched by the ctor, its init 0x152909 and Load/Save)
    long GetGameTime() const { return this->mGameTime; }
    long mGameTime;                         // +0x0d0
    long mLD4;                              // +0x0d4
    long mAveragePhysicsUpdateTime;         // +0x0d8
    long mAverageTimeBetweenPhysicsTicks;   // +0x0dc
    bool mTimeOfDayRuns;                    // +0x0e0 (saved)
    bool mGameStarted;                      // +0x0e1
    bool mBE2;                              // +0x0e2
    bool mCivTimerRuns;                     // +0x0e3 (saved)
    bool mCivTimerFirstWarning;             // +0x0e4 (saved)
    bool mCivTimerSecondWarning;            // +0x0e5 (saved)
    bool mGameOver;                         // +0x0e6 (saved)
    bool mBE7;                              // +0x0e7
    bool mResourceTimerPending;             // +0x0e8 (saved)
    std::vector<EEPUnitSwap> mUnitSwaps;    // +0x0ec (QueueUnitSwap; the `_Buy` is vector<EETileCoordinate>'s ICF twin)
};

extern EEPhysics* gPhysics;                 // 0x5318f0 (provisional name)
extern UString gPhysicsThreadName;          // 0x52a270 (RTSPhysics's thread name)

#endif
