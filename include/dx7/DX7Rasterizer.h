// DX7Rasterizer (RECONSTRUCTION): the GERasterizer of the DX7 renderer plug-ins, plus the headers every DX7 unit
// includes. Slot order from the vtable 0x10010324 and the exports. The original's sizeof 0x7c (copy ctor 0x10001271):
// GERasterizer 0x68, the per-screen DX7 state vector +0x68, the debug color +0x78.
//
// Whole in Rust (rust/crates/ee_render/src/dx7 rastobj.rs, R4.3 part 13): the per-screen DX7 records, the debug colour, the light
// slots and every method; DX7TnLRasterizer is the object's T&L flavour (inTnL). The class keeps the vtable the game
// calls through GERasterizer and one member; DX7Rasterizer's own four virtuals (GetIsScreenValid, CreateVertexBuffers,
// DrawTerrainMaterial, GetVideoMemory) and the protected LockVB/Set2DMode/Set3DMode/UnlockVB are Rust's.
#ifndef DX7_DX7RASTERIZER_H
#define DX7_DX7RASTERIZER_H

#include <vector>
#include <utility>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/U2DPolygon.h"
#include "lle/GERasterizer.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/GEModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEMovie.h"
#include "lle/GELight.h"
#include "lle/U2DSparseArray.h"

#include "dx7/DX7Common.h"
#include "lle/GEVertexArray.h"

struct EeDx7Rasterizer;                     // rust/ee_dx7.h

class DX7_API DX7Rasterizer : public GERasterizer {
public:
    DX7Rasterizer(const UString& inName, bool inTnL = false);
    virtual ~DX7Rasterizer();

    virtual long ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                unsigned long inMode, bool inFullScreen, unsigned long inBackBuffers);
    virtual long ActivateRenderingSurface(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                          unsigned long inWidth, unsigned long inHeight);
    virtual long AddLight(GELight& inLight, unsigned long inScreen, unsigned long& outIndex);
    virtual long AddValidScreen(GEScreen* inScreen);
    virtual long BeginScene(GESurface* inTarget, unsigned long inScreen);
    virtual bool CanDoCurrentState(GETexture* inTexture, unsigned long inScreen);
    virtual long Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen);
    virtual long ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen);
    virtual GEMovie* CreateMovie(const FSFileSpec& inFile, unsigned long inScreen);
    virtual GEMovie* CreateMovie(const FSFileSpec& inFile, unsigned long inScreen, const GERectangle& inRectangle,
                                 bool inLoop);
    virtual GEPolygonArray* CreatePolygonArray(unsigned long inCount);
    virtual GEVertexArray* CreateVertexArray(unsigned long inType, unsigned long inCount);
    virtual long Deactivate();
    long Draw(GELine* inLine, unsigned long inScreen) { return this->Draw(inLine, 1, inScreen); }
    long Draw(GEPoint* inPoint, unsigned long inScreen) { return this->Draw(inPoint, 1, inScreen); }
    virtual long Draw(GEBitmap* inBitmap, unsigned long inScreen);
    virtual long Draw(GELine* inLine, unsigned long inCount, unsigned long inScreen);
    virtual long Draw(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inSorted);
    virtual long Draw(GEPoint* inPoint, unsigned long inCount, unsigned long inScreen);
    virtual long Draw(GERhombus* inRhombus, unsigned long inScreen);
    virtual long Draw(GEText* inText, GESurface* inTarget);
    virtual long Draw(GEText* inText, const GERectangle& inRectangle, GESurface* inTarget);
    virtual long Draw(GETerrainMesh& inMesh, unsigned long inScreen, bool inSorted);
    virtual long Draw(GEPolygonArray& inPolygons, unsigned long inCount, GEVertexArray& inVertices,
                      unsigned long inVertexCount, GETexture* inTexture, unsigned long inScreen, bool inSorted);
    virtual long Draw3DLine(std::vector<std::pair<GE3DPoint, float> >& inPoints, GETexture* inTexture,
                            float inWidth, unsigned long inScreen, bool inSorted);
    virtual long DrawGridLines(GETerrainMesh& inMesh,
                               std::vector<std::pair<U2DPoint<long>, U2DPoint<long> > >& inLines,
                               GETexture* inTexture, float inWidth, unsigned long inScreen, bool inSorted);
    virtual long DrawSky(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inSorted);
    virtual long DrawTerrainGrid(GETerrainMesh& inMesh, unsigned long inScreen, bool inSorted);
    virtual long DrawTerrainHighlight(GETerrainMesh& inMesh, const U2DRectangle<long>& inRectangle,
                                      unsigned long inScreen, bool inSorted, const GEColor& inColor,
                                      float inOpacity);
    virtual long DrawWater(GETerrainMesh& inMesh, unsigned long inScreen, bool inSorted);
    virtual long EndScene(unsigned long inScreen);
    virtual long GetAvailableVideoMemory(unsigned long inScreen, unsigned long& outBytes);
    virtual long GetTotalVideoMemory(unsigned long inScreen, unsigned long& outBytes);
    virtual unsigned long GetCaps(unsigned long inCap, unsigned long inScreen);
    virtual bool GetCanModulateTextureAlpha(unsigned long inScreen);
    virtual bool GetDirtyRectangleSupport(unsigned long inScreen);
    virtual void Light(GETerrainMesh& inMesh, unsigned long inScreen);
    virtual long RemoveAllLights(unsigned long inScreen);
    virtual long RemoveLight(unsigned long inIndex, unsigned long inScreen);
    virtual void SetAlphaTesting(unsigned long inState, unsigned long inScreen);
    virtual void SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen);
    virtual void SetAlphaTestingRefValue(float inValue, unsigned long inScreen);
    virtual void SetAmbientColor(const GEColor& inColor, unsigned long inScreen);
    virtual void SetAmbientIntensity(float inIntensity, unsigned long inScreen);
    virtual void SetAntiAliasing(unsigned long inState, unsigned long inScreen);
    virtual void SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen);
    virtual void SetColorKeying(unsigned long inState, unsigned long inScreen);
    virtual void SetCulling(unsigned long inState);
    virtual void SetDebugColor(const GEColor& inColor);
    virtual void SetDithering(unsigned long inState, unsigned long inScreen);
    virtual void SetDrawStyle(unsigned long inStyle, unsigned long inScreen);
    virtual void SetFog(unsigned long inState, unsigned long inScreen);
    virtual void SetFogColor(const GEColor& inColor, unsigned long inScreen);
    virtual void SetFogRange(float inStart, float inEnd, unsigned long inScreen);
    virtual void SetLighting(unsigned long inState, unsigned long inScreen);
    virtual void SetShading(unsigned long inState, unsigned long inScreen);
    virtual void SetShadingColorSource(unsigned long inSource, unsigned long inScreen);
    virtual void SetSpecular(unsigned long inState, unsigned long inScreen);
    virtual void SetTexture(GETexture* inTexture, unsigned long inScreen);
    virtual void SetViewport(const GEViewport& inViewport, unsigned long inScreen);
    virtual void SetZBuffer(unsigned long inState, unsigned long inScreen);
    virtual void SetZCompare(unsigned long inState, unsigned long inScreen);
    virtual long ShowScene(unsigned long inScreen);
    virtual void TransformToCamera(GETerrainMesh& inMesh, GEViewport& inViewport, float inScale, bool inA, bool inB);
    virtual long UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen);

protected:
    virtual long ActivateRasterizer();
    virtual long ActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary,
                                   GETextureManager* inManager, GEWindow* inWindow);
    virtual long CreateTextureHandle(GETexture* inTexture);
    virtual long DemoteTexture(GETexture* inTexture);
    virtual void DestroyTextureHandle(GETexture* inTexture);
    virtual long DrawModelHierarchy(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inSorted);
    virtual long PromoteTexture(GETexture* inTexture);
    virtual void SetRenderTarget(GESurface* inTarget, unsigned long inScreen);

    EeDx7Rasterizer* mDX7;                  // the rasterizer (ee_dx7 rastobj.rs)

    friend struct DX7RasterizerHost;
    DX7Rasterizer(const DX7Rasterizer&) = delete;
    DX7Rasterizer& operator=(const DX7Rasterizer&) = delete;
};

#endif
