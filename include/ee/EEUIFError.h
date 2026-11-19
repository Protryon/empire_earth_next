// C:\EEX\Empire Earth\User Interface Thread\EEUIFError.h (RECONSTRUCTION; file name inferred). The game's message
// box: once the form class EEUIFError (RTTI EEUIFError > UIForm; vtable 0x44a770) and its buttons EEUIFEBOK and
// EEUIFEBCancel, now kinds over the form and button shells (ee_game shells_dialog.rs; ee/EEUIShell.h); the plans
// and layouts are ee_screens dialog.rs'. EEUIFError.cpp keeps the box's host. Requires lle/UIForm.h and
// ee/EEUIManager.h.
#ifndef EE_EEUIFERROR_H
#define EE_EEUIFERROR_H

// what a button hands to the UI thread when the box closes (8 bytes, no ctor; provisional name)
struct EEUIFErrorCallbackData {
    EEUIFErrorCallback mCallback;           // +0x00
    void* mData;                            // +0x04
};

// A new message box shown as inKind (ee_screens dialog.rs EE_SCREENS_DIALOG_*: 0 a message with OK, shown when
// inFlag; 1 Yes/No; 2 Yes/No/Cancel; 3 a text, or a password when inFlag, which OK hands to inFirst; 4 Cancel only,
// closing inCloseForm); inFirst is OK's or Yes' or Cancel's callback, inSecond No's, each given inData (the
// original's DisplayMessage 0x261788, DisplayQuestion 0x261894, DisplayYesNoCancel 0x2619f9, DisplayPassword
// 0x26151a and DisplayCancel 0x26167f)
void EEUIFErrorDisplay(UIManager* inManager, unsigned long inKind, const UWideString& inMessage, unsigned long inAlignment,
                       void* inData, EEUIFErrorCallback inFirst, EEUIFErrorCallback inSecond, UIForm* inCloseForm,
                       bool inFlag);

#endif
