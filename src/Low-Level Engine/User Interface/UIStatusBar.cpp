// C:\EEX\Low-Level Engine\User Interface\UIStatusBar.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007db5b..0x1007e312: the U2D key init chains, the ctor, the dtor, then the out-of-line members in alphabetical
// order. The header-inline members (and the implicit copy members, ??_E) are emitted first by UIControlsTable.cpp.
//
// The arithmetic is Rust (rust/crates/ee_ui/src/status.rs, docs/RUST_PLAN.md R6): the value's clamp and the fill's
// width, the rectangles a move lays out, CopyData's inset fill and centred text. The rhombi, the texts and every
// draw stay here.
#include <windows.h>

#include "rust/ee_ui.h"
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
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIStatusBar.h"

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top)
static inline EeUiRect UIStatusRect(const GERectangle& r)
{
    return EeUiRect{r.GetBottom(), r.GetLeft(), r.GetRight(), r.GetTop()};
}
static inline GERectangle UIStatusRect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }

// FUNCTION: LLE 0x1007dc7b
// Creates a progress bar control: empty range, black fill, white text and a text area inset by the border
UIStatusBar::UIStatusBar(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                         unsigned long inBorderThickness)
    : UIControl(kControlTypeStatusBar, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mMinRange = 0;
    this->mMaxRange = 0;
    this->mCurrentRangeValue = 0;
    this->mText = NULL;
    this->mCopiedText = NULL;
    this->mHasCopiedText = false;
    this->mTextColor = GEColor(1.0f, 1.0f, 1.0f);
    this->mStatusRhombus.SetRectangle(GERectangle(inRectangle.GetLeft() - 0.5f, inRectangle.GetTop() - 0.5f,
                                            inRectangle.GetRight() + 0.5f, inRectangle.GetBottom() + 0.5f));
    this->mStatusRhombus.SetOpacity(1.0f);
    this->mStatusRhombus.SetColor(GEColor(0.0f, 0.0f, 0.0f));
    this->mTextRectangle.SetLeft(this->mRectangle.GetLeft() + inBorderThickness);
    this->mTextRectangle.SetTop(this->mRectangle.GetTop() + inBorderThickness);
    this->mTextRectangle.SetRight(this->mRectangle.GetRight() - inBorderThickness);
    this->mTextRectangle.SetBottom(this->mRectangle.GetBottom() - inBorderThickness);
}

// FUNCTION: LLE 0x1007ddef
// Destroys the status bar and its text
UIStatusBar::~UIStatusBar()
{
    if (this->mText) {
        delete this->mText;
        delete this->mCopiedText;
    }
}

// FUNCTION: LLE 0x1007de39
// Changes the text font
void UIStatusBar::ChangeFont(GEFont* inFont)
{
    if (this->mText) {
        this->mText->SetFont(inFont);
        this->mCopiedText->SetFont(inFont);
    }
}

// FUNCTION: LLE 0x1007de62
// Snapshots the fill (inset by the border) and the text, centred in the text area, for drawing
long UIStatusBar::CopyData()
{
    GERectangle theRectangle;
    unsigned long theBorder = this->mBorderThickness;
    long theResult = UIControl::CopyData();
    this->mCopiedStatusRhombus = this->mStatusRhombus;
    EeUiRect theFill;
    if (ee_ui_status_copy_fill(UIStatusRect(this->mCopiedStatusRhombus.GetRectangle()), theBorder, &theFill)) {
        theRectangle = UIStatusRect(theFill);
        this->mCopiedStatusRhombus.SetRectangle(theRectangle);
    }
    if (this->mText && this->mText->GetText().GetLength() > 0) {
        const GERectangle& theExtent = this->mText->GetTextExtent();
        this->mCopiedTextRectangle = UIStatusRect(ee_ui_status_copy_text(UIStatusRect(this->mTextRectangle),
                                                                         theExtent.GetWidth(), theExtent.GetHeight()));
        *this->mCopiedText = *this->mText;
        this->mHasCopiedText = true;
    } else
        this->mHasCopiedText = false;
    return theResult;
}

// FUNCTION: LLE 0x1007e008
// Draws the frame and the fill when it has width
long UIStatusBar::DrawPolygons()
{
    long theResult = UIControl::DrawPolygons();
    if (this->mCopiedStatusRhombus.GetRectangle().GetWidth() > 0.0f)
        theResult = this->GetRasterizer()->Draw(&this->mCopiedStatusRhombus, this->GetScreenIndex());
    return theResult;
}

// FUNCTION: LLE 0x1007e052
// Draws the frame text and the bar's text clipped to its area
long UIStatusBar::DrawTextA()
{
    long theResult = UIControl::DrawTextA();
    if (this->mHasCopiedText) {
        this->mCopiedText->SetLocation(this->mCopiedTextRectangle.GetTopLeft());
        theResult = this->GetForm()->DrawClippedText(this->mCopiedTextRectangle, this->mCopiedText, this);
    }
    return theResult;
}

// FUNCTION: LLE 0x1007e0a1
// Sets the progress value, clamped to the range, sizing the fill to match (at least 1 pixel above 0)
void UIStatusBar::SetCurrentRangeValue(long inValue)
{
    EeUiRect theFill;
    inValue = ee_ui_status_fill(inValue, this->mMinRange, this->mMaxRange, this->GetWidth(),
                                UIStatusRect(this->mStatusRhombus.GetRectangle()), &theFill);
    this->mStatusRhombus.SetRectangle(UIStatusRect(theFill));
    if (this->mCurrentRangeValue != inValue) {
        this->mCurrentRangeValue = inValue;
        this->SetDirty(true);
    }
}

// FUNCTION: LLE 0x1007e18d
// Moves the bar, resizing its fill and text area
void UIStatusBar::SetLocation(const GE2DPoint& inLocation)
{
    UIControl::SetLocation(inLocation);
    EeUiRect theFill;
    EeUiRect theText;
    ee_ui_status_location(UIStatusRect(this->mRectangle), this->GetBorderThickness(), &theFill, &theText);
    this->mStatusRhombus.SetRectangle(UIStatusRect(theFill));
    this->SetCurrentRangeValue(this->mCurrentRangeValue);
    this->mTextRectangle = UIStatusRect(theText);
}

// FUNCTION: LLE 0x1007e27f
// Sets the value range and resets the value to the minimum
void UIStatusBar::SetRange(unsigned long inMin, unsigned long inMax)
{
    this->mMinRange = inMin;
    this->mMaxRange = inMax;
    this->SetCurrentRangeValue(inMin);
}

// FUNCTION: LLE 0x1007e29c
// Sets the bar's text, creating its text objects with the font on first use
void UIStatusBar::SetText(const UWideString& inText, GEFont* inFont)
{
    if (this->mText == NULL) {
        this->mText = new GEText(inFont);
        this->mCopiedText = new GEText(inFont);
    }
    this->mText->SetText(inText);
    this->SetDirty(true);
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1007db5b
// SYNTHETIC: LLE 0x1007db65
// SYNTHETIC: LLE 0x1007db75
// SYNTHETIC: LLE 0x1007db81
// SYNTHETIC: LLE 0x1007db8b
// SYNTHETIC: LLE 0x1007db95
// SYNTHETIC: LLE 0x1007dba5
// SYNTHETIC: LLE 0x1007dbb1
// SYNTHETIC: LLE 0x1007dbbb
// SYNTHETIC: LLE 0x1007dbc5
// SYNTHETIC: LLE 0x1007dbd5
// SYNTHETIC: LLE 0x1007dbe1
// SYNTHETIC: LLE 0x1007dbeb
// SYNTHETIC: LLE 0x1007dbf5
// SYNTHETIC: LLE 0x1007dc05
// SYNTHETIC: LLE 0x1007dc11
// SYNTHETIC: LLE 0x1007dc1b
// SYNTHETIC: LLE 0x1007dc25
// SYNTHETIC: LLE 0x1007dc35
// SYNTHETIC: LLE 0x1007dc41
// SYNTHETIC: LLE 0x1007dc4b
// SYNTHETIC: LLE 0x1007dc55
// SYNTHETIC: LLE 0x1007dc65
// SYNTHETIC: LLE 0x1007dc71
