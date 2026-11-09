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
#include "rust/ee_rust.h"

// The algorithms are in Rust (rust/crates/ee_dx7/src/raster.rs); this class keeps the members, the virtuals and the
// engine objects, each method forwards or is an ee_dx7cpp_rast_* trampoline.

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

// The D3D light indices in use, shared by every screen (provisional name).
unsigned long gLightsInUse[8];

// Trampoline access to the protected members (the extern "C" hosts below).
struct DX7RasterAccess : DX7Rasterizer {
    using DX7Rasterizer::SetRenderTarget;
    using DX7Rasterizer::Set3DMode;
    void MakeActive() { GERasterizer::sActiveRasterizer = this; }
    GERScreenState* State(unsigned long inScreen) { return this->mScreenStates[inScreen]; }
    DX7RScreenState* DX7State(unsigned long inScreen) { return this->mDX7ScreenStates[inScreen]; }
    void SetCullingUnknown() { this->mCulling = -16; }
    long CallActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary, GETextureManager* inManager, GEWindow* inWindow)
        { return this->ActivateTheScreen(inScreen, inPrimary, inSecondary, inManager, inWindow); }
};

// What the Rust activation asks the C++ side for (the ee_dx7cpp_rast_* hosts below).
struct DX7ActivateContext {
    DX7RasterAccess* mRasterizer;
    unsigned long mScreen;
    GETextureManager* mManager;
    GEWindow* mWindow;
};

// The light fields Rust's UpdateLight reads
static EeDx7LightParams DX7LightParams(GELight& inLight)
{
    EeDx7LightParams theParams;
    theParams.location[0] = inLight.GetLocation().GetX();
    theParams.location[1] = inLight.GetLocation().GetY();
    theParams.location[2] = inLight.GetLocation().GetZ();
    theParams.direction[0] = inLight.GetDirection().GetX();
    theParams.direction[1] = inLight.GetDirection().GetY();
    theParams.direction[2] = inLight.GetDirection().GetZ();
    theParams.computed_color[0] = inLight.GetComputedColor().GetRed();
    theParams.computed_color[1] = inLight.GetComputedColor().GetGreen();
    theParams.computed_color[2] = inLight.GetComputedColor().GetBlue();
    theParams.style = inLight.GetStyle();
    theParams.attenuation[0] = inLight.GetAttenuation0();
    theParams.attenuation[1] = inLight.GetAttenuation1();
    theParams.attenuation[2] = inLight.GetAttenuation2();
    theParams.range = inLight.GetRange();
    return theParams;
}

// FUNCTION: DX7HR 0x1000978b
// Creates the DirectX 7 rasterizer with the given name and a dark blue debug colour
DX7Rasterizer::DX7Rasterizer(const UString& inName)
    : GERasterizer(inName)
{
    this->mDebugColor = 0xff000040;
}

// FUNCTION: DX7HR 0x100097bd
// Destroys the rasterizer (base class only)
DX7Rasterizer::~DX7Rasterizer()
{
}

// FUNCTION: DX7HR 0x100097d8
// Enumerates the DirectDraw devices into screens (Ex enumeration on 98/2000) and makes this the active rasterizer
long DX7Rasterizer::ActivateRasterizer()
{
    return ee_dx7r_activate_rasterizer(this);
}

// FUNCTION: DX7HR 0x10009876
// Sets up a screen to render into an offscreen surface of the given size at the desktop depth, retrying with a
// 16-bit mode if that fails
long DX7Rasterizer::ActivateRenderingSurface(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                             unsigned long inWidth, unsigned long inHeight)
{
    DX7ActivateContext theContext = { static_cast<DX7RasterAccess*>(this), inScreen, inManager, inWindow };
    return ee_dx7r_activate_rendering_surface(&theContext, inWidth, inHeight);
}

// FUNCTION: DX7HR 0x10009ab4
// Sets up a screen's primary, back buffer and optional Z-buffer (flipping chain full screen, offscreen buffer
// windowed), activates Direct3D on them and forces every render state to its default
long DX7Rasterizer::ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                   unsigned long inBackBuffers, bool inZBuffer, unsigned long inZBufferFormat)
{
    DX7ActivateContext theContext = { static_cast<DX7RasterAccess*>(this), inScreen, inManager, inWindow };
    return ee_dx7r_activate_screen(&theContext, inWindow->mWidth, inWindow->mHeight, inBackBuffers, inZBuffer, inZBufferFormat);
}

// FUNCTION: DX7HR 0x10009fa2
// Creates the HAL Direct3D device on the back buffer, sets the viewport and fixed render states, the vertex buffer
// and the card quirks (texture alpha modulation off on small Rage Pro cards, no dirty rectangles on two cards)
long DX7Rasterizer::ActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary,
                                      GETextureManager* inManager, GEWindow* inWindow)
{
    DX7Surface* theTarget = dynamic_cast<DX7Surface*>(inSecondary);
    return ee_dx7r_activate_the_screen(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]),
                                       static_cast<DX7Screen*>(this->mDX7ScreenStates[inScreen]->mScreen)->mDirectDraw,
                                       (GXSurface*)*theTarget, theTarget->GetWidth(), theTarget->GetHeight(), false);
}

// FUNCTION: DX7HR 0x1000a581
// Takes a free light slot (of 8), sets the light into it and enables it; returns 7 when all slots are in use
long DX7Rasterizer::AddLight(GELight& inLight, unsigned long inScreen, unsigned long& outIndex)
{
    EeDx7LightParams theParams = DX7LightParams(inLight);
    uint32_t theIndex = 0;
    long theResult = ee_dx7r_add_light(this->mDX7ScreenStates[inScreen]->GetDevice(), gLightsInUse, &theParams, &theIndex);
    outIndex = theIndex;
    return theResult;
}

// FUNCTION: DX7HR 0x1000a5ff
// Adds a screen with a fresh DirectX 7 screen state
long DX7Rasterizer::AddValidScreen(GEScreen* inScreen)
{
    this->mDX7ScreenStates.push_back(new DX7RScreenState(inScreen));
    return GERasterizer::AddValidScreen(inScreen);
}

// FUNCTION: DX7HR 0x1000a64e
// Sets the render target and begins the scene, waiting while the device is still drawing; 9 when the surface is lost
long DX7Rasterizer::BeginScene(GESurface* inTarget, unsigned long inScreen)
{
    this->SetRenderTarget(inTarget, inScreen);
    return ee_dx7r_begin_scene(this->mDX7ScreenStates[inScreen]->GetDevice());
}

// FUNCTION: DX7HR 0x1000a6af
// Sets the texture and asks the device whether it can draw the current state in one pass
bool DX7Rasterizer::CanDoCurrentState(GETexture* inTexture, unsigned long inScreen)
{
    bool theResult = false;
    this->SetTexture(inTexture, inScreen);
    unsigned long thePasses;
    HRESULT theError = this->mDX7ScreenStates[inScreen]->GetDevice()->ValidateDevice(&thePasses);
    if (theError == GX_OK)
        theResult = true;
    else
        ESRecordNativeError(theError);
    return theResult;
}

// FUNCTION: DX7HR 0x1000a6f7
// Fills a rectangle of the render target with a colour, waiting while the device is still drawing
long DX7Rasterizer::Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen)
{
    const float theRect[4] = { inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(), inRectangle.GetBottom() };
    const float theRGB[3] = { inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue() };
    return ee_dx7r_clear(this->mDX7ScreenStates[inScreen]->GetDevice(), theRect, theRGB);
}

// FUNCTION: DX7HR 0x1000a861
// Fills a rectangle of the Z-buffer with a depth value, waiting while the device is still drawing
long DX7Rasterizer::ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen)
{
    const float theRect[4] = { inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(), inRectangle.GetBottom() };
    return ee_dx7r_clear_z(this->mDX7ScreenStates[inScreen]->GetDevice(), theRect, inZ);
}

// FUNCTION: DX7HR 0x1000a973
// Returns 0: this rasterizer does not measure the polygon area of the vertex buffer
float DX7Rasterizer::ComputePolyAreaOfCurrentVB(unsigned long inScreen)
{
    return 0.0f;
}

// FUNCTION: DX7HR 0x1000a97c
// Converts an engine transformation to a Direct3D matrix (transposed)
void DX7Rasterizer::ConvertToD3DMatrix(const GETransformation& inTransformation, GXMATRIX& outMatrix)
{
    outMatrix._11 = inTransformation.GetElement(0, 0);
    outMatrix._12 = inTransformation.GetElement(1, 0);
    outMatrix._13 = inTransformation.GetElement(2, 0);
    outMatrix._14 = inTransformation.GetElement(3, 0);
    outMatrix._21 = inTransformation.GetElement(0, 1);
    outMatrix._22 = inTransformation.GetElement(1, 1);
    outMatrix._23 = inTransformation.GetElement(2, 1);
    outMatrix._24 = inTransformation.GetElement(3, 1);
    outMatrix._31 = inTransformation.GetElement(0, 2);
    outMatrix._32 = inTransformation.GetElement(1, 2);
    outMatrix._33 = inTransformation.GetElement(2, 2);
    outMatrix._34 = inTransformation.GetElement(3, 2);
    outMatrix._41 = inTransformation.GetElement(0, 3);
    outMatrix._42 = inTransformation.GetElement(1, 3);
    outMatrix._43 = inTransformation.GetElement(2, 3);
    outMatrix._44 = inTransformation.GetElement(3, 3);
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
    GEVertexArray* theArray = NULL;
    switch (inType) {
    case 0:
        theArray = new DX7StandardVertexArray(inCount);
        break;
    case 1:
        theArray = new DX7StdColorVertexArray(inCount);
        break;
    }
    return theArray;
}

// FUNCTION: DX7HR 0x1000aaa5
// Creates the screen's system-memory vertex buffer of 2,400 coloured vertices
long DX7Rasterizer::CreateVertexBuffers(unsigned long inScreen)
{
    return ee_dx7r_create_vertex_buffers(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]), false);
}

long DX7Rasterizer::DemoteTexture(GETexture* inTexture)
{
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000ab24
// Releases every screen's texture, vertex buffer, lights, device and Direct3D, deletes the screens and their states
long DX7Rasterizer::Deactivate()
{
    unsigned long theScreen = 0;
    for (std::vector<DX7RScreenState*>::iterator theState = this->mDX7ScreenStates.begin();
         theState != this->mDX7ScreenStates.end(); theState++, theScreen++) {
        this->SetTexture(NULL, theScreen);
        if ((*theState)->mVertexBuffer != NULL) {
            (*theState)->mVertexBuffer->Release();
            (*theState)->mVertexBuffer = NULL;
        }
        if ((*theState)->mDevice != NULL) {
            this->RemoveAllLights(theScreen);
            (*theState)->GetDevice()->Release();
            (*theState)->mDevice = NULL;
        }
        if ((*theState)->mDirect3D != NULL) {
            (*theState)->mDirect3D->Release();
            (*theState)->mDirect3D = NULL;
        }
        if ((*theState)->mScreen != NULL)
            delete (DX7Screen*)(*theState)->mScreen;
        delete *theState;
    }
    this->mDX7ScreenStates.clear();
    return GERasterizer::Deactivate();
}

// FUNCTION: DX7HR 0x1000abbf
// Does nothing (textures need no handle here)
void DX7Rasterizer::DestroyTextureHandle(GETexture* inTexture)
{
}

// FUNCTION: DX7HR 0x1000abc2
// Ends the scene, waiting while the device is still drawing; 9 when the surface is lost
long DX7Rasterizer::EndScene(unsigned long inScreen)
{
    return ee_dx7r_end_scene(this->mDX7ScreenStates[inScreen]->GetDevice());
}

// FUNCTION: DX7HR 0x1000ac18
// Returns the free video memory of the screen
long DX7Rasterizer::GetAvailableVideoMemory(unsigned long inScreen, unsigned long& outBytes)
{
    unsigned long theTotal = 0;
    unsigned long theFree = 0;
    long theResult = this->GetVideoMemory(inScreen, theTotal, theFree);
    if (theResult == kErrorOK)
        outBytes = theFree;
    return theResult;
}

// FUNCTION: DX7HR 0x1000ac48
// Returns whether the screen's card can modulate texture alpha
bool DX7Rasterizer::GetCanModulateTextureAlpha(unsigned long inScreen)
{
    return this->mDX7ScreenStates[inScreen]->mCanModulateTextureAlpha;
}

// FUNCTION: DX7HR 0x1000ac58
// Returns 0 for any capability
unsigned long DX7Rasterizer::GetCaps(unsigned long inCap, unsigned long inScreen)
{
    return 0;
}

// FUNCTION: DX7HR 0x1000ac5d
// Returns whether the screen's card supports dirty-rectangle updates
bool DX7Rasterizer::GetDirtyRectangleSupport(unsigned long inScreen)
{
    return this->mDX7ScreenStates[inScreen]->mDirtyRectangleSupport;
}

// FUNCTION: DX7HR 0x1000ac6d
// Returns whether the screen's device has a hardware-rasterizing Direct3D device
bool DX7Rasterizer::GetIsScreenValid(DX7Screen* inScreen)
{
    return ee_dx7r_is_screen_valid((GXDirectDraw*)*inScreen, false);
}

// FUNCTION: DX7HR 0x1000ad17
// Returns the total video memory of the screen
long DX7Rasterizer::GetTotalVideoMemory(unsigned long inScreen, unsigned long& outBytes)
{
    unsigned long theTotal = 0;
    unsigned long theFree = 0;
    long theResult = this->GetVideoMemory(inScreen, theTotal, theFree);
    if (theResult == kErrorOK)
        outBytes = theTotal;
    return theResult;
}

// FUNCTION: DX7HR 0x1000ad47
// Reads the screen's total and free video memory; 0xd when it cannot be read or is 0
long DX7Rasterizer::GetVideoMemory(unsigned long inScreen, unsigned long& outTotal, unsigned long& outFree)
{
    uint32_t theTotal = 0;
    uint32_t theFree = 0;
    long theResult = ee_dx7r_get_video_memory(static_cast<DX7Screen*>(this->mDX7ScreenStates[inScreen]->mScreen)->mDirectDraw,
                                              &theTotal, &theFree);
    if (theResult == kErrorOK) {
        outTotal = theTotal;
        outFree = theFree;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000adb6
// Locks room for the given number of vertices in the vertex buffer, discarding it when full; always returns success
long DX7Rasterizer::LockVB(unsigned long inCount, unsigned long inScreen, GXVERTEX_STD_COLOR*& outVertices)
{
    return ee_dx7r_lock_vb(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]), inCount,
                           reinterpret_cast<void**>(&outVertices));
}

long DX7Rasterizer::PromoteTexture(GETexture* inTexture)
{
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000ae3a
// Removes all 8 lights
long DX7Rasterizer::RemoveAllLights(unsigned long inScreen)
{
    return ee_dx7r_remove_all_lights(this->mDX7ScreenStates[inScreen]->GetDevice(), gLightsInUse);
}

// FUNCTION: DX7HR 0x1000ae5c
// Disables a light and frees its slot; 9 when the slot was not in use
long DX7Rasterizer::RemoveLight(unsigned long inIndex, unsigned long inScreen)
{
    return ee_dx7r_remove_light(this->mDX7ScreenStates[inScreen]->GetDevice(), gLightsInUse, inIndex);
}

// FUNCTION: DX7HR 0x1000aea4
// Switches the screen to 2D drawing: identity world and view, a projection mapping pixels to the viewport
long DX7Rasterizer::Set2DMode(unsigned long inScreen)
{
    return ee_dx7r_set_2d_mode(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]));
}

// FUNCTION: DX7HR 0x1000b072
// Switches the screen to 3D drawing: the camera view (Y and Z swapped) and the perspective or orthographic projection
long DX7Rasterizer::Set3DMode(unsigned long inScreen)
{
    GETransformation theSwap;
    GETransformation theView(false);
    theSwap.SetElement(1, 1, 0.0f);
    theSwap.SetElement(1, 2, 1.0f);
    theSwap.SetElement(2, 1, 1.0f);
    theSwap.SetElement(2, 2, 0.0f);
    theSwap.Apply(this->GetViewport(inScreen).GetCameraInverse(), theView);
    const GETransformation& theProjection = this->mScreenStates[inScreen]->mProjectionMode == 1
        ? this->GetViewport(inScreen).GetPerspectiveProjection() : this->GetViewport(inScreen).GetOrthographicProjection();
    float theViewElements[16];
    float theProjectionElements[16];
    for (unsigned long r = 0; r < 4; r++)
        for (unsigned long c = 0; c < 4; c++) {
            theViewElements[r * 4 + c] = theView.GetElement(r, c);
            theProjectionElements[r * 4 + c] = theProjection.GetElement(r, c);
        }
    return ee_dx7r_set_3d_mode(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]), theViewElements, theProjectionElements);
}

// FUNCTION: DX7HR 0x1000b22b
// Turns alpha testing on or off when it changes
void DX7Rasterizer::SetAlphaTesting(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_alpha_testing(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mAlphaTesting))
        GERasterizer::SetAlphaTesting(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b26c
// Sets the alpha test compare function (skipped when unchanged)
void DX7Rasterizer::SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_alpha_testing_compare_state(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mAlphaTestingCompareState))
        GERasterizer::SetAlphaTestingCompareState(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b2fa
// Sets the alpha test reference value (0..1 scaled to 0..255) when it changes
void DX7Rasterizer::SetAlphaTestingRefValue(float inValue, unsigned long inScreen)
{
    if (ee_dx7r_set_alpha_testing_ref_value(this->mDX7ScreenStates[inScreen]->mDevice, inValue, this->mScreenStates[inScreen]->mAlphaTestingRefValue))
        GERasterizer::SetAlphaTestingRefValue(inValue, inScreen);
}

// FUNCTION: DX7HR 0x1000b35b
// Sets the ambient light colour and sends it scaled by the ambient intensity
void DX7Rasterizer::SetAmbientColor(const GEColor& inColor, unsigned long inScreen)
{
    this->mAmbientColor = inColor;
    this->mComputedAmbientColor.SetRed(inColor.GetRed() * this->mAmbientIntensity);
    this->mComputedAmbientColor.SetGreen(inColor.GetGreen() * this->mAmbientIntensity);
    this->mComputedAmbientColor.SetBlue(inColor.GetBlue() * this->mAmbientIntensity);
    const float theRGB[3] = { this->mComputedAmbientColor.GetRed(), this->mComputedAmbientColor.GetGreen(),
                              this->mComputedAmbientColor.GetBlue() };
    ee_dx7r_set_ambient(this->mDX7ScreenStates[inScreen]->mDevice, theRGB);
}

// FUNCTION: DX7HR 0x1000b403
// Sets the ambient light intensity and sends the ambient colour scaled by it
void DX7Rasterizer::SetAmbientIntensity(float inIntensity, unsigned long inScreen)
{
    this->mAmbientIntensity = inIntensity;
    this->mComputedAmbientColor.SetRed(this->mAmbientColor.GetRed() * inIntensity);
    this->mComputedAmbientColor.SetGreen(this->mAmbientColor.GetGreen() * inIntensity);
    this->mComputedAmbientColor.SetBlue(this->mAmbientColor.GetBlue() * inIntensity);
    const float theRGB[3] = { this->mComputedAmbientColor.GetRed(), this->mComputedAmbientColor.GetGreen(),
                              this->mComputedAmbientColor.GetBlue() };
    ee_dx7r_set_ambient(this->mDX7ScreenStates[inScreen]->mDevice, theRGB);
}

// FUNCTION: DX7HR 0x1000b49d
// Turns sort-independent antialiasing on or off when it changes
void DX7Rasterizer::SetAntiAliasing(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_anti_aliasing(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mAntiAliasing))
        GERasterizer::SetAntiAliasing(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b4ea
// Sets the source and destination blend factors, turning alpha blending off for one/zero and sending only what changed
void DX7Rasterizer::SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen)
{
    if (ee_dx7r_set_blending(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]), inSource, inDestination, this->mScreenStates[inScreen]->mBlendSource,
                             this->mScreenStates[inScreen]->mBlendDestination))
        GERasterizer::SetBlending(inSource, inDestination, inScreen);
}

// FUNCTION: DX7HR 0x1000b79d
// Turns colour keying on or off when it changes
void DX7Rasterizer::SetColorKeying(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_color_keying(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mColorKeying))
        GERasterizer::SetColorKeying(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b7e7
// Sets the culling mode (clockwise, counter-clockwise or none) on every screen's device
void DX7Rasterizer::SetCulling(unsigned long inState)
{
    if (inState == this->GetCulling())
        return;
    for (unsigned long i = 0; i < this->mDX7ScreenStates.size(); i++) {
        GXDevice* theDevice = this->mDX7ScreenStates[i]->mDevice;
        if (theDevice != NULL)
            ee_dx7r_set_cull_mode(theDevice, inState);
    }
    GERasterizer::SetCulling(inState);
}

// FUNCTION: DX7HR 0x1000b843
// Sets the debug colour (opaque)
void DX7Rasterizer::SetDebugColor(const GEColor& inColor)
{
    const float theRGB[3] = { inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue() };
    this->mDebugColor = ee_dx7r_debug_color(theRGB);
}

// FUNCTION: DX7HR 0x1000b8aa
// Turns the no-texture debug display on or off (base class only)
void DX7Rasterizer::SetDisplayNoTextureMode(bool inState, unsigned long inScreen)
{
    GERasterizer::SetDisplayNoTextureMode(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b8bb
// Turns the overdraw debug display on or off (base class only)
void DX7Rasterizer::SetDisplayOverdrawMode(bool inState, unsigned long inScreen)
{
    GERasterizer::SetDisplayOverdrawMode(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b8cc
// Turns dithering on or off when it changes
void DX7Rasterizer::SetDithering(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_dithering(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mDithering))
        GERasterizer::SetDithering(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b916
// Sets solid, wireframe or point drawing when it changes
void DX7Rasterizer::SetDrawStyle(unsigned long inStyle, unsigned long inScreen)
{
    if (ee_dx7r_set_draw_style(this->mDX7ScreenStates[inScreen]->mDevice, inStyle, this->mScreenStates[inScreen]->mDrawStyle))
        GERasterizer::SetDrawStyle(inStyle, inScreen);
}

// FUNCTION: DX7HR 0x1000b970
// Turns fog on or off when it changes
void DX7Rasterizer::SetFog(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_fog(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mFogState))
        GERasterizer::SetFog(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b9ba
// Sets the fog colour when it changes
void DX7Rasterizer::SetFogColor(const GEColor& inColor, unsigned long inScreen)
{
    const float theRGB[3] = { inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue() };
    if (ee_dx7r_set_fog_color(this->mDX7ScreenStates[inScreen]->mDevice, theRGB, this->mScreenStates[inScreen]->mFog.mColor == inColor))
        GERasterizer::SetFogColor(inColor, inScreen);
}

// FUNCTION: DX7HR 0x1000ba78
// Sets the fog start and end distances, sending only the ones that changed
void DX7Rasterizer::SetFogRange(float inStart, float inEnd, unsigned long inScreen)
{
    ee_dx7r_set_fog_range(this->mDX7ScreenStates[inScreen]->mDevice, inStart, inEnd, this->mScreenStates[inScreen]->mFog.mStart, this->mScreenStates[inScreen]->mFog.mEnd);
    GERasterizer::SetFogRange(inStart, inEnd, inScreen);
}

// FUNCTION: DX7HR 0x1000baf6
// Records the fog type (base class only; no device state)
void DX7Rasterizer::SetFogType(unsigned long inType, unsigned long inScreen)
{
    GERasterizer::SetFogType(inType, inScreen);
}

// FUNCTION: DX7HR 0x1000bb07
// Records the fog value (base class only; no device state)
void DX7Rasterizer::SetFogValue(float inValue, unsigned long inScreen)
{
    GERasterizer::SetFogValue(inValue, inScreen);
}

// FUNCTION: DX7HR 0x1000bb1c
// Turns lighting on or off when it changes
void DX7Rasterizer::SetLighting(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_lighting(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mLighting))
        GERasterizer::SetLighting(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000bb66
// Makes a surface the render target (swapping the references) and fits the viewport to its size
void DX7Rasterizer::SetRenderTarget(GESurface* inTarget, unsigned long inScreen)
{
    if (inTarget == this->mScreenStates[inScreen]->mRenderTarget)
        return;
    if (this->mScreenStates[inScreen]->mRenderTarget != NULL)
        this->mScreenStates[inScreen]->mRenderTarget->Dereference();
    this->mScreenStates[inScreen]->mRenderTarget = inTarget;
    inTarget->Reference();
    ee_dx7r_set_render_target(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]), (GXSurface*)*static_cast<DX7Surface*>(inTarget),
                              inTarget->GetWidth(), inTarget->GetHeight());
}

// FUNCTION: DX7HR 0x1000bcb3
// Sets Gouraud or flat shading when it changes
void DX7Rasterizer::SetShading(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_shading(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mShading))
        GERasterizer::SetShading(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000bcfd
// Takes lighting colours from the material (source 0) or from the vertex colours (white material) when it changes
void DX7Rasterizer::SetShadingColorSource(unsigned long inSource, unsigned long inScreen)
{
    if (ee_dx7r_set_shading_color_source(this->mDX7ScreenStates[inScreen]->mDevice, inSource, this->mScreenStates[inScreen]->mShadingColorSource))
        GERasterizer::SetShadingColorSource(inSource, inScreen);
}

// FUNCTION: DX7HR 0x1000be76
// Turns specular highlights on or off when it changes
void DX7Rasterizer::SetSpecular(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_specular(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mSpecular))
        GERasterizer::SetSpecular(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000bec0
// Binds a texture to stage 0 with its colour and alpha operations, addressing, filters and mip bias, sending
// only what changed; null disables texturing; stage 1 is always disabled
void DX7Rasterizer::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    GERScreenState* theState = this->mScreenStates[inScreen];
    if (inTexture == theState->mTexture && (inTexture == NULL || !inTexture->HasChanged()))
        return;
    if ((inTexture != NULL) != theState->m33C)
        theState->m33C = inTexture != NULL;
    if (theState->mTexture != NULL)
        theState->mTexture->Dereference();
    theState->mTexture = inTexture;
    if (inTexture != NULL) {
        inTexture->Reference();
        inTexture->Promote();
    }
    this->mScreenStates[inScreen]->m32C[3]++;
    EeDx7ScreenState* theDX7State = reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]);
    if (inTexture == NULL) {
        ee_dx7r_clear_texture(theDX7State);
        return;
    }
    const GETStage* theStage = inTexture->GetStage();
    EeDx7StageView theView;
    theView.surface = (GXSurface*)*static_cast<DX7Surface*>(inTexture->GetStageTexture(0)->GetFrameSurface());
    theView.mColorMode = theStage->mColorMode;
    theView.mAlphaMode = theStage->mAlphaMode;
    theView.mTextureAddressingMode = theStage->mTextureAddressingMode;
    theView.mMinFilterMode = theStage->mMinFilterMode;
    theView.mMagFilterMode = theStage->mMagFilterMode;
    theView.mMipFilterMode = theStage->mMipFilterMode;
    theView.mMipMapBias = theStage->mMipMapBias;
    ee_dx7r_apply_texture_stage(theDX7State, &theView);
    inTexture->ValidateChanges();
}

// FUNCTION: DX7HR 0x1000c634
// Sets the viewport rectangle and depth range on the device and marks the 2D/3D transforms for refresh
void DX7Rasterizer::SetViewport(const GEViewport& inViewport, unsigned long inScreen)
{
    GERasterizer::SetViewport(inViewport, inScreen);
    GERectangle theRectangle;
    theRectangle = inViewport.GetViewRectangle();
    const float theRect[4] = { theRectangle.GetLeft(), theRectangle.GetTop(), theRectangle.GetRight(), theRectangle.GetBottom() };
    float theNear, theFar;
    inViewport.GetDepthRange(theNear, theFar);
    ee_dx7r_set_viewport(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]), theRect, theNear, theFar);
}

// FUNCTION: DX7HR 0x1000c839
// Records whether to wait for the vertical sync (base class only)
void DX7Rasterizer::SetWaitForVSync(bool inState, unsigned long inScreen)
{
    GERasterizer::SetWaitForVSync(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000c84a
// Sets Z testing and Z writing from the Z-buffer state when it changes, sending only what changed
void DX7Rasterizer::SetZBuffer(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_z_buffer(reinterpret_cast<EeDx7ScreenState*>(this->mDX7ScreenStates[inScreen]), inState, this->mScreenStates[inScreen]->mZBuffer))
        GERasterizer::SetZBuffer(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000c910
// Sets the Z compare function when it changes
void DX7Rasterizer::SetZCompare(unsigned long inState, unsigned long inScreen)
{
    if (ee_dx7r_set_z_compare(this->mDX7ScreenStates[inScreen]->mDevice, inState, this->mScreenStates[inScreen]->mZCompare))
        GERasterizer::SetZCompare(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000c99e
// Shows the frame (flip full screen, blit to the window's client area windowed); every 12 frames updates the
// frame, polygon and texture-change statistics
long DX7Rasterizer::ShowScene(unsigned long inScreen)
{
    GERScreenState* theState = this->mScreenStates[inScreen];
    EeDx7FrameStats theStats;
    theStats.frame_counter = theState->m32C[0];
    theStats.scene_polys = theState->m32C[1];
    theStats.last_time = theState->m32C[2];
    theStats.texture_changes = theState->m32C[3];
    theStats.drawn_poly_count = theState->mDrawnPolyCount;
    theStats.drawn_poly_area = theState->mDrawnPolyArea;
    theStats.area_per_frame = theState->mDrawnPolyAreaPerFrame;
    theStats.area_per_second = theState->mDrawnPolyAreaPerSecond;
    theStats.polys_per_frame = theState->mDrawnPolysPerFrame;
    theStats.polys_per_second = theState->mDrawnPolysPerSecond;
    theStats.frames_per_second = theState->mFramesPerSecond;
    theStats.scene_polys_per_frame = theState->mScenePolysPerFrame;
    theStats.scene_polys_per_second = theState->mScenePolysPerSecond;
    theStats.texture_changes_per_frame = theState->mTotalTextureChangesPerFrame;
    ee_dx7r_update_frame_stats(&theStats, theStats.frame_counter + 1 == 12 ? TMGetTime() : 0);   // the clock is read every 12 frames only
    theState->m32C[0] = theStats.frame_counter;
    theState->m32C[1] = theStats.scene_polys;
    theState->m32C[2] = theStats.last_time;
    theState->m32C[3] = theStats.texture_changes;
    theState->mDrawnPolyCount = theStats.drawn_poly_count;
    theState->mDrawnPolyArea = theStats.drawn_poly_area;
    theState->mDrawnPolyAreaPerFrame = theStats.area_per_frame;
    theState->mDrawnPolyAreaPerSecond = theStats.area_per_second;
    theState->mDrawnPolysPerFrame = theStats.polys_per_frame;
    theState->mDrawnPolysPerSecond = theStats.polys_per_second;
    theState->mFramesPerSecond = theStats.frames_per_second;
    theState->mScenePolysPerFrame = theStats.scene_polys_per_frame;
    theState->mScenePolysPerSecond = theStats.scene_polys_per_second;
    theState->mTotalTextureChangesPerFrame = theStats.texture_changes_per_frame;

    GXSurface* thePrimary = *static_cast<DX7Surface*>(theState->mPrimarySurface);
    GXSurface* theSecondary = NULL;
    unsigned long theWidth = 0, theHeight = 0;
    float theX = 0.0f, theY = 0.0f;
    bool theFullScreen = theState->mScreen->GetIsFullScreen();
    if (!theFullScreen) {
        theSecondary = *static_cast<DX7Surface*>(theState->mSecondarySurface);
        theWidth = theState->mSecondarySurface->GetWidth();
        theHeight = theState->mSecondarySurface->GetHeight();
        GE2DPoint theOrigin = theState->mWindow->GetClientOrigin();
        theX = theOrigin.GetX();
        theY = theOrigin.GetY();
    }
    return ee_dx7r_show_scene(theFullScreen, thePrimary, theSecondary, theWidth, theHeight, theX, theY, theState->mPrimarySurface,
                              theState->mWaitForVSync);
}

// FUNCTION: DX7HR 0x1000cbe5
// Sends a light (point or directional, colour, attenuation, range) to its slot; 9 when the slot is not in use
long DX7Rasterizer::UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen)
{
    EeDx7LightParams theParams = DX7LightParams(inLight);
    return ee_dx7r_update_light(this->mDX7ScreenStates[inScreen]->GetDevice(), gLightsInUse, inIndex, &theParams);
}

// FUNCTION: DX7HR 0x1000ccab
// Unlocks the vertex buffer, waiting while it is busy
long DX7Rasterizer::UnlockVB(unsigned long inScreen)
{
    return ee_dx7r_unlock_vb(this->mDX7ScreenStates[inScreen]->mVertexBuffer);
}

// ---- the trampolines the Rust rasterizer calls
extern "C" {
bool ee_dx7cpp_rast_is_full_screen(DX7ActivateContext* c) { return c->mRasterizer->State(c->mScreen)->mScreen->GetIsFullScreen(); }
void ee_dx7cpp_rast_current_mode(DX7ActivateContext* c, GEVideoMode* outMode) { *outMode = c->mRasterizer->State(c->mScreen)->mScreen->GetCurrentMode(); }
unsigned long ee_dx7cpp_rast_mode_count(DX7ActivateContext* c) { return c->mRasterizer->State(c->mScreen)->mScreen->GetModeCount(); }
void ee_dx7cpp_rast_mode(DX7ActivateContext* c, unsigned long inIndex, GEVideoMode* outMode) { *outMode = c->mRasterizer->State(c->mScreen)->mScreen->GetMode(inIndex); }
int32_t ee_dx7cpp_rast_create_surface(DX7ActivateContext* c, GESurfaceDescription* inDescription, void** outSurface)
{
    return c->mRasterizer->State(c->mScreen)->mScreen->CreateSurface(*inDescription, reinterpret_cast<GESurface**>(outSurface));
}
void* ee_dx7cpp_rast_attached_surface(void* inSurface) { return static_cast<GESurface*>(inSurface)->GetAttachedSurface(0); }
int32_t ee_dx7cpp_rast_attach_surface(void* inSurface, void* inAttached) { return static_cast<GESurface*>(inSurface)->AttachSurface(static_cast<GESurface*>(inAttached)); }
void ee_dx7cpp_rast_dereference(void* inSurface) { static_cast<GESurface*>(inSurface)->Dereference(); }
int32_t ee_dx7cpp_rast_activate_the_screen(DX7ActivateContext* c, void* inPrimary, void* inSecondary)
{
    return c->mRasterizer->CallActivateTheScreen(c->mScreen, static_cast<GESurface*>(inPrimary), static_cast<GESurface*>(inSecondary), c->mManager, c->mWindow);
}
void ee_dx7cpp_rast_commit_activation(DX7ActivateContext* c, void* inPrimary, void* inSecondary)
{
    GERScreenState* theState = c->mRasterizer->State(c->mScreen);
    GEScreen* theScreen = theState->mScreen;
    theScreen->SetTextureManager(c->mManager);
    c->mManager->SetScreen(theScreen);
    theState->mPrimarySurface = static_cast<GESurface*>(inPrimary);
    theState->mTexture = NULL;
    theState->mRenderTarget = NULL;
    theState->mSecondarySurface = static_cast<GESurface*>(inSecondary);
    theState->mWindow = c->mWindow;
    theState->mIsScreenActivated = true;
    c->mRasterizer->SetRenderTarget(static_cast<GESurface*>(inSecondary), c->mScreen);
    static_cast<GESurface*>(inSecondary)->Reference();
}
void ee_dx7cpp_rast_apply_defaults(DX7ActivateContext* c)
{
    DX7RasterAccess* theRasterizer = c->mRasterizer;
    unsigned long inScreen = c->mScreen;
    GERScreenState* theState = theRasterizer->State(inScreen);
    theState->mAlphaTesting = -16;
    theRasterizer->SetAlphaTesting(0, inScreen);
    theState->mAlphaTestingCompareState = -16;
    theRasterizer->SetAlphaTestingCompareState(0, inScreen);
    theState->mAlphaTestingRefValue = 4294967296.0f;
    theRasterizer->SetAlphaTestingRefValue(0.0f, inScreen);
    theState->mAntiAliasing = -16;
    theRasterizer->SetAntiAliasing(0, inScreen);
    theState->mBlendSource = -16;
    theState->mBlendDestination = -16;
    theRasterizer->SetBlending(1, 0, inScreen);
    theState->mColorKeying = -16;
    theRasterizer->SetColorKeying(0, inScreen);
    theRasterizer->SetCullingUnknown();
    theRasterizer->SetCulling(0);
    theState->mDithering = -16;
    theRasterizer->SetDithering(0, inScreen);
    theState->mDrawStyle = -16;
    theRasterizer->SetDrawStyle(0, inScreen);
    theState->mFogState = -16;
    theRasterizer->SetFog(0, inScreen);
    theState->mLighting = -16;
    theRasterizer->SetLighting(0, inScreen);
    theState->mProjectionMode = -16;
    theRasterizer->SetProjectionMode(1, inScreen);
    theState->mShading = -16;
    theRasterizer->SetShading(1, inScreen);
    theState->mShadingColorSource = -16;
    theRasterizer->SetShadingColorSource(1, inScreen);
    theState->mSpecular = -16;
    theRasterizer->SetSpecular(0, inScreen);
    theState->mZBuffer = -16;
    theRasterizer->SetZBuffer(0, inScreen);
    theState->mZCompare = -16;
    theRasterizer->SetZCompare(0, inScreen);
    theRasterizer->DX7State(inScreen)->mIs3DMode = false;
    theRasterizer->Set3DMode(inScreen);
}
void ee_dx7cpp_rast_verify(bool inOK, uint32_t inLine) { ESVERIFY_AT(inOK, inLine); }
void* ee_dx7cpp_rast_new_screen(DX7Rasterizer*, void* inDirectDraw, const char* inName, bool inAttached, bool inPrimary)
{
    return static_cast<GEScreen*>(new DX7Screen(NULL, static_cast<GXDirectDraw*>(inDirectDraw), inName, inAttached, inPrimary));
}
bool ee_dx7cpp_rast_screen_is_valid(DX7Rasterizer* r, void* inScreen) { return r->GetIsScreenValid(static_cast<DX7Screen*>(static_cast<GEScreen*>(inScreen))); }
void ee_dx7cpp_rast_add_valid_screen(DX7Rasterizer* r, void* inScreen) { r->AddValidScreen(static_cast<GEScreen*>(inScreen)); }
void ee_dx7cpp_rast_delete_screen(void* inScreen) { delete static_cast<DX7Screen*>(static_cast<GEScreen*>(inScreen)); }
void ee_dx7cpp_rast_set_active(DX7Rasterizer* r) { static_cast<DX7RasterAccess*>(r)->MakeActive(); }
int32_t ee_dx7cpp_rast_flip(void* inPrimary, bool inWait) { return static_cast<GESurface*>(inPrimary)->Flip(inWait); }
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
