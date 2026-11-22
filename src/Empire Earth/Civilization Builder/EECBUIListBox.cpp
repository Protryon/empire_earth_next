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
#include "ee/EEScreensShims.h"

// the scroll bar's opacity (a unit-local float ahead of the statics' strings in .rdata: 0x4cfa64)

const UWideString EECBUIListBox::sSeparator("/");
const UWideString EECBUIListBox::sCategory("Category");

// Creates a civilization builder list box (upgrade lines only selectable, points shown, sorted lines)
EECBUIListBox::EECBUIListBox(UIControlsTable& inTable, unsigned long inId, std::vector<UWideString>& inData,
                             GEFont* inFont, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                             unsigned long inBorderThickness, bool inIsTabable)
    : UILBString(inId, inData, inFont, true, inRectangle, inForm, inIndex, inBorderThickness)
{
    ee_screens_cbb_construct((uintptr_t)&inTable, (uintptr_t)static_cast<UIControl*>(this), inIsTabable);
}

// the constructor's body, AppendStringArray and CopyData: hud_bodies.rs over these (UI lane round 12)
#define CBB(l) static_cast<EECBUIListBox*>(reinterpret_cast<UIControl*>(l))
extern "C" void ee_screenscpp_cbb_setup_text(uintptr_t inTable, uintptr_t inList)
{ ((UIControlsTable*)inTable)->SetupGEText(CBB(inList)->GetGEText()); }
extern "C" void ee_screenscpp_cbb_set_header_colour(uintptr_t inList, float inRed, float inGreen, float inBlue)
{ CBB(inList)->mC13C = GEColor(inRed, inGreen, inBlue); }
extern "C" void ee_screenscpp_cbb_set_select_upgrades_only(uintptr_t inList, bool inValue)
{ CBB(inList)->mSelectUpgradesOnly = inValue; }
extern "C" void ee_screenscpp_cbb_set_show_points(uintptr_t inList, bool inValue)
{ CBB(inList)->mShowPoints = inValue; }
extern "C" void ee_screenscpp_cbb_set_tabable(uintptr_t inList, bool inValue) { CBB(inList)->mIsTabable = inValue; }
extern "C" void ee_screenscpp_cbb_scroll_bg_colour(uintptr_t inList, float inRed, float inGreen, float inBlue)
{ CBB(inList)->SetScrollBarBgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_cbb_scroll_bg_opacity(uintptr_t inList, float inOpacity)
{ CBB(inList)->SetScrollBarBgOpacity(inOpacity); }
extern "C" uintptr_t ee_screenscpp_cbb_lines_count(uintptr_t inLines)
{ return ((std::vector<UWideString>*)inLines)->size(); }
extern "C" void ee_screenscpp_cbb_line(uintptr_t inLines, uintptr_t inIndex, void* inOut)
{ EEShimOut(inOut, (*(std::vector<UWideString>*)inLines)[inIndex]); }
extern "C" int64_t ee_screenscpp_cbb_base_copy_data(uintptr_t inList)
{ return (int64_t)CBB(inList)->UILBString::CopyData(); }
extern "C" uint64_t ee_screenscpp_cbb_selection(uintptr_t inList) { return (uint64_t)CBB(inList)->GetSelectionIndex(); }
extern "C" void ee_screenscpp_cbb_copied_no_selection(uintptr_t inList) { CBB(inList)->SetCopiedHasSelection(false); }
extern "C" void ee_screenscpp_cbb_copy_header(uintptr_t inList) { CBB(inList)->mC148 = CBB(inList)->mC13C; }
extern "C" void ee_screenscpp_cbb_copy_show_points(uintptr_t inList)
{ CBB(inList)->mCopiedShowPoints = CBB(inList)->mShowPoints; }
extern "C" void ee_screenscpp_cbb_set_sort_lines(uintptr_t inList, bool inValue) { CBB(inList)->SetSortLines(inValue); }
