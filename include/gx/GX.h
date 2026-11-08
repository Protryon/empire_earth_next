// GX: the subset of DirectDraw 7 and Direct3D 7 the renderer uses, reimplemented on wgpu (webgpu.h). PORT.md
// "Plan: DirectX -> wgpu". The renderer (DX7 Display Plug-ins) keeps its structure; its DirectX objects became the
// GX classes below, which keep the COM-style method names and HRESULT-style results, with the constants renamed
// D3D*/DD* -> GX* (same values as the DirectX 7 SDK, for comparison with the original).
//
// Model: a GXSurface holds a CPU copy in its pixel format (Lock, DrawText) and a GPU texture (RGBA8; drawing, blits,
// presentation). Each side is marked valid or stale and synchronised lazily: uploads before GPU use, read-backs
// before CPU use. Blits between two CPU-side surfaces run on the CPU (DirectDraw semantics, colour keys compared in
// the surface format). A GXDevice renders into a target surface with the fixed-function state (transforms, one
// texture stage, lights, material, fog, alpha test/blend, z) mapped onto one WGSL shader with cached pipelines. All
// entry points take one process-wide lock (the game draws and loads from several threads).
//
// Pointer-size clean: builds for x86_64 (the target) and i686.
#ifndef GX_GX_H
#define GX_GX_H

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

typedef long GXRESULT;
struct PlatWindow;   // platform/PlatWindow.h: the window the primary surface presents in

// ---- results
#define GX_OK 0
#define GXERR_GENERIC ((GXRESULT)0x80004005L)
#define GXERR_INVALIDPARAMS ((GXRESULT)0x80070057L)
#define GXERR_OUTOFMEMORY ((GXRESULT)0x8007000EL)
#define GXERR_UNSUPPORTED ((GXRESULT)0x80004001L)
#define GXERR_SURFACEBUSY ((GXRESULT)(0x88760000L | 430))
#define GXERR_SURFACELOST ((GXRESULT)(0x88760000L | 450))
#define GXERR_WASSTILLDRAWING ((GXRESULT)(0x88760000L | 540))
#define GXERR_NOTLOCKED ((GXRESULT)(0x88760000L | 584))
#define GXERR_NOTFOUND ((GXRESULT)(0x88760000L | 255))
#define GXENUMRET_CANCEL 0
#define GXENUMRET_OK 1

// ---- DirectDraw: surface description, caps, blits
#define GXSD_CAPS 0x00000001L
#define GXSD_HEIGHT 0x00000002L
#define GXSD_WIDTH 0x00000004L
#define GXSD_BACKBUFFERCOUNT 0x00000020L
#define GXSD_PIXELFORMAT 0x00001000L
#define GXSD_CKSRCBLT 0x00010000L
#define GXSD_MIPMAPCOUNT 0x00020000L

#define GXSCAPS_COMPLEX 0x00000008L
#define GXSCAPS_FLIP 0x00000010L
#define GXSCAPS_OFFSCREENPLAIN 0x00000040L
#define GXSCAPS_PRIMARYSURFACE 0x00000200L
#define GXSCAPS_SYSTEMMEMORY 0x00000800L
#define GXSCAPS_TEXTURE 0x00001000L
#define GXSCAPS_3DDEVICE 0x00002000L
#define GXSCAPS_VIDEOMEMORY 0x00004000L
#define GXSCAPS_ZBUFFER 0x00020000L
#define GXSCAPS_MIPMAP 0x00400000L
#define GXSCAPS_LOCALVIDMEM 0x10000000L
#define GXSCAPS2_HINTDYNAMIC 0x00000004L
#define GXSCAPS2_HINTSTATIC 0x00000008L
#define GXSCAPS2_TEXTUREMANAGE 0x00000010L

#define GXPF_ALPHAPIXELS 0x00000001L
#define GXPF_FOURCC 0x00000004L
#define GXPF_RGB 0x00000040L
#define GXPF_ZBUFFER 0x00000400L

#define GXBLT_COLORFILL 0x00000400L
#define GXBLT_KEYSRC 0x00008000L
#define GXBLT_DONOTWAIT 0x08000000L
#define GXFLIP_NOVSYNC 0x00000008L
#define GXFLIP_DONOTWAIT 0x00000020L
#define GXCKEY_SRCBLT 0x00000008L
#define GXLOCK_WRITEONLY 0x00000020L
#define GXLOCK_NOOVERWRITE 0x00001000L
#define GXLOCK_DISCARDCONTENTS 0x00002000L

#define GXSCL_FULLSCREEN 0x00000001L
#define GXSCL_ALLOWREBOOT 0x00000002L
#define GXSCL_NORMAL 0x00000008L
#define GXSCL_EXCLUSIVE 0x00000010L
#define GXSCL_SETFOCUSWINDOW 0x00000080L
#define GXSCL_SETDEVICEWINDOW 0x00000100L
#define GXSCL_CREATEDEVICEWINDOW 0x00000200L
#define GXSCL_MULTITHREADED 0x00000400L

#define GXEDM_REFRESHRATES 0x00000001L
#define GXENUM_ATTACHEDSECONDARYDEVICES 0x00000001L
#define GXENUM_DETACHEDSECONDARYDEVICES 0x00000002L
#define GXENUM_NONDISPLAYDEVICES 0x00000004L
#define GXCAPS_3D 0x00000001L
#define GXFXCAPS_BLTARITHSTRETCHY 0x00000020L
#define GXFXCAPS_BLTSHRINKX 0x00000400L
#define GXFXCAPS_BLTSHRINKY 0x00001000L
#define GXFXCAPS_BLTSTRETCHX 0x00004000L
#define GXFXCAPS_BLTSTRETCHY 0x00010000L
#define GXBD_16 0x00000400L
#define GXBD_32 0x00000100L

// ---- Direct3D: device caps, vertex buffers
#define GXDEVCAPS_FLOATTLVERTEX 0x00000001L
#define GXDEVCAPS_HWTRANSFORMANDLIGHT 0x00010000L
#define GXDEVCAPS_HWRASTERIZATION 0x00080000L
#define GXVBCAPS_SYSTEMMEMORY 0x00000800L
#define GXVBCAPS_WRITEONLY 0x00010000L
#define GXFVF_XYZ 0x002
#define GXFVF_NORMAL 0x010
#define GXFVF_DIFFUSE 0x040
#define GXFVF_TEX1 0x100
#define GXCLEAR_TARGET 0x00000001L
#define GXCLEAR_ZBUFFER 0x00000002L

enum GXPRIMITIVETYPE { GXPT_POINTLIST = 1, GXPT_LINELIST = 2, GXPT_TRIANGLELIST = 4 };
enum GXTRANSFORMSTATETYPE { GXTS_WORLD = 1, GXTS_VIEW = 2, GXTS_PROJECTION = 3 };

enum GXRENDERSTATETYPE {
    GXRS_ANTIALIAS = 2, GXRS_ZENABLE = 7, GXRS_FILLMODE = 8, GXRS_SHADEMODE = 9, GXRS_ZWRITEENABLE = 14,
    GXRS_ALPHATESTENABLE = 15, GXRS_SRCBLEND = 19, GXRS_DESTBLEND = 20, GXRS_CULLMODE = 22, GXRS_ZFUNC = 23,
    GXRS_ALPHAREF = 24, GXRS_ALPHAFUNC = 25, GXRS_DITHERENABLE = 26, GXRS_ALPHABLENDENABLE = 27, GXRS_FOGENABLE = 28,
    GXRS_SPECULARENABLE = 29, GXRS_FOGCOLOR = 34, GXRS_FOGSTART = 36, GXRS_FOGEND = 37, GXRS_COLORKEYENABLE = 41,
    GXRS_CLIPPING = 136, GXRS_LIGHTING = 137, GXRS_EXTENTS = 138, GXRS_AMBIENT = 139, GXRS_FOGVERTEXMODE = 140,
    GXRS_COLORVERTEX = 141, GXRS_LOCALVIEWER = 142, GXRS_NORMALIZENORMALS = 143, GXRS_DIFFUSEMATERIALSOURCE = 145,
    GXRS_SPECULARMATERIALSOURCE = 146, GXRS_AMBIENTMATERIALSOURCE = 147, GXRS_EMISSIVEMATERIALSOURCE = 148,
    GXRS_COUNT = 160
};
enum GXTEXTURESTAGESTATETYPE {
    GXTSS_COLOROP = 1, GXTSS_COLORARG1 = 2, GXTSS_COLORARG2 = 3, GXTSS_ALPHAOP = 4, GXTSS_ALPHAARG1 = 5,
    GXTSS_ALPHAARG2 = 6, GXTSS_TEXCOORDINDEX = 11, GXTSS_ADDRESS = 12, GXTSS_MAGFILTER = 16, GXTSS_MINFILTER = 17,
    GXTSS_MIPFILTER = 18, GXTSS_MIPMAPLODBIAS = 19, GXTSS_COUNT = 20
};
enum { GXTOP_DISABLE = 1, GXTOP_SELECTARG1 = 2, GXTOP_SELECTARG2 = 3, GXTOP_MODULATE = 4 };
enum { GXTA_DIFFUSE = 0, GXTA_CURRENT = 1, GXTA_TEXTURE = 2 };
enum { GXBLEND_ZERO = 1, GXBLEND_ONE = 2, GXBLEND_SRCCOLOR = 3, GXBLEND_INVSRCCOLOR = 4, GXBLEND_SRCALPHA = 5,
       GXBLEND_INVSRCALPHA = 6, GXBLEND_DESTALPHA = 7, GXBLEND_INVDESTALPHA = 8, GXBLEND_DESTCOLOR = 9,
       GXBLEND_INVDESTCOLOR = 10, GXBLEND_SRCALPHASAT = 11, GXBLEND_BOTHSRCALPHA = 12, GXBLEND_BOTHINVSRCALPHA = 13 };
enum { GXCMP_NEVER = 1, GXCMP_LESS = 2, GXCMP_EQUAL = 3, GXCMP_LESSEQUAL = 4, GXCMP_GREATER = 5, GXCMP_NOTEQUAL = 6,
       GXCMP_GREATEREQUAL = 7, GXCMP_ALWAYS = 8 };
enum { GXCULL_NONE = 1, GXCULL_CW = 2, GXCULL_CCW = 3 };
enum { GXFILL_POINT = 1, GXFILL_WIREFRAME = 2, GXFILL_SOLID = 3 };
enum { GXSHADE_FLAT = 1, GXSHADE_GOURAUD = 2 };
enum { GXFOG_NONE = 0, GXFOG_LINEAR = 3 };
enum { GXMCS_MATERIAL = 0, GXMCS_COLOR1 = 1 };
enum { GXTADDRESS_WRAP = 1, GXTADDRESS_MIRROR = 2, GXTADDRESS_CLAMP = 3 };
enum { GXTFG_POINT = 1, GXTFG_LINEAR = 2, GXTFG_FLATCUBIC = 3 };
enum { GXTFN_POINT = 1, GXTFN_LINEAR = 2, GXTFN_ANISOTROPIC = 3 };
enum { GXTFP_NONE = 1, GXTFP_POINT = 2, GXTFP_LINEAR = 3 };
enum { GXLIGHT_POINT = 1, GXLIGHT_DIRECTIONAL = 3 };
enum { GXANTIALIAS_NONE = 0, GXANTIALIAS_SORTINDEPENDENT = 2 };

// device kinds (the IIDs CreateDevice took): every kind is the same wgpu device
enum GXIID { GXIID_Direct3D7 = 1, GXIID_HALDevice, GXIID_TnLHalDevice, GXIID_RGBDevice };

#define GXRGBA(r, g, b, a) ((unsigned long)(((unsigned long)(a) << 24) | ((unsigned long)(r) << 16) | \
                                            ((unsigned long)(g) << 8) | (unsigned long)(b)))
#define GXRGB(r, g, b) (0xff000000UL | ((unsigned long)((r) * 255) << 16) | ((unsigned long)((g) * 255) << 8) | \
                        (unsigned long)((b) * 255))
#define GXRGB_MAKE(r, g, b) ((unsigned long)(((unsigned long)(r) << 16) | ((unsigned long)(g) << 8) | (unsigned long)(b)))
#define GXRGBA_GETALPHA(c) ((unsigned long)(c) >> 24)
#define GXRGBA_GETRED(c) (((unsigned long)(c) >> 16) & 0xff)
#define GXRGBA_GETGREEN(c) (((unsigned long)(c) >> 8) & 0xff)
#define GXRGBA_GETBLUE(c) ((unsigned long)(c) & 0xff)
#define GXRGBA_SETALPHA(c, a) (((unsigned long)(a) << 24) | ((unsigned long)(c) & 0x00ffffff))
#define GXMAKEFOURCC(a, b, c, d) ((unsigned long)(unsigned char)(a) | ((unsigned long)(unsigned char)(b) << 8) | \
                                  ((unsigned long)(unsigned char)(c) << 16) | ((unsigned long)(unsigned char)(d) << 24))

// ---- structures (the DirectX 7 fields the renderer reads or writes)
struct GXPIXELFORMAT {
    unsigned long dwSize;
    unsigned long dwFlags;
    unsigned long dwFourCC;
    unsigned long dwRGBBitCount;
    unsigned long dwRBitMask;
    unsigned long dwGBitMask;
    unsigned long dwBBitMask;
    unsigned long dwRGBAlphaBitMask;
};
struct GXSCAPS {
    unsigned long dwCaps;
    unsigned long dwCaps2;
};
struct GXCOLORKEY {
    unsigned long dwColorSpaceLowValue;
    unsigned long dwColorSpaceHighValue;
};
struct GXSURFACEDESC {
    unsigned long dwSize;
    unsigned long dwFlags;
    unsigned long dwHeight;
    unsigned long dwWidth;
    long lPitch;
    unsigned long dwBackBufferCount;
    unsigned long dwMipMapCount;
    unsigned long dwRefreshRate;
    void* lpSurface;
    GXCOLORKEY ddckCKSrcBlt;
    GXPIXELFORMAT ddpfPixelFormat;
    GXSCAPS ddsCaps;
};
struct GXCAPS {
    unsigned long dwSize;
    unsigned long dwCaps;
    unsigned long dwFXCaps;
};
struct GXBLTFX {
    unsigned long dwSize;
    unsigned long dwFillColor;
};
struct GXDEVICEIDENTIFIER {
    char szDescription[512];
    unsigned long dwVendorId;
    unsigned long dwDeviceId;
};
struct GXDEVICEDESC {
    unsigned long dwDevCaps;
    unsigned long dwDeviceRenderBitDepth;
};
struct GXVERTEXBUFFERDESC {
    unsigned long dwSize;
    unsigned long dwCaps;
    unsigned long dwFVF;
    unsigned long dwNumVertices;
};
struct GXVIEWPORT {
    unsigned long dwX;
    unsigned long dwY;
    unsigned long dwWidth;
    unsigned long dwHeight;
    float dvMinZ;
    float dvMaxZ;
};
struct GXRECT {
    long x1, y1, x2, y2;
};
struct GXMATRIX {
    float _11, _12, _13, _14;
    float _21, _22, _23, _24;
    float _31, _32, _33, _34;
    float _41, _42, _43, _44;
};
struct GXCOLORVALUE {
    float r, g, b, a;
};
struct GXVECTOR {
    float x, y, z;
};
struct GXMATERIAL {   // the D3DMATERIAL7 unions: both spellings name the same member
    union { GXCOLORVALUE dcvDiffuse; GXCOLORVALUE diffuse; };
    union { GXCOLORVALUE dcvAmbient; GXCOLORVALUE ambient; };
    union { GXCOLORVALUE dcvSpecular; GXCOLORVALUE specular; };
    union { GXCOLORVALUE dcvEmissive; GXCOLORVALUE emissive; };
    union { float dvPower; float power; };
};
struct GXLIGHT {
    unsigned long dltType;
    GXCOLORVALUE dcvDiffuse;
    GXCOLORVALUE dcvSpecular;
    GXCOLORVALUE dcvAmbient;
    GXVECTOR dvPosition;
    GXVECTOR dvDirection;
    float dvRange;
    float dvFalloff;
    float dvAttenuation0;
    float dvAttenuation1;
    float dvAttenuation2;
    float dvTheta;
    float dvPhi;
};
// the untransformed, unlit vertex (D3DVERTEX: position, normal, one texture coordinate)
struct GXVERTEX {
    float x, y, z;
    float nx, ny, nz;
    float tu, tv;
};

class GXSurface;
class GXDirectDraw;
class GXDevice;
class GXVertexBuffer;
struct GXSurfaceData;
struct GXDeviceData;
typedef GXDirectDraw GXDirect3D;   // IDirect3D7 was a QueryInterface of the DirectDraw object; here it is the same

typedef GXRESULT (CALLBACK* GXENUMMODESCALLBACK)(GXSURFACEDESC* inMode, void* inContext);
typedef GXRESULT (CALLBACK* GXENUMDEVICESCALLBACK)(char* inDescription, char* inName, GXDEVICEDESC* inCaps,
                                                   void* inContext);
typedef GXRESULT (CALLBACK* GXENUMPIXELFORMATSCALLBACK)(GXPIXELFORMAT* inFormat, void* inContext);
// inMonitor: an opaque token of the display (non-NULL: the primary display; DirectDraw's HMONITOR)
typedef BOOL (WINAPI* GXDDENUMCALLBACKEX)(GUID* inGUID, char* inDescription, char* inName, void* inContext,
                                          void* inMonitor);

// The display device of one monitor (IDirectDraw7 + IDirect3D7)
class GXDirectDraw {
public:
    unsigned long AddRef();
    unsigned long Release();
    GXRESULT QueryInterface(GXIID inIID, void** outObject);
    GXRESULT GetCaps(GXCAPS* outCaps, GXCAPS* outHELCaps);
    GXRESULT EnumDisplayModes(unsigned long inFlags, GXSURFACEDESC* inFilter, void* inContext,
                              GXENUMMODESCALLBACK inCallback);
    GXRESULT GetDisplayMode(GXSURFACEDESC* outMode);
    GXRESULT SetDisplayMode(unsigned long inWidth, unsigned long inHeight, unsigned long inBitCount,
                            unsigned long inRefreshRate, unsigned long inFlags);
    GXRESULT SetCooperativeLevel(PlatWindow* inWindow, unsigned long inFlags);   // an HWND in DirectDraw
    GXRESULT CreateSurface(GXSURFACEDESC* inDescription, GXSurface** outSurface, void* inOuter);
    GXRESULT GetAvailableVidMem(GXSCAPS* inCaps, unsigned long* outTotal, unsigned long* outFree);
    GXRESULT GetDeviceIdentifier(GXDEVICEIDENTIFIER* outIdentifier, unsigned long inFlags);
    GXRESULT RestoreAllSurfaces();
    GXRESULT TestCooperativeLevel();
    GXRESULT FlipToGDISurface();
    // IDirect3D7
    GXRESULT EnumDevices(GXENUMDEVICESCALLBACK inCallback, void* inContext);
    GXRESULT CreateDevice(GXIID inKind, GXSurface* inTarget, GXDevice** outDevice);
    GXRESULT CreateVertexBuffer(GXVERTEXBUFFERDESC* inDescription, GXVertexBuffer** outBuffer, unsigned long inFlags);

    GXDirectDraw();
private:
    ~GXDirectDraw();
    friend GXRESULT GXDirectDrawCreate(GUID* inGUID, GXDirectDraw** outDirectDraw);
    unsigned long mReferences;
    PlatWindow* mWindow;
    unsigned long mCooperativeFlags;
    unsigned long mModeWidth, mModeHeight, mModeBitCount;
};

GXRESULT GXDirectDrawCreate(GUID* inGUID, GXDirectDraw** outDirectDraw);
// reports one display device: the primary monitor (GUID NULL)
GXRESULT GXDirectDrawEnumerateEx(GXDDENUMCALLBACKEX inCallback, void* inContext, unsigned long inFlags);

// A DirectDraw surface: primary (the window), back buffer, offscreen plain, texture (+ mip levels), Z buffer
class GXSurface {
public:
    unsigned long AddRef();
    unsigned long Release();
    GXRESULT AddAttachedSurface(GXSurface* inSurface);
    GXRESULT DeleteAttachedSurface(unsigned long inFlags, GXSurface* inSurface);
    GXRESULT GetAttachedSurface(GXSCAPS* inCaps, GXSurface** outSurface);
    GXRESULT Blt(RECT* inDestination, GXSurface* inSource, RECT* inSourceRect, unsigned long inFlags, GXBLTFX* inFX);
    GXRESULT Flip(GXSurface* inOverride, unsigned long inFlags);
    GXRESULT Lock(RECT* inRect, GXSURFACEDESC* outDescription, unsigned long inFlags, HANDLE inEvent);
    GXRESULT Unlock(RECT* inRect);
    GXRESULT SetColorKey(unsigned long inFlags, GXCOLORKEY* inKey);
    GXRESULT GetSurfaceDesc(GXSURFACEDESC* outDescription);
    GXRESULT Restore();
    GXRESULT IsLost();

    // GX extensions
    // true when the GPU texture holds the newest pixels (a render target that was drawn into)
    bool IsGPUResident() const;
    // the GPU copy's pixels per surface pixel (GXRenderScale: a back buffer's; 1 for the others)
    float GetRenderScale() const;
    // draws text (platform/PlatFont.h) inside inBounds: inDraw draws into a 32-bit BGRA target that covers inBounds
    // (surface pixel inOriginX + x, inOriginY + y is the target's x, y). On a GPU-resident surface the target starts
    // transparent and premultiplied (PLAT_FONT_TARGET_PREMULTIPLIED) and is blended onto the GPU copy without a
    // read-back; otherwise it holds the CPU copy's pixels and the changed ones are written back (DirectDraw's GetDC
    // and GDI text before). Not on the primary or a Z buffer.
    typedef bool (*TextCallback)(const struct PlatFontTarget& inTarget, long inOriginX, long inOriginY, void* inContext);
    GXRESULT DrawText(const RECT& inBounds, TextCallback inDraw, void* inContext);

    GXSurfaceData* mData;
private:
    GXSurface();
    ~GXSurface();
    friend class GXDirectDraw;
    friend struct GXSurfaceData;
};

class GXVertexBuffer {
public:
    unsigned long AddRef();
    unsigned long Release();
    GXRESULT Lock(unsigned long inFlags, void** outData, unsigned long* outSize);
    GXRESULT Unlock();

    unsigned char* mVertices;
    unsigned long mStride;
    unsigned long mCount;
private:
    GXVertexBuffer();
    ~GXVertexBuffer();
    friend class GXDirectDraw;
    unsigned long mReferences;
};

// port: the display settings (EEApplyDisplaySettings, the settings form's video tab; config.yaml)
// the render scale's share of the display's pixels, 50..100 percent (GXRenderScale)
void GXSetRenderPercent(unsigned long inPercent);
// 0 presents on v-sync, -1 as fast as possible, else at most that many frames a second (no v-sync)
void GXSetFrameLimit(long inFramesPerSecond);
// texture sampling: 0 as the game asks (bilinear), 1 trilinear mip maps, 2 trilinear and 8x anisotropic
void GXSetTextureFiltering(unsigned long inMode);
// a gamma applied when presenting (1 unchanged; above 1 brighter)
void GXSetBrightness(float inGamma);
// the game's resolution for a UI scale in percent (0: GXAutoUIScale) in full screen (the display's logical size
// divided by the scale) or windowed (a window of 80% of it); GX lists every such size as a display mode
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);
// the UI scale "Auto" stands for: the display's logical height over 1080 lines, at least 100 percent
unsigned long GXAutoUIScale();

// A fixed-function 3D device rendering into a target surface (IDirect3DDevice7)
class GXDevice {
public:
    unsigned long AddRef();
    unsigned long Release();
    GXRESULT GetCaps(GXDEVICEDESC* outCaps);
    GXRESULT EnumTextureFormats(GXENUMPIXELFORMATSCALLBACK inCallback, void* inContext);
    GXRESULT BeginScene();
    GXRESULT EndScene();
    GXRESULT SetRenderTarget(GXSurface* inTarget, unsigned long inFlags);
    GXRESULT SetViewport(GXVIEWPORT* inViewport);
    GXRESULT GetViewport(GXVIEWPORT* outViewport);
    GXRESULT Clear(unsigned long inCount, GXRECT* inRects, unsigned long inFlags, unsigned long inColor, float inZ,
                   unsigned long inStencil);
    GXRESULT SetTransform(GXTRANSFORMSTATETYPE inState, GXMATRIX* inMatrix);
    GXRESULT GetTransform(GXTRANSFORMSTATETYPE inState, GXMATRIX* outMatrix);
    GXRESULT SetRenderState(GXRENDERSTATETYPE inState, unsigned long inValue);
    GXRESULT GetRenderState(GXRENDERSTATETYPE inState, unsigned long* outValue);
    GXRESULT SetTextureStageState(unsigned long inStage, GXTEXTURESTAGESTATETYPE inState, unsigned long inValue);
    GXRESULT SetTexture(unsigned long inStage, GXSurface* inTexture);
    GXRESULT SetMaterial(GXMATERIAL* inMaterial);
    GXRESULT SetLight(unsigned long inIndex, GXLIGHT* inLight);
    GXRESULT GetLight(unsigned long inIndex, GXLIGHT* outLight);
    GXRESULT LightEnable(unsigned long inIndex, BOOL inEnable);
    GXRESULT ValidateDevice(unsigned long* outPasses);
    GXRESULT DrawPrimitiveVB(GXPRIMITIVETYPE inType, GXVertexBuffer* inBuffer, unsigned long inStart,
                             unsigned long inCount, unsigned long inFlags);
    GXRESULT DrawIndexedPrimitiveVB(GXPRIMITIVETYPE inType, GXVertexBuffer* inBuffer, unsigned long inStart,
                                    unsigned long inCount, unsigned short* inIndices, unsigned long inIndexCount,
                                    unsigned long inFlags);

    GXDeviceData* mData;
private:
    GXDevice();
    ~GXDevice();
    friend class GXDirectDraw;
};

#endif
