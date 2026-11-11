// TEST ONLY: the in-process UI fixture of the POSIX test driver (tools/harness/posix_game.py). Port addition: the
// Windows harness reaches the same engine methods through its debugger and presentation bridge
// (tools/harness/presentation_bridge.c ui_test); on Linux and macOS there is no such debugger, so the game carries the
// fixture itself, and the Windows fixture driver (tools/harness/fixture_game.py, the gates) uses it too: it reads
// nothing by memory offset. It does nothing unless EE_TEST_UI names a directory.
//
// Every 250 ms a thread of its own (never the render or UI thread) takes the UI manager's lock and
//   - writes <dir>/ui-state.txt (atomically: .tmp + rename) in the bridge's format: "form <id> controls <n>",
//     "command <seq>", "pid <pid>", "stack <ids>", "control <i>" (controls with a 0x3f0 trigger event),
//     "combo <i> sel <s> lines <n> a|b|..", "check <i> <0|1>", "list <i> sel <s> lines <n> a|b|..",
//     "window <w> <h>", "mouseover <i>" (the control under the game's pointer, -1 for none: the form's own hit test); plus the
//     game telemetry: "game ms <t> ticks <n> over <0|1> players <n> map <0|1> local <i>" (ticks: the completed
//     physics ticks, PTs), "comm ct <n> last <n> paused <0|1> multiplayer <0|1>" (the communications ticks processed,
//     the last received, the pause), one "player <i> observer <0|1> result <r> population <n> victory <n> civ <locked>
//     <chosen> stock <f>/<w>/<s>/<g>/<i>" per world player, and "clock <num>/<den> ms <t> frames <n>" (the harness
//     clock's factor and time, the frames written so far);
//   - when <dir>/frame.request exists, removes it and writes the next presented frame to <dir>/frame.bmp (a top-down
//     32-bit BMP, through the renderer's GXTestSetPresentHook / GXTestReadFrame);
//   - executes the newest line of <dir>/test.command, "<seq> click|select|check|lselect <form> <control> [<k>]"
//     (or "<seq> zoom <form> 0 <percent>", the camera zoom, or "<seq> pointer <form> <x> <y>", the game's pointer
//     moved to a window position and the UI updated: hover through the controls' real hit test, or
//     "<seq> lclick <form> <x> <y>", the same and then a left button press and release as the platform's mouse
//     events deliver them (ISHandleEvent): a click through the real hit test, or
//     "<seq> boxselect <form> <mode> <x0> <y0> <x1> <y1>", a box selection in the game view: see "the box selection"
//     below, or "<seq> order <form> <kind>", an order to a group of the local player's units through the real input
//     path, judged unit by unit once its message executed: see EETestFixtureOrders.cpp),
//     through the controls' own methods (the events go to the application's UI queue, as a player's input does),
//     once per sequence number, only while <form> is on top; the verdict goes to stderr ("TEST UI ...").
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <thread>
#include <chrono>
#ifndef _WIN32
#include <unistd.h>
#else
#include <process.h>
#endif

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
#include "lle/ISDeviceInput.h"
#include "platform/PlatWindow.h"
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
#include "lle/FSPath.h"
#include "lle/UIScrollBar.h"
#include "lle/UIFileListBox.h"
#include "lle/UIComboBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UIBitmap.h"
#include "lle/UIBackground.h"
#include "lle/NESynchronousServer.h"
#include "lle/FSRestorable.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEARemovePlayer.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEGameOptions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGameVariant.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEUICPing.h"
#include "ee/EEUIELobbyEvents.h"
#include "ee/EEUIEPostGameSummaryEvents.h"
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
#include "ee/EEUnitClassIDs.h"
#include "ee/EEGraphics.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

bool EEIsLobbyOpen();                        // EELobbyInterface.cpp: the external (WON) lobby is open
namespace WONAPI { bool LobbyIAmReady(); }   // LobbyStagingSupport.cpp
extern void (*gEESelectUnitsTestHook)(const std::vector<long>& inUnitIDs);   // EEMessage.cpp: each box selection's ids
// the orders (EETestFixtureOrders.cpp)
void EETestOrdersInstall();
const char* EETestOrderStart(unsigned long inSeq, unsigned long inKind);
std::string EETestOrderSnapshot();

// the renderer's test hooks (rust/crates/ee_gx ffi.rs; GX.cpp keeps them linked): a callback before every presented
// frame, and a read-back of that frame as 32-bit BGRA rows
extern "C" void GXTestSetPresentHook(void (*inHook)(void* inWindow, void* inFrame));
extern "C" long GXTestReadFrame(void* inFrame, void* outPixels, unsigned long inCapacity, unsigned long* outWidth,
                                unsigned long* outHeight);

namespace {

const unsigned long kTriggerEvent = 0x3f0;
unsigned long sLastCommand = 0;
std::string sDirectory;                      // EE_TEST_UI
std::atomic<int> sFrameWanted(0);            // a frame.request seen: the next presented frame goes to frame.bmp
std::atomic<unsigned long> sFramesWritten(0);

// Replaces inTo with inFrom -> true on success. Windows' rename does not replace an existing file (the reader may
// hold it open for a moment: the next poll writes it again)
bool ReplaceWith(const std::string& inFrom, const std::string& inTo)
{
#ifdef _WIN32
    return MoveFileExA(inFrom.c_str(), inTo.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(inFrom.c_str(), inTo.c_str()) == 0;
#endif
}

// Before every presented frame (the render thread): when a frame was asked for, writes it to <dir>/frame.bmp
// (atomically: .tmp + rename), a top-down 32-bit BMP
void PresentHook(void* inWindow, void* inFrame)
{
    if (!sFrameWanted.exchange(0))
        return;
    unsigned long theWidth = 0, theHeight = 0;
    GXTestReadFrame(inFrame, NULL, 0, &theWidth, &theHeight);
    if (theWidth == 0 || theHeight == 0 || theWidth > 16384 || theHeight > 16384)
        return;
    std::string thePixels((size_t)theWidth * theHeight * 4, '\0');
    if (GXTestReadFrame(inFrame, &thePixels[0], (unsigned long)thePixels.size(), &theWidth, &theHeight) != 0)
        return;
    unsigned char theHeader[54] = {0};
    uint32_t theSize = 54 + (uint32_t)thePixels.size();
    int32_t theTopDown = -(int32_t)theHeight;
    theHeader[0] = 'B';
    theHeader[1] = 'M';
    memcpy(theHeader + 2, &theSize, 4);
    theHeader[10] = 54;    // the pixels' offset
    theHeader[14] = 40;    // BITMAPINFOHEADER
    memcpy(theHeader + 18, &theWidth, 4);
    memcpy(theHeader + 22, &theTopDown, 4);
    theHeader[26] = 1;     // planes
    theHeader[28] = 32;    // bits per pixel (BI_RGB)
    std::string theTemporary = sDirectory + "/frame.tmp";
    FILE* theFile = fopen(theTemporary.c_str(), "wb");
    if (!theFile)
        return;
    bool theWritten = fwrite(theHeader, 1, sizeof(theHeader), theFile) == sizeof(theHeader)
        && fwrite(thePixels.data(), 1, thePixels.size(), theFile) == thePixels.size();
    fclose(theFile);
    if (theWritten) {
        if (ReplaceWith(theTemporary, sDirectory + "/frame.bmp"))
            sFramesWritten++;
    }
}

void PrintText(FILE* outFile, const char* inText)
{
    for (int k = 0; inText && inText[k] && k < 80; k++) {
        unsigned char c = (unsigned char)inText[k];
        if (c < 0x20 || c >= 0x7f || c == '|' || c == '\\')
            fprintf(outFile, "\\x%02x", c);
        else
            fputc(c, outFile);
    }
}

void PrintLines(FILE* outFile, const UILBString* inList)
{
    unsigned long theLines = inList ? inList->GetLineCount() : 0;
    for (unsigned long k = 0; k < theLines && k < 64; k++) {
        fputc(k ? '|' : ' ', outFile);
        PrintText(outFile, (const char*)inList->GetData(k));
    }
    fputc('\n', outFile);
}

void WriteTelemetry(FILE* outFile)
{
    EEPhysics* thePhysics = gPhysics;
    unsigned long thePlayers = gWorld.GetNumPlayers();
    fprintf(outFile, "game ms %ld ticks %lu over %d players %lu map %d local %ld\n",
            thePhysics ? (long)thePhysics->mGameTime : -1L, thePhysics ? (unsigned long)thePhysics->mNumTicksDone : 0UL,
            thePhysics ? (int)thePhysics->mGameOver : 0, thePlayers, gWorld.mMap != NULL ? 1 : 0,
            (long)gWorld.mCurrentPlayer);
    NESynchronousCommunications* theCommunications = gCommunications;
    if (theCommunications)
        fprintf(outFile, "comm ct %lu last %ld paused %d multiplayer %d\n",
                theCommunications->GetNumberOfCTsProcessedInPhysics(), (long)theCommunications->GetLastTickReceived(),
                theCommunications->GetIsGamePaused() ? 1 : 0, theCommunications->GetIsMultiplayerSession() ? 1 : 0);
    for (unsigned long i = 0; i < thePlayers && i < 17; i++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(i);
        if (thePlayer) {
            const long* s = thePlayer->mResourceCaps;
            fprintf(outFile, "player %lu observer %d result %lu population %ld victory %ld civ %d %d "
                    "stock %ld/%ld/%ld/%ld/%ld\n", i,
                    (int)thePlayer->mObserverMode, (unsigned long)thePlayer->mGameResult,
                    (long)thePlayer->mPopulationCount, (long)thePlayer->mEndOfGameCount,
                    thePlayer->mCivilizationLocked ? 1 : 0, thePlayer->mCivilizationChosen ? 1 : 0,
                    (long)s[kResourceFood], (long)s[kResourceWood], (long)s[kResourceStone], (long)s[kResourceGold],
                    (long)s[kResourceIron]);
        }
    }
    uint32_t theNum = 1, theDen = 1;
    PlatSimGetTimeFactor(&theNum, &theDen);
    fprintf(outFile, "clock %lu/%lu ms %lu frames %lu\n", (unsigned long)theNum, (unsigned long)theDen,
            (unsigned long)PlatTimeMilliseconds(), sFramesWritten.load());
}

// ---- the WON lobby (TEST ONLY). The lobby owns the UI thread while it is open and runs its own message loop
// (WONAPI::WindowManager), so its state is read and its controls are driven on that loop, by a UserEvent this thread
// posts (WindowManager::PostEvent is thread-safe); nothing of the lobby's is touched from this thread. The snapshot
// goes into ui-state.txt as
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
        for (std::list<SmartPtr<Component> >::iterator it = theContainer->mChildList.begin();
             it != theContainer->mChildList.end(); ++it)
            LobbyWalk(it->get(), outText, outLists);
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
        for (std::list<SmartPtr<Component> >::iterator it = theContainer->mChildList.begin();
             it != theContainer->mChildList.end(); ++it)
            if (Component* theFound = LobbyFindById(it->get(), inId))
                return theFound;
    }
    return NULL;
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
        Component* theScope = (theManager != NULL && theManager->mModalComponent != NULL) ? theManager->mModalComponent : theRoot;
        if (!strcmp(theOp, "wonclick") || !strcmp(theOp, "wonactivate")) {
            Component* theTarget = LobbyFindById(theScope, a);
            if (theTarget == NULL)
                theTarget = LobbyFindById(theRoot, a);
            if (theTarget != NULL && !strcmp(theOp, "wonclick")) {
                int x = theTarget->Width() / 2, y = theTarget->Height() / 2;
                theTarget->LocalToRoot(x, y);
                Window* theTargetWindow = theTarget->GetRootWindow();
                if (theTargetWindow != NULL) {
                    theTargetWindow->QueueEvent(new MouseEvent(InputEvent_MouseMove, x, y, 0, 0));
                    theTargetWindow->QueueEvent(new MouseEvent(InputEvent_MouseDown, x, y, 0, 0));
                    theTargetWindow->QueueEvent(new MouseEvent(InputEvent_MouseUp, x, y, 0, 0));
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
                theWindow->QueueEvent(new MouseEvent(InputEvent_MouseMove, (int)a, (int)b, 0, 0));
                theWindow->QueueEvent(new MouseEvent(InputEvent_MouseDown, (int)a, (int)b, 0, 0));
                theWindow->QueueEvent(new MouseEvent(InputEvent_MouseUp, (int)a, (int)b, 0, 0));
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
             (theManager != NULL && theManager->mModalComponent != NULL) ? 1 : 0, theGame != NULL ? 1 : 0,
             (theGame != NULL && theGame->mIsHost) ? 1 : 0,
             (theGame != NULL && theGame->mClients.get() != NULL) ? (int)theGame->mClients->mClients.size() : 0,
             (theClient != NULL && theClient->IsReady()) ? 1 : 0, (theGame != NULL && theGame->AreAllReady()) ? 1 : 0,
             theGame != NULL ? GUIText(theGame->mName).c_str() : "-");
    theText += theLine;
    std::vector<ListArea*> theLists;
    sLobbyModal = theManager != NULL ? theManager->mModalComponent : NULL;
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
    if (!EEIsLobbyOpen())
        return;
    WONAPI::WindowManager* theManager = WONAPI::GetLobbyWindow();
    if (theManager == NULL)
        return;
    if (!sLobbyWorkPosted.exchange(1))
        theManager->PostEvent(new WONAPI::UserEvent(LobbyWork, 0));
}

}   // namespace (the lobby hooks)

// ---- the box selection (TEST ONLY). "<seq> boxselect <form> <mode> <x0> <y0> <x1> <y1>" drags a box in the game view
// through the real input path, on a thread of its own (not under the UI lock): the pointer to (x0, y0) (window
// coordinates, as "pointer"), a left button press as the platform delivers it (ISHandleEvent), the pointer in 12
// steps to (x1, y1), the release. The game form starts the drag on the press (EEModeNormal -> EEUICSelectUnits::
// StartSelection), the release builds the box selection (EEALasso::SelectUnits: EEMSelectUnits from the picker, with
// Shift EEMAddUnitsToSelectedGroup) and the physics executes it (EEPlayer::SelectUnits: the Rust pick, ee_units
// select.rs; AddUnitsToSelectedGroup). Modes:
//   0 select   a plain box: the local player's selected group becomes the box's units;
//   1 add      Shift held (DIK_LSHIFT, a key press as the platform delivers it) from before the press to after the
//              release: the box's units are added to the group;
//   2 dead     as 0, but just before the release one unit of the current selection (a living one the player may
//              delete, EEComplexUnit::CanDelete: the candidate of the last poll) is selected alone and deleted,
//              through the game's own messages (EEMSelectTheseUnits as the unit icon bar sends it, EEMKillUnit(0) as
//              the Delete key's event does, both as UI actions queued before the box's): the box's ids are picked
//              while it lives, and the physics kills it before it executes the box selection, which then holds a
//              dead unit's id.
// The verdicts: "box-started" (or "box-busy", "box-no-candidate"), then "box-released" when the drag is done. Each
// box selection message's decoded ids reach the fixture through gEESelectUnitsTestHook (EEMSelectUnits' ctor, armed
// only for the release). Every poll then writes, read under the world's read lock:
//   "selection <n> <ids>" (the local player's active group, ids sorted, comma separated, "-" for none),
//   "view <l> <t> <r> <b>" (the game view's rectangle, window coordinates), and for the last box command
//   "boxselect seq <s> mode <m> phase <p> lasso <n> <ids> before <n> <ids> expected <n> <ids> match <0|1>
//    dead <id|-1> dead_in_lasso <0|1> dead_state <none|gone|invalid|dying|alive>"
// (phase 1 dragging, 2 released, 3 the message seen). expected is the original's rule, applied here to the objects as
// the C++ reads them (not through the unit store): of the box's ids, the local player's own units (its list) that are
// their own complex unit, built, not a path point and valid, in mode 1 with the group before the box (those still
// valid in the list); match: the group holds exactly those (as sets). The driver waits for match after phase 3.
namespace {

struct BoxState {
    unsigned long mSeq = 0;
    int mMode = 0;
    int mPhase = 0;
    bool mArmed = false;                     // the next EEMSelectUnits is this command's
    long mDead = -1;
    std::vector<long> mLasso;
    std::vector<long> mBefore;
};
std::mutex sBoxMutex;
BoxState sBox;
std::vector<long> sBoxGroup;                 // the last poll's selection
long sBoxCandidate = -1;                     // the last poll's dead candidate
std::atomic<int> sBoxDragging(0);

// EEMSelectUnits' ctor (the UI thread), each box selection's decoded ids
void BoxLassoHook(const std::vector<long>& inUnitIDs)
{
    std::lock_guard<std::mutex> theLock(sBoxMutex);
    if (!sBox.mArmed)
        return;
    sBox.mArmed = false;
    sBox.mLasso = inUnitIDs;
    sBox.mPhase = 3;
}

// A UI action that sends one game message as local input (as the UI's own buttons do, on the UI thread)
class BoxSendAction : public EEAction {
public:
    explicit BoxSendAction(NEMessage* inMessage) : mMessage(inMessage) {}
    virtual ~BoxSendAction()
    {
        if (this->mMessage)
            this->mMessage->Dereference();
    }
    virtual void Virtual4()
    {
        if (this->mMessage) {
            gCommunications->ProcessLocalUserInput(this->mMessage, kSlotNone);
            this->mMessage = NULL;
        }
    }
    virtual void Virtual8() {}
    NEMessage* mMessage;
};

void BoxPost(NEMessage* inMessage)
{
    BoxSendAction* theAction = new BoxSendAction(inMessage);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
}

void BoxPointer(float inX, float inY)
{
    gMouse->mInputLock->Enter();
    gMouse->SetPosition(GE2DPoint(inX, inY));
    gMouse->mInputLock->Leave();
}

void BoxEvent(uint32_t inType, uint32_t inCode)
{
    PlatInputEvent theEvent;
    memset(&theEvent, 0, sizeof(theEvent));
    theEvent.mType = inType;
    if (inType == PLAT_EVENT_KEY_DOWN || inType == PLAT_EVENT_KEY_UP)
        theEvent.mKey.mScanCode = inCode;
    else
        theEvent.mMouse.mButton = inCode;
    ISHandleEvent(theEvent);
}

void BoxDrag(unsigned long inSeq, int inMode, float inX0, float inY0, float inX1, float inY1, long inDead)
{
    const uint32_t kLeftShift = 0x2a;        // DIK_LSHIFT
    if (inMode == 1) {
        BoxEvent(PLAT_EVENT_KEY_DOWN, kLeftShift);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    BoxPointer(inX0, inY0);
    gUIManager->ForceUIUpdate();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    BoxEvent(PLAT_EVENT_MOUSE_DOWN, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    for (int k = 1; k <= 12; k++) {
        BoxPointer(inX0 + (inX1 - inX0) * k / 12.0f, inY0 + (inY1 - inY0) * k / 12.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (inMode == 2 && inDead >= 0) {        // queued before the release: sent, and executed, before the box
        std::vector<long> theOne(1, inDead);
        BoxPost(new EEMSelectTheseUnits(theOne));
        BoxPost(new EEMKillUnit((unsigned char)0));   // the first selected unit
    }
    {
        std::lock_guard<std::mutex> theLock(sBoxMutex);
        sBox.mArmed = true;
        sBox.mPhase = 2;
    }
    BoxEvent(PLAT_EVENT_MOUSE_UP, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    if (inMode == 1)
        BoxEvent(PLAT_EVENT_KEY_UP, kLeftShift);
    sBoxDragging = 0;
    fprintf(stderr, "TEST UI box-released boxselect control %d value %ld on form 0 (seq %lu, top form 0)\n", inMode, inDead,
            inSeq);
    fflush(stderr);
}

void BoxIDs(std::string& outText, std::vector<long> inIDs)
{
    std::sort(inIDs.begin(), inIDs.end());
    char theNumber[24];
    snprintf(theNumber, sizeof(theNumber), "%lu ", (unsigned long)inIDs.size());
    outText += theNumber;
    if (inIDs.empty())
        outText += "-";
    for (size_t i = 0; i < inIDs.size(); i++) {
        snprintf(theNumber, sizeof(theNumber), "%s%ld", i ? "," : "", inIDs[i]);
        outText += theNumber;
    }
}

// inID's unit of inPlayer's list when it is its own complex unit, else NULL
EEComplexUnit* BoxOwnComplex(EEPlayer* inPlayer, long inID)
{
    RTSUnit* theUnit = inPlayer->GetUnit(inID);
    if (theUnit == NULL)
        return NULL;
    EEUnit* theEEUnit = static_cast<EEUnit*>(theUnit);
    EEComplexUnit* theComplex = theEEUnit->mComplexUnit.Get();
    return static_cast<EEUnit*>(theComplex) == theEEUnit ? theComplex : NULL;
}

// The selection lines (under the world's read lock, never under the UI lock: the UI thread takes the two the other
// way round) -> the text for ui-state.txt
std::string BoxSnapshot()
{
    std::string theText;
    if (gPhysics == NULL || gWorld.mMap == NULL || gWorld.mLock == NULL)
        return theText;
    gWorld.mLock->ReadLock();
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (thePlayer == NULL || thePlayer->mActiveGroup == NULL) {
        gWorld.mLock->ReadUnlock();
        return theText;
    }
    std::vector<long> theGroup;
    long theCandidate = -1, theFallback = -1;   // a living unit it may delete; else any living one (refused: "alive")
    EESelectedGroup* theSelected = thePlayer->mActiveGroup;
    for (size_t i = 0; i < theSelected->mUnits.size(); i++) {
        EEComplexUnit* theUnit = theSelected->mUnits[i];
        if (theUnit == NULL)
            continue;
        theGroup.push_back((long)theUnit->mUnitID);
        bool theLiving = theUnit->mIsValid && theUnit->mHitPoints > 0;
        if (theCandidate < 0 && theLiving && theUnit->CanDelete())
            theCandidate = (long)theUnit->mUnitID;
        if (theFallback < 0 && theLiving)
            theFallback = (long)theUnit->mUnitID;
    }
    if (theCandidate < 0)
        theCandidate = theFallback;
    BoxState theBox;
    {
        std::lock_guard<std::mutex> theLock(sBoxMutex);
        theBox = sBox;
        sBoxGroup = theGroup;
        sBoxCandidate = theCandidate;
    }
    theText += "selection ";
    BoxIDs(theText, theGroup);
    theText += "\n";
    if (theBox.mSeq != 0) {
        std::vector<long> theExpected;
        if (theBox.mMode == 1)
            for (size_t i = 0; i < theBox.mBefore.size(); i++) {
                EEComplexUnit* theUnit = BoxOwnComplex(thePlayer, theBox.mBefore[i]);
                if (theUnit != NULL && theUnit->mIsValid)
                    theExpected.push_back(theBox.mBefore[i]);
            }
        if (theBox.mPhase == 3)
            for (size_t i = 0; i < theBox.mLasso.size(); i++) {
                long theID = theBox.mLasso[i];
                EEComplexUnit* theUnit = BoxOwnComplex(thePlayer, theID);
                if (theUnit != NULL && theUnit->mIsBuilt && theUnit->mUnitClass->mUnitType != kUnitTypePathPoint
                    && theUnit->mIsValid && std::find(theExpected.begin(), theExpected.end(), theID) == theExpected.end())
                    theExpected.push_back(theID);
            }
        std::vector<long> theSortedGroup(theGroup), theSortedExpected(theExpected);
        std::sort(theSortedGroup.begin(), theSortedGroup.end());
        std::sort(theSortedExpected.begin(), theSortedExpected.end());
        bool theMatch = theBox.mPhase == 3 && theSortedGroup == theSortedExpected;
        const char* theDeadState = "none";
        if (theBox.mDead >= 0) {
            RTSUnit* theDead = thePlayer->GetUnit(theBox.mDead);
            if (theDead == NULL)
                theDeadState = "gone";
            else if (!theDead->mIsValid)
                theDeadState = "invalid";
            else if (static_cast<EEUnit*>(theDead)->mHitPoints <= 0)
                theDeadState = "dying";
            else
                theDeadState = "alive";
        }
        bool theDeadInLasso = theBox.mDead >= 0
            && std::find(theBox.mLasso.begin(), theBox.mLasso.end(), theBox.mDead) != theBox.mLasso.end();
        char theLine[160];
        snprintf(theLine, sizeof(theLine), "boxselect seq %lu mode %d phase %d lasso ", theBox.mSeq, theBox.mMode,
                 theBox.mPhase);
        theText += theLine;
        BoxIDs(theText, theBox.mLasso);
        theText += " before ";
        BoxIDs(theText, theBox.mBefore);
        theText += " expected ";
        BoxIDs(theText, theExpected);
        snprintf(theLine, sizeof(theLine), " match %d dead %ld dead_in_lasso %d dead_state %s\n", theMatch ? 1 : 0,
                 theBox.mDead, theDeadInLasso ? 1 : 0, theDeadState);
        theText += theLine;
    }
    gWorld.mLock->ReadUnlock();
    GERectangle theView = gViewport.GetViewRectangle();
    char theLine[120];
    snprintf(theLine, sizeof(theLine), "view %ld %ld %ld %ld\n", (long)theView.GetLeft(), (long)theView.GetTop(),
             (long)theView.GetRight(), (long)theView.GetBottom());
    theText += theLine;
    return theText;
}

// The boxselect command (under the UI lock, from Execute) -> its verdict
const char* BoxStart(unsigned long inSeq, const char* inLine)
{
    unsigned long theSeq = 0, theForm = 0;
    long theMode = -1, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    char theOp[32] = {0};
    if (sscanf(inLine, "%lu %31s %lu %ld %ld %ld %ld %ld", &theSeq, theOp, &theForm, &theMode, &x0, &y0, &x1, &y1) != 8
        || theMode < 0 || theMode > 2 || gMouse == NULL)
        return "rejected";
    if (sBoxDragging.exchange(1))
        return "box-busy";
    long theDead = -1;
    {
        std::lock_guard<std::mutex> theLock(sBoxMutex);
        if (theMode == 2 && sBoxCandidate < 0) {
            sBoxDragging = 0;
            return "box-no-candidate";
        }
        theDead = theMode == 2 ? sBoxCandidate : -1;
        sBox = BoxState();
        sBox.mSeq = inSeq;
        sBox.mMode = (int)theMode;
        sBox.mPhase = 1;
        sBox.mDead = theDead;
        sBox.mBefore = sBoxGroup;
    }
    std::thread(BoxDrag, inSeq, (int)theMode, (float)x0, (float)y0, (float)x1, (float)y1, theDead).detach();
    return "box-started";
}

}   // namespace (the box selection)

void Execute(const std::string& inDirectory, UIForm* inForm, unsigned long inFormID)
{
    FILE* theCommand = fopen((inDirectory + "/test.command").c_str(), "r");
    if (!theCommand)
        return;
    char theLine[200] = {0};
    bool theRead = fgets(theLine, sizeof(theLine), theCommand) != NULL;
    fclose(theCommand);
    if (!theRead)
        return;
    unsigned long theSeq = 0, a = 0, b = 0, c = 0;
    char theOp[32] = {0};
    int theFields = sscanf(theLine, "%lu %31s %lu %lu %lu", &theSeq, theOp, &a, &b, &c);
    if (theFields < 4 || theSeq <= sLastCommand)
        return;
    sLastCommand = theSeq;
    const char* theVerdict = "rejected";
    if (!strncmp(theOp, "won", 3)) {
        // a lobby command: "<seq> won<op> <a> <b>", run on the lobby's message loop (the verdict comes from there)
        if (EEIsLobbyOpen()) {
            {
                std::lock_guard<std::mutex> theLock(sLobbyMutex);
                sLobbyCommand = std::string(theOp) + " " + std::to_string(a) + " " + std::to_string(b);
                sLobbyCommandSeq = theSeq;
            }
            LobbyPostWork();
            theVerdict = "lobby-posted";
        }
        fprintf(stderr, "TEST UI %s %s control %lu value %lu on form %lu (seq %lu, top form %lu)\n", theVerdict, theOp, a, b,
                0UL, theSeq, inFormID);
        return;
    }
    UIControl* theControl = (a == inFormID && b < inForm->GetControlCount()) ? inForm->GetControl(b) : NULL;
    if (theControl && !strcmp(theOp, "click") && theControl->GetTriggerEvent(kTriggerEvent)) {
        if (UIRadioButton* theRadio = dynamic_cast<UIRadioButton*>(theControl))
            theRadio->Check();
        theControl->FireTriggerEvent(kTriggerEvent);
        theVerdict = "triggered";
    } else if (theControl && !strcmp(theOp, "select") && theFields >= 5) {
        UIComboBox* theCombo = dynamic_cast<UIComboBox*>(theControl);
        if (theCombo && theCombo->GetListBox() && c < theCombo->GetListBox()->GetLineCount()) {
            theCombo->SetCurrentSelection(c);
            theVerdict = "selected";
        }
    } else if (theControl && !strcmp(theOp, "check") && theFields >= 5) {
        if (UICheckBox* theCheck = dynamic_cast<UICheckBox*>(theControl)) {
            theCheck->SetChecked(c != 0);
            theVerdict = "checked";
        }
    } else if (!strcmp(theOp, "zoom") && theFields >= 5 && a == inFormID) {
        // "<seq> zoom <form> 0 <percent>": the camera zoom (100 = the original's farthest)
        gWorld.mCameraControl.SetZoom(c / 100.0f);
        theVerdict = "zoomed";
    } else if (!strcmp(theOp, "pointer") && theFields >= 5 && a == inFormID && gMouse) {
        // "<seq> pointer <form> <x> <y>": the pointer where a player's mouse would put it (UIManager::UpdateUI
        // hit-tests the form's controls at it on the next UI update)
        gMouse->SetPosition(GE2DPoint((float)b, (float)c));
        gUIManager->ForceUIUpdate();
        theVerdict = "pointer-moved";
    } else if (!strcmp(theOp, "lclick") && theFields >= 5 && a == inFormID && gMouse) {
        gMouse->SetPosition(GE2DPoint((float)b, (float)c));
        gUIManager->ForceUIUpdate();
        std::thread([]() {   // not under the UI lock: the UI thread reads the button between the two
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            PlatInputEvent theEvent;
            memset(&theEvent, 0, sizeof(theEvent));
            theEvent.mType = PLAT_EVENT_MOUSE_DOWN;
            theEvent.mMouse.mButton = 1;
            ISHandleEvent(theEvent);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            theEvent.mType = PLAT_EVENT_MOUSE_UP;
            ISHandleEvent(theEvent);
        }).detach();
        theVerdict = "clicked";
    } else if (!strcmp(theOp, "boxselect") && a == inFormID) {
        theVerdict = BoxStart(theSeq, theLine);
    } else if (!strcmp(theOp, "order") && a == inFormID) {
        theVerdict = EETestOrderStart(theSeq, b);
    } else if (theControl && !strcmp(theOp, "lselect") && theFields >= 5) {
        UILBString* theList = dynamic_cast<UILBString*>(theControl);
        if (theList && c < theList->GetLineCount()) {
            theList->ChangeSelectionIndex(c, true, false);
            theVerdict = "list-selected";
        }
    }
    fprintf(stderr, "TEST UI %s %s control %lu value %lu on form %lu (seq %lu, top form %lu)\n", theVerdict, theOp, b, c,
            a, theSeq, inFormID);
}

void Poll(const std::string& inDirectory)
{
    UIManager* theManager = gUIManager;
    if (!theManager)
        return;
    std::string theSelection = BoxSnapshot();   // before the UI lock (the world's read lock inside)
    theSelection += EETestOrderSnapshot();
    theManager->LockUIForCopyData();
    UIForm* theForm = theManager->GetCurrentForm();
    std::string theTemporary = inDirectory + "/ui-state.tmp";
    FILE* theState = fopen(theTemporary.c_str(), "w");
    if (theState) {
        unsigned long theFormID = theForm ? theForm->GetFormID() : 0;
        unsigned long theCount = theForm ? theForm->GetControlCount() : 0;
        fprintf(theState, "form %lu controls %lu\n", theFormID, theCount);
        fprintf(theState, "command %lu\n", sLastCommand);
#ifdef _WIN32
        fprintf(theState, "pid %lu\n", (unsigned long)_getpid());
#else
        fprintf(theState, "pid %lu\n", (unsigned long)getpid());
#endif
        fprintf(theState, "stack");
        for (unsigned long i = 0; i < theManager->GetFormCount() && i < 64; i++) {
            UIForm* theStacked = theManager->GetStackedForm(i);
            if (theStacked)
                fprintf(theState, " %lu", theStacked->GetFormID());
        }
        fprintf(theState, "\n");
        UIControl* theOver = theForm ? theForm->GetMouseOverControl() : NULL;
        fprintf(theState, "mouseover %ld\n", theOver ? (long)theOver->GetIndex() : -1L);
        GEWindow* theWindow = theManager->GetRasterizer()->GetWindow(theManager->GetScreen());
        if (theWindow)   // the window's size in the game's (logical) pixels: the pointer's coordinates
            fprintf(theState, "window %lu %lu\n", (unsigned long)theWindow->GetWidth(), (unsigned long)theWindow->GetHeight());
        for (unsigned long i = 0; i < theCount && i < 2000; i++) {
            UIControl* theControl = theForm->GetControl(i);
            if (!theControl)
                continue;
            if (theControl->GetTriggerEvent(kTriggerEvent))
                fprintf(theState, "control %lu event-rva 0\n", i);   // the bridge format (no RVA here)
            if (UIComboBox* theCombo = dynamic_cast<UIComboBox*>(theControl)) {
                unsigned long theSelection = 0xffffffffUL;
                theCombo->GetCurrentSelection(theSelection);
                const UILBString* theList = theCombo->GetListBox();
                fprintf(theState, "combo %lu sel %ld lines %lu", i, (long)(int32_t)theSelection,
                        theList ? theList->GetLineCount() : 0UL);
                PrintLines(theState, theList);
            } else if (UICheckBox* theCheck = dynamic_cast<UICheckBox*>(theControl)) {
                fprintf(theState, "check %lu %d\n", i, theCheck->GetIsChecked() ? 1 : 0);
            } else if (UILBString* theList = dynamic_cast<UILBString*>(theControl)) {
                unsigned long theSelection = 0xffffffffUL;
                theList->GetCurrentSelection(theSelection);
                fprintf(theState, "list %lu sel %ld lines %lu", i, (long)(int32_t)theSelection, theList->GetLineCount());
                PrintLines(theState, theList);
            }
        }
        WriteTelemetry(theState);
        fputs(theSelection.c_str(), theState);
        if (EEIsLobbyOpen()) {   // the WON lobby: the last snapshot its own thread took, and a new one asked for
            std::string theSnapshot;
            {
                std::lock_guard<std::mutex> theLock(sLobbyMutex);
                theSnapshot = sLobbySnapshot;
            }
            fprintf(theState, "lobbyopen 1\n%s", theSnapshot.c_str());
            LobbyPostWork();
        }
        fclose(theState);
        std::string theRequest = inDirectory + "/frame.request";
        if (remove(theRequest.c_str()) == 0)   // the driver asks for the next presented frame
            sFrameWanted = 1;
        ReplaceWith(theTemporary, inDirectory + "/ui-state.txt");
        if (theForm)
            Execute(inDirectory, theForm, theFormID);
    }
    theManager->UnlockUIForCopyData();
}

}   // namespace

namespace {
// The game's debug output (PlatDebugOutput: GX's log, EE-AI lines, ...) goes to stderr as "debugstring <text>", the
// lines the debugger harness read (no debugger runs under the fixture driver)
void DebugToStderr(const char* inText)
{
    size_t theLength = inText ? strlen(inText) : 0;
    fprintf(stderr, "debugstring %s%s", inText ? inText : "", theLength && inText[theLength - 1] == '\n' ? "" : "\n");
    fflush(stderr);
}
}   // namespace

// The test switches, when EE_TEST_UI names a directory (EERun, first, before the engine starts its clocks and threads):
//   EE_TEST_TIME_FACTOR=F (or N/D, or a decimal): the harness clock (platform/PlatSim.h PlatSimSetTimeFactor);
//   EE_TEST_SEED=S: the start seed (gPlatSimSeed: EEMStartLoading::Execute takes it instead of the lobby's);
//   EE_TEST_NO_HUMAN_DEFEAT=1: the idle local human is never taken out or defeated (gPlatSimNoHumanDefeat);
// and the debug output goes to stderr ("debugstring <text>": PlatDebugSetHandler).
// Each one taken is announced on stderr ("TEST UI ..."); a malformed one stops the game (a run must not go on
// believing it is accelerated or seeded when it is not).
void EETestFixtureConfigure()
{
    const char* theDirectory = getenv("EE_TEST_UI");
    if (!theDirectory || !*theDirectory)
        return;
    PlatDebugSetHandler(DebugToStderr);
    if (const char* theFactor = getenv("EE_TEST_TIME_FACTOR")) {
        unsigned long theNum = 0, theDen = 1, theWhole = 0;
        char theRest[8] = {0};
        int theFields = sscanf(theFactor, "%lu/%lu", &theNum, &theDen);
        if (theFields == 1 && strchr(theFactor, '.')) {   // a decimal: up to three places
            theDen = 1;
            theNum = 0;
            if (sscanf(theFactor, "%lu.%7[0-9]", &theWhole, theRest) >= 1) {
                size_t thePlaces = strlen(theRest);
                theNum = theWhole;
                for (size_t k = 0; k < thePlaces && k < 3; k++) {
                    theNum = theNum * 10 + (unsigned long)(theRest[k] - '0');
                    theDen *= 10;
                }
            }
        }
        if (theFields < 1 || !PlatSimSetTimeFactor((uint32_t)theNum, (uint32_t)theDen)) {
            fprintf(stderr, "TEST UI EE_TEST_TIME_FACTOR=%s is not a factor in 1/1000..4096\n", theFactor);
            exit(3);
        }
        fprintf(stderr, "TEST UI clock factor %lu/%lu\n", theNum, theDen);
    }
    if (const char* theSeed = getenv("EE_TEST_SEED")) {
        char* theEnd = NULL;
        unsigned long long theValue = strtoull(theSeed, &theEnd, 0);
        if (!*theSeed || *theEnd || theValue > 0xffffffffULL) {
            fprintf(stderr, "TEST UI EE_TEST_SEED=%s is not a 32-bit number\n", theSeed);
            exit(3);
        }
        gPlatSimSeed = (uint32_t)theValue;
        gPlatSimSeedSet = 1;
        fprintf(stderr, "TEST UI seed %lu\n", (unsigned long)gPlatSimSeed);
    }
    if (const char* theFlag = getenv("EE_TEST_NO_HUMAN_DEFEAT")) {
        gPlatSimNoHumanDefeat = atoi(theFlag) != 0;
        fprintf(stderr, "TEST UI no human defeat %d\n", gPlatSimNoHumanDefeat);
    }
}

// Starts the fixture's thread when EE_TEST_UI names a directory (EERun, after the engine is initialised)
void EETestFixtureStart()
{
    const char* theDirectory = getenv("EE_TEST_UI");
    if (!theDirectory || !*theDirectory)
        return;
    std::string theCopy(theDirectory);
    sDirectory = theCopy;
    GXTestSetPresentHook(PresentHook);   // frame.request -> frame.bmp
    gEESelectUnitsTestHook = BoxLassoHook;   // each box selection's ids (the boxselect command)
    EETestOrdersInstall();                   // each executed input's view (the order command)
    fprintf(stderr, "TEST UI fixture on %s\n", theCopy.c_str());
    std::thread([theCopy]() {
        for (;;) {
            Poll(theCopy);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }).detach();
}
