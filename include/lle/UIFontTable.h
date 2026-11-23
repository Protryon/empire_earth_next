// C:\EEX\Low-Level Engine\User Interface\UIFontTable.h (RECONSTRUCTION). The UI database's font records and the
// shared GEFont cache. UIFontTable.cpp (0x1006ff58..0x1007042f) holds the ctor and the static members; the dtor and
// the implicit copy members are emitted by UIControlsTable.cpp (0x100676af..). Vtable 0x10099960 (the dtor only).
// sizeof 0x20. Requires lle/LowLevelEngine.h, lle/GEFont.h.
#ifndef LLE_UIFONTTABLE_H
#define LLE_UIFONTTABLE_H

#include "rust/ui/fonttableobj.h"
#include "rust/ui/lle_hosts.h"

class FSFileSpec;
class GEFont;
class UIManager;

// Whole in Rust (rust/crates/ee_ui/src/fonttableobj.rs, R6 ui round 18): the records and the shared fonts are the
// Rust statics; every member forwards.
class LLE_API UIFontTable {
public:
    UIFontTable() { }
    virtual ~UIFontTable() {}
    // Loads the UI font table from the database file: face name, size, quality and style per entry; returns the read result
    static long Create(FSFileSpec& inFile, UIManager* inManager) { return (long)ee_ui_font_tables(EeUiFontsCall::Create, (intptr_t)&inFile, (intptr_t)inManager, 0, 0, ee_ui_font_table_host); }
    // Returns a shared font for a table entry scaled to a pixel height on the screen (NULL for an empty entry)
    static GEFont* CreateClosestFont(unsigned long inIndex, unsigned long inPixelHeight) { return reinterpret_cast<GEFont*>( ee_ui_font_tables(EeUiFontsCall::CreateClosestFont, inIndex, inPixelHeight, 0, 0, ee_ui_font_table_host)); }
    // Deletes every font table entry, drops one reference to each shared font and empties both lists
    static void DeleteAll() { ee_ui_font_tables(EeUiFontsCall::DeleteAll, 0, 0, 0, 0, ee_ui_font_table_host); }
    // Returns the shared font for a table entry (NULL for an empty entry: the original read through it)
    static GEFont* GetFont(unsigned long inIndex) { return reinterpret_cast<GEFont*>(ee_ui_font_tables(EeUiFontsCall::GetFont, inIndex, 0, 0, 0, ee_ui_font_table_host)); }
    // Returns a shared font with the given face, style, quality and size, creating it on first use; adds a reference
    static GEFont* FindOrCreateFont(const UString& inName, unsigned long inStyle, unsigned long inQuality, unsigned long inSize) { return reinterpret_cast<GEFont*>(ee_ui_font_tables(EeUiFontsCall::FindOrCreateFont, (intptr_t)&inName, inStyle, inQuality, inSize, ee_ui_font_table_host)); }
};

#endif
