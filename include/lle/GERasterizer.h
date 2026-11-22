// C:\EEX\Low-Level Engine\Graphics Engine\GERasterizer.h (RECONSTRUCTION; GERasterizer.cpp is a __FILE__ anchor at
// ~0x10023807). Abstract base of the renderer plug-ins (DX7HRDisplay DX7Rasterizer 0x10324 gives slot order and
// signatures). The accessors are inline (GEBitmap.cpp emits them first, 0x1000ed7c..).
#ifndef LLE_GERASTERIZER_H
#define LLE_GERASTERIZER_H

#include <vector>
#include <utility>
#include "lle/GERenderState.h"
#include "rust/render/ge.h"

class FSFileSpec;
class GEBitmap;
class GEFont;
class GELight;
class GELine;
class GEModel;
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

// The fog's attributes and a screen's render state are the Rust records EeGeRastFog and EeGeRastScreen
// (rust/crates/ee_render/src/ge rastobj.rs; rust/render/ge.h): the plug-in reads and writes the device-side fields
// through GERasterizer::GetScreenState.
typedef EeGeRastFog GERFogAttributes;

struct EeGeRasterizer;                      // rust/ee_ge.h

class LLE_API GERasterizer {                // sizeof 0x68 in the original (vector deleting dtor 0xf279)
public:
    virtual long ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                unsigned long inMode, bool inFullScreen, unsigned long inBackBuffers) = 0;
    virtual long AddLight(GELight& inLight, unsigned long inScreen, unsigned long& outIndex) = 0;
    virtual long BeginScene(GESurface* inTarget, unsigned long inScreen) = 0;
    virtual long Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen) = 0;
    virtual long ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen) = 0;
    // (the original's two CreateMovie slots: no movie player since 2026-11-17)
    virtual GEPolygonArray* CreatePolygonArray(unsigned long inCount) = 0;
    virtual GEVertexArray* CreateVertexArray(unsigned long inType, unsigned long inCount) = 0;   // slot 0x2c
    virtual long Deactivate() { return ee_ge_rast_deactivate(this->mRust), kErrorOK; }
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
    virtual long GetTotalVideoMemory(unsigned long inScreen, unsigned long& outBytes) = 0;
    virtual unsigned long GetCaps(unsigned long inCap, unsigned long inScreen) = 0;
    virtual bool GetDirtyRectangleSupport(unsigned long inScreen) = 0;
    virtual void Light(GETerrainMesh& inMesh, unsigned long inScreen) = 0;
    virtual long RemoveAllLights(unsigned long inScreen) = 0;
    virtual void SetAlphaTesting(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::AlphaTesting, (intptr_t)inState); }
    virtual void SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::AlphaTestingCompareState, (intptr_t)inState); }
    virtual void SetAlphaTestingRefValue(float inValue, unsigned long inScreen)
        { this->GetScreenState(inScreen)->alpha_testing_ref_value = inValue; }
    virtual void SetAmbientColor(const GEColor& inColor, unsigned long inScreen)
        { ee_ge_rast_set_ambient_color(this->mRust, reinterpret_cast<const EeGeRastColor*>(&inColor)); }
    virtual void SetAmbientIntensity(float inIntensity, unsigned long inScreen)
        { ee_ge_rast_set_ambient_intensity(this->mRust, inIntensity); }
    virtual void SetAntiAliasing(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::AntiAliasing, (intptr_t)inState); }
    virtual void SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen)
        { ee_ge_rast_set_blending(this->mRust, inScreen, inSource, inDestination); }
    virtual void SetColorKeying(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ColorKeying, (intptr_t)inState); }
    virtual void SetCulling(unsigned long inState)
        { ee_ge_rast_set(this->mRust, EeGeRastVal::Culling, (intptr_t)inState); }
    virtual void SetDisplayNoTextureMode(bool inState, unsigned long inScreen) {}
    virtual void SetDisplayOverdrawMode(bool inState, unsigned long inScreen) {}
    virtual void SetDithering(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::Dithering, (intptr_t)inState); }
    virtual void SetFog(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::FogState, (intptr_t)inState); }
    virtual void SetFogColor(const GEColor& inColor, unsigned long inScreen)
        { ee_ge_rast_set_fog_color(this->mRust, inScreen, reinterpret_cast<const EeGeRastColor*>(&inColor)); }
    virtual void SetFogRange(float inStart, float inEnd, unsigned long inScreen)
        { ee_ge_rast_set_fog_range(this->mRust, inScreen, inStart, inEnd); }
    virtual void SetFogType(unsigned long inType, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::FogType, (intptr_t)inType); }
    virtual void SetLighting(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::Lighting, (intptr_t)inState); }
    virtual void SetProjectionMode(unsigned long inMode, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ProjectionMode, (intptr_t)inMode); }
    virtual void SetShading(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::Shading, (intptr_t)inState); }
    virtual void SetShadingColorSource(unsigned long inSource, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ShadingColorSource, (intptr_t)inSource); }
    virtual void SetSpecular(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::Specular, (intptr_t)inState); }
    virtual void SetTexture(GETexture* inTexture, unsigned long inScreen) = 0;
    virtual void SetViewport(const GEViewport& inViewport, unsigned long inScreen)
        { ee_ge_rast_set_viewport(this->mRust, inScreen, reinterpret_cast<const EeGeViewport*>(&inViewport)); }
    virtual void SetWaitForVSync(bool inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::WaitForVSync, inState); }
    virtual void SetZBuffer(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ZBuffer, (intptr_t)inState); }
    virtual void SetZCompare(unsigned long inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ZCompare, (intptr_t)inState); }
    virtual long ShowScene(unsigned long inScreen) = 0;
    virtual void TransformToCamera(GETerrainMesh& inMesh, GEViewport& inViewport, float inScale, bool inWaterOnly,
                                   bool inOrthographic) = 0;
    virtual long UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen) = 0;

    long Activate() { return (long)ee_ge_rast_activate(this->mRust); }
    long Draw(GELine* inLine, unsigned long inScreen) { return this->Draw(inLine, 1, inScreen); }
    long Draw(GEPoint* inPoint, unsigned long inScreen) { return this->Draw(inPoint, 1, inScreen); }
    static GERasterizer* GetActiveRasterizer() { return static_cast<GERasterizer*>(ee_ge_rast_active()); }
    float GetAlphaTestingRefValue(unsigned long inScreen) const
        { return this->GetScreenState(inScreen)->alpha_testing_ref_value; }
    const GEColor& GetAmbientColor() const
        { return *reinterpret_cast<const GEColor*>(ee_ge_rast_color(this->mRust, false)); }
    float GetAmbientIntensity() const { return ee_ge_rast_ambient_intensity(this->mRust); }
    void GetBlending(unsigned long& outSource, unsigned long& outDestination, unsigned long inScreen) const
    {
        outSource = this->GetScreenState(inScreen)->blend_source;
        outDestination = this->GetScreenState(inScreen)->blend_destination;
    }
    unsigned long GetFog(unsigned long inScreen) const { return this->GetScreenState(inScreen)->fog_state; }
    void GetFogAttributes(GERFogAttributes& outAttributes, unsigned long inScreen) const
        { outAttributes = this->GetScreenState(inScreen)->fog; }
    float GetFramesPerSecond(unsigned long inScreen) const { return this->GetScreenState(inScreen)->frames_per_second; }
    bool GetIsActivated() const { return ee_ge_rast_get(this->mRust, EeGeRastVal::IsActivated) != 0; }
    bool GetIsScreenActivated(unsigned long inScreen) const
        { return this->GetScreenState(inScreen)->is_screen_activated; }
    unsigned long GetLighting(unsigned long inScreen) const { return this->GetScreenState(inScreen)->lighting; }
    const UString& GetName() const
        { return *reinterpret_cast<const UString*>(ee_ge_rast_get(this->mRust, EeGeRastVal::Name)); }
    float GetPixelSize(GEModel* inModel, GEViewport* inViewport, GETransformation& inTransformation, float inScale,
                       bool inOrthographic);
    float GetPixelSize(const GEPhysicalModel& inModel, GEViewport& inViewport, bool inOrthographic);
    GESurface* GetPrimarySurface(unsigned long inScreen) const { return static_cast<GESurface*>(this->GetScreenState(inScreen)->primary_surface); }
    static GERasterizer* GetRasterizer(unsigned long inIndex)
        { return static_cast<GERasterizer*>(ee_ge_rast_list(3, NULL, inIndex)); }
    static unsigned long GetRasterizerCount() { return (unsigned long)(uintptr_t)ee_ge_rast_list(2, NULL, 0); }
    GESurface* GetRenderTarget(unsigned long inScreen) const { return static_cast<GESurface*>(this->GetScreenState(inScreen)->render_target); }
    GEScreen* GetScreen(unsigned long inScreen) const { return static_cast<GEScreen*>(this->GetScreenState(inScreen)->screen); }
    unsigned long GetScreenCount() const
        { return (unsigned long)ee_ge_rast_get(this->mRust, EeGeRastVal::ScreenCount); }
    unsigned long GetScreenID() const { return (unsigned long)ee_ge_rast_get(this->mRust, EeGeRastVal::ScreenId); }
    GESurface* GetSecondarySurface(unsigned long inScreen) const
        { return static_cast<GESurface*>(this->GetScreenState(inScreen)->secondary_surface); }
    GETextureManager* GetTextureManager(unsigned long inScreen) const
        { return this->GetScreen(inScreen)->GetTextureManager(); }
    GEViewport& GetViewport(unsigned long inScreen) const { return *reinterpret_cast<GEViewport*>(&this->GetScreenState(inScreen)->viewport); }
    GEWindow* GetWindow(unsigned long inScreen) const { return static_cast<GEWindow*>(this->GetScreenState(inScreen)->window); }
    unsigned long GetZBuffer(unsigned long inScreen) const { return this->GetScreenState(inScreen)->z_buffer; }
    unsigned long GetZCompare(unsigned long inScreen) const { return this->GetScreenState(inScreen)->z_compare; }
    void SetScreenID(unsigned long inID) { ee_ge_rast_set(this->mRust, EeGeRastVal::ScreenId, (intptr_t)inID); }
    void SetWaterRippleEffect(float inWaterRipple0, float inWaterRipple1, float inWaterRipple2);
protected:
    GERasterizer(const UString& inName);
    // The plug-in's access to the base's state (the census of R4.3's twelfth part: no member is read directly)
    EeGeRastScreen* GetScreenState(unsigned long inScreen) const { return ee_ge_rast_screen(this->mRust, inScreen); }
    virtual ~GERasterizer();                // slot 0x114
    // Eight more pure slots follow the dtor (vtable 0x10097954 ends at 0x138), in alphabetical order; names and
    // signatures proven by DX7HRDisplay.dll's overrides (exports ?ActivateRasterizer@DX7Rasterizer@@MAEJXZ, ...).
    // Activate calls ActivateRasterizer and keys mIsActivated on its keseOK result.
    virtual long ActivateRasterizer() = 0;                                              // slot 0x118
    virtual long CreateTextureHandle(GETexture* inTexture) = 0;     // slot 0x120: GETexture gains a surface
    virtual void DestroyTextureHandle(GETexture* inTexture) = 0;    // slot 0x128: GETexture loses its surface
    virtual long PromoteTexture(GETexture* inTexture) = 0;          // slot 0x130
    virtual void SetRenderTarget(GESurface* inTarget, unsigned long inScreen) = 0;      // slot 0x134

    // The state is Rust's (rust/crates/ee_render/src/ge rastobj.rs: the colours, the screen records, the debug font and text,
    // the drawing lock, the ripple, the culling, the flags, the name, the screen id, and the statics sRasterizers and
    // sActiveRasterizer); every call it makes on another object goes through GERasterizerHost (GERasterizer.cpp).
    EeGeRasterizer* mRust;

    friend LLE_API long GEInitialize(FSFileSpec& inDirectory, HINSTANCE__* const inInstance);   // Graphics Engine.cpp
    friend void GEShutdown(bool inForced);
    static void ClearRasterizers() { ee_ge_rast_list(0, NULL, 0); }
    static void AddRasterizer(GERasterizer* inRasterizer) { ee_ge_rast_list(1, inRasterizer, 0); }
    friend class GETexture;                 // calls CreateTextureHandle/DestroyTextureHandle (GETexture.cpp)
    friend struct GERasterizerHost;
    GERasterizer(const GERasterizer&) = delete;
    GERasterizer& operator=(const GERasterizer&) = delete;
};

#endif
