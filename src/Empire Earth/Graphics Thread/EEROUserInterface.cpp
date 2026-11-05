// C:\EEX\Empire Earth\Graphics Thread\EEROUserInterface.cpp (no __FILE__; name from the RTTI class). The render
// object that draws the UI (vtable 0x441bec): 0x118741..0x118aa2 (the viewport's init chain, the ctor, ??_G, the
// dtor, the four slots); EEUnitPicker.cpp (EEROWater first) starts at 0x118aa2. The three floats after the vtable
// (0x441c00..0x441c08) are this unit's named consts. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETransformation.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGraphics.h"
#include "ee/RTSGraphics.h"
#include "ee/EERenderObject.h"
#include "ee/EEROEffects.h"
#include "ee/EEROUserInterface.h"

// the UI's viewport (0x529518) (provisional name)
GEViewport gUIViewport;
// SYNTHETIC: EXE 0x518741   (wrapper)
// SYNTHETIC: EXE 0x51874b   (ctor)
// SYNTHETIC: EXE 0x518756   (registrar)
// SYNTHETIC: EXE 0x518762   (dtor)

// the panel corners of the z-buffer clears (.rdata 0x441c00.., reloaded per use: named consts) (provisional names)
const float kUnitPanelRight = 0.17f;
const float kPanelTop = 0.71666664f;
const float kRightPanelLeft = 0.8275f;

// the current player has a selection (FrameStarting sets it, EEROUnits reads it; 0x5297a0) (provisional name)
bool gUnitPanelActive;

// FUNCTION: EXE 0x51876d
// Creates the UI's render object for a UI manager
EEROUserInterface::EEROUserInterface(UIManager* inManager)
{
    this->mUIManager = inManager;
}

// SYNTHETIC: EXE 0x518788
// (the scalar deleting destructor, emitted with the vtable; slot EEROUserInterface+0x10)

// FUNCTION: EXE 0x5187a4
// Destroys the UI's render object
EEROUserInterface::~EEROUserInterface()
{
}

// FUNCTION: EXE 0x5187af
// Copies the UI's frame data and notes whether the current player has a selection or a viewed unit (the unit
// panel is shown)
void EEROUserInterface::FrameStarting()
{
    this->mUIManager->CopyData();
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (thePlayer != NULL &&
        ((thePlayer->mSelectedGroup != NULL && thePlayer->mSelectedGroup->GetFirstUnit() != NULL) ||
         gWorld.GetCurrentPlayer()->mSelectedViewedUnit != NULL))
        gUnitPanelActive = true;
    else
        gUnitPanelActive = false;
}

// FUNCTION: EXE 0x518802
// Clears the z buffer under the bottom-right panel, and under the unit panel when it is shown, so the
// world draws behind them (not with the free camera or in UI state 0x11)
void EEROUserInterface::SceneStarting()
{
    GERectangle theZero = gU4E8F08.GetViewRectangle();
    GERectangle theRectangle;
    if (gFreeCamera || gUIManager->mMainGameForm->mUIState == 0x11)
        return;
    theRectangle.SetLeft(gGameWindowWidth * kRightPanelLeft);
    theRectangle.SetTop(gGameWindowHeight * kPanelTop);
    theRectangle.SetRight(theZero.GetRight());
    theRectangle.SetBottom(theZero.GetBottom());
    gRasterizer->ClearZBuffer(0.0f, theRectangle, gScreenIndex);
    if (!gUnitPanelActive)
        return;
    theRectangle.SetLeft(0.0f);
    theRectangle.SetTop(gGameWindowHeight * kPanelTop);
    theRectangle.SetRight(gGameWindowWidth * kUnitPanelRight);
    theRectangle.SetBottom(theZero.GetBottom());

    gRasterizer->ClearZBuffer(0.0f, theRectangle, gScreenIndex);
}

// FUNCTION: EXE 0x518907
// Draws the UI's polygons over the whole render target with fog, lighting, alpha test and z buffer off,
// restoring the fog and alpha reference (lighting stays off)
void EEROUserInterface::RenderScene()
{
    unsigned long theFog = gRasterizer->GetFog(gScreenIndex);
    float theAlphaRefValue = gRasterizer->GetAlphaTestingRefValue(gScreenIndex);
    gRasterizer->SetFog(0, gScreenIndex);
    gRasterizer->SetShadingColorSource(1, gScreenIndex);
    gRasterizer->SetLighting(0, gScreenIndex);
    gRasterizer->SetAlphaTestingRefValue(0.0f, gScreenIndex);
    gRasterizer->SetZBuffer(kZBufferDisabled, gScreenIndex);
    gRasterizer->SetCulling(kCullNone);
    GERectangle theRectangle(0.0f, 0.0f, (float)gRasterizer->GetRenderTarget(gScreenIndex)->GetWidth(),
                             (float)gRasterizer->GetRenderTarget(gScreenIndex)->GetHeight());
    gUIViewport.SetViewRectangle(theRectangle);
    gRasterizer->SetViewport(gUIViewport, gScreenIndex);
    this->mUIManager->DrawPolygons();
    gRasterizer->SetFog(theFog, gScreenIndex);
    gRasterizer->SetAlphaTestingRefValue(theAlphaRefValue, gScreenIndex);
}

// FUNCTION: EXE 0x518a99
// Draws the UI's text
void EEROUserInterface::FrameDone()
{
    this->mUIManager->DrawText();
}
