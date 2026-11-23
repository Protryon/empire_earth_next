// C:\EEX\Empire Earth\User Interface Thread\EEUIManager.h (RECONSTRUCTION; file name inferred). The game's UI
// manager (RTTI EEUIManager > UIManager; vtable 0x44ae90, ctor 0x271319, sizeof 0x140: `new` at the startup code).
// The game holds it in `gUIManager` (0x53191c, assigned at 0x1424f7; provisional name). EEUIManager.cpp is
// 0x271319..0x2750a9+ (the ctor registers the control-event factories 0x2726b3.. with RegisterControlEventCallback
// and one form-event factory 0x271a10; catalog). Field names
// are provisional. Requires lle/UIManager.h.
#ifndef EE_EEUIMANAGER_H
#define EE_EEUIMANAGER_H

#include "rust/game/screens.h"   // EeScreensUiManagerFields (ee_screens ui_objects.rs)
#include "ee/EEGameUI.h"
#include "rust/ui/formobj.h"

class EEUIFMainGame;
class EEUIFScenarioEditor;

// the message boxes' button callbacks (EEUIFError, 0x260ffb)
typedef void (*EEUIFErrorCallback)(UIForm* inForm);
// the password prompt's callback: gets the typed text (EEUIFError 0x26151a stores it with the edit box's text;
// provisional name)
typedef void (*EEUIFPasswordCallback)(const char* inPassword);
// what a button hands to the UI thread when the box closes (8 bytes, no ctor; provisional name)
struct EEUIFErrorCallbackData {
    EEUIFErrorCallback mCallback;           // +0x00
    void* mData;                            // +0x04
};

// The game's message box (once the form class EEUIFError, RTTI EEUIFError > UIForm, vtable 0x44a770, and its buttons
// EEUIFEBOK and EEUIFEBCancel; now kinds over the form and button shells: ee_game shells_dialog.rs, the plans and
// layouts ee_screens dialog.rs'; EEUIFError.cpp keeps the box's host): a new box shown as inKind
// (EE_SCREENS_DIALOG_*: 0 a message with OK, shown when inFlag; 1 Yes/No; 2 Yes/No/Cancel; 3 a text, or a password
// when inFlag, which OK hands to inFirst; 4 Cancel only, closing inCloseForm); inFirst is OK's or Yes' or Cancel's
// callback, inSecond No's, each given inData (the original's DisplayMessage 0x261788, DisplayQuestion 0x261894,
// DisplayYesNoCancel 0x2619f9, DisplayPassword 0x26151a and DisplayCancel 0x26167f). EEUIFError.cpp registers it in
// the game's UI table (ee/EEGameUI.h).
inline void EEUIFErrorDisplay(UIManager* inManager, unsigned long inKind, const UWideString& inMessage,
                              unsigned long inAlignment, void* inData, EEUIFErrorCallback inFirst,
                              EEUIFErrorCallback inSecond, UIForm* inCloseForm, bool inFlag)
{
    gEEGameUI.mErrorDisplay(inManager, inKind, inMessage, inAlignment, inData, inFirst, inSecond, inCloseForm, inFlag);
}

struct EEUIManagerCall;                     // (EEUIManager.cpp: the host's record of one ui_manager.rs call)

// the own members (the main game and scenario editor forms, the editor flags) are the Rust record
// EeScreensUiManagerFields (ee_screens ui_objects.rs), under their names
class EEUIManager : public UIManager, public EeScreensUiManagerFields {
public:
    // the ee_screens ui_manager.rs host (EE_SCREENS_UM_OP_*; a member: UIManager's updates are protected)
    static int64_t HostOp(EEUIManagerCall& ioCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC);
    // EEUIManager.cpp: 0x271319 (`ret 0x10`; the fourth argument is unused) registers the event
    // factories, loads the UI database, then the members (store order +0x12c, +0x13c, +0x130, +0x13d, +0x138)
    EEUIManager(GERasterizer* inRasterizer, unsigned long inNumberOfSurfaces, unsigned long inScreen,
                const UString& inUnused);
    virtual ~EEUIManager();
    // vtable overrides (slots from 0x44ae90): CreateUserDefinedBackground 0x14 (0x274564), CreateUserDefinedControl
    // 0x18 (0x274647; not declared yet), CreateForm 0x1c (0x274c94), UpdateKeyboard 0x2c (0x2750a9), UpdateMouse
    // 0x30 (0x2751ab)
    // Creates the game's framed generic background for a background table entry (flags from its attributes)
    virtual UIBackground* CreateUserDefinedBackground(const UIBackgroundTable& inTable)
    {
        return reinterpret_cast<UIBackground*>(ee_screens_um_create_background((uintptr_t)this, (uintptr_t)&inTable));
    } //274564, actual base slot14
    // Creates the game's own controls from the UI database (mini map, status buttons, icon bars, WON lists ...;
    // ee_screens ui_manager_calls.rs over the shims below)
    virtual UIControl* CreateUserDefinedControl(UIControlsTable& inTable)
    {
        return reinterpret_cast<UIControl*>(ee_screens_um_create_control((uintptr_t)this, (uintptr_t)&inTable));
    } //274647, actual base slot18
    // Creates the main game form as the game's own class; other forms as the base manager does
    virtual UIForm* CreateForm(const UIDbDataForms& inData)
    {
        return reinterpret_cast<UIForm*>(ee_screens_um_create_form((uintptr_t)this, (uintptr_t)&inData));
    }
    virtual long UpdateKeyboard();
    virtual long UpdateMouse(const GE2DPoint& inMouseLocation);
    // 0x274d79 / 0x274e2d: a new message box with the cancel / yes-no-cancel buttons (provisional names).
    // inAlignment: EEUIFError::Display*'s (pushed as a dword)
    void DisplayCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inCallback,
                       UIForm* inCancel)
    {
        EEUIFErrorDisplay(this, EE_SCREENS_DIALOG_CANCEL, inMessage, inAlignment, inForm, inCallback, NULL, inCancel, false);
    }
    void DisplayYesNoCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inYes,
                            EEUIFErrorCallback inNo)
    {
        EEUIFErrorDisplay(this, EE_SCREENS_DIALOG_YES_NO_CANCEL, inMessage, inAlignment, inForm, inYes, inNo, NULL, false);
    }
    // Creates the scenario editor form on first use
    void CreateScenarioEditor() { ee_screens_um_create_editor((uintptr_t)this); }            // 0x273e7d (creates the editor form at +0x130 once)
    // 0x274db5: a new EEUIFError (0x1bc bytes) showing inMessage with one button (0x261788). Parameter names are
    // provisional
    void DisplayMessage(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inCallback,
                        bool inFlag)
    {
        EEUIFErrorDisplay(this, EE_SCREENS_DIALOG_MESSAGE, inMessage, inAlignment, inForm, inCallback, NULL, NULL, inFlag);
    }
    // 0x274d40: a new EEUIFError (0x1bc bytes) with a password edit box (0x26151a); inCallback gets the text
    // (the WON game room's join, 0x1ea544). Parameter names are provisional
    void DisplayPasswordPrompt(const UWideString& inMessage, unsigned long inAlignment,
                               EEUIFPasswordCallback inCallback, bool inFlag)
    {
        EEUIFErrorDisplay(this, EE_SCREENS_DIALOG_PASSWORD, inMessage, inAlignment, NULL, (EEUIFErrorCallback)inCallback, NULL,
                          NULL, inFlag);
    }
    // 0x274df1: the same with two buttons (0x261894)
    void DisplayQuestion(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inYes,
                         EEUIFErrorCallback inNo)
    {
        EEUIFErrorDisplay(this, EE_SCREENS_DIALOG_QUESTION, inMessage, inAlignment, inForm, inYes, inNo, NULL, false);
    }
    void StartScenarioEditor();             // 0x274e69 (sets +0x13c, starts a local server)
    // Deletes the scenario editor form
    void DeleteScenarioEditor() { ee_screens_um_delete_editor((uintptr_t)this); }            // 0x274d24 (deletes +0x130 through its vtable; the main screen's Open)
    // 0x26fa55 / 0x26fa6a: the main group's mL1C = 0 / 1 and its mB25 (EEUIFMGStatusButton::Command's buttons 1 and
    // 0; defined in EEUIFMGStatusButton.cpp's range; provisional names)
    // Asks the main panel to refill its unit (clearing its flag)
    // (status_button_calls.rs)
    void Fn26FA55() { ee_screens_sbt_main_group_refill(false); }
    // Asks the main panel to refill its unit (setting its flag)
    // (status_button_calls.rs)
    void Fn26FA6A() { ee_screens_sbt_main_group_refill(true); }

    // the record's forms as their classes (+0x12c GetFormByID(0xb) in the ctor; +0x130 CreateScenarioEditor's)
    EEUIFScenarioEditor* GetScenarioEditorForm() const { return (EEUIFScenarioEditor*)this->mScenarioEditorForm; }
    EEUIFMainGame* GetMainGameForm() const { return (EEUIFMainGame*)this->mMainGameForm; }
    bool GetInScenarioEditor() const { return this->mInScenarioEditor; }
};

extern EEUIManager* gUIManager;

// the forms' ids and control indices are Rust enums (ee_game screens ui_ids.rs, rust/game/screens.h)

#endif
