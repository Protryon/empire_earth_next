// C:\EEX\Low-Level Engine\Graphics Engine\GETexture.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10032bda..0x10033b1f: the U2D key init chains (U2DPoint.h, U2DRectangle.h; no guard byte), then the GETexture
// members: the two private ctors, the dtor, then alphabetical (Demote .. Unlock; SetFrame/SetSurface/Unlock after
// the SetStage* family). The accessors are header-inline (GEBitmap.cpp emits them).
//
// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10032bda
// SYNTHETIC: LLE 0x10032be4
// SYNTHETIC: LLE 0x10032bf4
// SYNTHETIC: LLE 0x10032c00
// SYNTHETIC: LLE 0x10032c0a
// SYNTHETIC: LLE 0x10032c14
// SYNTHETIC: LLE 0x10032c24
// SYNTHETIC: LLE 0x10032c30
// SYNTHETIC: LLE 0x10032c3a
// SYNTHETIC: LLE 0x10032c44
// SYNTHETIC: LLE 0x10032c54
// SYNTHETIC: LLE 0x10032c60
// SYNTHETIC: LLE 0x10032c6a
// SYNTHETIC: LLE 0x10032c74
// SYNTHETIC: LLE 0x10032c84
// SYNTHETIC: LLE 0x10032c90
// SYNTHETIC: LLE 0x10032c9a
// SYNTHETIC: LLE 0x10032ca4
// SYNTHETIC: LLE 0x10032cb4
// SYNTHETIC: LLE 0x10032cc0
// SYNTHETIC: LLE 0x10032cca
// SYNTHETIC: LLE 0x10032cd4
// SYNTHETIC: LLE 0x10032ce4
// SYNTHETIC: LLE 0x10032cf0
//
// port (R4.3, eighth part): the texture is a whole Rust object (ee_ge texobj_tex.rs): the constructors' fields, the
// destructor's releases, Load's two frame formats, Recreate, the stage setters over every frame, SetFrame,
// SetSurface and the texture pack's logical shift. This unit is its shell: the forwarders and the host, which makes
// each call the original made on another object (its surfaces, screen, frames, manager, the rasterizer, the file).
#include <windows.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
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
#include "lle/Graphics Engine.h"
#include "rust/ee_ge.h"
#include "rust/ee_openformats.h"

// the Rust mirrors (texobj_tex.rs), field for field
static_assert(sizeof(EeGeTStage) == sizeof(GETStage), "GETStage");
static_assert(offsetof(EeGeTStage, mip_filter_mode) == offsetof(GETStage, mMipFilterMode), "GETStage");
static_assert(offsetof(EeGeTStage, mip_map_bias) == offsetof(GETStage, mMipMapBias), "GETStage");
static_assert(sizeof(EeGeTexCoord) == sizeof(GETextureCoordinate), "GETextureCoordinate");
static_assert(offsetof(EeGeTexCoord, v) == offsetof(GETextureCoordinate, mV), "GETextureCoordinate");
static_assert(sizeof(EeGeSurfaceDescription) == sizeof(GESurfaceDescription), "GESurfaceDescription");

// port (D9): stores one decoded frame (top row first, R, G, B(, A)) into a surface as the original's Targa reader
// (GESurface::LoadTGA, removed) stored a texture file's frame (the pixel loop is Rust: ee_ge pixel.rs, store_frame):
// 32-bit surfaces get A8R8G8B8 whatever their format, 16-bit ones GEPackColor of their format; no alpha (3 channels,
// or a surface without alpha) is 0xff. Each row continues after the surface's row padding plus the surface's pixel
// width minus the frame's (LoadTGA's skip, computed from the surface's pixel width); a frame larger than the surface
// is refused.
static long GEStoreFrame(GESurface* inSurface, const EeOfTextureFrame& inFrame)
{
    EeGeStoreTarget theTarget;
    theTarget.pixel_depth = inSurface->GetPixelDepth();
    theTarget.pixel_format = inSurface->GetPixelFormat();
    theTarget.no_alpha = inSurface->GetSurfaceDescription().mNoAlpha;
    theTarget.pixel_width = inSurface->GetPixelWidth();
    theTarget.pixel_height = inSurface->GetPixelHeight();
    if (!ee_ge_texture_frame_fits(&theTarget, inFrame.width, inFrame.height))
        return kErrorFailed;
    long theResult = inSurface->Lock();
    if (theResult != kErrorOK)
        return theResult;
    theTarget.bytes_per_row = inSurface->GetBytesPerRow();
    ee_ge_texture_store_frame(&theTarget, inFrame.width, inFrame.height, inFrame.channels, inFrame.pixels,
                              inSurface->GetMemoryAddress());
    return inSurface->Unlock();
}

static GERectangle GERectangleOf(const float* inRect)
{
    GERectangle theRectangle;
    theRectangle.SetLeft(inRect[0]);
    theRectangle.SetTop(inRect[1]);
    theRectangle.SetRight(inRect[2]);
    theRectangle.SetBottom(inRect[3]);
    return theRectangle;
}

GERasterizer* GETexture::sRasterizer;

// The calls the texture makes on other objects (EeGeTexOp), in the original's statements
intptr_t GETexture::Host(void* inOwner, EeGeTexOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    GESurface* theSurface = (GESurface*)inObj;
    GETexture* theTexture = (GETexture*)inObj;
    switch (inOp) {
    case EeGeTexOp::SurfReference:
        theSurface->Reference();
        return 0;
    case EeGeTexOp::SurfDereference:
        theSurface->Dereference();
        return 0;
    case EeGeTexOp::SurfParentScreen:
        return (intptr_t)theSurface->GetParentScreen();
    case EeGeTexOp::SurfAllocatedSize:
        return (intptr_t)theSurface->GetAllocatedSize();
    case EeGeTexOp::SurfLogicalShift:
        return (intptr_t)theSurface->GetLogicalShift();
    case EeGeTexOp::SurfSetLogicalShift:
        theSurface->SetLogicalShift((unsigned long)a);
        return 0;
    case EeGeTexOp::SurfLock:
        return theSurface->Lock();
    case EeGeTexOp::SurfUnlock:
        return theSurface->Unlock();
    case EeGeTexOp::SurfMemory:
        return (intptr_t)theSurface->GetMemoryAddress();
    case EeGeTexOp::SurfBytesPerRow:
        return (intptr_t)theSurface->GetBytesPerRow();
    case EeGeTexOp::SurfDescription:
        *(GESurfaceDescription*)a = theSurface->GetSurfaceDescription();
        return 0;
    case EeGeTexOp::SurfAttached:
        return (intptr_t)theSurface->GetAttachedSurface(0);
    case EeGeTexOp::SurfAttachedCount:
        return (intptr_t)theSurface->GetAttachedSurfaceCount();
    case EeGeTexOp::SurfWidth:
        return (intptr_t)theSurface->GetWidth();
    case EeGeTexOp::SurfHeight:
        return (intptr_t)theSurface->GetHeight();
    case EeGeTexOp::SurfCopyWithFilter: {
        const EeGeTexCopy* theCopy = (const EeGeTexCopy*)b;
        GERectangle theSource = GERectangleOf(theCopy->source);
        GERectangle theDestination = GERectangleOf(theCopy->destination);
        return theSurface->CopyWithFilter((GESurface*)a, theSource, theDestination);
    }
    case EeGeTexOp::SurfCopyTo: {
        const EeGeTexCopy* theCopy = (const EeGeTexCopy*)b;
        GERectangle theSource = GERectangleOf(theCopy->source);
        GERectangle theDestination = GERectangleOf(theCopy->destination);
        return theSurface->CopyTo((GESurface*)a, theSource, theDestination, theCopy->transparent);
    }
    case EeGeTexOp::ScreenCreateSurface:
        return ((GEScreen*)inObj)->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b);
    case EeGeTexOp::FileInfo: {
        EeOfTextureInfo theInfo;
        ee_of_texture_info((const EeOfTexture*)inObj, &theInfo);
        uint32_t* theOut = (uint32_t*)a;
        theOut[0] = theInfo.width;
        theOut[1] = theInfo.height;
        theOut[2] = theInfo.frame_count;
        return 0;
    }
    case EeGeTexOp::StoreFrame: {
        EeOfTextureFrame theFrame;
        ee_of_texture_frame((const EeOfTexture*)a, (uint32_t)b, &theFrame);
        return GEStoreFrame(theSurface, theFrame);
    }
    case EeGeTexOp::TexNew:
        return (intptr_t)new GETexture((GETextureManager*)inObj);
    case EeGeTexOp::TexObj:
        return (intptr_t)theTexture->mRust;
    case EeGeTexOp::TexDereference:
        theTexture->Dereference();
        return 0;
    case EeGeTexOp::TexBaseDereference:
        theTexture->UReferenceCounter::Dereference();
        return 0;
    case EeGeTexOp::TexRefCount:
        return (intptr_t)theTexture->mReferenceCount;
    case EeGeTexOp::ManagerRemove:
        return ((GETextureManager*)inObj)->RemoveTexture((GETexture*)a);
    case EeGeTexOp::ManagerPromote:
        return ((GETextureManager*)inObj)->PromoteTexture((GETexture*)a);
    case EeGeTexOp::RasterCreate:
        return GETexture::sRasterizer->CreateTextureHandle(theTexture);
    case EeGeTexOp::RasterDestroy:
        GETexture::sRasterizer->DestroyTextureHandle(theTexture);
        return 0;
    default:
        return 0;
    }
}

// FUNCTION: LLE 0x10032cfa
// Creates an empty single-frame texture for a texture manager, with point filtering and clamped addressing
GETexture::GETexture(GETextureManager* inManager)
{
    GETexture::sRasterizer = GERasterizer::GetActiveRasterizer();
    this->mRust = ee_ge_tex_new(&GETexture::Host, this, inManager);
}

// FUNCTION: LLE 0x10032d76
// Creates a texture around an existing surface (referenced) and registers it with the active rasterizer
GETexture::GETexture(GESurface* inSurface, GETextureManager* inManager)
{
    GETexture::sRasterizer = GERasterizer::GetActiveRasterizer();
    this->mRust = ee_ge_tex_new(&GETexture::Host, this, inManager);
    ee_ge_tex_construct_on_surface(this->mRust, inSurface);
}

// FUNCTION: LLE 0x10032e11
// Destroys the texture, releasing its extra animation frames, its rasterizer handle and its surface
GETexture::~GETexture()
{
    ee_ge_tex_destroy(this->mRust);
}

// FUNCTION: LLE 0x10032e7c
// Drops a reference; at the last outside reference a managed texture removes itself from its manager
void GETexture::Dereference()
{
    ee_ge_tex_dereference(this->mRust);
}

// FUNCTION: LLE 0x10032eab
// Returns the surface's allocated size, times the frame count when each frame has its own texture
unsigned long GETexture::GetAllocatedSize()
{
    return ee_ge_tex_allocated_size(this->mRust);
}

// FUNCTION: LLE 0x10032ec4
// Loads the texture (and its animation frames) from its decoded PNG frames (port, D9: the original read a texture or
// .tga file), building or dropping MIP levels as asked; frames become separate textures (format 0) or cells of one
// grid surface (format 1)
long GETexture::Load(const EeOfTexture* inTexture, GESurfaceDescription& inDescription, GEScreen* inScreen,
                     bool inCreateMIPMaps, long inNumMipLevelsToDrop, unsigned long inFrameFormat)
{
    return ee_ge_tex_load(this->mRust, inTexture, (EeGeSurfaceDescription*)&inDescription, inScreen, inCreateMIPMaps,
                          inNumMipLevelsToDrop, inFrameFormat);
}

// FUNCTION: LLE 0x100336c9
// Locks the texture's surface and returns its memory and row pitch
long GETexture::Lock(unsigned char*& outMemory, unsigned long& outBytesPerRow)
{
    return ee_ge_tex_lock(this->mRust, &outMemory, &outBytesPerRow);
}

// FUNCTION: LLE 0x100336fe
// Recreates the texture's surface on another screen with the same description
long GETexture::Recreate(GEScreen* inScreen)
{
    return ee_ge_tex_recreate(this->mRust, inScreen);
}

// port: see the header
void GETexture::SetLogicalShift(unsigned long inShift)
{
    ee_ge_tex_set_logical_shift(this->mRust, inShift);
}

// FUNCTION: LLE 0x10033749 (Save: wrote the texture file header and every frame's surface, a .sst; retired under
// D9, the textures are PNG files)

// FUNCTION: LLE 0x1003381c
// Sets the alpha blend mode on every frame (or on the texture), marking changed ones
void GETexture::SetStageAlphaMode(unsigned long inMode, unsigned long inStage)
{
    ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::AlphaMode, inMode);
}

// FUNCTION: LLE 0x10033861
void GETexture::SetStageColorMode(unsigned long inMode, unsigned long inStage)
{
    ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::ColorMode, inMode);
}

// FUNCTION: LLE 0x100338a6
void GETexture::SetStageMagFilterMode(unsigned long inMode, unsigned long inStage)
{
    ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::MagFilterMode, inMode);
}

// FUNCTION: LLE 0x100338eb
void GETexture::SetStageMinFilterMode(unsigned long inMode, unsigned long inStage)
{
    ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::MinFilterMode, inMode);
}

// FUNCTION: LLE 0x10033930
// (port: a texture pack's texture keeps MIP filtering where none is asked for: texobj_tex.rs)
void GETexture::SetStageMipFilterMode(unsigned long inMode, unsigned long inStage)
{
    ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::MipFilterMode, inMode);
}

// FUNCTION: LLE 0x10033975
void GETexture::SetStageMipMapBias(float inBias, unsigned long inStage)
{
    ee_ge_tex_set_stage_bias(this->mRust, inBias);
}

// FUNCTION: LLE 0x100339c8
void GETexture::SetStageTextureAddressingMode(unsigned long inMode, unsigned long inStage)
{
    ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::TextureAddressingMode, inMode);
}

// FUNCTION: LLE 0x10033a52
// Shows the given animation frame: picks the frame texture, or moves the UV corner to its grid cell
void GETexture::SetFrame(unsigned long inFrame)
{
    ee_ge_tex_set_frame(this->mRust, inFrame);
}

// FUNCTION: LLE 0x10033ab7
// Replaces the texture's surface (new rasterizer handle, promotion cleared); already-exists error if unchanged
long GETexture::SetSurface(GESurface* inSurface)
{
    return ee_ge_tex_set_surface(this->mRust, inSurface);
}

// FUNCTION: LLE 0x10033b17
// Unlocks the texture's surface
long GETexture::Unlock()
{
    return ee_ge_tex_unlock(this->mRust);
}
