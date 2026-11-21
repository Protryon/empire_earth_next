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
    case EeGeTexOp::TexBaseDereference:
        theTexture->UReferenceCounter::Dereference();
        return 0;
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


// The pack's name of a texture: "textures\x" -> "textures-4x\x"; false for a name outside "textures\" (or no pack
// directory), for the pictures the game reads as data, and while the pack is off
bool GETextureManager::GetTexturePackName(const UString& inName, UString& outName)
{
    char theBuffer[1024];
    if (!ee_ge_texpack_name(inName, true, theBuffer, sizeof(theBuffer)))
        return false;
    outName = UString(theBuffer);
    return true;
}



// The calls the manager makes on other objects (EeGeManOp), in the original's statements
intptr_t GETextureManager::Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    GETextureManager* theManager = (GETextureManager*)inOwner;
    GETexture* theTexture = (GETexture*)inObj;
    switch (inOp) {
    case EeGeManOp::LockCreate: {
        TSCriticalSection* theLock = NULL;
        ESVERIFY_AT(TSScheduler::CreateCriticalSection(theLock) == keseOK, 43);
        return (intptr_t)theLock;
    }
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
        theTexture->UReferenceCounter::Dereference();
        return 0;
    case EeGeManOp::DumpHeader:
        fprintf(*(FSFileSpec*)inObj, "\n\n*********Texture Manager - Current Loaded Files In Memory*********\n\n");
        return 0;
    case EeGeManOp::DumpLine:
        fprintf(*(FSFileSpec*)b, "The %s texture file is loaded in memory with a reference count of %ld.\n",
                (const char*)a, theTexture->mReferenceCount);
        return 0;
    case EeGeManOp::TexFindFile:
        return ee_of_find_texture(&GEOpenFormatsHost, inObj, (const char*)a, (EeOfTexture**)c);
    case EeGeManOp::TexFreeFile:
        ee_of_texture_free((EeOfTexture*)inObj);
        return 0;
    case EeGeManOp::TexNew:
        return (intptr_t)new GETexture(theManager);
    case EeGeManOp::TexNewFromSurface:
        return (intptr_t)new GETexture((GESurface*)inObj, theManager);
    case EeGeManOp::TexSurfaceDescription: {
        GESurfaceDescription* theDescription = (GESurfaceDescription*)a;
        GESurface* theSurface = theTexture->GetSurface();
        theDescription->mPixelFormat = theSurface->GetPixelFormat();
        theDescription->mPixelDepth = theSurface->GetPixelDepth();
        theDescription->mSurfaceVolatility = theSurface->GetSurfaceVolatility();
        theDescription->mIs3DRenderable = theSurface->GetIs3DRenderable();
        theDescription->mIsTransparencyEnabled = theSurface->GetIsTransparencyEnabled();
        return 0;
    }
    case EeGeManOp::TexPackLog:
        if (getenv("EE_TEXTURE_PACK_LOG"))   // port: the test's evidence (posix_game.py)
            fprintf(stderr, "texture pack: %s logical %lux%lu pixels %lux%lu (%ld MIP levels dropped)\n",
                    (const char*)a, theTexture->GetWidth(), theTexture->GetHeight(),
                    theTexture->GetSurface()->GetPixelWidth(), theTexture->GetSurface()->GetPixelHeight(), (long)b);
        return 0;
    default:
        return 0;
    }
}


// Creates a texture manager on a search path (referenced), with no textures and its own lock
GETextureManager::GETextureManager(FSPath* inTexturePath)
{
    this->mRust = ee_ge_texman_new(&GETextureManager::Host, this, inTexturePath);
}

// Destroys the texture manager, removing every unreferenced texture and releasing the path and lock
GETextureManager::~GETextureManager()
{
    ee_ge_texman_free(this->mRust);
}



// ---- GETextureSet (GETextureSet.cpp, folded here in R4.3 part 16)

void GETextureSet::Dereference(void* inTexture) { static_cast<GETexture*>(inTexture)->Dereference(); }

// ---- GESurface (GESurface.cpp, folded here in R4.3 part 16)

// The calls the image members make on other objects (EeOfSurfOp), in the original's statements
static intptr_t GESurfaceHost(EeOfSurfOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    GESurface* theSurface = (GESurface*)inObj;
    FSFileSpec* theFile = (FSFileSpec*)inObj;
    unsigned long theWritten;
    switch (inOp) {
    case EeOfSurfOp::Format: return (intptr_t)theSurface->GetPixelFormat();
    case EeOfSurfOp::Depth: return (intptr_t)theSurface->GetPixelDepth();
    case EeOfSurfOp::BytesPerRow: return (intptr_t)theSurface->GetBytesPerRow();
    case EeOfSurfOp::Memory: return (intptr_t)theSurface->GetMemoryAddress();
    case EeOfSurfOp::PixelWidth: return (intptr_t)theSurface->GetPixelWidth();
    case EeOfSurfOp::PixelHeight: return (intptr_t)theSurface->GetPixelHeight();
    case EeOfSurfOp::NoAlpha: return theSurface->GetSurfaceDescription().mNoAlpha;
    case EeOfSurfOp::Lock: return theSurface->Lock();
    case EeOfSurfOp::Unlock: return theSurface->Unlock();
    case EeOfSurfOp::FileIsOpen: return theFile->GetIsOpen();
    case EeOfSurfOp::FileAccess: return (intptr_t)theFile->GetFileAccess();
    case EeOfSurfOp::FileOpen: return theFile->Open((unsigned long)a);
    case EeOfSurfOp::FileClose: return theFile->Close();
    case EeOfSurfOp::FileFOpen: return theFile->FOpen((char*)a);
    case EeOfSurfOp::FileStream: return (intptr_t)(FILE*)*theFile;
    case EeOfSurfOp::FilePosition: return theFile->GetPosition(*(long*)a);
    case EeOfSurfOp::FileSetPosition: return theFile->SetPosition((long)a, (unsigned long)b, c != 0);
    case EeOfSurfOp::FileWrite: return theFile->Write((const unsigned char*)a, (unsigned long)b, theWritten);
    case EeOfSurfOp::FileWriteU16: return theFile->Write((unsigned short)a);
    case EeOfSurfOp::FileWriteLong: return theFile->Write((long)a);
    case EeOfSurfOp::FileRead: return theFile->Read((unsigned char*)a, (unsigned long)b, *(unsigned long*)c);
    case EeOfSurfOp::FileLength: return theFile->GetLength(*(unsigned long*)a);
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
