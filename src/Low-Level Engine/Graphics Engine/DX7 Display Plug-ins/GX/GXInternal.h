// GX internals shared by GX.cpp (core, surfaces, display device) and GXDevice.cpp (the 3D device). See include/gx/GX.h.
#ifndef GX_GXINTERNAL_H
#define GX_GXINTERNAL_H

#ifndef NOMINMAX
#define NOMINMAX   // std::min/std::max
#endif
#include <map>
#include <mutex>
#include <vector>

#include "gx/GX.h"
#include "webgpu/webgpu.h"
#include "webgpu/wgpu.h"

// every public GX entry point holds this (recursive: GX calls itself)
std::recursive_mutex& GXLock();
#define GX_GUARD std::lock_guard<std::recursive_mutex> theGuard(GXLock())

void GXLog(const char* inFormat, ...);

// A pixel format as channel masks (DirectDraw's description of a surface's memory)
struct GXFormat {
    unsigned long mBits;            // bits per pixel (16, 32; 0 for the Z buffer)
    unsigned long mMask[4];         // R, G, B, A
    unsigned long mShift[4];
    unsigned long mWidth[4];        // bits per channel
    bool mIsZ;
    void Set(const GXPIXELFORMAT& inFormat);
    bool IsBGRA32() const { return mBits == 32 && mMask[0] == 0x00ff0000 && mMask[1] == 0x0000ff00 && mMask[2] == 0xff; }
    // one pixel <-> B, G, R, A bytes (the GPU textures are BGRA8Unorm); no alpha mask -> 255
    void ToBGRA(unsigned long inPixel, unsigned char* outBGRA) const;
    unsigned long FromBGRA(const unsigned char* inBGRA) const;
};

// The GPU side of the process (one adapter, one device)
struct GXCore {
    WGPUInstance mInstance = NULL;
    WGPUAdapter mAdapter = NULL;
    WGPUDevice mDevice = NULL;
    WGPUQueue mQueue = NULL;
    WGPUAdapterInfo mInfo = {};
    bool mFailed = false;

    // the command stream: one open encoder, at most one open render pass (on mPassTarget [+ mPassDepth])
    WGPUCommandEncoder mEncoder = NULL;
    WGPURenderPassEncoder mPass = NULL;
    GXSurfaceData* mPassTarget = NULL;
    GXSurfaceData* mPassDepth = NULL;
    WGPURenderPipeline mPassPipeline = NULL;
    unsigned long long mSerial = 1;     // the open encoder's number (resources record the serial that used them)

    // rings written with wgpuQueueWriteBuffer: offsets restart after each submit
    WGPUBuffer mVertexRing = NULL, mIndexRing = NULL, mUniformRing = NULL;
    unsigned long mVertexOffset = 0, mIndexOffset = 0, mUniformOffset = 0;
    static const unsigned long kVertexRingSize = 16u << 20, kIndexRingSize = 4u << 20, kUniformRingSize = 8u << 20;

    // shared objects
    WGPUBindGroupLayout mLayout = NULL;          // uniform (dynamic) + texture + sampler
    WGPUPipelineLayout mPipelineLayout = NULL;
    WGPUShaderModule mShader[2] = {NULL, NULL};  // fixed function: smooth, flat
    WGPUShaderModule mBlitShader = NULL;
    std::map<unsigned long, WGPURenderPipeline> mPipelines;
    std::map<unsigned long, WGPURenderPipeline> mBlitPipelines;
    std::map<unsigned long, WGPUSampler> mSamplers;
    WGPUTexture mWhite = NULL;                   // 1x1 white: the texture of untextured draws
    WGPUTextureView mWhiteView = NULL;
    std::map<unsigned long, WGPUBindGroup> mWhiteGroups;

    bool Start();
    void EndPass();
    // opens (or keeps) a render pass on inTarget with inDepth (may be NULL)
    void BeginPass(GXSurfaceData* inTarget, GXSurfaceData* inDepth);
    void Submit();
    void Wait();
    WGPUSampler Sampler(unsigned long inKey);   // see GXSamplerKey
    WGPUBindGroup Group(GXSurfaceData* inTexture, unsigned long inSamplerKey);
    // copies data into a ring (aligned), submitting first when it is full; returns the offset
    unsigned long Ring(WGPUBuffer inRing, unsigned long& ioOffset, unsigned long inSize, const void* inData,
                       unsigned long inBytes, unsigned long inAlign);
};
GXCore& GXGetCore();

// sampler key: address (2 bits) | min (2) | mag (2) | mip (2)
unsigned long GXSamplerKey(unsigned long inAddress, unsigned long inMin, unsigned long inMag, unsigned long inMip);

// the blits (GX.cpp): a quad from a source texture (or a fill colour) into the open pass
enum GXBlitMode { kGXBlitCopy, kGXBlitKeyed, kGXBlitFill, kGXBlitPremultiplied, kGXBlitDepth };
void GXBlit(GXSurfaceData* inTarget, const RECT& inDestination, GXSurfaceData* inSource, const RECT* inSourceRect,
            GXBlitMode inMode, const float inColor[4], float inDepth, bool inLinear);

struct GXSurfaceData {
    GXSurface* mOwner = NULL;
    unsigned long mReferences = 1;
    unsigned long mCaps = 0, mCaps2 = 0;
    unsigned long mWidth = 0, mHeight = 0;
    GXPIXELFORMAT mPixelFormat = {};
    GXFormat mFormat = {};
    HWND mWindow = NULL;                 // primary: the window it shows in
    bool mIsPrimary = false, mIsZ = false, mIsTexture = false, mIsRenderTarget = false;

    // CPU copy
    unsigned char* mMemory = NULL;
    long mPitch = 0;
    HBITMAP mDIB = NULL;                 // when GetDC moved the memory into a DIB section
    HDC mDC = NULL;
    HGDIOBJ mOldBitmap = NULL;
    unsigned char* mTextCopy = NULL;     // GetDC on a format GDI cannot draw: a 32-bit DIB copy
    bool mDCOut = false;
    bool mLocked = false;
    bool mCPUValid = true, mGPUValid = false;

    // GPU copy (mip levels: the chain's top owns the texture, each level uploads into its own mip)
    WGPUTexture mTexture = NULL;
    WGPUTextureView mView = NULL;        // whole texture (sampling)
    WGPUTextureView mTargetView = NULL;  // mip 0 (render target)
    unsigned long mMipLevels = 1;
    GXSurfaceData* mMipTop = NULL;       // NULL for the top
    unsigned long mMipLevel = 0;
    bool mLevelsDirty = false;           // top: some level's CPU copy is newer
    unsigned long long mUsedSerial = 0;  // the last encoder that read or wrote the texture
    std::map<unsigned long, WGPUBindGroup> mGroups;

    // attachments
    std::vector<GXSurface*> mAttached;   // flip chain / mip chain (owned references) and AddAttachedSurface
    GXSurfaceData* mDepth = NULL;        // the attached Z buffer
    bool mHasKey = false;
    unsigned long mKey = 0;              // source colour key, in the pixel format

    // the window's swap chain (primary surfaces)
    WGPUSurface mSwapChain = NULL;
    unsigned long mSwapWidth = 0, mSwapHeight = 0;
    WGPUTextureFormat mSwapFormat = WGPUTextureFormat_Undefined;
    WGPUPresentMode mSwapMode = WGPUPresentMode_Fifo;

    bool AllocateMemory();
    bool CreateTexture();
    void Upload();                       // CPU -> GPU when the GPU copy is stale (whole surface)
    void UploadLevel(GXSurfaceData* inLevel);
    void Download();                     // GPU -> CPU when the CPU copy is stale
    void ForGPURead();                   // the texture is about to be sampled
    void ForGPUWrite();                  // the texture is about to be drawn into: CPU copy goes stale
    void ForCPURead();
    void ForCPUWrite();                  // after a CPU change: GPU copy goes stale
    GXSurfaceData* Top() { return this->mMipTop ? this->mMipTop : this; }
    unsigned long ReadPixel(unsigned long x, unsigned long y) const;
    void WritePixel(unsigned long x, unsigned long y, unsigned long inPixel);
    void Present(GXSurfaceData* inSource, bool inWait);
};

struct GXDeviceData;   // GXDevice.cpp

#endif
