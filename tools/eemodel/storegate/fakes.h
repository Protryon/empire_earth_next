// storegate: the engine's abstract classes the model and texture loaders call (GERasterizer, GEScreen, GESurface)
// as plain memory objects, so the game's own GEModelManager, GEModel, GETextureManager and GETexture code runs
// without a renderer. The vertex and polygon arrays are the game's own (GEVertexArray, GEPolygonArray, Rust arrays):
// with the capture on (ee_ge_store_capture; ee_render geval vertarr_capture.rs) each keeps a shadow of what was set,
// as the gate's own memory arrays kept it before (every field as floats, 32-bit indices, the sets past the end
// counted), and the digests read the shadows. tools/eemodel/storegate.py generates the members not written here
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

class StoreRasterizer : public GERasterizer {
public:
    StoreRasterizer() : GERasterizer(UString("storegate")) {}
    // the game's own arrays: what is set on them is the capture's (ee_ge_store_capture, digest.cpp)
    GEPolygonArray* CreatePolygonArray(unsigned long inCount) override { return new GEPolygonArray(inCount); }
    GEVertexArray* CreateVertexArray(unsigned long inType, unsigned long inCount) override
        { return new GEVertexArray(inType, inCount); }
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
