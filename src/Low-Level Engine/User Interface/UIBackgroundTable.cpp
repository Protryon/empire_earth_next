// C:\EEX\Low-Level Engine\User Interface\UIBackgroundTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005c34a..0x1005c84e: UIBackgroundTable's inline members first (0x1005c34a..0x1005c575), then the
// sUIBackgroundArray init chain, the default ctor, Create and DeleteAll. UIBFull.cpp's inline block started at
// 0x1005c84e.
//
// UIBackgroundTable is whole in Rust with UIManager::CreateBackgrounds (rust/crates/ee_ui/src/bgtableobj.rs, R6 ui
// round 20): the members are the record EeUiBgTableFields, the array a Rust list; Create, DeleteAll and
// CreateBackgrounds call out through ee_ui_bg_table_host (the database file, an entry's new and delete, the
// manager's forms, a background's construction by type).
#include <windows.h>
#include <stddef.h>
#include <string.h>

#include "rust/ui/bgtableobj.h"
#include "rust/ui/button.h"
#include "rust/ui/layout.h"
#include "rust/ui/obj.h"
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
#include "lle/UIShims.h"
#include "rust/ui/lle_hosts.h"

static_assert(sizeof(EeUiLayout) == sizeof(UILayoutUtility) && sizeof(EeUiColor) == sizeof(GEColor), "bgtableobj.rs");

// The Rust table's calls out (EeUiOp 590..600): the object is the file (Create) or the manager (CreateBackgrounds)
struct UIBackgroundTableHost {
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
#define t (static_cast<UIBackgroundTable*>(reinterpret_cast<EeUiBgTableFields*>(a)))
#define m static_cast<UIManager*>(inObj)
EE_UI_VOID(bgt_file_open) { ee_uicpp_file_open(inObj); }
EE_UI_CASE(bgt_file_read) { return ee_uicpp_file_read(inObj, reinterpret_cast<unsigned char*>(b), (unsigned long)c); }
EE_UI_VOID(bgt_file_close) { ee_uicpp_file_close(inObj); }
EE_UI_CASE(bgt_new) { return (intptr_t) static_cast<EeUiBgTableFields*>(new UIBackgroundTable(*reinterpret_cast<const EeUiBgTableFields*>(a), reinterpret_cast<const char*>(b))); }
EE_UI_VOID(bgt_delete) { delete t; }
EE_UI_CASE(bgt_form) { return (intptr_t)m->GetFormByID((unsigned long)a); }
EE_UI_CASE(bgt_texture_depth) { return m->GetUITextureBitDepth(); }
EE_UI_CASE(bgt_entry_alpha) { return t->GetHasAlpha(); }
EE_UI_CASE(bgt_entry_form_id) { return t->GetFormID(); }
EE_UI_CASE(bgt_make) { return (intptr_t)UIBackgroundTableHost::Make(m, *t, reinterpret_cast<UIForm*>(b), *reinterpret_cast<const GETextureDescription*>(c)); }
#undef m
#undef t

// Creates an empty background table entry (IDs -1, bitmap type, hidden, full opacity, draw priority 1)
void UIBackgroundTable::CreateBackgrounds(UIManager* inManager) { ee_ui_bgtable_create_backgrounds(inManager, ee_ui_bg_table_host); }
