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
#include "UIObjHost.h"

static_assert(sizeof(GETextureDescription) == sizeof(EeUiTextureDescription), "ee_ui bgobj.rs");
static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui bgobj.rs");

// The Rust backgrounds' calls out (EeUiOp 540..562): the object is the record; the wide ops pass twelve floats
struct UIBackgroundHost {
    static UIBackground* Background(EeUiBackgroundFields* r) { return static_cast<UIBackground*>(r); }
    static intptr_t Op(void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
    {
        EeUiBackgroundFields* r = static_cast<EeUiBackgroundFields*>(inObj);
        UIForm* theForm = static_cast<UIForm*>(r->mForm);
        UIManager* m = theForm->GetManager();
        unsigned long s = m->GetScreen();
        GEBitmap* theBitmap = reinterpret_cast<GEBitmap*>(a);
        GETexture* theTexture = reinterpret_cast<GETexture*>(a);
        switch (inOp) {
        case EeUiOp::BgAdd: theForm->AddBackground(Background(r)); break;
        case EeUiOp::BgWindow:
            return a ? m->GetRasterizer()->GetWindow(s)->GetHeight() : m->GetRasterizer()->GetWindow(s)->GetWidth();
        case EeUiOp::BgExclusive: return theForm->GetUseExclusiveCoords();
        case EeUiOp::BgFormCurrent: return theForm == m->GetCurrentForm();
        case EeUiOp::BgLock: a ? theForm->LockDrawBackground() : theForm->UnlockDrawBackground(); break;
        case EeUiOp::BgRedraw: theForm->RedrawIntersectControls(UIObjHost::Rect(f)); break;
        case EeUiOp::BgDirty: m->AddDirtyRectangle(UIObjHost::Rect(f), theForm); break;
        case EeUiOp::BgVirtLoad: return Background(r)->LoadBackground();
        case EeUiOp::BgVirtUnload: return Background(r)->UnloadBackground();
        case EeUiOp::BgVirtStretched:
            return static_cast<UIBitmapBackground*>(Background(r))
                ->DrawStrectchedBackground(UIObjHost::Rect(f), UIObjHost::Rect(f + 4));
        case EeUiOp::BgVirtTiled:
            return static_cast<UITiledBackground*>(Background(r))
                ->DrawTiledBackground(UIObjHost::Rect(f), a, b, c, d, f[4], f[5]);
        case EeUiOp::BgBitmapNew: return (intptr_t) new GEBitmap;
        case EeUiOp::BgBitmapRelease: theBitmap->Dereference(); break;
        case EeUiOp::BgBitmapTexture: return (intptr_t)theBitmap->GetTexture(s);
        case EeUiOp::BgBitmapSetTexture: theBitmap->SetTexture(reinterpret_cast<GETexture*>(b), s); break;
        case EeUiOp::BgTextureSize:
            f[0] = (float)theTexture->GetHeight();
            return theTexture->GetWidth();
        case EeUiOp::BgTextureLoad:
            theTexture = m->GetRasterizer()->GetTextureManager(s)->LoadTexture(
                UString(reinterpret_cast<const char*>(a)), *reinterpret_cast<const GETextureDescription*>(b));
            theTexture->SetStageAlphaMode(kStageOpModulate, -1);
            theTexture->SetStageColorMode(kStageOpModulate, -1);
            theTexture->SetStageMinFilterMode(1, -1);
            theTexture->SetStageMagFilterMode(1, -1);
            theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
            theTexture->SetStageMipMapBias(-0.75f, -1);
            theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
            return (intptr_t)theTexture;
        case EeUiOp::BgFormRect: UIObjHost::PutRect(f, theForm->GetRectangle()); break;
        case EeUiOp::BgFormFade: f[0] = theForm->GetFadeOpacity(); break;
        default: break;
        }
        return 0;
    }
    static intptr_t CallRect(EeUiBackgroundFields* r, EeUiBgCall inCall, const GERectangle& inRectangle)
    {
        float f[4];
        UIObjHost::PutRect(f, inRectangle);
        return ee_ui_bg_call(r, inCall, 0, 0, f);
    }
};
typedef EeUiBgCall C;

// Creates a background with its layout and draw priority and adds it to the form
UIBackground::UIBackground(unsigned long inBackgroundID, const UILayoutUtility& inLayout, UIForm* inForm,
                           unsigned long inDrawPriority, bool inVisible)
{
    ee_ui_bg_init(this, static_cast<EeUiBackgroundFields*>(this), UIBackgroundHost::Op, inBackgroundID,
                  reinterpret_cast<const EeUiLayout*>(&inLayout), inForm, inDrawPriority, inVisible);
    ee_ui_direct_background(this, this);   // the direct path's registry (ee_ui direct.rs): this background's record
}

UIBackground::~UIBackground() { ee_ui_bg_free(this); }
intptr_t UIBackground::CallRect(C inCall, const GERectangle& inRectangle) { return UIBackgroundHost::CallRect(this, inCall, inRectangle); }
void UIBackground::Close() { this->Call(C::Close); }
void UIBackground::Open() { this->Call(C::Open); }
void UIBackground::RelayoutBackground() { this->Call(C::Relayout); }
void UIBackground::SetRectangle(const GERectangle& inRectangle) { this->CallRect(C::SetRectangle, inRectangle); }
void UIBackground::SetVisibility(bool inVisible) { this->Call(C::SetVisibility, inVisible); }

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
long UIBitmapBackground::DrawBackground(const GERectangle& inRectangle) { return (long)this->CallRect(C::BitmapDraw, inRectangle); }
long UIBitmapBackground::DrawStrectchedBackground(const GERectangle& inSource, const GERectangle& inDestination)
{
    float f[12] = {0};
    UIObjHost::PutRect(f, inSource);
    UIObjHost::PutRect(f + 4, inDestination);
    return (long)this->Call(C::BitmapStretched, 0, 0, f);
}
long UIBitmapBackground::LoadBackground() { return (long)this->Call(C::BitmapLoad); }
void UIBitmapBackground::SetBitmapTexture(GETexture* inTexture) { this->Call(C::BitmapSetTexture, (intptr_t)inTexture); }
long UIBitmapBackground::UnloadBackground() { return (long)this->Call(C::BitmapUnload); }

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

long UITiledBackground::DrawBackground(const GERectangle& inRectangle) { return (long)this->CallRect(C::TiledDraw, inRectangle); }
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
long UIBFull::DrawBackground(const GERectangle& inRectangle) { return (long)this->CallRect(C::BFullDraw, inRectangle); }
long UIBFull::LoadBackground() { return (long)this->Call(C::BFullLoad); }
void UIBFull::SetBitmapTexture(GETexture* inTexture, unsigned long inColumn, unsigned long inRow)
{
    uint32_t theRow = (uint32_t)inRow;
    float f[4] = {0};
    memcpy(f, &theRow, 4);
    this->Call(C::BFullSetBitmapTexture, (intptr_t)inTexture, inColumn, f);
}
void UIBFull::SetNumberColumnsAndRows(unsigned long inColumns, unsigned long inRows) { this->Call(C::BFullSetGrid, inColumns, inRows); }
long UIBFull::UnloadBackground() { return (long)this->Call(C::BFullUnload); }

UIFullBackground::UIFullBackground(const UString& inFileName, unsigned long inColumns, unsigned long inRows,
                                   UIForm* inForm, unsigned long inDesignedWidth, unsigned long inDesignedHeight)
{
    ee_ui_bg_full_init(this, static_cast<EeUiBackgroundFields*>(this), UIBackgroundHost::Op, inFileName, inColumns,
                       inRows, inForm, inDesignedWidth, inDesignedHeight);
}

UIFullBackground::~UIFullBackground()
{
    ee_ui_bg_call(this, C::FullRelease, 0, 0, NULL);
    ee_ui_bg_free(this);
}
long UIFullBackground::DrawBackground(const GERectangle& inRectangle) { return (long)UIBackgroundHost::CallRect(this, C::FullDraw, inRectangle); }
long UIFullBackground::LoadBackground(const GETextureDescription& inDescription)
{
    return (long)ee_ui_bg_call(this, C::FullLoad, (intptr_t)&inDescription, 0, NULL);
}
long UIFullBackground::UnloadBackground() { return (long)ee_ui_bg_call(this, C::FullUnload, 0, 0, NULL); }
