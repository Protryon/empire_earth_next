// C:\EEX\Low-Level Engine\User Interface\UIFontTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006ff58..0x1007042f: the U2D key init chains, the sFontTable/sFont init chains (guarded atexit dtors), the ctor,
// the static members, vector<GEFont*>::push_back last. The dtor and the implicit copy members are emitted by
// UIControlsTable.cpp. UIForm.cpp's key chains start at 0x1007042f.
#include <windows.h>
#include <string.h>

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
#include "lle/GERhombus.h"
#include "lle/GEFont.h"
#include "lle/UResourceFile.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIFontTable.h"

// One font record of the UI database (0x80 bytes) (provisional names).
struct UIDbDataFonts {
    char mFontName[0x6c];                   // +0x00 empty: no font
    unsigned long mStringID;                // +0x6c face name in the string table
    unsigned long mSize;                    // +0x70
    unsigned long mQuality;                 // +0x74
    bool mBold;                             // +0x78 style bit 0x01
    bool mItalic;                           // +0x79 style bit 0x02
    bool mUnderline;                        // +0x7a style bit 0x04
    bool mStyle10;                          // +0x7b style bit 0x10
    bool mStyle08;                          // +0x7c style bit 0x08
    char mPad[3];
};

std::vector<UIFontTable*> UIFontTable::sFontTable;
std::vector<GEFont*> UIFontTable::sFont;

// FUNCTION: LLE 0x1007010a
// Creates an empty font table entry
UIFontTable::UIFontTable()
{
}

// FUNCTION: LLE 0x1007011f
// Loads the UI font table from the database file: face name, size, quality and style per entry; returns the read result
long UIFontTable::Create(FSFileSpec& inFile, UIManager* inManager)
{
    UWideString theString;
    UString theUnused;
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    long theResult;
    inFile.Open(kFileAccessRead);
    theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        UIFontTable::sFontTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            UIDbDataFonts theData;
            UIFontTable* theTable;
            theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theData), sizeof(theData), theRead);
            if (strlen(theData.mFontName) > 0) {
                theTable = new UIFontTable;
                inManager->GetStringTable()->GetString(theData.mStringID, theString);
                theTable->mName.Copy(theString, 0, theString.GetLength(), 0, true);
                unsigned long theStyle = 0;
                if (theData.mStyle08)
                    theStyle = 8;
                if (theData.mStyle10)
                    theStyle |= kFontStyleShadow;
                if (theData.mUnderline)
                    theStyle |= kFontStyleUnderline;
                if (theData.mItalic)
                    theStyle |= kFontStyleItalic;
                if (theData.mBold)
                    theStyle |= kFontStyleBold;
                theTable->mStyle = theStyle;
                theTable->mQuality = theData.mQuality;
                theTable->mSize = theData.mSize;
            } else
                theTable = NULL;
            UIFontTable::sFontTable[i] = theTable;
        }
    }
    inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x1007025f
// Returns a shared font for a table entry scaled to a pixel height on the screen (NULL for an empty entry)
GEFont* UIFontTable::CreateClosestFont(unsigned long inIndex, unsigned long inPixelHeight)
{
    GEFont* theFont = NULL;
    HDC theDC = GEDeviceContext::GetDeviceContext(NULL);
    UIFontTable* theTable = UIFontTable::sFontTable[inIndex];
    if (theTable) {
        long theSize = (long)((__int64)inPixelHeight * 72 / GetDeviceCaps(theDC, LOGPIXELSY)) + 1;
        theFont = UIFontTable::FindOrCreateFont(theTable->mName, theTable->mStyle, theTable->mQuality, theSize);
    }
    GEDeviceContext::ReleaseDeviceContext(NULL, theDC);
    return theFont;
}

// FUNCTION: LLE 0x100702d2
// Deletes every font table entry, drops one reference to each shared font and empties both lists
void UIFontTable::DeleteAll()
{
    unsigned long i;
    for (i = 0; i < UIFontTable::sFontTable.size(); i++) {
        if (UIFontTable::sFontTable[i])
            delete UIFontTable::sFontTable[i];
    }
    for (i = 0; i < UIFontTable::sFont.size(); i++)
        UIFontTable::sFont[i]->Dereference();
    UIFontTable::sFontTable.resize(0, NULL);
    // Fixed: the shared font list used to be kept, so a later lookup or DeleteAll used released fonts
    UIFontTable::sFont.clear();
}

// FUNCTION: LLE 0x10070337
// Returns the shared font for a table entry (no check for an empty entry)
GEFont* UIFontTable::GetFont(unsigned long inIndex)
{
    UIFontTable* theTable = UIFontTable::sFontTable[inIndex];
    return UIFontTable::FindOrCreateFont(theTable->mName, theTable->mStyle, theTable->mQuality, theTable->mSize);
}

// FUNCTION: LLE 0x1007035a
// Returns a shared font with the given face, style, quality and size, creating it on first use; adds a reference
GEFont* UIFontTable::FindOrCreateFont(const UString& inName, unsigned long inStyle, unsigned long inQuality,
                                      unsigned long inSize)
{
    GEFont* theFont = NULL;
    for (unsigned long i = 0; i < UIFontTable::sFont.size(); i++) {
        if (!UIFontTable::sFont[i]->CompareFont(inName, inStyle, inQuality, inSize))
            continue;
        theFont = UIFontTable::sFont[i];
        theFont->Reference();
        break;
    }
    if (theFont == NULL) {
        theFont = new GEFont(inName, inSize, inStyle, inQuality);
        theFont->Reference();
        UIFontTable::sFont.push_back(theFont);
    }
    return theFont;
}

// vector<GEFont*>::push_back
// SYNTHETIC: LLE 0x10070403
// Compiler-generated: the U2D key and static vector init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1006ff58
// SYNTHETIC: LLE 0x1006ff62
// SYNTHETIC: LLE 0x1006ff72
// SYNTHETIC: LLE 0x1006ff7e
// SYNTHETIC: LLE 0x1006ff88
// SYNTHETIC: LLE 0x1006ff92
// SYNTHETIC: LLE 0x1006ffa2
// SYNTHETIC: LLE 0x1006ffae
// SYNTHETIC: LLE 0x1006ffb8
// SYNTHETIC: LLE 0x1006ffc2
// SYNTHETIC: LLE 0x1006ffd2
// SYNTHETIC: LLE 0x1006ffde
// SYNTHETIC: LLE 0x1006ffe8
// SYNTHETIC: LLE 0x1006fff2
// SYNTHETIC: LLE 0x10070002
// SYNTHETIC: LLE 0x1007000e
// SYNTHETIC: LLE 0x10070018
// SYNTHETIC: LLE 0x10070022
// SYNTHETIC: LLE 0x10070032
// SYNTHETIC: LLE 0x1007003e
// SYNTHETIC: LLE 0x10070048
// SYNTHETIC: LLE 0x10070052
// SYNTHETIC: LLE 0x10070062
// SYNTHETIC: LLE 0x1007006e
// SYNTHETIC: LLE 0x10070078
// SYNTHETIC: LLE 0x10070082
// SYNTHETIC: LLE 0x1007009a
// SYNTHETIC: LLE 0x100700a6
// SYNTHETIC: LLE 0x100700c1
// SYNTHETIC: LLE 0x100700cb
// SYNTHETIC: LLE 0x100700e3
// SYNTHETIC: LLE 0x100700ef
