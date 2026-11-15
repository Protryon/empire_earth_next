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

// FUNCTION: LLE 0x1006e038
// Creates the mouse-wheel scroll event
UIEMouseWheelScroll::UIEMouseWheelScroll()
{
}

// FUNCTION: LLE 0x1006e04c
// Destroys the mouse-wheel scroll event
UIEMouseWheelScroll::~UIEMouseWheelScroll()
{
}

// FUNCTION: LLE 0x1006e053
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

// Implicit copy ctor and ??_E (emitted first)
// FUNCTION: LLE 0x1006de9d
// SYNTHETIC: LLE 0x1006debe

// UIHotKey's header-inline members, implicit copy members and ??_E
// FUNCTION: LLE 0x1006e075
// FUNCTION: LLE 0x1006e0b3
// FUNCTION: LLE 0x1006e138
// FUNCTION: LLE 0x1006e16b
// FUNCTION: LLE 0x1006e16f
// FUNCTION: LLE 0x1006e17d
// FUNCTION: LLE 0x1006e195
// FUNCTION: LLE 0x1006e19b
// FUNCTION: LLE 0x1006e1a5
// FUNCTION: LLE 0x1006e1af
// FUNCTION: LLE 0x1006e1cf
// FUNCTION: LLE 0x1006e1df
// FUNCTION: LLE 0x1006e243
// SYNTHETIC: LLE 0x1006e2a9

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1006df18
// SYNTHETIC: LLE 0x1006df22
// SYNTHETIC: LLE 0x1006df32
// SYNTHETIC: LLE 0x1006df3e
// SYNTHETIC: LLE 0x1006df48
// SYNTHETIC: LLE 0x1006df52
// SYNTHETIC: LLE 0x1006df62
// SYNTHETIC: LLE 0x1006df6e
// SYNTHETIC: LLE 0x1006df78
// SYNTHETIC: LLE 0x1006df82
// SYNTHETIC: LLE 0x1006df92
// SYNTHETIC: LLE 0x1006df9e
// SYNTHETIC: LLE 0x1006dfa8
// SYNTHETIC: LLE 0x1006dfb2
// SYNTHETIC: LLE 0x1006dfc2
// SYNTHETIC: LLE 0x1006dfce
// SYNTHETIC: LLE 0x1006dfd8
// SYNTHETIC: LLE 0x1006dfe2
// SYNTHETIC: LLE 0x1006dff2
// SYNTHETIC: LLE 0x1006dffe
// SYNTHETIC: LLE 0x1006e008
// SYNTHETIC: LLE 0x1006e012
// SYNTHETIC: LLE 0x1006e022
// SYNTHETIC: LLE 0x1006e02e
