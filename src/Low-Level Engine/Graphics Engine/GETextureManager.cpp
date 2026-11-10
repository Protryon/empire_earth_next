// C:\EEX\Low-Level Engine\Graphics Engine\GETextureManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10033bef..0x100346a7: GETextureManager's members (ctor first, then alphabetical), then the
// map<UString, GETexture*> operator[] and dtor COMDATs. No init chains. GEModelManager.cpp is its twin.
// The ESVERIFY is the original's line 43 (0x2b).
//
// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x10034643
// TEMPLATE: LLE 0x1003467c
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>

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
#include "rust/ee_rust.h"       // ee_of_texture_free
static unsigned long sTextureNumber = 1;    // 0x100d9ca8: appended to made textures' names (provisional name)
// port (D9): a texture's cache name, which is also the stem of its PNG files: the name as given, or without its last
// extension when it holds ".sst" (the original cached "<name>.tga"/"<name>.sst" and loaded those files; the stems
// are the files its open-format lookup read in their place). "textures\x" and "textures\x.sst" share one texture,
// as they shared the cached "textures\x.sst" before.
static UString GETextureStem(const UString& inName)
{
    if (inName.FindBackwards(".sst", -1) == inName.GetLength())
        return inName;
    const char* theName = inName;
    const char* theFile = theName;
    for (const char* p = theName; *p; p++)
        if (*p == '\\' || *p == '/')
            theFile = p + 1;
    const char* theDot = strrchr(theFile, '.');
    if (theDot == NULL)
        return inName;
    std::string theStem(theName, theDot - theName);
    return UString(theStem.c_str());
}

// port: the texture pack (SetTexturePack). The directory stays set when the pack is turned off, so a texture loaded
// from it is reloaded from it (RefreshAllTextures).
static char sTexturePackDirectory[64];      // "textures-4x\\" (with its separator), "" before the first SetTexturePack
static bool sTexturePackEnabled = false;
static long sTexturePackScaleShift = 0;      // the pack's files are 1 << this times the originals' size
static long sTexturePackMipLevelsToDrop = 0;

void GETextureManager::SetTexturePack(const char* inDirectory, long inScaleShift, long inMipLevelsToDrop)
{
    sTexturePackEnabled = inDirectory != NULL && inDirectory[0] != '\0'
                          && strlen(inDirectory) + 2 <= sizeof(sTexturePackDirectory);
    if (sTexturePackEnabled)
        sprintf(sTexturePackDirectory, "%s\\", inDirectory);
    sTexturePackScaleShift = inScaleShift > 0 ? inScaleShift : 0;
    sTexturePackMipLevelsToDrop = inMipLevelsToDrop > 0 ? inMipLevelsToDrop : 0;
    if (sTexturePackMipLevelsToDrop > sTexturePackScaleShift)
        sTexturePackMipLevelsToDrop = sTexturePackScaleShift;
}

// The pack's name of a texture: "textures\x" -> "textures-4x\x"; false for a name outside "textures\"
// (or no pack directory), and for the pictures the game reads as data (their pixels are values, not art; they are
// read through GEFindTargaFile, never the pack, but are refused here too)
static bool GETexturePackName(const UString& inName, UString& outName)
{
    static const char kTextures[] = "textures\\";
    static const char* const kDataPictures[] = {"colorpalette", "waterpalette", "volcano"};
    const char* theName = inName;
    if (sTexturePackDirectory[0] == '\0' || strlen(theName) <= sizeof(kTextures) - 1
        || PlatStrNICmp(theName, kTextures, sizeof(kTextures) - 1) != 0)
        return false;
    const char* theFile = theName + sizeof(kTextures) - 1;
    for (size_t i = 0; i < sizeof(kDataPictures) / sizeof(kDataPictures[0]); i++) {
        size_t theLength = strlen(kDataPictures[i]);
        if (PlatStrNICmp(theFile, kDataPictures[i], theLength) == 0
            && (theFile[theLength] == '\0' || theFile[theLength] == '.'))
            return false;
    }
    outName = UString(sTexturePackDirectory) + UString(theFile);
    return true;
}

bool GETextureManager::GetTexturePackName(const UString& inName, UString& outName)
{
    return sTexturePackEnabled && GETexturePackName(inName, outName);
}

long GETextureManager::GetTexturePackLogicalShift()
{
    return sTexturePackScaleShift - sTexturePackMipLevelsToDrop;
}

long GETextureManager::GetTexturePackMipLevelsToDrop()
{
    return sTexturePackMipLevelsToDrop;
}

// A texture just loaded from the pack with inMipLevels more dropped than the caller asked: its two sizes (the
// logical one is the original's, after the caller's drop) and the caller's drop as its GetNumMipLevelsToDrop (the
// 2D code computes the original's size as GetWidth() << GetNumMipLevelsToDrop())
void GETextureManager::SetTexturePackSizes(GETexture* inTexture, long inMipLevels)
{
    inTexture->mIsFromTexturePack = true;
    inTexture->mTexturePackMipLevels = inMipLevels;
    inTexture->mNumMipLevelsToDrop = (long)inTexture->mNumMipLevelsToDrop > inMipLevels
                                         ? inTexture->mNumMipLevelsToDrop - inMipLevels : 0;
    long theShift = sTexturePackScaleShift - inMipLevels;
    inTexture->SetLogicalShift(theShift > 0 ? (unsigned long)theShift : 0);
}

// GEFindTextureFile for a cached texture's name: in the pack when it was loaded from there (it falls back to the
// original when the pack's file is gone)
static long GEFindCachedTextureFile(FSPath* inPath, const UString& inName, bool inFromPack, EeOfTexture*& outTexture)
{
    UString thePackName;
    if (inFromPack && GETexturePackName(inName, thePackName)
        && GEFindTextureFile(inPath, thePackName, outTexture) == kErrorOK)
        return kErrorOK;
    return GEFindTextureFile(inPath, inName, outTexture);
}

//
// FUNCTION: LLE 0x10033bef
// Creates a texture manager on a search path (referenced), with no textures and its own lock
GETextureManager::GETextureManager(FSPath* inTexturePath)
{
    this->mScreen = NULL;
    this->mMemoryFootprint = 0;
    this->mTexturePath = inTexturePath;
    if (inTexturePath)
        inTexturePath->Reference();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mCriticalSection) == keseOK, 43);
}

// FUNCTION: LLE 0x10033c43
// Destroys the texture manager, removing every unreferenced texture and releasing the path and lock
GETextureManager::~GETextureManager()
{
    this->RemoveAllTextures();
    if (this->mTexturePath)
        this->mTexturePath->Dereference();
    TSScheduler::DestroyCriticalSection(this->mCriticalSection);
}

// FUNCTION: LLE 0x10033c69
// Prints every loaded texture file and its reference count to a dump file
void GETextureManager::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    std::map<UString, GETexture*>::iterator theIter;
    this->mCriticalSection->Enter();
    fprintf(inFile, "\n\n*********Texture Manager - Current Loaded Files In Memory*********\n\n");
    for (theIter = this->mTextures.begin(); theIter != this->mTextures.end(); theIter++)
        fprintf(inFile, "The %s texture file is loaded in memory with a reference count of %ld.\n",
                (const char*)theIter->first, theIter->second->mReferenceCount);
    this->mCriticalSection->Leave();
}

// FUNCTION: LLE 0x10033cd4
// Returns the named texture, loading and caching it from the search path if needed (port, D9: its PNG frames, the
// name's stem; the original tried <name>.tga, then <name>.sst); NULL if it cannot be found or loaded
GETexture* GETextureManager::LoadTexture(const UString& inName, const GETextureDescription& inDescription)
{
    GESurfaceDescription theSurfaceDescription;
    EeOfTexture* theFile = NULL;
    GETexture* theTexture = NULL;
    long theResult;
    this->mCriticalSection->Enter();
    UString theName = GETextureStem(inName);
    std::map<UString, GETexture*>::iterator theIter = this->mTextures.find(theName);
    if (theIter != this->mTextures.end())
        theTexture = theIter->second;
    if (theTexture == NULL) {
        // port: the texture pack's file first when the description allows it (cached under the original's name)
        bool theFromPack = false;
        UString thePackName;
        if (inDescription.mUseTexturePack && sTexturePackEnabled && GETexturePackName(theName, thePackName))
            theFromPack = GEFindTextureFile(this->mTexturePath, thePackName, theFile) == kErrorOK;
        theResult = theFromPack ? kErrorOK : GEFindTextureFile(this->mTexturePath, theName, theFile);
        if (theResult == kErrorOK) {
            if (inDescription.mPixelDepth == 16) {
                theSurfaceDescription.mPixelDepth = 16;
                theSurfaceDescription.mPixelFormat = inDescription.mHasAlpha ? 2 : 7;
            } else {
                theSurfaceDescription.mPixelFormat = kPixelFormatA8R8G8B8;
                theSurfaceDescription.mNoAlpha = !inDescription.mHasAlpha;
                theSurfaceDescription.mPixelDepth = 32;
            }
            theSurfaceDescription.mSurfaceVolatility = inDescription.mSurfaceVolatility;
            theSurfaceDescription.mIs3DRenderable = inDescription.mIs3DRenderable;
            theSurfaceDescription.mIsTransparencyEnabled = inDescription.mIsTransparencyEnabled;
            theTexture = new GETexture(this);
            // port: the pack's file is drawn smaller by dropping more MIP levels (on top of the caller's); it
            // gets MIP levels when it is bigger than its logical size, so it can be drawn at that size
            long thePackDrop = theFromPack ? sTexturePackMipLevelsToDrop : 0;
            long theDrop = inDescription.mNumMipLevelsToDrop + thePackDrop;
            bool theMIPMaps = inDescription.mCreateMIPMaps || (theFromPack && sTexturePackScaleShift > thePackDrop);
            if (theTexture->Load(theFile, theSurfaceDescription, this->mScreen, theMIPMaps, theDrop,
                                 inDescription.mFrameFormat) == kErrorOK) {
                if (theFromPack)
                    this->SetTexturePackSizes(theTexture, thePackDrop);
                if (getenv("EE_TEXTURE_PACK_LOG") && theFromPack)   // port: the test's evidence (posix_game.py)
                    fprintf(stderr, "texture pack: %s logical %lux%lu pixels %lux%lu (%ld MIP levels dropped)\n",
                            (const char*)theName, theTexture->GetWidth(), theTexture->GetHeight(),
                            theTexture->GetSurface()->GetPixelWidth(), theTexture->GetSurface()->GetPixelHeight(),
                            theDrop);
                this->mTextures[theName] = theTexture;
                this->mMemoryFootprint += theTexture->GetAllocatedSize();
                theTexture->mIsManaged = true;
            } else {
                theTexture->UReferenceCounter::Dereference();
                theTexture = NULL;
            }
            ee_of_texture_free(theFile);
        }
    }
    this->mCriticalSection->Leave();
    return theTexture;
}

// FUNCTION: LLE 0x10033f3f
// Wraps a surface in a new managed texture, named by a running counter, with modulate blending
GETexture* GETextureManager::MakeTexture(GESurface* inSurface)
{
    UString theName;
    GETexture* theTexture;
    this->mCriticalSection->Enter();
    theTexture = new GETexture(inSurface, this);
    sprintf(theName, "GETextureManager::MakeTexture(%ld)", sTextureNumber++);
    this->mTextures[theName] = theTexture;
    theTexture->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture->SetStageColorMode(kStageOpModulate, -1);
    theTexture->SetStageMinFilterMode(1, -1);
    theTexture->SetStageMagFilterMode(1, -1);
    theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture->SetStageMipMapBias(0.0f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    theTexture->mIsManaged = true;
    this->mCriticalSection->Leave();
    return theTexture;
}

// FUNCTION: LLE 0x1003400b
// Marks the texture as promoted; always succeeds
long GETextureManager::PromoteTexture(GETexture* inTexture)
{
    inTexture->SetIsPromoted(true);
    return kErrorOK;
}

// FUNCTION: LLE 0x10034018
// Reloads an already cached texture from its files (port, D9: the cache name is the PNG files' stem; the original
// looked <name>.tga, then <name>.sst up), updating the memory footprint; 3 when it is not cached
long GETextureManager::ReloadTexture(const UString& inName, const GETextureDescription& inDescription)
{
    GESurfaceDescription theSurfaceDescription;
    EeOfTexture* theFile = NULL;
    GETexture* theTexture;
    long theResult;
    std::map<UString, GETexture*>::iterator theIter;
    this->mCriticalSection->Enter();
    theIter = this->mTextures.find(inName);
    if (theIter != this->mTextures.end()) {
        theTexture = theIter->second;
        theResult = GEFindCachedTextureFile(this->mTexturePath, theIter->first, theTexture->mIsFromTexturePack, theFile);
        if (theResult == kErrorOK) {
            if (inDescription.mPixelDepth == 16) {
                theSurfaceDescription.mPixelDepth = 16;
                theSurfaceDescription.mPixelFormat = inDescription.mHasAlpha ? 2 : 7;
            } else {
                theSurfaceDescription.mPixelFormat = kPixelFormatA8R8G8B8;
                theSurfaceDescription.mNoAlpha = !inDescription.mHasAlpha;
                theSurfaceDescription.mPixelDepth = 32;
            }
            theSurfaceDescription.mSurfaceVolatility = inDescription.mSurfaceVolatility;
            theSurfaceDescription.mIs3DRenderable = inDescription.mIs3DRenderable;
            theSurfaceDescription.mIsTransparencyEnabled = inDescription.mIsTransparencyEnabled;
            this->mMemoryFootprint -= theTexture->GetAllocatedSize();
            // port: a pack texture keeps its file, extra drop and MIP levels (SetTexturePackSizes)
            bool theFromPack = theTexture->mIsFromTexturePack;
            long thePackDrop = theTexture->mTexturePackMipLevels;
            theResult = theTexture->Load(theFile, theSurfaceDescription, this->mScreen,
                                         inDescription.mCreateMIPMaps || (theFromPack && theTexture->GetCreateMIPMaps()),
                                         inDescription.mNumMipLevelsToDrop + thePackDrop,
                                         inDescription.mFrameFormat);
            if (theResult == kErrorOK && theFromPack)
                this->SetTexturePackSizes(theTexture, thePackDrop);
            if (theResult == kErrorOK)
                this->mMemoryFootprint += theTexture->GetAllocatedSize();
            // the original's file Close result (always kErrorOK for the in-memory file it read)
            theResult = kErrorOK;
            ee_of_texture_free(theFile);
        }
    } else
        theResult = 3;
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100341d9
// Reloads every cached texture from its files (or recreates its surface when they are gone),
// then reapplies the frames' saved stage settings to the whole texture; returns the last result
// NEAR: inline budget (the original calls _Ptrit::operator+ out of line for every theFrames[i]; ours expands it
// from the fifth statement of the second loop on); accessors instead of members tried. The while loop with
// theIter++ at the end puts _Inc after the body as in the original
long GETextureManager::RefreshAllTextures()
{
    long theResult = kErrorOK;
    std::vector<GETexture*> theFrames;
    GESurfaceDescription theSurfaceDescription;
    std::map<UString, GETexture*>::iterator theIter;
    EeOfTexture* theFile = NULL;
    GETexture* theTexture;
    GESurface* theSurface;
    unsigned long i;
    this->mCriticalSection->Enter();
    theIter = this->mTextures.begin();
    while (theIter != this->mTextures.end()) {
        theTexture = theIter->second;
        if (GEFindCachedTextureFile(this->mTexturePath, theIter->first, theTexture->mIsFromTexturePack, theFile)
            == kErrorOK) {
            theSurface = theTexture->GetSurface();
            theSurfaceDescription.mPixelFormat = theSurface->GetPixelFormat();
            theSurfaceDescription.mPixelDepth = theSurface->GetPixelDepth();
            theSurfaceDescription.mSurfaceVolatility = theSurface->GetSurfaceVolatility();
            theSurfaceDescription.mIs3DRenderable = theSurface->GetIs3DRenderable();
            theSurfaceDescription.mIsTransparencyEnabled = theSurface->GetIsTransparencyEnabled();
            if (theTexture->GetFrameCount() > 1) {
                if (theTexture->GetFrameCount() > theFrames.size())
                    theFrames.resize(theTexture->GetFrameCount());
                for (i = 1; i < theTexture->GetFrameCount(); i++) {
                    theFrames[i] = theTexture->GetFrameTexture(i);
                    theFrames[i]->Reference();
                }
            }
            // port: a pack texture with its extra drop, then its two sizes again (SetTexturePackSizes)
            bool theFromPack = theTexture->mIsFromTexturePack;
            long thePackDrop = theTexture->mTexturePackMipLevels;
            if (theTexture->Load(theFile, theSurfaceDescription, this->mScreen, theTexture->GetCreateMIPMaps(),
                                 theTexture->GetNumMipLevelsToDrop() + thePackDrop,
                                 theTexture->GetFrameFormat()) == kErrorOK && theFromPack)
                this->SetTexturePackSizes(theTexture, thePackDrop);
            if (theTexture->GetFrameCount() > 1) {
                for (i = 1; i < theTexture->GetFrameCount(); i++) {
                    theTexture->SetStageAlphaMode(theFrames[i]->GetStage()->mAlphaMode, -1);
                    theTexture->SetStageColorMode(theFrames[i]->GetStage()->mColorMode, -1);
                    theTexture->SetStageMagFilterMode(theFrames[i]->GetStage()->mMagFilterMode, -1);
                    theTexture->SetStageMinFilterMode(theFrames[i]->GetStage()->mMinFilterMode, -1);
                    theTexture->SetStageMipFilterMode(theFrames[i]->GetStage()->mMipFilterMode, -1);
                    theTexture->SetStageMipMapBias(theFrames[i]->GetStage()->mMipMapBias, -1);
                    theTexture->SetStageTextureAddressingMode(theFrames[i]->GetStage()->mTextureAddressingMode,
                                                              -1);
                    theFrames[i]->Dereference();
                }
            }
            // the original's file Close result (always kErrorOK for the in-memory file it read)
            theResult = kErrorOK;
            ee_of_texture_free(theFile);
        } else
            theResult = theTexture->Recreate(this->mScreen);
        theIter++;
    }
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100344d7
// Removes and releases every texture nobody else references; returns 5 if any are still in use
long GETextureManager::RemoveAllTextures()
{
    long theResult = kErrorOK;
    std::map<UString, GETexture*>::iterator theIter;
    GETexture* theTexture;
    this->mCriticalSection->Enter();
    for (theIter = this->mTextures.begin(); theIter != this->mTextures.end();) {
        theTexture = theIter->second;
        if (theTexture->mReferenceCount == 1) {
            this->mMemoryFootprint -= theTexture->GetAllocatedSize();
            theTexture->UReferenceCounter::Dereference();
            theIter = this->mTextures.erase(theIter);
        } else {
            theResult = 5;
            theIter++;
        }
    }
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1003454d
// Removes and releases one texture if nobody else references it; 5 if still in use, 3 if not managed here
long GETextureManager::RemoveTexture(GETexture* inTexture)
{
    long theResult = 3;
    std::map<UString, GETexture*>::iterator theIter;
    this->mCriticalSection->Enter();
    for (theIter = this->mTextures.begin(); theIter != this->mTextures.end(); theIter++)
        if (theIter->second == inTexture) {
            if (inTexture->mReferenceCount == 1) {
                this->mMemoryFootprint -= inTexture->GetAllocatedSize();
                inTexture->UReferenceCounter::Dereference();
                theIter = this->mTextures.erase(theIter);
                theResult = kErrorOK;
            } else
                theResult = 5;
            break;
        }
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100345d0
// Replaces the texture search path, moving the reference
void GETextureManager::SetTexturePath(FSPath* inTexturePath)
{
    if (this->mTexturePath)
        this->mTexturePath->Dereference();
    this->mTexturePath = inTexturePath;
    if (inTexturePath)
        inTexturePath->Reference();
}

// FUNCTION: LLE 0x100345f1
// Sets the MIP filter mode of every cached texture that uses MIP filtering
void GETextureManager::UpdateMIPFilterMode(unsigned long inMode)
{
    std::map<UString, GETexture*>::iterator theIter;
    this->mCriticalSection->Enter();
    for (theIter = this->mTextures.begin(); theIter != this->mTextures.end(); theIter++)
        if (theIter->second->GetStage()->mMipFilterMode != 0)
            theIter->second->SetStageMipFilterMode(inMode, -1);
    this->mCriticalSection->Leave();
}
