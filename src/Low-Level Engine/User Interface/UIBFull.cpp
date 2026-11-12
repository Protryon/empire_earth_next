// C:\EEX\Low-Level Engine\User Interface\UIBFull.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005c84e..0x1005d3a1: first includer of UITextureBackground.h (UIBFull.cpp sorts before UITextureBackground.cpp
// and UIBitmapBackground.cpp), so it emits UITextureBackground's inline members first (0x1005c84e..0x1005c9f9), then
// UIBFull's (GetBackgroundBitmap, the copy ctor, ??_E; operator= is ICF-folded at 0x10078dda), the U2D key init
// chains, the ctor and the out-of-line members. UIBitmap.cpp starts at 0x1005d3a1.
//
// DrawBackground's tile placement and LoadBackground's tile names are Rust (rust/crates/ee_ui/src/background.rs
// bfull_draw, full_tile_name; docs/RUST_PLAN.md R6); the bitmaps, textures and draws stay here.
#include <windows.h>

#include "rust/ee_ui.h"
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
#include "lle/UIBFull.h"

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top)
static inline EeUiRect UIBFullRect(const GERectangle& r) { return EeUiRect{r.GetBottom(), r.GetLeft(), r.GetRight(), r.GetTop()}; }
static inline GERectangle UIBFullRect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }

// The Rust tile placement's draw (ee_ui background.rs bfull_draw): one tile stretched onto its clipped part
struct UIBFullRust {
    static long Draw(UIBFull* inBackground, unsigned long inIndex, const GERectangle& inTile, const GERectangle& inClipped)
    {
        unsigned long theScreen = inBackground->mForm->GetManager()->GetScreen();
        GEBitmap* theBitmap = inBackground->mBitmaps[inIndex];
        GERectangle theTextureRectangle(0.0f, 0.0f, (float)inBackground->mBitmaps[0]->GetTexture(theScreen)->GetWidth(),
                                        (float)inBackground->mBitmaps[0]->GetTexture(theScreen)->GetHeight());
        theBitmap->ComputeStretchedCoordsAndScale(inTile, inClipped, theTextureRectangle, theScreen);
        theBitmap->SetLocation(inClipped.GetTopLeft());
        theBitmap->SetOpacity(inBackground->mForm->GetFadeOpacity());
        return inBackground->mForm->GetManager()->GetRasterizer()->Draw(theBitmap, theScreen);
    }
};
extern "C" long ee_uicpp_background_bfull_draw(void* inBackground, unsigned long inIndex, const EeUiRect* inTile,
                                               const EeUiRect* inClipped)
{
    return UIBFullRust::Draw(static_cast<UIBFull*>(inBackground), inIndex, UIBFullRect(*inTile), UIBFullRect(*inClipped));
}

// FUNCTION: LLE 0x1005cbc2
// Creates a full-screen background tiled from a grid of bitmaps (columns x rows)
UIBFull::UIBFull(unsigned long inBackgroundID, const UString& inTextureFileName,
                 const GETextureDescription& inDescription, const GERectangle& inTextureRectangle,
                 const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority, unsigned long inColumns,
                 unsigned long inRows, bool inVisible)
    : UITextureBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                          inDrawPriority, inVisible)
{
    this->mBitmaps = NULL;
    this->SetNumberColumnsAndRows(inColumns, inRows);
}

// FUNCTION: LLE 0x1005cc03
// Destroys the background, releasing every tile bitmap
UIBFull::~UIBFull()
{
    unsigned long theRow;
    unsigned long theColumn;
    for (theRow = 0; theRow < this->mRows; theRow++)
        for (theColumn = 0; theColumn < this->mColumns; theColumn++)
            this->GetBackgroundBitmap(theColumn, theRow)->Dereference();
    delete[] this->mBitmaps;
}

// FUNCTION: LLE 0x1005cc64
// Draws the tiles that overlap the given rectangle, each stretched to its share of the form and clipped
long UIBFull::DrawBackground(const GERectangle& inRectangle)
{
    unsigned long theScreen = this->mForm->GetManager()->GetScreen();
    GETexture* theTexture = this->mBitmaps[0]->GetTexture(theScreen);
    return ee_ui_background_bfull_draw(this, UIBFullRect(this->mForm->GetRectangle()), UIBFullRect(inRectangle),
                                       theTexture->GetWidth(), theTexture->GetHeight(),
                                       UIBFullRect(this->mTextureRectangle), this->mColumns, this->mRows);
}

// FUNCTION: LLE 0x1005cfe0
// Loads one texture per tile, named <file>_<row>_<column> (1-based), and assigns it to the tile bitmap
long UIBFull::LoadBackground()
{
    GETextureDescription theDescription = this->mTextureDescription;
    UString theFileName;
    unsigned long theScreen = this->mForm->GetManager()->GetScreen();
    GETextureManager* theManager = this->mForm->GetManager()->GetRasterizer()->GetTextureManager(theScreen);
    for (unsigned long theRow = 0; theRow < this->mRows; theRow++) {
        for (unsigned long theColumn = 0; theColumn < this->mColumns; theColumn++) {
            unsigned long theIndex = this->mColumns * theRow + theColumn;
            ee_ui_background_full_tile_name(this->mTextureFileName, theRow, theColumn, &theFileName);   // Rust: the name
            GETexture* theTexture = theManager->LoadTexture(theFileName, theDescription);
            theTexture->SetStageAlphaMode(kStageOpModulate, -1);
            theTexture->SetStageColorMode(kStageOpModulate, -1);
            theTexture->SetStageMinFilterMode(1, -1);
            theTexture->SetStageMagFilterMode(1, -1);
            theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
            theTexture->SetStageMipMapBias(-0.75f, -1);
            theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
            this->mBitmaps[theIndex]->SetTexture(theTexture, theScreen);
        }
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1005d227
// Replaces one tile's texture under the form's background lock
void UIBFull::SetBitmapTexture(GETexture* inTexture, unsigned long inColumn, unsigned long inRow)
{
    this->mForm->LockDrawBackground();
    this->GetBackgroundBitmap(inColumn, inRow)->SetTexture(inTexture, this->mForm->GetManager()->GetScreen());
    this->mForm->UnlockDrawBackground();
}

// FUNCTION: LLE 0x1005d274
// Resizes the tile grid, releasing the old tile bitmaps and creating empty new ones
void UIBFull::SetNumberColumnsAndRows(unsigned long inColumns, unsigned long inRows)
{
    if (this->mBitmaps) {
        for (unsigned long theRow = 0; theRow < this->mRows; theRow++)
            for (unsigned long theColumn = 0; theColumn < this->mColumns; theColumn++)
                this->GetBackgroundBitmap(theColumn, theRow)->Dereference();
        delete[] this->mBitmaps;
    }
    this->mColumns = inColumns;
    this->mRows = inRows;
    this->mBitmaps = new GEBitmap*[inColumns * inRows];
    for (unsigned long theRow = 0; theRow < this->mRows; theRow++)
        for (unsigned long theColumn = 0; theColumn < this->mColumns; theColumn++) {
            unsigned long theIndex = this->mColumns * theRow + theColumn;
            this->mBitmaps[theIndex] = new GEBitmap;
        }
}

// FUNCTION: LLE 0x1005d335
// Detaches every tile's texture; always kErrorOK
long UIBFull::UnloadBackground()
{
    unsigned long theRow;
    unsigned long theColumn;
    unsigned long theScreen = this->mForm->GetManager()->GetScreen();
    for (theRow = 0; theRow < this->mRows; theRow++)
        for (theColumn = 0; theColumn < this->mColumns; theColumn++) {
            GEBitmap* theBitmap = this->GetBackgroundBitmap(theColumn, theRow);
            if (theBitmap && theBitmap->GetTexture(theScreen))
                theBitmap->SetTexture(NULL, theScreen);
        }
    return kErrorOK;
}

// Header-inline members first emitted here (UITextureBackground.h, then UIBFull.h; names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005c84e
// SYNTHETIC: LLE 0x1005c899
// SYNTHETIC: LLE 0x1005c8b3
// SYNTHETIC: LLE 0x1005c8b7
// SYNTHETIC: LLE 0x1005c8bb
// SYNTHETIC: LLE 0x1005c8bf
// SYNTHETIC: LLE 0x1005c8d2
// SYNTHETIC: LLE 0x1005c8f0
// SYNTHETIC: LLE 0x1005c930
// SYNTHETIC: LLE 0x1005c976
// SYNTHETIC: LLE 0x1005c9f9
// SYNTHETIC: LLE 0x1005ca0e
// SYNTHETIC: LLE 0x1005ca41

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005caa2
// SYNTHETIC: LLE 0x1005caac
// SYNTHETIC: LLE 0x1005cabc
// SYNTHETIC: LLE 0x1005cac8
// SYNTHETIC: LLE 0x1005cad2
// SYNTHETIC: LLE 0x1005cadc
// SYNTHETIC: LLE 0x1005caec
// SYNTHETIC: LLE 0x1005caf8
// SYNTHETIC: LLE 0x1005cb02
// SYNTHETIC: LLE 0x1005cb0c
// SYNTHETIC: LLE 0x1005cb1c
// SYNTHETIC: LLE 0x1005cb28
// SYNTHETIC: LLE 0x1005cb32
// SYNTHETIC: LLE 0x1005cb3c
// SYNTHETIC: LLE 0x1005cb4c
// SYNTHETIC: LLE 0x1005cb58
// SYNTHETIC: LLE 0x1005cb62
// SYNTHETIC: LLE 0x1005cb6c
// SYNTHETIC: LLE 0x1005cb7c
// SYNTHETIC: LLE 0x1005cb88
// SYNTHETIC: LLE 0x1005cb92
// SYNTHETIC: LLE 0x1005cb9c
// SYNTHETIC: LLE 0x1005cbac
// SYNTHETIC: LLE 0x1005cbb8
