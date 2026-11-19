// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer.cpp
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x100091aa..0x1000d0fc: the U2D key init chains, the DirectDraw/Direct3D enumeration callbacks, DX7Rasterizer's
// ctor and dtor, then its members in alphabetical order, then the vector<DX7RScreenState*> COMDATs and the
// template-static init chain head. The enumeration callbacks and ActivateRasterizer were built without global
// optimisation (see the pragmas below); everything else is -O1.
#include <windows.h>
#include <stdint.h>
#include "gx/GX.h"
#include <float.h>

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "lle/GEPolygonArray.h"
#include "rust/render/dx7.h"
#include "rust/render/ge.h"
#include "lle/TSCriticalSection.h"
#include "platform/PlatFont.h"
#include "lle/GEBitmap.h"
#include "lle/GEFont.h"
#include "lle/GEFontBitmap.h"
#include "lle/GELine.h"
#include "lle/GEMaterial.h"
#include "lle/GEPoint.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"

// DX7Rasterizer is whole in Rust (rust/crates/ee_render/src/dx7/rastobj.rs, R4.3 part 13): the per-screen DX7 records, the
// debug colour, the light slots and every method below; DX7TnLRasterizer is its T&L flavour. The class keeps its vtable
// (the game calls it through GERasterizer) and one member, mDX7; the methods forward, and DX7RasterizerHost makes the
// calls on other objects Rust asks for.

// The calls the Rust rasterizer makes on other objects (EeDx7RastOp), in the original's order.
struct DX7RasterizerHost {
    static intptr_t Call(void* inOwner, EeDx7RastOp inOp, void* inObj, intptr_t a, intptr_t b)
    {
        GEScreen* theScreen = static_cast<GEScreen*>(inObj);
        GESurface* theSurface = static_cast<GESurface*>(inObj);
        GETexture* theTexture = static_cast<GETexture*>(inObj);
        switch (inOp) {
        case EeDx7RastOp::ScreenNew:
            return (intptr_t) static_cast<GEScreen*>(new DX7Screen(NULL, static_cast<GXDirectDraw*>(inObj),
                                                                   UString(reinterpret_cast<const char*>(a)), (b & 1) != 0, (b & 2) != 0));
        case EeDx7RastOp::ScreenDelete: delete static_cast<DX7Screen*>(theScreen); return 0;
        case EeDx7RastOp::ScreenDirectDraw: return (intptr_t)(GXDirectDraw*)*static_cast<DX7Screen*>(theScreen);
        case EeDx7RastOp::ScreenIsFullScreen: return theScreen->GetIsFullScreen();
        case EeDx7RastOp::ScreenCurrentMode: *reinterpret_cast<GEVideoMode*>(a) = theScreen->GetCurrentMode(); return 0;
        case EeDx7RastOp::ScreenModeCount: return (intptr_t)theScreen->GetModeCount();
        case EeDx7RastOp::ScreenMode: *reinterpret_cast<GEVideoMode*>(b) = theScreen->GetMode((unsigned long)a); return 0;
        case EeDx7RastOp::ScreenCreateSurface:
            return theScreen->CreateSurface(*reinterpret_cast<GESurfaceDescription*>(a), reinterpret_cast<GESurface**>(b));
        case EeDx7RastOp::ScreenSetTextureManager: theScreen->SetTextureManager(reinterpret_cast<GETextureManager*>(a)); return 0;
        case EeDx7RastOp::ManagerSetScreen: static_cast<GETextureManager*>(inObj)->SetScreen(reinterpret_cast<GEScreen*>(a)); return 0;
        case EeDx7RastOp::SurfaceAttached: return (intptr_t)theSurface->GetAttachedSurface(0);
        case EeDx7RastOp::SurfaceAttach: return theSurface->AttachSurface(reinterpret_cast<GESurface*>(a));
        case EeDx7RastOp::SurfaceReference: theSurface->Reference(); return 0;
        case EeDx7RastOp::SurfaceDereference: theSurface->Dereference(); return 0;
        case EeDx7RastOp::SurfaceGx: return (intptr_t)(GXSurface*)*static_cast<DX7Surface*>(theSurface);
        case EeDx7RastOp::SurfaceGxChecked: return (intptr_t)(GXSurface*)*dynamic_cast<DX7Surface*>(theSurface);
        case EeDx7RastOp::SurfaceSize:
            reinterpret_cast<uint32_t*>(a)[0] = theSurface->GetWidth();
            reinterpret_cast<uint32_t*>(a)[1] = theSurface->GetHeight();
            return 0;
        case EeDx7RastOp::SurfaceFlip: return theSurface->Flip(a != 0);
        case EeDx7RastOp::TextureHasChanged: return theTexture->HasChanged();
        case EeDx7RastOp::TextureReference: theTexture->Reference(); return 0;
        case EeDx7RastOp::TextureDereference: theTexture->Dereference(); return 0;
        case EeDx7RastOp::TexturePromote: theTexture->Promote(); return 0;
        case EeDx7RastOp::TextureStage: {
            const GETStage* theStage = theTexture->GetStage();
            EeDx7StageView* theView = reinterpret_cast<EeDx7StageView*>(a);
            theView->surface = (GXSurface*)*static_cast<DX7Surface*>(theTexture->GetStageTexture(0)->GetFrameSurface());
            theView->mColorMode = theStage->mColorMode;
            theView->mAlphaMode = theStage->mAlphaMode;
            theView->mTextureAddressingMode = theStage->mTextureAddressingMode;
            theView->mMinFilterMode = theStage->mMinFilterMode;
            theView->mMagFilterMode = theStage->mMagFilterMode;
            theView->mMipFilterMode = theStage->mMipFilterMode;
            theView->mMipMapBias = theStage->mMipMapBias;
            return 0;
        }
        case EeDx7RastOp::TextureValidate: theTexture->ValidateChanges(); return 0;
        case EeDx7RastOp::WindowOrigin: {
            GE2DPoint theOrigin = static_cast<GEWindow*>(inObj)->GetClientOrigin();
            reinterpret_cast<float*>(a)[0] = theOrigin.GetX();
            reinterpret_cast<float*>(a)[1] = theOrigin.GetY();
            return 0;
        }
        case EeDx7RastOp::LightParams: {
            GELight& theLight = *static_cast<GELight*>(inObj);
            EeDx7LightParams* theParams = reinterpret_cast<EeDx7LightParams*>(a);
            const float theFields[] = { theLight.GetLocation().GetX(), theLight.GetLocation().GetY(), theLight.GetLocation().GetZ(),
                                        theLight.GetDirection().GetX(), theLight.GetDirection().GetY(), theLight.GetDirection().GetZ(),
                                        theLight.GetComputedColor().GetRed(), theLight.GetComputedColor().GetGreen(),
                                        theLight.GetComputedColor().GetBlue() };
            memcpy(theParams->location, theFields, sizeof theFields);
            theParams->style = theLight.GetStyle();
            theParams->attenuation[0] = theLight.GetAttenuation0();
            theParams->attenuation[1] = theLight.GetAttenuation1();
            theParams->attenuation[2] = theLight.GetAttenuation2();
            theParams->range = theLight.GetRange();
            return 0;
        }
        case EeDx7RastOp::LockEnter: static_cast<TSCriticalSection*>(inObj)->Enter(); return 0;
        case EeDx7RastOp::LockLeave: static_cast<TSCriticalSection*>(inObj)->Leave(); return 0;
        case EeDx7RastOp::Time: return (intptr_t)TMGetTime();
        case EeDx7RastOp::Verify: ESVERIFY_AT(a != 0, (unsigned long)b); return 0;
        default: return 0;
        }
    }
};


// GERectangle's left, top, right, bottom (Rust's order; the class keeps another)
static void DX7Rect(const GERectangle& inRectangle, float outRect[4])
{
    outRect[0] = inRectangle.GetLeft();
    outRect[1] = inRectangle.GetTop();
    outRect[2] = inRectangle.GetRight();
    outRect[3] = inRectangle.GetBottom();
}

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
    uint32_t theIndex = 0;
    long theResult = ee_dx7r_add_light(this->mDX7, &inLight, inScreen, &theIndex);
    outIndex = theIndex;
    return theResult;
}

// Sets the render target and begins the scene, waiting while the device is still drawing; 9 when the surface is lost
long DX7Rasterizer::BeginScene(GESurface* inTarget, unsigned long inScreen) { return ee_dx7r_begin_scene(this->mDX7, inTarget, inScreen); }

// Fills a rectangle of the render target with a colour, waiting while the device is still drawing
long DX7Rasterizer::Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen)
{
    float theRect[4];
    DX7Rect(inRectangle, theRect);
    return ee_dx7r_clear(this->mDX7, theRect, reinterpret_cast<const EeGeRastColor*>(&inColor), 0.0f, inScreen);
}

// Fills a rectangle of the Z-buffer with a depth value, waiting while the device is still drawing
long DX7Rasterizer::ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen)
{
    float theRect[4];
    DX7Rect(inRectangle, theRect);
    return ee_dx7r_clear(this->mDX7, theRect, NULL, inZ, inScreen);
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
    ee_dx7r_deactivate(this->mDX7);
    return kErrorOK;
}

// Does nothing (textures need no handle here)
void DX7Rasterizer::DestroyTextureHandle(GETexture* inTexture)
{
}

// Ends the scene, waiting while the device is still drawing; 9 when the surface is lost
long DX7Rasterizer::EndScene(unsigned long inScreen) { return ee_dx7r_end_scene(this->mDX7, inScreen); }

// Returns whether the screen's card can modulate texture alpha
bool DX7Rasterizer::GetCanModulateTextureAlpha(unsigned long inScreen) { return ee_dx7r_card_flag(this->mDX7, inScreen, false); }

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
    uint32_t theBytes = 0;
    long theResult = ee_dx7r_video_memory(this->mDX7, inScreen, true, &theBytes);
    if (theResult == kErrorOK)
        outBytes = theBytes;
    return theResult;
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

static_assert(sizeof(EeDx7TextRect) == sizeof(RECT), "ee_dx7/src/draw2d.rs TextRect");

// One text draw (GDI in the original): a GXSurface::DrawText callback; the target covers the surface from
// (inOriginX, inOriginY). The font is referenced around the draw, as the original did.
struct DX7TextJob {
    GEText* mText;
    const RECT* mClip;
    long mResult;
};

static bool DX7DrawTextPlat(const PlatFontTarget& inTarget, long inOriginX, long inOriginY, void* inJob)
{
    DX7TextJob* theJob = static_cast<DX7TextJob*>(inJob);
    GEFont* theFont = theJob->mText->GetFont();
    theFont->Reference();
    if (!ee_dx7r_text_draw(theJob->mText, reinterpret_cast<const EeDx7TextRect*>(theJob->mClip), inOriginX, inOriginY,
                           inTarget.mWidth, inTarget.mHeight, inTarget.mScale, &inTarget))
        theJob->mResult = kErrorNativeError;
    theFont->Dereference();
    return true;
}

// Draws a text on the target: onto the GPU copy of a GPU-resident target (no read-back), else onto its CPU copy (a
// text-locked surface: the glyph atlases)
static long DX7DrawText(GEText* inText, const RECT* inClip, GESurface* inTarget)
{
    DX7TextJob theJob = {inText, inClip, kErrorOK};
    GXSurface* theSurface = *static_cast<DX7Surface*>(inTarget);
    EeDx7TextRect theBounds;
    ee_dx7r_text_bounds(inText, reinterpret_cast<const EeDx7TextRect*>(inClip), &theBounds);
    RECT theRect;
    memcpy(&theRect, &theBounds, sizeof(RECT));
    if (theSurface->DrawText(theRect, DX7DrawTextPlat, &theJob) != GX_OK && theJob.mResult == kErrorOK)
        theJob.mResult = kErrorNativeError;
    return theJob.mResult;
}

// Draws a text with GDI on the target, aligned; shadow fonts get a black or white shadow 1 pixel down-right
long DX7Rasterizer::Draw(GEText* inText, GESurface* inTarget)
{
    return DX7DrawText(inText, NULL, inTarget);
}

// Draws a text clipped to a rectangle: with the font's bitmap font when it has one, else with GDI (aligned, shadowed)
long DX7Rasterizer::Draw(GEText* inText, const GERectangle& inRectangle, GESurface* inTarget)
{
    if (inText->GetFont()->GetFontBitmap() != NULL)
        return inText->GetFont()->GetFontBitmap()->Draw(inText, inRectangle, this, 0);
    RECT theClip;
    theClip.top = UMFloor(inRectangle.GetTop());
    theClip.left = UMFloor(inRectangle.GetLeft());
    theClip.right = UMFloor(inRectangle.GetRight());
    theClip.bottom = UMFloor(inRectangle.GetBottom());
    return DX7DrawText(inText, &theClip, inTarget);
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
    const long theRect[4] = {inRectangle.mLeft, inRectangle.mTop, inRectangle.mRight, inRectangle.mBottom};
    return ee_dx7r_draw_terrain_highlight(this->mDX7, inScreen, inMesh.GetRust(), theRect, reinterpret_cast<const float*>(&inColor),
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
#include "lle/Graphics Engine.h"
#include <stddef.h>
#include "lle/GESurface.h"
#define DX7_SAME_OFFSET(cpp, rust, field) static_assert(offsetof(cpp, field) == offsetof(rust, field), #field)

// --- GESurfaceDescription: filled by Rust (ee_dx7/src/api.rs SurfaceDescription) --------------------------------------
static_assert(sizeof(GESurfaceDescription) == sizeof(EeDx7SurfaceDescription), "ee_dx7/src/api.rs SurfaceDescription");
#include "lle/GEScreen.h"
static_assert(sizeof(GEVideoMode) == sizeof(EeDx7VideoMode), "ee_dx7/src/screen.rs VideoMode");
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mWidth);
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mHeight);
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mRefreshRate);
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mBitCount);
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mPixelFormat);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mType);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mPixelFormat);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mSurfaceVolatility);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mWidth);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mHeight);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, m14);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mMipLevelCount);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mPixelDepth);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mIs3DRenderable);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mIsTransparencyEnabled);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mNoAlpha);

extern "C" void ee_dx7cpp_record_native_error(long inError) { ESRecordNativeError((unsigned long)inError); }

// --- DX7Screen and DX7Surface: whole in Rust (ee_dx7 scrobj.rs, surfobj.rs); the calls they make on C++ objects ------
#include "GEWindow.h"
#include "lle/UString.h"
#include "platform/PlatWindow.h"
#include "platform/PlatSim.h"
static_assert(offsetof(GERectangle, mBottom) == 0 && offsetof(GERectangle, mLeft) == 4 && offsetof(GERectangle, mRight) == 8 &&
              offsetof(GERectangle, mTop) == 12 && sizeof(GEColor) == 12, "ee_dx7/src/surfobj.rs ge_rect");

static intptr_t DX7ObjectHost(EeDx7ObjOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    GESurface* theSurface = static_cast<GESurface*>(inObj);
    switch (inOp) {
    case EeDx7ObjOp::NameNew: return (intptr_t) new UString(*static_cast<const UString*>(inObj));
    case EeDx7ObjOp::NameDelete: delete static_cast<UString*>(inObj); return 0;
    case EeDx7ObjOp::SurfaceNew:
        return (intptr_t) static_cast<GESurface*>(new DX7Surface(static_cast<GXSurface*>(inObj), *reinterpret_cast<const GESurfaceDescription*>(a),
                                                                 reinterpret_cast<DX7Screen*>(b)));
    case EeDx7ObjOp::SurfaceObject: return (intptr_t) static_cast<DX7Surface*>(theSurface)->mDX7;
    case EeDx7ObjOp::SurfaceReference: theSurface->Reference(); return 0;
    case EeDx7ObjOp::SurfaceDereference: theSurface->Dereference(); return 0;
    case EeDx7ObjOp::SurfaceAllocatedSize: return (intptr_t)theSurface->GetAllocatedSize();
    case EeDx7ObjOp::SurfaceMetrics:
    {   // ee_dx7/src/surface.rs LogicalMetrics: logical width and height set, pixel width and height, logical shift
        const uint32_t theMetrics[5] = { (uint32_t)theSurface->GetLogicalWidthSet(), (uint32_t)theSurface->GetLogicalHeightSet(),
                                         (uint32_t)theSurface->GetPixelWidth(), (uint32_t)theSurface->GetPixelHeight(),
                                         (uint32_t)theSurface->GetLogicalShift() };
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

// Creates a window (a platform window: platform/PlatWindow.h): style 1 is a framed, resizable window with an
// inWidth x inHeight client area, else a topmost borderless one at inOrigin (the original's popup), which is the full
// screen (borderless, no mode change) when it covers the display. The class name and the parent are unused. (0x1000d622)
long DX7Screen::CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                              UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                              GEWindow* inParent)
{
    int32_t theDisplayWidth = 0, theDisplayHeight = 0;
    // window sizes are in the display's own units (macOS: points), not its pixels
    PlatWindowGetDisplayLogicalSize(NULL, &theDisplayWidth, &theDisplayHeight);
    EeDx7WindowParams theParams;
    ee_dx7sc_window_params(gPlatSimMode != 0, inStyle, (int32_t)inWidth, (int32_t)inHeight, theDisplayWidth, theDisplayHeight,
                           inOrigin.GetX(), inOrigin.GetY(), &theParams);
    PlatWindow* theWindow = theParams.create_at
        ? PlatWindowCreateAt(inTitle, theParams.x, theParams.y, theParams.width, theParams.height, theParams.flags)
        : PlatWindowCreate(inTitle, theParams.width, theParams.height, theParams.flags);
    if (theWindow == NULL) {
        ESDebugOut("CreateAWindow: %s\n", PlatWindowLastError());
        ESRecordNativeError(PlatGetLastError());
        ESGetNativeError();
        return kErrorNativeError;
    }
    PlatCursorShow(0);   // the game draws its own pointer over its windows (WM_SETCURSOR's SetCursor(NULL))
    TSThread::DispatchWindowEvents();   // the messages CreateWindowEx sent (the window is not the game's yet)
    outWindow = new GEWindow(theWindow, inWidth, inHeight, inStyle, true);
    return kErrorOK;
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
