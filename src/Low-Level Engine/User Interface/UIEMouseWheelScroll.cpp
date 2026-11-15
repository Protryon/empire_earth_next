// C:\EEX\Low-Level Engine\User Interface\UIEMouseWheelScroll.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006de9d..0x1006e304: the implicit copy ctor and ??_E, the U2D key init chains, the ctor, dtor and ProcessEvent,
// then UIHotKey's header-inline members (this is the first unit that includes lle/UIHotKey.h) and its ??_E.
// UIEntityGroup.cpp's key chains start at 0x1006e304.
#include <windows.h>

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
#include "lle/TSCriticalSection.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEMouseWheelScroll.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIHotKey.h"

// Creates the mouse-wheel scroll event
UIEMouseWheelScroll::UIEMouseWheelScroll()
{
}

// Destroys the mouse-wheel scroll event
UIEMouseWheelScroll::~UIEMouseWheelScroll()
{
}

// Scrolls the control under the mouse by the manager's wheel scroll amount
long UIEMouseWheelScroll::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIControl* theControl = inForm->GetMouseOverControl();
    if (theControl) {
        long theAmount = UIManager::GetMouseWheelScrollAmount();
        theControl->MouseWheelTurned(theAmount);
    }
    return kErrorOK;
}
