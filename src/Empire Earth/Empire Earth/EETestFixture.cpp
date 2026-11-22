// TEST ONLY: the host of the in-process UI fixture (rust/crates/ee_game/src/fixture: the thread, the ui-state file,
// the commands, the pointer hold, the box selection, the casts and the orders; tools/harness/fixture_game.py and
// posix_game.py drive it). It does nothing unless EE_TEST_UI names a directory. Here: EEFixtureHost (the UI's
// controls, the input devices, the game's messages and actions, the picker and the units' methods the fixture asks
// for, one EeFixtureOp each), the hooks' trampolines, the WON lobby's walk (its own message loop), the debug output,
// the fault handler and the legacy harness's communications mirror.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <deque>
#include <mutex>
#include <atomic>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/U2DSparseArray.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "platform/PlatWindow.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "lle/UIComboBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMouse.h"
#include "ee/EEARust.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "platform/PlatSim.h"
#include "platform/PlatTime.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/EEUnit.h"
#include "lle/USingleton.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEUICSelectUnits.h"
#include "rust/game/screens.h"
#include "ee/EEScreenEvent.h"
#include <algorithm>
#include "rust/game/fixture.h"
#include "ee/EECombatGoals.h"

bool EEIsLobbyOpen();                        // EELobbyInterface.cpp: the external (WON) lobby is open
extern void (*gEESelectUnitsTestHook)(const std::vector<long>& inUnitIDs);   // EEMessage.cpp: each box selection's ids
extern void (*gEEMessageExecutedTestHook)(NEMessage* inMessage, int inPhase);   // EEPhysics.cpp (DrainInput)

// ---- the WON lobby (TEST ONLY): its walk, commands and snapshot are the Rust fixture's (lobby.rs), run on the
// lobby's own message loop by a UserEvent (this trampoline)
namespace {

void LobbyWork(int)
{
    ee_fixture_lobby_work();
}

// ---- the actions and hooks

// A UI action that sends one game message as local input (as the UI's own buttons do, on the UI thread), or with no
// message counts a run for the fixture (its UI round trip: ee_fixture_mark)
class FixtureAction : public EEAction {
public:
    explicit FixtureAction(NEMessage* inMessage) : mMessage(inMessage) {}
    virtual ~FixtureAction()
    {
        if (this->mMessage)
            this->mMessage->Dereference();
    }
    virtual void Virtual4()
    {
        if (this->mMessage == NULL)
            ee_fixture_mark();
        else
            gCommunications->ProcessLocalUserInput(this->mMessage, kSlotNone);
        this->mMessage = NULL;
    }
    virtual void Virtual8() {}
    NEMessage* mMessage;
};

void PostAction(NEMessage* inMessage)
{
    FixtureAction* theAction = new FixtureAction(inMessage);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
}

void LassoHook(const std::vector<long>& inUnitIDs)
{
    std::vector<int64_t> theIDs(inUnitIDs.begin(), inUnitIDs.end());
    ee_fixture_lasso(theIDs.data(), theIDs.size());
}

void MessageHook(NEMessage* inMessage, int inPhase)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    bool theLocal = thePlayer != NULL && gWorld.GetPlayerBySlot(inMessage->GetOriginatingSlot()) == thePlayer;
    ee_fixture_message((uint64_t)(uintptr_t)inMessage, inMessage->GetType(), theLocal, inPhase);
}

// The game's debug output (PlatDebugOutput: GX's log, EE-AI lines, ...) goes to stderr as "debugstring <text>", the
// lines the debugger harness read (no debugger runs under the fixture driver)
void DebugToStderr(const char* inText)
{
    size_t theLength = inText ? strlen(inText) : 0;
    fprintf(stderr, "debugstring %s%s", inText ? inText : "", theLength && inText[theLength - 1] == '\n' ? "" : "\n");
    fflush(stderr);
}

#ifdef _WIN32
// TEST ONLY: a fault under the fixture prints its code, place and return addresses (exe addresses: EE-AOC.map) on
// stderr, which the harness keeps as game.log, as EE-SIM's own handler does: an intermittent crash in a harness run
// otherwise leaves only its exit code.
LONG CALLBACK FixtureFault(EXCEPTION_POINTERS* inInfo)
{
    DWORD theCode = inInfo->ExceptionRecord->ExceptionCode;
    if (theCode < 0xC0000000UL || theCode == 0xE06D7363UL)   // not a fault (thread names, debug strings, C++)
        return EXCEPTION_CONTINUE_SEARCH;
    static volatile LONG sOnce = 0;
    if (InterlockedExchange(&sOnce, 1))
        return EXCEPTION_CONTINUE_SEARCH;
    void* theFrames[48];
    USHORT theCount = RtlCaptureStackBackTrace(0, 48, theFrames, NULL);
    char theText[2048];
    int n = snprintf(theText, sizeof(theText), "EE-AOC FAULT %08lx at %p thread %lu; frames:", (unsigned long)theCode,
                     inInfo->ExceptionRecord->ExceptionAddress, (unsigned long)GetCurrentThreadId());
    for (USHORT k = 0; k < theCount && n < (int)sizeof(theText) - 24; k++)
        n += snprintf(theText + n, sizeof(theText) - n, " %p", theFrames[k]);
    fprintf(stderr, "%s\n", theText);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

const unsigned long kTriggerEvent = 0x3f0;

template <class T> T* As(uint64_t inHandle)
{
    return reinterpret_cast<T*>((uintptr_t)inHandle);
}

int64_t Handle(const void* inObject)
{
    return (int64_t)(uintptr_t)inObject;
}

#define FORM As<UIForm>(c->h)
#define CONTROL As<UIControl>(c->h)
#define COMBO As<UIComboBox>(c->h)
#define LIST As<UILBString>(c->h)
#define GOAL static_cast<EEUGRust*>(As<EEUnitGoal>(c->h))
#define BUTTON As<EEUIFMGStatusButton>(c->h)
#define SELECT As<EEUICSelectUnits>(c->h)
#define CAMERA gWorld.mCameraControl
#define CALAMITIES EECalamityManager::GetInstance()

static_assert(kGoalGather == 6 && kGoalLocation == 1 && kGoalAttackUnit == 2 && kGoalAttackGround == 3 && kGoalPatrol == 9
              && kGoalBuildRepair == 4 && kGoalExplore == 10, "ee_game fixture compound.rs FIXTURE_GOAL_KINDS");

// One fixture call (rust/crates/ee_game/src/fixture/host.rs documents each operation's arguments and answers; the
// compound ones are compound.rs's over the Fx operations): one statement each
int64_t EEFixtureHost(EeFixtureCall* c)
{
    const int64_t* i = c->i;
    EEComplexUnit* u = As<EEComplexUnit>(c->h);
    switch (c->op) {
    case EeFixtureOp::Stderr:
        fputs(c->s, stderr);
        fflush(stderr);
        return 0;
    case EeFixtureOp::Exit:
        exit((int)i[0]);
    case EeFixtureOp::SetTimeFactor: return PlatSimSetTimeFactor((uint32_t)i[0], (uint32_t)i[1]) ? 1 : 0;
    case EeFixtureOp::ScreenEvent:   // as a dialog's answer posts it (the editor exercise's "exit without saving")
        ee_screens_event((uint32_t)i[0], As<UIForm>(c->h), NULL);
        return 0;
    case EeFixtureOp::Chat: gUserInterface->AddChatLogEntry(UWideString("TEST chat line: the chat display (EEUICChatLine)"), 0);
        return 1;
    case EeFixtureOp::Zoom: gWorld.mCameraControl.SetZoom(c->f[0]);
        return 0;
    case EeFixtureOp::UiUpdate: gUIManager->ForceUIUpdate();
        return 0;
    case EeFixtureOp::Input: {
        static const uint32_t kTypes[] = {PLAT_EVENT_KEY_DOWN, PLAT_EVENT_KEY_UP, PLAT_EVENT_MOUSE_DOWN,
                                          PLAT_EVENT_MOUSE_UP, PLAT_EVENT_MOUSE_WHEEL};
        PlatInputEvent theEvent;
        memset(&theEvent, 0, sizeof(theEvent));
        theEvent.mType = kTypes[i[0]];
        if (i[0] <= 1)
            theEvent.mKey.mScanCode = (uint32_t)i[1];
        else if (i[0] <= 3)
            theEvent.mMouse.mButton = (uint32_t)i[1];
        else
            theEvent.mMouse.mWheel = c->f[0];
        ISHandleEvent(theEvent);
        return 0;
    }
    case EeFixtureOp::State:
        return (gPhysics != NULL ? 1 : 0) | (gWorld.mMap != NULL ? 2 : 0) | (gWorld.mLock != NULL ? 4 : 0)
            | (gMouse != NULL ? 8 : 0) | (gISKeyboard != NULL ? 16 : 0) | (gUserInterface != NULL ? 32 : 0)
            | (EEIsLobbyOpen() ? 64 : 0) | (gUIManager != NULL ? 128 : 0);
    case EeFixtureOp::SelectControl:
        return Handle(kMainGameSelectUnits < FORM->GetControlCount() ? dynamic_cast<EEUICSelectUnits*>(FORM->GetControl(kMainGameSelectUnits)) : NULL);
    case EeFixtureOp::Mark: PostAction(NULL);
        return 0;
    case EeFixtureOp::UnitById: return Handle(gWorld.GetUnit((unsigned long)i[0]));
    case EeFixtureOp::PlayerUnit: return Handle(As<EEPlayer>(c->h)->GetUnit((unsigned long)i[0]));
    case EeFixtureOp::CanDelete: return u->CanDelete() ? 1 : 0;
    case EeFixtureOp::Distance: c->f[0] = u->DistanceSquared3D(GE3DPoint(c->f[0], c->f[1], c->f[2]));
        return 0;
    case EeFixtureOp::CanGather: return u->CanGather(As<EEUnit>(c->h2), i[0], i[1], i[2] != 0, i[3] != 0) ? 1 : 0;
    case EeFixtureOp::CanGetTo: return gContinentManager.CanGetTo(u, i[0], i[1]) ? 1 : 0;
    case EeFixtureOp::SeenBy: return EECombatIsSeenBy(As<EEUnit>(c->h), As<EEPlayer>(c->h2), true) ? 1 : 0;
    case EeFixtureOp::PatrolAction: return u->mUnitClass->FindAction(kActionGetNextPatrolPoint) != NULL ? 1 : 0;
    case EeFixtureOp::LastQueued: return u->mGoalQueues.empty() || u->mGoalQueues[0].mGoals.empty() ? 0 : Handle(u->mGoalQueues[0].mGoals.back());
    case EeFixtureOp::SetHitPoints: u->mHitPoints = (long)i[0];
        return 0;
    case EeFixtureOp::CalamityCount: return CALAMITIES != NULL ? (int64_t)CALAMITIES->GetCalamityClassCount() : 0;
    // ---- the compound operations' finer ones (compound.rs)
    case EeFixtureOp::FxInstallDebug: PlatDebugSetHandler(DebugToStderr);
        return 0;
    case EeFixtureOp::FxInstallFault:
#ifdef _WIN32
        AddVectoredExceptionHandler(1, FixtureFault);
#endif
        return 0;
    case EeFixtureOp::FxInstallLasso: gEESelectUnitsTestHook = LassoHook;   // each box selection's ids (the boxselect command)
        return 0;
    case EeFixtureOp::FxInstallMessageHook: gEEMessageExecutedTestHook = MessageHook;   // each executed input's view
        return 0;
    case EeFixtureOp::FxUiLockOn: gUIManager->LockUIForCopyData();
        return 0;
    case EeFixtureOp::FxUiLockOff: gUIManager->UnlockUIForCopyData();
        return 0;
    case EeFixtureOp::FxCurrentForm: return Handle(gUIManager->GetCurrentForm());
    case EeFixtureOp::FxFormMouseOver: return Handle(FORM->GetMouseOverControl());
    case EeFixtureOp::FxFormId: return FORM->GetFormID();
    case EeFixtureOp::FxFormControlCount: return FORM->GetControlCount();
    case EeFixtureOp::FxFormControl: return Handle(FORM->GetControl((unsigned long)i[0]));
    case EeFixtureOp::FxControlIndex: return (long)CONTROL->GetIndex();
    case EeFixtureOp::FxFormCount: return gUIManager->GetFormCount();
    case EeFixtureOp::FxStackedForm: return Handle(gUIManager->GetStackedForm((unsigned long)i[0]));
    case EeFixtureOp::FxUiScreen: return gUIManager->GetScreen();
    case EeFixtureOp::FxScreenCount: return gUIManager->GetRasterizer()->GetScreenCount();
    case EeFixtureOp::FxWindowAt: return Handle(gUIManager->GetRasterizer()->GetWindow((unsigned long)i[0]));
    case EeFixtureOp::FxWindowWidth: return As<GEWindow>(c->h)->GetWidth();   // the game's (logical) pixels: the pointer's
    case EeFixtureOp::FxWindowHeight: return As<GEWindow>(c->h)->GetHeight();
    case EeFixtureOp::FxHasTrigger: return CONTROL->GetTriggerEvent(kTriggerEvent) != NULL;
    case EeFixtureOp::FxAsCombo: return Handle(dynamic_cast<UIComboBox*>(CONTROL));
    case EeFixtureOp::FxAsCheck: return Handle(dynamic_cast<UICheckBox*>(CONTROL));
    case EeFixtureOp::FxAsListString: return Handle(dynamic_cast<UILBString*>(CONTROL));
    case EeFixtureOp::FxAsRadio: return Handle(dynamic_cast<UIRadioButton*>(CONTROL));
    case EeFixtureOp::FxAsStatusButton: return Handle(dynamic_cast<EEUIFMGStatusButton*>(CONTROL));
    case EeFixtureOp::FxComboSelection: return COMBO->GetCurrentSelection(*(unsigned long*)&c->i[0]);
    case EeFixtureOp::FxComboListBox: return Handle(COMBO->GetListBox());
    case EeFixtureOp::FxComboSetSelection: COMBO->SetCurrentSelection((unsigned long)i[0]);
        return 0;
    case EeFixtureOp::FxListSelection: return LIST->GetCurrentSelection(*(unsigned long*)&c->i[0]);
    case EeFixtureOp::FxListLineCount: return LIST->GetLineCount();
    case EeFixtureOp::FxListLine: ee_fixture_sink_text(c->sink, (const char*)LIST->GetData((unsigned long)i[0]));
        return 0;
    case EeFixtureOp::FxListChangeSelection: LIST->ChangeSelectionIndex((unsigned long)i[0], true, false);
        return 0;
    case EeFixtureOp::FxCheckIsChecked: return As<UICheckBox>(c->h)->GetIsChecked() ? 1 : 0;
    case EeFixtureOp::FxCheckSet: As<UICheckBox>(c->h)->SetChecked(i[0] != 0);
        return 0;
    case EeFixtureOp::FxAsEdit: return Handle(dynamic_cast<UIEditBox*>(CONTROL));
    case EeFixtureOp::FxEditSetNumber: As<UIEditBox>(c->h)->SetText((unsigned long)i[0]);
        return 0;
    case EeFixtureOp::FxRadioCheck: As<UIRadioButton>(c->h)->Check();
        return 0;
    case EeFixtureOp::FxFireTrigger: CONTROL->FireTriggerEvent(kTriggerEvent);
        return 0;
    case EeFixtureOp::FxScreenEventOf: return dynamic_cast<EEScreenEvent*>(CONTROL->GetTriggerEvent(kTriggerEvent)) != NULL ? (int64_t)dynamic_cast<EEScreenEvent*>(CONTROL->GetTriggerEvent(kTriggerEvent))->mEvent : -1;
    case EeFixtureOp::FxMousePresent: return gMouse != NULL;
    case EeFixtureOp::FxMouseLock: gMouse->mInputLock->Enter();
        return 0;
    case EeFixtureOp::FxMouseUnlock: gMouse->mInputLock->Leave();
        return 0;
    case EeFixtureOp::FxMouseX: c->f[0] = gMouse->mPosition.GetX();
        return 0;
    case EeFixtureOp::FxMouseY: c->f[0] = gMouse->mPosition.GetY();
        return 0;
    case EeFixtureOp::FxMouseSetPosition: gMouse->SetPosition(GE2DPoint(c->f[0], c->f[1]));
        return 0;
    case EeFixtureOp::FxModifierState: return gISKeyboard->GetModifierState() & (kModifierShift | kModifierControl);
    case EeFixtureOp::FxShiftModifier: return kModifierShift;
    case EeFixtureOp::FxSelectCaptured: return SELECT->GetForm()->GetIsMouseCaptured();
    case EeFixtureOp::FxSelectDragging: return SELECT->GetIsDragging();
    case EeFixtureOp::FxSelectDragX: c->f[0] = SELECT->GetDragEnd().GetX();
        return 0;
    case EeFixtureOp::FxSelectDragY: c->f[0] = SELECT->GetDragEnd().GetY();
        return 0;
    case EeFixtureOp::FxViewSide: c->f[0] = i[0] == 0 ? gViewport.GetViewRectangle().GetLeft() : i[0] == 1 ? gViewport.GetViewRectangle().GetTop() : i[0] == 2 ? gViewport.GetViewRectangle().GetRight() : gViewport.GetViewRectangle().GetBottom();
        return 0;
    case EeFixtureOp::FxPostSelect: PostAction(new EEMRecord(kMessageSelectTheseUnits, {std::vector<long>(c->ids, c->ids + c->n)}));
        return 0;
    case EeFixtureOp::FxPostKill: PostAction(new EEMRecord(kMessageKillUnit, {(unsigned char)0}));   // the first selected unit
        return 0;
    case EeFixtureOp::FxPostPath: PostAction(new EEMRecord(kMessagePathTo, {0.0f, c->f[0], c->f[1], false, true, false, false}));
        return 0;
    case EeFixtureOp::FxPostStop: PostAction(new EEMRecord(kMessageStopActiveGroup));
        return 0;
    case EeFixtureOp::FxNewCenterCamera: return Handle(new EEACenterCamera);
    case EeFixtureOp::FxCenterCameraAt: As<EEACenterCamera>(c->h)->SetLocation(i[0], i[1]);
        return 0;
    case EeFixtureOp::FxActionDereference: As<EEAction>(c->h)->Dereference();
        return 0;
    case EeFixtureOp::FxStatusButtonShows: return BUTTON->mIsCommand && BUTTON->mButton == (unsigned long)i[0] && BUTTON->GetDisplayMode() == kDisplayModeShown;
    case EeFixtureOp::FxStatusButtonCenter: c->f[0] = i[0] == 0 ? (BUTTON->GetRectangle().GetLeft() + BUTTON->GetRectangle().GetRight()) * 0.5f : (BUTTON->GetRectangle().GetTop() + BUTTON->GetRectangle().GetBottom()) * 0.5f;
        return 0;
    case EeFixtureOp::FxWorldHasLock: return gWorld.mLock != NULL;
    case EeFixtureOp::FxWorldLockStep: i[0] == 0 ? gWorld.mLock->ReadLock() : i[0] == 1 ? gWorld.mLock->ReadUnlock() : i[0] == 2 ? gWorld.mLock->WriteLock() : gWorld.mLock->WriteUnlock();
        return 0;
    case EeFixtureOp::FxPhysicsPresent: return gPhysics != NULL;
    case EeFixtureOp::FxPhysicsValue: return i[0] == 0 ? (int64_t)(long)gPhysics->mGameTime : i[0] == 1 ? (int64_t)(unsigned long)gPhysics->mNumTicksDone : (gPhysics->mGameOver ? 1 : 0);
    case EeFixtureOp::FxCommsPresent: return gCommunications != NULL;
    case EeFixtureOp::FxCommsValue: return i[0] == 0 ? (int64_t)gCommunications->GetNumberOfCTsProcessedInPhysics() : i[0] == 1 ? (int64_t)(long)gCommunications->GetLastTickReceived() : i[0] == 2 ? (gCommunications->GetIsGamePaused() ? 1 : 0) : (gCommunications->GetIsMultiplayerSession() ? 1 : 0);
    case EeFixtureOp::FxMapPresent: return gWorld.GetMap() != NULL;
    case EeFixtureOp::FxCameraFocus: CAMERA.GetLocation(CAMERA.mFraction, *(GE3DPoint*)&c->f[0]);
        return 0;
    case EeFixtureOp::FxCameraEndFocus: *(GE3DPoint*)&c->f[0] = CAMERA.GetEndFocus();
        return 0;
    case EeFixtureOp::FxCameraZoom: c->f[0] = CAMERA.mZoom;
        return 0;
    case EeFixtureOp::FxCameraMode: return (long)CAMERA.mMode;
    case EeFixtureOp::FxMapSize: return (long)gWorld.GetMap()->GetSize();
    case EeFixtureOp::FxMeshValidPoint: return gWorld.GetMap()->mTerrainMesh.GetIsValidPoint(UMFloorUnsigned(c->f[0]), UMFloorUnsigned(c->f[1]));
    case EeFixtureOp::FxActiveGroup: return gWorld.GetCurrentPlayer() != NULL ? Handle(gWorld.GetCurrentPlayer()->mActiveGroup) : 0;
    case EeFixtureOp::FxGroupUnitCount: return As<EEGroup>(c->h)->GetNumUnits();
    case EeFixtureOp::FxGroupUnit: return Handle(As<EEGroup>(c->h)->GetUnit((unsigned long)i[0]));
    case EeFixtureOp::FxPickUnit: return Handle(gUnitPicker.PickUnitUnreferenced(GE2DPoint(c->f[0], c->f[1]), (long)i[0]));
    case EeFixtureOp::FxUnitId: return (int64_t)As<EEUnit>(c->h)->mUnitID;
    case EeFixtureOp::FxPointUnitOrTile: return EEPointUnitOrTile(GE2DPoint(c->f[0], c->f[1]), *(EETile**)&c->h2, *(GE3DPoint*)&c->f[2], true);
    case EeFixtureOp::FxTileSeenByLocal: return As<EETile>(c->h)->IsSeenBy(gWorld.GetCurrentPlayer());
    case EeFixtureOp::FxNearestTown: return Handle(u->FindNearestTown());
    case EeFixtureOp::FxTownPatrolGroup: return Handle(As<EETown>(c->h)->mPatrolGroup);
    case EeFixtureOp::FxUnitTileX: return u->GetTileX();
    case EeFixtureOp::FxUnitTileY: return u->GetTileY();
    case EeFixtureOp::FxUnitPatrolGroup: return Handle(u->mPatrolGroup);
    case EeFixtureOp::FxGoalKind: return As<EEUnitGoal>(c->h)->mKind;
    case EeFixtureOp::FxGoalGatherResource: return Handle(GOAL->GetGatherResource());
    case EeFixtureOp::FxGoalAttackTarget: return Handle(GOAL->GetAttackTarget());
    case EeFixtureOp::FxGoalPatrolTarget: return Handle(GOAL->GetPatrolTarget());
    case EeFixtureOp::FxGoalPatrolGroup: return Handle(GOAL->GetPatrolGroup());
    case EeFixtureOp::FxGoalLocationAt: c->f[0] = i[0] == 0 ? GOAL->GetLocation().GetX() : GOAL->GetLocation().GetY();
        return 0;
    case EeFixtureOp::FxGoalGroundAt: c->f[0] = i[0] == 0 ? GOAL->GetGroundTarget().GetX() : GOAL->GetGroundTarget().GetY();
        return 0;
    case EeFixtureOp::FxBuildQueueSize: return (int64_t)u->mCitizenBuildQueue.size();
    case EeFixtureOp::FxBuildQueueAt: return Handle(u->mCitizenBuildQueue[(size_t)i[0]]);
    case EeFixtureOp::FxMessageInt: return (int64_t)As<EEMRecord>(c->h)->Get<int32_t>((unsigned long)i[0]);
    case EeFixtureOp::FxMessageBool: return As<EEMRecord>(c->h)->Get<bool>((unsigned long)i[0]) ? 1 : 0;
    case EeFixtureOp::FxMessageFloat: c->f[0] = As<EEMRecord>(c->h)->Get<float>((unsigned long)i[0]);
        return 0;
    case EeFixtureOp::FxCalamityClassAt: return Handle(CALAMITIES->GetCalamityClass((unsigned long)i[0]));
    case EeFixtureOp::FxCalamityClassEnum: return (unsigned long)As<EECalamityClass>(c->h)->mClassEnum;
    case EeFixtureOp::FxCalamityClassTarget: return As<EECalamityClass>(c->h)->GetTargetKind();
    case EeFixtureOp::FxCastAtUnit: return CALAMITIES->CreateUnitCalamity((unsigned long)i[0], (unsigned long)i[1], (unsigned long)i[3], (unsigned long)i[2]) ? 1 : 0;
    case EeFixtureOp::FxCastAtLocation: return CALAMITIES->CreateLocationCalamity((unsigned long)i[0], (unsigned long)i[1], (long)i[3], (long)i[4], (unsigned long)i[2]) ? 1 : 0;
    case EeFixtureOp::FxCastGlobal: return CALAMITIES->CreateGlobalCalamity((unsigned long)i[0], (unsigned long)i[1], (unsigned long)i[2]) ? 1 : 0;
    case EeFixtureOp::FxCalamityCount: return (int64_t)CALAMITIES->mCalamities.size();
    case EeFixtureOp::FxCalamityEnumAt: return (unsigned long)CALAMITIES->mCalamities[(size_t)i[0]]->mClass->mClassEnum;
    case EeFixtureOp::FxCalamityTimeAlive: return (long)CALAMITIES->mCalamities[(size_t)i[0]]->GetTimeAlive();
    // ---- the WON lobby's walk (lobby.rs)
    case EeFixtureOp::FxLobbyOpen: return EEIsLobbyOpen() ? 1 : 0;
    case EeFixtureOp::FxLobbyManager: return Handle(WONAPI::GetLobbyWindow());
    case EeFixtureOp::FxLobbyRootWindow: return Handle(WONAPI::LobbyGetRootWindow());
    case EeFixtureOp::FxLobbyPostWork: As<WONAPI::WindowManager>(c->h)->PostEvent(new WONAPI::UserEvent(LobbyWork, 0));
        return 0;
    case EeFixtureOp::FxWindowRoot: return Handle(As<WONAPI::Window>(c->h)->mRoot.get());
    case EeFixtureOp::FxManagerModal: return Handle(As<WONAPI::WindowManager>(c->h)->GetModalComponent());
    case EeFixtureOp::FxWindowMouse: As<WONAPI::Window>(c->h)->QueueEvent(new WONAPI::MouseEvent(i[0] == 0 ? WONAPI::InputEvent_MouseMove : i[0] == 1 ? WONAPI::InputEvent_MouseDown : WONAPI::InputEvent_MouseUp, (int)i[1], (int)i[2], 0, 0));
        return 0;
    case EeFixtureOp::FxWindowKeyChar: As<WONAPI::Window>(c->h)->QueueEvent(new WONAPI::KeyEvent(WONAPI::InputEvent_KeyChar, (int)i[0], 0));
        return 0;
    case EeFixtureOp::FxCompVisible: return As<WONAPI::Component>(c->h)->IsVisible() ? 1 : 0;
    case EeFixtureOp::FxCompControlId: return As<WONAPI::Component>(c->h)->mControlId;
    case EeFixtureOp::FxCompDisabled: return As<WONAPI::Component>(c->h)->IsDisabled() ? 1 : 0;
    case EeFixtureOp::FxCompWidth: return As<WONAPI::Component>(c->h)->Width();
    case EeFixtureOp::FxCompHeight: return As<WONAPI::Component>(c->h)->Height();
    case EeFixtureOp::FxCompLocalToRoot: As<WONAPI::Component>(c->h)->LocalToRoot(*(int*)&c->i[0], *(int*)&c->i[1]);
        return 0;
    case EeFixtureOp::FxCompRootWindow: return Handle(As<WONAPI::Component>(c->h)->GetRootWindow());
    case EeFixtureOp::FxCompNextSibling: return Handle(As<WONAPI::Component>(c->h)->GetNextSibling(true));
    case EeFixtureOp::FxAsLabel: return Handle(dynamic_cast<WONAPI::Label*>(As<WONAPI::Component>(c->h)));
    case EeFixtureOp::FxAsButton: return Handle(dynamic_cast<WONAPI::Button*>(As<WONAPI::Component>(c->h)));
    case EeFixtureOp::FxAsListArea: return Handle(dynamic_cast<WONAPI::ListArea*>(As<WONAPI::Component>(c->h)));
    case EeFixtureOp::FxAsContainer: return Handle(dynamic_cast<WONAPI::Container*>(As<WONAPI::Component>(c->h)));
    case EeFixtureOp::FxContainerFront: return Handle(As<WONAPI::Container>(c->h)->GetFrontChild());
    case EeFixtureOp::FxLabelHasText: return As<WONAPI::Label>(c->h)->mWrappedText.get() != NULL;
    case EeFixtureOp::FxLabelTextLen: return (int64_t)As<WONAPI::Label>(c->h)->mWrappedText->GetText().length();
    case EeFixtureOp::FxLabelTextAt: return (unsigned short)As<WONAPI::Label>(c->h)->mWrappedText->GetText().at((int)i[0]);
    case EeFixtureOp::FxButtonTextLen: return (int64_t)As<WONAPI::Button>(c->h)->GetText().length();
    case EeFixtureOp::FxButtonTextAt: return (unsigned short)As<WONAPI::Button>(c->h)->GetText().at((int)i[0]);
    case EeFixtureOp::FxButtonActivate: As<WONAPI::Button>(c->h)->Fn2F3440();   // Button::Activate
        return 0;
    case EeFixtureOp::FxListSelItem: return Handle(As<WONAPI::ListArea>(c->h)->GetSelItem());
    case EeFixtureOp::FxListItemCount: return As<WONAPI::ListArea>(c->h)->GetNumItems();
    case EeFixtureOp::FxListItemPos: return As<WONAPI::ListArea>(c->h)->GetItemPos(As<WONAPI::ListItem>(c->h2));
    case EeFixtureOp::FxListSetSel: As<WONAPI::ListArea>(c->h)->SetSelItem((int)i[0], true);
        return 0;
    case EeFixtureOp::FxLobbyShown: return EELobbyIsShown() ? 1 : 0;
    case EeFixtureOp::FxLobbyScreenType: return WONAPI::LobbyGetGameScreenType();
    case EeFixtureOp::FxLobbyOnLan: return WONAPI::LobbyOnLanScreen() ? 1 : 0;
    case EeFixtureOp::FxLobbyOnLogin: return WONAPI::LobbyOnLoginScreen() ? 1 : 0;
    case EeFixtureOp::FxMyGame: return Handle(WONAPI::GetMyLobbyGame());
    case EeFixtureOp::FxMyClient: return Handle(WONAPI::GetMyLobbyClient());
    case EeFixtureOp::FxGameIsHost: return As<WONAPI::LobbyGame>(c->h)->GetIsHost() ? 1 : 0;
    case EeFixtureOp::FxGameClientCount: return As<WONAPI::LobbyGame>(c->h)->GetClients().get() != NULL ? (int64_t)As<WONAPI::LobbyGame>(c->h)->GetClients()->GetCount() : 0;
    // AreAllReady reads the client list: a just-joined game has none yet (a fixture-only crash at Join Game)
    case EeFixtureOp::FxGameAllReady: return As<WONAPI::LobbyGame>(c->h)->GetClients().get() != NULL && As<WONAPI::LobbyGame>(c->h)->AreAllReady();
    case EeFixtureOp::FxClientReady: return As<WONAPI::LobbyClient>(c->h)->IsReady() ? 1 : 0;
    case EeFixtureOp::FxGameNameLen: return (int64_t)As<WONAPI::LobbyGame>(c->h)->GetName().length();
    case EeFixtureOp::FxGameNameAt: return (unsigned short)As<WONAPI::LobbyGame>(c->h)->GetName().at((int)i[0]);
    default:
        break;
    }
    return 0;
}

}   // namespace

// The test switches, when EE_TEST_UI names a directory (EERun, first, before the engine starts its clocks and
// threads): EE_TEST_TIME_FACTOR, EE_TEST_SEED, EE_TEST_NO_HUMAN_DEFEAT, EE_TEST_PORT_OFFSET (the Rust fixture's
// poll.rs); the debug output goes to stderr ("debugstring <text>")
// (EERun calls it through gEETestFixtureConfigure, which this unit sets at static initialization, and starts the
// fixture's thread itself: ee_fixture_start)
static void EETestFixtureConfigure()
{
    ee_fixture_configure(EEFixtureHost);
}
extern void (*gEETestFixtureConfigure)();   // EEGlobals.cpp
static const bool sEETestFixtureRegistered = (gEETestFixtureConfigure = EETestFixtureConfigure, true);

// TEST ONLY: the legacy debugger harness (tools/harness/game_state_probe.py) reads the communications' numbers from
// this mirror, by its map name: their state is Rust's (ee_net), at no offset of the C++ object. The presentation
// bridge (tools/harness/presentation_bridge.c) refreshes it before each present through EETestCommsRefresh.
extern "C" {
struct EETestComms {
    int32_t mLastTickReceived;
    uint32_t mCTsProcessed;
    uint8_t mPaused;
    uint8_t mMultiplayer;
    uint8_t mValid;   // 0: no communications object
};
EETestComms gEETestComms;
// The render thread calls it, so it reads gCommunications only under the UI copy lock, taken without waiting (as the
// bridge's ui_test takes it: a UI event can hold the lock while it waits for the render thread): the UI thread's
// actions and events replace the communications object under that lock (EEAJoinOrHostAGame's delete and new when a
// game is set up), and a read between the delete and the new (the legacy tour's crash in this function) used a
// freed object. When the lock is busy the mirror keeps its last values until the next present.
void EETestCommsRefresh()
{
    TSCriticalSection* theLock = gUIManager ? gUIManager->GetUILock() : NULL;
    if (!theLock || !theLock->TryEnter())
        return;
    NESynchronousCommunications* theCommunications = gCommunications;
    if (!theCommunications) {
        gEETestComms.mValid = 0;
    } else {
        gEETestComms.mLastTickReceived = (int32_t)theCommunications->GetLastTickReceived();
        gEETestComms.mCTsProcessed = (uint32_t)theCommunications->GetNumberOfCTsProcessedInPhysics();
        gEETestComms.mPaused = theCommunications->GetIsGamePaused() ? 1 : 0;
        gEETestComms.mMultiplayer = theCommunications->GetIsMultiplayerSession() ? 1 : 0;
        gEETestComms.mValid = 1;
    }
    theLock->Leave();
}
}
#if defined(_MSC_VER)
#pragma comment(linker, "/include:EETestCommsRefresh")
#pragma comment(linker, "/include:gEETestComms")
#endif
