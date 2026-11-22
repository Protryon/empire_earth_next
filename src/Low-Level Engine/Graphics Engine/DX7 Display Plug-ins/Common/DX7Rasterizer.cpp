// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer.cpp
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x100091aa..0x1000d0fc: the U2D key init chains, the DirectDraw/Direct3D enumeration callbacks, DX7Rasterizer's
// ctor and dtor, then its members in alphabetical order, then the vector<DX7RScreenState*> COMDATs and the
// template-static init chain head. The enumeration callbacks and ActivateRasterizer were built without global
// optimisation (see the pragmas below); everything else is -O1.
#include <windows.h>
#include <stdint.h>
#include <float.h>

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "rust/render/dx7.h"
#include "rust/render/ge.h"
#include "platform/PlatFont.h"
#include "lle/GEBitmap.h"
#include "lle/GEPoint.h"
#include "lle/GERhombus.h"

// DX7Rasterizer is whole in Rust (rust/crates/ee_render/src/dx7/rastobj.rs, R4.3 part 13): the per-screen DX7 records, the
// debug colour, the light slots and every method below; DX7TnLRasterizer is its T&L flavour. The class keeps its vtable
// (the game calls it through GERasterizer) and one member, mDX7; the methods forward, and DX7RasterizerHost makes the
// calls on other objects Rust asks for.

// The calls the Rust rasterizer makes on other objects (EeDx7RastOp), in the original's order.
struct DX7RasterizerHost {
    static intptr_t Call(void* inOwner, EeDx7RastOp inOp, void* inObj, intptr_t a, intptr_t b)
    {
        switch (inOp) {
        case EeDx7RastOp::ScreenNew:
            return (intptr_t) static_cast<GEScreen*>(new DX7Screen(NULL, static_cast<GXDirectDraw*>(inObj),
                                                                   UString(reinterpret_cast<const char*>(a)), (b & 1) != 0, (b & 2) != 0));
        case EeDx7RastOp::ScreenDelete: delete static_cast<DX7Screen*>(static_cast<GEScreen*>(inObj)); return 0;
        case EeDx7RastOp::ScreenDirectDraw: return (intptr_t)(GXDirectDraw*)*static_cast<DX7Screen*>(static_cast<GEScreen*>(inObj));
        case EeDx7RastOp::ScreenIsFullScreen: return static_cast<GEScreen*>(inObj)->GetIsFullScreen();
        case EeDx7RastOp::ScreenCurrentMode: *reinterpret_cast<GEVideoMode*>(a) = static_cast<GEScreen*>(inObj)->GetCurrentMode(); return 0;
        case EeDx7RastOp::ScreenModeCount: return (intptr_t)static_cast<GEScreen*>(inObj)->GetModeCount();
        case EeDx7RastOp::ScreenMode: *reinterpret_cast<GEVideoMode*>(b) = static_cast<GEScreen*>(inObj)->GetMode((unsigned long)a); return 0;
        case EeDx7RastOp::ScreenCreateSurface:
            return static_cast<GEScreen*>(inObj)->CreateSurface(*reinterpret_cast<GESurfaceDescription*>(a), reinterpret_cast<GESurface**>(b));
        case EeDx7RastOp::ScreenSetTextureManager: static_cast<GEScreen*>(inObj)->SetTextureManager(reinterpret_cast<GETextureManager*>(a)); return 0;
        case EeDx7RastOp::ManagerSetScreen: static_cast<GETextureManager*>(inObj)->SetScreen(reinterpret_cast<GEScreen*>(a)); return 0;
        case EeDx7RastOp::SurfaceAttached: return (intptr_t)static_cast<GESurface*>(inObj)->GetAttachedSurface(0);
        case EeDx7RastOp::SurfaceAttach: return static_cast<GESurface*>(inObj)->AttachSurface(reinterpret_cast<GESurface*>(a));
        case EeDx7RastOp::SurfaceReference: static_cast<GESurface*>(inObj)->Reference(); return 0;
        case EeDx7RastOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
        case EeDx7RastOp::SurfaceGx: return (intptr_t)(GXSurface*)*static_cast<DX7Surface*>(static_cast<GESurface*>(inObj));
        case EeDx7RastOp::SurfaceGxChecked: return (intptr_t)(GXSurface*)*dynamic_cast<DX7Surface*>(static_cast<GESurface*>(inObj));
        case EeDx7RastOp::SurfaceWidth: return (intptr_t)static_cast<GESurface*>(inObj)->GetWidth();
        case EeDx7RastOp::SurfaceHeight: return (intptr_t)static_cast<GESurface*>(inObj)->GetHeight();
        case EeDx7RastOp::SurfaceFlip: return static_cast<GESurface*>(inObj)->Flip(a != 0);
        case EeDx7RastOp::TextureHasChanged: return static_cast<GETexture*>(inObj)->HasChanged();
        case EeDx7RastOp::TextureReference: static_cast<GETexture*>(inObj)->Reference(); return 0;
        case EeDx7RastOp::TextureDereference: static_cast<GETexture*>(inObj)->Dereference(); return 0;
        case EeDx7RastOp::TexturePromote: static_cast<GETexture*>(inObj)->Promote(); return 0;
        case EeDx7RastOp::TextureStage: return (intptr_t)static_cast<GETexture*>(inObj)->GetStage();
        case EeDx7RastOp::TextureStageSurface:
            return (intptr_t)(GXSurface*)*static_cast<DX7Surface*>(static_cast<GETexture*>(inObj)->GetStageTexture(0)->GetFrameSurface());
        case EeDx7RastOp::TextureValidate: static_cast<GETexture*>(inObj)->ValidateChanges(); return 0;
        case EeDx7RastOp::WindowOrigin: *reinterpret_cast<GE2DPoint*>(a) = static_cast<GEWindow*>(inObj)->GetClientOrigin(); return 0;
        case EeDx7RastOp::LockEnter: static_cast<TSCriticalSection*>(inObj)->Enter(); return 0;
        case EeDx7RastOp::LockLeave: static_cast<TSCriticalSection*>(inObj)->Leave(); return 0;
        case EeDx7RastOp::Time: return (intptr_t)TMGetTime();
        case EeDx7RastOp::Verify: ESVERIFY_AT(a != 0, (unsigned long)b); return 0;
        default: return 0;
        }
    }
};


// Creates the DirectX 7 rasterizer with the given name and a dark blue debug colour (inTnL: the T&L rasterizer)
DX7Rasterizer::DX7Rasterizer(const UString& inName, bool inTnL)
    : GERasterizer(inName)
{
    this->mDX7 = ee_dx7r_new(&DX7RasterizerHost::Call, this, this->mRust, inTnL);
    ee_ge_draw2d_direct(static_cast<GERasterizer*>(this), this->mDX7);   // the 2D draw list draws on it in Rust
}

// Destroys the rasterizer (the DX7 records; the base's in the base)
DX7Rasterizer::~DX7Rasterizer()
{
    ee_ge_draw2d_direct(static_cast<GERasterizer*>(this), NULL);
    ee_dx7r_destroy(this->mDX7);
}

// Enumerates the DirectDraw devices into screens (Ex enumeration on 98/2000) and makes this the active rasterizer
long DX7Rasterizer::ActivateRasterizer() { return ee_dx7r_activate_rasterizer(this->mDX7); }

void* DX7Rasterizer::RustObjects(GERasterizer* inRasterizer, void** outBase)
{
    DX7Rasterizer* theRasterizer = static_cast<DX7Rasterizer*>(inRasterizer);
    *outBase = theRasterizer->mRust;
    return theRasterizer->mDX7;
}

// Sets up a screen's primary, back buffer and optional Z-buffer (flipping chain full screen, offscreen buffer
// windowed), activates Direct3D on them and forces every render state to its default
long DX7Rasterizer::ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                   unsigned long inBackBuffers, bool inZBuffer, unsigned long inZBufferFormat)
{
    return ee_dx7r_activate_screen(this->mDX7, inScreen, inManager, inWindow, inWindow->GetWidth(), inWindow->GetHeight(), inBackBuffers,
                                   inZBuffer, inZBufferFormat);
}

// Takes a free light slot (of 8), sets the light into it and enables it; returns 7 when all slots are in use
long DX7Rasterizer::AddLight(GELight& inLight, unsigned long inScreen, unsigned long& outIndex)
{
    return ee_dx7r_add_light(this->mDX7, &inLight, inScreen, &outIndex);
}

// Sets the render target and begins the scene, waiting while the device is still drawing; 9 when the surface is lost
long DX7Rasterizer::BeginScene(GESurface* inTarget, unsigned long inScreen) { return ee_dx7r_begin_scene(this->mDX7, inTarget, inScreen); }

// Fills a rectangle of the render target with a colour, waiting while the device is still drawing
long DX7Rasterizer::Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen)
{
    return ee_dx7r_clear(this->mDX7, reinterpret_cast<const float(*)[4]>(&inRectangle), reinterpret_cast<const EeGeRastColor*>(&inColor), 0.0f, inScreen);
}

// Fills a rectangle of the Z-buffer with a depth value, waiting while the device is still drawing
long DX7Rasterizer::ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen)
{
    return ee_dx7r_clear(this->mDX7, reinterpret_cast<const float(*)[4]>(&inRectangle), NULL, inZ, inScreen);
}

// Creates a polygon array of the given size (16-bit indices: the renderer's DX7PolygonArray, now GEPolygonArray's)
GEPolygonArray* DX7Rasterizer::CreatePolygonArray(unsigned long inCount)
{
    return new GEPolygonArray(inCount);
}

// Does nothing and returns success (textures need no handle here)
long DX7Rasterizer::CreateTextureHandle(GETexture* inTexture)
{
    return kErrorOK;
}

// Creates a vertex array of type 0 (standard: the renderer's DX7StandardVertexArray) or 1 (coloured:
// DX7StdColorVertexArray), both GEVertexArray's now; null for any other type
GEVertexArray* DX7Rasterizer::CreateVertexArray(unsigned long inType, unsigned long inCount)
{
    return inType <= 1 ? new GEVertexArray(inType, inCount) : NULL;
}

// Releases every screen's texture, vertex buffer, lights, device and Direct3D, deletes the screens and their states
long DX7Rasterizer::Deactivate()
{
    return ee_dx7r_deactivate(this->mDX7);
}

// Does nothing (textures need no handle here)
void DX7Rasterizer::DestroyTextureHandle(GETexture* inTexture)
{
}

// Ends the scene, waiting while the device is still drawing; 9 when the surface is lost
long DX7Rasterizer::EndScene(unsigned long inScreen) { return ee_dx7r_end_scene(this->mDX7, inScreen); }

// Returns 0 for any capability
unsigned long DX7Rasterizer::GetCaps(unsigned long inCap, unsigned long inScreen)
{
    return 0;
}

// Returns whether the screen's card supports dirty-rectangle updates
bool DX7Rasterizer::GetDirtyRectangleSupport(unsigned long inScreen) { return ee_dx7r_card_flag(this->mDX7, inScreen, true); }

// Returns the total video memory of the screen
long DX7Rasterizer::GetTotalVideoMemory(unsigned long inScreen, unsigned long& outBytes)
{
    return ee_dx7r_video_memory(this->mDX7, inScreen, true, &outBytes);
}

long DX7Rasterizer::PromoteTexture(GETexture* inTexture)
{
    return kErrorOK;
}

// Removes all 8 lights
long DX7Rasterizer::RemoveAllLights(unsigned long inScreen) { return ee_dx7r_remove_all_lights(this->mDX7, inScreen); }

// The render state setters (0x1000b22b..0x1000c910): each sends the state when it changes and records it in the base.
void DX7Rasterizer::SetAlphaTesting(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::AlphaTesting, inState, inScreen); }
void DX7Rasterizer::SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen)
{
    ee_dx7r_set(this->mDX7, EeDx7RastSet::AlphaTestingCompareState, inState, inScreen);
}
void DX7Rasterizer::SetAlphaTestingRefValue(float inValue, unsigned long inScreen) { ee_dx7r_set_float(this->mDX7, false, inValue, inScreen); }
void DX7Rasterizer::SetAmbientColor(const GEColor& inColor, unsigned long inScreen)
{
    ee_dx7r_set_color(this->mDX7, EeDx7RastColor::Ambient, reinterpret_cast<const EeGeRastColor*>(&inColor), inScreen);
}
void DX7Rasterizer::SetAmbientIntensity(float inIntensity, unsigned long inScreen) { ee_dx7r_set_float(this->mDX7, true, inIntensity, inScreen); }
void DX7Rasterizer::SetAntiAliasing(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::AntiAliasing, inState, inScreen); }
void DX7Rasterizer::SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen)
{
    ee_dx7r_set_blending(this->mDX7, inSource, inDestination, inScreen);
}
void DX7Rasterizer::SetColorKeying(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::ColorKeying, inState, inScreen); }
void DX7Rasterizer::SetCulling(unsigned long inState) { ee_dx7r_set_culling(this->mDX7, inState); }
void DX7Rasterizer::SetDithering(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Dithering, inState, inScreen); }
void DX7Rasterizer::SetFog(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Fog, inState, inScreen); }
void DX7Rasterizer::SetFogColor(const GEColor& inColor, unsigned long inScreen)
{
    ee_dx7r_set_color(this->mDX7, EeDx7RastColor::Fog, reinterpret_cast<const EeGeRastColor*>(&inColor), inScreen);
}
void DX7Rasterizer::SetFogRange(float inStart, float inEnd, unsigned long inScreen) { ee_dx7r_set_fog_range(this->mDX7, inStart, inEnd, inScreen); }
void DX7Rasterizer::SetLighting(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Lighting, inState, inScreen); }
void DX7Rasterizer::SetShading(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Shading, inState, inScreen); }
void DX7Rasterizer::SetShadingColorSource(unsigned long inSource, unsigned long inScreen)
{
    ee_dx7r_set(this->mDX7, EeDx7RastSet::ShadingColorSource, inSource, inScreen);
}
void DX7Rasterizer::SetSpecular(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Specular, inState, inScreen); }
void DX7Rasterizer::SetZBuffer(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::ZBuffer, inState, inScreen); }
void DX7Rasterizer::SetZCompare(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::ZCompare, inState, inScreen); }

// Makes a surface the render target (swapping the references) and fits the viewport to its size
void DX7Rasterizer::SetRenderTarget(GESurface* inTarget, unsigned long inScreen) { ee_dx7r_set_render_target(this->mDX7, inTarget, inScreen); }

// Binds a texture to stage 0 with its colour and alpha operations, addressing, filters and mip bias, sending
// only what changed; null disables texturing; stage 1 is always disabled
void DX7Rasterizer::SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_dx7r_set_texture(this->mDX7, inTexture, inScreen); }

// Sets the viewport rectangle and depth range on the device and marks the 2D/3D transforms for refresh
void DX7Rasterizer::SetViewport(const GEViewport& inViewport, unsigned long inScreen) { ee_dx7r_set_viewport(this->mDX7, &inViewport, inScreen); }

// Shows the frame (flip full screen, blit to the window's client area windowed); every 12 frames updates the
// frame, polygon and texture-change statistics
long DX7Rasterizer::ShowScene(unsigned long inScreen) { return ee_dx7r_show_scene(this->mDX7, inScreen); }

// Sends a light (point or directional, colour, attenuation, range) to its slot; 9 when the slot is not in use
long DX7Rasterizer::UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen)
{
    return ee_dx7r_update_light(this->mDX7, inIndex, &inLight, inScreen);
}

// ---- the draws (the four Draw units of the original, deleted in R4.3 part 14): Rust (ee_dx7 draw2d.rs, draw_model.rs,
// draw_terrain.rs), which reads the GE objects in place (ee_ge cppobj.rs, R4.3 part 17)
long DX7Rasterizer::Draw(GEBitmap* inBitmap, unsigned long inScreen)
{
    return ee_dx7r_draw_bitmap(this->mDX7, inScreen, inBitmap->GetRust());
}

long DX7Rasterizer::Draw(GELine* inLine, unsigned long inCount, unsigned long inScreen)
{
    return ee_dx7r_draw_lines(this->mDX7, inScreen, inLine, inCount);
}

long DX7Rasterizer::Draw(GEPoint* inPoint, unsigned long inCount, unsigned long inScreen)
{
    return ee_dx7r_draw_points(this->mDX7, inScreen, inPoint, inCount);
}

long DX7Rasterizer::Draw(GERhombus* inRhombus, unsigned long inScreen)
{
    return ee_dx7r_draw_rhombus(this->mDX7, inScreen, inRhombus);
}

// the primitives are their Rust objects (one pointer each: the draws walk the C++ arrays, ee_dx7 draw_ffi.rs)
static_assert(sizeof(GELine) == sizeof(void*) && sizeof(GEPoint) == sizeof(void*) && sizeof(GERhombus) == sizeof(void*),
              "ee_ge prim.rs");

// Draws a text with GDI on the target, aligned; shadow fonts get a black or white shadow 1 pixel down-right
long DX7Rasterizer::Draw(GEText* inText, GESurface* inTarget)
{
    return ee_dx7r_draw_text(inText, NULL, *static_cast<DX7Surface*>(inTarget));
}

// Draws a text clipped to a rectangle: with the font's bitmap font when it has one, else with GDI (aligned, shadowed)
long DX7Rasterizer::Draw(GEText* inText, const GERectangle& inRectangle, GESurface* inTarget)
{
    if (inText->GetFont()->GetFontBitmap() != NULL)
        return inText->GetFont()->GetFontBitmap()->Draw(inText, inRectangle, this, 0);
    return ee_dx7r_draw_text_clipped(inText, reinterpret_cast<const float(*)[4]>(&inRectangle), *static_cast<DX7Surface*>(inTarget));
}

// The model draws are Rust (ee_dx7 draw_model.rs; the original's CopyStd*/CopyAndSmooth* are its blend_*/white_copy, the
// transform stack a call parameter).

// Draws an indexed triangle list with an identity world transform and D3D clipping on; counts the triangles drawn
long DX7Rasterizer::Draw(GEPolygonArray& inPolygons, unsigned long inCount, GEVertexArray& inVertices,
                         unsigned long inVertexCount, GETexture* inTexture, unsigned long inScreen, bool inSorted)
{
    return ee_dx7r_draw_polygon_array(this->mDX7, inScreen, inPolygons.GetIndices(), inCount, inVertices.GetType(),
                                      inVertices.GetVertices(), inVertexCount, inTexture);
}

// Draws a model and its sub-models under the drawing lock (the float argument becomes the level-of-detail divisor)
long DX7Rasterizer::Draw(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inOrthographic)
{
    return ee_dx7r_draw_model(this->mDX7, inScreen, inModel, inOpacity, inOrthographic);
}

// Draws a textured polyline (at most 1200 segments) as a red line list in camera space, advancing the texture V
// along it (without the drawing lock)
long DX7Rasterizer::Draw3DLine(std::vector<std::pair<GE3DPoint, float> >& inPoints, GETexture* inTexture,
                               float inV, unsigned long inScreen, bool inOrthographic)
{
    static_assert(sizeof(std::pair<GE3DPoint, float>) == 16, "ee_dx7/src/draw_model.rs draw_3d_line: (x, y, z, advance)");
    return ee_dx7r_draw_3d_line(this->mDX7, inScreen, reinterpret_cast<const float(*)[4]>(inPoints.data()), inPoints.size(),
                                inTexture, inV, inOrthographic);
}

// Draws the sky model like any model; always returns kErrorOK
long DX7Rasterizer::DrawSky(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inOrthographic)
{
    this->Draw(inModel, inScreen, inOpacity, inOrthographic);
    return kErrorOK;
}

// The terrain draws are Rust (ee_dx7 draw_terrain.rs), reading the mesh's visible frame off its Rust object (ee_ge tmobj.rs).
static_assert(sizeof(std::pair<U2DPoint<long>, U2DPoint<long> >) == 4 * sizeof(long), "ee_dx7 draw_terrain grid lines");

// Draws the visible terrain texture by texture, unclipped then clipped polygons (space maps draw materials 1..8 first);
// with no visible tile row fills the view with two black quads instead
long DX7Rasterizer::Draw(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_terrain(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic);
}

// Draws grid lines between tile corners whose both ends are visible, as one red line list of at most 1200 lines
long DX7Rasterizer::DrawGridLines(GETerrainMesh& inMesh,
                                  std::vector<std::pair<U2DPoint<long>, U2DPoint<long> > >& inLines,
                                  GETexture* inTexture, float inWidth, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_grid_lines(this->mDX7, inScreen, inMesh.GetRust(), reinterpret_cast<const long(*)[4]>(inLines.data()),
                                   inLines.size(), inTexture, inWidth, inOrthographic);
}

// Draws two red edge lines for every visible tile, one draw call per tile
long DX7Rasterizer::DrawTerrainGrid(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_terrain_grid(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic);
}

// Shades a tile rectangle with a translucent colour over every valid visible tile, drawn in batches of 400 quads
long DX7Rasterizer::DrawTerrainHighlight(GETerrainMesh& inMesh, const U2DRectangle<long>& inRectangle,
                                         unsigned long inScreen, bool inOrthographic, const GEColor& inColor,
                                         float inOpacity)
{
    return ee_dx7r_draw_terrain_highlight(this->mDX7, inScreen, inMesh.GetRust(), &inRectangle.mBottom, reinterpret_cast<const float*>(&inColor),
                                          inOpacity, inOrthographic);
}

// Draws every water material, unclipped then clipped polygons, and counts its triangles
long DX7Rasterizer::DrawWater(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_water(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic);
}

// Lights the visible terrain and water vertices: ambient plus each active directional light, normalised, times
// the vertex colour
void DX7Rasterizer::Light(GETerrainMesh& inMesh, unsigned long inScreen) { ee_dx7r_light_terrain(this->mDX7, inScreen, inMesh.GetRust()); }

// Moves the visible terrain and water vertices to camera space (animating the water waves) and sets their clip flags,
// then sorts each material's polygons into unclipped and clipped lists; water-only mode skips dry vertices
void DX7Rasterizer::TransformToCamera(GETerrainMesh& inMesh, GEViewport& inViewport, float inScale, bool inWaterOnly,
                                      bool inOrthographic)
{
    ee_dx7r_transform_to_camera(this->mDX7, inMesh.GetRust(), &inViewport, inScale, inWaterOnly, inOrthographic);
}

// ---- the screens, surfaces and movies (DX7Trampolines.cpp, folded here in R4.3 part 17): their objects' hosts and
// constructors, and the layouts Rust fills

#include "lle/LowLevelEngine.h"
#include <stddef.h>
extern "C" void ee_dx7cpp_record_native_error(long inError) { ESRecordNativeError((unsigned long)inError); }

// --- DX7Screen and DX7Surface: whole in Rust (ee_dx7 scrobj.rs, surfobj.rs); the calls they make on C++ objects ------
#include "platform/PlatWindow.h"
#include "platform/PlatSim.h"
static_assert(offsetof(GERectangle, mBottom) == 0 && offsetof(GERectangle, mLeft) == 4 && offsetof(GERectangle, mRight) == 8 &&
              offsetof(GERectangle, mTop) == 12 && sizeof(GEColor) == 12, "ee_dx7/src/surfobj.rs ge_rect");

static intptr_t DX7ObjectHost(EeDx7ObjOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeDx7ObjOp::NameNew: return (intptr_t) new UString(*static_cast<const UString*>(inObj));
    case EeDx7ObjOp::NameDelete: delete static_cast<UString*>(inObj); return 0;
    case EeDx7ObjOp::SurfaceNew:
        return (intptr_t) static_cast<GESurface*>(new DX7Surface(static_cast<GXSurface*>(inObj), *reinterpret_cast<const GESurfaceDescription*>(a),
                                                                 reinterpret_cast<DX7Screen*>(b)));
    case EeDx7ObjOp::SurfaceObject: return (intptr_t) static_cast<DX7Surface*>(static_cast<GESurface*>(inObj))->mDX7;
    case EeDx7ObjOp::SurfaceReference: static_cast<GESurface*>(inObj)->Reference(); return 0;
    case EeDx7ObjOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
    case EeDx7ObjOp::SurfaceAllocatedSize: return (intptr_t)static_cast<GESurface*>(inObj)->GetAllocatedSize();
    case EeDx7ObjOp::SurfaceMetrics:
    {   // ee_dx7/src/surface.rs LogicalMetrics: logical width and height set, pixel width and height, logical shift
        const uint32_t theMetrics[5] = { (uint32_t)static_cast<GESurface*>(inObj)->GetLogicalWidthSet(), (uint32_t)static_cast<GESurface*>(inObj)->GetLogicalHeightSet(),
                                         (uint32_t)static_cast<GESurface*>(inObj)->GetPixelWidth(), (uint32_t)static_cast<GESurface*>(inObj)->GetPixelHeight(),
                                         (uint32_t)static_cast<GESurface*>(inObj)->GetLogicalShift() };
        memcpy(reinterpret_cast<void*>(a), theMetrics, sizeof theMetrics);
        return 0;
    }
    case EeDx7ObjOp::ProbeWindow: {   // Test32BitSupport's hidden 16 x 16 window
        UString theClassName("TestWindowClass"), theTitle("");
        return static_cast<DX7Screen*>(inObj)->CreateAWindow(GE2DPoint(0.0f, 0.0f), 16, 16, theClassName, theTitle,
                                                             *reinterpret_cast<GEWindow**>(a), 2, NULL);
    }
    case EeDx7ObjOp::WindowOs: return (intptr_t) static_cast<GEWindow*>(inObj)->GetOSRepresentation();
    case EeDx7ObjOp::WindowDelete: delete static_cast<GEWindow*>(inObj); return 0;
    case EeDx7ObjOp::DispatchEvents: TSThread::DispatchWindowEvents(); return 0;
    default: return 0;
    }
}

// Creates a screen for a DirectDraw device: lists its display modes, finds the current one, probes 32-bit support and
// registers it (the original's private ctor 0x1000d271; the rasterizer makes the screens while it enumerates)
DX7Screen::DX7Screen(void* inMonitor, GXDirectDraw* inDirectDraw, const UString& inName, bool inAttachedToDesktop,
                     bool inSharesPrimary)
{
    this->mDX7 = ee_dx7sc_new(DX7ObjectHost, this, inMonitor, inDirectDraw, &inName, inAttachedToDesktop, inSharesPrimary);
    ee_dx7sc_start(this->mDX7);
}

// CreateAWindow's C++ (ee_dx7 scrobj.rs create_window): the window object and the failure's debug line
static void* DX7NewWindow(void* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inStyle)
    { return new GEWindow(static_cast<PlatWindow*>(inWindow), inWidth, inHeight, inStyle, true); }
static void DX7WindowError(const char* inError) { ESDebugOut("CreateAWindow: %s\n", inError); }

// Creates a window (a platform window: platform/PlatWindow.h): style 1 is a framed, resizable window with an
// inWidth x inHeight client area, else a topmost borderless one at inOrigin (the original's popup), which is the full
// screen (borderless, no mode change) when it covers the display. The class name and the parent are unused. (0x1000d622)
long DX7Screen::CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                              UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                              GEWindow* inParent)
{
    return ee_dx7sc_create_window(gPlatSimMode != 0, inOrigin.GetX(), inOrigin.GetY(), inWidth, inHeight, inTitle, inStyle,
                                  reinterpret_cast<void**>(&outWindow), DX7NewWindow, DX7WindowError);
}

// Wraps a GX surface with its description and parent screen, unlocked and with no attachments (0x1000eced)
DX7Surface::DX7Surface(GXSurface* inSurface, const GESurfaceDescription& inDescription, GEScreen* inParentScreen)
{
    this->mDX7 = ee_dx7sf_new(DX7ObjectHost, static_cast<GESurface*>(this), inSurface,
                              reinterpret_cast<const EeDx7SurfaceDescription*>(&inDescription), inParentScreen);
}

// ---- the renderer the game links (DX7HRTnLDisplay.cpp's globals; its PIM entry points went with the plug-in boundary)
static UString gName("Direct3D Hardware TnL");
static DX7Rasterizer gTnLRasterizer(gName, true);   // the T&L flavour (rastobj.rs)

// The game's rasterizer (GEInitialize adds it when it finds a screen)
GERasterizer* DX7TnLRasterizer()
{
    return &gTnLRasterizer;
}
