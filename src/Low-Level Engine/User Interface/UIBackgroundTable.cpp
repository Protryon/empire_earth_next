// C:\EEX\Low-Level Engine\User Interface\UIBackgroundTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005c34a..0x1005c84e: UIBackgroundTable's inline members first (0x1005c34a..0x1005c575), then the
// sUIBackgroundArray init chain, the default ctor, Create and DeleteAll. No U2D key chains (the unit does not see the
// rasterizer header). UIBFull.cpp's inline block (UITextureBackground's members) starts at 0x1005c84e.
//
// Create's loop and the record's copy are Rust (rust/crates/ee_ui/src/background_table.rs, docs/RUST_PLAN.md R6): the
// file is read through ee_uicpp_file_* (UIEvent.cpp), the array and the new entries are reached here.
#include <windows.h>
#include <stddef.h>
#include <string.h>

#include "rust/ee_rust.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/UIBackgroundTable.h"

std::vector<UIBackgroundTable*> UIBackgroundTable::sUIBackgroundArray;

// The Rust record copy's view of an entry (ee_ui/src/background_table.rs BackgroundTableFields)
struct UIBackgroundTableRust {
    static EeUiBackgroundTableRefs Refs(UIBackgroundTable* inTable)
    {
#define EE_UI_BGTABLE_FIELD(member, field)                                                                            \
    static_assert(offsetof(UIBackgroundTable, member) - offsetof(UIBackgroundTable, mTileWidthPercent) ==            \
                      offsetof(EeUiBackgroundTableFields, field),                                                     \
                  "ee_ui/src/background_table.rs")
        EE_UI_BGTABLE_FIELD(mTileWidthPercent, tile_width_percent);
        EE_UI_BGTABLE_FIELD(mTileHeightPercent, tile_height_percent);
        EE_UI_BGTABLE_FIELD(mOpacity, opacity);
        EE_UI_BGTABLE_FIELD(mID, id);
        EE_UI_BGTABLE_FIELD(mBackgroundID, background_id);
        EE_UI_BGTABLE_FIELD(mFormID, form_id);
        EE_UI_BGTABLE_FIELD(mBackgroundType, background_type);
        EE_UI_BGTABLE_FIELD(mNumCols, num_cols);
        EE_UI_BGTABLE_FIELD(mNumRows, num_rows);
        EE_UI_BGTABLE_FIELD(mDrawPriority, draw_priority);
        EE_UI_BGTABLE_FIELD(mVisible, visible);
        EE_UI_BGTABLE_FIELD(mHasAlpha, has_alpha);
        EE_UI_BGTABLE_FIELD(mAttribute1, attribute1);
        EE_UI_BGTABLE_FIELD(mAttribute2, attribute2);
        EE_UI_BGTABLE_FIELD(mAttribute3, attribute3);
#undef EE_UI_BGTABLE_FIELD
        static_assert(sizeof(UIBackgroundTable) - offsetof(UIBackgroundTable, mTileWidthPercent) >=
                          sizeof(EeUiBackgroundTableFields), "ee_ui/src/background_table.rs");
        static_assert(sizeof(UIDbDataBackgrounds) == sizeof(EeUiDbBackground), "ee_ui/src/background_table.rs");
        static_assert(sizeof(GERectangle) == sizeof(EeUiRect) && sizeof(GEColor) == sizeof(EeUiColor), "ee_ui");
        EeUiBackgroundTableRefs theRefs;
        theRefs.fields = reinterpret_cast<EeUiBackgroundTableFields*>(&inTable->mTileWidthPercent);
        theRefs.layout = reinterpret_cast<EeUiLayout*>(&inTable->mLayout);
        theRefs.texture_rect = reinterpret_cast<EeUiRect*>(&inTable->mTextureRect);
        theRefs.color = reinterpret_cast<EeUiColor*>(&inTable->mColor);
        return theRefs;
    }
    static void Resize(unsigned long inCount) { UIBackgroundTable::sUIBackgroundArray.resize(inCount); }
    // a new entry from a named record, or NULL
    static void Set(unsigned long inIndex, const UIDbDataBackgrounds* inData)
    {
        UIBackgroundTable* theTable = NULL;
        if (inData) {
            theTable = new UIBackgroundTable;
            ee_ui_background_table_from_record(reinterpret_cast<const EeUiDbBackground*>(inData), Refs(theTable));
            theTable->mTextureFile.Copy(inData->mTextureFile, 0, strlen(inData->mTextureFile), 0, true);
        }
        UIBackgroundTable::sUIBackgroundArray[inIndex] = theTable;
    }
};

// The host calls of ee_ui/src/background_table_ffi.rs
extern "C" void ee_uicpp_bgtable_resize(unsigned long inCount) { UIBackgroundTableRust::Resize(inCount); }
extern "C" void ee_uicpp_bgtable_set(unsigned long inIndex, const EeUiDbBackground* inData)
{
    UIBackgroundTableRust::Set(inIndex, reinterpret_cast<const UIDbDataBackgrounds*>(inData));
}

// FUNCTION: LLE 0x1005c5be
// Creates an empty background table entry (IDs -1, bitmap type, hidden, full opacity, draw priority 1)
UIBackgroundTable::UIBackgroundTable()
{
    this->mID = -1;
    this->mBackgroundID = -1;
    this->mFormID = -1;
    this->mTileHeightPercent = 1.0f;
    this->mTileWidthPercent = 1.0f;
    this->mBackgroundType = kBackgroundBitmap;
    this->mVisible = false;
    this->mAttribute1 = false;
    this->mOpacity = 1.0f;
    this->mAttribute2 = false;
    this->mAttribute3 = false;
    this->mDrawPriority = 1;
}

// FUNCTION: LLE 0x1005c631
// Loads every background table entry from the file (NULL for nameless records); returns the last read result
// (Rust: a failed read keeps the previous record, the record's copy)
long UIBackgroundTable::Create(FSFileSpec& inFile)
{
    return ee_ui_background_table_create(&inFile);
}

// FUNCTION: LLE 0x1005c80e
// Deletes every background table entry and empties the table
void UIBackgroundTable::DeleteAll()
{
    for (unsigned long i = 0; i < UIBackgroundTable::sUIBackgroundArray.size(); i++)
        delete UIBackgroundTable::sUIBackgroundArray[i];
    UIBackgroundTable::sUIBackgroundArray.resize(0);
}

// Header-inline members first emitted here (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005c34a
// SYNTHETIC: LLE 0x1005c358
// SYNTHETIC: LLE 0x1005c370
// SYNTHETIC: LLE 0x1005c374
// SYNTHETIC: LLE 0x1005c378
// SYNTHETIC: LLE 0x1005c37c
// SYNTHETIC: LLE 0x1005c380
// SYNTHETIC: LLE 0x1005c384
// SYNTHETIC: LLE 0x1005c388
// SYNTHETIC: LLE 0x1005c38c
// SYNTHETIC: LLE 0x1005c390
// SYNTHETIC: LLE 0x1005c394
// SYNTHETIC: LLE 0x1005c398
// SYNTHETIC: LLE 0x1005c39c
// SYNTHETIC: LLE 0x1005c3aa
// SYNTHETIC: LLE 0x1005c3ae
// SYNTHETIC: LLE 0x1005c44b
// SYNTHETIC: LLE 0x1005c4f9

// Compiler-generated: the sUIBackgroundArray init chain (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005c575
// SYNTHETIC: LLE 0x1005c57f
// SYNTHETIC: LLE 0x1005c597
// SYNTHETIC: LLE 0x1005c5a3
