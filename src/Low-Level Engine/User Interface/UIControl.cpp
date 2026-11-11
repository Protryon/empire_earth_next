// C:\EEX\Low-Level Engine\User Interface\UIControl.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10064ed6..0x10065f69: the U2D key init chains, the sDarkBorder/sLightBorder init chains, the ctor first, then the
// out-of-line members in alphabetical order, then the map<unsigned long, UIEvent*> COMDATs (~_Tree, clear, _Dec).
// UIControl's header-inline members are emitted first by UIBackground.cpp. UIControlsTable.cpp's inline block
// starts at 0x10065f69.
#include <windows.h>

#include "rust/ee_rust.h"

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
#include "lle/GELine.h"
#include "lle/TSCriticalSection.h"
#include "lle/UResourceFile.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

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

// FUNCTION: LLE 0x10065076
// Creates a shown, dirty control with a transparent black background; the first one looks up the shared UI keys
UIControl::UIControl(unsigned long inType, unsigned long inId, const GERectangle& inRectangle, UIForm* inForm,
                     unsigned long inIndex, unsigned long inBorderThickness)
    : UIEntity(1)
{
    this->mBorderOpacity = 1.0f;
    this->mControlType = inType;
    this->mCopiedBorderOpacity = 1.0f;
    this->mId = inId;
    this->mForm = inForm;
    this->mIndex = inIndex;
    this->mDisplayMode = kDisplayModeShown;
    this->mBorderThickness = inBorderThickness;
    this->mLeftButtonDown = false;
    this->mRightButtonDown = false;
    this->mAlwaysDirty = false;
    this->mDrawCount = 0;
    this->mIsDirty = true;
    this->mBorderType = 0;
    this->mCopiedBorderType = 0;
    this->mCopiedHasBgOpacity = false;
    this->mUnknown9C = false;
    this->mStringTableID = 0;
    this->mHotKey = NULL;
    this->mRectangle = inRectangle;
    this->mHasBgOpacity = false;
    this->mBgRhombus.SetColor(GEColor(0.0f, 0.0f, 0.0f));
    this->mBgRhombus.SetOpacity(0.0f);
    this->mBgRhombus.SetRectangle(GERectangle(GE2DPoint(this->mRectangle.GetLeft() - 0.5f, this->mRectangle.GetTop() - 0.5f),
                                        GE2DPoint(this->mRectangle.mRight + 0.5f, this->mRectangle.mBottom + 0.5f)));
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

// FUNCTION: LLE 0x100653ab
// Destroys the control, releasing every trigger event it holds
UIControl::~UIControl()
{
    std::map<unsigned long, UIEvent*>::iterator theIterator;
    for (theIterator = this->mTriggerEvents.begin(); theIterator != this->mTriggerEvents.end(); theIterator++) {
        UIEvent* theEvent = theIterator->second;
        theEvent->Uninitialize();
        theEvent->Dereference();
    }
    this->mTriggerEvents.clear();
}

// FUNCTION: LLE 0x10065418
// Gives the control the form's keyboard capture
void UIControl::CaptureKeyboard()
{
    this->mForm->SetKeyboardCaptured(this);
}

// FUNCTION: LLE 0x10065422
// Gives the control the form's mouse capture
void UIControl::CaptureMouse()
{
    this->mForm->SetMouseCaptured(this);
}

// FUNCTION: LLE 0x1006542c
// Snapshots the background, border and rectangle for drawing and counts the surfaces still to draw
long UIControl::CopyData()
{
    if (this->mHasBgOpacity) {
        this->mCopiedHasBgOpacity = true;
        this->mCopiedBgRhombus = this->mBgRhombus;
    } else
        this->mCopiedHasBgOpacity = false;
    if (this->mAlwaysDirty)
        this->SetDirty(true);
    else
        UIControl::SetDirty(false);
    this->mCopiedBorderOpacity = this->mBorderOpacity;
    this->mCopiedBorderType = this->mBorderType;
    this->mCopiedDrawRectangle = this->mRectangle;
    this->mDrawCount = this->mForm->GetManager()->GetNumberOfSurfaces();
    return kErrorOK;
}

// FUNCTION: LLE 0x100654b4
// Returns whether Delete (or keypad Del with Num Lock off) is held
bool UIControl::DeleteKeyPressed()
{
    return UIControl::sDeleteKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumDeleteKey->GetValue();
}

// FUNCTION: LLE 0x100654ea
// Returns whether Down (or keypad 2 with Num Lock off) is held
bool UIControl::DownKeyPressed()
{
    return UIControl::sDownKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumDownKey->GetValue();
}

// FUNCTION: LLE 0x10065520
// Draws a bevelled border of the given thickness, dark top-left and light bottom-right (swapped for border type 1):
// the rows on top, then the bottom, left and right ones, each line placed by Rust (ee_ui draw.rs)
long UIControl::DrawBorder(const GERectangle& inRectangle, unsigned long inThickness)
{
    long theResult = kErrorOK;
    GELine theLine;
    if (inThickness > 0) {
        theLine.SetOpacity(this->mCopiedBorderOpacity);
        EeUiRect theRectangle = {inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
        for (unsigned int theSide = 0; theSide < 4; theSide++)
            for (unsigned long i = 0; i < inThickness; i++) {
                EeUiBorderLine thePlan = ee_ui_border_line(theRectangle, theSide, i, this->mCopiedBorderType);
                theLine.SetColor(thePlan.light ? UIControl::sLightBorder : UIControl::sDarkBorder);
                theLine.SetStartPoint(GE2DPoint(thePlan.start.x, thePlan.start.y));
                theLine.SetEndPoint(GE2DPoint(thePlan.end.x, thePlan.end.y));
                theResult = this->mForm->GetManager()->GetRasterizer()->Draw(&theLine, 1, this->mForm->GetManager()->GetScreen());
            }
    }
    return theResult;
}

// FUNCTION: LLE 0x100657cf
// Draws the copied background and the border
long UIControl::DrawPolygons()
{
    if (this->mCopiedHasBgOpacity)
        this->mForm->GetManager()->GetRasterizer()->Draw(&this->mCopiedBgRhombus, this->mForm->GetManager()->GetScreen());
    return this->DrawBorder(this->mCopiedDrawRectangle, this->mBorderThickness);
}

// FUNCTION: LLE 0x1006580f
// Returns whether Enter or keypad Enter was just released
bool UIControl::EnterKeyPressed()
{
    return UIControl::sEnterKey->JustUnpressed() || UIControl::sNumEnterKey->JustUnpressed();
}

// FUNCTION: LLE 0x10065834
// Returns whether End (or keypad 1 with Num Lock off) is held
bool UIControl::EndKeyPressed()
{
    return UIControl::sEndKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumEndKey->GetValue();
}

// FUNCTION: LLE 0x1006586a
// Queues the control's trigger event for the given trigger, if one is set
void UIControl::FireTriggerEvent(unsigned long inEvent)
{
    UIEvent* theEvent = this->GetTriggerEvent(inEvent);
    if (theEvent)
        this->mForm->GetManager()->AddEventToTrigger(theEvent, this->mForm, this);
}

// FUNCTION: LLE 0x100658a5
// Returns the UI manager's drawing surface
GESurface* UIControl::GetDrawingSurface() const
{
    return this->mForm->GetManager()->GetDrawingSurface();
}

// FUNCTION: LLE 0x100658b3
// Returns whether the control at this control's form slot holds the keyboard capture
bool UIControl::GetHasKeyboardCapture() const
{
    return this->mForm->GetControl(this->mIndex) == this->mForm->GetKeyboardCaptured();
}

// FUNCTION: LLE 0x100658ce
// Returns whether the control at this control's form slot holds the mouse capture
bool UIControl::GetHasMouseCapture() const
{
    return this->mForm->GetControl(this->mIndex) == this->mForm->GetMouseCaptured();
}

// FUNCTION: LLE 0x100658e9
// Fetches the control's help string from the string table; false when it has none
bool UIControl::GetHelpString(UWideString& outString) const
{
    bool theResult = false;
    if (this->mStringTableID > 0) {
        this->mForm->GetManager()->GetStringTable()->GetString(this->mStringTableID, outString);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: LLE 0x10065913
// Returns whether the left mouse button is down on the control at this control's form slot
bool UIControl::GetLeftMouseButtonDown() const
{
    return this->mForm->GetControl(this->mIndex)->mLeftButtonDown;
}

// FUNCTION: LLE 0x10065929
// Returns the form's mouse location
const GE2DPoint& UIControl::GetMouseLocation() const
{
    return this->mForm->GetMouseLocation();
}

// FUNCTION: LLE 0x10065932
// Returns the UI manager's rasterizer
GERasterizer* UIControl::GetRasterizer() const
{
    return this->mForm->GetManager()->GetRasterizer();
}

// FUNCTION: LLE 0x10065942
// Returns whether the right mouse button is down on the control at this control's form slot
bool UIControl::GetRightMouseButtonDown() const
{
    return this->mForm->GetControl(this->mIndex)->mRightButtonDown;
}

// FUNCTION: LLE 0x10065958
// Returns the UI manager's screen index
unsigned long UIControl::GetScreenIndex() const
{
    return this->mForm->GetManager()->GetScreen();
}

// FUNCTION: LLE 0x10065968
// Returns the UI manager's sound manager
SESoundManager* UIControl::GetSoundManager() const
{
    return this->mForm->GetManager()->GetSoundManager();
}

// FUNCTION: LLE 0x10065978
// Returns the trigger event set for the given trigger, or NULL
UIEvent* UIControl::GetTriggerEvent(unsigned long inEvent)
{
    std::map<unsigned long, UIEvent*>::iterator theIterator = this->mTriggerEvents.find(inEvent);
    if (theIterator != this->mTriggerEvents.end())
        return theIterator->second;
    return NULL;
}

// FUNCTION: LLE 0x100659a2
// Returns whether Home (or keypad 7 with Num Lock off) is held
bool UIControl::HomeKeyPressed()
{
    return UIControl::sHomeKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumHomeKey->GetValue();
}

// FUNCTION: LLE 0x100659d8
// Returns whether Left (or keypad 4 with Num Lock off) is held
bool UIControl::LeftKeyPressed()
{
    return UIControl::sLeftKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumLeftKey->GetValue();
}

// FUNCTION: LLE 0x10065a0e
// Releases the form's keyboard capture if this control holds it
void UIControl::ReleaseCapturedKeyboard()
{
    if (this->mForm->GetKeyboardCaptured() == this)
        this->mForm->SetKeyboardCaptured(NULL);
}

// FUNCTION: LLE 0x10065a23
// Releases the form's mouse capture if this control holds it
void UIControl::ReleaseCapturedMouse()
{
    if (this->mForm->GetMouseCaptured() == this)
        this->mForm->SetMouseCaptured(NULL);
}

// FUNCTION: LLE 0x10065a33
// Returns whether Right (or keypad 6 with Num Lock off) is held
bool UIControl::RightKeyPressed()
{
    return UIControl::sRightKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumRightKey->GetValue();
}

// FUNCTION: LLE 0x10065a69
// Sets the background rectangle, grown by half a pixel on each side
void UIControl::SetBgRectangle(const GERectangle& inRectangle)
{
    this->mBgRhombus.SetRectangle(GERectangle(GE2DPoint(inRectangle.GetLeft() - 0.5f, inRectangle.GetTop() - 0.5f),
                                        GE2DPoint(inRectangle.GetRight() + 0.5f, inRectangle.GetBottom() + 0.5f)));
}

// FUNCTION: LLE 0x10065ab9
// Marks the control dirty (forwarding to the control owning its form slot) or clean
void UIControl::SetDirty(bool inDirty)
{
    if (inDirty) {
        UIControl* theControl = this->mForm->GetControl(this->mIndex);
        if (theControl && this != theControl)
            theControl->SetDirty(true);
        else
            this->mIsDirty = true;
    } else
        this->mIsDirty = false;
}

// FUNCTION: LLE 0x10065af5
// Shows or hides the control; hiding an active control on the current form dirties its area and tabs away from it
void UIControl::SetDisplayMode(unsigned long inMode)
{
    if (this->mDisplayMode != inMode) {
        this->mDrawCount = 0;
        if (inMode == kDisplayModeHidden && this->mForm->GetEntityGroupById(this->mId)->GetIsEnabled() &&
            this->mForm == this->mForm->GetManager()->GetCurrentForm()) {
            this->HideControl();
            this->mForm->GetManager()->AddDirtyRectangle(this->mRectangle, this->mForm);
            if (this->mForm->GetTabbingOn() && this->mForm->GetTabStop() == this->mIndex)
                this->mForm->Tab();
        } else if (this->mForm->GetEntityGroupById(this->mId)->GetIsEnabled())
            this->SetDirty(true);
    }
    this->mDisplayMode = inMode;
}

// FUNCTION: LLE 0x10065b94
// Moves the control to a new top-left, dirtying its old area when it is shown and enabled
void UIControl::SetLocation(const GE2DPoint& inLocation)
{
    UIControl* theControl = this->mForm->GetControl(this->mIndex);
    float theDeltaX = inLocation.GetX() - this->mRectangle.GetLeft();
    float theDeltaY = inLocation.GetY() - this->mRectangle.GetTop();
    UIEntityGroup* theGroup = this->mForm->GetEntityGroupById(this->mId);
    if (theControl && theControl == this && theControl->mDisplayMode == kDisplayModeShown && theGroup->GetIsEnabled())
        this->mForm->GetManager()->AddDirtyRectangle(this->mRectangle, this->mForm);
    this->mRectangle = GERectangle(this->mRectangle.GetLeft() + theDeltaX, this->mRectangle.GetTop() + theDeltaY,
                             this->mRectangle.GetRight() + theDeltaX, this->mRectangle.GetBottom() + theDeltaY);
    this->mBgRhombus.SetRectangle(GERectangle(GE2DPoint(this->mRectangle.GetLeft() - 0.5f, this->mRectangle.GetTop() - 0.5f),
                                        GE2DPoint(this->mRectangle.GetRight() + 0.5f, this->mRectangle.GetBottom() + 0.5f)));
    this->SetDirty(true);
}

// FUNCTION: LLE 0x10065c7a
// Sets the control's rectangle, dirtying the old area when it is shown and enabled
void UIControl::SetRectangle(const GERectangle& inRectangle)
{
    UIControl* theControl = this->mForm->GetControl(this->mIndex);
    UIEntityGroup* theGroup = this->mForm->GetEntityGroupById(this->mId);
    if (theControl && theControl == this && theControl->mDisplayMode == kDisplayModeShown && theGroup->GetIsEnabled())
        this->mForm->GetManager()->AddDirtyRectangle(theControl->mRectangle, this->mForm);
    this->mRectangle = inRectangle;
    this->SetLocation(inRectangle.GetTopLeft());
    this->mBgRhombus.SetRectangle(GERectangle(GE2DPoint(this->mRectangle.GetLeft() - 0.5f, this->mRectangle.GetTop() - 0.5f),
                                        GE2DPoint(this->mRectangle.GetRight() + 0.5f, this->mRectangle.GetBottom() + 0.5f)));
}

// FUNCTION: LLE 0x10065d4c
// Replaces the event fired for a trigger (NULL just removes it), releasing the old one
void UIControl::SetTriggerEvent(unsigned long inEvent, UIEvent* inTriggerEvent)
{
    std::map<unsigned long, UIEvent*>::iterator theIterator = this->mTriggerEvents.find(inEvent);
    if (theIterator != this->mTriggerEvents.end()) {
        UIEvent* theEvent = theIterator->second;
        theEvent->Uninitialize();
        theEvent->Dereference();
        this->mTriggerEvents.erase(theIterator);
    }
    if (!inTriggerEvent)
        return;
    inTriggerEvent->Reference();
    inTriggerEvent->Initialize();
    this->mTriggerEvents.insert(std::map<unsigned long, UIEvent*>::value_type(inEvent, inTriggerEvent));
}

// FUNCTION: LLE 0x10065dbd
// Returns whether Shift is held or was just pressed
bool UIControl::ShiftKeyPressed()
{
    if (UIForm::sShiftKey->JustPressed() || UIForm::sShiftKey->GetValue())
        return true;
    return false;
}

// FUNCTION: LLE 0x10065de1
// Dispatches left/right mouse button presses, releases and double clicks to the control, then the mouse move
long UIControl::UpdateMouse(const GE2DPoint& inLocation)
{
    if (UIManager::GetLMouseButton()->JustDoublePressed())
        this->MouseLButtonDoublePressed();
    else if (this->mLeftButtonDown &&
             (UIManager::GetLMouseButton()->JustUnpressed() || !UIManager::GetLMouseButton()->GetValue())) {
        this->mLeftButtonDown = false;
        this->MouseLButtonUp();
    } else if (UIManager::GetLMouseButton()->JustPressed()) {
        this->mLeftButtonDown = true;
        this->MouseLButtonDown();
    }
    if (this->mRightButtonDown &&
        (UIManager::GetRMouseButton()->JustUnpressed() || !UIManager::GetRMouseButton()->GetValue())) {
        this->mRightButtonDown = false;
        this->MouseRButtonUp();
    } else if (UIManager::GetRMouseButton()->JustPressed()) {
        this->mRightButtonDown = true;
        this->MouseRButtonDown();
    }
    return this->MouseMove(inLocation);
}

// FUNCTION: LLE 0x10065eb3
// Returns whether Up (or keypad 8 with Num Lock off) is held
bool UIControl::UpKeyPressed()
{
    return UIControl::sUpKey->GetValue() || UIForm::GetNumLockOff() && UIControl::sNumUpKey->GetValue();
}


// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10064ed6
// SYNTHETIC: LLE 0x10064ee0
// SYNTHETIC: LLE 0x10064ef0
// SYNTHETIC: LLE 0x10064efc
// SYNTHETIC: LLE 0x10064f06
// SYNTHETIC: LLE 0x10064f10
// SYNTHETIC: LLE 0x10064f20
// SYNTHETIC: LLE 0x10064f2c
// SYNTHETIC: LLE 0x10064f36
// SYNTHETIC: LLE 0x10064f40
// SYNTHETIC: LLE 0x10064f50
// SYNTHETIC: LLE 0x10064f5c
// SYNTHETIC: LLE 0x10064f66
// SYNTHETIC: LLE 0x10064f70
// SYNTHETIC: LLE 0x10064f80
// SYNTHETIC: LLE 0x10064f8c
// SYNTHETIC: LLE 0x10064f96
// SYNTHETIC: LLE 0x10064fa0
// SYNTHETIC: LLE 0x10064fb0
// SYNTHETIC: LLE 0x10064fbc
// SYNTHETIC: LLE 0x10064fc6
// SYNTHETIC: LLE 0x10064fd0
// SYNTHETIC: LLE 0x10064fe0
// SYNTHETIC: LLE 0x10064fec

// Compiler-generated: the sDarkBorder/sLightBorder init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10064ff6
// SYNTHETIC: LLE 0x10065000
// SYNTHETIC: LLE 0x10065019
// SYNTHETIC: LLE 0x10065025
// SYNTHETIC: LLE 0x10065036
// SYNTHETIC: LLE 0x10065040
// SYNTHETIC: LLE 0x10065059
// SYNTHETIC: LLE 0x10065065

// map<unsigned long, UIEvent*> COMDATs first emitted here (~_Tree, clear, const_iterator::_Dec)
// TEMPLATE: LLE 0x10065ee9
// TEMPLATE: LLE 0x10065f14
// TEMPLATE: LLE 0x10065f28
