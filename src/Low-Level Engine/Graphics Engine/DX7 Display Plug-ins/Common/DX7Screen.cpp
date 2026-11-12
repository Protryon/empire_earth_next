// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Screen.cpp (file name inferred: no
// __FILE__; class name from the exports)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x1000d0fc..0x1000e793: the U2D key init chains, sScreens' init chain (0x1000d228, guarded dtor 0x1000d256), the
// ctor 0x1000d271, UString's scalar deleting dtor (new UString in the ctor), the dtor, the members in alphabetical
// order, then the vector<DX7Screen*>/vector<GEVideoMode>/vector<GESurface*> COMDATs and the template-static init
// chain 0x1000e751.
#include <windows.h>
#include "gx/GX.h"

#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/U2DPolygon.h"
#include "lle/GEModel.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "platform/PlatWindow.h"
#include "platform/PlatSim.h"
#include "lle/GERenderState.h"
#include "rust/ee_dx7.h"

// The screen algorithms are in Rust (rust/crates/ee_dx7/src/screen.rs); this class keeps the fields, the attached
// surfaces and the window, each method forwards or is a trampoline of ee_dx7cpp_screen_*.

// The U2D key init chains, then sScreens' chain (wrapper, ctor, atexit registrar, guarded dtor):
// SYNTHETIC: DX7HR 0x1000d0fc
// SYNTHETIC: DX7HR 0x1000d106
// SYNTHETIC: DX7HR 0x1000d117
// SYNTHETIC: DX7HR 0x1000d123
// SYNTHETIC: DX7HR 0x1000d12e
// SYNTHETIC: DX7HR 0x1000d138
// SYNTHETIC: DX7HR 0x1000d149
// SYNTHETIC: DX7HR 0x1000d155
// SYNTHETIC: DX7HR 0x1000d160
// SYNTHETIC: DX7HR 0x1000d16a
// SYNTHETIC: DX7HR 0x1000d17b
// SYNTHETIC: DX7HR 0x1000d187
// SYNTHETIC: DX7HR 0x1000d192
// SYNTHETIC: DX7HR 0x1000d19c
// SYNTHETIC: DX7HR 0x1000d1ad
// SYNTHETIC: DX7HR 0x1000d1b9
// SYNTHETIC: DX7HR 0x1000d1c4
// SYNTHETIC: DX7HR 0x1000d1ce
// SYNTHETIC: DX7HR 0x1000d1df
// SYNTHETIC: DX7HR 0x1000d1eb
// SYNTHETIC: DX7HR 0x1000d1f6
// SYNTHETIC: DX7HR 0x1000d200
// SYNTHETIC: DX7HR 0x1000d211
// SYNTHETIC: DX7HR 0x1000d21d
// SYNTHETIC: DX7HR 0x1000d228
// SYNTHETIC: DX7HR 0x1000d232
// SYNTHETIC: DX7HR 0x1000d24a
// SYNTHETIC: DX7HR 0x1000d256

std::vector<DX7Screen*> DX7Screen::sScreens;
unsigned long DX7Screen::sScreensCaptured;
bool DX7Screen::sFirstCaptureWasAttachedToDesktop;

// FUNCTION: DX7HR 0x1000d271
// Creates a screen for a DirectDraw device: lists its display modes, finds the current one, probes 32-bit support
// and registers it
DX7Screen::DX7Screen(void* inMonitor, GXDirectDraw* inDirectDraw, const UString& inName,
                     bool inAttachedToDesktop, bool inSharesPrimary)
    : mMonitor(inMonitor), mAttachedToDesktop(inAttachedToDesktop), mIsFullScreen(false),
      mSharesPrimary(inSharesPrimary), mCanMagFilterSurfaces(false), mCanMinFilterSurfaces(false)
{
    this->mDirectDraw = inDirectDraw;
    this->mName = new UString(inName);
    ee_dx7sc_initialize(this, this->mDirectDraw);
    this->Test32BitSupport();
    DX7Screen::sScreens.push_back(this);
}

// UString's scalar deleting dtor (the `new UString` above):
// SYNTHETIC: DX7HR 0x1000d409

// FUNCTION: DX7HR 0x1000d426
// Destroys the screen: frees its name, removes it from the screen list and releases the DirectDraw device
DX7Screen::~DX7Screen()
{
    std::vector<DX7Screen*>::iterator theScreen;
    delete this->mName;
    theScreen = std::find(DX7Screen::sScreens.begin(), DX7Screen::sScreens.end(), this);
    if (theScreen != DX7Screen::sScreens.end())
        DX7Screen::sScreens.erase(theScreen);
    this->mDirectDraw->Release();
}

// FUNCTION: DX7HR 0x1000d4b9
// Takes the screen for a window, full-screen exclusive (inToClipboard true) or windowed; a later screen only joins
// if it and the first are both on the desktop (else error 0xd); counts the captured screens
long DX7Screen::Capture(const GEWindow* inWindow, bool inToClipboard)
{
    EeDx7CaptureState theState = { DX7Screen::sScreensCaptured, DX7Screen::sFirstCaptureWasAttachedToDesktop };
    long theResult = ee_dx7sc_capture(this->mDirectDraw, inWindow->GetOSRepresentation(), inToClipboard, this->GetAttachedToDesktop(),
                                      &theState, &this->mIsFullScreen);
    DX7Screen::sScreensCaptured = theState.captured;
    DX7Screen::sFirstCaptureWasAttachedToDesktop = theState.first_attached;
    return theResult;
}

// FUNCTION: DX7HR 0x1000d622
// Creates a window (a platform window: platform/PlatWindow.h): style 1 is a framed, resizable window with an
// inWidth x inHeight client area, else a topmost borderless one at inOrigin (the original's popup), which is the
// full screen (borderless, no mode change) when it covers the display. The class name and the parent are unused.
long DX7Screen::CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                              UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                              GEWindow* inParent)
{
    long theResult = kErrorOK;
    int32_t theDisplayWidth = 0, theDisplayHeight = 0;
    // window sizes are in the display's own units (macOS: points), not its pixels
    PlatWindowGetDisplayLogicalSize(NULL, &theDisplayWidth, &theDisplayHeight);
    EeDx7WindowParams theParams;
    ee_dx7sc_window_params(gPlatSimMode != 0, inStyle, (int32_t)inWidth, (int32_t)inHeight, theDisplayWidth, theDisplayHeight,
                           inOrigin.GetX(), inOrigin.GetY(), &theParams);
    PlatWindow* theWindow = theParams.create_at
        ? PlatWindowCreateAt(inTitle, theParams.x, theParams.y, theParams.width, theParams.height, theParams.flags)
        : PlatWindowCreate(inTitle, theParams.width, theParams.height, theParams.flags);
    if (theWindow != NULL) {
        PlatCursorShow(0);   // the game draws its own pointer over its windows (WM_SETCURSOR's SetCursor(NULL))
        TSThread::DispatchWindowEvents();   // the messages CreateWindowEx sent (the window is not the game's yet)
        outWindow = new GEWindow(theWindow, inWidth, inHeight, inStyle, true);
    } else {
        ESDebugOut("CreateAWindow: %s\n", PlatWindowLastError());
        ESRecordNativeError(PlatGetLastError());
        ESGetNativeError();
        theResult = kErrorNativeError;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000d7e4
// Creates a DirectDraw surface of the described kind, with its flip chain or mipmap levels; a Z buffer that
// prefers 16 (or 32) bits falls back through the other depths
long DX7Screen::CreateSurface(GESurfaceDescription& inDescription, GESurface** outSurface)
{
    return ee_dx7sc_create_surface(this, this->mDirectDraw, reinterpret_cast<EeDx7SurfaceDescription*>(&inDescription),
                                   reinterpret_cast<void**>(outSurface));
}

// FUNCTION: DX7HR 0x1000dc2b
// FUNCTION: DX7HR 0x1000dc67
// (the texture-format and display-mode enumeration callbacks are in Rust)

// FUNCTION: DX7HR 0x1000dce2
// Returns the screen's desktop rectangle: 0,0 to the current mode's size (the display's: GX never changes the mode;
// the original asked the monitor where it had one, and the screens are created without one)
const GERectangle& DX7Screen::GetDesktopRectangle()
{
    this->mDesktopRectangle.SetTop(0.0f);
    this->mDesktopRectangle.SetLeft(0.0f);
    this->mDesktopRectangle.SetRight((float)this->mModes[this->mCurrentModeIndex].mWidth);
    this->mDesktopRectangle.SetBottom((float)this->mModes[this->mCurrentModeIndex].mHeight);
    return this->mDesktopRectangle;
}

// FUNCTION: DX7HR 0x1000dde7
// Returns the screen to normal cooperative level for the window and uncounts the capture; native error on failure
long DX7Screen::Release(const GEWindow* inWindow)
{
    EeDx7CaptureState theState = { DX7Screen::sScreensCaptured, DX7Screen::sFirstCaptureWasAttachedToDesktop };
    long theResult = ee_dx7sc_release(this->mDirectDraw, inWindow->GetOSRepresentation(), &theState, &this->mIsFullScreen);
    DX7Screen::sScreensCaptured = theState.captured;
    return theResult;
}

// FUNCTION: DX7HR 0x1000de20
// Restores every lost DirectDraw surface; if the device is not ready yet, sleeps 100 ms and returns a native error
long DX7Screen::RestoreAllSurfaces()
{
    return ee_dx7sc_restore_all(this->mDirectDraw);
}

// FUNCTION: DX7HR 0x1000de5d
// Switches a full-screen screen to the given display mode (no-op when windowed); records the mode index even on failure
long DX7Screen::SetMode(unsigned long inIndex)
{
    bool theUpdate = false;
    long theResult = ee_dx7sc_set_mode(this->mDirectDraw, this->mIsFullScreen, reinterpret_cast<const EeDx7VideoMode*>(&this->mModes[inIndex]), &theUpdate);
    if (theUpdate)
        this->mCurrentModeIndex = inIndex;
    return theResult;
}

// FUNCTION: DX7HR 0x1000dec6
// Probes 32-bit rendering and 32-bit texture support with a 16 x 16 test window and a 2 x 2 surface on a HAL device
// (looks up 800 x 600 x 16 without checking it was found)
void DX7Screen::Test32BitSupport()
{
#ifdef __APPLE__
    // port: GX renders and samples 32-bit everywhere; a probe window, even hidden, took the key window status from the
    // full screen game window at every renderer activation and macOS took it out of full screen
    this->m32BitDeviceSupport = true;
    this->m32BitTextureSupport = true;
    return;
#endif
    // port: style 2, a hidden probe window (a visible one took the focus from the full screen game window, which
    // left full screen or minimized at every renderer activation)
    ee_dx7sc_test_32bit_support(this, this->mDirectDraw);
}

// ---- the trampolines the Rust screen calls
extern "C" {
void* ee_dx7cpp_screen_new_surface(DX7Screen* inScreen, void* inGXSurface, const GESurfaceDescription* inDescription)
{
    return DX7Surface::NewForScreen(static_cast<GXSurface*>(inGXSurface), *inDescription, inScreen);
}
void ee_dx7cpp_screen_push_attached(DX7Screen*, void* inParent, void* inChild)
{
    static_cast<DX7Surface*>(static_cast<GESurface*>(inParent))->PushAttached(static_cast<GESurface*>(inChild));
}
void* ee_dx7cpp_screen_surface_gx(void* inSurface)
{
    return (GXSurface*)*dynamic_cast<DX7Surface*>(static_cast<GESurface*>(inSurface));
}
void ee_dx7cpp_screen_surface_dereference(void* inSurface) { static_cast<GESurface*>(inSurface)->Dereference(); }
void ee_dx7cpp_screen_set_filter_caps(DX7Screen* inScreen, bool inMin, bool inMag)
{
    inScreen->mCanMinFilterSurfaces = inMin;
    inScreen->mCanMagFilterSurfaces = inMag;
}
void ee_dx7cpp_screen_push_mode(DX7Screen* inScreen, const GEVideoMode* inMode) { inScreen->mModes.push_back(*inMode); }
void ee_dx7cpp_screen_set_current_mode(DX7Screen* inScreen, unsigned long inIndex) { inScreen->mCurrentModeIndex = inIndex; }
int32_t ee_dx7cpp_screen_create_probe_window(DX7Screen* inScreen, void** outWindow)
{
    GE2DPoint theOrigin(0.0f, 0.0f);
    UString theClassName("TestWindowClass");
    UString theTitle("");
    GEWindow* theWindow = NULL;
    long theResult = inScreen->CreateAWindow(theOrigin, 16, 16, theClassName, theTitle, theWindow, 2, NULL);
    *outWindow = theWindow;
    return theResult;
}
int32_t ee_dx7cpp_screen_capture(DX7Screen* inScreen, void* inWindow) { return inScreen->Capture(static_cast<GEWindow*>(inWindow), false); }
int32_t ee_dx7cpp_screen_release(DX7Screen* inScreen, void* inWindow) { return inScreen->Release(static_cast<GEWindow*>(inWindow)); }
void ee_dx7cpp_screen_delete_window(void* inWindow) { delete static_cast<GEWindow*>(inWindow); }
void ee_dx7cpp_screen_dispatch_events() { TSThread::DispatchWindowEvents(); }
unsigned long ee_dx7cpp_screen_mode_count(DX7Screen* inScreen) { return inScreen->GetModeCount(); }
void ee_dx7cpp_screen_mode(DX7Screen* inScreen, unsigned long inIndex, GEVideoMode* outMode) { *outMode = inScreen->GetMode(inIndex); }
void ee_dx7cpp_screen_set_32bit_support(DX7Screen* inScreen, bool inDevice, bool inTexture)
{
    inScreen->m32BitDeviceSupport = inDevice;
    inScreen->m32BitTextureSupport = inTexture;
}
}

// COMDATs this unit is the first (link order) to instantiate, then the template-static init chain head (it calls the
// three GELOD pools' $D ctors and atexit registrars: the unit sees no U2DSparseArray.h, so no fourth pool).
// TEMPLATE: DX7HR 0x1000e1c8
// TEMPLATE: DX7HR 0x1000e1f4
// TEMPLATE: DX7HR 0x1000e220
// TEMPLATE: DX7HR 0x1000e24c
// TEMPLATE: DX7HR 0x1000e29c
// TEMPLATE: DX7HR 0x1000e2f6
// TEMPLATE: DX7HR 0x1000e30b
// TEMPLATE: DX7HR 0x1000e5d3
// TEMPLATE: DX7HR 0x1000e644
// TEMPLATE: DX7HR 0x1000e65b
// TEMPLATE: DX7HR 0x1000e67e
// TEMPLATE: DX7HR 0x1000e6a6
// TEMPLATE: DX7HR 0x1000e6d8
// TEMPLATE: DX7HR 0x1000e6fb
// TEMPLATE: DX7HR 0x1000e72a
// TEMPLATE: DX7HR 0x1000e740
// SYNTHETIC: DX7HR 0x1000e751
// SYNTHETIC: DX7HR 0x1000e76f
// SYNTHETIC: DX7HR 0x1000e77b
// SYNTHETIC: DX7HR 0x1000e787

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// SYNTHETIC: DX7HRTNL 0x1000d144
// SYNTHETIC: DX7HRTNL 0x1000d14e
// SYNTHETIC: DX7HRTNL 0x1000d15f
// SYNTHETIC: DX7HRTNL 0x1000d16b
// SYNTHETIC: DX7HRTNL 0x1000d176
// SYNTHETIC: DX7HRTNL 0x1000d180
// SYNTHETIC: DX7HRTNL 0x1000d191
// SYNTHETIC: DX7HRTNL 0x1000d19d
// SYNTHETIC: DX7HRTNL 0x1000d1a8
// SYNTHETIC: DX7HRTNL 0x1000d1b2
// SYNTHETIC: DX7HRTNL 0x1000d1c3
// SYNTHETIC: DX7HRTNL 0x1000d1cf
// SYNTHETIC: DX7HRTNL 0x1000d1da
// SYNTHETIC: DX7HRTNL 0x1000d1e4
// SYNTHETIC: DX7HRTNL 0x1000d1f5
// SYNTHETIC: DX7HRTNL 0x1000d201
// SYNTHETIC: DX7HRTNL 0x1000d20c
// SYNTHETIC: DX7HRTNL 0x1000d216
// SYNTHETIC: DX7HRTNL 0x1000d227
// SYNTHETIC: DX7HRTNL 0x1000d233
// SYNTHETIC: DX7HRTNL 0x1000d23e
// SYNTHETIC: DX7HRTNL 0x1000d248
// SYNTHETIC: DX7HRTNL 0x1000d259
// SYNTHETIC: DX7HRTNL 0x1000d265
// SYNTHETIC: DX7HRTNL 0x1000d270
// SYNTHETIC: DX7HRTNL 0x1000d27a
// SYNTHETIC: DX7HRTNL 0x1000d292
// SYNTHETIC: DX7HRTNL 0x1000d29e
// FUNCTION: DX7HRTNL 0x1000d2b9
// SYNTHETIC: DX7HRTNL 0x1000d451
// FUNCTION: DX7HRTNL 0x1000d46e
// FUNCTION: DX7HRTNL 0x1000d501
// FUNCTION: DX7HRTNL 0x1000d66a
// FUNCTION: DX7HRTNL 0x1000d82c
// FUNCTION: DX7HRTNL 0x1000dc73
// FUNCTION: DX7HRTNL 0x1000dcaf
// FUNCTION: DX7HRTNL 0x1000dd2a
// FUNCTION: DX7HRTNL 0x1000de2f
// FUNCTION: DX7HRTNL 0x1000de68
// FUNCTION: DX7HRTNL 0x1000dea5
// FUNCTION: DX7HRTNL 0x1000df0e
// FUNCTION: DX7HRTNL 0x1000e1f7
// TEMPLATE: DX7HRTNL 0x1000e21c
// TEMPLATE: DX7HRTNL 0x1000e248
// TEMPLATE: DX7HRTNL 0x1000e274
// TEMPLATE: DX7HRTNL 0x1000e2a0
// TEMPLATE: DX7HRTNL 0x1000e2fa
// TEMPLATE: DX7HRTNL 0x1000e35f
// TEMPLATE: DX7HRTNL 0x1000e633
// TEMPLATE: DX7HRTNL 0x1000e6a4
// TEMPLATE: DX7HRTNL 0x1000e6bb
// TEMPLATE: DX7HRTNL 0x1000e703
// TEMPLATE: DX7HRTNL 0x1000e735
// TEMPLATE: DX7HRTNL 0x1000e758
// TEMPLATE: DX7HRTNL 0x1000e7af
// TEMPLATE: DX7HRTNL 0x1000e7c5
// SYNTHETIC: DX7HRTNL 0x1000e7d6
// SYNTHETIC: DX7HRTNL 0x1000e7f4
// SYNTHETIC: DX7HRTNL 0x1000e800
// SYNTHETIC: DX7HRTNL 0x1000e80c
// TEMPLATE: DX7HRTNL 0x1000e210
// TEMPLATE: DX7HRTNL 0x1000e30f
// TEMPLATE: DX7HRTNL 0x1000e627
// TEMPLATE: DX7HRTNL 0x1000e6de
// TEMPLATE: DX7HRTNL 0x1000e787
