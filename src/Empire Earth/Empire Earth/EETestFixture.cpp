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
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "lle/U2DSparseArray.h"
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
#include "lle/ISDeviceInput.h"
#include "platform/PlatWindow.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UIFileListBox.h"
#include "lle/UIComboBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UIBitmap.h"
#include "lle/NESynchronousServer.h"
#include "lle/FSRestorable.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEGameOptions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/GETerrainMesh.h"
#include "ee/EECameraControl.h"
#include "ee/EEMap.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGameVariant.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEUICPing.h"
#include "ee/EEStartingResources.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEResourceType.h"
#include "platform/PlatSim.h"
#include "platform/PlatTime.h"
#include "ee/RTSGraphics.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "lle/TSReadWriteLock.h"
#include "ee/EEUnit.h"
#include "lle/USingleton.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEGraphics.h"
#include "ee/EEUICSelectUnits.h"
#include "lle/ISModifierState.h"
#include "rust/game/screens.h"
#include "ee/EEScreenEvent.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>
#include "rust/game/fixture.h"

bool EEIsLobbyOpen();                        // EELobbyInterface.cpp: the external (WON) lobby is open
extern void (*gEESelectUnitsTestHook)(const std::vector<long>& inUnitIDs);   // EEMessage.cpp: each box selection's ids
extern void (*gEEMessageExecutedTestHook)(NEMessage* inMessage, int inPhase);   // EEPhysics.cpp (DrainInput)

// ---- the WON lobby (TEST ONLY). The lobby owns the UI thread while it is open and runs its own message loop
// (WONAPI::WindowManager), so its state is read and its controls are driven on that loop, by a UserEvent the fixture
// posts (WindowManager::PostEvent is thread-safe); nothing of the lobby's is touched from the fixture's thread. The
// snapshot goes into ui-state.txt as
//   "lobby shown <0|1> screen <t> lan <0|1> login <0|1> modal <0|1> game <0|1> host <0|1> clients <n> ready <0|1>
//    allready <0|1> name <text>" (screen: LobbyGetGameScreenType, 1 LAN; game: GetMyLobbyGame != NULL),
//   "wonctl <controlId> <x> <y> <w> <h> <enabled> <text>" per visible component with a control id (root coordinates
//   of the lobby window, 1024x768; the text of a button), and
//   "wonlist <k> items <n> sel <i>" per visible list area, in tree order, and "wontext <text>" per label of the
//   modal dialog (a message box's message).
// The commands: "<seq> wonclick <controlId> 0" (a mouse move, press and release at the component's centre, through
// the window's event queue: the real button path; the modal dialog's component is searched first),
// "<seq> wonactivate <controlId> 0" (Button::Activate, the keyboard path), "<seq> wonselect <k> <item>" (the k-th
// list area's selection, ListArea::SetSelItem with its event), "<seq> wonkey <code> 0" (one typed character to the
// focused control), "<seq> wonclickat <x> <y>" (a click at root coordinates). The verdict ("TEST UI lobby-...") is
// printed by the lobby thread when the command ran.
namespace {

std::mutex sLobbyMutex;
std::string sLobbySnapshot;                  // the last snapshot the lobby thread wrote
std::string sLobbyCommand;                   // a pending "<op> <a> <b>" for the lobby thread (empty: none)
unsigned long sLobbyCommandSeq = 0;
std::atomic<int> sLobbyWorkPosted(0);        // a UserEvent is queued and not yet delivered
WONAPI::Component* sLobbyModal = NULL;       // the walk's modal component (lobby thread only)
bool sLobbyInModal = false;

std::string GUIText(const WONAPI::GUIString& inText)
{
    std::string theOut;
    for (int k = 0; k < (int)inText.length() && k < 60; k++) {
        unsigned short c = inText.at(k);
        theOut += (c < 0x21 || c >= 0x7f || c == '|') ? '_' : (char)c;
    }
    return theOut.empty() ? std::string("-") : theOut;
}

// The visible components with a control id and the list areas below inComponent, in tree order
void LobbyWalk(WONAPI::Component* inComponent, std::string& outText, std::vector<WONAPI::ListArea*>& outLists)
{
    using namespace WONAPI;
    if (inComponent == NULL || !inComponent->IsVisible())
        return;
    bool theModalEntered = inComponent == sLobbyModal;
    if (theModalEntered)
        sLobbyInModal = true;
    if (sLobbyInModal && inComponent->mControlId == 0) {   // the modal dialog's texts (a message box's message)
        if (Label* theLabel = dynamic_cast<Label*>(inComponent)) {
            if (theLabel->mWrappedText.get() != NULL && theLabel->mWrappedText->GetText().length() > 0)
                outText += "wontext " + GUIText(theLabel->mWrappedText->GetText()) + "\n";
        }
    }
    if (inComponent->mControlId != 0) {
        int x = 0, y = 0;
        inComponent->LocalToRoot(x, y);
        std::string theText("-");
        if (Button* theButton = dynamic_cast<Button*>(inComponent))
            theText = GUIText(theButton->GetText());
        else if (Label* theLabel = dynamic_cast<Label*>(inComponent))
            theText = theLabel->mWrappedText.get() ? GUIText(theLabel->mWrappedText->GetText()) : std::string("-");
        char theLine[200];
        snprintf(theLine, sizeof(theLine), "wonctl %d %d %d %d %d %d %s\n", inComponent->mControlId, x, y,
                 inComponent->Width(), inComponent->Height(), inComponent->IsDisabled() ? 0 : 1, theText.c_str());
        outText += theLine;
    }
    if (ListArea* theList = dynamic_cast<ListArea*>(inComponent)) {
        ListItem* theSelected = theList->GetSelItem();
        char theLine[100];
        snprintf(theLine, sizeof(theLine), "wonlist %d items %d sel %d\n", (int)outLists.size(), theList->GetNumItems(),
                 theSelected ? theList->GetItemPos(theSelected) : -1);
        outText += theLine;
        outLists.push_back(theList);
    }
    if (Container* theContainer = dynamic_cast<Container*>(inComponent)) {
        for (Component* theChild = theContainer->GetFrontChild(); theChild != NULL;
             theChild = theChild->GetNextSibling(true))
            LobbyWalk(theChild, outText, outLists);
    }
    if (theModalEntered)
        sLobbyInModal = false;
}

// The first visible, enabled component with control id inId below inComponent
WONAPI::Component* LobbyFindById(WONAPI::Component* inComponent, long inId)
{
    using namespace WONAPI;
    if (inComponent == NULL || !inComponent->IsVisible())
        return NULL;
    if (inComponent->mControlId == inId && !inComponent->IsDisabled())
        return inComponent;
    if (Container* theContainer = dynamic_cast<Container*>(inComponent)) {
        for (Component* theChild = theContainer->GetFrontChild(); theChild != NULL;
             theChild = theChild->GetNextSibling(true))
            if (Component* theFound = LobbyFindById(theChild, inId))
                return theFound;
    }
    return NULL;
}

// A click (a mouse move, press and release) at root coordinates of a lobby window
void LobbyClick(WONAPI::Window* inWindow, int x, int y)
{
    using namespace WONAPI;
    inWindow->QueueEvent(new MouseEvent(InputEvent_MouseMove, x, y, 0, 0));
    inWindow->QueueEvent(new MouseEvent(InputEvent_MouseDown, x, y, 0, 0));
    inWindow->QueueEvent(new MouseEvent(InputEvent_MouseUp, x, y, 0, 0));
}

// On the lobby thread (a delivered UserEvent): runs the pending command, then takes the snapshot
void LobbyWork(int)
{
    using namespace WONAPI;
    std::string theCommand;
    unsigned long theSeq = 0;
    {
        std::lock_guard<std::mutex> theLock(sLobbyMutex);
        theCommand.swap(sLobbyCommand);
        theSeq = sLobbyCommandSeq;
    }
    Window* theWindow = LobbyGetRootWindow();
    WindowManager* theManager = GetLobbyWindow();
    Component* theRoot = theWindow != NULL ? theWindow->mRoot.get() : NULL;
    if (!theCommand.empty()) {
        char theOp[32] = {0};
        long a = 0, b = 0;
        sscanf(theCommand.c_str(), "%31s %ld %ld", theOp, &a, &b);
        const char* theVerdict = "lobby-rejected";
        Component* theScope = (theManager != NULL && theManager->GetModalComponent() != NULL) ? theManager->GetModalComponent() : theRoot;
        if (!strcmp(theOp, "wonclick") || !strcmp(theOp, "wonactivate")) {
            Component* theTarget = LobbyFindById(theScope, a);
            if (theTarget == NULL)
                theTarget = LobbyFindById(theRoot, a);
            if (theTarget != NULL && !strcmp(theOp, "wonclick")) {
                int x = theTarget->Width() / 2, y = theTarget->Height() / 2;
                theTarget->LocalToRoot(x, y);
                if (Window* theTargetWindow = theTarget->GetRootWindow()) {
                    LobbyClick(theTargetWindow, x, y);
                    theVerdict = "lobby-clicked";
                }
            } else if (theTarget != NULL) {
                if (Button* theButton = dynamic_cast<Button*>(theTarget)) {
                    theButton->Fn2F3440();   // Button::Activate
                    theVerdict = "lobby-activated";
                }
            }
        } else if (!strcmp(theOp, "wonclickat")) {
            // "<seq> wonclickat <x> <y>": a left click at root coordinates of the lobby window (a tab, a list row)
            if (theWindow != NULL) {
                LobbyClick(theWindow, (int)a, (int)b);
                theVerdict = "lobby-clicked";
            }
        } else if (!strcmp(theOp, "wonkey")) {
            // "<seq> wonkey <code> 0": one typed character to the lobby window's focus (WM_CHAR's path)
            if (theWindow != NULL && a > 0) {
                theWindow->QueueEvent(new KeyEvent(InputEvent_KeyChar, (int)a, 0));
                theVerdict = "lobby-typed";
            }
        } else if (!strcmp(theOp, "wonselect")) {
            std::string theUnused;
            std::vector<ListArea*> theLists;
            LobbyWalk(theRoot, theUnused, theLists);
            if (a >= 0 && a < (long)theLists.size() && b >= 0 && b < theLists[a]->GetNumItems()) {
                theLists[a]->SetSelItem((int)b, true);
                theVerdict = "lobby-selected";
            }
        }
        fprintf(stderr, "TEST UI %s %s control %ld value %ld on form 0 (seq %lu, top form 0)\n", theVerdict, theOp, a, b, theSeq);
        fflush(stderr);
    }
    std::string theText;
    LobbyGame* theGame = GetMyLobbyGame();
    LobbyClient* theClient = GetMyLobbyClient();
    char theLine[300];
    snprintf(theLine, sizeof(theLine), "lobby shown %d screen %d lan %d login %d modal %d game %d host %d clients %d ready %d allready %d name %s\n",
             EELobbyIsShown() ? 1 : 0, LobbyGetGameScreenType(), LobbyOnLanScreen() ? 1 : 0, LobbyOnLoginScreen() ? 1 : 0,
             (theManager != NULL && theManager->GetModalComponent() != NULL) ? 1 : 0, theGame != NULL ? 1 : 0,
             (theGame != NULL && theGame->GetIsHost()) ? 1 : 0,
             (theGame != NULL && theGame->GetClients().get() != NULL) ? (int)theGame->GetClients()->GetCount() : 0,
             // AreAllReady reads the client list: a just-joined game has none yet (a fixture-only crash at Join Game)
             (theClient != NULL && theClient->IsReady()) ? 1 : 0,
             (theGame != NULL && theGame->GetClients().get() != NULL && theGame->AreAllReady()) ? 1 : 0,
             theGame != NULL ? GUIText(theGame->GetName()).c_str() : "-");
    theText += theLine;
    std::vector<ListArea*> theLists;
    sLobbyModal = theManager != NULL ? theManager->GetModalComponent() : NULL;
    sLobbyInModal = false;
    LobbyWalk(theRoot, theText, theLists);
    {
        std::lock_guard<std::mutex> theLock(sLobbyMutex);
        sLobbySnapshot = theText;
    }
    sLobbyWorkPosted = 0;
}

// Queues the lobby work on the lobby's message loop when none is queued (any thread)
void LobbyPostWork()
{
    WONAPI::WindowManager* theManager = EEIsLobbyOpen() ? WONAPI::GetLobbyWindow() : NULL;
    if (theManager != NULL && !sLobbyWorkPosted.exchange(1))
        theManager->PostEvent(new WONAPI::UserEvent(LobbyWork, 0));
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

UIControl* ControlOf(EeFixtureCall* c)
{
    UIForm* theForm = As<UIForm>(c->h);
    return theForm != NULL && (unsigned long)c->i[0] < theForm->GetControlCount() ? theForm->GetControl((unsigned long)c->i[0]) : NULL;
}

void Lines(EeFixtureCall* c, const UILBString* inList)
{
    for (unsigned long k = 0; inList != NULL && k < inList->GetLineCount() && k < 64; k++)
        ee_fixture_sink_text(c->sink, (const char*)inList->GetData(k));
}

// One fixture call (rust/crates/ee_game/src/fixture/host.rs documents each operation's arguments and answers)
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
    case EeFixtureOp::SetTimeFactor:
        return PlatSimSetTimeFactor((uint32_t)i[0], (uint32_t)i[1]) ? 1 : 0;
    case EeFixtureOp::SetSwitch:
        if (i[0] == 0) {
            gPlatSimSeed = (uint32_t)i[1];
            gPlatSimSeedSet = 1;
        } else if (i[0] == 1)
            gPlatSimNoHumanDefeat = (int)i[1];
        else
            gPlatSimPortOffset = (int)i[1];
        return 0;
    case EeFixtureOp::Install:
        if (i[0] == 0) {
            PlatDebugSetHandler(DebugToStderr);
#ifdef _WIN32
            AddVectoredExceptionHandler(1, FixtureFault);
#endif
        } else {
            gEESelectUnitsTestHook = LassoHook;        // each box selection's ids (the boxselect command)
            gEEMessageExecutedTestHook = MessageHook;  // each executed input's view (the order command)
        }
        return 0;
    case EeFixtureOp::Clock: {
        uint32_t theNum = 1, theDen = 1;
        PlatSimGetTimeFactor(&theNum, &theDen);
        c->i[0] = theNum;
        c->i[1] = theDen;
        c->i[2] = (unsigned long)PlatTimeMilliseconds();
        return 0;
    }
    case EeFixtureOp::UiLock:
        if (i[0])
            gUIManager->LockUIForCopyData();
        else
            gUIManager->UnlockUIForCopyData();
        return 0;
    case EeFixtureOp::TopForm: {
        UIForm* theForm = gUIManager->GetCurrentForm();
        UIControl* theOver = theForm ? theForm->GetMouseOverControl() : NULL;
        c->i[0] = theForm ? theForm->GetFormID() : 0;
        c->i[1] = theForm ? theForm->GetControlCount() : 0;
        c->i[2] = theOver ? (long)theOver->GetIndex() : -1;
        for (unsigned long k = 0; k < gUIManager->GetFormCount() && k < 64; k++)
            if (UIForm* theStacked = gUIManager->GetStackedForm(k))
                ee_fixture_sink_id(c->sink, theStacked->GetFormID());
        return Handle(theForm);
    }
    case EeFixtureOp::Window: {   // no screen while the rasterizer is re-activated (a settings change)
        GERasterizer* theRasterizer = gUIManager->GetRasterizer();
        GEWindow* theWindow = gUIManager->GetScreen() < theRasterizer->GetScreenCount()
            ? theRasterizer->GetWindow(gUIManager->GetScreen()) : NULL;
        if (theWindow == NULL)
            return 0;
        c->i[0] = theWindow->GetWidth();   // the game's (logical) pixels: the pointer's coordinates
        c->i[1] = theWindow->GetHeight();
        return 1;
    }
    case EeFixtureOp::Control: {
        UIControl* theControl = ControlOf(c);
        if (theControl == NULL)
            return -1;
        c->i[1] = theControl->GetTriggerEvent(kTriggerEvent) != NULL;
        unsigned long theSelection = 0xffffffffUL;
        if (UIComboBox* theCombo = dynamic_cast<UIComboBox*>(theControl)) {
            theCombo->GetCurrentSelection(theSelection);
            c->i[2] = (int32_t)theSelection;
            c->i[3] = theCombo->GetListBox() ? theCombo->GetListBox()->GetLineCount() : 0;
            Lines(c, theCombo->GetListBox());
            return 1;
        }
        if (UICheckBox* theCheck = dynamic_cast<UICheckBox*>(theControl)) {
            c->i[2] = theCheck->GetIsChecked() ? 1 : 0;
            return 2;
        }
        if (UILBString* theList = dynamic_cast<UILBString*>(theControl)) {
            theList->GetCurrentSelection(theSelection);
            c->i[2] = (int32_t)theSelection;
            c->i[3] = theList->GetLineCount();
            Lines(c, theList);
            return 3;
        }
        return 0;
    }
    case EeFixtureOp::Click: {
        UIControl* theControl = ControlOf(c);
        if (theControl == NULL || !theControl->GetTriggerEvent(kTriggerEvent))
            return 0;
        if (UIRadioButton* theRadio = dynamic_cast<UIRadioButton*>(theControl))
            theRadio->Check();
        theControl->FireTriggerEvent(kTriggerEvent);
        return 1;
    }
    case EeFixtureOp::ComboSelect: {
        UIComboBox* theCombo = dynamic_cast<UIComboBox*>(ControlOf(c));
        if (theCombo == NULL || theCombo->GetListBox() == NULL || (unsigned long)i[1] >= theCombo->GetListBox()->GetLineCount())
            return 0;
        theCombo->SetCurrentSelection((unsigned long)i[1]);
        return 1;
    }
    case EeFixtureOp::CheckSet: {
        UICheckBox* theCheck = dynamic_cast<UICheckBox*>(ControlOf(c));
        if (theCheck != NULL)
            theCheck->SetChecked(i[1] != 0);
        return theCheck != NULL;
    }
    case EeFixtureOp::ListSelect: {
        UILBString* theList = dynamic_cast<UILBString*>(ControlOf(c));
        if (theList == NULL || (unsigned long)i[1] >= theList->GetLineCount())
            return 0;
        theList->ChangeSelectionIndex((unsigned long)i[1], true, false);
        return 1;
    }
    case EeFixtureOp::ClickEvent: {   // the editor exercise: the main menu's Editors, the editors' Scenario Editor
        UIForm* theForm = As<UIForm>(c->h);
        for (unsigned long k = 0; k < theForm->GetControlCount(); k++) {
            UIControl* theOne = theForm->GetControl(k);
            EEScreenEvent* theEvent = theOne ? dynamic_cast<EEScreenEvent*>(theOne->GetTriggerEvent(kTriggerEvent)) : NULL;
            if (theEvent != NULL && theEvent->mEvent == (unsigned long)i[0]) {
                theOne->FireTriggerEvent(kTriggerEvent);
                return 1;
            }
        }
        return 0;
    }
    case EeFixtureOp::ScreenEvent:   // as a dialog's answer posts it (the editor exercise's "exit without saving")
        ee_screens_event((uint32_t)i[0], As<UIForm>(c->h), NULL);
        return 0;
    case EeFixtureOp::Chat:
        gUserInterface->AddChatLogEntry(UWideString("TEST chat line: the chat display (EEUICChatLine)"), 0);
        return 1;
    case EeFixtureOp::Zoom:
        gWorld.mCameraControl.SetZoom(c->f[0]);
        return 0;
    case EeFixtureOp::MouseSet:
    case EeFixtureOp::MouseFix: {
        if (gMouse == NULL)
            return c->op == EeFixtureOp::MouseSet ? 0 : -1;
        GE2DPoint theAt(c->f[0], c->f[1]);
        int64_t theResult = 1;
        gMouse->mInputLock->Enter();
        if (c->op == EeFixtureOp::MouseSet || gMouse->mPosition.GetX() != theAt.GetX() || gMouse->mPosition.GetY() != theAt.GetY())
            gMouse->SetPosition(theAt);
        else
            theResult = 0;
        c->f[0] = gMouse->mPosition.GetX();
        c->f[1] = gMouse->mPosition.GetY();
        gMouse->mInputLock->Leave();
        return theResult;
    }
    case EeFixtureOp::UiUpdate:
        gUIManager->ForceUIUpdate();
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
    case EeFixtureOp::Modifiers: {
        unsigned long theState = gISKeyboard->GetModifierState() & (kModifierShift | kModifierControl);
        return theState == 0 ? 0 : theState == kModifierShift ? 1 : 2;
    }
    case EeFixtureOp::State:
        return (gPhysics != NULL ? 1 : 0) | (gWorld.mMap != NULL ? 2 : 0) | (gWorld.mLock != NULL ? 4 : 0)
            | (gMouse != NULL ? 8 : 0) | (gISKeyboard != NULL ? 16 : 0) | (gUserInterface != NULL ? 32 : 0)
            | (EEIsLobbyOpen() ? 64 : 0) | (gUIManager != NULL ? 128 : 0);
    case EeFixtureOp::SelectControl: {
        UIForm* theForm = As<UIForm>(c->h);
        return Handle(kMainGameSelectUnits < theForm->GetControlCount()
            ? dynamic_cast<EEUICSelectUnits*>(theForm->GetControl(kMainGameSelectUnits)) : NULL);
    }
    case EeFixtureOp::SelectState: {   // as UpdateMouse writes it
        EEUICSelectUnits* theControl = As<EEUICSelectUnits>(c->h);
        c->i[0] = theControl->GetForm()->GetIsMouseCaptured();
        c->i[1] = theControl->GetIsDragging();
        c->f[0] = theControl->GetDragEnd().GetX();
        c->f[1] = theControl->GetDragEnd().GetY();
        return 0;
    }
    case EeFixtureOp::ViewRect: {
        GERectangle theView = gViewport.GetViewRectangle();
        c->f[0] = theView.GetLeft();
        c->f[1] = theView.GetTop();
        c->f[2] = theView.GetRight();
        c->f[3] = theView.GetBottom();
        return 0;
    }
    case EeFixtureOp::Post:
        if (i[0] == 0)
            PostAction(new EEMRecord(kMessageSelectTheseUnits, {std::vector<long>(c->ids, c->ids + c->n)}));
        else if (i[0] == 1)
            PostAction(new EEMRecord(kMessageKillUnit, {(unsigned char)0}));   // the first selected unit
        else if (i[0] == 2)
            PostAction(new EEMRecord(kMessagePathTo, {0.0f, c->f[0], c->f[1], false, true, false, false}));
        else
            PostAction(new EEMRecord(kMessageStopActiveGroup));
        return 0;
    case EeFixtureOp::Mark:
        PostAction(NULL);
        return 0;
    case EeFixtureOp::CenterCamera: {   // posted outside the world's lock, as the mini map's click does
        EEACenterCamera* theAction = new EEACenterCamera;
        theAction->SetLocation(i[0], i[1]);
        theAction->Dereference();
        return 0;
    }
    case EeFixtureOp::StatusButton: {
        int64_t theFound = 0;
        gUIManager->LockUIForCopyData();
        UIForm* theForm = gUIManager->GetCurrentForm();
        for (unsigned long k = 0; theForm != NULL && k < theForm->GetControlCount() && !theFound; k++) {
            EEUIFMGStatusButton* theButton = dynamic_cast<EEUIFMGStatusButton*>(theForm->GetControl(k));
            if (theButton == NULL || !theButton->mIsCommand || theButton->mButton != (unsigned long)i[0]
                || theButton->GetDisplayMode() != kDisplayModeShown)
                continue;
            const GERectangle& theRectangle = theButton->GetRectangle();
            c->f[0] = (theRectangle.GetLeft() + theRectangle.GetRight()) * 0.5f;
            c->f[1] = (theRectangle.GetTop() + theRectangle.GetBottom()) * 0.5f;
            theFound = 1;
        }
        gUIManager->UnlockUIForCopyData();
        return theFound;
    }
    case EeFixtureOp::WorldLock:
        if (gWorld.mLock == NULL)
            return 0;
        if (i[0] == 0)
            gWorld.mLock->ReadLock();
        else if (i[0] == 1)
            gWorld.mLock->ReadUnlock();
        else if (i[0] == 2)
            gWorld.mLock->WriteLock();
        else
            gWorld.mLock->WriteUnlock();
        return 1;
    case EeFixtureOp::Physics:
        if (gPhysics == NULL)
            return 0;
        c->i[0] = (long)gPhysics->mGameTime;
        c->i[1] = (unsigned long)gPhysics->mNumTicksDone;
        c->i[2] = gPhysics->mGameOver ? 1 : 0;
        return 1;
    case EeFixtureOp::Comms: {
        NESynchronousCommunications* theCommunications = gCommunications;
        if (theCommunications == NULL)
            return 0;
        c->i[0] = theCommunications->GetNumberOfCTsProcessedInPhysics();
        c->i[1] = (long)theCommunications->GetLastTickReceived();
        c->i[2] = theCommunications->GetIsGamePaused() ? 1 : 0;
        c->i[3] = theCommunications->GetIsMultiplayerSession() ? 1 : 0;
        return 1;
    }
    case EeFixtureOp::Camera: {   // plain reads: the scroll check compares polls a second apart
        EEMap* theMap = gWorld.GetMap();
        if (theMap == NULL)
            return 0;
        EECameraControl& theCamera = gWorld.mCameraControl;
        GE3DPoint theFocus;
        theCamera.GetLocation(theCamera.mFraction, theFocus);
        GE3DPoint theTarget = theCamera.GetEndFocus();
        GETerrainMesh& theMesh = theMap->mTerrainMesh;
        c->f[0] = theFocus.GetX();
        c->f[1] = theFocus.GetY();
        c->f[2] = theFocus.GetZ();
        c->f[3] = theTarget.GetX();
        c->f[4] = theTarget.GetY();
        c->f[5] = theCamera.mZoom;
        c->i[0] = (long)theCamera.mMode;
        c->i[1] = (long)theMap->GetSize();
        c->i[2] = theMesh.GetIsValidPoint(UMFloorUnsigned(theFocus.GetX()), UMFloorUnsigned(theFocus.GetY()));
        c->i[3] = theMesh.GetIsValidPoint(UMFloorUnsigned(theTarget.GetX()), UMFloorUnsigned(theTarget.GetY()));
        return 1;
    }
    case EeFixtureOp::ActiveGroup: {
        EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
        if (thePlayer == NULL || thePlayer->mActiveGroup == NULL)
            return 0;
        for (EEComplexUnit* theUnit : thePlayer->mActiveGroup->mUnits)
            ee_fixture_sink_id(c->sink, Handle(theUnit));
        return 1;
    }
    case EeFixtureOp::UnitById:
        return Handle(gWorld.GetUnit((unsigned long)i[0]));
    case EeFixtureOp::PlayerUnit:
        return Handle(As<EEPlayer>(c->h)->GetUnit((unsigned long)i[0]));
    case EeFixtureOp::CanDelete:
        return u->CanDelete() ? 1 : 0;
    case EeFixtureOp::Pick: {
        EEUnit* theUnit = gUnitPicker.PickUnitUnreferenced(GE2DPoint(c->f[0], c->f[1]), (long)i[0]);
        return theUnit != NULL ? (int64_t)theUnit->mUnitID : -1;
    }
    case EeFixtureOp::PointTile: {
        EETile* theTile = NULL;
        GE3DPoint theWorld(0.0f, 0.0f, 0.0f);
        if (!EEPointUnitOrTile(GE2DPoint(c->f[0], c->f[1]), theTile, theWorld, true) || theTile == NULL
            || !theTile->IsSeenBy(gWorld.GetCurrentPlayer()))
            return 0;
        c->f[0] = theWorld.GetX();
        c->f[1] = theWorld.GetY();
        c->f[2] = theWorld.GetZ();
        return 1;
    }
    case EeFixtureOp::Distance:
        c->f[0] = u->DistanceSquared3D(GE3DPoint(c->f[0], c->f[1], c->f[2]));
        return 0;
    case EeFixtureOp::CanGather:
        return u->CanGather(As<EEUnit>(c->h2), i[0], i[1], i[2] != 0, i[3] != 0) ? 1 : 0;
    case EeFixtureOp::CanGetTo:
        return gContinentManager.CanGetTo(u, i[0], i[1]) ? 1 : 0;
    case EeFixtureOp::SeenBy:
        return EEIsSeenBy(As<EEUnit>(c->h), As<EEPlayer>(c->h2), true) ? 1 : 0;
    case EeFixtureOp::PatrolAction:
        return u->mUnitClass->FindAction(kActionGetNextPatrolPoint) != NULL ? 1 : 0;
    case EeFixtureOp::NearestRoute: {   // EEMPatrol(-1): the first unit's nearest town's route (EEUnitHost town point)
        EETown* theTown = u->FindNearestTown();
        EEComplexUnit* thePoint = theTown != NULL && theTown->mPatrolGroup != NULL
            ? theTown->mPatrolGroup->GetNearest(u->GetTileX(), u->GetTileY()) : NULL;
        if (thePoint == NULL)
            return -1;
        c->h2 = (uint64_t)Handle(thePoint->mPatrolGroup);
        return (int64_t)thePoint->mUnitID;
    }
    case EeFixtureOp::Goal: {
        EEUnitGoal* theGoal = As<EEUnitGoal>(c->h);
        EEUGRust* r = static_cast<EEUGRust*>(theGoal);
        EEUnit* theTarget = NULL;
        c->i[0] = 7;
        switch (theGoal->mKind) {
        case kGoalGather:
            c->i[0] = 0;
            theTarget = r->GetGatherResource();
            break;
        case kGoalLocation:
            c->i[0] = 1;
            c->f[0] = r->GetLocation().GetX();
            c->f[1] = r->GetLocation().GetY();
            break;
        case kGoalAttackUnit:
            c->i[0] = 2;
            theTarget = r->GetAttackTarget();
            break;
        case kGoalAttackGround:
            c->i[0] = 3;
            c->f[0] = r->GetGroundTarget().GetX();
            c->f[1] = r->GetGroundTarget().GetY();
            break;
        case kGoalPatrol:
            c->i[0] = 4;
            theTarget = r->GetPatrolTarget();
            c->i[2] = Handle(r->GetPatrolGroup());
            break;
        case kGoalBuildRepair:
            c->i[0] = 5;
            break;
        case kGoalExplore:
            c->i[0] = 6;
            break;
        default:
            break;
        }
        c->i[1] = theTarget != NULL ? (long)theTarget->mUnitID : -1;
        c->h2 = (uint64_t)Handle(theTarget);
        return theGoal->mKind;
    }
    case EeFixtureOp::LastQueued:
        if (u->mGoalQueues.empty() || u->mGoalQueues[0].mGoals.empty())
            return 0;
        return Handle(u->mGoalQueues[0].mGoals.back());
    case EeFixtureOp::BuildQueue:
        for (EEUnit* theUnit : u->mCitizenBuildQueue)
            ee_fixture_sink_id(c->sink, Handle(theUnit));
        return 0;
    case EeFixtureOp::SetHitPoints:
        u->mHitPoints = (long)i[0];
        return 0;
    case EeFixtureOp::MessageValue: {
        EEMRecord* theMessage = As<EEMRecord>(c->h);
        if (i[1] == 2)
            c->f[0] = theMessage->Get<float>((unsigned long)i[0]);
        return i[1] == 0 ? (int64_t)theMessage->Get<int32_t>((unsigned long)i[0])
                         : i[1] == 1 ? (theMessage->Get<bool>((unsigned long)i[0]) ? 1 : 0) : 0;
    }
    case EeFixtureOp::CalamityCount: {
        EECalamityManager* theManager = EECalamityManager::GetInstance();
        return theManager != NULL ? (int64_t)theManager->GetCalamityClassCount() : 0;
    }
    case EeFixtureOp::CalamityClass: {
        EECalamityClass* theClass = EECalamityManager::GetInstance()->GetCalamityClass((unsigned long)i[0]);
        if (theClass == NULL)
            return 0;
        c->i[1] = (unsigned long)theClass->mClassEnum;
        c->i[2] = theClass->GetTargetKind();
        return 1;
    }
    case EeFixtureOp::CalamityCast: {
        EECalamityManager* m = EECalamityManager::GetInstance();
        unsigned long theRow = (unsigned long)i[0], theCaster = (unsigned long)i[1], thePlayer = (unsigned long)i[2];
        if (i[3] == 0)
            return m->CreateUnitCalamity(theRow, theCaster, (unsigned long)i[4], thePlayer) ? 1 : 0;
        if (i[3] == 1)
            return m->CreateLocationCalamity(theRow, theCaster, (long)i[4], (long)i[5], thePlayer) ? 1 : 0;
        return m->CreateGlobalCalamity(theRow, theCaster, thePlayer) ? 1 : 0;
    }
    case EeFixtureOp::CalamityAlive: {
        EECalamityManager* theManager = EECalamityManager::GetInstance();
        long theAlive = -1;
        for (size_t k = 0; k < theManager->mCalamities.size(); k++)
            if ((unsigned long)theManager->mCalamities[k]->mClass->mClassEnum == (unsigned long)i[0])
                theAlive = (long)theManager->mCalamities[k]->GetTimeAlive();
        return theAlive;
    }
    case EeFixtureOp::LobbyPost:
        if (!EEIsLobbyOpen())
            return 0;
        {
            std::lock_guard<std::mutex> theLock(sLobbyMutex);
            sLobbyCommand = c->s;
            sLobbyCommandSeq = (unsigned long)i[0];
        }
        LobbyPostWork();
        return 1;
    case EeFixtureOp::LobbySnapshot: {   // the last snapshot the lobby's own thread took, and a new one asked for
        if (!EEIsLobbyOpen())
            return 0;
        std::string theSnapshot;
        {
            std::lock_guard<std::mutex> theLock(sLobbyMutex);
            theSnapshot = sLobbySnapshot;
        }
        ee_fixture_sink_text(c->sink, theSnapshot.c_str());
        LobbyPostWork();
        return 1;
    }
    }
    return 0;
}

}   // namespace

// The test switches, when EE_TEST_UI names a directory (EERun, first, before the engine starts its clocks and
// threads): EE_TEST_TIME_FACTOR, EE_TEST_SEED, EE_TEST_NO_HUMAN_DEFEAT, EE_TEST_PORT_OFFSET (the Rust fixture's
// poll.rs); the debug output goes to stderr ("debugstring <text>")
void EETestFixtureConfigure()
{
    ee_fixture_configure(EEFixtureHost);
}

// Starts the fixture's thread when EE_TEST_UI names a directory (EERun, after the engine is initialised)
void EETestFixtureStart()
{
    ee_fixture_start();
}

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
