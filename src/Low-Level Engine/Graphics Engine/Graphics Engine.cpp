// C:\EEX\Low-Level Engine\Graphics Engine\Graphics Engine.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10038527..0x10038abf (the last GE unit: "Graphics Engine" sorts after "GEWindow"): the U2D point/rectangle key
// chains, PIMModule's header-inline block (gone), the rasterizer plug-in list (gone), GEInitialize, the pixel-format packers and
// the shutdown hook, then vector COMDATs and the UList template statics' init pairs.
#include <windows.h>
#include "platform/PlatFont.h"

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSReadWriteLock.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/FSFileSpec.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/Graphics Engine.h"
#include "rust/ee_ge.h"       // the pixel formats (ee_ge pixel.rs)

void GEShutdown(bool inForced);

// (GEPixelFormat and sPixelFormats (0x100d9eb8), the shift and position of each channel per pixel format, are in
// Rust: ee_ge pixel.rs; GEPackColor and GEUnpack* forward)

HINSTANCE__* GEScreen::sInstance;           // 0x1012d948 (.bss after this unit's U2D keys)

// The renderer linked into the game: the T&L DX7 rasterizer (DX7Rasterizer.cpp). The original loaded every 'rast'
// plug-in DLL of the folder (sRasterizerWrappers, 0x1012d8d8, kept each module and its RPIMGetRasterizer); the plug-in
// boundary is gone (R4.3 part 14), its PIMIsValid test stays.
GERasterizer* DX7TnLRasterizer();

// Starts the graphics engine: registers the shutdown hook and adds the rasterizer when it activates and finds a screen
// (the original's PIMIsValid: activate, count the screens, deactivate)
long GEInitialize(FSFileSpec& inDirectory, HINSTANCE__* const inInstance)
{
    URegisterShutdown(GEShutdown);
    PlatFontRegisterBuiltIn();   // the faces the fonts are made from (GDI's system fonts before)
    GEScreen::sInstance = inInstance;
    GERasterizer::ClearRasterizers();
    GERasterizer* theRasterizer = DX7TnLRasterizer();
    bool theValid = theRasterizer->Activate() == kErrorOK && theRasterizer->GetScreenCount() > 0;
    theRasterizer->Deactivate();
    if (theValid)
        GERasterizer::AddRasterizer(theRasterizer);
    return kErrorOK;
}

// Packs a color and alpha (0..1 floats) into a pixel of the given format
unsigned long GEPackColor(unsigned long inFormat, const GEColor& inColor, float inAlpha)
{
    return ee_ge_pack_color_float(inFormat, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue(), inAlpha);
}

// Packs 8-bit red, green, blue and alpha into a pixel of the given format
unsigned long GEPackColor(unsigned long inFormat, unsigned long inRed, unsigned long inGreen, unsigned long inBlue,
                          unsigned long inAlpha)
{
    return ee_ge_pack_color(inFormat, inRed, inGreen, inBlue, inAlpha);
}

// Shuts the graphics engine down: forgets the rasterizers
void GEShutdown(bool inForced)
{
    GERasterizer::ClearRasterizers();
}

// Returns a pixel's red channel scaled to 8 bits for the given format
unsigned char GEUnpackRed(unsigned long inFormat, unsigned long inColor)
{
    return ee_ge_unpack_red(inFormat, inColor);
}

// Returns a pixel's green channel scaled to 8 bits for the given format
unsigned char GEUnpackGreen(unsigned long inFormat, unsigned long inColor)
{
    return ee_ge_unpack_green(inFormat, inColor);
}

// Returns a pixel's blue channel scaled to 8 bits for the given format
unsigned char GEUnpackBlue(unsigned long inFormat, unsigned long inColor)
{
    return ee_ge_unpack_blue(inFormat, inColor);
}

// Returns a pixel's alpha channel scaled to 8 bits for the given format
unsigned char GEUnpackAlpha(unsigned long inFormat, unsigned long inColor)
{
    return ee_ge_unpack_alpha(inFormat, inColor);
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
