// C:\EEX\Low-Level Engine\User Interface\UIBackgroundTable.h (RECONSTRUCTION). One background record of the UI
// database, loaded by Create into the static array. Vtable 0x10098ba8 (the dtor only); the original's sizeof was 0x80
// (??_E stride). Requires lle/UILayoutUtility.h, lle/GEColor.h.
//
// Whole in Rust (rust/crates/ee_ui/src/bgtableobj.rs, R6 ui round 20): the members are EeUiBgTableFields, the class's
// private base (the inline getters read it in place; the texture file stays a UString); the array, Create, DeleteAll
// and CreateBackgrounds (UIManager's) are Rust's. The database record is ee_ui background_table.rs DbBackground
// (UIDbDataBackgrounds, 308 bytes).
#ifndef LLE_UIBACKGROUNDTABLE_H
#define LLE_UIBACKGROUNDTABLE_H

#include "rust/ee_ui.h"
#include "lle/UILayoutUtility.h"

class FSFileSpec;
class UIManager;

class LLE_API UIBackgroundTable : private EeUiBgTableFields {
    friend struct UIBackgroundTableHost;    // the Rust table's calls out (UIBackgroundTable.cpp)
public:
    UIBackgroundTable();
    virtual ~UIBackgroundTable() {}         // 0x1005c34a
    static long Create(FSFileSpec& inFile);
    static void CreateBackgrounds(UIManager* inManager);
    static void DeleteAll();
    static unsigned long GetArraySize() { return ee_ui_bgtable_count(); }
    bool GetAttribute1() const { return this->mAttribute1; }
    bool GetAttribute2() const { return this->mAttribute2; }
    bool GetAttribute3() const { return this->mAttribute3; }
    unsigned long GetBackgroundID() const { return this->mBackgroundID; }
    unsigned long GetBackgroundType() const { return this->mBackgroundType; }
    const GEColor& GetColor() const { return *reinterpret_cast<const GEColor*>(&this->mColor); }
    unsigned long GetDrawPriority() const { return this->mDrawPriority; }
    unsigned long GetFormID() const { return this->mFormID; }
    bool GetHasAlpha() const { return this->mHasAlpha; }
    long GetID() const { return this->mID; }
    const UILayoutUtility& GetLayout() const { return *reinterpret_cast<const UILayoutUtility*>(&this->mLayout); }
    long GetNumCols() const { return this->mNumCols; }
    long GetNumRows() const { return this->mNumRows; }
    float GetOpacity() const { return this->mOpacity; }
    const UString& GetTextureFile() const { return this->mTextureFile; }
    const GERectangle& GetTextureRect() const { return *reinterpret_cast<const GERectangle*>(&this->mTextureRect); }
    float GetTileWidthPercent() const { return this->mTileWidthPercent; }
    float GetTileHeightPercent() const { return this->mTileHeightPercent; }
    static UIBackgroundTable* GetUIBackgroundTable(unsigned long inIndex)
        { return static_cast<UIBackgroundTable*>(ee_ui_bgtable_at(inIndex)); }
    bool GetVisible() const { return this->mVisible; }
private:
    UString mTextureFile;
};

// UI database background type (UIBackgroundTable::mBackgroundType): CreateBackgrounds creates the class named here; others go to CreateUserDefinedBackground
enum UIBackgroundType {
    kBackgroundBitmap = 0,        // CreateBackgrounds case: new UIBitmapBackground
    kBackgroundTiled = 1,         // CreateBackgrounds case: new UITiledBackground
    kBackgroundFull = 2,          // CreateBackgrounds case: new UIBFull
    kBackgroundLRTiled = 3,       // CreateBackgrounds case: new UITiledBackground (kTilingLeftRight)
    kBackgroundTBTiled = 4,       // CreateBackgrounds case: new UITiledBackground (kTilingTopBottom)
    kBackgroundStretchedTiled = 5,  // CreateBackgrounds case: new UITiledBackground (kTilingStretched)
    kBackgroundGeneric = 101,     // CreateBackgrounds case: EEUIManager::CreateUserDefinedBackground: new EEUIBGenericBackground
};

#endif
