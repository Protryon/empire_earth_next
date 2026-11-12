// C:\EEX\Empire Earth\UI Events\EEUIEScrollZoomEvents.cpp (file name inferred: no __FILE__; RTTI classes
// EEUIEGrabScroll, EEUIEKeyboardScroll, EEUIEHotKeyZoom). The camera's grab scroll, keyboard scroll and hot-key zoom
// events: 0x1d99ff..0x1d9d67. One unit, not three: their constants are one .rdata run right after the game settings
// unit's vtables (0x446480 0.05 keyboard, 0x446484 0.07 grab, 0x446488/0x44648c zoom), so the keyboard's constant
// precedes the grab's although the grab's code comes first (definition order in one object). Their ctors/??_G sit with
// their creators elsewhere. 0x1d9d67 (PlayerTypeFromMenu, called only by the lobby: 0x1db039, 0x1db068) is the first
// function of EEUIELobbyEvents.cpp (now ee_screens lobby_form.rs), so this unit ends at 0x1d9d67. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

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
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMouse.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEUIEGrabScroll.h"
#include "ee/EEUIEKeyboardScroll.h"
#include "ee/EEUIEHotKeyZoom.h"

static const float kKeyboardScrollFactor = 0.05f;
static const float kGrabScrollFactor = 0.07f;
static const float kZoomStepMinimum = 0.0035f;
static const float kZoomStepScale = 0.00649999967f;

// FUNCTION: EXE 0x5d99ff
// Middle-button grab scroll: while held, scrolls the camera by the mouse motion (scaled by zoom and grab speed)
// and takes over mouse-move events with the cursor frozen; release restores them
long EEUIEGrabScroll::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    ISEntity* theX;
    ISEntity* theY;
    ISEntity* theButton;
    gISMouse->GetEntity(kMouseXAxis, theX);
    gISMouse->GetEntity(kMouseYAxis, theY);
    gISMouse->GetEntity(kMouseMiddleButton, theButton);
    if (!theButton)
        return kErrorOK;
    if (this->mIsGrabbing) {
        float theSpeed = gGamePreferences.mSettings.GetGrabScrollSpeed();
        float theZoom = gWorld.mCameraControl.GetZoom();
        float theDX = theX->GetValue() * (theZoom + 0.5f) * (theSpeed + 0.5f) * kGrabScrollFactor;
        float theDY = -(theY->GetValue() * (gGamePreferences.mSettings.GetGrabScrollSpeed() + 0.5f)
                        * (gWorld.mCameraControl.GetZoom() + 0.5f) * kGrabScrollFactor);
        if (theDX != 0.0f || theDY != 0.0f)
            gWorld.mCameraControl.Scroll(theDX, theDY);
        if (theButton->GetValue() == 0) {
            this->mIsGrabbing = false;
            inForm->SetMouseMoveEvent(this->mSavedMouseMoveEvent);
            if (this->mSavedMouseMoveEvent) {
                this->mSavedMouseMoveEvent->Dereference();
                this->mSavedMouseMoveEvent = NULL;
            }
            gMouse->mIsFrozen = false;
        }
    } else if (theButton->GetValue() == 1) {
        this->mIsGrabbing = true;
        this->mSavedMouseMoveEvent = inForm->GetMouseMoveEvent();
        if (this->mSavedMouseMoveEvent)
            this->mSavedMouseMoveEvent->Reference();
        inForm->SetMouseMoveEvent(this);
        gMouse->mIsFrozen = true;
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5d9b6f
// Scrolls the camera one step up, down, left or right, scaled by zoom and the grab scroll speed setting
long EEUIEKeyboardScroll::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    float theDX = 0.0f;
    float theDY = 0.0f;
    switch (this->mDirection) {
    case 0:
        theDY = (gGamePreferences.mSettings.GetGrabScrollSpeed() + 0.5f) * (gWorld.mCameraControl.GetZoom() + 0.5f)
                * kKeyboardScrollFactor;
        break;
    case 1:
        theDY = -((gWorld.mCameraControl.GetZoom() + 0.5f)
                  * ((gGamePreferences.mSettings.GetGrabScrollSpeed() + 0.5f) * kKeyboardScrollFactor));
        break;
    case 2:
        theDX = -((gWorld.mCameraControl.GetZoom() + 0.5f)
                  * ((gGamePreferences.mSettings.GetGrabScrollSpeed() + 0.5f) * kKeyboardScrollFactor));
        break;
    case 3:
        theDX = (gGamePreferences.mSettings.GetGrabScrollSpeed() + 0.5f) * (gWorld.mCameraControl.GetZoom() + 0.5f)
                * kKeyboardScrollFactor;
        break;
    }
    gWorld.mCameraControl.Scroll(theDX, theDY);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d9c3b
// Zooms the camera one step in or out (step grows with zoom), clamped to 0..1; in camera mode 3 (outside a
extern float gCameraMaxZoom;   // port: EECameraControl.cpp

// cinematic) returns to mode 0 at the default zoom instead; nothing when zooming is disabled
long EEUIEHotKeyZoom::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    float theZoom = gWorld.mCameraControl.GetZoom();
    float theAngle = theZoom * 1.5707964f;
    float theStep = min(max(UMCos(1.5707964f - theAngle), 0.0f), 1.0f);
    if (gGamePreferences.mSettings.mZoomEnabled) {
        if (!gWorld.mInCinematic && gWorld.mCameraControl.mMode == 3) {
            gWorld.mCameraControl.SetMode(0);
            gWorld.mCameraControl.ResetZoom();
            gWorld.mCameraControl.SetChanged();
        } else {
            theStep = theStep * kZoomStepScale + kZoomStepMinimum;
            if (this->mDirection == 0)
                theZoom = theZoom - theStep;
            else
                theZoom = theZoom + theStep;
            theZoom = min(max(theZoom, 0.0f), gCameraMaxZoom);   // port: the farther zoom-out (1.0 in the original)
            gWorld.mCameraControl.SetZoom(theZoom);
        }
    }
    return kErrorOK;
}
