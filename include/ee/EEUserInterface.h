// C:\EEX\Empire Earth\User Interface Thread\EEUserInterface.h (RECONSTRUCTION; file name inferred). The UI thread's
// front end (RTTI EEUserInterface, vtable 0x44ba40; ctor 0x276631, dtor 0x276740, sizeof 0x60; `gUserInterface`).
// Its members are the Rust record EeScreensUserInterfaceFields (ee_screens ui_objects.rs; the UI cluster lane,
// round 2), the class's only base, under their names; the constructor, the destructor and the posts are Rust's
// (each post the original's `mThread->Post(id, p1, p2, true)`), the thread's loop and message switch too
// (ui_game.rs, ui_thread.rs). EEUserInterface.cpp keeps the handlers that call the game's C++ and the hosts. The
// posts the rest of the game makes (the chat log's lines, the UI events, the loaded objects, the dialog messages) and
// the graphics' resume are inline: out of line they put a dozen of the game's units in one dependency cycle with the
// UI thread (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts, round 3"). Requires lle/LowLevelEngine.h (and
// ee/RTSGraphics.h's includes).
#ifndef EE_EEUSERINTERFACE_H
#define EE_EEUSERINTERFACE_H

#include "rust/game/screens.h"   // EeScreensUserInterfaceFields, the posts, the actions
#include "rust/data/tables.h"    // ee_data_world_entry (AddChatLogEntry's display time)
#include "ee/EEWorldConstants.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EESMDialogMessage.h"
#include "ee/EEGraphics.h"
#include "platform/PlatWindow.h"
#include "platform/PlatTime.h"
#include "lle/UIEvent.h"

class EEAction;
class EESMDialogMessage;
struct EEUIFErrorCallbackData;               // ee/EEUIManager.h
class EECivilization;
class FSFileSpec;
class TSThread;
struct PlatInputEvent;
class GEColor;
class UIEvent;
class UIForm;
class UReferenceCounter;
class UWideString;

class EEUserInterface : public EeScreensUserInterfaceFields {
public:
    EEUserInterface() { ee_screens_ui_construct(this); }
    ~EEUserInterface() { ee_screens_ui_destroy(this); }
    TSThread* GetThread() const { return (TSThread*)this->mThread; }   // (a TSThread* is its Rust object)
    long Post(unsigned long inMessage, const void* inParameter1 = NULL, const void* inParameter2 = NULL)
    {
        return ee_screens_ui_post(this, (uint32_t)inMessage, const_cast<void*>(inParameter1), const_cast<void*>(inParameter2));
    }

    // the action queue (ee_screens actions.rs)
    void PostAction(EEAction* inAction) { ee_screens_actions_post(inAction); }
    // the UI thread's loop (ui_game.rs) and its message switch (0x401..0x443, ui_thread.rs)
    long Run(void* inParameter) { return (long)ee_screens_ui_run(this); }

    // the posts (thread message ids; provisional names by message number)
    long OpenCivilizationBuilder(EECivilization* inCivilization, bool inFlag) { return this->Post(0x420, inCivilization, (void*)inFlag); }
    long CloseForm(UIForm* inForm) { return this->Post(0x419, inForm); }
    long OpenForm(UIForm* inForm) { return this->Post(0x40d, inForm); }
    long SwitchToForm(UIForm* inForm) { return this->Post(0x414, inForm); }
    void LeaveScenarioEditor() { this->Post(0x410); }
    void StartScenarioTest() { this->Post(0x40f); }
    void StartScenarioTest2() { this->Post(0x415); }
    void SetLoadPercent(long inPlayer, unsigned long inPercent) { this->Post(0x443, (void*)(intptr_t)inPlayer, (void*)(uintptr_t)inPercent); }
    void SetLagStatus(unsigned short inLag, unsigned char inStatus) { this->Post(0x442, (void*)(uintptr_t)inLag, (void*)(inStatus == 1)); }
    void UpdateJITManufacturing() { this->Post(0x416); }
    long PostEnterScenarioEditor(bool inFlag) { return this->Post(0x411, (void*)inFlag); }
    void PostPauseInformation(bool inFlag) { this->Post(0x427, (void*)inFlag); }
    void PostStartLoading() { ee_screens_ui_post_start_loading(this); }   // 0x407, 0x413 from the editor
    void PostUpdateBackgroundMedia() { this->Post(0x403); }
    void PostShowEpochName() { this->Post(0x404); }
    void PostSelectEditorTab(long inX, long inY) { this->Post(0x421, (void*)(intptr_t)inX, (void*)(intptr_t)inY); }
    void PostCancelGame() { this->Post(0x412); }
    void PostLoadGameFailed(bool inFlag) { this->Post(0x430, NULL, (void*)inFlag); }
    void PostHostMigrated() { this->Post(0x426); }
    void PostHideMessages() { this->Post(0x432); }
    void PostErrorClosed(UIForm* inForm, EEUIFErrorCallbackData* inData) { this->Post(0x417, inForm, inData); }
    void PostLobbyUpdatePlayer(long inX, long inY) { this->Post(0x436, (void*)(intptr_t)inX, (void*)(intptr_t)inY); }
    void PostLobbyUpdateGameOptions() { this->Post(0x437); }
    void PostLobbyShowTransferProgress(long inX, long inY) { this->Post(0x439, (void*)(intptr_t)inX, (void*)(intptr_t)inY); }
    void PostLobbyUpdateForm() { this->Post(0x438); }
    void PostRefreshSavedGroup(long inX) { this->Post(0x435, (void*)(intptr_t)inX); }
    void PostLoginDone() { this->Post(0x441); }   // DEAD-IN-AOC (base exe 0x661aef; only EEWonNet's MOTD callback)
    // the load percentage (0x40a; in multiplayer also EEMLoadPercent at every 20 percent step)
    void SetLoadProgress(float inFraction) { ee_screens_ui_set_load_progress(this, inFraction); }
    void SetLoadingText(long inX) { this->Post(0x40b, (void*)(intptr_t)inX); }
    // 0x424 once until it is handled (nothing on a NULL object)
    void PostMessage424Once() { ee_screens_ui_post_icon_bar_once(this); }

    // the posts that build C++ objects
    // 0x40e and 0x40c post a reference (ee_screens ui_objects_ffi.rs)
    long PostEvent(UIEvent* inEvent, UIForm* inForm) { return (long)ee_screens_ui_post_event(this, inEvent, inForm); }
    void ShowLoadedObject(UReferenceCounter* inObject) { ee_screens_ui_show_loaded(this, inObject); }
    // 0x41b: a new EEScreenCapture of a width (no surface or completion event yet)
    void PostScreenShot(long inX, bool inFlag1, bool inFlag2)
    {
        this->Post(0x41b, new EEScreenCapture{inX, NULL, NULL, inFlag2}, (void*)inFlag1);
    }
    // 0x422 (a copy)
    void PostDialogMessage(const EESMDialogMessage& inMessage) { this->Post(0x422, new EESMDialogMessage(inMessage), NULL); }
    // Posts loading a saved game file (message 0x408)
    void PostLoadSavedGame(const FSFileSpec& inSpec) { this->Post(0x408, new FSFileSpec(inSpec), NULL); }   // 0x408 (a copy; EEUserInterface.cpp)
    // 0x418: a new EEChatLogEntry with a name, colour, text, display time and type
    long AddChatLogEntry(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                         unsigned long inTime, unsigned long inType)
    {
        return this->Post(0x418, new EEChatLogEntry{inColor, inName, inText, inType, inTime}, NULL);
    }
    // a white system line to the chat log, shown three times the usual time (gWorldTable's
    // kWorldChatLogDisplayTime: ee/EEDatabaseTables.h's GetLongValue)
    long AddChatLogEntry(const UWideString& inText, long inY)
    {
        return this->AddChatLogEntry(GEColor(1.0f, 1.0f, 1.0f), " > ", inText,
                                     ee_data_world_entry((uint32_t)kWorldChatLogDisplayTime).mLongValue * 3, 2);
    }

    // the handlers that call the game's C++ (EEUserInterface.cpp)
    // Handles the game window's events (platform/PlatWindow.h; the original's window procedure, case by case):
    // close, minimise and restore (muting while minimised), focus (the game minimises itself and suspends input
    // while inactive), the Control/AltGr state, typed text and the input method's composition (shown by the edit
    // box; the system draws its candidate list). The decisions and the original's statics are Rust's (ee_screens
    // ui_thread.rs), over the host ee_screenscpp_ui_event. The window's cursor is hidden at creation
    // (WM_SETCURSOR's SetCursor(NULL)), and the screen saver / Alt-Tab system commands need no handling.
    static void HandleEvent(const PlatInputEvent& inEvent)
    {
        ee_screens_ui_event(inEvent.mType, inEvent.mKey.mVirtualKey, inEvent.mText.mText, inEvent.mText.mCursor,
                            inEvent.mText.mLength);
    }
    // (the members below are Rust's: ee_game screens ui_calls.rs)
    // Switches to another rasterizer: hides the cursor, releases the screens, deactivates the old rasterizer and
    // resizes the game screen to 1024x768
    void ChangeRasterizer(long inRasterizer, long inScreen) { ee_screens_ui_change_rasterizer(inRasterizer, inScreen); }  // 0x276804 (message 0x41e)
    // Resizes the game screen (unless the size is unchanged and not forced), relays out the forms and redraws the
    // mini map
    void ResizeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inForce, bool inFlag)
    {
        ee_screens_ui_resize_game_screen((uint32_t)inWidth, (uint32_t)inHeight, inForce, inFlag);
    }
    // Shows the WON game and system messages of the day on the multiplayer form when it is visible
    void ShowMessageOfTheDay(const UWideString& inGame, const UWideString& inSystem)
    {
        ee_screens_ui_show_motd(&inGame, &inSystem);
    }  // 0x276a38 (message 0x440)
    // Opens the WON server selection, remembering the password when asked
    void OpenSelectServer() { ee_screens_ui_open_select_server(); }                // 0x27783c (message 0x441)
    // Pauses the graphics thread and waits until it is paused
    void PauseGraphics() { ee_screens_ui_pause_graphics(); }
    // Resumes the graphics thread and waits until it runs
    void ResumeGraphics() { ee_screens_ui_resume_graphics(); }
    // Updates the interface: keyboard and mouse input, the UI under the world read lock, then the queued actions
    void UpdateInterface() { ee_screens_ui_update_interface(); }                 // 0x278ed3 (input, UpdateUI, the actions)
};

// 0x27fa40 (no arguments; returns a status): what EEUIEProcessTimerExpired's event does (provisional name)
long EEProcessTimerExpired();

// (EEPumpWindowEvent and EENativeMessageHook, the window's event pump and native message hook: Empire Earth.cpp,
// which installs them)

#endif
