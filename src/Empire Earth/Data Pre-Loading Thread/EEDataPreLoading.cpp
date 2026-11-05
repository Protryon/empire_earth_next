// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEDataPreLoading.cpp (__FILE__ 0x4bc5cc). The data pre-loading thread:
// 0xd3ff4..0xd47a1 (ctor, ??_G, dtor, the members in definition order, then vector::push_back's COMDAT; the table
// loader 0xd47a1 that follows is the Database unit EEAIUnitTargetingTable.cpp). Unit flags: the exe's module set.
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
// FUNCTION: EXE 0x4d3ff4
// Creates the data pre-loading thread at the given priority and waits until it is running
EEDataPreLoading::EEDataPreLoading(long inThreadPriority)
{
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadCreated) == keseOK, 55);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadDone) == keseOK, 56);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMessageQueueDone) == keseOK, 57);
    this->mQuit = false;
    this->mQueueing = false;
    this->mQueue.clear();

    ESVERIFY_AT(TSScheduler::CreateThread(this->mThread, EEDataPreLoading::Main, (void*)this) == keseOK, 65);
    ESVERIFY_AT(this->mThread->SetPriority(inThreadPriority) == keseOK, 66);
    this->mThreadCreated->Wait(kTSWaitInfinite);
    this->mThread->SetName(gDataPreLoadingThreadName);
}

// SYNTHETIC: EXE 0x4d4105
// (the scalar deleting destructor, emitted with the vtable; slot EEDataPreLoading+0x0)

// FUNCTION: EXE 0x4d4121
// Destroys the pre-loader: frees its events, deletes every queued message and destroys the thread
EEDataPreLoading::~EEDataPreLoading()
{
    TSScheduler::DestroyEvent(this->mThreadCreated);
    TSScheduler::DestroyEvent(this->mThreadDone);
    TSScheduler::DestroyEvent(this->mMessageQueueDone);
    for (std::vector<EEDataPreLoadingMessage*>::iterator it = this->mQueue.begin(); it != this->mQueue.end(); ++it)
        delete *it;
    this->mQueue.clear();
    TSScheduler::DestroyThread(this->mThread);
}

// FUNCTION: EXE 0x4d4176
// Runs the pre-loading thread: drains its messages (queueing or handling each, 1 ms apart) until told to quit
unsigned long EEDataPreLoading::Main(void* inThis)
{
    EEDataPreLoading* theThis = static_cast<EEDataPreLoading*>(inThis);
    unsigned long theMessage;
    void* theParameter1;
    void* theParameter2;
    theThis->mThread->Receive(theMessage, theParameter1, theParameter2);
    theThis->mThreadCreated->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    while (!theThis->mQuit) {
        long theResult;
        do {
            theResult = theThis->mThread->Receive(theMessage, theParameter1, theParameter2);
            if (UMGetFloatingPointPrecision() != kFPPrecision53)
                UMSetFloatingPointPrecision(kFPPrecision53);
            if (UMGetFloatingPointRoundingMode() != 0)
                UMSetFloatingPointRoundingMode(0);
            if (theResult == kErrorOK) {
                if (theThis->mQueueing)
                    theThis->QueueMessage(theMessage, theParameter1, theParameter2);
                else
                    theThis->HandleMessage(theMessage, theParameter1, theParameter2);
            }
            PlatSleep(1);
        } while (theResult == kErrorOK);
    }
    theThis->mThreadDone->Set();
    return 0;
}

// FUNCTION: EXE 0x4d4231
// Handles one pre-loader message: quit, start/flush queueing, (un)load unit class or tech media, load a game
void EEDataPreLoading::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    switch (inMessage) {
    case kThreadMessageQuit:
        this->mQuit = true;
        break;
    case 0x407:
        this->StartQueueing();
        break;
    case 0x408:
        this->FlushQueue();
        break;
    case 0x404:
    case 0x409: {
        EEPlayer* thePlayer = gWorld.mPlayers[(long)reinterpret_cast<intptr_t>(inParameter1)];
        if (thePlayer != NULL) {
            EEUnitClass* theClass = gUnitClassTable[(long)reinterpret_cast<intptr_t>(inParameter2)];
            if (theClass != NULL)
                this->LoadUnitClassMedia(thePlayer, theClass, inMessage == 0x404);
        }
        break;
    }
    case 0x40a:
    case 0x40b: {
        EEPlayer* thePlayer = gWorld.mPlayers[(long)reinterpret_cast<intptr_t>(inParameter1)];
        if (thePlayer != NULL) {
            EETechTreeNode* theNode = thePlayer->mTechnologies->Get((long)reinterpret_cast<intptr_t>(inParameter2));
            if (theNode != NULL)
                this->LoadTechnologyMedia(thePlayer, theNode, inMessage == 0x40a);
        }
        break;
    }
    case 0x402:
        this->LoadGame(inParameter1 != NULL, inParameter2 != NULL);
        break;
    case 0x40c:
        this->mMessageQueueDone->Set();
        break;
    }
}

// FUNCTION: EXE 0x4d4332
// Loads (or unloads) a unit class's media and command buttons for a player
void EEDataPreLoading::LoadUnitClassMedia(EEPlayer* inPlayer, EEUnitClass* inClass, bool inLoad)
{
    if (inLoad == true) {
        inPlayer->SetUnitClassMedia(inClass->mUnitType, 0x7f);
        inPlayer->LoadUnitClassButtons(inClass->mUnitType);
    } else {
        inPlayer->UnloadUnitClassMedia(inClass->mUnitType);
        inPlayer->UnloadUnitClassButtons(inClass->mUnitType);
    }
}

// FUNCTION: EXE 0x4d4376
// Loads (or unloads) a technology's media for a player
void EEDataPreLoading::LoadTechnologyMedia(EEPlayer* inPlayer, EETechTreeNode* inNode, bool inLoad)
{
    TMGetTime();
    if (inLoad == true)
        inNode->LoadMedia(inPlayer);
    else
        inNode->UnloadMedia(inPlayer);
    TMGetTime();
}

// FUNCTION: EXE 0x4d439f
// Builds the game world from the current game options with load progress, then reports loaded to the host;
// on failure exits the game and shows the error
void EEDataPreLoading::LoadGame(bool inSkipGameMedia, bool inClearWorld)
{
    EEGameOptions theOptions(gGameOptions);
    UWideString theError;
    long thePriority = this->mThread->GetPriority();
    this->mThread->SetPriority(PLAT_PRIORITY_ABOVE_NORMAL);
    gUserInterface->SetLoadProgress(0.0f);
    gUserInterface->SetLoadingText(IDS_LOADING_GLOBAL_MEDIA);
    if (!inSkipGameMedia)
        EELoadGameMedia();
    bool theFlag;
    if (gGameOptions.mLoadType == 4 || gGameOptions.mLoadType == 1 || gGameOptions.mLoadType == 2) {
        theFlag = true;
        gLoadingSavedGame = true;
    } else
        theFlag = false;
    bool theCreated = gGameState.Create(theOptions, theError);
    if (theCreated) {
        if (inClearWorld == true) {
            gGraphics->PostReleaseScene();
            gGraphics->PostMessage50AAndWait();
            gWorld.mLock->WriteLock();
            gWorld.Clear();
            gPhysics->ClearServerMessages();
            gWorld.mLock->WriteUnlock();
        }
        gUserInterface->SetLoadProgress(0.6f);
        gUserInterface->SetLoadingText(IDS_POPULATING_WORLD);
        gWorld.StartGame(&gGameState, false);
        gUserInterface->ResumeGraphics();
    }
    gGameState.Reset();
    if (theFlag)
        gLoadingSavedGame = false;
    if (theCreated) {
        if (!gCommunications->GetIsMultiplayerSession())
            gUserInterface->SetLoadingText(IDS_LOADING_COMPLETE);
        else
            gUserInterface->SetLoadingText(IDS_WAITING_FOR_PLAYERS);
        gUserInterface->SetLoadProgress(1.0f);
        EEMStartLoadingResponse* theResponse;
        if (gCommunications->IsClient())
            theResponse = new EEMStartLoadingResponse(static_cast<EEClient*>(gCommunications)->GetMySendToSlot(),
                                                      gWorld.GetCurrentPlayer()->GetPlayerID(), 0);
        else
            theResponse = new EEMStartLoadingResponse(0xfe, gWorld.GetCurrentPlayer()->GetPlayerID(), 0);
        gCommunications->ProcessLocalUserInput(theResponse, kSlotNone);
        if (gCommunications->IsClient())
            (static_cast<EEClient*>(gCommunications))->mHasLoaded = true;
    } else {
        EEExitGame();
        gUIManager->DisplayMessage(theError, true, NULL, EEDataPreLoading::LoadGameErrorDone, true);
    }
    this->mThread->SetPriority(thePriority);
}

// FUNCTION: EXE 0x4d4613
// Tells the user interface that loading the game failed once the error box is closed
void EEDataPreLoading::LoadGameErrorDone(UIForm* inForm)
{
    gUserInterface->PostLoadGameFailed(false);
}

// FUNCTION: EXE 0x4d4621
// Starts holding incoming messages in the queue instead of handling them
void EEDataPreLoading::StartQueueing()
{
    this->mQueueing = true;
}

// FUNCTION: EXE 0x4d4626
// Stops queueing and handles then deletes every queued message in order
void EEDataPreLoading::FlushQueue()
{
    this->mQueueing = false;
    for (std::vector<EEDataPreLoadingMessage*>::iterator it = this->mQueue.begin(); it != this->mQueue.end(); ++it) {
        this->HandleMessage((*it)->mMessage, (*it)->mParameter1, (*it)->mParameter2);
        delete *it;
    }
    this->mQueue.clear();
}

// FUNCTION: EXE 0x4d465f
// Queues a message for later; a flush message instead handles the queue now
void EEDataPreLoading::QueueMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    if (inMessage == 0x408)
        this->FlushQueue();
    else {
        EEDataPreLoadingMessage* theMessage = new EEDataPreLoadingMessage;
        theMessage->mMessage = inMessage;
        theMessage->mParameter1 = inParameter1;
        theMessage->mParameter2 = inParameter2;
        this->mQueue.push_back(theMessage);
    }
}

// FUNCTION: EXE 0x4d46a9
// Asks the pre-loading thread to load the game
void EEDataPreLoading::PostLoadGame(bool inSkipGameMedia, bool inClearWorld)
{
    this->mThread->Post(0x402, (void*)(inSkipGameMedia == true), (void*)(inClearWorld == true), true);
}

// FUNCTION: EXE 0x4d46d2
// Asks the pre-loading thread to load a player's technology media
void EEDataPreLoading::PostLoadTechnology(long inPlayer, long inTechnology)
{
    this->mThread->Post(0x40a, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inTechnology), true);
}

// FUNCTION: EXE 0x4d46ed
// Asks the pre-loading thread to load a player's unit class media
void EEDataPreLoading::PostLoadUnitClass(long inPlayer, long inUnitClass)
{
    this->mThread->Post(0x404, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inUnitClass), true);
}

// FUNCTION: EXE 0x4d4708
// Asks the pre-loading thread to unload a player's technology media
void EEDataPreLoading::PostUnloadTechnology(long inPlayer, long inTechnology)
{
    this->mThread->Post(0x40b, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inTechnology), true);
}

// FUNCTION: EXE 0x4d4723
// Asks the pre-loading thread to unload a player's unit class media
void EEDataPreLoading::PostUnloadUnitClass(long inPlayer, long inUnitClass)
{
    this->mThread->Post(0x409, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inUnitClass), true);
}

// FUNCTION: EXE 0x4d473e
// Waits until the pre-loading thread has handled every message posted so far; returns the post result
long EEDataPreLoading::WaitForMessageQueue()
{
    long theResult = this->mThread->Post(0x40c, NULL, NULL, true);
    if (theResult == kErrorOK) {
        this->mMessageQueueDone->Wait(kTSWaitInfinite);
        this->mMessageQueueDone->Clear();
    }
    return theResult;
}

// TEMPLATE: EXE 0x4d4775
// std::vector<EEDataPreLoadingMessage*>::push_back (QueueMessage)
