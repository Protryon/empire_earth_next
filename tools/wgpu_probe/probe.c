/* wgpu phase-0 probe (PORT.md "Plan: DirectX -> wgpu"): links wgpu-native statically with the build's static C
   runtime, opens a window, renders and presents a few cleared frames, and checks one offscreen clear by reading it
   back. Prints the adapter, the surface format and PASS/FAIL. Not part of the game.

     probe.exe [--fallback] [--dx12|--vulkan|--gl] [--frames N]

   --fallback asks for the software adapter (WARP on DX12), as on a machine without a GPU. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webgpu/webgpu.h"
#include "webgpu/wgpu.h"

static WGPUAdapter gAdapter;
static WGPUDevice gDevice;
static int gAdapterDone, gDeviceDone, gMapDone;

static void Print(const char* inWhat, WGPUStringView inText)
{
    printf("%s%.*s\n", inWhat, (int)(inText.data ? (inText.length == WGPU_STRLEN ? strlen(inText.data) : inText.length) : 0),
           inText.data ? inText.data : "");
}

static void OnAdapter(WGPURequestAdapterStatus inStatus, WGPUAdapter inAdapter, WGPUStringView inMessage, void* a, void* b)
{
    if (inStatus == WGPURequestAdapterStatus_Success)
        gAdapter = inAdapter;
    else
        Print("adapter request failed: ", inMessage);
    gAdapterDone = 1;
}

static void OnDevice(WGPURequestDeviceStatus inStatus, WGPUDevice inDevice, WGPUStringView inMessage, void* a, void* b)
{
    if (inStatus == WGPURequestDeviceStatus_Success)
        gDevice = inDevice;
    else
        Print("device request failed: ", inMessage);
    gDeviceDone = 1;
}

static void OnError(WGPUDevice const* inDevice, WGPUErrorType inType, WGPUStringView inMessage, void* a, void* b)
{
    printf("wgpu error %d: ", (int)inType);
    Print("", inMessage);
}

static void OnMap(WGPUMapAsyncStatus inStatus, WGPUStringView inMessage, void* a, void* b)
{
    gMapDone = inStatus == WGPUMapAsyncStatus_Success ? 1 : -1;
    if (gMapDone < 0)
        Print("map failed: ", inMessage);
}

static LRESULT CALLBACK WindowProc(HWND inWindow, UINT inMessage, WPARAM inW, LPARAM inL)
{
    return DefWindowProcA(inWindow, inMessage, inW, inL);
}

static void Pump(void)
{
    MSG theMessage;
    while (PeekMessageA(&theMessage, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&theMessage);
        DispatchMessageA(&theMessage);
    }
}

static void Clear(WGPUQueue inQueue, WGPUTextureView inView, double r, double g, double b)
{
    WGPUCommandEncoder theEncoder = wgpuDeviceCreateCommandEncoder(gDevice, NULL);
    WGPURenderPassColorAttachment theColor = {0};
    theColor.view = inView;
    theColor.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    theColor.loadOp = WGPULoadOp_Clear;
    theColor.storeOp = WGPUStoreOp_Store;
    theColor.clearValue.r = r;
    theColor.clearValue.g = g;
    theColor.clearValue.b = b;
    theColor.clearValue.a = 1.0;
    WGPURenderPassDescriptor thePass = {0};
    thePass.colorAttachmentCount = 1;
    thePass.colorAttachments = &theColor;
    WGPURenderPassEncoder theRender = wgpuCommandEncoderBeginRenderPass(theEncoder, &thePass);
    wgpuRenderPassEncoderEnd(theRender);
    wgpuRenderPassEncoderRelease(theRender);
    WGPUCommandBuffer theCommands = wgpuCommandEncoderFinish(theEncoder, NULL);
    wgpuQueueSubmit(inQueue, 1, &theCommands);
    wgpuCommandBufferRelease(theCommands);
    wgpuCommandEncoderRelease(theEncoder);
}

int main(int argc, char** argv)
{
    int theFallback = 0, theFrames = 60;
    WGPUInstanceBackend theBackends = WGPUInstanceBackend_Primary;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--fallback")) theFallback = 1;
        else if (!strcmp(argv[i], "--dx12")) theBackends = WGPUInstanceBackend_DX12;
        else if (!strcmp(argv[i], "--vulkan")) theBackends = WGPUInstanceBackend_Vulkan;
        else if (!strcmp(argv[i], "--gl")) theBackends = WGPUInstanceBackend_GL;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) theFrames = atoi(argv[++i]);
    }
    printf("probe: %d-bit, backends 0x%x, fallback %d\n", (int)(sizeof(void*) * 8), (unsigned)theBackends, theFallback);

    WGPUInstanceExtras theExtras = {0};
    theExtras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
    theExtras.backends = theBackends;
    WGPUInstanceDescriptor theInstanceDescriptor = {0};
    theInstanceDescriptor.nextInChain = &theExtras.chain;
    WGPUInstance theInstance = wgpuCreateInstance(&theInstanceDescriptor);
    if (!theInstance) { printf("FAIL: no instance\n"); return 1; }

    WNDCLASSA theClass = {0};
    theClass.lpfnWndProc = WindowProc;
    theClass.hInstance = GetModuleHandleA(NULL);
    theClass.lpszClassName = "wgpuprobe";
    RegisterClassA(&theClass);
    HWND theWindow = CreateWindowA("wgpuprobe", "wgpu probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 336, 279,
                                   NULL, NULL, theClass.hInstance, NULL);
    RECT theClient;
    GetClientRect(theWindow, &theClient);

    WGPUSurfaceSourceWindowsHWND theSource = {0};
    theSource.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    theSource.hinstance = theClass.hInstance;
    theSource.hwnd = theWindow;
    WGPUSurfaceDescriptor theSurfaceDescriptor = {0};
    theSurfaceDescriptor.nextInChain = &theSource.chain;
    WGPUSurface theSurface = wgpuInstanceCreateSurface(theInstance, &theSurfaceDescriptor);

    WGPURequestAdapterOptions theOptions = {0};
    theOptions.featureLevel = WGPUFeatureLevel_Core;
    theOptions.forceFallbackAdapter = theFallback;
    theOptions.compatibleSurface = theSurface;
    WGPURequestAdapterCallbackInfo theAdapterCallback = {0};
    theAdapterCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    theAdapterCallback.callback = OnAdapter;
    wgpuInstanceRequestAdapter(theInstance, &theOptions, theAdapterCallback);
    for (int i = 0; i < 1000 && !gAdapterDone; i++) { wgpuInstanceProcessEvents(theInstance); Sleep(1); }
    if (!gAdapter) { printf("FAIL: no adapter\n"); return 1; }
    WGPUAdapterInfo theInfo = {0};
    wgpuAdapterGetInfo(gAdapter, &theInfo);
    Print("adapter: ", theInfo.device);
    Print("description: ", theInfo.description);
    printf("backend %d, adapter type %d (3 = CPU)\n", (int)theInfo.backendType, (int)theInfo.adapterType);

    WGPUDeviceDescriptor theDeviceDescriptor = {0};
    theDeviceDescriptor.uncapturedErrorCallbackInfo.callback = OnError;
    WGPURequestDeviceCallbackInfo theDeviceCallback = {0};
    theDeviceCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    theDeviceCallback.callback = OnDevice;
    wgpuAdapterRequestDevice(gAdapter, &theDeviceDescriptor, theDeviceCallback);
    for (int i = 0; i < 1000 && !gDeviceDone; i++) { wgpuInstanceProcessEvents(theInstance); Sleep(1); }
    if (!gDevice) { printf("FAIL: no device\n"); return 1; }
    WGPUQueue theQueue = wgpuDeviceGetQueue(gDevice);

    WGPUSurfaceCapabilities theCaps = {0};
    wgpuSurfaceGetCapabilities(theSurface, gAdapter, &theCaps);
    if (theCaps.formatCount == 0) { printf("FAIL: the surface has no format\n"); return 1; }
    WGPUSurfaceConfiguration theConfig = {0};
    theConfig.device = gDevice;
    theConfig.format = theCaps.formats[0];
    theConfig.usage = WGPUTextureUsage_RenderAttachment;
    theConfig.width = theClient.right - theClient.left;
    theConfig.height = theClient.bottom - theClient.top;
    theConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
    theConfig.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(theSurface, &theConfig);
    printf("surface %ux%u format %d\n", theConfig.width, theConfig.height, (int)theConfig.format);

    LARGE_INTEGER theFrequency, theStart, theEnd;
    QueryPerformanceFrequency(&theFrequency);
    QueryPerformanceCounter(&theStart);
    int thePresented = 0;
    for (int f = 0; f < theFrames; f++) {
        Pump();
        WGPUSurfaceTexture theTexture = {0};
        wgpuSurfaceGetCurrentTexture(theSurface, &theTexture);
        if (theTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
            theTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
            printf("frame %d: surface texture status %d\n", f, (int)theTexture.status);
            continue;
        }
        WGPUTextureView theView = wgpuTextureCreateView(theTexture.texture, NULL);
        Clear(theQueue, theView, (double)f / theFrames, 0.3, 0.6);
        wgpuSurfacePresent(theSurface);
        wgpuTextureViewRelease(theView);
        wgpuTextureRelease(theTexture.texture);
        thePresented++;
    }
    QueryPerformanceCounter(&theEnd);
    printf("presented %d/%d frames, %.2f ms per frame\n", thePresented, theFrames,
           1000.0 * (double)(theEnd.QuadPart - theStart.QuadPart) / theFrequency.QuadPart / (theFrames ? theFrames : 1));

    /* an offscreen 64x64 clear read back through a mappable buffer */
    WGPUTextureDescriptor theTextureDescriptor = {0};
    theTextureDescriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    theTextureDescriptor.dimension = WGPUTextureDimension_2D;
    theTextureDescriptor.size.width = 64;
    theTextureDescriptor.size.height = 64;
    theTextureDescriptor.size.depthOrArrayLayers = 1;
    theTextureDescriptor.format = WGPUTextureFormat_RGBA8Unorm;
    theTextureDescriptor.mipLevelCount = 1;
    theTextureDescriptor.sampleCount = 1;
    WGPUTexture theOffscreen = wgpuDeviceCreateTexture(gDevice, &theTextureDescriptor);
    WGPUTextureView theOffscreenView = wgpuTextureCreateView(theOffscreen, NULL);
    Clear(theQueue, theOffscreenView, 0.25, 0.5, 0.75);
    WGPUBufferDescriptor theBufferDescriptor = {0};
    theBufferDescriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    theBufferDescriptor.size = 256 * 64;
    WGPUBuffer theBuffer = wgpuDeviceCreateBuffer(gDevice, &theBufferDescriptor);
    WGPUCommandEncoder theEncoder = wgpuDeviceCreateCommandEncoder(gDevice, NULL);
    WGPUTexelCopyTextureInfo theFrom = {0};
    theFrom.texture = theOffscreen;
    theFrom.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo theTo = {0};
    theTo.buffer = theBuffer;
    theTo.layout.bytesPerRow = 256;
    theTo.layout.rowsPerImage = 64;
    WGPUExtent3D theSize = {64, 64, 1};
    wgpuCommandEncoderCopyTextureToBuffer(theEncoder, &theFrom, &theTo, &theSize);
    WGPUCommandBuffer theCommands = wgpuCommandEncoderFinish(theEncoder, NULL);
    wgpuQueueSubmit(theQueue, 1, &theCommands);
    WGPUBufferMapCallbackInfo theMapCallback = {0};
    theMapCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    theMapCallback.callback = OnMap;
    wgpuBufferMapAsync(theBuffer, WGPUMapMode_Read, 0, 256 * 64, theMapCallback);
    for (int i = 0; i < 5000 && !gMapDone; i++) { wgpuDevicePoll(gDevice, 0, NULL); wgpuInstanceProcessEvents(theInstance); Sleep(1); }
    int theOK = 0;
    if (gMapDone > 0) {
        const unsigned char* p = (const unsigned char*)wgpuBufferGetConstMappedRange(theBuffer, 0, 256 * 64) + 256 * 32 + 4 * 32;
        printf("readback pixel (32,32): %u %u %u %u (expect ~64 128 191 255)\n", p[0], p[1], p[2], p[3]);
        theOK = abs(p[0] - 64) <= 1 && abs(p[1] - 128) <= 1 && abs(p[2] - 191) <= 1 && p[3] == 255;
        wgpuBufferUnmap(theBuffer);
    }
    printf("%s\n", theOK && thePresented == theFrames ? "PASS" : "FAIL");
    return theOK && thePresented == theFrames ? 0 : 1;
}
