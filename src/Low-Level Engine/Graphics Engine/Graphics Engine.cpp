// C:\EEX\Low-Level Engine\Graphics Engine\Graphics Engine.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10038527..0x10038abf (the last GE unit: "Graphics Engine" sorts after "GEWindow"): the U2D point/rectangle key
// chains, PIMModule's header-inline block, the rasterizer plug-in list, GEInitialize, the pixel-format packers and
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
#include "lle/PIMModule.h"
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
#include "rust/ee_rust.h"       // the pixel formats (ee_ge pixel.rs)

void GEShutdown(bool inForced);

// A loaded rasterizer plug-in (RTTI .?AVRasterizerWrapper@@, vtable 0x980fc: only the deleting dtor).
class RasterizerWrapper {
public:
    RasterizerWrapper(PIMModule* inModule);
    virtual ~RasterizerWrapper() { this->mModule->Unload(); }

    GERasterizer* (*mGetRasterizer)();      // +0x04 RPIMGetRasterizer
    PIMModule* mModule;                     // +0x08
};

// (GEPixelFormat and sPixelFormats (0x100d9eb8), the shift and position of each channel per pixel format, are in
// Rust: ee_ge pixel.rs; GEPackColor and GEUnpack* forward)

HINSTANCE__* GEScreen::sInstance;           // 0x1012d948 (.bss after this unit's U2D keys)
static std::vector<RasterizerWrapper*> sRasterizerWrappers;     // 0x1012d8d8

// FUNCTION: LLE 0x100386f5
// Deletes every loaded rasterizer plug-in wrapper (unloading its module) and empties the list
static void DeleteRasterizerWrappers()
{
    for (unsigned long i = 0; i < sRasterizerWrappers.size(); i++)
        delete sRasterizerWrappers[i];
    sRasterizerWrappers.clear();
}

// FUNCTION: LLE 0x10038731
// Starts the graphics engine: registers the shutdown hook and loads every rasterizer plug-in ('rast') in a
// folder; returns success when the folder runs out
long GEInitialize(FSFileSpec& inDirectory, HINSTANCE__* const inInstance)
{
    PIMModule* theModule;
    RasterizerWrapper* theWrapper;
    long theResult;
    URegisterShutdown(GEShutdown);
    PlatFontRegisterBuiltIn();   // the faces the fonts are made from (GDI's system fonts before)
    GEScreen::sInstance = inInstance;
    GERasterizer::sRasterizers.clear();
    DeleteRasterizerWrappers();
    theResult = PIMModule::GetFirstModule(inDirectory, 'rast', theModule);
    while (theResult == kErrorOK) {
        theModule->GetName();
        if (theModule->IsValid()) {
            theWrapper = new RasterizerWrapper(theModule);
            sRasterizerWrappers.push_back(theWrapper);
            GERasterizer::sRasterizers.push_back(theWrapper->mGetRasterizer());
            theModule->GetName();
        } else {
            theModule->GetName();
            theModule->Unload();
        }
        theResult = PIMModule::GetNextModule(theModule);
    }
    if (theResult == kErrorNoMoreFiles)
        theResult = kErrorOK;
    return theResult;
}

// FUNCTION: LLE 0x100387f9
// Wraps a rasterizer plug-in module, looking up its RPIMGetRasterizer entry point
RasterizerWrapper::RasterizerWrapper(PIMModule* inModule)
{
    this->mModule = inModule;
    this->mGetRasterizer = (GERasterizer* (*)())inModule->GetEntry("RPIMGetRasterizer");
}

// FUNCTION: LLE 0x10038860
// Packs a color and alpha (0..1 floats) into a pixel of the given format
unsigned long GEPackColor(unsigned long inFormat, const GEColor& inColor, float inAlpha)
{
    return ee_ge_pack_color_float(inFormat, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue(), inAlpha);
}

// FUNCTION: LLE 0x1003890c
// Packs 8-bit red, green, blue and alpha into a pixel of the given format
unsigned long GEPackColor(unsigned long inFormat, unsigned long inRed, unsigned long inGreen, unsigned long inBlue,
                          unsigned long inAlpha)
{
    return ee_ge_pack_color(inFormat, inRed, inGreen, inBlue, inAlpha);
}

// FUNCTION: LLE 0x1003896b
// Shuts the graphics engine down: forgets the rasterizers and unloads their plug-ins
void GEShutdown(bool inForced)
{
    GERasterizer::sRasterizers.clear();
    DeleteRasterizerWrappers();
}

// FUNCTION: LLE 0x1003897a
// Returns a pixel's red channel scaled to 8 bits for the given format
unsigned char GEUnpackRed(unsigned long inFormat, unsigned long inColor)
{
    return ee_ge_unpack_red(inFormat, inColor);
}

// FUNCTION: LLE 0x10038996
// Returns a pixel's green channel scaled to 8 bits for the given format
unsigned char GEUnpackGreen(unsigned long inFormat, unsigned long inColor)
{
    return ee_ge_unpack_green(inFormat, inColor);
}

// FUNCTION: LLE 0x100389b2
// Returns a pixel's blue channel scaled to 8 bits for the given format
unsigned char GEUnpackBlue(unsigned long inFormat, unsigned long inColor)
{
    return ee_ge_unpack_blue(inFormat, inColor);
}

// FUNCTION: LLE 0x100389ce
// Returns a pixel's alpha channel scaled to 8 bits for the given format
unsigned char GEUnpackAlpha(unsigned long inFormat, unsigned long inColor)
{
    return ee_ge_unpack_alpha(inFormat, inColor);
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Header-inline members emitted here (markers at the end):
// SYNTHETIC: LLE 0x10038527
// SYNTHETIC: LLE 0x10038531
// SYNTHETIC: LLE 0x10038541
// SYNTHETIC: LLE 0x1003854d
// SYNTHETIC: LLE 0x10038557
// SYNTHETIC: LLE 0x10038561
// SYNTHETIC: LLE 0x10038571
// SYNTHETIC: LLE 0x1003857d
// SYNTHETIC: LLE 0x10038587
// SYNTHETIC: LLE 0x10038591
// SYNTHETIC: LLE 0x100385a1
// SYNTHETIC: LLE 0x100385ad
// SYNTHETIC: LLE 0x100385b7
// SYNTHETIC: LLE 0x100385c1
// SYNTHETIC: LLE 0x100385d1
// SYNTHETIC: LLE 0x100385dd
// SYNTHETIC: LLE 0x100385e7
// SYNTHETIC: LLE 0x100385f1
// SYNTHETIC: LLE 0x10038601
// SYNTHETIC: LLE 0x1003860d
// SYNTHETIC: LLE 0x10038617
// SYNTHETIC: LLE 0x10038621
// SYNTHETIC: LLE 0x10038631
// SYNTHETIC: LLE 0x1003863d
// SYNTHETIC: LLE 0x10038647
// SYNTHETIC: LLE 0x1003865a
// SYNTHETIC: LLE 0x1003867f
// SYNTHETIC: LLE 0x10038681
// SYNTHETIC: LLE 0x1003868f
// SYNTHETIC: LLE 0x10038692
// SYNTHETIC: LLE 0x10038695
// SYNTHETIC: LLE 0x100386aa
// SYNTHETIC: LLE 0x100386bd
// SYNTHETIC: LLE 0x100386c7
// SYNTHETIC: LLE 0x100386df
// SYNTHETIC: LLE 0x100386eb
// SYNTHETIC: LLE 0x1003883b
// SYNTHETIC: LLE 0x100389ea
// SYNTHETIC: LLE 0x10038a16
// SYNTHETIC: LLE 0x10038a51
// 0x10038a7d..0x10038abf: init pairs of UList<GELODMetaVertex*>/<GELODPolygon*>/<GELODVertex*>::sNodePool (their
// $D/$E are GEBoundingBox.cpp's COMDATs 0x14ed4..; GELOD.h comes in through GEModel.h).
// SYNTHETIC: LLE 0x10038a7d
// SYNTHETIC: LLE 0x10038a9b
// SYNTHETIC: LLE 0x10038aa7
// SYNTHETIC: LLE 0x10038ab3
