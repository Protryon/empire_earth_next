// C:\EEX\Low-Level Engine\User Interface\UIBackgroundTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005c34a..0x1005c84e: UIBackgroundTable's inline members first (0x1005c34a..0x1005c575), then the
// sUIBackgroundArray init chain, the default ctor, Create and DeleteAll. UIBFull.cpp's inline block started at
// 0x1005c84e.
//
// UIBackgroundTable is whole in Rust with UIManager::CreateBackgrounds (rust/crates/ee_ui/src/bgtableobj.rs, R6 ui
// round 20): the members are the record EeUiBgTableFields, the array a Rust list; Create, DeleteAll and
// CreateBackgrounds call out through UIBackgroundTableHost::Op (the database file, an entry's new and delete, the
// manager's forms, a background's construction by type).
#include <windows.h>
#include <stddef.h>
#include <string.h>

#include "rust/ee_ui.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GERhombus.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIBackgroundTable.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITiledBackground.h"
#include "lle/UIBFull.h"

static_assert(sizeof(EeUiLayout) == sizeof(UILayoutUtility) && sizeof(EeUiColor) == sizeof(GEColor), "bgtableobj.rs");

// the database file's calls (UIEvent.cpp)
extern "C" void ee_uicpp_file_open(void* inFile);
extern "C" long ee_uicpp_file_read_count(void* inFile, unsigned long* outCount);
extern "C" long ee_uicpp_file_read(void* inFile, unsigned char* outData, unsigned long inLength);
extern "C" void ee_uicpp_file_close(void* inFile);

// The Rust table's calls out (EeUiOp 590..600): the object is the file (Create) or the manager (CreateBackgrounds)
struct UIBackgroundTableHost {
    static intptr_t Op(void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t, float* f)
    {
        UIBackgroundTable* t = static_cast<UIBackgroundTable*>(reinterpret_cast<EeUiBgTableFields*>(a));
        UIManager* m = static_cast<UIManager*>(inObj);
        switch (inOp) {
        case EeUiOp::BgtFileOpen: ee_uicpp_file_open(inObj); break;
        case EeUiOp::BgtFileCount: {
            unsigned long theCount = 0;
            long theResult = ee_uicpp_file_read_count(inObj, &theCount);
            uint32_t theBits = (uint32_t)theCount;
            memcpy(f, &theBits, 4);
            return theResult;
        }
        case EeUiOp::BgtFileRead: return ee_uicpp_file_read(inObj, reinterpret_cast<unsigned char*>(b), (unsigned long)c);
        case EeUiOp::BgtFileClose: ee_uicpp_file_close(inObj); break;
        case EeUiOp::BgtNew:
            t = new UIBackgroundTable;
            *static_cast<EeUiBgTableFields*>(t) = *reinterpret_cast<const EeUiBgTableFields*>(a);
            t->mTextureFile = reinterpret_cast<const char*>(b);
            return (intptr_t) static_cast<EeUiBgTableFields*>(t);
        case EeUiOp::BgtDelete: delete t; break;
        case EeUiOp::BgtForm: return (intptr_t)m->GetFormByID((unsigned long)a);
        case EeUiOp::BgtTextureDepth: return m->GetUITextureBitDepth();
        case EeUiOp::BgtEntryAlpha: return t->GetHasAlpha();
        case EeUiOp::BgtEntryFormId: return t->GetFormID();
        case EeUiOp::BgtMake: return (intptr_t)Make(m, *t, reinterpret_cast<UIForm*>(b), *reinterpret_cast<const GETextureDescription*>(c));
        default: break;
        }
        return 0;
    }
    // A background of the entry's type on the form (others the manager's user-defined backgrounds)
    static UIBackground* Make(UIManager* m, const UIBackgroundTable& t, UIForm* inForm, const GETextureDescription& d)
    {
        const UString& n = t.GetTextureFile();
        const GERectangle& r = t.GetTextureRect();
        unsigned long id = t.GetBackgroundID(), p = t.GetDrawPriority();
        switch (t.GetBackgroundType()) {
        case kBackgroundBitmap: return new UIBitmapBackground(id, n, d, r, t.GetLayout(), inForm, p, t.GetVisible());
        // Fixed: the original passed the row ID (GetID) where every other case passes the background ID
        case kBackgroundTiled: return new UITiledBackground(id, n, d, r, t.GetLayout(), inForm, p, t.GetVisible());
        case kBackgroundFull:
            return new UIBFull(id, n, d, r, t.GetLayout(), inForm, p, t.GetNumCols(), t.GetNumRows(), t.GetVisible());
        case kBackgroundLRTiled:
            return new UITiledBackground(id, n, d, r, t.GetLayout(), inForm, p, t.GetVisible(), kTilingLeftRight);
        case kBackgroundTBTiled:
            return new UITiledBackground(id, n, d, r, t.GetLayout(), inForm, p, t.GetVisible(), kTilingTopBottom);
        case kBackgroundStretchedTiled:
            return new UITiledBackground(id, n, d, r, t.GetLayout(), inForm, p, t.GetVisible(), kTilingStretched,
                                         t.GetTileWidthPercent(), t.GetTileHeightPercent());
        default: return m->CreateUserDefinedBackground(t);
        }
    }
};

// FUNCTION: LLE 0x1005c5be
// Creates an empty background table entry (IDs -1, bitmap type, hidden, full opacity, draw priority 1)
UIBackgroundTable::UIBackgroundTable() { ee_ui_bgtable_init(this); }
// FUNCTION: LLE 0x1005c631
// Loads every background table entry from the file (NULL for nameless records); returns the last read result
long UIBackgroundTable::Create(FSFileSpec& inFile) { return ee_ui_bgtable_create(&inFile, UIBackgroundTableHost::Op); }
// FUNCTION: LLE 0x1005c80e
// Deletes every background table entry and empties the table
void UIBackgroundTable::DeleteAll() { ee_ui_bgtable_delete_all(UIBackgroundTableHost::Op); }
// FUNCTION: LLE 0x1007a216
// Creates every background in the UI background database on its form, by type (UIManager::CreateBackgrounds)
void UIBackgroundTable::CreateBackgrounds(UIManager* inManager)
{
    ee_ui_bgtable_create_backgrounds(inManager, UIBackgroundTableHost::Op);
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
