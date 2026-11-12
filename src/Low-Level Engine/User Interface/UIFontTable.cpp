// C:\EEX\Low-Level Engine\User Interface\UIFontTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006ff58..0x1007042f: the U2D key init chains, the sFontTable/sFont init chains (guarded atexit dtors), the ctor,
// the static members, vector<GEFont*>::push_back last. The dtor and the implicit copy members are emitted by
// UIControlsTable.cpp. UIForm.cpp's key chains start at 0x1007042f.
#include <windows.h>
#include <string.h>

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
    uint32_t mStringID;                     // +0x6c face name in the string table
    uint32_t mSize;                         // +0x70
    uint32_t mQuality;                      // +0x74
    bool mBold;                             // +0x78 style bit 0x01
    bool mItalic;                           // +0x79 style bit 0x02
    bool mUnderline;                        // +0x7a style bit 0x04
    bool mStyle10;                          // +0x7b style bit 0x10
    bool mStyle08;                          // +0x7c style bit 0x08
    char mPad[3];
};
static_assert(sizeof(UIDbDataFonts) == 128, "on-disk record: 128 bytes on every target");
static_assert(sizeof(UIDbDataFonts) == sizeof(EeUiDbFont) && offsetof(UIDbDataFonts, mStringID) == offsetof(EeUiDbFont, string_id) &&
                  offsetof(UIDbDataFonts, mBold) == offsetof(EeUiDbFont, bold) &&
                  offsetof(UIDbDataFonts, mStyle08) == offsetof(EeUiDbFont, style08),
              "the record ee_ui font.rs reads");

std::vector<UIFontTable*> UIFontTable::sFontTable;
std::vector<GEFont*> UIFontTable::sFont;

// The Rust search's view of the shared fonts (ee_ui font.rs, docs/RUST_PLAN.md R6): the query, the list, a font's
// CompareFont, Reference and construction
struct UIFontQuery {
    const UString* mName;
    unsigned long mStyle;
    unsigned long mQuality;
    unsigned long mSize;
};
struct UIFontTableRust {
    static std::vector<GEFont*>& Fonts() { return UIFontTable::sFont; }
};
extern "C" size_t ee_uicpp_font_count() { return UIFontTableRust::Fonts().size(); }
extern "C" bool ee_uicpp_font_matches(const void* inQuery, size_t inIndex)
{
    const UIFontQuery* q = static_cast<const UIFontQuery*>(inQuery);
    return UIFontTableRust::Fonts()[inIndex]->CompareFont(*q->mName, q->mStyle, q->mQuality, q->mSize);
}
extern "C" void ee_uicpp_font_take(size_t inIndex) { UIFontTableRust::Fonts()[inIndex]->Reference(); }
extern "C" size_t ee_uicpp_font_create(const void* inQuery)
{
    const UIFontQuery* q = static_cast<const UIFontQuery*>(inQuery);
    GEFont* theFont = new GEFont(*q->mName, q->mSize, q->mStyle, q->mQuality);
    theFont->Reference();
    UIFontTableRust::Fonts().push_back(theFont);
    return UIFontTableRust::Fonts().size() - 1;
}

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
            EeUiFontEntry theEntry = ee_ui_font_entry(reinterpret_cast<const EeUiDbFont*>(&theData));
            if (theEntry.present) {
                theTable = new UIFontTable;
                inManager->GetStringTable()->GetString(theEntry.string_id, theString);
                theTable->mName.Copy(theString, 0, theString.GetLength(), 0, true);
                theTable->mStyle = theEntry.style;
                theTable->mQuality = theEntry.quality;
                theTable->mSize = theEntry.size;
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
    UIFontTable* theTable = UIFontTable::sFontTable[inIndex];
    if (theTable) {
        // the screen's LOGPIXELSY was 96 for the game (a DPI-unaware process): fonts are sized in game pixels
        long theSize = ee_ui_font_closest_size(inPixelHeight);
        theFont = UIFontTable::FindOrCreateFont(theTable->mName, theTable->mStyle, theTable->mQuality, theSize);
    }
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
    UIFontQuery theQuery = {&inName, inStyle, inQuality, inSize};
    return UIFontTableRust::Fonts()[ee_ui_font_find_or_create(&theQuery)];
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
