// storegate's digests of what a GEModel and a GETexture store, read through their members (GEStoreDigest is a
// friend of GEModel, GEMaterial, GETexture and GETextureManager). FNV-1a over every value, floats by their bits; an
// array shared with an earlier frame is hashed as that frame's number.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "fakes.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"

#include <string.h>
#include <algorithm>

namespace {

struct Hash {
    unsigned long long mValue = 0xcbf29ce484222325ull;
    void Bytes(const void* inData, size_t inLength)
    {
        const unsigned char* p = static_cast<const unsigned char*>(inData);
        for (size_t i = 0; i < inLength; i++) {
            mValue ^= p[i];
            mValue *= 0x100000001b3ull;
        }
    }
    void U(unsigned long long inValue) { Bytes(&inValue, sizeof inValue); }
    void F(float inValue)
    {
        unsigned int theBits;
        memcpy(&theBits, &inValue, 4);
        U(theBits);
    }
    void S(const UString& inString)
    {
        const char* theText = inString;
        U(inString.GetLength());
        Bytes(theText, strlen(theText));
    }
    void Point(const GE3DPoint& inPoint)
    {
        F(inPoint.GetX());
        F(inPoint.GetY());
        F(inPoint.GetZ());
    }
    void Transform(const GETransformation& inTransform)
    {
        for (unsigned long r = 0; r < 4; r++)
            for (unsigned long c = 0; c < 4; c++)
                F(inTransform.GetElement(r, c));
    }
    void Floats(const std::vector<float>& inValues)
    {
        U(inValues.size());
        for (float v : inValues)
            F(v);
    }
};

}  // namespace

struct GEStoreDigest {
static void HashSurface(Hash& ioHash, GESurface* inSurface)
{
    for (unsigned long theLevel = 0; inSurface; theLevel++) {
        StoreSurface* theSurface = dynamic_cast<StoreSurface*>(inSurface);
        if (!theSurface) {
            ioHash.U(0xdead);
            return;
        }
        const GESurfaceDescription& d = theSurface->mDescription;
        ioHash.U(d.mType);
        ioHash.U(d.mPixelFormat);
        ioHash.U(d.mSurfaceVolatility);
        ioHash.U(d.mWidth);
        ioHash.U(d.mHeight);
        ioHash.U(d.m14);
        ioHash.U((unsigned long long)(long long)d.mMipLevelCount);
        ioHash.U(d.mPixelDepth);
        ioHash.U(d.mIs3DRenderable);
        ioHash.U(d.mIsTransparencyEnabled);
        ioHash.U(d.mNoAlpha);
        ioHash.U(theSurface->GetLogicalShift());
        ioHash.U(theSurface->mLocked);
        ioHash.U(theSurface->mLockCount);
        ioHash.U(theSurface->GetReferenceCount());
        ioHash.Bytes(theSurface->mPixels.data(), theSurface->mPixels.size());
        inSurface = theSurface->mNext;
    }
}

static void HashTexture(Hash& ioHash, GETexture* inTexture, bool inFrames)
{
    ioHash.U(inTexture->mIsManaged);
    const GETStage& s = inTexture->mStage;
    ioHash.U(s.mTexture == inTexture);
    ioHash.U(s.mColorMode);
    ioHash.U(s.mAlphaMode);
    ioHash.U(s.mTextureAddressingMode);
    ioHash.U(s.mMinFilterMode);
    ioHash.U(s.mMagFilterMode);
    ioHash.U(s.mMipFilterMode);
    ioHash.F(s.mMipMapBias);
    ioHash.F(inTexture->mFrameTopLeftCorner.GetU());
    ioHash.F(inTexture->mFrameTopLeftCorner.GetV());
    ioHash.U(inTexture->mFrameTexture == inTexture);
    ioHash.U(inTexture->mData != NULL);
    ioHash.F(inTexture->mFrameUVSize);
    ioHash.U(inTexture->mFrameFormat);
    ioHash.U(inTexture->mFrame);
    ioHash.U(inTexture->mFrameCount);
    ioHash.U(inTexture->mFramesPerRow);
    ioHash.U((unsigned long long)(long long)inTexture->mMipLevelCount);
    ioHash.U(inTexture->mNumMipLevelsToDrop);
    ioHash.U(inTexture->mIsCompressed);
    ioHash.U(inTexture->mIsPromoted);
    ioHash.U(inTexture->mHasChanged);
    ioHash.U(inTexture->mCreateMIPMaps);
    ioHash.U(inTexture->mIsFromTexturePack);
    ioHash.U((unsigned long long)(long long)inTexture->mTexturePackMipLevels);
    ioHash.U(inTexture->GetReferenceCount());
    ioHash.U(inTexture->GetAllocatedSize());
    HashSurface(ioHash, inTexture->mSurface);
    if (inFrames && inTexture->mFrameFormat == 0 && inTexture->mFrames) {
        ioHash.U(inTexture->mFrames[0] == inTexture);
        for (unsigned long i = 1; i < inTexture->mFrameCount; i++)
            HashTexture(ioHash, inTexture->mFrames[i], false);
    } else
        ioHash.U(inTexture->mFrames != NULL);
}

// The index of the first entry equal to entry i (an array a frame shares with an earlier one)
template <class T> static size_t FirstOf(T* const* inArray, size_t i)
{
    for (size_t k = 0; k < i; k++)
        if (inArray[k] == inArray[i])
            return k;
    return i;
}

static unsigned long long HashManager(GETextureManager* inManager);

static void HashModel(Hash& ioHash, GEModel* m, GEModel* inParent)
{
    ioHash.U(m->mParentModel == inParent);
    ioHash.U(m->GetReferenceCount());
    ioHash.Transform(m->mGraphicsPreModelTransform);
    ioHash.Transform(m->mGraphicsPostModelTransform);
    ioHash.Transform(m->mGraphicsTransform);
    ioHash.Point(m->mCenterPoint);
    ioHash.S(m->mName);
    ioHash.U(m->mSharedFrames.size());
    for (const auto& theEntry : m->mSharedFrames) {
        ioHash.U(theEntry.first);
        ioHash.U((unsigned long long)(long long)theEntry.second);
    }
    ioHash.F(m->mGraphicsScale);
    ioHash.F(m->mModelOpacity);
    ioHash.F(m->mFractionalFramePosition);
    ioHash.U(m->mGraphicsCurrentFrame);
    ioHash.U(m->mGraphicsNextFrame);
    ioHash.U(m->mSubModelCount);
    ioHash.U(m->mFrameCount);
    ioHash.U(m->mMaterialCount);
    ioHash.U(m->mPolygonCount);
    ioHash.U(m->mVertexCount);
    ioHash.U(m->mTagPointCount);
    ioHash.U(m->mLODCount);
    for (int i = 0; i < 6; i++)
        ioHash.U(m->mClipToPlane[i]);
    ioHash.U(m->SizeOf());
    for (unsigned long f = 0; f < m->mFrameCount && m->mInternalTransforms; f++)
        ioHash.Transform(m->mInternalTransforms[f]);
    for (unsigned long f = 0; f < m->mFrameCount && m->mBoundingBoxes; f++) {
        const GEBoundingBox& b = m->mBoundingBoxes[f];
        ioHash.Point(b.GetBottomLeftFront());
        ioHash.Point(b.GetTopRightBack());
        ioHash.F(b.GetWidth());
        ioHash.F(b.GetHeight());
        ioHash.F(b.GetDepth());
    }
    if (m->mGraphicsBoundingBox) {
        ioHash.Point(m->mGraphicsBoundingBox->GetBottomLeftFront());
        ioHash.Point(m->mGraphicsBoundingBox->GetTopRightBack());
    }
    for (unsigned long f = 0; f < m->mFrameCount && m->mBoundingRadii; f++)
        ioHash.F(m->mBoundingRadii[f]);
    for (unsigned long i = 0; i < m->mTagPointCount; i++) {
        if (m->mTagPoints)
            ioHash.Point(m->mTagPoints[i]);
        if (m->mTagPointNames)
            ioHash.S(m->mTagPointNames[i]);
    }
    for (unsigned long f = 0; f < m->mFrameCount && m->mFrameTagPoints; f++) {
        size_t theFirst = FirstOf(m->mFrameTagPoints, f);
        ioHash.U(theFirst);
        if (theFirst == f && m->mFrameTagPoints[f])
            for (unsigned long i = 0; i < m->mTagPointCount; i++)
                ioHash.Point(m->mFrameTagPoints[f][i]);
    }
    for (unsigned long i = 0; i < m->mMaterialCount && m->mMaterials; i++) {
        GEMaterial* theMaterial = m->mMaterials[i];
        ioHash.S(theMaterial->mName);
        ioHash.F(theMaterial->mColor.GetRed());
        ioHash.F(theMaterial->mColor.GetGreen());
        ioHash.F(theMaterial->mColor.GetBlue());
        ioHash.U(theMaterial->mScreenCount);
        for (unsigned long k = 0; k < theMaterial->mScreenCount; k++)
            ioHash.U(theMaterial->mTextures[k] != NULL);
        ioHash.U(theMaterial->mLODCount);
        for (unsigned long k = 0; k < theMaterial->mLODCount; k++) {
            ioHash.U(theMaterial->mPolygonStartingIndices[k]);
            ioHash.U(theMaterial->mPolygonRangeLengths[k]);
        }
        ioHash.U(theMaterial->mVertexStartingIndex);
        ioHash.U(theMaterial->mVertexRangeLength);
        ioHash.U(theMaterial->mTextureIndex);
        ioHash.F(theMaterial->mOpacity);
        if (m->mTextureFilenames)
            ioHash.S(m->mTextureFilenames[i]);
    }
    for (unsigned long l = 0; l < m->mLODCount && m->mPolygonArrays; l++) {
        StorePolygonArray* theArray = dynamic_cast<StorePolygonArray*>(m->mPolygonArrays[l]);
        ioHash.U(theArray != NULL);
        if (!theArray)
            continue;
        ioHash.U(theArray->mIndices.size());
        ioHash.Bytes(theArray->mIndices.data(), theArray->mIndices.size() * sizeof(unsigned long));
        ioHash.U(theArray->mOutOfRange);
    }
    for (unsigned long f = 0; f < m->mFrameCount && m->mVertexArrays; f++) {
        size_t theFirst = FirstOf(m->mVertexArrays, f);
        ioHash.U(theFirst);
        StoreVertexArray* theArray = dynamic_cast<StoreVertexArray*>(m->mVertexArrays[f]);
        if (theFirst != f || !theArray)
            continue;
        ioHash.U(theArray->mType);
        ioHash.Floats(theArray->mPoints);
        ioHash.Floats(theArray->mNormals);
        ioHash.Floats(theArray->mUVs);
        ioHash.Floats(theArray->mColors);
        ioHash.Floats(theArray->mOpacities);
        ioHash.U(theArray->mOutOfRange);
    }
    for (unsigned long i = 0; i < m->mSubModelCount && m->mSubModels; i++) {
        ioHash.U(m->mSubModels[i] != NULL);
        if (m->mSubModels[i])
            HashModel(ioHash, m->mSubModels[i], m);
    }
}

};

unsigned long long StoreDigestModel(GEModel* inModel)
{
    Hash theHash;
    GEStoreDigest::HashModel(theHash, inModel, NULL);
    return theHash.mValue;
}

unsigned long long StoreDigestTexture(GETexture* inTexture)
{
    Hash theHash;
    GEStoreDigest::HashTexture(theHash, inTexture, true);
    return theHash.mValue;
}

unsigned long long StoreDigestTextureManager(GETextureManager* inManager)
{
    return GEStoreDigest::HashManager(inManager);
}

unsigned long long GEStoreDigest::HashManager(GETextureManager* inManager)
{
    // the cache's names are not compared (they lost their extension under D9): the textures' digests, sorted
    std::vector<unsigned long long> theDigests;
    for (const auto& theEntry : inManager->mTextures)
        theDigests.push_back(StoreDigestTexture(theEntry.second));
    std::sort(theDigests.begin(), theDigests.end());
    Hash theHash;
    for (unsigned long long d : theDigests)
        theHash.U(d);
    theHash.U(inManager->mMemoryFootprint);
    return theHash.mValue;
}
