// C:\EEX\Low-Level Engine\User Interface\UIIMECandidate.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100774b9..0x10077f75: the U2D key init chains, the ctor, the dtor, then the out-of-line members in alphabetical
// order. The implicit copy members and ??_E are emitted by UIForm.cpp (first includer of UIIMECandidate.h).
// UILayoutUtility.cpp starts at 0x10077f75.
#include <windows.h>
#include <imm.h>
#include <mbstring.h>

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
#include "lle/GELine.h"
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
#include "lle/UIIMECandidate.h"

// FUNCTION: LLE 0x100775d9
// Creates the IME candidate window with its screen-size ratios and font, a blue selection bar and black background
UIIMECandidate::UIIMECandidate(UIManager* inManager, float inHeightRatio, float inWidthRatio, GEFont* inFont)
    : mManager(inManager), mHeightRatio(inHeightRatio)
{
    this->mWidthRatio = inWidthRatio;
    this->mText = new GEText(inFont);
    this->mText->SetIsTransparent(true);
    this->mCopyText = new GEText(inFont);
    this->mSelectionRhombus.SetColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mBackgroundRhombus.SetColor(GEColor(0.0f, 0.0f, 0.0f));
}

// FUNCTION: LLE 0x100776d7
// Destroys the candidate window's texts
UIIMECandidate::~UIIMECandidate()
{
    if (this->mText)
        delete this->mText;
    if (this->mCopyText)
        delete this->mCopyText;
}

// FUNCTION: LLE 0x1007771e
// Redraws what the candidate window covered on the current form
void UIIMECandidate::CloseCandidateWindow()
{
    this->mManager->AddDirtyRectangle(this->mCandidateRectangle, this->mManager->GetCurrentForm());
    this->mManager->GetCurrentForm()->RedrawIntersectControls(this->mCandidateRectangle);
}

// FUNCTION: LLE 0x1007774a
// Copies the candidate lines, text, rectangle and selection for drawing
long UIIMECandidate::CopyData()
{
    this->mCopyCandidateStrings = this->mCandidateStrings;
    *this->mCopyText = *this->mText;
    this->mCopyCandidateRectangle = this->mCandidateRectangle;
    this->mCopySelection = this->mSelection;
    return kErrorOK;
}

// FUNCTION: LLE 0x1007778c
// Draws the candidate window background, the selection bar and a two-pixel bevelled border
long UIIMECandidate::DrawPolygons()
{
    long theResult;
    GELine theLine;
    GEColor theDarkColor;
    GEColor theLightColor;
    GERectangle theRectangle;
    unsigned long theHeight = this->mCopyText->GetFont()->GetHeight();
    unsigned long i;
    this->mBackgroundRhombus.SetRectangle(this->mCopyCandidateRectangle);
    theResult = this->mManager->GetRasterizer()->Draw(&this->mBackgroundRhombus, this->mManager->GetScreen());
    if (this->mCopySelection >= 0) {
        theRectangle.SetLeft(this->mCopyCandidateRectangle.GetLeft() + 2.0f);
        theRectangle.SetRight(this->mCopyCandidateRectangle.GetRight() - 2.0f);
        theRectangle.SetTop(this->mCopySelection * theHeight + this->mCopyCandidateRectangle.GetTop());
        theRectangle.SetBottom(theRectangle.GetTop() + theHeight);
        theRectangle.And(this->mCopyCandidateRectangle);
        this->mSelectionRhombus.SetRectangle(theRectangle);
        theResult = this->mManager->GetRasterizer()->Draw(&this->mSelectionRhombus, this->mManager->GetScreen());
    }
    UIControl::GetBorderColors(theDarkColor, theLightColor);
    for (i = 0; i < 2; i++) {
        theLine.SetColor(theDarkColor);
        theLine.SetStartPoint(GE2DPoint(this->mCopyCandidateRectangle.GetLeft(), this->mCopyCandidateRectangle.GetTop() + i));
        theLine.SetEndPoint(GE2DPoint(this->mCopyCandidateRectangle.GetRight() + 1.0f - i,
                                      this->mCopyCandidateRectangle.GetTop() + i));
        theResult = this->mManager->GetRasterizer()->Draw(&theLine, 1, this->mManager->GetScreen());
    }
    for (i = 0; i < 2; i++) {
        theLine.SetColor(theLightColor);
        theLine.SetStartPoint(GE2DPoint(this->mCopyCandidateRectangle.GetLeft() + i,
                                        this->mCopyCandidateRectangle.GetBottom() - i));
        theLine.SetEndPoint(GE2DPoint(this->mCopyCandidateRectangle.GetRight() + 1.0f,
                                      this->mCopyCandidateRectangle.GetBottom() - i));
        theResult = this->mManager->GetRasterizer()->Draw(&theLine, 1, this->mManager->GetScreen());
    }
    for (i = 0; i < 2; i++) {
        theLine.SetColor(theDarkColor);
        theLine.SetStartPoint(GE2DPoint(this->mCopyCandidateRectangle.GetLeft() + i, this->mCopyCandidateRectangle.GetTop()));
        theLine.SetEndPoint(GE2DPoint(this->mCopyCandidateRectangle.GetLeft() + i,
                                      this->mCopyCandidateRectangle.GetBottom() + 1.0f - i));
        theResult = this->mManager->GetRasterizer()->Draw(&theLine, 1, this->mManager->GetScreen());
    }
    for (i = 0; i < 2; i++) {
        theLine.SetColor(theLightColor);
        theLine.SetStartPoint(GE2DPoint(this->mCopyCandidateRectangle.GetRight() - i,
                                        this->mCopyCandidateRectangle.GetTop() + i));
        theLine.SetEndPoint(GE2DPoint(this->mCopyCandidateRectangle.GetRight() - i,
                                      this->mCopyCandidateRectangle.GetBottom() + 1.0f));
        theResult = this->mManager->GetRasterizer()->Draw(&theLine, 1, this->mManager->GetScreen());
    }
    return theResult;
}

// FUNCTION: LLE 0x10077aa8
// Draws each candidate line onto the secondary surface
long UIIMECandidate::DrawTextA()
{
    GERectangle theRectangle;
    unsigned long theHeight = this->mCopyText->GetFont()->GetHeight();
    theRectangle.SetLeft(this->mCopyCandidateRectangle.GetLeft() + 2.0f);
    theRectangle.SetRight(this->mCopyCandidateRectangle.GetRight() - 2.0f);
    for (unsigned long i = 0; i < this->mCopyCandidateStrings.size(); i++) {
        theRectangle.SetTop(this->mCopyCandidateRectangle.GetTop() + i * theHeight + 2.0f);
        theRectangle.SetBottom(theRectangle.GetTop() + theHeight + 2.0f);
        this->mCopyText->SetLocation(theRectangle.GetTopLeft());
        this->mCopyText->SetText(this->mCopyCandidateStrings[i]);
        this->mManager->GetRasterizer()->Draw(this->mCopyText, theRectangle,
                                        this->mManager->GetRasterizer()->GetSecondarySurface(this->mManager->GetScreen()));
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x10077bbc
// Places the candidate window below the control (above it near the screen bottom), clipped to the screen;
// the IME page size it sets comes from the window's previous height
void UIIMECandidate::OpenCandidateWindow(const GERectangle& inScreenRectangle, const GERectangle& inControlRectangle)
{
    GEWindow* theWindow = this->mManager->GetRasterizer()->GetWindow(this->mManager->GetScreen());
    long thePageSize = UMFloor(this->mCandidateRectangle.GetHeight() / this->mText->GetFont()->GetHeight());
    this->mCandidateRectangle.SetLeft(inControlRectangle.GetLeft());
    this->mCandidateRectangle.SetTop(inControlRectangle.GetBottom() + 1.0f);
    this->mCandidateRectangle.SetRight(inScreenRectangle.GetWidth() * this->mWidthRatio + inControlRectangle.GetLeft());
    this->mCandidateRectangle.SetBottom(inScreenRectangle.GetHeight() * this->mHeightRatio + inControlRectangle.GetTop());
    if (!inScreenRectangle.Contains(this->mCandidateRectangle)) {
        if (this->mCandidateRectangle.GetLeft() < inScreenRectangle.GetLeft()) {
            this->mCandidateRectangle.SetLeft(inScreenRectangle.GetLeft());
            this->mCandidateRectangle.SetRight(inScreenRectangle.GetWidth() * this->mWidthRatio + inScreenRectangle.GetLeft());
        } else if (this->mCandidateRectangle.GetRight() > inScreenRectangle.GetRight()) {
            this->mCandidateRectangle.SetRight(inScreenRectangle.GetRight());
            this->mCandidateRectangle.SetLeft(inScreenRectangle.GetRight() -
                                        inScreenRectangle.GetWidth() * this->mWidthRatio);
        }
        if (this->mCandidateRectangle.GetBottom() > inScreenRectangle.GetBottom()) {
            this->mCandidateRectangle.SetTop(inControlRectangle.GetTop() - inScreenRectangle.GetHeight() * this->mHeightRatio -
                                       1.0f);
            this->mCandidateRectangle.SetBottom(inControlRectangle.GetTop() - 1.0f);
        }
        this->mCandidateRectangle.And(inScreenRectangle);
    }
    HIMC theContext = ImmGetContext(theWindow->GetOSRepresentation());
    if (theContext) {
        ImmNotifyIME(theContext, NI_SETCANDIDATE_PAGESIZE, 0, thePageSize);
        ImmReleaseContext(theWindow->GetOSRepresentation(), theContext);
    }
}

// FUNCTION: LLE 0x10077d28
// Fills the window with the current page of IME candidates ("n: text"), sizes it to fit and marks the selected line
void UIIMECandidate::SetCandidateList(const UIIMECandidateList& inList, float inLeft)
{
    UWideString theNumber;
    UWideString theCandidate;
    UWideString theLine;
    GEWindow* theWindow = this->mManager->GetRasterizer()->GetWindow(this->mManager->GetScreen());
    unsigned long theMaxWidth = 0;
    unsigned long thePageSize = UMFloor(this->mCandidateRectangle.GetHeight() / this->mText->GetFont()->GetHeight());
    this->mCandidateStrings.clear();
    const unsigned char* theString = reinterpret_cast<const unsigned char*>(&inList) + inList.mOffset[0];
    unsigned long thePageStart = inList.mSelection / thePageSize * thePageSize;
    unsigned long i;
    for (i = 0; i < thePageStart; i++)
        theString = PlatMbsNInc(theString, PlatMbsLen(theString)) + 1;
    for (; i < inList.mCount && i < thePageStart + thePageSize; i++) {
        sprintf(theNumber, "%2ld: ", i + 1);
        unsigned long theLength = PlatMbsLen(theString);
        theCandidate.CopyString(reinterpret_cast<const char*>(theString), theLength);
        theLine = theNumber + theCandidate;
        this->mText->SetText(theLine);
        if (theMaxWidth < this->mText->GetTextExtent().GetWidth())
            theMaxWidth = UMFloor(this->mText->GetTextExtent().GetWidth() + 4.0f);
        this->mCandidateStrings.push_back(theLine);
        theString = PlatMbsNInc(theString, theLength) + 1;
    }
    if (theMaxWidth > theWindow->GetWidth())
        theMaxWidth = theWindow->GetWidth();
    float theWidth = theMaxWidth;
    if (theWidth + inLeft > theWindow->GetWidth())
        inLeft = theWindow->GetWidth() - theWidth;
    this->mManager->AddDirtyRectangle(this->mCandidateRectangle, this->mManager->GetCurrentForm());
    this->mManager->GetCurrentForm()->RedrawIntersectControls(this->mCandidateRectangle);
    this->mCandidateRectangle.SetLeft(inLeft);
    this->mCandidateRectangle.SetRight(theWidth + inLeft);
    this->mSelection = inList.mSelection - thePageStart;
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x100774b9
// SYNTHETIC: LLE 0x100774c3
// SYNTHETIC: LLE 0x100774d3
// SYNTHETIC: LLE 0x100774df
// SYNTHETIC: LLE 0x100774e9
// SYNTHETIC: LLE 0x100774f3
// SYNTHETIC: LLE 0x10077503
// SYNTHETIC: LLE 0x1007750f
// SYNTHETIC: LLE 0x10077519
// SYNTHETIC: LLE 0x10077523
// SYNTHETIC: LLE 0x10077533
// SYNTHETIC: LLE 0x1007753f
// SYNTHETIC: LLE 0x10077549
// SYNTHETIC: LLE 0x10077553
// SYNTHETIC: LLE 0x10077563
// SYNTHETIC: LLE 0x1007756f
// SYNTHETIC: LLE 0x10077579
// SYNTHETIC: LLE 0x10077583
// SYNTHETIC: LLE 0x10077593
// SYNTHETIC: LLE 0x1007759f
// SYNTHETIC: LLE 0x100775a9
// SYNTHETIC: LLE 0x100775b3
// SYNTHETIC: LLE 0x100775c3
// SYNTHETIC: LLE 0x100775cf
