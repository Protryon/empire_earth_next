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

// ---- GEFontBitmap.
//
// port (R4.3, tenth part): the font bitmap is a whole Rust object (ee_ge fbobj.rs): the constructor's glyph rendering,
// the destructor, CreateNewFontTexture, CreateSurfaceAlpha and the two draws. This unit is its shell: the forwarders
// and the host, which makes each call the original made on another object (the rasterizer, the font, the working
// text, the drawing bitmap, the textures, surfaces and screens). Floats cross the host as their bits.

static_assert(sizeof(EeGeSurfaceDescription) == sizeof(GESurfaceDescription), "ee_ge texobj.rs EeGeSurfaceDescription");

intptr_t GEFontBitmap::Host(void* inOwner, EeGeFbOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    GEText* theText = (GEText*)inObj;
    GEBitmap* theBitmap = (GEBitmap*)inObj;
    GESurface* theSurface = (GESurface*)inObj;
    GERasterizer* theRasterizer = (GERasterizer*)inObj;
    GETexture* theTexture = (GETexture*)inObj;
    GEScreen* theScreen = (GEScreen*)inObj;
    unsigned long* theLongs = (unsigned long*)a;
    float* theFloats = (float*)a;
    GETextureCoordinate* theCoordinates = (GETextureCoordinate*)a;
    switch (inOp) {
    case EeGeFbOp::TextNew:
        return (intptr_t)new GEText((GEFont*)inObj);
    case EeGeFbOp::TextDelete:
        delete theText;
        return 0;
    case EeGeFbOp::TextPrepare:
        theText->SetFgColor(GEColor());
        theText->SetIsTransparent(true);
        return 0;
    case EeGeFbOp::TextSetByte: {
        // a cell is a Windows-1252 byte's character (decision D6: the text is UTF-8, so the cell holds that
        // character's UTF-8 and Draw maps a drawn character back to its cell)
        char theByte = (char)a;
        char theCell[8];
        PlatCp1252ToUtf8(&theByte, 1, theCell, sizeof(theCell));
        UWideString theString;
        theString = theCell;
        theText->SetText(theString);
        return 0;
    }
    case EeGeFbOp::TextExtent:
        theFloats[0] = theText->GetTextExtent().GetWidth();
        theFloats[1] = theText->GetTextExtent().GetHeight();
        return 0;
    case EeGeFbOp::TextSetLocation:
        theText->SetLocation(*(const GE2DPoint*)a);
        return 0;
    case EeGeFbOp::TextString:
        return (intptr_t)(const char*)theText->GetText();
    case EeGeFbOp::TextLocation:
        return (intptr_t)&theText->GetLocation();
    case EeGeFbOp::TextColor:
        return (intptr_t)&theText->GetFgColor();
    case EeGeFbOp::TextOps:
        return (intptr_t)GERustTextOps();
    case EeGeFbOp::FontRange: {
        PlatFontMetrics theMetrics;   // the font's character range (TEXTMETRIC's tmFirstChar..tmLastChar)
        PlatFontGetMetrics(((GEFont*)inObj)->GetOSFont(), &theMetrics);
        theLongs[0] = theMetrics.mFirstChar;
        theLongs[1] = theMetrics.mLastChar;
        return 0;
    }
    case EeGeFbOp::FontSetBitmap:
        ((GEFont*)inObj)->SetFontBitmap((GEFontBitmap*)inOwner);
        return 0;
    case EeGeFbOp::RasterScreen:
        return (intptr_t)theRasterizer->GetScreen((unsigned long)a);
    case EeGeFbOp::RasterGetBlending:
        theRasterizer->GetBlending(theLongs[0], theLongs[1], (unsigned long)b);
        return 0;
    case EeGeFbOp::RasterSetBlending:
        theRasterizer->SetBlending(theLongs[0], theLongs[1], (unsigned long)b);
        return 0;
    case EeGeFbOp::RasterDrawText:
        return theRasterizer->Draw((GEText*)a, (GESurface*)b);
    case EeGeFbOp::RasterDrawBitmap:
        return theRasterizer->Draw((GEBitmap*)a, (unsigned long)b);
    case EeGeFbOp::BmpNew:
        return (intptr_t)new GEBitmap;
    case EeGeFbOp::BmpDereference:
        theBitmap->Dereference();
        return 0;
    case EeGeFbOp::BmpSetTexture:
        theBitmap->SetTexture((GETexture*)a, (unsigned long)b);
        return 0;
    case EeGeFbOp::BmpStretch: {
        const GERectangle* theRects = (const GERectangle*)a;
        theBitmap->ComputeStretchedCoordsAndScale(theRects[0], theRects[0], theRects[1], (unsigned long)b);
        return 0;
    }
    case EeGeFbOp::BmpUvsPerVertex: {
        GETextureCoordinate theCoordinate;
        theBitmap->GetTextureCoordinatesPerVertex(theCoordinates[0], theCoordinate, theCoordinate, theCoordinates[1]);
        return 0;
    }
    case EeGeFbOp::BmpScales:
        theFloats[0] = theBitmap->GetXScale();
        theFloats[1] = theBitmap->GetYScale();
        return 0;
    case EeGeFbOp::BmpSetColor:
        theBitmap->SetColor(*(const GEColor*)a);
        return 0;
    case EeGeFbOp::BmpSetUvs:
        theBitmap->SetTextureCoordinates(theCoordinates[0], theCoordinates[1], theCoordinates[2], theCoordinates[3]);
        return 0;
    case EeGeFbOp::BmpSetXyScale: {
        uint32_t theBits[2] = {(uint32_t)a, (uint32_t)b};
        float theScales[2];
        memcpy(theScales, theBits, sizeof(theScales));
        theBitmap->SetXYScale(theScales[0], theScales[1]);
        return 0;
    }
    case EeGeFbOp::BmpSetLocation:
        theBitmap->SetLocation(*(const GE2DPoint*)a);
        return 0;
    case EeGeFbOp::TexSurface:
        return (intptr_t)theTexture->GetSurface();
    case EeGeFbOp::TexReference:
        theTexture->Reference();
        return 0;
    case EeGeFbOp::TexStage:
        switch ((EeGeTexStage)a) {
        case EeGeTexStage::MinFilterMode:
            theTexture->SetStageMinFilterMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::MagFilterMode:
            theTexture->SetStageMagFilterMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::MipFilterMode:
            theTexture->SetStageMipFilterMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::TextureAddressingMode:
            theTexture->SetStageTextureAddressingMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::AlphaMode:
            theTexture->SetStageAlphaMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::ColorMode:
            theTexture->SetStageColorMode((unsigned long)b, -1);
            break;
        default:
            break;
        }
        return 0;
    case EeGeFbOp::SurfTextUnlock:
        return theSurface->TextUnlock();
    case EeGeFbOp::SurfLock:
        return theSurface->Lock();
    case EeGeFbOp::SurfUnlock:
        return theSurface->Unlock();
    case EeGeFbOp::SurfPixels:
        theLongs[0] = theSurface->GetWidth();
        theLongs[1] = theSurface->GetHeight();
        theLongs[2] = theSurface->GetBytesPerRow();
        return (intptr_t)theSurface->GetMemoryAddress();
    case EeGeFbOp::SurfClear: {
        GEColor theBlack(0.0f, 0.0f, 0.0f);
        return theSurface->Clear(theBlack, 1.0f);
    }
    case EeGeFbOp::ScreenCreateSurface:
        return theScreen->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b);
    case EeGeFbOp::ScreenMakeTexture:
        return (intptr_t)theScreen->GetTextureManager()->MakeTexture((GESurface*)a);
    case EeGeFbOp::Verify:
        ESVerify(__FILE__, a == 0 ? 228 : 235, a == 0 ? "theResult == keseOK" : "theTexture != NULL");
        return 0;
    default:
        return 0;
    }
}

// FUNCTION: LLE 0x10015404
// Always returns true (any font matches)
bool GEFontBitmap::operator==(GEFont* inFont)
{
    return true;
}

// FUNCTION: LLE 0x10015409
// Renders every glyph of the font into 256x256 alpha textures and records each glyph's cell (Rust: ee_ge fbobj.rs)
GEFontBitmap::GEFontBitmap(GERasterizer* inRasterizer, GEFont* inFont, unsigned long inScreen)
{
    this->mRust = NULL;
    this->mRust = ee_ge_fb_new(&GEFontBitmap::Host, this, inRasterizer, inFont, inScreen);
}

// FUNCTION: LLE 0x100159ac
// Destroys the glyph bitmap, releasing its drawing bitmap
GEFontBitmap::~GEFontBitmap()
{
    ee_ge_fb_destroy(this->mRust);
}

// FUNCTION: LLE 0x10015b99
// Draws the text glyph by glyph at its location in its colour; returns the last draw's result (Rust: ee_ge fbobj.rs)
long GEFontBitmap::Draw(GEText* inText, GERasterizer* inRasterizer, unsigned long inScreen)
{
    return ee_ge_fb_draw(this->mRust, inText, NULL, inRasterizer, inScreen);
}

// FUNCTION: LLE 0x10015dcd
// Draws the text glyph by glyph clipped to a rectangle (Rust: ee_ge fbobj.rs)
long GEFontBitmap::Draw(GEText* inText, const GERectangle& inRectangle, GERasterizer* inRasterizer,
                        unsigned long inScreen)
{
    return ee_ge_fb_draw(this->mRust, inText, GERust(inRectangle), inRasterizer, inScreen);
}
