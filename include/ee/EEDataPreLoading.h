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

#include "rust/game/preload.h"
class TSEvent;
class TSThread;
class UIForm;

class EEDataPreLoading {
public:
    EEDataPreLoading(long inThreadPriority) { ee_preload_construct(this, inThreadPriority); }
    // +0x00 0xd4121 (??_G 0xd4105)
    virtual ~EEDataPreLoading() { ee_preload_destruct(this); }

    // 0xd4176 (the thread function)
    static unsigned long Main(void* inThis) { return ee_preload_main(inThis); }
    void HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2) { ee_preload_handle_message(this, (uint32_t)inMessage, inParameter1, inParameter2); }
    static void LoadGameErrorDone(UIForm* inForm);          // 0xd4613 (LoadGame's error box callback)
    // 0xd46a9 (0x402)
    void PostLoadGame(bool inSkipGameMedia, bool inClearWorld) { this->mThread->Post(0x402, (void*)(inSkipGameMedia == true), (void*)(inClearWorld == true), true); }
    // 0xd46d2 (0x40a)
    void PostLoadTechnology(long inPlayer, long inTechnology) { this->mThread->Post(0x40a, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inTechnology), true); }
    // 0xd46ed (0x404)
    void PostLoadUnitClass(long inPlayer, long inUnitClass) { this->mThread->Post(0x404, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inUnitClass), true); }
    // 0xd4708 (0x40b)
    void PostUnloadTechnology(long inPlayer, long inTechnology) { this->mThread->Post(0x40b, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inTechnology), true); }
    // 0xd4723 (0x409)
    void PostUnloadUnitClass(long inPlayer, long inUnitClass) { this->mThread->Post(0x409, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inUnitClass), true); }
    // 0xd473e (0x40c, then mMessageQueueDone)
    long WaitForMessageQueue() { return (long)ee_preload_wait_for_queue(this); }

    TSThread* mThread;                                      // +0x04
    TSEvent* mThreadCreated;                                // +0x08
    TSEvent* mThreadDone;                                   // +0x0c
    TSEvent* mMessageQueueDone;                             // +0x10
    // (+0x14 mQueue, +0x24 mQuit, +0x25 mQueueing: the Rust pre-loader's, ee_game preload.rs)
};

extern EEDataPreLoading* gDataPreLoading;   // 0x531900 (EEMLoadUnitData posts through it; provisional name)

#endif
