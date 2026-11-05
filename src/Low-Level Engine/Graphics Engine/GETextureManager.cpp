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
static unsigned long sTextureNumber = 1;    // 0x100d9ca8: appended to made textures' names (provisional name)
// Release-build trace of a texture file that failed to load or close: compiled out, but its arguments (the file's
// volume, directory and name) are still evaluated. GEModelManager.cpp has the same shape (provisional name).
inline void GEReportTextureFile(const UString& inVolume, const UString& inDirectory, const UString& inFile)
{
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
// Returns the named texture (.tga first, then .sst), loading and caching it from the search path if needed;
// NULL if it cannot be found or loaded
// NEAR: frame slots only (the original puts theTGAName at -0x3c and theFoundName at -0x2c, ours the reverse);
// declaration orders of the function-scope locals tried
GETexture* GETextureManager::LoadTexture(const UString& inName, const GETextureDescription& inDescription)
{
    GESurfaceDescription theSurfaceDescription;
    FSFileSpec* theFile = NULL;
    GETexture* theTexture = NULL;
    UString theFoundName;
    bool theIsSST;
    long theResult;
    this->mCriticalSection->Enter();
    theIsSST = false;
    if (inName.FindBackwards(".sst", -1) != inName.GetLength())
        theIsSST = true;
    UString theTGAName = inName + ".tga";
    UString theSSTName(inName);
    if (theIsSST) {
        theSSTName = inName;
        std::map<UString, GETexture*>::iterator theIter = this->mTextures.find(theSSTName);
        if (theIter != this->mTextures.end())
            theTexture = theIter->second;
    } else {
        std::map<UString, GETexture*>::iterator theIter = this->mTextures.find(theTGAName);
        if (theIter != this->mTextures.end())
            theTexture = theIter->second;
        else {
            theSSTName = inName + ".sst";
            theIter = this->mTextures.find(theSSTName);
            if (theIter != this->mTextures.end())
                theTexture = theIter->second;
        }
    }
    if (theTexture == NULL) {
        if (!theIsSST && this->mTexturePath->FindFile(theTGAName, theFile) == kErrorOK) {
            theFoundName = theTGAName;
            theResult = kErrorOK;
        } else {
            theResult = this->mTexturePath->FindFile(theSSTName, theFile);
            theFoundName = theSSTName;
        }
        if (theResult == kErrorOK) {
            if (theFile->Open(kFileAccessRead) == kErrorOK) {
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
                if (theTexture->Load(*theFile, theSurfaceDescription, this->mScreen, inDescription.mCreateMIPMaps,
                                     inDescription.mNumMipLevelsToDrop, inDescription.mFrameFormat) == kErrorOK) {
                    this->mTextures[theFoundName] = theTexture;
                    this->mMemoryFootprint += theTexture->GetAllocatedSize();
                    theTexture->mIsManaged = true;
                } else {
                    GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
                    theTexture->UReferenceCounter::Dereference();
                    theTexture = NULL;
                }
                if (theFile->Close() != kErrorOK)
                    GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
            } else
                GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
            delete theFile;
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
// Reloads an already cached texture from its file (.tga or .sst name), updating the memory footprint
long GETextureManager::ReloadTexture(const UString& inName, const GETextureDescription& inDescription)
{
    GESurfaceDescription theSurfaceDescription;
    FSFileSpec* theFile = NULL;
    UString theFoundName;
    GETexture* theTexture;
    long theResult;
    std::map<UString, GETexture*>::iterator theIter;
    this->mCriticalSection->Enter();
    UString theTGAName = inName + ".tga";
    theIter = this->mTextures.find(theTGAName);
    if (theIter != this->mTextures.end())
        theFoundName = theTGAName;
    else {
        UString theSSTName = inName + ".sst";
        theIter = this->mTextures.find(theSSTName);
        if (theIter != this->mTextures.end())
            theFoundName = theSSTName;
    }
    if (theIter != this->mTextures.end()) {
        theTexture = theIter->second;
        theResult = this->mTexturePath->FindFile(theFoundName, theFile);
        if (theResult == kErrorOK) {
            theResult = theFile->Open(kFileAccessRead);
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
                theResult = theTexture->Load(*theFile, theSurfaceDescription, this->mScreen, inDescription.mCreateMIPMaps,
                                             inDescription.mNumMipLevelsToDrop, inDescription.mFrameFormat);
                if (theResult == kErrorOK)
                    this->mMemoryFootprint += theTexture->GetAllocatedSize();
                else
                    GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
                theResult = theFile->Close();
                if (theResult != kErrorOK)
                    GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
            } else
                GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
            delete theFile;
        }
    } else
        theResult = 3;
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100341d9
// Reloads every cached texture from its file (or recreates its surface when the file is gone),
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
    FSFileSpec* theFile = NULL;
    GETexture* theTexture;
    GESurface* theSurface;
    unsigned long i;
    this->mCriticalSection->Enter();
    theIter = this->mTextures.begin();
    while (theIter != this->mTextures.end()) {
        theTexture = theIter->second;
        if (this->mTexturePath->FindFile(theIter->first, theFile) == kErrorOK) {
            theResult = theFile->Open(kFileAccessRead);
            if (theResult == kErrorOK) {
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
                if (theTexture->Load(*theFile, theSurfaceDescription, this->mScreen, theTexture->GetCreateMIPMaps(),
                                     theTexture->GetNumMipLevelsToDrop(), theTexture->GetFrameFormat()) != kErrorOK)
                    GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
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
                theResult = theFile->Close();
                if (theResult != kErrorOK)
                    GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
            } else
                GEReportTextureFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
            delete theFile;
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
