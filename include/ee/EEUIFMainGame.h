// C:\EEX\Empire Earth\User Interface Thread\EEUIFMainGame.h (RECONSTRUCTION; file name inferred). The in-game
// form (RTTI EEUIFMainGame > UIForm; vtable 0x44aa10, 20 slots = UIForm's; EEUIFMainGame.cpp 0x26272b..0x26783d). Its
// controls are reached by index through UIForm::GetControl. Layout from the ctor 0x262e73 / dtor 0x2647e1.
// Exactly two array members (each array member costs every includer a `$E` number). Requires lle/UIForm.h. The members
// the rest of the game calls (the saved game's state, the interface state, the error strings, the backgrounds) are
// inline over the Rust form: out of line they put the game's messages, server, scenario manager, renderer and saves
// in one dependency cycle with the form (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts, round 3").
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
class EEUIFMGUnitHack;
class EEUnit;
class UIBackground;
class TMPoll;
class TSCriticalSection;
class EEUIFMainGame;
class EESMDialogMessage;
class EETechTreeNode;
class EECalamityClass;
class EEMedia;
class EEComplexUnit;
class EETile;
#include "rust/game/screens.h"   // EeScreensMainFormFields, EeScreensMainFormIo

// One call of the form's work in Rust (ee_screens main_form.rs, host EEUIFMainGame::HostOp): the form, the
// picked unit, the held text, the chat line's colour, name, text and flags, the dialog message
struct EEUIFMGFormCall {
    EEUIFMainGame* mForm;                   // first: main_form_ffi.rs reads it (the toolkit's direct path)
    EEUnit* mPicked;
    UWideString mText;
    const GEColor* mColor;
    UWideString mName;
    UWideString mChat;
    unsigned long mFlags;
    const EESMDialogMessage* mMessage;
    EETechTreeNode* mNode;                  // the start modes' tech tree node, unit, caster, calamity
    EEUnit* mUnit;
    EEUnit* mCaster;
    EECalamityClass* mCalamity;
    EEMedia* mOldMedia;                     // UpdateBackgroundMedia's old and new media
    EEMedia* mMedia;
    EEComplexUnit* mFirst;                  // the mouse modes' first selected unit and pointed tile
    EETile* mTile;
    EEUIBuildWall* mWall;                   // the placement whose Update runs (the game form's or the editor's)
    EEUIBuildGate* mGate;
    EEUIFMGStatusButton* mButton;           // a command button's press, CopyData or help string (its result)
    UWideString* mOut;
    explicit EEUIFMGFormCall(EEUIFMainGame* inForm)
        : mForm(inForm), mPicked(NULL), mColor(NULL), mFlags(0), mMessage(NULL), mNode(NULL), mUnit(NULL),
          mCaster(NULL), mCalamity(NULL), mOldMedia(NULL), mMedia(NULL), mFirst(NULL), mTile(NULL),
          mWall(NULL), mGate(NULL), mButton(NULL), mOut(NULL) {}
};


// the plain members (+0x520 mKeyboardModes .. +0x5f3 mIsButtonBlinking: the modes of each interface state, the state,
// the media, the groups, the poll's time, the hot key and clock modes, the flags) are the Rust record
// EeScreensMainFormFields (ee_screens main_form_fields.rs), under their names
class EEUIFMainGame : public UIForm, public EeScreensMainFormFields {
public:
    virtual void SetHelpTextPopupLocation() {}   // +0x24 datacmp: slot 9 = 0x4e123d (an empty `ret`)
    EEUIFMainGame(UIManager* inManager, const UIDbDataForms& inData);
    // Destroys the main game form, its actions, panel groups, poll and unit hack
    virtual ~EEUIFMainGame() { ee_screens_mfb_destroy((uintptr_t)this); }
    // Activates the form: frees the mouse, resumes a game paused only by leaving it, refreshes the saved groups
    virtual void Activate() { this->Rust(EE_SCREENS_MF_DO_ACTIVATE); }                // +0x04 0x2648d4
    // Deactivates the form: frees and resets the mouse and leaves any interface mode
    virtual void Deactivate() { this->Rust(EE_SCREENS_MF_DO_DEACTIVATE); }              // +0x14 0x264d40
    // Runs the keyboard mode of the current interface state (the cinematic's keys), else the form's keys
    virtual long UpdateKeyboard()
    {
        return (long)this->Rust(EE_SCREENS_MF_DO_KEYBOARD, this->mKeyboardModes[this->mUIState]);
    }          // +0x2c 0x271290 (EEUIFMGStatusButton.cpp's range)
    // Runs the mouse: the unit hack, the form's controls, then the current interface mode's mouse (ee_screens
    // mouse_modes.rs update_mouse); returns the last UpdateMouse result (the modes answer 0)
    virtual long UpdateMouse() { return (long)this->Rust(EE_SCREENS_MF_DO_MOUSE); }             // +0x30 0x2712b8 (likewise)
    // Closes the form: hides the messages, gives the graphics thread the UI render object back and unloads the
    // background media
    virtual void Close() { this->Rust(EE_SCREENS_MF_DO_CLOSE); }                   // +0x38 0x264cf1
    virtual long CopyData();                // +0x3c 0x264dc4
    // Draws the form's fixed background panels (the unit panels only with a selection)
    virtual long DrawPolygons() { return (long)this->Rust(EE_SCREENS_MF_DO_DRAW); }            // +0x40 0x26550c
    // Stores an error log line for the next snapshot
    // Opens the form: sound latency and volume, centred mouse, empty error log, the cancel-mode event, the
    // backgrounds and the civilization builder button's blinking
    virtual void Open() { this->Rust(EE_SCREENS_MF_DO_OPEN); }                    // +0x48 0x266038

    // the saved game's main-form state (EEPWorldHelper's Load 0x14c39b / Save 0x14c525 call them; provisional
    // names)
    // Reads the information and civilization builder blinking flags from a saved game (version 0.0xd2 on)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
    {
        return (long)ee_screens_mff_load((uintptr_t)&this->mInfoBlinking, (uintptr_t)&this->mIsButtonBlinking,
                                         (uint32_t)inMajorVersion, (uint32_t)inMinorVersion, (uintptr_t)&inFile);
    }
    // Writes the information and civilization builder blinking flags to the saved game
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
    {
        return (long)ee_screens_mff_save(this->mInfoBlinking, this->mIsButtonBlinking, (uintptr_t)&outFile);
    }

    // 0x2668ee (EEMLoadMainScreen passes 0x11, EEMSavedGroupSelect 0)
    void SetUIState(long inUIState)
    {
        EEUIFMGFormCall theCall(this);
        ee_screens_main_form_ui_state(&theCall, inUIState);
    }
    // (0x264b1f / 0x264b35, BuildGate / BuildWall over the build action +0x514: EEUIBuildGate/EEUIBuildWall::Build
    // call mBuild's own, ee/EEARust.h)

    // the rest in address order (provisional names)
    void DisplayChatLine(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                         unsigned long inFlags);    // 0x2649a2 (controls 0x65..0x6b: EEUICChatLine)
    // Creates one of the six panel groups (main, statistics, chat, diplomacy, options, civilization builder)
    void CreateGroup(unsigned long inIndex) { ee_screens_mfb_create_group((uintptr_t)this, (uint32_t)inIndex); }
    void DisplayChat(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                     unsigned long inFlags, unsigned long inArg5);
    void SetEntityGroup(long inGroup);
    // Tells whether a screen point is in the game view and not under the unit information panel
    bool IsInGameView(const GE2DPoint& inLocation)
    {
        return ee_screens_mfb_in_game_view((uintptr_t)this, inLocation.GetX(), inLocation.GetY());
    }     // 0x265a06 (in the view, outside background 0x64)
    void SetErrorString(const UWideString& inText, unsigned long inIndex)   // 0x265ff6 (mStrings, mStringsChanged)
    {
        ee_screens_mff_set_error((uintptr_t)this, (uint32_t)inIndex, (uintptr_t)&inText);   // (hud_more.rs)
    }
    void ShowInformation(EEUIFMGInformation* inInformation);   // 0x26533e (the scrolling text 0xd3, portrait 0xdf)
    // 0x266384: starts placing a building from a unit's tech when it can be produced; otherwise beeps and explains in
    // the chat log (ee_screens main_form.rs, as the other start modes)
    void BuildUnit(unsigned long inUnitID, long inTechnology)
    {
        EEUIFMGFormCall theCall(this);
        ee_screens_main_form_build_unit(&theCall, (int64_t)inUnitID, inTechnology);
    }
    // the start modes (ee_screens main_form.rs): 0x2664b4 a gate for a tech (state 0xb), 0x266594 a wall (state 6),
    // 0x26662d a calamity cast with the first selected unit (state 5; at once for a self-targeted one), 0x26679a the
    // flare (state 0x15), 0x26681d patrol when units are selected (state 0x10)
    void StartGate(long inTechnology) { this->StartMode(0, inTechnology); }
    void StartWall(long inTechnology) { this->StartMode(1, inTechnology); }
    void StartCalamity(long inTechnology) { this->StartMode(2, inTechnology); }
    void StartFlare() { this->StartMode(3, 0); }
    void StartPatrol() { this->StartMode(4, 0); }
    void StartMode(uint32_t inMode, long inTechnology)
    {
        EEUIFMGFormCall theCall(this);
        ee_screens_main_form_start(&theCall, inMode, inTechnology);
    }
    void UpdateCursorKind();                // 0x266f8b (the cursor: ee_screens cursor.rs, with 0x265791 GetAttackStance,
                                            // 0x26588c GetCommandOnUnit, 0x265a4f EEGetAttackRangeKind, 0x265d7a,
                                            // 0x265e35 GetRepairCommand, 0x265e89 GetTransportCommand)
    // the members that are Rust's whole (EE_SCREENS_MF_DO_*, with their argument: the original's
    // UpdateBackgroundMedia 0x264b4b, SetPauseText 0x26572f, PauseInformation 0x2662d7,
    // ResumeInformation 0x266367, SetChatTop 0x26676e, HideMessages 0x2669e8,
    // RefreshSavedGroups 0x26716e, UpdateIconBar 0x2671b0, and the overrides above)
    int64_t Rust(uint32_t inWhich, int64_t inA = 0)
    {
        EEUIFMGFormCall theCall(this);
        return ee_screens_main_form_member(&theCall, inWhich, inA);
    }
    // a command button's member in Rust (EE_SCREENS_SB_DO_*; ee_screens status_button.rs over the form's host)
    int64_t ButtonCall(EEUIFMGStatusButton* inButton, uint32_t inWhich, UWideString* outString = NULL)
    {
        EEUIFMGFormCall theCall(this);
        theCall.mButton = inButton;
        theCall.mOut = outString;
        return ee_screens_status_button_member(&theCall, inWhich);
    }
    void ShowAllBackgrounds() { this->Rust(EE_SCREENS_MF_DO_SHOW_BACKGROUNDS); }   // 0x26568e (EERenderObject's)
    void HideAllBackgrounds() { this->Rust(EE_SCREENS_MF_DO_HIDE_BACKGROUNDS); }
    // UIForm's protected members the host's shims call (main_form_calls.rs)
    using UIForm::GetDrawToolTipShadow;
    using UIForm::SetDrawToolTipShadow;

    UWideString mStrings[0x24];             // +0x1a0 (CopyData copies them into control 0x1b's list while mStringsChanged)
    EEUIBuildWall mBuildWall;               // +0x494
    EEUIBuildGate mBuildGate;               // +0x4b4
    UIBackground* mUnitInfoBackground;      // +0x4d4 the background 0x64 (Open)
    UIEvent* mWheelZoom;                    // +0x4d8 (EE_SCREENS_CAM_WHEEL_ZOOM)
    UIEvent* mGrabScroll;                   // +0x4dc (the middle button's pressed and unpressed event: EECreateGrabScroll)
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
};

#endif
