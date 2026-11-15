// C:\EEX\Empire Earth\Civilization Builder\EECBUIListBox.cpp (__FILE__ 0x27e426). The civilization builder's list
// box: 0x27cfdb..0x27efca. Its logic is Rust's (ee_screens civ_list.rs: AddLine, AddCategory, SortLines and the
// line order, ChangeSelectionIndex, SetUpgrades and FormatUpgrade, ReformatUpgrades, DeleteLine and its category's
// header, GetSelectedUpgrade, TabbedTo, DrawTextA's lines); this unit keeps the ctor, the snapshot and the
// forwarders, and HostOp, the base's calls Rust makes on a list. Unit flags: the exe's module set.
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
#include "lle/GEText.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIControlsTable.h"

#include "ee/EmpireEarth.h"
#include "ee/EECBUIListBox.h"
#include "rust/ee_screens.h"   // ee_screens_cbl_*: the list's logic (civ_list.rs); EE_SCREENS_CBW_L_*

// the scroll bar's opacity (a unit-local float ahead of the statics' strings in .rdata: 0x4cfa64)
static const float kScrollBarBgOpacity = 0.2f;

const UWideString EECBUIListBox::sSeparator("/");
const UWideString EECBUIListBox::sCategory("Category");
static const GEColor kBlack(0.0f, 0.0f, 0.0f);
static const GEColor kWhite(1.0f, 1.0f, 1.0f);
static const GEColor kDarkBlue(0.0f, 0.0f, 0.39f);

static void EECBPut(EeScreensCbOut* outText, const UWideString& inText)
{
    ee_screens_cb_out_text(outText, reinterpret_cast<const uint8_t*>((const char*)inText), inText.GetTCHARLength());
}

// Creates a civilization builder list box (upgrade lines only selectable, points shown, sorted lines)
EECBUIListBox::EECBUIListBox(UIControlsTable& inTable, unsigned long inId, std::vector<UWideString>& inData,
                             GEFont* inFont, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                             unsigned long inBorderThickness, bool inIsTabable)
    : UILBString(inId, inData, inFont, true, inRectangle, inForm, inIndex, inBorderThickness)
{
    inTable.SetupGEText(this->GetGEText());
    this->mC13C = kBlack;
    this->mSelectUpgradesOnly = true;
    this->mShowPoints = true;
    this->mSortLines = true;
    this->mIsTabable = inIsTabable;
    this->SetScrollBarBgColor(kWhite);
    this->SetScrollBarBgOpacity(kScrollBarBgOpacity);
    this->ChangeSelectionIndex(0, false, false);
}

EECBUIListBox::~EECBUIListBox()
{
}

void EECBUIListBox::AddLine(const UWideString& inLine)
{
    ee_screens_cbl_add_line(this, reinterpret_cast<const uint8_t*>((const char*)inLine), inLine.GetTCHARLength());
}

void EECBUIListBox::AppendStringArray(std::vector<UWideString>& inLines)
{
    for (std::vector<UWideString>::iterator theIter = inLines.begin(); theIter != inLines.end(); theIter++)
        this->AddLine(*theIter);
}

void EECBUIListBox::ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
{
    ee_screens_cbl_change_selection(this, (uint32_t)inIndex, inFireEvent, inScrollTo);
}

// Snapshots the list for drawing: no highlight on line 0, the header colour and the show-points flag
long EECBUIListBox::CopyData()
{
    long theResult = UILBString::CopyData();
    if (this->GetSelectionIndex() == 0)
        this->SetCopiedHasSelection(false);
    this->mC148 = this->mC13C;
    this->mCopiedShowPoints = this->mShowPoints;
    return theResult;
}

void EECBUIListBox::DeleteLine(unsigned long inLine)
{
    ee_screens_cbl_delete_line(this, (uint32_t)inLine);
}

long EECBUIListBox::DrawTextA()
{
    return ee_screens_cbl_draw_text(this);
}

void EECBUIListBox::SetCivilizationUpgrades(std::vector<EECivUpgrade>& inUpgrades)
{
    ee_screens_cbl_set_civilization_upgrades(this, inUpgrades.data(), inUpgrades.size());
}

void EECBUIListBox::TabbedTo()
{
    ee_screens_cbl_tabbed_to(this);
}

// The list's host ops (EE_SCREENS_CBW_L_*: the base's members; ee_screens civ_whole.rs)
int64_t EECBUIListBox::HostOp(uint32_t inOp, EECBUIListBox* p, int64_t inA, int64_t inB, int64_t inC,
                              const UWideString& inText, EeScreensCbOut* outText)
{
    switch (inOp) {
    case EE_SCREENS_CBW_L_COUNT:
        return p->GetNumLines();
    case EE_SCREENS_CBW_L_LINE:
        EECBPut(outText, p->mData[(unsigned long)inA]);
        return 0;
    case EE_SCREENS_CBW_L_SET_LINE:
        p->mData[(unsigned long)inA] = inText;
        return 0;
    case EE_SCREENS_CBW_L_APPEND:
        p->UIListBox<UWideString>::AppendLine(inText);
        return 0;
    case EE_SCREENS_CBW_L_DELETE:
        p->UIListBox<UWideString>::DeleteLine((unsigned long)inA);
        return 0;
    case EE_SCREENS_CBW_L_DELETE_ALL:
        p->DeleteAllEntries();
        return 0;
    case EE_SCREENS_CBW_L_SELECT:
        p->UIListBox<UWideString>::ChangeSelectionIndex((unsigned long)inA, inB != 0, inC != 0);
        return 0;
    case EE_SCREENS_CBW_L_CURRENT: {
        unsigned long theIndex = 0;
        long theResult = p->GetCurrentSelection(theIndex);
        ee_screens_cb_out_value(outText, (int64_t)theIndex);
        return theResult;
    }
    case EE_SCREENS_CBW_L_AUTO:
        return p->GetAutoSelect();
    case EE_SCREENS_CBW_L_HAS:
        return p->GetHasSelection();
    case EE_SCREENS_CBW_L_INDEX:
        return p->GetSelectionIndex();
    case EE_SCREENS_CBW_L_DIRTY:
        p->SetDirty(true);
        return 0;
    case EE_SCREENS_CBW_L_FLAG:
        return inA == EE_SCREENS_CBW_FLAG_UPGRADES_ONLY ? p->mSelectUpgradesOnly
               : inA == EE_SCREENS_CBW_FLAG_SHOW_POINTS ? p->mShowPoints
               : inA == EE_SCREENS_CBW_FLAG_SORT        ? p->mSortLines
                                                        : p->mCopiedShowPoints;
    case EE_SCREENS_CBW_L_SET_FLAG:
        (inA == EE_SCREENS_CBW_FLAG_UPGRADES_ONLY ? p->mSelectUpgradesOnly
         : inA == EE_SCREENS_CBW_FLAG_SHOW_POINTS ? p->mShowPoints
         : inA == EE_SCREENS_CBW_FLAG_SORT        ? p->mSortLines
                                                  : p->mCopiedShowPoints) = inB != 0;
        return 0;
    case EE_SCREENS_CBW_L_DRAW_BEGIN:
        p->UIControl::DrawTextA();
        return p->GetDrawCount();
    case EE_SCREENS_CBW_L_COPY_LINE:
        EECBPut(outText, p->mCopyLineArray[(unsigned long)inA]);
        return 0;
    case EE_SCREENS_CBW_L_DRAW: {
        // the line's rectangle (the copied text rectangle, 2 in on the left, 1 out on the right), then the text
        GERectangle theRectangle = p->GetCopyTextRectangle();
        theRectangle.SetLeft(theRectangle.GetLeft() + 2.0f);
        theRectangle.SetRight(theRectangle.GetRight() + 1.0f);
        unsigned long theLineHeight = p->GetLineHeight();
        float theTop = p->GetCopyTextRectangle().GetTop() + (unsigned long)inA * theLineHeight;
        theRectangle.SetTop(theTop);
        theRectangle.SetBottom(theTop + theLineHeight);
        GEText* theText = p->GetGEText();
        bool theBold = (inB & EE_SCREENS_CBW_DRAW_BOLD) != 0;
        if (theBold)
            theText->SetFont(UIFontTable::FindOrCreateFont(theText->GetFont()->GetFontName(),
                                                           theText->GetFont()->GetFontStyle() | kFontStyleBold,
                                                           theText->GetFont()->GetFontQuality(),
                                                           theText->GetFont()->GetFontSize()));
        bool theRight = (inB & EE_SCREENS_CBW_DRAW_RIGHT) != 0;
        theText->SetAlignment(theRight ? kTextAlignRight : kTextAlignLeft);
        theText->SetText(inText);
        theText->SetFgColor(inC == EE_SCREENS_CBW_COLOUR_HEADER ? p->mC148
                            : inC == EE_SCREENS_CBW_COLOUR_BLACK ? kBlack
                                                                 : kDarkBlue);
        theText->SetLocation(theRight ? theRectangle.GetTopRight() : theRectangle.GetTopLeft());
        long theResult = p->GetForm()->DrawClippedText(theRectangle, theText, p);
        if (theBold)
            theText->SetFont(UIFontTable::FindOrCreateFont(theText->GetFont()->GetFontName(),
                                                           theText->GetFont()->GetFontStyle() & ~kFontStyleBold,
                                                           theText->GetFont()->GetFontQuality(),
                                                           theText->GetFont()->GetFontSize()));
        return theResult;
    }
    case EE_SCREENS_CBW_L_DRAW_END:
        return p->DrawScrollBarText();
    }
    return 0;
}
