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
LLE_API UString gDbUIBackgroundFileName("DB\\dbuiback.dat");
LLE_API UString gDbUIControlsFileName("DB\\dbuicontrols.dat");
LLE_API UString gDbUIFormsFileName("DB\\dbuiforms.dat");
LLE_API UString gDbUIControlEventsFileName("DB\\dbuicontrolevents.dat");
LLE_API UString gDbUIFormEventsFileName("DB\\dbuiformevents.dat");
LLE_API UString gDbUIFontsFileName("DB\\dbuifonts.dat");
LLE_API UString gDbUIHotKeyFileName("DB\\dbuihotkey.dat");

#include "lle/GEText.h"
#include "lle/User Interface.h"

static void UIShutdown(bool inForced);

long gUIInitialized;

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
    while (PlatMbsCmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"") != 0) {
        if (theTabCount == 0) {
            theChar.CopyString(theCurrent, 1);
            if (inStopAtNewline && PlatMbsNBCmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"\n", 1) == 0)
                theNewline = true;
            if (inBreakAtSpace && PlatIsMbcSpace(GetMultiByteChar(theCurrent))) {
                theFoundSpace = true;
                theSpaceIndex = theIndex + 1;
            }
            if (PlatMbsNBCmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"\r", 1) == 0)
                theChar = "";
            else if (PlatMbsNBCmp(reinterpret_cast<const unsigned char*>(theCurrent), (const unsigned char*)"\t", 1) == 0) {
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
            theCurrent = (const char*)PlatMbsInc(reinterpret_cast<const unsigned char*>(theCurrent));
        theIndex++;
    }
    if (theCurrent == (const char*)inString && !theNewline)
        theIndex = inString.GetLength();
    return theIndex;
}

// Marks the user interface initialized and registers its shutdown hook; always succeeds
long UIInitialize()
{
    gUIInitialized = 1;
    URegisterShutdown(UIShutdown);
    return kErrorOK;
}

// Shutdown hook: deletes every background table
static void UIShutdown(bool inForced)
{
    UIBackgroundTable::DeleteAll();
}
