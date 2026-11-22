// C:\EEX\Low-Level Engine\User Interface\UIControl.h (RECONSTRUCTION). Abstract base of every form control.
// UIControl.cpp holds the ctor, dtor and the out-of-line members; the inline members are emitted first by
// UIBackground.cpp (0x10058f73..0x100594ef, declaration order). Virtual slot order from the vtable (0x10098a74,
// 34 slots; the pure slots named from the UIButton/UIDualButton overrides). sizeof 0xa0 (??_E stride 0x100594a5).
// Requires lle/UIEntity.h, lle/GEColor.h, lle/GERhombus.h.
//
// The members are EeUiControlFields (rust/crates/ee_ui/src/controlobj.rs, R6 ui round 19), the class's private base:
// the record's field names are the members', the inline getters read them in place, and the base's logic (the
// constructor's state, CopyData, SetDirty, SetDisplayMode, SetLocation, SetRectangle, the trigger events, the captures,
// the help string) runs in Rust over it, calling out through HostOp. The two background rhombi are C++ GERhombus
// objects the record holds by address; the trigger events a Rust map. A Rust caller reaches a control's record
// through GetFields().
#ifndef LLE_UICONTROL_H
#define LLE_UICONTROL_H

#include <map>

#include "rust/ui/controlobj.h"
#include "rust/ui/obj.h"
#include "rust/ui/rect.h"
#include "lle/ISEntity.h"
#include "rust/ui/mouse_ffi.h"
#include "rust/ui/ids.h"
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

class LLE_API UIControl : public UIEntity, public EeUiControlFields {   // (public: the toolkit's shims, UIControl.cpp)
    friend class UIForm;                    // the form's tabbing calls TabbedTo/TabbedFrom (protected)
    friend class UIComboBox;                // drives its parts' protected mouse/tab handlers (ui-edit)
    friend struct UIObjHost;                // UIControl.cpp: the whole objects' base ops (ee_ui obj.rs)
public:
    virtual void AddIMEResultString(const UWideString& inString) {}   // slot 0x04
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor) { return kErrorFailed; }
    virtual void ChangeFont(GEFont* inFont) {}
    virtual long CloseIMECompositionString() { return kErrorFailed; }
    static bool ControlKeyPressed() { return UIControl::sControlKey->GetValue() > 0; }
    virtual long CopyData() { return (long)this->Call(EeUiControlCall::CopyData); }
    static bool CopyKeyPressed() { return UIControl::sControlKey->GetValue() && UIControl::sCopyKey->GetValue(); }
    static bool CutKeyPressed()
    {
        return UIControl::sControlKey->GetValue() && UIControl::sCutKey->GetValue() || UIControl::ShiftKeyPressed() && UIControl::sDeleteKey->GetValue();
    }
    // Returns whether Delete (or keypad Del with Num Lock off) is held
    static bool DeleteKeyPressed();
    // Returns whether Down (or keypad 2 with Num Lock off) is held
    static bool DownKeyPressed();
    virtual long DrawPolygons();
    virtual long DrawTextA() { this->mDrawCount--; return kErrorOK; }
    // Returns whether End (or keypad 1 with Num Lock off) is held
    static bool EndKeyPressed();
    // Returns whether Enter or keypad Enter was just released
    static bool EnterKeyPressed() { return UIControl::sEnterKey->JustUnpressed() || UIControl::sNumEnterKey->JustUnpressed(); }
    void FireTriggerEvent(unsigned long inEvent) { this->Call(EeUiControlCall::FireTrigger, inEvent); }
    bool GetAlwaysDirty() { return this->mAlwaysDirty; }
    float GetBorderOpacity() const { return this->mBorderOpacity; }
    unsigned long GetBorderThickness() { return this->mBorderThickness; }
    float GetBgOpacity() const
    {
        float theOpacity;
        this->BgRhombus()->GetOpacityPerVertex(theOpacity, theOpacity, theOpacity, theOpacity);
        return theOpacity;
    }
    void GetBgColor(GEColor& outColor) const { this->BgRhombus()->GetColorPerVertex(outColor, outColor, outColor, outColor); }
    const GERectangle& GetCopiedDrawRectangle() const { return UIControl::AsRect(this->mCopiedDrawRectangle); }
    unsigned long GetDisplayMode() const { return this->mDisplayMode; }
    unsigned long GetDrawCount() const { return this->mDrawCount; }
    // the record (ee_ui controlobj.rs): the members and the base's logic, for Rust callers
    UIForm* GetForm() const { return static_cast<UIForm*>(this->mForm); }
    bool GetHasMouseCapture() const { return this->Call(EeUiControlCall::HasMouseCapture) != 0; }
    unsigned long GetHeight() const { return UMFloor(this->GetRectangle().GetBottom() - this->GetRectangle().GetTop() + 1.0f); }
    virtual bool GetHelpString(UWideString& outString) const { return this->Call(EeUiControlCall::HelpString, (intptr_t)&outString) != 0; }
    UIHotKey* GetHotKey() const { return static_cast<UIHotKey*>(this->mHotKey); }
    unsigned long GetId() const { return this->mId; }
    virtual bool GetIMEEnabled() const { return false; }
    unsigned long GetIndex() const { return this->mIndex; }
    bool GetIsDirty() const { return this->mIsDirty; }
    bool GetLeftMouseButtonDown() const { return this->Call(EeUiControlCall::LeftButtonDown) != 0; }
    // Returns the UI manager's rasterizer
    GERasterizer* GetRasterizer() const;
    const GERectangle& GetRectangle() const { return UIControl::AsRect(this->mRectangle); }
    bool GetRightMouseButtonDown() const { return this->Call(EeUiControlCall::RightButtonDown) != 0; }
    // (their bodies need the form's, the manager's or the background table's class)
    // Returns the UI manager's screen index
    unsigned long GetScreenIndex() const;
    // Returns the UI manager's sound manager
    SESoundManager* GetSoundManager() const;
    unsigned long GetStringTableID() const { return this->mStringTableID; }
    virtual bool GetTabable() const { return true; }
    UIEvent* GetTriggerEvent(unsigned long inEvent) { return (UIEvent*)this->Call(EeUiControlCall::TriggerEvent, inEvent); }
    virtual unsigned long GetType() const { return this->mControlType; }
    unsigned long GetWidth() const { return UMFloor(this->GetRectangle().GetWidth() + 1.0f); }
    virtual void HideControl() {}
    // Returns whether Home (or keypad 7 with Num Lock off) is held
    static bool HomeKeyPressed();
    // Returns whether Left (or keypad 4 with Num Lock off) is held
    static bool LeftKeyPressed();
public:   // (the mouse dispatch's shims: UIControl.cpp)
    virtual long MouseLButtonDown() = 0;    // slot 0x34
    virtual long MouseLButtonUp() = 0;
    virtual long MouseLButtonDoublePressed()
    {
        this->mLeftButtonDown = true;
        return this->MouseLButtonDown();
    }
    virtual long MouseMove(const GE2DPoint& inLocation) = 0;  // slot 0x40
    virtual long MouseRButtonDown() = 0;
    virtual long MouseRButtonUp() = 0;
    virtual long MouseWheelTurned(long inDelta) { return kErrorFailed; }
    virtual long OpenIMECompositionString() { return kErrorFailed; }
    static bool PasteKeyPressed()
    {
        return UIControl::sControlKey->GetValue() && UIControl::sPasteKey->GetValue() || UIControl::ShiftKeyPressed() && UIControl::sInsertKey->GetValue();
    }
    virtual void ReleaseCapturedKeyboard() { this->Call(EeUiControlCall::ReleaseKeyboard); }
    virtual void ReleaseCapturedMouse() { this->Call(EeUiControlCall::ReleaseMouse); }
    // Returns whether Right (or keypad 6 with Num Lock off) is held
    static bool RightKeyPressed();
    void SetAlwaysDirty(bool inAlwaysDirty) { this->mAlwaysDirty = inAlwaysDirty; }
    virtual void SetBgColor(const GEColor& inColor)
    {
        this->BgRhombus()->SetColor(inColor);
        this->SetDirty(true);
    }
    virtual void SetBgOpacity(float inOpacity)
    {
        this->mHasBgOpacity = inOpacity != 0.0f;
        this->BgRhombus()->SetOpacity(inOpacity);
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
    virtual void SetDirty(bool inDirty) { this->Call(EeUiControlCall::SetDirty, inDirty); }
    virtual void SetDisplayMode(unsigned long inMode) { this->Call(EeUiControlCall::SetDisplayMode, inMode); }
    virtual void SetLocation(const GE2DPoint& inLocation);
    virtual void SetRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiControlCall::SetRectangle, inRectangle); }
    void SetStringTableID(unsigned long inID) { this->mStringTableID = inID; }
    void SetTriggerEvent(unsigned long inEvent, UIEvent* inTriggerEvent) { this->Call(EeUiControlCall::SetTrigger, inEvent, (intptr_t)inTriggerEvent); }
    // Returns whether Shift is held or was just pressed
    static bool ShiftKeyPressed();
    // (public: the game calls slot 0x74 on a form's control, chat 0x1d2e09; the toolkit's shims)
    virtual void TabbedTo() = 0;            // slot 0x74
    virtual void TabbedFrom() = 0;
public:
    virtual long UpdateKeyboard() = 0;      // slot 0x7c
    // Dispatches left/right mouse button presses, releases and double clicks to the control, then the mouse move
    // (ee_ui mouse.rs)
    long UpdateMouse(const GE2DPoint& inLocation) { return ee_ui_control_update_mouse(this, &inLocation); }
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat) { return kErrorOK; }
    // Returns whether Up (or keypad 8 with Num Lock off) is held
    static bool UpKeyPressed();
    virtual ~UIControl();
protected:
    UIControl(unsigned long inType, unsigned long inId, const GERectangle& inRectangle, UIForm* inForm,
              unsigned long inIndex, unsigned long inBorderThickness);
    // The base's logic (ee_ui controlobj.rs)
    void CaptureKeyboard() { this->Call(EeUiControlCall::CaptureKeyboard); }
    void CaptureMouse() { this->Call(EeUiControlCall::CaptureMouse); }
    // Draws a bevelled border of the given thickness, dark top-left and light bottom-right (swapped for border type 1):
    // Rust (ee_ui draw2d.rs, one GELine on the 2D draw list)
    virtual long DrawBorder(const GERectangle& inRectangle, unsigned long inThickness); // slot 0x84
public:   // (the shims' access)
    // Returns the form's mouse location
    const GE2DPoint& GetMouseLocation() const;
protected:
    void SetBgRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiControlCall::SetBgRectangle, inRectangle); }
    // port: the display mode stored without SetDisplayMode's dirtying (two game controls' hides)
    void StoreDisplayMode(unsigned long inMode) { this->mDisplayMode = inMode; }
private:
    // the original's members (+0x08 mDisplayMode .. +0x9e mAlwaysDirty, sizeof 0xa0) are EeUiControlFields'; the
    // rectangles are GERectangle's layout (bottom, left, right, top), the rhombi GERhombus* the constructor makes
    static const GERectangle& AsRect(const EeUiRect& inRect) { return *reinterpret_cast<const GERectangle*>(&inRect); }
    GERhombus* BgRhombus() const { return static_cast<GERhombus*>(this->mBgRhombus); }
    GERhombus* CopiedBgRhombus() const { return static_cast<GERhombus*>(this->mCopiedBgRhombus); }
    intptr_t Call(EeUiControlCall inCall, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const { return ee_ui_control_call(const_cast<UIControl*>(this), inCall, a, b, f); }
    intptr_t CallRect(EeUiControlCall inCall, const GERectangle& inRectangle) const;
public:
    // the record's calls out that are not Rust's (EeUiOp 5, 16, 17, 71..74, 169, 218, 245, 520..533; ee_ui
    // lle_control.rs: ee_ui_control_host comes first)
    static intptr_t HostOp(void* inControl, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f);
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number)
public:   // (the key shims' access: UIControl.cpp)
    static ISEntity* sSpaceKey;
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
    static GEColor sLightBorder;
    static GEColor sDarkBorder;
#endif
};

#include "lle/UIMultiByte.h"

#endif
