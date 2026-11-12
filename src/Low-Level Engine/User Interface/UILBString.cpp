// C:\EEX\Low-Level Engine\User Interface\UILBString.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10078251..0x100785cd: the U2D key init chains, then DrawTextA and UpdateTranslatedKey. The class's inline members
// and every UIListBox<UWideString> member are emitted first by UIComboBox.cpp. UILineGraph.cpp's key chains start at
// 0x100785cd.
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
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"

// FUNCTION: LLE 0x10078371
// Draws the scroll bar text and each visible line of the string list box, clipped by the form
long UILBString::DrawTextA()
{
    GERectangle theRectangle;
    long theResult = kErrorOK;
    unsigned long theLineHeight = this->mLineHeight;
    unsigned long i = 0;
    this->DrawScrollBarText();
    this->mDrawCount--;
    EeUiRect theCopy = {this->mCopyTextRectangle.mBottom, this->mCopyTextRectangle.mLeft, this->mCopyTextRectangle.mRight,
                        this->mCopyTextRectangle.mTop};
    for (; i < this->mDrawLineCount; i++) {
        // the row, one pixel wider than the text rectangle (ee_ui draw.rs)
        EeUiRect theRow = ee_ui_string_list_row(theCopy, i, theLineHeight);
        theRectangle = GERectangle(theRow.left, theRow.top, theRow.right, theRow.bottom);
        this->mCopiedText->SetLocation(GE2DPoint(theRectangle.mLeft, theRectangle.mTop));
        this->mCopiedText->SetText(this->mCopyLineArray[i]);
        theResult = this->mForm->DrawClippedText(theRectangle, this->mCopiedText, this);
    }
    return theResult;
}

// The Rust type-to-find's questions (ee_ui find_key.rs): the virtual line count and a line's first byte
struct UILBStringRust {
    static unsigned long Lines(UILBString* inList) { return inList->GetNumLines(); }
};
extern "C" unsigned long ee_uicpp_string_list_lines(void* inList)
{
    return UILBStringRust::Lines(static_cast<UILBString*>(inList));
}
extern "C" int ee_uicpp_string_list_first(void* inList, unsigned long inIndex)
{
    UWideString theLine = static_cast<UILBString*>(inList)->GetData(inIndex);
    return theLine.GetLength() > 0 ? (unsigned char)theLine[0] : -1;
}

// FUNCTION: LLE 0x10078470
// Selects the next line starting with the typed letter (any case), wrapping to the top; kErrorNotFound when none does
// (the search is Rust: ee_ui find_key.rs)
long UILBString::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    long i = ee_ui_string_list_find(this, inKey, this->mCurrentSelection);
    if (i < 0)
        return kErrorNotFound;
    if (this->mScrollBar)
        this->mScrollBar->SetCurrentRangeValue(i);
    this->ChangeSelectionIndex(i, true, true);
    this->SetDirty(true);
    return kErrorOK;
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10078251
// SYNTHETIC: LLE 0x1007825b
// SYNTHETIC: LLE 0x1007826b
// SYNTHETIC: LLE 0x10078277
// SYNTHETIC: LLE 0x10078281
// SYNTHETIC: LLE 0x1007828b
// SYNTHETIC: LLE 0x1007829b
// SYNTHETIC: LLE 0x100782a7
// SYNTHETIC: LLE 0x100782b1
// SYNTHETIC: LLE 0x100782bb
// SYNTHETIC: LLE 0x100782cb
// SYNTHETIC: LLE 0x100782d7
// SYNTHETIC: LLE 0x100782e1
// SYNTHETIC: LLE 0x100782eb
// SYNTHETIC: LLE 0x100782fb
// SYNTHETIC: LLE 0x10078307
// SYNTHETIC: LLE 0x10078311
// SYNTHETIC: LLE 0x1007831b
// SYNTHETIC: LLE 0x1007832b
// SYNTHETIC: LLE 0x10078337
// SYNTHETIC: LLE 0x10078341
// SYNTHETIC: LLE 0x1007834b
// SYNTHETIC: LLE 0x1007835b
// SYNTHETIC: LLE 0x10078367
