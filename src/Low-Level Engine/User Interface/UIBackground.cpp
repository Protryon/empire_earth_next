// C:\EEX\Low-Level Engine\User Interface\UIBackground.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10058d38..0x1005c34a: the first User Interface unit. It opens with the header-inline members of every UI class it
// is the first to include (UILayoutUtility, TMPoll's implicit operator=, UIBackground, UIEntity, UIEntityGroup,
// UIControl, UIFormManager, UIForm incl. its implicit copy members, UIEvent), the U2D key init chains (0x10059ebb),
// UIManager's inline and implicit copy members, then its own code (0x1005a76e) and the vector/deque/map COMDATs
// (0x1005aa1b..). UIBackgroundTable.cpp starts at 0x1005c34a.
//
// The backgrounds are whole in Rust (rust/crates/ee_ui/src/bgobj.rs, R6 ui round 20): this unit holds every
// background class's forwarders (UIBackground, UIBitmapBackground, UITiledBackground, UIBFull, UIFullBackground; the
// units UIBitmapBackground.cpp, UITiledBackground.cpp, UILRTiledBackground.cpp, UITBTiledBackground.cpp,
// UIStretchedTiledBackground.cpp, UIBFull.cpp and UIFullBackground.cpp are gone) and the Rust object's calls out
// (UIBackgroundHost::Op: the form's questions, locks and redraws, the virtuals, the bitmaps and textures).
#define LLE_UIBACKGROUND_CPP   // UIManager::sMouseWheel/sMouseLeftHanded are visible here (UIManager.h) (provisional name)
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureManager.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UITiledBackground.h"

static_assert(sizeof(GETextureDescription) == sizeof(EeUiTextureDescription), "ee_ui bgobj.rs");
static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui bgobj.rs");

// eight floats for the wide calls (two rectangles, or a rectangle and four floats; ee_ui bgobj.rs)
struct UIF8 {
    UIF8(const GERectangle& r, const GERectangle& s) : f{r.mBottom, r.mLeft, r.mRight, r.mTop, s.mBottom, s.mLeft, s.mRight, s.mTop} {}
    UIF8(const GERectangle& r, float a, float b, float c, float d) : f{r.mBottom, r.mLeft, r.mRight, r.mTop, a, b, c, d} {}
    operator float*() { return this->f; }
    float f[8];
};
#define r static_cast<EeUiBackgroundFields*>(inObj)
#define theForm (static_cast<UIForm*>(r->mForm))
#define m (theForm->GetManager())
#define s (m->GetScreen())
#define theBitmap reinterpret_cast<GEBitmap*>(a)
EE_UI_VOID(bg_add) { theForm->AddBackground(static_cast<UIBackground*>(r)); }
EE_UI_CASE(bg_window) { return a ? m->GetRasterizer()->GetWindow(s)->GetHeight() : m->GetRasterizer()->GetWindow(s)->GetWidth(); }
EE_UI_CASE(bg_exclusive) { return theForm->GetUseExclusiveCoords(); }
EE_UI_CASE(bg_form_current) { return theForm == m->GetCurrentForm(); }
EE_UI_VOID(bg_lock) { a ? theForm->LockDrawBackground() : theForm->UnlockDrawBackground(); }
EE_UI_VOID(bg_redraw) { theForm->RedrawIntersectControls(UIObjHost::Rect(f)); }
EE_UI_VOID(bg_dirty) { m->AddDirtyRectangle(UIObjHost::Rect(f), theForm); }
EE_UI_CASE(bg_virt_load) { return static_cast<UIBackground*>(r)->LoadBackground(); }
EE_UI_CASE(bg_virt_unload) { return static_cast<UIBackground*>(r)->UnloadBackground(); }
EE_UI_CASE(bg_virt_stretched) { return static_cast<UIBitmapBackground*>(static_cast<UIBackground*>(r)) ->DrawStrectchedBackground(UIObjHost::Rect(f), UIObjHost::Rect(f + 4)); }
EE_UI_CASE(bg_virt_tiled) { return static_cast<UITiledBackground*>(static_cast<UIBackground*>(r)) ->DrawTiledBackground(UIObjHost::Rect(f), a, b, c, d, f[4], f[5]); }
EE_UI_CASE(bg_bitmap_new) { return (intptr_t) new GEBitmap; }
EE_UI_VOID(bg_bitmap_release) { theBitmap->Dereference(); }
EE_UI_CASE(bg_bitmap_texture) { return (intptr_t)theBitmap->GetTexture(s); }
EE_UI_VOID(bg_bitmap_set_texture) { theBitmap->SetTexture(reinterpret_cast<GETexture*>(b), s); }
EE_UI_VOID(bg_form_rect) { UIObjHost::PutRect(f, theForm->GetRectangle()); }
EE_UI_VOID(bg_form_fade) { f[0] = theForm->GetFadeOpacity(); }
#undef theBitmap
#undef s
#undef m
#undef theForm
#undef r
typedef EeUiBgCall C;

// the texture's size and load: Rust's (ee_ui lle_bg_edit.rs, UI lane round 12) before the host above
#define UIBGM(r) static_cast<UIForm*>(static_cast<EeUiBackgroundFields*>((void*)(r))->mForm)->GetManager()
extern "C" uintptr_t ee_uicpp_bg_texture_load(uintptr_t inRecord, EeUiShimText inName, uintptr_t inDescription)
{ return (uintptr_t)UIBGM(inRecord)->GetRasterizer()->GetTextureManager(UIBGM(inRecord)->GetScreen())->LoadTexture(UIShimString(inName), *reinterpret_cast<const GETextureDescription*>(inDescription)); }


long UIBitmapBackground::DrawStrectchedBackground(const GERectangle& inSource, const GERectangle& inDestination)
{
    return (long)this->Call(C::BitmapStretched, 0, 0, UIF8(inSource, inDestination));
}

long UITiledBackground::DrawTiledBackground(const GERectangle& inClip, unsigned long inFirstRow, unsigned long inLastRow,
                                            unsigned long inFirstColumn, unsigned long inLastColumn, float inWidth,
                                            float inHeight)
{
    return (long)this->Call(C::DrawTiled, inFirstRow, inLastRow,
                            UIF8(inClip, UIF4::Bits((int32_t)inFirstColumn), UIF4::Bits((int32_t)inLastColumn), inWidth, inHeight));
}


