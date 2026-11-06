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

// The pixel formats the GE pixel format numbers stand for, as GXPIXELFORMAT fields after dwSize (.data 0x100166fc,
// stride 0x1c; only this unit reads it) (provisional names).
struct DX7PixelFormat {
    unsigned long mFlags;
    unsigned long mFourCC;
    unsigned long mBitCount;
    unsigned long mRedMask;
    unsigned long mGreenMask;
    unsigned long mBlueMask;
    unsigned long mAlphaMask;
};

DX7PixelFormat gPixelFormats[12] = {
    { GXPF_RGB | GXPF_ALPHAPIXELS, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 },
    { GXPF_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0 },
    { GXPF_RGB | GXPF_ALPHAPIXELS, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000 },
    { GXPF_RGB | GXPF_ALPHAPIXELS, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000 },
    { GXPF_RGB | GXPF_ALPHAPIXELS, 0, 16, 0xf800, 0x07c0, 0x003e, 0x0001 },
    { GXPF_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0 },
    { GXPF_RGB, 0, 16, 0xf800, 0x07c0, 0x003e, 0 },
    { GXPF_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0 },
    { GXPF_FOURCC, GXMAKEFOURCC('D', 'X', 'T', '1'), 0, 0, 0, 0, 0 },
    { GXPF_ZBUFFER, 0, 32, 0, 0xffffffff, 0, 0 },
    { GXPF_ZBUFFER, 0, 24, 0, 0x00ffffff, 0, 0 },
    { GXPF_ZBUFFER, 0, 16, 0, 0x0000ffff, 0, 0 }
};

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
// and registers it; the filter-cap tests use | instead of &, so both surface filters are always reported available
DX7Screen::DX7Screen(void* inMonitor, GXDirectDraw* inDirectDraw, const UString& inName,
                     bool inAttachedToDesktop, bool inSharesPrimary)
    : mMonitor(inMonitor), mAttachedToDesktop(inAttachedToDesktop), mIsFullScreen(false),
      mSharesPrimary(inSharesPrimary), mCanMagFilterSurfaces(false), mCanMinFilterSurfaces(false)
{
    this->mDirectDraw = inDirectDraw;
    this->mName = new UString(inName);
    GXCAPS theCaps;
    memset(&theCaps, 0, sizeof(theCaps));
    theCaps.dwSize = sizeof(theCaps);
    this->mDirectDraw->GetCaps(&theCaps, NULL);
    // Test the reported capability bits (a bitwise or made every test true).
    if (theCaps.dwFXCaps & GXFXCAPS_BLTARITHSTRETCHY) {
        if ((theCaps.dwFXCaps & GXFXCAPS_BLTSHRINKX) && (theCaps.dwFXCaps & GXFXCAPS_BLTSHRINKY))
            this->mCanMinFilterSurfaces = true;
        if ((theCaps.dwFXCaps & GXFXCAPS_BLTSTRETCHX) && (theCaps.dwFXCaps & GXFXCAPS_BLTSTRETCHY))
            this->mCanMagFilterSurfaces = true;
    }
    this->mDirectDraw->EnumDisplayModes(GXEDM_REFRESHRATES, NULL, &this->mModes, DX7Screen::EnumVideoModeCallback);
    GXSURFACEDESC theMode;
    memset(&theMode, 0, sizeof(theMode));
    theMode.dwSize = sizeof(theMode);
    this->mDirectDraw->GetDisplayMode(&theMode);
    for (unsigned long i = 0; i < this->mModes.size(); i++) {
        if (this->mModes[i].mWidth == theMode.dwWidth && this->mModes[i].mHeight == theMode.dwHeight &&
            this->mModes[i].mBitCount == theMode.ddpfPixelFormat.dwRGBBitCount) {
            this->mCurrentModeIndex = i;
            break;
        }
    }
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
    long theResult = kErrorOK;
    HRESULT theError;
    if (inToClipboard) {
        if (DX7Screen::sScreensCaptured == 0) {
            if (this->GetAttachedToDesktop()) {
                theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(), GXSCL_SETFOCUSWINDOW);
                if (theError == GX_OK)
                    theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                                  GXSCL_MULTITHREADED | GXSCL_SETDEVICEWINDOW |
                                                                  GXSCL_EXCLUSIVE | GXSCL_ALLOWREBOOT |
                                                                  GXSCL_FULLSCREEN);
                if (theError == GX_OK) {
                    this->mIsFullScreen = true;
                    DX7Screen::sFirstCaptureWasAttachedToDesktop = true;
                }
            } else {
                theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                              GXSCL_MULTITHREADED | GXSCL_EXCLUSIVE |
                                                              GXSCL_ALLOWREBOOT | GXSCL_FULLSCREEN);
                if (theError == GX_OK) {
                    this->mIsFullScreen = true;
                    DX7Screen::sFirstCaptureWasAttachedToDesktop = false;
                }
            }
        } else if (DX7Screen::sFirstCaptureWasAttachedToDesktop && this->GetAttachedToDesktop()) {
            theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                          GXSCL_MULTITHREADED | GXSCL_CREATEDEVICEWINDOW |
                                                          GXSCL_SETFOCUSWINDOW | GXSCL_EXCLUSIVE | GXSCL_FULLSCREEN);
            if (theError == GX_OK)
                this->mIsFullScreen = true;
        } else
            theResult = 0xd;
        if (theResult == kErrorOK && theError != GX_OK) {
            ESRecordNativeError(theError);
            theResult = kErrorNativeError;
            this->mIsFullScreen = false;
        }
    } else {
        if (DX7Screen::sScreensCaptured == 0) {
            this->mIsFullScreen = false;
            theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                          GXSCL_MULTITHREADED | GXSCL_NORMAL);
            if (theError == GX_OK)
                DX7Screen::sFirstCaptureWasAttachedToDesktop = this->GetAttachedToDesktop();
        } else if (DX7Screen::sFirstCaptureWasAttachedToDesktop && this->GetAttachedToDesktop()) {
            this->mIsFullScreen = false;
            theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                          GXSCL_MULTITHREADED | GXSCL_NORMAL);
        } else
            theResult = 0xd;
        if (theResult == kErrorOK && theError != GX_OK) {
            ESRecordNativeError(theError);
            theResult = kErrorNativeError;
        }
    }
    if (theResult == kErrorOK)
        DX7Screen::sScreensCaptured++;
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
    PlatWindow* theWindow;
    if (gPlatSimMode)   // the headless simulation (platform/PlatSim.h): never shown
        theWindow = PlatWindowCreate(inTitle, (int32_t)inWidth, (int32_t)inHeight, PLAT_WINDOW_HIDDEN | PLAT_WINDOW_BORDERLESS);
    else if (inStyle == 1)
        theWindow = PlatWindowCreate(inTitle, (int32_t)inWidth, (int32_t)inHeight, PLAT_WINDOW_RESIZABLE);
    else if (inStyle == 2)   // port: a hidden window (Test32BitSupport's probe)
        theWindow = PlatWindowCreate(inTitle, (int32_t)inWidth, (int32_t)inHeight, PLAT_WINDOW_HIDDEN | PLAT_WINDOW_BORDERLESS);
    else if ((int32_t)inWidth >= theDisplayWidth && (int32_t)inHeight >= theDisplayHeight)
        theWindow = PlatWindowCreate(inTitle, theDisplayWidth, theDisplayHeight, PLAT_WINDOW_FULLSCREEN);   // the display's own units
    else
        theWindow = PlatWindowCreateAt(inTitle, UMFloatToLong(inOrigin.GetX()), UMFloatToLong(inOrigin.GetY()),
                                       (int32_t)inWidth, (int32_t)inHeight, PLAT_WINDOW_BORDERLESS | PLAT_WINDOW_TOPMOST);
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
    GESurfaceDescription theDescription = inDescription;
    *outSurface = NULL;
    GXSurface* theSurface = NULL;
    GXSurface* theAttachedSurface = NULL;
    long theResult = kErrorOK;
    if (inDescription.mType == kSurfaceZBuffer) {
        if (inDescription.mPixelFormat == kPixelFormatZPrefer16) {
            inDescription.mPixelFormat = kPixelFormatZ16;
            theResult = this->CreateSurface(inDescription, outSurface);
            if (theResult == kErrorOK)
                inDescription.mPixelDepth = 16;
            else {
                inDescription.mPixelFormat = kPixelFormatZ24;
                theResult = this->CreateSurface(inDescription, outSurface);
                if (theResult == kErrorOK)
                    inDescription.mPixelDepth = 24;
                else {
                    inDescription.mPixelFormat = kPixelFormatZ32;
                    theResult = this->CreateSurface(inDescription, outSurface);
                    inDescription.mPixelDepth = 32;
                }
            }
            if (theResult != kErrorOK)
                return theResult;
        } else if (inDescription.mPixelFormat == kPixelFormatZPrefer32) {
            inDescription.mPixelFormat = kPixelFormatZ32;
            theResult = this->CreateSurface(inDescription, outSurface);
            if (theResult == kErrorOK)
                inDescription.mPixelDepth = 32;
            else {
                inDescription.mPixelFormat = kPixelFormatZ24;
                theResult = this->CreateSurface(inDescription, outSurface);
                if (theResult == kErrorOK)
                    inDescription.mPixelDepth = 24;
                else {
                    inDescription.mPixelFormat = kPixelFormatZ16;
                    theResult = this->CreateSurface(inDescription, outSurface);
                    inDescription.mPixelDepth = 16;
                }
            }
            if (theResult != kErrorOK)
                return theResult;
        }
    }
    if (*outSurface != NULL)
        return theResult;
    unsigned long theFlags = inDescription.mIsTransparencyEnabled ? GXSD_CKSRCBLT : 0;
    GXSURFACEDESC theDesc;
    memset(&theDesc, 0, sizeof(theDesc));
    theDesc.dwWidth = inDescription.mWidth;
    theDesc.dwHeight = inDescription.mHeight;
    theDesc.ddpfPixelFormat.dwFlags = gPixelFormats[inDescription.mPixelFormat].mFlags;
    theDesc.ddpfPixelFormat.dwFourCC = gPixelFormats[inDescription.mPixelFormat].mFourCC;
    theDesc.ddpfPixelFormat.dwRGBBitCount = gPixelFormats[inDescription.mPixelFormat].mBitCount;
    theDesc.ddpfPixelFormat.dwRBitMask = gPixelFormats[inDescription.mPixelFormat].mRedMask;
    theDesc.ddpfPixelFormat.dwGBitMask = gPixelFormats[inDescription.mPixelFormat].mGreenMask;
    theDesc.ddpfPixelFormat.dwBBitMask = gPixelFormats[inDescription.mPixelFormat].mBlueMask;
    theDesc.ddpfPixelFormat.dwRGBAlphaBitMask = gPixelFormats[inDescription.mPixelFormat].mAlphaMask;
    theDesc.dwSize = sizeof(theDesc);
    theDesc.ddpfPixelFormat.dwSize = sizeof(theDesc.ddpfPixelFormat);
    switch (inDescription.mType) {
    case kSurfacePrimary:
        theDesc.dwFlags = theFlags | GXSD_CAPS;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            GXSCAPS_PRIMARYSURFACE | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM | GXSCAPS_3DDEVICE :
            GXSCAPS_PRIMARYSURFACE | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM;
        break;
    case kSurfaceOffscreen:
        theDesc.dwFlags = theFlags | GXSD_CAPS | GXSD_HEIGHT | GXSD_WIDTH | GXSD_PIXELFORMAT;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            GXSCAPS_OFFSCREENPLAIN | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM | GXSCAPS_3DDEVICE :
            GXSCAPS_OFFSCREENPLAIN | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM;
        break;
    case kSurfaceFlippingPrimary:
        theDesc.dwFlags = theFlags | GXSD_CAPS | GXSD_BACKBUFFERCOUNT;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP | GXSCAPS_COMPLEX | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM |
                GXSCAPS_3DDEVICE :
            GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP | GXSCAPS_COMPLEX | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM;
        theDesc.dwBackBufferCount = inDescription.m14;
        break;
    case kSurfaceZBuffer:
        theDesc.dwFlags = theFlags | GXSD_CAPS | GXSD_HEIGHT | GXSD_WIDTH | GXSD_PIXELFORMAT;
        theDesc.ddsCaps.dwCaps = GXSCAPS_ZBUFFER | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM;
        break;
    case kSurfaceSystemMemory:
        theDesc.dwFlags = theFlags | GXSD_CAPS | GXSD_HEIGHT | GXSD_WIDTH | GXSD_PIXELFORMAT;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            GXSCAPS_OFFSCREENPLAIN | GXSCAPS_SYSTEMMEMORY | GXSCAPS_3DDEVICE :
            GXSCAPS_OFFSCREENPLAIN | GXSCAPS_SYSTEMMEMORY;
        break;
    case kSurfaceTexture:
        theDesc.dwFlags = theFlags | GXSD_CAPS | GXSD_HEIGHT | GXSD_WIDTH | GXSD_PIXELFORMAT;
        if (inDescription.mIs3DRenderable) {
            theDesc.ddsCaps.dwCaps = GXSCAPS_TEXTURE | GXSCAPS_3DDEVICE | GXSCAPS_VIDEOMEMORY |
                                     GXSCAPS_LOCALVIDMEM;
            theDesc.ddsCaps.dwCaps2 = inDescription.mSurfaceVolatility ? GXSCAPS2_HINTSTATIC :
                                      GXSCAPS2_HINTDYNAMIC;
        } else {
            theDesc.ddsCaps.dwCaps = GXSCAPS_TEXTURE;
            theDesc.ddsCaps.dwCaps2 = inDescription.mSurfaceVolatility ?
                GXSCAPS2_TEXTUREMANAGE | GXSCAPS2_HINTSTATIC :
                GXSCAPS2_TEXTUREMANAGE | GXSCAPS2_HINTDYNAMIC;
        }
        break;
    case kSurfaceMipMappedTexture:
        theDesc.dwFlags = theFlags | GXSD_CAPS | GXSD_HEIGHT | GXSD_WIDTH | GXSD_PIXELFORMAT | GXSD_MIPMAPCOUNT;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            GXSCAPS_TEXTURE | GXSCAPS_COMPLEX | GXSCAPS_MIPMAP | GXSCAPS_3DDEVICE :
            GXSCAPS_TEXTURE | GXSCAPS_COMPLEX | GXSCAPS_MIPMAP;
        theDesc.ddsCaps.dwCaps2 = inDescription.mSurfaceVolatility ?
            GXSCAPS2_TEXTUREMANAGE | GXSCAPS2_HINTSTATIC :
            GXSCAPS2_TEXTUREMANAGE | GXSCAPS2_HINTDYNAMIC;
        theDesc.dwMipMapCount = inDescription.mMipLevelCount;
        break;
    }
    HRESULT theError = this->mDirectDraw->CreateSurface(&theDesc, &theSurface, NULL);
    if (theError == GX_OK) {
        *outSurface = new DX7Surface(theSurface, inDescription, this);
        if (inDescription.mType == kSurfaceFlippingPrimary) {
            inDescription.mType = kSurfaceBackBuffer;
            GXSCAPS theCaps;
            memset(&theCaps, 0, sizeof(theCaps));
            theCaps.dwCaps = GXSCAPS_FLIP;
            DX7Surface* theParent = static_cast<DX7Surface*>(*outSurface);
            for (unsigned long i = 0; i < inDescription.m14; i++) {
                theParent->mSurface->GetAttachedSurface(&theCaps, &theAttachedSurface);
                theParent->mAttachedSurfaces.push_back(new DX7Surface(theAttachedSurface, inDescription, this));
                theParent = static_cast<DX7Surface*>(theParent->mAttachedSurfaces[0]);
            }
        } else if (inDescription.mType == kSurfaceMipMappedTexture) {
            GXSCAPS theCaps;
            memset(&theCaps, 0, sizeof(theCaps));
            DX7Surface* theParent = static_cast<DX7Surface*>(*outSurface);
            theCaps.dwCaps = GXSCAPS_MIPMAP;
            for (unsigned long i = 0; i < inDescription.mMipLevelCount - 1; i++) {
                theParent->mSurface->GetAttachedSurface(&theCaps, &theAttachedSurface);
                theDescription.mWidth >>= 1;
                theDescription.mHeight >>= 1;
                theParent->mAttachedSurfaces.push_back(new DX7Surface(theAttachedSurface, theDescription, this));
                theParent = static_cast<DX7Surface*>(theParent->mAttachedSurfaces[0]);
            }
        }
    } else {
        ESRecordNativeError(theError);
        theResult = kErrorNativeError;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000dc2b
// Texture format enumeration callback: sets the flag and stops when it sees a 32-bit ARGB 8888 format
long __stdcall DX7Screen::EnumTextureFormatCallback(GXPIXELFORMAT* inFormat, void* inContext)
{
    long theResult = GXENUMRET_OK;
    if (inFormat->dwRGBBitCount == 32 && (inFormat->dwRGBAlphaBitMask & 0xff000000) > 0 &&
        (inFormat->dwRBitMask & 0x00ff0000) > 0 && (inFormat->dwGBitMask & 0x0000ff00) > 0 &&
        (inFormat->dwBBitMask & 0x000000ff) > 0) {
        *static_cast<bool*>(inContext) = true;
        theResult = GXENUMRET_CANCEL;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000dc67
// Display mode enumeration callback: adds the mode (size, refresh rate, depth, matching GE pixel format) to the list
long __stdcall DX7Screen::EnumVideoModeCallback(GXSURFACEDESC* inDescription, void* inContext)
{
    GEVideoMode theMode;
    theMode.mWidth = inDescription->dwWidth;
    theMode.mHeight = inDescription->dwHeight;
    theMode.mRefreshRate = inDescription->dwRefreshRate;
    theMode.mBitCount = inDescription->ddpfPixelFormat.dwRGBBitCount;
    theMode.mPixelFormat = kPixelFormatUnknown;
    for (unsigned long i = 0; i < 12; i++) {
        if (gPixelFormats[i].mBitCount == theMode.mBitCount &&
            gPixelFormats[i].mRedMask == inDescription->ddpfPixelFormat.dwRBitMask &&
            gPixelFormats[i].mGreenMask == inDescription->ddpfPixelFormat.dwGBitMask &&
            gPixelFormats[i].mBlueMask == inDescription->ddpfPixelFormat.dwBBitMask &&
            gPixelFormats[i].mAlphaMask == inDescription->ddpfPixelFormat.dwRGBAlphaBitMask) {
            theMode.mPixelFormat = i;
            break;
        }
    }
    static_cast<std::vector<GEVideoMode>*>(inContext)->push_back(theMode);
    return GXENUMRET_OK;
}

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
    long theResult = kErrorOK;
    HRESULT theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(), GXSCL_NORMAL);
    if (theError == GX_OK) {
        this->mIsFullScreen = false;
        DX7Screen::sScreensCaptured--;
    } else {
        ESRecordNativeError(theError);
        theResult = kErrorNativeError;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000de20
// Restores every lost DirectDraw surface; if the device is not ready yet, sleeps 100 ms and returns a native error
long DX7Screen::RestoreAllSurfaces()
{
    long theResult = kErrorOK;
    if (this->mDirectDraw->TestCooperativeLevel() == GX_OK) {
        HRESULT theError = this->mDirectDraw->RestoreAllSurfaces();
        if (theError != GX_OK) {
            ESRecordNativeError(theError);
            theResult = kErrorNativeError;
        }
    } else {
        theResult = kErrorNativeError;
        PlatSleep(100);
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000de5d
// Switches a full-screen screen to the given display mode (no-op when windowed); records the mode index even on failure
long DX7Screen::SetMode(unsigned long inIndex)
{
    long theResult = kErrorOK;
    if (this->mIsFullScreen == true) {
        HRESULT theError = this->mDirectDraw->SetDisplayMode(this->mModes[inIndex].mWidth, this->mModes[inIndex].mHeight,
                                                       gPixelFormats[this->mModes[inIndex].mPixelFormat].mBitCount,
                                                       this->mModes[inIndex].mRefreshRate, 0);
        if (theError == GX_OK)
            this->mCurrentModeIndex = inIndex;
        else {
            ESRecordNativeError(theError);
            theResult = kErrorNativeError;
        }
        this->mCurrentModeIndex = inIndex;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000dec6
// Probes 32-bit rendering and 32-bit texture support with a 16 x 16 test window and a 2 x 2 surface on a HAL device
// (looks up 800 x 600 x 16 without checking it was found)
void DX7Screen::Test32BitSupport()
{
    GESurfaceDescription theDescription;
    GXDirect3D* theDirect3D = NULL;
    GXDevice* theDevice = NULL;
    GESurface* theSurface = NULL;
    GESurface* theTexture = NULL;
    GEWindow* theWindow = NULL;
    GE2DPoint theOrigin(0.0f, 0.0f);
    UString theClassName("TestWindowClass");
    UString theTitle("");
#ifdef __APPLE__
    // port: GX renders and samples 32-bit everywhere; a probe window, even hidden, took the key window status from the
    // full screen game window at every renderer activation and macOS took it out of full screen
    this->m32BitDeviceSupport = true;
    this->m32BitTextureSupport = true;
    return;
#endif
    // port: style 2, a hidden probe window (a visible one took the focus from the full screen game window, which
    // left full screen or minimized at every renderer activation)
    long theResult = this->CreateAWindow(theOrigin, 16, 16, theClassName, theTitle, theWindow, 2, NULL);
    if (theResult == kErrorOK) {
        if (this->Capture(theWindow, false) == kErrorOK) {
            unsigned long i;
            for (i = 0; i < this->GetModeCount(); i++) {
                if (this->GetMode(i).mWidth == 800 && this->GetMode(i).mHeight == 600 && this->GetMode(i).mBitCount == 16)
                    break;
            }
            theDescription.mWidth = 2;
            theDescription.mHeight = 2;
            theDescription.mType = kSurfaceOffscreen;
            theDescription.mPixelFormat = this->GetMode(i).mPixelFormat;
            theDescription.mPixelDepth = this->GetMode(i).mBitCount;
            theDescription.mSurfaceVolatility = 0;
            theDescription.m14 = 0;
            theDescription.mMipLevelCount = 0;
            theDescription.mIs3DRenderable = true;
            theDescription.mIsTransparencyEnabled = false;
            if (this->CreateSurface(theDescription, &theSurface) == kErrorOK && theSurface != NULL) {
                DX7Surface* theTarget = dynamic_cast<DX7Surface*>(theSurface);
                if (this->mDirectDraw->QueryInterface(GXIID_Direct3D7, (void**)&theDirect3D) == GX_OK) {
                    if (theDirect3D->CreateDevice(GXIID_HALDevice, *theTarget, &theDevice) == GX_OK) {
                        GXDEVICEDESC theCaps;
                        theDevice->GetCaps(&theCaps);
                        this->m32BitDeviceSupport = (theCaps.dwDeviceRenderBitDepth & GXBD_32) > 0;
                        this->m32BitTextureSupport = false;
                        theDevice->EnumTextureFormats(DX7Screen::EnumTextureFormatCallback, &this->m32BitTextureSupport);
                        if (this->m32BitTextureSupport) {
                            theDescription.mWidth = 16;
                            theDescription.mHeight = 16;
                            theDescription.mPixelDepth = 32;
                            theDescription.mPixelFormat = kPixelFormatA8R8G8B8;
                            theDescription.mType = kSurfaceTexture;
                            theDescription.mSurfaceVolatility = 1;
                            theDescription.m14 = 0;
                            theDescription.mMipLevelCount = 0;
                            theDescription.mIs3DRenderable = false;
                            theDescription.mIsTransparencyEnabled = false;
                            if (this->CreateSurface(theDescription, &theTexture) == kErrorOK && theTexture != NULL)
                                theTexture->Dereference();
                            else
                                this->m32BitTextureSupport = false;
                        }
                        theDevice->Release();
                    } else {
                        this->m32BitDeviceSupport = false;
                        this->m32BitTextureSupport = false;
                    }
                    theDirect3D->Release();
                } else {
                    this->m32BitDeviceSupport = false;
                    this->m32BitTextureSupport = false;
                }
                theSurface->Dereference();
            } else {
                this->m32BitDeviceSupport = false;
                this->m32BitTextureSupport = false;
            }
            this->Release(theWindow);
        } else {
            this->m32BitDeviceSupport = false;
            this->m32BitTextureSupport = false;
        }
        delete theWindow;
        // the activation the probe's destruction gives back to the game window (Win32 sent it during DestroyWindow:
        // the game centres its pointer on it)
        TSThread::DispatchWindowEvents();
    } else {
        this->m32BitDeviceSupport = false;
        this->m32BitTextureSupport = false;
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
