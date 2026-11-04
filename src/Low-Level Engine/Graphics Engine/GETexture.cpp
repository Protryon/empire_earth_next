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
#include <windows.h>
#include <math.h>
#include <string.h>

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
#include "lle/FSMemoryFile.h"
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

// The 14-byte header in front of a texture file's frames (Load reads it, Save writes it; ".tga" files have none).
// Names PROVISIONAL.
#pragma pack(push, 1)
struct GETextureFileHeader {
    unsigned char mIsCompressed;            // +0x00
    unsigned char mMipLevelCount;           // +0x01 Save writes 1
    unsigned long mFrameCount;              // +0x02
    unsigned long mWidth;                   // +0x06
    unsigned long mHeight;                  // +0x0a
};
#pragma pack(pop)

GERasterizer* GETexture::sRasterizer;

// FUNCTION: LLE 0x10032cfa
// Creates an empty single-frame texture for a texture manager, with point filtering and clamped addressing
GETexture::GETexture(GETextureManager* inManager) : mFrameTopLeftCorner()
{
    GETexture::sRasterizer = GERasterizer::GetActiveRasterizer();
    this->mFrameUVSize = 1.0f;
    this->mManager = inManager;
    this->mStage.mMipMapBias = 0.0f;
    this->mScreen = NULL;
    this->mSurface = NULL;
    this->mData = NULL;
    this->mIsCompressed = false;
    this->mIsPromoted = false;
    this->mHasChanged = true;
    this->mFrameCount = 1;
    this->mFramesPerRow = 1;
    this->mFrameFormat = 1;
    this->mFrames = NULL;
    this->mFrameTexture = this;
    this->mFrame = 0;
    this->mMipLevelCount = 0;
    this->mNumMipLevelsToDrop = 0;
    this->mStage.mTexture = this;
    this->mStage.mColorMode = kStageOpTexture;
    this->mStage.mAlphaMode = kStageOpTexture;
    this->mStage.mTextureAddressingMode = kAddressingClamp;
    this->mStage.mMinFilterMode = kMinFilterPoint;
    this->mStage.mMagFilterMode = kMagFilterPoint;
    this->mStage.mMipFilterMode = kMipFilterPoint;
    this->mIsManaged = false;
}

// FUNCTION: LLE 0x10032d76
// Creates a texture around an existing surface (referenced) and registers it with the active rasterizer
GETexture::GETexture(GESurface* inSurface, GETextureManager* inManager) : mFrameTopLeftCorner()
{
    GETexture::sRasterizer = GERasterizer::GetActiveRasterizer();
    this->mManager = inManager;
    this->mSurface = inSurface;
    inSurface->Reference();
    this->mScreen = inSurface->GetParentScreen();
    this->mFrameUVSize = 1.0f;
    this->mStage.mMipMapBias = 0.0f;
    this->mData = NULL;
    this->mIsCompressed = false;
    this->mIsPromoted = false;
    this->mHasChanged = true;
    this->mFrameCount = 1;
    this->mFramesPerRow = 1;
    this->mFrameFormat = 1;
    this->mFrames = NULL;
    this->mFrameTexture = this;
    this->mFrame = 0;
    this->mMipLevelCount = 0;
    this->mNumMipLevelsToDrop = 0;
    this->mStage.mTexture = this;
    this->mStage.mColorMode = kStageOpTexture;
    this->mStage.mAlphaMode = kStageOpTexture;
    this->mStage.mTextureAddressingMode = kAddressingClamp;
    this->mStage.mMinFilterMode = kMinFilterPoint;
    this->mStage.mMagFilterMode = kMagFilterPoint;
    this->mStage.mMipFilterMode = kMipFilterPoint;
    this->mIsManaged = false;
    GETexture::sRasterizer->CreateTextureHandle(this);
}

// FUNCTION: LLE 0x10032e11
// Destroys the texture, releasing its extra animation frames, its rasterizer handle and its surface
GETexture::~GETexture()
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 1; i < this->mFrameCount; i++)
            this->mFrames[i]->UReferenceCounter::Dereference();
        this->SetFrame(0);
        if (this->mFrames)
            delete[] this->mFrames;
    }
    GETexture::sRasterizer->DestroyTextureHandle(this);
    if (this->mSurface) {
        this->mSurface->Dereference();
        this->mSurface = NULL;
    }
}

// FUNCTION: LLE 0x10032e79
// Does nothing and returns success (textures are never demoted)
long GETexture::Demote()
{
    return kErrorOK;
}

// FUNCTION: LLE 0x10032e7c
// Drops a reference; at the last outside reference a managed texture removes itself from its manager
void GETexture::Dereference()
{
    if (this->mReferenceCount > 2)
        UReferenceCounter::Dereference();
    else {
        if (this->mReferenceCount > 1)
            UReferenceCounter::Dereference();
        if (this->mIsManaged)
            this->mManager->RemoveTexture(this);
    }
}

// FUNCTION: LLE 0x10032eab
// Returns the surface's allocated size, times the frame count when each frame has its own texture
unsigned long GETexture::GetAllocatedSize()
{
    unsigned long theSize = this->mSurface->GetAllocatedSize();
    if (this->mFrameFormat == 0)
        theSize *= this->mFrameCount;
    return theSize;
}

// NEAR: control flow, calls and frame order match; what is left is slot choice. The original keeps theResult in a
// frame slot (-8) and gives the dead parameter slots to i (+8), theRow, theX, j, theTarget; ours gives +8 to
// theResult (declaration order has no effect). Tried: theResult initialised at the top, a separate counter per
// loop (only the frame loop of format 0 as theFrame helps), store orders in format 1.
// The format-1 path never releases its single-frame staging surface (theSurface): the original leaks it too
// (only theGrid is dereferenced, 0x100336af).
// FUNCTION: LLE 0x10032ec4
// Loads the texture (and its animation frames) from a texture or .tga file, building or dropping MIP levels
// as asked; frames become separate textures (format 0) or cells of one grid surface (format 1)
long GETexture::Load(FSFileSpec& inFile, GESurfaceDescription& inDescription, GEScreen* inScreen,
                     bool inCreateMIPMaps, long inNumMipLevelsToDrop, unsigned long inFrameFormat)
{
    FSMemoryFile theMemoryFile;
    GERectangle theSource;
    GESurfaceDescription theDescription;
    GESurfaceDescription theDropDescription;
    bool theIsTGA = false;
    long theMipLevels = 0;
    GERectangle theDestination;
    bool theSuccess = true;
    FSFileSpec* theFile;
    unsigned long theLength;
    GETextureFileHeader theHeader;
    long theResult;
    unsigned long i;
    unsigned long theFrame;
    long j;
    long theSize;
    unsigned long theGridSize;
    const char* theName;
    unsigned char theType;
    GESurface* theSurface;
    GESurface* theDropSurface;
    GESurface* theGrid;
    GESurface* theMip;
    GESurface* theNext;
    GESurface* theTarget;
    float theX;
    unsigned long theRow;

    if (!dynamic_cast<FSMemoryFile*>(&inFile) && inFile.GetLength(theLength) == kErrorOK) {
        theMemoryFile.Load(inFile, theLength);
        theFile = &theMemoryFile;
    } else
        theFile = &inFile;
    if (this->mSurface) {
        this->mSurface->Dereference();
        this->mSurface = NULL;
    }
    if (this->mFrameFormat == 0) {
        for (i = 1; i < this->mFrameCount; i++)
            this->mFrames[i]->Dereference();
        if (this->mFrames) {
            delete[] this->mFrames;
            this->mFrames = NULL;
        }
    }
    this->mNumMipLevelsToDrop = inNumMipLevelsToDrop;
    this->mCreateMIPMaps = inCreateMIPMaps;
    memset(&theHeader, 0, sizeof(theHeader));
    theName = inFile.GetFile();
    if (_stricmp(theName + strlen(theName) - 4, ".tga") == 0) {
        const unsigned char* theData = dynamic_cast<FSMemoryFile*>(theFile)->GetData();
        theHeader.mIsCompressed = 0;
        theHeader.mMipLevelCount = 0;
        theHeader.mFrameCount = 1;
        theHeader.mWidth = *(unsigned short*)(theData + 12);
        theHeader.mHeight = *(unsigned short*)(theData + 14);
        theIsTGA = true;
        theResult = kErrorOK;
        theLength = sizeof(theHeader);
    } else
        theResult = theFile->Read(reinterpret_cast<unsigned char*>(&theHeader), sizeof(theHeader), theLength);
    if (theResult == kErrorOK && theLength == sizeof(theHeader)) {
        if (inFrameFormat == 0) {
            this->mFrameFormat = 0;
            this->mFrames = new GETexture*[theHeader.mFrameCount];
            if (this->mFrames) {
                this->mFrames[0] = this;
                this->mFrameTexture = this;
                for (i = 1; i < theHeader.mFrameCount; i++) {
                    this->mFrames[i] = new GETexture(this->mManager);
                    if (this->mFrames[i] == NULL)
                        theSuccess = false;
                }
            } else
                theSuccess = false;
            if (theSuccess) {
                theDescription = inDescription;
                theDescription.mWidth = theHeader.mWidth;
                theDescription.mHeight = theHeader.mHeight;
                theDescription.m14 = 0;
                theSize = theHeader.mWidth;
                do {
                    theSize >>= 1;
                    theMipLevels++;
                } while (theSize > 0);
                if (theMipLevels - 1 < inNumMipLevelsToDrop)
                    inNumMipLevelsToDrop = theMipLevels - 1;
                if (inCreateMIPMaps) {
                    theDescription.mType = kSurfaceMipMappedTexture;
                    theDescription.mMipLevelCount = theMipLevels;
                } else if (inNumMipLevelsToDrop > 0) {
                    theDescription.mType = kSurfaceMipMappedTexture;
                    theDescription.mMipLevelCount = inNumMipLevelsToDrop + 1;
                } else {
                    theDescription.mType = kSurfaceTexture;
                    theDescription.mMipLevelCount = 1;
                }
                if (inNumMipLevelsToDrop > 0) {
                    if (inNumMipLevelsToDrop == theMipLevels - 1) {
                        theDropDescription = theDescription;
                        theDropDescription.mWidth = 1;
                        theDropDescription.mHeight = 1;
                        theDropDescription.mType = kSurfaceTexture;
                        theDropDescription.mMipLevelCount = 1;
                    } else if (inCreateMIPMaps) {
                        theDropDescription = theDescription;
                        theDropDescription.mType = kSurfaceMipMappedTexture;
                        theDropDescription.mWidth = theHeader.mWidth >> inNumMipLevelsToDrop;
                        theDropDescription.mHeight = theHeader.mHeight >> inNumMipLevelsToDrop;
                        theDropDescription.mMipLevelCount = theMipLevels - inNumMipLevelsToDrop;
                    } else {
                        theDropDescription = theDescription;
                        theDropDescription.mType = kSurfaceTexture;
                        theDropDescription.mMipLevelCount = 1;
                        theDropDescription.mWidth = theHeader.mWidth >> inNumMipLevelsToDrop;
                        theDropDescription.mHeight = theHeader.mHeight >> inNumMipLevelsToDrop;
                    }
                    inDescription = theDropDescription;
                    this->mMipLevelCount = theDropDescription.mMipLevelCount;
                }
                this->mFrameCount = theHeader.mFrameCount;
                for (theFrame = 0; theFrame < theHeader.mFrameCount; theFrame++) {
                    if (inScreen->CreateSurface(theDescription, &theSurface) == kErrorOK) {
                        if (theIsTGA)
                            theType = 0;
                        else if (theFile->Read(theType) != kErrorOK)
                            goto drop;
                        if (theSurface->Load(*theFile, theType) == kErrorOK) {
                            theMip = theSurface;
                            for (j = 1; j < theDescription.mMipLevelCount; j++) {
                                theNext = theMip->GetAttachedSurface(0);
                                theSource.SetRight((float)theMip->GetWidth());
                                theSource.SetBottom((float)theMip->GetHeight());
                                theDestination.SetRight((float)theNext->GetWidth());
                                theDestination.SetBottom((float)theNext->GetHeight());
                                theMip->CopyWithFilter(theNext, theSource, theDestination);
                                theMip = theNext;
                            }
                        }
                    } else {
                        // No surface was created for this frame: report it instead of setting a stale pointer.
                        theResult = kErrorOutOfMemory;
                        continue;
                    }
                drop:
                    if (inNumMipLevelsToDrop > 0) {
                        // Swap in the reduced surface only when it was created.
                        if (inScreen->CreateSurface(theDropDescription, &theDropSurface) == kErrorOK) {
                            theTarget = theDropSurface;
                            theMip = theSurface;
                            for (j = 0; j < inNumMipLevelsToDrop; j++)
                                theMip = theMip->GetAttachedSurface(0);
                            j = 0;
                            while (j < this->mMipLevelCount) {
                                theSource.SetRight((float)theMip->GetWidth());
                                theSource.SetBottom((float)theMip->GetHeight());
                                theMip->CopyTo(theTarget, theSource, theSource, false);
                                j++;
                                if (j < this->mMipLevelCount) {
                                    theMip = theMip->GetAttachedSurface(0);
                                    theTarget = theTarget->GetAttachedSurface(0);
                                }
                            }
                            theSurface->Dereference();
                            theSurface = theDropSurface;
                        }
                    }
                    theResult = this->mFrames[theFrame]->SetSurface(theSurface);
                    if (theResult == kErrorOK)
                        theSurface->Dereference();
                }
            } else
                theResult = kErrorOutOfMemory;
        } else if (inFrameFormat == 1) {
            theGridSize = 1;
            while (theHeader.mFrameCount > theGridSize)
                theGridSize *= 4;
            this->mFramesPerRow = (unsigned long)sqrt((double)theGridSize);
            this->mFrameTexture = this;
            this->mFrameFormat = 1;
            this->mFrameCount = theHeader.mFrameCount;
            this->mFrameUVSize = 1.0f / this->mFramesPerRow;
            this->mFrameTopLeftCorner.SetU(0.0f);
            this->mFrameTopLeftCorner.SetV(0.0f);
            inDescription.mWidth = theHeader.mWidth;
            inDescription.mHeight = theHeader.mHeight;
            inDescription.m14 = 0;
            if (theHeader.mMipLevelCount > 1) {
                inDescription.mType = kSurfaceMipMappedTexture;
                inDescription.mMipLevelCount = theHeader.mMipLevelCount;
            } else {
                inDescription.mType = kSurfaceTexture;
                inDescription.mMipLevelCount = 1;
            }
            theResult = inScreen->CreateSurface(inDescription, &theSurface);
            if (theResult == kErrorOK) {
                inDescription.mWidth = this->mFramesPerRow * theHeader.mWidth;
                inDescription.mHeight = this->mFramesPerRow * theHeader.mHeight;
                theResult = inScreen->CreateSurface(inDescription, &theGrid);
                if (theResult == kErrorOK) {
                    for (i = 0; i < theHeader.mFrameCount; i++) {
                        theMip = theSurface;
                        theTarget = theGrid;
                        for (j = 0; j < theHeader.mMipLevelCount; j++) {
                            if (theIsTGA)
                                theType = 0;
                            else {
                                theResult = theFile->Read(theType);
                                if (theResult != kErrorOK)
                                    return theResult;
                            }
                            theResult = theMip->Load(*theFile, theType);
                            if (theResult != kErrorOK)
                                return theResult;
                            theSource.SetLeft(0.0f);
                            theSource.SetTop(0.0f);
                            theSource.SetRight((float)theMip->GetWidth());
                            theSource.SetBottom((float)theMip->GetHeight());
                            theX = (float)i / (float)this->mFramesPerRow;
                            theRow = (unsigned long)theX;
                            theDestination.SetLeft((theX - theRow) * theMip->GetWidth() * this->mFramesPerRow);
                            theDestination.SetTop((float)(theMip->GetHeight() * theRow));
                            theDestination.SetRight(theMip->GetWidth() + theDestination.GetLeft());
                            theDestination.SetBottom(theMip->GetHeight() + theDestination.GetTop());
                            theResult = theMip->CopyTo(theTarget, theSource, theDestination, false);
                            if (theResult != kErrorOK)
                                return theResult;
                            if (theMip->GetAttachedSurfaceCount() > 0)
                                theMip = theMip->GetAttachedSurface(0);
                            if (theTarget->GetAttachedSurfaceCount() > 0)
                                theTarget = theTarget->GetAttachedSurface(0);
                        }
                    }
                    theResult = this->SetSurface(theGrid);
                    if (theResult == kErrorOK)
                        theGrid->Dereference();
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x100336c9
// Locks the texture's surface and returns its memory and row pitch
long GETexture::Lock(unsigned char*& outMemory, unsigned long& outBytesPerRow)
{
    long theResult = this->mSurface->Lock();
    if (theResult == kErrorOK) {
        outMemory = this->mSurface->GetMemoryAddress();
        outBytesPerRow = this->mSurface->GetBytesPerRow();
    }
    return theResult;
}

// FUNCTION: LLE 0x100336fe
// Recreates the texture's surface on another screen with the same description
long GETexture::Recreate(GEScreen* inScreen)
{
    GESurfaceDescription theDescription;
    this->mScreen = inScreen;
    theDescription = this->mSurface->GetSurfaceDescription();
    this->mSurface->Dereference();
    this->mSurface = NULL;
    return this->mScreen->CreateSurface(theDescription, &this->mSurface);
}

// FSBasicFile::GetFileAccess is header-inline (FSBasicFile.cpp defines it; its export is ICF-folded at 0x4c365)
// FUNCTION: LLE 0x10033749
// Writes the texture file header and every frame's surface to a file (opening it for writing if closed);
// returns the first error
long GETexture::Save(FSFileSpec& inFile, unsigned char inType, long inFrame)
{
    long theResult;
    bool theWasOpen = inFile.GetIsOpen();
    GETextureFileHeader theHeader;
    unsigned long theWritten;
    unsigned long i;
    if (theWasOpen) {
        if (inFile.GetFileAccess() != kFileAccessWrite)
            return kErrorFailed;
    } else {
        theResult = inFile.Open(kFileAccessWrite);
        if (theResult != kErrorOK)
            return theResult;
    }
    theHeader.mWidth = this->mSurface->GetWidth();
    theHeader.mHeight = this->mSurface->GetHeight();
    theHeader.mIsCompressed = this->mIsCompressed;
    theHeader.mMipLevelCount = 1;
    theHeader.mFrameCount = this->mFrameCount;
    theResult = inFile.Write(reinterpret_cast<const unsigned char*>(&theHeader), sizeof(theHeader), theWritten);
    if (theResult == kErrorOK && theWritten == sizeof(theHeader)) {
        for (i = 0; i < theHeader.mFrameCount; i++) {
            theResult = inFile.Write(inType);
            if (theResult != kErrorOK)
                break;
            theResult = this->GetFrameTexture(i)->GetSurface()->Save(inFile, inType);
            if (theResult != kErrorOK)
                break;
        }
    }
    if (!theWasOpen)
        theResult = inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x1003381c
// Sets the alpha blend mode on every frame (or on the texture), marking changed ones
void GETexture::SetStageAlphaMode(unsigned long inMode, unsigned long inStage)
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 0; i < this->mFrameCount; i++) {
            if (this->mFrames[i]->mStage.mAlphaMode != inMode) {
                this->mFrames[i]->mStage.mAlphaMode = inMode;
                this->mFrames[i]->SetHasChanged();
            }
        }
    } else if (this->mStage.mAlphaMode != inMode) {
        this->mStage.mAlphaMode = inMode;
        this->SetHasChanged();
    }
}

// FUNCTION: LLE 0x10033861
// Sets the color blend mode on every frame (or on the texture), marking changed ones
void GETexture::SetStageColorMode(unsigned long inMode, unsigned long inStage)
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 0; i < this->mFrameCount; i++) {
            if (this->mFrames[i]->mStage.mColorMode != inMode) {
                this->mFrames[i]->mStage.mColorMode = inMode;
                this->mFrames[i]->SetHasChanged();
            }
        }
    } else if (this->mStage.mColorMode != inMode) {
        this->mStage.mColorMode = inMode;
        this->SetHasChanged();
    }
}

// FUNCTION: LLE 0x100338a6
// Sets the magnification filter on every frame (or on the texture), marking changed ones
void GETexture::SetStageMagFilterMode(unsigned long inMode, unsigned long inStage)
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 0; i < this->mFrameCount; i++) {
            if (this->mFrames[i]->mStage.mMagFilterMode != inMode) {
                this->mFrames[i]->mStage.mMagFilterMode = inMode;
                this->mFrames[i]->SetHasChanged();
            }
        }
    } else if (this->mStage.mMagFilterMode != inMode) {
        this->mStage.mMagFilterMode = inMode;
        this->SetHasChanged();
    }
}

// FUNCTION: LLE 0x100338eb
// Sets the minification filter on every frame (or on the texture), marking changed ones
void GETexture::SetStageMinFilterMode(unsigned long inMode, unsigned long inStage)
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 0; i < this->mFrameCount; i++) {
            if (this->mFrames[i]->mStage.mMinFilterMode != inMode) {
                this->mFrames[i]->mStage.mMinFilterMode = inMode;
                this->mFrames[i]->SetHasChanged();
            }
        }
    } else if (this->mStage.mMinFilterMode != inMode) {
        this->mStage.mMinFilterMode = inMode;
        this->SetHasChanged();
    }
}

// FUNCTION: LLE 0x10033930
// Sets the MIP filter on every frame (or on the texture), marking changed ones
void GETexture::SetStageMipFilterMode(unsigned long inMode, unsigned long inStage)
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 0; i < this->mFrameCount; i++) {
            if (this->mFrames[i]->mStage.mMipFilterMode != inMode) {
                this->mFrames[i]->mStage.mMipFilterMode = inMode;
                this->mFrames[i]->SetHasChanged();
            }
        }
    } else if (this->mStage.mMipFilterMode != inMode) {
        this->mStage.mMipFilterMode = inMode;
        this->SetHasChanged();
    }
}

// FUNCTION: LLE 0x10033975
// Sets the MIP-map bias on every frame (or on the texture), marking changed ones
void GETexture::SetStageMipMapBias(float inBias, unsigned long inStage)
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 0; i < this->mFrameCount; i++) {
            if (this->mFrames[i]->mStage.mMipMapBias != inBias) {
                this->mFrames[i]->mStage.mMipMapBias = inBias;
                this->mFrames[i]->SetHasChanged();
            }
        }
    } else if (this->mStage.mMipMapBias != inBias) {
        this->mStage.mMipMapBias = inBias;
        this->SetHasChanged();
    }
}

// FUNCTION: LLE 0x100339c8
// Sets the texture addressing mode on every frame (or on the texture), marking changed ones
void GETexture::SetStageTextureAddressingMode(unsigned long inMode, unsigned long inStage)
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 0; i < this->mFrameCount; i++) {
            if (this->mFrames[i]->mStage.mTextureAddressingMode != inMode) {
                this->mFrames[i]->mStage.mTextureAddressingMode = inMode;
                this->mFrames[i]->SetHasChanged();
            }
        }
    } else if (this->mStage.mTextureAddressingMode != inMode) {
        this->mStage.mTextureAddressingMode = inMode;
        this->SetHasChanged();
    }
}

// FUNCTION: LLE 0x10033a0d
// Sets the stage texture on every frame (or on the texture), marking changed ones
void GETexture::SetStageTexture(GETexture* inTexture, unsigned long inStage)
{
    unsigned long i;
    if (this->mFrameFormat == 0) {
        for (i = 0; i < this->mFrameCount; i++) {
            if (this->mFrames[i]->mStage.mTexture != inTexture) {
                this->mFrames[i]->mStage.mTexture = inTexture;
                this->mFrames[i]->SetHasChanged();
            }
        }
    } else if (this->mStage.mTexture != inTexture) {
        this->mStage.mTexture = inTexture;
        this->SetHasChanged();
    }
}

// FUNCTION: LLE 0x10033a52
// Shows the given animation frame: picks the frame texture, or moves the UV corner to its grid cell
void GETexture::SetFrame(unsigned long inFrame)
{
    this->mFrame = inFrame;
    if (this->mFrameFormat == 0)
        this->mFrameTexture = this->mFrames[inFrame];
    else {
        float theFramesPerRow = (float)this->mFramesPerRow;
        float theX = (float)inFrame / theFramesPerRow;
        float theRow = (float)(unsigned long)theX;
        this->mFrameTopLeftCorner.SetU(theX - theRow);
        this->mFrameTopLeftCorner.SetV(theRow / theFramesPerRow);
    }
    this->SetHasChanged();
}

// FUNCTION: LLE 0x10033ab7
// Replaces the texture's surface (new rasterizer handle, promotion cleared); already-exists error if unchanged
long GETexture::SetSurface(GESurface* inSurface)
{
    long theResult = kErrorOK;
    if (inSurface != this->mSurface) {
        if (this->mSurface) {
            GETexture::sRasterizer->DestroyTextureHandle(this);
            this->mSurface->Dereference();
        }
        this->mSurface = inSurface;
        inSurface->Reference();
        this->mScreen = inSurface->GetParentScreen();
        this->mIsPromoted = false;
        GETexture::sRasterizer->CreateTextureHandle(this);
        this->SetHasChanged();
    } else
        theResult = kErrorAlreadyExists;
    return theResult;
}

// FUNCTION: LLE 0x10033b17
// Unlocks the texture's surface
long GETexture::Unlock()
{
    return this->mSurface->Unlock();
}
