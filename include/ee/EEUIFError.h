// C:\EEX\Empire Earth\User Interface Thread\EEUIFError.h (RECONSTRUCTION; file name inferred).
// The game's message box form (RTTI EEUIFError > UIForm; vtable 0x44a770; sizeof 0x1bc: EEUIManager's `new`).
// EEUIFError.cpp is 0x260c27..0x2624a6 (its button classes EEUIFEBOK/EEUIFEBCancel live in the .cpp). Layout from the
// ctor 0x260ffb (its ESVERIFY strings name mOk/mNo/mCancel/mError/mEditBox) and the dtor 0x2614b7. The kinds' plans and
// layouts are Rust's (ee_screens dialog.rs). Requires lle/UIForm.h and ee/EEUIManager.h.
#ifndef EE_EEUIFERROR_H
#define EE_EEUIFERROR_H

class EEUIFEBOK;
class EEUIFEBCancel;
class EEUIBGenericBackground;
class UITextLabel;
class UIEditBox;

// the box's host for ee_screens dialog.rs (EEUIFError.cpp)
extern "C" uint32_t ee_screenscpp_dialog(void* inDialog, uint32_t inOp, uint32_t inA, uint32_t inB);

// what a button hands to the UI thread when the form closes (EEUIFError::Close news one: 8 bytes, no ctor;
// provisional name)
struct EEUIFErrorCallbackData {
    EEUIFErrorCallback mCallback;           // +0x00
    void* mData;                            // +0x04
};

class EEUIFError : public UIForm {
public:
    EEUIFError(UIManager* inManager);
    virtual ~EEUIFError();
    // shows the box as a kind (ee_screens dialog.rs EE_SCREENS_DIALOG_*: 0 a message with OK, shown when inFlag; 1
    // Yes/No; 2 Yes/No/Cancel; 3 a text, or a password when inFlag, which OK hands to inFirst; 4 Cancel only, closing
    // inCloseForm); inFirst is OK's or Yes' or Cancel's callback, inSecond No's, each given inData (the original's
    // DisplayMessage 0x261788, DisplayQuestion 0x261894, DisplayYesNoCancel 0x2619f9, DisplayPassword 0x26151a and
    // DisplayCancel 0x26167f)
    void Display(unsigned long inKind, const UWideString& inMessage, unsigned long inAlignment, void* inData,
                 EEUIFErrorCallback inFirst, EEUIFErrorCallback inSecond, UIForm* inCloseForm, bool inFlag);
protected:
    virtual void Close();                   // 0x261c4c (slot 0x38)
    virtual long DrawBackground(const GERectangle& inRectangle);   // 0x261baf (slot 0x18)
    virtual void Open();                    // 0x261bbc (slot 0x48)
private:
    friend uint32_t ee_screenscpp_dialog(void* inDialog, uint32_t inOp, uint32_t inA, uint32_t inB);   // its host
    EEUIFEBOK* mOk;                         // +0x1a0 (id 1)
    EEUIFEBCancel* mNo;                     // +0x1a4 (id 2)
    EEUIFEBCancel* mCancel;                 // +0x1a8 (id 3)
    UITextLabel* mError;                    // +0x1ac the message
    long mMode;                             // +0x1b0 0..4 (the Display* members)
    EEUIBGenericBackground* mBackground;    // +0x1b4
    UIEditBox* mEditBox;                    // +0x1b8 (id 4)
};

#endif
