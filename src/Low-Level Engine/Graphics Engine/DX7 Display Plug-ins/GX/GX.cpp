// GX core (include/gx/GX.h): the wgpu device, the command stream, pixel formats, surfaces with their CPU and GPU
// copies, the blits, presentation and the display object. The 3D device is GXDevice.cpp.
#include <stdarg.h>
#include "platform/PlatMessage.h"
#include "platform/PlatTime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "GXInternal.h"

std::recursive_mutex& GXLock()
{
    static std::recursive_mutex sLock;
    return sLock;
}

void GXLog(const char* inFormat, ...)
{
    char theText[1024];
    va_list theArguments;
    va_start(theArguments, inFormat);
    vsnprintf(theText, sizeof(theText) - 2, inFormat, theArguments);
    va_end(theArguments);
    strcat(theText, "\n");
    PlatDebugOutput(theText);
    if (getenv("EE_GX_LOG"))
        fputs(theText, stderr);
}

static unsigned long CountTrailingZeros(unsigned long inValue)
{
    unsigned long n = 0;
    if (!inValue)
        return 0;
    while (!(inValue & 1)) {
        inValue >>= 1;
        n++;
    }
    return n;
}

static unsigned long CountBits(unsigned long inValue)
{
    unsigned long n = 0;
    for (; inValue; inValue &= inValue - 1)
        n++;
    return n;
}

void GXFormat::Set(const GXPIXELFORMAT& inFormat)
{
    this->mIsZ = (inFormat.dwFlags & GXPF_ZBUFFER) != 0;
    this->mBits = inFormat.dwRGBBitCount;
    this->mMask[0] = inFormat.dwRBitMask;
    this->mMask[1] = inFormat.dwGBitMask;
    this->mMask[2] = inFormat.dwBBitMask;
    this->mMask[3] = (inFormat.dwFlags & GXPF_ALPHAPIXELS) ? inFormat.dwRGBAlphaBitMask : 0;
    for (int i = 0; i < 4; i++) {
        this->mShift[i] = CountTrailingZeros(this->mMask[i]);
        this->mWidth[i] = CountBits(this->mMask[i]);
    }
}

void GXFormat::ToBGRA(unsigned long inPixel, unsigned char* outBGRA) const
{
    static const int kOrder[4] = {2, 1, 0, 3};   // R, G, B, A -> byte positions in BGRA
    for (int i = 0; i < 4; i++) {
        unsigned long theWidth = this->mWidth[i];
        unsigned long theValue;
        if (theWidth == 0)
            theValue = i == 3 ? 255 : 0;
        else {
            theValue = (inPixel & this->mMask[i]) >> this->mShift[i];
            if (theWidth >= 8)
                theValue >>= theWidth - 8;
            else {
                unsigned long theMax = (1u << theWidth) - 1;
                theValue = (theValue * 255 + theMax / 2) / theMax;
            }
        }
        outBGRA[kOrder[i]] = (unsigned char)theValue;
    }
}

unsigned long GXFormat::FromBGRA(const unsigned char* inBGRA) const
{
    static const int kOrder[4] = {2, 1, 0, 3};
    unsigned long thePixel = 0;
    for (int i = 0; i < 4; i++) {
        unsigned long theWidth = this->mWidth[i];
        if (theWidth == 0)
            continue;
        unsigned long theValue = inBGRA[kOrder[i]];
        if (theWidth >= 8)
            theValue <<= theWidth - 8;
        else {
            unsigned long theMax = (1u << theWidth) - 1;
            theValue = (theValue * theMax + 127) / 255;
        }
        thePixel |= (theValue << this->mShift[i]) & this->mMask[i];
    }
    return thePixel;
}

// ---- core

static void OnError(WGPUDevice const* inDevice, WGPUErrorType inType, WGPUStringView inMessage, void*, void*)
{
    GXLog("GX: wgpu error %d: %.*s", (int)inType, (int)(inMessage.length == WGPU_STRLEN ? strlen(inMessage.data) :
                                                         inMessage.length), inMessage.data ? inMessage.data : "");
}

static void OnLog(WGPULogLevel inLevel, WGPUStringView inMessage, void*)
{
    GXLog("GX: wgpu log %d: %.*s", (int)inLevel, (int)(inMessage.length == WGPU_STRLEN ? strlen(inMessage.data) :
                                                      inMessage.length), inMessage.data ? inMessage.data : "");
}

static void OnAdapter(WGPURequestAdapterStatus inStatus, WGPUAdapter inAdapter, WGPUStringView, void* inCore, void* inDone)
{
    if (inStatus == WGPURequestAdapterStatus_Success)
        static_cast<GXCore*>(inCore)->mAdapter = inAdapter;
    *static_cast<int*>(inDone) = 1;
}

static void OnDevice(WGPURequestDeviceStatus inStatus, WGPUDevice inDevice, WGPUStringView, void* inCore, void* inDone)
{
    if (inStatus == WGPURequestDeviceStatus_Success)
        static_cast<GXCore*>(inCore)->mDevice = inDevice;
    *static_cast<int*>(inDone) = 1;
}

GXCore& GXGetCore()
{
    static GXCore sCore;
    return sCore;
}

extern const char* const kGXFixedFunctionShader;   // GXDevice.cpp
static const char* const kBlitShader = R"WGSL(
struct B { dst: vec4f, src: vec4f, key: vec4f, color: vec4f, mode: vec4u, misc: vec4f };
@group(0) @binding(0) var<uniform> b: B;
@group(0) @binding(1) var t: texture_2d<f32>;
@group(0) @binding(2) var s: sampler;
struct V { @builtin(position) p: vec4f, @location(0) uv: vec2f };
@vertex fn vertex_main(@builtin(vertex_index) i: u32) -> V {
    var c = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0), vec2f(0.0, 1.0), vec2f(1.0, 0.0),
                            vec2f(1.0, 1.0));
    let k = c[i];
    var o: V;
    o.p = vec4f(mix(b.dst.x, b.dst.z, k.x), mix(b.dst.y, b.dst.w, k.y), b.misc.x, 1.0);
    o.uv = mix(b.src.xy, b.src.zw, k);
    return o;
}
@fragment fn fragment_main(v: V) -> @location(0) vec4f {
    let c = textureSampleLevel(t, s, v.uv, 0.0);
    if (b.mode.x == 2u) { return b.color; }
    if (b.mode.x == 1u && all(abs(c.rgb - b.key.rgb) < vec3f(0.002))) { discard; }
    if (b.mode.y == 1u) { return vec4f(c.rgb, 1.0); }   // presentation: opaque
    return c;
}
)WGSL";

static WGPUStringView View(const char* inText)
{
    WGPUStringView theView;
    theView.data = inText;
    theView.length = WGPU_STRLEN;
    return theView;
}

WGPUShaderModule GXCreateShader(const char* inSource)
{
    WGPUShaderSourceWGSL theSource = {};
    theSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    theSource.code = View(inSource);
    WGPUShaderModuleDescriptor theDescriptor = {};
    theDescriptor.nextInChain = &theSource.chain;
    return wgpuDeviceCreateShaderModule(GXGetCore().mDevice, &theDescriptor);
}

static WGPUBuffer CreateBuffer(unsigned long inSize, WGPUBufferUsage inUsage)
{
    WGPUBufferDescriptor theDescriptor = {};
    theDescriptor.size = inSize;
    theDescriptor.usage = inUsage | WGPUBufferUsage_CopyDst;
    return wgpuDeviceCreateBuffer(GXGetCore().mDevice, &theDescriptor);
}

bool GXCore::Start()
{
    if (this->mDevice)
        return true;
    if (this->mFailed)
        return false;
    this->mFailed = true;
    if (const char* theFile = getenv("EE_GX_STDERR")) {   // debugging: wgpu's panic messages go to stderr
#ifdef _WIN32
        HANDLE theHandle = CreateFileA(theFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
        if (theHandle != INVALID_HANDLE_VALUE)
            SetStdHandle(STD_ERROR_HANDLE, theHandle);
#else
        freopen(theFile, "w", stderr);
#endif
    }
    if (getenv("EE_GX_LOG")) {   // wgpu's own log (backend details of validation errors)
        wgpuSetLogCallback(OnLog, NULL);
        const char* theLevel = getenv("EE_GX_LOG");
        wgpuSetLogLevel(!strcmp(theLevel, "debug") ? WGPULogLevel_Debug : !strcmp(theLevel, "info") ? WGPULogLevel_Info :
                        WGPULogLevel_Warn);
    }
    WGPUInstanceExtras theExtras = {};
    theExtras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
    // Windows: DX12 (WARP without a GPU), else Vulkan; Linux: Vulkan; macOS: Metal
    theExtras.backends = WGPUInstanceBackend_DX12 | WGPUInstanceBackend_Vulkan | WGPUInstanceBackend_Metal;
    if (const char* theFlags = getenv("EE_WGPU_FLAGS"))   // wgpu instance flags (debugging: 1 debug, 2 validation)
        theExtras.flags = (WGPUInstanceFlag)strtoul(theFlags, NULL, 0);
    if (const char* theCompiler = getenv("EE_WGPU_DX12_COMPILER"))   // fxc or dxc (with dxcompiler.dll)
        theExtras.dx12ShaderCompiler = !PlatStrICmp(theCompiler, "dxc") ? WGPUDx12Compiler_Dxc : WGPUDx12Compiler_Fxc;
    if (const char* theBackend = getenv("EE_WGPU_BACKEND")) {
        if (!PlatStrICmp(theBackend, "dx12")) theExtras.backends = WGPUInstanceBackend_DX12;
        else if (!PlatStrICmp(theBackend, "vulkan")) theExtras.backends = WGPUInstanceBackend_Vulkan;
        else if (!PlatStrICmp(theBackend, "gl")) theExtras.backends = WGPUInstanceBackend_GL;
        else if (!PlatStrICmp(theBackend, "metal")) theExtras.backends = WGPUInstanceBackend_Metal;
    }
    WGPUInstanceDescriptor theInstanceDescriptor = {};
    theInstanceDescriptor.nextInChain = &theExtras.chain;
    this->mInstance = wgpuCreateInstance(&theInstanceDescriptor);
    if (!this->mInstance) {
        GXLog("GX: no wgpu instance");
        return false;
    }
    for (int theFallback = 0; theFallback < 2 && !this->mAdapter; theFallback++) {
        WGPURequestAdapterOptions theOptions = {};
        theOptions.featureLevel = WGPUFeatureLevel_Core;
        theOptions.powerPreference = WGPUPowerPreference_HighPerformance;
        theOptions.forceFallbackAdapter = theFallback || getenv("EE_WGPU_FALLBACK") != NULL;
        int theDone = 0;
        WGPURequestAdapterCallbackInfo theCallback = {};
        theCallback.mode = WGPUCallbackMode_AllowProcessEvents;
        theCallback.callback = OnAdapter;
        theCallback.userdata1 = this;
        theCallback.userdata2 = &theDone;
        wgpuInstanceRequestAdapter(this->mInstance, &theOptions, theCallback);
        for (int i = 0; i < 5000 && !theDone; i++) {
            wgpuInstanceProcessEvents(this->mInstance);
            if (!theDone)
                PlatSleep(1);
        }
    }
    if (!this->mAdapter) {
        GXLog("GX: no adapter");
        return false;
    }
    wgpuAdapterGetInfo(this->mAdapter, &this->mInfo);
    WGPUDeviceDescriptor theDeviceDescriptor = {};
    theDeviceDescriptor.uncapturedErrorCallbackInfo.callback = OnError;
    int theDone = 0;
    WGPURequestDeviceCallbackInfo theCallback = {};
    theCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    theCallback.callback = OnDevice;
    theCallback.userdata1 = this;
    theCallback.userdata2 = &theDone;
    wgpuAdapterRequestDevice(this->mAdapter, &theDeviceDescriptor, theCallback);
    for (int i = 0; i < 5000 && !theDone; i++) {
        wgpuInstanceProcessEvents(this->mInstance);
        if (!theDone)
            PlatSleep(1);
    }
    if (!this->mDevice) {
        GXLog("GX: no device");
        return false;
    }
    this->mQueue = wgpuDeviceGetQueue(this->mDevice);
    GXLog("GX: adapter %.*s (backend %d, type %d)", (int)(this->mInfo.device.length == WGPU_STRLEN ?
          strlen(this->mInfo.device.data) : this->mInfo.device.length), this->mInfo.device.data ? this->mInfo.device.data : "",
          (int)this->mInfo.backendType, (int)this->mInfo.adapterType);

    this->mVertexRing = CreateBuffer(kVertexRingSize, WGPUBufferUsage_Vertex);
    this->mIndexRing = CreateBuffer(kIndexRingSize, WGPUBufferUsage_Index);
    this->mUniformRing = CreateBuffer(kUniformRingSize, WGPUBufferUsage_Uniform);

    WGPUBindGroupLayoutEntry theEntries[3] = {};
    theEntries[0].binding = 0;
    theEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    theEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
    theEntries[0].buffer.hasDynamicOffset = 1;
    theEntries[1].binding = 1;
    theEntries[1].visibility = WGPUShaderStage_Fragment;
    theEntries[1].texture.sampleType = WGPUTextureSampleType_Float;
    theEntries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    theEntries[2].binding = 2;
    theEntries[2].visibility = WGPUShaderStage_Fragment;
    theEntries[2].sampler.type = WGPUSamplerBindingType_Filtering;
    WGPUBindGroupLayoutDescriptor theLayout = {};
    theLayout.entryCount = 3;
    theLayout.entries = theEntries;
    this->mLayout = wgpuDeviceCreateBindGroupLayout(this->mDevice, &theLayout);
    WGPUPipelineLayoutDescriptor thePipelineLayout = {};
    thePipelineLayout.bindGroupLayoutCount = 1;
    thePipelineLayout.bindGroupLayouts = &this->mLayout;
    this->mPipelineLayout = wgpuDeviceCreatePipelineLayout(this->mDevice, &thePipelineLayout);

    std::string theSmooth(kGXFixedFunctionShader), theFlat(kGXFixedFunctionShader);
    for (size_t p; (p = theSmooth.find("INTERP")) != std::string::npos;)
        theSmooth.replace(p, 6, "");
    for (size_t p; (p = theFlat.find("INTERP")) != std::string::npos;)
        theFlat.replace(p, 6, "@interpolate(flat)");
    this->mShader[0] = GXCreateShader(theSmooth.c_str());
    this->mShader[1] = GXCreateShader(theFlat.c_str());
    this->mBlitShader = GXCreateShader(kBlitShader);

    WGPUTextureDescriptor theWhite = {};
    theWhite.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    theWhite.dimension = WGPUTextureDimension_2D;
    theWhite.size.width = theWhite.size.height = theWhite.size.depthOrArrayLayers = 1;
    theWhite.format = WGPUTextureFormat_BGRA8Unorm;
    theWhite.mipLevelCount = theWhite.sampleCount = 1;
    this->mWhite = wgpuDeviceCreateTexture(this->mDevice, &theWhite);
    this->mWhiteView = wgpuTextureCreateView(this->mWhite, NULL);
    static const unsigned char kWhite[4] = {255, 255, 255, 255};
    WGPUTexelCopyTextureInfo theTo = {};
    theTo.texture = this->mWhite;
    theTo.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout theLayoutData = {};
    theLayoutData.bytesPerRow = 4;
    theLayoutData.rowsPerImage = 1;
    WGPUExtent3D theSize = {1, 1, 1};
    wgpuQueueWriteTexture(this->mQueue, &theTo, kWhite, 4, &theLayoutData, &theSize);
    this->mFailed = false;
    return true;
}

void GXCore::EndPass()
{
    if (this->mPass) {
        wgpuRenderPassEncoderEnd(this->mPass);
        wgpuRenderPassEncoderRelease(this->mPass);
        this->mPass = NULL;
    }
    this->mPassTarget = NULL;
    this->mPassDepth = NULL;
    this->mPassPipeline = NULL;
}

void GXCore::BeginPass(GXSurfaceData* inTarget, GXSurfaceData* inDepth)
{
    if (this->mPass && this->mPassTarget == inTarget && this->mPassDepth == inDepth)
        return;
    this->EndPass();
    if (!this->mEncoder)
        this->mEncoder = wgpuDeviceCreateCommandEncoder(this->mDevice, NULL);
    WGPURenderPassColorAttachment theColor = {};
    theColor.view = inTarget->mTargetView;
    theColor.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    theColor.loadOp = WGPULoadOp_Load;
    theColor.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDepthStencilAttachment theDepth = {};
    WGPURenderPassDescriptor thePass = {};
    thePass.colorAttachmentCount = 1;
    thePass.colorAttachments = &theColor;
    if (inDepth) {
        theDepth.view = inDepth->mTargetView;
        theDepth.depthLoadOp = WGPULoadOp_Load;
        theDepth.depthStoreOp = WGPUStoreOp_Store;
        thePass.depthStencilAttachment = &theDepth;
        inDepth->mUsedSerial = this->mSerial;
    }
    this->mPass = wgpuCommandEncoderBeginRenderPass(this->mEncoder, &thePass);
    this->mPassTarget = inTarget;
    this->mPassDepth = inDepth;
    inTarget->mUsedSerial = this->mSerial;
}

void GXCore::Submit()
{
    this->EndPass();
    if (this->mEncoder) {
        WGPUCommandBuffer theCommands = wgpuCommandEncoderFinish(this->mEncoder, NULL);
        wgpuQueueSubmit(this->mQueue, 1, &theCommands);
        wgpuCommandBufferRelease(theCommands);
        wgpuCommandEncoderRelease(this->mEncoder);
        this->mEncoder = NULL;
    }
    this->mVertexOffset = this->mIndexOffset = this->mUniformOffset = 0;
    this->mSerial++;
}

void GXCore::Wait()
{
    wgpuDevicePoll(this->mDevice, 1, NULL);
}

unsigned long GXSamplerKey(unsigned long inAddress, unsigned long inMin, unsigned long inMag, unsigned long inMip)
{
    return (inAddress & 3) | ((inMin & 3) << 2) | ((inMag & 3) << 4) | ((inMip & 3) << 6);
}

WGPUSampler GXCore::Sampler(unsigned long inKey)
{
    std::map<unsigned long, WGPUSampler>::iterator theFound = this->mSamplers.find(inKey);
    if (theFound != this->mSamplers.end())
        return theFound->second;
    WGPUSamplerDescriptor theDescriptor = {};
    WGPUAddressMode theAddress;
    switch (inKey & 3) {
    case GXTADDRESS_WRAP: theAddress = WGPUAddressMode_Repeat; break;
    case GXTADDRESS_MIRROR: theAddress = WGPUAddressMode_MirrorRepeat; break;
    default: theAddress = WGPUAddressMode_ClampToEdge; break;
    }
    theDescriptor.addressModeU = theDescriptor.addressModeV = theDescriptor.addressModeW = theAddress;
    theDescriptor.minFilter = ((inKey >> 2) & 3) >= GXTFN_LINEAR ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
    theDescriptor.magFilter = ((inKey >> 4) & 3) >= GXTFG_LINEAR ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
    unsigned long theMip = (inKey >> 6) & 3;
    theDescriptor.mipmapFilter = theMip == GXTFP_LINEAR ? WGPUMipmapFilterMode_Linear : WGPUMipmapFilterMode_Nearest;
    theDescriptor.lodMinClamp = 0.0f;
    theDescriptor.lodMaxClamp = theMip == GXTFP_NONE || theMip == 0 ? 0.0f : 32.0f;
    theDescriptor.maxAnisotropy = 1;
    WGPUSampler theSampler = wgpuDeviceCreateSampler(this->mDevice, &theDescriptor);
    this->mSamplers[inKey] = theSampler;
    return theSampler;
}

WGPUBindGroup GXCore::Group(GXSurfaceData* inTexture, unsigned long inSamplerKey)
{
    // a mip level samples its chain's texture; a surface without one (primary, Z) samples white
    if (inTexture)
        inTexture = inTexture->Top();
    if (inTexture && !inTexture->mView) {
        GXLog("GX: sampling a surface without a texture (%lux%lu)", inTexture->mWidth, inTexture->mHeight);
        inTexture = NULL;
    }
    std::map<unsigned long, WGPUBindGroup>& theGroups = inTexture ? inTexture->mGroups : this->mWhiteGroups;
    std::map<unsigned long, WGPUBindGroup>::iterator theFound = theGroups.find(inSamplerKey);
    if (theFound != theGroups.end())
        return theFound->second;
    WGPUBindGroupEntry theEntries[3] = {};
    theEntries[0].binding = 0;
    theEntries[0].buffer = this->mUniformRing;
    theEntries[0].size = 2048;
    theEntries[1].binding = 1;
    theEntries[1].textureView = inTexture ? inTexture->mView : this->mWhiteView;
    theEntries[2].binding = 2;
    theEntries[2].sampler = this->Sampler(inSamplerKey);
    WGPUBindGroupDescriptor theDescriptor = {};
    theDescriptor.layout = this->mLayout;
    theDescriptor.entryCount = 3;
    theDescriptor.entries = theEntries;
    WGPUBindGroup theGroup = wgpuDeviceCreateBindGroup(this->mDevice, &theDescriptor);
    theGroups[inSamplerKey] = theGroup;
    return theGroup;
}

unsigned long GXCore::Ring(WGPUBuffer inRing, unsigned long& ioOffset, unsigned long inSize, const void* inData,
                           unsigned long inBytes, unsigned long inAlign)
{
    unsigned long theOffset = (ioOffset + inAlign - 1) / inAlign * inAlign;
    unsigned long thePadded = (inBytes + 3) & ~3u;
    if (theOffset + thePadded > inSize) {
        this->Submit();
        theOffset = 0;
    }
    if (thePadded == inBytes)
        wgpuQueueWriteBuffer(this->mQueue, inRing, theOffset, inData, inBytes);
    else {
        std::vector<unsigned char> theCopy(thePadded, 0);
        memcpy(&theCopy[0], inData, inBytes);
        wgpuQueueWriteBuffer(this->mQueue, inRing, theOffset, &theCopy[0], thePadded);
    }
    ioOffset = theOffset + thePadded;
    return theOffset;
}

// ---- surfaces

bool GXSurfaceData::AllocateMemory()
{
    if (this->mMemory)
        return true;
    if (this->mIsZ || this->mIsPrimary || this->mFormat.mBits == 0)
        return false;
    this->mPitch = (long)((this->mWidth * (this->mFormat.mBits / 8) + 3) & ~3u);
    this->mMemory = (unsigned char*)calloc((size_t)this->mPitch * this->mHeight, 1);
    return this->mMemory != NULL;
}

bool GXSurfaceData::CreateTexture()
{
    GXSurfaceData* theTop = this->Top();
    if (theTop->mTexture)
        return true;
    if (theTop->mIsPrimary)
        return false;
    GXCore& theCore = GXGetCore();
    WGPUTextureDescriptor theDescriptor = {};
    theDescriptor.dimension = WGPUTextureDimension_2D;
    theDescriptor.size.width = theTop->mWidth ? theTop->mWidth : 1;
    theDescriptor.size.height = theTop->mHeight ? theTop->mHeight : 1;
    theDescriptor.size.depthOrArrayLayers = 1;
    theDescriptor.mipLevelCount = theTop->mMipLevels;
    theDescriptor.sampleCount = 1;
    if (theTop->mIsZ) {
        theDescriptor.format = WGPUTextureFormat_Depth32Float;
        theDescriptor.usage = WGPUTextureUsage_RenderAttachment;
    } else {
        theDescriptor.format = WGPUTextureFormat_BGRA8Unorm;
        theDescriptor.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc |
                              WGPUTextureUsage_RenderAttachment;
    }
    theTop->mTexture = wgpuDeviceCreateTexture(theCore.mDevice, &theDescriptor);
    if (!theTop->mTexture)
        return false;
    theTop->mView = wgpuTextureCreateView(theTop->mTexture, NULL);
    WGPUTextureViewDescriptor theTarget = {};
    theTarget.format = theDescriptor.format;
    theTarget.dimension = WGPUTextureViewDimension_2D;
    theTarget.baseMipLevel = 0;
    theTarget.mipLevelCount = 1;
    theTarget.arrayLayerCount = 1;
    theTarget.aspect = WGPUTextureAspect_All;
    theTop->mTargetView = wgpuTextureCreateView(theTop->mTexture, &theTarget);
    return true;
}

void GXSurfaceData::UploadLevel(GXSurfaceData* inLevel)
{
    GXCore& theCore = GXGetCore();
    GXSurfaceData* theTop = this->Top();
    if (!inLevel->mMemory || !theTop->mTexture)
        return;
    if (theTop->mUsedSerial == theCore.mSerial)
        theCore.Submit();   // the open encoder used the old pixels: keep them for it
    unsigned long theWidth = inLevel->mWidth, theHeight = inLevel->mHeight;
    std::vector<unsigned char> thePixels((size_t)theWidth * theHeight * 4);
    const GXFormat& theFormat = inLevel->mFormat;
    bool theKeyed = theTop->mIsTexture && theTop->mHasKey;
    for (unsigned long y = 0; y < theHeight; y++) {
        const unsigned char* theRow = inLevel->mMemory + (size_t)y * inLevel->mPitch;
        unsigned char* theOut = &thePixels[(size_t)y * theWidth * 4];
        if (theFormat.IsBGRA32()) {
            memcpy(theOut, theRow, theWidth * 4);
            if (!theFormat.mMask[3])
                for (unsigned long x = 0; x < theWidth; x++)
                    theOut[x * 4 + 3] = 255;
        } else
            for (unsigned long x = 0; x < theWidth; x++)
                theFormat.ToBGRA(inLevel->ReadPixel(x, y), theOut + x * 4);
        if (theKeyed)   // colour-keyed texels are transparent to the 3D device (COLORKEYENABLE and blending)
            for (unsigned long x = 0; x < theWidth; x++)
                if ((inLevel->ReadPixel(x, y) & ~theFormat.mMask[3]) == (theTop->mKey & ~theFormat.mMask[3]))
                    theOut[x * 4 + 3] = 0;
    }
    WGPUTexelCopyTextureInfo theTo = {};
    theTo.texture = theTop->mTexture;
    theTo.mipLevel = inLevel->mMipLevel;
    theTo.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout theLayout = {};
    theLayout.bytesPerRow = theWidth * 4;
    theLayout.rowsPerImage = theHeight;
    WGPUExtent3D theSize = {theWidth, theHeight, 1};
    wgpuQueueWriteTexture(theCore.mQueue, &theTo, &thePixels[0], thePixels.size(), &theLayout, &theSize);
    inLevel->mGPUValid = true;
}

void GXSurfaceData::Upload()
{
    GXSurfaceData* theTop = this->Top();
    if (!theTop->CreateTexture())
        return;
    if (!theTop->mGPUValid)
        theTop->UploadLevel(theTop);
    if (theTop->mLevelsDirty) {
        for (GXSurfaceData* theLevel = theTop; theLevel;) {
            if (!theLevel->mGPUValid)
                theTop->UploadLevel(theLevel);
            GXSurfaceData* theNext = NULL;
            for (size_t i = 0; i < theLevel->mAttached.size(); i++)
                if (theLevel->mAttached[i]->mData->mMipTop == theTop)
                    theNext = theLevel->mAttached[i]->mData;
            theLevel = theNext;
        }
        theTop->mLevelsDirty = false;
    }
    theTop->mGPUValid = true;
}

static void OnMap(WGPUMapAsyncStatus inStatus, WGPUStringView, void* inDone, void*)
{
    *static_cast<int*>(inDone) = inStatus == WGPUMapAsyncStatus_Success ? 1 : -1;
}

void GXSurfaceData::Download()
{
    if (this->mCPUValid)
        return;
    GXCore& theCore = GXGetCore();
    if (!this->AllocateMemory() || !this->mTexture) {
        this->mCPUValid = true;
        return;
    }
    theCore.Submit();
    unsigned long theRow = (this->mWidth * 4 + 255) & ~255u;
    WGPUBufferDescriptor theDescriptor = {};
    theDescriptor.size = (uint64_t)theRow * this->mHeight;
    theDescriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer theBuffer = wgpuDeviceCreateBuffer(theCore.mDevice, &theDescriptor);
    WGPUCommandEncoder theEncoder = wgpuDeviceCreateCommandEncoder(theCore.mDevice, NULL);
    WGPUTexelCopyTextureInfo theFrom = {};
    theFrom.texture = this->mTexture;
    theFrom.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo theTo = {};
    theTo.buffer = theBuffer;
    theTo.layout.bytesPerRow = theRow;
    theTo.layout.rowsPerImage = this->mHeight;
    WGPUExtent3D theSize = {this->mWidth, this->mHeight, 1};
    wgpuCommandEncoderCopyTextureToBuffer(theEncoder, &theFrom, &theTo, &theSize);
    WGPUCommandBuffer theCommands = wgpuCommandEncoderFinish(theEncoder, NULL);
    wgpuQueueSubmit(theCore.mQueue, 1, &theCommands);
    wgpuCommandBufferRelease(theCommands);
    wgpuCommandEncoderRelease(theEncoder);
    int theDone = 0;
    WGPUBufferMapCallbackInfo theCallback = {};
    theCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    theCallback.callback = OnMap;
    theCallback.userdata1 = &theDone;
    wgpuBufferMapAsync(theBuffer, WGPUMapMode_Read, 0, (size_t)theDescriptor.size, theCallback);
    // poll without a blocking wait (a blocking device poll never returned in an x64 session, wgpu-native v29 on
    // DX12/WARP): give the copy up to 10 s
    unsigned long long theStart = PlatTimeMilliseconds64();
    for (unsigned long theSpin = 0; !theDone; theSpin++) {
        wgpuDevicePoll(theCore.mDevice, 0, NULL);
        wgpuInstanceProcessEvents(theCore.mInstance);
        if (theDone)
            break;
        if (PlatTimeMilliseconds64() - theStart > 10000) {
            GXLog("GX: read-back of %lux%lu timed out", this->mWidth, this->mHeight);
            break;
        }
        if (theSpin > 64)
            PlatSleep(theSpin > 1000 ? 1 : 0);
    }
    if (theDone > 0) {
        const unsigned char* theData = (const unsigned char*)wgpuBufferGetConstMappedRange(theBuffer, 0,
                                                                                            (size_t)theDescriptor.size);
        for (unsigned long y = 0; y < this->mHeight; y++) {
            const unsigned char* theIn = theData + (size_t)y * theRow;
            if (this->mFormat.IsBGRA32())
                memcpy(this->mMemory + (size_t)y * this->mPitch, theIn, this->mWidth * 4);
            else
                for (unsigned long x = 0; x < this->mWidth; x++)
                    this->WritePixel(x, y, this->mFormat.FromBGRA(theIn + x * 4));
        }
        wgpuBufferUnmap(theBuffer);
    } else
        GXLog("GX: read-back failed");
    wgpuBufferRelease(theBuffer);
    this->mCPUValid = true;
}

void GXSurfaceData::ForGPURead()
{
    GXSurfaceData* theTop = this->Top();
    if (!theTop->mGPUValid || theTop->mLevelsDirty || !theTop->mTexture)
        theTop->Upload();
}

void GXSurfaceData::ForGPUWrite()
{
    this->ForGPURead();
    this->mCPUValid = false;
}

void GXSurfaceData::ForCPURead()
{
    if (!this->mCPUValid)
        this->Download();
    this->AllocateMemory();
}

void GXSurfaceData::ForCPUWrite()
{
    this->ForCPURead();
    this->mGPUValid = false;
    if (this->mMipTop)
        this->mMipTop->mLevelsDirty = true;
}

unsigned long GXSurfaceData::ReadPixel(unsigned long x, unsigned long y) const
{
    const unsigned char* p = this->mMemory + (size_t)y * this->mPitch;
    if (this->mFormat.mBits == 32)
        return ((const uint32_t*)p)[x];
    return ((const uint16_t*)p)[x];
}

void GXSurfaceData::WritePixel(unsigned long x, unsigned long y, unsigned long inPixel)
{
    unsigned char* p = this->mMemory + (size_t)y * this->mPitch;
    if (this->mFormat.mBits == 32)
        ((uint32_t*)p)[x] = (uint32_t)inPixel;
    else
        ((uint16_t*)p)[x] = (uint16_t)inPixel;
}

static bool GPUPreferred(GXSurfaceData* inSurface)
{
    return inSurface->mIsRenderTarget || !inSurface->mCPUValid;
}

static void ToNDC(const RECT& inRect, unsigned long inWidth, unsigned long inHeight, float outNDC[4])
{
    outNDC[0] = 2.0f * inRect.left / inWidth - 1.0f;
    outNDC[1] = 1.0f - 2.0f * inRect.top / inHeight;
    outNDC[2] = 2.0f * inRect.right / inWidth - 1.0f;
    outNDC[3] = 1.0f - 2.0f * inRect.bottom / inHeight;
}

struct GXBlitUniform {
    float mDestination[4], mSource[4], mKey[4], mColor[4];
    uint32_t mMode[4];
    float mMisc[4];
};

static WGPURenderPipeline BlitPipeline(GXBlitMode inMode, WGPUTextureFormat inFormat, bool inDepth)
{
    GXCore& theCore = GXGetCore();
    unsigned long theKey = (unsigned long)inMode | (inDepth ? 8 : 0) | ((unsigned long)inFormat << 4);
    std::map<unsigned long, WGPURenderPipeline>::iterator theFound = theCore.mBlitPipelines.find(theKey);
    if (theFound != theCore.mBlitPipelines.end())
        return theFound->second;
    WGPUBlendState theBlend = {};
    theBlend.color.operation = theBlend.alpha.operation = WGPUBlendOperation_Add;
    theBlend.color.srcFactor = theBlend.alpha.srcFactor = WGPUBlendFactor_One;
    theBlend.color.dstFactor = theBlend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    WGPUColorTargetState theTarget = {};
    theTarget.format = inFormat;
    theTarget.blend = inMode == kGXBlitPremultiplied ? &theBlend : NULL;
    theTarget.writeMask = inMode == kGXBlitDepth ? WGPUColorWriteMask_None : WGPUColorWriteMask_All;
    WGPUFragmentState theFragment = {};
    theFragment.module = theCore.mBlitShader;
    theFragment.entryPoint = View("fragment_main");
    theFragment.targetCount = 1;
    theFragment.targets = &theTarget;
    WGPUDepthStencilState theDepth = {};
    theDepth.format = WGPUTextureFormat_Depth32Float;
    theDepth.depthWriteEnabled = inMode == kGXBlitDepth ? WGPUOptionalBool_True : WGPUOptionalBool_False;
    theDepth.depthCompare = WGPUCompareFunction_Always;
    theDepth.stencilFront.compare = theDepth.stencilBack.compare = WGPUCompareFunction_Always;
    theDepth.stencilFront.failOp = theDepth.stencilBack.failOp = WGPUStencilOperation_Keep;
    theDepth.stencilFront.depthFailOp = theDepth.stencilBack.depthFailOp = WGPUStencilOperation_Keep;
    theDepth.stencilFront.passOp = theDepth.stencilBack.passOp = WGPUStencilOperation_Keep;
    WGPURenderPipelineDescriptor theDescriptor = {};
    theDescriptor.layout = theCore.mPipelineLayout;
    theDescriptor.vertex.module = theCore.mBlitShader;
    theDescriptor.vertex.entryPoint = View("vertex_main");
    theDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    theDescriptor.primitive.frontFace = WGPUFrontFace_CCW;
    theDescriptor.primitive.cullMode = WGPUCullMode_None;
    theDescriptor.depthStencil = inDepth ? &theDepth : NULL;
    theDescriptor.multisample.count = 1;
    theDescriptor.multisample.mask = 0xffffffff;
    theDescriptor.fragment = &theFragment;
    WGPURenderPipeline thePipeline = wgpuDeviceCreateRenderPipeline(theCore.mDevice, &theDescriptor);
    theCore.mBlitPipelines[theKey] = thePipeline;
    return thePipeline;
}

// records the blit quad into the open pass (the caller opened it on the target)
static void DrawBlit(WGPUTextureFormat inFormat, unsigned long inWidth, unsigned long inHeight,
                     const RECT& inDestination, GXSurfaceData* inSource, const RECT* inSourceRect, GXBlitMode inMode,
                     const float inColor[4], float inDepth, bool inLinear, unsigned long inUniformOffset)
{
    GXCore& theCore = GXGetCore();
    WGPURenderPipeline thePipeline = BlitPipeline(inMode, inFormat, theCore.mPassDepth != NULL);
    if (thePipeline != theCore.mPassPipeline) {
        wgpuRenderPassEncoderSetPipeline(theCore.mPass, thePipeline);
        theCore.mPassPipeline = thePipeline;
    }
    wgpuRenderPassEncoderSetViewport(theCore.mPass, 0.0f, 0.0f, (float)inWidth, (float)inHeight, 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(theCore.mPass, 0, 0, inWidth, inHeight);
    WGPUBindGroup theGroup = theCore.Group(inSource, GXSamplerKey(GXTADDRESS_CLAMP, inLinear ? GXTFN_LINEAR : GXTFN_POINT,
                                                                  inLinear ? GXTFG_LINEAR : GXTFG_POINT, GXTFP_NONE));
    uint32_t theOffset = inUniformOffset;
    wgpuRenderPassEncoderSetBindGroup(theCore.mPass, 0, theGroup, 1, &theOffset);
    wgpuRenderPassEncoderDraw(theCore.mPass, 6, 1, 0, 0);
    if (inSource)
        inSource->Top()->mUsedSerial = theCore.mSerial;
}

static unsigned long BlitUniform(unsigned long inWidth, unsigned long inHeight, const RECT& inDestination,
                                 GXSurfaceData* inSource, const RECT* inSourceRect, GXBlitMode inMode,
                                 const float inColor[4], float inDepth, bool inOpaque = false)
{
    GXCore& theCore = GXGetCore();
    GXBlitUniform theUniform = {};
    ToNDC(inDestination, inWidth, inHeight, theUniform.mDestination);
    if (inSource && inSourceRect) {
        theUniform.mSource[0] = (float)inSourceRect->left / inSource->mWidth;
        theUniform.mSource[1] = (float)inSourceRect->top / inSource->mHeight;
        theUniform.mSource[2] = (float)inSourceRect->right / inSource->mWidth;
        theUniform.mSource[3] = (float)inSourceRect->bottom / inSource->mHeight;
    } else {
        theUniform.mSource[2] = 1.0f;
        theUniform.mSource[3] = 1.0f;
    }
    if (inSource && inMode == kGXBlitKeyed) {
        unsigned char theKey[4];
        inSource->mFormat.ToBGRA(inSource->mKey, theKey);
        theUniform.mKey[0] = theKey[2] / 255.0f;
        theUniform.mKey[1] = theKey[1] / 255.0f;
        theUniform.mKey[2] = theKey[0] / 255.0f;
    }
    if (inColor)
        memcpy(theUniform.mColor, inColor, sizeof(theUniform.mColor));
    theUniform.mMode[0] = inMode == kGXBlitKeyed ? 1 : inMode == kGXBlitFill || inMode == kGXBlitDepth ? 2 : 0;
    theUniform.mMisc[0] = inDepth;
    theUniform.mMode[1] = inOpaque ? 1 : 0;
    return theCore.Ring(theCore.mUniformRing, theCore.mUniformOffset, GXCore::kUniformRingSize - 2048, &theUniform,
                        sizeof(theUniform), 256);
}

void GXBlit(GXSurfaceData* inTarget, const RECT& inDestination, GXSurfaceData* inSource, const RECT* inSourceRect,
            GXBlitMode inMode, const float inColor[4], float inDepth, bool inLinear)
{
    GXCore& theCore = GXGetCore();
    if (inSource)
        inSource->ForGPURead();
    if (inMode == kGXBlitDepth) {
        if (!inTarget->mDepth)
            return;
        inTarget->mDepth->CreateTexture();
    }
    inTarget->ForGPUWrite();
    if (!inTarget->CreateTexture())
        return;
    // the uniform first: a full ring submits, which would close the pass
    unsigned long theOffset = BlitUniform(inTarget->mWidth, inTarget->mHeight, inDestination, inSource, inSourceRect,
                                          inMode, inColor, inDepth);
    GXSurfaceData* theDepth = inMode == kGXBlitDepth ? inTarget->mDepth :
        (theCore.mPass && theCore.mPassTarget == inTarget ? theCore.mPassDepth : NULL);
    theCore.BeginPass(inTarget, theDepth);
    DrawBlit(WGPUTextureFormat_BGRA8Unorm, inTarget->mWidth, inTarget->mHeight, inDestination, inSource, inSourceRect,
             inMode, inColor, inDepth, inLinear, theOffset);
}

// TEST ONLY (tools/harness/presentation_bridge.c finds these by map address): a callback before every presented
// frame, and a read-back of a frame as 32-bit BGRA rows. Kept by /include so the linker does not drop them.
// (inWindow: the window's native handle, an HWND on Win32)
typedef void (*GXTestPresentHook)(void* inWindow, GXSurface* inFrame);
static GXTestPresentHook sTestPresentHook;
#if defined(_M_IX86)
#pragma comment(linker, "/include:_GXTestSetPresentHook")
#pragma comment(linker, "/include:_GXTestReadFrame")
#else
#pragma comment(linker, "/include:GXTestSetPresentHook")
#pragma comment(linker, "/include:GXTestReadFrame")
#endif

extern "C" void GXTestSetPresentHook(GXTestPresentHook inHook)
{
    GX_GUARD;
    sTestPresentHook = inHook;
}

extern "C" long GXTestReadFrame(GXSurface* inFrame, void* outPixels, unsigned long inCapacity, unsigned long* outWidth,
                                unsigned long* outHeight)
{
    GX_GUARD;
    GXSurfaceData* theData = inFrame->mData;
    *outWidth = theData->mWidth;
    *outHeight = theData->mHeight;
    if (!outPixels || inCapacity < theData->mWidth * theData->mHeight * 4)
        return 1;
    theData->ForCPURead();   // a read-back; the GPU copy stays valid
    if (!theData->mMemory)
        return 1;
    for (unsigned long y = 0; y < theData->mHeight; y++) {
        unsigned char* theOut = (unsigned char*)outPixels + (size_t)y * theData->mWidth * 4;
        if (theData->mFormat.IsBGRA32())
            memcpy(theOut, theData->mMemory + (size_t)y * theData->mPitch, theData->mWidth * 4);
        else
            for (unsigned long x = 0; x < theData->mWidth; x++)
                theData->mFormat.ToBGRA(theData->ReadPixel(x, y), theOut + x * 4);
    }
    return 0;
}

void GXSurfaceData::Present(GXSurfaceData* inSource, bool inWait)
{
    GXCore& theCore = GXGetCore();
    if (!this->mWindow || !inSource)
        return;
    if (sTestPresentHook)
        sTestPresentHook(this->mNative.mWindow, inSource->mOwner);
    int32_t theClientWidth = 0, theClientHeight = 0;
    PlatWindowGetClientSize(this->mWindow, &theClientWidth, &theClientHeight);
    if (theClientWidth <= 0 || theClientHeight <= 0)
        return;
    RECT theClient = {0, 0, theClientWidth, theClientHeight};
    inSource->ForGPURead();
    if (!this->mSwapChain) {
        // the wgpu surface of the window's native kind (Win32 HWND, X11 Window, Wayland surface, Metal layer)
        WGPUSurfaceDescriptor theDescriptor = {};
        WGPUSurfaceSourceWindowsHWND theWin32 = {};
        WGPUSurfaceSourceXlibWindow theX11 = {};
        WGPUSurfaceSourceWaylandSurface theWayland = {};
        WGPUSurfaceSourceMetalLayer theMetal = {};
        const PlatNativeWindow& n = this->mNative;
        switch (n.mKind) {
        case PLAT_NATIVE_WIN32:
            theWin32.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
            theWin32.hinstance = n.mDisplay;
            theWin32.hwnd = n.mWindow;
            theDescriptor.nextInChain = &theWin32.chain;
            break;
        case PLAT_NATIVE_X11:
            theX11.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
            theX11.display = n.mDisplay;
            theX11.window = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(n.mWindow));
            theDescriptor.nextInChain = &theX11.chain;
            break;
        case PLAT_NATIVE_WAYLAND:
            theWayland.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
            theWayland.display = n.mDisplay;
            theWayland.surface = n.mWindow;
            theDescriptor.nextInChain = &theWayland.chain;
            break;
        case PLAT_NATIVE_METAL:
            theMetal.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
            theMetal.layer = n.mWindow;
            theDescriptor.nextInChain = &theMetal.chain;
            break;
        default:
            return;
        }
        this->mSwapChain = wgpuInstanceCreateSurface(theCore.mInstance, &theDescriptor);
        if (!this->mSwapChain)
            return;
    }
    WGPUPresentMode theMode = WGPUPresentMode_Fifo;
    if (!inWait || getenv("EE_GX_NOVSYNC")) {
        WGPUSurfaceCapabilities theCaps = {};
        wgpuSurfaceGetCapabilities(this->mSwapChain, theCore.mAdapter, &theCaps);
        for (size_t i = 0; i < theCaps.presentModeCount; i++)
            if (theCaps.presentModes[i] == WGPUPresentMode_Immediate || theCaps.presentModes[i] == WGPUPresentMode_Mailbox)
                theMode = theCaps.presentModes[i];
        if (this->mSwapFormat == WGPUTextureFormat_Undefined && theCaps.formatCount)
            this->mSwapFormat = theCaps.formats[0];
        wgpuSurfaceCapabilitiesFreeMembers(theCaps);
    }
    if (this->mSwapFormat == WGPUTextureFormat_Undefined) {
        WGPUSurfaceCapabilities theCaps = {};
        wgpuSurfaceGetCapabilities(this->mSwapChain, theCore.mAdapter, &theCaps);
        this->mSwapFormat = theCaps.formatCount ? theCaps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
        for (size_t i = 0; i < theCaps.formatCount; i++)
            if (theCaps.formats[i] == WGPUTextureFormat_BGRA8Unorm)
                this->mSwapFormat = WGPUTextureFormat_BGRA8Unorm;
        wgpuSurfaceCapabilitiesFreeMembers(theCaps);
    }
    if ((unsigned long)theClient.right != this->mSwapWidth || (unsigned long)theClient.bottom != this->mSwapHeight ||
        theMode != this->mSwapMode) {
        WGPUSurfaceConfiguration theConfig = {};
        theConfig.device = theCore.mDevice;
        theConfig.format = this->mSwapFormat;
        theConfig.usage = WGPUTextureUsage_RenderAttachment;
        theConfig.width = theClient.right;
        theConfig.height = theClient.bottom;
        theConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
        theConfig.presentMode = theMode;
        wgpuSurfaceConfigure(this->mSwapChain, &theConfig);
        this->mSwapWidth = theClient.right;
        this->mSwapHeight = theClient.bottom;
        this->mSwapMode = theMode;
    }
    WGPUSurfaceTexture theTexture = {};
    wgpuSurfaceGetCurrentTexture(this->mSwapChain, &theTexture);
    if (theTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        theTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        if (theTexture.texture)
            wgpuTextureRelease(theTexture.texture);
        this->mSwapWidth = 0;   // reconfigure next time
        return;
    }
    RECT theAll = {0, 0, (long)this->mSwapWidth, (long)this->mSwapHeight};
    bool theLinear = this->mSwapWidth != inSource->mWidth || this->mSwapHeight != inSource->mHeight;
    unsigned long theOffset = BlitUniform(this->mSwapWidth, this->mSwapHeight, theAll, inSource, NULL, kGXBlitCopy, NULL,
                                          0.0f, true);
    theCore.EndPass();
    if (!theCore.mEncoder)
        theCore.mEncoder = wgpuDeviceCreateCommandEncoder(theCore.mDevice, NULL);
    WGPUTextureView theView = wgpuTextureCreateView(theTexture.texture, NULL);
    WGPURenderPassColorAttachment theColor = {};
    theColor.view = theView;
    theColor.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    theColor.loadOp = WGPULoadOp_Clear;
    theColor.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor thePass = {};
    thePass.colorAttachmentCount = 1;
    thePass.colorAttachments = &theColor;
    theCore.mPass = wgpuCommandEncoderBeginRenderPass(theCore.mEncoder, &thePass);
    DrawBlit(this->mSwapFormat, this->mSwapWidth, this->mSwapHeight, theAll, inSource, NULL, kGXBlitCopy, NULL, 0.0f,
             theLinear, theOffset);
    theCore.Submit();
    wgpuSurfacePresent(this->mSwapChain);
    wgpuTextureViewRelease(theView);
    wgpuTextureRelease(theTexture.texture);
}

GXSurface::GXSurface() : mData(new GXSurfaceData)
{
    this->mData->mOwner = this;
}

GXSurface::~GXSurface()
{
    GXSurfaceData* theData = this->mData;
    GXCore& theCore = GXGetCore();
    if (theCore.mPassTarget == theData || theCore.mPassDepth == theData)
        theCore.EndPass();
    for (size_t i = 0; i < theData->mAttached.size(); i++)
        theData->mAttached[i]->Release();
    for (std::map<unsigned long, WGPUBindGroup>::iterator i = theData->mGroups.begin(); i != theData->mGroups.end(); ++i)
        wgpuBindGroupRelease(i->second);
    free(theData->mMemory);
    if (theData->mTargetView)
        wgpuTextureViewRelease(theData->mTargetView);
    if (theData->mView)
        wgpuTextureViewRelease(theData->mView);
    if (theData->mTexture)
        wgpuTextureRelease(theData->mTexture);
    if (theData->mSwapChain)
        wgpuSurfaceRelease(theData->mSwapChain);
    delete theData;
}

unsigned long GXSurface::AddRef()
{
    GX_GUARD;
    return ++this->mData->mReferences;
}

unsigned long GXSurface::Release()
{
    GX_GUARD;
    unsigned long theCount = --this->mData->mReferences;
    if (theCount == 0)
        delete this;
    return theCount;
}

GXRESULT GXSurface::AddAttachedSurface(GXSurface* inSurface)
{
    GX_GUARD;
    if (!inSurface)
        return GXERR_INVALIDPARAMS;
    inSurface->AddRef();
    this->mData->mAttached.push_back(inSurface);
    if (inSurface->mData->mIsZ) {
        GXCore& theCore = GXGetCore();
        if (theCore.mPassTarget == this->mData)
            theCore.EndPass();
        this->mData->mDepth = inSurface->mData;
    }
    return GX_OK;
}

GXRESULT GXSurface::DeleteAttachedSurface(unsigned long inFlags, GXSurface* inSurface)
{
    GX_GUARD;
    std::vector<GXSurface*>& theAttached = this->mData->mAttached;
    std::vector<GXSurface*>::iterator theFound = std::find(theAttached.begin(), theAttached.end(), inSurface);
    if (theFound == theAttached.end())
        return GXERR_NOTFOUND;
    theAttached.erase(theFound);
    if (this->mData->mDepth == inSurface->mData) {
        GXCore& theCore = GXGetCore();
        if (theCore.mPassTarget == this->mData)
            theCore.EndPass();
        this->mData->mDepth = NULL;
    }
    inSurface->Release();
    return GX_OK;
}

GXRESULT GXSurface::GetAttachedSurface(GXSCAPS* inCaps, GXSurface** outSurface)
{
    GX_GUARD;
    *outSurface = NULL;
    for (size_t i = 0; i < this->mData->mAttached.size(); i++) {
        GXSurfaceData* theData = this->mData->mAttached[i]->mData;
        bool theMatch = (inCaps->dwCaps & GXSCAPS_ZBUFFER) ? theData->mIsZ :
                        (inCaps->dwCaps & GXSCAPS_MIPMAP) ? theData->mMipTop != NULL :
                        (inCaps->dwCaps & GXSCAPS_FLIP) ? !theData->mIsZ && !theData->mMipTop : false;
        if (theMatch) {
            *outSurface = this->mData->mAttached[i];
            (*outSurface)->AddRef();
            return GX_OK;
        }
    }
    return GXERR_NOTFOUND;
}

// DirectDraw Blt on the CPU: nearest-neighbour stretch, source colour key compared in the source format
static void CPUBlt(GXSurfaceData* inTarget, const RECT& inDestination, GXSurfaceData* inSource, const RECT& inSourceRect,
                   bool inKeyed)
{
    long theDW = inDestination.right - inDestination.left, theDH = inDestination.bottom - inDestination.top;
    long theSW = inSourceRect.right - inSourceRect.left, theSH = inSourceRect.bottom - inSourceRect.top;
    if (theDW <= 0 || theDH <= 0 || theSW <= 0 || theSH <= 0)
        return;
    bool theSame = inTarget->mFormat.mBits == inSource->mFormat.mBits &&
                   !memcmp(inTarget->mFormat.mMask, inSource->mFormat.mMask, sizeof(inTarget->mFormat.mMask));
    unsigned long theBytes = inTarget->mFormat.mBits / 8;
    if (theSame && !inKeyed && theDW == theSW && theDH == theSH && inSource != inTarget) {
        for (long y = 0; y < theDH; y++)
            memcpy(inTarget->mMemory + (size_t)(inDestination.top + y) * inTarget->mPitch + inDestination.left * theBytes,
                   inSource->mMemory + (size_t)(inSourceRect.top + y) * inSource->mPitch + inSourceRect.left * theBytes,
                   theDW * theBytes);
        return;
    }
    unsigned long theKeyMask = ~inSource->mFormat.mMask[3];
    for (long y = 0; y < theDH; y++) {
        long theSY = inSourceRect.top + (long)((long long)y * theSH / theDH);
        if (theSY < 0 || theSY >= (long)inSource->mHeight || inDestination.top + y < 0 ||
            inDestination.top + y >= (long)inTarget->mHeight)
            continue;
        for (long x = 0; x < theDW; x++) {
            long theSX = inSourceRect.left + (long)((long long)x * theSW / theDW);
            long theX = inDestination.left + x;
            if (theSX < 0 || theSX >= (long)inSource->mWidth || theX < 0 || theX >= (long)inTarget->mWidth)
                continue;
            unsigned long thePixel = inSource->ReadPixel(theSX, theSY);
            if (inKeyed && (thePixel & theKeyMask) == (inSource->mKey & theKeyMask))
                continue;
            if (!theSame) {
                unsigned char theBGRA[4];
                inSource->mFormat.ToBGRA(thePixel, theBGRA);
                thePixel = inTarget->mFormat.FromBGRA(theBGRA);
            }
            inTarget->WritePixel(theX, inDestination.top + y, thePixel);
        }
    }
}

static RECT Whole(GXSurfaceData* inSurface)
{
    RECT theRect = {0, 0, (long)inSurface->mWidth, (long)inSurface->mHeight};
    return theRect;
}

static RECT Clip(const RECT& inRect, GXSurfaceData* inSurface)
{
    RECT theRect = inRect;
    theRect.left = std::max<LONG>(theRect.left, 0);
    theRect.top = std::max<LONG>(theRect.top, 0);
    theRect.right = std::min<LONG>(theRect.right, (LONG)inSurface->mWidth);
    theRect.bottom = std::min<LONG>(theRect.bottom, (LONG)inSurface->mHeight);
    return theRect;
}

GXRESULT GXSurface::Blt(RECT* inDestination, GXSurface* inSource, RECT* inSourceRect, unsigned long inFlags, GXBLTFX* inFX)
{
    GX_GUARD;
    GXSurfaceData* theTarget = this->mData;
    if (theTarget->mIsZ)
        return GXERR_UNSUPPORTED;
    if (inFlags & GXBLT_COLORFILL) {
        if (theTarget->mIsPrimary)
            return GX_OK;
        RECT theRect = Clip(inDestination ? *inDestination : Whole(theTarget), theTarget);
        if (theRect.right <= theRect.left || theRect.bottom <= theRect.top)
            return GX_OK;
        unsigned long theFill = inFX ? inFX->dwFillColor : 0;
        if (!GPUPreferred(theTarget)) {
            theTarget->ForCPUWrite();
            for (long y = theRect.top; y < theRect.bottom; y++)
                for (long x = theRect.left; x < theRect.right; x++)
                    theTarget->WritePixel(x, y, theFill);
        } else {
            unsigned char theBGRA[4];
            theTarget->mFormat.ToBGRA(theFill, theBGRA);
            float theColor[4] = {theBGRA[2] / 255.0f, theBGRA[1] / 255.0f, theBGRA[0] / 255.0f, theBGRA[3] / 255.0f};
            GXBlit(theTarget, theRect, NULL, NULL, kGXBlitFill, theColor, 0.0f, false);
        }
        return GX_OK;
    }
    if (!inSource)
        return GXERR_INVALIDPARAMS;
    GXSurfaceData* theSource = inSource->mData;
    if (theTarget->mIsPrimary) {   // windowed presentation: the back buffer to the window
        theTarget->Present(theSource, true);
        return GX_OK;
    }
    RECT theSourceRect = inSourceRect ? *inSourceRect : Whole(theSource);
    RECT theDestination = inDestination ? *inDestination : Whole(theTarget);
    bool theKeyed = (inFlags & GXBLT_KEYSRC) && theSource->mHasKey;
    if (!GPUPreferred(theTarget) && theSource->mCPUValid && theSource->mMemory && theSource != theTarget) {
        theTarget->ForCPUWrite();
        CPUBlt(theTarget, theDestination, theSource, theSourceRect, theKeyed);
    } else if (theSource == theTarget) {
        // overlapping copy inside one surface: through the CPU
        theSource->ForCPUWrite();
        std::vector<unsigned char> theCopy((size_t)theSource->mPitch * theSource->mHeight);
        memcpy(&theCopy[0], theSource->mMemory, theCopy.size());
        GXSurfaceData theTemporary = *theSource;
        theTemporary.mMemory = &theCopy[0];
        CPUBlt(theTarget, theDestination, &theTemporary, theSourceRect, theKeyed);
        theTemporary.mMemory = NULL;
        theTemporary.mAttached.clear();
        theTemporary.mGroups.clear();
        theTemporary.mTexture = NULL;
        theTemporary.mView = theTemporary.mTargetView = NULL;
        theTemporary.mSwapChain = NULL;
    } else
        GXBlit(theTarget, theDestination, theSource, &theSourceRect, theKeyed ? kGXBlitKeyed : kGXBlitCopy, NULL, 0.0f,
               false);
    return GX_OK;
}

GXRESULT GXSurface::Flip(GXSurface* inOverride, unsigned long inFlags)
{
    GX_GUARD;
    GXSurfaceData* theBack = NULL;
    for (size_t i = 0; i < this->mData->mAttached.size(); i++)
        if (!this->mData->mAttached[i]->mData->mIsZ)
            theBack = this->mData->mAttached[i]->mData;
    if (theBack)
        this->mData->Present(theBack, !(inFlags & GXFLIP_NOVSYNC));
    return GX_OK;
}

GXRESULT GXSurface::Lock(RECT* inRect, GXSURFACEDESC* outDescription, unsigned long inFlags, HANDLE inEvent)
{
    GX_GUARD;
    GXSurfaceData* theData = this->mData;
    if (theData->mIsPrimary || theData->mIsZ)
        return GXERR_UNSUPPORTED;
    if (inFlags & (GXLOCK_WRITEONLY | GXLOCK_DISCARDCONTENTS))
        theData->mCPUValid = true;   // the old pixels are not wanted
    theData->ForCPURead();
    if (!theData->mMemory)
        return GXERR_OUTOFMEMORY;
    theData->mLocked = true;
    this->GetSurfaceDesc(outDescription);
    outDescription->lpSurface = theData->mMemory;
    if (inRect)
        outDescription->lpSurface = theData->mMemory + (size_t)inRect->top * theData->mPitch +
                                    inRect->left * (theData->mFormat.mBits / 8);
    return GX_OK;
}

GXRESULT GXSurface::Unlock(RECT* inRect)
{
    GX_GUARD;
    if (!this->mData->mLocked)
        return GXERR_NOTLOCKED;
    this->mData->mLocked = false;
    this->mData->ForCPUWrite();
    return GX_OK;
}

GXRESULT GXSurface::SetColorKey(unsigned long inFlags, GXCOLORKEY* inKey)
{
    GX_GUARD;
    if (inKey) {
        bool theChanged = !this->mData->mHasKey || this->mData->mKey != inKey->dwColorSpaceLowValue;
        this->mData->mHasKey = true;
        this->mData->mKey = inKey->dwColorSpaceLowValue;
        if (theChanged && this->mData->mIsTexture) {   // the key is baked into the texture's alpha
            this->mData->ForCPURead();
            this->mData->mGPUValid = false;
        }
    } else
        this->mData->mHasKey = false;
    return GX_OK;
}

GXRESULT GXSurface::GetSurfaceDesc(GXSURFACEDESC* outDescription)
{
    GX_GUARD;
    memset(outDescription, 0, sizeof(*outDescription));
    outDescription->dwSize = sizeof(*outDescription);
    outDescription->dwFlags = GXSD_CAPS | GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT;
    outDescription->dwWidth = this->mData->mWidth;
    outDescription->dwHeight = this->mData->mHeight;
    outDescription->lPitch = this->mData->mPitch ? this->mData->mPitch :
                             (long)((this->mData->mWidth * (this->mData->mFormat.mBits / 8) + 3) & ~3u);
    outDescription->ddpfPixelFormat = this->mData->mPixelFormat;
    outDescription->ddsCaps.dwCaps = this->mData->mCaps;
    outDescription->ddsCaps.dwCaps2 = this->mData->mCaps2;
    return GX_OK;
}

GXRESULT GXSurface::Restore()
{
    return GX_OK;
}

GXRESULT GXSurface::IsLost()
{
    return GX_OK;
}

bool GXSurface::IsGPUResident() const
{
    GX_GUARD;
    return !this->mData->mCPUValid;
}

GXRESULT GXSurface::DrawText(const RECT& inBounds, TextCallback inDraw, void* inContext)
{
    GX_GUARD;
    GXSurfaceData* theTarget = this->mData;
    RECT theBounds = Clip(inBounds, theTarget);
    long theWidth = theBounds.right - theBounds.left, theHeight = theBounds.bottom - theBounds.top;
    if (theWidth <= 0 || theHeight <= 0)
        return GX_OK;
    std::vector<unsigned char> theBits((size_t)theWidth * theHeight * 4);
    PlatFontTarget theDraw = {&theBits[0], (int32_t)(theWidth * 4), (int32_t)theWidth, (int32_t)theHeight, 0};
    if (theTarget->mIsPrimary || theTarget->mIsZ)
        return GXERR_UNSUPPORTED;
    if (!theTarget->mCPUValid) {
        // GPU-resident (a render target drawn into): the text goes onto a transparent buffer as premultiplied colour
        // and coverage, which is blended onto the GPU copy (no read-back)
        theDraw.mFlags = PLAT_FONT_TARGET_PREMULTIPLIED;
        if (!inDraw(theDraw, theBounds.left, theBounds.top, inContext))
            return GXERR_GENERIC;
        bool theAny = false;
        for (size_t i = 3; i < theBits.size() && !theAny; i += 4)
            theAny = theBits[i] != 0;
        if (!theAny)
            return GX_OK;
        GXSurfaceData* theText = new GXSurfaceData;
        theText->mWidth = theWidth;
        theText->mHeight = theHeight;
        GXPIXELFORMAT theFormat = {sizeof(GXPIXELFORMAT), GXPF_RGB | GXPF_ALPHAPIXELS, 0, 32, 0xff0000, 0xff00, 0xff,
                                   0xff000000};
        theText->mPixelFormat = theFormat;
        theText->mFormat.Set(theFormat);
        theText->mMemory = &theBits[0];
        theText->mPitch = theWidth * 4;
        theText->mGPUValid = false;
        theText->Upload();
        RECT theSource = {0, 0, theWidth, theHeight};
        GXBlit(theTarget, theBounds, theText, &theSource, kGXBlitPremultiplied, NULL, 0.0f, false);
        theText->mMemory = NULL;
        for (std::map<unsigned long, WGPUBindGroup>::iterator i = theText->mGroups.begin(); i != theText->mGroups.end(); ++i)
            wgpuBindGroupRelease(i->second);
        wgpuTextureViewRelease(theText->mTargetView);
        wgpuTextureViewRelease(theText->mView);
        wgpuTextureRelease(theText->mTexture);
        delete theText;
        return GX_OK;
    }
    // the CPU copy: the text draws onto a 32-bit copy of the bounds and the pixels it changed are written back in the
    // surface's format (alpha opaque), as the original's GDI text on the surface's DC
    if (!theTarget->AllocateMemory())   // a new CPU-side surface has no memory until first used
        return GXERR_OUTOFMEMORY;
    for (long y = 0; y < theHeight; y++)
        for (long x = 0; x < theWidth; x++)
            theTarget->mFormat.ToBGRA(theTarget->ReadPixel(theBounds.left + x, theBounds.top + y),
                                      &theBits[((size_t)y * theWidth + x) * 4]);
    std::vector<unsigned char> theBefore(theBits);
    if (!inDraw(theDraw, theBounds.left, theBounds.top, inContext))
        return GXERR_GENERIC;
    for (long y = 0; y < theHeight; y++)
        for (long x = 0; x < theWidth; x++) {
            size_t i = ((size_t)y * theWidth + x) * 4;
            if (memcmp(&theBefore[i], &theBits[i], 3)) {
                theBits[i + 3] = 255;
                theTarget->WritePixel(theBounds.left + x, theBounds.top + y, theTarget->mFormat.FromBGRA(&theBits[i]));
            }
        }
    theTarget->ForCPUWrite();
    return GX_OK;
}

// ---- vertex buffers

GXVertexBuffer::GXVertexBuffer() : mVertices(NULL), mStride(0), mCount(0), mReferences(1)
{
}

GXVertexBuffer::~GXVertexBuffer()
{
    free(this->mVertices);
}

unsigned long GXVertexBuffer::AddRef()
{
    GX_GUARD;
    return ++this->mReferences;
}

unsigned long GXVertexBuffer::Release()
{
    GX_GUARD;
    unsigned long theCount = --this->mReferences;
    if (!theCount)
        delete this;
    return theCount;
}

GXRESULT GXVertexBuffer::Lock(unsigned long inFlags, void** outData, unsigned long* outSize)
{
    *outData = this->mVertices;
    if (outSize)
        *outSize = this->mStride * this->mCount;
    return GX_OK;
}

GXRESULT GXVertexBuffer::Unlock()
{
    return GX_OK;
}

// ---- the display object

static const GXPIXELFORMAT kXRGB32 = {sizeof(GXPIXELFORMAT), GXPF_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0};
static const GXPIXELFORMAT kRGB565 = {sizeof(GXPIXELFORMAT), GXPF_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0};

static void DesktopSize(unsigned long& outWidth, unsigned long& outHeight)
{
    int32_t theWidth = 0, theHeight = 0;
    PlatWindowGetDisplaySize(NULL, &theWidth, &theHeight);   // the primary display (the window system is started)
    outWidth = (unsigned long)theWidth;
    outHeight = (unsigned long)theHeight;
    if (!outWidth || !outHeight) {
        outWidth = 1024;
        outHeight = 768;
    }
}

GXDirectDraw::GXDirectDraw() : mReferences(1), mWindow(NULL), mCooperativeFlags(0), mModeBitCount(32)
{
    DesktopSize(this->mModeWidth, this->mModeHeight);
}

GXDirectDraw::~GXDirectDraw()
{
}

GXRESULT GXDirectDrawCreate(GUID* inGUID, GXDirectDraw** outDirectDraw)
{
    GX_GUARD;
    *outDirectDraw = NULL;
    if (!GXGetCore().Start())
        return GXERR_GENERIC;
    *outDirectDraw = new GXDirectDraw;
    return GX_OK;
}

GXRESULT GXDirectDrawEnumerateEx(GXDDENUMCALLBACKEX inCallback, void* inContext, unsigned long inFlags)
{
    char theDescription[] = "wgpu display";
    char theName[] = "display";
    static char sPrimaryDisplay;   // the token of the one display (DirectDraw's HMONITOR)
    inCallback(NULL, theDescription, theName, inContext, &sPrimaryDisplay);
    return GX_OK;
}

unsigned long GXDirectDraw::AddRef()
{
    GX_GUARD;
    return ++this->mReferences;
}

unsigned long GXDirectDraw::Release()
{
    GX_GUARD;
    unsigned long theCount = --this->mReferences;
    if (!theCount)
        delete this;
    return theCount;
}

GXRESULT GXDirectDraw::QueryInterface(GXIID inIID, void** outObject)
{
    if (inIID != GXIID_Direct3D7) {
        *outObject = NULL;
        return GXERR_UNSUPPORTED;
    }
    this->AddRef();
    *outObject = this;
    return GX_OK;
}

GXRESULT GXDirectDraw::GetCaps(GXCAPS* outCaps, GXCAPS* outHELCaps)
{
    if (outCaps) {
        outCaps->dwCaps = GXCAPS_3D;
        outCaps->dwFXCaps = GXFXCAPS_BLTARITHSTRETCHY | GXFXCAPS_BLTSHRINKX | GXFXCAPS_BLTSHRINKY |
                            GXFXCAPS_BLTSTRETCHX | GXFXCAPS_BLTSTRETCHY;
    }
    if (outHELCaps) {
        outHELCaps->dwCaps = 0;
        outHELCaps->dwFXCaps = 0;
    }
    return GX_OK;
}

GXRESULT GXDirectDraw::EnumDisplayModes(unsigned long inFlags, GXSURFACEDESC* inFilter, void* inContext,
                                        GXENUMMODESCALLBACK inCallback)
{
    // the game picks among its own resolutions: offer the usual 4:3 and wide sizes up to the desktop, in 16 and 32 bits
    static const unsigned long kSizes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {1152, 864}, {1280, 720},
                                              {1280, 960}, {1280, 1024}, {1366, 768}, {1600, 900}, {1600, 1200},
                                              {1920, 1080}, {1920, 1200}, {2560, 1440}, {3840, 2160}};
    unsigned long theDesktopWidth, theDesktopHeight;
    DesktopSize(theDesktopWidth, theDesktopHeight);
    const size_t theCount = sizeof(kSizes) / sizeof(kSizes[0]);
    bool theDesktopListed = false;
    for (size_t i = 0; i < theCount; i++)
        theDesktopListed |= kSizes[i][0] == theDesktopWidth && kSizes[i][1] == theDesktopHeight;
    for (int theDepth = 0; theDepth < 2; theDepth++)
        for (size_t i = 0; i <= theCount; i++) {
            // the desktop's own size is always a mode (the screen's current mode is looked up among them)
            unsigned long theWidth = i < theCount ? kSizes[i][0] : theDesktopWidth;
            unsigned long theHeight = i < theCount ? kSizes[i][1] : theDesktopHeight;
            if (i == theCount && theDesktopListed)
                continue;
            // modes up to 1600x1200 always (a window can be larger than a small desktop), larger ones when they fit
            if (i < theCount && theWidth > 1600 && (theWidth > theDesktopWidth || theHeight > theDesktopHeight))
                continue;
            GXSURFACEDESC theMode = {};
            theMode.dwSize = sizeof(theMode);
            theMode.dwFlags = GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT;
            theMode.dwWidth = theWidth;
            theMode.dwHeight = theHeight;
            theMode.dwRefreshRate = 60;
            theMode.ddpfPixelFormat = theDepth ? kXRGB32 : kRGB565;
            if (inCallback(&theMode, inContext) == GXENUMRET_CANCEL)
                return GX_OK;
        }
    return GX_OK;
}

GXRESULT GXDirectDraw::GetDisplayMode(GXSURFACEDESC* outMode)
{
    memset(outMode, 0, sizeof(*outMode));
    outMode->dwSize = sizeof(*outMode);
    outMode->dwFlags = GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT;
    outMode->dwWidth = this->mModeWidth;
    outMode->dwHeight = this->mModeHeight;
    outMode->dwRefreshRate = 60;
    outMode->ddpfPixelFormat = this->mModeBitCount == 16 ? kRGB565 : kXRGB32;
    return GX_OK;
}

GXRESULT GXDirectDraw::SetDisplayMode(unsigned long inWidth, unsigned long inHeight, unsigned long inBitCount,
                                      unsigned long inRefreshRate, unsigned long inFlags)
{
    // no mode switch: the game renders at this size and the frame is scaled to the window
    this->mModeWidth = inWidth;
    this->mModeHeight = inHeight;
    this->mModeBitCount = inBitCount;
    return GX_OK;
}

GXRESULT GXDirectDraw::SetCooperativeLevel(PlatWindow* inWindow, unsigned long inFlags)
{
    this->mWindow = inWindow;
    this->mCooperativeFlags = inFlags;
    return GX_OK;
}

GXRESULT GXDirectDraw::CreateSurface(GXSURFACEDESC* inDescription, GXSurface** outSurface, void* inOuter)
{
    GX_GUARD;
    *outSurface = NULL;
    unsigned long theCaps = inDescription->ddsCaps.dwCaps;
    GXSurface* theSurface = new GXSurface;
    GXSurfaceData* theData = theSurface->mData;
    theData->mCaps = theCaps;
    theData->mCaps2 = inDescription->ddsCaps.dwCaps2;
    theData->mIsPrimary = (theCaps & GXSCAPS_PRIMARYSURFACE) != 0;
    theData->mIsZ = (theCaps & GXSCAPS_ZBUFFER) != 0;
    theData->mIsTexture = (theCaps & GXSCAPS_TEXTURE) != 0;
    theData->mIsRenderTarget = (theCaps & GXSCAPS_3DDEVICE) != 0 && !theData->mIsTexture;
    if (theData->mIsPrimary) {
        theData->mWidth = this->mModeWidth;
        theData->mHeight = this->mModeHeight;
        theData->mPixelFormat = this->mModeBitCount == 16 ? kRGB565 : kXRGB32;
        theData->mWindow = this->mWindow;
        if (this->mWindow)
            PlatWindowGetNative(this->mWindow, &theData->mNative);
    } else {
        theData->mWidth = inDescription->dwWidth;
        theData->mHeight = inDescription->dwHeight;
        theData->mPixelFormat = (inDescription->dwFlags & GXSD_PIXELFORMAT) ? inDescription->ddpfPixelFormat :
                                (this->mModeBitCount == 16 ? kRGB565 : kXRGB32);
    }
    theData->mFormat.Set(theData->mPixelFormat);
    if (!theData->mIsZ && !theData->mIsPrimary && theData->mFormat.mBits != 16 && theData->mFormat.mBits != 32) {
        theSurface->Release();   // DXT1 and other formats the renderer never creates
        return GXERR_UNSUPPORTED;
    }
    if (theData->mIsZ) {
        theData->mCPUValid = false;
        theData->mGPUValid = true;
    }
    if (inDescription->dwFlags & GXSD_CKSRCBLT) {
        theData->mHasKey = true;
        theData->mKey = inDescription->ddckCKSrcBlt.dwColorSpaceLowValue;
    }
    if ((theCaps & GXSCAPS_MIPMAP) && (inDescription->dwFlags & GXSD_MIPMAPCOUNT) && inDescription->dwMipMapCount > 1) {
        unsigned long theLevels = inDescription->dwMipMapCount;
        theData->mMipLevels = theLevels;
        GXSurfaceData* theParent = theData;
        for (unsigned long i = 1; i < theLevels; i++) {
            GXSurface* theLevel = new GXSurface;
            GXSurfaceData* theLevelData = theLevel->mData;
            theLevelData->mCaps = theCaps;
            theLevelData->mIsTexture = true;
            theLevelData->mWidth = std::max(1ul, theData->mWidth >> i);
            theLevelData->mHeight = std::max(1ul, theData->mHeight >> i);
            theLevelData->mPixelFormat = theData->mPixelFormat;
            theLevelData->mFormat = theData->mFormat;
            theLevelData->mMipTop = theData;
            theLevelData->mMipLevel = i;
            theParent->mAttached.push_back(theLevel);
            theParent = theLevelData;
        }
    }
    if (theData->mIsPrimary && (theCaps & GXSCAPS_FLIP)) {
        unsigned long theCount = std::max(1ul, inDescription->dwBackBufferCount);
        GXSurfaceData* theParent = theData;
        for (unsigned long i = 0; i < theCount && i < 1; i++) {   // one back buffer is enough: frames are presented
            GXSurface* theBack = new GXSurface;
            GXSurfaceData* theBackData = theBack->mData;
            theBackData->mCaps = (theCaps & ~(GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP)) | GXSCAPS_3DDEVICE;
            theBackData->mIsRenderTarget = true;
            theBackData->mWidth = theData->mWidth;
            theBackData->mHeight = theData->mHeight;
            theBackData->mPixelFormat = theData->mPixelFormat;
            theBackData->mFormat = theData->mFormat;
            theParent->mAttached.push_back(theBack);
        }
    }
    *outSurface = theSurface;
    return GX_OK;
}

GXRESULT GXDirectDraw::GetAvailableVidMem(GXSCAPS* inCaps, unsigned long* outTotal, unsigned long* outFree)
{
    *outTotal = 512u << 20;
    *outFree = 448u << 20;
    return GX_OK;
}

GXRESULT GXDirectDraw::GetDeviceIdentifier(GXDEVICEIDENTIFIER* outIdentifier, unsigned long inFlags)
{
    GX_GUARD;
    GXCore& theCore = GXGetCore();
    memset(outIdentifier, 0, sizeof(*outIdentifier));
    const WGPUStringView& theName = theCore.mInfo.device;
    size_t theLength = theName.data ? (theName.length == WGPU_STRLEN ? strlen(theName.data) : theName.length) : 0;
    snprintf(outIdentifier->szDescription, sizeof(outIdentifier->szDescription), "wgpu %.*s", (int)theLength,
             theName.data ? theName.data : "");
    outIdentifier->dwVendorId = theCore.mInfo.vendorID;
    outIdentifier->dwDeviceId = theCore.mInfo.deviceID;
    return GX_OK;
}

GXRESULT GXDirectDraw::RestoreAllSurfaces()
{
    return GX_OK;
}

GXRESULT GXDirectDraw::TestCooperativeLevel()
{
    return GX_OK;
}

GXRESULT GXDirectDraw::FlipToGDISurface()
{
    return GX_OK;
}

GXRESULT GXDirectDraw::EnumDevices(GXENUMDEVICESCALLBACK inCallback, void* inContext)
{
    GXDEVICEDESC theCaps = {};
    theCaps.dwDevCaps = GXDEVCAPS_HWRASTERIZATION | GXDEVCAPS_HWTRANSFORMANDLIGHT | GXDEVCAPS_FLOATTLVERTEX;
    theCaps.dwDeviceRenderBitDepth = GXBD_16 | GXBD_32;
    char theDescription[] = "wgpu";
    char theName[] = "wgpu";
    inCallback(theDescription, theName, &theCaps, inContext);
    return GX_OK;
}

GXRESULT GXDirectDraw::CreateVertexBuffer(GXVERTEXBUFFERDESC* inDescription, GXVertexBuffer** outBuffer,
                                          unsigned long inFlags)
{
    GX_GUARD;
    unsigned long theStride = 0;
    if (inDescription->dwFVF & GXFVF_XYZ) theStride += 12;
    if (inDescription->dwFVF & GXFVF_NORMAL) theStride += 12;
    if (inDescription->dwFVF & GXFVF_DIFFUSE) theStride += 4;
    if (inDescription->dwFVF & GXFVF_TEX1) theStride += 8;
    GXVertexBuffer* theBuffer = new GXVertexBuffer;
    theBuffer->mStride = theStride;
    theBuffer->mCount = inDescription->dwNumVertices;
    theBuffer->mVertices = (unsigned char*)calloc(theStride, inDescription->dwNumVertices);
    *outBuffer = theBuffer;
    return GX_OK;
}
