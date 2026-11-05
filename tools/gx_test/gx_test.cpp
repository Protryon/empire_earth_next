// GX self-test (include/gx/GX.h on wgpu): exercises the renderer's DirectDraw/Direct3D 7 subset without the game and
// checks the results by read-back. Prints one line per check and PASS/FAIL. Not part of the game.
//
//   gx_test.exe [--frames N]      (EE_GX_LOG=1: wgpu's log on stderr)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gx/GX.h"

static int gFailures;

static void Check(bool inOK, const char* inWhat, unsigned long inGot, unsigned long inWant)
{
    printf("%s %s (got %08lx, want %08lx)\n", inOK ? "ok  " : "FAIL", inWhat, inGot, inWant);
    if (!inOK)
        gFailures++;
}

static bool Near(unsigned long a, unsigned long b, int inTolerance)
{
    for (int i = 0; i < 32; i += 8)
        if (abs((int)((a >> i) & 0xff) - (int)((b >> i) & 0xff)) > inTolerance)
            return false;
    return true;
}

static unsigned long Pixel(GXSurface* inSurface, long x, long y)
{
    GXSURFACEDESC theDesc = {};
    theDesc.dwSize = sizeof(theDesc);
    if (inSurface->Lock(NULL, &theDesc, 0, NULL) != GX_OK)
        return 0xdeadbeef;
    unsigned long theValue = theDesc.ddpfPixelFormat.dwRGBBitCount == 32 ?
        ((unsigned long*)((unsigned char*)theDesc.lpSurface + y * theDesc.lPitch))[x] :
        ((unsigned short*)((unsigned char*)theDesc.lpSurface + y * theDesc.lPitch))[x];
    inSurface->Unlock(NULL);
    return theValue & (theDesc.ddpfPixelFormat.dwRGBBitCount == 32 ? 0x00ffffff : 0xffff);
}

static GXSurface* Create(GXDirectDraw* inDD, unsigned long inCaps, unsigned long w, unsigned long h, unsigned long inBits,
                         bool inAlpha)
{
    GXSURFACEDESC theDesc = {};
    theDesc.dwSize = sizeof(theDesc);
    theDesc.dwFlags = GXSD_CAPS | GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT;
    theDesc.dwWidth = w;
    theDesc.dwHeight = h;
    theDesc.ddsCaps.dwCaps = inCaps;
    GXPIXELFORMAT& f = theDesc.ddpfPixelFormat;
    f.dwSize = sizeof(f);
    if (inCaps & GXSCAPS_ZBUFFER) {
        f.dwFlags = GXPF_ZBUFFER;
        f.dwRGBBitCount = 32;
    } else if (inBits == 32) {
        f.dwFlags = GXPF_RGB | (inAlpha ? GXPF_ALPHAPIXELS : 0);
        f.dwRGBBitCount = 32;
        f.dwRBitMask = 0xff0000;
        f.dwGBitMask = 0xff00;
        f.dwBBitMask = 0xff;
        f.dwRGBAlphaBitMask = inAlpha ? 0xff000000 : 0;
    } else {
        f.dwFlags = GXPF_RGB;
        f.dwRGBBitCount = 16;
        f.dwRBitMask = 0xf800;
        f.dwGBitMask = 0x07e0;
        f.dwBBitMask = 0x001f;
    }
    GXSurface* theSurface = NULL;
    if (inDD->CreateSurface(&theDesc, &theSurface, NULL) != GX_OK)
        printf("FAIL CreateSurface caps %08lx\n", inCaps);
    return theSurface;
}

static LRESULT CALLBACK WindowProc(HWND inWindow, UINT inMessage, WPARAM inW, LPARAM inL)
{
    return DefWindowProcA(inWindow, inMessage, inW, inL);
}

struct Vertex {   // the renderer's D3DVERTEX_STD_COLOR
    float x, y, z, nx, ny, nz;
    unsigned long color;
    float tu, tv;
};

static bool DrawHello(HDC inDC, void*)
{
    SetBkMode(inDC, TRANSPARENT);
    SetTextColor(inDC, RGB(255, 255, 0));
    return ExtTextOutA(inDC, 4, 4, 0, NULL, "Hello", 5, NULL) != 0;
}

int main(int argc, char** argv)
{
    int theFrames = 30;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            theFrames = atoi(argv[++i]);
    WNDCLASSA theClass = {};
    theClass.lpfnWndProc = WindowProc;
    theClass.hInstance = GetModuleHandleA(NULL);
    theClass.lpszClassName = "gxtest";
    RegisterClassA(&theClass);
    HWND theWindow = CreateWindowA("gxtest", "GX test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 300, 250, NULL, NULL,
                                   theClass.hInstance, NULL);
    GXDirectDraw* theDD = NULL;
    if (GXDirectDrawCreate(NULL, &theDD) != GX_OK) {
        printf("FAIL no GX device\n");
        return 1;
    }
    theDD->SetCooperativeLevel(theWindow, GXSCL_NORMAL);
    GXSURFACEDESC thePrimaryDesc = {};
    thePrimaryDesc.dwSize = sizeof(thePrimaryDesc);
    thePrimaryDesc.dwFlags = GXSD_CAPS;
    thePrimaryDesc.ddsCaps.dwCaps = GXSCAPS_PRIMARYSURFACE;
    GXSurface* thePrimary = NULL;
    theDD->CreateSurface(&thePrimaryDesc, &thePrimary, NULL);
    GXSurface* theBack = Create(theDD, GXSCAPS_OFFSCREENPLAIN | GXSCAPS_3DDEVICE, 64, 64, 32, false);
    GXSurface* theZ = Create(theDD, GXSCAPS_ZBUFFER, 64, 64, 32, false);
    theBack->AddAttachedSurface(theZ);
    GXSurface* theTexture = Create(theDD, GXSCAPS_TEXTURE, 16, 16, 32, true);
    GXSurface* theSprite = Create(theDD, GXSCAPS_OFFSCREENPLAIN, 8, 8, 16, false);

    // 1. colour fill on the GPU-side back buffer
    GXBLTFX theFX = {sizeof(GXBLTFX), 0x00336699};
    theBack->Blt(NULL, NULL, NULL, GXBLT_COLORFILL, &theFX);
    Check(Pixel(theBack, 10, 10) == 0x336699, "colour fill (GPU)", Pixel(theBack, 10, 10), 0x336699);

    // 2. a CPU-side 565 sprite with a colour key, blitted keyed onto the back buffer (GPU) and onto a CPU surface
    GXSURFACEDESC theDesc = {};
    theDesc.dwSize = sizeof(theDesc);
    theSprite->Lock(NULL, &theDesc, 0, NULL);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            ((unsigned short*)((unsigned char*)theDesc.lpSurface + y * theDesc.lPitch))[x] = x < 4 ? 0xf800 : 0x001f;
    theSprite->Unlock(NULL);
    GXCOLORKEY theKey = {0x001f, 0x001f};
    theSprite->SetColorKey(GXCKEY_SRCBLT, &theKey);
    RECT theDest = {20, 20, 28, 28};
    theBack->Blt(&theDest, theSprite, NULL, GXBLT_KEYSRC, NULL);
    Check(Pixel(theBack, 21, 21) == 0xff0000, "keyed blit, opaque texel", Pixel(theBack, 21, 21), 0xff0000);
    Check(Pixel(theBack, 26, 21) == 0x336699, "keyed blit, keyed texel", Pixel(theBack, 26, 21), 0x336699);
    GXSurface* theCPU = Create(theDD, GXSCAPS_OFFSCREENPLAIN, 16, 16, 16, false);
    GXBLTFX theGreen = {sizeof(GXBLTFX), 0x07e0};
    theCPU->Blt(NULL, NULL, NULL, GXBLT_COLORFILL, &theGreen);
    RECT theDest2 = {0, 0, 8, 8};
    theCPU->Blt(&theDest2, theSprite, NULL, GXBLT_KEYSRC, NULL);
    Check(Pixel(theCPU, 1, 1) == 0xf800 && Pixel(theCPU, 6, 1) == 0x07e0, "keyed blit (CPU)", Pixel(theCPU, 6, 1), 0x07e0);

    // 3. a textured, vertex-coloured quad in 2D mode (identity world/view, pixel projection)
    theTexture->Lock(NULL, &theDesc, 0, NULL);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            ((unsigned long*)((unsigned char*)theDesc.lpSurface + y * theDesc.lPitch))[x] = 0xff00ff00;
    theTexture->Unlock(NULL);
    GXDevice* theDevice = NULL;
    theDD->CreateDevice(GXIID_HALDevice, theBack, &theDevice);
    GXMATRIX theIdentity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    GXMATRIX theProjection = theIdentity;
    theProjection._11 = 2.0f / 64;
    theProjection._41 = -1.0f;
    theProjection._22 = -2.0f / 64;
    theProjection._42 = 1.0f;
    theDevice->SetTransform(GXTS_WORLD, &theIdentity);
    theDevice->SetTransform(GXTS_VIEW, &theIdentity);
    theDevice->SetTransform(GXTS_PROJECTION, &theProjection);
    theDevice->SetRenderState(GXRS_LIGHTING, FALSE);
    theDevice->SetRenderState(GXRS_ZENABLE, FALSE);
    theDevice->SetRenderState(GXRS_CULLMODE, GXCULL_NONE);
    theDevice->SetTexture(0, theTexture);
    theDevice->SetTextureStageState(0, GXTSS_COLOROP, GXTOP_MODULATE);
    theDevice->SetTextureStageState(0, GXTSS_COLORARG1, GXTA_TEXTURE);
    theDevice->SetTextureStageState(0, GXTSS_COLORARG2, GXTA_DIFFUSE);
    GXVERTEXBUFFERDESC theVBDesc = {sizeof(theVBDesc), 0, GXFVF_XYZ | GXFVF_NORMAL | GXFVF_DIFFUSE | GXFVF_TEX1, 64};
    GXVertexBuffer* theVB = NULL;
    theDD->CreateVertexBuffer(&theVBDesc, &theVB, 0);
    Vertex* v = NULL;
    theVB->Lock(0, (void**)&v, NULL);
    const float q[4][2] = {{40, 40}, {60, 40}, {40, 60}, {60, 60}};
    for (int i = 0; i < 4; i++) {
        Vertex theVertex = {q[i][0], q[i][1], 0.5f, 0, 0, -1, 0xff8080ff, (float)(i & 1), (float)(i >> 1)};
        v[i] = theVertex;
    }
    theVB->Unlock();
    unsigned short theIndices[6] = {0, 1, 2, 2, 1, 3};
    theDevice->BeginScene();
    theDevice->DrawIndexedPrimitiveVB(GXPT_TRIANGLELIST, theVB, 0, 4, theIndices, 6, 0);
    theDevice->EndScene();
    unsigned long theQuad = Pixel(theBack, 50, 50);
    Check(Near(theQuad, 0x008000, 2), "textured quad (green x vertex colour)", theQuad, 0x008000);
    Check(Pixel(theBack, 39, 50) == 0x336699 && Pixel(theBack, 40, 50) != 0x336699, "quad left edge at x = 40 (pixel centres)",
          Pixel(theBack, 40, 50), 0x008000);

    // 4. lit draw: one directional light from the camera, white material
    theDevice->SetRenderState(GXRS_LIGHTING, TRUE);
    theDevice->SetRenderState(GXRS_COLORVERTEX, FALSE);
    theDevice->SetRenderState(GXRS_DIFFUSEMATERIALSOURCE, GXMCS_MATERIAL);
    theDevice->SetTexture(0, NULL);
    theDevice->SetTextureStageState(0, GXTSS_COLOROP, GXTOP_DISABLE);
    GXMATERIAL theMaterial = {};
    theMaterial.dcvDiffuse.r = 1.0f;
    theMaterial.dcvDiffuse.g = 0.5f;
    theMaterial.dcvDiffuse.a = 1.0f;
    theDevice->SetMaterial(&theMaterial);
    GXLIGHT theLight = {};
    theLight.dltType = GXLIGHT_DIRECTIONAL;
    theLight.dcvDiffuse.r = theLight.dcvDiffuse.g = theLight.dcvDiffuse.b = 1.0f;
    theLight.dvDirection.z = 1.0f;   // travels away from the viewer: lights normals facing -z
    theDevice->SetLight(0, &theLight);
    theDevice->LightEnable(0, TRUE);
    theVB->Lock(0, (void**)&v, NULL);
    for (int i = 0; i < 4; i++) {
        v[i].x -= 30;
        v[i].y -= 30;
    }
    theVB->Unlock();
    theDevice->DrawIndexedPrimitiveVB(GXPT_TRIANGLELIST, theVB, 0, 4, theIndices, 6, 0);
    unsigned long theLit = Pixel(theBack, 20, 20);
    Check(Near(theLit, 0xff8000, 2), "lit quad (material diffuse x light)", theLit, 0xff8000);

    // 5. z: a far quad behind a near one
    theDevice->SetRenderState(GXRS_LIGHTING, FALSE);
    theDevice->SetRenderState(GXRS_ZENABLE, TRUE);
    theDevice->SetRenderState(GXRS_ZWRITEENABLE, TRUE);
    theDevice->SetRenderState(GXRS_ZFUNC, GXCMP_LESSEQUAL);
    GXRECT theAll = {0, 0, 64, 64};
    theDevice->Clear(1, &theAll, GXCLEAR_ZBUFFER, 0, 1.0f, 0);
    theVB->Lock(0, (void**)&v, NULL);
    const float z[2] = {0.2f, 0.8f};
    const unsigned long c[2] = {0xffff0000, 0xff0000ff};
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 4; i++) {
            Vertex theVertex = {q[i][0] - 40 + 2, q[i][1] - 40 + 40, z[k], 0, 0, -1, c[k], 0, 0};
            v[4 + k * 4 + i] = theVertex;
        }
    theVB->Unlock();
    theDevice->DrawIndexedPrimitiveVB(GXPT_TRIANGLELIST, theVB, 4, 4, theIndices, 6, 0);
    theDevice->DrawIndexedPrimitiveVB(GXPT_TRIANGLELIST, theVB, 8, 4, theIndices, 6, 0);
    Check(Pixel(theBack, 10, 50) == 0xff0000, "z test keeps the near quad", Pixel(theBack, 10, 50), 0xff0000);

    // 6. GDI text on the GPU-side back buffer (coverage from two grounds) and on a CPU surface through GetDC
    RECT theText = {0, 0, 64, 24};
    theBack->DrawText(theText, DrawHello, NULL);
    bool theYellow = false;
    for (int y = 4; y < 20 && !theYellow; y++)
        for (int x = 4; x < 40 && !theYellow; x++)
            theYellow = Pixel(theBack, x, y) == 0xffff00;
    Check(theYellow, "GPU text has full-coverage yellow pixels", 0, 0);
    HDC theDC = NULL;
    GXSurface* theTextSurface = Create(theDD, GXSCAPS_OFFSCREENPLAIN, 64, 24, 16, false);
    if (theTextSurface->GetDC(&theDC) == GX_OK) {
        DrawHello(theDC, NULL);
        theTextSurface->ReleaseDC(theDC);
        bool theAny = false;
        for (int y = 4; y < 20 && !theAny; y++)
            for (int x = 4; x < 40 && !theAny; x++)
                theAny = Pixel(theTextSurface, x, y) == 0xffe0;
        Check(theAny, "GetDC text on a 565 surface", 0, 0);
    } else
        Check(false, "GetDC", 0, 0);

    // 7. present to the window
    for (int f = 0; f < theFrames; f++) {
        MSG theMessage;
        while (PeekMessageA(&theMessage, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&theMessage);
            DispatchMessageA(&theMessage);
        }
        RECT theScreen = {0, 0, 64, 64};
        thePrimary->Blt(&theScreen, theBack, NULL, 0, NULL);
    }
    printf("presented %d frames\n", theFrames);
    theVB->Release();
    theDevice->Release();
    theTextSurface->Release();
    theCPU->Release();
    theSprite->Release();
    theTexture->Release();
    theZ->Release();
    theBack->Release();
    thePrimary->Release();
    theDD->Release();
    printf("%s (%d failed)\n", gFailures ? "FAIL" : "PASS", gFailures);
    return gFailures ? 1 : 0;
}
