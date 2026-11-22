// C:\EEX\Empire Earth\Civilization Builder\EECBUIListBox.cpp (__FILE__ 0x27e426). The civilization builder's list
// box: 0x27cfdb..0x27efca. Its logic is Rust's (ee_screens civ_list.rs: AddLine, AddCategory, SortLines and the
// line order, ChangeSelectionIndex, SetUpgrades and FormatUpgrade, ReformatUpgrades, DeleteLine and its category's
// header, GetSelectedUpgrade, TabbedTo, DrawTextA's lines); this unit keeps the ctor, the snapshot and the
// forwarders (the base's calls Rust makes on a list are the builder host's shims: EECivilizationBuilder.cpp). Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/UIFontTable.h"
#include "lle/UIForm.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIControlsTable.h"

#include "ee/EECBUIListBox.h"
#include "rust/game/screens.h"   // ee_screens_cbl_*: the list's logic (civ_list.rs); EE_SCREENS_CBW_L_*

// the scroll bar's opacity (a unit-local float ahead of the statics' strings in .rdata: 0x4cfa64)
static const float kScrollBarBgOpacity = 0.2f;

const UWideString EECBUIListBox::sSeparator("/");
const UWideString EECBUIListBox::sCategory("Category");
static const GEColor kBlack(0.0f, 0.0f, 0.0f);
static const GEColor kWhite(1.0f, 1.0f, 1.0f);

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
