// C:\EEX\Empire Earth\Scenario Manager\EEScenarioManager.cpp (file name inferred: no __FILE__): 0x1afa90..0x1ba02f,
// with the functions of EESMDialogMessage.cpp folded in. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "ee/EESMDialogMessage.h"
#include "lle/FSFile.h"
#include <vector>
#include "ee/EEAreaUtilities.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/EESMEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMArea.h"
#include "ee/EESMTrigger.h"
#include "ee/EEDeleteVectorContents.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEDebugTrace.h"
#include "lle/SESoundManager.h"
#include "lle/FSFileSpec.h"
#include "GEWindow.h"
#include "ee/EECameraControl.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGameSound.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"
#include "ee/EEStringIDs.h"
#include "ee/EEPersistentTag.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"
// the vector Load/Save helpers (cdecl template COMDATs 0x1b5faf / 0x1b5fff: the count, then each element's slot)
template<class T> long EESMLoadVector(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                                      std::vector<T>& outVector)
{
    unsigned long theCount;
    long theResult = inFile.Read(theCount);
    outVector.resize(theCount);
    for (std::vector<T>::iterator theIterator = outVector.begin(); theIterator != outVector.end() && theResult == kErrorOK;
         theIterator++)
        theResult = theIterator->Load(inMajorVersion, inMinorVersion, inFile);
    return theResult;
}
template<class T> long EESMSaveVector(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile,
                                      std::vector<T>& inVector)
{
    unsigned long theCount = inVector.size();
    long theResult = outFile.Write(theCount);
    for (std::vector<T>::iterator theIterator = inVector.begin(); theIterator != inVector.end() && theResult == 0;
         theIterator++)
        theResult = theIterator->Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

bool gScenarioSimDialogs = false;

static const float kTriggerInterval = 500.0f;  // 0x445fcc (trigger interval, ms: Update, ApplyPendingEffects; unit .rdata; provisional name)
extern float gF4BE374;                       // 0x4be374 (= 1.0f; Update ends the cinematic when it is 1; provisional name)
extern float& gSideScrollSpeed;               // 0x530828 (MoveCameraToStart's camera step scale; provisional name)

// FUNCTION: EXE 0x5afa90
// Creates a scenario holding only the default entries (see Reset)
EEScenarioManager::EEScenarioManager()
    : FSPersistent(kPersistentScenario), EeScenarioManagerFields()
{
    this->mTimedEffects = ee_scenario_timed_new();
    this->Reset();
}

// SYNTHETIC: EXE 0x5afb95
// (the scalar deleting destructor, emitted with the vtable; slot EEScenarioManager+0x10)

// FUNCTION: EXE 0x5afbb1
// Empties the scenario back to its defaults (an Always True condition, a Do Nothing effect, a None object, the
// Off Map and Previous Location areas); zeroes the clocks and ID counters, clears the timed and pending effects
void EEScenarioManager::Reset()
{
    UWideString theName;

    ee_scenario_manager_reset(this, this->mTimedEffects);   // the numbers, the timed effects (manager.rs reset)
    DeleteVectorContents(this->mTriggers);
    DeleteVectorContents(this->mConditions);
    EESMCondition* theCondition = new EESMCondition;
    gResource->GetString(IDS_ALWAYS_TRUE, theName);
    theCondition->mName = theName;
    theCondition->mType = kSMConditionAlwaysTrue;
    this->mConditions.push_back(theCondition);

    DeleteVectorContents(this->mEffects);
    EESMEffect* theEffect = new EESMEffect;
    gResource->GetString(IDS_DO_NOTHING, theName);
    theEffect->mName = theName;
    theEffect->mType = kSMEffectNone;
    this->mEffects.push_back(theEffect);

    DeleteVectorContents(this->mObjects);
    EESMObject* theObject = new EESMObject;
    gResource->GetString(IDS_NONE, theName);
    theObject->SetName(theName);
    this->mObjects.push_back(theObject);

    DeleteVectorContents(this->mAreas);
    this->mAreas.clear();
    EESMArea* theArea = new EESMArea;
    gResource->GetString(IDS_OFF_MAP, theName);
    theArea->mName = theName;
    theArea->SetAnchor(U2DPoint<long>(-10000, -10000));
    this->mAreas.push_back(theArea);
    theArea = new EESMArea;
    gResource->GetString(IDS_PREVIOUS_LOCATION, theName);
    theArea->mName = theName;
    theArea->SetAnchor(U2DPoint<long>(-1, -1));
    this->mAreas.push_back(theArea);

    this->mPendingEffects.clear();
}

// FUNCTION: EXE 0x5afe66
// Destroys the scenario: unloads the unit class media Prepare loaded and deletes every trigger, condition, effect,
// object, area and pending effect
EEScenarioManager::~EEScenarioManager()
{
    for (unsigned long i = 0; i < this->mUnitClassMedia.size(); i++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(this->mUnitClassMedia[i].mPlayer);
        thePlayer->GetUpgrade(this->mUnitClassMedia[i].mUnitClass)->UnloadMedia(thePlayer);
        // expression feeds the trace call 0x9ba76, compiled away here (owner-approved stand-in, 2026-10-25)
    }
    DeleteVectorContents(this->mTriggers);
    DeleteVectorContents(this->mConditions);
    DeleteVectorContents(this->mEffects);
    DeleteVectorContents(this->mObjects);
    DeleteVectorContents(this->mAreas);
    ee_scenario_timed_free(this->mTimedEffects);
    this->mTimedEffects = NULL;
    DeleteVectorContents(this->mPendingEffects);
}

// FUNCTION: EXE 0x5afffb
// Queues a dialog message for ShowDialogMessage
void EEScenarioManager::AddMessage(const EESMDialogMessage& inMessage)
{
    this->mMessages.push_back(inMessage);
}

// FUNCTION: EXE 0x5b000d
// Records a copy of an applied effect whose change a saved game must replay on load (ignored until
// ApplyPendingEffects has run)
void EEScenarioManager::AddV134Effect(const EESMEffect& inEffect)
{
    if (this->mB234)
        return;
    EESMEffect* theEffect = new EESMEffect;
    theEffect->Copy(inEffect);
    this->mPendingEffects.push_back(theEffect);
}

// FUNCTION: EXE 0x5b005b
// Schedules an effect (by index) to run at the given scenario time, keeping the timed effects sorted by time
void EEScenarioManager::AddTimedEffect(long inEffect, unsigned long inTime, bool inIsInCinematic)
{
    ee_scenario_timed_add(this->mTimedEffects, inEffect, inTime, inIsInCinematic);
}

// FUNCTION: EXE 0x5b0982
// Creates a new, default scenario manager (the persistent object factory)
FSPersistent* EEScenarioManager::CreateDefault() const
{
    return new EEScenarioManager;
}

// FUNCTION: EXE 0x5b1ae4
// Ends a cinematic: delays the other timed effects and trigger timers by its length + 3 s, drops its unrun effects
// and dialog messages, delays the wonder timers and restores the camera saved by StartCinematic
// Called by EEMEndCinematic::Execute 0xbdf7d
// NEAR: the two discarded expressions per pending entry (a compiled-out trace) and the inline budget of begin()
void EEScenarioManager::EndCinematic()
{
    float theElapsed = this->mTime - this->mCinematicStartTime;
    if (!gWorld.mInCinematic || this->mIsEndingCinematic)
        return;
    gSoundManager->SetLatency(100);
    unsigned long i;
    ee_scenario_timed_end_cinematic(this->mTimedEffects, (long)theElapsed + 3000);   // manager.rs end_cinematic
    unsigned long t;
    for (t = 0; t < this->GetNumTriggers(); t++) {
        if (!this->GetTrigger(t)->mIsInCinematic && this->GetTrigger(t)->mConditionDuration > 0)
            this->GetTrigger(t)->mConditionStartTime = this->GetTrigger(t)->mConditionStartTime + (long)theElapsed + 3000;
    }
    gUserInterface->PostHideMessages();
    if (this->mMessages.size() > 0) {
        for (long j = this->mMessages.size() - 1; j >= 0; j--) {
            if (this->mMessages[j].mIsCinematic == true)
                this->mMessages.erase(this->mMessages.begin() + j);
        }
    }
    for (i = 0; i < gWorld.mNumPlayers; i++) {
        if (gWorld.GetPlayer(i) != NULL)
            gWorld.GetPlayer(i)->DelayWonderTimers((long)theElapsed);
    }
    this->mCinematicStartTime = 0.0f;
    gGraphics->PostLetterboxOut(3000);
    this->mIsEndingCinematic = true;
    gWorld.mCameraControl.ResetPath();
    gWorld.mCameraControl.ClearUnitB();
    gWorld.mCameraControl.ClearUnitA();
    gWorld.mCameraControl.mTurnYaw = 0.0f;
    gWorld.mCameraControl.SetMode(this->mSavedCameraMode);
    gWorld.mCameraControl.SetZoom(this->mSavedZoom);
    gWorld.mCameraControl.SetOffset(this->mK008.mOffset, true);
    gWorld.mCameraControl.SetLocation(this->mK008.mFocus, true, false);
    gWorld.mCameraControl.SetViewTurn(this->mK008.mT00, true);
    gWorld.mCameraControl.SetOffsetTurn(this->mK008.mT40, true);
    gWorld.mCameraControl.SetChanged();
    gWorld.mCameraControl.Update(0.0f);
}

// FUNCTION: EXE 0x5b1e47
// Leaves the cinematic once its letterbox is gone (Update): restores the game speed, the main game form, the
// mouse, fades in and stops the one-shot sounds
void EEScenarioManager::ExitCinematic()
{
    gWorld.mInCinematic = false;
    gWorld.SetGameSpeed(gWorld.mSavedGameSpeed);
    gUIManager->mMainGameForm->SetUIState(0);
    gUIManager->mMainGameForm->mIsCursorLocked = false;
    gMouse->mPointer = 0;
    gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() / 2), (float)(gGameWindow->GetHeight() / 2)));
    gGraphics->PostFadeIn(0);
    gSoundManager->StopAllOneShotSounds();
}

// FUNCTION: EXE 0x5b1eee
// Starts a cinematic (EESMEffect 0x1bd86c): saves the game speed and camera, closes the main game form, locks the
// cursor, unselects every player's units, drops the non-cinematic dialog messages and letterboxes the screen
void EEScenarioManager::StartCinematic()
{
    if (gWorld.mInCinematic)
        return;
    gSoundManager->SetLatency(300);
    this->mCinematicStartTime = this->mTime;
    gWorld.mInCinematic = true;
    gWorld.mSavedGameSpeed = gWorld.GetGameSpeed();
    gUserInterface->CloseForm(gUIManager->mMainGameForm);
    gSoundManager->StopAllOneShotSounds();
    gWorld.mCameraControl.GetOffset(gWorld.mCameraControl.mFraction, this->mK008.mOffset);
    gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, this->mK008.mFocus);
    gWorld.mCameraControl.GetViewTurn(gWorld.mCameraControl.mFraction, this->mK008.mT00);
    gWorld.mCameraControl.GetOffsetTurn(gWorld.mCameraControl.mFraction, this->mK008.mT40);
    this->mSavedCameraMode = gWorld.mCameraControl.mMode;
    this->mSavedZoom = gWorld.mCameraControl.GetZoom();
    gWorld.mCameraControl.ResetPath();
    gWorld.mCameraControl.ClearUnitB();
    gWorld.mCameraControl.ClearUnitA();
    gUIManager->mMainGameForm->SetUIState(0x11);
    gUIManager->mMainGameForm->mIsCursorLocked = true;
    gMouse->mPointer = 6;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        gWorld.GetPlayer(i)->UnselectAllUnits();
        gWorld.GetPlayer(i)->mSelectedGroup->CopySelection(gWorld.GetPlayer(i)->mActiveGroup);
    }
    gUserInterface->PostHideMessages();
    if (this->mMessages.size() > 0) {
        for (long j = this->mMessages.size() - 1; j >= 0; j--) {
            if (!this->mMessages[j].mIsCinematic)
                this->mMessages.erase(this->mMessages.begin() + j);
        }
    }
    gGraphics->PostLetterboxIn(3000);
}

namespace {

int64_t RoundFloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// Update's world (ee_scenario round.rs): each operation the original's statement at its place
int64_t RoundOp(void* inContext, uint32_t inOp, int64_t inA, int64_t inB, float inTime)
{
    EEScenarioManager* theManager = static_cast<EEScenarioManager*>(inContext);
    switch (inOp) {
    case EE_SCENARIO_ROUND_GAME_SPEED: return RoundFloatBits(gWorld.mGameSpeed);
    case EE_SCENARIO_ROUND_TICK_SECONDS: return RoundFloatBits(gPhysics->GetTickSeconds());
    case EE_SCENARIO_ROUND_MESSAGE_COUNT: return (int64_t)theManager->mMessages.size();
    case EE_SCENARIO_ROUND_SHOW_DIALOG: theManager->ShowDialogMessage(); return 0;
    case EE_SCENARIO_ROUND_HUMAN_PLAYING: {
        bool theHumanPlaying = false;
        for (long i = 0; i < (long)gWorld.mNumPlayers; i++) {
            if (gWorld.GetPlayer(i) != NULL && gWorld.GetPlayer(i)->IsHuman() && !gWorld.GetPlayer(i)->mObserverMode)
                theHumanPlaying = true;
        }
        return theHumanPlaying;
    }
    case EE_SCENARIO_ROUND_IN_CINEMATIC: return gWorld.mInCinematic;
    case EE_SCENARIO_ROUND_MOVE_CAMERA: theManager->MoveCameraToStart(); return 0;
    case EE_SCENARIO_ROUND_TRIGGER_COUNT: return (int64_t)theManager->mTriggers.size();
    case EE_SCENARIO_ROUND_EVALUATE_TRIGGER: theManager->GetTrigger((unsigned long)inA)->Evaluate(inTime); return 0;
    case EE_SCENARIO_ROUND_EFFECT_EXISTS: return theManager->mEffects[(unsigned long)inA] != NULL;
    case EE_SCENARIO_ROUND_EXECUTE: theManager->mEffects[(unsigned long)inA]->Execute(inB != 0); return 0;
    case EE_SCENARIO_ROUND_CLEAR_CHAT: theManager->mLastChatText.Clear(); return 0;
    case EE_SCENARIO_ROUND_CAPTURE:
        if (EESimCaptureRecord::Active())
            EESMCaptureUpdate(theManager, inA != 0);
        return 0;
    case EE_SCENARIO_ROUND_LETTERBOX: return RoundFloatBits(gF4BE374);
    case EE_SCENARIO_ROUND_EXIT_CINEMATIC: theManager->ExitCinematic(); return 0;
    }
    return 0;
}

}   // namespace

// FUNCTION: EXE 0x5b20e6
// Runs a scenario tick (EEPhysics 0x153265): shows queued dialog, advances the clocks (tick / game speed in a
// cinematic); while a human plays, every 500 ms evaluates the triggers and runs the due timed effects (ee_scenario
// round.rs update)
void EEScenarioManager::Update()
{
    EeScenarioRoundHost theHost = {this, this, this->mTimedEffects, RoundOp};
    ee_scenario_manager_update(&theHost);
}

// FUNCTION: EXE 0x5b260a
// Resolves a scenario player choice (0 nature, 1/3 a random/every human, 2/4 a random/every computer, 5/6 a random
// player with/without nature, 7+ player n-6) into a list; returns the chosen player, -1 for none or a group
long EEScenarioManager::ChoosePlayer(long inType, std::vector<long>* outPlayers, bool inSingle)
{
    // the choice is Rust's (ee_scenario condition.rs choose_player, the AI's shuffle with it; the SMCP record too)
    EESMHostContext theContext = {NULL, NULL};
    EeScenarioHost theHost = EESMHost(&theContext);
    int32_t thePlayers[EE_SCENARIO_MAX_PLAYERS];
    size_t theCount = 0;
    long theResult = ee_scenario_choose_player(&theHost, (int32_t)inType, inSingle, outPlayers != NULL, thePlayers,
                                               &theCount);
    if (outPlayers != NULL)
        outPlayers->assign(thePlayers, thePlayers + theCount);
    return theResult;
}

// FUNCTION: EXE 0x5b286d
// Replays every pending effect of a loaded game, makes the triggers evaluate on the next tick and starts recording
// new pending effects
void EEScenarioManager::ApplyPendingEffects()
{
    this->mTriggerTimer = kTriggerInterval;
    unsigned long theCount = this->mPendingEffects.size();
    for (unsigned long i = 0; i < theCount; i++)
        if (this->mPendingEffects[i] != NULL)
            this->mPendingEffects[i]->Execute(0);
    this->mB234 = false;
}

// FUNCTION: EXE 0x5b28c0
// Unloads the unit class media loaded before, then preloads the media of each unit class a Create Object effect
// makes for one fixed player (objects not selected on the map)
void EEScenarioManager::Prepare()
{
    std::vector<long> thePlayers;
    unsigned long i;
    if (this->mUnitClassMedia.size() > 0) {
        for (i = 0; i < this->mUnitClassMedia.size(); i++) {
            EEPlayer* thePlayer = gWorld.GetPlayer(this->mUnitClassMedia[i].mPlayer);
            EEUnitClassUpgrade* theUpgrade = thePlayer->GetUpgrade(this->mUnitClassMedia[i].mUnitClass);
            theUpgrade->UnloadMedia(thePlayer);
            EEDebugTrace("Scenario:: Unloading Media for %S for player %d", theUpgrade->GetUnitClass()->GetName(),
                         this->mUnitClassMedia[i].mPlayer);
        }
    }
    this->mUnitClassMedia.clear();
    for (unsigned long theTrigger = 0; theTrigger < this->mTriggers.size(); theTrigger++) {
        EESMTrigger* theSMTrigger = this->mTriggers[theTrigger];
        for (unsigned long theWrapper = 0; theWrapper < theSMTrigger->mEffects.size(); theWrapper++) {
            EESMEffect* theEffect = this->mEffects[theSMTrigger->GetEffect(theWrapper)->mEffectIndex];
            if (theEffect->GetType() != kSMEffectObject || theEffect->mObjectAction != kSMObjectActionCreate)
                continue;
            EESMObject* theObject = this->mObjects[theEffect->mObject];
            if (theObject->GetIsSelectOnMap())
                continue;
            this->ChoosePlayer(theObject->GetOwner(), &thePlayers, false);
            if (thePlayers.size() == 1) {
                long thePlayerNumber = thePlayers[0];
                long theUnitClass = theObject->GetClass();
                if (theUnitClass != 0 && gUnitClassTable[theUnitClass] != NULL) {
                    EEPlayer* thePlayer = gWorld.GetPlayer(thePlayerNumber);
                    thePlayer->mUnitClassUpgrades[theUnitClass]->LoadMedia(thePlayer, 0x7f);
                    EESMUnitClassMedia theMedia;
                    theMedia.mPlayer = thePlayerNumber;
                    theMedia.mUnitClass = theUnitClass;
                    this->mUnitClassMedia.push_back(theMedia);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x5b31e8
// Replaces a unit ID with another in every scenario object
void EEScenarioManager::ReplaceUnit(long inOldUnit, long inNewUnit)
{
    unsigned long theCount = this->mObjects.size();
    for (unsigned long i = 0; i < theCount; i++)
        this->mObjects[i]->ReplaceUnit(inOldUnit, inNewUnit);
}

// FUNCTION: EXE 0x5b404a
// Resets, then reads triggers, conditions, effects, objects, areas, timed and pending effects (dropped when loading
// a scenario), ID counters, clocks, media strings, start camera and dialog messages; returns the first error
long EEScenarioManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->LoadUncaptured(inMajorVersion, inMinorVersion, inFile);
    if (EESimCaptureRecord::Active())
        EESMCaptureSaved(this, 0, theResult, inMajorVersion, inMinorVersion);
    return theResult;
}

// Load's body (Load adds the simulation capture's SMSV record: ee/EEScenarioCapture.h): after Reset, ee_scenario
// manager.rs load over the stream (the elements created and kept here: EEScenarioHost.cpp's StreamIO)
long EEScenarioManager::LoadUncaptured(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    this->Reset();
    UWideString* theStrings[] = {&this->mExportFileName, &this->mLastChatText};
    EESMStream theStream = {&inFile, theStrings, NULL, NULL, this, inMajorVersion, inMinorVersion};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_manager_load(&theTable, this, this->mTimedEffects, inMinorVersion);
}

namespace {

// a new element of a manager list read by its Load and kept as Load kept it: an "object 0" condition is freed, a
// pending effect only kept when a saved game (not a scenario) loads; freed on an error
template<class T> long EESMLoadElement(EESMStream* inStream, std::vector<T*>& ioList, uint32_t inKind)
{
    T* theElement = new T;
    long theResult = theElement->Load(inStream->mMajor, inStream->mMinor, *inStream->mFile);
    bool theKeep = theResult == kErrorOK;
    if (theKeep && inKind == EE_SCENARIO_ELEMENT_PENDING)
        theKeep = !GetIsLoadingScenario();
    if (theKeep)
        ioList.push_back(theElement);
    else
        delete theElement;
    return theResult;
}

template<class T> int32_t EESMListIO(EESMStream* inStream, std::vector<T*>& ioList, uint32_t inOp, uint32_t inKind,
                                     unsigned long inIndex, void* ioValue)
{
    switch (inOp) {
    case EE_SCENARIO_IO_ELEMENT_LOAD: return (int32_t)EESMLoadElement(inStream, ioList, inKind);
    case EE_SCENARIO_IO_ELEMENT_SAVE:
        return (int32_t)ioList[inIndex]->Save(inStream->mMajor, inStream->mMinor, *inStream->mFile);
    case EE_SCENARIO_IO_ELEMENT_COUNT: *static_cast<unsigned long*>(ioValue) = ioList.size(); return 0;
    }
    return 0;
}

}   // namespace

int32_t EESMManagerIO(EESMStream* inStream, uint32_t inOp, uint32_t inSlot, unsigned long inIndex, void* ioValue)
{
    EEScenarioManager* theManager = inStream->mManager;
    FSFile& theFile = *inStream->mFile;
    if (inOp == EE_SCENARIO_IO_MEDIA)
        return (int32_t)(inSlot == 0 ? theManager->mMediaStrings.Load(inStream->mMajor, inStream->mMinor, theFile)
                                     : theManager->mMediaStrings.Save(inStream->mMajor, inStream->mMinor, theFile));
    if (inOp == EE_SCENARIO_IO_MESSAGES)
        return (int32_t)(inSlot == 0 ? EESMLoadVector(inStream->mMajor, inStream->mMinor, theFile, theManager->mMessages)
                                     : EESMSaveVector(inStream->mMajor, inStream->mMinor, theFile, theManager->mMessages));
    if (inOp == EE_SCENARIO_IO_ELEMENT_LOAD && inSlot == EE_SCENARIO_ELEMENT_CONDITIONS) {
        // a condition naming object 0 is dropped (freed: it leaked)
        EESMCondition* theCondition = new EESMCondition;
        long theResult = theCondition->Load(inStream->mMajor, inStream->mMinor, theFile);
        if (theResult == kErrorOK && (theCondition->mType != kSMConditionObject || theCondition->mObjectIndex != 0))
            theManager->mConditions.push_back(theCondition);
        else
            delete theCondition;
        return (int32_t)theResult;
    }
    switch (inSlot) {
    case EE_SCENARIO_ELEMENT_TRIGGERS: return EESMListIO(inStream, theManager->mTriggers, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_CONDITIONS:
        return EESMListIO(inStream, theManager->mConditions, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_EFFECTS: return EESMListIO(inStream, theManager->mEffects, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_OBJECTS: return EESMListIO(inStream, theManager->mObjects, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_AREAS: return EESMListIO(inStream, theManager->mAreas, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_PENDING:
        return EESMListIO(inStream, theManager->mPendingEffects, inOp, inSlot, inIndex, ioValue);
    }
    return 0;
}

// FUNCTION: EXE 0x5b45cb
// Sets the run flag of the first timed entry of an effect whose flag differs
void EEScenarioManager::SetTimedEffectHasRun(EESMEffect* inEffect, bool inFlag)
{
    // an entry's effect is inEffect when its index is inEffect's place in mEffects (GetEffect: NULL past the end)
    unsigned long theIndex = std::find(this->mEffects.begin(), this->mEffects.end(), inEffect) - this->mEffects.begin();
    ee_scenario_timed_set_has_run(this->mTimedEffects, inEffect != NULL && theIndex < this->mEffects.size(), theIndex,
                                  inFlag);
}

// FUNCTION: EXE 0x5b481a
// Shows the next queued dialog message (in a cinematic the next cinematic one), stopping one-shot sounds for a
// voiced one and moving the camera to its unit; none left: hides the messages; nothing while paused
void EEScenarioManager::ShowDialogMessage()
{
    this->mIsShowingDialog = false;
    if (gCommunications == NULL || gCommunications->GetIsGamePaused())
        return;
    if (this->mMessages.size() > 0) {
        long theIndex;
        if (gWorld.mInCinematic) {
            theIndex = -1;
            for (unsigned long i = 0; i < this->mMessages.size(); i++) {
                if (this->mMessages[i].mIsCinematic) {
                    theIndex = i;
                    break;
                }
            }
        } else
            theIndex = 0;
        if (theIndex != -1) {
            if (this->GetDialogMessage(theIndex).mSound.GetLength() > 0)
                gSoundManager->StopAllOneShotSounds();
            gUserInterface->PostDialogMessage(this->mMessages[theIndex]);
            EEUnit* theUnit = gWorld.GetUnit(this->mMessages[theIndex].mUnitID);
            if (theUnit != NULL && this->mMessages[theIndex].mScrollSpeed >= 0) {
                gWorld.mCameraControl.ClearUnitA();
                gWorld.mCameraControl.ClearUnitB();
                GE3DPoint theOffset(-10000.0f, 0.0f, 0.0f);
                gWorld.mCameraControl.SetMode(0);
                float theAngle;
                if (this->mMessages[theIndex].mScrollSpeed == 0)
                    theAngle = 3.1415927f;
                else
                    theAngle = 0.0f;
                EESMDialogMessage& theMessage = this->mMessages[theIndex];
                EESMDialogMessage& theOther = this->mMessages[theIndex];
                gWorld.mCameraControl.MoveTo(theUnit, theOffset, true, (float)theOther.mScrollSpeed,
                                               theMessage.mScrollSpeed == 0 ? theUnit : NULL, theOffset, false, 0.1f,
                                               theAngle, 0.0f);
            }
            this->mMessages.erase(this->mMessages.begin() + theIndex);
            this->mIsShowingDialog = true;
            return;
        }
    }
    gUserInterface->PostHideMessages();
    gSoundManager->StopAllOneShotSounds();
}

// FUNCTION: EXE 0x5b4d1a
// Writes triggers, conditions, effects, objects and areas (not the built-ins), timed and pending effects, ID
// counters, clocks, media strings, start camera and dialog messages; returns the first write error (ee_scenario
// manager.rs save over the stream)
long EEScenarioManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mExportFileName, &this->mLastChatText};
    EESMStream theStream = {&outFile, theStrings, NULL, NULL, this, inMajorVersion, inMinorVersion};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_manager_save(&theTable, this, this->mTimedEffects, inMinorVersion);
}

// FUNCTION: EXE 0x5b5173
// Moves the camera one zoom-scaled step towards the scenario's start location; stops moving once it is there
void EEScenarioManager::MoveCameraToStart()
{
    GE3DPoint theLocation = gWorld.mLine.GetStart();
    if (theLocation.GetX() == this->mStartX && theLocation.GetY() == this->mStartY) {
        this->mIsMovingCamera = false;
        return;
    }
    float theStep = (gWorld.mCameraControl.mZoom + 0.5f) * gSideScrollSpeed;
    float theDX = theLocation.GetX() - this->mStartX;
    float theAbsDX = UMAbs(theDX);
    float theDY = theLocation.GetY() - this->mStartY;
    float theAbsDY = UMAbs(theDY);
    float theTotal = theAbsDY + theAbsDX;
    float theStepX = theAbsDX * theStep / theTotal;
    float theStepY = theAbsDY * theStep / theTotal;
    if (theStepX > UMAbs(theDX))
        theStepX = UMAbs(theDX);
    if (theLocation.GetX() > this->mStartX)
        theStepX = -theStepX;
    if (theStepY > UMAbs(theDY))
        theStepY = UMAbs(theDY);
    if (theLocation.GetY() > this->mStartY)
        theStepY = -theStepY;
    theLocation.SetX(theLocation.GetX() + theStepX);
    theLocation.SetY(theLocation.GetY() + theStepY);
    gWorld.mCameraControl.SetLocation(theLocation, false, true);
}

// FUNCTION: EXE 0x5b5652
// Empties a vector of 8-byte entries (vector<T>::clear; one body for every such vector)

// FUNCTION: EXE 0x5b568d
// Appends a trigger to its list (std::vector<EESMTrigger*>::push_back)

// FUNCTION: EXE 0x5b56b9
// Appends a condition to its list (std::vector<EESMCondition*>::push_back)

// FUNCTION: EXE 0x5b56e5
// Appends an effect to its list (std::vector<EESMEffect*>::push_back)

// FUNCTION: EXE 0x5b5711
// Erases one entry of a pointer list (vector<T*>::erase; one body for every pointer vector)

// FUNCTION: EXE 0x5b5772
// Empties a pointer list (vector<T*>::clear; one body for every pointer vector)

// FUNCTION: EXE 0x5b57ad
// Appends a scenario object to its list (std::vector<EESMObject*>::push_back)

// FUNCTION: EXE 0x5b57d9
// Appends an area to its list (std::vector<EESMArea*>::push_back)

// FUNCTION: EXE 0x5b5805
// Appends a dialog message to its list (std::vector<EESMDialogMessage>::push_back)

// FUNCTION: EXE 0x5b5831
// Erases one dialog message from the list (std::vector<EESMDialogMessage>::erase)

// TEMPLATE: EXE 0x5b58a6
// ??H?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@std@@QBE?AV01@H@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b58b8
// Appends a timed effect entry to its list (std::vector<EESMTimedEffect>::push_back)

// FUNCTION: EXE 0x5b58e4
// Erases one timed effect entry from the list (std::vector<EESMTimedEffect>::erase)

// FUNCTION: EXE 0x5b5945
// Empties a list of timed effects (std::vector<EESMTimedEffect>::clear)

// FUNCTION: EXE 0x5b5980
// Inserts a dialog message at a position in its list (std::vector<EESMDialogMessage>::insert)

// TEMPLATE: EXE 0x5b59d0
// ?_Buy@?$vector@VEESMDialogMessage@@V?$allocator@VEESMDialogMessage@@@std@@@std@@IAE_NI@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b5a09
// ?_Tidy@?$vector@VEESMDialogMessage@@V?$allocator@VEESMDialogMessage@@@std@@@std@@IAEXXZ (Dinkum COMDAT)

// FUNCTION: EXE 0x5b5a43
// Inserts n copies of a dialog message at a position in its list (std::vector<EESMDialogMessage>::insert)

// FUNCTION: EXE 0x5b5cfd
// Destroys a dialog message's text and sound strings
// (EESMDialogMessage's implicit dtor: destroys +0x18, +0x04, no vptr store; emitted for vector<EESMDialogMessage>)

// TEMPLATE: EXE 0x5b5d14
// ?_Ufill@?$vector@VEESMDialogMessage@@V?$allocator@VEESMDialogMessage@@@std@@@std@@IAEPAVEESMDialogMessage@@PAV3@IABV3@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b5d3f
// ??G?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@std@@QBE?AV01@H@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b5d53
// Deletes every trigger in a list and empties it (DeleteVectorContents<EESMTrigger*>)

// FUNCTION: EXE 0x5b5d90
// Deletes every condition in a list and empties it (DeleteVectorContents<EESMCondition*>)

// FUNCTION: EXE 0x5b5dcd
// Deletes every effect in a list and empties it (DeleteVectorContents<EESMEffect*>)

// FUNCTION: EXE 0x5b5e0a
// Deletes every scenario object in a list and empties it (DeleteVectorContents<EESMObject*>)

// FUNCTION: EXE 0x5b5e3a
// Deletes every area in a list and empties it (DeleteVectorContents<EESMArea*>)

// FUNCTION: EXE 0x5b5e77
// Sorts the timed effects by due time, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5eab
// Sorts the areas by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5edf
// Sorts the conditions by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5f13
// Sorts the effects by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5f47
// Sorts the scenario objects by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5f7b
// Sorts the triggers by name, keeping the order of equal ones (std::stable_sort)

// FUNCTION: EXE 0x5b5faf
// Reads the dialog message list: the count, then each message; returns the first read error (EESMLoadVector)

// FUNCTION: EXE 0x5b5fff
// Writes the dialog message list: the count, then each message; returns the first write error (EESMSaveVector)

// FUNCTION: EXE 0x5b6054
// Copies a range of dialog messages (std::copy)

// TEMPLATE: EXE 0x5b607d
// ?_Ucopy@?$vector@VEESMDialogMessage@@V?$allocator@VEESMDialogMessage@@@std@@@std@@IAEPAVEESMDialogMessage@@V?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@2@0PAV3@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b60a9
// ?fill@std@@YAXV?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@1@0ABVEESMDialogMessage@@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b60c6
// ?copy_backward@std@@YA?AV?$_Ptrit@VEESMDialogMessage@@HPAV1@AAV1@PAV1@AAV1@@1@V21@00@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b60f2
// ?fill@std@@YAXV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@0ABUEESMEntry164@@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b6113
// ?_Construct@std@@YAXPAVEESMDialogMessage@@ABV2@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b6125
// Destroys a dialog message and frees it when asked (EESMDialogMessage scalar deleting destructor)

// FUNCTION: EXE 0x5b614e
// Resizes the dialog message list, padding with empty messages (vector<EESMDialogMessage>::resize)

// FUNCTION: EXE 0x5b616a
// Resizes the dialog message list, padding with copies of a message (vector<EESMDialogMessage>::resize)

// FUNCTION: EXE 0x5b6225
// Erases a range of dialog messages from the list (std::vector<EESMDialogMessage>::erase)

// FUNCTION: EXE 0x5b6290
// Gets a temporary buffer and runs the stable sort of the timed effects by due time (std::_Stable_sort)

// FUNCTION: EXE 0x5b62fd
// Gets a temporary buffer and runs the stable sort of the areas by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b636a
// Gets a temporary buffer and runs the stable sort of the conditions by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b63d7
// Gets a temporary buffer and runs the stable sort of the effects by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b6444
// Gets a temporary buffer and runs the stable sort of the scenario objects by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b64b1
// Gets a temporary buffer and runs the stable sort of the triggers by name (std::_Stable_sort)

// FUNCTION: EXE 0x5b651e
// Copies a range of dialog messages (std::copy)

// TEMPLATE: EXE 0x5b6542
// ??0?$_Temp_iterator@UEESMTimedEffect@@@std@@QAE@H@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b658a
// ??0?$_Temp_iterator@PAVEESMArea@@@std@@QAE@H@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b65d1
// Counts the entries in a range of the timed effects (std::_Distance)

// FUNCTION: EXE 0x5b65fc
// Stable-sorts the timed effects with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b6715
// Stable-sorts the areas with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b682b
// Stable-sorts the conditions with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b6941
// Stable-sorts the effects with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b6a57
// Stable-sorts the scenario objects with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// FUNCTION: EXE 0x5b6b6d
// Stable-sorts the triggers with the temporary buffer, merging in place when it is short (std::_Stable_sort)

// TEMPLATE: EXE 0x5b6c83
// ?_Distance@std@@YAXV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@0AAHUrandom_access_iterator_tag@1@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b6c98
// Insertion-sorts a short run of the timed effects by due time (std::_Insertion_sort)

// FUNCTION: EXE 0x5b6cc1
// Merge-sorts the timed effects by due time through the temporary buffer (std::_Buffered_merge_sort)

// FUNCTION: EXE 0x5b6da8
// Merges two sorted runs of the timed effects, through the buffer when they fit (std::_Buffered_merge)

// TEMPLATE: EXE 0x5b70cf
// ?_Distance@std@@YAXV?$_Ptrit@PAVEESMArea@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0AAHUrandom_access_iterator_tag@1@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b70e1
// Merges two sorted runs of the areas, through the buffer when they fit (std::_Buffered_merge)

// FUNCTION: EXE 0x5b7407
// Merges two sorted runs of the conditions, through the buffer when they fit (std::_Buffered_merge)

// FUNCTION: EXE 0x5b772d
// Insertion-sorts a short run of the effects by name (std::_Insertion_sort)

// FUNCTION: EXE 0x5b7756
// Merge-sorts the effects by name through the temporary buffer (std::_Buffered_merge_sort)

// FUNCTION: EXE 0x5b783d
// Merges two sorted runs of the effects, through the buffer when they fit (std::_Buffered_merge)

// FUNCTION: EXE 0x5b7b63
// Merges two sorted runs of the scenario objects, through the buffer when they fit (std::_Buffered_merge)

// FUNCTION: EXE 0x5b7e89
// Insertion-sorts a short run of the areas by name (std::_Insertion_sort)

// FUNCTION: EXE 0x5b7eb2
// Merge-sorts the areas by name through the temporary buffer (std::_Buffered_merge_sort)

// FUNCTION: EXE 0x5b7f99
// Merges two sorted runs of the triggers, through the buffer when they fit (std::_Buffered_merge)

// TEMPLATE: EXE 0x5b82bf
// ?_First@?$_Temp_iterator@UEESMTimedEffect@@@std@@QBEPAUEESMEntry164@@XZ (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b82c5
// ?_Last@?$_Temp_iterator@UEESMTimedEffect@@@std@@QBEPAUEESMEntry164@@XZ (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b82cc
// ?_Init@?$_Temp_iterator@UEESMTimedEffect@@@std@@QAEAAV12@XZ (Dinkum COMDAT)

// FUNCTION: EXE 0x5b82d7
// Insertion-sorts a run of the timed effects by due time (std::_Insertion_sort_1)

// TEMPLATE: EXE 0x5b8381
// ??E?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@std@@QAEAAV01@XZ (Dinkum COMDAT)

// FUNCTION: EXE 0x5b8387
// Merges adjacent sorted chunks of the timed effects between list and buffer (std::_Chunked_merge)

// FUNCTION: EXE 0x5b84cb
// Merges adjacent sorted chunks of the timed effects between list and buffer (std::_Chunked_merge)

// TEMPLATE: EXE 0x5b8578
// ?copy@std@@YA?AV?$_Temp_iterator@UEESMTimedEffect@@@1@V?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@0V21@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b85b5
// Merges two sorted runs of the timed effects into a destination (std::merge)

// FUNCTION: EXE 0x5b8652
// Merges two sorted runs of the timed effects from the back (std::_Merge_backward)

// FUNCTION: EXE 0x5b8700
// Finds the first of the timed effects not ordered before a value (std::lower_bound)

// FUNCTION: EXE 0x5b8738
// Finds the first of the timed effects ordered after a value (std::upper_bound)

// FUNCTION: EXE 0x5b8770
// Rotates a range of the timed effects, through the buffer when it fits (std::_Buffered_rotate)

// FUNCTION: EXE 0x5b8915
// Insertion-sorts a run of the areas by name (std::_Insertion_sort_1)

// FUNCTION: EXE 0x5b89c1
// Merges adjacent sorted chunks of the areas between list and buffer (std::_Chunked_merge)

// TEMPLATE: EXE 0x5b8a59
// ?copy@std@@YA?AV?$_Temp_iterator@PAVEESMArea@@@1@V?$_Ptrit@PAVEESMArea@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0V21@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b8a96
// Merges two sorted runs of the areas from the back (std::_Merge_backward)

// FUNCTION: EXE 0x5b8b41
// Insertion-sorts a run of the effects by name (std::_Insertion_sort_1)

// FUNCTION: EXE 0x5b8bed
// Merges adjacent sorted chunks of the effects between list and buffer (std::_Chunked_merge)

// FUNCTION: EXE 0x5b8d29
// Merges adjacent sorted chunks of the effects between list and buffer (std::_Chunked_merge)

// FUNCTION: EXE 0x5b8dc1
// Merges two sorted runs of the effects into a destination (std::merge)

// FUNCTION: EXE 0x5b8e57
// Merges two sorted runs of the effects from the back (std::_Merge_backward)

// FUNCTION: EXE 0x5b8f02
// Finds the first of the effects not ordered before a value (std::lower_bound)

// FUNCTION: EXE 0x5b8f3a
// Finds the first of the effects ordered after a value (std::upper_bound)

// FUNCTION: EXE 0x5b8f72
// Merges adjacent sorted chunks of the areas between list and buffer (std::_Chunked_merge)

// FUNCTION: EXE 0x5b90ae
// Rotates a range of the areas, through the buffer when it fits (std::_Buffered_rotate)

// FUNCTION: EXE 0x5b9250
// Merges two sorted runs of the areas into a destination (std::merge)

// FUNCTION: EXE 0x5b92e6
// Finds the first of the areas not ordered before a value (std::lower_bound)

// FUNCTION: EXE 0x5b931e
// Finds the first of the areas ordered after a value (std::upper_bound)

// TEMPLATE: EXE 0x5b9356
// ??4?$_Temp_iterator@UEESMTimedEffect@@@std@@QAEAAV01@ABUEESMEntry164@@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b9395
// ??4?$_Temp_iterator@PAVEESMArea@@@std@@QAEAAV01@ABQAVEESMArea@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b93d2
// Moves one of the timed effects back to its sorted place (std::_Unguarded_insert)

// FUNCTION: EXE 0x5b9412
// Merges two sorted runs of the timed effects into the temporary buffer (std::merge)

// FUNCTION: EXE 0x5b94f2
// Merges two sorted runs of the timed effects into a destination (std::merge)

// TEMPLATE: EXE 0x5b957e
// ?copy@std@@YA?AV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@V21@00@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b95ab
// ?_Iter_swap@std@@YAXV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@0PAUEESMEntry164@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b95d2
// Finds the first of the timed effects not ordered before a value (std::_Lower_bound)

// FUNCTION: EXE 0x5b9657
// Finds the first of the timed effects ordered after a value (std::_Upper_bound)

// FUNCTION: EXE 0x5b96dc
// Rotates a range of the timed effects (std::rotate)

// FUNCTION: EXE 0x5b971d
// Finds the first of the areas ordered after a value (std::_Upper_bound)

// FUNCTION: EXE 0x5b979d
// Rotates a range of the areas (std::rotate)

// FUNCTION: EXE 0x5b97de
// Finds the first of the areas not ordered before a value (std::_Lower_bound)

// TEMPLATE: EXE 0x5b985f
// ?_Unguarded_insert@std@@YAXV?$_Ptrit@PAVEESMEffect@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@PAVEESMEffect@@UEESMEffectLess@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b98aa
// Merges two sorted runs of the effects into the temporary buffer (std::merge)

// FUNCTION: EXE 0x5b998c
// Merges two sorted runs of the effects into a destination (std::merge)

// FUNCTION: EXE 0x5b9a0c
// Finds the first of the effects not ordered before a value (std::_Lower_bound)

// FUNCTION: EXE 0x5b9a8d
// Finds the first of the effects ordered after a value (std::_Upper_bound)

// FUNCTION: EXE 0x5b9b0d
// Merges two sorted runs of the areas into the temporary buffer (std::merge)

// FUNCTION: EXE 0x5b9bef
// Merges two sorted runs of the areas into a destination (std::merge)

// TEMPLATE: EXE 0x5b9c6f
// ?copy@std@@YA?AV?$_Ptrit@PAVEESMTrigger@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@V21@00@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b9c93
// ?_Unguarded_insert@std@@YAXV?$_Ptrit@PAVEESMArea@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@PAVEESMArea@@UEESMAreaLess@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b9cde
// Rotates a range of the timed effects by element swaps (std::_Rotate)

// FUNCTION: EXE 0x5b9d12
// Rotates a range of the areas by element swaps (std::_Rotate)

// TEMPLATE: EXE 0x5b9d46
// ?_Rotate@std@@YAXV?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@1@00PAHPAUEESMEntry164@@@Z (Dinkum COMDAT)

// TEMPLATE: EXE 0x5b9e20
// ?_Rotate@std@@YAXV?$_Ptrit@PAVEESMArea@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@00PAHPAPAVEESMArea@@@Z (Dinkum COMDAT)

// FUNCTION: EXE 0x5b9eb8
// Creates an empty dialog message: no unit or duration, no camera move (scroll speed -1), not for a cinematic
EESMDialogMessage::EESMDialogMessage()
{
    this->mScrollSpeed = -1;
    this->mUnitType = 0;
    this->mDuration = 0;
    this->mUnitID = 0;
    this->mIsCinematic = false;
}

// FUNCTION: EXE 0x5b9ee9
// Creates a copy of a dialog message
EESMDialogMessage::EESMDialogMessage(const EESMDialogMessage& inMessage)
{
    this->mText = inMessage.mText;
    this->mUnitType = inMessage.mUnitType;
    this->mSound = inMessage.mSound;
    this->mDuration = inMessage.mDuration;
    this->mUnitID = inMessage.mUnitID;
    this->mScrollSpeed = inMessage.mScrollSpeed;
    this->mIsCinematic = inMessage.mIsCinematic;
}

// FUNCTION: EXE 0x5b9f4a
// Copies another dialog message's text, unit, sound, duration, scroll speed and cinematic flag
void EESMDialogMessage::operator=(const EESMDialogMessage& inMessage)
{
    this->mText = inMessage.mText;
    this->mUnitType = inMessage.mUnitType;
    this->mSound = inMessage.mSound;
    this->mDuration = inMessage.mDuration;
    this->mUnitID = inMessage.mUnitID;
    this->mScrollSpeed = inMessage.mScrollSpeed;
    this->mIsCinematic = inMessage.mIsCinematic;
}

// TEMPLATE: EXE 0x5ba01a
// ??H?$_Ptrit@UEESMTimedEffect@@HPAU1@AAU1@PAU1@AAU1@@std@@QBE?AV01@H@Z (Dinkum COMDAT)

// ------------------------------------------------------------------------------------------------ the lists
// The editor's list operations, the name lists and the id searches are ee_scenario lists.rs (with the index upkeep
// each change makes: the Renumber/Remap broadcasts over every element, the elements' own and EERemapID); the vectors
// and elements stay here, reached by operation (EESMListOp)

namespace {
struct EESMListContext {
    EEScenarioManager* mManager;
    std::vector<UWideString>* mNames;
    UWideString mText;
};

unsigned long& EESMListID(EESMObject* inObject) { return inObject->mL64; }
template<class T> unsigned long& EESMListID(T* inElement) { return inElement->mID; }
void EESMListSetName(EESMObject* inObject, const char* inName) { inObject->SetName(inName); }
template<class T> void EESMListSetName(T* inElement, const char* inName) { inElement->mName = inName; }
void* EESMListFields(void*) { return NULL; }
void* EESMListFields(EeScenarioConditionFields* inFields) { return inFields; }
void* EESMListFields(EeScenarioEffectFields* inFields) { return inFields; }
void* EESMListFields(EeScenarioObjectFields* inFields) { return inFields; }

// One list's operations (EE_SCENARIO_LIST_OP_*)
template<class T> void* EESMListEdit(std::vector<T*>& ioList, uint32_t inOp, size_t inIndex, void* inElement, const char* inText)
{
    T* theElement = static_cast<T*>(inElement);
    switch (inOp) {
    case EE_SCENARIO_LIST_OP_COUNT: return (void*)(uintptr_t)ioList.size();
    case EE_SCENARIO_LIST_OP_AT: return inIndex < ioList.size() ? ioList[inIndex] : NULL;
    case EE_SCENARIO_LIST_OP_SET: ioList[inIndex] = theElement; break;
    case EE_SCENARIO_LIST_OP_INSERT: ioList.insert(ioList.begin() + inIndex, theElement); break;
    case EE_SCENARIO_LIST_OP_ERASE: ioList.erase(ioList.begin() + inIndex); break;
    case EE_SCENARIO_LIST_OP_PUSH: ioList.push_back(theElement); break;
    case EE_SCENARIO_LIST_OP_CREATE: return new T;
    case EE_SCENARIO_LIST_OP_COPY: theElement->Copy(*ioList[inIndex]); break;
    case EE_SCENARIO_LIST_OP_DESTROY: delete theElement; break;
    case EE_SCENARIO_LIST_OP_NAME: return (void*)(const char*)theElement->mName;
    case EE_SCENARIO_LIST_OP_SET_NAME: EESMListSetName(theElement, inText); break;
    case EE_SCENARIO_LIST_OP_ID: return (void*)(uintptr_t)EESMListID(theElement);
    case EE_SCENARIO_LIST_OP_SET_ID: EESMListID(theElement) = (unsigned long)inIndex; break;
    case EE_SCENARIO_LIST_OP_FIELDS: return EESMListFields(theElement);
    }
    return NULL;
}

void* EESMListOp(void* inContext, uint32_t inOp, uint32_t inList, uintptr_t inIndex, void* inElement, const char* inText)
{
    EESMListContext* theContext = static_cast<EESMListContext*>(inContext);
    EEScenarioManager* m = theContext->mManager;
    EESMTrigger* theTrigger = static_cast<EESMTrigger*>(inElement);
    switch (inOp) {
    case EE_SCENARIO_LIST_OP_OBJECT_COUNT: return (void*)(uintptr_t)static_cast<EESMEffect*>(inElement)->mObjects.size();
    case EE_SCENARIO_LIST_OP_OBJECTS: return static_cast<EESMEffect*>(inElement)->mObjects.data();
    case EE_SCENARIO_LIST_OP_ENTRY_COUNT:
        return (void*)(uintptr_t)(inList ? theTrigger->mEffects.size() : theTrigger->mConditions.size());
    case EE_SCENARIO_LIST_OP_ENTRY:
        return inList ? (void*)&theTrigger->mEffects[inIndex]->mEffectIndex : (void*)&theTrigger->mConditions[inIndex]->mValue;
    case EE_SCENARIO_LIST_OP_RESOURCE:
        gResource->GetString(inIndex ? IDS_EXISTS : IDS_COPY, theContext->mText);
        return (void*)(const char*)theContext->mText;
    case EE_SCENARIO_LIST_OP_OUT_NAME: theContext->mNames->push_back(UWideString(inText)); return NULL;
    }
    switch (inList) {
    case EE_SCENARIO_LIST_TRIGGERS: return EESMListEdit(m->mTriggers, inOp, inIndex, inElement, inText);
    case EE_SCENARIO_LIST_CONDITIONS: return EESMListEdit(m->mConditions, inOp, inIndex, inElement, inText);
    case EE_SCENARIO_LIST_EFFECTS: return EESMListEdit(m->mEffects, inOp, inIndex, inElement, inText);
    case EE_SCENARIO_LIST_OBJECTS: return EESMListEdit(m->mObjects, inOp, inIndex, inElement, inText);
    case EE_SCENARIO_LIST_AREAS: return EESMListEdit(m->mAreas, inOp, inIndex, inElement, inText);
    case EE_SCENARIO_LIST_PENDING: return EESMListEdit(m->mPendingEffects, inOp, inIndex, inElement, inText);
    }
    return NULL;
}

struct EESMLists {
    EESMListContext mContext;
    EeScenarioListHost mHost;
    EESMLists(EEScenarioManager* inManager, std::vector<UWideString>* inNames = NULL) : mContext{inManager, inNames}
    {
        this->mHost = {&this->mContext, inManager, inManager->mTimedEffects, EESMListOp};
    }
};
}   // namespace

// The editor's Sort, New, MoveUp/MoveDown, Delete and Duplicate of a list (EE_SCENARIO_LIST_*: 0x1b00c1..0x1b0674,
// 0x1b074c..0x1b0b65, 0x1b0beb..0x1b0e71 and 0x1b3e75..0x1b4017, 0x1b0ec6..0x1b10ed, 0x1b120d..0x1b1978)
void EEScenarioManager::SortList(unsigned long inList) 
{
    EESMLists theLists(this);
    ee_scenario_list_sort(&theLists.mHost, inList);
}
void* EEScenarioManager::NewEntry(unsigned long inList) 
{
    EESMLists theLists(this);
    return ee_scenario_list_new(&theLists.mHost, inList);
}
void EEScenarioManager::MoveEntry(unsigned long inList, unsigned long inIndex, bool inUp)
{
    EESMLists theLists(this);
    ee_scenario_list_move(&theLists.mHost, inList, inIndex, inUp);
}
bool EEScenarioManager::DeleteEntry(unsigned long inList, unsigned long inIndex)
{
    EESMLists theLists(this);
    return ee_scenario_list_delete(&theLists.mHost, inList, inIndex);
}
void* EEScenarioManager::DuplicateEntry(unsigned long inList, unsigned long inIndex)
{
    EESMLists theLists(this);
    return ee_scenario_list_duplicate(&theLists.mHost, inList, inIndex);
}

// The name lists (EE_SCENARIO_NAMES_*: 0x1b3317..0x1b3a2a)
void EEScenarioManager::GetNames(unsigned long inWhich, std::vector<UWideString>& outNames)
{
    EESMLists theLists(this, &outNames);
    ee_scenario_list_names(&theLists.mHost, inWhich);
}

// FUNCTION: EXE 0x5b322d
EESMCondition* EEScenarioManager::FindConditionByID(long inID)
{
    EESMLists theLists(this);
    return static_cast<EESMCondition*>(ee_scenario_list_find(&theLists.mHost, EE_SCENARIO_LIST_CONDITIONS, inID));
}

// FUNCTION: EXE 0x5b3269
EESMEffect* EEScenarioManager::FindEffectByID(long inID)
{
    EESMLists theLists(this);
    return static_cast<EESMEffect*>(ee_scenario_list_find(&theLists.mHost, EE_SCENARIO_LIST_EFFECTS, inID));
}

// FUNCTION: EXE 0x5b32a8
EESMObject* EEScenarioManager::FindObjectByID(long inID)
{
    EESMLists theLists(this);
    return static_cast<EESMObject*>(ee_scenario_list_find(&theLists.mHost, EE_SCENARIO_LIST_OBJECTS, inID));
}

// FUNCTION: EXE 0x5b32e4
long EEScenarioManager::GetObjectIndex(EESMObject* inObject)
{
    EESMLists theLists(this);
    return ee_scenario_list_index_of(&theLists.mHost, EE_SCENARIO_LIST_OBJECTS, inObject);
}

// EESMObject::SetName's conditions (inParent false) or SetParent (true) over the lists
void EEScenarioManager::ObjectChanged(EESMObject* inObject, bool inParent, long inValue)
{
    EESMLists theLists(this);
    if (inParent)
        ee_scenario_list_set_parent(&theLists.mHost, inObject, inValue);
    else
        ee_scenario_list_rename_object_conditions(&theLists.mHost, inObject);
}

// ------------------------------------------------------------------------------------------------ the text file
// The trigger text file of the instructions tab (EESEIAExport, EESEIAImport) is ee_scenario text.rs: ExportText
// 0x1b23dc, ImportTriggerText 0x1b3ae9 with ValidateText 0x1b52bb, ImportText 0x1b4a95 and the line parsers
// (ParseConditionID 0x1b463b, ParseEffectID 0x1b46bf, ParsePlayerName 0x1b4796). The elements' texts, the players'
// names, the media strings' sections, the file and the language's error formats stay here, reached by operation
// (EESMTextOp)

namespace {
struct EESMTextContext {
    EEScenarioManager* mManager;
    FSFileSpec* mFile;
    char mBuffer[0x200];
};

// an element's text by slot (EE_SCENARIO_TEXT_*: an effect's three, a condition's chat text)
UWideString& EESMText(void* inElement, uint32_t inSlot)
{
    EESMEffect* theEffect = static_cast<EESMEffect*>(inElement);
    switch (inSlot) {
    case EE_SCENARIO_TEXT_DESCRIPTION: return theEffect->mDescription;
    case EE_SCENARIO_TEXT_MESSAGE: return theEffect->mMessage;
    case EE_SCENARIO_TEXT_NEW_NAME: return theEffect->mNewName;
    }
    return static_cast<EESMCondition*>(inElement)->mChatText;
}

// the import's messages by EE_SCENARIO_TEXT_ERROR_*
const unsigned long kSMTextErrors[] = {IDS_IS_INVALID_TOKEN, IDS_IS_INVALID_EFFECT_ID_FOR_TOKEN,
                                       IDS_IS_INVALID_EFFECT_TYPE_TO_BE_TOKEN,
                                       IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS,
                                       IDS_IS_INVALID_CONDITION_ID_FOR_TOKEN, IDS_IS_INVALID_CONDITION_TYPE_TO_BE_TOKEN,
                                       IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS_2747,
                                       IDS_INVALID_PLAYER_NUMBER_FOR_THIS_SCENARIO};

void* EESMTextOp(void* inContext, uint32_t inOp, uint32_t inSlot, long inValue, void* inElement, const char* inText,
                 const char* inText2)
{
    EESMTextContext* theContext = static_cast<EESMTextContext*>(inContext);
    EEScenarioManager* m = theContext->mManager;
    FILE* theFile = *theContext->mFile;
    EEPlayer* thePlayer;
    switch (inOp) {
    case EE_SCENARIO_TEXT_OP_GET: return (void*)(const char*)EESMText(inElement, inSlot);
    case EE_SCENARIO_TEXT_OP_SET: EESMText(inElement, inSlot) = inText; break;
    case EE_SCENARIO_TEXT_OP_PLAYERS: return (void*)(uintptr_t)gWorld.mNumPlayers;
    case EE_SCENARIO_TEXT_OP_PLAYER_NAME:
        thePlayer = gWorld.GetPlayer(inValue);
        return thePlayer != NULL ? (void*)(const char*)thePlayer->mName : NULL;
    case EE_SCENARIO_TEXT_OP_SET_PLAYER_NAME: gWorld.GetPlayer(inValue)->mName = inText; break;
    case EE_SCENARIO_TEXT_OP_WRITE: return fprintf(theFile, "%s", inText) < 0 ? NULL : theContext;
    case EE_SCENARIO_TEXT_OP_EOF: return feof(theFile) ? theContext : NULL;
    case EE_SCENARIO_TEXT_OP_READ: return fgets(theContext->mBuffer, sizeof(theContext->mBuffer), theFile);
    case EE_SCENARIO_TEXT_OP_CLEAR_ERRORS: m->mImportErrors.clear(); break;
    case EE_SCENARIO_TEXT_OP_ERROR: {
        UWideString theFormat;
        UWideString theMessage;
        gResource->GetString(kSMTextErrors[inSlot], theFormat);
        if (inSlot == EE_SCENARIO_TEXT_ERROR_EFFECT_ID || inSlot == EE_SCENARIO_TEXT_ERROR_CONDITION_ID
            || inSlot == EE_SCENARIO_TEXT_ERROR_PLAYER)
            sprintf(theMessage, theFormat, inValue, inText);
        else
            sprintf(theMessage, theFormat, inText, inText2);
        m->mImportErrors.push_back(theMessage);
        break;
    }
    case EE_SCENARIO_TEXT_OP_MEDIA: return (void*)(const char*)m->mMediaStrings.GetSection(inSlot);
    case EE_SCENARIO_TEXT_OP_SET_MEDIA: m->mMediaStrings.GetSection(inSlot) = inText; break;
    }
    return NULL;
}

struct EESMTextFile {
    EESMTextContext mContext;
    EESMLists mLists;
    EeScenarioTextHost mHost;
    EESMTextFile(EEScenarioManager* inManager, FSFileSpec& inFile) : mContext{inManager, &inFile}, mLists(inManager)
    {
        this->mHost = {&this->mLists.mHost, &this->mContext, EESMTextOp};
    }
};
}   // namespace

// FUNCTION: EXE 0x5b23dc
// Writes the trigger text file (EESEIAExport): the media strings, then the effect, condition and player name
// entries; 9 on a write error
long EEScenarioManager::ExportText(FSFileSpec& inFile)
{
    EESMTextFile theText(this, inFile);
    return ee_scenario_text_export(&theText.mHost);
}

// FUNCTION: EXE 0x5b3ae9
// Reads a trigger text file (EESEIAImport) of "{id_Kind:name}" lines, each followed by its text; with inValidate
// lists the bad entries first (mImportErrors), then applies them; 9 on a bad entry
long EEScenarioManager::ImportTriggerText(FSFileSpec& inFile, unsigned long inKind, bool inValidate)
{
    EESMTextFile theText(this, inFile);
    return ee_scenario_text_import(&theText.mHost, inKind, inValidate);
}
