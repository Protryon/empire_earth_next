// C:\EEX\Low-Level Engine\User Interface\UIScalableButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007c38e..0x1007c901: the U2D key init chains, the ctor (its ??_G follows: the class is not exported, so no ??_E
// and no implicit copy members), then CopyData, DrawPolygons and SetLocation. UIScrollBar.cpp's key chains start at
// 0x1007c901.
#include <windows.h>

#include "rust/ee_ui.h"
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

// The Rust slices' records (rust/crates/ee_ui/src/button_variants.rs) and the GE types
struct UIScalableButtonRust {
    static EeUiRect Rect(const GERectangle& inRectangle)
    {
        return EeUiRect{inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    }
    static GERectangle Rect(const EeUiRect& inRectangle)
    {
        return GERectangle(inRectangle.left, inRectangle.top, inRectangle.right, inRectangle.bottom);
    }
    static GE2DPoint Point(const EeUiPoint& inPoint) { return GE2DPoint(inPoint.x, inPoint.y); }
};

// FUNCTION: LLE 0x1007c4ae
// Creates a scalable button from source rectangles for its normal, focused, pressed and text parts
UIScalableButton::UIScalableButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                                   GEBitmap* inPressedBitmap, const GERectangle& inNormalRectangle,
                                   const GERectangle& inFocusedRectangle, const GERectangle& inPressedRectangle,
                                   const GERectangle& inTextRectangle, const GERectangle& inRectangle,
                                   UIForm* inForm, unsigned long inIndex)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap, inRectangle, inForm, inIndex, 0)
{
    this->mNormalRectangle = inNormalRectangle;
    this->mFocusedRectangle = inFocusedRectangle;
    this->mPressedRectangle = inPressedRectangle;
    this->mTextRectangle = inTextRectangle;
}

// FUNCTION: LLE 0x1007c609
// Picks the source rectangle for the button state (focused falls back to normal without a focus bitmap), then snapshots
long UIScalableButton::CopyData()
{
    EeUiRect theNormal = UIScalableButtonRust::Rect(this->mNormalRectangle);
    EeUiRect theFocused = UIScalableButtonRust::Rect(this->mFocusedRectangle);
    EeUiRect thePressed = UIScalableButtonRust::Rect(this->mPressedRectangle);
    EeUiRect theCurrent = UIScalableButtonRust::Rect(this->mCopiedRectangle);
    this->mCopiedRectangle = UIScalableButtonRust::Rect(ee_ui_scalable_copied_rect(this->mState, this->mFocusedBitmap != NULL, &theNormal,
                                                                           &theFocused, &thePressed, &theCurrent));
    return UIButton::CopyData();
}

// FUNCTION: LLE 0x1007c65b
// Draws the button as a left cap, a right cap and a stretched middle cut from its bitmap; returns the last draw result
long UIScalableButton::DrawPolygons()
{
    GERectangle theSource;
    GERectangle theDestination;
    long theResult = kErrorOK;
    if (this->mCopiedBitmap) {
        // the left cap, the right cap and the stretched middle (ee_ui button_variants.rs)
        EeUiSlice theSlices[3];
        EeUiRect theCopied = UIScalableButtonRust::Rect(this->mCopiedRectangle);
        EeUiRect theDraw = UIScalableButtonRust::Rect(this->mCopiedDrawRectangle);
        ee_ui_scalable_slices(&theCopied, &theDraw, this->mCopiedBitmap->GetTexture(this->GetScreenIndex())->GetWidth(), theSlices);
        for (int i = 0; i < 3; i++) {
            theSource = UIScalableButtonRust::Rect(theSlices[i].source);
            theDestination = UIScalableButtonRust::Rect(theSlices[i].destination);
            this->mCopiedBitmap->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, this->GetScreenIndex());
            this->mCopiedBitmap->SetLocation(UIScalableButtonRust::Point(theSlices[i].location));
            theResult = this->GetRasterizer()->Draw(this->mCopiedBitmap, this->GetScreenIndex());
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1007c8f5
// Moves the button (plain button behaviour)
void UIScalableButton::SetLocation(const GE2DPoint& inLocation)
{
    UIButton::SetLocation(inLocation);
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x1007c5e7
// SYNTHETIC: LLE 0x1007c38e
// SYNTHETIC: LLE 0x1007c398
// SYNTHETIC: LLE 0x1007c3a8
// SYNTHETIC: LLE 0x1007c3b4
// SYNTHETIC: LLE 0x1007c3be
// SYNTHETIC: LLE 0x1007c3c8
// SYNTHETIC: LLE 0x1007c3d8
// SYNTHETIC: LLE 0x1007c3e4
// SYNTHETIC: LLE 0x1007c3ee
// SYNTHETIC: LLE 0x1007c3f8
// SYNTHETIC: LLE 0x1007c408
// SYNTHETIC: LLE 0x1007c414
// SYNTHETIC: LLE 0x1007c41e
// SYNTHETIC: LLE 0x1007c428
// SYNTHETIC: LLE 0x1007c438
// SYNTHETIC: LLE 0x1007c444
// SYNTHETIC: LLE 0x1007c44e
// SYNTHETIC: LLE 0x1007c458
// SYNTHETIC: LLE 0x1007c468
// SYNTHETIC: LLE 0x1007c474
// SYNTHETIC: LLE 0x1007c47e
// SYNTHETIC: LLE 0x1007c488
// SYNTHETIC: LLE 0x1007c498
// SYNTHETIC: LLE 0x1007c4a4
