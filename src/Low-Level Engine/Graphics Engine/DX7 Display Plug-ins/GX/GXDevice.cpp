// GX 3D device (include/gx/GX.h): Direct3D 7's fixed-function pipeline, as far as the renderer uses it, on one WGSL
// shader. Vertices are the renderer's single format (position, normal, ARGB diffuse, one texture coordinate);
// transforms use Direct3D's row-vector convention (v * World * View * Projection) and its pixel centres (integer
// coordinates: half a pixel left/up of WebGPU's). Lighting: Direct3D 7's equation in view space (global ambient,
// emissive, up to 8 point/directional lights with attenuation and range, Blinn specular with a non-local viewer),
// material colours from the material or the vertex colour. Vertex fog (linear in view-space z), one texture stage
// (select/modulate of texture and diffuse), specular add, colour key (texels whose key was baked into alpha 0),
// alpha test, blending and z by pipeline state. Not emulated: wireframe/point fill, dithering, antialiasing.
#include <math.h>
#include "platform/PlatTime.h"
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "GXInternal.h"

extern const char* const kGXFixedFunctionShader = R"WGSL(
struct Light { diffuse: vec4f, specular: vec4f, ambient: vec4f, position: vec4f, direction: vec4f, atten: vec4f,
               kind: vec4u };
struct FF {
    wvp: mat4x4f, wv: mat4x4f,
    matDiffuse: vec4f, matAmbient: vec4f, matSpecular: vec4f, matEmissive: vec4f,
    ambient: vec4f, fogColor: vec4f,
    fog: vec4f,      // start, end, enabled, specular power
    pixel: vec4f,    // 1 / viewport width, 1 / viewport height, alpha reference (0..255), mip bias
    mode: vec4u,     // lighting, diffuse from vertex, ambient from vertex, specular
    stage: vec4u,    // colour op, colour arg 1, colour arg 2, alpha op
    stage2: vec4u,   // alpha arg 1, alpha arg 2, alpha function (0 = no test), colour key
    lights: array<Light, 8>,
};
@group(0) @binding(0) var<uniform> u: FF;
@group(0) @binding(1) var tex: texture_2d<f32>;
@group(0) @binding(2) var smp: sampler;

struct VIn { @location(0) pos: vec3f, @location(1) nrm: vec3f, @location(2) bgra: vec4f, @location(3) uv: vec2f };
struct VOut {
    @builtin(position) pos: vec4f,
    @location(0) INTERP diffuse: vec4f,
    @location(1) INTERP specular: vec4f,
    @location(2) uv: vec2f,
    @location(3) fog: f32,
};

@vertex fn vertex_main(v: VIn) -> VOut {
    var o: VOut;
    var clip = u.wvp * vec4f(v.pos, 1.0);
    clip.x = clip.x + u.pixel.x * clip.w;   // Direct3D 7 pixel centres: half a pixel right and down
    clip.y = clip.y - u.pixel.y * clip.w;
    o.pos = clip;
    o.uv = v.uv;
    let P = (u.wv * vec4f(v.pos, 1.0)).xyz;
    let col = v.bgra.zyxw;   // the ARGB dword's bytes are B, G, R, A
    var diffuse = col;
    var spec = vec4f(0.0);
    if (u.mode.x != 0u) {
        let N = normalize((u.wv * vec4f(v.nrm, 0.0)).xyz);
        let md = select(u.matDiffuse, col, u.mode.y != 0u);
        let ma = select(u.matAmbient, col, u.mode.z != 0u);
        var acc = u.matEmissive.rgb + ma.rgb * u.ambient.rgb;
        var sacc = vec3f(0.0);
        for (var i = 0u; i < 8u; i = i + 1u) {
            let L = u.lights[i];
            if (L.kind.y == 0u) { continue; }
            var dir = -L.direction.xyz;
            var att = 1.0;
            if (L.kind.x != 3u) {
                let d = L.position.xyz - P;
                let dist = length(d);
                if (dist > L.atten.w) { continue; }
                dir = d / max(dist, 1e-6);
                att = 1.0 / max(L.atten.x + L.atten.y * dist + L.atten.z * dist * dist, 1e-6);
            }
            let ndl = dot(N, dir);
            acc = acc + att * (ma.rgb * L.ambient.rgb + md.rgb * L.diffuse.rgb * max(ndl, 0.0));
            if (u.mode.w != 0u && ndl > 0.0) {
                let H = normalize(dir + vec3f(0.0, 0.0, -1.0));
                sacc = sacc + att * u.matSpecular.rgb * L.specular.rgb * pow(max(dot(N, H), 0.0), u.fog.w);
            }
        }
        diffuse = vec4f(clamp(acc, vec3f(0.0), vec3f(1.0)), md.a);
        spec = vec4f(clamp(sacc, vec3f(0.0), vec3f(1.0)), 0.0);
    }
    o.diffuse = diffuse;
    o.specular = spec;
    o.fog = 1.0;
    if (u.fog.z != 0.0) {
        o.fog = clamp((u.fog.y - P.z) / max(u.fog.y - u.fog.x, 1e-6), 0.0, 1.0);
    }
    return o;
}

fn arg(a: u32, t: vec4f, d: vec4f) -> vec4f {
    if (a == 2u) { return t; }
    return d;   // diffuse; stage 0's current is the diffuse colour
}

fn op(o: u32, a1: vec4f, a2: vec4f) -> vec4f {
    if (o == 2u) { return a1; }
    if (o == 3u) { return a2; }
    if (o == 4u) { return a1 * a2; }
    return a2;
}

fn passes(f: u32, a: f32, r: f32) -> bool {
    switch f {
        case 1u: { return false; }
        case 2u: { return a < r; }
        case 3u: { return a == r; }
        case 4u: { return a <= r; }
        case 5u: { return a > r; }
        case 6u: { return a != r; }
        case 7u: { return a >= r; }
        default: { return true; }
    }
}

@fragment fn fragment_main(i: VOut) -> @location(0) vec4f {
    let t = textureSampleBias(tex, smp, i.uv, u.pixel.w);
    var c = i.diffuse;
    if (u.stage.x != 1u) {
        let rgb = op(u.stage.x, arg(u.stage.y, t, i.diffuse), arg(u.stage.z, t, i.diffuse)).rgb;
        var a = i.diffuse.a;
        if (u.stage.w != 1u) {
            a = op(u.stage.w, arg(u.stage2.x, t, i.diffuse), arg(u.stage2.y, t, i.diffuse)).a;
        }
        c = vec4f(rgb, a);
        if (u.stage2.w != 0u && t.a == 0.0) { discard; }
    }
    var rgb = c.rgb;
    if (u.mode.w != 0u) { rgb = min(rgb + i.specular.rgb, vec3f(1.0)); }
    rgb = mix(u.fogColor.rgb, rgb, i.fog);
    if (u.stage2.z != 0u && !passes(u.stage2.z, round(c.a * 255.0), u.pixel.z)) { discard; }
    return vec4f(rgb, c.a);
}
)WGSL";

struct GXLightUniform {
    float mDiffuse[4], mSpecular[4], mAmbient[4], mPosition[4], mDirection[4], mAttenuation[4];
    uint32_t mKind[4];
};
struct GXFixedFunctionUniform {
    float mWVP[16], mWV[16];
    float mMaterialDiffuse[4], mMaterialAmbient[4], mMaterialSpecular[4], mMaterialEmissive[4];
    float mAmbient[4], mFogColor[4], mFog[4], mPixel[4];
    uint32_t mMode[4], mStage[4], mStage2[4];
    GXLightUniform mLights[8];
};

struct GXDeviceData {
    unsigned long mReferences = 1;
    GXSurface* mTarget = NULL;
    GXVIEWPORT mViewport = {};
    GXMATRIX mTransform[4] = {};          // [GXTS_WORLD..GXTS_PROJECTION]
    unsigned long mRenderState[GXRS_COUNT] = {};
    unsigned long mStageState[2][GXTSS_COUNT] = {};
    GXSurface* mTexture[2] = {NULL, NULL};
    GXMATERIAL mMaterial = {};
    GXLIGHT mLights[8] = {};
    bool mLightOn[8] = {};
    bool mUniformDirty = true;
    unsigned long mUniformOffset = 0;
    unsigned long long mUniformSerial = 0;
};

static void Identity(GXMATRIX& outMatrix)
{
    memset(&outMatrix, 0, sizeof(outMatrix));
    outMatrix._11 = outMatrix._22 = outMatrix._33 = outMatrix._44 = 1.0f;
}

// row-vector product: a then b
static void Multiply(const GXMATRIX& a, const GXMATRIX& b, float* out)
{
    const float* A = &a._11;
    const float* B = &b._11;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[r * 4 + c] = A[r * 4 + 0] * B[0 * 4 + c] + A[r * 4 + 1] * B[1 * 4 + c] + A[r * 4 + 2] * B[2 * 4 + c] +
                             A[r * 4 + 3] * B[3 * 4 + c];
}

static float AsFloat(unsigned long inValue)
{
    float theValue;
    uint32_t theBits = (uint32_t)inValue;
    memcpy(&theValue, &theBits, 4);
    return theValue;
}

static void Color(unsigned long inARGB, float* out)
{
    out[0] = ((inARGB >> 16) & 0xff) / 255.0f;
    out[1] = ((inARGB >> 8) & 0xff) / 255.0f;
    out[2] = (inARGB & 0xff) / 255.0f;
    out[3] = ((inARGB >> 24) & 0xff) / 255.0f;
}

static void Copy(const GXCOLORVALUE& inColor, float* out)
{
    out[0] = inColor.r;
    out[1] = inColor.g;
    out[2] = inColor.b;
    out[3] = inColor.a;
}

GXDevice::GXDevice() : mData(new GXDeviceData)
{
    GXDeviceData* d = this->mData;
    for (int i = 1; i <= 3; i++)
        Identity(d->mTransform[i]);
    // Direct3D 7's defaults for the states the renderer does not set itself
    d->mRenderState[GXRS_ZENABLE] = 1;
    d->mRenderState[GXRS_ZWRITEENABLE] = 1;
    d->mRenderState[GXRS_ZFUNC] = GXCMP_LESSEQUAL;
    d->mRenderState[GXRS_FILLMODE] = GXFILL_SOLID;
    d->mRenderState[GXRS_SHADEMODE] = GXSHADE_GOURAUD;
    d->mRenderState[GXRS_SRCBLEND] = GXBLEND_ONE;
    d->mRenderState[GXRS_DESTBLEND] = GXBLEND_ZERO;
    d->mRenderState[GXRS_CULLMODE] = GXCULL_CCW;
    d->mRenderState[GXRS_ALPHAFUNC] = GXCMP_ALWAYS;
    d->mRenderState[GXRS_LIGHTING] = 1;
    d->mRenderState[GXRS_COLORVERTEX] = 1;
    d->mRenderState[GXRS_DIFFUSEMATERIALSOURCE] = GXMCS_COLOR1;
    d->mRenderState[GXRS_AMBIENTMATERIALSOURCE] = GXMCS_MATERIAL;
    d->mRenderState[GXRS_FOGEND] = 0x3f800000;   // 1.0f
    for (int s = 0; s < 2; s++) {
        d->mStageState[s][GXTSS_COLOROP] = s ? GXTOP_DISABLE : GXTOP_MODULATE;
        d->mStageState[s][GXTSS_ALPHAOP] = s ? GXTOP_DISABLE : GXTOP_SELECTARG1;
        d->mStageState[s][GXTSS_COLORARG1] = GXTA_TEXTURE;
        d->mStageState[s][GXTSS_COLORARG2] = GXTA_CURRENT;
        d->mStageState[s][GXTSS_ALPHAARG1] = GXTA_TEXTURE;
        d->mStageState[s][GXTSS_ALPHAARG2] = GXTA_CURRENT;
        d->mStageState[s][GXTSS_ADDRESS] = GXTADDRESS_WRAP;
        d->mStageState[s][GXTSS_MAGFILTER] = GXTFG_POINT;
        d->mStageState[s][GXTSS_MINFILTER] = GXTFN_POINT;
        d->mStageState[s][GXTSS_MIPFILTER] = GXTFP_NONE;
    }
}

GXDevice::~GXDevice()
{
    for (int i = 0; i < 2; i++)
        if (this->mData->mTexture[i])
            this->mData->mTexture[i]->Release();
    if (this->mData->mTarget)
        this->mData->mTarget->Release();
    delete this->mData;
}

GXRESULT GXDirectDraw::CreateDevice(GXIID inKind, GXSurface* inTarget, GXDevice** outDevice)
{
    GX_GUARD;
    *outDevice = NULL;
    if (!inTarget)
        return GXERR_INVALIDPARAMS;
    GXDevice* theDevice = new GXDevice;
    theDevice->SetRenderTarget(inTarget, 0);
    GXVIEWPORT theViewport = {0, 0, inTarget->mData->mWidth, inTarget->mData->mHeight, 0.0f, 1.0f};
    theDevice->SetViewport(&theViewport);
    *outDevice = theDevice;
    return GX_OK;
}

unsigned long GXDevice::AddRef()
{
    GX_GUARD;
    return ++this->mData->mReferences;
}

unsigned long GXDevice::Release()
{
    GX_GUARD;
    unsigned long theCount = --this->mData->mReferences;
    if (!theCount)
        delete this;
    return theCount;
}

GXRESULT GXDevice::GetCaps(GXDEVICEDESC* outCaps)
{
    outCaps->dwDevCaps = GXDEVCAPS_HWRASTERIZATION | GXDEVCAPS_HWTRANSFORMANDLIGHT | GXDEVCAPS_FLOATTLVERTEX;
    outCaps->dwDeviceRenderBitDepth = GXBD_16 | GXBD_32;
    return GX_OK;
}

GXRESULT GXDevice::EnumTextureFormats(GXENUMPIXELFORMATSCALLBACK inCallback, void* inContext)
{
    static const GXPIXELFORMAT kFormats[] = {
        {sizeof(GXPIXELFORMAT), GXPF_RGB | GXPF_ALPHAPIXELS, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000},
        {sizeof(GXPIXELFORMAT), GXPF_RGB | GXPF_ALPHAPIXELS, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000},
        {sizeof(GXPIXELFORMAT), GXPF_RGB | GXPF_ALPHAPIXELS, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000},
        {sizeof(GXPIXELFORMAT), GXPF_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0},
    };
    for (size_t i = 0; i < sizeof(kFormats) / sizeof(kFormats[0]); i++) {
        GXPIXELFORMAT theFormat = kFormats[i];
        if (inCallback(&theFormat, inContext) == GXENUMRET_CANCEL)
            break;
    }
    return GX_OK;
}

// TEST ONLY (tools/harness timeshim --no-render): EE_GX_NULL3D=<ms> skips the device's draws and clears, except for
// one whole scene every <ms> real milliseconds (0: never), so accelerated games do not wait for the rasterizer.
static long sNull3DKeyMs = -2;   // -2 not read yet, -1 off
static bool sDrawScene = true;
static unsigned long long sNextKeyScene;

// DIAGNOSTIC (opt-in): EE_GX_STATS=<n> logs a summary of the 3D draws every <n> scenes: draws and primitives by
// topology, lit/textured/blended draws, and the vertices after the world-view-projection transform (in front of the
// camera, inside the clip volume) with their colour range. Shows whether a blank 3D view is "nothing drawn", "drawn
// off screen" or "drawn black".
struct GXDrawStats {
    unsigned long mScenes = 0, mDraws[3] = {}, mPrimitives[3] = {}, mLit = 0, mTextured = 0, mBlended = 0;
    unsigned long mVertices = 0, mInFront = 0, mInside = 0, mClears = 0, mFogged = 0, mZ = 0, mFogColor = 0, mAmbient = 0;
    float mFogStart = 0.0f, mFogEnd = 0.0f;   // of the last fogged draw
    unsigned long mFogZone[3] = {};          // fogged vertices: view z before the fog start, inside the fog, past the end
    unsigned char mMin[4] = {255, 255, 255, 255}, mMax[4] = {};
    float mMinClip[3] = {3e38f, 3e38f, 3e38f}, mMaxClip[3] = {-3e38f, -3e38f, -3e38f};
};
static long sStatsEvery = -2;
static GXDrawStats sStats;

static bool StatsOn()
{
    if (sStatsEvery == -2) {
        const char* theValue = getenv("EE_GX_STATS");
        sStatsEvery = theValue ? std::max(1L, strtol(theValue, NULL, 10)) : -1;
    }
    return sStatsEvery > 0;
}

static void StatsScene()
{
    if (!StatsOn() || ++sStats.mScenes < (unsigned long)sStatsEvery)
        return;
    GXDrawStats& s = sStats;
    GXLog("GX stats: %lu scenes, %lu clears; draws points %lu lines %lu triangles %lu (primitives %lu/%lu/%lu); lit %lu "
          "textured %lu blended %lu fogged %lu z %lu", s.mScenes, s.mClears, s.mDraws[0], s.mDraws[1], s.mDraws[2],
          s.mPrimitives[0], s.mPrimitives[1], s.mPrimitives[2], s.mLit, s.mTextured, s.mBlended, s.mFogged, s.mZ);
    GXLog("GX stats: vertices %lu, in front %lu, inside %lu; clip x %g..%g y %g..%g w %g..%g", s.mVertices, s.mInFront,
          s.mInside, s.mMinClip[0], s.mMaxClip[0], s.mMinClip[1], s.mMaxClip[1], s.mMinClip[2], s.mMaxClip[2]);
    GXLog("GX stats: BGRA min %u,%u,%u,%u max %u,%u,%u,%u; fog %g..%g colour %08lx (view z before/in/past %lu/%lu/%lu); "
          "ambient %08lx", s.mMin[0], s.mMin[1], s.mMin[2], s.mMin[3], s.mMax[0], s.mMax[1], s.mMax[2], s.mMax[3],
          s.mFogStart, s.mFogEnd, s.mFogColor, s.mFogZone[0], s.mFogZone[1], s.mFogZone[2], s.mAmbient);
    sStats = GXDrawStats();
}

GXRESULT GXDevice::BeginScene()
{
    GX_GUARD;
    StatsScene();
    if (sNull3DKeyMs == -2) {
        const char* theValue = getenv("EE_GX_NULL3D");
        sNull3DKeyMs = theValue ? strtol(theValue, NULL, 10) : -1;
        if (theValue)
            GXLog("GX: null 3D device, one scene every %ld ms", sNull3DKeyMs);
    }
    if (sNull3DKeyMs >= 0) {
        unsigned long long theNow = PlatTimeMilliseconds64();
        sDrawScene = sNull3DKeyMs > 0 && theNow >= sNextKeyScene;
        if (sDrawScene)
            sNextKeyScene = theNow + (unsigned long long)sNull3DKeyMs;
    }
    return GX_OK;
}

GXRESULT GXDevice::EndScene()
{
    return GX_OK;
}

GXRESULT GXDevice::SetRenderTarget(GXSurface* inTarget, unsigned long inFlags)
{
    GX_GUARD;
    if (!inTarget)
        return GXERR_INVALIDPARAMS;
    inTarget->AddRef();
    if (this->mData->mTarget)
        this->mData->mTarget->Release();
    this->mData->mTarget = inTarget;
    inTarget->mData->mIsRenderTarget = true;
    return GX_OK;
}

GXRESULT GXDevice::SetViewport(GXVIEWPORT* inViewport)
{
    GX_GUARD;
    this->mData->mViewport = *inViewport;
    this->mData->mUniformDirty = true;
    return GX_OK;
}

GXRESULT GXDevice::GetViewport(GXVIEWPORT* outViewport)
{
    GX_GUARD;
    *outViewport = this->mData->mViewport;
    return GX_OK;
}

GXRESULT GXDevice::Clear(unsigned long inCount, GXRECT* inRects, unsigned long inFlags, unsigned long inColor, float inZ,
                         unsigned long inStencil)
{
    GX_GUARD;
    if (!this->mData->mTarget)
        return GXERR_GENERIC;
    if (!sDrawScene)
        return GX_OK;
    if (StatsOn())
        sStats.mClears++;
    GXSurfaceData* theTarget = this->mData->mTarget->mData;
    const GXVIEWPORT& v = this->mData->mViewport;
    float theColor[4];
    Color(inColor, theColor);
    for (unsigned long i = 0; i < (inRects ? inCount : 1); i++) {
        RECT theRect = {(long)v.dwX, (long)v.dwY, (long)(v.dwX + v.dwWidth), (long)(v.dwY + v.dwHeight)};
        if (inRects) {
            theRect.left = std::max(theRect.left, (LONG)inRects[i].x1);
            theRect.top = std::max(theRect.top, (LONG)inRects[i].y1);
            theRect.right = std::min(theRect.right, (LONG)inRects[i].x2);
            theRect.bottom = std::min(theRect.bottom, (LONG)inRects[i].y2);
        }
        theRect.right = std::min(theRect.right, (LONG)theTarget->mWidth);
        theRect.bottom = std::min(theRect.bottom, (LONG)theTarget->mHeight);
        if (theRect.right <= theRect.left || theRect.bottom <= theRect.top)
            continue;
        if (inFlags & GXCLEAR_TARGET)
            GXBlit(theTarget, theRect, NULL, NULL, kGXBlitFill, theColor, 0.0f, false);
        if (inFlags & GXCLEAR_ZBUFFER)
            GXBlit(theTarget, theRect, NULL, NULL, kGXBlitDepth, theColor, inZ, false);
    }
    return GX_OK;
}

GXRESULT GXDevice::SetTransform(GXTRANSFORMSTATETYPE inState, GXMATRIX* inMatrix)
{
    GX_GUARD;
    if (inState < GXTS_WORLD || inState > GXTS_PROJECTION)
        return GXERR_INVALIDPARAMS;
    this->mData->mTransform[inState] = *inMatrix;
    this->mData->mUniformDirty = true;
    return GX_OK;
}

GXRESULT GXDevice::GetTransform(GXTRANSFORMSTATETYPE inState, GXMATRIX* outMatrix)
{
    GX_GUARD;
    if (inState < GXTS_WORLD || inState > GXTS_PROJECTION)
        return GXERR_INVALIDPARAMS;
    *outMatrix = this->mData->mTransform[inState];
    return GX_OK;
}

GXRESULT GXDevice::SetRenderState(GXRENDERSTATETYPE inState, unsigned long inValue)
{
    GX_GUARD;
    if ((unsigned)inState >= GXRS_COUNT)
        return GXERR_INVALIDPARAMS;
    if (this->mData->mRenderState[inState] != inValue) {
        this->mData->mRenderState[inState] = inValue;
        this->mData->mUniformDirty = true;
    }
    return GX_OK;
}

GXRESULT GXDevice::GetRenderState(GXRENDERSTATETYPE inState, unsigned long* outValue)
{
    GX_GUARD;
    if ((unsigned)inState >= GXRS_COUNT)
        return GXERR_INVALIDPARAMS;
    *outValue = this->mData->mRenderState[inState];
    return GX_OK;
}

GXRESULT GXDevice::SetTextureStageState(unsigned long inStage, GXTEXTURESTAGESTATETYPE inState, unsigned long inValue)
{
    GX_GUARD;
    if (inStage > 1 || (unsigned)inState >= GXTSS_COUNT)
        return GXERR_INVALIDPARAMS;
    this->mData->mStageState[inStage][inState] = inValue;
    this->mData->mUniformDirty = true;
    return GX_OK;
}

GXRESULT GXDevice::SetTexture(unsigned long inStage, GXSurface* inTexture)
{
    GX_GUARD;
    if (inStage > 1)
        return GXERR_INVALIDPARAMS;
    if (inTexture)
        inTexture->AddRef();
    if (this->mData->mTexture[inStage])
        this->mData->mTexture[inStage]->Release();
    this->mData->mTexture[inStage] = inTexture;
    this->mData->mUniformDirty = true;
    return GX_OK;
}

GXRESULT GXDevice::SetMaterial(GXMATERIAL* inMaterial)
{
    GX_GUARD;
    this->mData->mMaterial = *inMaterial;
    this->mData->mUniformDirty = true;
    return GX_OK;
}

GXRESULT GXDevice::SetLight(unsigned long inIndex, GXLIGHT* inLight)
{
    GX_GUARD;
    if (inIndex >= 8)
        return GXERR_INVALIDPARAMS;
    this->mData->mLights[inIndex] = *inLight;
    this->mData->mUniformDirty = true;
    return GX_OK;
}

GXRESULT GXDevice::GetLight(unsigned long inIndex, GXLIGHT* outLight)
{
    GX_GUARD;
    if (inIndex >= 8)
        return GXERR_INVALIDPARAMS;
    *outLight = this->mData->mLights[inIndex];
    return GX_OK;
}

GXRESULT GXDevice::LightEnable(unsigned long inIndex, BOOL inEnable)
{
    GX_GUARD;
    if (inIndex >= 8)
        return GXERR_INVALIDPARAMS;
    this->mData->mLightOn[inIndex] = inEnable != FALSE;
    this->mData->mUniformDirty = true;
    return GX_OK;
}

GXRESULT GXDevice::ValidateDevice(unsigned long* outPasses)
{
    *outPasses = 1;
    return GX_OK;
}

static WGPUBlendFactor BlendFactor(unsigned long inBlend, bool inTargetAlpha)
{
    switch (inBlend) {
    case GXBLEND_ZERO: return WGPUBlendFactor_Zero;
    case GXBLEND_ONE: return WGPUBlendFactor_One;
    case GXBLEND_SRCCOLOR: return WGPUBlendFactor_Src;
    case GXBLEND_INVSRCCOLOR: return WGPUBlendFactor_OneMinusSrc;
    case GXBLEND_SRCALPHA: return WGPUBlendFactor_SrcAlpha;
    case GXBLEND_INVSRCALPHA: return WGPUBlendFactor_OneMinusSrcAlpha;
    case GXBLEND_DESTALPHA: return inTargetAlpha ? WGPUBlendFactor_DstAlpha : WGPUBlendFactor_One;
    case GXBLEND_INVDESTALPHA: return inTargetAlpha ? WGPUBlendFactor_OneMinusDstAlpha : WGPUBlendFactor_Zero;
    case GXBLEND_DESTCOLOR: return WGPUBlendFactor_Dst;
    case GXBLEND_INVDESTCOLOR: return WGPUBlendFactor_OneMinusDst;
    case GXBLEND_SRCALPHASAT: return WGPUBlendFactor_SrcAlphaSaturated;
    default: return WGPUBlendFactor_One;
    }
}

static WGPUCompareFunction Compare(unsigned long inFunction)
{
    switch (inFunction) {
    case GXCMP_NEVER: return WGPUCompareFunction_Never;
    case GXCMP_LESS: return WGPUCompareFunction_Less;
    case GXCMP_EQUAL: return WGPUCompareFunction_Equal;
    case GXCMP_LESSEQUAL: return WGPUCompareFunction_LessEqual;
    case GXCMP_GREATER: return WGPUCompareFunction_Greater;
    case GXCMP_NOTEQUAL: return WGPUCompareFunction_NotEqual;
    case GXCMP_GREATEREQUAL: return WGPUCompareFunction_GreaterEqual;
    default: return WGPUCompareFunction_Always;
    }
}

static WGPUStringView Name(const char* inText)
{
    WGPUStringView theView;
    theView.data = inText;
    theView.length = WGPU_STRLEN;
    return theView;
}

// pipeline key: topology 2 | cull 2 | blend 1 | src 4 | dst 4 | depth 1 | zfunc 4 | zwrite 1 | flat 1 | target alpha 1
static WGPURenderPipeline Pipeline(unsigned long inKey)
{
    GXCore& theCore = GXGetCore();
    std::map<unsigned long, WGPURenderPipeline>::iterator theFound = theCore.mPipelines.find(inKey);
    if (theFound != theCore.mPipelines.end())
        return theFound->second;
    unsigned long theTopology = inKey & 3, theCull = (inKey >> 2) & 3;
    bool theBlend = (inKey >> 4) & 1;
    unsigned long theSource = (inKey >> 5) & 15, theDestination = (inKey >> 9) & 15;
    bool theDepth = (inKey >> 13) & 1;
    unsigned long theZFunction = (inKey >> 14) & 15;
    bool theZWrite = (inKey >> 18) & 1, theFlat = (inKey >> 19) & 1, theTargetAlpha = (inKey >> 20) & 1;

    WGPUVertexAttribute theAttributes[4] = {};
    theAttributes[0].format = WGPUVertexFormat_Float32x3;
    theAttributes[0].offset = 0;
    theAttributes[0].shaderLocation = 0;
    theAttributes[1].format = WGPUVertexFormat_Float32x3;
    theAttributes[1].offset = 12;
    theAttributes[1].shaderLocation = 1;
    theAttributes[2].format = WGPUVertexFormat_Unorm8x4;   // the ARGB dword: B, G, R, A in memory (swizzled)
    theAttributes[2].offset = 24;
    theAttributes[2].shaderLocation = 2;
    theAttributes[3].format = WGPUVertexFormat_Float32x2;
    theAttributes[3].offset = 28;
    theAttributes[3].shaderLocation = 3;
    WGPUVertexBufferLayout theBuffer = {};
    theBuffer.stepMode = WGPUVertexStepMode_Vertex;
    theBuffer.arrayStride = 36;
    theBuffer.attributeCount = 4;
    theBuffer.attributes = theAttributes;

    WGPUBlendState theBlendState = {};
    if (theBlend) {
        WGPUBlendFactor theSrc = BlendFactor(theSource, theTargetAlpha);
        WGPUBlendFactor theDst = BlendFactor(theDestination, theTargetAlpha);
        if (theSource == GXBLEND_BOTHSRCALPHA) {
            theSrc = WGPUBlendFactor_SrcAlpha;
            theDst = WGPUBlendFactor_OneMinusSrcAlpha;
        } else if (theSource == GXBLEND_BOTHINVSRCALPHA) {
            theSrc = WGPUBlendFactor_OneMinusSrcAlpha;
            theDst = WGPUBlendFactor_SrcAlpha;
        }
        theBlendState.color.operation = theBlendState.alpha.operation = WGPUBlendOperation_Add;
        theBlendState.color.srcFactor = theBlendState.alpha.srcFactor = theSrc;
        theBlendState.color.dstFactor = theBlendState.alpha.dstFactor = theDst;
        if (theSrc == WGPUBlendFactor_SrcAlphaSaturated)
            theBlendState.alpha.srcFactor = WGPUBlendFactor_One;   // not allowed on the alpha channel
    }
    WGPUColorTargetState theTarget = {};
    theTarget.format = WGPUTextureFormat_BGRA8Unorm;
    theTarget.blend = theBlend ? &theBlendState : NULL;
    theTarget.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState theFragment = {};
    theFragment.module = theCore.mShader[theFlat ? 1 : 0];
    theFragment.entryPoint = Name("fragment_main");
    theFragment.targetCount = 1;
    theFragment.targets = &theTarget;
    WGPUDepthStencilState theDepthState = {};
    theDepthState.format = WGPUTextureFormat_Depth32Float;
    theDepthState.depthWriteEnabled = theZWrite ? WGPUOptionalBool_True : WGPUOptionalBool_False;
    theDepthState.depthCompare = theZFunction ? Compare(theZFunction) : WGPUCompareFunction_Always;
    theDepthState.stencilFront.compare = theDepthState.stencilBack.compare = WGPUCompareFunction_Always;
    theDepthState.stencilFront.failOp = theDepthState.stencilBack.failOp = WGPUStencilOperation_Keep;
    theDepthState.stencilFront.depthFailOp = theDepthState.stencilBack.depthFailOp = WGPUStencilOperation_Keep;
    theDepthState.stencilFront.passOp = theDepthState.stencilBack.passOp = WGPUStencilOperation_Keep;
    WGPURenderPipelineDescriptor theDescriptor = {};
    theDescriptor.layout = theCore.mPipelineLayout;
    theDescriptor.vertex.module = theCore.mShader[theFlat ? 1 : 0];
    theDescriptor.vertex.entryPoint = Name("vertex_main");
    theDescriptor.vertex.bufferCount = 1;
    theDescriptor.vertex.buffers = &theBuffer;
    theDescriptor.primitive.topology = theTopology == 0 ? WGPUPrimitiveTopology_PointList :
                                       theTopology == 1 ? WGPUPrimitiveTopology_LineList :
                                       WGPUPrimitiveTopology_TriangleList;
    // Direct3D culls by screen winding: CCW culls counter-clockwise triangles, CW clockwise ones
    theDescriptor.primitive.frontFace = WGPUFrontFace_CCW;
    theDescriptor.primitive.cullMode = theCull == 2 ? WGPUCullMode_Front : theCull == 1 ? WGPUCullMode_Back :
                                       WGPUCullMode_None;
    theDescriptor.depthStencil = theDepth ? &theDepthState : NULL;
    theDescriptor.multisample.count = 1;
    theDescriptor.multisample.mask = 0xffffffff;
    theDescriptor.fragment = &theFragment;
    WGPURenderPipeline thePipeline = wgpuDeviceCreateRenderPipeline(theCore.mDevice, &theDescriptor);
    theCore.mPipelines[inKey] = thePipeline;
    return thePipeline;
}

static void BuildUniform(GXDeviceData* d, GXSurfaceData* inTexture, GXFixedFunctionUniform& u)
{
    memset(&u, 0, sizeof(u));
    const unsigned long* rs = d->mRenderState;
    float theWV[16];
    Multiply(d->mTransform[GXTS_WORLD], d->mTransform[GXTS_VIEW], theWV);
    GXMATRIX theWVMatrix;
    memcpy(&theWVMatrix, theWV, sizeof(theWV));
    Multiply(theWVMatrix, d->mTransform[GXTS_PROJECTION], u.mWVP);
    memcpy(u.mWV, theWV, sizeof(theWV));
    Copy(d->mMaterial.dcvDiffuse, u.mMaterialDiffuse);
    Copy(d->mMaterial.dcvAmbient, u.mMaterialAmbient);
    Copy(d->mMaterial.dcvSpecular, u.mMaterialSpecular);
    Copy(d->mMaterial.dcvEmissive, u.mMaterialEmissive);
    Color(rs[GXRS_AMBIENT], u.mAmbient);
    Color(rs[GXRS_FOGCOLOR], u.mFogColor);
    u.mFog[0] = AsFloat(rs[GXRS_FOGSTART]);
    u.mFog[1] = AsFloat(rs[GXRS_FOGEND]);
    u.mFog[2] = rs[GXRS_FOGENABLE] && rs[GXRS_FOGVERTEXMODE] == GXFOG_LINEAR ? 1.0f : 0.0f;
    u.mFog[3] = d->mMaterial.dvPower;
    u.mPixel[0] = d->mViewport.dwWidth ? 1.0f / d->mViewport.dwWidth : 0.0f;
    u.mPixel[1] = d->mViewport.dwHeight ? 1.0f / d->mViewport.dwHeight : 0.0f;
    u.mPixel[2] = (float)(rs[GXRS_ALPHAREF] & 0xff);
    u.mPixel[3] = std::max(-16.0f, std::min(15.99f, AsFloat(d->mStageState[0][GXTSS_MIPMAPLODBIAS])));
    bool theColorVertex = rs[GXRS_COLORVERTEX] != 0;
    u.mMode[0] = rs[GXRS_LIGHTING] ? 1 : 0;
    u.mMode[1] = theColorVertex && rs[GXRS_DIFFUSEMATERIALSOURCE] == GXMCS_COLOR1;
    u.mMode[2] = theColorVertex && rs[GXRS_AMBIENTMATERIALSOURCE] == GXMCS_COLOR1;
    u.mMode[3] = rs[GXRS_LIGHTING] && rs[GXRS_SPECULARENABLE];
    const unsigned long* s = d->mStageState[0];
    u.mStage[0] = s[GXTSS_COLOROP];
    u.mStage[1] = s[GXTSS_COLORARG1];
    u.mStage[2] = s[GXTSS_COLORARG2];
    u.mStage[3] = s[GXTSS_ALPHAOP];
    u.mStage2[0] = s[GXTSS_ALPHAARG1];
    u.mStage2[1] = s[GXTSS_ALPHAARG2];
    u.mStage2[2] = rs[GXRS_ALPHATESTENABLE] ? (rs[GXRS_ALPHAFUNC] ? rs[GXRS_ALPHAFUNC] : GXCMP_ALWAYS) : 0;
    u.mStage2[3] = rs[GXRS_COLORKEYENABLE] && inTexture && inTexture->Top()->mHasKey ? 1 : 0;
    const GXMATRIX& V = d->mTransform[GXTS_VIEW];
    for (int i = 0; i < 8; i++) {
        const GXLIGHT& L = d->mLights[i];
        GXLightUniform& o = u.mLights[i];
        Copy(L.dcvDiffuse, o.mDiffuse);
        Copy(L.dcvSpecular, o.mSpecular);
        Copy(L.dcvAmbient, o.mAmbient);
        // to view space: positions as points, directions as vectors (row vectors times View)
        const GXVECTOR& p = L.dvPosition;
        o.mPosition[0] = p.x * V._11 + p.y * V._21 + p.z * V._31 + V._41;
        o.mPosition[1] = p.x * V._12 + p.y * V._22 + p.z * V._32 + V._42;
        o.mPosition[2] = p.x * V._13 + p.y * V._23 + p.z * V._33 + V._43;
        const GXVECTOR& n = L.dvDirection;
        float theX = n.x * V._11 + n.y * V._21 + n.z * V._31;
        float theY = n.x * V._12 + n.y * V._22 + n.z * V._32;
        float theZ = n.x * V._13 + n.y * V._23 + n.z * V._33;
        float theLength = sqrtf(theX * theX + theY * theY + theZ * theZ);
        if (theLength > 0.0f) {
            theX /= theLength;
            theY /= theLength;
            theZ /= theLength;
        }
        o.mDirection[0] = theX;
        o.mDirection[1] = theY;
        o.mDirection[2] = theZ;
        o.mAttenuation[0] = L.dvAttenuation0;
        o.mAttenuation[1] = L.dvAttenuation1;
        o.mAttenuation[2] = L.dvAttenuation2;
        o.mAttenuation[3] = L.dvRange > 0.0f ? L.dvRange : 3.4e38f;
        o.mKind[0] = L.dltType;
        o.mKind[1] = d->mLightOn[i] ? 1 : 0;
    }
}

static void DrawStats(GXDeviceData* d, GXPRIMITIVETYPE inType, GXVertexBuffer* inBuffer, unsigned long inStart,
                      unsigned long inCount, const unsigned short* inIndices, unsigned long inIndexCount, bool inTextured)
{
    if (!StatsOn())
        return;
    GXDrawStats& s = sStats;
    unsigned long theTopology = inType == GXPT_POINTLIST ? 0 : inType == GXPT_LINELIST ? 1 : 2;
    unsigned long theElements = inIndices ? inIndexCount : inCount;
    s.mDraws[theTopology]++;
    s.mPrimitives[theTopology] += theTopology == 0 ? theElements : theTopology == 1 ? theElements / 2 : theElements / 3;
    s.mLit += d->mRenderState[GXRS_LIGHTING] ? 1 : 0;
    s.mTextured += inTextured ? 1 : 0;
    s.mBlended += d->mRenderState[GXRS_ALPHABLENDENABLE] ? 1 : 0;
    s.mZ += d->mRenderState[GXRS_ZENABLE] ? 1 : 0;
    s.mAmbient = d->mRenderState[GXRS_AMBIENT];
    if (d->mRenderState[GXRS_FOGENABLE]) {
        s.mFogged++;
        s.mFogStart = AsFloat(d->mRenderState[GXRS_FOGSTART]);
        s.mFogEnd = AsFloat(d->mRenderState[GXRS_FOGEND]);
        s.mFogColor = d->mRenderState[GXRS_FOGCOLOR];
    }
    float theWV[16], theWVP[16];
    Multiply(d->mTransform[GXTS_WORLD], d->mTransform[GXTS_VIEW], theWV);
    GXMATRIX theWVMatrix;
    memcpy(&theWVMatrix, theWV, sizeof(theWV));
    Multiply(theWVMatrix, d->mTransform[GXTS_PROJECTION], theWVP);
    for (unsigned long i = 0; i < inCount; i++) {
        const unsigned char* v = inBuffer->mVertices + (size_t)(inStart + i) * 36;
        float p[3];
        memcpy(p, v, sizeof(p));
        float c[4];
        for (int k = 0; k < 4; k++)
            c[k] = p[0] * theWVP[k] + p[1] * theWVP[4 + k] + p[2] * theWVP[8 + k] + theWVP[12 + k];
        s.mVertices++;
        if (d->mRenderState[GXRS_FOGENABLE]) {
            float z = p[0] * theWV[2] + p[1] * theWV[6] + p[2] * theWV[10] + theWV[14];
            s.mFogZone[z < s.mFogStart ? 0 : z <= s.mFogEnd ? 1 : 2]++;
        }
        if (c[3] > 0.0f) {
            s.mInFront++;
            if (fabsf(c[0]) <= c[3] && fabsf(c[1]) <= c[3] && c[2] >= 0.0f && c[2] <= c[3])
                s.mInside++;
        }
        const float theClip[3] = {c[3] != 0.0f ? c[0] / c[3] : 0.0f, c[3] != 0.0f ? c[1] / c[3] : 0.0f, c[3]};
        for (int k = 0; k < 3; k++) {
            s.mMinClip[k] = std::min(s.mMinClip[k], theClip[k]);
            s.mMaxClip[k] = std::max(s.mMaxClip[k], theClip[k]);
        }
        for (int k = 0; k < 4; k++) {
            s.mMin[k] = std::min(s.mMin[k], v[24 + k]);
            s.mMax[k] = std::max(s.mMax[k], v[24 + k]);
        }
    }
}

static GXRESULT Draw(GXDeviceData* d, GXPRIMITIVETYPE inType, GXVertexBuffer* inBuffer, unsigned long inStart,
                     unsigned long inCount, const unsigned short* inIndices, unsigned long inIndexCount)
{
    GXCore& theCore = GXGetCore();
    if (!sDrawScene)
        return GX_OK;
    if (!d->mTarget || !inBuffer || !inCount || inBuffer->mStride != 36 || inStart + inCount > inBuffer->mCount)
        return inCount ? GXERR_INVALIDPARAMS : GX_OK;
    if (inIndices && !inIndexCount)
        return GX_OK;
    GXSurfaceData* theTarget = d->mTarget->mData;
    GXSurfaceData* theTexture = d->mTexture[0] && d->mStageState[0][GXTSS_COLOROP] != GXTOP_DISABLE ?
                                d->mTexture[0]->mData : NULL;
    if (theTexture)
        theTexture->ForGPURead();
    theTarget->ForGPUWrite();
    if (!theTarget->CreateTexture())
        return GXERR_GENERIC;
    GXSurfaceData* theDepth = theTarget->mDepth;
    if (theDepth && !theDepth->CreateTexture())
        theDepth = NULL;

    // room in the rings first (a full ring submits, which closes the pass)
    unsigned long theVertexBytes = inCount * 36;
    unsigned long theIndexBytes = inIndices ? ((inIndexCount * 2 + 3) & ~3u) : 0;
    if (theCore.mVertexOffset + theVertexBytes + 64 > GXCore::kVertexRingSize ||
        theCore.mIndexOffset + theIndexBytes + 64 > GXCore::kIndexRingSize ||
        theCore.mUniformOffset + 4096 > GXCore::kUniformRingSize - 2048)
        theCore.Submit();
    if (d->mUniformDirty || d->mUniformSerial != theCore.mSerial) {
        GXFixedFunctionUniform theUniform;
        BuildUniform(d, theTexture, theUniform);
        d->mUniformOffset = theCore.Ring(theCore.mUniformRing, theCore.mUniformOffset, GXCore::kUniformRingSize - 2048,
                                         &theUniform, sizeof(theUniform), 256);
        d->mUniformSerial = theCore.mSerial;
        d->mUniformDirty = false;
    }
    unsigned long theVertexOffset = theCore.Ring(theCore.mVertexRing, theCore.mVertexOffset, GXCore::kVertexRingSize,
                                                 inBuffer->mVertices + (size_t)inStart * 36, theVertexBytes, 4);
    unsigned long theIndexOffset = 0;
    if (inIndices)
        theIndexOffset = theCore.Ring(theCore.mIndexRing, theCore.mIndexOffset, GXCore::kIndexRingSize, inIndices,
                                      inIndexCount * 2, 4);

    const unsigned long* rs = d->mRenderState;
    unsigned long theTopology = inType == GXPT_POINTLIST ? 0 : inType == GXPT_LINELIST ? 1 : 2;
    unsigned long theCull = theTopology == 2 ? (rs[GXRS_CULLMODE] == GXCULL_CW ? 1 : rs[GXRS_CULLMODE] == GXCULL_CCW ? 2 : 0)
                                             : 0;
    bool theBlend = rs[GXRS_ALPHABLENDENABLE] != 0;
    bool theZ = theDepth && rs[GXRS_ZENABLE];
    unsigned long theKey = theTopology | (theCull << 2) | ((theBlend ? 1u : 0u) << 4) |
                           (theBlend ? ((rs[GXRS_SRCBLEND] & 15) << 5) | ((rs[GXRS_DESTBLEND] & 15) << 9) : 0) |
                           ((theDepth ? 1u : 0u) << 13) | ((theZ ? (rs[GXRS_ZFUNC] & 15) : 0) << 14) |
                           ((theZ && rs[GXRS_ZWRITEENABLE] ? 1u : 0u) << 18) |
                           ((rs[GXRS_SHADEMODE] == GXSHADE_FLAT ? 1u : 0u) << 19) |
                           ((theTarget->mFormat.mMask[3] ? 1u : 0u) << 20);
    WGPURenderPipeline thePipeline = Pipeline(theKey);

    theCore.BeginPass(theTarget, theDepth);
    if (thePipeline != theCore.mPassPipeline) {
        wgpuRenderPassEncoderSetPipeline(theCore.mPass, thePipeline);
        theCore.mPassPipeline = thePipeline;
    }
    const GXVIEWPORT& v = d->mViewport;
    float theX = (float)std::min(v.dwX, theTarget->mWidth), theY = (float)std::min(v.dwY, theTarget->mHeight);
    float theW = (float)std::min(v.dwWidth, theTarget->mWidth - (unsigned long)theX);
    float theH = (float)std::min(v.dwHeight, theTarget->mHeight - (unsigned long)theY);
    // a scaled target (GXRenderScale): the logical viewport on its larger texture
    float theScale = theTarget->mScale;
    theX *= theScale;
    theY *= theScale;
    theW = std::min(theW * theScale, (float)theTarget->TexWidth() - theX);
    theH = std::min(theH * theScale, (float)theTarget->TexHeight() - theY);
    if (theW <= 0.0f || theH <= 0.0f)
        return GX_OK;
    float theMinZ = std::max(0.0f, std::min(1.0f, v.dvMinZ)), theMaxZ = std::max(theMinZ, std::min(1.0f, v.dvMaxZ));
    wgpuRenderPassEncoderSetViewport(theCore.mPass, theX, theY, theW, theH, theMinZ, theMaxZ);
    wgpuRenderPassEncoderSetScissorRect(theCore.mPass, 0, 0, theTarget->TexWidth(), theTarget->TexHeight());
    const unsigned long* s = d->mStageState[0];
    WGPUBindGroup theGroup = theCore.Group(theTexture ? theTexture->Top() : NULL,
                                           GXSamplerKey(s[GXTSS_ADDRESS], s[GXTSS_MINFILTER], s[GXTSS_MAGFILTER],
                                                        theTexture && theTexture->Top()->mMipLevels > 1 ?
                                                        s[GXTSS_MIPFILTER] : GXTFP_NONE));
    DrawStats(d, inType, inBuffer, inStart, inCount, inIndices, inIndexCount, theTexture != NULL);
    uint32_t theOffset = d->mUniformOffset;
    wgpuRenderPassEncoderSetBindGroup(theCore.mPass, 0, theGroup, 1, &theOffset);
    wgpuRenderPassEncoderSetVertexBuffer(theCore.mPass, 0, theCore.mVertexRing, theVertexOffset, theVertexBytes);
    if (inIndices) {
        wgpuRenderPassEncoderSetIndexBuffer(theCore.mPass, theCore.mIndexRing, WGPUIndexFormat_Uint16, theIndexOffset,
                                            theIndexBytes);
        wgpuRenderPassEncoderDrawIndexed(theCore.mPass, inIndexCount, 1, 0, 0, 0);
    } else
        wgpuRenderPassEncoderDraw(theCore.mPass, inCount, 1, 0, 0);
    if (theTexture)
        theTexture->Top()->mUsedSerial = theCore.mSerial;
    theTarget->mUsedSerial = theCore.mSerial;
    return GX_OK;
}

GXRESULT GXDevice::DrawPrimitiveVB(GXPRIMITIVETYPE inType, GXVertexBuffer* inBuffer, unsigned long inStart,
                                   unsigned long inCount, unsigned long inFlags)
{
    GX_GUARD;
    return Draw(this->mData, inType, inBuffer, inStart, inCount, NULL, 0);
}

GXRESULT GXDevice::DrawIndexedPrimitiveVB(GXPRIMITIVETYPE inType, GXVertexBuffer* inBuffer, unsigned long inStart,
                                          unsigned long inCount, unsigned short* inIndices, unsigned long inIndexCount,
                                          unsigned long inFlags)
{
    GX_GUARD;
    return Draw(this->mData, inType, inBuffer, inStart, inCount, inIndices, inIndexCount);
}
