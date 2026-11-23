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
#include "lle/UIShims.h"
#include "rust/ui/lle_hosts.h"

// The Rust font table's calls out (EeUiOp, obj.rs; ee_ui fonttableobj.rs): a the file, manager, name or font
static UWideString sFace;
#define theFile reinterpret_cast<FSFileSpec*>(a)
#define theFont reinterpret_cast<GEFont*>(a)
#define theName (*reinterpret_cast<const UString*>(b))
EE_UI_VOID(font_tbl_file_open) { theFile->Open(kFileAccessRead); }
EE_UI_VOID(font_tbl_file_close) { theFile->Close(); }
EE_UI_CASE(font_name)
{
    reinterpret_cast<UIManager*>(a)->GetStringTable()->GetString((unsigned long)b, sFace);
    UString* theCopy = new UString;
    theCopy->Copy(sFace, 0, sFace.GetLength(), 0, true);
    return (intptr_t)theCopy;
}
EE_UI_VOID(font_name_delete) { delete reinterpret_cast<UString*>(a); }
EE_UI_CASE(font_compare) { return theFont->CompareFont(theName, (unsigned long)c, (unsigned long)(uint32_t)d, (unsigned long)((uint64_t)d >> 32)); }
EE_UI_VOID(font_reference) { theFont->Reference(); }
EE_UI_CASE(font_create) { return (intptr_t) new GEFont(*reinterpret_cast<const UString*>(a), (unsigned long)d, (unsigned long)b, (unsigned long)c); }
EE_UI_VOID(font_release) { theFont->Dereference(); }
#undef theName
#undef theFont
#undef theFile
// Creates an empty font table entry
UIFontTable::UIFontTable()
{
}

long UIFontTable::Create(FSFileSpec& inFile, UIManager* inManager) { return (long)ee_ui_font_tables(EeUiFontsCall::Create, (intptr_t)&inFile, (intptr_t)inManager, 0, 0, ee_ui_font_table_host); }

void UIFontTable::DeleteAll() { ee_ui_font_tables(EeUiFontsCall::DeleteAll, 0, 0, 0, 0, ee_ui_font_table_host); }

GEFont* UIFontTable::GetFont(unsigned long inIndex) { return reinterpret_cast<GEFont*>(ee_ui_font_tables(EeUiFontsCall::GetFont, inIndex, 0, 0, 0, ee_ui_font_table_host)); }

GEFont* UIFontTable::CreateClosestFont(unsigned long inIndex, unsigned long inPixelHeight)
{
    return reinterpret_cast<GEFont*>(
        ee_ui_font_tables(EeUiFontsCall::CreateClosestFont, inIndex, inPixelHeight, 0, 0, ee_ui_font_table_host));
}

GEFont* UIFontTable::FindOrCreateFont(const UString& inName, unsigned long inStyle, unsigned long inQuality,
    unsigned long inSize)
{
    return reinterpret_cast<GEFont*>(ee_ui_font_tables(EeUiFontsCall::FindOrCreateFont, (intptr_t)&inName, inStyle,
                                                       inQuality, inSize, ee_ui_font_table_host));
}
