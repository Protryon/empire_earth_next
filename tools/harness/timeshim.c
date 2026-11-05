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

/* patched by timeshim.py: magic, factor num/den, RTSGraphics::Main [lo, hi) as RVAs of the exe, frame cap ms, null 3D,
   key ms. One source for the i686 and the x86_64 DLL (timeshim.py builds the one of the exe's architecture). */
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

/* ---- null 3D: not here ----
 * The renderer runs on GX (wgpu); its null 3D device is GX's own test switch (EE_GX_NULL3D, set by timeshim.py in the
 * environment the game inherits). kConfig.mNull3D/mKeyMs stay in the block (timeshim.py fills them; informational).
 * The earlier Direct3D 7 vtable hooks (IDirect3DDevice7 Clear/Draw*) are gone with the DX7 renderer. */

/* ---- render frame cap (exe IAT Sleep, only for calls returning into RTSGraphics::Main) ----
 * RTSGraphics::Main renders a frame and calls Sleep(1) in every loop iteration; the software renderer then holds the
 * world lock for most of the wall time. With mFrameMs set, a Sleep(1) whose return address lies in
 * [mRenderLo, mRenderHi) (RVAs of RTSGraphics::Main in the running exe; timeshim.py locates it per build) waits until
 * mFrameMs REAL ms after the previous frame. All other Sleep calls pass through unchanged. */
static LONGLONG sNextFrame;
static volatile LONG sCapped;
static __attribute__((noinline)) VOID WINAPI ShimExeSleep(DWORD ms)
{
    ULONG_PTR from = (ULONG_PTR)__builtin_return_address(0) - (ULONG_PTR)GetModuleHandleA(NULL);   /* an RVA */
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
            if (!to) continue;
            VirtualProtect(&slots->u1.Function, sizeof(void *), PAGE_READWRITE, &old);
            slots->u1.Function = (DWORD_PTR)to;
            VirtualProtect(&slots->u1.Function, sizeof(void *), old, &old);
            count++;
        }
    }
    return count;
}

/* every 10 real s: frame-capped sleeps */
static DWORD WINAPI Stats(LPVOID unused)
{
    char m[160];
    (void)unused;
    for (;;) {
        rSleep(10000);
        wsprintfA(m, "EE-TEST-TIMESHIM stats capped-sleeps %ld\n", sCapped);
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
        wsprintfA(msg, "EE-TEST-TIMESHIM factor %ld/%ld engine %p kernel32 slots %d hires %d frame-ms %lu render rva %08lx..%08lx "
                  "exe slots %d null-3d %lu key-ms %lu\n", sNum, sDen, engine, patched, pCreateWaitableTimerExW != NULL,
                  kConfig.mFrameMs, kConfig.mRenderLo, kConfig.mRenderHi, exeslots, kConfig.mNull3D, kConfig.mKeyMs);
        OutputDebugStringA(msg);
        if (kConfig.mFrameMs) CloseHandle(CreateThread(NULL, 0, Stats, NULL, 0, NULL));
    }
    return TRUE;
}
