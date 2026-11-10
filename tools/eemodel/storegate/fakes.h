// storegate: the engine's abstract classes the model and texture loaders call (GERasterizer, GEScreen, GESurface,
// GEVertexArray, GEPolygonArray) as plain memory objects, so the game's own GEModelManager, GEModel, GETextureManager
// and GETexture code runs without a renderer. tools/eemodel/storegate.py generates the members not written here
// (fakes_generated.h: every other pure virtual, returning a zero value).
#ifndef STOREGATE_FAKES_H
#define STOREGATE_FAKES_H

#include <windows.h>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/GEColor.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/GERectangle.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/GERenderState.h"
#include "lle/GERasterizer.h"

// A surface in memory: pixel rows of GetBytesPerRow() bytes (a few bytes past the pixels, so that a loader's row skip
// shows), a MIP chain as attached surfaces (each level half the last, at least 1), zero-filled when created.
class StoreSurface : public GESurface {
public:
    StoreSurface(GEScreen* inScreen, const GESurfaceDescription& inDescription, long inLevel);
    ~StoreSurface() override;
    long AttachSurface(GESurface* inSurface) override { return kErrorFailed; }
    long CopyTo(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination, bool inTransparent) override;
    long CopyWithFilter(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination) override;
    long DetachSurface(unsigned long inIndex) override { return kErrorFailed; }
    unsigned long GetAllocatedSize() override;
    GESurface* GetAttachedSurface(unsigned long inIndex) override { return inIndex == 0 ? mNext : NULL; }
    unsigned long GetAttachedSurfaceCount() override { return mNext ? 1 : 0; }
    unsigned long GetBytesPerRow() const override { return mBytesPerRow; }
    unsigned long GetHeight() const override { return mDescription.mHeight >> mLogicalShift; }
    bool GetIs3DRenderable() const override { return mDescription.mIs3DRenderable; }
    bool GetIsTextLocked() const override { return false; }
    bool GetIsTransparencyEnabled() const override { return mDescription.mIsTransparencyEnabled; }
    unsigned char* GetMemoryAddress() const override { return mLocked ? const_cast<unsigned char*>(&mPixels[0]) : NULL; }
    GEScreen* GetParentScreen() const override { return mScreen; }
    unsigned long GetPixelFormat() const override { return mDescription.mPixelFormat; }
    unsigned long GetPixelDepth() const override { return mDescription.mPixelDepth; }
    unsigned long GetSurfaceVolatility() const override { return mDescription.mSurfaceVolatility; }
    const GESurfaceDescription& GetSurfaceDescription() const override { return mDescription; }
    const GEColor& GetTransparencyColor() const override { return mTransparency; }
    unsigned long GetType() const override { return mDescription.mType; }
    unsigned long GetWidth() const override { return mDescription.mWidth >> mLogicalShift; }
    long Lock() override { mLocked = true; mLockCount++; return kErrorOK; }
    long RestoreSurface() override { return kErrorOK; }
    void SetTransparencyColor(GEColor& inColor) override { mTransparency = inColor; }
    long TextLock() override { return kErrorOK; }
    long TextUnlock() override { return kErrorOK; }
    long Unlock() override { mLocked = false; return kErrorOK; }
#include "fakes_generated_surface.h"

    GEScreen* mScreen;
    GESurfaceDescription mDescription;
    unsigned long mBytesPerRow;
    std::vector<unsigned char> mPixels;
    StoreSurface* mNext;
    GEColor mTransparency;
    bool mLocked;
    unsigned long mLockCount;
    long mLevel;
};

class StoreScreen : public GEScreen {
public:
    StoreScreen() { this->mTextureManager = NULL; this->mRasterizerIndex = 0; }
    long CreateSurface(GESurfaceDescription& inDescription, GESurface** outSurface) override;
#include "fakes_generated_screen.h"
};

// The vertex and polygon arrays keep what was set; a set past the end is counted, not stored.
class StoreVertexArray : public GEVertexArray {
public:
    StoreVertexArray(unsigned long inType, unsigned long inCount);
    void CopyFrom(const GEVertexArray& inArray) override;
    void GetColor(GEColor& outColor, unsigned long inIndex) const override;
    void GetNormal(GEVector& outNormal, unsigned long inIndex) const override;
    float GetOpacity(unsigned long inIndex) const override;
    void GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const override;
    unsigned long GetSize() const override { return (unsigned long)mPoints.size(); }
    void GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const override;
    unsigned long GetType() const override { return mType; }
    void SetColor(const GEColor& inColor, unsigned long inIndex) override;
    void SetNormal(const GEVector& inNormal, unsigned long inIndex) override;
    void SetOpacity(float inOpacity, unsigned long inIndex) override;
    void SetPoint(const GE3DPoint& inPoint, unsigned long inIndex) override;
    void SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex) override;
    void SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                   const GETextureCoordinate& inCoordinate, float inOpacity, unsigned long inIndex) override;
    unsigned long SizeOf() const override { return (unsigned long)(mPoints.size() * 48); }

    unsigned long mType;
    std::vector<float> mPoints;     // x, y, z
    std::vector<float> mNormals;
    std::vector<float> mUVs;        // u, v
    std::vector<float> mColors;     // r, g, b
    std::vector<float> mOpacities;
    unsigned long mOutOfRange;
};

class StorePolygonArray : public GEPolygonArray {
public:
    StorePolygonArray(unsigned long inCount) : mIndices(3 * (size_t)inCount, 0), mOutOfRange(0) {}
    unsigned long GetSize() const override { return (unsigned long)(mIndices.size() / 3); }
    unsigned long GetVertexIndex1(unsigned long inPolygon) const override { return Get(inPolygon, 0); }
    unsigned long GetVertexIndex2(unsigned long inPolygon) const override { return Get(inPolygon, 1); }
    unsigned long GetVertexIndex3(unsigned long inPolygon) const override { return Get(inPolygon, 2); }
    void SetVertexIndex1(unsigned long inPolygon, unsigned long inIndex) override { Set(inPolygon, 0, inIndex); }
    void SetVertexIndex2(unsigned long inPolygon, unsigned long inIndex) override { Set(inPolygon, 1, inIndex); }
    void SetVertexIndex3(unsigned long inPolygon, unsigned long inIndex) override { Set(inPolygon, 2, inIndex); }
    void SetVertexIndices(unsigned long inPolygon, unsigned long inIndex1, unsigned long inIndex2,
                          unsigned long inIndex3) override
    {
        Set(inPolygon, 0, inIndex1);
        Set(inPolygon, 1, inIndex2);
        Set(inPolygon, 2, inIndex3);
    }
    unsigned long SizeOf() const override { return (unsigned long)(mIndices.size() * 4); }

    unsigned long Get(unsigned long inPolygon, int inCorner) const
    {
        size_t i = 3 * (size_t)inPolygon + inCorner;
        return i < mIndices.size() ? mIndices[i] : 0;
    }
    void Set(unsigned long inPolygon, int inCorner, unsigned long inIndex)
    {
        size_t i = 3 * (size_t)inPolygon + inCorner;
        if (i < mIndices.size())
            mIndices[i] = inIndex;
        else
            mOutOfRange++;
    }

    std::vector<unsigned long> mIndices;
    unsigned long mOutOfRange;
};

class StoreRasterizer : public GERasterizer {
public:
    StoreRasterizer() : GERasterizer(UString("storegate")) {}
    GEPolygonArray* CreatePolygonArray(unsigned long inCount) override { return new StorePolygonArray(inCount); }
    GEVertexArray* CreateVertexArray(unsigned long inType, unsigned long inCount) override
        { return new StoreVertexArray(inType, inCount); }
    long ActivateRasterizer() override { return kErrorOK; }
    long CreateTextureHandle(GETexture* inTexture) override { return kErrorOK; }
    void DestroyTextureHandle(GETexture* inTexture) override {}
#include "fakes_generated_rasterizer.h"
};

// digest.cpp: the stored state, read through the classes' members
unsigned long long StoreDigestModel(GEModel* inModel);
unsigned long long StoreDigestTexture(GETexture* inTexture);
unsigned long long StoreDigestTextureManager(GETextureManager* inManager);

#endif
