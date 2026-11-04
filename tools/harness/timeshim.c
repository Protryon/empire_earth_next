/* timeshim.c -- TEST-ONLY time-acceleration shim for differential game sessions (owner, round 39, 2026-11-01).
 *
 * NOT ORIGINAL SOURCE, never part of a reconstructed module or of matching credit.
 *
 * What: every clock and every timed wait of the staged Low-Level Engine.dll runs F times faster than real time.
 * All simulation timing in Empire Earth goes through the engine: TMGetTime/TMGetMicroTime/TMPoll (QPC, or
 * timeGetTime), the physics tick (TMTimer -> timeSetEvent, TIME_CALLBACK_EVENT_SET | TIME_PERIODIC at
 * 1000/PTrate ms), the server's communications-tick send thread (WaitForMultipleObjects timeout = send interval - 50
 * with TMGetTime drift correction), TSEvent::Wait (WaitForSingleObject) and the client/server main loops. The exe's own
 * Sleep(1)/GetTickCount uses are thread hand-off polls or WONAPI and are left alone, so the two exe builds (original,
 * full link) are treated identically: both load the same staged engine copy.
 *
 * How (staged artifacts only):
 *  - the staged engine copy's import "WINMM.dll" is renamed to "EETMS.dll" (same length; timeshim.py). This DLL is
 *    EETMS.dll: timeGetTime/timeSetEvent/timeKillEvent are implemented on the virtual clock, every other engine winmm
 *    import is forwarded to the system winmm.dll (.def forwarders).
 *  - at load (DllMain) it rewrites, in memory, the engine's IAT slots for KERNEL32 QueryPerformanceCounter, Sleep,
 *    WaitForSingleObject and WaitForMultipleObjects to the scaled versions below. No other module is touched.
 * Virtual clock: v(q) = q0 + (q - q0) * F on the QPC scale (QueryPerformanceFrequency is unchanged), and
 * timeGetTime = t0 + (q - q0) * F * 1000 / freq. Finite wait timeouts and Sleep durations are divided by F with
 * 100 ns precision (a high-resolution waitable timer added to the wait set). Periodic timers run on their own thread
 * with absolute deadlines on the real QPC scale (period / F; no burst catch-up, like winmm).
 * The factor is a patched constant (kConfig below, written by timeshim.py into the built DLL): no file or registry
 * access at run time. Each start is announced with OutputDebugString ("EE-TEST-TIMESHIM ...").
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

/* patched by timeshim.py: magic, factor num/den, RTSGraphics::Main [lo, hi), frame cap ms, null 3D, key ms */
volatile struct { char mMagic[16]; LONG mNum; LONG mDen; DWORD mRenderLo, mRenderHi, mFrameMs, mNull3D, mKeyMs; } kConfig =
    { "EE-TIMESHIM-CFG", 1, 1, 0, 0, 0, 0, 0 };

static LONGLONG sQ0, sFreq;
static DWORD sT0;
static LONG sNum = 1, sDen = 1;
static HANDLE (WINAPI *pCreateWaitableTimerExW)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
static DWORD sTimerTls = TLS_OUT_OF_INDEXES;

static BOOL (WINAPI *rQPC)(LARGE_INTEGER *);
static VOID (WINAPI *rSleep)(DWORD);
static DWORD (WINAPI *rWFSO)(HANDLE, DWORD);
static DWORD (WINAPI *rWFMO)(DWORD, const HANDLE *, BOOL, DWORD);

static LONGLONG RealNow(void) { LARGE_INTEGER q; rQPC(&q); return q.QuadPart; }

/* (a * num) / den without overflow for the ranges here (a < 2^40 typical; num <= 4096) */
static LONGLONG Scale(LONGLONG a, LONG num, LONG den) { return a / den * num + (a % den) * num / den; }

static LONGLONG VirtualCount(void) { return sQ0 + Scale(RealNow() - sQ0, sNum, sDen); }

static BOOL WINAPI ShimQPC(LARGE_INTEGER *out) { out->QuadPart = VirtualCount(); return TRUE; }

DWORD WINAPI ShimTimeGetTime(void)
{
    LONGLONG v = Scale(RealNow() - sQ0, sNum, sDen);           /* virtual counts since start */
    return sT0 + (DWORD)(v / sFreq * 1000 + (v % sFreq) * 1000 / sFreq);
}

/* virtual milliseconds -> real 100 ns units (rounded up, at least 1) */
static LONGLONG Real100ns(DWORD ms) { LONGLONG r = ((LONGLONG)ms * 10000 * sDen + sNum - 1) / sNum; return r > 0 ? r : 1; }

static HANDLE ThreadTimer(void)
{
    HANDLE h = (HANDLE)TlsGetValue(sTimerTls);
    if (!h) {
        h = pCreateWaitableTimerExW ? pCreateWaitableTimerExW(NULL, NULL, 2 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS) : NULL;
        if (!h) h = CreateWaitableTimerA(NULL, FALSE, NULL);
        TlsSetValue(sTimerTls, h);
    }
    return h;
}

static void ArmRelative(HANDLE timer, LONGLONG units100ns)
{
    LARGE_INTEGER due; due.QuadPart = -units100ns;
    SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
}

static DWORD WINAPI ShimWFMO(DWORD n, const HANDLE *h, BOOL all, DWORD ms)
{
    HANDLE set[MAXIMUM_WAIT_OBJECTS];
    HANDLE timer;
    DWORD i, r;
    if (ms == INFINITE || ms == 0 || all || n >= MAXIMUM_WAIT_OBJECTS || !(timer = ThreadTimer()))
        return rWFMO(n, h, all, ms == INFINITE || ms == 0 ? ms : (DWORD)((Real100ns(ms) + 9999) / 10000));
    for (i = 0; i < n; i++) set[i] = h[i];
    set[n] = timer;
    ArmRelative(timer, Real100ns(ms));
    r = rWFMO(n + 1, set, FALSE, INFINITE);
    if (r == WAIT_OBJECT_0 + n) return WAIT_TIMEOUT;
    CancelWaitableTimer(timer);
    return r;
}

static DWORD WINAPI ShimWFSO(HANDLE h, DWORD ms) { return ShimWFMO(1, &h, FALSE, ms); }

static VOID WINAPI ShimSleep(DWORD ms)
{
    HANDLE timer;
    if (ms == 0 || ms == INFINITE || !(timer = ThreadTimer())) { rSleep(ms); return; }
    ArmRelative(timer, Real100ns(ms));
    rWFSO(timer, INFINITE);
}

/* ---- periodic / one-shot timers (timeSetEvent) ---- */
typedef struct Timer {
    struct Timer *mNext;
    UINT mID;
    UINT mFlags;
    LPTIMECALLBACK mCallback;
    DWORD_PTR mUser;
    LONGLONG mPeriod;          /* real QPC counts */
    HANDLE mThread, mStop;
    DWORD mThreadId;
} Timer;

static CRITICAL_SECTION sLock;
static Timer *sTimers;
static UINT sNextID = 0x7e000001;

static DWORD WINAPI TimerMain(LPVOID p)
{
    Timer *t = (Timer *)p;
    HANDLE timer = ThreadTimer(), set[2];
    LONGLONG deadline = RealNow() + t->mPeriod;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);   /* winmm's timer thread runs time-critical */
    set[0] = t->mStop; set[1] = timer;
    for (;;) {
        LONGLONG now = RealNow();
        if (deadline > now) {
            ArmRelative(timer, (deadline - now) * 10000000 / sFreq + 1);
            if (rWFMO(2, set, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) break;
        } else if (rWFSO(t->mStop, 0) == WAIT_OBJECT_0)
            break;
        if (t->mFlags & TIME_CALLBACK_EVENT_SET) SetEvent((HANDLE)t->mCallback);
        else if (t->mFlags & TIME_CALLBACK_EVENT_PULSE) PulseEvent((HANDLE)t->mCallback);
        else if (t->mCallback) t->mCallback(t->mID, 0, t->mUser, 0, 0);
        if (!(t->mFlags & TIME_PERIODIC)) break;
        deadline += t->mPeriod;
        now = RealNow();
        if (deadline < now) deadline = now;        /* no burst catch-up */
    }
    return 0;
}

MMRESULT WINAPI ShimTimeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK cb, DWORD_PTR user, UINT flags)
{
    Timer *t = (Timer *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Timer));
    if (!t) return 0;
    t->mFlags = flags; t->mCallback = cb; t->mUser = user;
    t->mPeriod = Scale((LONGLONG)(delay ? delay : 1) * sFreq / 1000, sDen, sNum);
    if (t->mPeriod < 1) t->mPeriod = 1;
    t->mStop = CreateEventA(NULL, TRUE, FALSE, NULL);
    EnterCriticalSection(&sLock);
    t->mID = sNextID++;
    t->mNext = sTimers; sTimers = t;
    LeaveCriticalSection(&sLock);
    t->mThread = CreateThread(NULL, 0, TimerMain, t, 0, &t->mThreadId);
    return t->mID;
}

MMRESULT WINAPI ShimTimeKillEvent(UINT id)
{
    Timer **pp, *t = NULL;
    EnterCriticalSection(&sLock);
    for (pp = &sTimers; *pp; pp = &(*pp)->mNext)
        if ((*pp)->mID == id) { t = *pp; *pp = t->mNext; break; }
    LeaveCriticalSection(&sLock);
    if (!t) return TIMERR_NOCANDO;   /* MMSYSERR_INVALPARAM in winmm; TMTimer only tests == TIMERR_NOERROR */
    SetEvent(t->mStop);
    if (GetCurrentThreadId() != t->mThreadId) rWFSO(t->mThread, INFINITE);
    CloseHandle(t->mThread);
    CloseHandle(t->mStop);
    HeapFree(GetProcessHeap(), 0, t);
    return TIMERR_NOERROR;
}

/* ---- null 3D device (opt-in, mNull3D) ----
 * The staged renderer (DX7HRDisplay.dll, loaded by the engine through LoadLibraryA/ExA) is left unchanged on disk:
 * scene building, transforms, lighting, vertex-buffer fills, textures and the present all still run. Only the device's
 * rasterising calls are skipped: IDirect3DDevice7::Clear, DrawPrimitive*, DrawIndexedPrimitive* return D3D_OK without
 * drawing. Every mKeyMs REAL ms one scene (BeginScene..) is drawn normally so the presented framebuffer (UI fixture
 * evidence, render.bmp) stays current; mKeyMs 0 = never draw. Hooks: the renderer's IAT slot for DirectDrawCreateEx,
 * then the IDirect3D7::CreateDevice and IDirect3DDevice7 vtable slots (shared per class, patched once). */
static const GUID kIID_IDirectDraw7 = { 0x15e65ec0, 0x3b9c, 0x11d2, { 0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b } };
static const GUID kIID_IDirect3D7 = { 0xf5049e77, 0x4861, 0x11d2, { 0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8 } };
typedef HRESULT (WINAPI *ddcreate_t)(GUID *, void **, REFIID, void *);
typedef HRESULT (WINAPI *qi_t)(void *, REFIID, void **);
typedef ULONG (WINAPI *rel_t)(void *);
typedef HRESULT (WINAPI *createdev_t)(void *, REFCLSID, void *, void **);
typedef HRESULT (WINAPI *scene_t)(void *);
static ddcreate_t sRealCreateEx;
static volatile LONG sDrawScene = 1;
static LONGLONG sNextKey;
static volatile LONG sNullCalls, sKeyScenes, sScenes;

/* The real slot values, per vtable: device classes (RGB/HAL/T&L) have distinct vtables, and d3dim may be reloaded at
 * another address; a hook finds the original through the calling object's own vtable. */
#define kMaxTables 16
static struct { void **mTable; void *mReal[49]; } sTables[kMaxTables];
static void *RealSlot(void *object, int index)
{
    void **t = *(void ***)object;
    int i;
    for (i = 0; i < kMaxTables; i++) if (sTables[i].mTable == t) return sTables[i].mReal[index];
    return NULL;
}

static int SlotSwap(void **slot, void *to, void **save)
{
    DWORD old;
    if (*slot == to) return 0;
    if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old)) return 0;
    if (save) *save = *slot;
    *slot = to;
    VirtualProtect(slot, sizeof(void *), old, &old);
    return 1;
}

static int HookTable(void **t, int index, void *to)
{
    int i;
    EnterCriticalSection(&sLock);
    for (i = 0; i < kMaxTables && sTables[i].mTable && sTables[i].mTable != t; i++) ;
    if (i < kMaxTables) {
        sTables[i].mTable = t;
        if (t[index] != to) sTables[i].mReal[index] = t[index];
    }
    LeaveCriticalSection(&sLock);
    return i < kMaxTables ? SlotSwap(&t[index], to, NULL) : 0;
}

static HRESULT WINAPI NullBeginScene(void *dev)
{
    LONGLONG now = RealNow();
    sScenes++;
    if (kConfig.mKeyMs && now >= sNextKey) {
        sDrawScene = 1; sKeyScenes++;
        sNextKey = now + (LONGLONG)kConfig.mKeyMs * sFreq / 1000;
    } else
        sDrawScene = 0;
    return ((scene_t)RealSlot(dev, 5))(dev);
}

#define NULL_DRAW(index, args, params) \
    static HRESULT WINAPI NullDraw##index params \
    { if (sDrawScene) return ((HRESULT (WINAPI *) params)RealSlot(d, index)) args; sNullCalls++; return 0; }
NULL_DRAW(10, (d, a, b, c, e, f, g), (void *d, DWORD a, void *b, DWORD c, DWORD e, float f, DWORD g))      /* Clear */
NULL_DRAW(25, (d, a, b, c, e, f), (void *d, DWORD a, DWORD b, void *c, DWORD e, DWORD f))                   /* DrawPrimitive */
NULL_DRAW(26, (d, a, b, c, e, f, g, h), (void *d, DWORD a, DWORD b, void *c, DWORD e, void *f, DWORD g, DWORD h)) /* DrawIndexedPrimitive */
NULL_DRAW(29, (d, a, b, c, e, f), (void *d, DWORD a, DWORD b, void *c, DWORD e, DWORD f))                   /* DrawPrimitiveStrided */
NULL_DRAW(30, (d, a, b, c, e, f, g, h), (void *d, DWORD a, DWORD b, void *c, DWORD e, void *f, DWORD g, DWORD h)) /* DrawIndexedPrimitiveStrided */
NULL_DRAW(31, (d, a, b, c, e, f), (void *d, DWORD a, void *b, DWORD c, DWORD e, DWORD f))                   /* DrawPrimitiveVB */
NULL_DRAW(32, (d, a, b, c, e, f, g, h), (void *d, DWORD a, void *b, DWORD c, DWORD e, void *f, DWORD g, DWORD h)) /* DrawIndexedPrimitiveVB */

static HRESULT WINAPI NullCreateDevice(void *d3d, REFCLSID cls, void *surface, void **out)
{
    HRESULT r = ((createdev_t)RealSlot(d3d, 4))(d3d, cls, surface, out);
    if (SUCCEEDED(r) && *out) {
        void **t = *(void ***)*out;
        char m[128];
        int n = HookTable(t, 5, (void *)NullBeginScene);
        n += HookTable(t, 10, (void *)NullDraw10);
        n += HookTable(t, 25, (void *)NullDraw25);
        n += HookTable(t, 26, (void *)NullDraw26);
        n += HookTable(t, 29, (void *)NullDraw29);
        n += HookTable(t, 30, (void *)NullDraw30);
        n += HookTable(t, 31, (void *)NullDraw31);
        n += HookTable(t, 32, (void *)NullDraw32);
        wsprintfA(m, "EE-TEST-TIMESHIM null-3d device %p vtable %p newly hooked %d key-ms %lu\n", *out, t, n, kConfig.mKeyMs);
        OutputDebugStringA(m);
    }
    return r;
}

static HRESULT WINAPI NullCreateEx(GUID *guid, void **out, REFIID iid, void *outer)
{
    HRESULT r = sRealCreateEx(guid, out, iid, outer);
    if (SUCCEEDED(r) && *out && IsEqualGUID(iid, &kIID_IDirectDraw7)) {
        void *d3d = NULL;
        if (SUCCEEDED(((qi_t)(*(void ***)*out)[0])(*out, &kIID_IDirect3D7, &d3d)) && d3d) {
            HookTable(*(void ***)d3d, 4, (void *)NullCreateDevice);
            ((rel_t)(*(void ***)d3d)[2])(d3d);
        }
    }
    return r;
}

static int PatchSlot(HMODULE m, const char *name, void *to, void **save)
{
    BYTE *base = (BYTE *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d;
    if (!dir->VirtualAddress) return 0;
    for (d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress); d->Name; d++) {
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA *slots = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++)
            if (!IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)
                && !lstrcmpA((const char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name, name))
                return SlotSwap((void **)&slots->u1.Function, to, save);
    }
    return 0;
}

static void HookRenderer(HMODULE m)
{
    char path[MAX_PATH], msg[160], *b;
    if (!m || !kConfig.mNull3D || sRealCreateEx) return;
    GetModuleFileNameA(m, path, MAX_PATH);
    b = path + lstrlenA(path);
    while (b > path && b[-1] != '\\') b--;
    if (PatchSlot(m, "DirectDrawCreateEx", (void *)NullCreateEx, (void **)&sRealCreateEx)) {
        wsprintfA(msg, "EE-TEST-TIMESHIM null-3d hooked %s\n", b);
        OutputDebugStringA(msg);
    }
}
static HMODULE (WINAPI *rLoadLibraryA)(LPCSTR);
static HMODULE (WINAPI *rLoadLibraryExA)(LPCSTR, HANDLE, DWORD);
static HMODULE WINAPI ShimLoadLibraryA(LPCSTR n) { HMODULE m = rLoadLibraryA(n); HookRenderer(m); return m; }
static HMODULE WINAPI ShimLoadLibraryExA(LPCSTR n, HANDLE f, DWORD fl) { HMODULE m = rLoadLibraryExA(n, f, fl); HookRenderer(m); return m; }

/* ---- render frame cap (exe IAT Sleep, only for calls returning into RTSGraphics::Main) ----
 * RTSGraphics::Main renders a frame and calls Sleep(1) in every loop iteration; the software renderer then holds the
 * world lock for most of the wall time. With mFrameMs set, a Sleep(1) whose return address lies in
 * [mRenderLo, mRenderHi) (RTSGraphics::Main of the running image; timeshim.py locates it per build) waits until
 * mFrameMs REAL ms after the previous frame. All other Sleep calls pass through unchanged. */
static LONGLONG sNextFrame;
static volatile LONG sCapped;
static __attribute__((noinline)) VOID WINAPI ShimExeSleep(DWORD ms)
{
    DWORD from = (DWORD)__builtin_return_address(0);
    if (kConfig.mFrameMs && from >= kConfig.mRenderLo && from < kConfig.mRenderHi) {
        LONGLONG now = RealNow(), frame = (LONGLONG)kConfig.mFrameMs * sFreq / 1000;
        HANDLE timer;
        if (sNextFrame > now && (timer = ThreadTimer())) {
            ArmRelative(timer, (sNextFrame - now) * 10000000 / sFreq + 1);
            rWFSO(timer, INFINITE);
            now = RealNow();
        } else
            rSleep(ms);
        sNextFrame = now + frame;
        sCapped++;
        return;
    }
    ShimSleep(ms);
}

/* ---- IAT patch of the engine's KERNEL32 imports ---- */
static int PatchImports(HMODULE m, int exe)
{
    BYTE *base = (BYTE *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d;
    int count = 0;
    if (!dir->VirtualAddress) return 0;
    for (d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress); d->Name; d++) {
        IMAGE_THUNK_DATA *names, *slots;
        if (lstrcmpiA((char *)(base + d->Name), "KERNEL32.dll")) continue;
        names = (IMAGE_THUNK_DATA *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        slots = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            const char *n;
            void *to = NULL;
            DWORD old;
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            n = (const char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name;
            if (!lstrcmpA(n, "QueryPerformanceCounter")) to = (void *)ShimQPC;
            else if (!lstrcmpA(n, "Sleep")) to = kConfig.mFrameMs ? (void *)ShimExeSleep : (void *)ShimSleep;
            else if (!lstrcmpA(n, "WaitForSingleObject")) to = (void *)ShimWFSO;
            else if (!lstrcmpA(n, "WaitForMultipleObjects")) to = (void *)ShimWFMO;
            else if (kConfig.mNull3D && !lstrcmpA(n, "LoadLibraryA")) to = (void *)ShimLoadLibraryA;
            else if (kConfig.mNull3D && !lstrcmpA(n, "LoadLibraryExA")) to = (void *)ShimLoadLibraryExA;
            if (!to) continue;
            VirtualProtect(&slots->u1.Function, sizeof(void *), PAGE_READWRITE, &old);
            slots->u1.Function = (DWORD_PTR)to;
            VirtualProtect(&slots->u1.Function, sizeof(void *), old, &old);
            count++;
        }
    }
    return count;
}

/* every 10 real s: scenes begun, scenes drawn (keyframes), skipped device calls, frame-capped sleeps */
static DWORD WINAPI Stats(LPVOID unused)
{
    char m[160];
    for (;;) {
        rSleep(10000);
        wsprintfA(m, "EE-TEST-TIMESHIM stats scenes %ld drawn %ld nulled-calls %ld capped-sleeps %ld\n", sScenes, sKeyScenes,
                  sNullCalls, sCapped);
        OutputDebugStringA(m);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        HMODULE k = GetModuleHandleA("kernel32.dll"), engine;
        LARGE_INTEGER f;
        char msg[160];
        int patched, exeslots = 0;
        DisableThreadLibraryCalls(inst);
        rQPC = (void *)GetProcAddress(k, "QueryPerformanceCounter");
        rSleep = (void *)GetProcAddress(k, "Sleep");
        rWFSO = (void *)GetProcAddress(k, "WaitForSingleObject");
        rWFMO = (void *)GetProcAddress(k, "WaitForMultipleObjects");
        rLoadLibraryA = (void *)GetProcAddress(k, "LoadLibraryA");
        rLoadLibraryExA = (void *)GetProcAddress(k, "LoadLibraryExA");
        pCreateWaitableTimerExW = (void *)GetProcAddress(k, "CreateWaitableTimerExW");
        sTimerTls = TlsAlloc();
        InitializeCriticalSection(&sLock);
        sNum = kConfig.mNum > 0 ? kConfig.mNum : 1;
        sDen = kConfig.mDen > 0 ? kConfig.mDen : 1;
        QueryPerformanceFrequency(&f); sFreq = f.QuadPart;
        sQ0 = RealNow();
        sT0 = timeGetTime();
        engine = GetModuleHandleA(NULL);   /* the engine, the renderer and the game are one image */
        patched = engine ? PatchImports(engine, 0) : -1;
        exeslots = patched;
        HookRenderer(engine);
        wsprintfA(msg, "EE-TEST-TIMESHIM factor %ld/%ld engine %p kernel32 slots %d hires %d frame-ms %lu render %08lx..%08lx "
                  "exe slots %d null-3d %lu key-ms %lu\n", sNum, sDen, engine, patched, pCreateWaitableTimerExW != NULL,
                  kConfig.mFrameMs, kConfig.mRenderLo, kConfig.mRenderHi, exeslots, kConfig.mNull3D, kConfig.mKeyMs);
        OutputDebugStringA(msg);
        if (kConfig.mNull3D || kConfig.mFrameMs) CloseHandle(CreateThread(NULL, 0, Stats, NULL, 0, NULL));
    }
    return TRUE;
}
