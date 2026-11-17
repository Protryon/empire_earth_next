// C:\EEX\Low-Level Engine\User Interface\UIFontTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006ff58..0x1007042f: the U2D key init chains, the sFontTable/sFont init chains (guarded atexit dtors), the ctor,
// the static members, vector<GEFont*>::push_back last. The dtor and the implicit copy members are emitted by
// UIControlsTable.cpp. UIForm.cpp's key chains start at 0x1007042f.
//
// UIFontTable is whole in Rust (rust/crates/ee_ui/src/fonttableobj.rs, R6 ui round 18): the records, the shared
// font list, Create, CreateClosestFont, GetFont, FindOrCreateFont and DeleteAll; this unit keeps the forwarders and
// the host of their calls out (UIFontTableHost: the file, the string table's face names, the GEFonts).
#include <windows.h>
#include <string.h>

#include "rust/ui/fonttableobj.h"
#include "rust/ui/obj.h"
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

// The Rust font table's calls out (EeUiOp, obj.rs; ee_ui fonttableobj.rs): a the file, manager, name or font
static intptr_t UIFontTableHost(void* inObject, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
{
    static UWideString sFace;   // Create's face name (kept between records, as the original's local was)
    FSFileSpec* theFile = reinterpret_cast<FSFileSpec*>(a);
    GEFont* theFont = reinterpret_cast<GEFont*>(a);
    const UString& theName = *reinterpret_cast<const UString*>(b);
    (void)inObject;
    (void)f;
    switch (inOp) {
    case EeUiOp::TblFileOpen: theFile->Open(kFileAccessRead); return 0;
    case EeUiOp::TblFileReadCount: {
        unsigned long theCount = 0;
        long theResult = theFile->Read(theCount);
        return (intptr_t)((uint64_t)theCount << 32 | (uint32_t)theResult);
    }
    case EeUiOp::TblFileRead: {
        unsigned long theRead = 0;
        return theFile->Read(reinterpret_cast<unsigned char*>(b), (unsigned long)c, theRead);
    }
    case EeUiOp::TblFileClose: theFile->Close(); return 0;
    case EeUiOp::FontName: {
        reinterpret_cast<UIManager*>(a)->GetStringTable()->GetString((unsigned long)b, sFace);
        UString* theCopy = new UString;
        theCopy->Copy(sFace, 0, sFace.GetLength(), 0, true);
        return (intptr_t)theCopy;
    }
    case EeUiOp::FontNameDelete: delete reinterpret_cast<UString*>(a); return 0;
    case EeUiOp::FontCompare:
        return theFont->CompareFont(theName, (unsigned long)c, (unsigned long)(uint32_t)d, (unsigned long)((uint64_t)d >> 32));
    case EeUiOp::FontReference: theFont->Reference(); return 0;
    case EeUiOp::FontNew: {
        GEFont* theNew = new GEFont(*reinterpret_cast<const UString*>(a), (unsigned long)d, (unsigned long)b, (unsigned long)c);
        theNew->Reference();
        return (intptr_t)theNew;
    }
    case EeUiOp::FontRelease: theFont->Dereference(); return 0;
    default: return 0;
    }
}

// Creates an empty font table entry
UIFontTable::UIFontTable()
{
}

// Loads the UI font table from the database file: face name, size, quality and style per entry; returns the read result
long UIFontTable::Create(FSFileSpec& inFile, UIManager* inManager)
{
    return (long)ee_ui_font_tables(EeUiFontsCall::Create, (intptr_t)&inFile, (intptr_t)inManager, 0, 0, UIFontTableHost);
}

// Returns a shared font for a table entry scaled to a pixel height on the screen (NULL for an empty entry)
GEFont* UIFontTable::CreateClosestFont(unsigned long inIndex, unsigned long inPixelHeight)
{
    return reinterpret_cast<GEFont*>(
        ee_ui_font_tables(EeUiFontsCall::CreateClosestFont, inIndex, inPixelHeight, 0, 0, UIFontTableHost));
}

// Deletes every font table entry, drops one reference to each shared font and empties both lists
void UIFontTable::DeleteAll() { ee_ui_font_tables(EeUiFontsCall::DeleteAll, 0, 0, 0, 0, UIFontTableHost); }

// Returns the shared font for a table entry (NULL for an empty entry: the original read through it)
GEFont* UIFontTable::GetFont(unsigned long inIndex)
{
    return reinterpret_cast<GEFont*>(ee_ui_font_tables(EeUiFontsCall::GetFont, inIndex, 0, 0, 0, UIFontTableHost));
}

// Returns a shared font with the given face, style, quality and size, creating it on first use; adds a reference
GEFont* UIFontTable::FindOrCreateFont(const UString& inName, unsigned long inStyle, unsigned long inQuality,
                                      unsigned long inSize)
{
    return reinterpret_cast<GEFont*>(ee_ui_font_tables(EeUiFontsCall::FindOrCreateFont, (intptr_t)&inName, inStyle,
                                                       inQuality, inSize, UIFontTableHost));
}
