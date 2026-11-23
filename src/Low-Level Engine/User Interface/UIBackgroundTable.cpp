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
// a background of the entry's type on the form b with the texture description c (lle_hosts.rs bgt_make: the type's
// class; the tiled ones' tiling d and across and down percents f; others the manager's user-defined backgrounds)
#define BGT_ARGS t->GetBackgroundID(), t->GetTextureFile(), *reinterpret_cast<const GETextureDescription*>(c), t->GetTextureRect(), t->GetLayout(), reinterpret_cast<UIForm*>(b), t->GetDrawPriority()
EE_UI_CASE(bgt_make_bitmap) { return (intptr_t) static_cast<UIBackground*>(new UIBitmapBackground(BGT_ARGS, t->GetVisible())); }
// Fixed: the original passed the row ID (GetID) for a tiled background where every other type passes the background ID
EE_UI_CASE(bgt_make_tiled) { return (intptr_t) static_cast<UIBackground*>(new UITiledBackground(BGT_ARGS, t->GetVisible(), (UITiling)d, f[0], f[1])); }
EE_UI_CASE(bgt_make_full) { return (intptr_t) static_cast<UIBackground*>(new UIBFull(BGT_ARGS, t->GetNumCols(), t->GetNumRows(), t->GetVisible())); }
EE_UI_CASE(bgt_make_user) { return (intptr_t)m->CreateUserDefinedBackground(*t); }
#undef BGT_ARGS
#undef m
#undef t

// Creates an empty background table entry (IDs -1, bitmap type, hidden, full opacity, draw priority 1)
void UIBackgroundTable::CreateBackgrounds(UIManager* inManager) { ee_ui_bgtable_create_backgrounds(inManager, ee_ui_bg_table_host); }
