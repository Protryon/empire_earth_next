// C:\EEX\Empire Earth\Scenario Editor\EESELoadThread.h (RECONSTRUCTION; file name inferred). The
// scenario editor's loading thread (not polymorphic; EESELoadThread.cpp 0x172125..0x172b0f, __FILE__ 0x4c2f70). The
// member names mThread/mThreadCreated/mThreadDone/mMessageQueueDone, the parameter inThreadPriority and
// EESELoadThread::Main come from the ctor's ESVERIFY texts; everything else is named by offset (provisional name).
// The editor holds it at EEUIFScenarioEditor +0x200. Posted messages 0x401..0x40c go to HandleMessage (0x17285c).
// Requires lle/TSScheduler.h (the threads, the events) (its EESEAChangeLoadText: ee/EEARust.h). The posts
// are inline (one TSThread::Post each): out of line, the game's actions named the editor's load thread for three of
// them (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts, round 3").
#ifndef EE_EESELOADTHREAD_H
#define EE_EESELOADTHREAD_H
#include "rust/game/screens.h"

class EESEAChangeLoadText;

// 0x52a230 "ScenEdit" (its $E chain 0x13f0ec sits in another unit): the thread's name (provisional name)
extern UString gScenarioEditorThreadName;

class EEUIFScenarioEditor;

class EESELoadThread {
public:
    // 0x172125 (new'd by the editor 0x19f794)
    EESELoadThread(EEUIFScenarioEditor* inEditor, long inThreadPriority) { ee_screens_selt_construct(this, inEditor, inThreadPriority); }
    ~EESELoadThread() { ee_screens_selt_destroy(this); }
    // 0x1727ae (the thread procedure)
    static unsigned long Main(void* inThis) { return ee_screens_selt_main(inThis); }
    void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2) { ee_screens_selt_message(this, inMessage, inParameter1, inParameter2); }

    // the posting side (one TSThread::Post each; provisional names by message number)
    void Post404() { this->mThread->Post(0x404, NULL, NULL, true); }   // start the scenario test and finish
    void Post401() { this->mThread->Post(kThreadMessageQuit, NULL, NULL, true); }   // leave the editor and quit
    // (0x1729e7 Post40B, a UI event to post, and 0x172a04 Post40C, a loaded object to display, each keeping a
    // reference: composed in Rust, editor_sefmx.rs EE_SCREENS_SEFMX_REFERENCE + _THREAD_POST)
    // a terrain type's media to load
    void Post408(void* inParameter) { this->mThread->Post(0x408, inParameter, NULL, true); }
    // a player's epoch media to load
    void Post406(void* inParameter1, void* inParameter2) { this->mThread->Post(0x406, inParameter1, inParameter2, true); }
    // a player's start epoch to apply (ICF-folded into 0x11aa6c, EEAResetAllPlayerInfo, with RTSGraphics::PostSetRasterizer)
    void Post407(void* inParameter1, void* inParameter2) { this->mThread->Post(0x407, inParameter1, inParameter2, true); }
    // a player's tech tree to reload
    void Post403(void* inParameter, bool inFlag) { this->mThread->Post(0x403, inParameter, (void*)inFlag, true); }
    // a unit class's placement ghost to create for a player
    void Post402(void* inParameter1, void* inParameter2) { this->mThread->Post(0x402, inParameter1, inParameter2, true); }
    void Post405() { this->mThread->Post(0x405, NULL, NULL, true); }   // start the scenario test (second variant)
    // a terrain type's media to unload
    void Post409(void* inParameter) { this->mThread->Post(0x409, inParameter, NULL, true); }

    // (the handlers Handle402..Handle408 and the bodies: ee_game screens editor_selt.rs)

    EEUIFScenarioEditor* mEditor;           // +0x00 (the ctor's first argument: the editor; Handle402 reads its mTabGroups[3], mouse location)
    TSThread* mThread;                      // +0x04
    TSEvent* mThreadCreated;                // +0x08
    TSEvent* mThreadDone;                   // +0x0c
    TSEvent* mMessageQueueDone;             // +0x10
    EESEAChangeLoadText* mChangeLoadText;   // +0x14 (new in the ctor; dereferenced by the dtor)
    bool mIsDone;                           // +0x18 the quit flag (Main loops until it is set)
};

#endif
