// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEDataPreLoading.h (RECONSTRUCTION; names provisional unless an assert
// names them). Layout proven: EEDataPreLoading sizeof 0x28 = its new-site size, ctor 0xd3ff4 exact in
// EEDataPreLoading.cpp. The data pre-loading thread (RTTI EEDataPreLoading, no base; vtable 0x43e4e4: the deleting dtor
// only). Its thread receives the messages the Post* members send and loads or unloads unit-class and
// technology media, or a whole game; while queueing is on, messages wait in a queue. The bodies are Rust (ee_game
// preload.rs, world lane round 1), with the queue and its two flags. Defined by EEDataPreLoading.cpp
// 0xd3ff4..0xd47a1 (the ctor's asserts name mThreadCreated, mThreadDone, mMessageQueueDone, mThread, Main and
// inThreadPriority). Requires <vector>.
#ifndef EE_EEDATAPRELOADING_H
#define EE_EEDATAPRELOADING_H

class EEPlayer;
class EETechTreeNode;
class EEUnitClass;
class TSEvent;
class TSThread;
class UIForm;

class EEDataPreLoading {
public:
    void Shutdown();                        // COMDAT 0x1444ac: mThread->Post(0x401), mThreadDone->Wait (inline: defined in Empire Earth.cpp, its only user)
    EEDataPreLoading(long inThreadPriority);
    virtual ~EEDataPreLoading();                            // +0x00 0xd4121 (??_G 0xd4105)

    static unsigned long Main(void* inThis);                // 0xd4176 (the thread function)
    void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2);
    static void LoadGameErrorDone(UIForm* inForm);          // 0xd4613 (LoadGame's error box callback)
    void StartQueueing();                                   // 0xd4621 (message 0x407)
    void FlushQueue();                                      // 0xd4626 (message 0x408)
    void QueueMessage(unsigned long inMessage, void* inParameter1, void* inParameter2);
    void PostLoadGame(bool inSkipGameMedia, bool inClearWorld);  // 0xd46a9 (0x402)
    void PostLoadTechnology(long inPlayer, long inTechnology);    // 0xd46d2 (0x40a)
    void PostLoadUnitClass(long inPlayer, long inUnitClass);      // 0xd46ed (0x404)
    void PostUnloadTechnology(long inPlayer, long inTechnology);  // 0xd4708 (0x40b)
    void PostUnloadUnitClass(long inPlayer, long inUnitClass);    // 0xd4723 (0x409)
    long WaitForMessageQueue();                             // 0xd473e (0x40c, then mMessageQueueDone)

    TSThread* mThread;                                      // +0x04
    TSEvent* mThreadCreated;                                // +0x08
    TSEvent* mThreadDone;                                   // +0x0c
    TSEvent* mMessageQueueDone;                             // +0x10
    // (+0x14 mQueue, +0x24 mQuit, +0x25 mQueueing: the Rust pre-loader's, ee_game preload.rs)
};

extern EEDataPreLoading* gDataPreLoading;   // 0x531900 (EEMLoadUnitData posts through it; provisional name)

#endif
