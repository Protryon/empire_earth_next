// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\DX7HRTnLDisplay\DX7TnLRasterizer.cpp (folder and file
// name inferred: no __FILE__ in DX7HRTnLDisplay.dll names it; it sorts after Common\DX7Surface.cpp in link order)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// DX7HRTnLDisplay.dll 0x1000fd16..0x10010512: the U2D key init chains, the TnL device enumeration callback, then
// DX7TnLRasterizer's ctor and dtor and its members in alphabetical order, then the template-static init chain head
// and its atexit registrars.
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <float.h>

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7TnLRasterizer.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"

// The U2D key init chains:
// SYNTHETIC: DX7HRTNL 0x1000fd16
// SYNTHETIC: DX7HRTNL 0x1000fd20
// SYNTHETIC: DX7HRTNL 0x1000fd31
// SYNTHETIC: DX7HRTNL 0x1000fd3d
// SYNTHETIC: DX7HRTNL 0x1000fd48
// SYNTHETIC: DX7HRTNL 0x1000fd52
// SYNTHETIC: DX7HRTNL 0x1000fd63
// SYNTHETIC: DX7HRTNL 0x1000fd6f
// SYNTHETIC: DX7HRTNL 0x1000fd7a
// SYNTHETIC: DX7HRTNL 0x1000fd84
// SYNTHETIC: DX7HRTNL 0x1000fd95
// SYNTHETIC: DX7HRTNL 0x1000fda1
// SYNTHETIC: DX7HRTNL 0x1000fdac
// SYNTHETIC: DX7HRTNL 0x1000fdb6
// SYNTHETIC: DX7HRTNL 0x1000fdc7
// SYNTHETIC: DX7HRTNL 0x1000fdd3
// SYNTHETIC: DX7HRTNL 0x1000fdde
// SYNTHETIC: DX7HRTNL 0x1000fde8
// SYNTHETIC: DX7HRTNL 0x1000fdf9
// SYNTHETIC: DX7HRTNL 0x1000fe05
// SYNTHETIC: DX7HRTNL 0x1000fe10
// SYNTHETIC: DX7HRTNL 0x1000fe1a
// SYNTHETIC: DX7HRTNL 0x1000fe2b
// SYNTHETIC: DX7HRTNL 0x1000fe37
// SYNTHETIC: DX7HRTNL 0x1000fe42
// SYNTHETIC: DX7HRTNL 0x1000fe4c
// SYNTHETIC: DX7HRTNL 0x1000fe5d
// SYNTHETIC: DX7HRTNL 0x1000fe69

// Built without /Og like DX7Rasterizer.cpp's enumeration callbacks (memory-resident locals, `leave`; CLAUDE.md rule
// 4). The required caps live in a memory local (`mov [ebp-0xc], 0x90000`), so they are a named variable.
#pragma optimize("g", off)

// FUNCTION: DX7HRTNL 0x1000fe74
// Device enumeration callback: marks the screen valid and stops at the first device with hardware T&L and rasterization
HRESULT CALLBACK DX7TnLEnumDevicesCallback(LPSTR inDescription, LPSTR inName, LPD3DDEVICEDESC7 inCaps,
                                           LPVOID inContext)
{
    bool* theContext = (bool*)inContext;
    HRESULT theResult = D3DENUMRET_OK;
    unsigned long theRequiredCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_HWRASTERIZATION;
    bool theFlag = (inCaps->dwDevCaps & theRequiredCaps) == theRequiredCaps;
    if (theFlag) {
        *theContext = true;
        theResult = D3DENUMRET_CANCEL;
    }
    return theResult;
}

#pragma optimize("g", on)

// FUNCTION: DX7HRTNL 0x1000feba
// Creates the T&L rasterizer under the given name
DX7TnLRasterizer::DX7TnLRasterizer(const UString& inName) : DX7Rasterizer(inName)
{
}

// FUNCTION: DX7HRTNL 0x1000fed2
// Destroys the T&L rasterizer (base members only)
DX7TnLRasterizer::~DX7TnLRasterizer()
{
}

// FUNCTION: DX7HRTNL 0x1000fedd
// Creates the T&L HAL device on the back buffer, sets the viewport, fixed render states and vertex buffer, then
// resets the cached states; on a CreateDevice failure the IDirect3D7 interface is never released
long DX7TnLRasterizer::ActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary,
                                         GETextureManager* inManager, GEWindow* inWindow)
{
    DX7Surface* theTarget = dynamic_cast<DX7Surface*>(inSecondary);
    IDirect3D7* theDirect3D = NULL;
    IDirect3DDevice7* theDevice = NULL;
    long theResult;
    HRESULT theError = static_cast<DX7Screen*>(this->mDX7ScreenStates[inScreen]->mScreen)->mDirectDraw->QueryInterface(
        IID_IDirect3D7, (void**)&theDirect3D);
    if (theError == DD_OK) {
        theError = theDirect3D->CreateDevice(IID_IDirect3DTnLHalDevice, *theTarget, &theDevice);
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
                this->mDX7ScreenStates[inScreen]->mCanModulateTextureAlpha = true;
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
                this->mDX7ScreenStates[inScreen]->mDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
            }
            return theResult;
        }
        // CreateDevice failed: release the IDirect3D7 the query returned.
        theDirect3D->Release();
    }
    theResult = kErrorNativeError;
    ESRecordNativeError(theError);
    return theResult;
}

// FUNCTION: DX7HRTNL 0x10010391
// Creates the screen's write-only vertex buffer (2400 vertices: position, normal, colour, one texture)
long DX7TnLRasterizer::CreateVertexBuffers(unsigned long inScreen)
{
    IDirect3D7* theDirect3D = this->mDX7ScreenStates[inScreen]->mDirect3D;
    long theResult = kErrorOK;
    D3DVERTEXBUFFERDESC theDescription;
    memset(&theDescription, 0, sizeof(theDescription));
    theDescription.dwSize = sizeof(theDescription);
    theDescription.dwCaps = D3DVBCAPS_WRITEONLY;
    theDescription.dwFVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;
    theDescription.dwNumVertices = 0x960;
    HRESULT theError = theDirect3D->CreateVertexBuffer(&theDescription, &this->mDX7ScreenStates[inScreen]->mVertexBuffer, 0);
    if (theError != D3D_OK) {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    }
    return theResult;
}

// Built without /Og like DX7Rasterizer::GetIsScreenValid (CLAUDE.md rule 4).
#pragma optimize("g", off)

// FUNCTION: DX7HRTNL 0x10010410
// Tells whether a screen's DirectDraw object offers a device with hardware T&L and rasterization
bool DX7TnLRasterizer::GetIsScreenValid(DX7Screen* inScreen)
{
    long theRetVal = 9;
    IDirect3D7* theIDirect3D = NULL;
    bool theIsValid = false;
    HRESULT theHResult = ((IDirectDraw7*)*inScreen)->QueryInterface(IID_IDirect3D7, (void**)&theIDirect3D);
    if (theHResult == DD_OK) {
        theHResult = theIDirect3D->EnumDevices(DX7TnLEnumDevicesCallback, &theIsValid);
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

// The template-static init chain head and its atexit registrars:
// SYNTHETIC: DX7HRTNL 0x100104ba
// SYNTHETIC: DX7HRTNL 0x100104e2
// SYNTHETIC: DX7HRTNL 0x100104ee
// SYNTHETIC: DX7HRTNL 0x100104fa
// SYNTHETIC: DX7HRTNL 0x10010506
