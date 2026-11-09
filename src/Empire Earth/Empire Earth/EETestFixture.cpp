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
//     events deliver them (ISHandleEvent): a click through the real hit test),
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
#include <atomic>

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
        std::string theFinal = sDirectory + "/frame.bmp";
        remove(theFinal.c_str());   // Windows: rename does not replace
        if (rename(theTemporary.c_str(), theFinal.c_str()) == 0)
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
        fclose(theState);
        std::string theRequest = inDirectory + "/frame.request";
        if (remove(theRequest.c_str()) == 0)   // the driver asks for the next presented frame
            sFrameWanted = 1;
        rename(theTemporary.c_str(), (inDirectory + "/ui-state.txt").c_str());
        if (theForm)
            Execute(inDirectory, theForm, theFormID);
    }
    theManager->UnlockUIForCopyData();
}

}   // namespace

// The test switches, when EE_TEST_UI names a directory (EERun, first, before the engine starts its clocks and threads):
//   EE_TEST_TIME_FACTOR=F (or N/D, or a decimal): the harness clock (platform/PlatSim.h PlatSimSetTimeFactor);
//   EE_TEST_SEED=S: the start seed (gPlatSimSeed: EEMStartLoading::Execute takes it instead of the lobby's);
//   EE_TEST_NO_HUMAN_DEFEAT=1: the idle local human is never taken out or defeated (gPlatSimNoHumanDefeat).
// Each one taken is announced on stderr ("TEST UI ..."); a malformed one stops the game (a run must not go on
// believing it is accelerated or seeded when it is not).
void EETestFixtureConfigure()
{
    const char* theDirectory = getenv("EE_TEST_UI");
    if (!theDirectory || !*theDirectory)
        return;
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
    fprintf(stderr, "TEST UI fixture on %s\n", theCopy.c_str());
    std::thread([theCopy]() {
        for (;;) {
            Poll(theCopy);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }).detach();
}
