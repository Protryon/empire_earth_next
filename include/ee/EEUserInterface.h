// C:\EEX\Empire Earth\User Interface Thread\EEUserInterface.h (RECONSTRUCTION; file name inferred). The UI thread's
// front end (RTTI EEUserInterface, vtable 0x44ba40; ctor 0x276631, dtor 0x276740, sizeof 0x60; `gUserInterface`).
// Its members are the Rust record EeScreensUserInterfaceFields (ee_screens ui_objects.rs; the UI cluster lane,
// round 2), the class's only base, under their names; the constructor, the destructor and the posts are Rust's
// (each post the original's `mThread->Post(id, p1, p2, true)`), the thread's loop and message switch too
// (ui_game.rs, ui_thread.rs). EEUserInterface.cpp keeps the posts that build C++ objects, the handlers that call the
// game's C++ and the hosts.
#ifndef EE_EEUSERINTERFACE_H
#define EE_EEUSERINTERFACE_H

#include "rust/game/screens.h"   // EeScreensUserInterfaceFields, the posts, the actions

class EEAction;
class EESMDialogMessage;
struct EEUIFErrorCallbackData;               // ee/EEUIFError.h
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

    // the action queue and the undo and redo lists (ee_screens actions.rs: 7 when the list is empty)
    void PostAction(EEAction* inAction) { ee_screens_actions_post(inAction); }
    long Redo() { return ee_screens_actions_redo(); }
    long Undo() { return ee_screens_actions_undo(); }
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
    void PostRestartGame() { this->Post(0x428); }
    void PostQuit() { this->Post(0x401); }
    void PostStartLoading() { ee_screens_ui_post_start_loading(this); }   // 0x407, 0x413 from the editor
    void PostUpdateBackgroundMedia() { this->Post(0x403); }
    void PostShowEpochName() { this->Post(0x404); }
    void PostSelectEditorTab(long inX, long inY) { this->Post(0x421, (void*)(intptr_t)inX, (void*)(intptr_t)inY); }
    void PostDisplayChanged(bool inFlag) { this->Post(0x429, (void*)inFlag); }
    void PostCancelGame() { this->Post(0x412); }
    void PostLoadGameFailed(bool inFlag) { this->Post(0x430, NULL, (void*)inFlag); }
    void PostHostMigrated() { this->Post(0x426); }
    void PostHideMessages() { this->Post(0x432); }
    void PostErrorClosed(UIForm* inForm, EEUIFErrorCallbackData* inData) { this->Post(0x417, inForm, inData); }
    long PostDisplayCivBuilder() { return this->Post(0x41f); }
    void PostChangeRasterizer(long inX, long inY) { this->Post(0x41e, (void*)(intptr_t)inX, (void*)(intptr_t)inY); }
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

    // the posts that build C++ objects (EEUserInterface.cpp)
    long PostEvent(UIEvent* inEvent, UIForm* inForm);   // 0x40e (a reference)
    void ShowLoadedObject(UReferenceCounter* inObject);   // 0x40c (a reference)
    void PostScreenShot(long inX, bool inFlag1, bool inFlag2);   // 0x41b (a new EEScreenCapture)
    void PostDialogMessage(const EESMDialogMessage& inMessage);   // 0x422 (a copy)
    void PostLoadSavedGame(const FSFileSpec& inSpec);   // 0x408 (a copy)
    // 0x418: a new EEChatLogEntry; the second a white system line shown three times the usual time
    long AddChatLogEntry(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                         unsigned long inTime, unsigned long inType);
    long AddChatLogEntry(const UWideString& inText, long inY);

    // the handlers that call the game's C++ (EEUserInterface.cpp)
    static void HandleEvent(const PlatInputEvent& inEvent);   // the game window's events (ui_thread.rs)
    void ChangeRasterizer(long inRasterizer, long inScreen);  // 0x276804 (message 0x41e)
    void ResizeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inForce, bool inFlag);   // 0x2768fe
    void ShowMessageOfTheDay(const UWideString& inGame, const UWideString& inSystem);  // 0x276a38 (message 0x440)
    void OpenSelectServer();                // 0x27783c (message 0x441)
    void PauseGraphics();                   // 0x277e63
    void ResumeGraphics();
    void UpdateInterface();                 // 0x278ed3 (input, UpdateUI, the actions)
};

// 0x27fa40 (no arguments; returns a status): what EEUIEProcessTimerExpired's event does (provisional name)
long EEProcessTimerExpired();

bool EEPumpWindowEvent();   // TSThread::SetEventPump's pump: one window event per call (EEUserInterface.cpp)
#ifdef _WIN32
int EENativeMessageHook(void* inWindow, uint32_t inMessage, uint64_t inWParam, int64_t inLParam);
#endif

#endif
