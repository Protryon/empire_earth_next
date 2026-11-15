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
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "dx7/DX7Movie.h"
#include "dx7/DX7StdColorVertexArray.h"
#include "rust/ee_dx7.h"
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

// The U2D key init chains:
// SYNTHETIC: DX7HR 0x100091aa
// SYNTHETIC: DX7HR 0x100091b4
// SYNTHETIC: DX7HR 0x100091c5
// SYNTHETIC: DX7HR 0x100091d1
// SYNTHETIC: DX7HR 0x100091dc
// SYNTHETIC: DX7HR 0x100091e6
// SYNTHETIC: DX7HR 0x100091f7
// SYNTHETIC: DX7HR 0x10009203
// SYNTHETIC: DX7HR 0x1000920e
// SYNTHETIC: DX7HR 0x10009218
// SYNTHETIC: DX7HR 0x10009229
// SYNTHETIC: DX7HR 0x10009235
// SYNTHETIC: DX7HR 0x10009240
// SYNTHETIC: DX7HR 0x1000924a
// SYNTHETIC: DX7HR 0x1000925b
// SYNTHETIC: DX7HR 0x10009267
// SYNTHETIC: DX7HR 0x10009272
// SYNTHETIC: DX7HR 0x1000927c
// SYNTHETIC: DX7HR 0x1000928d
// SYNTHETIC: DX7HR 0x10009299
// SYNTHETIC: DX7HR 0x100092a4
// SYNTHETIC: DX7HR 0x100092ae
// SYNTHETIC: DX7HR 0x100092bf
// SYNTHETIC: DX7HR 0x100092cb
// SYNTHETIC: DX7HR 0x100092d6
// SYNTHETIC: DX7HR 0x100092e0
// SYNTHETIC: DX7HR 0x100092f1
// SYNTHETIC: DX7HR 0x100092fd

// DX7Rasterizer is whole in Rust (rust/crates/ee_dx7/src/rastobj.rs, R4.3 part 13): the per-screen DX7 records, the
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

static_assert(sizeof(GEVideoMode) == sizeof(EeDx7VideoMode) && sizeof(GESurfaceDescription) == sizeof(EeDx7SurfaceDescription),
              "ee_dx7/src/screen.rs VideoMode, api.rs SurfaceDescription");

// GERectangle's left, top, right, bottom (Rust's order; the class keeps another)
static void DX7Rect(const GERectangle& inRectangle, float outRect[4])
{
    outRect[0] = inRectangle.GetLeft();
    outRect[1] = inRectangle.GetTop();
    outRect[2] = inRectangle.GetRight();
    outRect[3] = inRectangle.GetBottom();
}

// FUNCTION: DX7HR 0x1000978b
// Creates the DirectX 7 rasterizer with the given name and a dark blue debug colour (inTnL: the T&L rasterizer)
DX7Rasterizer::DX7Rasterizer(const UString& inName, bool inTnL)
    : GERasterizer(inName)
{
    this->mDX7 = ee_dx7r_new(&DX7RasterizerHost::Call, this, this->mRust, inTnL);
}

// FUNCTION: DX7HR 0x100097bd
// Destroys the rasterizer (the DX7 records; the base's in the base)
DX7Rasterizer::~DX7Rasterizer()
{
    ee_dx7r_destroy(this->mDX7);
}

// FUNCTION: DX7HR 0x100097d8
// Enumerates the DirectDraw devices into screens (Ex enumeration on 98/2000) and makes this the active rasterizer
long DX7Rasterizer::ActivateRasterizer() { return ee_dx7r_activate_rasterizer(this->mDX7); }

// FUNCTION: DX7HR 0x10009876
// Sets up a screen to render into an offscreen surface of the given size at the desktop depth, retrying with a
// 16-bit mode if that fails
long DX7Rasterizer::ActivateRenderingSurface(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                             unsigned long inWidth, unsigned long inHeight)
{
    return ee_dx7r_activate_rendering_surface(this->mDX7, inScreen, inManager, inWindow, inWidth, inHeight);
}

// FUNCTION: DX7HR 0x10009ab4
// Sets up a screen's primary, back buffer and optional Z-buffer (flipping chain full screen, offscreen buffer
// windowed), activates Direct3D on them and forces every render state to its default
long DX7Rasterizer::ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                   unsigned long inBackBuffers, bool inZBuffer, unsigned long inZBufferFormat)
{
    return ee_dx7r_activate_screen(this->mDX7, inScreen, inManager, inWindow, inWindow->GetWidth(), inWindow->GetHeight(), inBackBuffers,
                                   inZBuffer, inZBufferFormat);
}

// FUNCTION: DX7HR 0x10009fa2
// Creates the HAL (or T&L) Direct3D device on the back buffer, sets the viewport and fixed render states, the vertex
// buffer and the card quirks (texture alpha modulation off on small Rage Pro cards, no dirty rectangles on two cards)
long DX7Rasterizer::ActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary,
                                      GETextureManager* inManager, GEWindow* inWindow)
{
    return ee_dx7r_activate_the_screen(this->mDX7, inScreen, inSecondary);
}

// FUNCTION: DX7HR 0x1000a581
// Takes a free light slot (of 8), sets the light into it and enables it; returns 7 when all slots are in use
long DX7Rasterizer::AddLight(GELight& inLight, unsigned long inScreen, unsigned long& outIndex)
{
    uint32_t theIndex = 0;
    long theResult = ee_dx7r_add_light(this->mDX7, &inLight, inScreen, &theIndex);
    outIndex = theIndex;
    return theResult;
}

// FUNCTION: DX7HR 0x1000a5ff
// Adds a screen with a fresh DirectX 7 screen state
long DX7Rasterizer::AddValidScreen(GEScreen* inScreen)
{
    ee_dx7r_add_valid_screen(this->mDX7, inScreen);
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000a64e
// Sets the render target and begins the scene, waiting while the device is still drawing; 9 when the surface is lost
long DX7Rasterizer::BeginScene(GESurface* inTarget, unsigned long inScreen) { return ee_dx7r_begin_scene(this->mDX7, inTarget, inScreen); }

// FUNCTION: DX7HR 0x1000a6af
// Sets the texture and asks the device whether it can draw the current state in one pass
bool DX7Rasterizer::CanDoCurrentState(GETexture* inTexture, unsigned long inScreen)
{
    return ee_dx7r_can_do_current_state(this->mDX7, inTexture, inScreen);
}

// FUNCTION: DX7HR 0x1000a6f7
// Fills a rectangle of the render target with a colour, waiting while the device is still drawing
long DX7Rasterizer::Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen)
{
    float theRect[4];
    DX7Rect(inRectangle, theRect);
    return ee_dx7r_clear(this->mDX7, theRect, reinterpret_cast<const EeGeRastColor*>(&inColor), 0.0f, inScreen);
}

// FUNCTION: DX7HR 0x1000a861
// Fills a rectangle of the Z-buffer with a depth value, waiting while the device is still drawing
long DX7Rasterizer::ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen)
{
    float theRect[4];
    DX7Rect(inRectangle, theRect);
    return ee_dx7r_clear(this->mDX7, theRect, NULL, inZ, inScreen);
}

// FUNCTION: DX7HR 0x1000a9e5
// Creates a movie player for a file on the given screen
GEMovie* DX7Rasterizer::CreateMovie(const FSFileSpec& inFile, unsigned long inScreen)
{
    return new DX7Movie(inFile, inScreen);
}

// FUNCTION: DX7HR 0x1000aa07
// Creates a movie player for a file in a rectangle of the given screen, optionally looping
GEMovie* DX7Rasterizer::CreateMovie(const FSFileSpec& inFile, unsigned long inScreen, const GERectangle& inRectangle,
                                    bool inLoop)
{
    return new DX7Movie(inFile, inScreen, inRectangle, inLoop);
}

// FUNCTION: DX7HR 0x1000aa3e
// Creates a polygon array of the given size
GEPolygonArray* DX7Rasterizer::CreatePolygonArray(unsigned long inCount)
{
    return new DX7PolygonArray(inCount);
}

// FUNCTION: DX7HR 0x1000aa5c
// Does nothing and returns success (textures need no handle here)
long DX7Rasterizer::CreateTextureHandle(GETexture* inTexture)
{
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000aa61
// Creates a vertex array of type 0 (standard) or 1 (coloured); null for any other type
GEVertexArray* DX7Rasterizer::CreateVertexArray(unsigned long inType, unsigned long inCount)
{
    if (inType == 0)
        return new DX7StandardVertexArray(inCount);
    return inType == 1 ? new DX7StdColorVertexArray(inCount) : NULL;
}

long DX7Rasterizer::DemoteTexture(GETexture* inTexture)
{
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000ab24
// Releases every screen's texture, vertex buffer, lights, device and Direct3D, deletes the screens and their states
long DX7Rasterizer::Deactivate()
{
    ee_dx7r_deactivate(this->mDX7);
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000abbf
// Does nothing (textures need no handle here)
void DX7Rasterizer::DestroyTextureHandle(GETexture* inTexture)
{
}

// FUNCTION: DX7HR 0x1000abc2
// Ends the scene, waiting while the device is still drawing; 9 when the surface is lost
long DX7Rasterizer::EndScene(unsigned long inScreen) { return ee_dx7r_end_scene(this->mDX7, inScreen); }

// FUNCTION: DX7HR 0x1000ac18
// Returns the free video memory of the screen
long DX7Rasterizer::GetAvailableVideoMemory(unsigned long inScreen, unsigned long& outBytes)
{
    uint32_t theBytes = 0;
    long theResult = ee_dx7r_video_memory(this->mDX7, inScreen, false, &theBytes);
    if (theResult == kErrorOK)
        outBytes = theBytes;
    return theResult;
}

// FUNCTION: DX7HR 0x1000ac48
// Returns whether the screen's card can modulate texture alpha
bool DX7Rasterizer::GetCanModulateTextureAlpha(unsigned long inScreen) { return ee_dx7r_card_flag(this->mDX7, inScreen, false); }

// FUNCTION: DX7HR 0x1000ac58
// Returns 0 for any capability
unsigned long DX7Rasterizer::GetCaps(unsigned long inCap, unsigned long inScreen)
{
    return 0;
}

// FUNCTION: DX7HR 0x1000ac5d
// Returns whether the screen's card supports dirty-rectangle updates
bool DX7Rasterizer::GetDirtyRectangleSupport(unsigned long inScreen) { return ee_dx7r_card_flag(this->mDX7, inScreen, true); }

// FUNCTION: DX7HR 0x1000ad17
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

// FUNCTION: DX7HR 0x1000ae3a
// Removes all 8 lights
long DX7Rasterizer::RemoveAllLights(unsigned long inScreen) { return ee_dx7r_remove_all_lights(this->mDX7, inScreen); }

// FUNCTION: DX7HR 0x1000ae5c
// Disables a light and frees its slot; 9 when the slot was not in use
long DX7Rasterizer::RemoveLight(unsigned long inIndex, unsigned long inScreen) { return ee_dx7r_remove_light(this->mDX7, inIndex, inScreen); }

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
void DX7Rasterizer::SetDebugColor(const GEColor& inColor)
{
    ee_dx7r_set_color(this->mDX7, EeDx7RastColor::Debug, reinterpret_cast<const EeGeRastColor*>(&inColor), 0);
}
void DX7Rasterizer::SetDithering(unsigned long inState, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::Dithering, inState, inScreen); }
void DX7Rasterizer::SetDrawStyle(unsigned long inStyle, unsigned long inScreen) { ee_dx7r_set(this->mDX7, EeDx7RastSet::DrawStyle, inStyle, inScreen); }
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

// FUNCTION: DX7HR 0x1000bb66
// Makes a surface the render target (swapping the references) and fits the viewport to its size
void DX7Rasterizer::SetRenderTarget(GESurface* inTarget, unsigned long inScreen) { ee_dx7r_set_render_target(this->mDX7, inTarget, inScreen); }

// FUNCTION: DX7HR 0x1000bec0
// Binds a texture to stage 0 with its colour and alpha operations, addressing, filters and mip bias, sending
// only what changed; null disables texturing; stage 1 is always disabled
void DX7Rasterizer::SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_dx7r_set_texture(this->mDX7, inTexture, inScreen); }

// FUNCTION: DX7HR 0x1000c634
// Sets the viewport rectangle and depth range on the device and marks the 2D/3D transforms for refresh
void DX7Rasterizer::SetViewport(const GEViewport& inViewport, unsigned long inScreen) { ee_dx7r_set_viewport(this->mDX7, &inViewport, inScreen); }

// FUNCTION: DX7HR 0x1000c99e
// Shows the frame (flip full screen, blit to the window's client area windowed); every 12 frames updates the
// frame, polygon and texture-change statistics
long DX7Rasterizer::ShowScene(unsigned long inScreen) { return ee_dx7r_show_scene(this->mDX7, inScreen); }

// FUNCTION: DX7HR 0x1000cbe5
// Sends a light (point or directional, colour, attenuation, range) to its slot; 9 when the slot is not in use
long DX7Rasterizer::UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen)
{
    return ee_dx7r_update_light(this->mDX7, inIndex, &inLight, inScreen);
}

// ---- the draws (the four Draw units of the original, deleted in R4.3 part 14): the 2D draws are Rust (ee_dx7 draw2d.rs),
// the ee_dx7cpp_* below read the GE objects for it
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

extern "C" {
Dx7StdColorVertex* ee_dx7cpp_color_vertices(void* inArray)
{
    return reinterpret_cast<Dx7StdColorVertex*>(static_cast<DX7StdColorVertexArray*>(static_cast<GEVertexArray*>(inArray))->mVertices);
}
void ee_dx7cpp_texture_frame(void* inTexture, float* outSize, float* outCorner)
{
    GETexture* theTexture = static_cast<GETexture*>(inTexture);
    *outSize = theTexture->GetFrameUVSize();
    outCorner[0] = theTexture->GetFrameTopLeftCorner().GetU();
    outCorner[1] = theTexture->GetFrameTopLeftCorner().GetV();
}
int32_t ee_dx7cpp_text_measure(const void* inFont, const char* inText, uint32_t inLength) { return PlatFontMeasure(static_cast<const PlatFont*>(inFont), inText, inLength); }
const void* ee_dx7cpp_text_scaled(const void* inFont, float inScale) { return PlatFontScaled(static_cast<const PlatFont*>(inFont), inScale); }
int32_t ee_dx7cpp_font_height(const void* inFont)
{
    PlatFontMetrics theMetrics;
    PlatFontGetMetrics(static_cast<const PlatFont*>(inFont), &theMetrics);
    return theMetrics.mHeight;
}
void ee_dx7cpp_text_draw(const void* inFont, const char* inText, uint32_t inLength, int32_t inX, int32_t inY, uint32_t inColor, int inOpaque,
                         uint32_t inBackground, const int32_t* inClip, const void* inTarget)
{
    PlatFontRect theClip = {inClip[0], inClip[1], inClip[2], inClip[3]};
    PlatFontDraw(static_cast<const PlatFont*>(inFont), inText, inLength, inX, inY, inColor, inOpaque, inBackground, &theClip,
                 static_cast<const PlatFontTarget*>(inTarget));
}
}

// One text draw's plain inputs (Rust: draw2d.rs TextInfo). The layout and the draws are in Rust.
static EeDx7TextInfo DX7TextInfoOf(GEText* inText)
{
    GEFont* theFont = inText->GetFont();
    EeDx7TextInfo theInfo;
    theInfo.font = theFont->GetOSFont();
    theInfo.text = inText->GetText();
    // the text's bytes, as PlatFontMeasure/PlatFontDraw take it (GetLength counts characters: under UTF-8, decision
    // D6, a multi-byte character made it short, and the text lost its last characters)
    theInfo.length = inText->GetText().GetTCHARLength();
    theInfo.x = inText->GetLocation().GetX();
    theInfo.y = inText->GetLocation().GetY();
    theInfo.alignment = inText->GetAlignment();
    theInfo.transparent = inText->GetIsTransparent();
    theInfo.shadow_font = (theFont->GetFontStyle() & kFontStyleShadow) != 0;
    theInfo.foreground[0] = inText->GetFgColor().GetRed();
    theInfo.foreground[1] = inText->GetFgColor().GetGreen();
    theInfo.foreground[2] = inText->GetFgColor().GetBlue();
    theInfo.background[0] = inText->GetBgColor().GetRed();
    theInfo.background[1] = inText->GetBgColor().GetGreen();
    theInfo.background[2] = inText->GetBgColor().GetBlue();
    return theInfo;
}

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
    EeDx7TextInfo theInfo = DX7TextInfoOf(theJob->mText);
    if (!ee_dx7r_text_draw(&theInfo, reinterpret_cast<const EeDx7TextRect*>(theJob->mClip), inOriginX, inOriginY,
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
    EeDx7TextInfo theInfo = DX7TextInfoOf(inText);
    EeDx7TextRect theBounds;
    ee_dx7r_text_bounds(&theInfo, reinterpret_cast<const EeDx7TextRect*>(inClip), &theBounds);
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
// transform stack a call parameter); the ee_dx7cpp_* below read the GE objects for it.

extern "C" {
void ee_dx7cpp_model_info(void* inModel, EeDx7ModelInfo* outInfo)
{
    GEModel* theModel = static_cast<GEModel*>(inModel);
    outInfo->material_count = theModel->GetMaterialCount();
    outInfo->lod_count = theModel->GetLODCount();
    outInfo->sub_model_count = theModel->GetSubModelCount();
    outInfo->current_frame = theModel->GetGraphicsCurrentFrame();
    outInfo->next_frame = theModel->GetGraphicsNextFrame();
    outInfo->vertex_type = theModel->GetVertexArray(0)->GetType();
    outInfo->scale = theModel->GetGraphicsScale();
    outInfo->opacity = theModel->GetModelOpacity();
    outInfo->fraction = theModel->GetFractionalFramePosition();
    const GETransformation& theTransform = theModel->GetGraphicsTransform();
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            outInfo->transform[r][c] = theTransform.GetElement(r, c);
}

void ee_dx7cpp_material_info(void* inModel, unsigned long inScreen, unsigned long inIndex, unsigned long inLOD, EeDx7MaterialInfo* outInfo)
{
    GEMaterial* theMaterial = static_cast<GEModel*>(inModel)->GetMaterial(inIndex);
    outInfo->texture = theMaterial->GetTexture(inScreen);
    outInfo->polygon_start = theMaterial->GetPolygonStartingIndex(inLOD);
    outInfo->polygon_length = theMaterial->GetPolygonRangeLength(inLOD);
    outInfo->vertex_start = theMaterial->GetVertexStartingIndex();
    outInfo->vertex_length = theMaterial->GetVertexRangeLength();
    outInfo->opacity = theMaterial->GetOpacity();
    outInfo->color[0] = theMaterial->GetColor().GetRed();
    outInfo->color[1] = theMaterial->GetColor().GetGreen();
    outInfo->color[2] = theMaterial->GetColor().GetBlue();
}

const unsigned short* ee_dx7cpp_polygon_indices(void* inModel, unsigned long inLOD)
{
    return static_cast<DX7PolygonArray*>(static_cast<GEModel*>(inModel)->GetPolygonArray(inLOD))->mIndices;
}

const void* ee_dx7cpp_frame_vertices(void* inModel, unsigned long inFrame, unsigned long inVertexType)
{
    GEVertexArray* theArray = static_cast<GEModel*>(inModel)->GetVertexArray(inFrame);
    if (inVertexType == 0)
        return static_cast<DX7StandardVertexArray*>(theArray)->mVertices;
    return static_cast<DX7StdColorVertexArray*>(theArray)->mVertices;
}

void* ee_dx7cpp_sub_model(void* inModel, unsigned long inIndex) { return static_cast<GEModel*>(inModel)->GetSubModel(inIndex); }
}

// Draws an indexed triangle list with an identity world transform and D3D clipping on; counts the triangles drawn
long DX7Rasterizer::Draw(GEPolygonArray& inPolygons, unsigned long inCount, GEVertexArray& inVertices,
                         unsigned long inVertexCount, GETexture* inTexture, unsigned long inScreen, bool inSorted)
{
    unsigned long theType = inVertices.GetType();
    const void* theVertices = theType == 1 ? static_cast<const void*>(((DX7StdColorVertexArray&)inVertices).mVertices)
                                           : static_cast<const void*>(((DX7StandardVertexArray&)inVertices).mVertices);
    return ee_dx7r_draw_polygon_array(this->mDX7, inScreen, ((DX7PolygonArray&)inPolygons).mIndices,
                                      inCount, theType, theVertices, inVertexCount, inTexture);
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

// Draws a model and recursively its sub-models (no parent: the top of a hierarchy); stops at the first sub-model error
long DX7Rasterizer::DrawModelHierarchy(GEModel* inModel, unsigned long inScreen, float inDetail, bool inOrthographic)
{
    return ee_dx7r_draw_model_hierarchy(this->mDX7, inScreen, inModel, inDetail, inOrthographic);
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

// ---- the renderer the game links (DX7HRTnLDisplay.cpp's globals; its PIM entry points went with the plug-in boundary)
static UString gName("Direct3D Hardware TnL");
static DX7Rasterizer gTnLRasterizer(gName, true);   // the T&L flavour (rastobj.rs)

// The game's rasterizer (GEInitialize adds it when it finds a screen)
GERasterizer* DX7TnLRasterizer()
{
    return &gTnLRasterizer;
}

// COMDATs this unit is the first (link order) to instantiate, then the template-static init chain head and atexit
// registrars (GELOD x3, then U2DSparseArray: the unit includes U2DSparseArray.h after GEModel.h):
// TEMPLATE: DX7HR 0x1000ccf2
// TEMPLATE: DX7HR 0x1000cd1e
// TEMPLATE: DX7HR 0x1000cd59
// TEMPLATE: DX7HR 0x1000cd9d
// TEMPLATE: DX7HR 0x1000d033
// TEMPLATE: DX7HR 0x1000d047
// TEMPLATE: DX7HR 0x1000d067
// TEMPLATE: DX7HR 0x1000d07f
// SYNTHETIC: DX7HR 0x1000d0a4
// SYNTHETIC: DX7HR 0x1000d0cc
// SYNTHETIC: DX7HR 0x1000d0d8
// SYNTHETIC: DX7HR 0x1000d0e4
// SYNTHETIC: DX7HR 0x1000d0f0

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// SYNTHETIC: DX7HRTNL 0x10009217
// SYNTHETIC: DX7HRTNL 0x10009221
// SYNTHETIC: DX7HRTNL 0x10009232
// SYNTHETIC: DX7HRTNL 0x1000923e
// SYNTHETIC: DX7HRTNL 0x10009249
// SYNTHETIC: DX7HRTNL 0x10009253
// SYNTHETIC: DX7HRTNL 0x10009264
// SYNTHETIC: DX7HRTNL 0x10009270
// SYNTHETIC: DX7HRTNL 0x1000927b
// SYNTHETIC: DX7HRTNL 0x10009285
// SYNTHETIC: DX7HRTNL 0x10009296
// SYNTHETIC: DX7HRTNL 0x100092a2
// SYNTHETIC: DX7HRTNL 0x100092ad
// SYNTHETIC: DX7HRTNL 0x100092b7
// SYNTHETIC: DX7HRTNL 0x100092c8
// SYNTHETIC: DX7HRTNL 0x100092d4
// SYNTHETIC: DX7HRTNL 0x100092df
// SYNTHETIC: DX7HRTNL 0x100092e9
// SYNTHETIC: DX7HRTNL 0x100092fa
// SYNTHETIC: DX7HRTNL 0x10009306
// SYNTHETIC: DX7HRTNL 0x10009311
// SYNTHETIC: DX7HRTNL 0x1000931b
// SYNTHETIC: DX7HRTNL 0x1000932c
// SYNTHETIC: DX7HRTNL 0x10009338
// SYNTHETIC: DX7HRTNL 0x10009343
// SYNTHETIC: DX7HRTNL 0x1000934d
// SYNTHETIC: DX7HRTNL 0x1000935e
// SYNTHETIC: DX7HRTNL 0x1000936a
// FUNCTION: DX7HRTNL 0x10009375
// FUNCTION: DX7HRTNL 0x100093b8
// FUNCTION: DX7HRTNL 0x100095af
// FUNCTION: DX7HRTNL 0x100097f8
// FUNCTION: DX7HRTNL 0x1000982a
// FUNCTION: DX7HRTNL 0x10009845
// FUNCTION: DX7HRTNL 0x100098e3
// FUNCTION: DX7HRTNL 0x10009b21
// FUNCTION: DX7HRTNL 0x1000a00f
// FUNCTION: DX7HRTNL 0x1000a5ee
// FUNCTION: DX7HRTNL 0x1000a66c
// FUNCTION: DX7HRTNL 0x1000a6bb
// FUNCTION: DX7HRTNL 0x1000a71c
// FUNCTION: DX7HRTNL 0x1000a764
// FUNCTION: DX7HRTNL 0x1000a8ce
// FUNCTION: DX7HRTNL 0x1000a9e0
// FUNCTION: DX7HRTNL 0x1000a9e9
// FUNCTION: DX7HRTNL 0x1000aa52
// FUNCTION: DX7HRTNL 0x1000aa74
// FUNCTION: DX7HRTNL 0x1000aaab
// FUNCTION: DX7HRTNL 0x1000aac9
// FUNCTION: DX7HRTNL 0x1000ab0d
// FUNCTION: DX7HRTNL 0x1000ab8c
// FUNCTION: DX7HRTNL 0x1000ac2c
// FUNCTION: DX7HRTNL 0x1000ac2f
// FUNCTION: DX7HRTNL 0x1000ac85
// FUNCTION: DX7HRTNL 0x1000acb5
// FUNCTION: DX7HRTNL 0x1000acc5
// FUNCTION: DX7HRTNL 0x1000acca
// FUNCTION: DX7HRTNL 0x1000acda
// FUNCTION: DX7HRTNL 0x1000ad84
// FUNCTION: DX7HRTNL 0x1000adb4
// FUNCTION: DX7HRTNL 0x1000ae23
// FUNCTION: DX7HRTNL 0x1000aea7
// FUNCTION: DX7HRTNL 0x1000aec9
// FUNCTION: DX7HRTNL 0x1000af11
// FUNCTION: DX7HRTNL 0x1000b0df
// FUNCTION: DX7HRTNL 0x1000b298
// FUNCTION: DX7HRTNL 0x1000b2d9
// FUNCTION: DX7HRTNL 0x1000b367
// FUNCTION: DX7HRTNL 0x1000b3c8
// FUNCTION: DX7HRTNL 0x1000b470
// FUNCTION: DX7HRTNL 0x1000b50a
// FUNCTION: DX7HRTNL 0x1000b557
// FUNCTION: DX7HRTNL 0x1000b80a
// FUNCTION: DX7HRTNL 0x1000b854
// FUNCTION: DX7HRTNL 0x1000b8b0
// FUNCTION: DX7HRTNL 0x1000b917
// FUNCTION: DX7HRTNL 0x1000b928
// FUNCTION: DX7HRTNL 0x1000b939
// FUNCTION: DX7HRTNL 0x1000b983
// FUNCTION: DX7HRTNL 0x1000b9dd
// FUNCTION: DX7HRTNL 0x1000ba27
// FUNCTION: DX7HRTNL 0x1000bae5
// FUNCTION: DX7HRTNL 0x1000bb63
// FUNCTION: DX7HRTNL 0x1000bb74
// FUNCTION: DX7HRTNL 0x1000bb89
// FUNCTION: DX7HRTNL 0x1000bbd3
// FUNCTION: DX7HRTNL 0x1000bd20
// FUNCTION: DX7HRTNL 0x1000bd6a
// FUNCTION: DX7HRTNL 0x1000bee3
// FUNCTION: DX7HRTNL 0x1000bf2d
// FUNCTION: DX7HRTNL 0x1000c6a1
// FUNCTION: DX7HRTNL 0x1000c8a6
// FUNCTION: DX7HRTNL 0x1000c8b7
// FUNCTION: DX7HRTNL 0x1000c97d
// FUNCTION: DX7HRTNL 0x1000ca0b
// FUNCTION: DX7HRTNL 0x1000cc52
// FUNCTION: DX7HRTNL 0x1000cd18
// TEMPLATE: DX7HRTNL 0x1000cd5f
// TEMPLATE: DX7HRTNL 0x1000cd8b
// TEMPLATE: DX7HRTNL 0x1000cdc6
// TEMPLATE: DX7HRTNL 0x1000ce0a
// TEMPLATE: DX7HRTNL 0x1000d0a0
// TEMPLATE: DX7HRTNL 0x1000d0b4
// TEMPLATE: DX7HRTNL 0x1000d0d4
// SYNTHETIC: DX7HRTNL 0x1000d0ec
// SYNTHETIC: DX7HRTNL 0x1000d114
// SYNTHETIC: DX7HRTNL 0x1000d120
// SYNTHETIC: DX7HRTNL 0x1000d12c
// SYNTHETIC: DX7HRTNL 0x1000d138
// FUNCTION: DX7HRTNL 0x1000ac27
