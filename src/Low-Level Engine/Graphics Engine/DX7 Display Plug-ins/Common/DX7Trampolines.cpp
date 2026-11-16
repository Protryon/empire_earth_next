// The DX7 renderer's trampolines into Rust (rust/crates/ee_dx7, docs/RUST_PLAN.md R4.2): each class's bodies forward to
// `extern "C"` functions with the opaque Rust object. New unit, no original address (tools/units.py DX7_TRAMPOLINES).
// (The vertex and polygon arrays' classes were merged into GEVertexArray and GEPolygonArray in R4.3 part 17.)
#include <windows.h>
#include <stdint.h>

#include "rust/ee_dx7.h"
#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include <stddef.h>
#include "dx7/DX7Rasterizer.h"
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
#include "dx7/DX7Screen.h"
#include "dx7/DX7Surface.h"
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

// --- DX7Movie: whole in Rust (ee_dx7 movobj.rs); the thread entry, pl_mpeg's callbacks, the mixer's pull and the calls
// on C++ objects ---------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <vector>
#include "pl_mpeg/pl_mpeg.h"
#include "platform/PlatPath.h"
#include "dx7/DX7Movie.h"

static unsigned long DX7MoviePlay(void* inMovie) { return ee_dx7mv_play(static_cast<EeDx7Movie*>(inMovie)); }
static void DX7MovieMix(void* inMovie, float* outFrames, unsigned long inCount) { ee_dx7mv_mix(inMovie, outFrames, inCount); }
static void DX7MovieAudio(plm_t*, plm_samples_t* inSamples, void* inMovie)
{
    ee_dx7mv_samples(static_cast<EeDx7Movie*>(inMovie), inSamples->interleaved, inSamples->count);
}
static void DX7MovieVideo(plm_t*, plm_frame_t* inFrame, void* inMovie)
{
    static std::vector<unsigned char> sBGRA;   // one movie plays at a time (sMovieCriticalSection)
    unsigned long theStride = inFrame->width * 4;
    if (sBGRA.size() < theStride * inFrame->height)
        sBGRA.resize(theStride * inFrame->height);
    plm_frame_to_bgra(inFrame, &sBGRA[0], (int)theStride);
    ee_dx7mv_frame(static_cast<EeDx7Movie*>(inMovie), &sBGRA[0], theStride);
}

static intptr_t DX7MovieHost(EeDx7MovieOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    GESurface* theSurface = static_cast<GESurface*>(inObj);
    switch (inOp) {
    case EeDx7MovieOp::CsNew: {
        TSCriticalSection* theSection = NULL;
        long theResult = TSScheduler::CreateCriticalSection(theSection);
        if (a != 0)
            ESVERIFY_AT(theResult == keseOK, (unsigned long)a);
        return (intptr_t)theSection;
    }
    case EeDx7MovieOp::CsDelete: TSScheduler::DestroyCriticalSection(static_cast<TSCriticalSection*>(inObj)); return 0;
    case EeDx7MovieOp::CsEnter: static_cast<TSCriticalSection*>(inObj)->Enter(); return 0;
    case EeDx7MovieOp::CsLeave: static_cast<TSCriticalSection*>(inObj)->Leave(); return 0;
    case EeDx7MovieOp::EventNew: {
        TSEvent* theEvent = NULL;
        ESVERIFY_AT(TSScheduler::CreateEvent(theEvent, false) == keseOK, (unsigned long)a);
        return (intptr_t)theEvent;
    }
    case EeDx7MovieOp::EventDelete: TSScheduler::DestroyEvent(static_cast<TSEvent*>(inObj)); return 0;
    case EeDx7MovieOp::EventSet: static_cast<TSEvent*>(inObj)->Set(); return 0;
    case EeDx7MovieOp::EventClear: static_cast<TSEvent*>(inObj)->Clear(); return 0;
    case EeDx7MovieOp::EventWait: static_cast<TSEvent*>(inObj)->Wait(INFINITE); return 0;
    case EeDx7MovieOp::ThreadNew: {
        TSThread* theThread = NULL;
        ESVERIFY_AT(TSScheduler::CreateThread(theThread, DX7MoviePlay, inObj) == keseOK, (unsigned long)a);
        return (intptr_t)theThread;
    }
    case EeDx7MovieOp::ThreadDelete: TSScheduler::DestroyThread(static_cast<TSThread*>(inObj)); return 0;
    case EeDx7MovieOp::ThreadPriority: static_cast<TSThread*>(inObj)->SetPriority((long)a); return 0;
    case EeDx7MovieOp::MovieReference: static_cast<GEMovie*>(inObj)->Reference(); return 0;
    case EeDx7MovieOp::MovieDereference: static_cast<GEMovie*>(inObj)->Dereference(); return 0;
    case EeDx7MovieOp::FileName: {
        const FSFileSpec& theFile = *static_cast<const FSFileSpec*>(inObj);
        snprintf(reinterpret_cast<char*>(a), MAX_PATH, "%s%s%s", (const char*)theFile.GetVolume(), (const char*)theFile.GetDirectory(),
                 (const char*)theFile.GetFile());
        return 0;
    }
    case EeDx7MovieOp::FpGet: return (intptr_t)(a == 0 ? UMGetFloatingPointPrecision() : UMGetFloatingPointRoundingMode());
    case EeDx7MovieOp::FpSet:
        if (a == 0)
            UMSetFloatingPointPrecision((unsigned long)b);
        else
            UMSetFloatingPointRoundingMode((unsigned long)b);
        return 0;
    case EeDx7MovieOp::Open: {   // the game's path on the native file system (case-insensitive lookup off Windows)
        char theNative[PLAT_MAX_PATH];
        const char* thePath = reinterpret_cast<const char*>(a);
        return (intptr_t)(PlatPathGetAttributes(thePath) != PLAT_ATTR_INVALID && PlatPathToNative(thePath, theNative, sizeof(theNative))
                              ? plm_create_with_filename(theNative) : NULL);
    }
    case EeDx7MovieOp::InstallVideo: plm_set_video_decode_callback(static_cast<plm_t*>(inObj), DX7MovieVideo, reinterpret_cast<void*>(a)); return 0;
    case EeDx7MovieOp::InstallAudio: {
        double theLead;
        memcpy(&theLead, &b, sizeof theLead);
        plm_set_audio_decode_callback(static_cast<plm_t*>(inObj), DX7MovieAudio, reinterpret_cast<void*>(a));
        plm_set_audio_lead_time(static_cast<plm_t*>(inObj), theLead);
        return 0;
    }
    case EeDx7MovieOp::MixerRate: return SEMixer::GetActiveMixer() ? (intptr_t)SEMixer::GetActiveMixer()->GetOutputRate() : 0;
    case EeDx7MovieOp::MixerSource: SEMixer::GetActiveMixer()->SetMovieSource(inObj ? DX7MovieMix : NULL, inObj); return 0;
    case EeDx7MovieOp::ActiveScreen: return (intptr_t)GERasterizer::GetActiveRasterizer()->GetScreen((unsigned long)a);
    case EeDx7MovieOp::ActiveTarget: return (intptr_t)GERasterizer::GetActiveRasterizer()->GetSecondarySurface((unsigned long)a);
    case EeDx7MovieOp::ScreenCreateSurface:
        return static_cast<GEScreen*>(inObj)->CreateSurface(*reinterpret_cast<GESurfaceDescription*>(a), reinterpret_cast<GESurface**>(b));
    case EeDx7MovieOp::SurfaceInfo: {
        const uint32_t theInfo[4] = { (uint32_t)theSurface->GetPixelFormat(), (uint32_t)theSurface->GetPixelDepth(),
                                      (uint32_t)theSurface->GetWidth(), (uint32_t)theSurface->GetHeight() };
        memcpy(reinterpret_cast<void*>(a), theInfo, sizeof theInfo);
        return 0;
    }
    case EeDx7MovieOp::SurfaceLock: return theSurface->Lock();
    case EeDx7MovieOp::SurfaceUnlock: return theSurface->Unlock();
    case EeDx7MovieOp::SurfaceMemory:
        reinterpret_cast<uintptr_t*>(a)[0] = (uintptr_t)theSurface->GetMemoryAddress();
        reinterpret_cast<uintptr_t*>(a)[1] = theSurface->GetBytesPerRow();
        return 0;
    case EeDx7MovieOp::SurfaceReference: theSurface->Reference(); return 0;
    case EeDx7MovieOp::SurfaceDereference: theSurface->Dereference(); return 0;
    case EeDx7MovieOp::SurfaceCopy: {
        const float* r = reinterpret_cast<const float*>(b);
        GERectangle theSource(r[0], r[1], r[2], r[3]), theDestination(r[4], r[5], r[6], r[7]);
        return theSurface->CopyTo(reinterpret_cast<GESurface*>(a), theSource, theDestination, false);
    }
    default: return 0;
    }
}

// Creates a movie shown over the whole back buffer and starts its playback thread
DX7Movie::DX7Movie(const FSFileSpec& inFile, unsigned long inScreen)
{
    this->mDX7 = ee_dx7mv_new(DX7MovieHost, static_cast<GEMovie*>(this), &inFile, inScreen, NULL, false);
}

// Creates a movie drawn into the given rectangle, optionally looping, and starts its playback thread
DX7Movie::DX7Movie(const FSFileSpec& inFile, unsigned long inScreen, GERectangle inRectangle, bool inLoop)
{
    this->mDX7 = ee_dx7mv_new(DX7MovieHost, static_cast<GEMovie*>(this), &inFile, inScreen,
                              reinterpret_cast<const float*>(&inRectangle), inLoop);
}
