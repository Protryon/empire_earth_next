// C:\EEX\Low-Level Engine\User Interface\UIFontTable.h (RECONSTRUCTION). The UI database's font records and the
// shared GEFont cache. UIFontTable.cpp (0x1006ff58..0x1007042f) holds the ctor and the static members; the dtor and
// the implicit copy members are emitted by UIControlsTable.cpp (0x100676af..). Vtable 0x10099960 (the dtor only).
// sizeof 0x20. Requires lle/LowLevelEngine.h, lle/GEFont.h.
#ifndef LLE_UIFONTTABLE_H
#define LLE_UIFONTTABLE_H

#include <vector>

class FSFileSpec;
class GEFont;
class UIManager;

class LLE_API UIFontTable {
    friend struct UIFontTableRust;          // UIFontTable.cpp: the Rust search's view of sFont (ee_ui font.rs)
public:
    UIFontTable();
    virtual ~UIFontTable() {}
    static long Create(FSFileSpec& inFile, UIManager* inManager);
    // a font of the record's face and style whose point size fits inPixelHeight on the screen
    static GEFont* CreateClosestFont(unsigned long inIndex, unsigned long inPixelHeight);
    static void DeleteAll();
    static GEFont* GetFont(unsigned long inIndex);
    static GEFont* FindOrCreateFont(const UString& inName, unsigned long inStyle, unsigned long inQuality,
                                    unsigned long inSize);
protected:
    UString mName;                          // +0x04
    unsigned long mStyle;                   // +0x14
    unsigned long mQuality;                 // +0x18
    unsigned long mSize;                    // +0x1c
private:
#ifdef LOWLEVELENGINE_EXPORTS
    static std::vector<GEFont*> sFont;              // 0x1012e610 every font handed out (referenced)
    static std::vector<UIFontTable*> sFontTable;    // 0x1012e628 one per database record
#endif
};

#endif
