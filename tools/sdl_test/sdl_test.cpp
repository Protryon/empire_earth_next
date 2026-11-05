// The window layer's test program (`python -I tools/xbuild.py sdltest`): the game's future window path end to end on
// one system, without the game. It starts the window system of include/platform/PlatWindow.h (on SDL3), opens a
// window, builds a wgpu surface from the native handles the layer hands out (HWND, X11 Window, Wayland surface or
// CAMetalLayer, as GX will), presents --frames cleared frames, pumps the events, and checks:
//   - an offscreen render target cleared on the GPU and read back has the clear colour;
//   - text drawn by PlatFont (Liberation Sans as "Arial", --fonts DIR) into a CPU buffer, uploaded to a texture and
//     copied on the GPU into the target, reads back with its ink where it was drawn;
//   - the clipboard keeps a Windows-1252 text through the system's UTF-8 clipboard;
//   - the key-state queries answer (nothing is held).
// Exit code: 0 pass, 1 a check failed, 2 no window system / GPU adapter (CI machines without a display or GPU:
// reported, not a failure of the layer).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include "platform/PlatFont.h"
#include "platform/PlatKeys.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"
#include "platform/PlatWindow.h"

#if defined(_WIN32)
#include <windows.h>   // the VK_* codes on Windows
#endif

static int sFailures;
#define CHECK(c) Check((c), #c, __LINE__)
static void Check(bool inOk, const char* inText, int inLine)
{
    if (!inOk) {
        sFailures++;
        printf("FAIL line %d: %s\n", inLine, inText);
    }
}

struct Wait {
    int mDone;
    void* mResult;
};

static void OnAdapter(WGPURequestAdapterStatus inStatus, WGPUAdapter inAdapter, WGPUStringView inMessage, void* inWait,
                      void*)
{
    Wait* w = static_cast<Wait*>(inWait);
    w->mResult = inStatus == WGPURequestAdapterStatus_Success ? inAdapter : NULL;
    if (inStatus != WGPURequestAdapterStatus_Success)
        printf("adapter: %.*s\n", (int)(inMessage.length == WGPU_STRLEN ? strlen(inMessage.data) : inMessage.length),
               inMessage.data ? inMessage.data : "");
    w->mDone = 1;
}

static void OnDevice(WGPURequestDeviceStatus inStatus, WGPUDevice inDevice, WGPUStringView, void* inWait, void*)
{
    Wait* w = static_cast<Wait*>(inWait);
    w->mResult = inStatus == WGPURequestDeviceStatus_Success ? inDevice : NULL;
    w->mDone = 1;
}

static void OnMapped(WGPUMapAsyncStatus inStatus, WGPUStringView, void* inWait, void*)
{
    Wait* w = static_cast<Wait*>(inWait);
    w->mResult = reinterpret_cast<void*>(static_cast<intptr_t>(inStatus == WGPUMapAsyncStatus_Success ? 1 : 0));
    w->mDone = 1;
}

static void Pump(WGPUInstance inInstance, WGPUDevice inDevice, Wait& w)
{
    for (int i = 0; i < 10000 && !w.mDone; i++) {
        if (inDevice)
            wgpuDevicePoll(inDevice, 0, NULL);
        wgpuInstanceProcessEvents(inInstance);
        if (!w.mDone)
            PlatSleep(1);
    }
}

static WGPUSurface CreateSurface(WGPUInstance inInstance, const PlatNativeWindow& n)
{
    WGPUSurfaceDescriptor d = {};
    WGPUSurfaceSourceWindowsHWND theWin32 = {};
    WGPUSurfaceSourceXlibWindow theX11 = {};
    WGPUSurfaceSourceWaylandSurface theWayland = {};
    WGPUSurfaceSourceMetalLayer theMetal = {};
    switch (n.mKind) {
    case PLAT_NATIVE_WIN32:
        theWin32.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        theWin32.hinstance = n.mDisplay;
        theWin32.hwnd = n.mWindow;
        d.nextInChain = &theWin32.chain;
        break;
    case PLAT_NATIVE_X11:
        theX11.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
        theX11.display = n.mDisplay;
        theX11.window = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(n.mWindow));
        d.nextInChain = &theX11.chain;
        break;
    case PLAT_NATIVE_WAYLAND:
        theWayland.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
        theWayland.display = n.mDisplay;
        theWayland.surface = n.mWindow;
        d.nextInChain = &theWayland.chain;
        break;
    case PLAT_NATIVE_METAL:
        theMetal.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
        theMetal.layer = n.mWindow;
        d.nextInChain = &theMetal.chain;
        break;
    default:
        return NULL;
    }
    return wgpuInstanceCreateSurface(inInstance, &d);
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
    if (PlatClipboardSetText("Caf\xe9 \x80 1")) {   // "Café € 1" in Windows-1252
        char* theText = PlatClipboardGetText();
        CHECK(theText && strcmp(theText, "Caf\xe9 \x80 1") == 0);
        free(theText);
    } else {
        printf("no clipboard here\n");
    }
    CHECK(!PlatKeyIsDown(VK_ESCAPE) && !PlatScanCodeIsDown(DIK_ESCAPE));
    PlatKeyIsToggled(VK_NUMLOCK);

    // wgpu on the window
    WGPUInstanceDescriptor theInstanceDescriptor = {};
    WGPUInstance theInstance = wgpuCreateInstance(&theInstanceDescriptor);
    WGPUSurface theSurface = CreateSurface(theInstance, theNative);
    CHECK(theSurface != NULL);
    WGPURequestAdapterOptions theOptions = {};
    theOptions.compatibleSurface = theSurface;
    theOptions.powerPreference = WGPUPowerPreference_HighPerformance;
    Wait w = {0, NULL};
    WGPURequestAdapterCallbackInfo theAdapterCallback = {};
    theAdapterCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    theAdapterCallback.callback = OnAdapter;
    theAdapterCallback.userdata1 = &w;
    wgpuInstanceRequestAdapter(theInstance, &theOptions, theAdapterCallback);
    Pump(theInstance, NULL, w);
    WGPUAdapter theAdapter = static_cast<WGPUAdapter>(w.mResult);
    if (!theAdapter) {
        printf("no GPU adapter for this surface\n");
        return 2;
    }
    WGPUAdapterInfo theInfo = {};
    wgpuAdapterGetInfo(theAdapter, &theInfo);
    printf("adapter: %.*s (backend %d)\n",
           (int)(theInfo.device.length == WGPU_STRLEN ? strlen(theInfo.device.data) : theInfo.device.length),
           theInfo.device.data ? theInfo.device.data : "", (int)theInfo.backendType);
    w = Wait{0, NULL};
    WGPUDeviceDescriptor theDeviceDescriptor = {};
    WGPURequestDeviceCallbackInfo theDeviceCallback = {};
    theDeviceCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    theDeviceCallback.callback = OnDevice;
    theDeviceCallback.userdata1 = &w;
    wgpuAdapterRequestDevice(theAdapter, &theDeviceDescriptor, theDeviceCallback);
    Pump(theInstance, NULL, w);
    WGPUDevice theDevice = static_cast<WGPUDevice>(w.mResult);
    if (!theDevice) {
        printf("no device\n");
        return 2;
    }
    WGPUQueue theQueue = wgpuDeviceGetQueue(theDevice);

    // the offscreen check: clear on the GPU, copy PlatFont's text in, read back
    const uint32_t W = 256, H = 64;
    WGPUTextureDescriptor theTargetDescriptor = {};
    theTargetDescriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
    theTargetDescriptor.dimension = WGPUTextureDimension_2D;
    theTargetDescriptor.size = {W, H, 1};
    theTargetDescriptor.format = WGPUTextureFormat_BGRA8Unorm;
    theTargetDescriptor.mipLevelCount = 1;
    theTargetDescriptor.sampleCount = 1;
    WGPUTexture theTarget = wgpuDeviceCreateTexture(theDevice, &theTargetDescriptor);
    WGPUTextureDescriptor theTextDescriptor = theTargetDescriptor;
    theTextDescriptor.usage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
    theTextDescriptor.size = {128, 32, 1};
    WGPUTexture theTextTexture = wgpuDeviceCreateTexture(theDevice, &theTextDescriptor);
    bool theHaveText = !theFonts.empty() && LoadFonts(theFonts);
    if (!theFonts.empty())
        CHECK(theHaveText);
    if (theHaveText) {
        std::vector<uint32_t> thePixels(128 * 32, 0xFFFFFFFFu);
        PlatFontTarget theText = {thePixels.data(), 128 * 4, 128, 32};
        PlatFont* theFont = PlatFontCreate("Arial", -16, 1, 0, 0, 1);
        PlatFontDraw(theFont, "Empire", 6, 4, 4, 0x000000, 0, 0, NULL, &theText);
        PlatFontDestroy(theFont);
        WGPUTexelCopyTextureInfo theDestination = {};
        theDestination.texture = theTextTexture;
        WGPUTexelCopyBufferLayout theLayout = {0, 128 * 4, 32};
        WGPUExtent3D theSize = {128, 32, 1};
        wgpuQueueWriteTexture(theQueue, &theDestination, thePixels.data(), thePixels.size() * 4, &theLayout, &theSize);
    }
    WGPUCommandEncoder theEncoder = wgpuDeviceCreateCommandEncoder(theDevice, NULL);
    WGPUTextureView theTargetView = wgpuTextureCreateView(theTarget, NULL);
    WGPURenderPassColorAttachment theClear = {};
    theClear.view = theTargetView;
    theClear.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    theClear.loadOp = WGPULoadOp_Clear;
    theClear.storeOp = WGPUStoreOp_Store;
    theClear.clearValue = {0.25, 0.5, 0.75, 1.0};
    WGPURenderPassDescriptor thePass = {};
    thePass.colorAttachmentCount = 1;
    thePass.colorAttachments = &theClear;
    WGPURenderPassEncoder theRender = wgpuCommandEncoderBeginRenderPass(theEncoder, &thePass);
    wgpuRenderPassEncoderEnd(theRender);
    wgpuRenderPassEncoderRelease(theRender);
    if (theHaveText) {
        WGPUTexelCopyTextureInfo theFrom = {}, theTo = {};
        theFrom.texture = theTextTexture;
        theTo.texture = theTarget;
        theTo.origin = {16, 16, 0};
        WGPUExtent3D theSize = {128, 32, 1};
        wgpuCommandEncoderCopyTextureToTexture(theEncoder, &theFrom, &theTo, &theSize);
    }
    WGPUBufferDescriptor theBufferDescriptor = {};
    theBufferDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    theBufferDescriptor.size = W * H * 4;
    WGPUBuffer theReadBack = wgpuDeviceCreateBuffer(theDevice, &theBufferDescriptor);
    WGPUTexelCopyTextureInfo theSource = {};
    theSource.texture = theTarget;
    WGPUTexelCopyBufferInfo theCopy = {};
    theCopy.buffer = theReadBack;
    theCopy.layout.bytesPerRow = W * 4;
    theCopy.layout.rowsPerImage = H;
    WGPUExtent3D theAll = {W, H, 1};
    wgpuCommandEncoderCopyTextureToBuffer(theEncoder, &theSource, &theCopy, &theAll);
    WGPUCommandBuffer theCommands = wgpuCommandEncoderFinish(theEncoder, NULL);
    wgpuQueueSubmit(theQueue, 1, &theCommands);
    wgpuCommandBufferRelease(theCommands);
    wgpuCommandEncoderRelease(theEncoder);
    w = Wait{0, NULL};
    WGPUBufferMapCallbackInfo theMapCallback = {};
    theMapCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    theMapCallback.callback = OnMapped;
    theMapCallback.userdata1 = &w;
    wgpuBufferMapAsync(theReadBack, WGPUMapMode_Read, 0, W * H * 4, theMapCallback);
    Pump(theInstance, theDevice, w);
    CHECK(w.mDone && w.mResult);
    if (w.mDone && w.mResult) {
        const unsigned char* p = static_cast<const unsigned char*>(wgpuBufferGetConstMappedRange(theReadBack, 0, W * H * 4));
        // BGRA: the clear colour at a corner (0.25, 0.5, 0.75 -> 64, 128, 191 within rounding)
        printf("read back: corner %d %d %d %d\n", p[0], p[1], p[2], p[3]);
        CHECK(abs(p[0] - 191) <= 1 && abs(p[1] - 128) <= 1 && abs(p[2] - 64) <= 1 && p[3] == 255);
        if (theHaveText) {
            int theInk = 0, theWhite = 0;
            for (uint32_t y = 16; y < 48; y++)
                for (uint32_t x = 16; x < 144; x++) {
                    const unsigned char* q = p + (y * W + x) * 4;
                    if (q[0] < 64 && q[1] < 64 && q[2] < 64)
                        theInk++;
                    else if (q[0] == 255 && q[1] == 255 && q[2] == 255)
                        theWhite++;
                }
            printf("read back: text %d ink, %d white pixels\n", theInk, theWhite);
            CHECK(theInk > 50 && theWhite > 1000);
        }
        wgpuBufferUnmap(theReadBack);
    }

    // the window's frames
    WGPUSurfaceCapabilities theCaps = {};
    wgpuSurfaceGetCapabilities(theSurface, theAdapter, &theCaps);
    WGPUTextureFormat theFormat = theCaps.formatCount ? theCaps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    wgpuSurfaceCapabilitiesFreeMembers(theCaps);
    int32_t theWidth = 0, theHeight = 0, theConfigured = -1;
    int theEvents = 0, thePresented = 0;
    bool theQuit = false;
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
        if (theConfigured != theWidth * 65536 + theHeight) {
            WGPUSurfaceConfiguration theConfig = {};
            theConfig.device = theDevice;
            theConfig.format = theFormat;
            theConfig.usage = WGPUTextureUsage_RenderAttachment;
            theConfig.width = static_cast<uint32_t>(theWidth);
            theConfig.height = static_cast<uint32_t>(theHeight);
            theConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
            theConfig.presentMode = WGPUPresentMode_Fifo;
            wgpuSurfaceConfigure(theSurface, &theConfig);
            theConfigured = theWidth * 65536 + theHeight;
        }
        WGPUSurfaceTexture theFrame = {};
        wgpuSurfaceGetCurrentTexture(theSurface, &theFrame);
        if (theFrame.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
            theFrame.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
            if (theFrame.texture)
                wgpuTextureRelease(theFrame.texture);
            theConfigured = -1;
            continue;
        }
        WGPUTextureView theView = wgpuTextureCreateView(theFrame.texture, NULL);
        WGPUCommandEncoder theFrameEncoder = wgpuDeviceCreateCommandEncoder(theDevice, NULL);
        WGPURenderPassColorAttachment theColor = theClear;
        theColor.view = theView;
        theColor.clearValue = {0.1 + 0.4 * (f % 30) / 30.0, 0.2, 0.3, 1.0};
        WGPURenderPassDescriptor theFramePass = {};
        theFramePass.colorAttachmentCount = 1;
        theFramePass.colorAttachments = &theColor;
        WGPURenderPassEncoder theFrameRender = wgpuCommandEncoderBeginRenderPass(theFrameEncoder, &theFramePass);
        wgpuRenderPassEncoderEnd(theFrameRender);
        wgpuRenderPassEncoderRelease(theFrameRender);
        WGPUCommandBuffer theFrameCommands = wgpuCommandEncoderFinish(theFrameEncoder, NULL);
        wgpuQueueSubmit(theQueue, 1, &theFrameCommands);
        wgpuCommandBufferRelease(theFrameCommands);
        wgpuCommandEncoderRelease(theFrameEncoder);
        if (wgpuSurfacePresent(theSurface) == WGPUStatus_Success)
            thePresented++;
        wgpuTextureViewRelease(theView);
        wgpuTextureRelease(theFrame.texture);
    }
    printf("%d frames presented at %dx%d in %u ms, %d events\n", thePresented, theWidth, theHeight,
           PlatTimeMilliseconds() - theStart, theEvents);
    CHECK(thePresented > 0 || theQuit);

    wgpuBufferRelease(theReadBack);
    wgpuTextureViewRelease(theTargetView);
    wgpuTextureRelease(theTarget);
    wgpuTextureRelease(theTextTexture);
    wgpuSurfaceRelease(theSurface);
    wgpuQueueRelease(theQueue);
    wgpuDeviceRelease(theDevice);
    wgpuAdapterRelease(theAdapter);
    wgpuInstanceRelease(theInstance);
    PlatWindowDestroy(theWindow);
    PlatWindowSystemStop();
    printf("%s\n", sFailures ? "FAILED" : "PASS");
    return sFailures ? 1 : 0;
}
