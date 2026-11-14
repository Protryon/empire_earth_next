// C:\EEX\Low-Level Engine\User Interface\UIFontTable.h (RECONSTRUCTION). The UI database's font records and the
// shared GEFont cache. UIFontTable.cpp (0x1006ff58..0x1007042f) holds the ctor and the static members; the dtor and
// the implicit copy members are emitted by UIControlsTable.cpp (0x100676af..). Vtable 0x10099960 (the dtor only).
// sizeof 0x20. Requires lle/LowLevelEngine.h, lle/GEFont.h.
#ifndef LLE_UIFONTTABLE_H
#define LLE_UIFONTTABLE_H

class FSFileSpec;
class GEFont;
class UIManager;

// Whole in Rust (rust/crates/ee_ui/src/fonttableobj.rs, R6 ui round 18): the records and the shared fonts are the
// Rust statics; every member forwards.
class LLE_API UIFontTable {
public:
    UIFontTable();
    virtual ~UIFontTable() {}
    static long Create(FSFileSpec& inFile, UIManager* inManager);
    static GEFont* CreateClosestFont(unsigned long inIndex, unsigned long inPixelHeight);
    static void DeleteAll();
    static GEFont* GetFont(unsigned long inIndex);
    static GEFont* FindOrCreateFont(const UString& inName, unsigned long inStyle, unsigned long inQuality,
                                    unsigned long inSize);
};

#endif
