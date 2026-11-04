// C:\EEX\Low-Level Engine\User Interface\UIBackgroundTable.h (RECONSTRUCTION). One background record of the UI
// database, loaded by Create into the static sUIBackgroundArray. UIBackgroundTable.cpp (0x1005c34a..0x1005c84e) emits
// the inline members first (the dtor, the getters, the implicit copy members, ??_E), then the sUIBackgroundArray
// init chain, the default ctor, Create and DeleteAll. Vtable 0x10098ba8 (the dtor only). sizeof 0x80 (??_E stride).
// Requires lle/UILayoutUtility.h, lle/GEColor.h, <vector>.
#ifndef LLE_UIBACKGROUNDTABLE_H
#define LLE_UIBACKGROUNDTABLE_H

#include <vector>

#include "lle/UILayoutUtility.h"

class FSFileSpec;

// The database record Create reads (0x134 bytes per background). Provisional name (after UIDbDataForms, a proven
// export parameter type) and field names; offsets proven by Create 0x1005c631.
struct UIDbDataBackgrounds {
    char mName[0x64];                       // +0x000 an empty name leaves the slot NULL
    char mTextureFile[0x64];                // +0x064
    float mTopOffset;                       // +0x0c8
    float mBottomOffset;                    // +0x0cc
    float mLeftOffset;                      // +0x0d0
    float mRightOffset;                     // +0x0d4
    float mTileWidthPercent;                // +0x0d8
    float mTileHeightPercent;               // +0x0dc
    float mOpacity;                         // +0x0e0
    long mID;                               // +0x0e4
    unsigned long mBackgroundID;            // +0x0e8
    unsigned long mFormID;                  // +0x0ec
    long mTextureTop;                       // +0x0f0
    long mTextureLeft;                      // +0x0f4
    long mTextureBottom;                    // +0x0f8
    long mTextureRight;                     // +0x0fc
    unsigned long mBackgroundType;          // +0x100
    unsigned long mTopOffsetType;           // +0x104
    unsigned long mBottomOffsetType;        // +0x108
    unsigned long mLeftOffsetType;          // +0x10c
    unsigned long mRightOffsetType;         // +0x110
    long mNumCols;                          // +0x114
    long mNumRows;                          // +0x118
    long mDrawPriority;                     // +0x11c at least 1
    long mRed;                              // +0x120 0..255
    long mGreen;                            // +0x124
    long mBlue;                             // +0x128
    bool mVisible;                          // +0x12c
    bool mHasAlpha;                         // +0x12d
    bool mAttribute1;                       // +0x12e
    bool mAttribute2;                       // +0x12f
    bool mAttribute3;                       // +0x130
};
static_assert(sizeof(UIDbDataBackgrounds) == 308, "on-disk record: 308 bytes on every target");

class LLE_API UIBackgroundTable {
public:
    UIBackgroundTable();
    virtual ~UIBackgroundTable() {}         // 0x1005c34a
    static long Create(FSFileSpec& inFile);
    static void DeleteAll();
    static unsigned long GetArraySize() { return UIBackgroundTable::sUIBackgroundArray.size(); }
    bool GetAttribute1() const { return this->mAttribute1; }
    bool GetAttribute2() const { return this->mAttribute2; }
    bool GetAttribute3() const { return this->mAttribute3; }
    unsigned long GetBackgroundID() const { return this->mBackgroundID; }
    unsigned long GetBackgroundType() const { return this->mBackgroundType; }
    const GEColor& GetColor() const { return this->mColor; }
    unsigned long GetDrawPriority() const { return this->mDrawPriority; }
    unsigned long GetFormID() const { return this->mFormID; }
    bool GetHasAlpha() const { return this->mHasAlpha; }
    long GetID() const { return this->mID; }
    const UILayoutUtility& GetLayout() const { return this->mLayout; }
    long GetNumCols() const { return this->mNumCols; }
    long GetNumRows() const { return this->mNumRows; }
    float GetOpacity() const { return this->mOpacity; }
    const UString& GetTextureFile() const { return this->mTextureFile; }
    const GERectangle& GetTextureRect() const { return this->mTextureRect; }
    float GetTileWidthPercent() const { return this->mTileWidthPercent; }
    float GetTileHeightPercent() const { return this->mTileHeightPercent; }
    static UIBackgroundTable* GetUIBackgroundTable(unsigned long inIndex) { return UIBackgroundTable::sUIBackgroundArray[inIndex]; }
    bool GetVisible() const { return this->mVisible; }
private:
    UILayoutUtility mLayout;                // +0x04
    GERectangle mTextureRect;               // +0x24
    GEColor mColor;                         // +0x34
    UString mTextureFile;                   // +0x40
    float mTileWidthPercent;                // +0x50
    float mTileHeightPercent;               // +0x54
    float mOpacity;                         // +0x58
    long mID;                               // +0x5c
    unsigned long mBackgroundID;            // +0x60
    unsigned long mFormID;                  // +0x64
    unsigned long mBackgroundType;          // +0x68
    long mNumCols;                          // +0x6c
    long mNumRows;                          // +0x70
    unsigned long mDrawPriority;            // +0x74
    bool mVisible;                          // +0x78
    bool mHasAlpha;                         // +0x79
    bool mAttribute1;                       // +0x7a
    bool mAttribute2;                       // +0x7b
    bool mAttribute3;                       // +0x7c
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS)   // the exe view compiles the inline getters
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number)
    static std::vector<UIBackgroundTable*> sUIBackgroundArray;   // 0x1012dfc0
#endif
};

// UI database background type (UIBackgroundTable::mBackgroundType): UIManager::CreateBackgrounds creates the class named here; others go to CreateUserDefinedBackground
enum UIBackgroundType {
    kBackgroundBitmap = 0,        // UIManager::CreateBackgrounds case: new UIBitmapBackground
    kBackgroundTiled = 1,         // UIManager::CreateBackgrounds case: new UITiledBackground
    kBackgroundFull = 2,          // UIManager::CreateBackgrounds case: new UIBFull
    kBackgroundLRTiled = 3,       // UIManager::CreateBackgrounds case: new UILRTiledBackground
    kBackgroundTBTiled = 4,       // UIManager::CreateBackgrounds case: new UITBTiledBackground
    kBackgroundStretchedTiled = 5,  // UIManager::CreateBackgrounds case: new UIStretchedTiledBackground
    kBackgroundGeneric = 101,     // UIManager::CreateBackgrounds case: EEUIManager::CreateUserDefinedBackground: new EEUIBGenericBackground
};

#endif
