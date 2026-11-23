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

#include "rust/ui/bgtableobj.h"
#include "rust/ui/ids.h"
#include "lle/UILayoutUtility.h"

class FSFileSpec;
class UIManager;

class LLE_API UIBackgroundTable : private EeUiBgTableFields {
    friend struct UIBackgroundTableHost;    // the Rust table's calls out (UIBackgroundTable.cpp)
public:
    UIBackgroundTable();
    virtual ~UIBackgroundTable() {}
    // Loads every background table entry from the file (NULL for nameless records); returns the last read result
    static long Create(FSFileSpec& inFile);
    // Creates every background in the UI background database on its form, by type (UIManager::CreateBackgrounds)
    static void CreateBackgrounds(UIManager* inManager);
    // Deletes every background table entry and empties the table
    static void DeleteAll();
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
    bool GetVisible() const { return this->mVisible; }
private:
    UString mTextureFile;
};

// the enum (UI round 13): rust/ui/ids.h (ee_ui ids.rs)
#endif
