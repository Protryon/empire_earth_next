/* TEST ONLY (round 39, agent input): a scripted, deterministic HUMAN player for the ai-game exercise.
 *
 * NOT ORIGINAL SOURCE, never part of a reconstructed module or of matching credit. Staged as <stage>/TestHuman.dll and
 * loaded by the presentation bridge only when <stage>/human.cfg exists (tools/boot/human/human.py writes both).
 *
 * WHAT IT DRIVES: the game's own input path. A scripted mouse click or key press is a write of the engine's input
 * device state (ISMouse::mButtons transition counters, ISKeyboard::mStatus / mModifierState / key counts, the game's
 * cursor EEMouse::mPosition) followed by the call the user-interface thread makes every frame,
 * gUIManager->UpdateUI(gMouse->mPosition) (EEUserInterface::Method278ED3 0x678ed3), under the same locks
 * (EEMouse::mInputLock, gWorld.mLock read). UpdateUI runs the hot keys (UIForm::UpdateKeyboard -> UIHotKey::FireEvents),
 * the hover hit test (UIForm::UpdateMouseLocation), the controls' mouse handlers and the main game form's per-mode
 * mouse tests (EEUIFMainGame: StartSelection / StartCommand / build placement ...); the actions those post
 * (EEAMouseClick, EEALasso, EEABuild, EEAMiniMap ...) are executed by the UI thread (Method277F21) and reach
 * gCommunications->ProcessLocalUserInput like a player's. Only the DirectInput read (ISMouse/ISKeyboard::Refresh) is
 * replaced; the device delivers nothing on this headless box.
 *
 * WHEN (determinism): an input executes at the PT of the communications tick (CT) it rides in, and the server's send
 * thread (NESynchronousServer::Send, wall-time timer) picks the CT. So the scripted PT is the SERVER's
 * mCurrentPhysicsTick (+0x178, the first PT of the next CT to be sent): this thread takes the server's own
 * mSendCommunicationsTickLock (+0x168, entered by SendCommunicationsTick first), steps CT by CT until the next CT starts
 * at PT >= the scripted PT, waits until physics has executed every PT already sent (gPhysics->mUB8 == 0 and mUB4
 * stable: the world the player sees is the state at that PT in both builds), performs the input, waits until the UI
 * thread's action queue is empty, and releases the lock: the inputs ride exactly that CT. Timing only; the image is
 * not patched and no game state is written except the input-device fields above (restored after each action) and the
 * cursor (restored).
 *
 * Every action is logged to <stage>/human.log and OutputDebugString ("TEST HUMAN ..."): scripted PT, server PT/CT,
 * physics PT, PTs per CT, inputs queued, the resolved screen point, and a short result.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

typedef unsigned char BYTEv;
#define TC __attribute__((thiscall))
typedef void (TC *Fv)(void*);
typedef long (TC *FlP)(void*, const void*);
typedef void (TC *FvP)(void*, const void*);
typedef void (TC *FvPP)(void*, const void*, void*);
typedef const void* (TC *FpV)(void*);
typedef const char* (TC *FcV)(void*);
typedef char (__cdecl *S2W)(const void*, void**, void*, char);

static HMODULE self;
static char stage_dir[MAX_PATH];
static FILE* hlog;

/* ---- configuration (human.cfg) ---- */
static DWORD c_ui_manager, c_world, c_comm, c_ui, c_mouse, c_physics, c_s2w, c_minitf, c_gamewindow,
             c_vt_minimap, c_vt_status, c_vt_select, c_vt_command, c_last_report, c_report_period;
static DWORD c_settle_ms = 150, c_margin = 0;
/* no_civ_wait 1 (opt-in; final campaign lane mp): a multiplayer game has no civilization-builder preempt, so do not
   wait for mBB4A (the 120000 x Sleep(1) wait outlasts the first ~300 game s of a 4-peer game at F=1) */
static DWORD c_no_civ_wait, c_lockstep;
typedef struct { unsigned long pt; char op[24]; char args[200]; int line; } Action;
static Action acts[512]; static int nacts;

/* ---- engine exports ---- */
static Fv cs_enter, cs_leave, rw_rlock, rw_runlock;
typedef void* (TC *GetTrigger)(void*, unsigned long);
static GetTrigger get_trigger;
typedef unsigned long (__cdecl *TMGetTimeFn)(void);
static TMGetTimeFn tm_get_time;

static void note(const char* fmt, ...)
{
    char text[700]; va_list ap; va_start(ap, fmt); vsnprintf(text, sizeof text - 2, fmt, ap); va_end(ap);
    if (hlog) { fprintf(hlog, "[%lu] %s\n", (unsigned long)GetTickCount(), text); fflush(hlog); }
    char dbg[720]; snprintf(dbg, sizeof dbg, "TEST HUMAN %s\n", text); OutputDebugStringA(dbg);
}

static DWORD R32(DWORD a) { return a ? *(volatile DWORD*)(uintptr_t)a : 0; }
static BYTE R8(DWORD a) { return *(volatile BYTE*)(uintptr_t)a; }
#define PTR(x) ((void*)(uintptr_t)(x))
#define VT(obj, off) ((*(void***)(obj))[(off) / 4])

static DWORD server(void) { return R32(c_comm); }
static DWORD world_lock(void) { return R32(c_world + 0xb8); }
static DWORD mouse_obj(void) { return R32(c_mouse); }
static DWORD ui_manager(void) { return R32(c_ui_manager); }
static DWORD ismouse(void) { static DWORD* p; if (!p) p = (DWORD*)GetProcAddress(GetModuleHandleA("Low-Level Engine.dll"), "?gISMouse@@3PAVISMouse@@A"); return p ? *p : 0; }
static DWORD iskeyboard(void) { static DWORD* p; if (!p) p = (DWORD*)GetProcAddress(GetModuleHandleA("Low-Level Engine.dll"), "?gISKeyboard@@3PAVISKeyboard@@A"); return p ? *p : 0; }

static DWORD top_form(void)
{
    DWORD m = ui_manager(); if (!m) return 0;
    DWORD first = R32(m + 0x30), end = R32(m + 0x34);
    return end > first ? R32(end - 4) : 0;
}
static DWORD form_id(DWORD f) { return f ? R32(f + 0x180) : 0xffffffff; }
static DWORD find_form(DWORD id)
{
    DWORD m = ui_manager(); if (!m) return 0;
    for (DWORD p = R32(m + 0x30); p < R32(m + 0x34); p += 4) if (form_id(R32(p)) == id) return R32(p);
    return 0;
}

/* CRITICAL_SECTION waiters (Windows 8+ encoding: LockCount bit 0 clear = locked; waiters = (~LockCount) >> 2) */
static int cs_waiters(DWORD cs) { LONG lc = ((CRITICAL_SECTION*)PTR(cs))->LockCount; return (int)(((~(ULONG)lc)) >> 2); }

/* ---- input locks: the UI thread's order (mInputLock; then gWorld.mLock read; UpdateUI takes the UI critical
   section itself). After taking mInputLock wait until the UI thread queues on it, so it has finished its
   keyboard Refresh and cannot read the devices while we write them. ---- */
static int input_locked;
/* hang diagnostics (f04, full link, PT 7000: the driver stopped inside one action while holding the CT lock, every
   thread waiting; not reproduced): the driver's current phase, and a watchdog thread that logs it with the owners of
   the locks the driver takes when one phase lasts > 20 s. Read-only. */
static volatile const char* phase = "start"; static volatile DWORD phase_t, driver_tid; static int ct_holding;
#define PHASE(s) (phase = (s), phase_t = GetTickCount())
static DWORD ct_lock(void);
static void ct_enter(void); static void ct_leave(void);
static DWORD cs_owner(DWORD cs) { return cs ? (DWORD)(ULONG_PTR)((CRITICAL_SECTION*)PTR(cs))->OwningThread : 0; }
static DWORD WINAPI watchdog(LPVOID unused)
{
    (void)unused; DWORD reported = 0;
    for (;;) {
        Sleep(1000);
        DWORD t = phase_t;
        if (!t || GetTickCount() - t < 20000 || reported == t) continue;
        reported = t;
        DWORD il = mouse_obj() ? R32(mouse_obj() + 0x7c) : 0, ct = server() ? ct_lock() : 0, wl = R32(c_world + 0xb8);
        note("WATCHDOG: driver thread %lu in phase '%s' for %lu s; CT lock %08lx owner %lu (driver holds %d); mInputLock %08lx owner %lu waiters %d; "
             "gWorld.mLock %08lx dwords %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx", driver_tid, (const char*)phase,
             (GetTickCount() - t) / 1000, ct, cs_owner(ct), ct_holding, il, cs_owner(il), il ? cs_waiters(il) : 0, wl,
             wl ? R32(wl) : 0, wl ? R32(wl + 4) : 0, wl ? R32(wl + 8) : 0, wl ? R32(wl + 12) : 0, wl ? R32(wl + 16) : 0,
             wl ? R32(wl + 20) : 0, wl ? R32(wl + 24) : 0, wl ? R32(wl + 28) : 0);
    }
    return 0;
}
static void lock_input(void)
{
    DWORD il = R32(mouse_obj() + 0x7c);
    PHASE("mInputLock");
    /* never wait without bound for the UI thread's input lock while holding the server's CT lock: after 10 s release
       the CT lock (the game may then send CTs: this action lands late and is logged LOCK-RELEASED), wait, and re-take
       it (lock order mInputLock -> CT for the rest of this action) */
    for (DWORD t0 = GetTickCount(); !TryEnterCriticalSection((CRITICAL_SECTION*)PTR(il)); YieldProcessor())
        if (ct_holding && GetTickCount() - t0 > 10000) {
            note("  LOCK-RELEASED: mInputLock %08lx owned by thread %lu for 10 s while the driver held the CT lock (owner of CT: driver)", il, cs_owner(il));
            ct_leave(); EnterCriticalSection((CRITICAL_SECTION*)PTR(il)); ct_enter(); break;
        }
    cs_enter(PTR(il));                                  /* the bookkeeping Enter (recursive: we own it) */
    LeaveCriticalSection((CRITICAL_SECTION*)PTR(il));   /* drop the raw TryEnter count */
    for (int i = 0; i < 300 && cs_waiters(il) < 1; i++) Sleep(1);
    PHASE("gWorld.mLock read");
    rw_rlock(PTR(world_lock()));
    PHASE("input locked");
    input_locked = 1;
}
static void unlock_input(void)
{
    rw_runlock(PTR(world_lock()));
    cs_leave(PTR(R32(mouse_obj() + 0x7c)));
    input_locked = 0;
}

static void frame(void)
{
    DWORD m = ui_manager(), mo = mouse_obj();
    typedef long (TC *UpdateUI)(void*, const void*);
    ((UpdateUI)VT(PTR(m), 0x28))(PTR(m), PTR(mo + 4));
}

/* device state ---------------------------------------------------------------------------------------- */
static void mouse_sync(void) { DWORD d = ismouse(); memcpy(PTR(d + 0x1c), PTR(d + 0x18), 4); }
static void mouse_toggle(int b) { DWORD d = ismouse(); mouse_sync(); *(BYTE*)PTR(d + 0x18 + b) += 1; }
static void mouse_move_flag(long dx) { DWORD d = ismouse(); DWORD st = R32(d + 0x48); if (st) *(LONG*)PTR(st) = dx; }
static void set_cursor(float x, float y) { DWORD mo = mouse_obj(); *(float*)PTR(mo + 4) = x; *(float*)PTR(mo + 8) = y; }

static void key_sync(void) { DWORD k = iskeyboard(); memcpy(PTR(k + 0x13c), PTR(k + 0x3c), 256); *(LONG*)PTR(k + 0x38) = *(LONG*)PTR(k + 0x34); }
static void key_set(int dik, int down)
{
    DWORD k = iskeyboard(); BYTE* st = (BYTE*)PTR(k + 0x3c);
    if (((st[dik] & 1) != 0) != (down != 0)) {
        st[dik]++;
        *(LONG*)PTR(k + 0x34) += down ? 1 : -1;
        unsigned long mod = 0;
        if (dik == 0x2a || dik == 0x36) mod = 1;
        if (dik == 0x38 || dik == 0xb8) mod |= 4;
        if (dik == 0x1d || dik == 0x9d) mod |= 2;
        if (mod) { if (down) *(DWORD*)PTR(k + 0x28) |= mod; else *(DWORD*)PTR(k + 0x28) &= ~mod; }
    }
}
static void key_idle(void) { DWORD k = iskeyboard(); key_sync(); *(LONG*)PTR(k + 0x38) = 0; }

/* ---- keys by name (DIK codes) ---- */
static const struct { const char* n; int k; } KEYS[] = {
    {"esc",1},{"1",2},{"2",3},{"3",4},{"4",5},{"5",6},{"6",7},{"7",8},{"8",9},{"9",10},{"0",11},{"minus",12},{"equals",13},
    {"back",14},{"tab",15},{"q",16},{"w",17},{"e",18},{"r",19},{"t",20},{"y",21},{"u",22},{"i",23},{"o",24},{"p",25},
    {"lbracket",26},{"rbracket",27},{"enter",28},{"a",30},{"s",31},{"d",32},{"f",33},{"g",34},{"h",35},{"j",36},{"k",37},
    {"l",38},{"semicolon",39},{"apostrophe",40},{"grave",41},{"backslash",43},{"z",44},{"x",45},{"c",46},{"v",47},{"b",48},
    {"n",49},{"m",50},{"comma",51},{"period",52},{"slash",53},{"space",57},{"f1",59},{"f2",60},{"f3",61},{"f4",62},
    {"f5",63},{"f6",64},{"f7",65},{"f8",66},{"f9",67},{"f10",68},{"f11",87},{"f12",88},{"home",0xc7},{"up",0xc8},
    {"pgup",0xc9},{"left",0xcb},{"right",0xcd},{"end",0xcf},{"down",0xd0},{"pgdn",0xd1},{"insert",0xd2},{"delete",0xd3},
    {"shift",0x2a},{"ctrl",0x1d},{"alt",0x38},{NULL,0}};
static int dik_of(const char* name)
{
    for (int i = 0; KEYS[i].n; i++) if (!_stricmp(KEYS[i].n, name)) return KEYS[i].k;
    if (!_strnicmp(name, "dik", 3)) return (int)strtol(name + 3, NULL, 0);
    return -1;
}

/* ---- primitives (callers hold the input locks) ---- */
static void do_key(const char* spec)
{
    /* spec: [ctrl+][shift+][alt+]KEY */
    char buf[64]; snprintf(buf, sizeof buf, "%s", spec);
    int mods[3], nm = 0, key = -1;
    for (char* t = strtok(buf, "+"); t; t = strtok(NULL, "+")) {
        int k = dik_of(t);
        if (k == 0x2a || k == 0x1d || k == 0x38) { if (nm < 3) mods[nm++] = k; }
        else key = k;
    }
    if (key < 0) { note("  key %s: unknown key", spec); return; }
    key_sync();
    for (int i = 0; i < nm; i++) key_set(mods[i], 1);
    if (nm) { frame(); key_sync(); }
    key_set(key, 1); frame(); key_sync();
    key_set(key, 0); frame(); key_sync();
    for (int i = 0; i < nm; i++) key_set(mods[i], 0);
    if (nm) frame();
    key_idle();
}

static void hold_mods(const char* mods, int down)
{
    if (!mods) return;
    if (strstr(mods, "shift")) key_set(0x2a, down);
    if (strstr(mods, "ctrl")) key_set(0x1d, down);
    if (strstr(mods, "alt")) key_set(0x38, down);
}

static float saved_x, saved_y;
static void click_at(int button, float x, float y, const char* mods)
{
    DWORD mo = mouse_obj();
    saved_x = *(float*)PTR(mo + 4); saved_y = *(float*)PTR(mo + 8);
    key_sync(); hold_mods(mods, 1); if (mods) { frame(); key_sync(); }
    set_cursor(x, y);
    mouse_toggle(button); frame();          /* press: hover hit test at (x, y), then the press handlers */
    mouse_toggle(button); frame();          /* release */
    mouse_sync();
    hold_mods(mods, 0); if (mods) frame();
    key_idle();
    set_cursor(saved_x, saved_y);
}
static void drag(int button, float x0, float y0, float x1, float y1, const char* mods)
{
    DWORD mo = mouse_obj();
    saved_x = *(float*)PTR(mo + 4); saved_y = *(float*)PTR(mo + 8);
    key_sync(); hold_mods(mods, 1); if (mods) { frame(); key_sync(); }
    set_cursor(x0, y0);
    mouse_toggle(button); frame();
    for (int s = 1; s <= 4; s++) {          /* four moves along the drag (lX != 0: a mouse move) */
        mouse_sync(); mouse_move_flag(1);
        set_cursor(x0 + (x1 - x0) * s / 4, y0 + (y1 - y0) * s / 4); frame();
    }
    mouse_move_flag(0);
    mouse_toggle(button); frame();
    mouse_sync();
    hold_mods(mods, 0); if (mods) frame();
    key_idle();
    set_cursor(saved_x, saved_y);
}

/* ---- world <-> screen ---- */
static int screen_to_world(float sx, float sy, float* wx, float* wy)
{
    float pt[2] = {sx, sy}, w[3] = {0, 0, 0}; void* tile = NULL;
    char ok = ((S2W)PTR(c_s2w))(pt, &tile, w, 1);
    *wx = w[0]; *wy = w[1];
    return ok != 0;
}
static float win_w(void) { return (float)R32(R32(c_gamewindow) + 0x0c); }
static float win_h(void) { return (float)R32(R32(c_gamewindow) + 0x10); }
/* the game view: the screen above the bottom panel (EEUIFMainGame::IsInGameView's area is approximated by a margin;
   a point outside it is refused and the camera is centred first) */
static int in_view(float x, float y) { return x >= 24 && y >= 40 && x <= win_w() - 24 && y <= win_h() * 0.70f; }
static int world_to_screen(float wx, float wy, float* sx, float* sy)
{
    float x = win_w() / 2, y = win_h() * 0.38f;
    for (int it = 0; it < 12; it++) {
        float a, b, ax, ay, bx, by;
        if (!screen_to_world(x, y, &a, &b) || !screen_to_world(x + 4, y, &ax, &ay) || !screen_to_world(x, y + 4, &bx, &by)) return 0;
        float j11 = (ax - a) / 4, j21 = (ay - b) / 4, j12 = (bx - a) / 4, j22 = (by - b) / 4;
        float det = j11 * j22 - j12 * j21; if (fabsf(det) < 1e-9f) return 0;
        float ex = wx - a, ey = wy - b;
        if (fabsf(ex) < 0.02f && fabsf(ey) < 0.02f) { *sx = x; *sy = y; return 1; }
        x += (j22 * ex - j12 * ey) / det; y += (-j21 * ex + j11 * ey) / det;
        if (x < -4000 || x > 8000 || y < -4000 || y > 8000) return 0;
    }
    float a, b; if (!screen_to_world(x, y, &a, &b)) return 0;
    *sx = x; *sy = y; return fabsf(a - wx) < 0.1f && fabsf(b - wy) < 0.1f;
}

/* the main game form's controls */
static DWORD control(DWORD form, DWORD i) { return (form && i < R32(form + 0x170)) ? R32(R32(form + 0xf8) + 4 * i) : 0; }
static int control_center(DWORD c, float* x, float* y)
{
    if (!c) return 0;
    float* r = (float*)PTR(c + 0x18);   /* GERectangle bottom, left, right, top */
    *x = (r[1] + r[2]) / 2; *y = (r[3] + r[0]) / 2; return 1;
}
static DWORD find_by_vtable(DWORD form, DWORD vt)
{
    if (!form) return 0;   /* no such form on the stack (probe runs: a scenario after closing diplomacy; was an access violation at 0x170) */
    for (DWORD i = 0; i < R32(form + 0x170); i++) { DWORD c = control(form, i); if (c && R32(c) == vt) return c; }
    return 0;
}

/* the mini map: screen point of a world tile, inverting the control's own mapping
   world = gMiniMapTransformation((mouse - mOrigin) / (W * 0.00125, H * 0.0016666667)) */
static int minimap_point(float wx, float wy, float* sx, float* sy)
{
    DWORD form = find_form(11), mm = find_by_vtable(form, c_vt_minimap);
    if (!mm) return 0;
    static FvPP apply; if (!apply) apply = (FvPP)GetProcAddress(GetModuleHandleA("Low-Level Engine.dll"), "?Apply@GETransformation@@QBEXABVGE2DPoint@@AAV2@@Z");
    float o[2] = {0, 0}, e1[2] = {100, 0}, e2[2] = {0, 100}, w0[2], w1[2], w2[2];
    apply(PTR(c_minitf), o, w0); apply(PTR(c_minitf), e1, w1); apply(PTR(c_minitf), e2, w2);
    float a = (w1[0] - w0[0]) / 100, c = (w1[1] - w0[1]) / 100, b = (w2[0] - w0[0]) / 100, d = (w2[1] - w0[1]) / 100;
    float det = a * d - b * c; if (fabsf(det) < 1e-9f) return 0;
    float ex = wx - w0[0], ey = wy - w0[1];
    float px = (d * ex - b * ey) / det, py = (-c * ex + a * ey) / det;
    float* origin = (float*)PTR(mm + 0xa0);
    *sx = origin[0] + px * (win_w() * 0.00125f); *sy = origin[1] + py * (win_h() * 0.0016666667f);
    return 1;
}

/* ---- units ---- */
static DWORD player(DWORD i) { return R32(c_world + 0x74 + 4 * i); }
static DWORD nplayers(void) { return R32(c_world + 0xcc); }
static DWORD local_index(void) { return R32(c_world + 0xb84); }
static DWORD find_unit(long id)
{
    for (DWORD p = 0; p < nplayers() && p < 17; p++)
        for (DWORD u = R32(player(p) + 0x450), n = 0; u && n < 100000; u = R32(u + 0x0c), n++)
            if ((long)R32(u + 0x24) == id) return u;
    return 0;
}
static long unit_type(DWORD u) { DWORD cls = R32(u + 0x2c); return cls ? (long)R32(cls + 0x1bc) : -1; }
/* the n-th live unit (list order) of player p with class type t (t < 0: any) */
static DWORD find_unit_of(DWORD p, long t, int nth)
{
    if (p >= nplayers()) return 0;
    for (DWORD u = R32(player(p) + 0x450), n = 0; u && n < 100000; u = R32(u + 0x0c), n++)
        if ((t < 0 || unit_type(u) == t) && (long)R32(u + 0x3c) > 0 && R32(u + 0x14) && nth-- == 0) return u;
    return 0;
}
static int unit_tile(DWORD u, float* x, float* y)
{
    DWORD cu = R32(u + 0x30);
    if (cu) { float* p = (float*)PTR(cu + 0x10c); *x = p[0]; *y = p[1]; if (*x > 0 || *y > 0) return 1; }
    *x = (long)R32(u + 0x1c) + 0.5f; *y = (long)R32(u + 0x20) + 0.5f; return (long)R32(u + 0x1c) > -10000;
}

/* the N-th nearest live unit of player p with type t (t < 0: any) to the local player's first town center (type 41)
   or, without one, its first unit; ties broken by list order */
static int home(float* x, float* y)
{
    DWORD u = find_unit_of(local_index(), 41, 0); if (!u) u = find_unit_of(local_index(), -1, 0);
    return u && unit_tile(u, x, y);
}
static DWORD find_near(DWORD p, long t, int nth)
{
    float hx, hy; if (p >= nplayers() || !home(&hx, &hy)) return 0;
    DWORD taken[16]; int ntaken = 0;
    for (int k = 0; k <= nth && k < 16; k++) {
        DWORD best = 0; float bd = 1e30f;
        for (DWORD u = R32(player(p) + 0x450), n = 0; u && n < 100000; u = R32(u + 0x0c), n++) {
            if ((t >= 0 && unit_type(u) != t) || (long)R32(u + 0x3c) <= 0 || !R32(u + 0x14)) continue;
            int skip = 0; for (int i = 0; i < ntaken; i++) skip |= taken[i] == u; if (skip) continue;
            float x, y; if (!unit_tile(u, &x, &y)) continue;
            float d = (x - hx) * (x - hx) + (y - hy) * (y - hy);
            if (d < bd) { bd = d; best = u; }
        }
        if (!best) return 0;
        taken[ntaken++] = best;
    }
    return ntaken ? taken[ntaken - 1] : 0;
}

/* a world target: "x y" (tile coordinates), "unit ID", "of P TYPE [N]" (the N-th unit of player P with type TYPE),
   "mine TYPE [N]" (the local player's) ; returns the number of tokens used, 0 on failure */
static int world_target(char** tok, int ntok, float* x, float* y, long* uid)
{
    *uid = -1;
    if (ntok >= 2 && !strcmp(tok[0], "unit")) {
        DWORD u = find_unit(strtol(tok[1], NULL, 0)); if (!u || !unit_tile(u, x, y)) return 0;
        *uid = (long)R32(u + 0x24); return 2;
    }
    if (ntok >= 3 && !strcmp(tok[0], "of")) {
        int nth = ntok >= 4 && tok[3][0] >= '0' && tok[3][0] <= '9' ? atoi(tok[3]) : 0;
        DWORD u = find_unit_of(strtoul(tok[1], NULL, 0), strtol(tok[2], NULL, 0), nth);
        if (!u || !unit_tile(u, x, y)) return 0;
        *uid = (long)R32(u + 0x24); return ntok >= 4 && tok[3][0] >= '0' && tok[3][0] <= '9' ? 4 : 3;
    }
    if (ntok >= 2 && !strcmp(tok[0], "mine")) {
        int nth = ntok >= 3 && tok[2][0] >= '0' && tok[2][0] <= '9' ? atoi(tok[2]) : 0;
        DWORD u = find_unit_of(local_index(), strtol(tok[1], NULL, 0), nth);
        if (!u || !unit_tile(u, x, y)) return 0;
        *uid = (long)R32(u + 0x24); return ntok >= 3 && tok[2][0] >= '0' && tok[2][0] <= '9' ? 3 : 2;
    }
    if (ntok >= 3 && !strcmp(tok[0], "near")) {   /* near P TYPE [N]: the N-th nearest to home */
        int has = ntok >= 4 && tok[3][0] >= '0' && tok[3][0] <= '9';
        DWORD u = find_near(strtoul(tok[1], NULL, 0), strtol(tok[2], NULL, 0), has ? atoi(tok[3]) : 0);
        if (!u || !unit_tile(u, x, y)) return 0;
        *uid = (long)R32(u + 0x24); return has ? 4 : 3;
    }
    if (ntok >= 4 && !strcmp(tok[0], "anyof")) {   /* anyof P TYPE K: the (K mod count)-th live unit (list order)
                                                     of player P with type TYPE (< 0: any); fuzz scripts */
        DWORD p = strtoul(tok[1], NULL, 0); long t = strtol(tok[2], NULL, 0); unsigned long count = 0;
        if (p >= nplayers()) return 0;
        for (DWORD u = R32(player(p) + 0x450), m = 0; u && m < 100000; u = R32(u + 0x0c), m++)
            if ((t < 0 || unit_type(u) == t) && (long)R32(u + 0x3c) > 0 && R32(u + 0x14)) count++;
        DWORD u = count ? find_unit_of(p, t, (int)(strtoul(tok[3], NULL, 0) % count)) : 0;
        if (!u || !unit_tile(u, x, y)) return 0;
        *uid = (long)R32(u + 0x24); return 4;
    }
    if (ntok >= 1 && !strcmp(tok[0], "home")) { if (!home(x, y)) return 0; return 1; }
    if (ntok >= 3 && !strcmp(tok[0], "home+")) { if (!home(x, y)) return 0; *x += (float)atof(tok[1]); *y += (float)atof(tok[2]); return 3; }
    if (ntok >= 2) { *x = (float)atof(tok[0]); *y = (float)atof(tok[1]); return 2; }
    return 0;
}

/* release every lock, let the UI thread run (it executes posted actions; the camera/render follow), re-lock */
static void breathe(DWORD ms) { int was = input_locked; if (was) unlock_input(); Sleep(ms); if (was) lock_input(); }

/* centre the camera on a world point the way a player does: a left click on the mini map */
static int center_camera(float wx, float wy)
{
    float sx, sy;
    if (!minimap_point(wx, wy, &sx, &sy)) { note("  camera: mini map not found"); return 0; }
    click_at(0, sx, sy, NULL);
    return 1;
}

/* camera calibration: a mini-map click centres the camera's look-at point, which shows at the view centre
   (w/2, 0.38 h) only for the default camera; a scenario's camera (pitch / zoom) puts it elsewhere (a new dynasty: the
   clicked point showed ~30 tiles below the centre, off the view). The first world click after a `camera` line measures
   that offset (view centre minus the clicked mini-map point) and later `camera` lines aim at TARGET minus it. */
static float cam_px, cam_py, cam_dx, cam_dy, cam_bx, cam_by; static int cam_pending, cam_before;
static void cam_measure(void)
{
    if (!cam_pending) return;
    cam_pending = 0;
    float cx, cy; if (!screen_to_world(win_w() / 2, win_h() * 0.38f, &cx, &cy)) return;
    float dx = cx - cam_px, dy = cy - cam_py;
    /* only when the camera really moved to the click (a cinematic locks it: g01 learned nonsense offsets) */
    float mx = cx - cam_bx, my = cy - cam_by, want = (cam_px - cam_bx) * (cam_px - cam_bx) + (cam_py - cam_by) * (cam_py - cam_by);
    if (!cam_before || (want > 4.0f && mx * mx + my * my < 0.25f * want)) return;
    if (dx * dx + dy * dy < 40.0f * 40.0f) { cam_dx = dx; cam_dy = dy; }
}

static int world_screen(float wx, float wy, float* sx, float* sy)
{
    /* no automatic camera move: the camera follows its target only while PTs run (EEPhysics::Virtual20 calls
       EECameraControl::Fn129A34), and the gate holds the server; put a `camera TARGET` line some PTs earlier */
    cam_measure();
    if (world_to_screen(wx, wy, sx, sy) && in_view(*sx, *sy)) return 1;
    float cx = 0, cy = 0; screen_to_world(win_w() / 2, win_h() * 0.38f, &cx, &cy);
    note("  not in view: world %.1f,%.1f -> screen %.1f,%.1f; the view centre shows %.1f,%.1f (add a camera line earlier)", wx, wy, *sx, *sy, cx, cy);
    return 0;
}

/* ---- state dump (for script authoring; human-state.txt) ---- */
static void dump_state(const char* label)
{
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\human-state.txt", stage_dir);
    FILE* f = fopen(path, "a"); if (!f) return;
    DWORD form = find_form(11); HMODULE exe = GetModuleHandleA(NULL);
    fprintf(f, "== %s top form %lu local player %lu players %lu\n", label, form_id(top_form()), local_index(), nplayers());
    if (top_form() && top_form() != form) {
        DWORD tf = top_form();
        for (DWORD i = 0; i < R32(tf + 0x170); i++) {
            DWORD c = control(tf, i); if (!c) continue;
            float* r = (float*)PTR(c + 0x18);
            DWORD ev = get_trigger ? (DWORD)(uintptr_t)get_trigger(PTR(c), 0x3f0) : 0;
            fprintf(f, "top %lu control %lu mode %lu rect %.0f,%.0f-%.0f,%.0f event %06lx\n", form_id(tf), i,
                    R32(c + 8), r[1], r[3], r[2], r[0], ev ? (unsigned long)(R32(R32(ev) + 4) - (DWORD)(uintptr_t)exe) : 0);
        }
    }
    if (form) {
        for (DWORD i = 0; i < R32(form + 0x170); i++) {
            DWORD c = control(form, i); if (!c || (R32(c) == c_vt_status && !R32(c + 8))) continue;   /* hidden status buttons: skipped */
            float* r = (float*)PTR(c + 0x18);
            DWORD ev = get_trigger ? (DWORD)(uintptr_t)get_trigger(PTR(c), 0x3f0) : 0;
            fprintf(f, "control %lu vt %06lx mode %lu rect %.0f,%.0f-%.0f,%.0f event %06lx", i, (unsigned long)(R32(c) - (DWORD)(uintptr_t)exe),
                    R32(c + 8), r[1], r[3], r[2], r[0], ev ? (unsigned long)(R32(R32(ev) + 4) - (DWORD)(uintptr_t)exe) : 0);
            if (R32(c) == c_vt_status) fprintf(f, " status tech %ld type %lu unit %lu cmd %u button %lu avail %u", (long)R32(c + 0x12c),
                                               R32(c + 0x130), R32(c + 0x13c), R8(c + 0x148), R32(c + 0x144), R8(c + 0x14a));
            fprintf(f, "\n");
        }
        /* hot keys: form->mHotKeyList (Dinkum vector at +0x28: allocator, _First +0x2c, _Last +0x30) */
        DWORD hk0 = R32(form + 0x2c), hk1 = R32(form + 0x30);
        for (DWORD p = hk0, n = 0; p < hk1 && n < 600; p += 4, n++) {
            DWORD h = R32(p), key = R32(h + 8);
            DWORD ev = R32(h + 0x1c) ? R32(h + 0x1c) : R32(h + 0x24) ? R32(h + 0x24) : R32(h + 0x18);
            fprintf(f, "hotkey %lu key %lu mods %lu,%lu,%lu event %06lx enabled %u\n", n, key ? R32(key + 0x1c) : 0,
                    R32(h + 0xc) ? R32(R32(h + 0xc) + 0x1c) : 0, R32(h + 0x10) ? R32(R32(h + 0x10) + 0x1c) : 0,
                    R32(h + 0x14) ? R32(R32(h + 0x14) + 0x1c) : 0,
                    ev ? (unsigned long)(R32(R32(ev) + 4) - (DWORD)(uintptr_t)exe) : 0, R8(h + 0x34));
        }
    }
    {   /* player 0 (gaia): per type, the count and the nearest to home */
        long types[64]; int counts[64], nt = 0;
        for (DWORD u = R32(player(0) + 0x450), n = 0; u && n < 200000; u = R32(u + 0x0c), n++) {
            long ty = unit_type(u); int i; for (i = 0; i < nt && types[i] != ty; i++);
            if (i == nt && nt < 64) { types[nt] = ty; counts[nt++] = 0; }
            if (i < nt) counts[i]++;
        }
        fprintf(f, "player 0 types:");
        for (int i = 0; i < nt; i++) {
            DWORD u = find_near(0, types[i], 0); float x = 0, y = 0; if (u) unit_tile(u, &x, &y);
            fprintf(f, " %ld x%d near %lu@%.1f,%.1f", types[i], counts[i], u ? R32(u + 0x24) : 0, x, y);
        }
        fprintf(f, "\n");
    }
    for (DWORD p = 1; p < nplayers() && p < 17; p++) {   /* per type: the count (every unit, not only the first 60) */
        long types[128]; int counts[128], nt = 0;
        for (DWORD u = R32(player(p) + 0x450), n = 0; u && n < 200000; u = R32(u + 0x0c), n++) {
            long ty = unit_type(u); int i; for (i = 0; i < nt && types[i] != ty; i++);
            if (i == nt && nt < 128) { types[nt] = ty; counts[nt++] = 0; }
            if (i < nt) counts[i]++;
        }
        fprintf(f, "player %lu types:", p);
        for (int i = 0; i < nt; i++) fprintf(f, " %ld x%d", types[i], counts[i]);
        fprintf(f, "\n");
    }
    for (DWORD p = 1; p < nplayers() && p < 17; p++) {
        DWORD n = 0; fprintf(f, "player %lu units:", p);
        for (DWORD u = R32(player(p) + 0x450); u && n < 60; u = R32(u + 0x0c), n++) {
            float x, y; unit_tile(u, &x, &y);
            fprintf(f, " %lu:%ld@%.1f,%.1f", R32(u + 0x24), unit_type(u), x, y);
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

static unsigned long selected_count(void)
{
    DWORD p = player(local_index()); DWORD g = p ? R32(p + 0x9b0) : 0; if (!g) return 0;
    typedef unsigned long (TC *Count)(void*);
    return ((Count)VT(PTR(g), 0x18))(PTR(g));
}

/* ---- one action (input locks held) ---- */
static void perform(Action* a, char* result, size_t n)
{
    char buf[200]; snprintf(buf, sizeof buf, "%s", a->args);
    char* tok[24]; int nt = 0;
    for (char* t = strtok(buf, " \t"); t && nt < 24; t = strtok(NULL, " \t")) tok[nt++] = t;
    const char* mods = NULL;
    for (int i = 0; i < nt; i++) if (!strncmp(tok[i], "mod=", 4)) { mods = tok[i] + 4; tok[i] = tok[--nt]; i--; }
    float x, y, sx, sy; long uid;
    DWORD form = find_form(11);
    snprintf(result, n, "ok");
    if (!strcmp(a->op, "key") && nt >= 1) { do_key(tok[0]); return; }
    if (!strcmp(a->op, "click") && nt >= 1) {      /* a control of the TOP form, by index: left click at its centre */
        const char* colon = strchr(tok[0], ':');   /* FORM:CTRL: only when FORM is the top form */
        if (colon && strtoul(tok[0], NULL, 0) != form_id(top_form())) {
            snprintf(result, n, "FAILED click %s: top form is %lu", tok[0], form_id(top_form())); return;
        }
        DWORD c = control(top_form(), strtoul(colon ? colon + 1 : tok[0], NULL, 0));
        if (!control_center(c, &x, &y)) { snprintf(result, n, "FAILED no control %s on form %lu", tok[0], form_id(top_form())); return; }
        if (nt >= 2 && !strcmp(tok[nt - 1], "shown") && !R32(c + 8)) { snprintf(result, n, "FAILED control %s hidden", tok[0]); return; }
        click_at(nt >= 2 && !strcmp(tok[1], "right") ? 1 : 0, x, y, mods);
        snprintf(result, n, "control %s at %.0f,%.0f (form %lu)", tok[0], x, y, form_id(top_form())); return;
    }
    if (!strcmp(a->op, "button") && nt >= 1 && (!strncmp(tok[0], "pick:", 5) || !strncmp(tok[0], "pickcmd:", 8) || !strncmp(tok[0], "picktech:", 9))) {
        /* fuzz: the (K mod count)-th shown status button in control order (pick: any, pickcmd: commands only,
           picktech: technologies only); FAILED when none is shown */
        int kind = tok[0][4] == ':' ? 0 : tok[0][4] == 'c' ? 1 : 2;
        unsigned long k = strtoul(strchr(tok[0], ':') + 1, NULL, 0), count = 0;
        DWORD shown[128];
        for (DWORD i = 0; form && i < R32(form + 0x170) && count < 128; i++) {
            DWORD c = control(form, i);
            if (!c || R32(c) != c_vt_status || !R32(c + 8)) continue;
            if (kind == 1 && !R8(c + 0x148)) continue;
            if (kind == 2 && R8(c + 0x148)) continue;
            shown[count++] = c;
        }
        if (!count) { snprintf(result, n, "FAILED no shown status button (%s)", tok[0]); return; }
        DWORD c = shown[k % count];
        control_center(c, &x, &y);
        click_at(nt >= 2 && !strcmp(tok[1], "right") ? 1 : 0, x, y, mods);
        snprintf(result, n, "status button %lu/%lu %s %ld at %.0f,%.0f avail %u", k % count, count, R8(c + 0x148) ? "cmd" : "tech",
                 R8(c + 0x148) ? (long)R32(c + 0x144) : (long)R32(c + 0x12c), x, y, R8(c + 0x14a));
        return;
    }
    if (!strcmp(a->op, "button") && nt >= 1) {     /* a status-box button by technology (or cmd:N) */
        int cmd = !strncmp(tok[0], "cmd:", 4); long want = strtol(tok[0] + (cmd ? 4 : 0), NULL, 0);
        for (DWORD i = 0; form && i < R32(form + 0x170); i++) {
            DWORD c = control(form, i);
            if (!c || R32(c) != c_vt_status || !R32(c + 8)) continue;
            if (cmd ? (R8(c + 0x148) && (long)R32(c + 0x144) == want) : (!R8(c + 0x148) && (long)R32(c + 0x12c) == want)) {
                control_center(c, &x, &y);
                click_at(nt >= 2 && !strcmp(tok[1], "right") ? 1 : 0, x, y, mods);
                snprintf(result, n, "status button %lu tech %ld at %.0f,%.0f avail %u", i, (long)R32(c + 0x12c), x, y, R8(c + 0x14a));
                return;
            }
        }
        snprintf(result, n, "FAILED no shown status button %s", tok[0]); return;
    }
    if ((!strcmp(a->op, "lclick") || !strcmp(a->op, "rclick")) && nt >= 1) {
        int b = a->op[0] == 'r';
        if (!strcmp(tok[0], "screen")) { x = (float)atof(tok[1]); y = (float)atof(tok[2]); click_at(b, x, y, mods); snprintf(result, n, "screen %.0f,%.0f", x, y); return; }
        if (!world_target(tok, nt, &x, &y, &uid)) { snprintf(result, n, "FAILED target %s", a->args); return; }
        if (!world_screen(x, y, &sx, &sy)) { snprintf(result, n, "FAILED world %.2f,%.2f not on screen", x, y); return; }
        click_at(b, sx, sy, mods);
        snprintf(result, n, "world %.2f,%.2f unit %ld -> screen %.1f,%.1f", x, y, uid, sx, sy); return;
    }
    if (!strcmp(a->op, "box") && nt >= 1) {        /* box TARGET [PX]: drag-select a screen square of half-size PX
                                                      (default 50) around the target's screen point */
        int used = world_target(tok, nt, &x, &y, &uid);
        if (!used) { snprintf(result, n, "FAILED box target %s", a->args); return; }
        float half = used < nt ? (float)atof(tok[used]) : 50.0f;
        if (!world_screen(x, y, &sx, &sy)) { snprintf(result, n, "FAILED box %.2f,%.2f not on screen", x, y); return; }
        float l = sx - half, r = sx + half, tp = sy - half, btm = sy + half;
        if (l < 30) l = 30;
        if (tp < 45) tp = 45;
        if (r > win_w() - 30) r = win_w() - 30;
        if (btm > win_h() * 0.70f) btm = win_h() * 0.70f;
        drag(0, l, tp, r, btm, mods);
        snprintf(result, n, "box world %.2f,%.2f screen %.0f,%.0f-%.0f,%.0f", x, y, l, tp, r, btm); return;
    }

    if ((!strcmp(a->op, "minimap") || !strcmp(a->op, "rminimap")) && nt >= 1) {   /* left (camera) / right (send) */
        if (!world_target(tok, nt, &x, &y, &uid) || !minimap_point(x, y, &sx, &sy)) { snprintf(result, n, "FAILED minimap %s", a->args); return; }
        click_at(a->op[0] == 'r' ? 1 : 0, sx, sy, mods);
        snprintf(result, n, "minimap world %.1f,%.1f -> screen %.1f,%.1f", x, y, sx, sy); return;
    }
    if (!strcmp(a->op, "camera") && nt >= 1) {
        if (!world_target(tok, nt, &x, &y, &uid)) { snprintf(result, n, "FAILED camera %s", a->args); return; }
        float px = x - cam_dx, py = y - cam_dy;
        cam_before = screen_to_world(win_w() / 2, win_h() * 0.38f, &cam_bx, &cam_by);   /* the view centre before the move */
        if (!center_camera(px, py)) { snprintf(result, n, "FAILED camera %s", a->args); return; }
        cam_px = px; cam_py = py; cam_pending = 1;
        snprintf(result, n, "camera on %.1f,%.1f (aim %.1f,%.1f)", x, y, px, py); return;
    }
    if (!strcmp(a->op, "dump")) { dump_state(nt ? tok[0] : "dump"); return; }
    if (!strcmp(a->op, "type")) {   /* type TEXT: each character as the window procedure's WM_CHAR handler passes it
                                       (EEUserInterface WndProc -> gUIManager->UpdateTranslatedKey(&wParam, repeat 1)) */
        typedef long (TC *UTK)(void*, const char*, short);
        static UTK utk; if (!utk) utk = (UTK)GetProcAddress(GetModuleHandleA("Low-Level Engine.dll"), "?UpdateTranslatedKey@UIManager@@QAEJPBDF@Z");
        if (!utk) { snprintf(result, n, "FAILED UpdateTranslatedKey not exported"); return; }
        int typed = 0;
        for (const char* c = a->args; *c; c++) { char ch[4] = {*c == '_' ? ' ' : *c, 0, 0, 0}; utk(PTR(ui_manager()), ch, 1); frame(); typed++; }
        key_idle();
        snprintf(result, n, "typed %d chars on form %lu", typed, form_id(top_form())); return;
    }
    if (!strcmp(a->op, "recover")) {   /* close whatever form is over the main game form with its own cancel control
                                          (27 diplomacy: 186 cancel; 28 options: 0 return; 7 civ builder: 16 cancel;
                                          35 civ selection: 0 cancel; 29 chat log: a press outside it), else Escape;
                                          up to 4 rounds */
        char done[160] = ""; int round;
        for (round = 0; round < 4; round++) {
            DWORD id = form_id(top_form());
            if (id == 11) break;
            long ci = id == 27 ? 186 : id == 28 ? 0 : id == 7 ? 16 : id == 35 ? 0 : -1;
            if (id == 29) click_at(0, 3, win_h() / 2, NULL);   /* chat log: a press outside the form closes it */
            else if (ci >= 0 && control_center(control(top_form(), (DWORD)ci), &x, &y)) click_at(0, x, y, NULL); else do_key("esc");
            size_t len = strlen(done); snprintf(done + len, sizeof done - len, "%s%lu:%ld", len ? "," : "", id, ci);
            breathe(150);
        }
        snprintf(result, n, "recover %s -> top form %lu", done[0] ? done : "none", form_id(top_form())); return;
    }
    snprintf(result, n, "FAILED unknown action %s %s", a->op, a->args);
}

/* ---- the gate ---- */
static DWORD ct_lock(void) { return R32(server() + 0x168); }
/* enter the server's CT lock by spinning on TryEnter (a blocked Enter loses the hand-off to the send thread, which
   re-enters ~1/F ms after leaving), then once more through TSCriticalSection::Enter for its own bookkeeping */
static void ct_enter(void) { CRITICAL_SECTION* cs = (CRITICAL_SECTION*)PTR(ct_lock()); while (!TryEnterCriticalSection(cs)) YieldProcessor(); cs_enter(cs); ct_holding = 1; }
static void ct_leave(void) { CRITICAL_SECTION* cs = (CRITICAL_SECTION*)PTR(ct_lock()); ct_holding = 0; cs_leave(cs); LeaveCriticalSection(cs); }
static DWORD s_pt(void) { return R32(server() + 0x178); }
static DWORD s_ct(void) { return R32(server() + 0x190); }
static DWORD s_inputs(void) { return R32(server() + 0x84 + 0x10); }
static DWORD ui_actions(void) { return R32(R32(c_ui) + 0x04 + 0x10); }
static DWORD ph_done(void) { return R32(R32(c_physics) + 0xb4); }
static DWORD ph_left(void) { return R32(R32(c_physics) + 0xb8); }

/* the labels of the queued inputs [from, size) of the server's mIncomingPlayerInputs (Dinkum deque: _Map +4,
   _Mapsize +8, _Myoff +0xc, _Mysize +0x10; 1024 pointers per block); EEMessage::GetMessageLabel is slot +0x2c */
static void input_labels(DWORD from, char* out, size_t n)
{
    DWORD d = server() + 0x84, map = R32(d + 4), mapsize = R32(d + 8), off = R32(d + 0xc), size = R32(d + 0x10);
    out[0] = 0;
    typedef const char* (TC *Label)(void*);
    for (DWORD i = from; i < size && i < from + 8 && mapsize; i++) {
        DWORD k = off + i, block = (k / 1024) % mapsize, m = R32(R32(map + 4 * block) + 4 * (k % 1024));
        const char* l = m ? ((Label)VT(PTR(m), 0x2c))(PTR(m)) : "?";
        size_t len = strlen(out); snprintf(out + len, n - len, "%s%s(%u)", len ? "," : "", l ? l : "?", m ? R8(m + 8) : 0);
    }
}

/* EEPhysics::ReportStatistics (called at the top of every physics timer tick, also while no PT runs) posts
   EEMReportStatistics through ProcessLocalUserInput when the local player's statistics 0x14/0x15 changed and
   TMGetTime() - sLastReport > sReportPeriod (srand(TMGetTime()): 2.5..5 game-clock s). A human's commands change those
   statistics, so the report is a WALL-TIME input. To keep it on the action's CT: act right after a report check
   refreshed sLastReport (so no refresh can fall inside the action), then keep the gate closed until the next refresh,
   which posts the report (if the values changed) into the same CT. Only reads the two statics and TMGetTime. */
static DWORD report_guard = 600;   /* game-clock ms an action may take (grows with the measured maximum) */
static int report_wait_refresh(DWORD timeout_ms)
{
    DWORD l0 = R32(c_last_report), t0 = GetTickCount();
    while (R32(c_last_report) == l0) { if (GetTickCount() - t0 > timeout_ms) return 0; Sleep(0); }
    return 1;
}
static void report_align_pre(void)
{
    if (!c_last_report || !tm_get_time) return;
    DWORD el = tm_get_time() - R32(c_last_report), per = R32(c_report_period);
    if (per && el + report_guard >= per && !report_wait_refresh(20000)) note("  WARNING report refresh not seen (pre)");
}

static int game_running(void)
{
    DWORD s = server();
    return s && R8(s + 0x174) == 3 && R32(c_physics) && find_form(11) && R32(c_world + 0xbc);
}

static void wait_physics_caught_up(void)
{
    DWORD last = ph_done(), stable = 0;
    for (int i = 0; i < 20000; i++) {   /* <= ~20 s */
        Sleep(1);
        DWORD now = ph_done();
        if (now == last && ph_left() == 0) { if (++stable >= 30) return; } else stable = 0;
        last = now;
    }
    note("  WARNING physics did not settle (done %lu left %lu)", ph_done(), ph_left());
}

/* diagnostic (read-only): name the queued inputs that are not the script's (wall-time inputs of the game or the
   harness), from the game start on; physics = the last PT executed */
static void stray_poll(void)
{
    static char last[400];
    if (!server() || !s_inputs()) return;
    char l[400]; input_labels(0, l, sizeof l);
    if (strcmp(l, last)) { note("  stray input(s) queued at server PT %lu CT %lu physics %lu: %s", s_pt(), s_ct(), R32(c_physics) ? ph_done() : 0, l); snprintf(last, sizeof last, "%s", l); }
}

/* ---- lockstep (opt-in, human.cfg `lockstep 1`; timing only, the same CT gate as around every action): from the game
   start the driver holds the server's CT lock and lets it send the next CT only when physics has executed every PT
   already sent. An input the game posts itself while a PT executes (EEMReplantFarms when any player's granary
   completes: EEComplexUnit::SetBuilt / EEMakeBuilding) then rides the next CT in every run, instead of the CT the
   wall-clock send thread happens to be at (a later-epoch start diverged at the first replant). ---- */
static DWORD ls_steps, ls_skips, ls_on;
/* physics' PT counter (mUB4) minus the server's PT when physics has caught up: 0 in a random-map game; a scenario
   (or a loaded game) continues the file's physics PT while the server counts from its own start */
static long ls_off;
static int ls_step(void)   /* one CT when physics has caught up; 0 if the server is gone */
{
    if (!server()) return 0;
    if ((long)(ph_done() - s_pt()) < ls_off) { YieldProcessor(); return 1; }
    DWORD ct0 = s_ct(), t0 = GetTickCount();
    ct_leave();
    /* pure spin (no Sleep): a send thread behind schedule sends CTs back to back, re-entering ~1/F ms after each */
    for (DWORD spin = 0; s_ct() == ct0; spin++) {
        if (!server()) return 0;
        if ((spin & 1023) == 0 && GetTickCount() - t0 > 50) break;   /* paused / not sending: take the lock back */
    }
    ct_enter();
    if (s_ct() != ct0) {
        ls_steps++;
        if (s_ct() != ct0 + 1 && ++ls_skips <= 40)
            note("  lockstep: %lu CTs sent in one release (CT %lu -> %lu, server PT %lu, physics %lu)", s_ct() - ct0, ct0, s_ct(), s_pt(), ph_done());
    }
    return 1;
}
static void ls_wait(DWORD ms)   /* wait ms of wall time, stepping in lockstep meanwhile */
{
    for (DWORD t0 = GetTickCount(); GetTickCount() - t0 < ms; ) { stray_poll(); if (ls_on) { if (!ls_step()) return; } else Sleep(1); }
}

/* one CT through the gate with no physics condition (a CT before the game runs carries no PTs); 0 if the server is
   gone. The same hand-off as ls_step. */
static int ls_free_step(void)
{
    if (!server()) return 0;
    DWORD ct0 = s_ct(), t0 = GetTickCount();
    ct_leave();
    for (DWORD spin = 0; s_ct() == ct0; spin++) {
        if (!server()) return 0;
        if ((spin & 1023) == 0 && GetTickCount() - t0 > 50) break;
    }
    ct_enter();
    if (s_ct() != ct0 + 1 && s_ct() != ct0) note("  lockstep arm: %lu CTs sent in one release (CT %lu -> %lu, server PT %lu)", s_ct() - ct0, ct0, s_ct(), s_pt());
    return 1;
}

/* wait (holding the lock, so no further CT can be sent) until physics has executed the CT just released: its PT
   counter moved off `from` and nothing is left to run (stable 30 ms); 0 after timeout_ms */
static int ls_wait_first_ct(DWORD from, DWORD timeout_ms)
{
    DWORD t0 = GetTickCount(), last = ph_done(), stable = 0;
    while (GetTickCount() - t0 < timeout_ms) {
        Sleep(1);
        DWORD now = ph_done();
        if (now != from && now == last && ph_left() == 0) { if (++stable >= 30) return 1; } else stable = 0;
        last = now;
    }
    return 0;
}

/* lockstep from the first game PT (2026-11-03; was: engaged when the polling driver noticed the running game, at
   server PT 294..826 under box load, so a replant posted before that rode a wall-time CT). Arm before the server
   runs: hold the CT lock once the game's server exists in the closed (1) or running (3) state with the world's map
   loaded (the lobby server is replaced before that; the map comes with the game's own server), and let CTs through
   one at a time while they carry no PTs (state != running: mCurrentPhysicsTick only advances in SendCommunications-
   Tick's running branch). The first CT that does carry PTs is then the only one sent until physics has executed it;
   the offset (physics PT - server PT: 0 in a random map, a scenario continues the file's physics PT) is measured
   there and the usual gate (ls_step) takes over: every PT CT of the game is sent only after physics caught up. */
static void ls_arm(void)
{
    for (DWORD last = GetTickCount(); ; Sleep(1)) {
        DWORD s = server(); BYTE st = s ? R8(s + 0x174) : 0;
        if (s && (st == 1 || st == 3) && R32(c_world + 0xbc)) break;
        if (GetTickCount() - last > 2000) {
            last = GetTickCount();
            note("waiting (lockstep arm): server %08lx state %u physics %08lx map %08lx top %lu", s, st, R32(c_physics), R32(c_world + 0xbc), form_id(top_form()));
        }
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    ct_enter();
    DWORD pt0 = s_pt(), ph0 = ph_done(), free_cts = 0;
    note("lockstep armed: server %08lx state %u PT %lu CT %lu physics %lu", server(), R8(server() + 0x174), pt0, s_ct(), ph0);
    while (s_pt() == pt0) {   /* CTs without PTs (lobby/closed/paused) pass freely, one per release */
        DWORD c0 = s_ct();
        if (!ls_free_step()) { note("server gone while arming"); return; }
        if (s_ct() != c0) free_cts++;
    }
    DWORD first_pt = s_pt(), first_ct = s_ct();
    /* physics normally runs the first PT CT within ms; should it ever need another one, release CTs one at a time
       (logged: the run's lockstep then started late) */
    DWORD extra = 0;
    while (!ls_wait_first_ct(ph0, 15000)) {
        note("  WARNING lockstep arm: physics still at %lu (left %lu) 15 s after the first PT CT; releasing one more CT", ph_done(), ph_left());
        if (!ls_free_step()) { note("server gone while arming"); return; }
        extra++;
    }
    ls_off = (long)(ph_done() - s_pt()); ls_on = 1;
    note("lockstep on: server PT %lu CT %lu physics %lu (offset %ld; first PT CT %lu -> server PT %lu after %lu PT-less CTs%s)",
         s_pt(), s_ct(), ph_done(), ls_off, first_ct - 1, first_pt, free_cts, extra ? ", LATE: extra CTs released" : "");
}

/* wait 1 ms, or one lockstep step when the gate is on */
static int idle_step(void) { if (ls_on) return ls_step(); Sleep(1); return 1; }

static DWORD WINAPI human_main(LPVOID unused)
{
    (void)unused;
    if (c_lockstep) ls_arm();
    for (DWORD last = GetTickCount(); !game_running(); ) {
        if (!idle_step()) { note("server gone; stopping"); return 0; }
        if (server() && R8(server() + 0x174) == 3) stray_poll();
        if (GetTickCount() - last > 2000) {
            last = GetTickCount(); DWORD s = server();
            note("waiting: server %08lx state %u physics %08lx form11 %08lx map %08lx top %lu", s, s ? R8(s + 0x174) : 0,
                 R32(c_physics), find_form(11), R32(c_world + 0xbc), form_id(top_form()));
        }
    }
    /* ai_game's civilization-builder preempt (a wall-time UI action at game start) sets the local player's mBB4A:
       start only after it, so the two never interleave (--no-civ-preempt: after 120 s) */
    for (DWORD t0 = GetTickCount(); GetTickCount() - t0 < 120000 && !c_no_civ_wait && !R8(player(local_index()) + 0xb4a); ) ls_wait(1);
    for (DWORD t0 = GetTickCount(); form_id(top_form()) != 11 && GetTickCount() - t0 < 30000; ) ls_wait(1);
    ls_wait(1000);
    note("game running: server %08lx state %u PT %lu CT %lu PTs/CT %u; physics done %lu; local player %lu; %d actions",
         server(), R8(server() + 0x174), s_pt(), s_ct(), R8(server() + 0x1f9), ph_done(), local_index(), nacts);
    note("start: server PT %lu, local mBB4A %u, top form %lu", s_pt(), R8(player(local_index()) + 0xb4a), form_id(top_form()));
    dump_state("start");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    driver_tid = GetCurrentThreadId();
    { HANDLE w = CreateThread(NULL, 0, watchdog, NULL, 0, NULL); if (w) CloseHandle(w); }
    int holding = ls_on;
    /* an action whose PT had passed before the driver could start (the game start, the civ preempt) cannot be pinned:
       it is DROPPED, not performed late (a late action lands on a wall-time-dependent PT); start scripts later */
    DWORD start_pt = s_pt() + 3 * (R8(server() + 0x1f9) ? R8(server() + 0x1f9) : 14) + 4;
    for (int k = 0; k < nacts; k++) {
        Action* a = &acts[k];
        if (a->pt < start_pt) { note("DROPPED act %d line %d PT %lu %s %s: the driver started at server PT %lu", k, a->line, a->pt, a->op, a->args, start_pt); continue; }
        DWORD margin = c_margin ? c_margin : 60;
        if (!holding) {
            for (;;) {
                if (!server()) { note("server gone; stopping"); return 0; }
                /* few steps under the lock (a send thread that is behind schedule re-enters ~1/F ms after a send,
                   and can then win the lock: SKIPPED-CT): approach with a margin of 3 CTs, polling without sleep
                   once within 24 CTs (a catching-up server sends CTs back to back) */
                DWORD ptsct = R8(server() + 0x1f9); if (!c_margin) margin = 3 * (ptsct ? ptsct : 4) + 4;
                if (game_running() && s_pt() + margin >= a->pt) break;
                if (game_running() && s_pt() + 8 * margin >= a->pt) { YieldProcessor(); continue; }
                stray_poll();   /* also before the first action (diagnostic: inputs that are not the script's) */
                Sleep(1);
            }
            ct_enter(); holding = 1;
        }
        DWORD steps = 0, skips = 0;
        PHASE("ct step");
        while (ls_on && s_pt() < a->pt) { DWORD c0 = s_ct(); if (!ls_step()) { note("server gone; stopping"); return 0; } if (s_ct() != c0) steps++; stray_poll(); }
        while (s_pt() < a->pt) {
            DWORD ct0 = s_ct();
            ct_leave();
            for (DWORD spin = 0; s_ct() == ct0; spin++) { if (!server()) { note("server gone; stopping"); return 0; } if (spin > 2000) Sleep(0); }
            ct_enter();
            if (s_ct() != ct0 + 1) skips++;
            steps++;
        }
        DWORD at_pt = s_pt(), at_ct = s_ct(), ptsct = R8(server() + 0x1f9);
        wait_physics_caught_up();
        DWORD ph = ph_done();
        Sleep(c_settle_ms);                       /* the UI's own periodic refresh (status box, selection display) */
        report_align_pre();
        DWORD lr_pre = c_last_report ? R32(c_last_report) : 0, tm_pre = tm_get_time ? tm_get_time() : 0;
        DWORD in0 = s_inputs();
        lock_input();
        char result[300]; DWORD sel0 = selected_count();
        PHASE(a->op);
        perform(a, result, sizeof result);
        PHASE("unlock");
        unlock_input();
        PHASE("ui queue drain");
        for (int i = 0, quiet = 0; i < 3000 && quiet < 20; i++) { Sleep(1); if (ui_actions() == 0) quiet++; else quiet = 0; }
        Sleep(5);
        int race = 0;
        if (c_last_report && strcmp(a->op, "dump")) {
            DWORD took = tm_get_time ? tm_get_time() - tm_pre : 0;
            if (2 * took + 200 > report_guard) report_guard = 2 * took + 200;
            race = R32(c_last_report) != lr_pre;   /* a report check fired during the action: its values may be partial */
            if (!report_wait_refresh(20000)) note("  WARNING report refresh not seen (post)");
            Sleep(5);
        }
        DWORD in1 = s_inputs();
        char labels[400]; input_labels(in0 < in1 ? in0 : in1, labels, sizeof labels);
        DWORD next = k + 1 < nacts ? acts[k + 1].pt : 0xffffffff;
        if (!ls_on && next > s_pt() + margin) { ct_leave(); holding = 0; }
        note("act %d line %d PT %lu %s %s | server PT %lu CT %lu PTs/CT %lu physics %lu stepped %lu%s | inputs +%ld | selected %lu | %s | queued %s",
             k, a->line, a->pt, a->op, a->args, at_pt, at_ct, ptsct, ph, steps, skips ? " SKIPPED-CT" : race ? " REPORT-RACE" : "",
             (long)in1 - (long)in0, sel0, result, labels);
        if (at_pt >= a->pt + (ptsct ? ptsct : 1)) note("  LATE: the scripted PT %lu had passed (server PT %lu)", a->pt, at_pt);
        phase_t = 0;   /* between actions: the watchdog stays quiet */
    }
    if (ls_on) {   /* keep the lockstep to the end of the game (the session stops the process) */
        note("script complete; lockstep continues (steps %lu, multi-CT sends %lu)", ls_steps, ls_skips);
        for (DWORD last = GetTickCount(); ls_step(); ) { stray_poll(); if (GetTickCount() - last > 30000) { last = GetTickCount(); note("lockstep: server PT %lu steps %lu multi-CT sends %lu", s_pt(), ls_steps, ls_skips); } }
        return 0;
    }
    if (holding) ct_leave();
    note("script complete");
    Sleep(1000);
    dump_state("end");
    return 0;
}

static int load_config(void)
{
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\human.cfg", stage_dir);
    FILE* f = fopen(path, "r"); if (!f) return 0;
    char line[300];
    while (fgets(line, sizeof line, f)) {

        char key[40] = {0}; unsigned long v = 0;
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "act ", 4) && nacts < 512) {
            Action* a = &acts[nacts]; int used = 0;
            if (sscanf(line + 4, "%d %lu %23s %n", &a->line, &a->pt, a->op, &used) >= 3) {
                snprintf(a->args, sizeof a->args, "%s", line + 4 + used); nacts++;
            }
            continue;
        }
        if (sscanf(line, "%39s %lx", key, &v) != 2) continue;
#define K(n, var) if (!strcmp(key, n)) var = (DWORD)v;
        K("ui_manager", c_ui_manager) K("world", c_world) K("communications", c_comm) K("user_interface", c_ui)
        K("mouse", c_mouse) K("physics", c_physics) K("screen_to_world", c_s2w) K("minimap_transformation", c_minitf)
        K("game_window", c_gamewindow) K("vt_minimap", c_vt_minimap) K("vt_status_button", c_vt_status)
        K("vt_select_units", c_vt_select) K("vt_command_units", c_vt_command) K("settle_ms", c_settle_ms)
        K("margin", c_margin) K("last_report", c_last_report) K("report_period", c_report_period)
        K("no_civ_wait", c_no_civ_wait) K("lockstep", c_lockstep)
#undef K
    }
    fclose(f);
    return c_ui_manager && c_world && c_comm && c_ui && c_mouse && c_physics && c_s2w && c_minitf && c_gamewindow;
}

__declspec(dllexport) int TestHumanStart(void)
{
    static int started;
    if (started) return 1;
    started = 1;
    HMODULE engine = GetModuleHandleA("Low-Level Engine.dll");
    cs_enter = (Fv)GetProcAddress(engine, "?Enter@TSCriticalSection@@QAEXXZ");
    cs_leave = (Fv)GetProcAddress(engine, "?Leave@TSCriticalSection@@QAEXXZ");
    rw_rlock = (Fv)GetProcAddress(engine, "?ReadLock@TSReadWriteLock@@QAEXXZ");
    rw_runlock = (Fv)GetProcAddress(engine, "?ReadUnlock@TSReadWriteLock@@QAEXXZ");
    get_trigger = (GetTrigger)GetProcAddress(engine, "?GetTriggerEvent@UIControl@@QAEPAVUIEvent@@K@Z");
    tm_get_time = (TMGetTimeFn)GetProcAddress(engine, "?TMGetTime@@YAKXZ");
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\human.log", stage_dir);
    hlog = fopen(path, "a");
    if (!cs_enter || !cs_leave || !rw_rlock || !rw_runlock) { note("engine exports missing"); return 0; }
    if (!load_config()) { note("human.cfg incomplete"); return 0; }
    note("loaded: %d actions; ui_manager %08lx world %08lx comm %08lx s2w %08lx", nacts, c_ui_manager, c_world, c_comm, c_s2w);
    HANDLE t = CreateThread(NULL, 0, human_main, NULL, 0, NULL);
    if (t) CloseHandle(t);
    return t != NULL;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        self = module; DisableThreadLibraryCalls(module);
        GetModuleFileNameA(module, stage_dir, sizeof stage_dir);
        char* tail = strrchr(stage_dir, '\\'); if (tail) *tail = 0;
    }
    return TRUE;
}
