// DX7Rasterizer (RECONSTRUCTION): the GERasterizer of the DX7 renderer plug-ins, plus the headers every DX7 unit
// includes. Slot order from the vtable 0x10010324 and the exports; the four virtuals after GERasterizer's (0x138..)
// are DX7Rasterizer's own (DX7TnLRasterizer overrides GetIsScreenValid and CreateVertexBuffers). sizeof 0x7c (copy
// ctor 0x10001271): GERasterizer 0x68, the per-screen DX7 state vector +0x68, the debug color +0x78.
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
#include "dx7/DX7StdColorVertexArray.h"

class DX7Screen;
struct GETMPolygon;

// The texture stage 0 state the rasterizer last set (sizeof 0x30; SetTexture 0x1000bec0 copies it to a local with
// rep movsd). Names provisional.
struct DX7RTextureStage {
    GXSurface* mTexture;          // +0x00
    unsigned long mColorMode;               // +0x04 GETStage::mColorMode
    unsigned long mAlphaMode;               // +0x08 GETStage::mAlphaMode
    unsigned long mColorArgument1;          // +0x0c GXTSS_COLORARG1
    unsigned long mColorArgument2;          // +0x10 GXTSS_COLORARG2
    unsigned long mAlphaArgument1;          // +0x14 GXTSS_ALPHAARG1
    unsigned long mAlphaArgument2;          // +0x18 GXTSS_ALPHAARG2
    unsigned long mTextureAddressingMode;   // +0x1c
    unsigned long mMinFilterMode;           // +0x20
    unsigned long mMagFilterMode;           // +0x24
    unsigned long mMipFilterMode;           // +0x28
    float mMipMapBias;                      // +0x2c
};

// The rasterizer's DX7 state for one screen (sizeof 0x74, AddValidScreen 0x1000a5ff). Names provisional. The device
// state caches mirror D3D render/texture-stage states so the setters skip redundant calls (ActivateTheScreen 0x1000a0b2
// seeds them; -16 forces the next set).
struct DX7RScreenState {
    DX7RScreenState(GEScreen* inScreen)
    {
        this->mScreen = inScreen;
        this->mDirect3D = NULL;
        this->mDevice = NULL;
        this->mVertexBuffer = NULL;
        this->mVertexBufferPosition = 0;
        this->mViewportChanged = true;
    }

    // inline accessor (not exported: the struct is the plug-in's own). DX7Rasterizer.cpp reaches the device through it:
    // the extra inline level is what makes the budget refuse every _Ptrit::operator+ in SetRenderTarget (0x1000bb66,
    // exact only with it at the SetViewport site) and moves SetTexture/SetBlending/SetZBuffer closer. The DrawModel/
    // DrawTerrain units keep ->mDevice (DrawModelHierarchy 0x100042c1 is exact only with at most two accessor sites).
    GXDevice* GetDevice() { return this->mDevice; }

    GXVIEWPORT mViewport;                 // +0x00
    GXDirect3D* mDirect3D;                  // +0x18
    GXDevice* mDevice;              // +0x1c
    GEScreen* mScreen;                      // +0x20
    GXVertexBuffer* mVertexBuffer;  // +0x24
    unsigned long mVertexBufferPosition;    // +0x28 vertices used (LockVB: 0x960 per buffer)
    unsigned long mStage1ColorOperation;    // +0x2c GXTSS_COLOROP of stage 1
    DX7RTextureStage mStage;                // +0x30 stage 0 (SetTexture copies it whole)
    unsigned long mBlendSource;             // +0x60
    unsigned long mBlendDestination;        // +0x64
    unsigned long mZEnable;                 // +0x68
    bool mIs3DMode;                         // +0x6c
    bool mViewportChanged;                  // +0x6d
    bool mZWriteEnable;                     // +0x6e
    bool mAlphaBlendEnable;                 // +0x6f
    bool mCanModulateTextureAlpha;          // +0x70
    bool mDirtyRectangleSupport;            // +0x71
};

class DX7_API DX7Rasterizer : public GERasterizer {
public:
    DX7Rasterizer(const UString& inName);
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
    virtual void SetDisplayNoTextureMode(bool inState, unsigned long inScreen);
    virtual void SetDisplayOverdrawMode(bool inState, unsigned long inScreen);
    virtual void SetDithering(unsigned long inState, unsigned long inScreen);
    virtual void SetDrawStyle(unsigned long inStyle, unsigned long inScreen);
    virtual void SetFog(unsigned long inState, unsigned long inScreen);
    virtual void SetFogColor(const GEColor& inColor, unsigned long inScreen);
    virtual void SetFogRange(float inStart, float inEnd, unsigned long inScreen);
    virtual void SetFogType(unsigned long inType, unsigned long inScreen);
    virtual void SetFogValue(float inValue, unsigned long inScreen);
    virtual void SetLighting(unsigned long inState, unsigned long inScreen);
    virtual void SetShading(unsigned long inState, unsigned long inScreen);
    virtual void SetShadingColorSource(unsigned long inSource, unsigned long inScreen);
    virtual void SetSpecular(unsigned long inState, unsigned long inScreen);
    virtual void SetTexture(GETexture* inTexture, unsigned long inScreen);
    virtual void SetViewport(const GEViewport& inViewport, unsigned long inScreen);
    virtual void SetWaitForVSync(bool inState, unsigned long inScreen);
    virtual void SetZBuffer(unsigned long inState, unsigned long inScreen);
    virtual void SetZCompare(unsigned long inState, unsigned long inScreen);
    virtual long ShowScene(unsigned long inScreen);
    virtual void TransformToCamera(GETerrainMesh& inMesh, GEViewport& inViewport, float inScale, bool inA, bool inB);
    virtual long UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen);
    virtual bool GetIsScreenValid(DX7Screen* inScreen);                                     // slot 0x138

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
    virtual long CreateVertexBuffers(unsigned long inScreen);                               // slot 0x13c
    virtual long DrawTerrainMaterial(GEViewport* inViewport,
                                     std::vector<GETerrainMesh::GETMPolygon*>& inPolygons,
                                     unsigned long inScreen);                               // slot 0x140
    virtual long GetVideoMemory(unsigned long inScreen, unsigned long& outTotal, unsigned long& outFree);   // 0x144

    float ComputePolyAreaOfCurrentVB(unsigned long inScreen);
    void ConvertToD3DMatrix(const GETransformation& inTransformation, GXMATRIX& outMatrix);
    void CopyAndSmoothStdColorVertices(GEModel* inModel, GEMaterial* inMaterial, GXVERTEX_STD_COLOR* outVertices);
    void CopyAndSmoothStdVertices(GEModel* inModel, GEMaterial* inMaterial, GXVERTEX_STD_COLOR* outVertices);
    void CopyStdColorVertices(GEModel* inModel, GEMaterial* inMaterial, GXVERTEX_STD_COLOR* outVertices);
    void CopyStdVertices(GEModel* inModel, GEMaterial* inMaterial, GXVERTEX_STD_COLOR* outVertices);
    long LockVB(unsigned long inCount, unsigned long inScreen, GXVERTEX_STD_COLOR*& outVertices);
    long Set2DMode(unsigned long inScreen);
    long Set3DMode(unsigned long inScreen);
    long UnlockVB(unsigned long inScreen);

    std::vector<DX7RScreenState*> mDX7ScreenStates;     // +0x68
    unsigned long mDebugColor;              // +0x78 (ctor 0xff000040)
};

#endif
