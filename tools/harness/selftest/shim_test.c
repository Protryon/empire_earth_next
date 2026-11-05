/* TEST ONLY: the time shim's target in the harness self test (selftest_inproc.py). Staged as EE-AOC.exe with its
 * WINMM import renamed to EETMS.dll (timeshim.stage_timeshim), it imports what the shim patches: KERNEL32
 * QueryPerformanceCounter, Sleep, WaitForSingleObject, WaitForMultipleObjects (IAT) and WINMM timeGetTime,
 * timeSetEvent, timeKillEvent (EETMS.dll exports). Real time comes from GetTickCount64 and SleepEx, which the shim
 * leaves alone. Prints one line of results. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

static volatile LONG ticks;
static void CALLBACK tick(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR a, DWORD_PTR b)
{
    (void)id; (void)msg; (void)user; (void)a; (void)b;
    InterlockedIncrement(&ticks);
}

/* stands in for RTSGraphics::Main: its Sleep(1) calls are frame-capped when the shim's render range covers it */
__attribute__((noinline, used)) void render_main(int frames)
{
    int i;
    for (i = 0; i < frames; i++) Sleep(1);
}
__attribute__((noinline, used)) void render_main_end(void) {}

int main(void)
{
    LARGE_INTEGER f, q0, q1;
    HANDLE e[2];
    ULONGLONG r0, r1, a, b;
    DWORD t0, t1, wfso, wfmo;
    UINT id;
    QueryPerformanceFrequency(&f);
    e[0] = CreateEventA(NULL, FALSE, FALSE, NULL); e[1] = CreateEventA(NULL, FALSE, FALSE, NULL);
    QueryPerformanceCounter(&q0); t0 = timeGetTime(); r0 = GetTickCount64();
    a = GetTickCount64(); Sleep(800); b = GetTickCount64();
    ULONGLONG sleep_ms = b - a;
    a = GetTickCount64(); wfso = WaitForSingleObject(e[0], 800); b = GetTickCount64();
    ULONGLONG wfso_ms = b - a;
    a = GetTickCount64(); wfmo = WaitForMultipleObjects(2, e, FALSE, 800); b = GetTickCount64();
    ULONGLONG wfmo_ms = b - a;
    id = timeSetEvent(80, 1, tick, 0, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);
    SleepEx(500, FALSE);
    timeKillEvent(id);
    a = GetTickCount64(); render_main(5); b = GetTickCount64();
    ULONGLONG render_ms = b - a;
    QueryPerformanceCounter(&q1); t1 = timeGetTime(); r1 = GetTickCount64();
    double real = (double)(r1 - r0), qpc = (q1.QuadPart - q0.QuadPart) * 1000.0 / f.QuadPart;
    printf("sleep_ms %llu wfso %lu wfso_ms %llu wfmo %lu wfmo_ms %llu ticks %ld render_ms %llu qpc_ratio %.2f tgt_ratio %.2f\n",
           sleep_ms, wfso, wfso_ms, wfmo, wfmo_ms, ticks, render_ms, qpc / real, (t1 - t0) / real);
    return 0;
}
