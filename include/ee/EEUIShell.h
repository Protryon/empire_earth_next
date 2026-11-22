// New (port; UI cluster round 3): the generic C++ shells of the game's control classes whose logic is Rust
// (ee_game screens shells.rs). One shell per toolkit base (a button, a radio button, a check box, a combo box, a
// string list box, an edit box, a slider): its virtuals ask the Rust kind (EE_SCREENS_SK_*) when the kind overrides
// them (its mask, EE_SCREENS_SV_* bits), else run the base's; the kind calls the base's back through BaseCall. The
// kind's own state and methods are Rust (EEUIShell::Op); the game's C++ it needs is ee_screenscpp_shell
// (EEUIShell.cpp). KindName is the class the kind replaces (the editor and civilization gates print it).
// Requires lle/GEColor.h, lle/GERhombus.h (the toolkit headers' own).
#ifndef EE_EEUISHELL_H
#define EE_EEUISHELL_H

#include <utility>
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
    // the kind's op on inControl (0 for a control that is not a shell: Rust finds the shell by the control's address,
    // the one each shell registered at its Make)
    static int64_t Op(UIControl* inControl, uint32_t inOp, int64_t inA = 0, int64_t inB = 0, float* ioF = NULL)
    {
        return ee_screens_shell_op_at(inControl, inOp, inA, inB, ioF);
    }
    static int64_t Op(UIForm* inForm, uint32_t inOp, int64_t inA = 0, int64_t inB = 0, float* ioF = NULL)
    {
        return ee_screens_shell_op_at(inForm, inOp, inA, inB, ioF);
    }
    const char* KindName() const { return ee_screens_shell_name(this->mState); }
    // the base class's virtual inVirtual (EE_SCREENS_SV_*), for the kind
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF) = 0;
    // the shell's own virtual inVirtual (through its vtable: a kind's override runs), and its keyboard capture
    virtual int64_t SelfCall(uint32_t inVirtual) = 0;
    virtual void Keyboard(bool inCapture) = 0;
    // the control's text (inWhich 0: its own GEText, 1: its label's; a button's, a combo box's, a list's, an edit box's)
    virtual GEText* Text(int64_t inWhich) { return NULL; }
    virtual ~EEUIShell() { ee_screens_shell_free(this->mState); }
protected:
    // the kind's destructor body now (a most derived class whose kind still needs its own members)
    void Free() { ee_screens_shell_free(std::exchange(this->mState, nullptr)); }
    // the kind made, its override mask read and its constructor body run (ee_screens_shell_make)
    void Make(uint32_t inKind, void* inControl, const std::vector<int64_t>& inArgs)
    {
        ee_screens_shell_make(&this->mState, &this->mOver, inKind, inControl, inArgs.data(), (uint32_t)inArgs.size());
    }
    bool Over(uint32_t inVirtual) const { return ((this->mOver >> inVirtual) & 1) != 0; }
    int64_t Virt(uint32_t inVirtual, int64_t inA = 0, int64_t inB = 0, float* ioF = NULL) const
    {
        return ee_screens_shell_virtual(this->mState, inVirtual, inA, inB, ioF);
    }
    // a point's or a rectangle's floats for Virt (x, y; left, top, right, bottom), and a rectangle from them
    struct F4 { float f[4]; };
    static F4 PointF(const GE2DPoint& inPoint) { return {{inPoint.GetX(), inPoint.GetY(), 0.0f, 0.0f}}; }
    static F4 RectF(const GERectangle& inR) { return {{inR.GetLeft(), inR.GetTop(), inR.GetRight(), inR.GetBottom()}}; }
    static GERectangle Rect(const float* inF) { return inF ? GERectangle(inF[0], inF[1], inF[2], inF[3]) : GERectangle(); }

    EeScreensShell* mState = NULL;
    uint64_t mOver = 0;
};

// The shell over the toolkit control B (D the most derived shell, whose constructor makes the kind): every override
// is the kind's when its mask has it, else B's. BaseCall is B's virtual for the kind; a shell's own virtuals beyond
// these are D's, D::Extra their base (inSelf false) and vtable (inSelf true) calls.
#define EE_SHELL_IS(V) this->Over(EE_SCREENS_SV_##V)
#define EE_SHELL_TO(V, ...) this->Virt(EE_SCREENS_SV_##V, ##__VA_ARGS__)
template <class B, class D> class EEUIShellOf : public B, public EEUIShell {
public:
    using B::B;
    virtual long CopyData() { return EE_SHELL_IS(COPY_DATA) ? (long)EE_SHELL_TO(COPY_DATA) : B::CopyData(); }
    virtual long DrawPolygons() { return EE_SHELL_IS(DRAW_POLYGONS) ? (long)EE_SHELL_TO(DRAW_POLYGONS) : B::DrawPolygons(); }
    virtual long DrawTextA() { return EE_SHELL_IS(DRAW_TEXT) ? (long)EE_SHELL_TO(DRAW_TEXT) : B::DrawTextA(); }
    virtual long MouseLButtonDown() { return EE_SHELL_IS(MOUSE_L_DOWN) ? (long)EE_SHELL_TO(MOUSE_L_DOWN) : B::MouseLButtonDown(); }
    virtual long MouseLButtonUp() { return EE_SHELL_IS(MOUSE_L_UP) ? (long)EE_SHELL_TO(MOUSE_L_UP) : B::MouseLButtonUp(); }
    virtual long MouseLButtonDoublePressed()
    {
        return EE_SHELL_IS(MOUSE_L_DOUBLE) ? (long)EE_SHELL_TO(MOUSE_L_DOUBLE) : B::MouseLButtonDoublePressed();
    }
    virtual long MouseRButtonDown() { return EE_SHELL_IS(MOUSE_R_DOWN) ? (long)EE_SHELL_TO(MOUSE_R_DOWN) : B::MouseRButtonDown(); }
    virtual long MouseRButtonUp() { return EE_SHELL_IS(MOUSE_R_UP) ? (long)EE_SHELL_TO(MOUSE_R_UP) : B::MouseRButtonUp(); }
    virtual long UpdateKeyboard()
    {
        return EE_SHELL_IS(UPDATE_KEYBOARD) ? (long)EE_SHELL_TO(UPDATE_KEYBOARD) : B::UpdateKeyboard();
    }
    virtual void TabbedTo() { EE_SHELL_IS(TABBED_TO) ? (void)EE_SHELL_TO(TABBED_TO) : B::TabbedTo(); }
    virtual void TabbedFrom() { EE_SHELL_IS(TABBED_FROM) ? (void)EE_SHELL_TO(TABBED_FROM) : B::TabbedFrom(); }
    virtual long MouseMove(const GE2DPoint& inLocation)
    {
        return EE_SHELL_IS(MOUSE_MOVE) ? (long)EE_SHELL_TO(MOUSE_MOVE, 0, 0, PointF(inLocation).f) : B::MouseMove(inLocation);
    }
    virtual void SetDirty(bool inDirty) { EE_SHELL_IS(SET_DIRTY) ? (void)EE_SHELL_TO(SET_DIRTY, inDirty) : B::SetDirty(inDirty); }
    virtual bool GetTabable() const { return EE_SHELL_IS(GET_TABABLE) ? EE_SHELL_TO(GET_TABABLE) != 0 : B::GetTabable(); }
    virtual bool GetHelpString(UWideString& outString) const
    {
        return EE_SHELL_IS(HELP_STRING) ? EE_SHELL_TO(HELP_STRING, (int64_t)&outString) != 0 : B::GetHelpString(outString);
    }
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
    {
        switch (inVirtual) {
        case EE_SCREENS_SV_COPY_DATA: return B::CopyData();
        case EE_SCREENS_SV_DRAW_POLYGONS: return B::DrawPolygons();
        case EE_SCREENS_SV_DRAW_TEXT: return B::DrawTextA();
        case EE_SCREENS_SV_MOUSE_L_DOWN: return B::MouseLButtonDown();
        case EE_SCREENS_SV_MOUSE_L_UP: return B::MouseLButtonUp();
        case EE_SCREENS_SV_MOUSE_L_DOUBLE: return B::MouseLButtonDoublePressed();
        case EE_SCREENS_SV_MOUSE_R_DOWN: return B::MouseRButtonDown();
        case EE_SCREENS_SV_MOUSE_R_UP: return B::MouseRButtonUp();
        case EE_SCREENS_SV_UPDATE_KEYBOARD: return B::UpdateKeyboard();
        case EE_SCREENS_SV_TABBED_TO: B::TabbedTo(); return 0;
        case EE_SCREENS_SV_TABBED_FROM: B::TabbedFrom(); return 0;
        case EE_SCREENS_SV_MOUSE_MOVE: return B::MouseMove(GE2DPoint(ioF[0], ioF[1]));
        case EE_SCREENS_SV_SET_DIRTY: B::SetDirty(inA != 0); return 0;
        case EE_SCREENS_SV_GET_TABABLE: return B::GetTabable();
        case EE_SCREENS_SV_HELP_STRING: return B::GetHelpString(*reinterpret_cast<UWideString*>(inA));
        default: return static_cast<D*>(this)->Extra(inVirtual, inA, inB, false);
        }
    }
    virtual int64_t SelfCall(uint32_t inVirtual)
    {
        return inVirtual == EE_SCREENS_SV_UPDATE_KEYBOARD ? this->UpdateKeyboard()
                                                          : static_cast<D*>(this)->Extra(inVirtual, 0, 0, true);
    }
    virtual void Keyboard(bool inCapture) { inCapture ? this->CaptureKeyboard() : this->ReleaseCapturedKeyboard(); }
    int64_t Extra(uint32_t, int64_t, int64_t, bool) { return 0; }
};

// a button's own three (UIButton, UICheckBox, UIRadioButton)
template <class B, class D> class EEUIShellButtonOf : public EEUIShellOf<B, D> {
public:
    using EEUIShellOf<B, D>::EEUIShellOf;
    virtual void ButtonDown() { EE_SHELL_IS(BUTTON_DOWN) ? (void)EE_SHELL_TO(BUTTON_DOWN) : B::ButtonDown(); }
    virtual void ButtonUp() { EE_SHELL_IS(BUTTON_UP) ? (void)EE_SHELL_TO(BUTTON_UP) : B::ButtonUp(); }
    virtual void MouseOver() { EE_SHELL_IS(MOUSE_OVER) ? (void)EE_SHELL_TO(MOUSE_OVER) : B::MouseOver(); }
    int64_t Extra(uint32_t inVirtual, int64_t inA, int64_t inB, bool inSelf)
    {
        switch (inVirtual) {
        case EE_SCREENS_SV_BUTTON_DOWN: inSelf ? this->ButtonDown() : B::ButtonDown(); return 0;
        case EE_SCREENS_SV_BUTTON_UP: inSelf ? this->ButtonUp() : B::ButtonUp(); return 0;
        case EE_SCREENS_SV_MOUSE_OVER: inSelf ? (void)0 : B::MouseOver(); return 0;
        default: return 0;
        }
    }
};

class EEUIShellButton : public EEUIShellButtonOf<UIButton, EEUIShellButton> {
public:
    EEUIShellButton(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, GEBitmap* inNormal,
                    GEBitmap* inFocused, GEBitmap* inPressed, const GERectangle& inRectangle, UIForm* inForm,
                    unsigned long inIndex, unsigned long inBorderThickness)
        : EEUIShellButtonOf(inId, inNormal, inFocused, inPressed, inRectangle, inForm, inIndex, inBorderThickness)
    {
        this->Make(inKind, static_cast<UIButton*>(this), inArgs);
    }
    virtual GEText* Text(int64_t) { return this->GetGEText(); }
};

class EEUIShellRadio : public EEUIShellButtonOf<UIRadioButton, EEUIShellRadio> {
public:
    EEUIShellRadio(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, const UWideString& inText,
                   GEFont* inFont, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                   unsigned long inBorderThickness, bool inIsPushButton, bool inIsChecked, bool inScaleBitmap)
        : EEUIShellButtonOf(inId, inText, inFont, inRectangle, inForm, inIndex, inBorderThickness, inIsPushButton,
                            inIsChecked, inScaleBitmap)
    {
        this->Make(inKind, static_cast<UIRadioButton*>(this), inArgs);
    }
    virtual GEText* Text(int64_t inWhich) { return inWhich ? this->GetTextLabel()->GetGEText() : this->GetGEText(); }
    virtual void Check() { EE_SHELL_IS(CHECK) ? (void)EE_SHELL_TO(CHECK) : UIRadioButton::Check(); }
    virtual void UnCheck() { EE_SHELL_IS(UNCHECK) ? (void)EE_SHELL_TO(UNCHECK) : UIRadioButton::UnCheck(); }
    int64_t Extra(uint32_t inVirtual, int64_t inA, int64_t inB, bool inSelf)
    {
        switch (inVirtual) {
        case EE_SCREENS_SV_CHECK: inSelf ? (void)0 : UIRadioButton::Check(); return 0;
        case EE_SCREENS_SV_UNCHECK: inSelf ? (void)0 : UIRadioButton::UnCheck(); return 0;
        default: return EEUIShellButtonOf::Extra(inVirtual, inA, inB, inSelf);
        }
    }
};

class EEUIShellCheck : public EEUIShellButtonOf<UICheckBox, EEUIShellCheck> {
public:
    EEUIShellCheck(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, const UWideString& inText,
                   GEFont* inFont, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                   unsigned long inBorderThickness, bool inScaleBitmap)
        : EEUIShellButtonOf(inId, inText, inFont, inRectangle, inForm, inIndex, inBorderThickness, inScaleBitmap)
    {
        this->Make(inKind, static_cast<UICheckBox*>(this), inArgs);
    }
    virtual GEText* Text(int64_t inWhich) { return inWhich ? this->GetTextLabel()->GetGEText() : this->GetGEText(); }
};

class EEUIShellCombo : public EEUIShellOf<UIComboBox, EEUIShellCombo> {
public:
    EEUIShellCombo(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, unsigned long inTextBoxType,
                   const std::vector<UWideString>& inData, GEFont* inFont, const GERectangle& inRectangle,
                   unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars,
                   unsigned long inBorderThickness)
        : EEUIShellOf(inId, inTextBoxType, inData, inFont, inRectangle, inMaxListBoxHeight, inForm, inIndex, inMaxChars,
                      inBorderThickness)
    {
        this->Make(inKind, static_cast<UIComboBox*>(this), inArgs);
    }
    unsigned long Selection() const { return this->GetSelectionIndex(); }
    virtual void ChangeSelection()
    {
        EE_SHELL_IS(CHANGE_SELECTION) ? (void)EE_SHELL_TO(CHANGE_SELECTION) : UIComboBox::ChangeSelection();
    }
    virtual GEText* Text(int64_t) { return this->GetGEText(); }
    int64_t Extra(uint32_t inVirtual, int64_t, int64_t, bool inSelf)
    {
        return inVirtual == EE_SCREENS_SV_CHANGE_SELECTION && !inSelf ? (UIComboBox::ChangeSelection(), 0) : 0;
    }
};

class EEUIShellList : public EEUIShellOf<UILBString, EEUIShellList> {
public:
    EEUIShellList(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId,
                  const std::vector<UWideString>& inData, GEFont* inFont, bool inAutoSelect,
                  const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : EEUIShellOf(inId, inData, inFont, inAutoSelect, inRectangle, inForm, inIndex, inBorderThickness)
    {
        this->Make(inKind, static_cast<UILBString*>(this), inArgs);
    }
    // EEUIFMGErrorLog's (the main form's error strings)
    void SetLine(unsigned long inIndex, const UWideString& inLine) { (this->mData[inIndex] = inLine), this->SetDirty(true); }
    // the visible lines (first | end << 32: EESEUITechListBox::CopyData's range)
    int64_t View()
    {
        return this->ScrollBarExists() ? (int64_t)this->GetDisplayLine() |
                                             (int64_t)(this->GetNumberLinesDisplayed() + this->GetDisplayLine()) << 32
                                       : (int64_t)this->GetNumLines() << 32;
    }
    virtual void ButtonUp() { EE_SHELL_IS(BUTTON_UP) ? (void)EE_SHELL_TO(BUTTON_UP) : UILBString::ButtonUp(); }
    virtual void ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
    {
        EE_SHELL_IS(CHANGE_SELECTION_INDEX)
            ? (void)EE_SHELL_TO(CHANGE_SELECTION_INDEX, inIndex, (inFireEvent ? 1 : 0) | (inScrollTo ? 2 : 0))
            : UILBString::ChangeSelectionIndex(inIndex, inFireEvent, inScrollTo);
    }
    virtual GEText* Text(int64_t) { return this->GetGEText(); }
    int64_t Extra(uint32_t inVirtual, int64_t inA, int64_t inB, bool inSelf)
    {
        switch (inSelf ? 0 : inVirtual) {
        case EE_SCREENS_SV_BUTTON_UP: UILBString::ButtonUp(); return 0;
        case EE_SCREENS_SV_CHANGE_SELECTION_INDEX:
            UILBString::ChangeSelectionIndex((unsigned long)inA, (inB & 1) != 0, (inB & 2) != 0);
            return 0;
        default: return 0;
        }
    }
    static bool EnterKey() { return UIControl::Key(EeUiKey::Enter)->GetValue() != 0; }
};

class EEUIShellEdit : public EEUIShellOf<UIEditBox, EEUIShellEdit> {
public:
    EEUIShellEdit(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId, GEFont* inFont,
                  unsigned long inMaxChars, bool inNumbersOnly, const GERectangle& inRectangle, UIForm* inForm,
                  unsigned long inIndex, unsigned long inBorderThickness, bool inMultiLine, bool inPasswordBox)
        : EEUIShellOf(inId, inFont, inMaxChars, inNumbersOnly, inRectangle, inForm, inIndex, inBorderThickness,
                      inMultiLine, inPasswordBox)
    {
        this->Make(inKind, static_cast<UIEditBox*>(this), inArgs);
    }
    virtual void ProcessEnterKey()
    {
        EE_SHELL_IS(PROCESS_ENTER) ? (void)EE_SHELL_TO(PROCESS_ENTER) : UIEditBox::ProcessEnterKey();
    }
    virtual GEText* Text(int64_t) { return this->GetGEText(); }
    int64_t Extra(uint32_t inVirtual, int64_t, int64_t, bool inSelf)
    {
        return inVirtual == EE_SCREENS_SV_PROCESS_ENTER && !inSelf ? (UIEditBox::ProcessEnterKey(), 0) : 0;
    }
};

class EEUIShellSlider : public EEUIShellOf<UISlider, EEUIShellSlider> {
public:
    EEUIShellSlider(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId,
                    const GERectangle& inRectangle, unsigned long inSliderStyle, UIForm* inForm, unsigned long inIndex,
                    unsigned long inBorderThickness)
        : EEUIShellOf(inId, inRectangle, inSliderStyle, inForm, inIndex, inBorderThickness)
    {
        this->Make(inKind, static_cast<UISlider*>(this), inArgs);
    }
    virtual void ChangeRange(unsigned long inValue)
    {
        EE_SHELL_IS(CHANGE_RANGE) ? (void)EE_SHELL_TO(CHANGE_RANGE, inValue) : UISlider::ChangeRange(inValue);
    }
    int64_t Extra(uint32_t inVirtual, int64_t inA, int64_t, bool inSelf)
    {
        return inVirtual == EE_SCREENS_SV_CHANGE_RANGE && !inSelf ? (UISlider::ChangeRange((unsigned long)inA), 0) : 0;
    }
};

// a plain control (UIControl, its pure virtuals EEUIRustControl's: nothing, 0): the paint areas, the icon bar. Its
// slots hold the C++ objects a kind's host cases need (the icon bar's bitmaps and sounds); EEUIRustControl's class
// ops are Rust's (the icon bar's: icon_bar_ops.rs over EEUIShell.cpp's shims)
class EEUIShellControl : public EEUIShellOf<EEUIRustControl, EEUIShellControl> {
public:
    EEUIShellControl(uint32_t inKind, const std::vector<int64_t>& inArgs, unsigned long inId,
                     const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : EEUIShellOf(inId, inRectangle, inForm, inIndex, inBorderThickness)
    {
        this->Make(inKind, static_cast<EEUIRustControl*>(this), inArgs);
    }
    virtual ~EEUIShellControl() { this->Free(); }   // the kind's release still reaches the slots and RustOp
    void* mSlots[4] = {};
    const GE2DPoint& MouseLocation() const { return this->GetMouseLocation(); }
    void Capture() { this->CaptureMouse(); }
};

// a form (UIForm): its virtuals the kind's when it overrides them; a kind reaches the form's base calls and its own
// virtuals through EE_SCREENS_SH_FORM_BASE + v and EE_SCREENS_SH_FORM_SELF (the host's object is the UIForm*)
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
    virtual void Activate() { EE_SHELL_IS(ACTIVATE) ? (void)EE_SHELL_TO(ACTIVATE) : UIForm::Activate(); }
    virtual void Deactivate() { EE_SHELL_IS(DEACTIVATE) ? (void)EE_SHELL_TO(DEACTIVATE) : UIForm::Deactivate(); }
    virtual void SetHelpTextPopupLocation()
    {
        EE_SHELL_IS(HELP_POPUP) ? (void)EE_SHELL_TO(HELP_POPUP) : UIForm::SetHelpTextPopupLocation();
    }
    virtual long UpdateKeyboard()
    {
        return EE_SHELL_IS(UPDATE_KEYBOARD) ? (long)EE_SHELL_TO(UPDATE_KEYBOARD) : UIForm::UpdateKeyboard();
    }
    virtual long UpdateMouse() { return EE_SHELL_IS(UPDATE_MOUSE) ? (long)EE_SHELL_TO(UPDATE_MOUSE) : UIForm::UpdateMouse(); }
    virtual void RelayoutForm(const GERectangle& inR)
    {
        EE_SHELL_IS(RELAYOUT) ? (void)EE_SHELL_TO(RELAYOUT, 0, 0, RectF(inR).f) : UIForm::RelayoutForm(inR);
    }
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat)
    {
        return EE_SHELL_IS(TRANSLATED_KEY) ? (long)EE_SHELL_TO(TRANSLATED_KEY, (int64_t)inKey, inRepeat)
                                           : UIForm::UpdateTranslatedKey(inKey, inRepeat);
    }
    virtual int64_t BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
    {
        switch (inVirtual) {
        case EE_SCREENS_SV_ACTIVATE: UIForm::Activate(); return 0;
        case EE_SCREENS_SV_DEACTIVATE: UIForm::Deactivate(); return 0;
        case EE_SCREENS_SV_HELP_POPUP: UIForm::SetHelpTextPopupLocation(); return 0;
        case EE_SCREENS_SV_UPDATE_KEYBOARD: return UIForm::UpdateKeyboard();
        case EE_SCREENS_SV_UPDATE_MOUSE: return UIForm::UpdateMouse();
        case EE_SCREENS_SV_RELAYOUT: UIForm::RelayoutForm(Rect(ioF)); return 0;
        case EE_SCREENS_SV_TRANSLATED_KEY: return UIForm::UpdateTranslatedKey(reinterpret_cast<const char*>(inA), (short)inB);
        case EE_SCREENS_SV_DRAW_BACKGROUND: return UIForm::DrawBackground(Rect(ioF));
        case EE_SCREENS_SV_CLOSE: UIForm::Close(); return 0;
        case EE_SCREENS_SV_COPY_DATA: return UIForm::CopyData();
        case EE_SCREENS_SV_DRAW_POLYGONS: return UIForm::DrawPolygons();
        case EE_SCREENS_SV_DRAW_TEXT: return UIForm::DrawTextA();
        case EE_SCREENS_SV_OPEN: UIForm::Open(); return 0;
        case EE_SCREENS_SV_SET_RECTANGLE: UIForm::SetRectangle(Rect(ioF)); return 0;
        default: return 0;
        }
    }
    // the form's own virtual inVirtual through its vtable (a kind's override runs)
    virtual int64_t SelfCall(uint32_t inVirtual)
    {
        switch (inVirtual) {
        case EE_SCREENS_SV_ACTIVATE: this->Activate(); return 0;
        case EE_SCREENS_SV_DEACTIVATE: this->Deactivate(); return 0;
        case EE_SCREENS_SV_UPDATE_KEYBOARD: return this->UpdateKeyboard();
        case EE_SCREENS_SV_UPDATE_MOUSE: return this->UpdateMouse();
        case EE_SCREENS_SV_CLOSE: this->Close(); return 0;
        case EE_SCREENS_SV_COPY_DATA: return this->CopyData();
        case EE_SCREENS_SV_DRAW_POLYGONS: return this->DrawPolygons();
        case EE_SCREENS_SV_OPEN: this->Open(); return 0;
        default: return 0;
        }
    }
    virtual void Keyboard(bool inCapture) {}
protected:
    virtual long DrawBackground(const GERectangle& inR)
    {
        return EE_SHELL_IS(DRAW_BACKGROUND) ? (long)EE_SHELL_TO(DRAW_BACKGROUND, 0, 0, RectF(inR).f) : UIForm::DrawBackground(inR);
    }
    virtual void Close() { EE_SHELL_IS(CLOSE) ? (void)EE_SHELL_TO(CLOSE) : UIForm::Close(); }
    virtual long CopyData() { return EE_SHELL_IS(COPY_DATA) ? (long)EE_SHELL_TO(COPY_DATA) : UIForm::CopyData(); }
    virtual long DrawPolygons() { return EE_SHELL_IS(DRAW_POLYGONS) ? (long)EE_SHELL_TO(DRAW_POLYGONS) : UIForm::DrawPolygons(); }
    virtual long DrawTextA() { return EE_SHELL_IS(DRAW_TEXT) ? (long)EE_SHELL_TO(DRAW_TEXT) : UIForm::DrawTextA(); }
    virtual void Open() { EE_SHELL_IS(OPEN) ? (void)EE_SHELL_TO(OPEN) : UIForm::Open(); }
    virtual void SetRectangle(const GERectangle& inR)
    {
        EE_SHELL_IS(SET_RECTANGLE) ? (void)EE_SHELL_TO(SET_RECTANGLE, 0, 0, RectF(inR).f) : UIForm::SetRectangle(inR);
    }
};


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
