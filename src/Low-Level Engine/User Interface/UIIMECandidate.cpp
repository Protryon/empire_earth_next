// C:\EEX\Low-Level Engine\User Interface\UIIMECandidate.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100774b9..0x10077f75: the U2D key init chains, the ctor, the dtor, then the out-of-line members in alphabetical
// order. The implicit copy members and ??_E are emitted by UIForm.cpp (first includer of UIIMECandidate.h).
// UILayoutUtility.cpp starts at 0x10077f75.
//
// The layout is Rust (rust/crates/ee_ui/src/ime.rs, docs/RUST_PLAN.md R6): where the window opens, the candidate
// list's page, numbers and width, the selection bar, the border's lines and the text rows. The strings' walk, the
// texts, rhombi, line and draws stay here.
#include <windows.h>
#include <mbstring.h>

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

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top)
static inline EeUiRect UIIMERect(const GERectangle& r) { return EeUiRect{r.GetBottom(), r.GetLeft(), r.GetRight(), r.GetTop()}; }
static inline GERectangle UIIMERect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }

// The Rust SetCandidateList's walk over the list's strings (ee_ui ime.rs)
struct UIIMECandidateRust {
    struct List {
        UIIMECandidate* mWindow;
        const unsigned char* mString;
    };
    static float Line(List* inList, const char* inNumber, unsigned long inLength)
    {
        UWideString theNumber;
        UWideString theCandidate;
        UWideString theLine;
        theNumber.CopyString(inNumber, inLength);   // ASCII: characters are bytes
        unsigned long theLength = PlatMbsLen(inList->mString);
        theCandidate.CopyString(reinterpret_cast<const char*>(inList->mString), theLength);
        theLine = theNumber + theCandidate;
        inList->mWindow->mText->SetText(theLine);
        inList->mWindow->mCandidateStrings.push_back(theLine);
        inList->mString = PlatMbsNInc(inList->mString, theLength) + 1;
        return inList->mWindow->mText->GetTextExtent().GetWidth();
    }
};
extern "C" void ee_uicpp_ime_skip(void* inList)
{
    UIIMECandidateRust::List* theList = static_cast<UIIMECandidateRust::List*>(inList);
    theList->mString = PlatMbsNInc(theList->mString, PlatMbsLen(theList->mString)) + 1;
}
extern "C" float ee_uicpp_ime_line(void* inList, const unsigned char* inNumber, unsigned long inLength)
{
    return UIIMECandidateRust::Line(static_cast<UIIMECandidateRust::List*>(inList), reinterpret_cast<const char*>(inNumber),
                                    inLength);
}

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
    unsigned long theHeight = this->mCopyText->GetFont()->GetHeight();
    this->mBackgroundRhombus.SetRectangle(this->mCopyCandidateRectangle);
    theResult = this->mManager->GetRasterizer()->Draw(&this->mBackgroundRhombus, this->mManager->GetScreen());
    EeUiCandidateFrame theFrame = ee_ui_ime_frame(UIIMERect(this->mCopyCandidateRectangle), this->mCopySelection, theHeight);
    if (theFrame.has_selection) {
        this->mSelectionRhombus.SetRectangle(UIIMERect(theFrame.selection));
        theResult = this->mManager->GetRasterizer()->Draw(&this->mSelectionRhombus, this->mManager->GetScreen());
    }
    UIControl::GetBorderColors(theDarkColor, theLightColor);
    for (unsigned long i = 0; i < 8; i++) {
        const EeUiCandidateLine& l = theFrame.lines[i];
        theLine.SetColor(l.light ? theLightColor : theDarkColor);
        theLine.SetStartPoint(GE2DPoint(l.x0, l.y0));
        theLine.SetEndPoint(GE2DPoint(l.x1, l.y1));
        theResult = this->mManager->GetRasterizer()->Draw(&theLine, 1, this->mManager->GetScreen());
    }
    return theResult;
}

// FUNCTION: LLE 0x10077aa8
// Draws each candidate line onto the secondary surface
long UIIMECandidate::DrawTextA()
{
    unsigned long theHeight = this->mCopyText->GetFont()->GetHeight();
    for (unsigned long i = 0; i < this->mCopyCandidateStrings.size(); i++) {
        GERectangle theRectangle = UIIMERect(ee_ui_ime_row(UIIMERect(this->mCopyCandidateRectangle), i, theHeight));
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
    // (the IME page size the original computed here from the window's previous height went to
    // ImmNotifyIME(NI_SETCANDIDATE_PAGESIZE); the system draws the list now)
    this->mCandidateRectangle = UIIMERect(ee_ui_ime_open_rect(UIIMERect(inScreenRectangle), UIIMERect(inControlRectangle),
                                                              this->mWidthRatio, this->mHeightRatio));
}

// FUNCTION: LLE 0x10077d28
// Fills the window with the current page of IME candidates ("n: text"), sizes it to fit and marks the selected line
void UIIMECandidate::SetCandidateList(const UIIMECandidateList& inList, float inLeft)
{
    GEWindow* theWindow = this->mManager->GetRasterizer()->GetWindow(this->mManager->GetScreen());
    this->mCandidateStrings.clear();
    UIIMECandidateRust::List theList = {this, reinterpret_cast<const unsigned char*>(&inList) + inList.mOffset[0]};
    EeUiCandidateFit theFit = ee_ui_ime_candidate_list(&theList, this->mCandidateRectangle.GetHeight(),
                                                       this->mText->GetFont()->GetHeight(), inList.mSelection,
                                                       inList.mCount, theWindow->GetWidth(), inLeft);
    this->mManager->AddDirtyRectangle(this->mCandidateRectangle, this->mManager->GetCurrentForm());
    this->mManager->GetCurrentForm()->RedrawIntersectControls(this->mCandidateRectangle);
    this->mCandidateRectangle.SetLeft(theFit.left);
    this->mCandidateRectangle.SetRight(theFit.right);
    this->mSelection = theFit.selection;
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
