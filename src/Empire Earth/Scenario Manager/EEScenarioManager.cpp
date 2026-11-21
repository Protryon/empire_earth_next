// C:\EEX\Empire Earth\Scenario Manager\EEScenarioManager.cpp (file name inferred: no __FILE__): 0x1afa90..0x1ba02f,
// with the functions of EESMDialogMessage.cpp folded in. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include <vector>
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
#include "ee/EEDebugTrace.h"
#include "lle/SESoundManager.h"
#include "GEWindow.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGameSound.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"
#include "ee/EEStringIDs.h"
#include "ee/EEPersistentTag.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"

bool gScenarioSimDialogs = false;

extern float gF4BE374;                       // 0x4be374 (= 1.0f; Update ends the cinematic when it is 1; provisional name)
extern float& gSideScrollSpeed;               // 0x530828 (MoveCameraToStart's camera step scale; provisional name)

namespace {
int64_t RoundOp(void* inContext, uint32_t inOp, int64_t inA, int64_t inB, float inTime);
}
// the manager's round host (ee_scenario round.rs, cinematic.rs)
static EeScenarioRoundHost EESMRoundHost(EEScenarioManager* inManager)
{
    return {inManager, inManager, inManager->mTimedEffects, RoundOp};
}

// Creates a scenario holding only the default entries (see Reset)
EEScenarioManager::EEScenarioManager()
    : FSPersistent(kPersistentScenario), EeScenarioManagerFields()
{
    this->mTimedEffects = ee_scenario_timed_new();
    this->Reset();
}

static void EESMResetLists(EEScenarioManager* inManager);

// Empties the scenario back to its defaults (an Always True condition, a Do Nothing effect, a None object, the
// Off Map and Previous Location areas); zeroes the clocks and ID counters, clears the timed and pending effects
void EEScenarioManager::Reset()
{
    ee_scenario_manager_reset(this, this->mTimedEffects);   // the numbers, the timed effects (manager.rs reset)
    EESMResetLists(this);   // the lists and their built-ins (lists.rs reset)
    this->mPendingEffects.clear();
}

// Destroys the scenario: unloads the unit class media Prepare loaded and deletes every trigger, condition, effect,
// object, area and pending effect
EEScenarioManager::~EEScenarioManager()
{
    ee_scenario_manager_destroy(EESMRoundHost(this));   // ee_scenario manager_compose.rs destroy
}

// Queues a dialog message for ShowDialogMessage
void EEScenarioManager::AddMessage(const EESMDialogMessage& inMessage)
{
    this->mMessages.push_back(inMessage);
}

// Records a copy of an applied effect whose change a saved game must replay on load (ignored until
// ApplyPendingEffects has run)
void EEScenarioManager::AddV134Effect(const EESMEffect& inEffect)
{
    ee_scenario_add_pending(EESMRoundHost(this), &inEffect);   // ee_scenario manager_compose.rs add_pending
}

// Schedules an effect (by index) to run at the given scenario time, keeping the timed effects sorted by time
void EEScenarioManager::AddTimedEffect(long inEffect, unsigned long inTime, bool inIsInCinematic)
{
    ee_scenario_timed_add(this->mTimedEffects, inEffect, inTime, inIsInCinematic);
}

// Creates a new, default scenario manager (the persistent object factory)
FSPersistent* EEScenarioManager::CreateDefault() const
{
    return new EEScenarioManager;
}

// Ends a cinematic: delays the other timed effects and trigger timers by its length + 3 s, drops its unrun effects
// and dialog messages, delays the wonder timers and restores the camera saved by StartCinematic
// Called by EEMEndCinematic::Execute 0xbdf7d
// NEAR: the two discarded expressions per pending entry (a compiled-out trace) and the inline budget of begin()
void EEScenarioManager::EndCinematic()
{
    ee_scenario_end_cinematic(EESMRoundHost(this));   // cinematic.rs end_cinematic
}

// Leaves the cinematic once its letterbox is gone (Update): restores the game speed, the main game form, the
// mouse, fades in and stops the one-shot sounds
void EEScenarioManager::ExitCinematic()
{
    ee_scenario_exit_cinematic(EESMRoundHost(this));   // cinematic.rs exit_cinematic
}

// Starts a cinematic (EESMEffect 0x1bd86c): saves the game speed and camera, closes the main game form, locks the
// cursor, unselects every player's units, drops the non-cinematic dialog messages and letterboxes the screen
void EEScenarioManager::StartCinematic()
{
    ee_scenario_start_cinematic(EESMRoundHost(this));   // cinematic.rs start_cinematic
}

namespace {

GE3DPoint sRoundStart;   // MoveCameraToStart's point (the original's local; manager_compose.rs)

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
    case EE_SCENARIO_ROUND_DIALOG_PAUSED: return gCommunications == NULL || gCommunications->GetIsGamePaused();
    case EE_SCENARIO_ROUND_MESSAGE_CINEMATIC: return theManager->mMessages[(size_t)inA].mIsCinematic;
    case EE_SCENARIO_ROUND_IN_CINEMATIC: return gWorld.mInCinematic;
    case EE_SCENARIO_ROUND_MOVE_CAMERA: theManager->MoveCameraToStart(); return 0;
    case EE_SCENARIO_ROUND_TRIGGER_COUNT: return (int64_t)theManager->mTriggers.size();
    case EE_SCENARIO_ROUND_EVALUATE_TRIGGER: theManager->GetTrigger((unsigned long)inA)->Evaluate(inTime); return 0;
    case EE_SCENARIO_ROUND_EFFECT_EXISTS: return theManager->mEffects[(unsigned long)inA] != NULL;
    case EE_SCENARIO_ROUND_EXECUTE: theManager->mEffects[(unsigned long)inA]->Execute(inB != 0); return 0;
    case EE_SCENARIO_ROUND_CLEAR_CHAT: theManager->mLastChatText.Clear(); return 0;
    case EE_SCENARIO_ROUND_CAPTURE_RECORD: EESMCaptureUpdate(theManager, inA != 0);   // TEST ONLY (SMUP)
        return 0;
    case EE_SCENARIO_ROUND_DELETE_TRIGGERS: DeleteVectorContents(theManager->mTriggers);
        return 0;
    case EE_SCENARIO_ROUND_DELETE_CONDITIONS: DeleteVectorContents(theManager->mConditions);
        return 0;
    case EE_SCENARIO_ROUND_DELETE_EFFECTS: DeleteVectorContents(theManager->mEffects);
        return 0;
    case EE_SCENARIO_ROUND_DELETE_OBJECTS: DeleteVectorContents(theManager->mObjects);
        return 0;
    case EE_SCENARIO_ROUND_DELETE_AREAS: DeleteVectorContents(theManager->mAreas);
        return 0;
    case EE_SCENARIO_ROUND_DELETE_PENDING: DeleteVectorContents(theManager->mPendingEffects);
        return 0;
    case EE_SCENARIO_ROUND_TIMED_FREE: ee_scenario_timed_free(theManager->mTimedEffects);
        return 0;
    case EE_SCENARIO_ROUND_TIMED_FORGET: theManager->mTimedEffects = NULL;
        return 0;
    case EE_SCENARIO_ROUND_EFFECT_NEW: return (int64_t)(intptr_t) new EESMEffect;
    case EE_SCENARIO_ROUND_EFFECT_COPY: ((EESMEffect*)(intptr_t)inA)->Copy(*(const EESMEffect*)(intptr_t)inB);
        return 0;
    case EE_SCENARIO_ROUND_PENDING_PUSH: theManager->mPendingEffects.push_back((EESMEffect*)(intptr_t)inA);
        return 0;
    case EE_SCENARIO_ROUND_EFFECTS: return (int64_t)theManager->mEffects.size();
    case EE_SCENARIO_ROUND_EFFECT_AT: return (int64_t)(intptr_t)theManager->mEffects[(size_t)inA];
    case EE_SCENARIO_ROUND_LINE_START: return (int64_t)(intptr_t)&(sRoundStart = gWorld.mLine.GetStart());
    case EE_SCENARIO_ROUND_CAMERA_ZOOM_FIELD: return RoundFloatBits(gWorld.mCameraControl.mZoom);
    case EE_SCENARIO_ROUND_SIDE_SCROLL_SPEED: return RoundFloatBits(gSideScrollSpeed);
    case EE_SCENARIO_ROUND_CAMERA_SET_START: gWorld.mCameraControl.SetLocation(sRoundStart, false, true);
        return 0;
    case EE_SCENARIO_ROUND_LETTERBOX: return RoundFloatBits(gF4BE374);
    case EE_SCENARIO_ROUND_LATENCY: gSoundManager->SetLatency((long)inA);
        return 0;
    case EE_SCENARIO_ROUND_TRIGGER_FIELDS: return (int64_t)(uintptr_t)static_cast<EeScenarioTriggerFields*>(theManager->GetTrigger((unsigned long)inA));
    case EE_SCENARIO_ROUND_POST_HIDE: gUserInterface->PostHideMessages();
        return 0;
    case EE_SCENARIO_ROUND_STOP_SOUNDS: gSoundManager->StopAllOneShotSounds();
        return 0;
    case EE_SCENARIO_ROUND_MESSAGE_ERASE: theManager->mMessages.erase(theManager->mMessages.begin() + (size_t)inA);
        return 0;
    case EE_SCENARIO_ROUND_NUM_PLAYERS: return (int64_t)gWorld.mNumPlayers;
    case EE_SCENARIO_ROUND_PLAYER_EXISTS: return gWorld.GetPlayer((unsigned long)inA) != NULL;
    case EE_SCENARIO_ROUND_DELAY_WONDERS: gWorld.GetPlayer((unsigned long)inA)->DelayWonderTimers((long)inB);
        return 0;
    case EE_SCENARIO_ROUND_PLAYER_HUMAN: return gWorld.GetPlayer((unsigned long)inA)->IsHuman();
    case EE_SCENARIO_ROUND_PLAYER_OBSERVER: return gWorld.GetPlayer((unsigned long)inA)->mObserverMode;
    case EE_SCENARIO_ROUND_UNSELECT: gWorld.GetPlayer((unsigned long)inA)->UnselectAllUnits();
        return 0;
    case EE_SCENARIO_ROUND_COPY_SELECTION:
        gWorld.GetPlayer((unsigned long)inA)->mSelectedGroup->CopySelection(gWorld.GetPlayer((unsigned long)inA)->mActiveGroup);
        return 0;
    case EE_SCENARIO_ROUND_LETTERBOX_OUT: gGraphics->PostLetterboxOut(3000);
        return 0;
    case EE_SCENARIO_ROUND_LETTERBOX_IN: gGraphics->PostLetterboxIn(3000);
        return 0;
    case EE_SCENARIO_ROUND_FADE_IN: gGraphics->PostFadeIn(0);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_RESET_PATH: gWorld.mCameraControl.ResetPath();
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_CLEAR_B: gWorld.mCameraControl.ClearUnitB();
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_CLEAR_A: gWorld.mCameraControl.ClearUnitA();
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_TURN_RESET: return RoundFloatBits(gWorld.mCameraControl.mTurnYaw = 0.0f);
    case EE_SCENARIO_ROUND_CAMERA_SET_MODE: gWorld.mCameraControl.SetMode((long)inA);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_SET_ZOOM: gWorld.mCameraControl.SetZoom(theManager->mSavedZoom);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_SET_OFFSET: gWorld.mCameraControl.SetOffset(theManager->mK008.mOffset, true);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_SET_LOCATION: gWorld.mCameraControl.SetLocation(theManager->mK008.mFocus, true, false);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_SET_VIEW_TURN: gWorld.mCameraControl.SetViewTurn(theManager->mK008.mT00, true);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_SET_OFFSET_TURN: gWorld.mCameraControl.SetOffsetTurn(theManager->mK008.mT40, true);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_SET_CHANGED: gWorld.mCameraControl.SetChanged();
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_UPDATE: gWorld.mCameraControl.Update(0.0f);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_GET_OFFSET: gWorld.mCameraControl.GetOffset(gWorld.mCameraControl.mFraction, theManager->mK008.mOffset);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_GET_LOCATION: gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theManager->mK008.mFocus);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_GET_VIEW_TURN: gWorld.mCameraControl.GetViewTurn(gWorld.mCameraControl.mFraction, theManager->mK008.mT00);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_GET_OFFSET_TURN:
        gWorld.mCameraControl.GetOffsetTurn(gWorld.mCameraControl.mFraction, theManager->mK008.mT40);
        return 0;
    case EE_SCENARIO_ROUND_CAMERA_MODE: return (int64_t)gWorld.mCameraControl.mMode;
    case EE_SCENARIO_ROUND_CAMERA_ZOOM: return RoundFloatBits(gWorld.mCameraControl.GetZoom());
    case EE_SCENARIO_ROUND_SET_IN_CINEMATIC: return gWorld.mInCinematic = inA != 0;
    case EE_SCENARIO_ROUND_SAVE_GAME_SPEED: return RoundFloatBits(gWorld.mSavedGameSpeed = gWorld.GetGameSpeed());
    case EE_SCENARIO_ROUND_RESTORE_GAME_SPEED: gWorld.SetGameSpeed(gWorld.mSavedGameSpeed);
        return 0;
    case EE_SCENARIO_ROUND_CLOSE_MAIN_FORM: gUserInterface->CloseForm(gUIManager->GetMainGameForm());
        return 0;
    case EE_SCENARIO_ROUND_FORM_UI_STATE: gUIManager->GetMainGameForm()->SetUIState((unsigned long)inA);
        return 0;
    case EE_SCENARIO_ROUND_FORM_CURSOR_LOCK: return gUIManager->GetMainGameForm()->mIsCursorLocked = inA != 0;
    case EE_SCENARIO_ROUND_MOUSE_POINTER: return gMouse->mPointer = (long)inA;
    case EE_SCENARIO_ROUND_MOUSE_CENTER:
        gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() / 2), (float)(gGameWindow->GetHeight() / 2)));
        return 0;
    case EE_SCENARIO_ROUND_MESSAGE_VOICED: return theManager->mMessages[(size_t)inA].mSound.GetLength() > 0;
    case EE_SCENARIO_ROUND_POST_DIALOG: gUserInterface->PostDialogMessage(theManager->mMessages[(size_t)inA]);
        return 0;
    case EE_SCENARIO_ROUND_MESSAGE_UNIT: return gWorld.GetUnit(theManager->mMessages[(size_t)inA].mUnitID) != NULL;
    case EE_SCENARIO_ROUND_MESSAGE_SCROLL: return theManager->mMessages[(size_t)inA].mScrollSpeed;
    case EE_SCENARIO_ROUND_MESSAGE_MOVE_TO:
        gWorld.mCameraControl.MoveTo(gWorld.GetUnit(theManager->mMessages[(size_t)inA].mUnitID), GE3DPoint(-10000.0f, 0.0f, 0.0f), true,
                                     (float)theManager->mMessages[(size_t)inA].mScrollSpeed, NULL, GE3DPoint(-10000.0f, 0.0f, 0.0f), false, 0.1f, 0.0f, 0.0f);
        return 0;
    case EE_SCENARIO_ROUND_MESSAGE_MOVE_TO_FACING:
        gWorld.mCameraControl.MoveTo(gWorld.GetUnit(theManager->mMessages[(size_t)inA].mUnitID), GE3DPoint(-10000.0f, 0.0f, 0.0f), true,
                                     (float)theManager->mMessages[(size_t)inA].mScrollSpeed, gWorld.GetUnit(theManager->mMessages[(size_t)inA].mUnitID),
                                     GE3DPoint(-10000.0f, 0.0f, 0.0f), false, 0.1f, 3.1415927f, 0.0f);
        return 0;
    case EE_SCENARIO_ROUND_PENDING_COUNT: return (int64_t)theManager->mPendingEffects.size();
    case EE_SCENARIO_ROUND_PENDING_EXISTS: return theManager->mPendingEffects[(size_t)inA] != NULL;
    case EE_SCENARIO_ROUND_PENDING_EXECUTE: theManager->mPendingEffects[(size_t)inA]->Execute(0);
        return 0;
    case EE_SCENARIO_ROUND_MEDIA_COUNT: return (int64_t)theManager->mUnitClassMedia.size();
    case EE_SCENARIO_ROUND_MEDIA_UNLOAD:
        gWorld.GetPlayer(theManager->mUnitClassMedia[(size_t)inA].mPlayer)->GetUpgrade(theManager->mUnitClassMedia[(size_t)inA].mUnitClass)->UnloadMedia(gWorld.GetPlayer(theManager->mUnitClassMedia[(size_t)inA].mPlayer));
        return 0;
    case EE_SCENARIO_ROUND_MEDIA_TRACE:
        EEDebugTrace("Scenario:: Unloading Media for %S for player %d", gWorld.GetPlayer(theManager->mUnitClassMedia[(size_t)inA].mPlayer)->GetUpgrade(theManager->mUnitClassMedia[(size_t)inA].mUnitClass)->GetUnitClass()->GetName(),
                     theManager->mUnitClassMedia[(size_t)inA].mPlayer);
        return 0;
    case EE_SCENARIO_ROUND_MEDIA_CLEAR: theManager->mUnitClassMedia.clear();
        return 0;
    case EE_SCENARIO_ROUND_OBJECT_COUNT: return (int64_t)theManager->mObjects.size();
    case EE_SCENARIO_ROUND_OBJECT_REPLACE: theManager->mObjects[(size_t)inA]->ReplaceUnit((long)(inB >> 32), (long)(int32_t)inB);
        return 0;
    }
    return 0;
}

}   // namespace

// Runs a scenario tick (EEPhysics 0x153265): shows queued dialog, advances the clocks (tick / game speed in a
// cinematic); while a human plays, every 500 ms evaluates the triggers and runs the due timed effects (ee_scenario
// round.rs update)
void EEScenarioManager::Update()
{
    EeScenarioRoundHost theHost = EESMRoundHost(this);
    ee_scenario_manager_update(&theHost);
}

// Replays every pending effect of a loaded game, makes the triggers evaluate on the next tick and starts recording
// new pending effects
void EEScenarioManager::ApplyPendingEffects()
{
    ee_scenario_apply_pending(EESMRoundHost(this));   // cinematic.rs apply_pending
}

static void EESMPrepareMedia(EEScenarioManager* inManager);

// Unloads the unit class media loaded before, then preloads the media of each unit class a Create Object effect
// makes for one fixed player (objects not selected on the map; which: ee_scenario lists.rs prepare_media)
void EEScenarioManager::Prepare()
{
    ee_scenario_unload_media(EESMRoundHost(this), true);   // cinematic.rs unload_media
    EESMPrepareMedia(this);
}

// Replaces a unit ID with another in every scenario object
void EEScenarioManager::ReplaceUnit(long inOldUnit, long inNewUnit)
{
    ee_scenario_replace_unit(EESMRoundHost(this), inOldUnit, inNewUnit);   // cinematic.rs replace_unit
}

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

template<class T> int32_t EESMListIO(EESMStream* inStream, std::vector<T*>& ioList, uint32_t inOp, uint32_t inKind,
                                     unsigned long inIndex, void* ioValue)
{
    switch (inOp) {
    case EE_SCENARIO_IO_ELEMENT_NEW: *static_cast<void**>(ioValue) = new T;
        return 0;
    case EE_SCENARIO_IO_ELEMENT_READ: return (int32_t)static_cast<T*>(ioValue)->Load(inStream->mMajor, inStream->mMinor, *inStream->mFile);
    case EE_SCENARIO_IO_ELEMENT_KEEP: ioList.push_back(static_cast<T*>(ioValue));
        return 0;
    case EE_SCENARIO_IO_ELEMENT_FREE: delete static_cast<T*>(ioValue);
        return 0;
    case EE_SCENARIO_IO_ELEMENT_SAVE:
        return (int32_t)ioList[inIndex]->Save(inStream->mMajor, inStream->mMinor, *inStream->mFile);
    case EE_SCENARIO_IO_ELEMENT_COUNT: *static_cast<unsigned long*>(ioValue) = ioList.size(); return 0;
    }
    return 0;
}

}   // namespace

// a loaded condition naming object 0 (it is dropped)
static bool EESMConditionNamesObjectZero(EESMCondition* inCondition)
{
    return inCondition->mType == kSMConditionObject && inCondition->mObjectIndex == 0;
}

int32_t EESMManagerIO(EESMStream* inStream, uint32_t inOp, uint32_t inSlot, unsigned long inIndex, void* ioValue)
{
    EEScenarioManager* theManager = inStream->mManager;
    FSFile& theFile = *inStream->mFile;
    if (inOp == EE_SCENARIO_IO_MEDIA)
        return (int32_t)(inSlot == 0 ? theManager->mMediaStrings.Load(inStream->mMajor, inStream->mMinor, theFile)
                                     : theManager->mMediaStrings.Save(inStream->mMajor, inStream->mMinor, theFile));
    switch (inOp) {   // the manager's own (manager_compose.rs element_load, messages)
    case EE_SCENARIO_IO_LOADING_SCENARIO: return GetIsLoadingScenario();
    case EE_SCENARIO_IO_CONDITION_OBJECT_ZERO: return EESMConditionNamesObjectZero(static_cast<EESMCondition*>(ioValue));
    case EE_SCENARIO_IO_MESSAGES_COUNT: *static_cast<unsigned long*>(ioValue) = theManager->mMessages.size();
        return 0;
    case EE_SCENARIO_IO_MESSAGES_RESIZE: theManager->mMessages.resize(*static_cast<unsigned long*>(ioValue));
        return 0;
    case EE_SCENARIO_IO_MESSAGE_LOAD: return (int32_t)theManager->mMessages[inIndex].Load(inStream->mMajor, inStream->mMinor, theFile);
    case EE_SCENARIO_IO_MESSAGE_SAVE: return (int32_t)theManager->mMessages[inIndex].Save(inStream->mMajor, inStream->mMinor, theFile);
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

// Sets the run flag of the first timed entry of an effect whose flag differs
void EEScenarioManager::SetTimedEffectHasRun(EESMEffect* inEffect, bool inFlag)
{
    ee_scenario_set_timed_has_run(EESMRoundHost(this), inEffect, inFlag);   // manager_compose.rs set_timed_has_run
}

// Shows the next queued dialog message (in a cinematic the next cinematic one), stopping one-shot sounds for a
// voiced one and moving the camera to its unit; none left: hides the messages; nothing while paused (ee_scenario
// round.rs show_dialog)
void EEScenarioManager::ShowDialogMessage()
{
    EeScenarioRoundHost theHost = EESMRoundHost(this);
    ee_scenario_show_dialog(&theHost);
}

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

// Moves the camera one zoom-scaled step towards the scenario's start location; stops moving once it is there (the
// step: ee_scenario round.rs camera_step)
void EEScenarioManager::MoveCameraToStart()
{
    ee_scenario_move_camera_to_start(EESMRoundHost(this));   // manager_compose.rs move_camera_to_start
}

// Creates an empty dialog message: no unit or duration, no camera move (scroll speed -1), not for a cinematic
EESMDialogMessage::EESMDialogMessage()
{
    this->mScrollSpeed = -1;
    this->mUnitType = 0;
    this->mDuration = 0;
    this->mUnitID = 0;
    this->mIsCinematic = false;
}

// a dialog message's Load and Save (ee_scenario save.rs message_io): the text, unit type, sound, duration, unit,
// scroll speed, and (a save, or a load from version 0.200 on) the cinematic flag; the first error
static long EESMMessageIO(EESMDialogMessage* inMessage, unsigned long inMajor, unsigned long inMinor, FSFile& inFile,
                          bool inSave)
{
    UWideString* theStrings[] = {&inMessage->mText, &inMessage->mSound};
    EESMStream theStream = {&inFile, theStrings, NULL};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    EeScenarioMessageNumbers theNumbers = {&inMessage->mScrollSpeed, &inMessage->mUnitType, &inMessage->mDuration,
                                           &inMessage->mUnitID, &inMessage->mIsCinematic};
    return ee_scenario_message_io(&theTable, &theNumbers, inMajor, inMinor, inSave);
}

long EESMDialogMessage::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return EESMMessageIO(this, inMajorVersion, inMinorVersion, inFile, false);
}

long EESMDialogMessage::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return EESMMessageIO(this, inMajorVersion, inMinorVersion, outFile, true);
}

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

static_assert(IDS_NONE == 13001 && IDS_COPY == 13002 && IDS_EXISTS == 13003 && IDS_ALWAYS_TRUE == 13004 &&
                  IDS_DO_NOTHING == 13005 && IDS_OFF_MAP == 13006 && IDS_PREVIOUS_LOCATION == 13007,
              "lists.rs EE_SCENARIO_STRING_IDS");
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
    case EE_SCENARIO_LIST_OP_RESOURCE: gResource->GetString((unsigned long)inIndex, theContext->mText);
        return NULL;
    case EE_SCENARIO_LIST_OP_TEXT: return (void*)(const char*)theContext->mText;
    case EE_SCENARIO_LIST_OP_MEDIA: gWorld.GetPlayer(inList)->mUnitClassUpgrades[inIndex]->LoadMedia(gWorld.GetPlayer(inList), 0x7f);
        return NULL;
    case EE_SCENARIO_LIST_OP_MEDIA_KEEP: m->mUnitClassMedia.push_back({(long)inList, (long)inIndex});
        return NULL;
    case EE_SCENARIO_LIST_OP_ANCHOR:
        static_cast<EESMArea*>(inElement)->SetAnchor(U2DPoint<long>((long)(int32_t)inList, (long)(intptr_t)inIndex));
        return NULL;
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

// Reset's lists and built-ins (ee_scenario lists.rs reset)
static void EESMResetLists(EEScenarioManager* inManager)
{
    EESMLists theLists(inManager);
    ee_scenario_list_reset(&theLists.mHost);
}

// Prepare's media (ee_scenario lists.rs prepare_media: the player choices through the scenario host)
static void EESMPrepareMedia(EEScenarioManager* inManager)
{
    EESMLists theLists(inManager);
    EESMHostContext theContext = {NULL, NULL};
    EeScenarioHost theHost = EESMHost(&theContext);
    ee_scenario_prepare_media(&theLists.mHost, &theHost);
}

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

EESMCondition* EEScenarioManager::FindConditionByID(long inID)
{
    EESMLists theLists(this);
    return static_cast<EESMCondition*>(ee_scenario_list_find(&theLists.mHost, EE_SCENARIO_LIST_CONDITIONS, inID));
}

EESMEffect* EEScenarioManager::FindEffectByID(long inID)
{
    EESMLists theLists(this);
    return static_cast<EESMEffect*>(ee_scenario_list_find(&theLists.mHost, EE_SCENARIO_LIST_EFFECTS, inID));
}

EESMObject* EEScenarioManager::FindObjectByID(long inID)
{
    EESMLists theLists(this);
    return static_cast<EESMObject*>(ee_scenario_list_find(&theLists.mHost, EE_SCENARIO_LIST_OBJECTS, inID));
}

long EEScenarioManager::GetObjectIndex(EESMObject* inObject)
{
    EESMLists theLists(this);
    return ee_scenario_list_index_of(&theLists.mHost, EE_SCENARIO_LIST_OBJECTS, inObject);
}

// EESMObject::SetName's conditions (inParent false) or SetParent (true) over the lists
void EEScenarioManager::ObjectChanged(EESMObject* inObject, bool inParent, long inValue)
{
    EESMLists theLists(this);
    ee_scenario_object_changed(&theLists.mHost, inObject, inParent, inValue);   // manager_compose.rs
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
    UWideString mFormat;    // an import error's format and message (text.rs HostText::error)
    UWideString mMessage;
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

static_assert(IDS_IS_INVALID_TOKEN == 10027 && IDS_IS_INVALID_EFFECT_ID_FOR_TOKEN == 10028 &&
                  IDS_IS_INVALID_EFFECT_TYPE_TO_BE_TOKEN == 10029 && IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS == 10030 &&
                  IDS_IS_INVALID_CONDITION_ID_FOR_TOKEN == 10054 && IDS_IS_INVALID_CONDITION_TYPE_TO_BE_TOKEN == 10053 &&
                  IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS_2747 == 10055 &&
                  IDS_INVALID_PLAYER_NUMBER_FOR_THIS_SCENARIO == 10056,
              "text.rs ERROR_FORMATS");

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
    case EE_SCENARIO_TEXT_OP_ERROR_FORMAT: gResource->GetString((unsigned long)inValue, theContext->mFormat);
        break;
    case EE_SCENARIO_TEXT_OP_ERROR_PRINT_ID: sprintf(theContext->mMessage, theContext->mFormat, inValue, inText);
        break;
    case EE_SCENARIO_TEXT_OP_ERROR_PRINT_TEXTS: sprintf(theContext->mMessage, theContext->mFormat, inText, inText2);
        break;
    case EE_SCENARIO_TEXT_OP_ERROR_PUSH: m->mImportErrors.push_back(theContext->mMessage);
        break;
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

// Writes the trigger text file (EESEIAExport): the media strings, then the effect, condition and player name
// entries; 9 on a write error
long EEScenarioManager::ExportText(FSFileSpec& inFile)
{
    EESMTextFile theText(this, inFile);
    return ee_scenario_text_export(&theText.mHost);
}

// Reads a trigger text file (EESEIAImport) of "{id_Kind:name}" lines, each followed by its text; with inValidate
// lists the bad entries first (mImportErrors), then applies them; 9 on a bad entry
long EEScenarioManager::ImportTriggerText(FSFileSpec& inFile, unsigned long inKind, bool inValidate)
{
    EESMTextFile theText(this, inFile);
    return ee_scenario_text_import(&theText.mHost, inKind, inValidate);
}
