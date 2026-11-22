// C:\EEX\Low-Level Engine\User Interface\UIControl.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10064ed6..0x10065f69: the U2D key init chains, the sDarkBorder/sLightBorder init chains, the ctor first, then the
// out-of-line members in alphabetical order, then the map<unsigned long, UIEvent*> COMDATs (~_Tree, clear, _Dec).
// UIControl's header-inline members are emitted first by UIBackground.cpp. UIControlsTable.cpp's inline block
// starts at 0x10065f69.
#include <windows.h>

#include "rust/ui/controlobj.h"
#include "rust/ui/draw.h"
#include "rust/ui/draw2d.h"
#include "rust/ui/draw_ffi.h"
#include "rust/ui/mouse_ffi.h"
#include "rust/ui/obj.h"
#include "rust/ui/rect.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/GEText.h"
#include "lle/GELine.h"
#include "lle/TSCriticalSection.h"
#include "lle/UResourceFile.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"
#include "rust/ui/lle_control.h"

GEColor UIControl::sDarkBorder(0.0f, 0.0f, 0.0f);
GEColor UIControl::sLightBorder;
ISEntity* UIControl::sSpaceKey;
ISEntity* UIControl::sAltKey;
ISEntity* UIControl::sEnterKey;
ISEntity* UIControl::sControlKey;
ISEntity* UIControl::sEndKey;
ISEntity* UIControl::sHomeKey;
ISEntity* UIControl::sLeftKey;
ISEntity* UIControl::sRightKey;
ISEntity* UIControl::sUpKey;
ISEntity* UIControl::sDownKey;
ISEntity* UIControl::sDeleteKey;
ISEntity* UIControl::sBackSpaceKey;
ISEntity* UIControl::sNumEndKey;
ISEntity* UIControl::sNumEnterKey;
ISEntity* UIControl::sNumHomeKey;
ISEntity* UIControl::sNumLeftKey;
ISEntity* UIControl::sNumRightKey;
ISEntity* UIControl::sNumUpKey;
ISEntity* UIControl::sNumDownKey;
ISEntity* UIControl::sNumDeleteKey;
ISEntity* UIControl::sCutKey;
ISEntity* UIControl::sCopyKey;
ISEntity* UIControl::sPasteKey;
ISEntity* UIControl::sInsertKey;

// the record's rectangles are read as GERectangle (bottom, left, right, top)
static_assert(sizeof(EeUiRect) == sizeof(GERectangle), "EeUiRect is GERectangle's layout");

// The record's calls out (ee_ui controlobj.rs): the form's and manager's, the virtuals, the rhombi; the event's is
// Rust's (lle_control.rs, the host ee_ui_control_host)
extern "C" intptr_t ee_uicpp_control_host(EE_UI_HOST_ARGS) { return UIControl::HostOp(inObj, inOp, a, b, c, d, f); }
#define c static_cast<UIControl*>(inControl)
#define theForm static_cast<UIControl*>(inControl)->GetForm()
intptr_t UIControl::HostOp(void* inControl, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
{
    switch (inOp) {
    case EeUiOp::CtlFormControl: return (intptr_t)theForm->GetControl((unsigned long)a);
    case EeUiOp::CtlSlotLeftDown: return theForm->GetControl((unsigned long)a)->mLeftButtonDown;
    case EeUiOp::CtlSlotRightDown: return theForm->GetControl((unsigned long)a)->mRightButtonDown;
    case EeUiOp::CtlGroupEnabled: return theForm->GetEntityGroupById((unsigned long)a)->GetIsEnabled();
    case EeUiOp::CtlCurrentForm: return (intptr_t)theForm->GetManager()->GetCurrentForm();
    case EeUiOp::CtlKeyboardCaptured: return (intptr_t)theForm->GetKeyboardCaptured();
    case EeUiOp::CtlMouseCaptured: return (intptr_t)theForm->GetMouseCaptured();
    case EeUiOp::CtlSetKeyboardCaptured: theForm->SetKeyboardCaptured(reinterpret_cast<UIControl*>(a)); return 0;
    case EeUiOp::CtlSetMouseCaptured: theForm->SetMouseCaptured(reinterpret_cast<UIControl*>(a)); return 0;
    case EeUiOp::CtlHide: c->HideControl(); return 0;
    case EeUiOp::CtlAddDirty: theForm->GetManager()->AddDirtyRectangle(UIObjHost::Rect(f), theForm); return 0;
    case EeUiOp::CtlTab: theForm->Tab(); return 0;
    case EeUiOp::CtlAddEvent: theForm->GetManager()->AddEventToTrigger(reinterpret_cast<UIEvent*>(a), theForm, c); return 0;
    case EeUiOp::CtlHelpString: theForm->GetManager()->GetStringTable()->GetString((unsigned long)b, *reinterpret_cast<UWideString*>(a)); return 0;
    case EeUiOp::TabStop: return theForm->GetTabStop();
    case EeUiOp::TabbingOn: return theForm->GetTabbingOn();
    case EeUiOp::FormSurfaces: return theForm->GetManager()->GetNumberOfSurfaces();
    case EeUiOp::SetDirty: c->SetDirty(a != 0); return 0;
    case EeUiOp::FormCtlSetDirty: reinterpret_cast<UIControl*>(a)->SetDirty(b != 0); return 0;
    case EeUiOp::VirtSetLocation: c->SetLocation(GE2DPoint(f[0], f[1])); return 0;
    // the direct path's virtuals (ee_ui direct.rs: the game's Rust on a control, an override running)
    case EeUiOp::SetDisplayMode: c->SetDisplayMode((unsigned long)a); return 0;
    case EeUiOp::ReleaseMouse: c->ReleaseCapturedMouse(); return 0;
    case EeUiOp::SetBgOpacity: c->SetBgOpacity(f[0]); return 0;
    case EeUiOp::PartSetBgColor: c->SetBgColor(GEColor(f[0], f[1], f[2])); return 0;
    case EeUiOp::BgOpacity: f[0] = c->GetBgOpacity(); return 0;
    case EeUiOp::CtlTabbedTo: c->TabbedTo(); return 0;
    default: return 0;
    }
}
#undef c
#undef theForm

// The first control's key lookup (lle_control.rs): the statics in its order, the one-statement shims
#define CTL(x) reinterpret_cast<UIControl*>(x)
static ISEntity** const kUIControlKeySlots[] = {&UIControl::sSpaceKey, &UIControl::sEnterKey, &UIControl::sControlKey, &UIControl::sAltKey, &UIControl::sEndKey, &UIControl::sHomeKey, &UIControl::sLeftKey, &UIControl::sRightKey, &UIControl::sUpKey, &UIControl::sDownKey, &UIControl::sDeleteKey, &UIControl::sBackSpaceKey, &UIControl::sNumEndKey, &UIControl::sNumHomeKey, &UIControl::sNumLeftKey, &UIControl::sNumRightKey, &UIControl::sNumUpKey, &UIControl::sNumDownKey, &UIControl::sNumDeleteKey, &UIControl::sNumEnterKey, &UIControl::sCutKey, &UIControl::sCopyKey, &UIControl::sPasteKey, &UIControl::sInsertKey};
extern "C" bool ee_uicpp_control_keys_ready() { return UIControl::sSpaceKey != NULL; }
extern "C" void ee_uicpp_control_key_fetch(uint32_t inSlot, uint32_t inCode)
{ gISKeyboard->GetEntity(inCode, *kUIControlKeySlots[inSlot]); }
extern "C" uintptr_t ee_uicpp_mouse_l_button() { return (uintptr_t)UIManager::GetLMouseButton(); }
extern "C" uintptr_t ee_uicpp_mouse_r_button() { return (uintptr_t)UIManager::GetRMouseButton(); }
extern "C" bool ee_uicpp_control_left_down(uintptr_t inControl) { return CTL(inControl)->mLeftButtonDown; }
extern "C" bool ee_uicpp_control_right_down(uintptr_t inControl) { return CTL(inControl)->mRightButtonDown; }
extern "C" void ee_uicpp_control_set_left_down(uintptr_t inControl, bool inDown)
{ CTL(inControl)->mLeftButtonDown = inDown; }
extern "C" void ee_uicpp_control_set_right_down(uintptr_t inControl, bool inDown)
{ CTL(inControl)->mRightButtonDown = inDown; }
extern "C" long ee_uicpp_control_mouse_left_double(uintptr_t inControl)
{ return CTL(inControl)->MouseLButtonDoublePressed(); }
extern "C" long ee_uicpp_control_mouse_left_up(uintptr_t inControl) { return CTL(inControl)->MouseLButtonUp(); }
extern "C" long ee_uicpp_control_mouse_left_down(uintptr_t inControl) { return CTL(inControl)->MouseLButtonDown(); }
extern "C" long ee_uicpp_control_mouse_right_up(uintptr_t inControl) { return CTL(inControl)->MouseRButtonUp(); }
extern "C" long ee_uicpp_control_mouse_right_down(uintptr_t inControl) { return CTL(inControl)->MouseRButtonDown(); }
extern "C" long ee_uicpp_control_mouse_move(uintptr_t inControl, uintptr_t inLocation)
{ return CTL(inControl)->MouseMove(*reinterpret_cast<const GE2DPoint*>(inLocation)); }

intptr_t UIControl::Call(EeUiControlCall inCall, intptr_t a, intptr_t b, float* f) const
{
    return ee_ui_control_call(const_cast<UIControl*>(this), inCall, a, b, f);
}
// a call with a rectangle (GERectangle is the buffer's order: bottom, left, right, top)
intptr_t UIControl::CallRect(EeUiControlCall inCall, const GERectangle& inRectangle) const
{
    float f[4] = {inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    return this->Call(inCall, 0, 0, f);
}

typedef EeUiControlCall C;

// Creates a shown, dirty control with a transparent black background (the record's state, ee_ui controlobj.rs); the
// first one looks up the shared UI keys
UIControl::UIControl(unsigned long inType, unsigned long inId, const GERectangle& inRectangle, UIForm* inForm,
                     unsigned long inIndex, unsigned long inBorderThickness)
    : UIEntity(1)
{
    this->mBgRhombus = new GERhombus;
    this->mCopiedBgRhombus = new GERhombus;
    ee_ui_control_init(this, this, ee_ui_control_host, inType, inId, reinterpret_cast<const float*>(&inRectangle), inForm, inIndex, inBorderThickness);
    ee_ui_control_fetch_keys();
}

// Destroys the control: every trigger event released (ee_ui controlobj.rs), then the background rhombi
UIControl::~UIControl()
{
    ee_ui_control_release(this);
    delete this->BgRhombus();
    delete this->CopiedBgRhombus();
}

void UIControl::FireTriggerEvent(unsigned long inEvent) { this->Call(C::FireTrigger, inEvent); }
bool UIControl::GetHelpString(UWideString& outString) const { return this->Call(C::HelpString, (intptr_t)&outString) != 0; }
UIEvent* UIControl::GetTriggerEvent(unsigned long inEvent) { return (UIEvent*)this->Call(C::TriggerEvent, inEvent); }
void UIControl::SetTriggerEvent(unsigned long inEvent, UIEvent* inTriggerEvent)
{
    this->Call(C::SetTrigger, inEvent, (intptr_t)inTriggerEvent);
}

void UIControl::SetBgRectangle(const GERectangle& inRectangle) { this->CallRect(C::SetBgRectangle, inRectangle); }

void UIControl::SetLocation(const GE2DPoint& inLocation)
{
    float f[4] = {inLocation.GetX(), inLocation.GetY(), 0.0f, 0.0f};
    this->Call(C::SetLocation, 0, 0, f);
}

void UIControl::SetRectangle(const GERectangle& inRectangle) { this->CallRect(C::SetRectangle, inRectangle); }

// Returns whether Delete (or keypad Del with Num Lock off) is held
bool UIControl::DeleteKeyPressed()
{
    return UIControl::sDeleteKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumDeleteKey->GetValue();
}

// Returns whether Down (or keypad 2 with Num Lock off) is held
bool UIControl::DownKeyPressed()
{
    return UIControl::sDownKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumDownKey->GetValue();
}

// Draws a bevelled border of the given thickness, dark top-left and light bottom-right (swapped for border type 1):
// Rust (ee_ui draw2d.rs, one GELine on the 2D draw list)
long UIControl::DrawBorder(const GERectangle& inRectangle, unsigned long inThickness)
{
    return ee_ui_draw_border(this->GetRasterizer(), this->GetScreenIndex(), *reinterpret_cast<const EeUiRect*>(&inRectangle), inThickness,
                             this->mCopiedBorderOpacity, this->mCopiedBorderType, reinterpret_cast<const float*>(&UIControl::sLightBorder),
                             reinterpret_cast<const float*>(&UIControl::sDarkBorder));
}

// Draws the copied background and the border
long UIControl::DrawPolygons()
{
    if (this->mCopiedHasBgOpacity)
        this->GetRasterizer()->Draw(this->CopiedBgRhombus(), this->GetScreenIndex());
    return this->DrawBorder(this->GetCopiedDrawRectangle(), this->mBorderThickness);
}

// Returns whether Enter or keypad Enter was just released
bool UIControl::EnterKeyPressed()
{
    return UIControl::sEnterKey->JustUnpressed() || UIControl::sNumEnterKey->JustUnpressed();
}

// Returns whether End (or keypad 1 with Num Lock off) is held
bool UIControl::EndKeyPressed()
{
    return UIControl::sEndKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumEndKey->GetValue();
}

// Returns whether Home (or keypad 7 with Num Lock off) is held
bool UIControl::HomeKeyPressed()
{
    return UIControl::sHomeKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumHomeKey->GetValue();
}

// Returns whether Left (or keypad 4 with Num Lock off) is held
bool UIControl::LeftKeyPressed()
{
    return UIControl::sLeftKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumLeftKey->GetValue();
}

// Returns whether Right (or keypad 6 with Num Lock off) is held
bool UIControl::RightKeyPressed()
{
    return UIControl::sRightKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumRightKey->GetValue();
}

// Returns whether Shift is held or was just pressed
bool UIControl::ShiftKeyPressed()
{
    return UIForm::sShiftKey->JustPressed() || UIForm::sShiftKey->GetValue();
}

// The ops every whole object's host shares (ee_ui obj.rs): UIControl's members on the object
bool UIObjHost::Base(UIControl* c, EeUiOp inOp, intptr_t a, float* f, intptr_t* outResult)
{
    intptr_t r = 0;
    switch (inOp) {
    case EeUiOp::BaseCopyData: r = c->UIControl::CopyData(); break;
    case EeUiOp::BaseDrawPolygons: r = c->UIControl::DrawPolygons(); break;
    case EeUiOp::BaseDrawText: r = c->UIControl::DrawTextA(); break;
    case EeUiOp::BaseSetLocation: c->UIControl::SetLocation(GE2DPoint(f[0], f[1])); break;
    case EeUiOp::SetDirty: c->SetDirty(a != 0); break;
    case EeUiOp::Rect: UIObjHost::PutRect(f, c->GetRectangle()); break;
    case EeUiOp::MouseLocation: f[0] = c->GetMouseLocation().GetX(); f[1] = c->GetMouseLocation().GetY(); break;
    case EeUiOp::CaptureMouse: c->CaptureMouse(); break;
    case EeUiOp::ReleaseMouse: c->ReleaseCapturedMouse(); break;
    case EeUiOp::FireTrigger: c->FireTriggerEvent((unsigned long)a); break;
    case EeUiOp::BorderThickness: r = c->mBorderThickness; break;
    case EeUiOp::Width: r = c->GetWidth(); break;
    case EeUiOp::Index: r = c->mIndex; break;
    case EeUiOp::BorderType: r = c->mBorderType; break;
    case EeUiOp::SetBorderType: c->mBorderType = (unsigned long)a; break;
    case EeUiOp::TabStop: r = c->GetForm()->GetTabStop(); break;
    case EeUiOp::TabbingOn: r = c->GetForm()->GetTabbingOn(); break;
    case EeUiOp::SetTabStop: c->GetForm()->SetTabStop((unsigned long)a); break;
    case EeUiOp::Tabable: r = c->GetTabable(); break;
    case EeUiOp::HasMouseCapture: r = c->GetHasMouseCapture(); break;
    case EeUiOp::RightButtonDown: r = c->GetRightMouseButtonDown(); break;
    case EeUiOp::Key: {
        ISEntity* theKey = (a & 1) ? UIControl::sEnterKey : UIControl::sSpaceKey;
        r = a < 2 ? theKey->JustPressed() : theKey->JustUnpressed();
        break;
    }
    case EeUiOp::CaptureKeyboard: c->CaptureKeyboard(); break;
    case EeUiOp::ReleaseKeyboard: c->ReleaseCapturedKeyboard(); break;
    case EeUiOp::ManagerBitmap: r = (intptr_t)c->GetForm()->GetManager()->GetBitmap(ee_ui_obj_manager_bitmap(a)); break;
    case EeUiOp::CopiedDrawRect: UIObjHost::PutRect(f, c->GetCopiedDrawRectangle()); break;
    case EeUiOp::BaseSetRectangle: c->UIControl::SetRectangle(UIObjHost::Rect(f)); break;
    case EeUiOp::ControlCount: r = c->GetForm()->GetControlCount(); break;
    case EeUiOp::AlwaysDirty: r = c->GetAlwaysDirty(); break;
    case EeUiOp::SetAlwaysDirty: c->SetAlwaysDirty(a != 0); break;
    case EeUiOp::SetDisplayMode: c->SetDisplayMode((unsigned long)a); break;
    case EeUiOp::BgOpacity: f[0] = c->GetBgOpacity(); break;
    case EeUiOp::BorderOpacity: f[0] = c->GetBorderOpacity(); break;
    case EeUiOp::SetBgOpacity: c->SetBgOpacity(f[0]); break;
    case EeUiOp::SetBorderOpacity: c->SetBorderOpacity(f[0]); break;
    default: return false;
    }
    *outResult = r;
    return true;
}

// The ops on the bitmaps and texts a whole object holds by address (ee_ui obj.rs): a the object, b an argument
bool UIObjHost::Media(UIControl* c, EeUiOp inOp, intptr_t a, intptr_t b, float* f, intptr_t* outResult)
{
    GEBitmap* theBitmap = reinterpret_cast<GEBitmap*>(a);
    GEText* theText = reinterpret_cast<GEText*>(a);
    intptr_t r = 0;
    switch (inOp) {
    case EeUiOp::BitmapReference: theBitmap->Reference(); break;
    case EeUiOp::BitmapRelease: theBitmap->Dereference(); break;
    case EeUiOp::BitmapTextureWidth: r = theBitmap->GetTexture(c->GetScreenIndex())->GetWidth(); break;
    case EeUiOp::StringLength: r = reinterpret_cast<const UWideString*>(a)->GetLength(); break;
    case EeUiOp::TextCreate: r = (intptr_t) new GEText(reinterpret_cast<GEFont*>(a)); break;
    case EeUiOp::TextDelete: delete theText; break;
    case EeUiOp::TextSetFont: theText->SetFont(reinterpret_cast<GEFont*>(b)); break;
    case EeUiOp::TextLength: r = theText->GetText().GetLength(); break;
    case EeUiOp::TextExtent:
        f[0] = theText->GetTextExtent().GetWidth();
        f[1] = theText->GetTextExtent().GetHeight();
        break;
    case EeUiOp::TextCopy: *theText = *reinterpret_cast<GEText*>(b); break;
    case EeUiOp::TextSetText: theText->SetText(*reinterpret_cast<const UWideString*>(b)); break;
    case EeUiOp::TextSetLocation: theText->SetLocation(GE2DPoint(f[0], f[1])); break;
    case EeUiOp::TextSetFgColor: theText->SetFgColor(GEColor(f[0], f[1], f[2])); break;
    case EeUiOp::DrawClippedText: r = c->GetForm()->DrawClippedText(UIObjHost::Rect(f), theText, c); break;
    default: return false;
    }
    *outResult = r;
    return true;
}

// The ops on a part a whole object owns (ee_ui obj.rs): another control's virtuals, by address
bool UIObjHost::Part(UIControl* p, EeUiOp inOp, float* f, intptr_t* outResult)
{
    intptr_t r = 0;
    switch (inOp) {
    case EeUiOp::DeletePart: delete p; break;
    case EeUiOp::PartCopyData: r = p->CopyData(); break;
    case EeUiOp::PartDrawPolygons: r = p->DrawPolygons(); break;
    case EeUiOp::PartDrawText: r = p->DrawTextA(); break;
    case EeUiOp::PartRect: UIObjHost::PutRect(f, p->GetRectangle()); break;
    case EeUiOp::PartMouseDown: r = p->MouseLButtonDown(); break;
    case EeUiOp::PartMouseUp: r = p->MouseLButtonUp(); break;
    case EeUiOp::PartMouseMove: r = p->MouseMove(GE2DPoint(f[0], f[1])); break;
    case EeUiOp::PartSetLocation: p->SetLocation(GE2DPoint(f[0], f[1])); break;
    case EeUiOp::PartSetRectangle: p->SetRectangle(UIObjHost::Rect(f)); break;
    case EeUiOp::PartSetBgColor: p->SetBgColor(GEColor(f[0], f[1], f[2])); break;
    case EeUiOp::PartSetBgOpacity: p->SetBgOpacity(f[0]); break;
    default: return false;
    }
    *outResult = r;
    return true;
}

// Returns whether Up (or keypad 8 with Num Lock off) is held
bool UIControl::UpKeyPressed()
{
    return UIControl::sUpKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumUpKey->GetValue();
}

// (their bodies need the form's, the manager's or the background table's class)
// Returns the UI manager's screen index
unsigned long UIControl::GetScreenIndex() const { return this->GetForm()->GetManager()->GetScreen(); }
// Returns the UI manager's sound manager
SESoundManager* UIControl::GetSoundManager() const { return this->GetForm()->GetManager()->GetSoundManager(); }
// Dispatches left/right mouse button presses, releases and double clicks to the control, then the mouse move
// (ee_ui mouse.rs)
long UIControl::UpdateMouse(const GE2DPoint& inLocation) { return ee_ui_control_update_mouse(this, &inLocation); }
// Returns the form's mouse location
const GE2DPoint& UIControl::GetMouseLocation() const { return this->GetForm()->GetMouseLocation(); }
// Returns the UI manager's rasterizer
GERasterizer* UIControl::GetRasterizer() const { return this->GetForm()->GetManager()->GetRasterizer(); }
