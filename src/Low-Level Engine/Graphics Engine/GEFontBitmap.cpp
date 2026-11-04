// C:\EEX\Low-Level Engine\Graphics Engine\GEFontBitmap.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100152e4..0x100166d3: the U2D key init chains, GEFontBitmap (an anchor: __FILE__ in the binary), then the
// GEFBCharacter/vector<GEFBCharacter> COMDATs.
#include <windows.h>
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
// Renders every glyph of the font into 256x256 alpha textures and records each glyph's cell
GEFontBitmap::GEFontBitmap(GERasterizer* inRasterizer, GEFont* inFont, unsigned long inScreen)
{
    UWideString theString;
    GERectangle theRect;
    GETextureCoordinate theCoordinate;
    GEText theText(inFont);
    GE2DPoint thePosition;
    HDC theContext = GEDeviceContext::GetDeviceContext(inRasterizer->GetWindow(inScreen)->GetOSRepresentation());
    HGDIOBJ theFont = inFont->GetOSFont();
    GEScreen* theScreen = inRasterizer->GetScreen(inScreen);
    GETexture* theTexture = NULL;
    unsigned long theRowHeight = 0;
    unsigned long theSource;
    unsigned long theDestination;
    inRasterizer->GetBlending(theSource, theDestination, inScreen);
    inRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, inScreen);
    this->mBitmap = new GEBitmap;
    theText.SetFgColor(GEColor());
    theText.SetIsTransparent(true);
    HGDIOBJ theOldFont = SelectObject(theContext, theFont);
    GetCurrentObject(theContext, OBJ_FONT);
    TEXTMETRIC theMetrics;
    GetTextMetrics(theContext, &theMetrics);
    this->mFirstCharacter = theMetrics.tmFirstChar;
    this->mCharacters.resize(theMetrics.tmLastChar - theMetrics.tmFirstChar + 1);
    // tmLastChar is the last character of the font, included (mCharacters has room for it).
    for (unsigned long theCharacter = theMetrics.tmFirstChar; theCharacter <= theMetrics.tmLastChar; theCharacter++) {
        unsigned long theIndex = theCharacter - this->mFirstCharacter;
        sprintf(theString, "%c", theCharacter);
        theText.SetText(theString);
        float theHeight = theText.GetTextExtent().GetHeight();
        if (theHeight > theRowHeight)
            theRowHeight = theHeight;
        if (theText.GetTextExtent().GetWidth() + thePosition.GetX() >= 256.0f) {
            thePosition.SetY(thePosition.GetY() + theRowHeight);
            theRowHeight = 0;
            thePosition.SetX(0.0f);
        }
        if (theTexture == NULL || thePosition.GetY() + theHeight >= 256.0f) {
            if (theTexture != NULL) {
                theTexture->GetSurface()->TextUnlock();
                this->CreateSurfaceAlpha(theTexture->GetSurface());
            }
            theTexture = this->CreateNewFontTexture(theScreen);
            thePosition.SetXY(0.0f, 0.0f);
        }
        this->mCharacters[theIndex].mRect.SetLeft(thePosition.GetX());
        this->mCharacters[theIndex].mRect.SetTop(thePosition.GetY());
        this->mCharacters[theIndex].mRect.SetRight(theText.GetTextExtent().GetWidth() + thePosition.GetX());
        this->mCharacters[theIndex].mRect.SetBottom(thePosition.GetY() + theText.GetTextExtent().GetHeight());
        theText.SetLocation(thePosition);
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
        thePosition.SetX(this->mCharacters[theIndex].mRect.GetWidth() + thePosition.GetX());
    }
    inFont->SetFontBitmap(this);
    theTexture->GetSurface()->TextUnlock();
    this->CreateSurfaceAlpha(theTexture->GetSurface());
    SelectObject(theContext, theOldFont);
    inRasterizer->SetBlending(theSource, theDestination, inScreen);
    GEDeviceContext::ReleaseDeviceContext(inRasterizer->GetWindow(inScreen)->GetOSRepresentation(), theContext);
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

    theSurface->Clear(GEColor(0.0f, 0.0f, 0.0f), 1.0f);
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
// Turns a rendered glyph texture into white with alpha taken from the green channel (black stays transparent)
long GEFontBitmap::CreateSurfaceAlpha(GESurface* inSurface)
{
    inSurface->Lock();
    unsigned long theSkip = (inSurface->GetBytesPerRow() - inSurface->GetWidth() * 2) / 2;
    unsigned short* thePixel = reinterpret_cast<unsigned short*>(inSurface->GetMemoryAddress());
    for (unsigned long y = 0; y < inSurface->GetHeight(); y++) {
        for (unsigned long x = 0; x < inSurface->GetWidth(); x++) {
            if (GEUnpackGreen(kPixelFormatA4R4G4B4, *thePixel) == 0)
                *thePixel = GEPackColor(kPixelFormatA4R4G4B4, 0, 0, 0, 0);
            else
                *thePixel = GEPackColor(kPixelFormatA4R4G4B4, 255, 255, 255, GEUnpackGreen(kPixelFormatA4R4G4B4, *thePixel));
            thePixel++;
        }
        thePixel += theSkip;
    }
    inSurface->Unlock();
    return 0;
}

// FUNCTION: LLE 0x10015b99
// Draws the text glyph by glyph at its location in its colour; returns the last draw's result
long GEFontBitmap::Draw(GEText* inText, GERasterizer* inRasterizer, unsigned long inScreen)
{
    long theResult = 0;
    unsigned char theBuffer[8];
    GE2DPoint theLocation;
    theLocation = inText->GetLocation();
    const unsigned char* theCharacter = reinterpret_cast<const unsigned char*>((const char*)inText->GetText());
    this->mBitmap->SetColor(inText->GetFgColor());
    while (_mbscmp(theCharacter, (const unsigned char*)"") != 0) {
        strcpy(reinterpret_cast<char*>(theBuffer), "");
        _mbsnbcat(theBuffer, theCharacter, 1);
        long theIndex = (char)*theCharacter - this->mFirstCharacter;
        this->mBitmap->SetTexture(this->mCharacters[theIndex].mTexture, inScreen);
        this->mBitmap->SetTextureCoordinates(
            this->mCharacters[theIndex].mTopLeft,
            GETextureCoordinate(this->mCharacters[theIndex].mBottomRight.GetU(), this->mCharacters[theIndex].mTopLeft.GetV()),
            GETextureCoordinate(this->mCharacters[theIndex].mTopLeft.GetU(), this->mCharacters[theIndex].mBottomRight.GetV()),
            this->mCharacters[theIndex].mBottomRight);
        this->mBitmap->SetXYScale(this->mCharacters[theIndex].mXScale, this->mCharacters[theIndex].mYScale);
        this->mBitmap->SetLocation(theLocation);
        theResult = inRasterizer->Draw(this->mBitmap, inScreen);
        theLocation.SetX(theLocation.GetX() + this->mCharacters[theIndex].mRect.GetWidth());
        theCharacter = _mbsinc(theCharacter);
    }
    return theResult;
}

// FUNCTION: LLE 0x10015dcd
// Draws the text glyph by glyph clipped to a rectangle; a glyph fully outside is skipped without advancing the pen
long GEFontBitmap::Draw(GEText* inText, const GERectangle& inRectangle, GERasterizer* inRasterizer,
                        unsigned long inScreen)
{
    GERectangle theSource;
    GE2DPoint theLocation = inText->GetLocation();
    GERectangle theDestination(inText->GetLocation());
    long theResult = 0;
    unsigned char theBuffer[4];
    const unsigned char* theCharacter = reinterpret_cast<const unsigned char*>((const char*)inText->GetText());
    this->mBitmap->SetColor(inText->GetFgColor());
    while (_mbscmp(theCharacter, (const unsigned char*)"") != 0) {
        strcpy(reinterpret_cast<char*>(theBuffer), "");
        _mbsnbcat(theBuffer, theCharacter, 1);
        long theIndex = (char)*theCharacter - this->mFirstCharacter;
        this->mBitmap->SetTexture(this->mCharacters[theIndex].mTexture, inScreen);
        theDestination = GERectangle(theLocation, this->mCharacters[theIndex].mRect.GetWidth(),
                                     this->mCharacters[theIndex].mRect.GetHeight());
        if (inRectangle.Contains(theDestination)) {
            this->mBitmap->SetTextureCoordinates(
                this->mCharacters[theIndex].mTopLeft,
                GETextureCoordinate(this->mCharacters[theIndex].mBottomRight.GetU(), this->mCharacters[theIndex].mTopLeft.GetV()),
                GETextureCoordinate(this->mCharacters[theIndex].mTopLeft.GetU(), this->mCharacters[theIndex].mBottomRight.GetV()),
                this->mCharacters[theIndex].mBottomRight);
            this->mBitmap->SetXYScale(this->mCharacters[theIndex].mXScale, this->mCharacters[theIndex].mYScale);
        } else if (theDestination.Intersects(inRectangle)) {
            theSource = this->mCharacters[theIndex].mRect;
            if (inRectangle.GetTop() > theDestination.GetTop()) {
                theSource.SetTop(theSource.GetTop() + (inRectangle.GetTop() - theDestination.GetTop()));
                theDestination.SetTop(inRectangle.GetTop());
            }
            if (inRectangle.GetLeft() > theDestination.GetLeft()) {
                theSource.SetLeft(theSource.GetLeft() + (inRectangle.GetLeft() - theDestination.GetLeft()));
                theDestination.SetLeft(inRectangle.GetLeft());
            }
            if (inRectangle.GetRight() < theDestination.GetRight()) {
                theSource.SetRight(theSource.GetRight() - (theDestination.GetRight() - inRectangle.GetRight()));
                theDestination.SetRight(inRectangle.GetRight());
            }
            if (inRectangle.GetBottom() < theDestination.GetBottom()) {
                theSource.SetBottom(theSource.GetBottom() - (theDestination.GetBottom() - inRectangle.GetBottom()));
                theDestination.SetBottom(inRectangle.GetBottom());
            }
            this->mBitmap->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, inScreen);
        } else {
            theDestination.SetLeft(theDestination.GetRight() + 1.0f);
            theCharacter = _mbsinc(theCharacter);
            continue;
        }
        this->mBitmap->SetLocation(theDestination.GetTopLeft());
        theResult = inRasterizer->Draw(this->mBitmap, inScreen);
        theLocation.SetX(theLocation.GetX() + this->mCharacters[theIndex].mRect.GetWidth());
        theCharacter = _mbsinc(theCharacter);
    }
    return theResult;
}
