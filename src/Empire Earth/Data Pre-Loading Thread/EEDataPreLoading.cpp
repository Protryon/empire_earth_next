// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEDataPreLoading.cpp (__FILE__ 0x4bc5cc). The data pre-loading thread:
// 0xd3ff4..0xd47a1 (ctor, ??_G, dtor, the members in definition order, then vector::push_back's COMDAT; the table
// loader 0xd47a1 that follows is the Database unit EEAIUnitTargetingTable.cpp). Unit flags: the exe's module set.
// The bodies are Rust (ee_game preload.rs, world lane round 1).
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/TSEvent.h"
#include "lle/TSThread.h"
#include "lle/TSReadWriteLock.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEStringIDs.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGameState.h"
#include "ee/EEGraphics.h"
#include "ee/EEPhysics.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECommunications.h"
#include "ee/EEClient.h"
#include "ee/EEMessage.h"
#include "ee/EEDataPreLoading.h"
#include "rust/game/preload.h"
// The thread's bodies are Rust (ee_game preload.rs, world lane round 1: the loop, the messages, the queue and its two
// flags, the media loads, the game's load); each C++ call they make is a case of ee_preloadcpp below.

// Creates the data pre-loading thread at the given priority and waits until it is running
EEDataPreLoading::EEDataPreLoading(long inThreadPriority)
{
    ee_preload_construct(this, inThreadPriority);   // (preload.rs)
}

// Destroys the pre-loader: frees its events, drops every queued message and destroys the thread
EEDataPreLoading::~EEDataPreLoading()
{
    ee_preload_destruct(this);
}

unsigned long EEDataPreLoading::Main(void* inThis) { return ee_preload_main(inThis); }
void EEDataPreLoading::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
    { ee_preload_handle_message(this, (uint32_t)inMessage, inParameter1, inParameter2); }
void EEDataPreLoading::StartQueueing() { ee_preload_start_queueing(); }
void EEDataPreLoading::FlushQueue() { ee_preload_flush_queue(this); }
void EEDataPreLoading::QueueMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
    { ee_preload_queue_message(this, (uint32_t)inMessage, inParameter1, inParameter2); }
long EEDataPreLoading::WaitForMessageQueue() { return (long)ee_preload_wait_for_queue(this); }

// Tells the user interface that loading the game failed once the error box is closed
void EEDataPreLoading::LoadGameErrorDone(UIForm* inForm)
{
    gUserInterface->PostLoadGameFailed(false);
}

// Asks the pre-loading thread to load the game
void EEDataPreLoading::PostLoadGame(bool inSkipGameMedia, bool inClearWorld)
{
    this->mThread->Post(0x402, (void*)(inSkipGameMedia == true), (void*)(inClearWorld == true), true);
}

// Asks the pre-loading thread to load a player's technology media
void EEDataPreLoading::PostLoadTechnology(long inPlayer, long inTechnology)
{
    this->mThread->Post(0x40a, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inTechnology), true);
}

// Asks the pre-loading thread to load a player's unit class media
void EEDataPreLoading::PostLoadUnitClass(long inPlayer, long inUnitClass)
{
    this->mThread->Post(0x404, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inUnitClass), true);
}

// Asks the pre-loading thread to unload a player's technology media
void EEDataPreLoading::PostUnloadTechnology(long inPlayer, long inTechnology)
{
    this->mThread->Post(0x40b, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inTechnology), true);
}

// Asks the pre-loading thread to unload a player's unit class media
void EEDataPreLoading::PostUnloadUnitClass(long inPlayer, long inUnitClass)
{
    this->mThread->Post(0x409, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inUnitClass), true);
}

#define P64(x) ((int64_t)(intptr_t)(x))
#define DP ((EEDataPreLoading*)p)
#define PL ((EEPlayer*)p)

// The pre-loader's calls (ee_game preload.rs EePreloadCpp): p the pre-loader or the object named, a the integers, f the
// floats
extern "C" int64_t ee_preloadcpp(EePreloadCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EePreloadCpp O;
    switch (inOp) {
    case O::Receive: return DP->mThread->Receive(*(unsigned long*)a[0], *(void**)a[1], *(void**)a[2]);
    case O::ThreadCreatedSet: DP->mThreadCreated->Set();
        break;
    case O::ThreadDoneSet: DP->mThreadDone->Set();
        break;
    case O::QueueDoneSet: DP->mMessageQueueDone->Set();
        break;
    case O::QueueDoneWait: DP->mMessageQueueDone->Wait(kTSWaitInfinite);
        break;
    case O::QueueDoneClear: DP->mMessageQueueDone->Clear();
        break;
    case O::ThreadPost: return DP->mThread->Post((unsigned long)a[0], NULL, NULL, true);
    case O::ThreadPriority: return DP->mThread->GetPriority();
    case O::SetThreadPriority: DP->mThread->SetPriority((long)a[0]);
        break;
    case O::Player: return P64(gWorld.mPlayers[a[0]]);
    case O::Class: return P64(gUnitClassTable[a[0]]);
    case O::Technology: return P64(PL->mTechnologies->Get((long)a[0]));
    case O::ClassUnitType: return ((EEUnitClass*)p)->mUnitType;
    case O::SetUnitClassMedia: PL->SetUnitClassMedia((long)a[0], 0x7f);
        break;
    case O::LoadUnitClassButtons: PL->LoadUnitClassButtons((long)a[0]);
        break;
    case O::UnloadUnitClassMedia: PL->UnloadUnitClassMedia((long)a[0]);
        break;
    case O::UnloadUnitClassButtons: PL->UnloadUnitClassButtons((long)a[0]);
        break;
    case O::TechLoadMedia: ((EETechTreeNode*)p)->LoadMedia((EEPlayer*)a[0]);
        break;
    case O::TechUnloadMedia: ((EETechTreeNode*)p)->UnloadMedia((EEPlayer*)a[0]);
        break;
    // the game's load
    case O::NewOptions: return P64(new EEGameOptions(gGameOptions));
    case O::DeleteOptions: delete (EEGameOptions*)p;
        break;
    case O::NewError: return P64(new UWideString());
    case O::DeleteError: delete (UWideString*)p;
        break;
    case O::LoadProgress: gUserInterface->SetLoadProgress(f[0]);
        break;
    case O::TextLoadingGlobalMedia: gUserInterface->SetLoadingText(IDS_LOADING_GLOBAL_MEDIA);
        break;
    case O::TextPopulatingWorld: gUserInterface->SetLoadingText(IDS_POPULATING_WORLD);
        break;
    case O::TextLoadingComplete: gUserInterface->SetLoadingText(IDS_LOADING_COMPLETE);
        break;
    case O::TextWaitingForPlayers: gUserInterface->SetLoadingText(IDS_WAITING_FOR_PLAYERS);
        break;
    case O::LoadType: return gGameOptions.mLoadType;
    case O::SetLoadingSavedGame: gLoadingSavedGame = a[0] != 0;
        break;
    case O::GameStateCreate: return gGameState.Create(*(EEGameOptions*)a[0], *(UWideString*)a[1]);
    case O::GameStateReset: gGameState.Reset();
        break;
    case O::ReleaseScene: gGraphics->PostReleaseScene();
        break;
    case O::WorldWriteLock: gWorld.mLock->WriteLock();
        break;
    case O::WorldWriteUnlock: gWorld.mLock->WriteUnlock();
        break;
    case O::WorldClear: gWorld.Clear();
        break;
    case O::StartGame: gWorld.StartGame(&gGameState, false);
        break;
    case O::ResumeGraphics: gUserInterface->ResumeGraphics();
        break;
    case O::IsMultiplayer: return gCommunications->GetIsMultiplayerSession();
    case O::IsClient: return gCommunications->IsClient();
    case O::NewLoadedResponseClient: return P64(new EEMRecord(kMessageStartLoadingResponse, {static_cast<EEClient*>(gCommunications)->GetMySendToSlot(), gWorld.GetCurrentPlayer()->GetPlayerID(), 0}));
    case O::NewLoadedResponseServer: return P64(new EEMRecord(kMessageStartLoadingResponse, {0xfe, gWorld.GetCurrentPlayer()->GetPlayerID(), 0}));
    case O::ProcessLocalInput: gCommunications->ProcessLocalUserInput((NEMessage*)p, kSlotNone);
        break;
    case O::SetHasLoaded: (static_cast<EEClient*>(gCommunications))->mHasLoaded = true;
        break;
    case O::DisplayError: gUIManager->DisplayMessage(*(UWideString*)p, true, NULL, EEDataPreLoading::LoadGameErrorDone, true);
        break;
    case O::CreateEvent: return TSScheduler::CreateEvent(*(a[0] == 0 ? &DP->mThreadCreated : a[0] == 1 ? &DP->mThreadDone : &DP->mMessageQueueDone)) == keseOK;
    case O::DestroyEvent: TSScheduler::DestroyEvent(*(a[0] == 0 ? &DP->mThreadCreated : a[0] == 1 ? &DP->mThreadDone : &DP->mMessageQueueDone));
        break;
    case O::CreateThread: return TSScheduler::CreateThread(DP->mThread, EEDataPreLoading::Main, (void*)DP) == keseOK;
    case O::DestroyThread: TSScheduler::DestroyThread(DP->mThread);
        break;
    case O::SetThreadPriorityOk: return DP->mThread->SetPriority((long)a[0]) == keseOK;
    case O::ThreadCreatedWait: DP->mThreadCreated->Wait(kTSWaitInfinite);
        break;
    case O::SetThreadName: DP->mThread->SetName(gDataPreLoadingThreadName);
        break;
    case O::Verify: ESVERIFY_AT(a[0] != 0, (long)a[1]);
        break;
    }
    return 0;
}
