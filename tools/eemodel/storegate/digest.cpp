// storegate's digests of what a GEModel and a GETexture store: a model read through its members (GEStoreDigest is a
// friend of GEModel and GETexture; a material through its accessors), a texture and the manager's cache through their accessors. FNV-1a over
// every value, floats by their bits; an array shared with an earlier frame is hashed as that frame's number.
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
    ioHash.U(inTexture->GetIsManaged());
    const GETStage& s = *inTexture->GetStage();
    ioHash.U(s.mTexture == inTexture);
    ioHash.U(s.mColorMode);
    ioHash.U(s.mAlphaMode);
    ioHash.U(s.mTextureAddressingMode);
    ioHash.U(s.mMinFilterMode);
    ioHash.U(s.mMagFilterMode);
    ioHash.U(s.mMipFilterMode);
    ioHash.F(s.mMipMapBias);
    ioHash.F(inTexture->GetFrameTopLeftCorner().GetU());
    ioHash.F(inTexture->GetFrameTopLeftCorner().GetV());
    ioHash.U(inTexture->GetCurrentFrameTexture() == inTexture);
    ioHash.U(inTexture->GetOwnData() != NULL);
    ioHash.F(inTexture->GetFrameUVSize());
    ioHash.U(inTexture->GetFrameFormat());
    ioHash.U(inTexture->GetFrame());
    ioHash.U(inTexture->GetFrameCount());
    ioHash.U(inTexture->GetFramesPerRow());
    ioHash.U((unsigned long long)(long long)(long)inTexture->GetMipLevelCount());
    ioHash.U(inTexture->GetNumMipLevelsToDrop());
    ioHash.U(inTexture->GetOwnIsCompressed());
    ioHash.U(inTexture->GetOwnIsPromoted());
    ioHash.U(inTexture->HasChanged());
    ioHash.U(inTexture->GetCreateMIPMaps());
    ioHash.U(inTexture->GetIsFromTexturePack());
    ioHash.U((unsigned long long)(long long)inTexture->GetTexturePackMipLevels());
    ioHash.U(inTexture->GetReferenceCount());
    ioHash.U(inTexture->GetAllocatedSize());
    HashSurface(ioHash, inTexture->GetSurface());
    if (inFrames && inTexture->GetFrameFormat() == 0 && inTexture->GetFrames()) {
        ioHash.U(inTexture->GetFrames()[0] == inTexture);
        for (unsigned long i = 1; i < inTexture->GetFrameCount(); i++)
            HashTexture(ioHash, inTexture->GetFrames()[i], false);
    } else
        ioHash.U(inTexture->GetFrames() != NULL);
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
    ioHash.U(m->GetParentModel() == inParent);
    ioHash.U(m->GetReferenceCount());
    ioHash.Transform(m->GetGraphicsPreModelTransform());
    ioHash.Transform(m->GetGraphicsPostModelTransform());
    ioHash.Transform(m->GetGraphicsTransform());
    ioHash.Point(m->GetCenterPoint());
    ioHash.S(m->GetName());
    ioHash.U(m->GetSharedFrameCount());
    for (unsigned long i = 0; i < m->GetSharedFrameCount(); i++) {
        unsigned long theFrame;
        long theOffset;
        m->GetSharedFrame(i, theFrame, theOffset);
        ioHash.U(theFrame);
        ioHash.U((unsigned long long)(long long)theOffset);
    }
    ioHash.F(m->GetGraphicsScale());
    ioHash.F(m->GetModelOpacity());
    ioHash.F(m->GetFractionalFramePosition());
    ioHash.U(m->GetGraphicsCurrentFrame());
    ioHash.U(m->GetGraphicsNextFrame());
    ioHash.U(m->GetSubModelCount());
    ioHash.U(m->GetFrameCount());
    ioHash.U(m->GetMaterialCount());
    ioHash.U(m->GetPolygonCount());
    ioHash.U(m->GetVertexCount());
    ioHash.U(m->GetTagPointCount());
    ioHash.U(m->GetLODCount());
    for (int i = 0; i < 6; i++)
        ioHash.U(m->GetClipToPlane(i));
    ioHash.U(m->SizeOf());
    for (unsigned long f = 0; f < m->GetFrameCount() && m->GetInternalTransforms(); f++)
        ioHash.Transform(m->GetInternalTransforms()[f]);
    for (unsigned long f = 0; f < m->GetFrameCount() && m->GetBoundingBoxes(); f++) {
        const GEBoundingBox& b = m->GetBoundingBoxes()[f];
        ioHash.Point(b.GetBottomLeftFront());
        ioHash.Point(b.GetTopRightBack());
        ioHash.F(b.GetWidth());
        ioHash.F(b.GetHeight());
        ioHash.F(b.GetDepth());
    }
    if (m->GetGraphicsBoundingBoxPointer()) {
        ioHash.Point(m->GetGraphicsBoundingBoxPointer()->GetBottomLeftFront());
        ioHash.Point(m->GetGraphicsBoundingBoxPointer()->GetTopRightBack());
    }
    for (unsigned long f = 0; f < m->GetFrameCount() && m->GetBoundingRadii(); f++)
        ioHash.F(m->GetBoundingRadii()[f]);
    for (unsigned long i = 0; i < m->GetTagPointCount(); i++) {
        if (m->GetTagPointList())
            ioHash.Point(m->GetTagPointList()[i]);
        if (m->GetTagPointNames())
            ioHash.S(m->GetTagPointNames()[i]);
    }
    for (unsigned long f = 0; f < m->GetFrameCount() && m->GetFrameTagPoints(); f++) {
        size_t theFirst = FirstOf(m->GetFrameTagPoints(), f);
        ioHash.U(theFirst);
        if (theFirst == f && m->GetFrameTagPoints()[f])
            for (unsigned long i = 0; i < m->GetTagPointCount(); i++)
                ioHash.Point(m->GetFrameTagPoints()[f][i]);
    }
    for (unsigned long i = 0; i < m->GetMaterialCount() && m->GetMaterials(); i++) {
        GEMaterial* theMaterial = m->GetMaterials()[i];
        ioHash.S(theMaterial->GetName());
        ioHash.F(theMaterial->GetColor().GetRed());
        ioHash.F(theMaterial->GetColor().GetGreen());
        ioHash.F(theMaterial->GetColor().GetBlue());
        unsigned long theScreens = ee_ge_mat_get(theMaterial->GetRust(), EeGeMatField::ScreenCount, 0);
        ioHash.U(theScreens);
        for (unsigned long k = 0; k < theScreens; k++)
            ioHash.U(theMaterial->GetTexture(k) != NULL);
        ioHash.U(theMaterial->GetLODCount());
        for (unsigned long k = 0; k < theMaterial->GetLODCount(); k++) {
            ioHash.U(theMaterial->GetPolygonStartingIndex(k));
            ioHash.U(theMaterial->GetPolygonRangeLength(k));
        }
        ioHash.U(theMaterial->GetVertexStartingIndex());
        ioHash.U(theMaterial->GetVertexRangeLength());
        ioHash.U(theMaterial->GetTextureIndex());
        ioHash.F(theMaterial->GetOpacity());
        if (m->GetTextureFilenames())
            ioHash.S(m->GetTextureFilenames()[i]);
    }
    for (unsigned long l = 0; l < m->GetLODCount() && m->GetPolygonArrays(); l++) {
        StorePolygonArray* theArray = dynamic_cast<StorePolygonArray*>(m->GetPolygonArrays()[l]);
        ioHash.U(theArray != NULL);
        if (!theArray)
            continue;
        ioHash.U(theArray->mIndices.size());
        ioHash.Bytes(theArray->mIndices.data(), theArray->mIndices.size() * sizeof(unsigned long));
        ioHash.U(theArray->mOutOfRange);
    }
    for (unsigned long f = 0; f < m->GetFrameCount() && m->GetVertexArrays(); f++) {
        size_t theFirst = FirstOf(m->GetVertexArrays(), f);
        ioHash.U(theFirst);
        StoreVertexArray* theArray = dynamic_cast<StoreVertexArray*>(m->GetVertexArrays()[f]);
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
    for (unsigned long i = 0; i < m->GetSubModelCount() && m->GetSubModels(); i++) {
        ioHash.U(m->GetSubModels()[i] != NULL);
        if (m->GetSubModels()[i])
            HashModel(ioHash, m->GetSubModels()[i], m);
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
    for (unsigned long i = 0; i < inManager->GetTextureCount(); i++)
        theDigests.push_back(StoreDigestTexture(inManager->GetTextureAt(i)));
    std::sort(theDigests.begin(), theDigests.end());
    Hash theHash;
    for (unsigned long long d : theDigests)
        theHash.U(d);
    theHash.U(inManager->GetMemoryFootprint());
    return theHash.mValue;
}
