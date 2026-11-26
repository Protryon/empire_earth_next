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
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGameSound.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"
#include "ee/EEScenarioHost.h"

bool gScenarioSimDialogs = false;

extern float gF4BE374;                       // 0x4be374 (= 1.0f; Update ends the cinematic when it is 1; provisional name)
extern float& gSideScrollSpeed;               // 0x530828 (MoveCameraToStart's camera step scale; provisional name)

// Creates a scenario holding only the default entries (see Reset)
EEScenarioManager::EEScenarioManager()
    : FSPersistent(kPersistentScenario), EeScenarioManagerFields()
{
    this->mTimedEffects = ee_scenario_timed_new();
    this->Reset();
}

static GE3DPoint sRoundStart;   // MoveCameraToStart's point (the original's local; manager_compose.rs)

// Update's world (ee_scenario round.rs): each operation the original's statement at its place
extern "C" int64_t ee_scenariocpp_round_op(EEScenarioManager* theManager, uint32_t inOp, int64_t inA, int64_t inB, float inTime)
{
    switch (inOp) {
    case EE_SCENARIO_ROUND_TICK_SECONDS: return EESMBits(gPhysics->GetTickSeconds());
    case EE_SCENARIO_ROUND_MESSAGE_COUNT: return (int64_t)theManager->mMessages.size();
    case EE_SCENARIO_ROUND_DIALOG_PAUSED: return gCommunications == NULL || gCommunications->GetIsGamePaused();
    case EE_SCENARIO_ROUND_MESSAGE_CINEMATIC: return theManager->mMessages[(size_t)inA].mIsCinematic;
    case EE_SCENARIO_ROUND_MOVE_CAMERA: ee_scenario_en_round(theManager, EE_SCENARIO_MN_MOVE_CAMERA, NULL, 0, 0); return 0;
    case EE_SCENARIO_ROUND_TRIGGER_COUNT: return (int64_t)theManager->mTriggers.size();
    case EE_SCENARIO_ROUND_EVALUATE_TRIGGER: theManager->GetTrigger((unsigned long)inA)->Evaluate(inTime); return 0;
    case EE_SCENARIO_ROUND_EFFECT_EXISTS: return theManager->mEffects[(unsigned long)inA] != NULL;
    case EE_SCENARIO_ROUND_EXECUTE: theManager->mEffects[(unsigned long)inA]->Execute(inB != 0); return 0;
    case EE_SCENARIO_ROUND_CLEAR_CHAT: theManager->mLastChatText.Clear(); return 0;
    case EE_SCENARIO_ROUND_CAPTURE_RECORD: ee_scenario_en_cap_update(theManager, inA != 0);   // TEST ONLY (SMUP)
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
    case EE_SCENARIO_ROUND_CAMERA_ZOOM_FIELD: return EESMBits(gWorld.mCameraControl.mZoom);
    case EE_SCENARIO_ROUND_SIDE_SCROLL_SPEED: return EESMBits(gSideScrollSpeed);
    case EE_SCENARIO_ROUND_CAMERA_SET_START: gWorld.mCameraControl.SetLocation(sRoundStart, false, true);
        return 0;
    case EE_SCENARIO_ROUND_LETTERBOX: return EESMBits(gF4BE374);
    case EE_SCENARIO_ROUND_LATENCY: gSoundManager->SetLatency((long)inA);
        return 0;
    case EE_SCENARIO_ROUND_TRIGGER_FIELDS: return (int64_t)(uintptr_t)static_cast<EeScenarioTriggerFields*>(theManager->GetTrigger((unsigned long)inA));
    case EE_SCENARIO_ROUND_POST_HIDE: gUserInterface->PostHideMessages();
        return 0;
    case EE_SCENARIO_ROUND_STOP_SOUNDS: gSoundManager->StopAllOneShotSounds();
        return 0;
    case EE_SCENARIO_ROUND_MESSAGE_ERASE: theManager->mMessages.erase(theManager->mMessages.begin() + (size_t)inA);
        return 0;
    case EE_SCENARIO_ROUND_DELAY_WONDERS: gWorld.GetPlayer((unsigned long)inA)->DelayWonderTimers((long)inB);
        return 0;
    case EE_SCENARIO_ROUND_PLAYER_HUMAN: return gWorld.GetPlayer((unsigned long)inA)->IsHuman();
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
    case EE_SCENARIO_ROUND_CAMERA_TURN_RESET: return EESMBits(gWorld.mCameraControl.mTurnYaw = 0.0f);
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
    case EE_SCENARIO_ROUND_CAMERA_GET_OFFSET_TURN: gWorld.mCameraControl.GetOffsetTurn(gWorld.mCameraControl.mFraction, theManager->mK008.mT40); return 0;
    case EE_SCENARIO_ROUND_CAMERA_MODE: return (int64_t)gWorld.mCameraControl.mMode;
    case EE_SCENARIO_ROUND_CAMERA_ZOOM: return EESMBits(gWorld.mCameraControl.GetZoom());
    case EE_SCENARIO_ROUND_SAVE_GAME_SPEED: return EESMBits(gWorld.mSavedGameSpeed = gWorld.GetGameSpeed());
    case EE_SCENARIO_ROUND_RESTORE_GAME_SPEED: gWorld.SetGameSpeed(gWorld.mSavedGameSpeed);
        return 0;
    case EE_SCENARIO_ROUND_CLOSE_MAIN_FORM: gUserInterface->CloseForm(gUIManager->GetMainGameForm());
        return 0;
    case EE_SCENARIO_ROUND_FORM_UI_STATE: gUIManager->GetMainGameForm()->SetUIState((unsigned long)inA);
        return 0;
    case EE_SCENARIO_ROUND_FORM_CURSOR_LOCK: return gUIManager->GetMainGameForm()->mIsCursorLocked = inA != 0;
    case EE_SCENARIO_ROUND_MOUSE_POINTER: return gMouse->mPointer = (long)inA;
    case EE_SCENARIO_ROUND_MOUSE_CENTER: gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() / 2), (float)(gGameWindow->GetHeight() / 2))); return 0;
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
    case EE_SCENARIO_IO_ELEMENT_SAVE: return (int32_t)ioList[inIndex]->Save(inStream->mMajor, inStream->mMinor, *inStream->mFile);
    case EE_SCENARIO_IO_ELEMENT_COUNT: *static_cast<unsigned long*>(ioValue) = ioList.size(); return 0;
    }
    return 0;
}

}   // namespace

// a loaded condition naming object 0 (it is dropped)
static bool EESMConditionNamesObjectZero(EESMCondition* inCondition)
{
    return inCondition->mType == EE_SCENARIO_SM_CONDITION_OBJECT && inCondition->mObjectIndex == 0;
}

int32_t EESMManagerIO(EESMStream* inStream, uint32_t inOp, uint32_t inSlot, unsigned long inIndex, void* ioValue)
{
    EEScenarioManager* theManager = inStream->mManager;
    FSFile& theFile = *inStream->mFile;
    switch (inOp) {   // the manager's own (manager_compose.rs element_load, messages), its media strings
    case EE_SCENARIO_IO_MEDIA: return (int32_t)theManager->mMediaStrings.Load(inStream->mMajor, inStream->mMinor, theFile);
    case EE_SCENARIO_IO_MEDIA_SAVE: return (int32_t)theManager->mMediaStrings.Save(inStream->mMajor, inStream->mMinor, theFile);
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
    case EE_SCENARIO_ELEMENT_CONDITIONS: return EESMListIO(inStream, theManager->mConditions, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_EFFECTS: return EESMListIO(inStream, theManager->mEffects, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_OBJECTS: return EESMListIO(inStream, theManager->mObjects, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_AREAS: return EESMListIO(inStream, theManager->mAreas, inOp, inSlot, inIndex, ioValue);
    case EE_SCENARIO_ELEMENT_PENDING: return EESMListIO(inStream, theManager->mPendingEffects, inOp, inSlot, inIndex, ioValue);
    }
    return 0;
}

// ------------------------------------------------------------------------------------------------ the lists
// The editor's list operations, the name lists and the id searches are ee_scenario lists.rs (with the index upkeep
// each change makes: the Renumber/Remap broadcasts over every element, the elements' own and EERemapID); the vectors
// and elements stay here, reached by operation (EESMListOp)

namespace {
thread_local UWideString sSMListText;   // a resource string read for the lists (the original's local)

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

}   // namespace

// (inContext the list host's context: the manager, the name list out)
extern "C" void* ee_scenariocpp_list_op(EeScenarioListContext* inContext, uint32_t inOp, uint32_t inList, uintptr_t inIndex, void* inElement, const char* inText)
{
    EEScenarioManager* m = static_cast<EEScenarioManager*>(inContext->manager);
    switch (inOp) {
    case EE_SCENARIO_LIST_OP_OBJECT_COUNT: return (void*)(uintptr_t)static_cast<EESMEffect*>(inElement)->mObjects.size();
    case EE_SCENARIO_LIST_OP_OBJECTS: return static_cast<EESMEffect*>(inElement)->mObjects.data();
    case EE_SCENARIO_LIST_OP_ENTRY_COUNT:
        return (void*)(uintptr_t)(inList ? static_cast<EESMTrigger*>(inElement)->mEffects.size() : static_cast<EESMTrigger*>(inElement)->mConditions.size());
    case EE_SCENARIO_LIST_OP_ENTRY:
        return inList ? (void*)&static_cast<EESMTrigger*>(inElement)->mEffects[inIndex]->mEffectIndex : (void*)&static_cast<EESMTrigger*>(inElement)->mConditions[inIndex]->mValue;
    case EE_SCENARIO_LIST_OP_RESOURCE: gResource->GetString((unsigned long)inIndex, sSMListText);
        return NULL;
    case EE_SCENARIO_LIST_OP_TEXT: return (void*)(const char*)sSMListText;
    case EE_SCENARIO_LIST_OP_MEDIA: gWorld.GetPlayer(inList)->mUnitClassUpgrades[inIndex]->LoadMedia(gWorld.GetPlayer(inList), 0x7f);
        return NULL;
    case EE_SCENARIO_LIST_OP_MEDIA_KEEP: m->mUnitClassMedia.push_back({(long)inList, (long)inIndex});
        return NULL;
    case EE_SCENARIO_LIST_OP_ANCHOR: static_cast<EESMArea*>(inElement)->SetAnchor(U2DPoint<long>((long)(int32_t)inList, (long)(intptr_t)inIndex)); return NULL;
    case EE_SCENARIO_LIST_OP_OUT_NAME: static_cast<std::vector<UWideString>*>(inContext->names)->push_back(UWideString(inText)); return NULL;
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


// ------------------------------------------------------------------------------------------------ the text file
// The trigger text file of the instructions tab (EESEIAExport, EESEIAImport) is ee_scenario text.rs: ExportText
// 0x1b23dc, ImportTriggerText 0x1b3ae9 with ValidateText 0x1b52bb, ImportText 0x1b4a95 and the line parsers
// (ParseConditionID 0x1b463b, ParseEffectID 0x1b46bf, ParsePlayerName 0x1b4796). The elements' texts, the players'
// names, the media strings' sections, the file and the language's error formats stay here, reached by operation
// (EESMTextOp)

namespace {
thread_local char sSMTextBuffer[0x200];   // a read line (the original's local)
thread_local UWideString sSMTextFormat;   // an import error's format and message (text.rs HostText::error)
thread_local UWideString sSMTextMessage;

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


}   // namespace

// (inContext the text host's context: the manager, the file)
extern "C" void* ee_scenariocpp_text_op(EeScenarioTextContext* inContext, uint32_t inOp, uint32_t inSlot, long inValue, void* inElement,
                                       const char* inText, const char* inText2)
{
    EEScenarioManager* m = static_cast<EEScenarioManager*>(inContext->manager);
    FILE* theFile = *static_cast<FSFileSpec*>(inContext->file);
    EEPlayer* thePlayer;
    switch (inOp) {
    case EE_SCENARIO_TEXT_OP_GET: return (void*)(const char*)EESMText(inElement, inSlot);
    case EE_SCENARIO_TEXT_OP_SET: EESMText(inElement, inSlot) = inText; break;
    case EE_SCENARIO_TEXT_OP_PLAYERS: return (void*)(uintptr_t)gWorld.mNumPlayers;
    case EE_SCENARIO_TEXT_OP_PLAYER_NAME: thePlayer = gWorld.GetPlayer(inValue); return thePlayer != NULL ? (void*)(const char*)thePlayer->mName : NULL;
    case EE_SCENARIO_TEXT_OP_SET_PLAYER_NAME: gWorld.GetPlayer(inValue)->mName = inText; break;
    case EE_SCENARIO_TEXT_OP_WRITE: return fprintf(theFile, "%s", inText) < 0 ? NULL : inContext;
    case EE_SCENARIO_TEXT_OP_EOF: return feof(theFile) ? inContext : NULL;
    case EE_SCENARIO_TEXT_OP_READ: return fgets(sSMTextBuffer, sizeof(sSMTextBuffer), theFile);
    case EE_SCENARIO_TEXT_OP_CLEAR_ERRORS: m->mImportErrors.clear(); break;
    case EE_SCENARIO_TEXT_OP_ERROR_FORMAT: gResource->GetString((unsigned long)inValue, sSMTextFormat);
        break;
    case EE_SCENARIO_TEXT_OP_ERROR_PRINT_ID: sprintf(sSMTextMessage, sSMTextFormat, inValue, inText);
        break;
    case EE_SCENARIO_TEXT_OP_ERROR_PRINT_TEXTS: sprintf(sSMTextMessage, sSMTextFormat, inText, inText2);
        break;
    case EE_SCENARIO_TEXT_OP_ERROR_PUSH: m->mImportErrors.push_back(sSMTextMessage);
        break;
    case EE_SCENARIO_TEXT_OP_MEDIA: return (void*)(const char*)m->mMediaStrings.GetSection(inSlot);
    case EE_SCENARIO_TEXT_OP_SET_MEDIA: m->mMediaStrings.GetSection(inSlot) = inText; break;
    }
    return NULL;
}

