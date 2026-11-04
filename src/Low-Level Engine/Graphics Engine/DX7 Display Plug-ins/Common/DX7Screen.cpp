// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Screen.cpp (file name inferred: no
// __FILE__; class name from the exports)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x1000d0fc..0x1000e793: the U2D key init chains, sScreens' init chain (0x1000d228, guarded dtor 0x1000d256), the
// ctor 0x1000d271, UString's scalar deleting dtor (new UString in the ctor), the dtor, the members in alphabetical
// order, then the vector<DX7Screen*>/vector<GEVideoMode>/vector<GESurface*> COMDATs and the template-static init
// chain 0x1000e751.
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

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
#include "dx7/DX7Multimon.h"
#include "lle/GERenderState.h"

// The pixel formats the GE pixel format numbers stand for, as DDPIXELFORMAT fields after dwSize (.data 0x100166fc,
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
    { DDPF_RGB | DDPF_ALPHAPIXELS, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 },
    { DDPF_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0 },
    { DDPF_RGB | DDPF_ALPHAPIXELS, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000 },
    { DDPF_RGB | DDPF_ALPHAPIXELS, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000 },
    { DDPF_RGB | DDPF_ALPHAPIXELS, 0, 16, 0xf800, 0x07c0, 0x003e, 0x0001 },
    { DDPF_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0 },
    { DDPF_RGB, 0, 16, 0xf800, 0x07c0, 0x003e, 0 },
    { DDPF_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0 },
    { DDPF_FOURCC, MAKEFOURCC('D', 'X', 'T', '1'), 0, 0, 0, 0, 0 },
    { DDPF_ZBUFFER, 0, 32, 0, 0xffffffff, 0, 0 },
    { DDPF_ZBUFFER, 0, 24, 0, 0x00ffffff, 0, 0 },
    { DDPF_ZBUFFER, 0, 16, 0, 0x0000ffff, 0, 0 }
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
DX7Screen::DX7Screen(HMONITOR__* inMonitor, IDirectDraw7* inDirectDraw, const UString& inName,
                     bool inAttachedToDesktop, bool inSharesPrimary)
    : mMonitor(inMonitor), mAttachedToDesktop(inAttachedToDesktop), mIsFullScreen(false),
      mSharesPrimary(inSharesPrimary), mCanMagFilterSurfaces(false), mCanMinFilterSurfaces(false)
{
    this->mDirectDraw = inDirectDraw;
    this->mName = new UString(inName);
    DDCAPS theCaps;
    memset(&theCaps, 0, sizeof(theCaps));
    theCaps.dwSize = sizeof(theCaps);
    this->mDirectDraw->GetCaps(&theCaps, NULL);
    // Test the reported capability bits (a bitwise or made every test true).
    if (theCaps.dwFXCaps & DDFXCAPS_BLTARITHSTRETCHY) {
        if ((theCaps.dwFXCaps & DDFXCAPS_BLTSHRINKX) && (theCaps.dwFXCaps & DDFXCAPS_BLTSHRINKY))
            this->mCanMinFilterSurfaces = true;
        if ((theCaps.dwFXCaps & DDFXCAPS_BLTSTRETCHX) && (theCaps.dwFXCaps & DDFXCAPS_BLTSTRETCHY))
            this->mCanMagFilterSurfaces = true;
    }
    this->mDirectDraw->EnumDisplayModes(DDEDM_REFRESHRATES, NULL, &this->mModes, (LPDDENUMMODESCALLBACK2)DX7Screen::EnumVideoModeCallback);
    DDSURFACEDESC2 theMode;
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
                theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(), DDSCL_SETFOCUSWINDOW);
                if (theError == DD_OK)
                    theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                                  DDSCL_MULTITHREADED | DDSCL_SETDEVICEWINDOW |
                                                                  DDSCL_EXCLUSIVE | DDSCL_ALLOWREBOOT |
                                                                  DDSCL_FULLSCREEN);
                if (theError == DD_OK) {
                    this->mIsFullScreen = true;
                    DX7Screen::sFirstCaptureWasAttachedToDesktop = true;
                }
            } else {
                theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                              DDSCL_MULTITHREADED | DDSCL_EXCLUSIVE |
                                                              DDSCL_ALLOWREBOOT | DDSCL_FULLSCREEN);
                if (theError == DD_OK) {
                    this->mIsFullScreen = true;
                    DX7Screen::sFirstCaptureWasAttachedToDesktop = false;
                }
            }
        } else if (DX7Screen::sFirstCaptureWasAttachedToDesktop && this->GetAttachedToDesktop()) {
            theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                          DDSCL_MULTITHREADED | DDSCL_CREATEDEVICEWINDOW |
                                                          DDSCL_SETFOCUSWINDOW | DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
            if (theError == DD_OK)
                this->mIsFullScreen = true;
        } else
            theResult = 0xd;
        if (theResult == kErrorOK && theError != DD_OK) {
            ESRecordNativeError(theError);
            theResult = kErrorNativeError;
            this->mIsFullScreen = false;
        }
    } else {
        if (DX7Screen::sScreensCaptured == 0) {
            this->mIsFullScreen = false;
            theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                          DDSCL_MULTITHREADED | DDSCL_NORMAL);
            if (theError == DD_OK)
                DX7Screen::sFirstCaptureWasAttachedToDesktop = this->GetAttachedToDesktop();
        } else if (DX7Screen::sFirstCaptureWasAttachedToDesktop && this->GetAttachedToDesktop()) {
            this->mIsFullScreen = false;
            theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(),
                                                          DDSCL_MULTITHREADED | DDSCL_NORMAL);
        } else
            theResult = 0xd;
        if (theResult == kErrorOK && theError != DD_OK) {
            ESRecordNativeError(theError);
            theResult = kErrorNativeError;
        }
    }
    if (theResult == kErrorOK)
        DX7Screen::sScreensCaptured++;
    return theResult;
}

// FUNCTION: DX7HR 0x1000d622
// Creates a window: style 1 is an overlapped window sized to its client area, else a topmost popup (child of inParent)
long DX7Screen::CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                              UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                              GEWindow* inParent)
{
    long theResult = kErrorOK;
    HWND theParent = NULL;
    unsigned long theWindowStyle = WS_POPUP | WS_VISIBLE;
    bool theHasMenu = false;
    HWND theWindow;
    if (inStyle == 1) {
        WNDCLASSEX theClass;
        theClass.cbSize = sizeof(theClass);
        GetClassInfoEx(GEScreen::GetInstance(), inClassName, &theClass);
        if (theClass.lpszMenuName != NULL)
            theHasMenu = true;
        RECT theRect;
        theRect.left = UMFloatToLong(inOrigin.GetX());
        theRect.top = UMFloatToLong(inOrigin.GetY());
        theRect.right = UMFloatToLong(inOrigin.GetX() + inWidth);
        theRect.bottom = UMFloatToLong(inOrigin.GetY() + inHeight);
        AdjustWindowRect(&theRect, WS_OVERLAPPEDWINDOW, theHasMenu);
        theWindow = CreateWindowEx(0, inClassName, inTitle, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                   UMFloatToLong(inOrigin.GetX()), UMFloatToLong(inOrigin.GetY()),
                                   theRect.right - theRect.left, theRect.bottom - theRect.top, NULL, NULL,
                                   GEScreen::GetInstance(), NULL);
    } else {
        if (inParent != NULL) {
            theParent = inParent->mWindow;
            theWindowStyle = WS_POPUP | WS_VISIBLE | WS_CHILD;
        }
        theWindow = CreateWindowEx(WS_EX_TOPMOST, inClassName, inTitle, theWindowStyle,
                                   UMFloatToLong(inOrigin.GetX()), UMFloatToLong(inOrigin.GetY()), inWidth, inHeight,
                                   theParent, NULL, GEScreen::GetInstance(), NULL);
    }
    if (theWindow != NULL)
        outWindow = new GEWindow(theWindow, inWidth, inHeight, inStyle, true);
    else {
        ESRecordNativeError(GetLastError());
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
    IDirectDrawSurface7* theSurface = NULL;
    IDirectDrawSurface7* theAttachedSurface = NULL;
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
    unsigned long theFlags = inDescription.mIsTransparencyEnabled ? DDSD_CKSRCBLT : 0;
    DDSURFACEDESC2 theDesc;
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
        theDesc.dwFlags = theFlags | DDSD_CAPS;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM | DDSCAPS_3DDEVICE :
            DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
        break;
    case kSurfaceOffscreen:
        theDesc.dwFlags = theFlags | DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM | DDSCAPS_3DDEVICE :
            DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
        break;
    case kSurfaceFlippingPrimary:
        theDesc.dwFlags = theFlags | DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM |
                DDSCAPS_3DDEVICE :
            DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
        theDesc.dwBackBufferCount = inDescription.m14;
        break;
    case kSurfaceZBuffer:
        theDesc.dwFlags = theFlags | DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
        theDesc.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
        break;
    case kSurfaceSystemMemory:
        theDesc.dwFlags = theFlags | DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY | DDSCAPS_3DDEVICE :
            DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        break;
    case kSurfaceTexture:
        theDesc.dwFlags = theFlags | DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
        if (inDescription.mIs3DRenderable) {
            theDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY |
                                     DDSCAPS_LOCALVIDMEM;
            theDesc.ddsCaps.dwCaps2 = inDescription.mSurfaceVolatility ? DDSCAPS2_HINTSTATIC :
                                      DDSCAPS2_HINTDYNAMIC;
        } else {
            theDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
            theDesc.ddsCaps.dwCaps2 = inDescription.mSurfaceVolatility ?
                DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTSTATIC :
                DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTDYNAMIC;
        }
        break;
    case kSurfaceMipMappedTexture:
        theDesc.dwFlags = theFlags | DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;
        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?
            DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP | DDSCAPS_3DDEVICE :
            DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;
        theDesc.ddsCaps.dwCaps2 = inDescription.mSurfaceVolatility ?
            DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTSTATIC :
            DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTDYNAMIC;
        theDesc.dwMipMapCount = inDescription.mMipLevelCount;
        break;
    }
    HRESULT theError = this->mDirectDraw->CreateSurface(&theDesc, &theSurface, NULL);
    if (theError == DD_OK) {
        *outSurface = new DX7Surface(theSurface, inDescription, this);
        if (inDescription.mType == kSurfaceFlippingPrimary) {
            inDescription.mType = kSurfaceBackBuffer;
            DDSCAPS2 theCaps;
            memset(&theCaps, 0, sizeof(theCaps));
            theCaps.dwCaps = DDSCAPS_FLIP;
            DX7Surface* theParent = static_cast<DX7Surface*>(*outSurface);
            for (unsigned long i = 0; i < inDescription.m14; i++) {
                theParent->mSurface->GetAttachedSurface(&theCaps, &theAttachedSurface);
                theParent->mAttachedSurfaces.push_back(new DX7Surface(theAttachedSurface, inDescription, this));
                theParent = static_cast<DX7Surface*>(theParent->mAttachedSurfaces[0]);
            }
        } else if (inDescription.mType == kSurfaceMipMappedTexture) {
            DDSCAPS2 theCaps;
            memset(&theCaps, 0, sizeof(theCaps));
            DX7Surface* theParent = static_cast<DX7Surface*>(*outSurface);
            theCaps.dwCaps = DDSCAPS_MIPMAP;
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
long __stdcall DX7Screen::EnumTextureFormatCallback(_DDPIXELFORMAT* inFormat, void* inContext)
{
    long theResult = DDENUMRET_OK;
    if (inFormat->dwRGBBitCount == 32 && (inFormat->dwRGBAlphaBitMask & 0xff000000) > 0 &&
        (inFormat->dwRBitMask & 0x00ff0000) > 0 && (inFormat->dwGBitMask & 0x0000ff00) > 0 &&
        (inFormat->dwBBitMask & 0x000000ff) > 0) {
        *static_cast<bool*>(inContext) = true;
        theResult = DDENUMRET_CANCEL;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000dc67
// Display mode enumeration callback: adds the mode (size, refresh rate, depth, matching GE pixel format) to the list
int __stdcall DX7Screen::EnumVideoModeCallback(_DDSURFACEDESC2* inDescription, void* inContext)
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
    return DDENUMRET_OK;
}

// FUNCTION: DX7HR 0x1000dce2
// Returns the screen's desktop rectangle: the monitor rectangle where available, else 0,0 to the current mode's size
// (on other OS types it is left unchanged)
const GERectangle& DX7Screen::GetDesktopRectangle()
{
    switch (UGetOSType()) {
    case kOSWindows95:
        this->mDesktopRectangle.SetTop(0.0f);
        this->mDesktopRectangle.SetLeft(0.0f);
        this->mDesktopRectangle.SetRight((float)this->mModes[this->mCurrentModeIndex].mWidth);
        this->mDesktopRectangle.SetBottom((float)this->mModes[this->mCurrentModeIndex].mHeight);
        break;
    case kOSWindows98:
    case kOSWindowsNT5:
        MONITORINFO theInfo;
        theInfo.cbSize = sizeof(theInfo);
        if (DX7GetMonitorInfo(this->mMonitor, &theInfo)) {
            GERectangle theRectangle(theInfo.rcMonitor);
            this->mDesktopRectangle = theRectangle;
        }
        else {
            this->mDesktopRectangle.SetTop(0.0f);
            this->mDesktopRectangle.SetLeft(0.0f);
            this->mDesktopRectangle.SetRight((float)this->mModes[this->mCurrentModeIndex].mWidth);
            this->mDesktopRectangle.SetBottom((float)this->mModes[this->mCurrentModeIndex].mHeight);
        }
        break;
    }
    return this->mDesktopRectangle;
}

// FUNCTION: DX7HR 0x1000dde7
// Returns the screen to normal cooperative level for the window and uncounts the capture; native error on failure
long DX7Screen::Release(const GEWindow* inWindow)
{
    long theResult = kErrorOK;
    HRESULT theError = this->mDirectDraw->SetCooperativeLevel(inWindow->GetOSRepresentation(), DDSCL_NORMAL);
    if (theError == DD_OK) {
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
    if (this->mDirectDraw->TestCooperativeLevel() == DD_OK) {
        HRESULT theError = this->mDirectDraw->RestoreAllSurfaces();
        if (theError != DD_OK) {
            ESRecordNativeError(theError);
            theResult = kErrorNativeError;
        }
    } else {
        theResult = kErrorNativeError;
        Sleep(100);
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
        if (theError == DD_OK)
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
    IDirect3D7* theDirect3D = NULL;
    IDirect3DDevice7* theDevice = NULL;
    GESurface* theSurface = NULL;
    GESurface* theTexture = NULL;
    GEWindow* theWindow = NULL;
    WNDCLASSEX theClass;
    theClass.cbSize = sizeof(theClass);
    theClass.style = CS_HREDRAW | CS_VREDRAW;
    theClass.lpfnWndProc = DX7Screen::TestWindowProc;
    theClass.cbClsExtra = 0;
    theClass.cbWndExtra = 0;
    theClass.hInstance = GEScreen::GetInstance();
    theClass.hIcon = NULL;
    theClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    theClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    theClass.lpszMenuName = NULL;
    theClass.lpszClassName = "TestWindowClass";
    theClass.hIconSm = NULL;
    RegisterClassEx(&theClass);
    GE2DPoint theOrigin(0.0f, 0.0f);
    long theResult = this->CreateAWindow(theOrigin, 16, 16, UString("TestWindowClass"), UString(""), theWindow, 0, NULL);
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
                if (this->mDirectDraw->QueryInterface(IID_IDirect3D7, (void**)&theDirect3D) == DD_OK) {
                    if (theDirect3D->CreateDevice(IID_IDirect3DHALDevice, *theTarget, &theDevice) == D3D_OK) {
                        D3DDEVICEDESC7 theCaps;
                        theDevice->GetCaps(&theCaps);
                        this->m32BitDeviceSupport = (theCaps.dwDeviceRenderBitDepth & DDBD_32) > 0;
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
    } else {
        this->m32BitDeviceSupport = false;
        this->m32BitTextureSupport = false;
    }
    UnregisterClass("TestWindowClass", GEScreen::GetInstance());
}

// FUNCTION: DX7HR 0x1000e1af
// Window procedure of the 32-bit probe window: default handling only
long __stdcall DX7Screen::TestWindowProc(HWND__* inWindow, unsigned int inMessage, unsigned int inWParam,
                                         long inLParam)
{
    return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
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
