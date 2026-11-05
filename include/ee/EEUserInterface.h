// C:\EEX\Empire Earth\User Interface Thread\EEUserInterface.h (RECONSTRUCTION; file name inferred). The UI thread's
// front end (RTTI EEUserInterface, vtable 0x44ba40; ctor 0x276631, dtor 0x276740, sizeof 0x60; `gUserInterface`).
// EEUserInterface.cpp (0x276631..0x2798ce, assert file "C:\EEX\Empire Earth\User Interface
// Thread\EEUserInterface.cpp") defines every member below; most are one-line posts of a thread message to mThread.
// Layout gap-free: ctor stores, dtor, the queue helpers, and the asserts' member names
// (mActionLock/mRedoLock/mUndoLock/mMessageQueueDone). Other names are provisional (by behaviour or by message number).
#ifndef EE_EEUSERINTERFACE_H
#define EE_EEUSERINTERFACE_H

#include <deque>

class EEAction;
class EESMDialogMessage;
struct EEUIFErrorCallbackData;               // ee/EEUIFError.h
class EECivilization;
class EEUIFMainGame;
class FSFile;
class FSFileSpec;
struct HWND__;
class GEColor;
class TSCriticalSection;
class TSEvent;
class TSThread;
class UIEvent;
class UIForm;
class UReferenceCounter;
class UWideString;

class EEUserInterface {
public:
    // 0x276631: the three queues in the init list (before the vptr); body: mThread = TSScheduler::GetCurrentThread(),
    // mB05C = false, mIsUpdating = true, mIsQuitting = false, mRandomSeed = TMGetTime(), mIconBarUpdatePosted = false, then the three
    // CreateCriticalSection asserts (lines 0xae..0xb0: mActionLock, mRedoLock, mUndoLock) and CreateEvent
    // (mMessageQueueDone, 0xb1). mLocalPlayer (+0x58) is not set here.
    EEUserInterface();
    // 0x276740 (??_G 0x276724): drains the queues (while (GetQueueSize(q, lock) > 0) PopFront(q, lock)->Dereference(),
    // action, undo, redo), destroys mActionLock, mRedoLock, mUndoLock, mMessageQueueDone
    virtual ~EEUserInterface();

    // 0x278f79: references inAction and pushes it on the action queue
    void PostAction(EEAction* inAction);
    // 0x2793cc: posts thread message 0x420 (inCivilization, inFlag) to the UI thread
    long OpenCivilizationBuilder(EECivilization* inCivilization, bool inFlag);   // 0x2793cc: returns Post's result
    // 0x2793e9: posts thread message 0x419 (inForm) (closes the chat line, the campaign lists); returns Post's result
    long CloseForm(UIForm* inForm);
    // 0x279492: posts thread message 0x40d (inForm) (opens the form)
    long OpenForm(UIForm* inForm);
    // 0x27932d: references inEvent and posts thread message 0x40e (inEvent, inForm)
    long PostEvent(UIEvent* inEvent, UIForm* inForm);   // 0x27932d: returns Post's result (EESEEHideUI::ProcessEvent 0x1af4c1 returns it)
    // 0x278fce: pops the redo queue's last action, calls its slot +0x04 and pushes it on the undo queue; returns 7
    // when the redo queue is empty (`== 0`), else 0. Provisional name (EEUIFSERedo)
    long Redo();
    // 0x27985e: the undo queue's last action, slot +0x08, then onto the redo queue; 7 when empty (`> 0` test)
    long Undo();
    // the scenario editor's loading thread (EESELoadThread::HandleMessage) (provisional names)
    void LeaveScenarioEditor();             // 0x27924e (thread message 0x410)
    void StartScenarioTest();               // 0x2791ee (thread message 0x40f)
    void StartScenarioTest2();              // 0x279468 (thread message 0x415)
    void ShowLoadedObject(UReferenceCounter* inObject);   // 0x27941b (thread message 0x40c)
    // 0x2795fa: posts thread message 0x443 (inPlayer, inPercent) (EEMLoadPercent; provisional name)
    void SetLoadPercent(long inPlayer, unsigned long inPercent);
    // 0x2795d6: posts thread message 0x442 (inLag, inStatus == 1) (EEMLagStatus; provisional name)
    void SetLagStatus(unsigned short inLag, unsigned char inStatus);
    void UpdateJITManufacturing();          // 0x279591 (thread message 0x416; EEMJITManufacturing; provisional name)
    // UI thread messages posted by the in-game options (provisional names, by message number)
    long PostEnterScenarioEditor(bool inFlag);  // 0x279263 (returns Post's result: EEUIEMGEnterScenarioEditor keeps it) (message 0x411; ReturnToEditor passes gUIManager->mB13C)
    void PostPauseInformation(bool inFlag);  // 0x279293 (message 0x427)
    void PostRestartGame();                 // 0x279453 (message 0x428; RestartGame)
    void PostQuit();                        // 0x27927e (message 0x401; the main screen's Exit)
    void PostStartLoading();                // 0x2792c3 (message 0x407, 0x413 when gUIManager+0x13c; EEMStartLoading; provisional name)
    long SwitchToForm(UIForm* inForm);      // 0x279402 (message 0x414 with inForm; the campaign choice; provisional name)

    // The rest of the class (names by thread message or rva; argument types inferred from `ret N`, byte pushes
    // (movzx -> bool) and what the body builds).
    void PostScreenShot(long inX, bool inFlag1, bool inFlag2);  // 0x279139 (new 0x10-byte block {inX, 0, 0, inFlag2})
    void PostUpdateBackgroundMedia();       // 0x279174
    void PostShowEpochName();               // 0x279189
    void PostDialogMessage(const EESMDialogMessage& inMessage);  // 0x27919e (posts a new copy, 0x40 bytes)
    void PostSelectEditorTab(long inX, long inY);  // 0x2791d3
    void PostDisplayChanged(bool inFlag);   // 0x279203
    void PostCancelGame();                  // 0x27921e
    void PostLoadGameFailed(bool inFlag);   // 0x279233 (inFlag as the second message parameter)
    void PostHostMigrated();                // 0x2792ae
    void PostLoadSavedGame(const FSFileSpec& inSpec);  // 0x2792ed (new FSFileSpec(inSpec))
    void PostPlayMovie(const FSFileSpec& inSpec, UReferenceCounter* inObject);   // 0x27934c (references inObject)
    void PostMovieDone(UReferenceCounter* inObject);   // 0x279396 (references inObject)
    void PostHideMessages();                // 0x2793b7
    void PostErrorClosed(UIForm* inForm, EEUIFErrorCallbackData* inData);  // 0x279438 (EEUIFError::Close)
    long PostDisplayCivBuilder();           // 0x27947d (returns Post's result: 0x65ff56 returns it)
    void PostChangeRasterizer(long inX, long inY);  // 0x2794ab
    // 0x2794c6: new EEChatLogEntry {inColor, inName, inText, mType = inType, mTime = inTime}, thread message 0x418
    long AddChatLogEntry(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                         unsigned long inTime, unsigned long inType);
    // 0x279536: AddChatLogEntry(GEColor(1, 1, 1), UWideString(" > "), inText, gWorldTable.GetLongValue(25) * 3, 2); inY unused
    long AddChatLogEntry(const UWideString& inText, long inY);
    void PostLobbyUpdatePlayer(long inX, long inY);  // 0x2795a6
    void PostLobbyUpdateGameOptions();      // 0x2795c1
    void PostLobbyShowTransferProgress(long inX, long inY);  // 0x279615
    void PostLobbyUpdateForm();             // 0x279630
    void PostRefreshSavedGroup(long inX);   // 0x279645
    // 0x27965e: clamps inFraction to [0, 1], percent = TMFloatToLong-style * 100; in multiplayer sends
    // EEMLoadPercent(mLocalPlayer, percent) every 5%, then thread message 0x40a (percent)
    void SetLoadProgress(float inFraction);
    void SetLoadingText(long inX);          // 0x27973d (string id: 0x7654, 0x7655)
    void PostMessage424Once();              // 0x279756 (if (this != NULL && !mIconBarUpdatePosted): post, mIconBarUpdatePosted = true)

    // in-unit helpers and the thread's handlers (all thiscall, called on this or gUserInterface; signatures
    // from the bodies, provisional names unless said)
    unsigned long GetQueueSize(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock);    // 0x277529
    EEAction* PeekBack(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock);           // 0x277e8e
    EEAction* PopFront(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock);           // 0x277eb4
    EEAction* PopBack(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock);            // 0x277ef2
    void PushBack(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock, EEAction* inAction);  // 0x278f54
    void ClearQueue(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock);  // 0x276fe0 (Dereference each)
    long Run(void* inParameter);            // 0x2779b8 (the UI thread's loop; called from 0x140d73)
    // the parameters are the thread message's void* (pointers for most messages): LONG_PTR keeps them whole on x64
    // (and is long on x86, the original's type)
    void HandleMessage(unsigned long inMessage, LONG_PTR inParam1, LONG_PTR inParam2);  // 0x27828f (void: no result in eax) (the 0x401..0x443 switch)
    // 0x2798ce (address taken at 0x144747; calls the members below through gUserInterface)
    static LRESULT CALLBACK WindowProc(HWND inWindow, UINT inMessage, WPARAM inWParam, LPARAM inLParam);
    void ChangeRasterizer(long inRasterizer, long inScreen);  // 0x276804 (switches gRasterizer; message 0x41e)
    // 0x2768fe: resizes the game window (Empire Earth.cpp 0x140306) when the size changes or inForce
    void ResizeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inForce, bool inFlag);
    void ShowEpochName();                   // 0x2769da (message 0x404: the status text)
    void ShowMessageOfTheDay(const UWideString& inGame, const UWideString& inSystem);  // 0x276a38 (message 0x440)
    // r25pre-won-2: dead in AoC (only EEWonNet.cpp's DEAD-IN-AOC MOTDCallback calls it; name PROVISIONAL): base exe
    // 0x661f01 posts 0x440 (two new UWideString copies). No body yet (owner question).
    void PostMOTD(const UWideString& inGame, const UWideString& inSystem);
    void PostLoginDone();                   // base exe 0x661aef (message 0x441; DEAD-IN-AOC like PostMOTD)
    void TestScenario();                    // 0x276ab0 (message 0x415: starts the tested scenario)
    void CloseIMECandidateWindow();         // 0x276c9c (IMN_CLOSECANDIDATE)
    void CloseIMECompositionString();       // 0x276cb1 (WM_IME_ENDCOMPOSITION)
    void SaveAndTestScenario();             // 0x276cbc (message 0x40f: saves ScenDefault and starts it)
    void LeaveMap(UIForm* inForm, bool inResize);      // 0x277023 (ClearQueue on all three, then the form)
    void CancelGame();                      // 0x277170 (message 0x412)
    void CloseScenarioEditor();             // 0x277290 (message 0x410)
    void EnterScenarioEditor(bool inLoad);  // 0x277315 (message 0x411: into the scenario editor)
    long CreateRenderObject();              // 0x27754b (pushes the UI's render object)
    void LoadSavedGame(FSFile* inFile);     // 0x27757c (message 0x408: loads a saved game; deletes inFile)
    void StartLoadingFromEditor();          // 0x2777cd (message 0x413)
    void OpenSelectServer();                // 0x27783c (message 0x441)
    void ShowCivilizationBuilder(long inX, bool inFlag1, bool inFlag2);  // 0x277c1f (messages 0x41f/0x420)
    void PauseGraphics();                   // 0x277e63 (waits for the graphics thread's +0x56 flag)
    void ProcessActions();                  // 0x277f21 (runs the queued actions)
    void UpdateIMECandidateList(HWND__* inWindow);  // 0x278040 (the IME candidate list)
    void UpdateIMECompositionString(HWND__* inWindow);  // 0x2780d3 (the IME composition string)
    void AddIMEResultString(HWND__* inWindow);  // 0x2781da (the IME result string)
    void UpdateInterface();                 // 0x278ed3 (input, UpdateUI, the actions)
    void OpenIMECandidateWindow();          // 0x278f90 (IMN_OPENCANDIDATE)
    void OpenIMECompositionString(HWND__* inWindow);  // 0x278fc0 (WM_IME_STARTCOMPOSITION)
    void RestartGame();                     // 0x279013 (message 0x428)
    void ShowLagStatus(unsigned long inPlayer, bool inLagging);  // 0x27977c (message 0x442; the caller passes (unsigned short))
    void ResumeGraphics();                  // 0x2798a3
    void ShowLoadingText(long inStringID);  // 0x279fbc (message 0x40b)

    std::deque<EEAction*> mActionQueue;     // +0x04 (lock mActionLock; PostAction)
    std::deque<EEAction*> mUndoQueue;       // +0x18 (lock mUndoLock; Undo pops, Redo pushes)
    std::deque<EEAction*> mRedoQueue;       // +0x2c (lock mRedoLock)
    TSThread* mThread;                      // +0x40 the constructing thread; every Post* goes to it
    TSCriticalSection* mActionLock;         // +0x44
    TSCriticalSection* mUndoLock;           // +0x48
    TSCriticalSection* mRedoLock;           // +0x4c
    TSEvent* mMessageQueueDone;             // +0x50
    unsigned long mRandomSeed;              // +0x54 TMGetTime(); EEMStartGame-style handler 0xc7cdc sets it with
                                            // gPhysics->mRandom's seed (provisional name)
    long mLocalPlayer;                      // +0x58 EEMLoadPercent's player (set at 0x1e1caa; provisional name)
    bool mB05C;                             // +0x5c
    unsigned char mIsUpdating;              // +0x5d (true; message handler sets it from inParam2; a byte: 0x278417 truncates a long into it)
    bool mIconBarUpdatePosted;              // +0x5e (PostMessage424Once's guard; the handler clears it)
    bool mIsQuitting;                       // +0x5f (set by the handler; the loop exits on it)
};

// 0x27fa40 (no arguments; returns a status): what EEUIEProcessTimerExpired's event does (provisional name)
long EEProcessTimerExpired();

#endif
