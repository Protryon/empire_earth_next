// C:\EEX\Low-Level Engine\Graphics Engine\GERasterizer.h (RECONSTRUCTION; GERasterizer.cpp is a __FILE__ anchor at
// ~0x10023807). Abstract base of the renderer plug-ins (DX7HRDisplay DX7Rasterizer 0x10324 gives slot order and
// signatures). The accessors are inline (GEBitmap.cpp emits them first, 0x1000ed7c..).
#ifndef LLE_GERASTERIZER_H
#define LLE_GERASTERIZER_H

#include <vector>
#include <utility>
#include "lle/GERenderState.h"

class FSFileSpec;
class GEBitmap;
class GEFont;
class GELight;
class GELine;
class GEModel;
class GEMovie;
class GEPhysicalModel;
class GEPoint;
class GEPolygonArray;
class GERhombus;
class GETerrainMesh;
class GETexture;
class GEText;
class TSCriticalSection;
template <class T> class U2DPoint;         // enough for DrawLines' parameter (game units that do not include
                                           // lle/U2DPoint.h never construct its serialization keys)
template <class T> class U2DRectangle;
struct RTSMapTileRun;

struct GERFogAttributes {                   // sizeof 0x20 (GetFogAttributes 0xeeb4; ctor COMDAT 0x23aca)
    GERFogAttributes()
    {
        this->mType = 0;
        this->mDensity = 1.0f;
        this->mStart = 0.0f;
        this->mEnd = 1.0f;
        this->mScale = 1.0f;
    }

    GEColor mColor;                         // +0x00
    unsigned long mType;                    // +0x0c
    float mDensity;                         // +0x10 (SetFogValue)
    float mStart;                           // +0x14 (SetFogRange)
    float mEnd;                             // +0x18
    float mScale;                           // +0x1c 1 / (mEnd - mStart)
};

// The rasterizer's state for one screen (provisional name; offsets proven by the inline accessors and setters).
// The ctor/dtor are inline: GERasterizer.cpp emits them after AddValidScreen (0x23a0e) and Deactivate (0x23b6a).
struct GERScreenState {
    GERScreenState(GEScreen* inScreen)
    {
        this->mScreen = inScreen;
        this->mPrimarySurface = NULL;
        this->mSecondarySurface = NULL;
        this->mRenderTarget = NULL;
        this->mTexture = NULL;
        this->mWindow = NULL;
        this->mDrawnPolyArea = 0.0f;
        this->mDrawnPolyAreaPerFrame = 0.0f;
        this->mDrawnPolyAreaPerSecond = 0.0f;
        this->mDrawnPolysPerFrame = 0.0f;
        this->mDrawnPolysPerSecond = 0.0f;
        this->mFramesPerSecond = 0.0f;
        this->mScenePolysPerFrame = 0.0f;
        this->mScenePolysPerSecond = 0.0f;
        this->mTotalTextureChangesPerFrame = 0.0f;
        this->mDrawnPolyCount = 0;
        this->m32C[0] = 0;
        this->m32C[1] = 0;
        this->m32C[2] = 0;
        this->m32C[3] = 0;
        this->m33C = false;
        this->mIsScreenActivated = false;
        this->mWaitForVSync = true;
    }
    ~GERScreenState()
    {
        if (this->mPrimarySurface != NULL)
            this->mPrimarySurface->Dereference();
        if (this->mSecondarySurface != NULL)
            this->mSecondarySurface->Dereference();
        if (this->mRenderTarget != NULL)
            this->mRenderTarget->Dereference();
        if (this->mTexture != NULL)
            this->mTexture->Dereference();
    }

    GEViewport mViewport;                   // +0x000 (GetViewport returns the element)
    GERFogAttributes mFog;                  // +0x288
    GEScreen* mScreen;                      // +0x2a8
    GESurface* mPrimarySurface;             // +0x2ac
    GESurface* mSecondarySurface;           // +0x2b0
    GESurface* mRenderTarget;               // +0x2b4
    GETexture* mTexture;                    // +0x2b8 (provisional name)
    GEWindow* mWindow;                      // +0x2bc
    float mAlphaTestingRefValue;            // +0x2c0
    float mDrawnPolyArea;                   // +0x2c4
    float mDrawnPolyAreaPerFrame;           // +0x2c8
    float mDrawnPolyAreaPerSecond;          // +0x2cc
    float mDrawnPolysPerFrame;              // +0x2d0
    float mDrawnPolysPerSecond;             // +0x2d4
    float mFramesPerSecond;                 // +0x2d8
    float mScenePolysPerFrame;              // +0x2dc
    float mScenePolysPerSecond;             // +0x2e0
    float mTotalTextureChangesPerFrame;     // +0x2e4
    unsigned long mAlphaTesting;            // +0x2e8
    unsigned long mAlphaTestingCompareState;   // +0x2ec
    unsigned long mAntiAliasing;            // +0x2f0
    unsigned long mBlendSource;             // +0x2f4
    unsigned long mBlendDestination;        // +0x2f8
    unsigned long mColorKeying;             // +0x2fc
    unsigned long mDithering;               // +0x300
    unsigned long mDrawStyle;               // +0x304
    unsigned long mFogState;                // +0x308 (SetFog)
    unsigned long mLighting;                // +0x30c
    unsigned long mProjectionMode;          // +0x310
    unsigned long mShading;                 // +0x314
    unsigned long mShadingColorSource;      // +0x318
    unsigned long mSpecular;                // +0x31c
    unsigned long mZBuffer;                 // +0x320
    unsigned long mZCompare;                // +0x324
    unsigned long mDrawnPolyCount;          // +0x328
    unsigned long m32C[4];                  // +0x32c
    bool m33C;                              // +0x33c
    bool mIsScreenActivated;                // +0x33d
    bool mWaitForVSync;                     // +0x33e
};

class LLE_API GERasterizer {                // sizeof 0x68 (vector deleting dtor 0xf279)
public:
    struct TransformStack;

    virtual long ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                unsigned long inMode, bool inFullScreen, unsigned long inBackBuffers) = 0;
    virtual long ActivateRenderingSurface(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                          unsigned long inWidth, unsigned long inHeight) = 0;
    virtual long AddLight(GELight& inLight, unsigned long inScreen, unsigned long& outIndex) = 0;
    virtual long AddValidScreen(GEScreen* inScreen);
    virtual long BeginScene(GESurface* inTarget, unsigned long inScreen) = 0;
    virtual bool CanDoCurrentState(GETexture* inTexture, unsigned long inScreen) = 0;
    virtual long Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen) = 0;
    virtual long ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen) = 0;
    virtual GEMovie* CreateMovie(const FSFileSpec& inFile, unsigned long inScreen) = 0;
    virtual GEMovie* CreateMovie(const FSFileSpec& inFile, unsigned long inScreen, const GERectangle& inRectangle,
                                 bool inLoop) = 0;
    virtual GEPolygonArray* CreatePolygonArray(unsigned long inCount) = 0;
    virtual GEVertexArray* CreateVertexArray(unsigned long inType, unsigned long inCount) = 0;   // slot 0x2c
    virtual long Deactivate();
    virtual long Draw(GEBitmap* inBitmap, unsigned long inScreen) = 0;                           // slot 0x54
    virtual long Draw(GELine* inLine, unsigned long inCount, unsigned long inScreen) = 0;        // slot 0x50
    virtual long Draw(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inSorted) = 0;
    virtual long Draw(GEPoint* inPoint, unsigned long inCount, unsigned long inScreen) = 0;      // slot 0x48
    virtual long Draw(GERhombus* inRhombus, unsigned long inScreen) = 0;
    virtual long Draw(GEText* inText, GESurface* inTarget) = 0;
    virtual long Draw(GEText* inText, const GERectangle& inRectangle, GESurface* inTarget) = 0;
    virtual long Draw(GETerrainMesh& inMesh, unsigned long inScreen, bool inSorted) = 0;
    virtual long Draw(GEPolygonArray& inPolygons, unsigned long inCount, GEVertexArray& inVertices,
                      unsigned long inVertexCount, GETexture* inTexture, unsigned long inScreen, bool inSorted) = 0;
    virtual long Draw3DLine(std::vector<std::pair<GE3DPoint, float> >& inPoints, GETexture* inTexture,
                            float inWidth, unsigned long inScreen, bool inSorted) = 0;
#ifdef LLE_EXE_HEADERS
    virtual long DrawGridLines(GETerrainMesh& inMesh, std::vector<RTSMapTileRun>& inLines,
#else
    virtual long DrawGridLines(GETerrainMesh& inMesh,
                               std::vector<std::pair<U2DPoint<long>, U2DPoint<long> > >& inLines,
#endif
                               GETexture* inTexture, float inWidth, unsigned long inScreen, bool inSorted) = 0;
    virtual long DrawSky(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inSorted) = 0;
    virtual long DrawTerrainGrid(GETerrainMesh& inMesh, unsigned long inScreen, bool inSorted) = 0;
    virtual long DrawTerrainHighlight(GETerrainMesh& inMesh, const U2DRectangle<long>& inRectangle,
                                      unsigned long inScreen, bool inSorted, const GEColor& inColor,
                                      float inOpacity) = 0;
    virtual long DrawWater(GETerrainMesh& inMesh, unsigned long inScreen, bool inSorted) = 0;
    virtual long EndScene(unsigned long inScreen) = 0;
    virtual long GetAvailableVideoMemory(unsigned long inScreen, unsigned long& outBytes) = 0;
    virtual long GetTotalVideoMemory(unsigned long inScreen, unsigned long& outBytes) = 0;
    virtual unsigned long GetCaps(unsigned long inCap, unsigned long inScreen) = 0;
    virtual bool GetCanModulateTextureAlpha(unsigned long inScreen) = 0;
    virtual bool GetDirtyRectangleSupport(unsigned long inScreen) = 0;
    virtual void Light(GETerrainMesh& inMesh, unsigned long inScreen) = 0;
    virtual long RemoveAllLights(unsigned long inScreen) = 0;
    virtual long RemoveLight(unsigned long inIndex, unsigned long inScreen) = 0;
    virtual void SetAlphaTesting(unsigned long inState, unsigned long inScreen);
    virtual void SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen);
    virtual void SetAlphaTestingRefValue(float inValue, unsigned long inScreen);
    virtual void SetAmbientColor(const GEColor& inColor, unsigned long inScreen);
    virtual void SetAmbientIntensity(float inIntensity, unsigned long inScreen);
    virtual void SetAntiAliasing(unsigned long inState, unsigned long inScreen);
    virtual void SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen);
    virtual void SetColorKeying(unsigned long inState, unsigned long inScreen);
    virtual void SetCulling(unsigned long inState);
    virtual void SetDebugColor(const GEColor& inColor) = 0;
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
    virtual void SetProjectionMode(unsigned long inMode, unsigned long inScreen);
    virtual void SetShading(unsigned long inState, unsigned long inScreen);
    virtual void SetShadingColorSource(unsigned long inSource, unsigned long inScreen);
    virtual void SetSpecular(unsigned long inState, unsigned long inScreen);
    virtual void SetTexture(GETexture* inTexture, unsigned long inScreen) = 0;
    virtual void SetViewport(const GEViewport& inViewport, unsigned long inScreen);
    virtual void SetWaitForVSync(bool inState, unsigned long inScreen);
    virtual void SetZBuffer(unsigned long inState, unsigned long inScreen);
    virtual void SetZCompare(unsigned long inState, unsigned long inScreen);
    virtual long ShowScene(unsigned long inScreen) = 0;
    virtual void TransformToCamera(GETerrainMesh& inMesh, GEViewport& inViewport, float inScale, bool inWaterOnly,
                                   bool inOrthographic) = 0;
    virtual long UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen) = 0;

    long Activate();
    long CreateFontBitmap(GEFont* inFont, unsigned long inScreen);
    long Draw(GELine* inLine, unsigned long inScreen) { return this->Draw(inLine, 1, inScreen); }
    long Draw(GEPoint* inPoint, unsigned long inScreen) { return this->Draw(inPoint, 1, inScreen); }
    static GERasterizer* GetActiveRasterizer() { return GERasterizer::sActiveRasterizer; }
    unsigned long GetAlphaTesting(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mAlphaTesting; }
    unsigned long GetAlphaTestingCompareState(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mAlphaTestingCompareState; }
    float GetAlphaTestingRefValue(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mAlphaTestingRefValue; }
    const GEColor& GetAmbientColor() const { return this->mAmbientColor; }
    float GetAmbientIntensity() const { return this->mAmbientIntensity; }
    unsigned long GetAntiAliasing(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mAntiAliasing; }
    void GetBlending(unsigned long& outSource, unsigned long& outDestination, unsigned long inScreen) const
    {
        outSource = this->mScreenStates[inScreen]->mBlendSource;
        outDestination = this->mScreenStates[inScreen]->mBlendDestination;
    }
    unsigned long GetColorKeying(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mColorKeying; }
    const GEColor& GetComputedAmbientColor() const { return this->mComputedAmbientColor; }
    unsigned long GetCulling() const { return this->mCulling; }
    unsigned long GetDithering(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mDithering; }
    unsigned long GetDrawStyle(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mDrawStyle; }
    float GetDrawnPolyAreaPerFrame(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mDrawnPolyAreaPerFrame; }
    float GetDrawnPolyAreaPerSecond(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mDrawnPolyAreaPerSecond; }
    float GetDrawnPolysPerFrame(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mDrawnPolysPerFrame; }
    float GetDrawnPolysPerSecond(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mDrawnPolysPerSecond; }
    unsigned long GetFog(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mFogState; }
    void GetFogAttributes(GERFogAttributes& outAttributes, unsigned long inScreen) const
    {
        outAttributes.mType = this->mScreenStates[inScreen]->mFog.mType;
        outAttributes.mDensity = this->mScreenStates[inScreen]->mFog.mDensity;
        outAttributes.mColor = this->mScreenStates[inScreen]->mFog.mColor;
        outAttributes.mStart = this->mScreenStates[inScreen]->mFog.mStart;
        outAttributes.mEnd = this->mScreenStates[inScreen]->mFog.mEnd;
        outAttributes.mScale = this->mScreenStates[inScreen]->mFog.mScale;
    }
    float GetFramesPerSecond(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mFramesPerSecond; }
    bool GetIsActivated() const { return this->mIsActivated; }
    bool GetIsScreenActivated(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mIsScreenActivated; }
    bool GetIsPolyAreaProfiling() { return this->mIsPolyAreaProfiling; }
    unsigned long GetLighting(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mLighting; }
    const UString& GetName() const { return this->mName; }
    unsigned long GetProjectionMode(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mProjectionMode; }
    float GetPixelSize(GEModel* inModel, GEViewport* inViewport, GETransformation& inTransformation, float inScale,
                       bool inOrthographic);
    float GetPixelSize(const GEPhysicalModel& inModel, GEViewport& inViewport, bool inOrthographic);
    GESurface* GetPrimarySurface(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mPrimarySurface; }
    static GERasterizer* GetRasterizer(unsigned long inIndex) { return GERasterizer::sRasterizers[inIndex]; }
    static unsigned long GetRasterizerCount() { return GERasterizer::sRasterizers.size(); }
    GESurface* GetRenderTarget(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mRenderTarget; }
    GEScreen* GetScreen(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mScreen; }
    unsigned long GetScreenCount() const { return this->mScreenStates.size(); }
    unsigned long GetScreenID() const { return this->mScreenID; }
    float GetScenePolysPerFrame(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mScenePolysPerFrame; }
    float GetScenePolysPerSecond(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mScenePolysPerSecond; }
    GESurface* GetSecondarySurface(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mSecondarySurface; }
    unsigned long GetShading(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mShading; }
    unsigned long GetSpecular(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mSpecular; }
    GETextureManager* GetTextureManager(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mScreen->GetTextureManager(); }
    float GetTotalTextureChangesPerFrame(unsigned long inScreen) const
        { return this->mScreenStates[inScreen]->mTotalTextureChangesPerFrame; }
    GEViewport& GetViewport(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mViewport; }
    GEWindow* GetWindow(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mWindow; }
    unsigned long GetZBuffer(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mZBuffer; }
    unsigned long GetZCompare(unsigned long inScreen) const { return this->mScreenStates[inScreen]->mZCompare; }
    void SetPolyAreaProfiling(bool inState) { this->mIsPolyAreaProfiling = inState; }
    void SetScreenID(unsigned long inID) { this->mScreenID = inID; }
    void SetWaterRippleEffect(float inWaterRipple0, float inWaterRipple1, float inWaterRipple2)
    {
        this->mWaterRipple0 = inWaterRipple0;
        this->mWaterRipple1 = inWaterRipple1;
        this->mWaterRipple2 = inWaterRipple2;
    }
protected:
    GERasterizer(const UString& inName);
    void AddToDrawnPolyArea(float inArea, unsigned long inScreen)
        { this->mScreenStates[inScreen]->mDrawnPolyArea += inArea; }
    void AddToDrawnPolyCount(unsigned long inCount, unsigned long inScreen)
        { this->mScreenStates[inScreen]->mDrawnPolyCount += inCount; }
    // The plug-in's access to the base's state (the census of R4.3's twelfth part: no member is read directly)
    GERScreenState* GetScreenState(unsigned long inScreen) const { return this->mScreenStates[inScreen]; }
    TSCriticalSection* GetDrawingAccess() const { return this->mDrawingAccess; }
    void GetWaterRippleEffect(float& outRipple0, float& outRipple1, float& outRipple2) const
    {
        outRipple0 = this->mWaterRipple0;
        outRipple1 = this->mWaterRipple1;
        outRipple2 = this->mWaterRipple2;
    }
    static void SetActiveRasterizer(GERasterizer* inRasterizer) { GERasterizer::sActiveRasterizer = inRasterizer; }
    void IsOrthographicModelVisible(GEModel* inModel, GEViewport* inViewport, GETransformation& inTransformation,
                                    bool& outVisible, bool& outClipped);
    void IsPerspectiveModelVisible(GEModel* inModel, float inScale, GEViewport* inViewport,
                                   GETransformation& inTransformation, bool& outVisible, bool& outClipped);
    virtual ~GERasterizer();                // slot 0x114
    // Eight more pure slots follow the dtor (vtable 0x10097954 ends at 0x138), in alphabetical order; names and
    // signatures proven by DX7HRDisplay.dll's overrides (exports ?ActivateRasterizer@DX7Rasterizer@@MAEJXZ, ...).
    // Activate calls ActivateRasterizer and keys mIsActivated on its keseOK result.
    virtual long ActivateRasterizer() = 0;                                              // slot 0x118
    virtual long ActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary,
                                   GETextureManager* inManager, GEWindow* inWindow) = 0;  // slot 0x11c
    virtual long CreateTextureHandle(GETexture* inTexture) = 0;     // slot 0x120: GETexture gains a surface
    virtual long DemoteTexture(GETexture* inTexture) = 0;           // slot 0x124
    virtual void DestroyTextureHandle(GETexture* inTexture) = 0;    // slot 0x128: GETexture loses its surface
    virtual long DrawModelHierarchy(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inSorted) = 0;
    virtual long PromoteTexture(GETexture* inTexture) = 0;          // slot 0x130
    virtual void SetRenderTarget(GESurface* inTarget, unsigned long inScreen) = 0;      // slot 0x134

    GEColor mAmbientColor;                  // +0x04
    GEColor mComputedAmbientColor;          // +0x10
    std::vector<GERScreenState*> mScreenStates;   // +0x1c
    GEFont* mDebugFont;                     // +0x2c provisional name ("Arial Bold" 9pt, ctor)
    GEText* mDebugText;                     // +0x30 (provisional name)
    unsigned long m34;                      // +0x34
    TSCriticalSection* mDrawingAccess;      // +0x38 (ctor ESVERIFY text)
    float mAmbientIntensity;                // +0x3c
    float mWaterRipple0;                    // +0x40
    float mWaterRipple1;                    // +0x44
    float mWaterRipple2;                    // +0x48
    unsigned long mCulling;                 // +0x4c
    bool mIsPolyAreaProfiling;              // +0x50
    bool mIsActivated;                      // +0x51
    UString mName;                          // +0x54
    unsigned long mScreenID;                // +0x64

    static std::vector<TransformStack> sTransformStack;
    friend LLE_API long GEInitialize(FSFileSpec& inDirectory, HINSTANCE__* const inInstance);   // Graphics Engine.cpp
    friend void GEShutdown(bool inForced);
    static void ClearRasterizers() { GERasterizer::sRasterizers.clear(); }
    static void AddRasterizer(GERasterizer* inRasterizer) { GERasterizer::sRasterizers.push_back(inRasterizer); }
    friend class GETexture;                 // calls CreateTextureHandle/DestroyTextureHandle (GETexture.cpp)
    static std::vector<GERasterizer*> sRasterizers;
    static GERasterizer* sActiveRasterizer;
};

// One level of the model-hierarchy transform stack (element size 8: GERasterizer.cpp's _Buy 0x1002462c). Layout from
// DX7Rasterizer::DrawModelHierarchy (DX7HRDisplay.dll 0x100042c1): back() gives the parent's combined transform
// (Apply's this) at +0 and its accumulated scale at +4; push_back stores the address of a stack local. Names
// are provisional.
struct GERasterizer::TransformStack {
    TransformStack() {}
    TransformStack(GETransformation* inTransformation, float inScale)
        : mTransformation(inTransformation), mScale(inScale) {}
    GETransformation* mTransformation;
    float mScale;
};

#endif
