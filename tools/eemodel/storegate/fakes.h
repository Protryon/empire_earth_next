// storegate: the engine's renderer objects the model and texture loaders call (GEScreen, GESurface: their Rust
// objects' C functions, defined by storegate.cpp) as plain memory objects, so the game's own GEModelManager, GEModel, GETextureManager and GETexture code runs
// without a renderer. The vertex and polygon arrays are the game's own (GEVertexArray, GEPolygonArray, Rust arrays):
// with the capture on (ee_ge_store_capture; ee_render geval vertarr_capture.rs) each keeps a shadow of what was set,
// as the gate's own memory arrays kept it before (every field as floats, 32-bit indices, the sets past the end
// counted), and the digests read the shadows.
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

// A surface in memory, the state behind a GESurface (its mDX7: storegate.cpp defines the ee_dx7sf_* functions the
// surface's members call, which the link takes before the Rust library's, /FORCE:MULTIPLE): pixel rows of
// mBytesPerRow bytes (a few bytes past the pixels, so that a loader's row skip shows), a MIP chain as attached
// surfaces (each level half the last, at least 1), zero-filled when created.
struct StoreSurface {
    StoreSurface(GESurface* inOwner, GEScreen* inScreen, const GESurfaceDescription& inDescription, long inLevel);
    ~StoreSurface();
    static StoreSurface* Of(GESurface* inSurface) { return reinterpret_cast<StoreSurface*>(inSurface->mDX7); }

    GESurface* mOwner;
    GEScreen* mScreen;
    GESurfaceDescription mDescription;
    unsigned long mBytesPerRow;
    std::vector<unsigned char> mPixels;
    GESurface* mNext;
    GEColor mTransparency;
    bool mLocked;
    unsigned long mLockCount;
    long mLevel;
};

// The screen behind a GEScreen (the ee_dx7sc_* functions): it creates the memory surfaces
struct StoreScreen {
    GEScreen* mOwner;
};

// digest.cpp: the stored state, read through the classes' members
unsigned long long StoreDigestModel(GEModel* inModel);
unsigned long long StoreDigestTexture(GETexture* inTexture);
unsigned long long StoreDigestTextureManager(GETextureManager* inManager);

#endif
