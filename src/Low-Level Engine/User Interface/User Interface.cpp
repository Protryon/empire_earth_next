// C:\EEX\Low-Level Engine\User Interface\User Interface.cpp (file name inferred, by analogy with
// Thread System\Thread System.cpp: the subsystem's initialize/shutdown entry points)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10082871..0x10082c43: the init chain of an unused file-static UIBackgroundTable, the gDbUI* file name chains, then
// UICreateMultilineTextFromString, UIGetStringCutOffIndex, UIInitialize and its shutdown hook. UString.cpp starts
// at 0x10082c43.
#include <windows.h>
#include <mbstring.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/UIBackgroundTable.h"

// Constructed and destroyed, referenced by nothing (its chain comes first) (provisional name)
static UIBackgroundTable sBackgroundTable;

// the UI database file names (declared in lle/UIManager.h; UIManager::Create opens them)
LLE_API UString gDbUIBackgroundFileName("DB\\dbuiback.dat");            // 0x1012ec70
LLE_API UString gDbUIControlsFileName("DB\\dbuicontrols.dat");          // 0x1012ec60
LLE_API UString gDbUIFormsFileName("DB\\dbuiforms.dat");                // 0x1012ec50
LLE_API UString gDbUIControlEventsFileName("DB\\dbuicontrolevents.dat"); // 0x1012ec40
LLE_API UString gDbUIFormEventsFileName("DB\\dbuiformevents.dat");      // 0x1012ec30
LLE_API UString gDbUIFontsFileName("DB\\dbuifonts.dat");                // 0x1012ec20
LLE_API UString gDbUIHotKeyFileName("DB\\dbuihotkey.dat");              // 0x1012ec10

#include "lle/GEText.h"
#include "lle/User Interface.h"

static void UIShutdown(bool inForced);

long gUIInitialized;                        // 0x1012ed00 (provisional name)

// FUNCTION: LLE 0x100829f5
// Splits a string into lines that fit the given pixel width; a newline ends a line
void UICreateMultilineTextFromString(const UWideString& inString, GEFont* inFont, unsigned long inWidth,
                                     std::vector<UWideString>& outLines)
{
    UWideString theLine;
    const char* theCurrent = inString;
    while (_mbscmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"") != 0) {
        unsigned long theCount = UIGetStringCutOffIndex(theCurrent, inFont, inWidth, true, true);
        bool theAtNewline = _mbsnbcmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"\n", 1) == 0;
        if (theCount == 0 && !theAtNewline)
            theCount = 1;   // always make progress
        theLine.CopyString(theCurrent, theCount);
        outLines.push_back(theLine);
        theCurrent = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>(theCurrent), theCount);
        // Fixed: the cut-off index stops before a newline without counting it, so the loop used to stay on the
        // newline forever (pushing empty lines); the newline that ends a line is skipped now
        if (_mbsnbcmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"\n", 1) == 0)
            theCurrent = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(theCurrent));
    }
}

// FUNCTION: LLE 0x10082a85
// Returns how many characters fit the pixel width (tab = 4 spaces), cutting at a space or newline; all if none fit
unsigned long UIGetStringCutOffIndex(const UWideString& inString, GEFont* inFont, unsigned long inWidth,
                                     bool inBreakAtSpace, bool inStopAtNewline)
{
    GEText theText(inFont);
    UWideString theChar;
    float theWidth = 0.0f;
    unsigned long theSpaceIndex = 0;
    unsigned long theTabCount = 0;
    unsigned long theIndex = 0;
    bool theNewline = false;
    bool theFoundSpace = false;
    const char* theCurrent = inString;
    while (_mbscmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"") != 0) {
        if (theTabCount == 0) {
            theChar.CopyString(theCurrent, 1);
            if (inStopAtNewline && _mbsnbcmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"\n", 1) == 0)
                theNewline = true;
            if (inBreakAtSpace && _ismbcspace(GetMultiByteChar(theCurrent))) {
                theFoundSpace = true;
                theSpaceIndex = theIndex + 1;
            }
            if (_mbsnbcmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"\r", 1) == 0)
                theChar = "";
            else if (_mbsnbcmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"\t", 1) == 0) {
                theTabCount = 4;
                theChar = "";
            }
        } else {
            theChar = " ";
            theTabCount--;
        }
        theText.SetText(theChar);
        theWidth += theText.GetTextExtent().GetWidth();
        if (theWidth > inWidth) {
            if (!theNewline && theFoundSpace)
                theIndex = theSpaceIndex;
            break;
        }
        if (theNewline)
            break;
        if (theTabCount == 0)
            theCurrent = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(theCurrent));
        theIndex++;
    }
    if (theCurrent == (const char*)inString && !theNewline)
        theIndex = inString.GetLength();
    return theIndex;
}

// FUNCTION: LLE 0x10082c26
// Marks the user interface initialized and registers its shutdown hook; always succeeds
long UIInitialize()
{
    gUIInitialized = 1;
    URegisterShutdown(UIShutdown);
    return kErrorOK;
}

// FUNCTION: LLE 0x10082c3e
// Shutdown hook: deletes every background table
static void UIShutdown(bool inForced)
{
    UIBackgroundTable::DeleteAll();
}

// Compiler-generated: the init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10082871
// SYNTHETIC: LLE 0x1008287b
// SYNTHETIC: LLE 0x10082885
// SYNTHETIC: LLE 0x10082891
// SYNTHETIC: LLE 0x100828a5
// SYNTHETIC: LLE 0x100828af
// SYNTHETIC: LLE 0x100828bf
// SYNTHETIC: LLE 0x100828cb
// SYNTHETIC: LLE 0x100828d5
// SYNTHETIC: LLE 0x100828df
// SYNTHETIC: LLE 0x100828ef
// SYNTHETIC: LLE 0x100828fb
// SYNTHETIC: LLE 0x10082905
// SYNTHETIC: LLE 0x1008290f
// SYNTHETIC: LLE 0x1008291f
// SYNTHETIC: LLE 0x1008292b
// SYNTHETIC: LLE 0x10082935
// SYNTHETIC: LLE 0x1008293f
// SYNTHETIC: LLE 0x1008294f
// SYNTHETIC: LLE 0x1008295b
// SYNTHETIC: LLE 0x10082965
// SYNTHETIC: LLE 0x1008296f
// SYNTHETIC: LLE 0x1008297f
// SYNTHETIC: LLE 0x1008298b
// SYNTHETIC: LLE 0x10082995
// SYNTHETIC: LLE 0x1008299f
// SYNTHETIC: LLE 0x100829af
// SYNTHETIC: LLE 0x100829bb
// SYNTHETIC: LLE 0x100829c5
// SYNTHETIC: LLE 0x100829cf
// SYNTHETIC: LLE 0x100829df
// SYNTHETIC: LLE 0x100829eb
