// C:\EEX\Low-Level Engine\User Interface\UIScalableButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007c38e..0x1007c901: the U2D key init chains, the ctor (its ??_G follows: the class is not exported, so no ??_E
// and no implicit copy members), then CopyData, DrawPolygons and SetLocation. UIScrollBar.cpp's key chains start at
// 0x1007c901.
#include <windows.h>

#include "rust/ui/buttonobj.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIScalableButton.h"
#include "lle/UITextLabel.h"
#include "UIButtonRust.h"

typedef EeUiButtonCall C;
#define EE_UI_SCALABLE(c, ...) UIButtonRust::Call(this, C::c, ##__VA_ARGS__)

// Creates a scalable button from source rectangles for its normal, focused, pressed and text parts
UIScalableButton::UIScalableButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                                   GEBitmap* inPressedBitmap, const GERectangle& inNormalRectangle,
                                   const GERectangle& inFocusedRectangle, const GERectangle& inPressedRectangle,
                                   const GERectangle& inTextRectangle, const GERectangle& inRectangle,
                                   UIForm* inForm, unsigned long inIndex)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap, inRectangle, inForm, inIndex, 0)
{
    const GERectangle* theRectangles[3] = {&inNormalRectangle, &inFocusedRectangle, &inPressedRectangle};
    for (int i = 0; i < 3; i++)
        EE_UI_SCALABLE(ScalableInit, i, 0, theRectangles[i]->mBottom, theRectangles[i]->mLeft, theRectangles[i]->mRight,
                       theRectangles[i]->mTop);
    this->SetTextRectangle(inTextRectangle);
}

// Picks the source rectangle for the button state (focused falls back to normal without a focus bitmap), then snapshots
long UIScalableButton::CopyData() { return (long)EE_UI_SCALABLE(ScalableCopyData); }
// Draws the button as a left cap, a right cap and a stretched middle cut from its bitmap; returns the last draw result
long UIScalableButton::DrawPolygons() { return (long)EE_UI_SCALABLE(ScalableDrawPolygons); }
// Moves the button (plain button behaviour)
void UIScalableButton::SetLocation(const GE2DPoint& inLocation) { UIButton::SetLocation(inLocation); }
