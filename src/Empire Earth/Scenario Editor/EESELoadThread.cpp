// C:\EEX\Empire Earth\Scenario Editor\EESELoadThread.cpp (__FILE__ 0x4c2f70, the ctor's ESVERIFYs). The scenario
// editor's loading thread: 0x172125..0x172b0f (ctor, the inline EESEAChangeLoadText members it is the first user of,
// dtor, the handlers, Main, HandleMessage, the posting wrappers). After EESEInstructions.cpp (..0x172125), before the
// next unit's globals chain 0x172b0f. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

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
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/TSEvent.h"
#include "lle/UMFastMath.h"

#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/UResourceFile.h"
#include "ee/EETileLocation.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x572125
// Starts the editor's loading thread at the given priority, waits until it runs and names it ScenEdit
EESELoadThread::EESELoadThread(EEUIFScenarioEditor* inEditor, long inThreadPriority)
{
    this->mIsDone = false;
    this->mEditor = inEditor;
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadCreated) == keseOK, 80);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadDone) == keseOK, 81);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMessageQueueDone) == keseOK, 82);

    ESVERIFY_AT(TSScheduler::CreateThread(this->mThread, EESELoadThread::Main, (void*)this) == keseOK, 85);
    ESVERIFY_AT(this->mThread->SetPriority(inThreadPriority) == keseOK, 86);
    this->mThreadCreated->Wait(INFINITE);
    this->mThread->SetName(gScenarioEditorThreadName);
    this->mChangeLoadText = new EESEAChangeLoadText;
}

// FUNCTION: EXE 0x572226
// Creates the loading text action (EESEAChangeLoadText ctor, inline; emitted out of line here)

// FUNCTION: EXE 0x57224d
// Runs the loading text action (EESEAChangeLoadText::Virtual4, inline in ee/EESEAChangeLoadText.h)

// SYNTHETIC: EXE 0x572262
// (EESEAChangeLoadText's scalar deleting destructor)

// FUNCTION: EXE 0x572295
// Destroys the loading thread: releases its text action, events and thread
EESELoadThread::~EESELoadThread()
{
    this->mChangeLoadText->Dereference();
    TSScheduler::DestroyEvent(this->mThreadCreated);
    TSScheduler::DestroyEvent(this->mThreadDone);
    TSScheduler::DestroyEvent(this->mMessageQueueDone);
    TSScheduler::DestroyThread(this->mThread);
}

// FUNCTION: EXE 0x5722c6
// Loads a player's unit media and the first n epochs' technology media, showing 'Loading <epoch>' text
void EESELoadThread::Handle406(unsigned long inPlayer, unsigned long inCount)
{
    UWideString theFormat;
    UWideString theText;
    UWideString theName;
    gWorld.mLock->ReadLock();
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    gWorld.mLock->ReadUnlock();
    gResource->GetString(IDS_LOADING_EPOCH_FOR_PLAYER, theFormat);
    if (theFormat.GetLength() > 0) {
        gResource->GetString(IDS_PREHISTORIC_AGE, theName);
        sprintf(theText, theFormat, (const char*)theName, inPlayer);
    }
    this->mChangeLoadText->Post(theText);
    thePlayer->LoadAllClassMedia();
    for (unsigned long i = 0; i < inCount; i++) {
        if (theFormat.GetLength() > 0) {
            gResource->GetString(IDS_EPOCH_NAME_FIRST + i, theName);
            sprintf(theText, theFormat, (const char*)theName, inPlayer);
        }
        this->mChangeLoadText->Post(theText);
        thePlayer->mTechnologies->Get(i)->LoadMedia(thePlayer);
    }
    this->mChangeLoadText->Post(UWideString(""));
}

// FUNCTION: EXE 0x572433
// Loads (with 'Loading <terrain>' text) or unloads a terrain type and the media and buttons of its
// unit classes for player 0
void EESELoadThread::Handle408(unsigned long inTerrain, bool inUnload)
{
    UWideString theFormat;
    UWideString theText;
    UWideString theName;
    EEPlayer* thePlayer = gWorld.mPlayers[0];
    EETerrainTable* theTerrain = EETerrainTable::Get(inTerrain);
    if (theTerrain != NULL && gWorld.mMap != NULL) {
        if (!inUnload) {
            gResource->GetString(IDS_LOADING_TERRAIN, theFormat);
            long theNameID = EETerrainTable::Get(inTerrain)->mNameID;
            gResource->GetString(theNameID, theName);
            sprintf(theText, theFormat, (const char*)theName);
            this->mChangeLoadText->Post(theText);
            theTerrain->Load(gWorld.mMap->mTerrainMesh);
            if (theTerrain->GetNumUnitClasses() > 0) {
                for (unsigned long i = 0; i < theTerrain->GetNumUnitClasses(); i++) {
                    if (gUnitClassTable[theTerrain->GetUnitClass(i)] != NULL) {
                        thePlayer->SetUnitClassMedia(theTerrain->GetUnitClass(i), 0x7f);
                        thePlayer->LoadUnitClassButtons(theTerrain->GetUnitClass(i));
                    }
                }
            }
            this->mChangeLoadText->Post(UWideString(""));
        } else {
            theTerrain->Unload(gWorld.mMap->mTerrainMesh);
            if (theTerrain->GetNumUnitClasses() > 0) {
                for (unsigned long i = 0; i < theTerrain->GetNumUnitClasses(); i++) {
                    if (gUnitClassTable[theTerrain->GetUnitClass(i)] != NULL) {
                        thePlayer->UnloadUnitClassMedia(theTerrain->GetUnitClass(i));
                        thePlayer->UnloadUnitClassButtons(theTerrain->GetUnitClass(i));
                    }
                }
            }
        }
    }
}

// FUNCTION: EXE 0x57263f
// Loads a unit class's media for a player and creates its placement ghost at the mouse (rotated as in
// the units tab unless it is a land building), then unloads the media again
void EESELoadThread::Handle402(unsigned long inUnitClass, unsigned long inPlayer)
{
    UWideString theFormat;
    UWideString theName;
    UWideString theText;
    gWorld.mLock->ReadLock();
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    gWorld.mLock->ReadUnlock();
    if (theClass != NULL && thePlayer != NULL) {
        gResource->GetString(IDS_LOADING_FOR_PLAYER, theFormat);
        gResource->GetString(theClass->mNameStringID, theName);
        if (theFormat.GetLength() > 0)
            sprintf(theText, theFormat, (const char*)theName, inPlayer);
        this->mChangeLoadText->Post(theText);
        thePlayer->SetUnitClassMedia(theClass->mUnitType, 0x7f);
        EEGhostUnit::Create(this->mEditor->GetMouseLocation(), theClass->mUnitType, thePlayer,
                            !theClass->IsLandBuilding() ? static_cast<EESEUnit*>(this->mEditor->mTabGroups[kEditorTabUnits])->GetRotationAngle() : 0.0f, false);
        thePlayer->UnloadUnitClassMedia(theClass->mUnitType);
        this->mChangeLoadText->Post(UWideString(""));
    }
}

// FUNCTION: EXE 0x5727ae
// Runs the loading thread: handles posted messages with 53-bit FPU precision until told to quit, then
// deletes the thread object
unsigned long EESELoadThread::Main(void* inThis)
{
    EESELoadThread* theThread = static_cast<EESELoadThread*>(inThis);
    unsigned long theMessage;
    void* theParameter1;
    void* theParameter2;
    theThread->mThreadCreated->Set();
    theThread->mThread->SetPriority(THREAD_PRIORITY_ABOVE_NORMAL);
    UMSetFloatingPointPrecision(kFPPrecision53);
    while (!theThread->mIsDone) {
        long theResult = theThread->mThread->Receive(theMessage, theParameter1, theParameter2);
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        if (theResult == kErrorOK)
            theThread->HandleMessage(theMessage, theParameter1, theParameter2);
        Sleep(1);
    }
    delete theThread;
    return 0;
}

// FUNCTION: EXE 0x57285c
// Dispatches a loading thread message: quit, ghost unit, epoch/terrain media, tech tree reload, scenario
// test start, posted UI events and loaded objects
void EESELoadThread::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    switch (inMessage) {
    case kThreadMessageQuit:
        gUserInterface->LeaveScenarioEditor();
        this->mIsDone = true;
        break;
    case kLoadThreadCreateGhostUnit:
        this->Handle402((unsigned long)reinterpret_cast<intptr_t>(inParameter1), (unsigned long)reinterpret_cast<intptr_t>(inParameter2));
        break;
    case kLoadThreadLoadEpochMedia:
        this->Handle406((unsigned long)reinterpret_cast<intptr_t>(inParameter1), (unsigned long)reinterpret_cast<intptr_t>(inParameter2));
        break;
    case 0x403:
        this->Handle403((unsigned long)reinterpret_cast<intptr_t>(inParameter1), inParameter2 != NULL);
        break;
    case kLoadThreadStartScenarioTest:
        gUserInterface->StartScenarioTest();
        this->mIsDone = true;
        break;
    case kLoadThreadPostEvent:
        gUserInterface->PostEvent(static_cast<UIEvent*>(inParameter1), NULL);
        static_cast<UReferenceCounter*>(inParameter1)->Dereference();
        break;
    case kLoadThreadLoadTerrain:
        this->Handle408((unsigned long)reinterpret_cast<intptr_t>(inParameter1), false);
        break;
    case kLoadThreadShowLoadedObject:
        gUserInterface->ShowLoadedObject(static_cast<UReferenceCounter*>(inParameter1));
        static_cast<UReferenceCounter*>(inParameter1)->Dereference();
        break;
    case kLoadThreadStartScenarioTest2:
        gUserInterface->StartScenarioTest2();
        this->mIsDone = true;
        break;
    case kLoadThreadUnloadEpochMedia:
        this->Handle407((unsigned long)reinterpret_cast<intptr_t>(inParameter1), (unsigned long)reinterpret_cast<intptr_t>(inParameter2));
        break;
    case kLoadThreadUnloadTerrain:
        this->Handle408((unsigned long)reinterpret_cast<intptr_t>(inParameter1), true);
        break;
    case kLoadThreadMessageQueueDone:
        this->mMessageQueueDone->Set();
        break;
    }
}

// FUNCTION: EXE 0x572967
// Reloads a player's tech tree under the world write lock and settles its pending and killed units
void EESELoadThread::Handle403(unsigned long inPlayer, bool inFlag)
{
    gWorld.mLock->WriteLock();
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    thePlayer->ReloadTechTree(inFlag);
    gWorld.FlushQueuedPlacements();
    thePlayer->AddPendingUnits(0, 0);
    thePlayer->RemoveKilledUnits();
    thePlayer->ReleasePendingUnits();
    gWorld.mLock->WriteUnlock();
}

// FUNCTION: EXE 0x5729bd
// Asks the loading thread to start the scenario test and finish
void EESELoadThread::Post404()
{
    this->mThread->Post(0x404, NULL, NULL, true);
}

// FUNCTION: EXE 0x5729d2
// Asks the loading thread to leave the scenario editor and quit
void EESELoadThread::Post401()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
}

// FUNCTION: EXE 0x5729e7
// Hands a UI event to the loading thread for posting (keeps a reference)
void EESELoadThread::Post40B(UReferenceCounter* inObject)
{
    inObject->Reference();
    this->mThread->Post(0x40b, inObject, NULL, true);
}

// FUNCTION: EXE 0x572a04
// Hands a loaded object to the loading thread for display (keeps a reference)
void EESELoadThread::Post40C(UReferenceCounter* inObject)
{
    inObject->Reference();
    this->mThread->Post(0x40c, inObject, NULL, true);
}

// FUNCTION: EXE 0x572a21
// Asks the loading thread to load a terrain type's media
void EESELoadThread::Post408(void* inParameter)
{
    this->mThread->Post(0x408, inParameter, NULL, true);
}

// FUNCTION: EXE 0x572a3a
// Asks the loading thread to load a player's epoch media
void EESELoadThread::Post406(void* inParameter1, void* inParameter2)
{
    this->mThread->Post(0x406, inParameter1, inParameter2, true);
}

// FUNCTION: EXE 0x572a55
// Asks the loading thread to reload a player's tech tree
void EESELoadThread::Post403(void* inParameter, bool inFlag)
{
    this->mThread->Post(0x403, inParameter, (void*)inFlag, true);
}

// FUNCTION: EXE 0x572a72
// Asks the loading thread to create a unit class's placement ghost for a player
void EESELoadThread::Post402(void* inParameter1, void* inParameter2)
{
    this->mThread->Post(0x402, inParameter1, inParameter2, true);
}

// FUNCTION: EXE 0x572a8d
// Asks the loading thread to start the scenario test (second variant) and finish
void EESELoadThread::Post405()
{
    this->mThread->Post(0x405, NULL, NULL, true);
}

// FUNCTION: EXE 0x572aa2
// Asks the loading thread to unload a terrain type's media
void EESELoadThread::Post409(void* inParameter)
{
    this->mThread->Post(0x409, inParameter, NULL, true);
}

// FUNCTION: EXE 0x572abb
// Unloads a player's unit media and the first n epochs' technology media
void EESELoadThread::Handle407(unsigned long inPlayer, unsigned long inCount)
{
    gWorld.mLock->ReadLock();
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    gWorld.mLock->ReadUnlock();
    thePlayer->UnloadAllClassMedia();
    for (unsigned long i = 0; i < inCount; i++)
        thePlayer->mTechnologies->Get(i)->UnloadMedia(thePlayer);
}
