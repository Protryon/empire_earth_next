// C:\EEX\Empire Earth\Scenario Editor\EESELoadThread.h (RECONSTRUCTION; file name inferred). The
// scenario editor's loading thread (not polymorphic; EESELoadThread.cpp 0x172125..0x172b0f, __FILE__ 0x4c2f70). The
// member names mThread/mThreadCreated/mThreadDone/mMessageQueueDone, the parameter inThreadPriority and
// EESELoadThread::Main come from the ctor's ESVERIFY texts; everything else is named by offset (provisional name).
// The editor holds it at EEUIFScenarioEditor +0x200. Posted messages 0x401..0x40c go to HandleMessage (0x17285c).
// Requires lle/TSThread.h, lle/TSEvent.h, lle/TSScheduler.h, ee/EESEAChangeLoadText.h.
#ifndef EE_EESELOADTHREAD_H
#define EE_EESELOADTHREAD_H
#include "ee/EEScenarioEditorIDs.h"

// 0x52a230 "ScenEdit" (its $E chain 0x13f0ec sits in another unit): the thread's name (provisional name)
extern UString gScenarioEditorThreadName;

class EEUIFScenarioEditor;

class EESELoadThread {
public:
    EESELoadThread(EEUIFScenarioEditor* inEditor, long inThreadPriority);    // 0x172125 (new'd by the editor 0x19f794)
    ~EESELoadThread();                                        // 0x172295
    static unsigned long Main(void* inThis);                  // 0x1727ae (the thread procedure)
    void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2);   // 0x17285c

    // the posting side (one TSThread::Post each; provisional names by message number)
    void Post404();                         // 0x1729bd
    void Post401();                         // 0x1729d2
    void Post40B(UReferenceCounter* inObject);   // 0x1729e7 (references inObject)
    void Post40C(UReferenceCounter* inObject);   // 0x172a04 (references inObject)
    void Post408(void* inParameter);        // 0x172a21
    void Post406(void* inParameter1, void* inParameter2);   // 0x172a3a
    void Post407(void* inParameter1, void* inParameter2);   // ICF-folded into 0x11aa6c (EEAResetAllPlayerInfo)
    void Post403(void* inParameter, bool inFlag);           // 0x172a55
    void Post402(void* inParameter1, void* inParameter2);   // 0x172a72
    void Post405();                         // 0x172a8d
    void Post409(void* inParameter);        // 0x172aa2

    // the handlers (names inferred from the message numbers)
    void Handle406(unsigned long inPlayer, unsigned long inCount);    // 0x1722c6
    void Handle408(unsigned long inArg1, bool inFlag);       // 0x172433 (0x409 passes true)
    void Handle402(unsigned long inUnitClass, unsigned long inPlayer);  // 0x17263f
    void Handle403(unsigned long inPlayer, bool inFlag);       // 0x172967
    void Handle407(unsigned long inPlayer, unsigned long inArg2);  // 0x172abb

    EEUIFScenarioEditor* mEditor;           // +0x00 (the ctor's first argument: the editor; Handle402 reads its mTabGroups[3], mouse location)
    TSThread* mThread;                      // +0x04
    TSEvent* mThreadCreated;                // +0x08
    TSEvent* mThreadDone;                   // +0x0c
    TSEvent* mMessageQueueDone;             // +0x10
    EESEAChangeLoadText* mChangeLoadText;   // +0x14 (new in the ctor; dereferenced by the dtor)
    bool mIsDone;                           // +0x18 the quit flag (Main loops until it is set)
};

#endif
