// The window layer's test program (`python -I tools/xbuild.py sdltest`): the game's future window path end to end on
// one system, without the game. It starts the window system of include/platform/PlatWindow.h (on SDL3), opens a
// window, hands GX the native handles the layer gives (HWND, X11 Window, Wayland surface or CAMetalLayer: the swap
// chain's surface is ee_gx's), presents --frames filled frames, pumps the events, and checks:
//   - an offscreen render target filled on the GPU and read back has the fill colour;
//   - text drawn by PlatFont (Liberation Sans as "Arial", --fonts DIR) into a CPU surface, uploaded and blitted on
//     the GPU into the target, reads back with its ink where it was drawn;
//   - the clipboard keeps a UTF-8 text (the game's, decision D6) through the system's clipboard;
//   - the key-state queries answer (nothing is held).
// Exit code: 0 pass, 1 a check failed, 2 no window system / GPU adapter (CI machines without a display or GPU:
// reported, not a failure of the layer).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>   // platform/wintypes.h elsewhere (RECT, HANDLE, the VK_* codes on Windows)

#include "gx/GX.h"
#include "platform/PlatFont.h"
#include "platform/PlatKeys.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"
#include "platform/PlatWindow.h"


static int sFailures;
#define CHECK(c) Check((c), #c, __LINE__)
static void Check(bool inOk, const char* inText, int inLine)
{
    if (!inOk) {
        sFailures++;
        printf("FAIL line %d: %s\n", inLine, inText);
    }
}

// GX's test hook (GX.h's harness seam): counts the frames GX was asked to present
extern "C" void GXTestSetPresentHook(void (*inHook)(void* inWindow, GXSurface* inFrame));
static int sPresented;
static void OnPresent(void*, GXSurface*)
{
    sPresented++;
}

static std::vector<std::vector<unsigned char>> sFontData;
static bool LoadFonts(const std::string& inDirectory)
{
    const char* kFiles[2] = {"LiberationSans-Regular.ttf", "LiberationSans-Bold.ttf"};
    for (int i = 0; i < 2; i++) {
        FILE* f = fopen((inDirectory + "/" + kFiles[i]).c_str(), "rb");
        if (!f)
            return false;
        std::vector<unsigned char> theData;
        unsigned char theBuffer[65536];
        size_t n;
        while ((n = fread(theBuffer, 1, sizeof(theBuffer), f)) > 0)
            theData.insert(theData.end(), theBuffer, theBuffer + n);
        fclose(f);
        sFontData.push_back(theData);
        if (!PlatFontRegister("Arial", i, 0, sFontData.back().data(), static_cast<uint32_t>(sFontData.back().size())))
            return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    int theFrames = 60;
    std::string theFonts;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            theFrames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fonts") && i + 1 < argc)
            theFonts = argv[++i];
    }
    if (!PlatWindowSystemStart()) {
        printf("no window system: %s\n", PlatWindowLastError());
        return 2;
    }
    PlatWindow* theWindow = PlatWindowCreate("Empire Earth: window layer test", 640, 480, PLAT_WINDOW_RESIZABLE);
    if (!theWindow) {
        printf("no window: %s\n", PlatWindowLastError());
        return 2;
    }
    PlatNativeWindow theNative;
    CHECK(PlatWindowGetNative(theWindow, &theNative));
    const char* kKinds[] = {"none", "win32", "x11", "wayland", "metal"};
    printf("native window: %s\n", kKinds[theNative.mKind]);
    int32_t theDisplayWidth = 0, theDisplayHeight = 0;
    if (PlatWindowGetDisplaySize(theWindow, &theDisplayWidth, &theDisplayHeight))
        printf("display: %dx%d\n", theDisplayWidth, theDisplayHeight);

    // clipboard and key state
    if (PlatClipboardSetText("Caf\xc3\xa9 \xe2\x82\xac 1")) {   // "Café € 1" (UTF-8, the game's text)
        char* theText = PlatClipboardGetText();
        CHECK(theText && strcmp(theText, "Caf\xc3\xa9 \xe2\x82\xac 1") == 0);
        free(theText);
    } else {
        printf("no clipboard here\n");
    }
    CHECK(!PlatKeyIsDown(VK_ESCAPE) && !PlatScanCodeIsDown(DIK_ESCAPE));
    PlatKeyIsToggled(VK_NUMLOCK);

    // GX on the window
    GXDirectDraw* theDirectDraw = NULL;
    if (GXDirectDrawCreate(NULL, &theDirectDraw) != GX_OK) {
        printf("no GPU adapter\n");
        return 2;
    }
    GXDEVICEIDENTIFIER theIdentifier;
    theDirectDraw->GetDeviceIdentifier(&theIdentifier, 0);
    printf("adapter: %s\n", theIdentifier.szDescription);
    CHECK(theDirectDraw->SetCooperativeLevel(theWindow, GXSCL_NORMAL) == GX_OK);
    GXTestSetPresentHook(OnPresent);

    // the offscreen check: fill on the GPU, copy PlatFont's text in, read back
    const uint32_t W = 256, H = 64;
    static const GXPIXELFORMAT kXRGB32 = {sizeof(GXPIXELFORMAT), GXPF_RGB, 0, 32, 0xff0000, 0xff00, 0xff, 0};
    GXSURFACEDESC theDescription = {};
    theDescription.dwSize = sizeof(theDescription);
    theDescription.dwFlags = GXSD_CAPS | GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT;
    theDescription.dwWidth = W;
    theDescription.dwHeight = H;
    theDescription.ddpfPixelFormat = kXRGB32;
    theDescription.ddsCaps.dwCaps = GXSCAPS_3DDEVICE;
    GXSurface* theTarget = NULL;
    CHECK(theDirectDraw->CreateSurface(&theDescription, &theTarget, NULL) == GX_OK);
    theDescription.dwWidth = 128;
    theDescription.dwHeight = 32;
    theDescription.ddsCaps.dwCaps = GXSCAPS_OFFSCREENPLAIN;
    GXSurface* theText = NULL;
    CHECK(theDirectDraw->CreateSurface(&theDescription, &theText, NULL) == GX_OK);
    bool theHaveText = !theFonts.empty() && LoadFonts(theFonts);
    if (!theFonts.empty())
        CHECK(theHaveText);
    GXBLTFX theFill = {sizeof(theFill), 0x004080BF};   // 0.25, 0.5, 0.75 -> 64, 128, 191
    CHECK(theTarget->Blt(NULL, NULL, NULL, GXBLT_COLORFILL, &theFill) == GX_OK);
    if (theHaveText) {
        GXSURFACEDESC theLocked;
        CHECK(theText->Lock(NULL, &theLocked, GXLOCK_WRITEONLY, NULL) == GX_OK);
        for (uint32_t y = 0; y < 32; y++)
            memset(static_cast<char*>(theLocked.lpSurface) + y * theLocked.lPitch, 0xff, 128 * 4);
        PlatFontTarget theDrawTarget = {theLocked.lpSurface, static_cast<int32_t>(theLocked.lPitch), 128, 32};
        PlatFont* theFont = PlatFontCreate("Arial", -16, 1, 0, 0, 1);
        PlatFontDraw(theFont, "Empire", 6, 4, 4, 0x000000, 0, 0, NULL, &theDrawTarget);
        PlatFontDestroy(theFont);
        CHECK(theText->Unlock(NULL) == GX_OK);
        RECT theDestination = {16, 16, 16 + 128, 16 + 32};
        CHECK(theTarget->Blt(&theDestination, theText, NULL, 0, NULL) == GX_OK);
    }
    GXSURFACEDESC theRead;
    CHECK(theTarget->Lock(NULL, &theRead, 0, NULL) == GX_OK);
    {
        const unsigned char* p = static_cast<const unsigned char*>(theRead.lpSurface);
        printf("read back: corner %d %d %d %d\n", p[0], p[1], p[2], p[3]);
        CHECK(abs(p[0] - 191) <= 1 && abs(p[1] - 128) <= 1 && abs(p[2] - 64) <= 1 && p[3] == 255);
        if (theHaveText) {
            int theInk = 0, theWhite = 0;
            for (uint32_t y = 16; y < 48; y++)
                for (uint32_t x = 16; x < 144; x++) {
                    const unsigned char* q = p + y * theRead.lPitch + x * 4;
                    if (q[0] < 64 && q[1] < 64 && q[2] < 64)
                        theInk++;
                    else if (q[0] == 255 && q[1] == 255 && q[2] == 255)
                        theWhite++;
                }
            printf("read back: text %d ink, %d white pixels\n", theInk, theWhite);
            CHECK(theInk > 50 && theWhite > 1000);
        }
    }
    CHECK(theTarget->Unlock(NULL) == GX_OK);

    // the window's frames: the primary with one back buffer, filled and flipped
    int32_t theWidth = 0, theHeight = 0;
    int theEvents = 0;
    bool theQuit = false;
    PlatWindowGetClientSize(theWindow, &theWidth, &theHeight);
    CHECK(theDirectDraw->SetDisplayMode(theWidth > 0 ? theWidth : 640, theHeight > 0 ? theHeight : 480, 32, 0, 0) == GX_OK);
    GXSURFACEDESC thePrimaryDescription = {};
    thePrimaryDescription.dwSize = sizeof(thePrimaryDescription);
    thePrimaryDescription.dwFlags = GXSD_CAPS | GXSD_BACKBUFFERCOUNT;
    thePrimaryDescription.dwBackBufferCount = 1;
    thePrimaryDescription.ddsCaps.dwCaps = GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP | GXSCAPS_COMPLEX;
    GXSurface* thePrimary = NULL;
    CHECK(theDirectDraw->CreateSurface(&thePrimaryDescription, &thePrimary, NULL) == GX_OK);
    GXSCAPS theBackCaps = {GXSCAPS_FLIP, 0};
    GXSurface* theBack = NULL;
    CHECK(thePrimary && thePrimary->GetAttachedSurface(&theBackCaps, &theBack) == GX_OK);
    uint32_t theStart = PlatTimeMilliseconds();
    for (int f = 0; f < theFrames && !theQuit; f++) {
        PlatInputEvent e;
        while (PlatWindowPollEvent(&e)) {
            theEvents++;
            if (e.mType == PLAT_EVENT_QUIT || e.mType == PLAT_EVENT_CLOSE)
                theQuit = true;
            if (e.mType == PLAT_EVENT_KEY_DOWN)
                printf("key: DIK %02x VK %02x\n", e.mKey.mScanCode, e.mKey.mVirtualKey);
        }
        PlatWindowGetClientSize(theWindow, &theWidth, &theHeight);
        if (theWidth <= 0 || theHeight <= 0)
            continue;
        GXBLTFX theFrameFill = {sizeof(theFrameFill), static_cast<unsigned long>((26 + (f % 30) * 102 / 30) << 16 | 51 << 8 | 77)};
        CHECK(theBack->Blt(NULL, NULL, NULL, GXBLT_COLORFILL, &theFrameFill) == GX_OK);
        CHECK(thePrimary->Flip(NULL, 0) == GX_OK);
    }
    printf("%d frames presented at %dx%d in %u ms, %d events\n", sPresented, theWidth, theHeight,
           PlatTimeMilliseconds() - theStart, theEvents);
    CHECK(sPresented > 0 || theQuit);

    if (theBack)
        theBack->Release();
    if (thePrimary)
        thePrimary->Release();
    theText->Release();
    theTarget->Release();
    theDirectDraw->Release();
    PlatWindowDestroy(theWindow);
    PlatWindowSystemStop();
    printf("%s\n", sFailures ? "FAILED" : "PASS");
    return sFailures ? 1 : 0;
}
