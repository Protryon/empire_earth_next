// C:\EEX\Empire Earth\Empire Earth\EEMusicManager.h (RECONSTRUCTION; file name inferred; layout proven: EEMusicManager
// sizeof 0x54 = its new-site size, ctor 0x131bde exact in EEMusicManager.cpp; member names are provisional). The music
// thread (RTTI EEMusicManager, no base; vtable 0x441fbc: the deleting dtor only). EEMusicManager.cpp 0x131b7c..0x13242b
// (__FILE__ 0x131bde: the ctor's asserts name the events, mThread and MusicManagerMain). The thread waits on five
// events (exit, pause, intensity, start, stop) or the time to
// the next song, and plays EEMusicObjects of the current player's epoch on its conductor. The game's instance is
// gMusicManager (0x531904). Names are provisional unless an assert names them. Requires lle/URandom.h.
#ifndef EE_EEMUSICMANAGER_H
#define EE_EEMUSICMANAGER_H

class EEMusicObject;
class SEConductor;
class TSEvent;
class TSThread;

class EEMusicManager {
public:
    EEMusicManager(SEConductor* inConductor, long inThreadPriority);    // 0x131bde
    virtual ~EEMusicManager();              // +0x00 0x131db4 (??_G 0x131d98)

    static unsigned long MusicManagerMain(void* inThis);    // 0x131e0a (the thread function)
    void Pause();                           // 0x131f67: mPause->Set()
    void SetIntensity(unsigned long inIntensity);   // 0x131f87 (then mSetIntensity->Set())
    void Start();                           // 0x132000: mStart->Set() (the settings turn the music on)
    void Stop();                            // 0x1321b1: mStop->Set() (the settings turn the music off)
    void Update(unsigned long inElapsedTime);   // 0x132410

    URandom mRandom;                        // +0x04 (seeded with TMGetTime())
    SEConductor* mConductor;                // +0x14
    TSThread* mThread;                      // +0x18
    TSEvent* mThreadCreated;                // +0x1c (the ctor's CreateEvent asserts name the events)
    TSEvent* mThreadDone;                   // +0x20
    TSEvent* mExit;                         // +0x24
    TSEvent* mPause;                        // +0x28
    TSEvent* mSetIntensity;                 // +0x2c
    TSEvent* mStart;                        // +0x30
    TSEvent* mStop;                         // +0x34
    EEMusicObject* mMusic;                  // +0x38 the song playing
    long mScoreID;                          // +0x3c its conductor score
    long mState;                            // +0x40 0 stopped, 1 paused, 2 playing
    unsigned long mIntensityDelay;          // +0x44 (5000 ms before an intensity may drop)
    unsigned long mIntensity;               // +0x48
    volatile unsigned long mTimeToNextSong; // +0x4c (the wait's timeout; 600000 at first; volatile: Update re-reads it)
    unsigned long mRequestedIntensity;      // +0x50
};

extern EEMusicManager* gMusicManager;       // 0x531904

#endif
