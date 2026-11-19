// C:\EEX\Empire Earth\User Interface Thread\EEUIFMainGame.h (RECONSTRUCTION; file name inferred). The in-game
// form (RTTI EEUIFMainGame > UIForm; vtable 0x44aa10, 20 slots = UIForm's; EEUIFMainGame.cpp 0x26272b..0x26783d). Its
// controls are reached by index through UIForm::GetControl. Layout from the ctor 0x262e73 / dtor 0x2647e1.
// Exactly two array members (each array member costs every includer a `$E` number). Requires lle/UIForm.h.
#ifndef EE_EEUIFMAINGAME_H
#define EE_EEUIFMAINGAME_H

#include "lle/GERhombus.h"
#include "ee/EEUIBuildGate.h"
#include "ee/EEUIBuildWall.h"

class EEABuild;
class EEACalamity;
class EEALasso;
class EEAMouseClick;
class EEAScroll;
class EEUIFMGGroup;
class EEUIFMGStatusButton;
class EEUIFMGInformation;
class EEUIFMGMainGroup;
class EEUIFMGUnitHack;
class EEUnit;
class UIBackground;
class TMPoll;
class TSCriticalSection;
#include "rust/game/screens.h"   // EeScreensMainFormFields, EeScreensMainFormIo


// the plain members (+0x520 mKeyboardModes .. +0x5f3 mIsButtonBlinking: the modes of each interface state, the state,
// the media, the groups, the poll's time, the hot key and clock modes, the flags) are the Rust record
// EeScreensMainFormFields (ee_screens main_form_fields.rs), under their names
class EEUIFMainGame : public UIForm, public EeScreensMainFormFields {
public:
    virtual void SetHelpTextPopupLocation() {}   // +0x24 datacmp: slot 9 = 0x4e123d (an empty `ret`)
    EEUIFMainGame(UIManager* inManager, const UIDbDataForms& inData);
    virtual ~EEUIFMainGame();
    virtual void Activate();                // +0x04 0x2648d4
    virtual void Deactivate();              // +0x14 0x264d40
    virtual long UpdateKeyboard();          // +0x2c 0x271290 (EEUIFMGStatusButton.cpp's range)
    virtual long UpdateMouse();             // +0x30 0x2712b8 (likewise)
    virtual void Close();                   // +0x38 0x264cf1
    virtual long CopyData();                // +0x3c 0x264dc4
    virtual long DrawPolygons();            // +0x40 0x26550c
    virtual void Open();                    // +0x48 0x266038

    // the saved game's main-form state (EEPWorldHelper's Load 0x14c39b / Save 0x14c525 call them; provisional
    // names)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);

    void SetUIState(long inUIState);        // 0x2668ee (EEMLoadMainScreen passes 0x11, EEMSavedGroupSelect 0)
    // 0x264b1f / 0x264b35: the build action (+0x514, EEABuild) BuildGate / BuildWall
    // (EEUIBuildGate/EEUIBuildWall::Build; provisional names)
    void BuildGate(const GE2DPoint& inLocation, long inDirection);
    void BuildWall(const GE2DPoint& inStart, const GE2DPoint& inEnd);

    // the rest in address order (provisional names)
    void DisplayChatLine(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                         unsigned long inFlags);    // 0x2649a2 (controls 0x65..0x6b: EEUICChatLine)
    void SetStatusText(const UWideString& inText);  // 0x264cd5 (control 0x19)
    void CreateGroup(unsigned long inIndex);
    void DisplayChat(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                     unsigned long inFlags, unsigned long inArg5);
    void SetEntityGroup(long inGroup);
    bool IsInGameView(const GE2DPoint& inLocation);     // 0x265a06 (in the view, outside background 0x64)
    void SetErrorString(const UWideString& inText, unsigned long inIndex);   // 0x265ff6 (mStrings, mStringsChanged)
    void ShowInformation(EEUIFMGInformation* inInformation);   // 0x26533e (the scrolling text 0xd3, portrait 0xdf)
    void BuildUnit(unsigned long inUnitID, long inTechnology);  // 0x266384 (the build mode for a unit's technology)
    void StartGate(long inTechnology);      // 0x2664b4 (state 0xb)
    void StartWall(long inTechnology);      // 0x266594 (state 6)
    void StartCalamity(long inTechnology);  // 0x26662d (state 5)
    void StartFlare();                      // 0x26679a (state 0x15)
    void StartPatrol();                     // 0x26681d (state 0x10)
    void UpdateCursorKind();                // 0x266f8b (the cursor: ee_screens cursor.rs, with 0x265791 GetAttackStance,
                                            // 0x26588c GetCommandOnUnit, 0x265a4f EEGetAttackRangeKind, 0x265d7a,
                                            // 0x265e35 GetRepairCommand, 0x265e89 GetTransportCommand)
    // the members that are Rust's whole (EE_SCREENS_MF_DO_*, with their argument: the original's
    // UpdateBackgroundMedia 0x264b4b, SetPauseText 0x26572f, PauseInformation 0x2662d7,
    // ResumeInformation 0x266367, SetChatTop 0x26676e, HideMessages 0x2669e8,
    // RefreshSavedGroups 0x26716e, UpdateIconBar 0x2671b0, and the overrides above)
    int64_t Rust(uint32_t inWhich, int64_t inA = 0);
    // a command button's member in Rust (EE_SCREENS_SB_DO_*; ee_screens status_button.rs over the form's host)
    int64_t ButtonCall(EEUIFMGStatusButton* inButton, uint32_t inWhich, UWideString* outString = NULL);
    void ShowAllBackgrounds();              // 0x26568e (EERenderObject's; Rust's whole)
    void HideAllBackgrounds();
    // the host of the per-frame work in Rust (ee_screens main_form.rs: CopyData, RefreshStatus, DisplayChatLine,
    // ShowInformation); a member for UIForm's protected GetMouseCaptured and CopyData
    static int64_t HostOp(void* inCall, uint32_t inOp, int64_t inA, int64_t inB, EeScreensMainFormIo* ioIo);

    UWideString mStrings[0x24];             // +0x1a0 (CopyData copies them into control 0x1b's list while mStringsChanged)
    GE3DPoint mPoint470;                    // +0x470 (the inline ctor's zeros)
    GERhombus mRhombus47C;                  // +0x47c
    EEUIBuildWall mBuildWall;               // +0x494
    EEUIBuildGate mBuildGate;               // +0x4b4
    UIBackground* mUnitInfoBackground;      // +0x4d4 the background 0x64 (Open)
    UIEvent* mWheelZoom;                    // +0x4d8 (EE_SCREENS_CAM_WHEEL_ZOOM)
    UIEvent* mGrabScroll;                   // +0x4dc (the middle button's pressed and unpressed event: EECreateGrabScroll)
    long mL4E0;                             // +0x4e0
    TSCriticalSection* mErrorCriticalSection;        // +0x4e4 (guards mStrings)
    EEUIFMGUnitHack* mUnitHack;             // +0x4e8
    EEUIFMGGroup* mGroups[6];               // +0x4ec (0: EEUIFMGMainGroup, 1: statistics, 2: chat, 3: diplomacy,
                                            //  4: options, 5: civilization builder; CreateGroup 0x26522b)
    TMPoll* mPoll;                          // +0x504 (200 ms)
    EEAScroll* mScroll;                     // +0x508
    EEAMouseClick* mMouseClick;             // +0x50c
    EEALasso* mLasso;                       // +0x510
    EEABuild* mBuild;                       // +0x514
    EEACalamity* mCalamity;                 // +0x518
    long mL51C;                             // +0x51c
};

#endif
