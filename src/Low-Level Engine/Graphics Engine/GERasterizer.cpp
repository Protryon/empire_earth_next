// C:\EEX\Low-Level Engine\Graphics Engine\GERasterizer.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// GERasterizer's base state is whole in Rust (rust/crates/ee_render/src/ge rastobj.rs, R4.3 part 12): the members, the screen
// records, the statics and every method below. The class keeps its vtable (the plug-in's base) and one member,
// mRust; the methods forward, and GERasterizerHost makes the calls on other objects Rust asks for.
#include <windows.h>
#include <stddef.h>

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
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GEPoint.h"
#include "lle/GEModel.h"
#include "lle/GEPhysicalModel.h"
#include "lle/Graphics Engine.h"
#include "platform/PlatFont.h"
#include "rust/render/ge.h"
#include "GERustMath.h"
#include "GERustViewport.h"

// The screen record is the Rust record (the plug-in reads its fields through GetScreenState).
static_assert(sizeof(GERScreenState) == sizeof(EeGeRastScreen), "GERScreenState is EeGeRastScreen");
static_assert(offsetof(GERScreenState, mFog) == offsetof(EeGeRastScreen, fog), "mFog");
static_assert(offsetof(GERScreenState, mScreen) == offsetof(EeGeRastScreen, screen), "mScreen");
static_assert(offsetof(GERScreenState, mAlphaTestingRefValue) == offsetof(EeGeRastScreen, alpha_testing_ref_value), "ref");
static_assert(offsetof(GERScreenState, mAlphaTesting) == offsetof(EeGeRastScreen, alpha_testing), "mAlphaTesting");
static_assert(offsetof(GERScreenState, mZCompare) == offsetof(EeGeRastScreen, z_compare), "mZCompare");
static_assert(offsetof(GERScreenState, m32C) == offsetof(EeGeRastScreen, counters), "m32C");
static_assert(offsetof(GERScreenState, mWaitForVSync) == offsetof(EeGeRastScreen, wait_for_vsync), "mWaitForVSync");
static_assert(sizeof(GERFogAttributes) == sizeof(EeGeRastFog) && sizeof(GEColor) == sizeof(EeGeRastColor), "fog");

static intptr_t GEFloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

static float GEBitsFloat(intptr_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, 4);
    return theValue;
}

// The calls the Rust rasterizer makes on other objects (EeGeRastOp), in the original's order.
struct GERasterizerHost {
    static intptr_t Call(void* inOwner, EeGeRastOp inOp, void* inObj, intptr_t a, intptr_t b)
    {
        GEModel* theModel = static_cast<GEModel*>(inObj);
        const GEPhysicalModel* thePlaced = static_cast<const GEPhysicalModel*>(inObj);
        GEText* theText = static_cast<GEText*>(inObj);
        switch (inOp) {
        case EeGeRastOp::NameNew: return (intptr_t) new UString(*reinterpret_cast<const UString*>(a));
        case EeGeRastOp::NameDelete: delete static_cast<UString*>(inObj); return 0;
        case EeGeRastOp::LockCreate: return TSScheduler::CreateCriticalSection(*reinterpret_cast<TSCriticalSection**>(a));
        case EeGeRastOp::LockDestroy: TSScheduler::DestroyCriticalSection(static_cast<TSCriticalSection*>(inObj)); return 0;
        case EeGeRastOp::Verify: ESVERIFY_AT(a != 0, (unsigned long)b); return 0;
        case EeGeRastOp::FontNew: return (intptr_t) new GEFont("Arial Bold", 9, 0, kFontQualityAntialiased);
        case EeGeRastOp::FontDereference: static_cast<GEFont*>(inObj)->Dereference(); return 0;
        case EeGeRastOp::TextNew: return (intptr_t) new GEText(reinterpret_cast<GEFont*>(a));
        case EeGeRastOp::TextDelete: delete theText; return 0;
        case EeGeRastOp::TextSetAlignment: theText->SetAlignment((unsigned long)a); return 0;
        case EeGeRastOp::TextSetFgColor: theText->SetFgColor(*reinterpret_cast<const GEColor*>(a)); return 0;
        case EeGeRastOp::TextSetBgColor: theText->SetBgColor(*reinterpret_cast<const GEColor*>(a)); return 0;
        case EeGeRastOp::TextSetFont: theText->SetFont(reinterpret_cast<GEFont*>(a)); return 0;
        case EeGeRastOp::TextSetIsTransparent: theText->SetIsTransparent(a != 0); return 0;
        case EeGeRastOp::TextSetLocation: theText->SetLocation(GEBitsFloat(a), GEBitsFloat(b)); return 0;
        case EeGeRastOp::ActivateRasterizer: return static_cast<GERasterizer*>(inOwner)->ActivateRasterizer();
        case EeGeRastOp::FontBitmapNew:
            new GEFontBitmap(static_cast<GERasterizer*>(inOwner), reinterpret_cast<GEFont*>(a), (unsigned long)b);
            return 0;
        case EeGeRastOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
        case EeGeRastOp::TextureDereference: static_cast<GETexture*>(inObj)->Dereference(); return 0;
        case EeGeRastOp::ModelCenter: *reinterpret_cast<GE3DPoint*>(a) = theModel->GetCenterPoint(); return 0;
        case EeGeRastOp::ModelBoundingRadius: return GEFloatBits(theModel->GetBoundingRadius((unsigned long)a));
        case EeGeRastOp::ModelGraphicsRadius: return GEFloatBits(theModel->GetGraphicsBoundingRadius());
        case EeGeRastOp::ModelGraphicsBox: *reinterpret_cast<GEBoundingBox*>(a) = theModel->GetGraphicsBoundingBox(); return 0;
        case EeGeRastOp::ModelSetClip: theModel->SetClipToPlane((unsigned long)a, b != 0); return 0;
        case EeGeRastOp::ModelInternalTransform:
            *reinterpret_cast<GETransformation*>(b) = theModel->GetInternalTransform((unsigned long)a);
            return 0;
        case EeGeRastOp::PlacedScale: return GEFloatBits(thePlaced->GetScale());
        case EeGeRastOp::PlacedFrame: return (intptr_t)thePlaced->GetFrame();
        case EeGeRastOp::PlacedModel: return (intptr_t)thePlaced->GetModel();
        case EeGeRastOp::PlacedLocation: *reinterpret_cast<GETransformation*>(a) = thePlaced->GetLocation(); return 0;
        default: return 0;
        }
    }
};

// The 2D draw list's executor (ee_ge list2d.rs): one virtual draw per command, for every draw the DX7 rasterizer's
// Rust does not make itself (the texts; any draw on another rasterizer)
static long GERasterizerDraw2D(void* inRasterizer, unsigned long inScreen, const EeGeDraw2D* inCmd)
{
    GERasterizer* theRasterizer = static_cast<GERasterizer*>(inRasterizer);
    switch (inCmd->kind) {
    case EeGeDraw2DKind::Bitmap: return theRasterizer->Draw(static_cast<GEBitmap*>(inCmd->object), inScreen);
    case EeGeDraw2DKind::Rhombus: return theRasterizer->Draw(static_cast<GERhombus*>(inCmd->object), inScreen);
    case EeGeDraw2DKind::Lines: return theRasterizer->Draw(static_cast<GELine*>(inCmd->object), inCmd->count, inScreen);
    case EeGeDraw2DKind::Points: return theRasterizer->Draw(static_cast<GEPoint*>(inCmd->object), inCmd->count, inScreen);
    case EeGeDraw2DKind::TextClipped:
        return theRasterizer->Draw(static_cast<GEText*>(inCmd->object), *reinterpret_cast<const GERectangle*>(&inCmd->clip),
                                   inCmd->surface ? static_cast<GESurface*>(inCmd->surface)
                                                  : theRasterizer->GetSecondarySurface(inScreen));
    case EeGeDraw2DKind::Text:
        return theRasterizer->Draw(static_cast<GEText*>(inCmd->object), static_cast<GESurface*>(inCmd->surface));
    }
    return 0;
}

// Creates a rasterizer with its drawing lock and a white-on-black Arial 9 debug text, not yet activated
GERasterizer::GERasterizer(const UString& inName)
{
    this->mRust = ee_ge_rast_new(&GERasterizerHost::Call, this, &inName);
    ee_ge_draw2d_init(&GERasterizerDraw2D, &GEVertexHost);
}

// Destroys the rasterizer, releasing the debug font and text and its drawing lock
GERasterizer::~GERasterizer()
{
    ee_ge_rast_destroy(this->mRust);
}

// Activates the rasterizer and makes it the active one on success (no active rasterizer on failure)
long GERasterizer::Activate()
{
    return (long)ee_ge_rast_activate(this->mRust);
}

// Deactivates the rasterizer: deletes every screen's render state and clears the active rasterizer
long GERasterizer::Deactivate()
{
    ee_ge_rast_deactivate(this->mRust);
    return kErrorOK;
}

// Returns a model's on-screen width in pixels (its scaled bounding diameter projected through the viewport)
float GERasterizer::GetPixelSize(GEModel* inModel, GEViewport* inViewport, GETransformation& inTransformation,
                                 float inScale, bool inOrthographic)
{
    return ee_ge_rast_pixel_size(this->mRust, inModel, GERust(*inViewport), GERust(inTransformation), inScale, inOrthographic);
}

// Returns a placed model's on-screen width in pixels, from its location, frame and scale
float GERasterizer::GetPixelSize(const GEPhysicalModel& inModel, GEViewport& inViewport, bool inOrthographic)
{
    return ee_ge_rast_placed_pixel_size(this->mRust, &inModel, GERust(inViewport), inOrthographic);
}

// The base's render state setters (0x10024352..0x100245e9): each records the state in the screen's record.
void GERasterizer::SetAlphaTesting(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::AlphaTesting, (intptr_t)inState);
}

void GERasterizer::SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::AlphaTestingCompareState, (intptr_t)inState);
}

void GERasterizer::SetAlphaTestingRefValue(float inValue, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::AlphaTestingRefValue, GEFloatBits(inValue));
}

// Sets the ambient light colour and recomputes the effective ambient colour (the screen is ignored)
void GERasterizer::SetAmbientColor(const GEColor& inColor, unsigned long inScreen)
{
    ee_ge_rast_set_ambient_color(this->mRust, reinterpret_cast<const EeGeRastColor*>(&inColor));
}

// Sets the ambient light intensity and recomputes the effective ambient colour (the screen is ignored)
void GERasterizer::SetAmbientIntensity(float inIntensity, unsigned long inScreen)
{
    ee_ge_rast_set_ambient_intensity(this->mRust, inIntensity);
}

void GERasterizer::SetAntiAliasing(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::AntiAliasing, (intptr_t)inState);
}

void GERasterizer::SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen)
{
    ee_ge_rast_set_blending(this->mRust, inScreen, inSource, inDestination);
}

void GERasterizer::SetColorKeying(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ColorKeying, (intptr_t)inState);
}

void GERasterizer::SetCulling(unsigned long inState)
{
    ee_ge_rast_set(this->mRust, EeGeRastVal::Culling, (intptr_t)inState);
}

// Does nothing (no-texture display mode is not supported by the base rasterizer)
void GERasterizer::SetDisplayNoTextureMode(bool inState, unsigned long inScreen)
{
}

// SetDisplayOverdrawMode: does nothing either (the same empty body, ICF-folded into 0x10024451)
void GERasterizer::SetDisplayOverdrawMode(bool inState, unsigned long inScreen)
{
}

void GERasterizer::SetDithering(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::Dithering, (intptr_t)inState);
}

void GERasterizer::SetFog(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::FogState, (intptr_t)inState);
}

void GERasterizer::SetFogColor(const GEColor& inColor, unsigned long inScreen)
{
    ee_ge_rast_set_fog_color(this->mRust, inScreen, reinterpret_cast<const EeGeRastColor*>(&inColor));
}

void GERasterizer::SetFogRange(float inStart, float inEnd, unsigned long inScreen)
{
    ee_ge_rast_set_fog_range(this->mRust, inScreen, inStart, inEnd);
}

void GERasterizer::SetFogType(unsigned long inType, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::FogType, (intptr_t)inType);
}

void GERasterizer::SetLighting(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::Lighting, (intptr_t)inState);
}

void GERasterizer::SetProjectionMode(unsigned long inMode, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ProjectionMode, (intptr_t)inMode);
}

void GERasterizer::SetShading(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::Shading, (intptr_t)inState);
}

void GERasterizer::SetShadingColorSource(unsigned long inSource, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ShadingColorSource, (intptr_t)inSource);
}

void GERasterizer::SetSpecular(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::Specular, (intptr_t)inState);
}

void GERasterizer::SetViewport(const GEViewport& inViewport, unsigned long inScreen)
{
    ee_ge_rast_set_viewport(this->mRust, inScreen, GERust(inViewport));
}

void GERasterizer::SetWaitForVSync(bool inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::WaitForVSync, inState);
}

void GERasterizer::SetZBuffer(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ZBuffer, (intptr_t)inState);
}

void GERasterizer::SetZCompare(unsigned long inState, unsigned long inScreen)
{
    ee_ge_rast_screen_set(this->mRust, inScreen, EeGeRastState::ZCompare, (intptr_t)inState);
}

// ---- the header's accessors (inline in the original) and the statics
GERScreenState* GERasterizer::GetScreenState(unsigned long inScreen) const
{
    return reinterpret_cast<GERScreenState*>(ee_ge_rast_screen(this->mRust, inScreen));
}

const GEColor& GERasterizer::GetAmbientColor() const { return *reinterpret_cast<const GEColor*>(ee_ge_rast_color(this->mRust, false)); }
float GERasterizer::GetAmbientIntensity() const { return GEBitsFloat(ee_ge_rast_get(this->mRust, EeGeRastVal::AmbientIntensity)); }
unsigned long GERasterizer::GetCulling() const { return (unsigned long)ee_ge_rast_get(this->mRust, EeGeRastVal::Culling); }
bool GERasterizer::GetIsActivated() const { return ee_ge_rast_get(this->mRust, EeGeRastVal::IsActivated) != 0; }
const UString& GERasterizer::GetName() const { return *reinterpret_cast<const UString*>(ee_ge_rast_get(this->mRust, EeGeRastVal::Name)); }
unsigned long GERasterizer::GetScreenCount() const { return (unsigned long)ee_ge_rast_get(this->mRust, EeGeRastVal::ScreenCount); }
unsigned long GERasterizer::GetScreenID() const { return (unsigned long)ee_ge_rast_get(this->mRust, EeGeRastVal::ScreenId); }
void GERasterizer::SetScreenID(unsigned long inID) { ee_ge_rast_set(this->mRust, EeGeRastVal::ScreenId, (intptr_t)inID); }

void GERasterizer::SetWaterRippleEffect(float inWaterRipple0, float inWaterRipple1, float inWaterRipple2)
{
    float theRipple[3] = {inWaterRipple0, inWaterRipple1, inWaterRipple2};
    ee_ge_rast_ripple(this->mRust, &theRipple, true);
}

GERasterizer* GERasterizer::GetActiveRasterizer() { return static_cast<GERasterizer*>(ee_ge_rast_active()); }
void GERasterizer::ClearRasterizers() { ee_ge_rast_list(0, NULL, 0); }
void GERasterizer::AddRasterizer(GERasterizer* inRasterizer) { ee_ge_rast_list(1, inRasterizer, 0); }
unsigned long GERasterizer::GetRasterizerCount() { return (unsigned long)(uintptr_t)ee_ge_rast_list(2, NULL, 0); }
GERasterizer* GERasterizer::GetRasterizer(unsigned long inIndex) { return static_cast<GERasterizer*>(ee_ge_rast_list(3, NULL, inIndex)); }

// 0x1002398a: GEText's scalar deleting dtor (see ~GERasterizer). A `delete p` compiles to a ??_G call whenever the
//   dtor body is inline-visible (even when the caller's budget is spent), and to ??1 + ??3 only when it is not;
//   UIForm.cpp has both shapes for the same field (~UIForm ??1+??3, RelayoutForm ??_G).
// 0x10024665..0x100246a7: the template-static init wrapper and three atexit registrars of
//   UList<GELODMetaVertex*>/<GELODPolygon*>/<GELODVertex*>::sNodePool (their $D ctors are GEBoundingBox.cpp's
//   0x10014ed4/0x10014efb/0x10014f22).

// ---- the engine's start-up and pixel formats (Graphics Engine.cpp, folded in R4.3 part 15)

HINSTANCE__* GEScreen::sInstance;

// The renderer linked into the game: the T&L DX7 rasterizer (DX7Rasterizer.cpp; the plug-in boundary went in part 14)
GERasterizer* DX7TnLRasterizer();

// Shuts the graphics engine down: forgets the rasterizers
static void GEShutdown(bool inForced)
{
    GERasterizer::ClearRasterizers();
}

// Starts the graphics engine: registers the shutdown hook and adds the rasterizer when it activates and finds a screen
// (the original's PIMIsValid: activate, count the screens, deactivate)
long GEInitialize(FSFileSpec& inDirectory, HINSTANCE__* const inInstance)
{
    URegisterShutdown(GEShutdown);
    PlatFontRegisterBuiltIn();   // the faces the fonts are made from (GDI's system fonts before)
    GEScreen::sInstance = inInstance;
    GERasterizer::ClearRasterizers();
    GERasterizer* theRasterizer = DX7TnLRasterizer();
    bool theValid = theRasterizer->Activate() == kErrorOK && theRasterizer->GetScreenCount() > 0;
    theRasterizer->Deactivate();
    if (theValid)
        GERasterizer::AddRasterizer(theRasterizer);
    return kErrorOK;
}

// The pixel formats (ee_ge pixel.rs): a colour and alpha (0..1 floats, or 8 bits each) packed into a pixel, a
// pixel's channels scaled to 8 bits
unsigned long GEPackColor(unsigned long inFormat, const GEColor& inColor, float inAlpha)
{
    return ee_ge_pack_color_float(inFormat, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue(), inAlpha);
}

unsigned long GEPackColor(unsigned long inFormat, unsigned long inRed, unsigned long inGreen, unsigned long inBlue,
                          unsigned long inAlpha)
{
    return ee_ge_pack_color(inFormat, inRed, inGreen, inBlue, inAlpha);
}

unsigned char GEUnpackRed(unsigned long inFormat, unsigned long inColor) { return ee_ge_unpack_red(inFormat, inColor); }
unsigned char GEUnpackGreen(unsigned long inFormat, unsigned long inColor) { return ee_ge_unpack_green(inFormat, inColor); }
unsigned char GEUnpackBlue(unsigned long inFormat, unsigned long inColor) { return ee_ge_unpack_blue(inFormat, inColor); }
