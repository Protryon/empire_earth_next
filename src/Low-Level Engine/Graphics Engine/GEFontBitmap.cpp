// C:\EEX\Low-Level Engine\Graphics Engine\GEFontBitmap.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100152e4..0x100166d3: the U2D key init chains, GEFontBitmap (an anchor: __FILE__ in the binary), then the
// GEFBCharacter/vector<GEFBCharacter> COMDATs.
#include <windows.h>
#include "platform/PlatFont.h"
#include <mbstring.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
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
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/Graphics Engine.h"
#include "GERustMath.h"
#include "GERustText.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x10016206
// TEMPLATE: LLE 0x10016230
// TEMPLATE: LLE 0x100162e4
// TEMPLATE: LLE 0x100162f6
// TEMPLATE: LLE 0x100165c5
// TEMPLATE: LLE 0x1001660f
// TEMPLATE: LLE 0x1001663c
// TEMPLATE: LLE 0x10016650
// TEMPLATE: LLE 0x1001666d
// TEMPLATE: LLE 0x10016699
// TEMPLATE: LLE 0x100166bd

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x100152e4
// SYNTHETIC: LLE 0x100152ee
// SYNTHETIC: LLE 0x100152fe
// SYNTHETIC: LLE 0x1001530a
// SYNTHETIC: LLE 0x10015314
// SYNTHETIC: LLE 0x1001531e
// SYNTHETIC: LLE 0x1001532e
// SYNTHETIC: LLE 0x1001533a
// SYNTHETIC: LLE 0x10015344
// SYNTHETIC: LLE 0x1001534e
// SYNTHETIC: LLE 0x1001535e
// SYNTHETIC: LLE 0x1001536a
// SYNTHETIC: LLE 0x10015374
// SYNTHETIC: LLE 0x1001537e
// SYNTHETIC: LLE 0x1001538e
// SYNTHETIC: LLE 0x1001539a
// SYNTHETIC: LLE 0x100153a4
// SYNTHETIC: LLE 0x100153ae
// SYNTHETIC: LLE 0x100153be
// SYNTHETIC: LLE 0x100153ca
// SYNTHETIC: LLE 0x100153d4
// SYNTHETIC: LLE 0x100153de
// SYNTHETIC: LLE 0x100153ee
// SYNTHETIC: LLE 0x100153fa

// FUNCTION: LLE 0x10015404
// Always returns true (any font matches)
bool GEFontBitmap::operator==(GEFont* inFont)
{
    return true;
}

// FUNCTION: LLE 0x10015409
// Renders every glyph of the font into 256x256 alpha textures and records each glyph's cell (the cells' layout is
// Rust: ee_ge font.rs GeFontPacker)
GEFontBitmap::GEFontBitmap(GERasterizer* inRasterizer, GEFont* inFont, unsigned long inScreen)
{
    UWideString theString;
    GERectangle theRect;
    GETextureCoordinate theCoordinate;
    GEText theText(inFont);
    EeGeFontPacker thePacker = {};   // the pen at the top left, no texture yet
    GEScreen* theScreen = inRasterizer->GetScreen(inScreen);
    GETexture* theTexture = NULL;
    unsigned long theSource;
    unsigned long theDestination;
    inRasterizer->GetBlending(theSource, theDestination, inScreen);
    inRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, inScreen);
    this->mBitmap = new GEBitmap;
    theText.SetFgColor(GEColor());
    theText.SetIsTransparent(true);
    PlatFontMetrics theMetrics;   // the font's character range (TEXTMETRIC's tmFirstChar..tmLastChar)
    PlatFontGetMetrics(inFont->GetOSFont(), &theMetrics);
    this->mFirstCharacter = theMetrics.mFirstChar;
    this->mCharacters.resize(theMetrics.mLastChar - theMetrics.mFirstChar + 1);
    // the last character of the font is included (mCharacters has room for it). A cell is a Windows-1252 byte's
    // character (port, decision D6: the text is UTF-8, so the cell holds that character's UTF-8 and Draw maps a
    // drawn character back to its cell; a Latin-1 or 1252 text draws as before)
    for (unsigned long theCharacter = theMetrics.mFirstChar; theCharacter <= theMetrics.mLastChar; theCharacter++) {
        unsigned long theIndex = theCharacter - this->mFirstCharacter;
        char theByte = static_cast<char>(theCharacter);
        char theCell[8];
        PlatCp1252ToUtf8(&theByte, 1, theCell, sizeof(theCell));
        theString = theCell;
        theText.SetText(theString);
        // the next cell: a new row at the right edge, a new texture at the bottom edge
        EeGeRectangle thePlace;
        if (ee_ge_font_cell_place(&thePacker, theText.GetTextExtent().GetWidth(), theText.GetTextExtent().GetHeight(),
                                  &thePlace)) {
            if (theTexture != NULL) {
                theTexture->GetSurface()->TextUnlock();
                this->CreateSurfaceAlpha(theTexture->GetSurface());
            }
            theTexture = this->CreateNewFontTexture(theScreen);
        }
        *GERust(this->mCharacters[theIndex].mRect) = thePlace;
        theText.SetLocation(this->mCharacters[theIndex].mRect.GetTopLeft());
        inRasterizer->Draw(&theText, theTexture->GetSurface());
        this->mCharacters[theIndex].mTexture = theTexture;
        this->mCharacters[theIndex].mTexture->Reference();
        this->mBitmap->SetTexture(theTexture, inScreen);
        theRect.SetTopLeft(this->mCharacters[theIndex].mRect.GetTopLeft());
        theRect.SetRight(this->mCharacters[theIndex].mRect.GetRight() - 1.0f);
        theRect.SetBottom(this->mCharacters[theIndex].mRect.GetBottom() - 1.0f);
        this->mBitmap->ComputeStretchedCoordsAndScale(theRect, theRect, this->mCharacters[theIndex].mRect, inScreen);
        this->mBitmap->GetTextureCoordinatesPerVertex(this->mCharacters[theIndex].mTopLeft, theCoordinate, theCoordinate,
                                                this->mCharacters[theIndex].mBottomRight);
        this->mCharacters[theIndex].mXScale = this->mBitmap->GetXScale();
        this->mCharacters[theIndex].mYScale = this->mBitmap->GetYScale();
    }
    inFont->SetFontBitmap(this);
    theTexture->GetSurface()->TextUnlock();
    this->CreateSurfaceAlpha(theTexture->GetSurface());
    inRasterizer->SetBlending(theSource, theDestination, inScreen);
}

// FUNCTION: LLE 0x100159ac
// Destroys the glyph bitmap, releasing its drawing bitmap
GEFontBitmap::~GEFontBitmap()
{
    this->mBitmap->Dereference();
}

// FUNCTION: LLE 0x100159cd
// Creates a cleared 256x256 A4R4G4B4 texture for glyphs: clamped, unfiltered, alpha from the texture
GETexture* GEFontBitmap::CreateNewFontTexture(GEScreen* inScreen)
{
    GESurfaceDescription theDescription;
    GESurface* theSurface;
    theDescription.mType = kSurfaceTexture;
    theDescription.mPixelFormat = kPixelFormatA4R4G4B4;
    theDescription.mPixelDepth = 16;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mWidth = 256;
    theDescription.mHeight = 256;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    long theResult = inScreen->CreateSurface(theDescription, &theSurface);
    ESVERIFY_AT(theResult == keseOK, 228);
GEColor theTemp1(0.0f, 0.0f, 0.0f);

    theSurface->Clear(theTemp1, 1.0f);
    GETexture* theTexture = inScreen->GetTextureManager()->MakeTexture(theSurface);
    ESVERIFY_AT(theTexture != NULL, 235);
    theTexture->SetStageMinFilterMode(0, -1);
    theTexture->SetStageMagFilterMode(0, -1);
    theTexture->SetStageMipFilterMode(kMipFilterNone, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    theTexture->SetStageAlphaMode(kStageOpTexture, -1);
    theTexture->SetStageColorMode(kStageOpModulate, -1);
    return theTexture;
}

// FUNCTION: LLE 0x10015ac5
// Turns a rendered glyph texture into white with alpha taken from the green channel (black stays transparent; the
// pixel pass is Rust: ee_ge font.rs)
long GEFontBitmap::CreateSurfaceAlpha(GESurface* inSurface)
{
    inSurface->Lock();
    ee_ge_font_surface_alpha(reinterpret_cast<uint16_t*>(inSurface->GetMemoryAddress()), inSurface->GetWidth(),
                             inSurface->GetHeight(), inSurface->GetBytesPerRow());
    inSurface->Unlock();
    return 0;
}

// The glyphs of a text's draw (Rust: ee_ge font.rs draw_plan; inClip NULL: unclipped), at most one a byte
static std::vector<EeGeGlyphDraw> GEFontBitmapPlan(GEText* inText, const GERectangle* inClip, unsigned long inFirst,
                                                   const std::vector<GEFBCharacter>& inCharacters)
{
    const char* theText = inText->GetText();
    std::vector<EeGeGlyphDraw> thePlan(strlen(theText));
    thePlan.resize(ee_ge_font_draw_plan(GERustTextOps(), theText, inFirst,
                                        reinterpret_cast<const EeGeFbCharacter*>(inCharacters.data()),
                                        inCharacters.size(), GERust(inText->GetLocation()),
                                        inClip != NULL ? GERust(*inClip) : NULL, thePlan.data(), thePlan.size()));
    return thePlan;
}

// FUNCTION: LLE 0x10015b99
// Draws the text glyph by glyph at its location in its colour; returns the last draw's result. A character's cell is
// its Windows-1252 byte (decision D6: the text is UTF-8); a character outside the code page or the font is skipped,
// as a byte outside the font was. The walk is Rust (ee_ge font.rs); the bitmap's calls are made here in its order.
long GEFontBitmap::Draw(GEText* inText, GERasterizer* inRasterizer, unsigned long inScreen)
{
    long theResult = 0;
    std::vector<EeGeGlyphDraw> thePlan = GEFontBitmapPlan(inText, NULL, this->mFirstCharacter, this->mCharacters);
    this->mBitmap->SetColor(inText->GetFgColor());
    for (size_t i = 0; i < thePlan.size(); i++) {
        const GEFBCharacter& theCell = this->mCharacters[thePlan[i].cell];
        this->mBitmap->SetTexture(theCell.mTexture, inScreen);
        this->mBitmap->SetTextureCoordinates(theCell.mTopLeft,
                                             GETextureCoordinate(theCell.mBottomRight.GetU(), theCell.mTopLeft.GetV()),
                                             GETextureCoordinate(theCell.mTopLeft.GetU(), theCell.mBottomRight.GetV()),
                                             theCell.mBottomRight);
        this->mBitmap->SetXYScale(theCell.mXScale, theCell.mYScale);
        this->mBitmap->SetLocation(*reinterpret_cast<const GE2DPoint*>(&thePlan[i].location));
        theResult = inRasterizer->Draw(this->mBitmap, inScreen);
    }
    return theResult;
}

// FUNCTION: LLE 0x10015dcd
// Draws the text glyph by glyph clipped to a rectangle: a glyph inside it whole, one across its edge cut to it, one
// fully outside not at all (its texture is still set and the pen still advances). The walk and the cuts are Rust
// (ee_ge font.rs).
long GEFontBitmap::Draw(GEText* inText, const GERectangle& inRectangle, GERasterizer* inRasterizer,
                        unsigned long inScreen)
{
    long theResult = 0;
    std::vector<EeGeGlyphDraw> thePlan = GEFontBitmapPlan(inText, &inRectangle, this->mFirstCharacter, this->mCharacters);
    this->mBitmap->SetColor(inText->GetFgColor());
    for (size_t i = 0; i < thePlan.size(); i++) {
        const EeGeGlyphDraw& theGlyph = thePlan[i];
        const GEFBCharacter& theCell = this->mCharacters[theGlyph.cell];
        this->mBitmap->SetTexture(theCell.mTexture, inScreen);
        if (theGlyph.kind == GE_GLYPH_SKIPPED)
            continue;
        if (theGlyph.kind == GE_GLYPH_FULL) {
            this->mBitmap->SetTextureCoordinates(theCell.mTopLeft,
                                                 GETextureCoordinate(theCell.mBottomRight.GetU(), theCell.mTopLeft.GetV()),
                                                 GETextureCoordinate(theCell.mTopLeft.GetU(), theCell.mBottomRight.GetV()),
                                                 theCell.mBottomRight);
            this->mBitmap->SetXYScale(theCell.mXScale, theCell.mYScale);
        } else {
            const GERectangle& theDestination = *reinterpret_cast<const GERectangle*>(&theGlyph.destination);
            this->mBitmap->ComputeStretchedCoordsAndScale(theDestination, theDestination,
                                                          *reinterpret_cast<const GERectangle*>(&theGlyph.source), inScreen);
        }
        this->mBitmap->SetLocation(*reinterpret_cast<const GE2DPoint*>(&theGlyph.location));
        theResult = inRasterizer->Draw(this->mBitmap, inScreen);
    }
    return theResult;
}
