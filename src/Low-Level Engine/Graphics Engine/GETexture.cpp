// C:\EEX\Low-Level Engine\Graphics Engine\GETexture.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10032bda..0x10033b1f: the U2D key init chains (U2DPoint.h, U2DRectangle.h; no guard byte), then the GETexture
// members: the two private ctors, the dtor, then alphabetical (Demote .. Unlock; SetFrame/SetSurface/Unlock after
// the SetStage* family). The accessors are header-inline (GEBitmap.cpp emits them).
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
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
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
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "rust/base/png.h"
#include "rust/render/ge.h"
#include "rust/render/openformats.h"
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "GEOpenFormats.h"   // the data path's host for ee_of_find_texture (a texture's PNG frames)
#include "lle/GETextureSet.h"
// the Rust mirrors (texobj_tex.rs), field for field
static_assert(sizeof(EeGeTexCoord) == sizeof(GETextureCoordinate), "GETextureCoordinate");
static_assert(offsetof(EeGeTexCoord, v) == offsetof(GETextureCoordinate, mV), "GETextureCoordinate");

GERasterizer* GETexture::sRasterizer;

// The calls the texture makes on other objects (EeGeTexOp), in the original's statements
intptr_t GETexture::Host(void* inOwner, EeGeTexOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    case EeGeTexOp::SurfReference:
        ((GESurface*)inObj)->Reference();
        return 0;
    case EeGeTexOp::SurfDereference:
        ((GESurface*)inObj)->Dereference();
        return 0;
    case EeGeTexOp::SurfParentScreen:
        return (intptr_t)((GESurface*)inObj)->GetParentScreen();
    case EeGeTexOp::SurfAllocatedSize:
        return (intptr_t)((GESurface*)inObj)->GetAllocatedSize();
    case EeGeTexOp::SurfLogicalShift:
        return (intptr_t)((GESurface*)inObj)->GetLogicalShift();
    case EeGeTexOp::SurfSetLogicalShift:
        ((GESurface*)inObj)->SetLogicalShift((unsigned long)a);
        return 0;
    case EeGeTexOp::SurfLock:
        return ((GESurface*)inObj)->Lock();
    case EeGeTexOp::SurfUnlock:
        return ((GESurface*)inObj)->Unlock();
    case EeGeTexOp::SurfMemory:
        return (intptr_t)((GESurface*)inObj)->GetMemoryAddress();
    case EeGeTexOp::SurfBytesPerRow:
        return (intptr_t)((GESurface*)inObj)->GetBytesPerRow();
    case EeGeTexOp::SurfDescription:
        *(GESurfaceDescription*)a = ((GESurface*)inObj)->GetSurfaceDescription();
        return 0;
    case EeGeTexOp::SurfAttached:
        return (intptr_t)((GESurface*)inObj)->GetAttachedSurface(0);
    case EeGeTexOp::SurfAttachedCount:
        return (intptr_t)((GESurface*)inObj)->GetAttachedSurfaceCount();
    case EeGeTexOp::SurfWidth:
        return (intptr_t)((GESurface*)inObj)->GetWidth();
    case EeGeTexOp::SurfHeight:
        return (intptr_t)((GESurface*)inObj)->GetHeight();
    case EeGeTexOp::SurfCopyWithFilter:   // b: the source and destination (EeGeTexCopy)
        return ((GESurface*)inObj)->CopyWithFilter((GESurface*)a, ((GERectangle*)b)[0], ((GERectangle*)b)[1]);
    case EeGeTexOp::SurfCopyTo:
        return ((GESurface*)inObj)->CopyTo((GESurface*)a, ((GERectangle*)b)[0], ((GERectangle*)b)[1], c != 0);
    case EeGeTexOp::ScreenCreateSurface:
        return ((GEScreen*)inObj)->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b);
    case EeGeTexOp::FileInfo:
        ee_of_texture_info((const EeOfTexture*)inObj, (EeOfTextureInfo*)a);
        return 0;
    case EeGeTexOp::FileFrame:
        ee_of_texture_frame((const EeOfTexture*)inObj, (uint32_t)a, (EeOfTextureFrame*)b);
        return 0;
    case EeGeTexOp::SurfPixelFormat: return (intptr_t)((GESurface*)inObj)->GetPixelFormat();
    case EeGeTexOp::SurfPixelDepth: return (intptr_t)((GESurface*)inObj)->GetPixelDepth();
    case EeGeTexOp::SurfVolatility: return (intptr_t)((GESurface*)inObj)->GetSurfaceVolatility();
    case EeGeTexOp::SurfIs3DRenderable: return ((GESurface*)inObj)->GetIs3DRenderable();
    case EeGeTexOp::SurfIsTransparencyEnabled: return ((GESurface*)inObj)->GetIsTransparencyEnabled();
    case EeGeTexOp::TexNew:
        return (intptr_t)new GETexture((GETextureManager*)inObj);
    case EeGeTexOp::TexBaseDereference:
        ((GETexture*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeTexOp::ManagerRemove:
        return ((GETextureManager*)inObj)->RemoveTexture((GETexture*)a);
    case EeGeTexOp::ManagerPromote:
        return ((GETextureManager*)inObj)->PromoteTexture((GETexture*)a);
    case EeGeTexOp::RasterCreate:
        return GETexture::sRasterizer->CreateTextureHandle(((GETexture*)inObj));
    case EeGeTexOp::RasterDestroy:
        GETexture::sRasterizer->DestroyTextureHandle(((GETexture*)inObj));
        return 0;
    default:
        return 0;
    }
}

// Creates an empty single-frame texture for a texture manager, with point filtering and clamped addressing
GETexture::GETexture(GETextureManager* inManager)
{
    GETexture::sRasterizer = GERasterizer::GetActiveRasterizer();
    this->mRust = ee_ge_tex_new(&GETexture::Host, this, inManager);
}

// Creates a texture around an existing surface (referenced) and registers it with the active rasterizer
GETexture::GETexture(GESurface* inSurface, GETextureManager* inManager)
{
    GETexture::sRasterizer = GERasterizer::GetActiveRasterizer();
    this->mRust = ee_ge_tex_new(&GETexture::Host, this, inManager);
    ee_ge_tex_construct_on_surface(this->mRust, inSurface);
}

// Destroys the texture, releasing its extra animation frames, its rasterizer handle and its surface
GETexture::~GETexture()
{
    ee_ge_tex_destroy(this->mRust);
}

// (the SetStage* setters: inline in lle/GETexture.h)

// ---- GETextureManager (GETextureManager.cpp, folded here in R4.3 part 16)

// the Rust mirrors of the descriptions (texobj.rs), field for field
static_assert(sizeof(EeGeTextureDescription) == sizeof(GETextureDescription), "GETextureDescription");
static_assert(offsetof(EeGeTextureDescription, num_mip_levels_to_drop) == offsetof(GETextureDescription, mNumMipLevelsToDrop), "");
static_assert(offsetof(EeGeTextureDescription, use_texture_pack) == offsetof(GETextureDescription, mUseTexturePack), "");


// The calls the manager makes on other objects (EeGeManOp), in the original's statements
intptr_t GETextureManager::Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    case EeGeManOp::LockCreate: return TSScheduler::CreateCriticalSection(*(TSCriticalSection**)a);
    case EeGeManOp::LockVerify: ESVERIFY_AT(a == keseOK, 43); return 0;
    case EeGeManOp::LockDestroy:
        TSScheduler::DestroyCriticalSection((TSCriticalSection*)inObj);
        return 0;
    case EeGeManOp::LockEnter:
        ((TSCriticalSection*)inObj)->Enter();
        return 0;
    case EeGeManOp::LockLeave:
        ((TSCriticalSection*)inObj)->Leave();
        return 0;
    case EeGeManOp::PathReference:
        ((FSPath*)inObj)->Reference();
        return 0;
    case EeGeManOp::PathDereference:
        ((FSPath*)inObj)->Dereference();
        return 0;
    case EeGeManOp::BaseDereference:
        ((GETexture*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeManOp::DumpText: fprintf(*(FSFileSpec*)inObj, "%s", (const char*)a); return 0;
    case EeGeManOp::TexFindFile:
        return ee_of_find_texture(&GEOpenFormatsHost, inObj, (const char*)a, (EeOfTexture**)c);
    case EeGeManOp::TexFreeFile:
        ee_of_texture_free((EeOfTexture*)inObj);
        return 0;
    case EeGeManOp::TexNew:
        return (intptr_t)new GETexture(((GETextureManager*)inOwner));
    case EeGeManOp::TexNewFromSurface:
        return (intptr_t)new GETexture((GESurface*)inObj, ((GETextureManager*)inOwner));
    default:
        return 0;
    }
}


// ---- GETextureSet (GETextureSet.cpp, folded here in R4.3 part 16)

void GETextureSet::Dereference(void* inTexture) { static_cast<GETexture*>(inTexture)->Dereference(); }

// ---- GESurface (GESurface.cpp, folded here in R4.3 part 16)

// The calls the image members make on other objects (EeOfSurfOp), in the original's statements
static intptr_t GESurfaceHost(EeOfSurfOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    unsigned long theWritten;
    switch (inOp) {
    case EeOfSurfOp::Format: return (intptr_t)((GESurface*)inObj)->GetPixelFormat();
    case EeOfSurfOp::Depth: return (intptr_t)((GESurface*)inObj)->GetPixelDepth();
    case EeOfSurfOp::BytesPerRow: return (intptr_t)((GESurface*)inObj)->GetBytesPerRow();
    case EeOfSurfOp::Memory: return (intptr_t)((GESurface*)inObj)->GetMemoryAddress();
    case EeOfSurfOp::PixelWidth: return (intptr_t)((GESurface*)inObj)->GetPixelWidth();
    case EeOfSurfOp::PixelHeight: return (intptr_t)((GESurface*)inObj)->GetPixelHeight();
    case EeOfSurfOp::NoAlpha: return ((GESurface*)inObj)->GetSurfaceDescription().mNoAlpha;
    case EeOfSurfOp::Lock: return ((GESurface*)inObj)->Lock();
    case EeOfSurfOp::Unlock: return ((GESurface*)inObj)->Unlock();
    case EeOfSurfOp::FileIsOpen: return ((FSFileSpec*)inObj)->GetIsOpen();
    case EeOfSurfOp::FileAccess: return (intptr_t)((FSFileSpec*)inObj)->GetFileAccess();
    case EeOfSurfOp::FileOpen: return ((FSFileSpec*)inObj)->Open((unsigned long)a);
    case EeOfSurfOp::FileClose: return ((FSFileSpec*)inObj)->Close();
    case EeOfSurfOp::FileFOpen: return ((FSFileSpec*)inObj)->FOpen((char*)a);
    case EeOfSurfOp::FileStream: return (intptr_t)(FILE*)*((FSFileSpec*)inObj);
    case EeOfSurfOp::FilePosition: return ((FSFileSpec*)inObj)->GetPosition(*(long*)a);
    case EeOfSurfOp::FileSetPosition: return ((FSFileSpec*)inObj)->SetPosition((long)a, (unsigned long)b, c != 0);
    case EeOfSurfOp::FileWrite: return ((FSFileSpec*)inObj)->Write((const unsigned char*)a, (unsigned long)b, theWritten);
    case EeOfSurfOp::FileWriteU16: return ((FSFileSpec*)inObj)->Write((unsigned short)a);
    case EeOfSurfOp::FileWriteLong: return ((FSFileSpec*)inObj)->Write((long)a);
    case EeOfSurfOp::FileRead: return ((FSFileSpec*)inObj)->Read((unsigned char*)a, (unsigned long)b, *(unsigned long*)c);
    case EeOfSurfOp::FileLength: return ((FSFileSpec*)inObj)->GetLength(*(unsigned long*)a);
    }
    return 0;
}

// Reads an image file's width, height and depth by file type (BMP: unsupported, JPG, JPG2, PNG)
long GESurface::GetImageFileAttributes(FSFileSpec& inFile, unsigned char inType, unsigned long& outWidth,
                                       unsigned long& outHeight, unsigned long& outDepth)
{
    return ee_of_surf_attributes(&GESurfaceHost, &inFile, inType, &outWidth, &outHeight, &outDepth);
}

// Loads an image file into the surface by file type (BMP: unsupported, JPG, JPG2, PNG; TGA: no longer read, D9)
long GESurface::Load(FSFileSpec& inFile, unsigned char inType)
{
    return ee_of_surf_load(&GESurfaceHost, this, &inFile, inType);
}

// Saves the surface as an image file of the given type (BMP, JPG, JPG2)
long GESurface::Save(FSFileSpec& inFile, unsigned char inType)
{
    return ee_of_surf_save(&GESurfaceHost, this, &inFile, inType);
}
