// C:\EEX\Low-Level Engine\User Interface\UIControl.h (RECONSTRUCTION). Abstract base of every form control.
// UIControl.cpp holds the ctor, dtor and the out-of-line members; the inline members are emitted first by
// UIBackground.cpp (0x10058f73..0x100594ef, declaration order). Virtual slot order from the vtable (0x10098a74,
// 34 slots; the pure slots named from the UIButton/UIDualButton overrides). sizeof 0xa0 (??_E stride 0x100594a5).
// Requires lle/UIEntity.h, lle/GEColor.h, lle/GERhombus.h, <map>.
#ifndef LLE_UICONTROL_H
#define LLE_UICONTROL_H

#include <map>

#include "lle/ISEntity.h"
#include "lle/UIEntity.h"

class GEFont;
class GERasterizer;
class GESurface;
class SESoundManager;
class UIEvent;
class UIForm;
class UIHotKey;

// winspool.h maps GetForm to GetFormA; the export is ?GetForm@UIControl@@
#ifdef GetForm
#undef GetForm
#endif

class LLE_API UIControl : public UIEntity {
    friend class UIForm;                    // the form's tabbing calls TabbedTo/TabbedFrom (protected)
    friend class UIComboBox;                // drives its parts' protected mouse/tab handlers (ui-edit)
    friend struct UIControlRust;            // UIControl.cpp: the Rust mouse dispatch's calls (ee_ui mouse.rs)
public:
    virtual void AddIMEResultString(const UWideString& inString) {}   // slot 0x04
    // the key statics are ISEntity*: GetValue() is slot 0x0c
    static bool AltKeyPressed() { return UIControl::sAltKey->GetValue() > 0; }
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor) { return kErrorFailed; }
    virtual void ChangeFont(GEFont* inFont) {}
    virtual long CloseIMECompositionString() { return kErrorFailed; }
    static bool ControlKeyPressed() { return UIControl::sControlKey->GetValue() > 0; }
    virtual long CopyData();
    static bool CopyKeyPressed() { return UIControl::sControlKey->GetValue() && UIControl::sCopyKey->GetValue(); }
    static bool CutKeyPressed()
    {
        return UIControl::sControlKey->GetValue() && UIControl::sCutKey->GetValue() || UIControl::ShiftKeyPressed() && UIControl::sDeleteKey->GetValue();
    }
    static bool DeleteKeyPressed();
    static bool DownKeyPressed();
    virtual long DrawPolygons();
    virtual long DrawTextA() { this->mDrawCount--; return kErrorOK; }
    static bool EndKeyPressed();
    static bool EnterKeyPressed();
    void FireTriggerEvent(unsigned long inEvent);
    bool GetAlwaysDirty() { return this->mAlwaysDirty; }
    static void GetBorderColors(GEColor& outDark, GEColor& outLight)
    {
        outDark = UIControl::sDarkBorder;
        outLight = UIControl::sLightBorder;
    }
    float GetBorderOpacity() const { return this->mBorderOpacity; }
    unsigned long GetBorderThickness() { return this->mBorderThickness; }
    float GetBgOpacity() const
    {
        float theOpacity;
        this->mBgRhombus.GetOpacityPerVertex(theOpacity, theOpacity, theOpacity, theOpacity);
        return theOpacity;
    }
    void GetBgColor(GEColor& outColor) const { this->mBgRhombus.GetColorPerVertex(outColor, outColor, outColor, outColor); }
    const GERectangle& GetBgRectangle() const { return this->mBgRhombus.GetRectangle(); }
    const GERectangle& GetCopiedDrawRectangle() const { return this->mCopiedDrawRectangle; }
    unsigned long GetDisplayMode() const { return this->mDisplayMode; }
    unsigned long GetDrawCount() const { return this->mDrawCount; }
    GESurface* GetDrawingSurface() const;
    UIForm* GetForm() const { return this->mForm; }
    bool GetHasKeyboardCapture() const;
    bool GetHasMouseCapture() const;
    unsigned long GetHeight() const { return UMFloor(this->mRectangle.GetBottom() - this->mRectangle.GetTop() + 1.0f); }
    virtual bool GetHelpString(UWideString& outString) const;
    UIHotKey* GetHotKey() const { return this->mHotKey; }
    unsigned long GetId() const { return this->mId; }
    virtual bool GetIMEEnabled() const { return false; }
    unsigned long GetIndex() const { return this->mIndex; }
    bool GetIsDirty() const { return this->mIsDirty; }
    bool GetLeftMouseButtonDown() const;
    GERasterizer* GetRasterizer() const;
    const GERectangle& GetRectangle() const { return this->mRectangle; }
    bool GetRightMouseButtonDown() const;
    unsigned long GetScreenIndex() const;
    SESoundManager* GetSoundManager() const;
    unsigned long GetStringTableID() const { return this->mStringTableID; }
    virtual bool GetTabable() const { return true; }
    UIEvent* GetTriggerEvent(unsigned long inEvent);
    virtual unsigned long GetType() const { return this->mControlType; }
    unsigned long GetWidth() const { return UMFloor(this->mRectangle.GetWidth() + 1.0f); }
    virtual void HideControl() {}
    static bool HomeKeyPressed();
    static bool LeftKeyPressed();
protected:
    virtual long MouseLButtonDown() = 0;    // slot 0x34
    virtual long MouseLButtonUp() = 0;
public:
    virtual long MouseLButtonDoublePressed()
    {
        this->mLeftButtonDown = true;
        return this->MouseLButtonDown();
    }
protected:
    virtual long MouseMove(const GE2DPoint& inLocation) = 0;  // slot 0x40
    virtual long MouseRButtonDown() = 0;
    virtual long MouseRButtonUp() = 0;
public:
    virtual long MouseWheelTurned(long inDelta) { return kErrorFailed; }
    virtual long OpenIMECompositionString() { return kErrorFailed; }
    static bool PasteKeyPressed()
    {
        return UIControl::sControlKey->GetValue() && UIControl::sPasteKey->GetValue() || UIControl::ShiftKeyPressed() && UIControl::sInsertKey->GetValue();
    }
    virtual void ReleaseCapturedKeyboard();
    virtual void ReleaseCapturedMouse();
    static bool RightKeyPressed();
    void SetAlwaysDirty(bool inAlwaysDirty) { this->mAlwaysDirty = inAlwaysDirty; }
    virtual void SetBgColor(const GEColor& inColor)
    {
        this->mBgRhombus.SetColor(inColor);
        this->SetDirty(true);
    }
    virtual void SetBgOpacity(float inOpacity)
    {
        this->mHasBgOpacity = inOpacity != 0.0f;
        this->mBgRhombus.SetOpacity(inOpacity);
    }
    static void SetBorderColors(const GEColor& inDark, const GEColor& inLight)
    {
        UIControl::sDarkBorder = inDark;
        UIControl::sLightBorder = inLight;
    }
    void SetBorderOpacity(float inOpacity) { this->mBorderOpacity = inOpacity; }
    void SetBorderThickness(unsigned long inThickness)
    {
        this->mBorderThickness = inThickness;
        this->SetDirty(true);
    }
    void SetBorderType(unsigned long inType) { this->mBorderType = inType; }
#ifdef LLE_EXE_HEADERS
    // inline forwards (the diplomacy form 0x5d4165, built without /Og: no extra code, each is one more inline site)
    void SetHiddenMode() { this->SetDisplayMode(0); }
    void SetTrigger(unsigned long inEvent, UIEvent* inTriggerEvent) { this->SetTriggerEvent(inEvent, inTriggerEvent); }
#endif
    virtual void SetDirty(bool inDirty);
    virtual void SetDisplayMode(unsigned long inMode);
    void SetHotKey(UIHotKey* inHotKey) { this->mHotKey = inHotKey; }
    virtual void SetLocation(const GE2DPoint& inLocation);
    virtual void SetRectangle(const GERectangle& inRectangle);
    void SetStringTableID(unsigned long inID) { this->mStringTableID = inID; }
    void SetTriggerEvent(unsigned long inEvent, UIEvent* inTriggerEvent);
    static bool ShiftKeyPressed();
#ifndef LLE_EXE_HEADERS   // the game calls slot 0x74 on a form's control (chat 0x1d2e09): public in its view
protected:
#endif
    virtual void TabbedTo() = 0;            // slot 0x74
    virtual void TabbedFrom() = 0;
public:
    virtual long UpdateKeyboard() = 0;      // slot 0x7c
    long UpdateMouse(const GE2DPoint& inLocation);
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat) { return kErrorOK; }
    static bool UpKeyPressed();
    virtual ~UIControl();
protected:
    UIControl(unsigned long inType, unsigned long inId, const GERectangle& inRectangle, UIForm* inForm,
              unsigned long inIndex, unsigned long inBorderThickness);
    void CaptureKeyboard();
    void CaptureMouse();
    virtual long DrawBorder(const GERectangle& inRectangle, unsigned long inThickness);   // slot 0x84
    const GE2DPoint& GetMouseLocation() const;
    void SetBgRectangle(const GERectangle& inRectangle);

    unsigned long mDisplayMode;             // +0x08 (1 at construction)
    std::map<unsigned long, UIEvent*> mTriggerEvents;   // +0x0c event id -> event (referenced)
    GERectangle mRectangle;                 // +0x18
    GERectangle mCopiedDrawRectangle;       // +0x28 mRectangle as of the last CopyData
    GERhombus mBgRhombus;                   // +0x38
    GERhombus mCopiedBgRhombus;             // +0x50
    UIForm* mForm;                          // +0x68
    UIHotKey* mHotKey;                      // +0x6c
    unsigned long mControlType;             // +0x70 (GetType)
    unsigned long mBorderType;              // +0x74
    unsigned long mCopiedBorderType;        // +0x78
    unsigned long mIndex;                   // +0x7c
    unsigned long mId;                      // +0x80
    unsigned long mDrawCount;               // +0x84
    unsigned long mBorderThickness;         // +0x88
    unsigned long mStringTableID;           // +0x8c
    float mBorderOpacity;                   // +0x90
    float mCopiedBorderOpacity;             // +0x94
    bool mIsDirty;                          // +0x98
    bool mLeftButtonDown;                   // +0x99
    bool mRightButtonDown;                  // +0x9a
    bool mCopiedHasBgOpacity;               // +0x9b
    bool mUnknown9C;                        // +0x9c
    bool mHasBgOpacity;                     // +0x9d
    bool mAlwaysDirty;                      // +0x9e
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number)
    static ISEntity* sSpaceKey;             // 0x1012e2f4
    static ISEntity* sAltKey;
    static ISEntity* sEnterKey;
    static ISEntity* sControlKey;
    static ISEntity* sEndKey;
    static ISEntity* sHomeKey;
    static ISEntity* sLeftKey;
    static ISEntity* sRightKey;
    static ISEntity* sUpKey;
    static ISEntity* sDownKey;
    static ISEntity* sDeleteKey;
    static ISEntity* sBackSpaceKey;
    static ISEntity* sNumEndKey;
    static ISEntity* sNumEnterKey;
    static ISEntity* sNumHomeKey;
    static ISEntity* sNumLeftKey;
    static ISEntity* sNumRightKey;
    static ISEntity* sNumUpKey;
    static ISEntity* sNumDownKey;
    static ISEntity* sNumDeleteKey;
    static ISEntity* sCutKey;
    static ISEntity* sCopyKey;
    static ISEntity* sPasteKey;
    static ISEntity* sInsertKey;
private:
    static GEColor sLightBorder;            // 0x1012e2d8
    static GEColor sDarkBorder;             // 0x1012e2e8
#endif
};

#include "lle/UIMultiByte.h"

// UIControl trigger-event ids (FireTriggerEvent / SetTriggerEvent); provisional names from the lle firing sites
enum UIControlTriggerID {
    kControlTriggerRangeChanged = 0x3e8,     // UISlider::ChangeRange fires it (db: Game Settings Music Volume etc.) (provisional name)
    kControlTriggerChecked = 0x3e9,          // UICheckBox::SetChecked(true), UIRadioButton::Check; UITriStateCheckBox::SetCheckState fires it for every state (db: Check Box ON) (provisional name)
    kControlTriggerUnchecked = 0x3ea,        // UICheckBox::SetChecked(false), UIRadioButton::UnCheck (db: Check Box OFF, UnCheck Box) (provisional name)
    kControlTriggerSelectionChanged = 0x3eb,  // UIListBox::ChangeSelectionIndex, UIComboBox::ChangeSelection (db: Change Focused File, Selected) (provisional name)
    kControlTriggerDoubleClicked = 0x3ec,    // UIListBox::MouseLButtonDoublePressed (db: Load Game By DbClick) (provisional name)
    kControlTriggerListOpened = 0x3ed,       // UIComboBox::OpenListBox (provisional name)
    kControlTriggerListClosed = 0x3ee,       // UIComboBox::CloseListBox (provisional name)
    kControlTriggerButtonDown = 0x3ef,       // UIButton::MouseLButtonDown / UpdateKeyboard key press, UIListBox::ButtonDown (next to ButtonDown()) (provisional name)
    kControlTriggerButtonUp = 0x3f0,         // UIButton::MouseLButtonUp inside the rectangle / key release, UIListBox::ButtonUp (next to ButtonUp(); db: Load Game, Cancel Save) (provisional name)
    kControlTriggerEnterPressed = 0x3f1,     // UIEditBox::ProcessEnterKey, UIListBox::UpdateKeyboard Enter (db: Save Game by Enter) (provisional name)
    kControlTriggerMouseOver = 0x3f2,        // UIForm::SetMouseOverControl on the new control (db: SI Map Mouse Over) (provisional name)
    kControlTriggerMouseOut = 0x3f3,         // UIForm::SetMouseOverControl on the old control (db: SI Map Mouse Out) (provisional name)
    kControlTriggerScrollingFinished = 0x3f4,  // UITLScrolling::CopyData end poll / StopScrolling (db: SRM Chat Dialog Finished) (provisional name)
    kControlTriggerRightButtonDown = 0x3f5,  // UIDualButton::MouseRButtonDown (provisional name)
    kControlTriggerRightButtonUp = 0x3f6,    // UIDualButton::MouseRButtonUp inside the rectangle (provisional name)
};

// UIControl::GetType: the type each control class passes to the UIControl ctor (or its GetType override)
enum UIControlType {
    kControlTypeCustom = 0,           // the game's own controls (EEUIMiniMap, EEUICPortrait, EEUICPing, ...) pass 0 (provisional name)
    kControlTypeButton = 1,           // UIButton ctor (provisional name)
    kControlTypeCheckBox = 2,         // UICheckBox::GetType (provisional name)
    kControlTypeComboBox = 3,         // UIComboBox ctor (provisional name)
    kControlTypeEditBox = 4,          // UIEditBox ctor (provisional name)
    kControlTypeListBox = 5,          // UIListBox ctor (provisional name)
    kControlTypeRadioButton = 6,      // UIRadioButton::GetType (provisional name)
    kControlTypeScrollBar = 7,        // UIScrollBar ctor (provisional name)
    kControlTypeSlider = 8,           // UISlider ctor (provisional name)
    kControlTypeStatusBar = 9,        // UIStatusBar ctor (provisional name)
    kControlTypeBitmap = 10,          // UIBitmap ctor (UIMovie passes 10 too) (provisional name)
    kControlTypeTextLabel = 11,       // UITextLabel ctor (provisional name)
    kControlTypeTriStateCheckBox = 12,  // UITriStateCheckBox::GetType (provisional name)
    kControlTypeGraph = 13,           // UIGraph ctor (provisional name)
};

// UIControl::mDisplayMode (+0x08, 1 at construction): SetDisplayMode(0) hides the control (HideControl, dirty rectangle, tab moves on); setrilo ui_framework.h "0 hidden, 1 shown".
enum UIDisplayMode {
    kDisplayModeHidden = 0,  // setrilo ui_framework.h displayMode_ "0 hidden"; UIControl::SetDisplayMode LLE 0x10065af5 hides the control on 0
    kDisplayModeShown = 1,  // setrilo ui_framework.h displayMode_ "1 shown"; the UIControl ctor sets 1
};

#endif
