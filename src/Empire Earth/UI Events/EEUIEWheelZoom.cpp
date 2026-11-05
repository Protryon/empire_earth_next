// C:\EEX\Empire Earth\UI Events\EEUIEWheelZoom.cpp (file name inferred: no __FILE__; RTTI class EEUIEWheelZoom).
// The mouse wheel zooms the camera: 0x1e9f40..0x1e9fed. Unit flags: the exe module set.
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
#include "ee/EEUIEWheelZoom.h"

extern float gCameraMaxZoom;   // port: EECameraControl.cpp

// FUNCTION: EXE 0x5e9f40
// Zooms the camera by the mouse wheel (clamped to 0..1); in camera mode 3 outside a cinematic, resets the camera
long EEUIEWheelZoom::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    float theZoom = gWorld.mCameraControl.mZoom;
    ISEntity* theWheel;
    if (!gZoomEnabled)
        return kErrorOK;
    gISMouse->GetEntity(kMouseZAxis, theWheel);
    if (!theWheel)
        return kErrorOK;
    if (!gWorld.mInCinematic && gWorld.mCameraControl.mMode == 3) {
        gWorld.mCameraControl.SetMode(0);
        gWorld.mCameraControl.ResetZoom();
        gWorld.mCameraControl.SetChanged();
    } else {
        theZoom = theZoom - theWheel->GetNormalizedValue() * 0.05f;
        if (theZoom < 0.0f)
            theZoom = 0.0f;
        else if (theZoom > gCameraMaxZoom)   // port: the farther zoom-out (1.0 in the original)
            theZoom = gCameraMaxZoom;
        gWorld.mCameraControl.SetZoom(theZoom);
    }
    return kErrorOK;
}
