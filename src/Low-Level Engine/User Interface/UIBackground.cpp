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

#include "rust/ui/bgobj.h"
#include "rust/ui/direct.h"
#include "rust/ui/layout.h"
#include "rust/ui/obj.h"
#include "rust/ui/rect.h"
#include "lle/LowLevelEngine.h"
#include "lle/UIShims.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIBackground.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIEvent.h"
#include "lle/UIManager.h"
#include "lle/UITiledBackground.h"
#include "lle/UIBFull.h"
#include "lle/UIFullBackground.h"
#include "lle/UIObjHost.h"
#include "rust/ui/lle_hosts.h"

static_assert(sizeof(GETextureDescription) == sizeof(EeUiTextureDescription), "ee_ui bgobj.rs");
static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui bgobj.rs");

// The Rust backgrounds' calls out (EeUiOp 540..562): the object is the record; the wide ops pass twelve floats
struct UIBackgroundHost {
    static UIBackground* Background(EeUiBackgroundFields* r) { return static_cast<UIBackground*>(r); }
    static intptr_t CallRect(EeUiBackgroundFields* r, EeUiBgCall inCall, const GERectangle& inRectangle)
    {
        float f[4];
        UIObjHost::PutRect(f, inRectangle);
        return ee_ui_bg_call(r, inCall, 0, 0, f);
    }
};
#define r static_cast<EeUiBackgroundFields*>(inObj)
#define theForm (static_cast<UIForm*>(r->mForm))
#define m (theForm->GetManager())
#define s (m->GetScreen())
#define theBitmap reinterpret_cast<GEBitmap*>(a)
EE_UI_VOID(bg_add) { theForm->AddBackground(UIBackgroundHost::Background(r)); }
EE_UI_CASE(bg_window) { return a ? m->GetRasterizer()->GetWindow(s)->GetHeight() : m->GetRasterizer()->GetWindow(s)->GetWidth(); }
EE_UI_CASE(bg_exclusive) { return theForm->GetUseExclusiveCoords(); }
EE_UI_CASE(bg_form_current) { return theForm == m->GetCurrentForm(); }
EE_UI_VOID(bg_lock) { a ? theForm->LockDrawBackground() : theForm->UnlockDrawBackground(); }
EE_UI_VOID(bg_redraw) { theForm->RedrawIntersectControls(UIObjHost::Rect(f)); }
EE_UI_VOID(bg_dirty) { m->AddDirtyRectangle(UIObjHost::Rect(f), theForm); }
EE_UI_CASE(bg_virt_load) { return UIBackgroundHost::Background(r)->LoadBackground(); }
EE_UI_CASE(bg_virt_unload) { return UIBackgroundHost::Background(r)->UnloadBackground(); }
EE_UI_CASE(bg_virt_stretched) { return static_cast<UIBitmapBackground*>(UIBackgroundHost::Background(r)) ->DrawStrectchedBackground(UIObjHost::Rect(f), UIObjHost::Rect(f + 4)); }
EE_UI_CASE(bg_virt_tiled) { return static_cast<UITiledBackground*>(UIBackgroundHost::Background(r)) ->DrawTiledBackground(UIObjHost::Rect(f), a, b, c, d, f[4], f[5]); }
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

// Creates a background with its layout and draw priority and adds it to the form
UIBackground::UIBackground(unsigned long inBackgroundID, const UILayoutUtility& inLayout, UIForm* inForm,
                           unsigned long inDrawPriority, bool inVisible)
{
    ee_ui_bg_init(this, static_cast<EeUiBackgroundFields*>(this), ee_ui_background_host, inBackgroundID,
                  reinterpret_cast<const EeUiLayout*>(&inLayout), inForm, inDrawPriority, inVisible);
    ee_ui_direct_background(this, this);   // the direct path's registry (ee_ui direct.rs): this background's record
}

UIBackground::~UIBackground() { ee_ui_bg_free(this); }
intptr_t UIBackground::CallRect(C inCall, const GERectangle& inRectangle) { return UIBackgroundHost::CallRect(this, inCall, inRectangle); }

UIBitmapBackground::UIBitmapBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                                       const GETextureDescription& inDescription,
                                       const GERectangle& inTextureRectangle, const UILayoutUtility& inLayout,
                                       UIForm* inForm, unsigned long inDrawPriority, bool inVisible)
    : UITextureBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                          inDrawPriority, inVisible)
{
    this->Call(C::BitmapInit);
}

UIBitmapBackground::~UIBitmapBackground() { this->Call(C::BitmapRelease); }
long UIBitmapBackground::DrawStrectchedBackground(const GERectangle& inSource, const GERectangle& inDestination)
{
    float f[12] = {0};
    UIObjHost::PutRect(f, inSource);
    UIObjHost::PutRect(f + 4, inDestination);
    return (long)this->Call(C::BitmapStretched, 0, 0, f);
}

UITiledBackground::UITiledBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                                     const GETextureDescription& inDescription, const GERectangle& inTextureRectangle,
                                     const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority,
                                     bool inVisible, UITiling inTiling, float inAcross, float inDown)
    : UIBitmapBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                         inDrawPriority, inVisible)
{
    this->mTiling = inTiling;
    this->mHorizontalCount = inAcross;
    this->mVerticalCount = inDown;
}

long UITiledBackground::DrawTiledBackground(const GERectangle& inClip, unsigned long inFirstRow, unsigned long inLastRow,
                                            unsigned long inFirstColumn, unsigned long inLastColumn, float inWidth,
                                            float inHeight)
{
    float f[12] = {0};
    UIObjHost::PutRect(f, inClip);
    uint32_t theColumns[2] = {(uint32_t)inFirstColumn, (uint32_t)inLastColumn};
    memcpy(f + 4, theColumns, sizeof theColumns);
    f[6] = inWidth;
    f[7] = inHeight;
    return (long)this->Call(C::DrawTiled, inFirstRow, inLastRow, f);
}

UIBFull::UIBFull(unsigned long inBackgroundID, const UString& inTextureFileName,
                 const GETextureDescription& inDescription, const GERectangle& inTextureRectangle,
                 const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority, unsigned long inColumns,
                 unsigned long inRows, bool inVisible)
    : UITextureBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                          inDrawPriority, inVisible)
{
    this->Call(C::BFullInit, inColumns, inRows);
}

UIBFull::~UIBFull() { this->Call(C::BFullRelease); }
void UIBFull::SetBitmapTexture(GETexture* inTexture, unsigned long inColumn, unsigned long inRow)
{
    uint32_t theRow = (uint32_t)inRow;
    float f[4] = {0};
    memcpy(f, &theRow, 4);
    this->Call(C::BFullSetBitmapTexture, (intptr_t)inTexture, inColumn, f);
}

UIFullBackground::UIFullBackground(const UString& inFileName, unsigned long inColumns, unsigned long inRows,
                                   UIForm* inForm, unsigned long inDesignedWidth, unsigned long inDesignedHeight)
{
    ee_ui_bg_full_init(this, static_cast<EeUiBackgroundFields*>(this), ee_ui_background_full_host, inFileName, inColumns,
                       inRows, inForm, inDesignedWidth, inDesignedHeight);
}

UIFullBackground::~UIFullBackground()
{
    ee_ui_bg_call(this, C::FullRelease, 0, 0, NULL);
    ee_ui_bg_free(this);
}

long UIFullBackground::DrawBackground(const GERectangle& inRectangle) { return (long)UIBackgroundHost::CallRect(this, EeUiBgCall::FullDraw, inRectangle); }
