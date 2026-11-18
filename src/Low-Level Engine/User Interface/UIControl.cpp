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
#include "UIObjHost.h"

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

// The record's calls out (ee_ui controlobj.rs): the form's and manager's, the virtuals, the rhombi, the events
intptr_t UIControl::HostOp(void* inControl, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
{
    UIControl* c = static_cast<UIControl*>(inControl);
    UIForm* theForm = c->GetForm();
    GERhombus* theRhombus = a == 0 ? c->BgRhombus() : c->CopiedBgRhombus();
    switch (inOp) {
    case EeUiOp::CtlFormControl: return (intptr_t)theForm->GetControl((unsigned long)a);
    case EeUiOp::CtlSlotLeftDown: return theForm->GetControl((unsigned long)a)->mLeftButtonDown;
    case EeUiOp::CtlSlotRightDown: return theForm->GetControl((unsigned long)a)->mRightButtonDown;
    case EeUiOp::CtlGroupEnabled: return theForm->GetEntityGroupById((unsigned long)a)->GetIsEnabled();
    case EeUiOp::CtlCurrentForm: return (intptr_t)theForm->GetManager()->GetCurrentForm();
    case EeUiOp::CtlKeyboardCaptured: return (intptr_t)theForm->GetKeyboardCaptured();
    case EeUiOp::CtlMouseCaptured: return (intptr_t)theForm->GetMouseCaptured();
    case EeUiOp::CtlSetKeyboardCaptured: theForm->SetKeyboardCaptured(reinterpret_cast<UIControl*>(a)); break;
    case EeUiOp::CtlSetMouseCaptured: theForm->SetMouseCaptured(reinterpret_cast<UIControl*>(a)); break;
    case EeUiOp::CtlHide: c->HideControl(); break;
    case EeUiOp::CtlAddDirty: theForm->GetManager()->AddDirtyRectangle(UIObjHost::Rect(f), theForm); break;
    case EeUiOp::CtlTab: theForm->Tab(); break;
    case EeUiOp::CtlAddEvent: theForm->GetManager()->AddEventToTrigger(reinterpret_cast<UIEvent*>(a), theForm, c); break;
    case EeUiOp::CtlHelpString:
        theForm->GetManager()->GetStringTable()->GetString((unsigned long)b, *reinterpret_cast<UWideString*>(a));
        break;
    case EeUiOp::TabStop: return theForm->GetTabStop();
    case EeUiOp::TabbingOn: return theForm->GetTabbingOn();
    case EeUiOp::FormSurfaces: return theForm->GetManager()->GetNumberOfSurfaces();
    case EeUiOp::SetDirty: c->SetDirty(a != 0); break;
    case EeUiOp::FormCtlSetDirty: reinterpret_cast<UIControl*>(a)->SetDirty(b != 0); break;
    case EeUiOp::VirtSetLocation: c->SetLocation(GE2DPoint(f[0], f[1])); break;
    // the direct path's virtuals (ee_ui direct.rs: the game's Rust on a control, an override running)
    case EeUiOp::SetDisplayMode: c->SetDisplayMode((unsigned long)a); break;
    case EeUiOp::ReleaseMouse: c->ReleaseCapturedMouse(); break;
    case EeUiOp::SetBgOpacity: c->SetBgOpacity(f[0]); break;
    case EeUiOp::PartSetBgColor: c->SetBgColor(GEColor(f[0], f[1], f[2])); break;
    case EeUiOp::BgOpacity: f[0] = c->GetBgOpacity(); break;
    case EeUiOp::CtlTabbedTo: c->TabbedTo(); break;
    case EeUiOp::FormEventCall: {
        UIEvent* theEvent = reinterpret_cast<UIEvent*>(a);
        if (b == 0)
            theEvent->Uninitialize();
        else if (b == 1)
            theEvent->Dereference();
        else if (b == 2)
            theEvent->Reference();
        else
            theEvent->Initialize();
        break;
    }
    case EeUiOp::RhombusSetRect: theRhombus->SetRectangle(UIObjHost::Rect(f)); break;
    case EeUiOp::RhombusCopy: *c->CopiedBgRhombus() = *c->BgRhombus(); break;
    case EeUiOp::RhombusSetOpacity: theRhombus->SetOpacity(f[0]); break;
    case EeUiOp::RhombusSetColor: theRhombus->SetColor(GEColor(f[0], f[1], f[2])); break;
    default: break;
    }
    return 0;
}

intptr_t UIControl::Call(EeUiControlCall inCall, intptr_t a, intptr_t b, float* f) const
{
    return ee_ui_control_call(const_cast<UIControl*>(this), inCall, a, b, f);
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
    float theRectangle[4];
    UIObjHost::PutRect(theRectangle, inRectangle);
    ee_ui_control_init(this, this, UIControl::HostOp, inType, inId, theRectangle, inForm, inIndex, inBorderThickness);
    if (UIControl::sSpaceKey != NULL)
        return;
    gISKeyboard->GetEntity(kKeySpace, UIControl::sSpaceKey);
    gISKeyboard->GetEntity(kKeyReturn, UIControl::sEnterKey);
    gISKeyboard->GetEntity(kKeyControl, UIControl::sControlKey);
    gISKeyboard->GetEntity(kKeyAlt, UIControl::sAltKey);
    gISKeyboard->GetEntity(kKeyEnd, UIControl::sEndKey);
    gISKeyboard->GetEntity(kKeyHome, UIControl::sHomeKey);
    gISKeyboard->GetEntity(kKeyLeft, UIControl::sLeftKey);
    gISKeyboard->GetEntity(kKeyRight, UIControl::sRightKey);
    gISKeyboard->GetEntity(kKeyUp, UIControl::sUpKey);
    gISKeyboard->GetEntity(kKeyDown, UIControl::sDownKey);
    gISKeyboard->GetEntity(kKeyDelete, UIControl::sDeleteKey);
    gISKeyboard->GetEntity(kKeyBack, UIControl::sBackSpaceKey);
    gISKeyboard->GetEntity(kKeyNumpad1, UIControl::sNumEndKey);
    gISKeyboard->GetEntity(kKeyNumpad7, UIControl::sNumHomeKey);
    gISKeyboard->GetEntity(kKeyNumpad4, UIControl::sNumLeftKey);
    gISKeyboard->GetEntity(kKeyNumpad6, UIControl::sNumRightKey);
    gISKeyboard->GetEntity(kKeyNumpad8, UIControl::sNumUpKey);
    gISKeyboard->GetEntity(kKeyNumpad2, UIControl::sNumDownKey);
    gISKeyboard->GetEntity(kKeyDecimal, UIControl::sNumDeleteKey);
    gISKeyboard->GetEntity(kKeyNumpadEnter, UIControl::sNumEnterKey);
    gISKeyboard->GetEntity(kKeyX, UIControl::sCutKey);
    gISKeyboard->GetEntity(kKeyC, UIControl::sCopyKey);
    gISKeyboard->GetEntity(kKeyV, UIControl::sPasteKey);
    gISKeyboard->GetEntity(kKeyInsert, UIControl::sInsertKey);
}

// Destroys the control: every trigger event released (ee_ui controlobj.rs), then the background rhombi
UIControl::~UIControl()
{
    ee_ui_control_release(this);
    delete this->BgRhombus();
    delete this->CopiedBgRhombus();
}

// The base's logic (ee_ui controlobj.rs)
void UIControl::CaptureKeyboard() { this->Call(C::CaptureKeyboard); }
void UIControl::CaptureMouse() { this->Call(C::CaptureMouse); }
long UIControl::CopyData() { return (long)this->Call(C::CopyData); }
void UIControl::FireTriggerEvent(unsigned long inEvent) { this->Call(C::FireTrigger, inEvent); }
bool UIControl::GetHasMouseCapture() const { return this->Call(C::HasMouseCapture) != 0; }
bool UIControl::GetHelpString(UWideString& outString) const { return this->Call(C::HelpString, (intptr_t)&outString) != 0; }
bool UIControl::GetLeftMouseButtonDown() const { return this->Call(C::LeftButtonDown) != 0; }
bool UIControl::GetRightMouseButtonDown() const { return this->Call(C::RightButtonDown) != 0; }
UIEvent* UIControl::GetTriggerEvent(unsigned long inEvent) { return (UIEvent*)this->Call(C::TriggerEvent, inEvent); }
void UIControl::ReleaseCapturedKeyboard() { this->Call(C::ReleaseKeyboard); }
void UIControl::ReleaseCapturedMouse() { this->Call(C::ReleaseMouse); }
void UIControl::SetDirty(bool inDirty) { this->Call(C::SetDirty, inDirty); }
void UIControl::SetDisplayMode(unsigned long inMode) { this->Call(C::SetDisplayMode, inMode); }
void UIControl::SetTriggerEvent(unsigned long inEvent, UIEvent* inTriggerEvent)
{
    this->Call(C::SetTrigger, inEvent, (intptr_t)inTriggerEvent);
}

void UIControl::SetBgRectangle(const GERectangle& inRectangle)
{
    float f[4];
    UIObjHost::PutRect(f, inRectangle);
    this->Call(C::SetBgRectangle, 0, 0, f);
}

void UIControl::SetLocation(const GE2DPoint& inLocation)
{
    float f[4] = {inLocation.GetX(), inLocation.GetY(), 0.0f, 0.0f};
    this->Call(C::SetLocation, 0, 0, f);
}

void UIControl::SetRectangle(const GERectangle& inRectangle)
{
    float f[4];
    UIObjHost::PutRect(f, inRectangle);
    this->Call(C::SetRectangle, 0, 0, f);
}

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
    EeUiRect theRectangle = {inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    return ee_ui_draw_border(this->GetRasterizer(), this->GetScreenIndex(), theRectangle, inThickness,
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

// Returns the UI manager's drawing surface
GESurface* UIControl::GetDrawingSurface() const
{
    return this->GetForm()->GetManager()->GetDrawingSurface();
}

// Returns the form's mouse location
const GE2DPoint& UIControl::GetMouseLocation() const
{
    return this->GetForm()->GetMouseLocation();
}

// Returns the UI manager's rasterizer
GERasterizer* UIControl::GetRasterizer() const
{
    return this->GetForm()->GetManager()->GetRasterizer();
}

// Returns the UI manager's screen index
unsigned long UIControl::GetScreenIndex() const
{
    return this->GetForm()->GetManager()->GetScreen();
}

// Returns the UI manager's sound manager
SESoundManager* UIControl::GetSoundManager() const
{
    return this->GetForm()->GetManager()->GetSoundManager();
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
    if (UIForm::sShiftKey->JustPressed() || UIForm::sShiftKey->GetValue())
        return true;
    return false;
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
    case EeUiOp::ManagerBitmap: {
        UIManager* m = c->GetForm()->GetManager();
        GEBitmap* theBitmaps[18] = {m->GetSBBitmapDownNormal(),  m->GetSBBitmapUpNormal(),
                                    m->GetSBBitmapDownPressed(), m->GetSBBitmapUpPressed(),
                                    m->GetSBBitmapRightNormal(), m->GetSBBitmapLeftNormal(),
                                    m->GetSBBitmapRightPressed(), m->GetSBBitmapLeftPressed(),
                                    m->GetCheckOff(), m->GetCheckOn(), m->GetCheckDownOff(), m->GetCheckDownOn(),
                                    m->GetCheckNot(), m->GetCheckDownNot(),
                                    m->GetRadioOff(), m->GetRadioOn(), m->GetRadioDownOff(), m->GetRadioDownOn()};
        r = (intptr_t)theBitmaps[a];
        break;
    }
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
    case EeUiOp::BitmapSetLocation: theBitmap->SetLocation(GE2DPoint(f[0], f[1])); break;
    case EeUiOp::BitmapScale:
        f[0] = theBitmap->GetXScale();
        f[1] = theBitmap->GetYScale();
        break;
    case EeUiOp::BitmapSetScale: theBitmap->SetXYScale(f[0], f[1]); break;
    case EeUiOp::BitmapSetOpacity: theBitmap->SetOpacity(f[0]); break;
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

// The Rust mouse dispatch's view of a control (ee_ui mouse.rs): the buttons' entities, the button-down members and
// the protected handlers
struct UIControlRust {
    static long Mouse(UIControl* inControl, const GE2DPoint* inLocation, int inCall, int inArg)
    {
        ISEntity* theButton = inCall >= 4 && inCall < 8 ? UIManager::GetRMouseButton() : UIManager::GetLMouseButton();
        switch (inCall) {
        case 0:
        case 4:
            return theButton->JustDoublePressed();
        case 1:
        case 5:
            return theButton->JustUnpressed();
        case 2:
        case 6:
            return theButton->GetValue() != 0;
        case 3:
        case 7:
            return theButton->JustPressed();
        case 8:
            return inControl->mLeftButtonDown;
        case 9:
            return inControl->mRightButtonDown;
        case 10:
            inControl->mLeftButtonDown = inArg != 0;
            return 0;
        case 11:
            inControl->mRightButtonDown = inArg != 0;
            return 0;
        case 12:
            return inControl->MouseLButtonDoublePressed();
        case 13:
            return inControl->MouseLButtonUp();
        case 14:
            return inControl->MouseLButtonDown();
        case 15:
            return inControl->MouseRButtonUp();
        case 16:
            return inControl->MouseRButtonDown();
        default:
            return inControl->MouseMove(*inLocation);
        }
    }
};
extern "C" long ee_uicpp_control_mouse(void* inControl, const void* inLocation, int inCall, int inArg)
{
    return UIControlRust::Mouse(static_cast<UIControl*>(inControl), static_cast<const GE2DPoint*>(inLocation), inCall,
                                inArg);
}

// Dispatches left/right mouse button presses, releases and double clicks to the control, then the mouse move
// (ee_ui mouse.rs)
long UIControl::UpdateMouse(const GE2DPoint& inLocation)
{
    return ee_ui_control_update_mouse(this, &inLocation);
}

// Returns whether Up (or keypad 8 with Num Lock off) is held
bool UIControl::UpKeyPressed()
{
    return UIControl::sUpKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumUpKey->GetValue();
}
