// New (port; UI cluster round 3): the generic C++ shells of the game's control classes whose logic is Rust
// (ee_game screens shells.rs). One shell per toolkit base (a button, a radio button, a check box, a combo box, a
// string list box, an edit box, a slider): its virtuals ask the Rust kind (EE_SCREENS_SK_*) when the kind overrides
// them (its mask, EE_SCREENS_SV_* bits), else run the base's; the kind calls the base's back through BaseCall. The
// kind's own state and methods are Rust (EEUIShell::Op); the game's C++ it needs is ee_screenscpp_shell
// (EEUIShell.cpp). KindName is the class the kind replaces (the editor and civilization gates print it).
// Requires lle/GEColor.h, lle/GERhombus.h (the toolkit headers' own).
#ifndef EE_EEUISHELL_H
#define EE_EEUISHELL_H

#include <vector>
#include "rust/game/screens.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIRadioButton.h"
#include "lle/UICheckBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIEditBox.h"
#include "lle/UIComboBox.h"
#include "lle/UISlider.h"
#include "lle/UIForm.h"
#include "ee/EEUIRustControl.h"

// the scenario editor's diplomacy button state (EESEDiplomacyShow, ee/EESERBUnitTypes.h: its bitmaps and help text),
// which the shells' op EE_SCREENS_SH_SE_DIPLOMACY sets; EESERBUnitTypes.cpp sets the pointer at static
// initialization, so the shells' unit names no editor unit (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts, round 3")
inline void (*gEEUIShellDiplomacyShow)(UIButton* inButton, unsigned long inState, bool inHelp) = nullptr;

class EEUIShell {
public:
    // the shell of inControl (inForm), NULL for another control (form)
    static EEUIShell* Of(UIControl* inControl) { return dynamic_cast<EEUIShell*>(inControl); }
    static EEUIShell* Of(UIForm* inForm) { return dynamic_cast<EEUIShell*>(inForm); }
    // the kind's op on inControl (0 for a control that is not a shell)
    static int64_t Op(UIControl* inControl, uint32_t inOp, int64_t inA = 0, int64_t inB = 0, float* ioF = NULL)
    {
        EEUIShell* theShell = Of(inControl);
        return theShell ? ee_screens_shell_op(theShell->mState, inOp, inA, inB, ioF) : 0;
    }
    static int64_t Op(UIForm* inForm, uint32_t inOp, int64_t inA = 0, int64_t inB = 0, float* ioF = NULL)
    {
        EEUIShell* theShell = Of(inForm);
        return theShell ? ee_screens_shell_op(theShell->mState, inOp, inA, inB, ioF) : 0;
    }
    const char* KindName() const { return ee_screens_shell_name(this->mState); }
    // the base class's virtual inVirtual (EE_SCREENS_SV_*), for the kind
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF) = 0;
    // the shell's own virtual inVirtual (through its vtable: a kind's override runs), and its keyboard capture
    virtual int64_t SelfCall(uint32_t inVirtual) = 0;
    virtual void Keyboard(bool inCapture) = 0;
    virtual ~EEUIShell() { ee_screens_shell_free(this->mState); }
protected:
    // the kind's destructor body now (a most derived class whose kind still needs its own members)
    void Free()
    {
        ee_screens_shell_free(this->mState);
        this->mState = NULL;
        this->mOver = 0;
    }
    void Make(uint32_t inKind, void* inControl, const std::vector<int64_t>& inArgs)
    {
        this->mState = ee_screens_shell_new(inKind, inControl, inArgs.data(), (uint32_t)inArgs.size());
        this->mOver = ee_screens_shell_overrides(this->mState);
        ee_screens_shell_init(this->mState);
    }
    bool Over(uint32_t inVirtual) const { return ((this->mOver >> inVirtual) & 1) != 0; }
    int64_t Virt(uint32_t inVirtual, int64_t inA = 0, int64_t inB = 0, float* ioF = NULL) const
    {
        return ee_screens_shell_virtual(this->mState, inVirtual, inA, inB, ioF);
    }

    EeScreensShell* mState = NULL;
    uint64_t mOver = 0;
};

// one override: the kind's when it has one, else the base's
#define EE_SHELL_LONG(Base, Name, V) \
    virtual long Name() { return this->Over(V) ? (long)this->Virt(V) : Base::Name(); }
#define EE_SHELL_VOID(Base, Name, V) \
    virtual void Name() { if (this->Over(V)) this->Virt(V); else Base::Name(); }
// the overrides every shell has, and their base calls
#define EE_SHELL_COMMON(Base) \
    EE_SHELL_LONG(Base, CopyData, EE_SCREENS_SV_COPY_DATA) \
    EE_SHELL_LONG(Base, DrawPolygons, EE_SCREENS_SV_DRAW_POLYGONS) \
    EE_SHELL_LONG(Base, DrawTextA, EE_SCREENS_SV_DRAW_TEXT) \
    EE_SHELL_LONG(Base, MouseLButtonDown, EE_SCREENS_SV_MOUSE_L_DOWN) \
    EE_SHELL_LONG(Base, MouseLButtonUp, EE_SCREENS_SV_MOUSE_L_UP) \
    EE_SHELL_LONG(Base, MouseLButtonDoublePressed, EE_SCREENS_SV_MOUSE_L_DOUBLE) \
    EE_SHELL_LONG(Base, MouseRButtonDown, EE_SCREENS_SV_MOUSE_R_DOWN) \
    EE_SHELL_LONG(Base, MouseRButtonUp, EE_SCREENS_SV_MOUSE_R_UP) \
    EE_SHELL_LONG(Base, UpdateKeyboard, EE_SCREENS_SV_UPDATE_KEYBOARD) \
    EE_SHELL_VOID(Base, TabbedTo, EE_SCREENS_SV_TABBED_TO) \
    EE_SHELL_VOID(Base, TabbedFrom, EE_SCREENS_SV_TABBED_FROM) \
    virtual long MouseMove(const GE2DPoint& inLocation) \
    { \
        float f[4] = {inLocation.GetX(), inLocation.GetY(), 0.0f, 0.0f}; \
        return this->Over(EE_SCREENS_SV_MOUSE_MOVE) ? (long)this->Virt(EE_SCREENS_SV_MOUSE_MOVE, 0, 0, f) \
                                                   : Base::MouseMove(inLocation); \
    } \
    virtual void SetDirty(bool inDirty) \
    { \
        if (this->Over(EE_SCREENS_SV_SET_DIRTY)) this->Virt(EE_SCREENS_SV_SET_DIRTY, inDirty); \
        else Base::SetDirty(inDirty); \
    } \
    virtual bool GetTabable() const \
    { \
        return this->Over(EE_SCREENS_SV_GET_TABABLE) ? this->Virt(EE_SCREENS_SV_GET_TABABLE) != 0 : Base::GetTabable(); \
    } \
    virtual bool GetHelpString(UWideString& outString) const \
    { \
        return this->Over(EE_SCREENS_SV_HELP_STRING) ? this->Virt(EE_SCREENS_SV_HELP_STRING, (int64_t)&outString) != 0 \
                                                    : Base::GetHelpString(outString); \
    }
#define EE_SHELL_COMMON_BASE(Base) \
    case EE_SCREENS_SV_COPY_DATA: return Base::CopyData(); \
    case EE_SCREENS_SV_DRAW_POLYGONS: return Base::DrawPolygons(); \
    case EE_SCREENS_SV_DRAW_TEXT: return Base::DrawTextA(); \
    case EE_SCREENS_SV_MOUSE_L_DOWN: return Base::MouseLButtonDown(); \
    case EE_SCREENS_SV_MOUSE_L_UP: return Base::MouseLButtonUp(); \
    case EE_SCREENS_SV_MOUSE_L_DOUBLE: return Base::MouseLButtonDoublePressed(); \
    case EE_SCREENS_SV_MOUSE_R_DOWN: return Base::MouseRButtonDown(); \
    case EE_SCREENS_SV_MOUSE_R_UP: return Base::MouseRButtonUp(); \
    case EE_SCREENS_SV_UPDATE_KEYBOARD: return Base::UpdateKeyboard(); \
    case EE_SCREENS_SV_TABBED_TO: Base::TabbedTo(); return 0; \
    case EE_SCREENS_SV_TABBED_FROM: Base::TabbedFrom(); return 0; \
    case EE_SCREENS_SV_MOUSE_MOVE: return Base::MouseMove(GE2DPoint(ioF[0], ioF[1])); \
    case EE_SCREENS_SV_SET_DIRTY: Base::SetDirty(inA != 0); return 0; \
    case EE_SCREENS_SV_GET_TABABLE: return Base::GetTabable(); \
    case EE_SCREENS_SV_HELP_STRING: return Base::GetHelpString(*reinterpret_cast<UWideString*>(inA));
// a button's own three
#define EE_SHELL_BUTTON(Base) \
    EE_SHELL_VOID(Base, ButtonDown, EE_SCREENS_SV_BUTTON_DOWN) \
    EE_SHELL_VOID(Base, ButtonUp, EE_SCREENS_SV_BUTTON_UP) \
    EE_SHELL_VOID(Base, MouseOver, EE_SCREENS_SV_MOUSE_OVER)
#define EE_SHELL_BUTTON_BASE(Base) \
    case EE_SCREENS_SV_BUTTON_DOWN: Base::ButtonDown(); return 0; \
    case EE_SCREENS_SV_BUTTON_UP: Base::ButtonUp(); return 0; \
    case EE_SCREENS_SV_MOUSE_OVER: Base::MouseOver(); return 0;

// the shell's own virtuals (SelfCall: UpdateKeyboard, and a button's ButtonDown and ButtonUp) and its keyboard capture
#define EE_SHELL_SELF_BODY(Extra)     virtual int64_t SelfCall(uint32_t inVirtual)     {         switch (inVirtual) {         case EE_SCREENS_SV_UPDATE_KEYBOARD: return this->UpdateKeyboard();         Extra         }         return 0;     }     virtual void Keyboard(bool inCapture)     {         if (inCapture) this->CaptureKeyboard();         else this->ReleaseCapturedKeyboard();     }
#define EE_SHELL_SELF EE_SHELL_SELF_BODY()
#define EE_SHELL_SELF_BUTTON     EE_SHELL_SELF_BODY(case EE_SCREENS_SV_BUTTON_DOWN: this->ButtonDown(); return 0;                        case EE_SCREENS_SV_BUTTON_UP: this->ButtonUp(); return 0;)

class EEUIShellButton : public UIButton, public EEUIShell {
public:
    EEUIShellButton(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, GEBitmap* inNormal,
                    GEBitmap* inFocused, GEBitmap* inPressed, const GERectangle& inRectangle, UIForm* inForm,
                    unsigned long inIndex, unsigned long inBorderThickness)
        : UIButton(inId, inNormal, inFocused, inPressed, inRectangle, inForm, inIndex, inBorderThickness)
    {
        this->Make(inKind, this, inArgs);
    }
    EE_SHELL_COMMON(UIButton)
    EE_SHELL_BUTTON(UIButton)
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    EE_SHELL_SELF_BUTTON
};

class EEUIShellRadio : public UIRadioButton, public EEUIShell {
public:
    EEUIShellRadio(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, const UWideString& inText,
                   GEFont* inFont, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                   unsigned long inBorderThickness, bool inIsPushButton, bool inIsChecked, bool inScaleBitmap)
        : UIRadioButton(inId, inText, inFont, inRectangle, inForm, inIndex, inBorderThickness, inIsPushButton,
                        inIsChecked, inScaleBitmap)
    {
        this->Make(inKind, this, inArgs);
    }
    EE_SHELL_COMMON(UIRadioButton)
    EE_SHELL_BUTTON(UIRadioButton)
    EE_SHELL_VOID(UIRadioButton, Check, EE_SCREENS_SV_CHECK)
    EE_SHELL_VOID(UIRadioButton, UnCheck, EE_SCREENS_SV_UNCHECK)
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    EE_SHELL_SELF_BUTTON
};

class EEUIShellCheck : public UICheckBox, public EEUIShell {
public:
    EEUIShellCheck(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, const UWideString& inText,
                   GEFont* inFont, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                   unsigned long inBorderThickness, bool inScaleBitmap)
        : UICheckBox(inId, inText, inFont, inRectangle, inForm, inIndex, inBorderThickness, inScaleBitmap)
    {
        this->Make(inKind, this, inArgs);
    }
    EE_SHELL_COMMON(UICheckBox)
    EE_SHELL_BUTTON(UICheckBox)
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    EE_SHELL_SELF_BUTTON
};

class EEUIShellCombo : public UIComboBox, public EEUIShell {
public:
    EEUIShellCombo(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, unsigned long inTextBoxType,
                   const std::vector<UWideString>& inData, GEFont* inFont, const GERectangle& inRectangle,
                   unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars,
                   unsigned long inBorderThickness)
        : UIComboBox(inId, inTextBoxType, inData, inFont, inRectangle, inMaxListBoxHeight, inForm, inIndex, inMaxChars,
                     inBorderThickness)
    {
        this->Make(inKind, this, inArgs);
    }
    unsigned long Selection() const { return this->GetSelectionIndex(); }
    EE_SHELL_COMMON(UIComboBox)
    EE_SHELL_VOID(UIComboBox, ChangeSelection, EE_SCREENS_SV_CHANGE_SELECTION)
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    EE_SHELL_SELF
};

class EEUIShellList : public UILBString, public EEUIShell {
public:
    EEUIShellList(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId,
                  const std::vector<UWideString>& inData, GEFont* inFont, bool inAutoSelect,
                  const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UILBString(inId, inData, inFont, inAutoSelect, inRectangle, inForm, inIndex, inBorderThickness)
    {
        this->Make(inKind, this, inArgs);
    }
    // EEUIFMGErrorLog's (the main form's error strings)
    void SetLine(unsigned long inIndex, const UWideString& inLine) { this->mData[inIndex] = inLine; this->SetDirty(true); }
    // the visible lines (first | end << 32: EESEUITechListBox::CopyData's range)
    int64_t View()
    {
        if (this->ScrollBarExists())
            return (int64_t)this->GetDisplayLine() | (int64_t)(this->GetNumberLinesDisplayed() + this->GetDisplayLine()) << 32;
        return (int64_t)this->GetNumLines() << 32;
    }
    // EESEUITechListBox::DrawPolygons' blue bars (shell_factories.rs draw_bars)
    using UILBString::GetCopyTextRectangle;
    using UILBString::GetLineHeight;
    EE_SHELL_COMMON(UILBString)
    EE_SHELL_VOID(UILBString, ButtonUp, EE_SCREENS_SV_BUTTON_UP)
    virtual void ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
    {
        if (this->Over(EE_SCREENS_SV_CHANGE_SELECTION_INDEX))
            this->Virt(EE_SCREENS_SV_CHANGE_SELECTION_INDEX, inIndex, (inFireEvent ? 1 : 0) | (inScrollTo ? 2 : 0));
        else
            UILBString::ChangeSelectionIndex(inIndex, inFireEvent, inScrollTo);
    }
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    EE_SHELL_SELF
    static bool EnterKey() { return UIControl::sEnterKey->GetValue() != 0; }
};

class EEUIShellEdit : public UIEditBox, public EEUIShell {
public:
    EEUIShellEdit(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, GEFont* inFont,
                  unsigned long inMaxChars, bool inNumbersOnly, const GERectangle& inRectangle, UIForm* inForm,
                  unsigned long inIndex, unsigned long inBorderThickness, bool inMultiLine, bool inPasswordBox)
        : UIEditBox(inId, inFont, inMaxChars, inNumbersOnly, inRectangle, inForm, inIndex, inBorderThickness,
                    inMultiLine, inPasswordBox)
    {
        this->Make(inKind, this, inArgs);
    }
    EE_SHELL_COMMON(UIEditBox)
    EE_SHELL_VOID(UIEditBox, ProcessEnterKey, EE_SCREENS_SV_PROCESS_ENTER)
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    EE_SHELL_SELF
};

class EEUIShellSlider : public UISlider, public EEUIShell {
public:
    EEUIShellSlider(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId,
                    const GERectangle& inRectangle, unsigned long inSliderStyle, UIForm* inForm, unsigned long inIndex,
                    unsigned long inBorderThickness)
        : UISlider(inId, inRectangle, inSliderStyle, inForm, inIndex, inBorderThickness)
    {
        this->Make(inKind, this, inArgs);
    }
    EE_SHELL_COMMON(UISlider)
    virtual void ChangeRange(unsigned long inValue)
    {
        if (this->Over(EE_SCREENS_SV_CHANGE_RANGE)) this->Virt(EE_SCREENS_SV_CHANGE_RANGE, inValue);
        else UISlider::ChangeRange(inValue);
    }
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    EE_SHELL_SELF
};

// a plain control (UIControl, its pure virtuals EEUIRustControl's: nothing, 0): the paint areas, the icon bar. Its
// slots hold the C++ objects a kind's host cases need (the icon bar's bitmaps and sounds); EEUIRustControl's class
// ops are Rust's (the icon bar's: icon_bar_ops.rs over EEUIShell.cpp's shims)
class EEUIShellControl : public EEUIRustControl, public EEUIShell {
public:
    EEUIShellControl(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId,
                     const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : EEUIRustControl(inId, inRectangle, inForm, inIndex, inBorderThickness), mKind(inKind)
    {
        this->Make(inKind, this, inArgs);
    }
    virtual ~EEUIShellControl() { this->Free(); }   // the kind's release still reaches the slots and RustOp
    uint32_t mKind;
    void* mSlots[4] = {};
    EE_SHELL_COMMON(EEUIRustControl)
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    EE_SHELL_SELF
    const GE2DPoint& MouseLocation() const { return this->GetMouseLocation(); }
    void Capture() { this->CaptureMouse(); }
protected:
};

// a form (UIForm): its virtuals the kind's when it overrides them; a kind reaches the form's base calls and its own
// virtuals through EE_SCREENS_SH_FORM_BASE + v and EE_SCREENS_SH_FORM_SELF (the host's object is the UIForm*)
#define EE_SHELL_RECT(Base, Name, V, Ret, Back) \
    virtual Ret Name(const GERectangle& inRectangle) \
    { \
        float f[4] = {inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(), inRectangle.GetBottom()}; \
        if (this->Over(V)) return (Ret)this->Virt(V, 0, 0, f); \
        Back Base::Name(inRectangle); \
    }
class EEUIShellForm : public UIForm, public EEUIShell {
public:
    EEUIShellForm(uint32_t inKind, const std::vector<int64_t>& inArgs, UIManager* inManager, const UIDbDataForms& inData,
                  bool inUseGDIForText)
        : UIForm(inManager, inData, inUseGDIForText)
    {
        this->Make(inKind, static_cast<UIForm*>(this), inArgs);
    }
    EEUIShellForm(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inControlCount,
                  unsigned long inControlGroupCount, UIManager* inManager, unsigned long inFormID, bool inUseGDIForText)
        : UIForm(inControlCount, inControlGroupCount, inManager, inFormID, inUseGDIForText)
    {
        this->Make(inKind, static_cast<UIForm*>(this), inArgs);
    }
    EE_SHELL_VOID(UIForm, Activate, EE_SCREENS_SV_ACTIVATE)
    EE_SHELL_VOID(UIForm, Deactivate, EE_SCREENS_SV_DEACTIVATE)
    EE_SHELL_VOID(UIForm, SetHelpTextPopupLocation, EE_SCREENS_SV_HELP_POPUP)
    EE_SHELL_LONG(UIForm, UpdateKeyboard, EE_SCREENS_SV_UPDATE_KEYBOARD)
    EE_SHELL_LONG(UIForm, UpdateMouse, EE_SCREENS_SV_UPDATE_MOUSE)
    EE_SHELL_RECT(UIForm, RelayoutForm, EE_SCREENS_SV_RELAYOUT, void, return)
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat)
    {
        return this->Over(EE_SCREENS_SV_TRANSLATED_KEY) ? (long)this->Virt(EE_SCREENS_SV_TRANSLATED_KEY, (int64_t)inKey, inRepeat)
                                                        : UIForm::UpdateTranslatedKey(inKey, inRepeat);
    }
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF);
    virtual int64_t SelfCall(uint32_t inVirtual);
    virtual void Keyboard(bool inCapture) {}
protected:
    EE_SHELL_RECT(UIForm, DrawBackground, EE_SCREENS_SV_DRAW_BACKGROUND, long, return)
    EE_SHELL_VOID(UIForm, Close, EE_SCREENS_SV_CLOSE)
    EE_SHELL_LONG(UIForm, CopyData, EE_SCREENS_SV_COPY_DATA)
    EE_SHELL_LONG(UIForm, DrawPolygons, EE_SCREENS_SV_DRAW_POLYGONS)
    EE_SHELL_LONG(UIForm, DrawTextA, EE_SCREENS_SV_DRAW_TEXT)
    EE_SHELL_VOID(UIForm, Open, EE_SCREENS_SV_OPEN)
    EE_SHELL_RECT(UIForm, SetRectangle, EE_SCREENS_SV_SET_RECTANGLE, void, return)
};

class UIControlsTable;

// the file combo box (EEUIFileComboBox, shells_files.rs): "none", then inPath's files matching inPattern without their
// extensions; CreateEntries refills it, SelectFile selects the line of a name (else "none": kErrorNotFound)
class FSPath;
UIComboBox* EEUIShellNewFileCombo(FSPath* inPath, const UString& inPattern, unsigned long inId, GEFont* inFont,
                                  const GE2DPoint& inLocation, unsigned long inWidth, UIForm* inForm, unsigned long inIndex);
inline void EEUIFileComboEntries(UIControl* inList) { EEUIShell::Op(inList, EE_SCREENS_SKOP_FILE_ENTRIES); }
inline long EEUIFileComboSelect(UIControl* inList, const UWideString& inName)
{
    return (long)EEUIShell::Op(inList, EE_SCREENS_SKOP_FILE_SELECT, (int64_t)&inName);
}

#endif
