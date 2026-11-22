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
#include "rust/render/dx7.h"
#include "GERustMath.h"

static_assert(sizeof(EeGeRastFog) == 0x20 && sizeof(GEColor) == sizeof(EeGeRastColor), "fog");

static intptr_t GEFloatBits(float inValue) { return UFloatBits(inValue); }

// The calls the Rust rasterizer makes on other objects (EeGeRastOp), in the original's order.
struct GERasterizerHost {
    static intptr_t Call(void* inOwner, EeGeRastOp inOp, void* inObj, intptr_t a, intptr_t b)
    {
        switch (inOp) {
        case EeGeRastOp::NameNew: return (intptr_t) new UString(*reinterpret_cast<const UString*>(a));
        case EeGeRastOp::NameDelete: delete static_cast<UString*>(inObj); return 0;
        case EeGeRastOp::LockCreate: return TSScheduler::CreateCriticalSection(*reinterpret_cast<TSCriticalSection**>(a));
        case EeGeRastOp::LockDestroy: TSScheduler::DestroyCriticalSection(static_cast<TSCriticalSection*>(inObj)); return 0;
        case EeGeRastOp::Verify: ESVERIFY_AT(a != 0, (unsigned long)b); return 0;
        case EeGeRastOp::FontNew: return (intptr_t) new GEFont("Arial Bold", 9, 0, kFontQualityAntialiased);
        case EeGeRastOp::FontDereference: static_cast<GEFont*>(inObj)->Dereference(); return 0;
        case EeGeRastOp::TextNew: return (intptr_t) new GEText(reinterpret_cast<GEFont*>(a));
        case EeGeRastOp::TextDelete: delete static_cast<GEText*>(inObj); return 0;
        case EeGeRastOp::TextSetAlignment: static_cast<GEText*>(inObj)->SetAlignment((unsigned long)a); return 0;
        case EeGeRastOp::TextSetFgColor: static_cast<GEText*>(inObj)->SetFgColor(*reinterpret_cast<const GEColor*>(a)); return 0;
        case EeGeRastOp::TextSetBgColor: static_cast<GEText*>(inObj)->SetBgColor(*reinterpret_cast<const GEColor*>(a)); return 0;
        case EeGeRastOp::TextSetFont: static_cast<GEText*>(inObj)->SetFont(reinterpret_cast<GEFont*>(a)); return 0;
        case EeGeRastOp::TextSetIsTransparent: static_cast<GEText*>(inObj)->SetIsTransparent(a != 0); return 0;
        case EeGeRastOp::TextSetLocation: static_cast<GEText*>(inObj)->SetLocation(UBitsFloat(a), UBitsFloat(b)); return 0;
        case EeGeRastOp::ActivateRasterizer: return static_cast<GERasterizer*>(inOwner)->ActivateRasterizer();
        case EeGeRastOp::FontBitmapNew:
            new GEFontBitmap(static_cast<GERasterizer*>(inOwner), reinterpret_cast<GEFont*>(a), (unsigned long)b);
            return 0;
        case EeGeRastOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
        case EeGeRastOp::TextureDereference: static_cast<GETexture*>(inObj)->Dereference(); return 0;
        case EeGeRastOp::ModelCenter: *reinterpret_cast<GE3DPoint*>(a) = static_cast<GEModel*>(inObj)->GetCenterPoint(); return 0;
        case EeGeRastOp::ModelBoundingRadius: return GEFloatBits(static_cast<GEModel*>(inObj)->GetBoundingRadius((unsigned long)a));
        case EeGeRastOp::ModelGraphicsRadius: return GEFloatBits(static_cast<GEModel*>(inObj)->GetGraphicsBoundingRadius());
        case EeGeRastOp::ModelGraphicsBox: *reinterpret_cast<GEBoundingBox*>(a) = static_cast<GEModel*>(inObj)->GetGraphicsBoundingBox(); return 0;
        case EeGeRastOp::ModelSetClip: static_cast<GEModel*>(inObj)->SetClipToPlane((unsigned long)a, b != 0); return 0;
        case EeGeRastOp::ModelInternalTransform:
            *reinterpret_cast<GETransformation*>(b) = static_cast<GEModel*>(inObj)->GetInternalTransform((unsigned long)a);
            return 0;
        case EeGeRastOp::PlacedScale: return GEFloatBits(static_cast<const GEPhysicalModel*>(inObj)->GetScale());
        case EeGeRastOp::PlacedFrame: return (intptr_t)static_cast<const GEPhysicalModel*>(inObj)->GetFrame();
        case EeGeRastOp::PlacedModel: return (intptr_t)static_cast<const GEPhysicalModel*>(inObj)->GetModel();
        case EeGeRastOp::PlacedLocation: *reinterpret_cast<GETransformation*>(a) = static_cast<const GEPhysicalModel*>(inObj)->GetLocation(); return 0;
        default: return 0;
        }
    }
};

// The 2D draw list's executor (ee_ge list2d.rs): one virtual draw per command, for every draw the DX7 rasterizer's
// Rust does not make itself (the texts; any draw on another rasterizer)
static long GERasterizerDraw2D(void* inRasterizer, unsigned long inScreen, const EeGeDraw2D* inCmd)
{
    switch (inCmd->kind) {
    case EeGeDraw2DKind::Bitmap: return static_cast<GERasterizer*>(inRasterizer)->Draw(static_cast<GEBitmap*>(inCmd->object), inScreen);
    case EeGeDraw2DKind::Rhombus: return static_cast<GERasterizer*>(inRasterizer)->Draw(static_cast<GERhombus*>(inCmd->object), inScreen);
    case EeGeDraw2DKind::Lines: return static_cast<GERasterizer*>(inRasterizer)->Draw(static_cast<GELine*>(inCmd->object), inCmd->count, inScreen);
    case EeGeDraw2DKind::Points: return static_cast<GERasterizer*>(inRasterizer)->Draw(static_cast<GEPoint*>(inCmd->object), inCmd->count, inScreen);
    case EeGeDraw2DKind::TextClipped:
        return static_cast<GERasterizer*>(inRasterizer)->Draw(static_cast<GEText*>(inCmd->object), *reinterpret_cast<const GERectangle*>(&inCmd->clip),
                                   inCmd->surface ? static_cast<GESurface*>(inCmd->surface)
                                                  : static_cast<GERasterizer*>(inRasterizer)->GetSecondarySurface(inScreen));
    case EeGeDraw2DKind::Text:
        return static_cast<GERasterizer*>(inRasterizer)->Draw(static_cast<GEText*>(inCmd->object), static_cast<GESurface*>(inCmd->surface));
    }
    return 0;
}

extern "C" intptr_t GEVertexHost(void* inOwner, EeGeBmpOp inOp, void* inObj, intptr_t a, intptr_t b);   // GEBitmap.cpp

intptr_t DX7RasterizerHost(void* inOwner, EeDx7RastOp inOp, void* inObj, intptr_t a, intptr_t b);   // DX7Rasterizer.cpp

// Creates a rasterizer with its drawing lock and a white-on-black Arial 9 debug text, not yet activated, and its DX7
// rasterizer with a dark blue debug colour (inTnL: the T&L rasterizer), which the 2D draw list draws on in Rust
GERasterizer::GERasterizer(const UString& inName, bool inTnL)
{
    this->mRust = ee_ge_rast_new(&GERasterizerHost::Call, this, &inName);
    ee_ge_draw2d_init(&GERasterizerDraw2D, &GEVertexHost);
    this->mDX7 = ee_dx7r_new(&DX7RasterizerHost, this, this->mRust, inTnL);
    ee_ge_draw2d_direct(this, this->mDX7);
}

// Destroys the DX7 records, then the debug font and text and the drawing lock
GERasterizer::~GERasterizer()
{
    ee_ge_draw2d_direct(this, NULL);
    ee_dx7r_destroy(this->mDX7);
    ee_ge_rast_destroy(this->mRust);
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

void GERasterizer::SetWaterRippleEffect(float inWaterRipple0, float inWaterRipple1, float inWaterRipple2)
{
    ee_ge_rast_ripple_values(this->mRust, inWaterRipple0, inWaterRipple1, inWaterRipple2);
}

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

// The start-up (GEInitialize, GEShutdown) is Rust's (ee_render ge startup.rs); its calls on the C++ objects
extern "C" {
bool ee_gecpp_fonts_register_builtin() { return PlatFontRegisterBuiltIn() != 0; }
void ee_gecpp_fonts_missing() { ESVerify(__FILE__, __LINE__, "PlatFontRegisterBuiltIn() (the fonts built into the program: PlatFontData.c)"); }
void ee_gecpp_set_instance(void* inInstance) { GEScreen::sInstance = static_cast<HINSTANCE__*>(inInstance); }
void* ee_gecpp_tnl_rasterizer() { return DX7TnLRasterizer(); }
long ee_gecpp_rast_activate(void* inRasterizer) { return static_cast<GERasterizer*>(inRasterizer)->Activate(); }
unsigned long ee_gecpp_rast_screen_count(void* inRasterizer) { return static_cast<GERasterizer*>(inRasterizer)->GetScreenCount(); }
void ee_gecpp_rast_deactivate(void* inRasterizer) { static_cast<GERasterizer*>(inRasterizer)->Deactivate(); }
}

// Starts the graphics engine (the directory is not read)
long GEInitialize(FSFileSpec&, HINSTANCE__* const inInstance) { return ee_ge_initialize(inInstance); }

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
