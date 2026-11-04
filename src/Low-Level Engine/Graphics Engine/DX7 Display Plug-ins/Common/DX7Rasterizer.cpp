// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer.cpp
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x100091aa..0x1000d0fc: the U2D key init chains, the DirectDraw/Direct3D enumeration callbacks, DX7Rasterizer's
// ctor and dtor, then its members in alphabetical order, then the vector<DX7RScreenState*> COMDATs and the
// template-static init chain head. The enumeration callbacks and ActivateRasterizer were built without global
// optimisation (see the pragmas below); everything else is -O1.
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "dx7/DX7Movie.h"
#include "dx7/DX7Multimon.h"
#include "LostDebug.h"  // NOT original source: EE_LOST_DEBUG filler (SetZBuffer; docs/LOST_DEBUG.md)

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

// The rasterizer whose ActivateRasterizer is enumerating the DirectDraw devices (the callbacks add the screens to it)
// and the D3D light indices in use, shared by every screen (provisional names).
DX7Rasterizer* gEnumeratingRasterizer;
unsigned long gLightsInUse[8];

// Owner decision (2026-10-23, CLAUDE.md rule 4): the three enumeration callbacks below and ActivateRasterizer were
// built without global optimisation while the rest of the unit is -O1: memory-resident locals (`mov [ebp-4], eax`
// then `cmp [ebp-4], 1`), the copies through a temporary for `?:`, `leave`, no jump threading; the ctor/dtor between
// them and ActivateRenderingSurface after them are ordinary -O1. Without the pragma: 0x10009308 4.4, 0x1000934b 22.1,
// 0x10009542 25.2, ActivateRasterizer 26.5; with it all four are exact.
#pragma optimize("g", off)

// Without /Og VC6 gives the locals frame slots in the order of a hash of their names (mod 16 buckets; later
// declarations first within a bucket), so the names below were chosen to reproduce the original's frame layout
// (provisional names, like every local name).
// FUNCTION: DX7HR 0x10009308
// Direct3D device enumeration callback: sets the flag and stops at the first hardware-rasterizing device
HRESULT CALLBACK DX7EnumDevicesCallback(LPSTR inDescription, LPSTR inName, LPD3DDEVICEDESC7 inCaps, LPVOID inContext)
{
    bool* theContext = (bool*)inContext;
    HRESULT theResult = D3DENUMRET_OK;
    bool theFlag = (inCaps->dwDevCaps & D3DDEVCAPS_HWRASTERIZATION) == D3DDEVCAPS_HWRASTERIZATION;
    if (theFlag) {
        *theContext = true;
        theResult = D3DENUMRET_CANCEL;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000934b
// DirectDraw enumeration callback (Windows 95): creates a screen for each 3D-capable device and keeps
// those with a hardware rasterizer
BOOL WINAPI DX7DirectDrawEnumerateCallback(GUID* inGUID, LPSTR inDescription, LPSTR inName, LPVOID inContext)
{
    unsigned long* theCounter = (unsigned long*)inContext;
    DX7Screen* theScreen;
    bool theIsAttached = inGUID == NULL;
    IDirectDraw7* theDDraw;
    bool theIsPrimaryDevice = inGUID == NULL;
    HRESULT theStatus;
    DDCAPS theCaps;
    (*theCounter)++;
    theStatus = DirectDrawCreateEx(inGUID, (void**)&theDDraw, IID_IDirectDraw7, NULL);
    if (theStatus == DD_OK) {
        theCaps.dwSize = sizeof(theCaps);
        theStatus = theDDraw->GetCaps(&theCaps, NULL);
        if (theStatus == DD_OK) {
            if ((theCaps.dwCaps & DDCAPS_3D) == DDCAPS_3D) {
                theScreen = new DX7Screen(NULL, theDDraw, inDescription, theIsAttached, theIsPrimaryDevice);
                // Keep a screen with a hardware rasterizer, as the Ex callback does (the test was inverted).
                if (gEnumeratingRasterizer->GetIsScreenValid(theScreen))
                    gEnumeratingRasterizer->AddValidScreen(theScreen);
                else
                    delete theScreen;
            } else
                theDDraw->Release();
        } else {
            ESRecordNativeError(theStatus);
            theDDraw->Release();
        }
    } else
        ESRecordNativeError(theStatus);
    return TRUE;
}

// FUNCTION: DX7HR 0x10009542
// DirectDraw enumeration callback (Windows 98/2000): creates a screen for each 3D-capable device, primary from the
// monitor flags, and adds it when it has a hardware rasterizer (else deletes it)
BOOL WINAPI DX7DirectDrawEnumerateExCallback(GUID* inGUID, LPSTR inDescription, LPSTR inName, LPVOID inContext,
                                             HMONITOR inMonitor)
{
    unsigned long* theCounter = (unsigned long*)inContext;
    DX7Screen* theScreen;
    bool theIsAttached = (inGUID == NULL && inMonitor == NULL) || inMonitor != NULL ? TRUE : FALSE;
    IDirectDraw7* theDDraw;
    bool theIsPrimaryDevice;
    MONITORINFO theInfo;
    HRESULT theStatus;
    DDCAPS theCaps;
    theInfo.cbSize = sizeof(theInfo);
    if (DX7GetMonitorInfo(inMonitor, &theInfo))
        theIsPrimaryDevice = (theInfo.dwFlags & MONITORINFOF_PRIMARY) == MONITORINFOF_PRIMARY;
    else
        theIsPrimaryDevice = inGUID == NULL;
    (*theCounter)++;
    theStatus = DirectDrawCreateEx(inGUID, (void**)&theDDraw, IID_IDirectDraw7, NULL);
    if (theStatus == DD_OK) {
        theCaps.dwSize = sizeof(theCaps);
        theStatus = theDDraw->GetCaps(&theCaps, NULL);
        if (theStatus == DD_OK) {
            if ((theCaps.dwCaps & DDCAPS_3D) == DDCAPS_3D) {
                theScreen = new DX7Screen(NULL, theDDraw, inDescription, theIsAttached, theIsPrimaryDevice);
                if (gEnumeratingRasterizer->GetIsScreenValid(theScreen))
                    gEnumeratingRasterizer->AddValidScreen(theScreen);
                else
                    delete theScreen;
            } else
                theDDraw->Release();
        } else {
            ESRecordNativeError(theStatus);
            theDDraw->Release();
        }
    } else
        ESRecordNativeError(theStatus);
    return TRUE;
}

#pragma optimize("g", on)

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

#pragma optimize("g", off)

// FUNCTION: DX7HR 0x100097d8
// Enumerates the DirectDraw devices into screens (Ex enumeration on 98/2000) and makes this the active rasterizer
long DX7Rasterizer::ActivateRasterizer()
{
    long theResult;
    HRESULT theStatus = DD_OK;
    unsigned long theNumberOfScreens = 0;
    unsigned long theOSType = UGetOSType();
    DX7InitializeMultimon(theOSType);
    gEnumeratingRasterizer = this;
    switch (theOSType) {
    case 1:
        theStatus = DirectDrawEnumerate(DX7DirectDrawEnumerateCallback, &theNumberOfScreens);
        break;
    case 2:
    case 3:
        theStatus = DirectDrawEnumerateEx(DX7DirectDrawEnumerateExCallback, &theNumberOfScreens,
                                         DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES |
                                         DDENUM_NONDISPLAYDEVICES);
        break;
    }
    gEnumeratingRasterizer = NULL;
    if (theStatus == DD_OK) {
        GERasterizer::sActiveRasterizer = this;
        theResult = kErrorOK;
    } else {
        ESRecordNativeError(theStatus);
        theResult = kErrorNativeError;
    }
    return theResult;
}

#pragma optimize("g", on)

// FUNCTION: DX7HR 0x10009876
// Sets up a screen to render into an offscreen surface of the given size at the desktop depth, retrying with a
// 16-bit mode if that fails
long DX7Rasterizer::ActivateRenderingSurface(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                             unsigned long inWidth, unsigned long inHeight)
{
    GESurfaceDescription theDescription;
    GESurface* theSurface = NULL;
    long theResult;
    GERScreenState* theState = this->mScreenStates[inScreen];
    GEScreen* theScreen = theState->mScreen;
    GEVideoMode theMode = theScreen->GetCurrentMode();
    unsigned long theVideoModeIndex;
    for (theVideoModeIndex = 0; theVideoModeIndex < theScreen->GetModeCount(); theVideoModeIndex++)
        if (theScreen->GetMode(theVideoModeIndex).mBitCount == theMode.mBitCount)
            break;
    ESVERIFY_AT(theVideoModeIndex < theScreen->GetModeCount(), 390);
    theMode = theScreen->GetMode(theVideoModeIndex);
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mWidth = inWidth;
    theDescription.mHeight = inHeight;
    theDescription.mPixelFormat = theMode.mPixelFormat;
    theDescription.mType = kSurfaceOffscreen;
    theDescription.mPixelDepth = theMode.mBitCount;
    theDescription.mSurfaceVolatility = 0;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mIs3DRenderable = true;
    bool theActivated = false;
    if (theScreen->CreateSurface(theDescription, &theSurface) == kErrorOK) {
        theResult = this->ActivateTheScreen(inScreen, theSurface, theSurface, inManager, inWindow);
        if (theResult == kErrorOK) {
            theScreen->SetTextureManager(inManager);
            inManager->SetScreen(theScreen);
            theState->mPrimarySurface = theSurface;
            theState->mTexture = NULL;
            theState->mRenderTarget = NULL;
            theState->mSecondarySurface = theSurface;
            theState->mWindow = inWindow;
            theState->mIsScreenActivated = true;
            this->SetRenderTarget(theSurface, inScreen);
            theSurface->Reference();
            theActivated = true;
        } else
            theSurface->Dereference();
    }
    if (!theActivated) {
        // the desktop depth failed: retry with a 16-bit mode
        for (theVideoModeIndex = 0; theVideoModeIndex < theScreen->GetModeCount(); theVideoModeIndex++)
            if (theScreen->GetMode(theVideoModeIndex).mBitCount == 16)
                break;
        ESVERIFY_AT(theVideoModeIndex < theScreen->GetModeCount(), 459);
        theMode = theScreen->GetMode(theVideoModeIndex);
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mWidth = inWidth;
        theDescription.mHeight = inHeight;
        theDescription.mPixelFormat = theMode.mPixelFormat;
        theDescription.mType = kSurfaceOffscreen;
        theDescription.mPixelDepth = theMode.mBitCount;
        theDescription.mSurfaceVolatility = 0;
        theDescription.m14 = 0;
        theDescription.mMipLevelCount = 0;
        theDescription.mIs3DRenderable = true;
        theResult = theScreen->CreateSurface(theDescription, &theSurface);
        if (theResult == kErrorOK) {
            theResult = this->ActivateTheScreen(inScreen, theSurface, theSurface, inManager, inWindow);
            if (theResult == kErrorOK) {
                theScreen->SetTextureManager(inManager);
                inManager->SetScreen(theScreen);
                theState->mPrimarySurface = theSurface;
                theState->mTexture = NULL;
                theState->mRenderTarget = NULL;
                theState->mSecondarySurface = theSurface;
                theState->mWindow = inWindow;
                theState->mIsScreenActivated = true;
                this->SetRenderTarget(theSurface, inScreen);
                theSurface->Reference();
            } else
                theSurface->Dereference();
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x10009ab4
// Sets up a screen's primary, back buffer and optional Z-buffer (flipping chain full screen, offscreen buffer
// windowed), activates Direct3D on them and forces every render state to its default
long DX7Rasterizer::ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                   unsigned long inBackBuffers, bool inZBuffer, unsigned long inZBufferFormat)
{
    GESurfaceDescription theDescription;
    bool theCreatedSecondary = false;
    GESurface* thePrimary = NULL;
    GESurface* theSecondary = NULL;
    GESurface* theZBuffer = NULL;
    long theResult;
    GERScreenState* theState = this->mScreenStates[inScreen];
    GEScreen* theScreen = theState->mScreen;
    GEVideoMode theMode = theScreen->GetCurrentMode();
    if (theScreen->GetIsFullScreen()) {
        theDescription.mType = inBackBuffers > 0 ? 2 : 0;
        theDescription.mSurfaceVolatility = 0;
        theDescription.mMipLevelCount = 0;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mHeight = theMode.mHeight;
        theDescription.mPixelFormat = theMode.mPixelFormat;
        theDescription.mPixelDepth = theMode.mBitCount;
        theDescription.mWidth = theMode.mWidth;
        theDescription.m14 = inBackBuffers;
        theDescription.mIs3DRenderable = true;
        theResult = theScreen->CreateSurface(theDescription, &thePrimary);
        if (inBackBuffers > 0)
            theSecondary = thePrimary->GetAttachedSurface(0);
        if (inZBuffer) {
            theDescription.mIsTransparencyEnabled = false;
            theDescription.mSurfaceVolatility = 0;
            theDescription.m14 = 0;
            theDescription.mMipLevelCount = 0;
            theDescription.mHeight = theMode.mHeight;
            theDescription.mWidth = theMode.mWidth;
            theDescription.mType = kSurfaceZBuffer;
            theDescription.mPixelFormat = inZBufferFormat;
            theDescription.mIs3DRenderable = true;
            theDescription.mPixelDepth = inZBufferFormat == kPixelFormatZ32 ? 32 : 16;
            theScreen->CreateSurface(theDescription, &theZBuffer);
            if (theSecondary != NULL)
                theResult = theSecondary->AttachSurface(theZBuffer);
            else
                theResult = thePrimary->AttachSurface(theZBuffer);
            theZBuffer->Dereference();
        }
    } else {
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mWidth = theMode.mWidth;
        theDescription.mHeight = theMode.mHeight;
        theDescription.mPixelFormat = theMode.mPixelFormat;
        theDescription.mPixelDepth = theMode.mBitCount;
        theDescription.mType = kSurfacePrimary;
        theDescription.mSurfaceVolatility = 0;
        theDescription.m14 = 0;
        theDescription.mMipLevelCount = 0;
        theDescription.mIs3DRenderable = true;
        theResult = theScreen->CreateSurface(theDescription, &thePrimary);
        if (inBackBuffers == 1) {
            theDescription.mIsTransparencyEnabled = false;
            theDescription.mWidth = inWindow->mWidth;
            theDescription.mHeight = inWindow->mHeight;
            theDescription.mType = kSurfaceOffscreen;
            theDescription.mPixelFormat = theMode.mPixelFormat;
            theDescription.mPixelDepth = theMode.mBitCount;
            theDescription.mSurfaceVolatility = 0;
            theDescription.m14 = 0;
            theDescription.mMipLevelCount = 0;
            theDescription.mIs3DRenderable = true;
            theResult = theScreen->CreateSurface(theDescription, &theSecondary);
            theCreatedSecondary = true;
        }
        if (inZBuffer) {
            if (theSecondary != NULL) {
                theDescription.mIsTransparencyEnabled = false;
                theDescription.mWidth = inWindow->mWidth;
                theDescription.mHeight = inWindow->mHeight;
                theDescription.mType = kSurfaceZBuffer;
                theDescription.mPixelFormat = inZBufferFormat;
                theDescription.mSurfaceVolatility = 0;
                theDescription.m14 = 0;
                theDescription.mMipLevelCount = 0;
                theDescription.mIs3DRenderable = true;
                theDescription.mPixelDepth = inZBufferFormat == kPixelFormatZ32 ? 32 : 16;
                theScreen->CreateSurface(theDescription, &theZBuffer);
                theResult = theSecondary->AttachSurface(theZBuffer);
            } else {
                theDescription.mIsTransparencyEnabled = false;
                theDescription.mWidth = theMode.mWidth;
                theDescription.mHeight = theMode.mHeight;
                theDescription.mType = kSurfaceZBuffer;
                theDescription.mPixelFormat = inZBufferFormat;
                theDescription.mSurfaceVolatility = 0;
                theDescription.m14 = 0;
                theDescription.mMipLevelCount = 0;
                theDescription.mIs3DRenderable = true;
                theDescription.mPixelDepth = inZBufferFormat == kPixelFormatZ32 ? 32 : 16;
                theScreen->CreateSurface(theDescription, &theZBuffer);
                theResult = thePrimary->AttachSurface(theZBuffer);
            }
            theZBuffer->Dereference();
        }
    }
    if (theResult == kErrorOK) {
        theResult = this->ActivateTheScreen(inScreen, thePrimary, theSecondary, inManager, inWindow);
        if (theResult == kErrorOK) {
            theScreen->SetTextureManager(inManager);
            inManager->SetScreen(theScreen);
            theState->mPrimarySurface = thePrimary;
            theState->mTexture = NULL;
            theState->mRenderTarget = NULL;
            theState->mSecondarySurface = theSecondary;
            theState->mWindow = inWindow;
            theState->mIsScreenActivated = true;
            this->SetRenderTarget(theSecondary, inScreen);
            theSecondary->Reference();
            if (theCreatedSecondary)
                theSecondary->Dereference();
            this->mScreenStates[inScreen]->mAlphaTesting = -16;
            this->SetAlphaTesting(0, inScreen);
            this->mScreenStates[inScreen]->mAlphaTestingCompareState = -16;
            this->SetAlphaTestingCompareState(0, inScreen);
            this->mScreenStates[inScreen]->mAlphaTestingRefValue = 4294967296.0f;
            this->SetAlphaTestingRefValue(0.0f, inScreen);
            this->mScreenStates[inScreen]->mAntiAliasing = -16;
            this->SetAntiAliasing(0, inScreen);
            this->mScreenStates[inScreen]->mBlendSource = -16;
            this->mScreenStates[inScreen]->mBlendDestination = -16;
            this->SetBlending(1, 0, inScreen);
            this->mScreenStates[inScreen]->mColorKeying = -16;
            this->SetColorKeying(0, inScreen);
            this->mCulling = -16;
            this->SetCulling(0);
            this->mScreenStates[inScreen]->mDithering = -16;
            this->SetDithering(0, inScreen);
            this->mScreenStates[inScreen]->mDrawStyle = -16;
            this->SetDrawStyle(0, inScreen);
            this->mScreenStates[inScreen]->mFogState = -16;
            this->SetFog(0, inScreen);
            this->mScreenStates[inScreen]->mLighting = -16;
            this->SetLighting(0, inScreen);
            this->mScreenStates[inScreen]->mProjectionMode = -16;
            this->SetProjectionMode(1, inScreen);
            this->mScreenStates[inScreen]->mShading = -16;
            this->SetShading(1, inScreen);
            this->mScreenStates[inScreen]->mShadingColorSource = -16;
            this->SetShadingColorSource(1, inScreen);
            this->mScreenStates[inScreen]->mSpecular = -16;
            this->SetSpecular(0, inScreen);
            this->mScreenStates[inScreen]->mZBuffer = -16;
            this->SetZBuffer(0, inScreen);
            this->mScreenStates[inScreen]->mZCompare = -16;
            this->SetZCompare(0, inScreen);
            this->mDX7ScreenStates[inScreen]->mIs3DMode = false;
            this->Set3DMode(inScreen);
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x10009fa2
// Creates the HAL Direct3D device on the back buffer, sets the viewport and fixed render states, the vertex buffer
// and the card quirks (texture alpha modulation off on small Rage Pro cards, no dirty rectangles on two cards)
long DX7Rasterizer::ActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary,
                                      GETextureManager* inManager, GEWindow* inWindow)
{
    UString theDeviceName;
    DX7Surface* theTarget = dynamic_cast<DX7Surface*>(inSecondary);
    IDirect3D7* theDirect3D = NULL;
    IDirect3DDevice7* theDevice = NULL;
    long theResult;
    HRESULT theError = static_cast<DX7Screen*>(this->mDX7ScreenStates[inScreen]->mScreen)->mDirectDraw->QueryInterface(
        IID_IDirect3D7, (void**)&theDirect3D);
    if (theError == DD_OK) {
        theError = theDirect3D->CreateDevice(IID_IDirect3DHALDevice, *theTarget, &theDevice);
        if (theError == DD_OK) {
            this->mDX7ScreenStates[inScreen]->mDirect3D = theDirect3D;
            this->mDX7ScreenStates[inScreen]->mDevice = theDevice;
            this->mDX7ScreenStates[inScreen]->mViewport.dwX = 0;
            this->mDX7ScreenStates[inScreen]->mViewport.dwY = 0;
            this->mDX7ScreenStates[inScreen]->mViewport.dwWidth = theTarget->GetWidth();
            this->mDX7ScreenStates[inScreen]->mViewport.dwHeight = theTarget->GetHeight();
            this->mDX7ScreenStates[inScreen]->mViewport.dvMinZ = 0.0f;
            this->mDX7ScreenStates[inScreen]->mViewport.dvMaxZ = 1.0f;
            theDevice->SetViewport(&this->mDX7ScreenStates[inScreen]->mViewport);
            theDevice->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, FALSE);
            theDevice->SetRenderState(D3DRENDERSTATE_EXTENTS, FALSE);
            theDevice->SetRenderState(D3DRENDERSTATE_CLIPPING, FALSE);
            theDevice->SetRenderState(D3DRENDERSTATE_NORMALIZENORMALS, TRUE);
            theDevice->SetRenderState(D3DRENDERSTATE_LOCALVIEWER, FALSE);
            theDevice->SetRenderState(D3DRENDERSTATE_FOGVERTEXMODE, D3DFOG_LINEAR);
            theDevice->SetRenderState(D3DRENDERSTATE_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
            theDevice->SetRenderState(D3DRENDERSTATE_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
            theResult = this->CreateVertexBuffers(inScreen);
            if (theResult == kErrorOK) {
                DDDEVICEIDENTIFIER2 theIdentifier;
                memset(&theIdentifier, 0, sizeof(theIdentifier));
                static_cast<DX7Screen*>(this->mDX7ScreenStates[inScreen]->mScreen)->mDirectDraw->GetDeviceIdentifier(&theIdentifier, 0);
                theDeviceName = theIdentifier.szDescription;
                unsigned long theTotalMemory;
                this->GetTotalVideoMemory(inScreen, theTotalMemory);
                theDeviceName.ToUpper();
                if (theDeviceName.Find("RAGE", 0) == theDeviceName.GetLength() ||
                    theDeviceName.Find("PRO", 0) == theDeviceName.GetLength() || theTotalMemory > 0x800000)
                    this->mDX7ScreenStates[inScreen]->mCanModulateTextureAlpha = true;
                else
                    this->mDX7ScreenStates[inScreen]->mCanModulateTextureAlpha = false;
                if ((theIdentifier.dwVendorId == 0x1002 && theIdentifier.dwDeviceId == 0x5046) ||
                    (theIdentifier.dwVendorId == 0x121a && theIdentifier.dwDeviceId == 9))
                    this->mDX7ScreenStates[inScreen]->mDirtyRectangleSupport = false;
                else
                    this->mDX7ScreenStates[inScreen]->mDirtyRectangleSupport = true;
                this->mDX7ScreenStates[inScreen]->mStage1ColorOperation = 1;
                this->mDX7ScreenStates[inScreen]->mZEnable = 1;
                this->mDX7ScreenStates[inScreen]->mZWriteEnable = true;
                this->mDX7ScreenStates[inScreen]->mAlphaBlendEnable = true;
                this->mDX7ScreenStates[inScreen]->mBlendSource = -16;
                this->mDX7ScreenStates[inScreen]->mBlendDestination = -16;
                this->mDX7ScreenStates[inScreen]->mStage.mTexture = NULL;
                this->mDX7ScreenStates[inScreen]->mStage.mColorMode = 4;
                this->mDX7ScreenStates[inScreen]->mStage.mAlphaMode = 4;
                this->mDX7ScreenStates[inScreen]->mStage.mColorArgument1 = -1;
                this->mDX7ScreenStates[inScreen]->mStage.mColorArgument2 = -1;
                this->mDX7ScreenStates[inScreen]->mStage.mAlphaArgument1 = -1;
                this->mDX7ScreenStates[inScreen]->mStage.mAlphaArgument2 = -1;
                this->mDX7ScreenStates[inScreen]->mStage.mTextureAddressingMode = 3;
                this->mDX7ScreenStates[inScreen]->mStage.mMinFilterMode = 3;
                this->mDX7ScreenStates[inScreen]->mStage.mMagFilterMode = 3;
                this->mDX7ScreenStates[inScreen]->mStage.mMipFilterMode = 3;
                this->mDX7ScreenStates[inScreen]->mStage.mMipMapBias = FLT_MAX;
                this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
            }
            return theResult;
        }
    }
    theResult = kErrorNativeError;
    ESRecordNativeError(theError);
    return theResult;
}

// FUNCTION: DX7HR 0x1000a581
// Takes a free light slot (of 8), sets the light into it and enables it; returns 7 when all slots are in use
long DX7Rasterizer::AddLight(GELight& inLight, unsigned long inScreen, unsigned long& outIndex)
{
    long theResult;
    for (outIndex = 0; outIndex < 8; outIndex++)
        if (gLightsInUse[outIndex] == 0)
            break;
    if (outIndex < 8) {
        gLightsInUse[outIndex] = 1;
        theResult = this->UpdateLight(outIndex, inLight, inScreen);
        if (theResult == kErrorOK) {
            if (this->mDX7ScreenStates[inScreen]->GetDevice()->LightEnable(outIndex, TRUE) != D3D_OK)
                theResult = kErrorNativeError;
        }
    } else
        theResult = 7;
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
    long theResult = kErrorOK;
    this->SetRenderTarget(inTarget, inScreen);
    HRESULT theError;
    do {
        theError = this->mDX7ScreenStates[inScreen]->GetDevice()->BeginScene();
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(1);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError != D3D_OK) {
        if (theError == DDERR_SURFACELOST) {
            theResult = 9;
            ESRecordNativeError(theError);
        } else {
            theResult = kErrorNativeError;
            ESRecordNativeError(theError);
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000a6af
// Sets the texture and asks the device whether it can draw the current state in one pass
bool DX7Rasterizer::CanDoCurrentState(GETexture* inTexture, unsigned long inScreen)
{
    bool theResult = false;
    this->SetTexture(inTexture, inScreen);
    unsigned long thePasses;
    HRESULT theError = this->mDX7ScreenStates[inScreen]->GetDevice()->ValidateDevice(&thePasses);
    if (theError == D3D_OK)
        theResult = true;
    else
        ESRecordNativeError(theError);
    return theResult;
}

// FUNCTION: DX7HR 0x1000a6f7
// Fills a rectangle of the render target with a colour, waiting while the device is still drawing
long DX7Rasterizer::Clear(const GEColor& inColor, const GERectangle& inRectangle, unsigned long inScreen)
{
    long theResult = kErrorOK;
    GERectangle theRectangle;
    theRectangle = inRectangle;
    theRectangle.RoundToNearest();
    D3DRECT theRect;
    theRect.x1 = UMFloorUnsigned(theRectangle.GetLeft());
    theRect.x2 = UMFloorUnsigned(theRectangle.GetRight());
    theRect.y1 = UMFloorUnsigned(theRectangle.GetTop());
    theRect.y2 = UMFloorUnsigned(theRectangle.GetBottom());
    HRESULT theError;
    do {
        theError = this->mDX7ScreenStates[inScreen]->GetDevice()->Clear(1, &theRect, D3DCLEAR_TARGET,
                                                              D3DRGB(inColor.GetRed(), inColor.GetGreen(),
                                                                     inColor.GetBlue()), 0.0f, 0);
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(0);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError != D3D_OK) {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000a861
// Fills a rectangle of the Z-buffer with a depth value, waiting while the device is still drawing
long DX7Rasterizer::ClearZBuffer(float inZ, const GERectangle& inRectangle, unsigned long inScreen)
{
    long theResult = kErrorOK;
    GERectangle theRectangle;
    theRectangle = inRectangle;
    theRectangle.RoundToNearest();
    D3DRECT theRect;
    theRect.x1 = UMFloorUnsigned(theRectangle.GetLeft());
    theRect.x2 = UMFloorUnsigned(theRectangle.GetRight());
    theRect.y1 = UMFloorUnsigned(theRectangle.GetTop());
    theRect.y2 = UMFloorUnsigned(theRectangle.GetBottom());
    HRESULT theError;
    do {
        theError = this->mDX7ScreenStates[inScreen]->GetDevice()->Clear(1, &theRect, D3DCLEAR_ZBUFFER, 0, inZ, 0);
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(0);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError != D3D_OK) {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000a973
// Returns 0: this rasterizer does not measure the polygon area of the vertex buffer
float DX7Rasterizer::ComputePolyAreaOfCurrentVB(unsigned long inScreen)
{
    return 0.0f;
}

// FUNCTION: DX7HR 0x1000a97c
// Converts an engine transformation to a Direct3D matrix (transposed)
void DX7Rasterizer::ConvertToD3DMatrix(const GETransformation& inTransformation, _D3DMATRIX& outMatrix)
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
    IDirect3D7* theDirect3D = this->mDX7ScreenStates[inScreen]->mDirect3D;
    long theResult = kErrorOK;
    D3DVERTEXBUFFERDESC theDescription;
    memset(&theDescription, 0, sizeof(theDescription));
    theDescription.dwSize = sizeof(theDescription);
    theDescription.dwCaps = D3DVBCAPS_WRITEONLY | D3DVBCAPS_SYSTEMMEMORY;
    theDescription.dwFVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;
    theDescription.dwNumVertices = 0x960;
    HRESULT theError = theDirect3D->CreateVertexBuffer(&theDescription, &this->mDX7ScreenStates[inScreen]->mVertexBuffer, 0);
    if (theError != D3D_OK) {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    }
    return theResult;
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
    long theResult = kErrorOK;
    HRESULT theError;
    do {
        theError = this->mDX7ScreenStates[inScreen]->GetDevice()->EndScene();
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(1);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError != D3D_OK) {
        if (theError == DDERR_SURFACELOST) {
            theResult = 9;
            ESRecordNativeError(theError);
        } else {
            theResult = kErrorNativeError;
            ESRecordNativeError(theError);
        }
    }
    return theResult;
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

// Built without /Og like the enumeration callbacks (memory-resident locals, the inlined operator IDirectDraw7*'s
// temporary, `leave`): 27.3 without the pragma, exact with it.
#pragma optimize("g", off)

// FUNCTION: DX7HR 0x1000ac6d
// Returns whether the screen's device has a hardware-rasterizing Direct3D device
bool DX7Rasterizer::GetIsScreenValid(DX7Screen* inScreen)
{
    long theRetVal = 9;
    IDirect3D7* theIDirect3D = NULL;
    bool theIsValid = false;
    HRESULT theHResult = ((IDirectDraw7*)*inScreen)->QueryInterface(IID_IDirect3D7, (void**)&theIDirect3D);
    if (theHResult == DD_OK) {
        theHResult = theIDirect3D->EnumDevices(DX7EnumDevicesCallback, &theIsValid);
        if (theHResult == D3D_OK) {
            if (theIsValid)
                theRetVal = kErrorOK;
        } else {
            theRetVal = kErrorNativeError;
            ESRecordNativeError(theHResult);
        }
        theIDirect3D->Release();
    } else {
        theRetVal = kErrorNativeError;
        ESRecordNativeError(theHResult);
    }
    return theRetVal == kErrorOK;
}

#pragma optimize("g", on)

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
    unsigned long theTotal = 0;
    unsigned long theFree = 0;
    long theResult = 0xd;
    DDSCAPS2 theCaps;
    memset(&theCaps, 0, sizeof(theCaps));
    theCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    HRESULT theError = static_cast<DX7Screen*>(this->mDX7ScreenStates[inScreen]->mScreen)->mDirectDraw->GetAvailableVidMem(
        &theCaps, &theTotal, &theFree);
    if (theError == DD_OK && theTotal > 0) {
        outTotal = theTotal;
        outFree = theFree;
        theResult = kErrorOK;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000adb6
// Locks room for the given number of vertices in the vertex buffer, discarding it when full; always returns success
long DX7Rasterizer::LockVB(unsigned long inCount, unsigned long inScreen, D3DVERTEX_STD_COLOR*& outVertices)
{
    unsigned long theFlags;
    if (inCount <= 0x960 - this->mDX7ScreenStates[inScreen]->mVertexBufferPosition)
        theFlags = DDLOCK_NOOVERWRITE | DDLOCK_WRITEONLY;
    else {
        this->mDX7ScreenStates[inScreen]->mVertexBufferPosition = 0;
        theFlags = DDLOCK_DISCARDCONTENTS | DDLOCK_WRITEONLY;
    }
    HRESULT theError;
    do {
        theError = this->mDX7ScreenStates[inScreen]->mVertexBuffer->Lock(theFlags, reinterpret_cast<void**>(&outVertices), NULL);
        if (outVertices == NULL || theError == DDERR_SURFACEBUSY)
            Sleep(0);
    } while ((outVertices == NULL && theError == D3D_OK) || theError == DDERR_SURFACEBUSY);
    return kErrorOK;
}

long DX7Rasterizer::PromoteTexture(GETexture* inTexture)
{
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000ae3a
// Removes all 8 lights
long DX7Rasterizer::RemoveAllLights(unsigned long inScreen)
{
    for (unsigned long i = 0; i < 8; i++)
        this->RemoveLight(i, inScreen);
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000ae5c
// Disables a light and frees its slot; 9 when the slot was not in use
long DX7Rasterizer::RemoveLight(unsigned long inIndex, unsigned long inScreen)
{
    long theResult = kErrorOK;
    if (inIndex < 8 && gLightsInUse[inIndex] == 1) {
        gLightsInUse[inIndex] = 0;
        if (this->mDX7ScreenStates[inScreen]->GetDevice()->LightEnable(inIndex, FALSE) != D3D_OK)
            theResult = kErrorNativeError;
    } else
        theResult = 9;
    return theResult;
}

// FUNCTION: DX7HR 0x1000aea4
// Switches the screen to 2D drawing: identity world and view, a projection mapping pixels to the viewport
long DX7Rasterizer::Set2DMode(unsigned long inScreen)
{
    GETransformation theTransformation;
    if (!this->mDX7ScreenStates[inScreen]->mIs3DMode && !this->mDX7ScreenStates[inScreen]->mViewportChanged)
        return kErrorOK;
    D3DMATRIX theMatrix;
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_CLIPPING, TRUE);
    this->ConvertToD3DMatrix(theTransformation, theMatrix);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetTransform(D3DTRANSFORMSTATE_WORLD, &theMatrix);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetTransform(D3DTRANSFORMSTATE_VIEW, &theMatrix);
    theTransformation.SetElement(0, 0, 2.0f / (float)this->mDX7ScreenStates[inScreen]->mViewport.dwWidth);
    theTransformation.SetElement(0, 3, -1.0f);
    theTransformation.SetElement(1, 1, -2.0f / (float)this->mDX7ScreenStates[inScreen]->mViewport.dwHeight);
    theTransformation.SetElement(1, 3, 1.0f);
    this->ConvertToD3DMatrix(theTransformation, theMatrix);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &theMatrix);
    this->mDX7ScreenStates[inScreen]->mIs3DMode = false;
    this->mDX7ScreenStates[inScreen]->mViewportChanged = false;
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000b072
// Switches the screen to 3D drawing: the camera view (Y and Z swapped) and the perspective or orthographic projection
long DX7Rasterizer::Set3DMode(unsigned long inScreen)
{
    GETransformation theSwap;
    if (this->mDX7ScreenStates[inScreen]->mIs3DMode && !this->mDX7ScreenStates[inScreen]->mViewportChanged)
        return kErrorOK;
    GETransformation theView(false);
    D3DMATRIX theMatrix;
    theSwap.SetElement(1, 1, 0.0f);
    theSwap.SetElement(1, 2, 1.0f);
    theSwap.SetElement(2, 1, 1.0f);
    theSwap.SetElement(2, 2, 0.0f);
    theSwap.Apply(this->GetViewport(inScreen).GetCameraInverse(), theView);
    this->ConvertToD3DMatrix(theView, theMatrix);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetTransform(D3DTRANSFORMSTATE_VIEW, &theMatrix);
    if (this->mScreenStates[inScreen]->mProjectionMode == 1)
        this->ConvertToD3DMatrix(this->GetViewport(inScreen).GetPerspectiveProjection(), theMatrix);
    else
        this->ConvertToD3DMatrix(this->GetViewport(inScreen).GetOrthographicProjection(), theMatrix);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &theMatrix);
    this->mDX7ScreenStates[inScreen]->mIs3DMode = true;
    this->mDX7ScreenStates[inScreen]->mViewportChanged = false;
    return kErrorOK;
}

// FUNCTION: DX7HR 0x1000b22b
// Turns alpha testing on or off when it changes
void DX7Rasterizer::SetAlphaTesting(unsigned long inState, unsigned long inScreen)
{
    if (inState != this->mScreenStates[inScreen]->mAlphaTesting) {
        if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, inState) == D3D_OK)
            GERasterizer::SetAlphaTesting(inState, inScreen);
    }
}

// FUNCTION: DX7HR 0x1000b26c
// Sets the alpha test compare function (skipped when unchanged)
void DX7Rasterizer::SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen)
{
    unsigned long theFunction = 0;
    // Compare with the current compare function (this tested the alpha-testing on/off state).
    if (inState == this->mScreenStates[inScreen]->mAlphaTestingCompareState)
        return;
    switch (inState) {
    case kCompareNever:
        theFunction = D3DCMP_NEVER;
        break;
    case kCompareLess:
        theFunction = D3DCMP_LESS;
        break;
    case kCompareLessEqual:
        theFunction = D3DCMP_LESSEQUAL;
        break;
    case kCompareEqual:
        theFunction = D3DCMP_EQUAL;
        break;
    case kCompareNotEqual:
        theFunction = D3DCMP_NOTEQUAL;
        break;
    case kCompareGreaterEqual:
        theFunction = D3DCMP_GREATEREQUAL;
        break;
    case kCompareGreater:
        theFunction = D3DCMP_GREATER;
        break;
    case kCompareAlways:
        theFunction = D3DCMP_ALWAYS;
        break;
    }
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_ALPHAFUNC, theFunction) == D3D_OK)
        GERasterizer::SetAlphaTestingCompareState(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b2fa
// Sets the alpha test reference value (0..1 scaled to 0..255) when it changes
void DX7Rasterizer::SetAlphaTestingRefValue(float inValue, unsigned long inScreen)
{
    if (inValue == this->mScreenStates[inScreen]->mAlphaTestingRefValue)
        return;
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_ALPHAREF, (long)(inValue * 255.0f)) ==
        D3D_OK)
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
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_AMBIENT,
        RGBA_MAKE(UMFloatToLong(this->mComputedAmbientColor.GetRed() * 255.0f),
                  UMFloatToLong(this->mComputedAmbientColor.GetGreen() * 255.0f),
                  UMFloatToLong(this->mComputedAmbientColor.GetBlue() * 255.0f), UMFloatToLong(255.0f)));
}

// FUNCTION: DX7HR 0x1000b403
// Sets the ambient light intensity and sends the ambient colour scaled by it
void DX7Rasterizer::SetAmbientIntensity(float inIntensity, unsigned long inScreen)
{
    this->mAmbientIntensity = inIntensity;
    this->mComputedAmbientColor.SetRed(this->mAmbientColor.GetRed() * inIntensity);
    this->mComputedAmbientColor.SetGreen(this->mAmbientColor.GetGreen() * inIntensity);
    this->mComputedAmbientColor.SetBlue(this->mAmbientColor.GetBlue() * inIntensity);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_AMBIENT,
        RGBA_MAKE(UMFloatToLong(this->mComputedAmbientColor.GetRed() * 255.0f),
                  UMFloatToLong(this->mComputedAmbientColor.GetGreen() * 255.0f),
                  UMFloatToLong(this->mComputedAmbientColor.GetBlue() * 255.0f), UMFloatToLong(255.0f)));
}

// FUNCTION: DX7HR 0x1000b49d
// Turns sort-independent antialiasing on or off when it changes
void DX7Rasterizer::SetAntiAliasing(unsigned long inState, unsigned long inScreen)
{
    if (inState == this->mScreenStates[inScreen]->mAntiAliasing)
        return;
    D3DANTIALIASMODE theMode;
    if (inState == 1)
        theMode = D3DANTIALIAS_SORTINDEPENDENT;
    else
        theMode = D3DANTIALIAS_NONE;
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_ANTIALIAS, theMode) == D3D_OK)
        GERasterizer::SetAntiAliasing(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b4ea
// Sets the source and destination blend factors, turning alpha blending off for one/zero and sending only what changed
void DX7Rasterizer::SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen)
{
    unsigned long theBlend = 0;
    if (inSource == this->mScreenStates[inScreen]->mBlendSource && inDestination == this->mScreenStates[inScreen]->mBlendDestination)
        return;
    bool theEnable;
    if (inSource == kBlendOne && inDestination == kBlendZero)
        theEnable = false;
    else
        theEnable = true;
    if (this->mDX7ScreenStates[inScreen]->mAlphaBlendEnable != theEnable) {
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, theEnable);
        this->mDX7ScreenStates[inScreen]->mAlphaBlendEnable = theEnable;
    }
    if (this->mDX7ScreenStates[inScreen]->mBlendSource != inSource) {
        switch (inSource) {
        case kBlendZero: theBlend = D3DBLEND_ZERO; break;
        case kBlendOne: theBlend = D3DBLEND_ONE; break;
        case kBlendSrcColor: theBlend = D3DBLEND_SRCCOLOR; break;
        case kBlendInvSrcColor: theBlend = D3DBLEND_INVSRCCOLOR; break;
        case kBlendSrcAlpha: theBlend = D3DBLEND_SRCALPHA; break;
        case kBlendInvSrcAlpha: theBlend = D3DBLEND_INVSRCALPHA; break;
        case kBlendDestAlpha: theBlend = D3DBLEND_DESTALPHA; break;
        case kBlendInvDestAlpha: theBlend = D3DBLEND_INVDESTALPHA; break;
        case kBlendDestColor: theBlend = D3DBLEND_DESTCOLOR; break;
        case kBlendInvDestColor: theBlend = D3DBLEND_INVDESTCOLOR; break;
        case kBlendSrcAlphaSat: theBlend = D3DBLEND_SRCALPHASAT; break;
        case kBlendBothSrcAlpha: theBlend = D3DBLEND_BOTHSRCALPHA; break;
        case kBlendBothInvSrcAlpha: theBlend = D3DBLEND_BOTHINVSRCALPHA; break;
        }
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_SRCBLEND, theBlend);
        this->mDX7ScreenStates[inScreen]->mBlendSource = inSource;
    }
    if (this->mDX7ScreenStates[inScreen]->mBlendDestination != inDestination) {
        switch (inDestination) {
        case kBlendZero: theBlend = D3DBLEND_ZERO; break;
        case kBlendOne: theBlend = D3DBLEND_ONE; break;
        case kBlendSrcColor: theBlend = D3DBLEND_SRCCOLOR; break;
        case kBlendInvSrcColor: theBlend = D3DBLEND_INVSRCCOLOR; break;
        case kBlendSrcAlpha: theBlend = D3DBLEND_SRCALPHA; break;
        case kBlendInvSrcAlpha: theBlend = D3DBLEND_INVSRCALPHA; break;
        case kBlendDestAlpha: theBlend = D3DBLEND_DESTALPHA; break;
        case kBlendInvDestAlpha: theBlend = D3DBLEND_INVDESTALPHA; break;
        case kBlendDestColor: theBlend = D3DBLEND_DESTCOLOR; break;
        case kBlendInvDestColor: theBlend = D3DBLEND_INVDESTCOLOR; break;
        case kBlendSrcAlphaSat: theBlend = D3DBLEND_SRCALPHASAT; break;
        case kBlendBothSrcAlpha: theBlend = D3DBLEND_BOTHSRCALPHA; break;
        case kBlendBothInvSrcAlpha: theBlend = D3DBLEND_BOTHINVSRCALPHA; break;
        }
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_DESTBLEND, theBlend);
        this->mDX7ScreenStates[inScreen]->mBlendDestination = inDestination;
    }
    GERasterizer::SetBlending(inSource, inDestination, inScreen);
}

// FUNCTION: DX7HR 0x1000b79d
// Turns colour keying on or off when it changes
void DX7Rasterizer::SetColorKeying(unsigned long inState, unsigned long inScreen)
{
    if (inState == this->mScreenStates[inScreen]->mColorKeying)
        return;
    bool theEnable;
    if (inState == 1)
        theEnable = true;
    else
        theEnable = false;
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, theEnable) == D3D_OK)
        GERasterizer::SetColorKeying(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b7e7
// Sets the culling mode (clockwise, counter-clockwise or none) on every screen's device
void DX7Rasterizer::SetCulling(unsigned long inState)
{
    if (inState == this->GetCulling())
        return;
    unsigned long theCullMode;
    switch (inState) {
    case kCullCW:
        theCullMode = D3DCULL_CW;
        break;
    case kCullCCW:
        theCullMode = D3DCULL_CCW;
        break;
    default:
        theCullMode = D3DCULL_NONE;
        break;
    }
    for (unsigned long i = 0; i < this->mDX7ScreenStates.size(); i++) {
        IDirect3DDevice7* theDevice = this->mDX7ScreenStates[i]->mDevice;
        if (theDevice != NULL)
            theDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, theCullMode);
    }
    GERasterizer::SetCulling(inState);
}

// FUNCTION: DX7HR 0x1000b843
// Sets the debug colour (opaque)
void DX7Rasterizer::SetDebugColor(const GEColor& inColor)
{
    this->mDebugColor = RGBA_MAKE(UMFloatToLong(inColor.GetRed() * 255.0f), UMFloatToLong(inColor.GetGreen() * 255.0f),
                            UMFloatToLong(inColor.GetBlue() * 255.0f), UMFloatToLong(255.0f));
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
    if (inState == this->mScreenStates[inScreen]->mDithering)
        return;
    bool theEnable;
    if (inState == 1)
        theEnable = true;
    else
        theEnable = false;
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_DITHERENABLE, theEnable) == D3D_OK)
        GERasterizer::SetDithering(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b916
// Sets solid, wireframe or point drawing when it changes
void DX7Rasterizer::SetDrawStyle(unsigned long inStyle, unsigned long inScreen)
{
    unsigned long theFillMode = 0;
    if (inStyle == this->mScreenStates[inScreen]->mDrawStyle)
        return;
    switch (inStyle) {
    case kDrawStyleSolid:
        theFillMode = D3DFILL_SOLID;
        break;
    case kDrawStyleWireframe:
        theFillMode = D3DFILL_WIREFRAME;
        break;
    case kDrawStylePoint:
        theFillMode = D3DFILL_POINT;
        break;
    }
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_FILLMODE, theFillMode) == D3D_OK)
        GERasterizer::SetDrawStyle(inStyle, inScreen);
}

// FUNCTION: DX7HR 0x1000b970
// Turns fog on or off when it changes
void DX7Rasterizer::SetFog(unsigned long inState, unsigned long inScreen)
{
    if (inState == this->mScreenStates[inScreen]->mFogState)
        return;
    bool theEnable;
    if (inState == 1)
        theEnable = true;
    else
        theEnable = false;
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_FOGENABLE, theEnable) == D3D_OK)
        GERasterizer::SetFog(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000b9ba
// Sets the fog colour when it changes
void DX7Rasterizer::SetFogColor(const GEColor& inColor, unsigned long inScreen)
{
    if (this->mScreenStates[inScreen]->mFog.mColor == inColor)
        return;
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_FOGCOLOR,
            D3DRGB(inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue())) == D3D_OK)
        GERasterizer::SetFogColor(inColor, inScreen);
}

// FUNCTION: DX7HR 0x1000ba78
// Sets the fog start and end distances, sending only the ones that changed
void DX7Rasterizer::SetFogRange(float inStart, float inEnd, unsigned long inScreen)
{
    if (this->mScreenStates[inScreen]->mFog.mStart != inStart)
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_FOGSTART, *reinterpret_cast<unsigned long*>(&inStart));
    if (this->mScreenStates[inScreen]->mFog.mEnd != inEnd)
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_FOGEND, *reinterpret_cast<unsigned long*>(&inEnd));
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
    if (inState == this->mScreenStates[inScreen]->mLighting)
        return;
    if (inState == 1)
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_LIGHTING, TRUE);
    else
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
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
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderTarget(*static_cast<DX7Surface*>(inTarget), 0);
    this->mDX7ScreenStates[inScreen]->mViewport.dwWidth = inTarget->GetWidth();
    this->mDX7ScreenStates[inScreen]->mViewport.dwHeight = inTarget->GetHeight();
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetViewport(&this->mDX7ScreenStates[inScreen]->mViewport);
}

// FUNCTION: DX7HR 0x1000bcb3
// Sets Gouraud or flat shading when it changes
void DX7Rasterizer::SetShading(unsigned long inState, unsigned long inScreen)
{
    if (inState == this->mScreenStates[inScreen]->mShading)
        return;
    D3DSHADEMODE theMode;
    if (inState == 1)
        theMode = D3DSHADE_GOURAUD;
    else
        theMode = D3DSHADE_FLAT;
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_SHADEMODE, theMode) == D3D_OK)
        GERasterizer::SetShading(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000bcfd
// Takes lighting colours from the material (source 0) or from the vertex colours (white material) when it changes
void DX7Rasterizer::SetShadingColorSource(unsigned long inSource, unsigned long inScreen)
{
    if (inSource == this->mScreenStates[inScreen]->mShadingColorSource)
        return;
    HRESULT theError;
    if (inSource == 0) {
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_COLORVERTEX, FALSE);
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
        theError = this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
    } else {
        D3DMATERIAL7 theMaterial;
        theMaterial.dcvAmbient.r = 1.0f;
        theMaterial.dcvAmbient.g = 1.0f;
        theMaterial.dcvAmbient.b = 1.0f;
        theMaterial.dcvAmbient.a = 1.0f;
        theMaterial.dcvDiffuse.r = 1.0f;
        theMaterial.dcvDiffuse.g = 1.0f;
        theMaterial.dcvDiffuse.b = 1.0f;
        theMaterial.dcvDiffuse.a = 1.0f;
        theMaterial.dcvEmissive.r = 0.0f;
        theMaterial.dcvEmissive.g = 0.0f;
        theMaterial.dcvEmissive.b = 0.0f;
        theMaterial.dcvEmissive.a = 0.0f;
        theMaterial.dcvSpecular.r = 0.0f;
        theMaterial.dcvSpecular.g = 0.0f;
        theMaterial.dcvSpecular.b = 0.0f;
        theMaterial.dcvSpecular.a = 0.0f;
        theMaterial.dvPower = 0.0f;
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetMaterial(&theMaterial);
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_COLORVERTEX, TRUE);
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
        theError = this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
    }
    if (theError == D3D_OK)
        GERasterizer::SetShadingColorSource(inSource, inScreen);
}

// FUNCTION: DX7HR 0x1000be76
// Turns specular highlights on or off when it changes
void DX7Rasterizer::SetSpecular(unsigned long inState, unsigned long inScreen)
{
    if (inState == this->mScreenStates[inScreen]->mSpecular)
        return;
    bool theEnable;
    if (inState == 1)
        theEnable = true;
    else
        theEnable = false;
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, theEnable) == D3D_OK)
        GERasterizer::SetSpecular(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000bec0
// Binds a texture to stage 0 with its colour and alpha operations, addressing, filters and mip bias, sending
// only what changed; null disables texturing; stage 1 is always disabled
void DX7Rasterizer::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    unsigned long theColorArgument1 = 0;
    unsigned long theColorArgument2 = 0;
    unsigned long theColorOperation = 0;
    unsigned long theAlphaArgument1 = 0;
    unsigned long theAlphaArgument2 = 0;
    unsigned long theAlphaOperation = 0;
    unsigned long theAddressing = 0;
    unsigned long theMinFilter = 0;
    unsigned long theMagFilter = 0;
    unsigned long theMipFilter = 0;
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
    if (inTexture == NULL) {
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTexture(0, NULL);
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_DISABLE);
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        this->mDX7ScreenStates[inScreen]->mStage1ColorOperation = 1;
        this->mDX7ScreenStates[inScreen]->mStage.mTexture = NULL;
        this->mDX7ScreenStates[inScreen]->mStage.mColorMode = 4;
        this->mDX7ScreenStates[inScreen]->mStage.mAlphaMode = 4;
        return;
    }
    DX7RTextureStage theCurrent = this->mDX7ScreenStates[inScreen]->mStage;
    IDirectDrawSurface7* theSurface = *static_cast<DX7Surface*>(inTexture->GetStageTexture(0)->GetFrameSurface());
    if (theSurface != theCurrent.mTexture) {
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTexture(0, theSurface);
        this->mDX7ScreenStates[inScreen]->mStage.mTexture =
            *static_cast<DX7Surface*>(inTexture->GetStageTexture(0)->GetFrameSurface());
    }
    if (inTexture->GetStage()->mColorMode != theCurrent.mColorMode) {
        switch (inTexture->GetStage()->mColorMode) {
        case kStageOpDiffuse:
            theColorArgument1 = D3DTA_TEXTURE;
            theColorArgument2 = D3DTA_DIFFUSE;
            theColorOperation = D3DTOP_SELECTARG2;
            break;
        case kStageOpTexture:
            theColorArgument1 = D3DTA_TEXTURE;
            theColorArgument2 = D3DTA_DIFFUSE;
            theColorOperation = D3DTOP_SELECTARG1;
            break;
        case kStageOpModulate:
            theColorArgument2 = D3DTA_DIFFUSE;
            theColorArgument1 = D3DTA_TEXTURE;
            theColorOperation = D3DTOP_MODULATE;
            break;
        case kStageOpModulateCurrent:
            theColorArgument2 = D3DTA_CURRENT;
            theColorArgument1 = D3DTA_TEXTURE;
            theColorOperation = D3DTOP_MODULATE;
            break;
        }
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_COLOROP, theColorOperation);
        this->mDX7ScreenStates[inScreen]->mStage.mColorMode = inTexture->GetStage()->mColorMode;
        if (theColorArgument1 != theCurrent.mColorArgument1) {
            this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, theColorArgument1);
            this->mDX7ScreenStates[inScreen]->mStage.mColorArgument1 = theColorArgument1;
        }
        if (theColorArgument2 != theCurrent.mColorArgument2) {
            this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_COLORARG2, theColorArgument2);
            this->mDX7ScreenStates[inScreen]->mStage.mColorArgument2 = theColorArgument2;
        }
    }
    if (inTexture->GetStage()->mAlphaMode != theCurrent.mAlphaMode) {
        switch (inTexture->GetStage()->mAlphaMode) {
        case kStageOpDiffuse:
            theAlphaArgument1 = D3DTA_TEXTURE;
            theAlphaArgument2 = D3DTA_DIFFUSE;
            theAlphaOperation = D3DTOP_SELECTARG2;
            break;
        case kStageOpTexture:
            theAlphaArgument1 = D3DTA_TEXTURE;
            theAlphaArgument2 = D3DTA_DIFFUSE;
            theAlphaOperation = D3DTOP_SELECTARG1;
            break;
        case kStageOpModulate:
            theAlphaArgument2 = D3DTA_DIFFUSE;
            theAlphaArgument1 = D3DTA_TEXTURE;
            theAlphaOperation = D3DTOP_MODULATE;
            break;
        case kStageOpModulateCurrent:
            theAlphaArgument2 = D3DTA_CURRENT;
            theAlphaArgument1 = D3DTA_TEXTURE;
            theAlphaOperation = D3DTOP_MODULATE;
            break;
        }
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, theAlphaOperation);
        this->mDX7ScreenStates[inScreen]->mStage.mAlphaMode = inTexture->GetStage()->mAlphaMode;
        if (theAlphaArgument1 != theCurrent.mAlphaArgument1) {
            this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG1, theAlphaArgument1);
            this->mDX7ScreenStates[inScreen]->mStage.mAlphaArgument1 = theAlphaArgument1;
        }
        if (theAlphaArgument2 != theCurrent.mAlphaArgument2) {
            this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG2, theAlphaArgument2);
            this->mDX7ScreenStates[inScreen]->mStage.mAlphaArgument2 = theAlphaArgument2;
        }
    }
    if (inTexture->GetStage()->mTextureAddressingMode != theCurrent.mTextureAddressingMode) {
        if (inTexture->GetStage()->mTextureAddressingMode == kAddressingClamp)
            theAddressing = D3DTADDRESS_CLAMP;
        else if (inTexture->GetStage()->mTextureAddressingMode == kAddressingMirror)
            theAddressing = D3DTADDRESS_MIRROR;
        else if (inTexture->GetStage()->mTextureAddressingMode == kAddressingWrap)
            theAddressing = D3DTADDRESS_WRAP;
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_ADDRESS, theAddressing);
        this->mDX7ScreenStates[inScreen]->mStage.mTextureAddressingMode = inTexture->GetStage()->mTextureAddressingMode;
    }
    if (inTexture->GetStage()->mMinFilterMode != theCurrent.mMinFilterMode) {
        if (inTexture->GetStage()->mMinFilterMode == kMinFilterPoint)
            theMinFilter = D3DTFN_POINT;
        else if (inTexture->GetStage()->mMinFilterMode == kMinFilterLinear)
            theMinFilter = D3DTFN_LINEAR;
        else if (inTexture->GetStage()->mMinFilterMode == kMinFilterAnisotropic)
            theMinFilter = D3DTFN_ANISOTROPIC;
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_MINFILTER, theMinFilter);
        this->mDX7ScreenStates[inScreen]->mStage.mMinFilterMode = inTexture->GetStage()->mMinFilterMode;
    }
    if (inTexture->GetStage()->mMagFilterMode != theCurrent.mMagFilterMode) {
        if (inTexture->GetStage()->mMagFilterMode == kMagFilterPoint)
            theMagFilter = D3DTFG_POINT;
        else if (inTexture->GetStage()->mMagFilterMode == kMagFilterLinear)
            theMagFilter = D3DTFG_LINEAR;
        else if (inTexture->GetStage()->mMagFilterMode == kMagFilterFlatCubic)
            theMagFilter = D3DTFG_FLATCUBIC;
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_MAGFILTER, theMagFilter);
        this->mDX7ScreenStates[inScreen]->mStage.mMagFilterMode = inTexture->GetStage()->mMagFilterMode;
    }
    if (inTexture->GetStage()->mMipFilterMode != theCurrent.mMipFilterMode) {
        if (inTexture->GetStage()->mMipFilterMode == kMipFilterNone)
            theMipFilter = D3DTFP_NONE;
        else if (inTexture->GetStage()->mMipFilterMode == kMipFilterPoint)
            theMipFilter = D3DTFP_POINT;
        else if (inTexture->GetStage()->mMipFilterMode == kMipFilterLinear)
            theMipFilter = D3DTFP_LINEAR;
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_MIPFILTER, theMipFilter);
        this->mDX7ScreenStates[inScreen]->mStage.mMipFilterMode = inTexture->GetStage()->mMipFilterMode;
    }
    if (inTexture->GetStage()->mMipMapBias != theCurrent.mMipMapBias) {
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(0, D3DTSS_MIPMAPLODBIAS,
                                                                  *(unsigned long*)&inTexture->GetStage()->mMipMapBias);
        this->mDX7ScreenStates[inScreen]->mStage.mMipMapBias = inTexture->GetStage()->mMipMapBias;
    }
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetTexture(1, NULL);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    this->mDX7ScreenStates[inScreen]->mStage1ColorOperation = 1;
    inTexture->ValidateChanges();
}

// FUNCTION: DX7HR 0x1000c634
// Sets the viewport rectangle and depth range on the device and marks the 2D/3D transforms for refresh
void DX7Rasterizer::SetViewport(const GEViewport& inViewport, unsigned long inScreen)
{
    GERectangle theRectangle;
    GERasterizer::SetViewport(inViewport, inScreen);
    theRectangle = inViewport.GetViewRectangle();
    theRectangle.RoundToNearest();
    this->mDX7ScreenStates[inScreen]->mViewport.dwX = UMFloorUnsigned(theRectangle.GetLeft());
    this->mDX7ScreenStates[inScreen]->mViewport.dwY = UMFloorUnsigned(theRectangle.GetTop());
    this->mDX7ScreenStates[inScreen]->mViewport.dwWidth = UMFloorUnsigned(theRectangle.GetWidth());
    this->mDX7ScreenStates[inScreen]->mViewport.dwHeight = UMFloorUnsigned(theRectangle.GetHeight());
    inViewport.GetDepthRange(this->mDX7ScreenStates[inScreen]->mViewport.dvMinZ, this->mDX7ScreenStates[inScreen]->mViewport.dvMaxZ);
    this->mDX7ScreenStates[inScreen]->GetDevice()->SetViewport(&this->mDX7ScreenStates[inScreen]->mViewport);
    this->mDX7ScreenStates[inScreen]->mViewportChanged = true;
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
    EE_LOST_DEBUG(1);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): 2001 twin inlines what AoC refuses (0 vs 2 out-of-line _Ptrit<GESurface*>::operator+); no assert-line evidence (the AoC asserts sit 9 lines earlier than the twin's)
    unsigned long theEnable = 1;
    bool theWrite = (inState & kZBufferWriteOnly) > 0;
    if (inState == this->mScreenStates[inScreen]->mZBuffer)
        return;
    if ((inState & kZBufferTestOnly) <= 0 && !theWrite)
        theEnable = 0;
    if (this->mDX7ScreenStates[inScreen]->mZEnable != theEnable) {
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_ZENABLE, theEnable);
        this->mDX7ScreenStates[inScreen]->mZEnable = theEnable;
    }
    if (this->mDX7ScreenStates[inScreen]->mZWriteEnable != theWrite) {
        this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, theWrite);
        this->mDX7ScreenStates[inScreen]->mZWriteEnable = theWrite;
    }
    GERasterizer::SetZBuffer(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000c910
// Sets the Z compare function when it changes
void DX7Rasterizer::SetZCompare(unsigned long inState, unsigned long inScreen)
{
    unsigned long theFunction = 0;
    if (inState == this->mScreenStates[inScreen]->mZCompare)
        return;
    switch (inState) {
    case kCompareNever:
        theFunction = D3DCMP_NEVER;
        break;
    case kCompareLess:
        theFunction = D3DCMP_LESS;
        break;
    case kCompareLessEqual:
        theFunction = D3DCMP_LESSEQUAL;
        break;
    case kCompareEqual:
        theFunction = D3DCMP_EQUAL;
        break;
    case kCompareNotEqual:
        theFunction = D3DCMP_NOTEQUAL;
        break;
    case kCompareGreaterEqual:
        theFunction = D3DCMP_GREATEREQUAL;
        break;
    case kCompareGreater:
        theFunction = D3DCMP_GREATER;
        break;
    case kCompareAlways:
        theFunction = D3DCMP_ALWAYS;
        break;
    }
    if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetRenderState(D3DRENDERSTATE_ZFUNC, theFunction) == D3D_OK)
        GERasterizer::SetZCompare(inState, inScreen);
}

// FUNCTION: DX7HR 0x1000c99e
// Shows the frame (flip full screen, blit to the window's client area windowed); every 12 frames updates the
// frame, polygon and texture-change statistics
long DX7Rasterizer::ShowScene(unsigned long inScreen)
{
    long theResult;
    GE2DPoint theOrigin;
    GERScreenState* theState = this->mScreenStates[inScreen];
    theState->m32C[0]++;
    if (theState->m32C[0] == 12) {
        const float kFramesInverse = 1.0f / 12.0f;
        theState->mDrawnPolyAreaPerFrame = theState->mDrawnPolyArea * kFramesInverse;
        theState->mDrawnPolysPerFrame = (float)theState->mDrawnPolyCount * kFramesInverse;
        theState->mScenePolysPerFrame = (float)theState->m32C[1] * kFramesInverse;
        theState->mTotalTextureChangesPerFrame = (float)theState->m32C[3] * kFramesInverse;
        unsigned long theTime = TMGetTime();
        float theElapsed = (float)(theTime - theState->m32C[2]);
        if (theElapsed > 0.0f) {
            theState->mDrawnPolyAreaPerSecond = theState->mDrawnPolyArea * 1000.0f / theElapsed;
            theState->mDrawnPolysPerSecond = (float)theState->mDrawnPolyCount * 1000.0f / theElapsed;
            theState->mFramesPerSecond = (float)theState->m32C[0] * 1000.0f / theElapsed;
            theState->mScenePolysPerSecond = (float)theState->m32C[1] * 1000.0f / theElapsed;
        } else {
            theState->mDrawnPolyAreaPerSecond = 0.0f;
            theState->mDrawnPolysPerSecond = 0.0f;
            theState->mFramesPerSecond = 0.0f;
            theState->mScenePolysPerSecond = 0.0f;
        }
        theState->m32C[2] = theTime;
        theState->mDrawnPolyArea = 0.0f;
        theState->mDrawnPolyCount = 0;
        theState->m32C[0] = 0;
        theState->m32C[1] = 0;
        theState->m32C[3] = 0;
    }
    if (theState->mScreen->GetIsFullScreen())
        theResult = theState->mPrimarySurface->Flip(theState->mWaitForVSync);
    else {
        IDirectDrawSurface7* thePrimary = *static_cast<DX7Surface*>(theState->mPrimarySurface);
        IDirectDrawSurface7* theSecondary = *static_cast<DX7Surface*>(theState->mSecondarySurface);
        RECT theSource;
        theSource.left = 0;
        theSource.top = 0;
        theSource.right = theState->mSecondarySurface->GetWidth();
        theSource.bottom = theState->mSecondarySurface->GetHeight();
        theOrigin = theState->mWindow->GetClientOrigin();
        RECT theDestination;
        theDestination.left = UMFloatToLong(theOrigin.GetX());
        theDestination.top = UMFloatToLong(theOrigin.GetY());
        theDestination.right = theDestination.left + theSource.right;
        theDestination.bottom = theDestination.top + theSource.bottom;
        HRESULT theError;
        do {
            theError = thePrimary->Blt(&theDestination, theSecondary, &theSource, 0, NULL);
            if (theError == DDERR_WASSTILLDRAWING)
                Sleep(0);
        } while (theError == DDERR_WASSTILLDRAWING);
        if (theError == DD_OK)
            theResult = kErrorOK;
        else {
            ESRecordNativeError(theError);
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000cbe5
// Sends a light (point or directional, colour, attenuation, range) to its slot; 9 when the slot is not in use
long DX7Rasterizer::UpdateLight(unsigned long inIndex, GELight& inLight, unsigned long inScreen)
{
    long theResult = kErrorOK;
    if (inIndex < 8 && gLightsInUse[inIndex] == 1) {
        D3DLIGHT7 theLight;
        memset(&theLight, 0, sizeof(theLight));
        theLight.dvPosition.x = inLight.GetLocation().GetX();
        theLight.dvPosition.y = inLight.GetLocation().GetY();
        theLight.dvPosition.z = inLight.GetLocation().GetZ();
        theLight.dvDirection.x = -inLight.GetDirection().GetX();
        theLight.dltType = inLight.GetStyle() ? D3DLIGHT_DIRECTIONAL : D3DLIGHT_POINT;
        theLight.dvDirection.y = -inLight.GetDirection().GetY();
        theLight.dvDirection.z = -inLight.GetDirection().GetZ();
        theLight.dcvDiffuse.r = inLight.GetComputedColor().GetRed();
        theLight.dcvDiffuse.g = inLight.GetComputedColor().GetGreen();
        theLight.dcvDiffuse.b = inLight.GetComputedColor().GetBlue();
        theLight.dvAttenuation0 = inLight.GetAttenuation0();
        theLight.dvAttenuation1 = inLight.GetAttenuation1();
        theLight.dvAttenuation2 = inLight.GetAttenuation2();
        theLight.dvRange = inLight.GetRange();
        if (this->mDX7ScreenStates[inScreen]->GetDevice()->SetLight(inIndex, &theLight) != D3D_OK)
            theResult = kErrorNativeError;
    } else
        theResult = 9;
    return theResult;
}

// FUNCTION: DX7HR 0x1000ccab
// Unlocks the vertex buffer, waiting while it is busy
long DX7Rasterizer::UnlockVB(unsigned long inScreen)
{
    long theResult;
    HRESULT theError;
    do {
        theError = this->mDX7ScreenStates[inScreen]->mVertexBuffer->Unlock();
        if (theError == DDERR_SURFACEBUSY)
            Sleep(0);
    } while (theError == DDERR_SURFACEBUSY);
    if (theError == D3D_OK)
        theResult = kErrorOK;
    else {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    }
    return theResult;
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
