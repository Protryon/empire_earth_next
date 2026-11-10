// C:\EEX\Low-Level Engine\User Interface\UIBitmapBackground.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005d72a..0x1005db0f: UIBitmapBackground's inline members (GetBitmap, the implicit copy ctor, ??_E; operator= is
// ICF-folded at 0x10078a91), the U2D key init chains, the ctor and the out-of-line members. UIBlinkingButton.cpp's
// inline block starts at 0x1005db0f.
#include <windows.h>

#include "rust/ee_rust.h"
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
#include "lle/TSCriticalSection.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIBitmapBackground.h"

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top); guarded: a golden driver includes several
// background units in one translation unit
#ifndef UI_BACKGROUND_RECT_HELPERS
#define UI_BACKGROUND_RECT_HELPERS
static inline EeUiRect UIBgRect(const GERectangle& r) { return EeUiRect{r.mBottom, r.mLeft, r.mRight, r.mTop}; }
static inline GERectangle UIBgRect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }
#endif

// A tile of the Rust backgrounds' plans, drawn by the background's (virtual) DrawStrectchedBackground
extern "C" long ee_uicpp_background_draw(void* inBackground, const EeUiRect* inSource, const EeUiRect* inDestination)
{
    return static_cast<UIBitmapBackground*>(inBackground)->DrawStrectchedBackground(UIBgRect(*inSource), UIBgRect(*inDestination));
}

// FUNCTION: LLE 0x1005d8ca
// Creates a background that stretches one texture over its rectangle
UIBitmapBackground::UIBitmapBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                                       const GETextureDescription& inDescription,
                                       const GERectangle& inTextureRectangle, const UILayoutUtility& inLayout,
                                       UIForm* inForm, unsigned long inDrawPriority, bool inVisible)
    : UITextureBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                          inDrawPriority, inVisible)
{
    this->mBitmap = new GEBitmap;
}

// FUNCTION: LLE 0x1005d914
// Destroys the background, releasing its bitmap
UIBitmapBackground::~UIBitmapBackground()
{
    this->mBitmap->Dereference();
}

// FUNCTION: LLE 0x1005d93c
// Draws the part of the background that overlaps the given rectangle
long UIBitmapBackground::DrawBackground(const GERectangle& inRectangle)
{
    return ee_ui_background_bitmap_draw(this, UIBgRect(this->mRectangle), UIBgRect(inRectangle));
}

// FUNCTION: LLE 0x1005d988
// Draws the texture stretched from the source rectangle and clipped to the destination rectangle
long UIBitmapBackground::DrawStrectchedBackground(const GERectangle& inSource, const GERectangle& inDestination)
{
    this->mBitmap->ComputeStretchedCoordsAndScale(inSource, inDestination, this->mTextureRectangle,
                                            this->mForm->GetManager()->GetScreen());
    this->mBitmap->SetLocation(inDestination.GetTopLeft());
    return this->mForm->GetManager()->GetRasterizer()->Draw(this->mBitmap, this->mForm->GetManager()->GetScreen());
}

// FUNCTION: LLE 0x1005d9f3
// Loads the background texture (clamped, mip-mapped) when a file name is set; always kErrorOK
long UIBitmapBackground::LoadBackground()
{
    GETextureDescription theDescription = this->mTextureDescription;
    unsigned long theScreen = this->mForm->GetManager()->GetScreen();
    GETextureManager* theManager = this->mForm->GetManager()->GetRasterizer()->GetTextureManager(theScreen);
    if (!(this->mTextureFileName.GetLength() > 0))
        return kErrorOK;
    GETexture* theTexture = theManager->LoadTexture(this->mTextureFileName, theDescription);
    theTexture->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture->SetStageColorMode(kStageOpModulate, -1);
    theTexture->SetStageMinFilterMode(1, -1);
    theTexture->SetStageMagFilterMode(1, -1);
    theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture->SetStageMipMapBias(-0.75f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    this->mBitmap->SetTexture(theTexture, theScreen);
    return kErrorOK;
}

// FUNCTION: LLE 0x1005daab
// Replaces the background texture under the form's background lock
void UIBitmapBackground::SetBitmapTexture(GETexture* inTexture)
{
    this->mForm->LockDrawBackground();
    this->mBitmap->SetTexture(inTexture, this->mForm->GetManager()->GetScreen());
    this->mForm->UnlockDrawBackground();
}

// FUNCTION: LLE 0x1005dae9
// Detaches the background texture; always kErrorOK
long UIBitmapBackground::UnloadBackground()
{
    unsigned long theScreen = this->mForm->GetManager()->GetScreen();
    if (this->mBitmap->GetTexture(theScreen))
        this->mBitmap->SetTexture(NULL, theScreen);
    return kErrorOK;
}

// Header-inline members first emitted here (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005d72a
// SYNTHETIC: LLE 0x1005d72e
// SYNTHETIC: LLE 0x1005d74f

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005d7aa
// SYNTHETIC: LLE 0x1005d7b4
// SYNTHETIC: LLE 0x1005d7c4
// SYNTHETIC: LLE 0x1005d7d0
// SYNTHETIC: LLE 0x1005d7da
// SYNTHETIC: LLE 0x1005d7e4
// SYNTHETIC: LLE 0x1005d7f4
// SYNTHETIC: LLE 0x1005d800
// SYNTHETIC: LLE 0x1005d80a
// SYNTHETIC: LLE 0x1005d814
// SYNTHETIC: LLE 0x1005d824
// SYNTHETIC: LLE 0x1005d830
// SYNTHETIC: LLE 0x1005d83a
// SYNTHETIC: LLE 0x1005d844
// SYNTHETIC: LLE 0x1005d854
// SYNTHETIC: LLE 0x1005d860
// SYNTHETIC: LLE 0x1005d86a
// SYNTHETIC: LLE 0x1005d874
// SYNTHETIC: LLE 0x1005d884
// SYNTHETIC: LLE 0x1005d890
// SYNTHETIC: LLE 0x1005d89a
// SYNTHETIC: LLE 0x1005d8a4
// SYNTHETIC: LLE 0x1005d8b4
// SYNTHETIC: LLE 0x1005d8c0
