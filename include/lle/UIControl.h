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
#include "rust/ui/lle_control.h"
#include "rust/ui/lle_hosts.h"
#include "rust/ui/ids.h"
#include "lle/UIEntity.h"
#include "lle/UIObjHost.h"
#include "lle/UReferenceCounter.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"


// winspool.h maps GetForm to GetFormA; the export is ?GetForm@UIControl@@
#ifdef GetForm
#undef GetForm
#endif

class LLE_API UIControl : public UIEntity, public EeUiControlFields {
public:   // (public: the toolkit's shims, UIControl.cpp)
    virtual void AddIMEResultString(const UWideString& inString) {}   // slot 0x04
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor) { return kErrorFailed; }
    static bool ControlKeyPressed() { return ee_ui_key_pressed(EeUiKeyPress::Control); }
    virtual long CopyData() { return (long)this->Call(EeUiControlCall::CopyData); }
    // Draws the copied background (when it has an opacity) and the border (ee_ui controlobj.rs)
    virtual long DrawPolygons() { return (long)this->Call(EeUiControlCall::DrawPolygons); }
    virtual long DrawTextA() { this->mDrawCount--; return kErrorOK; }
    void FireTriggerEvent(unsigned long inEvent) { this->Call(EeUiControlCall::FireTrigger, inEvent); }
    bool GetAlwaysDirty() { return this->mAlwaysDirty; }
    unsigned long GetBorderThickness() { return this->mBorderThickness; }
    float GetBgOpacity() const { return UIOut(ee_ui_control_call, const_cast<UIControl*>(this), EeUiControlCall::BgOpacity, (intptr_t)0, (intptr_t)0).f[0]; }
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
    unsigned long GetIndex() const { return this->mIndex; }
    bool GetLeftMouseButtonDown() const { return this->Call(EeUiControlCall::LeftButtonDown) != 0; }
    // Returns the UI manager's rasterizer
    GERasterizer* GetRasterizer() const { return this->GetForm()->GetManager()->GetRasterizer(); }
    const GERectangle& GetRectangle() const { return UIControl::AsRect(this->mRectangle); }
    bool GetRightMouseButtonDown() const { return this->Call(EeUiControlCall::RightButtonDown) != 0; }
    // Returns the UI manager's screen index
    unsigned long GetScreenIndex() const { return this->GetForm()->GetManager()->GetScreen(); }
    // Returns the UI manager's sound manager
    SESoundManager* GetSoundManager() const { return this->GetForm()->GetManager()->GetSoundManager(); }
    unsigned long GetStringTableID() const { return this->mStringTableID; }
    virtual bool GetTabable() const { return true; }
    UIEvent* GetTriggerEvent(unsigned long inEvent) { return (UIEvent*)this->Call(EeUiControlCall::TriggerEvent, inEvent); }
    virtual unsigned long GetType() const { return this->mControlType; }
    unsigned long GetWidth() const { return UMFloor(this->GetRectangle().GetWidth() + 1.0f); }
    virtual long MouseLButtonDown() = 0;    // slot 0x34
    virtual long MouseLButtonUp() = 0;
    // The left button down, then MouseLButtonDown (ee_ui controlobj.rs)
    virtual long MouseLButtonDoublePressed() { return (long)this->Call(EeUiControlCall::MouseDoublePressed); }
    virtual long MouseMove(const GE2DPoint& inLocation) = 0;  // slot 0x40
    virtual long MouseRButtonDown() = 0;
    virtual long MouseRButtonUp() = 0;
    // (the three below: the override of the control's kind, else the base's; ee_ui lle_chains.rs)
    void ReleaseCapturedKeyboard() { ee_ui_control_virt(this, 2, UIF4()); }
    void ReleaseCapturedMouse() { this->Call(EeUiControlCall::ReleaseMouse); }
    void SetAlwaysDirty(bool inAlwaysDirty) { this->mAlwaysDirty = inAlwaysDirty; }
    void SetBgColor(const GEColor& inColor) { ee_ui_control_virt(this, 1, UIF4(inColor)); }
    void SetBgOpacity(float inOpacity) { ee_ui_control_virt(this, 0, UIF4(inOpacity)); }
    // every control's border colours (ee_ui lle_control.rs; black and white until set)
    static void SetBorderColors(const GEColor& inDark, const GEColor& inLight) { ee_ui_set_border_colors(UIF4(inDark), UIF4(inLight)); }
    void SetBorderOpacity(float inOpacity) { this->mBorderOpacity = inOpacity; }
    void SetBorderThickness(unsigned long inThickness) { this->Call(EeUiControlCall::SetBorderThickness, inThickness); }
    virtual void SetDirty(bool inDirty) { this->Call(EeUiControlCall::SetDirty, inDirty); }
    void SetDisplayMode(unsigned long inMode) { this->Call(EeUiControlCall::SetDisplayMode, inMode); }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->Call(EeUiControlCall::SetLocation, 0, 0, UIF4(inLocation)); }
    virtual void SetRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiControlCall::SetRectangle, inRectangle); }
    void SetStringTableID(unsigned long inID) { this->mStringTableID = inID; }
    void SetTriggerEvent(unsigned long inEvent, UIEvent* inTriggerEvent) { this->Call(EeUiControlCall::SetTrigger, inEvent, (intptr_t)inTriggerEvent); }
    // Returns whether Shift is held or was just pressed
    static bool ShiftKeyPressed() { return ee_ui_key_pressed(EeUiKeyPress::Shift); }
    // (public: the game calls slot 0x74 on a form's control, chat 0x1d2e09; the toolkit's shims)
    virtual void TabbedTo() = 0;            // slot 0x74
    virtual void TabbedFrom() = 0;
    virtual long UpdateKeyboard() = 0;      // slot 0x7c
    // Dispatches left/right mouse button presses, releases and double clicks to the control, then the mouse move
    // (ee_ui mouse.rs)
    long UpdateMouse(const GE2DPoint& inLocation) { return ee_ui_control_update_mouse(this, &inLocation); }
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat) { return kErrorOK; }
    // the shared keys and mouse buttons: a Rust record of ISEntity pointers by EeUiKey (ee_ui lle_control.rs; once
    // the 24 key statics here, UIForm's Tab and Shift and UIManager's mouse buttons and wheel)
    static ISEntity*& Key(EeUiKey inKey) { return *reinterpret_cast<ISEntity**>(ee_ui_key_slot(inKey)); }
    virtual ~UIControl() { ee_ui_control_destruct(this); }
    // Creates a shown, dirty control with a transparent black background and its two rhombi; the first one looks up
    // the shared UI keys (ee_ui lle_control.rs, controlobj.rs)
    UIControl(unsigned long inType, unsigned long inId, const GERectangle& inRectangle, UIForm* inForm,
              unsigned long inIndex, unsigned long inBorderThickness, unsigned long inBorderType = 0)
        : UIEntity(1)
    {
        ee_ui_control_construct(this, this, inType, inId, reinterpret_cast<const float*>(&inRectangle), inForm, inIndex,
                                inBorderThickness, inBorderType);
    }
    // The base's logic (ee_ui controlobj.rs)
    void CaptureKeyboard() { this->Call(EeUiControlCall::CaptureKeyboard); }
    void CaptureMouse() { this->Call(EeUiControlCall::CaptureMouse); }
    // Returns the form's mouse location
    const GE2DPoint& GetMouseLocation() const { return this->GetForm()->GetMouseLocation(); }
    void SetBgRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiControlCall::SetBgRectangle, inRectangle); }
    // port: the display mode stored without SetDisplayMode's dirtying (two game controls' hides)
    void StoreDisplayMode(unsigned long inMode) { this->mDisplayMode = inMode; }
    // the original's members (+0x08 mDisplayMode .. +0x9e mAlwaysDirty, sizeof 0xa0) are EeUiControlFields'; the
    // rectangles are GERectangle's layout (bottom, left, right, top), the rhombi GERhombus* the constructor makes
    static const GERectangle& AsRect(const EeUiRect& inRect) { return *reinterpret_cast<const GERectangle*>(&inRect); }
    GERhombus* BgRhombus() const { return static_cast<GERhombus*>(this->mBgRhombus); }
    GERhombus* CopiedBgRhombus() const { return static_cast<GERhombus*>(this->mCopiedBgRhombus); }
    intptr_t Call(EeUiControlCall inCall, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const { return ee_ui_control_call(const_cast<UIControl*>(this), inCall, a, b, f); }
    intptr_t CallRect(EeUiControlCall inCall, const GERectangle& inRectangle) const { return this->Call(inCall, 0, 0, UIF4(inRectangle)); }
};

#include "lle/UIMultiByte.h"

#endif
