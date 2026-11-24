// C:\EEX\Low-Level Engine\Graphics Engine\GERasterizer.h (RECONSTRUCTION; GERasterizer.cpp is a __FILE__ anchor at
// ~0x10023807). Abstract base of the renderer plug-ins (DX7HRDisplay DX7Rasterizer 0x10324 gives slot order and
// signatures). The accessors are inline (GEBitmap.cpp emits them first, 0x1000ed7c..).
#ifndef LLE_GERASTERIZER_H
#define LLE_GERASTERIZER_H

#include <vector>
#include <utility>
#include "lle/GERenderState.h"
#include "rust/render/ge.h"
#include "rust/render/dx7.h"

#include "lle/LLEForward.h"
#include "lle/GERust.h"
#include "lle/GEBitmap.h"
#include "lle/GEText.h"
#include "lle/GESurface.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEVertexArray.h"
#include "GEWindow.h"

// The fog's attributes and a screen's render state are the Rust records EeGeRastFog and EeGeRastScreen
// (rust/crates/ee_render/src/ge rastobj.rs; rust/render/ge.h): the plug-in reads and writes the device-side fields
// through GERasterizer::GetScreenState.
typedef EeGeRastFog GERFogAttributes;

struct EeGeRasterizer;                      // rust/ee_ge.h

class LLE_API GERasterizer {                // sizeof 0x68 in the original (vector deleting dtor 0xf279)
public:
    // port (LLE round 12): the plug-in's virtuals went with the plug-in boundary. The game links one rasterizer, the
    // DX7 one (rust/crates/ee_render/src/dx7 rastobj.rs; DX7Rasterizer, folded in here), so each method calls its Rust
    // object (mDX7) directly.
    // (the GE types its members call are included above.)

    // Sets up a screen's primary, back buffer and optional Z-buffer (flipping chain full screen, offscreen buffer
    // windowed), activates Direct3D on them and forces every render state to its default
    long ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                        unsigned long inBackBuffers, bool inZBuffer, unsigned long inZBufferFormat)
        { return ee_dx7r_activate_screen(this->mDX7, inScreen, inManager, inWindow, inWindow->GetWidth(), inWindow->GetHeight(), inBackBuffers, inZBuffer, inZBufferFormat); }
    // Takes a free light slot (of 8), sets the light into it and enables it; returns 7 when all slots are in use
    long AddLight(GELight& inLight, unsigned long inScreen, unsigned long& outIndex)
        { return ee_dx7r_add_light(this->mDX7, &inLight, inScreen, &outIndex); }
    // Sets the render target and begins the scene, waiting while the device is still drawing; 9 when the surface is lost
    long BeginScene(GESurface* inTarget, unsigned long inScreen) { return ee_dx7r_begin_scene(this->mDX7, inTarget, inScreen); }
    // Fills a rectangle of the render target with a colour (Clear) or of the Z-buffer with a depth (ClearZBuffer),
    // waiting while the device is still drawing
    long Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen)
        { return ee_dx7r_clear(this->mDX7, reinterpret_cast<const float(*)[4]>(&inRectangle), reinterpret_cast<const EeGeRastColor*>(&inColor), 0.0f, inScreen); }
    long ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen)
        { return ee_dx7r_clear(this->mDX7, reinterpret_cast<const float(*)[4]>(&inRectangle), NULL, inZ, inScreen); }
    // (the original's two CreateMovie slots: no movie player since 2026-11-17)
    // A polygon array of the given size (16-bit indices: the renderer's DX7PolygonArray, now GEPolygonArray's); a
    // vertex array of type 0 (standard: DX7StandardVertexArray) or 1 (coloured: DX7StdColorVertexArray), else null
    GEPolygonArray* CreatePolygonArray(unsigned long inCount) { return new GEPolygonArray(inCount); }
    GEVertexArray* CreateVertexArray(unsigned long inType, unsigned long inCount)   // slot 0x2c
        { return inType <= 1 ? new GEVertexArray(inType, inCount) : NULL; }
    // Releases every screen's texture, vertex buffer, lights, device and Direct3D, deletes the screens and their states
    long Deactivate() { return ee_dx7r_deactivate(this->mDX7); }
    // The draws (ee_dx7 draw2d.rs, draw_model.rs, draw_terrain.rs: Rust reads the GE objects in place)
    long Draw(GEBitmap* inBitmap, unsigned long inScreen)                            // slot 0x54
        { return ee_dx7r_draw_bitmap(this->mDX7, inScreen, inBitmap->GetRust()); }
    long Draw(GELine* inLine, unsigned long inCount, unsigned long inScreen)         // slot 0x50
        { return ee_dx7r_draw_lines(this->mDX7, inScreen, inLine, inCount); }
    // a model and its sub-models under the drawing lock (the float argument becomes the level-of-detail divisor)
    long Draw(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inOrthographic)
        { return ee_dx7r_draw_model(this->mDX7, inScreen, inModel, inOpacity, inOrthographic); }
    long Draw(GEPoint* inPoint, unsigned long inCount, unsigned long inScreen)       // slot 0x48
        { return ee_dx7r_draw_points(this->mDX7, inScreen, inPoint, inCount); }
    long Draw(GERhombus* inRhombus, unsigned long inScreen) { return ee_dx7r_draw_rhombus(this->mDX7, inScreen, inRhombus); }
    // a text with the platform fonts on the target, aligned; shadow fonts get a black or white shadow 1 pixel down-right
    long Draw(GEText* inText, GESurface* inTarget) { return ee_dx7r_draw_text(inText, NULL, *inTarget); }
    // a text clipped to a rectangle: with the font's bitmap font when it has one, else as the one above
    long Draw(GEText* inText, const GERectangle& inRectangle, GESurface* inTarget)
    {
        return inText->GetFont()->GetFontBitmap() != NULL
                   ? inText->GetFont()->GetFontBitmap()->Draw(inText, inRectangle, this, 0)
                   : ee_dx7r_draw_text_clipped(inText, reinterpret_cast<const float(*)[4]>(&inRectangle), *inTarget);
    }
    // the visible terrain texture by texture, unclipped then clipped polygons (space maps draw materials 1..8 first);
    // with no visible tile row fills the view with two black quads instead
    long Draw(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
        { return ee_dx7r_draw_terrain(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic); }
    // an indexed triangle list with an identity world transform and D3D clipping on; counts the triangles drawn
    long Draw(GEPolygonArray& inPolygons, unsigned long inCount, GEVertexArray& inVertices,
              unsigned long inVertexCount, GETexture* inTexture, unsigned long inScreen, bool inSorted)
    {
        return ee_dx7r_draw_polygon_array(this->mDX7, inScreen, inPolygons.GetIndices(), inCount, inVertices.GetType(),
                                          inVertices.GetVertices(), inVertexCount, inTexture);
    }
    // a textured polyline (at most 1200 segments) as a red line list in camera space, advancing the texture V along it
    // (without the drawing lock); the points are std::pair<GE3DPoint, float>: (x, y, z, advance)
    template <class Point> long Draw3DLine(std::vector<Point>& inPoints, GETexture* inTexture, float inV, unsigned long inScreen, bool inOrthographic)
        { return ee_dx7r_draw_3d_line(this->mDX7, inScreen, reinterpret_cast<const float(*)[4]>(inPoints.data()), inPoints.size(), inTexture, inV, inOrthographic); }
    // grid lines between tile corners whose both ends are visible, as one red line list of at most 1200 lines (the runs:
    // RTSMapTileRun in the game's view, std::pair<U2DPoint<long>, U2DPoint<long> > in the LLE's; four longs each)
    template <class Mesh, class Run> long DrawGridLines(Mesh& inMesh, std::vector<Run>& inLines, GETexture* inTexture, float inWidth, unsigned long inScreen, bool inOrthographic)
        { return ee_dx7r_draw_grid_lines(this->mDX7, inScreen, inMesh.GetRust(), reinterpret_cast<const long(*)[4]>(inLines.data()), inLines.size(), inTexture, inWidth, inOrthographic); }
    // the sky model like any model; always kErrorOK
    long DrawSky(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inOrthographic)
        { return this->Draw(inModel, inScreen, inOpacity, inOrthographic), kErrorOK; }
    // two red edge lines for every visible tile, one draw call per tile
    template <class Mesh> long DrawTerrainGrid(Mesh& inMesh, unsigned long inScreen, bool inOrthographic)
        { return ee_dx7r_draw_terrain_grid(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic); }
    // shades a tile rectangle (a U2DRectangle<long>) with a translucent colour over every valid visible tile, drawn in
    // batches of 400 quads
    template <class Mesh, class Rect> long DrawTerrainHighlight(Mesh& inMesh, const Rect& inRectangle, unsigned long inScreen, bool inOrthographic, const GEColor& inColor, float inOpacity)
        { return ee_dx7r_draw_terrain_highlight(this->mDX7, inScreen, inMesh.GetRust(), &inRectangle.mBottom, reinterpret_cast<const float*>(&inColor), inOpacity, inOrthographic); }
    // every water material, unclipped then clipped polygons; counts its triangles
    template <class Mesh> long DrawWater(Mesh& inMesh, unsigned long inScreen, bool inOrthographic)
        { return ee_dx7r_draw_water(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic); }
    // Ends the scene, waiting while the device is still drawing; 9 when the surface is lost
    long EndScene(unsigned long inScreen) { return ee_dx7r_end_scene(this->mDX7, inScreen); }
    long GetTotalVideoMemory(unsigned long inScreen, unsigned long& outBytes)
        { return ee_dx7r_video_memory(this->mDX7, inScreen, true, &outBytes); }
    unsigned long GetCaps(unsigned long inCap, unsigned long inScreen) { return 0; }   // 0 for any capability
    // whether the screen's card supports dirty-rectangle updates
    bool GetDirtyRectangleSupport(unsigned long inScreen) { return ee_dx7r_card_flag(this->mDX7, inScreen, true); }
    // lights the visible terrain and water vertices: ambient plus each active directional light, normalised, times the
    // vertex colour
    template <class Mesh> void Light(Mesh& inMesh, unsigned long inScreen) { ee_dx7r_light_terrain(this->mDX7, inScreen, inMesh.GetRust()); }
    long RemoveAllLights(unsigned long inScreen) { return ee_dx7r_remove_all_lights(this->mDX7, inScreen); }
    // The render state setters (0x1000b22b..0x1000c910): each sends the state when it changes and records it in the
    // screen's state (GetScreenState); the fog type, projection mode and v-sync only record it
    void SetAlphaTesting(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::AlphaTesting, inState, inScreen); }
    void SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen)
        { ee_dx7r_set(this->mDX7, EeDx7RastSet::AlphaTestingCompareState, inState, inScreen); }
    void SetAlphaTestingRefValue(float inValue, unsigned long inScreen) { ee_dx7r_set_float(this->mDX7, false, inValue, inScreen); }
    void SetAmbientColor(const GEColor& inColor, unsigned long inScreen)
        { ee_dx7r_set_color(this->mDX7, EeDx7RastColor::Ambient, reinterpret_cast<const EeGeRastColor*>(&inColor), inScreen); }
    void SetAmbientIntensity(float inIntensity, unsigned long inScreen) { ee_dx7r_set_float(this->mDX7, true, inIntensity, inScreen); }
    void SetAntiAliasing(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::AntiAliasing, inState, inScreen); }
    void SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen)
        { ee_dx7r_set_blending(this->mDX7, inSource, inDestination, inScreen); }
    void SetColorKeying(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::ColorKeying, inState, inScreen); }
    void SetCulling(unsigned long inState) { ee_dx7r_set_culling(this->mDX7, inState); }
    void SetDisplayNoTextureMode(bool inState, unsigned long inScreen) {}
    void SetDisplayOverdrawMode(bool inState, unsigned long inScreen) {}
    void SetDithering(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Dithering, inState, inScreen); }
    void SetFog(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Fog, inState, inScreen); }
    void SetFogColor(const GEColor& inColor, unsigned long inScreen)
        { ee_dx7r_set_color(this->mDX7, EeDx7RastColor::Fog, reinterpret_cast<const EeGeRastColor*>(&inColor), inScreen); }
    void SetFogRange(float inStart, float inEnd, unsigned long inScreen) { ee_dx7r_set_fog_range(this->mDX7, inStart, inEnd, inScreen); }
    void SetFogType(unsigned long inType, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::FogType, (intptr_t)inType); }
    void SetLighting(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Lighting, inState, inScreen); }
    void SetProjectionMode(unsigned long inMode, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ProjectionMode, (intptr_t)inMode); }
    void SetShading(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Shading, inState, inScreen); }
    void SetShadingColorSource(unsigned long inSource, unsigned long inScreen)
        { ee_dx7r_set(this->mDX7, EeDx7RastSet::ShadingColorSource, inSource, inScreen); }
    void SetSpecular(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Specular, inState, inScreen); }
    // binds a texture to stage 0 with its colour and alpha operations, addressing, filters and mip bias, sending only
    // what changed; null disables texturing; stage 1 is always disabled
    void SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_dx7r_set_texture(this->mDX7, inTexture, inScreen); }
    // sets the viewport rectangle and depth range on the device and marks the 2D/3D transforms for refresh
    void SetViewport(const GEViewport& inViewport, unsigned long inScreen) { ee_dx7r_set_viewport(this->mDX7, &inViewport, inScreen); }
    void SetWaitForVSync(bool inState, unsigned long inScreen)
        { ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::WaitForVSync, inState); }
    void SetZBuffer(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::ZBuffer, inState, inScreen); }
    void SetZCompare(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::ZCompare, inState, inScreen); }
    // Shows the frame (flip full screen, blit to the window's client area windowed); every 12 frames updates the frame,
    // polygon and texture-change statistics
    long ShowScene(unsigned long inScreen) { return ee_dx7r_show_scene(this->mDX7, inScreen); }
    // moves the visible terrain and water vertices to camera space (animating the water waves) and sets their clip
    // flags, then sorts each material's polygons into unclipped and clipped lists; water-only mode skips dry vertices
    template <class Mesh> void TransformToCamera(Mesh& inMesh, GEViewport& inViewport, float inScale, bool inWaterOnly, bool inOrthographic)
        { ee_dx7r_transform_to_camera(this->mDX7, inMesh.GetRust(), &inViewport, inScale, inWaterOnly, inOrthographic); }
    // sends a light (point or directional, colour, attenuation, range) to its slot; 9 when the slot is not in use
    long UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen)
        { return ee_dx7r_update_light(this->mDX7, inIndex, &inLight, inScreen); }

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
    // a model's on-screen width in pixels (its scaled bounding diameter projected through the viewport); a placed
    // model's, from its location, frame and scale
    float GetPixelSize(GEModel* inModel, GEViewport* inViewport, GETransformation& inTransformation, float inScale,
                       bool inOrthographic)
        { return ee_ge_rast_pixel_size(this->mRust, inModel, GERust(*inViewport), GERust(inTransformation), inScale, inOrthographic); }
    float GetPixelSize(const GEPhysicalModel& inModel, GEViewport& inViewport, bool inOrthographic)
        { return ee_ge_rast_placed_pixel_size(this->mRust, &inModel, GERust(inViewport), inOrthographic); }
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
    void SetWaterRippleEffect(float inWaterRipple0, float inWaterRipple1, float inWaterRipple2)
        { ee_ge_rast_ripple_values(this->mRust, inWaterRipple0, inWaterRipple1, inWaterRipple2); }
    // the DX7 rasterizer of the given name (inTnL: its T&L flavour); the game has one (DX7TnLRasterizer). Without inTnL,
    // a rasterizer with no device (tools/eemodel/storegate: model and texture loads, no draws)
    GERasterizer(const UString& inName, bool inTnL);
    explicit GERasterizer(const UString& inName);
    ~GERasterizer();                        // slot 0x114
    // The plug-in's access to the base's state (the census of R4.3's twelfth part: no member is read directly)
    EeGeRastScreen* GetScreenState(unsigned long inScreen) const { return ee_ge_rast_screen(this->mRust, inScreen); }
    // The original's eight pure slots after the dtor (vtable 0x10097954 ends at 0x138; DX7HRDisplay.dll's exports
    // ?ActivateRasterizer@DX7Rasterizer@@MAEJXZ, ...). Activate calls ActivateRasterizer and keys mIsActivated on its
    // keseOK result: it enumerates the DirectDraw devices into screens and makes this the active rasterizer. Textures
    // need no handle here (CreateTextureHandle, DestroyTextureHandle and PromoteTexture do nothing).
    long ActivateRasterizer() { return this->mDX7 != NULL ? ee_dx7r_activate_rasterizer(this->mDX7) : kErrorOK; }   // slot 0x118
    long CreateTextureHandle(GETexture* inTexture) { return kErrorOK; }             // slot 0x120: GETexture gains a surface
    void DestroyTextureHandle(GETexture* inTexture) {}                              // slot 0x128: GETexture loses its surface
    long PromoteTexture(GETexture* inTexture) { return kErrorOK; }                  // slot 0x130
    // makes a surface the render target (swapping the references) and fits the viewport to its size (slot 0x134)
    void SetRenderTarget(GESurface* inTarget, unsigned long inScreen) { ee_dx7r_set_render_target(this->mDX7, inTarget, inScreen); }

    // The state is Rust's (rust/crates/ee_render/src/ge rastobj.rs: the colours, the screen records, the debug font and text,
    // the drawing lock, the ripple, the culling, the flags, the name, the screen id, and the statics sRasterizers and
    // sActiveRasterizer); every call it makes on another object goes through GERasterizerHost (GERasterizer.cpp).
    URust<EeGeRasterizer> mRust;
    EeDx7Rasterizer* mDX7 = NULL;           // the DX7 rasterizer (ee_dx7 rastobj.rs; none in the store gate)

    // the two Rust objects, for the graphics thread's Rust (EERenderObject.cpp)
    static void* RustObjects(GERasterizer* inRasterizer, void** outBase) { return *outBase = inRasterizer->mRust, inRasterizer->mDX7; }
};

#endif
