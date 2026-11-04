// C:\EEX\Empire Earth\Civilization Builder\EECBUIListBox.cpp (__FILE__ 0x27e426). The civilization builder's list
// box: 0x27cfdb..0x27efca (its globals' init chains first: the two class statics, two strings, four colours).
// Unit flags: the exe's module set. The file-static globals have provisional names.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <map>
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
#include "lle/GEText.h"
#include "lle/UIControlsTable.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECBEditableCivilization.h"

#include "ee/EECBUIListBox.h"
#include "LostDebug.h"
#include "ee/EEStringIDs.h"
// the scroll bar's opacity (a unit-local float ahead of the statics' strings in .rdata: 0x4cfa64)
static const float kScrollBarBgOpacity = 0.2f;

const UWideString EECBUIListBox::sSeparator("/");
const UWideString EECBUIListBox::sCategory("Category");
static const UWideString kIndent("       ");
static const UWideString kSpacer("  ");
static const GEColor kBlack(0.0f, 0.0f, 0.0f);
static const GEColor kWhite(1.0f, 1.0f, 1.0f);
static const GEColor kDarkBlue(0.0f, 0.0f, 0.39f);
static const GEColor kTextWhite(1.0f, 1.0f, 1.0f);

// the remove_if predicate of SetUpgradesExcept (an upgrade of the excluded list) (provisional name)
class EECBIsUpgradeIn {
public:
    EECBIsUpgradeIn(const std::vector<unsigned short>& inUpgrades) : mUpgrades(inUpgrades) {}
    // emitted with the remove_if instantiation (0x67e8c0)
    bool operator()(unsigned short inUpgrade) const
    {
        bool theFound = false;
        for (std::vector<unsigned short>::const_iterator theIter = this->mUpgrades.begin(); theIter != this->mUpgrades.end();
             theIter++) {
            if (inUpgrade == *theIter) {
                theFound = true;
                break;
            }
        }
        return theFound;
    }
private:
    const std::vector<unsigned short>& mUpgrades;
};

// the line order of SortLines: lines that start with the "unassigned" string (resource 0x8052) first, then the
// rest case-insensitively (provisional name)
class EECBLineSorter {
public:
    // emitted with the sort instantiation (0x67e9ac)
    bool operator()(const UWideString& inLeft, const UWideString& inRight) const
    {
        UWideString theUnassigned;
        UWideString theLeft(inLeft);
        UWideString theRight(inRight);
        bool theResult;
        gResource->GetString(IDS_SPECIAL_POWERS, theUnassigned);
        theUnassigned.ToLower();
        theLeft.ToLower();
        theRight.ToLower();
        if (theLeft.GetIndexedSubstring(0, theUnassigned.GetLength() - 1) == theUnassigned
            && theRight.GetIndexedSubstring(0, theUnassigned.GetLength() - 1) != theUnassigned)
            theResult = true;
        else if (theLeft.GetIndexedSubstring(0, theUnassigned.GetLength() - 1) != theUnassigned
                 && theRight.GetIndexedSubstring(0, theUnassigned.GetLength() - 1) == theUnassigned)
            theResult = false;
        else
            theResult = _mbsicmp(reinterpret_cast<const unsigned char*>((const char*)inLeft),
                                 reinterpret_cast<const unsigned char*>((const char*)inRight)) < 0;
        return theResult;
    }
};

// FUNCTION: EXE 0x67d187
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

// SYNTHETIC: EXE 0x67d25c
// (the scalar deleting destructor, emitted with the vtable; slot EECBUIListBox+0x0)

// FUNCTION: EXE 0x67d278
// Destroys the list box
EECBUIListBox::~EECBUIListBox()
{
}

// FUNCTION: EXE 0x67d284
// Adds the category header line for an upgrade line when its category has none yet, then sorts
bool EECBUIListBox::AddCategory(const UWideString& inLine)
{
    UWideString theCategory;
    UWideString theCost;
    UWideString theHeader;
    bool theResult = false;
    if (EECBUIListBox::GetCategory(inLine, theCategory) && !this->FindCategory(theCategory, NULL)
        && EECBUIListBox::GetToken(inLine, kCivTokenCost, theCost)) {
        theHeader = theCategory + EECBUIListBox::sSeparator + theCost + EECBUIListBox::sSeparator + EECBUIListBox::sCategory;
        UIListBox<UWideString>::AppendLine(theHeader);
        this->SortLines();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x67d386
// Adds a line (and its category header if new) and keeps the lines sorted
void EECBUIListBox::AddLine(const UWideString& inLine)
{
    UIListBox<UWideString>::AppendLine(inLine);
    if (!this->AddCategory(inLine))
        this->SortLines();
}

// FUNCTION: EXE 0x67d3ad
// Adds each line of the array with its category header
void EECBUIListBox::AppendStringArray(std::vector<UWideString>& inLines)
{
    UWideString theLine;
    for (std::vector<UWideString>::iterator theIter = inLines.begin(); theIter != inLines.end(); theIter++) {
        theLine = *theIter;
        this->AddLine(theLine);
    }
}

// FUNCTION: EXE 0x67d3f9
// Selects a line; in upgrade-only mode a header line passes the selection on to the nearest upgrade line in the
// direction of travel, and with upgrade selection off the selection is cleared
void EECBUIListBox::ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
{
    if (this->mSelectUpgradesOnly) {
        if (!this->IsUpgradeAt(inIndex, NULL)) {
            if (this->mAutoSelect)
                return;
            unsigned long theCurrent;
            if (this->GetCurrentSelection(theCurrent) != kErrorOK)
                theCurrent = 0;
            if (theCurrent < inIndex) {
                for (long i = inIndex + 1; i < (long)this->GetLineCount(); i++) {
                    if (this->IsUpgradeAt(i, NULL)) {
                        UIListBox<UWideString>::ChangeSelectionIndex(i, inFireEvent, inScrollTo);
                        return;
                    }
                }
                for (long j = inIndex - 1; j >= 0; j--) {
                    if (this->IsUpgradeAt(j, NULL)) {
                        UIListBox<UWideString>::ChangeSelectionIndex(j, inFireEvent, inScrollTo);
                        return;
                    }
                }
            } else {
                for (long i = inIndex - 1; i >= 0; i--) {
                    if (this->IsUpgradeAt(i, NULL)) {
                        UIListBox<UWideString>::ChangeSelectionIndex(i, inFireEvent, inScrollTo);
                        return;
                    }
                }
                for (long j = inIndex + 1; j < (long)this->GetLineCount(); j++) {
                    if (this->IsUpgradeAt(j, NULL)) {
                        UIListBox<UWideString>::ChangeSelectionIndex(j, inFireEvent, inScrollTo);
                        return;
                    }
                }
            }
        } else
            UIListBox<UWideString>::ChangeSelectionIndex(inIndex, inFireEvent, inScrollTo);
    } else
        this->ResetSelection();
}

// FUNCTION: EXE 0x67d4dc
// Rebuilds the list from upgrade ids: one formatted line per upgrade plus one header per category, sorted
void EECBUIListBox::SetUpgrades(std::vector<unsigned short>& inUpgrades, bool inFlag)
{
    std::map<UWideString, long> theCategories;
    UWideString theLine;
    UWideString theCategory;
    UWideString theCost;
    UWideString theHeader;
    bool theSort = this->mSortLines;
    this->DeleteAllEntries();
    this->mSortLines = false;
    for (std::vector<unsigned short>::iterator theIter = inUpgrades.begin(); theIter != inUpgrades.end();
         theIter++) {
        if (!this->FormatUpgrade(*theIter, theLine, inFlag, 0))
            continue;
        UIListBox<UWideString>::AppendLine(theLine);
        if (!EECBUIListBox::GetCategory(theLine, theCategory) || theCategories.find(theCategory) != theCategories.end()
            || !EECBUIListBox::GetToken(theLine, kCivTokenCost, theCost))
            continue;
        theHeader = theCategory + EECBUIListBox::sSeparator + theCost + EECBUIListBox::sSeparator + EECBUIListBox::sCategory;
        UIListBox<UWideString>::AppendLine(theHeader);
        theCategories[theCategory] = 1;
    }
    this->mSortLines = theSort;
    this->SortLines();
    this->ReformatUpgrades(inFlag);
}

// FUNCTION: EXE 0x67d6ae
// Snapshots the list for drawing: no highlight on line 0, the header colour and the show-points flag
long EECBUIListBox::CopyData()
{
    long theResult = UILBString::CopyData();
    if (this->mCurrentSelection == 0)
        this->mCopiedHasSelection = false;
    this->mC148 = this->mC13C;
    this->mCopiedShowPoints = this->mShowPoints;
    return theResult;
}

// FUNCTION: EXE 0x67d6f7
// Deletes a line and removes its category header when no upgrade of that category is left
void EECBUIListBox::DeleteLine(unsigned long inLine)
{
    UWideString theCategory;
    UWideString theLine;
    if (this->GetCategoryAt(inLine, theCategory) && this->GetLine(inLine, theLine)) {
        UIListBox<UWideString>::DeleteLine(inLine);
        this->RemoveCategoryIfEmpty(theLine);
    }
}

// FUNCTION: EXE 0x67d761
// Draws a category header line (the special powers category in bold)
long EECBUIListBox::DrawCategoryLine(const GERectangle& inRectangle, GEText* inText, const UWideString& inLine)
{
    UWideString theCategory;
    UWideString theCost;
    UWideString theUnassigned;
    bool theBold = false;
    long theResult = 3;
    if (EECBUIListBox::GetToken(inLine, kCivTokenCategory, theCategory)
        && EECBUIListBox::GetToken(inLine, kCivTokenUpgradeName, theCost)) {
        gResource->GetString(IDS_SPECIAL_POWERS, theUnassigned);
        if (theCategory == theUnassigned) {
            inText->SetFont(UIFontTable::FindOrCreateFont(inText->GetFont()->GetFontName(),
                                                          inText->GetFont()->GetFontStyle() | kFontStyleBold,
                                                          inText->GetFont()->GetFontQuality(),
                                                          inText->GetFont()->GetFontSize()));
            theBold = true;
        }
        inText->SetAlignment(kTextAlignLeft);
        inText->SetText(theCategory);
        inText->SetFgColor(this->mC148);
        inText->SetLocation(inRectangle.GetTopLeft());
        theResult = this->GetForm()->DrawClippedText(inRectangle, inText, this);
        if (theBold)
            inText->SetFont(UIFontTable::FindOrCreateFont(inText->GetFont()->GetFontName(),
                                                          inText->GetFont()->GetFontStyle() & ~kFontStyleBold,
                                                          inText->GetFont()->GetFontQuality(),
                                                          inText->GetFont()->GetFontSize()));
    }
    return theResult;
}

// FUNCTION: EXE 0x67d8c7
// Draws an upgrade line: value and name on the left (no value for civ powers), its points on the right
long EECBUIListBox::DrawUpgradeLine(const GERectangle& inRectangle, GEText* inText, const UWideString& inLine)
{
    UWideString theValue;
    UWideString theName;
    UWideString thePoints;
    long theResult = 3;
    unsigned long theUpgrade;
    unsigned long theCategory;
    unsigned long theIndex;
    if (EECBUIListBox::GetToken(inLine, kCivTokenUpgradeName, theName)
        && EECBUIListBox::GetToken(inLine, kCivTokenValue, theValue)
        && EECBUIListBox::GetToken(inLine, kCivTokenPoints, thePoints)) {
        theValue += " ";
        inText->SetAlignment(kTextAlignLeft);
        if (EECBUIListBox::GetNumber(inLine, kCivTokenUpgrade, &theUpgrade)) {
            gCivilizationManager.SplitAttribute((unsigned short)theUpgrade, &theCategory, &theIndex);
            if (theIndex >= 0x26)
                theValue.Clear();
        }
        inText->SetText(kIndent + theValue + theName);
        inText->SetLocation(inRectangle.GetTopLeft());
        inText->SetFgColor(kBlack);
        theResult = this->GetForm()->DrawClippedText(inRectangle, inText, this);
        if (theResult == kErrorOK && this->mCopiedShowPoints) {
            inText->SetAlignment(kTextAlignRight);
            inText->SetLocation(inRectangle.GetTopRight());
            inText->SetText(thePoints + kSpacer);
            inText->SetFgColor(kDarkBlue);
            theResult = this->GetForm()->DrawClippedText(inRectangle, inText, this);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67dac2
// Draws the list box's polygons
long EECBUIListBox::DrawPolygons()
{
    return UIListBox<UWideString>::DrawPolygons();
}

// FUNCTION: EXE 0x67dac8
// Draws every visible line as an upgrade or category line, then the scroll bar text
long EECBUIListBox::DrawTextA()
{
    UWideString theLine;
    UIControl::DrawTextA();
    GERectangle theRectangle;
    theRectangle = this->mCopyTextRectangle;
    theRectangle.SetLeft(theRectangle.GetLeft() + 2.0f);
    theRectangle.SetRight(theRectangle.GetRight() + 1.0f);
    GEText* theText = this->GetGEText();
    unsigned long theLineHeight = this->mLineHeight;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mDrawLineCount && theResult == kErrorOK; i++) {
        theLine = this->mCopyLineArray[i];
        float theTop = this->mCopyTextRectangle.GetTop() + i * theLineHeight;
        theRectangle.SetTop(theTop);
        theRectangle.SetBottom(theTop + theLineHeight);
        if (EECBUIListBox::IsUpgradeLine(theLine))
            theResult = this->DrawUpgradeLine(theRectangle, theText, theLine);
        else
            theResult = this->DrawCategoryLine(theRectangle, theText, theLine);
    }
    return this->DrawScrollBarText();
}

// FUNCTION: EXE 0x67dbca
// Reformats every upgrade line with its current cost (buy) or refund (sell) given its place in its category
void EECBUIListBox::ReformatUpgrades(bool inFlag)
{
    UWideString theLine;
    unsigned long theUpgrade = 0;
    unsigned long theIndex = 0;
    for (std::vector<UWideString>::iterator theIter = this->mData.begin(); theIter != this->mData.end(); theIter++) {
        if (EECBUIListBox::IsUpgradeLine(*theIter)) {
            if (EECBUIListBox::GetNumber(*theIter, kCivTokenUpgrade, &theUpgrade)) {
                if (this->FormatUpgrade((unsigned short)theUpgrade, theLine, inFlag, theIndex))
                    *theIter = theLine;
                theIndex++;
            }
        } else
            theIndex = 0;
    }
}

// FUNCTION: EXE 0x67dc4b
// Reads the category of the line at an index
bool EECBUIListBox::GetCategoryAt(unsigned long inIndex, UWideString& outCategory)
{
    UWideString theLine;
    bool theResult = false;
    if (this->GetLine(inIndex, theLine))
        theResult = EECBUIListBox::GetToken(theLine, kCivTokenCategory, outCategory);
    return theResult;
}

// FUNCTION: EXE 0x67dc95
// Reads the category of a formatted line
bool EECBUIListBox::GetCategory(const UWideString& inLine, UWideString& outCategory)
{
    return EECBUIListBox::GetToken(inLine, kCivTokenCategory, outCategory);
}

// FUNCTION: EXE 0x67dca8
// Reads the cost of a formatted line
bool EECBUIListBox::GetCost(const UWideString& inLine, unsigned long* outCost)
{
    return EECBUIListBox::GetNumber(inLine, kCivTokenCost, outCost);
}

// FUNCTION: EXE 0x67dcbb
// Reads the upgrade name of a formatted line
bool EECBUIListBox::GetUpgradeName(const UWideString& inLine, UWideString& outName)
{
    return EECBUIListBox::GetToken(inLine, kCivTokenUpgradeName, outName);
}

// FUNCTION: EXE 0x67dcce
// Finds the index of the line for an upgrade id; returns 3 when it is not listed
long EECBUIListBox::FindUpgrade(unsigned short inUpgrade, unsigned long* outIndex)
{
    UWideString theLine;
    long theResult = 3;
    unsigned short theUpgrade = 0;
    for (unsigned long i = 0; i < this->GetNumLines(); i++) {
        if (!this->GetLine(i, theLine) || !EECBUIListBox::GetUpgrade(theLine, &theUpgrade) || theUpgrade != inUpgrade)
            continue;
        *outIndex = i;
        theResult = kErrorOK;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x67dd4f
// Formats an upgrade as a list line: category/name/bonus/id/cost/points/value, the points being its cost to buy
// or its refund to sell; false for an upgrade without a level
bool EECBUIListBox::FormatUpgrade(unsigned short inUpgrade, UWideString& outLine, bool inFlag, long inIndex)
{
    long thePoints = 0;
    bool theResult = false;
    unsigned long theCategory;
    unsigned long theIndex;
    float theValue;
    float theBonus;
    long theNameID;
    gCivilizationManager.SplitAttribute(inUpgrade, &theCategory, &theIndex);
    bool theFound = gCivilizationManager.LookupLevel(inUpgrade, &theValue, &theBonus);
    long theCost = gCivilizationManager.GetExtraCost(inUpgrade);
    if (theFound && theValue != 0.0f && gCivilizationManager.GetCategoryNameID(theCategory, &theNameID)) {
        static UWideString theCategoryName;
        long theError = gResource->GetString(theNameID, theCategoryName);
        if (gEditableCivilization != NULL) {
            if (inFlag)
                thePoints = gEditableCivilization->GetCost(inUpgrade);
            else
                thePoints = gEditableCivilization->GetRefund(inUpgrade) - theCost * inIndex;
        }
        if (theError == kErrorOK) {
            static UWideString theUpgradeName;
            static UWideString theFormat;
            static UWideString theValueText;
            theNameID = theIndex <= 0x25 ? theIndex + IDS_CIV_BONUS_BASE : theIndex + IDS_CIV_POWER_BASE;
            theError = gResource->GetString(theNameID, theUpgradeName);
            gResource->GetString(IDS_FORMAT_PERCENT_6_2G, theFormat);
            sprintf(theValueText, theFormat, theValue * 100.0f);
            if (theError == kErrorOK) {
                sprintf(outLine, "%s%s%s%s%d%s%d%s%d%s%d%s%s", (const char*)theCategoryName,
                        (const char*)EECBUIListBox::sSeparator, (const char*)theUpgradeName, (const char*)EECBUIListBox::sSeparator,
                        (long)(theBonus * 100.0f), (const char*)EECBUIListBox::sSeparator, (long)inUpgrade,
                        (const char*)EECBUIListBox::sSeparator, theCost, (const char*)EECBUIListBox::sSeparator, thePoints,
                        (const char*)EECBUIListBox::sSeparator, (const char*)theValueText);
                theResult = true;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67df53
// Destroys one of FormatUpgrade's local static strings at exit (compiler-generated $E92)

// FUNCTION: EXE 0x67df5e
// Destroys one of FormatUpgrade's local static strings at exit (compiler-generated $E91)

// FUNCTION: EXE 0x67df69
// Destroys one of FormatUpgrade's local static strings at exit (compiler-generated $E90)

// FUNCTION: EXE 0x67df74
// Destroys one of FormatUpgrade's local static strings at exit (compiler-generated $E89)

// FUNCTION: EXE 0x67df7f
// Finds the header line of a category; returns its index when asked
bool EECBUIListBox::FindCategory(const UWideString& inCategory, unsigned long* outIndex)
{
    UWideString theCategory;
    UWideString theLine;
    UWideString theType;
    bool theResult = false;
    for (unsigned long i = 0; i < this->GetNumLines(); i++) {
        if (this->GetLine(i, theLine) && EECBUIListBox::GetToken(theLine, kCivTokenBonus, theType) && theType == EECBUIListBox::sCategory
            && EECBUIListBox::GetToken(theLine, kCivTokenCategory, theCategory) && theCategory == inCategory) {
            theResult = true;
            if (outIndex != NULL)
                *outIndex = i;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67e056
// Tells whether the line at an index is an upgrade line, copying it when asked
bool EECBUIListBox::IsUpgradeAt(unsigned long inIndex, UWideString* outLine)
{
    UWideString theLine;
    bool theResult = false;
    if (this->GetLine(inIndex, theLine)) {
        theResult = EECBUIListBox::IsUpgradeLine(theLine);
        if (theResult && outLine != NULL)
            *outLine = theLine;
    }
    return theResult;
}

// FUNCTION: EXE 0x67e0af
// Tells whether a formatted line is an upgrade line (it has an upgrade id token)
bool EECBUIListBox::IsUpgradeLine(const UWideString& inLine)
{
    UWideString theUpgrade;
    bool theResult = false;
    if (EECBUIListBox::GetToken(inLine, kCivTokenUpgrade, theUpgrade))
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x67e0e6
// Reads the bonus of a formatted line
bool EECBUIListBox::GetBonus(const UWideString& inLine, unsigned long* outBonus)
{
    UWideString theBonus;
    bool theResult = EECBUIListBox::GetNumber(inLine, kCivTokenBonus, outBonus);
    return theResult;
}

// FUNCTION: EXE 0x67e116
// Reads the id, bonus and cost of the selected upgrade line; returns 3 when line 0 or nothing is selected
long EECBUIListBox::GetSelectedUpgrade(unsigned short* outUpgrade, unsigned long* outBonus, unsigned long* outCost)
{
    UWideString theLine;
    unsigned long theSelection;
    long theResult = this->GetCurrentSelection(theSelection);
    if (theResult != kErrorOK)
        return theResult;
    if (theSelection != 0) {
        if (this->GetLine(theSelection, theLine) && EECBUIListBox::GetUpgrade(theLine, outUpgrade) && EECBUIListBox::GetBonus(theLine, outBonus)
            && EECBUIListBox::GetCost(theLine, outCost))
            theResult = kErrorOK;
    } else
        theResult = 3;
    return theResult;
}

// FUNCTION: EXE 0x67e19e
// Copies the line at an index; false past the end
bool EECBUIListBox::GetLine(unsigned long inIndex, UWideString& outLine) const
{
    bool theResult = false;
    if (inIndex < this->GetNumLines()) {
        outLine = this->mData[inIndex];
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x67e1d4
// Reads the token at an index of a '/'-separated line
bool EECBUIListBox::GetToken(const UWideString& inFormattedString, unsigned long inIndex, UWideString& outEntry)
{
    bool theResult = false;
    std::vector<UWideString> theTokens;
    if (inFormattedString.Tokenize(EECBUIListBox::sSeparator, theTokens) && inIndex < theTokens.size()) {
        outEntry = theTokens[inIndex];
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x67e23c
// Reads the token at an index as an unsigned number (0 when the token is missing)
bool EECBUIListBox::GetNumber(const UWideString& inLine, unsigned long inIndex, unsigned long* outValue)
{
    UWideString theToken;
    bool theResult = EECBUIListBox::GetToken(inLine, inIndex, theToken);
    if (theResult)
        theResult = sscanf(theToken, "%u", outValue) == 1;
    else
        *outValue = 0;
    return theResult;
}

// FUNCTION: EXE 0x67e297
// Reads the upgrade id of a formatted line
bool EECBUIListBox::GetUpgrade(const UWideString& inLine, unsigned short* outUpgrade)
{
    unsigned long theUpgrade;
    bool theResult = EECBUIListBox::GetNumber(inLine, kCivTokenUpgrade, &theUpgrade);
    if (theResult)
        *outUpgrade = (unsigned short)theUpgrade;
    return theResult;
}

// FUNCTION: EXE 0x67e2bb
// Fills the list with every upgrade except the excluded ones, in buy mode
void EECBUIListBox::SetUpgradesExcept(const std::vector<unsigned short>& inExcluded)
{
    std::vector<unsigned short> theUpgrades;
    theUpgrades = gCivilizationManager.GetUpgrades();
    // The removed tail is erased (it was listed too: duplicates or excluded upgrades appeared).
    theUpgrades.erase(std::remove_if(theUpgrades.begin(), theUpgrades.end(), EECBIsUpgradeIn(inExcluded)),
                      theUpgrades.end());
    this->SetUpgrades(theUpgrades, true);
}

// FUNCTION: EXE 0x67e346
// Fills the list with the civilization's own upgrades, in sell mode
void EECBUIListBox::SetCivilizationUpgrades(std::vector<unsigned short>& inUpgrades)
{
    std::vector<unsigned short> theUnused;
    this->SetUpgrades(inUpgrades, false);
}

// FUNCTION: EXE 0x67e378
// Handles a mouse button release as a plain list box
long EECBUIListBox::MouseLButtonUp()
{
    return UIListBox<UWideString>::MouseLButtonUp();
}

// FUNCTION: EXE 0x67e37e
// Handles a mouse move as a plain list box
long EECBUIListBox::MouseMove(const GE2DPoint& inLocation)
{
    return UIListBox<UWideString>::MouseMove(inLocation);
}

// FUNCTION: EXE 0x67e38b
// Removes the first line of the given category (its header); returns whether one was found
bool EECBUIListBox::RemoveCategory(const UWideString& inCategory)
{
    UWideString theCategory;
    UWideString theLine;
    bool theResult = false;
    for (unsigned long i = 0; i < this->GetNumLines(); i++) {
        if (this->GetLine(i, theLine) && EECBUIListBox::GetToken(theLine, kCivTokenCategory, theCategory)
            && theCategory == inCategory) {
            UIListBox<UWideString>::DeleteLine(i);
            theResult = true;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67e426
// Removes the category header of a deleted upgrade line when no upgrade of that category remains
bool EECBUIListBox::RemoveCategoryIfEmpty(const UWideString& inFormattedString)
{
    UWideString theOther;
    UWideString theCategory;
    UWideString theInputEntry;
    bool theResult = false;
    ESVERIFY_AT(this->GetToken(inFormattedString, keecbuiFormatIndexUpgradeName, theInputEntry), 1339);
    if (EECBUIListBox::GetCategory(inFormattedString, theCategory)) {
        bool theFound = false;
        for (unsigned long i = 0; i < this->GetNumLines(); i++) {
            if (this->IsUpgradeAt(i, NULL) && this->GetCategoryAt(i, theOther) && theOther == theCategory) {
                theFound = true;
                break;
            }
        }
        if (!theFound)
            theResult = this->RemoveCategory(theCategory);
    }
    return theResult;
}

// FUNCTION: EXE 0x67e503
// Clears the selection (selects line 0 without an event)
void EECBUIListBox::ResetSelection()
{
    UIListBox<UWideString>::ChangeSelectionIndex(0, false, false);
}

// FUNCTION: EXE 0x67e50f
// Sorts the lines (special powers first, then case-insensitively) when sorting is on
void EECBUIListBox::SortLines()
{
    if (this->mSortLines)
        std::sort(this->mData.begin(), this->mData.end(), EECBLineSorter());
}

// FUNCTION: EXE 0x67e55a
// On tabbing in, moves a selection off a header line in upgrade-only mode and redraws
void EECBUIListBox::TabbedTo()
{
    if (!this->mAutoSelect && this->mSelectUpgradesOnly && !this->mHasSelection && !this->IsUpgradeAt(this->mCurrentSelection, NULL))
        this->ChangeSelectionIndex(0, false, true);
    this->SetDirty(true);
}

// FUNCTION: EXE 0x67e5a2
// Looks up or inserts a category in the map (std::map<UWideString,long>::operator[], template COMDAT)

// FUNCTION: EXE 0x67e5dd
// Moves the upgrades not in the excluded list to the front (std::remove_if, template COMDAT)

// FUNCTION: EXE 0x67e65f
// Sorts the list lines with the line sorter (std::sort, template COMDAT)

// FUNCTION: EXE 0x67e71a
// Finds the first excluded upgrade (std::find_if, template COMDAT of remove_if)

// FUNCTION: EXE 0x67e744
// Copies the upgrades not in the excluded list (std::remove_copy_if, template COMDAT of remove_if)

// FUNCTION: EXE 0x67e77e
// Insertion-sorts the list lines (std::_Insertion_sort, template COMDAT of sort)

// FUNCTION: EXE 0x67e7a7
// Introsorts the list lines (std::_Sort, template COMDAT of sort)

// FUNCTION: EXE 0x67e880
// Insertion-sorts a sorted-prefix range of lines (std::_Unguarded_insertion_sort, template COMDAT of sort)

// FUNCTION: EXE 0x67e8c0
// Tells whether an upgrade is in the excluded list (the remove_if predicate)

// FUNCTION: EXE 0x67e8e3
// Insertion-sorts a range of lines (std::_Insertion_sort_1, template COMDAT of sort)

// FUNCTION: EXE 0x67e9ac
// Orders two list lines: special powers lines first, then case-insensitively (the sort predicate)

// FUNCTION: EXE 0x67eaff
// Builds a heap of list lines (std::make_heap, template COMDAT of sort)

// FUNCTION: EXE 0x67eb3c
// Sorts a heap of list lines (std::sort_heap, template COMDAT of sort)

// FUNCTION: EXE 0x67eb7b
// Partitions list lines around a pivot (std::_Unguarded_partition, template COMDAT of sort)

// FUNCTION: EXE 0x67eca5
// Inserts a line into its sorted place (std::_Unguarded_insert, template COMDAT of sort)

// FUNCTION: EXE 0x67ecf9
// Builds a heap of list lines (std::_Make_heap, template COMDAT of sort)

// FUNCTION: EXE 0x67ed57
// Pops the top of a heap of list lines (std::pop_heap, template COMDAT of sort)

// FUNCTION: EXE 0x67ed80
// Sifts a line down a heap (std::_Adjust_heap, template COMDAT of sort)

// FUNCTION: EXE 0x67ee26
// Pops the top of a heap of list lines (std::_Pop_heap_0, template COMDAT of sort)

// FUNCTION: EXE 0x67ee95
// Sifts a line up a heap (std::_Push_heap, template COMDAT of sort)

// FUNCTION: EXE 0x67ef08
// Pops the top of a heap of list lines into a slot (std::_Pop_heap, template COMDAT of sort)

// FUNCTION: EXE 0x67ef57
// Tells whether the list box takes the keyboard focus by tabbing (inline in the header)

// FUNCTION: EXE 0x67ef5e
// Returns the distance between two line iterators (_Ptrit::operator-, template COMDAT)

// FUNCTION: EXE 0x67ef6f
// Swaps two list lines (std::iter_swap, template COMDAT of sort)

// FUNCTION: EXE 0x67ef94
// Swaps two list lines through a temporary (std::_Iter_swap, template COMDAT of sort)

// SYNTHETIC: EXE 0x67cfdb
// SYNTHETIC: EXE 0x67cfe5
// SYNTHETIC: EXE 0x67cff6
// SYNTHETIC: EXE 0x67d002
// SYNTHETIC: EXE 0x67d01e
// SYNTHETIC: EXE 0x67d028
// SYNTHETIC: EXE 0x67d039
// SYNTHETIC: EXE 0x67d045
// SYNTHETIC: EXE 0x67d061
// SYNTHETIC: EXE 0x67d06b
// SYNTHETIC: EXE 0x67d07c
// SYNTHETIC: EXE 0x67d088
// SYNTHETIC: EXE 0x67d093
// SYNTHETIC: EXE 0x67d09d
// SYNTHETIC: EXE 0x67d0ae
// SYNTHETIC: EXE 0x67d0ba
// SYNTHETIC: EXE 0x67d0c5
// SYNTHETIC: EXE 0x67d0cf
// SYNTHETIC: EXE 0x67d0e8
// SYNTHETIC: EXE 0x67d0f4
// SYNTHETIC: EXE 0x67d0f5
// SYNTHETIC: EXE 0x67d0ff
// SYNTHETIC: EXE 0x67d118
// SYNTHETIC: EXE 0x67d124
// SYNTHETIC: EXE 0x67d125
// SYNTHETIC: EXE 0x67d12f
// SYNTHETIC: EXE 0x67d14a
// SYNTHETIC: EXE 0x67d156
// SYNTHETIC: EXE 0x67d157
// SYNTHETIC: EXE 0x67d161
// SYNTHETIC: EXE 0x67d17a
// SYNTHETIC: EXE 0x67d186
