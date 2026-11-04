/* rdp_modes.c -- TEST-ONLY DirectDraw proxy for the windowed software-display profile under RDP (owner, 2026-11-01).
 *
 * NOT ORIGINAL SOURCE, never part of a reconstructed module or of matching credit.
 *
 * Why: under an RDP session the Remote Display Adapter enumerates only the client's desktop mode (e.g. 1728x1084x32).
 * The game calls EESetVideoMode(640, 480, ..) at startup and then the configured 800x600, finds neither, and asserts
 * `theMode < theModeCount` (Empire Earth.cpp:2477). In the windowed test profile the mode list only gates that lookup:
 * the harness STOPs on any real display-mode change (SetDisplayMode, ChangeDisplaySettings*), and windowed runs make
 * none. So this proxy adds the two missing sizes to the enumeration.
 *
 * How: the staged DX7HRDisplay.dll's import "DDRAW.dll" is renamed to "DDRDP.dll" (same length; overlay manifest).
 * This DLL forwards its three imports to the system ddraw.dll, and after DirectDrawCreateEx(IID_IDirectDraw7) wraps the
 * shared IDirectDraw7 vtable's EnumDisplayModes slot. The wrapper runs the REAL enumeration first and passes every real
 * mode through unchanged; then, only for an unfiltered enumeration (no surface description) that was not cancelled, and
 * only for 640x480 / 800x600 / 1024x768 at 32 bits that the real list lacks, it reports a copy of a real enumerated 32-bit mode
 * (genuine pixel format) with the width, height and pitch changed. Each synthesized mode is announced with
 * OutputDebugString ("EE-TEST-RDP-MODE ..."), which the harness logs. On a display that has both sizes (the headless
 * VirtIO console) nothing is added, and the profile does not stage this proxy at all.
 */
#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>
#include <ddraw.h>

typedef HRESULT (WINAPI *create_ex_t)(GUID FAR *, LPVOID *, REFIID, IUnknown FAR *);
typedef HRESULT (WINAPI *enum_a_t)(LPDDENUMCALLBACKA, LPVOID);
typedef HRESULT (WINAPI *enum_ex_a_t)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *enum_modes_t)(IDirectDraw7 *, DWORD, LPDDSURFACEDESC2, LPVOID, LPDDENUMMODESCALLBACK2);

static HMODULE sReal;
static enum_modes_t sRealEnumModes;

static FARPROC Real(const char *inName)
{
    if (!sReal) {
        char thePath[MAX_PATH];
        UINT theLength = GetSystemDirectoryA(thePath, MAX_PATH - 16);
        lstrcpyA(thePath + theLength, "\\ddraw.dll");
        sReal = LoadLibraryA(thePath);
    }
    return sReal ? GetProcAddress(sReal, inName) : NULL;
}

/* The sizes the game sets during startup and the menus (640x480 splash, 1024x768 menus, 800x600 game window;
 * a non-RDP session logs exactly these three EESetVideoMode calls). */
static const DWORD kSizes[][2] = { { 640, 480 }, { 800, 600 }, { 1024, 768 } };
#define kSizeCount (sizeof(kSizes) / sizeof(kSizes[0]))

typedef struct {
    LPDDENUMMODESCALLBACK2 mCallback;
    LPVOID mContext;
    BOOL mHave[kSizeCount], mHaveTemplate, mCancelled;
    DDSURFACEDESC2 mTemplate;
} EnumState;

static HRESULT WINAPI Record(LPDDSURFACEDESC2 inDesc, LPVOID inContext)
{
    EnumState *theState = (EnumState *)inContext;
    HRESULT theResult;
    if ((inDesc->dwFlags & DDSD_PIXELFORMAT) && inDesc->ddpfPixelFormat.dwRGBBitCount == 32) {
        unsigned i;
        for (i = 0; i < kSizeCount; i++)
            if (inDesc->dwWidth == kSizes[i][0] && inDesc->dwHeight == kSizes[i][1]) theState->mHave[i] = TRUE;
        if (!theState->mHaveTemplate) {
            CopyMemory(&theState->mTemplate, inDesc, sizeof(DDSURFACEDESC2));
            theState->mHaveTemplate = TRUE;
        }
    }
    theResult = theState->mCallback(inDesc, theState->mContext);
    if (theResult == DDENUMRET_CANCEL) theState->mCancelled = TRUE;
    return theResult;
}

static HRESULT WINAPI Discard(LPDDSURFACEDESC2 inDesc, LPVOID inContext)
{
    return DDENUMRET_OK;
}

static void Synthesize(EnumState *inState, DWORD inWidth, DWORD inHeight)
{
    DDSURFACEDESC2 theDesc;
    char theText[96];
    if (inState->mCancelled) return;
    CopyMemory(&theDesc, &inState->mTemplate, sizeof(DDSURFACEDESC2));
    theDesc.dwWidth = inWidth;
    theDesc.dwHeight = inHeight;
    if (theDesc.dwFlags & DDSD_PITCH) theDesc.lPitch = (LONG)(inWidth * 4);
    wsprintfA(theText, "EE-TEST-RDP-MODE synthesized %lux%lux32 (not enumerated by the display)", inWidth, inHeight);
    OutputDebugStringA(theText);
    if (inState->mCallback(&theDesc, inState->mContext) == DDENUMRET_CANCEL) inState->mCancelled = TRUE;
}

/* A filtered enumeration (inDesc != NULL) reports only modes matching the filter's width, height, pixel depth and
 * refresh rate. A synthesized mode is offered only if it satisfies every field the filter sets; a filter on any other
 * field passes through untouched. */
static BOOL FilterAllows(LPDDSURFACEDESC2 inDesc, DWORD inWidth, DWORD inHeight)
{
    DWORD theKnown = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_REFRESHRATE | DDSD_PITCH | DDSD_CAPS;
    if (inDesc == NULL) return TRUE;
    if (inDesc->dwFlags & ~theKnown) return FALSE;
    if ((inDesc->dwFlags & DDSD_WIDTH) && inDesc->dwWidth != inWidth) return FALSE;
    if ((inDesc->dwFlags & DDSD_HEIGHT) && inDesc->dwHeight != inHeight) return FALSE;
    if ((inDesc->dwFlags & DDSD_PIXELFORMAT) && inDesc->ddpfPixelFormat.dwRGBBitCount != 32) return FALSE;
    return TRUE;
}

static HRESULT STDMETHODCALLTYPE EnumModes(IDirectDraw7 *inThis, DWORD inFlags, LPDDSURFACEDESC2 inDesc, LPVOID inContext,
                                           LPDDENUMMODESCALLBACK2 inCallback)
{
    EnumState theState;
    HRESULT theResult;
    char theText[128];
    wsprintfA(theText, "EE-TEST-RDP-ENUM flags %lx filter %lx %lux%lu", inFlags, inDesc ? inDesc->dwFlags : 0,
              inDesc ? inDesc->dwWidth : 0, inDesc ? inDesc->dwHeight : 0);
    OutputDebugStringA(theText);
    if (inCallback == NULL)
        return sRealEnumModes(inThis, inFlags, inDesc, inContext, inCallback);
    ZeroMemory(&theState, sizeof(theState));
    theState.mCallback = inCallback;
    theState.mContext = inContext;
    theResult = sRealEnumModes(inThis, inFlags, inDesc, &theState, Record);
    if (SUCCEEDED(theResult) && !theState.mHaveTemplate && inDesc != NULL) {
        /* the filter excluded every real mode: take the template from an unfiltered (silent) enumeration */
        EnumState theProbe;
        ZeroMemory(&theProbe, sizeof(theProbe));
        theProbe.mCallback = Discard;
        sRealEnumModes(inThis, inFlags, NULL, &theProbe, Record);
        if (theProbe.mHaveTemplate) {
            CopyMemory(&theState.mTemplate, &theProbe.mTemplate, sizeof(DDSURFACEDESC2));
            theState.mHaveTemplate = TRUE;
        }
    }
    if (SUCCEEDED(theResult) && theState.mHaveTemplate) {
        unsigned i;
        for (i = 0; i < kSizeCount; i++)
            if (!theState.mHave[i] && FilterAllows(inDesc, kSizes[i][0], kSizes[i][1]))
                Synthesize(&theState, kSizes[i][0], kSizes[i][1]);
    }
    return theResult;
}

HRESULT WINAPI DirectDrawCreateEx(GUID FAR *inGuid, LPVOID *outObject, REFIID inIID, IUnknown FAR *inOuter)
{
    create_ex_t theReal = (create_ex_t)Real("DirectDrawCreateEx");
    HRESULT theResult;
    if (!theReal) return DDERR_GENERIC;
    theResult = theReal(inGuid, outObject, inIID, inOuter);
    if (SUCCEEDED(theResult) && *outObject && IsEqualIID(inIID, &IID_IDirectDraw7)) {
        void **theTable = *(void ***)*outObject;
        enum_modes_t *theSlot = (enum_modes_t *)&theTable[8];   /* IDirectDraw7::EnumDisplayModes */
        if (*theSlot != EnumModes) {
            DWORD theOld;
            if (VirtualProtect(theSlot, sizeof(*theSlot), PAGE_READWRITE, &theOld)) {
                sRealEnumModes = *theSlot;
                *theSlot = EnumModes;
                VirtualProtect(theSlot, sizeof(*theSlot), theOld, &theOld);
            }
        }
    }
    return theResult;
}

HRESULT WINAPI DirectDrawEnumerateA(LPDDENUMCALLBACKA inCallback, LPVOID inContext)
{
    enum_a_t theReal = (enum_a_t)Real("DirectDrawEnumerateA");
    return theReal ? theReal(inCallback, inContext) : DDERR_GENERIC;
}

HRESULT WINAPI DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA inCallback, LPVOID inContext, DWORD inFlags)
{
    enum_ex_a_t theReal = (enum_ex_a_t)Real("DirectDrawEnumerateExA");
    return theReal ? theReal(inCallback, inContext, inFlags) : DDERR_GENERIC;
}

BOOL WINAPI DllMain(HINSTANCE inInstance, DWORD inReason, LPVOID inReserved)
{
    return TRUE;
}
