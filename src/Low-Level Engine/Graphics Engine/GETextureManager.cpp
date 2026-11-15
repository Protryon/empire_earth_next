// C:\EEX\Low-Level Engine\Graphics Engine\GETextureManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10033bef..0x100346a7: GETextureManager's members (ctor first, then alphabetical), then the
// map<UString, GETexture*> operator[] and dtor COMDATs. No init chains. GEModelManager.cpp is its twin.
// The ESVERIFY is the original's line 43 (0x2b).
//
// port (R4.3, seventh part): the manager is a whole Rust object (ee_ge texobj.rs): the cache, the path, the lock, the
// screen and the footprint with every method, the cache name's stem, the texture pack's file first and the surface
// description a texture description asks for. This unit is its shell: the forwarders and the host, which makes each
// call the original made on another object (the lock, the path, the textures, the file lookup, the dump's lines).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <map>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/TSScheduler.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GERenderState.h"
#include "GEOpenFormats.h"   // GEFindTextureFile: a texture's PNG frames
#include "rust/ee_ge.h"       // the manager (ee_ge texobj.rs), the texture pack (managers.rs)
#include "rust/ee_openformats.h"   // ee_of_texture_free

// the Rust mirrors of the descriptions (texobj.rs), field for field
static_assert(sizeof(EeGeSurfaceDescription) == sizeof(GESurfaceDescription), "GESurfaceDescription");
static_assert(offsetof(EeGeSurfaceDescription, mip_level_count) == offsetof(GESurfaceDescription, mMipLevelCount), "");
static_assert(offsetof(EeGeSurfaceDescription, pixel_depth) == offsetof(GESurfaceDescription, mPixelDepth), "");
static_assert(offsetof(EeGeSurfaceDescription, no_alpha) == offsetof(GESurfaceDescription, mNoAlpha), "");
static_assert(sizeof(EeGeTextureDescription) == sizeof(GETextureDescription), "GETextureDescription");
static_assert(offsetof(EeGeTextureDescription, num_mip_levels_to_drop) == offsetof(GETextureDescription, mNumMipLevelsToDrop), "");
static_assert(offsetof(EeGeTextureDescription, use_texture_pack) == offsetof(GETextureDescription, mUseTexturePack), "");

// port: the texture pack (SetTexturePack): its settings live in Rust (ee_ge managers.rs, TexturePack). The directory
// stays set when the pack is turned off, so a texture loaded from it is reloaded from it (RefreshAllTextures).
void GETextureManager::SetTexturePack(const char* inDirectory, long inScaleShift, long inMipLevelsToDrop)
{
    ee_ge_texpack_set(inDirectory, inScaleShift, inMipLevelsToDrop);
}

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

long GETextureManager::GetTexturePackLogicalShift()
{
    return ee_ge_texpack_logical_shift();
}

long GETextureManager::GetTexturePackMipLevelsToDrop()
{
    return ee_ge_texpack_mip_levels_to_drop();
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
    case EeGeManOp::RefCount:
        return (intptr_t)theTexture->mReferenceCount;
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
        return GEFindTextureFile((FSPath*)inObj, UString((const char*)a), *(EeOfTexture**)c);
    case EeGeManOp::TexFreeFile:
        ee_of_texture_free((EeOfTexture*)inObj);
        return 0;
    case EeGeManOp::TexNew:
        return (intptr_t)new GETexture(theManager);
    case EeGeManOp::TexNewFromSurface:
        return (intptr_t)new GETexture((GESurface*)inObj, theManager);
    case EeGeManOp::TexLoad: {
        const EeGeTexLoad* theLoad = (const EeGeTexLoad*)a;
        return theTexture->Load((const EeOfTexture*)theLoad->file, *(GESurfaceDescription*)theLoad->description,
                                (GEScreen*)b, theLoad->create_mipmaps, theLoad->num_mip_levels_to_drop,
                                theLoad->frame_format);
    }
    case EeGeManOp::TexGet:
        switch ((EeGeTexField)a) {
        case EeGeTexField::FromPack:
            return theTexture->GetIsFromTexturePack();
        case EeGeTexField::PackMipLevels:
            return theTexture->GetTexturePackMipLevels();
        case EeGeTexField::NumMipLevelsToDrop:
            return (intptr_t)theTexture->GetNumMipLevelsToDrop();
        case EeGeTexField::CreateMipMaps:
            return theTexture->GetCreateMIPMaps();
        case EeGeTexField::FrameFormat:
            return (intptr_t)theTexture->GetFrameFormat();
        case EeGeTexField::FrameCount:
            return (intptr_t)theTexture->GetFrameCount();
        default:
            return 0;
        }
    case EeGeManOp::TexSet:
        switch ((EeGeTexField)a) {
        case EeGeTexField::FromPack:
            theTexture->SetIsFromTexturePack(b != 0);
            break;
        case EeGeTexField::PackMipLevels:
            theTexture->SetTexturePackMipLevels((long)b);
            break;
        case EeGeTexField::NumMipLevelsToDrop:
            theTexture->SetNumMipLevelsToDrop((unsigned long)b);
            break;
        case EeGeTexField::IsManaged:
            theTexture->SetIsManaged(b != 0);
            break;
        case EeGeTexField::Promoted:
            theTexture->SetIsPromoted(b != 0);
            break;
        default:
            break;
        }
        return 0;
    case EeGeManOp::TexSetLogicalShift:
        theTexture->SetLogicalShift((unsigned long)a);
        return 0;
    case EeGeManOp::TexStage: {
        const GETStage* theStage = theTexture->GetStage();
        switch ((EeGeTexStage)a) {
        case EeGeTexStage::AlphaMode:
            return (intptr_t)theStage->mAlphaMode;
        case EeGeTexStage::ColorMode:
            return (intptr_t)theStage->mColorMode;
        case EeGeTexStage::MagFilterMode:
            return (intptr_t)theStage->mMagFilterMode;
        case EeGeTexStage::MinFilterMode:
            return (intptr_t)theStage->mMinFilterMode;
        case EeGeTexStage::MipFilterMode:
            return (intptr_t)theStage->mMipFilterMode;
        case EeGeTexStage::MipMapBias: {
            uint32_t theBits;
            memcpy(&theBits, &theStage->mMipMapBias, sizeof(theBits));
            return (intptr_t)theBits;
        }
        case EeGeTexStage::TextureAddressingMode:
            return (intptr_t)theStage->mTextureAddressingMode;
        default:
            return 0;
        }
    }
    case EeGeManOp::TexSetStage:
        switch ((EeGeTexStage)a) {
        case EeGeTexStage::AlphaMode:
            theTexture->SetStageAlphaMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::ColorMode:
            theTexture->SetStageColorMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::MagFilterMode:
            theTexture->SetStageMagFilterMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::MinFilterMode:
            theTexture->SetStageMinFilterMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::MipFilterMode:
            theTexture->SetStageMipFilterMode((unsigned long)b, -1);
            break;
        case EeGeTexStage::MipMapBias: {
            uint32_t theBits = (uint32_t)b;
            float theBias;
            memcpy(&theBias, &theBits, sizeof(theBias));
            theTexture->SetStageMipMapBias(theBias, -1);
            break;
        }
        case EeGeTexStage::TextureAddressingMode:
            theTexture->SetStageTextureAddressingMode((unsigned long)b, -1);
            break;
        default:
            break;
        }
        return 0;
    case EeGeManOp::TexFrame:
        return (intptr_t)theTexture->GetFrameTexture((unsigned long)a);
    case EeGeManOp::TexReference:
        theTexture->Reference();
        return 0;
    case EeGeManOp::TexDereference:
        theTexture->Dereference();
        return 0;
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
    case EeGeManOp::TexRecreate:
        return theTexture->Recreate((GEScreen*)a);
    case EeGeManOp::TexAllocatedSize:
        return (intptr_t)theTexture->GetAllocatedSize();
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

// port: the cache's texture at an index, in the cache's (name) order: the store gate digests every cached texture
GETexture* GETextureManager::GetTextureAt(unsigned long inIndex)
{
    return (GETexture*)ee_ge_texman_at(this->mRust, inIndex);
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

// Prints every loaded texture file and its reference count to a dump file
void GETextureManager::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    ee_ge_texman_dump(this->mRust, &inFile);
}

unsigned long GETextureManager::GetMemoryFootprint()
{
    return ee_ge_texman_footprint(this->mRust);
}

GEScreen* GETextureManager::GetScreen() const
{
    return (GEScreen*)ee_ge_texman_screen(this->mRust);
}

unsigned long GETextureManager::GetTextureCount()
{
    return ee_ge_texman_count(this->mRust);
}

FSPath* GETextureManager::GetTexturePath()
{
    return (FSPath*)ee_ge_texman_path(this->mRust);
}

void GETextureManager::SetScreen(GEScreen* inScreen)
{
    ee_ge_texman_set_screen(this->mRust, inScreen);
}

// Returns the named texture, loading and caching it from the search path if needed (port, D9: its PNG frames, the
// name's stem; the original tried <name>.tga, then <name>.sst); NULL if it cannot be found or loaded
GETexture* GETextureManager::LoadTexture(const UString& inName, const GETextureDescription& inDescription)
{
    return (GETexture*)ee_ge_texman_load(this->mRust, inName, (const EeGeTextureDescription*)&inDescription);
}

// Wraps a surface in a new managed texture, named by a running counter, with modulate blending
GETexture* GETextureManager::MakeTexture(GESurface* inSurface)
{
    return (GETexture*)ee_ge_texman_make(this->mRust, inSurface);
}

// Marks the texture as promoted; always succeeds
long GETextureManager::PromoteTexture(GETexture* inTexture)
{
    return ee_ge_texman_promote(this->mRust, inTexture);
}

// Reloads an already cached texture from its files (port, D9: the cache name is the PNG files' stem; the original
// looked <name>.tga, then <name>.sst up), updating the memory footprint; 3 when it is not cached
long GETextureManager::ReloadTexture(const UString& inName, const GETextureDescription& inDescription)
{
    return ee_ge_texman_reload(this->mRust, inName, (const EeGeTextureDescription*)&inDescription);
}

// Reloads every cached texture from its files (or recreates its surface when they are gone),
// then reapplies the frames' saved stage settings to the whole texture; returns the last result
long GETextureManager::RefreshAllTextures()
{
    return ee_ge_texman_refresh_all(this->mRust);
}

// Removes and releases every texture nobody else references; returns 5 if any are still in use
long GETextureManager::RemoveAllTextures()
{
    return ee_ge_texman_remove_all(this->mRust);
}

// Removes and releases one texture if nobody else references it; 5 if still in use, 3 if not managed here
long GETextureManager::RemoveTexture(GETexture* inTexture)
{
    return ee_ge_texman_remove(this->mRust, inTexture);
}

// Replaces the texture search path, moving the reference
void GETextureManager::SetTexturePath(FSPath* inTexturePath)
{
    ee_ge_texman_set_path(this->mRust, inTexturePath);
}

// Sets the MIP filter mode of every cached texture that uses MIP filtering
void GETextureManager::UpdateMIPFilterMode(unsigned long inMode)
{
    ee_ge_texman_update_mip_filter(this->mRust, inMode);
}
