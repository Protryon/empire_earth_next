/* Guarded boot check for EE-AOC.exe builds (original vs relinked), safe on this box.
 *
 *   exe_boot.exe <dir> <exe name> <oep|winmain|run> <timeout ms> [<dump file>]
 *
 * Runs <dir>\<exe> under a debugger with <dir> as the working directory and never lets it make a persistent change:
 * int3 guards sit on the entry of every API through which a write could leave the process (files, registry,
 * processes, windows/display, sockets, system parameters). Read-only file/key opens are logged and allowed (the
 * guard steps over them); anything else stops the run: the call is reported (name, arguments, caller) and the
 * process is killed before the call executes. The game install is never written: the only files the process may
 * open are opened read-only.
 *
 *   oep      stop at the entry point (all imports bound, every DLL initialised; no exe code has run). Dumps the IAT
 *            as module+offset per slot.
 *   winmain  stop at WinMain (0x140d73: mainCRTStartup has run every static initializer, __xi/__xc). Dumps the IAT
 *            and the whole .data section (incl. .bss) to <dump file>.
 *   run      no stop address: run until the first guard, a crash, exit, or the timeout. At the end the target's
 *            windows are listed and the largest visible one is captured to <dump file> (.bmp).
 * The log (stdout) prints addresses as module+offset so two runs from different directories can be diffed.
 *
 * Policy extended by owner decision (2026-10-23, "Registry + window"):
 *   - registry writes are allowed only below HKCU\Software\Mad Doc Software\EE-AOC (creating the Mad Doc Software
 *     key itself is allowed); tools/boot/exe_boot.py snapshots that subtree before every run and restores it after.
 *     A read-only NtCreateKey of an existing key elsewhere is an open and allowed; any other key write stops.
 *   - windows and DirectDraw/Direct3D initialisation are allowed; STOP on display-mode changes (ChangeDisplaySettings*,
 *     IDirectDraw*::SetDisplayMode), IDirectDraw*::SetCooperativeLevel with DDSCL_EXCLUSIVE or DDSCL_FULLSCREEN,
 *     gamma ramps, networking, process creation, file writes outside the staging directory.
 *   - the process is killed at the first STOP or at the timeout; the exit code is 3 if any window of it survives.
 *
 * Built as an x86_64 program (tools/harness/exe_boot.py, mingw64 gcc) for the x86_64 game: the harness is native to
 * its target, so every Windows structure it reads from the target (UNICODE_STRING, OBJECT_ATTRIBUTES, PE headers,
 * DDSURFACEDESC2) has the harness's own layout. The calling convention at a guarded entry is the "arch layer" below:
 * the first four arguments in RCX, RDX, R8, R9 and the rest above the 32-byte home area; the caller pops.
 */
#ifndef _WIN64
#error "exe_boot.c is the x86_64 harness: build it with mingw64 gcc (tools/harness/exe_boot.py)"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <windows.h>
#include <dbghelp.h>
#include <ddraw.h>
#include <d3d.h>

/* ---- arch layer: target addresses, registers and arguments at a function entry ---- */
typedef ULONG_PTR TADDR;   /* a target address (the harness is native to its target) */
#define PTR 8
#define AFMT "%016llx"
#define AV(x) ((unsigned long long)(TADDR)(x))
#define R_IP(c) ((c)->Rip)
#define R_SP(c) ((c)->Rsp)
#define R_AX(c) ((c)->Rax)
#define R_THIS(c) ((c)->Rcx)   /* member functions: this is the first argument */
#define R_FP(c) ((c)->Rbp)
#define ARCH_NAME "x64"
#define KEEP_RESULT ((TADDR)0x5eed5eed)   /* fake_return: leave the result register as is */
static BOOL rd(TADDR a, void *p, SIZE_T n);
static TADDR rdp(TADDR a);
static BOOL wrp(TADDR a, TADDR v);
/* integer/pointer argument n (1-based) of a function at its entry (the return address at [sp]): RCX, RDX, R8, R9,
   then [rsp+8n] (above the home area) */
static TADDR arg_n(const CONTEXT *c, int n)
{
    switch (n) { case 1: return c->Rcx; case 2: return c->Rdx; case 3: return c->R8; case 4: return c->R9; }
    return rdp(R_SP(c) + (TADDR)PTR * n);
}
/* replace argument n before the call runs (a register: applied by the caller's SetThreadContext) */
static BOOL set_arg(CONTEXT *c, int n, TADDR v)
{
    switch (n) { case 1: c->Rcx = v; return TRUE; case 2: c->Rdx = v; return TRUE;
                 case 3: c->R8 = v; return TRUE; case 4: c->R9 = v; return TRUE; }
    return wrp(R_SP(c) + (TADDR)PTR * n, v);
}
/* argument n (1-based, after this) of a member function: this in rcx, the rest from rdx */
static TADDR marg_n(const CONTEXT *c, int n)
{
    return arg_n(c, n + 1);
}
/* return to the caller from the entry of a function (nargs: its argument count; x64 callees pop nothing). result: the
   return register (KEEP_RESULT: unchanged) */
static void fake_return(CONTEXT *c, int nargs, TADDR result)
{
    R_IP(c) = rdp(R_SP(c));
    (void)nargs;
    R_SP(c) += 8;
    if (result != KEEP_RESULT) R_AX(c) = result;
}

#define MAXMOD 128
#define MAXGUARD 256
static struct { TADDR base; DWORD size; char name[64]; } mods[MAXMOD];
static int nmods;
typedef enum { G_STOP, G_FILE_CREATE, G_FILE_OPEN, G_SETINFO, G_SPI, G_MSGBOX_A, G_MSGBOX_W, G_WINDOW,
               G_REG_CREATE, G_REG_HANDLE, G_DD_COOP, G_DD_CREATE, G_DD_CREATE_EX, G_DD_QUERY, G_DD_RETURN,
               G_DD_SURFACE, G_D3D_DEVICE, G_RENDER_ACTIVATE, G_RENDER_PRESENT,
               G_DEVICE_CONTROL, G_WRITE_FILE, G_AUDIO_INITIALIZE, G_EXPERIMENTAL_ENTRY, G_NATIVE_ERROR,
               G_NET_SOCKET, G_NET_ADDRESS, G_NET_LISTEN, G_NET_CLOSE, G_NET_IOCTL, G_SEED_PIN, G_RNG_TRACE,
               G_MUTEX_BYPASS, G_REG_OPEN, G_REG_OPEN_EX, G_PT_SYNC, G_PT_WATCH, G_NHD_OUT, G_NHD_RESULT, G_MP_SKIP, G_MP_ACCEPT, G_MP_CHECKSUM } gkind;

/* ---- registry policy (owner decision 2026-10-23): writes only below HKCU\Software\Mad Doc Software\EE-AOC (the
   Mad Doc Software key itself may be created); key paths are resolved from the target's handles */
typedef struct { USHORT Length, MaximumLength; PWSTR Buffer; } USTR;
typedef LONG (WINAPI *NtQueryObject_t)(HANDLE, int, PVOID, ULONG, PULONG);
typedef struct { ULONG Length; HANDLE RootDirectory; USTR *ObjectName; ULONG Attributes; PVOID sd, sqos; } OBJATTR;
typedef LONG (WINAPI *NtOpenKey_t)(PHANDLE, ACCESS_MASK, OBJATTR *);
static char regMadDoc[600];   /* "\REGISTRY\USER\<sid>\Software\Mad Doc Software" */
static char regUser[600];
/* the policy's root key (HKCU-relative) and the game's key below it. --reg-root (TEST ONLY: the harness's own self
   test, tools/harness/selftest) moves both to another HKCU root so the policy can be exercised without touching the
   game's key; the game sessions never pass it */
static char regRootRel[200] = "Software\\Mad Doc Software";
static char regGameRel[220] = "Software\\Mad Doc Software\\EE-AOC";
static void reg_private(char *out, size_t n, const char *sub) { snprintf(out, n, "%s\\%s", regGameRel, sub); }

static void local_handle_name(HANDLE h, char *out, size_t n)
{
    static NtQueryObject_t q;
    BYTE buf[4096]; ULONG len; int k;
    USTR *u = (USTR *)buf;
    out[0] = 0;
    if (!q) q = (NtQueryObject_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryObject");
    if (q(h, 1 /* ObjectNameInformation */, buf, sizeof buf, &len) < 0) return;
    for (k = 0; k < u->Length / 2 && k < (int)n - 1; k++) out[k] = u->Buffer[k] < 128 ? (char)u->Buffer[k] : '?';
    out[k] = 0;
}

static void reg_init(void)
{
    HKEY hk;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        local_handle_name((HANDLE)hk, regMadDoc, sizeof regMadDoc - 32);
        snprintf(regUser,sizeof regUser,"%s",regMadDoc);
        if(strlen(regUser)>=9)regUser[strlen(regUser)-9]=0; /* remove \\Software */
        strcat(regMadDoc, regRootRel + 8);   /* "\\Mad Doc Software" (the root below Software) */
        RegCloseKey(hk);
    }
}

/* 2: inside ...\Mad Doc Software\EE-AOC (or the key itself), 1: the Mad Doc Software key, 0: elsewhere */
static int reg_class(const char *p)
{
    size_t n = strlen(regMadDoc);
    if (!regMadDoc[0] || _strnicmp(p, regMadDoc, n)) return 0;
    if (!p[n]) return 1;
    if (!_strnicmp(p + n, "\\EE-AOC", 7) && (p[n + 7] == 0 || p[n + 7] == '\\')) return 2;
    return 0;
}

/* does the native key path exist? (opened read-only by the harness, then closed) */
static int reg_exists(const char *p)
{
    static NtOpenKey_t ok;
    WCHAR w[600]; USTR u; OBJATTR oa = {sizeof oa}; HANDLE h; int k;
    if (!ok) ok = (NtOpenKey_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenKey");
    for (k = 0; p[k] && k < 599; k++) w[k] = (BYTE)p[k];
    w[k] = 0;
    u.Length = u.MaximumLength = (USHORT)(2 * k); u.Buffer = w;
    oa.ObjectName = &u; oa.Attributes = 0x40 /* OBJ_CASE_INSENSITIVE */;
    if (ok(&h, KEY_READ, &oa) < 0) return 0;
    CloseHandle(h);
    return 1;
}
#define KEY_WRITEISH (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK | DELETE | WRITE_DAC | WRITE_OWNER | \
                      GENERIC_WRITE | GENERIC_ALL | MAXIMUM_ALLOWED)
static struct { TADDR addr; BYTE orig; const char *name; gkind kind; } guards[MAXGUARD];
static int nguards;
static int windowed_test; /* opt-in staged build overlay; default boot policy is unchanged */
static int graphics_checkpoint;
static int audio_checkpoint;
static int live_session, frame_presented;
/* --audio-entry RVA: a relaid (full-link) image's EEInitializeSounds, located by fulllink_locator.py; the
   default is the original layout's. --winmain-rva likewise for the winmain mode. */
static DWORD audio_entry_rva = 0x143557;
/* the renderer's checkpoints (DX7Rasterizer::ActivateScreen / ShowScene); defaults: the retail DX7HRDisplay.dll.
   The fork's harness passes its build's own RVAs (--renderer-guards). */
static DWORD renderer_activate_rva = 0x9ab4, renderer_present_rva = 0xc99e;
/* --static-exe NATIVE_ERROR ABORT GENERATE NEXT (exe RVAs, hex): the fork's one-image build. The engine, the renderer
   and the C runtime are inside the exe, so the guards the DLL builds armed when those modules loaded (the engine's
   ESRecordNativeError and URandom exports, the runtime's abort, the renderer checkpoints of --renderer-guards) are
   armed in the exe at process creation instead. */
static int static_exe;
static DWORD static_native_error_rva, static_abort_rva, static_generate_rva, static_next_rva;
/* --stack-scan N (opt-in triage): after the 12 stack slots, list every EXE-module value among the next N
   dwords (likely return addresses) and the ebp chain; default 0 keeps the log unchanged. */
static int stack_scan;
static int av_dumped;
static int loopback_test;
static struct {DWORD tid;int depth;} net_creating[32];static int nnet_creating;
static struct {TADDR fd;int bound;} net_sockets[128];static int nnet_sockets;   /* SOCKET: pointer-sized */
static int net_thread(DWORD tid) {for(int i=0;i<nnet_creating;i++)if(net_creating[i].tid==tid)return i;return -1;}
static int net_socket(TADDR fd) {for(int i=0;i<nnet_sockets;i++)if(net_sockets[i].fd==fd)return i;return -1;}
static DWORD experimental_entries[160];   /* at most 8 unless --trace-once (round 36) */
static int nexperimental_entries;
/* TEST ONLY (round 36, owner-approved 2026-11-01): --seed-pin RVA OFFSET VALUE writes VALUE into [ecx+OFFSET] at
   every entry of the function at RVA (EEMStartLoading::Execute: the lobby's clock-derived start seed) and logs
   address/old/new; off unless requested */
static DWORD seed_pin_rva, seed_pin_offset, seed_pin_value;static int seed_pin;
/* TEST ONLY (round 39, owner-approved 2026-11-01): --mutex-bypass RVA guards the `test eax,eax` right after
   EECreateInstanceMutex's GetLastError call; ERROR_ALREADY_EXISTS (183) there is turned into 0 so a parallel test
   instance proceeds (the real mutex handle is kept); every hit is logged; the image is not patched */
static DWORD mutex_bypass_rva;
/* TEST ONLY (round 39, owner-approved 2026-11-01): --no-human-defeat OUT_RVA RESULT_RVA WORLD_VA PLAYERS_OFF CURRENT_OFF
   keeps the local (idle human) player from being marked out or defeated, so 4-AI games run to an AI-vs-AI finish.
   OUT is EEWorld::Fn1391F2(bool victorious, player) (thiscall, ret 8: marks the player out); a call with player ==
   gWorld.mCurrentPlayer and victorious false returns at its entry. RESULT is EEPlayer::Fn16AF5C(state) (ret 4: the
   writer of the result +0xa28); a call on gWorld.mPlayers[mCurrentPlayer] with state 2 returns at its entry. Every hit
   is logged (skips and passes); the image is not patched (tools/boot/no_human_defeat.py locates and checks both bodies) */
static DWORD nhd_out_rva, nhd_result_rva, nhd_players_off, nhd_current_off; static TADDR nhd_world;
/* TEST ONLY (round 40, agent mp; opt-in): --nhd-players MASK makes --no-human-defeat protect the players whose ids
   are set in MASK (every human of a multiplayer game, the same set in every process) instead of the local player.
   --mp-skip RVA returns at the entry of a void cdecl function without arguments (EEStartExternalLobby: the external
   WON lobby is not opened; every hit logged; image unpatched). --mp-loopback SELF PEER (last octets): loopback
   binds go to 127.0.0.SELF, every other destination except 127.0.0.SELF to 127.0.0.PEER (two processes of one
   game on one box, same ports). --mp-peers A,B,.. (final campaign, lane mp; opt-in): more peers of a 3-4 process
   game; a destination 127.0.0.A (one of them) keeps its address (the host answers every client at its own address,
   a migrated host is reached at its own), every other non-self destination still goes to 127.0.0.PEER. */
static DWORD nhd_players_mask, mp_skip_rva, mp_self_ip, mp_peer_ip, mp_more_ip[8]; static int nmp_more_ip;
/* TEST ONLY (round 40, agent mpengine; owner-approved 2026-11-02; opt-in): --mp-checksum RVA DELTA guards the
   `mov [gBinaryChecksum], eax` store in EEInitializeNetwork and adds DELTA to eax there (the byte-sum difference
   between the original engine DLL and the staged one), so a process running our engine computes the original's
   gBinaryChecksum and gGameVersion suffix and the other end's version check admits it; every hit logged; image unpatched */
static DWORD mp_checksum_rva, mp_checksum_delta;

/* TEST ONLY (round 39): --registry-subtree NAME redirects the game's own key HKCU\Software\Mad Doc Software\EE-AOC
   (and everything below it except the harness's "windowed-test *" bookkeeping) to EE-AOC\NAME, so parallel instances
   never share preferences; NtCreateKey/NtOpenKey/NtOpenKeyEx are answered with real handles to the redirected key */
static char reg_subtree[200];
static int trace_once;   /* --trace-once (TEST ONLY, round 36): log each experimental candidate entry once, then drop its guard */
/* TEST ONLY (round 36): --rng-trace GPHYSICS_VA logs every engine URandom::Generate/NextInSeries call on
   gPhysics->mRandom (+0x18): seed before the call, return address and the caller frame's return address; read-only */
static TADDR rng_trace_physics, rng_trace_entries[2];static int rng_trace_armed;
static DWORD rng_trace_offset = 0x18;   /* --rng-offset: EEPhysics::mRandom in the build's layout (VC6: +0x18) */
/* --image-base HEX: the image base the absolute VAs of --rng-trace and --no-human-defeat refer to (the map's preferred
   load address); they are moved by the actual base minus it at process creation (an ASLR exe). Off: used as given. */
static TADDR va_base; static int va_base_set;
/* the trace is armed only when <stage>/rng.arm appears (in game; map generation would be far too slow) */
static int game_windowed_capture;
static struct {TADDR handle;char name[600];} input_handles[32];static int ninput_handles;
static HANDLE hProc;
static TADDR exeBase, stopAddr, rearm;   /* rearm: guard address to re-insert after a single step */
static int nexc;
static HANDLE threads[256]; static DWORD tids[256]; static int held[256]; static int nth;
static char sandbox[MAX_PATH + 8];   /* "\??\<staging dir>\\": writes below it are allowed */
static int in_sandbox(const char *p) { return sandbox[0] && !_strnicmp(p, sandbox, strlen(sandbox)) && !strstr(p, ".."); }
/* below %LOCALAPPDATA%\D3DSCache (the D3D12 shader cache): refused with ACCESS_DENIED instead of a STOP */
static int shader_cache(const char *p) {
    static const char kCache[] = "\\appdata\\local\\d3dscache";
    for (; *p; p++) if (!_strnicmp(p, kCache, sizeof kCache - 1)) return 1;
    return 0;
}
static DWORD stepper;   /* the thread stepping over a guard; every other thread is suspended meanwhile */

static void hold_others(DWORD tid, int suspend)
{
    int k;
    for (k = 0; k < nth; k++)
        if (tids[k] != tid) {
            if(suspend && !held[k]) {if(SuspendThread(threads[k])!=(DWORD)-1)held[k]=1;}
            else if(!suspend && held[k]) {ResumeThread(threads[k]);held[k]=0;}
        }
}

static void mod_of(TADDR a, char *buf, size_t n)
{
    int i;
    for (i = 0; i < nmods; i++)
        if (a >= mods[i].base && a < mods[i].base + mods[i].size) {
            snprintf(buf, n, "%s+%lx", mods[i].name, (unsigned long)(a - mods[i].base));
            return;
        }
    snprintf(buf, n, AFMT, AV(a));
}

static BOOL rd(TADDR a, void *p, SIZE_T n) { SIZE_T got; return ReadProcessMemory(hProc, (void *)a, p, n, &got) && got == n; }
static DWORD rd32(TADDR a) { DWORD v = 0; rd(a, &v, 4); return v; }
static TADDR rdp(TADDR a) { TADDR v = 0; rd(a, &v, sizeof v); return v; }   /* a pointer-sized value */
static BOOL wr32(TADDR a, DWORD value) { SIZE_T n; return WriteProcessMemory(hProc, (void *)a, &value, 4, &n) && n == 4; }
static BOOL wrp(TADDR a, TADDR value) { SIZE_T n; return WriteProcessMemory(hProc, (void *)a, &value, sizeof value, &n) && n == sizeof value; }
static int loopback_socket_allowed(DWORD family,DWORD type,DWORD protocol) {
    return family==2 && (type==1 || type==2) && (protocol==0 || protocol==6 || protocol==17);
}
static int loopback_ioctl_allowed(DWORD code) {return code==0x8004667e || code==0x4004667f || code==0x9800000c;}
static int local_uia_pipe(const char* path) {
    const char* prefix="\\??\\PIPE\\UIA_PIPE_";unsigned long pid,token;char tail;
    return !_strnicmp(path,prefix,strlen(prefix)) && sscanf(path+strlen(prefix),"%lu_%lx%c",&pid,&token,&tail)==2 && pid>0;
}
/* TEST ONLY (round 39): --port-offset N moves the game's fixed TCP Protocol ports 33334/33335 to +N in every
   loopback bind/connect/sendto of this instance, so parallel instances do not collide (WSAEADDRINUSE) */
static int port_offset;
static int loopback_address(TADDR address,DWORD length,DWORD* prior) {
    WORD family=0,port=0;
    if(length<16 || !rd(address,&family,2) || family!=2)return 0;
    if(port_offset && rd(address+2,&port,2)) {
        WORD host=(WORD)((port>>8)|(port<<8));
        if(host>=33334 && host<=33335) {
            WORD moved=(WORD)(host+port_offset),net=(WORD)((moved>>8)|(moved<<8));SIZE_T n;
            if(!WriteProcessMemory(hProc,(void*)(address+2),&net,2,&n) || n!=2)return 0;
            printf("TEST instance port %u -> %u\n",(unsigned)host,(unsigned)moved);
        }
    }
    *prior=rd32(address+4);return wr32(address+4,0x0100007f);
}
static void wr8(TADDR a, BYTE b) { SIZE_T n; WriteProcessMemory(hProc, (void *)a, &b, 1, &n); FlushInstructionCache(hProc, (void *)a, 1); }

/* Test harness virtualization: DirectDraw/3D/input bookkeeping goes to real
   handles inside the snapshot-restored HKCU game subtree. Host keys stay read-only. */
static int redirect_ddraw_key(const char *path, CONTEXT *c, TADDR handle_out, TADDR disposition_out)
{
    HKEY local = NULL;
    HANDLE remote = NULL;
    DWORD disposition = 0;
    const char *wanted = "\\REGISTRY\\MACHINE\\Software\\Microsoft\\DirectDraw\\MostRecentApplication";
    const char *wanted3d = "\\REGISTRY\\MACHINE\\Software\\Microsoft\\Direct3D\\MostRecentApplication";
    const char *private_key;
    char input_path[800], input_private[1000], input_recent[800], dd_private[400], d3d_private[400], di_private[400], di_root[400];
    static int input_seeded;
    snprintf(input_path,sizeof input_path,"%s\\System\\CurrentControlSet\\Control\\MediaProperties\\PrivateProperties\\DirectInput",regUser);
    snprintf(input_recent,sizeof input_recent,"%s\\Software\\Microsoft\\DirectInput\\MostRecentApplication",regUser);
    if (!windowed_test) return 0;
    reg_private(dd_private, sizeof dd_private, "windowed-test DirectDraw\\MostRecentApplication");
    reg_private(d3d_private, sizeof d3d_private, "windowed-test Direct3D\\MostRecentApplication");
    reg_private(di_private, sizeof di_private, "windowed-test DirectInput\\MostRecentApplication");
    reg_private(di_root, sizeof di_root, "windowed-test DirectInput");
    if (!_stricmp(path, wanted))
        private_key = dd_private;
    else if (!_stricmp(path, wanted3d))
        private_key = d3d_private;
    else if (!_strnicmp(path,input_recent,strlen(input_recent)) && (!path[strlen(input_recent)] || !strcmp(path+strlen(input_recent),"\\")))
        private_key = di_private;
    else if(!_strnicmp(path,input_path,strlen(input_path)) && (!path[strlen(input_path)] || path[strlen(input_path)]=='\\')) {
        const char *sub="System\\CurrentControlSet\\Control\\MediaProperties\\PrivateProperties\\DirectInput";
        if(!input_seeded) {
            HKEY host=NULL, dest=NULL;
            if(RegCreateKeyExA(HKEY_CURRENT_USER,di_root,0,NULL,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,NULL,&dest,NULL)!=ERROR_SUCCESS)return -1;
            if(RegOpenKeyExA(HKEY_CURRENT_USER,sub,0,KEY_READ,&host)==ERROR_SUCCESS) {
                LONG copied=RegCopyTreeA(host,NULL,dest);RegCloseKey(host);
                if(copied!=ERROR_SUCCESS){RegCloseKey(dest);return -1;}
            }
            RegCloseKey(dest);input_seeded=1;
        }
        snprintf(input_private,sizeof input_private,"%s%s",di_root,path+strlen(input_path));
        private_key=input_private;
    } else return 0;
    if (RegCreateKeyExA(HKEY_CURRENT_USER,
            private_key,
            0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &local, &disposition) != ERROR_SUCCESS)
        return -1;
    if (!DuplicateHandle(GetCurrentProcess(), local, hProc, &remote, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        RegCloseKey(local);
        return -1;
    }
    RegCloseKey(local);
    if (!wrp(handle_out, (TADDR)remote) || (disposition_out && !wr32(disposition_out, disposition)))
        return -1;
    fake_return(c, 7, 0);   /* NtCreateKey (seven arguments) returns STATUS_SUCCESS; the handle is real, not a stub */
    return 1;
}

/* the object name of a handle in the target (duplicated into the harness, queried, closed) */
static void target_handle_name(TADDR h, char *out, size_t n)
{
    HANDLE dup;
    out[0] = 0;
    if (!h || !DuplicateHandle(hProc, (HANDLE)h, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS)) return;
    local_handle_name(dup, out, n);
    CloseHandle(dup);
}

/* --registry-subtree (round 39): the HKCU-relative private path for a native path inside the game's EE-AOC key, or 0
   when the path is elsewhere, is harness bookkeeping, or is already inside a "windowed-test *" subtree */
static int reg_instance_path(const char *full, char *out, size_t n)
{
    size_t m = strlen(regMadDoc), len;
    const char *rest;
    if (!reg_subtree[0] || !regMadDoc[0] || _strnicmp(full, regMadDoc, m) || _strnicmp(full + m, "\\EE-AOC", 7)) return 0;
    rest = full + m + 7;
    if (*rest && *rest != '\\') return 0;
    if (!_strnicmp(rest, "\\windowed-test ", 15)) return 0;
    snprintf(out, n, "%s\\%s%s", regGameRel, reg_subtree, rest);
    len = strlen(out);
    while (len && out[len - 1] == '\\') out[--len] = 0;
    return 1;
}

static int reg_instance_handle(HKEY local, CONTEXT *c, TADDR handle_out)
{
    HANDLE remote = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), local, hProc, &remote, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        RegCloseKey(local);
        return 0;
    }
    RegCloseKey(local);
    return wrp(handle_out, (TADDR)remote);
}

/* NtCreateKey (7 arguments) inside the game key: create the redirected key here and hand the target a real handle */
static int reg_instance_create(const char *full, CONTEXT *c, TADDR handle_out, TADDR disposition_out, DWORD options)
{
    char key[900];
    HKEY local = NULL;
    DWORD disposition = 0;
    if (!reg_instance_path(full, key, sizeof key)) return 0;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, key, 0, NULL, options & REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &local,
                        &disposition) != ERROR_SUCCESS)
        return -1;
    if (!reg_instance_handle(local, c, handle_out) || (disposition_out && !wr32(disposition_out, disposition))) return -1;
    printf("regredirect-instance create %s -> HKCU\\%s\n", full, key);
    fake_return(c, 7, 0);
    return 1;
}

/* NtOpenKey (3 arguments) / NtOpenKeyEx (4) inside the game key: open the redirected key (absent: not found) */
static int reg_instance_open(const char *full, CONTEXT *c, TADDR handle_out, int nargs)
{
    char key[900];
    HKEY local = NULL;
    LONG r;
    DWORD status = 0;
    if (!reg_instance_path(full, key, sizeof key)) return 0;
    r = RegOpenKeyExA(HKEY_CURRENT_USER, key, 0, KEY_ALL_ACCESS, &local);
    if (r == ERROR_FILE_NOT_FOUND) {
        status = 0xC0000034;   /* STATUS_OBJECT_NAME_NOT_FOUND */
        if (!wrp(handle_out, 0)) return -1;
    } else if (r != ERROR_SUCCESS || !reg_instance_handle(local, c, handle_out))
        return -1;
    printf("regredirect-instance open %s -> HKCU\\%s%s\n", full, key, status ? " (absent)" : "");
    fake_return(c, nargs, status);
    return 1;
}

static DWORD image_size(TADDR base)
{
    IMAGE_DOS_HEADER d; IMAGE_NT_HEADERS nt;   /* the harness's own PE32/PE32+: its target's */
    if (!rd(base, &d, sizeof d) || !rd(base + d.e_lfanew, &nt, sizeof nt)) return 0x1000;
    return nt.OptionalHeader.SizeOfImage;
}

/* Windowed tests discover COM entry points in the TARGET. Never load a staged
   graphics wrapper into the debugger or apply system DLL offsets to that wrapper. */
static void coverage_release(TADDR address);   /* TEST ONLY (round 39, cov): differential/coverage.h */
static int target_guard(TADDR address, const char *name, gkind kind)
{
    int i;
    MEMORY_BASIC_INFORMATION info;
    coverage_release(address);   /* a still-armed --coverage int3 at this address hands its byte back first */
    for (i = 0; i < nguards; i++) if (guards[i].addr == address) return guards[i].kind == kind;
    if (!address || nguards >= MAXGUARD || !VirtualQueryEx(hProc, (void *)address, &info, sizeof info)
        || info.State != MEM_COMMIT || !(info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return 0;
    guards[nguards].addr = address; guards[nguards].name = name; guards[nguards].kind = kind;
    if (!rd(address, &guards[nguards].orig, 1)) return 0;
    wr8(address, 0xCC); nguards++;
    return 1;
}

/* The per-PT hasher and the PT watch read VC6-layout game state with 32-bit pointers (differential/, not ported to
   this fork's builds yet): the x64 harness refuses their options and arms nothing. */
static struct { unsigned long last_key; int keyed; } pth;
static unsigned long ptsync_hits;
static int x64_refuse(const char *o) { fprintf(stderr, "%s: not available in the x64 harness\n", o); exit(2); return 0; }
static int ptsync_option(int argc, char **argv, int *option)
{
    (void)argc;
    return !strcmp(argv[*option], "--pt-sync") || !strcmp(argv[*option], "--pt-hash") ? x64_refuse(argv[*option]) : 0;
}
static int ptsync_arm(TADDR base, DWORD size) { (void)base; (void)size; return 1; }
static void ptsync_hit(void) {}
static int ptwatch_option(int argc, char **argv, int *option)
{
    (void)argc;
    return !strcmp(argv[*option], "--pt-watch") ? x64_refuse(argv[*option]) : 0;
}
static void ptwatch_env(const char *stage) { (void)stage; if (getenv("EE_PTWATCH")) x64_refuse("EE_PTWATCH"); }
static void ptwatch_tick(unsigned long key) { (void)key; }
static void ptwatch_hit(TADDR a, CONTEXT *c, DWORD tid) { (void)a; (void)c; (void)tid; }
static int ptwatch_single_step(HANDLE th, DWORD tid) { (void)th; (void)tid; return 0; }
#include "differential/coverage.h" /* TEST ONLY (round 39, agent cov): opt-in --coverage LIST OUT / EE_COVERAGE */

static TADDR target_export(TADDR base, const char *name)
{
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS nt; IMAGE_EXPORT_DIRECTORY ex;
    DWORD i, rva; TADDR names, functions, ordinals;
    if (!rd(base, &dos, sizeof dos) || !rd(base+dos.e_lfanew, &nt, sizeof nt)) return 0;
    rva = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!rva || !rd(base+rva, &ex, sizeof ex)) return 0;
    names=base+ex.AddressOfNames; functions=base+ex.AddressOfFunctions; ordinals=base+ex.AddressOfNameOrdinals;
    for (i=0; i<ex.NumberOfNames; i++) {
        char text[128]={0}; WORD ordinal;
        if (!rd(base+rd32(names+4*i),text,sizeof text-1) || strcmp(text,name)) continue;
        if (!rd(ordinals+2*i,&ordinal,2)) return 0;
        rva=rd32(functions+4*ordinal);
        if (rva>=nt.OptionalHeader.DataDirectory[0].VirtualAddress
            && rva<nt.OptionalHeader.DataDirectory[0].VirtualAddress+nt.OptionalHeader.DataDirectory[0].Size)
            return 0; /* unsupported forwarded export: fail closed */
        return base+rva;
    }
    return 0;
}

static int draw_iid(TADDR address)
{
    GUID value;
    const GUID ids[] = {
        {0x6c14db80,0xa733,0x11ce,{0xa5,0x21,0x00,0x20,0xaf,0x0b,0xe5,0x60}},
        {0xb3a6f3e0,0x2b43,0x11cf,{0xa2,0xde,0x00,0xaa,0x00,0xb9,0x33,0x56}},
        {0x9c59509a,0x39bd,0x11d1,{0x8c,0x4a,0x00,0xc0,0x4f,0xd9,0x30,0xc5}},
        {0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b}}
    };
    unsigned i;
    if (!rd(address,&value,sizeof value)) return 0;
    for (i=0;i<sizeof ids/sizeof *ids;i++) if (!memcmp(&value,&ids[i],sizeof value)) return 1;
    return 0;
}

static int d3d_iid(TADDR address) { GUID g;return rd(address,&g,sizeof g) && !memcmp(&g,&IID_IDirect3D7,sizeof g); }
static struct {TADDR at; DWORD tid; TADDR out;int type;} dd_returns[32];
static int ndd_returns;
/* An address in native ARM64 code of a CHPE system DLL (x86 emulation on ARM64; an int3 there would corrupt an
   ARM64 instruction). x64 on ARM64 is ARM64EC, not CHPE: the x64 harness does not run there, so never. */
static int chpe_arm64(TADDR va)
{
    (void)va;
    return 0;
}

static int watch_dd_return(CONTEXT *c, DWORD tid, TADDR out, int type)
{
    TADDR at=rdp(R_SP(c));
    if (ndd_returns>=32 || chpe_arm64(at) || !target_guard(at,"<DirectDraw interface return>",G_DD_RETURN)) return 0;
    dd_returns[ndd_returns].at=at;dd_returns[ndd_returns].tid=tid;dd_returns[ndd_returns].out=out;dd_returns[ndd_returns++].type=type;
    return 1;
}

/* UNICODE_STRING at a (the target's: the harness's own layout) -> narrow string */
static void ustr(TADDR a, char *out, size_t n)
{
    USTR u; USHORT len; TADDR buf; WCHAR w[512]; int k;
    out[0] = 0;
    if (!a || !rd(a, &u, sizeof u)) return;
    len = u.Length; buf = (TADDR)u.Buffer;
    if (len > sizeof w - 2) len = sizeof w - 2;
    if (!rd(buf, w, len)) return;
    w[len / 2] = 0;
    for (k = 0; w[k] && k < (int)n - 1; k++) out[k] = w[k] < 128 ? (char)w[k] : '?';
    out[k] = 0;
}

/* OBJECT_ATTRIBUTES at oa (the target's: the harness's own layout) */
static TADDR oa_root(TADDR oa) { OBJATTR o; return rd(oa, &o, sizeof o) ? (TADDR)o.RootDirectory : 0; }
static TADDR oa_name(TADDR oa) { OBJATTR o; return rd(oa, &o, sizeof o) ? (TADDR)o.ObjectName : 0; }
static void objname(TADDR oa, char *out, size_t n)
{
    char s[600];
    TADDR root = oa_root(oa);
    ustr(oa_name(oa), s, sizeof s);
    snprintf(out, n, "%s%s", root ? "<rel>\\" : "", s);
}

static void add_guard(HMODULE ours, TADDR theirs, const char *name, gkind kind)
{
    FARPROC p = GetProcAddress(ours, name);
    TADDR a;
    if (!p || nguards >= MAXGUARD) return;
    a = theirs + (TADDR)((BYTE *)p - (BYTE *)ours);
    guards[nguards].addr = a;
    guards[nguards].name = name;
    guards[nguards].kind = kind;
    rd(a, &guards[nguards].orig, 1);
    wr8(a, 0xCC);
    nguards++;
}

static void guard_module(const char *base, TADDR at, const char *full_path)
{
    HMODULE m;
    if(windowed_test && !_stricmp(base,"Low-Level Engine.dll"))
        target_guard(target_export(at,"?ESRecordNativeError@@YAKK@Z"),"ESRecordNativeError",G_NATIVE_ERROR);
    if(rng_trace_physics && !_stricmp(base,"Low-Level Engine.dll")) {
        rng_trace_entries[0]=target_export(at,"?Generate@URandom@@QEAAMXZ");
        rng_trace_entries[1]=target_export(at,"?NextInSeries@URandom@@AEAAKXZ");
        printf("TEST rng trace resolved %08lx %08lx (gPhysics %08lx)\n",(unsigned long)rng_trace_entries[0],
               (unsigned long)rng_trace_entries[1],(unsigned long)rng_trace_physics);
    }
    if(windowed_test && !_stricmp(base,"TestPresentation.dll"))
        target_guard(target_export(at,"TestPresentWindowed"),"TEST GDI present",G_RENDER_PRESENT);
    if(windowed_test && !_stricmp(base,"msvcrt.dll")) {
        m=GetModuleHandleA("msvcrt.dll");
        add_guard(m,at,"abort",G_STOP);
    }
    if(windowed_test && !_stricmp(base,"ucrtbase.dll")) {   /* the modern runtime (MSVC STL builds) */
        m=LoadLibraryA("ucrtbase.dll");   /* not loaded in this (MinGW) process */
        if(m)add_guard(m,at,"abort",G_STOP);
    }
    if (windowed_test && !_stricmp(base,"DX7HRDisplay.dll")) {
        if (!target_guard(at+renderer_activate_rva,"DX7Rasterizer::ActivateScreen",G_RENDER_ACTIVATE)
            || !target_guard(at+renderer_present_rva,"DX7Rasterizer::ShowScene",G_RENDER_PRESENT)) {
            printf("STOP cannot watch staged renderer checkpoints\n");TerminateProcess(hProc,1);
        }
    }
    if (!_stricmp(base, "ntdll.dll")) {
        m = GetModuleHandleA("ntdll.dll");
        add_guard(m, at, "NtCreateFile", G_FILE_CREATE);
        add_guard(m, at, "NtOpenFile", G_FILE_OPEN);
        add_guard(m, at, "NtSetInformationFile", G_SETINFO);
        add_guard(m, at, "NtDeleteFile", G_STOP);
        add_guard(m, at, "NtCreateKey", G_REG_CREATE);
        if (reg_subtree[0]) {   /* --registry-subtree only: reads of the game key are redirected too */
            add_guard(m, at, "NtOpenKey", G_REG_OPEN);
            add_guard(m, at, "NtOpenKeyEx", G_REG_OPEN_EX);
        }
        add_guard(m, at, "NtCreateKeyTransacted", G_STOP);
        add_guard(m, at, "NtSetValueKey", G_REG_HANDLE);
        add_guard(m, at, "NtDeleteKey", G_REG_HANDLE);
        add_guard(m, at, "NtDeleteValueKey", G_REG_HANDLE);
        add_guard(m, at, "NtRenameKey", G_STOP);
        add_guard(m, at, "NtSaveKey", G_STOP);
        add_guard(m, at, "NtRestoreKey", G_STOP);
        add_guard(m, at, "NtLoadKey", G_STOP);
        add_guard(m, at, "NtSetSystemTime", G_STOP);
        add_guard(m, at, "NtCreateUserProcess", G_STOP);
        add_guard(m, at, "NtCreateProcessEx", G_STOP);
        add_guard(m, at, "NtShutdownSystem", G_STOP);
        if(windowed_test) {
            add_guard(m,at,"NtDeviceIoControlFile",G_DEVICE_CONTROL);
            add_guard(m,at,"NtWriteFile",G_WRITE_FILE);
        }
    } else if (!_stricmp(base, "win32u.dll") && (m = LoadLibraryA("win32u.dll"))) {
        add_guard(m, at, "NtUserCreateWindowEx", G_WINDOW);
        add_guard(m, at, "NtUserChangeDisplaySettings", G_STOP);
        add_guard(m, at, "NtGdiSetDeviceGammaRamp", G_STOP);
        add_guard(m, at, "NtUserSystemParametersInfo", G_SPI);
    } else if (!_stricmp(base, "user32.dll") && (m = LoadLibraryA("user32.dll"))) {
        add_guard(m, at, "MessageBoxA", G_MSGBOX_A);
        add_guard(m, at, "MessageBoxW", G_MSGBOX_W);
        add_guard(m, at, "MessageBoxExA", G_MSGBOX_A);
        add_guard(m, at, "MessageBoxExW", G_MSGBOX_W);
        add_guard(m, at, "MessageBoxIndirectA", G_STOP);
        add_guard(m, at, "MessageBoxIndirectW", G_STOP);
        add_guard(m, at, "ChangeDisplaySettingsA", G_STOP);
        add_guard(m, at, "ChangeDisplaySettingsW", G_STOP);
        add_guard(m, at, "ChangeDisplaySettingsExA", G_STOP);
        add_guard(m, at, "ChangeDisplaySettingsExW", G_STOP);
    } else if (!_stricmp(base, "gdi32.dll") && (m = LoadLibraryA("gdi32.dll"))) {
        add_guard(m, at, "SetDeviceGammaRamp", G_STOP);
    } else if (!_stricmp(base, "ddraw.dll")) {
        if (windowed_test) {
            if (!target_guard(target_export(at,"DirectDrawCreate"),"DirectDrawCreate",G_DD_CREATE)
                || !target_guard(target_export(at,"DirectDrawCreateEx"),"DirectDrawCreateEx",G_DD_CREATE_EX)) {
                printf("STOP cannot guard target DirectDraw exports\n");
                TerminateProcess(hProc, 1);
            }
            return;
        }
        /* COM methods have no exports: take SetCooperativeLevel (vtable 20) and SetDisplayMode (21) of every
           IDirectDraw interface version from a DirectDraw object the harness creates itself (no cooperative level,
           no mode change), as offsets into ddraw.dll. Without them the run stops at the first DirectDrawCreate. */
        DWORD offs[8][2]; int noffs = -1;
        int i;
        /* Resolve the very same DLL as the target, including opt-in test wrappers.
           System-DDraw offsets must never be applied to a staged compatibility DLL. */
        m = LoadLibraryExA(full_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (noffs < 0 && m) {
            typedef HRESULT (WINAPI *ddc_t)(GUID *, LPDIRECTDRAW *, IUnknown *);
            typedef HRESULT (WINAPI *ddcx_t)(GUID *, LPVOID *, REFIID, IUnknown *);
            ddc_t ddc = (ddc_t)GetProcAddress(m, "DirectDrawCreate");
            ddcx_t ddcx = (ddcx_t)GetProcAddress(m, "DirectDrawCreateEx");
            IUnknown *objs[4] = {0};
            LPDIRECTDRAW dd = NULL;
            HKEY scratch = NULL, madDoc;
            noffs = 0;
            /* DirectDrawCreate records the caller in HKLM\...\DirectDraw\MostRecentApplication. The harness's own
               HKLM is redirected to a volatile scratch key inside the snapshot-restored EE-AOC subtree meanwhile, so
               resolving the vtables writes nothing outside it (the scratch key is deleted right after). */
            char scratch_key[300];
            reg_private(scratch_key, sizeof scratch_key, "exe_boot scratch");
            if (RegCreateKeyExA(HKEY_CURRENT_USER, scratch_key, 0, NULL,
                                REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &scratch, NULL) != ERROR_SUCCESS
                || RegOverridePredefKey(HKEY_LOCAL_MACHINE, scratch) != ERROR_SUCCESS) {
                printf("ddraw: cannot redirect the harness's HKLM; DirectDraw guarded by its exports only\n");
                ddc = NULL; ddcx = NULL;
            }
            if (ddc && ddc(NULL, &dd, NULL) == DD_OK) {
                objs[0] = (IUnknown *)dd;
                IDirectDraw_QueryInterface(dd, &IID_IDirectDraw2, (void **)&objs[1]);
                IDirectDraw_QueryInterface(dd, &IID_IDirectDraw4, (void **)&objs[2]);
            }
            if (ddcx) ddcx(NULL, (void **)&objs[3], &IID_IDirectDraw7, NULL);
            for (i = 0; i < 4; i++)
                if (objs[i]) {
                    void **vt = *(void ***)objs[i];
                    offs[noffs][0] = (DWORD)((BYTE *)vt[20] - (BYTE *)m);
                    offs[noffs++][1] = (DWORD)((BYTE *)vt[21] - (BYTE *)m);
                }
            for (i = 3; i >= 0; i--) if (objs[i]) objs[i]->lpVtbl->Release(objs[i]);
            if (scratch) {
                RegOverridePredefKey(HKEY_LOCAL_MACHINE, NULL);
                RegCloseKey(scratch);
                if (RegOpenKeyExA(HKEY_CURRENT_USER, regGameRel, 0, KEY_ALL_ACCESS, &madDoc)
                    == ERROR_SUCCESS) {
                    RegDeleteTreeA(madDoc, "exe_boot scratch");
                    RegCloseKey(madDoc);
                }
            }
            printf("ddraw: %d interface vtables resolved\n", noffs);
        }
        if (noffs <= 0) {
            add_guard(m, at, "DirectDrawCreate", G_STOP);
            add_guard(m, at, "DirectDrawCreateEx", G_STOP);
        }
        for (i = 0; i < noffs; i++) {
            int k, dup0 = 0, dup1 = 0;
            for (k = 0; k < nguards; k++) {
                dup0 |= guards[k].addr == at + offs[i][0];
                dup1 |= guards[k].addr == at + offs[i][1];
            }
            if (!dup0 && nguards < MAXGUARD) {
                guards[nguards].addr = at + offs[i][0]; guards[nguards].name = "IDirectDraw::SetCooperativeLevel";
                guards[nguards].kind = G_DD_COOP; rd(guards[nguards].addr, &guards[nguards].orig, 1);
                wr8(guards[nguards].addr, 0xCC); nguards++;
            }
            if (!dup1 && nguards < MAXGUARD) {
                guards[nguards].addr = at + offs[i][1]; guards[nguards].name = "IDirectDraw::SetDisplayMode";
                guards[nguards].kind = G_STOP; rd(guards[nguards].addr, &guards[nguards].orig, 1);
                wr8(guards[nguards].addr, 0xCC); nguards++;
            }
        }
    } else if (!_stricmp(base, "ws2_32.dll") && (m = LoadLibraryA("ws2_32.dll"))) {
        add_guard(m, at, "connect", loopback_test?G_NET_ADDRESS:G_STOP);
        add_guard(m, at, "bind", loopback_test?G_NET_ADDRESS:G_STOP);
        add_guard(m, at, "listen", loopback_test?G_NET_LISTEN:G_STOP);
        add_guard(m, at, "sendto", loopback_test?G_NET_ADDRESS:G_STOP);
        add_guard(m, at, "WSAConnect", loopback_test?G_NET_ADDRESS:G_STOP);
        add_guard(m, at, "WSASendTo", loopback_test?G_NET_ADDRESS:G_STOP);
        if(loopback_test) {
            add_guard(m,at,"socket",G_NET_SOCKET);add_guard(m,at,"WSASocketA",G_NET_SOCKET);add_guard(m,at,"WSASocketW",G_NET_SOCKET);
            add_guard(m,at,"closesocket",G_NET_CLOSE);
            add_guard(m,at,"WSAIoctl",G_NET_IOCTL);add_guard(m,at,"WSAJoinLeaf",G_STOP);
            add_guard(m,at,"WSAConnectByNameA",G_STOP);add_guard(m,at,"WSAConnectByNameW",G_STOP);
            if(mp_self_ip){add_guard(m,at,"accept",G_MP_ACCEPT);add_guard(m,at,"WSAAccept",G_MP_ACCEPT);}   /* TEST ONLY (round 40, mp) */
        }
    }
}

/* ---- windows of the target: listed at the end of a run (no messages are sent: the target is frozen while the
   harness handles a debug event), the largest visible one captured from the screen to a .bmp */
static DWORD targetPid;
static int nwin;
static HWND bigWin;
static long bigArea;

static BOOL CALLBACK list_win(HWND h, LPARAM verbose)
{
    DWORD pid = 0;
    char cls[128] = "";
    WCHAR wt[128] = {0};
    char title[128] = "";
    RECT r;
    int k;
    GetWindowThreadProcessId(h, &pid);
    if (pid != targetPid) return TRUE;
    nwin++;
    if (!verbose) return TRUE;
    GetClassNameA(h, cls, sizeof cls);
    InternalGetWindowText(h, wt, 127);
    for (k = 0; wt[k]; k++) title[k] = wt[k] < 128 ? (char)wt[k] : '?';
    GetWindowRect(h, &r);
    printf("window-at-end class \"%s\" title \"%s\" %s %ldx%ld at %ld,%ld\n", cls, title,
           IsWindowVisible(h) ? "visible" : "hidden", r.right - r.left, r.bottom - r.top, r.left, r.top);
    if (IsWindowVisible(h) && (long)(r.right - r.left) * (r.bottom - r.top) > bigArea) {
        bigArea = (long)(r.right - r.left) * (r.bottom - r.top);
        bigWin = h;
    }
    return TRUE;
}

static void screenshot(HWND h, const char *file)
{
    RECT r;
    HDC screen, mem;
    HBITMAP bmp;
    BITMAPINFOHEADER bi = {sizeof bi};
    BITMAPFILEHEADER bf = {0x4D42};
    BYTE *px;
    FILE *f;
    long w, hh, stride;
    if (!h || !file || !GetWindowRect(h, &r)) return;
    w = r.right - r.left; hh = r.bottom - r.top;
    if (w <= 0 || hh <= 0 || w > 8192 || hh > 8192) return;
    screen = GetDC(NULL);
    mem = CreateCompatibleDC(screen);
    bmp = CreateCompatibleBitmap(screen, w, hh);
    SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, hh, screen, r.left, r.top, SRCCOPY);
    stride = (w * 3 + 3) & ~3;
    bi.biWidth = w; bi.biHeight = hh; bi.biPlanes = 1; bi.biBitCount = 24; bi.biSizeImage = stride * hh;
    px = malloc(bi.biSizeImage);
    if (px && GetDIBits(mem, bmp, 0, hh, px, (BITMAPINFO *)&bi, DIB_RGB_COLORS) && (f = fopen(file, "wb"))) {
        bf.bfOffBits = sizeof bf + sizeof bi;
        bf.bfSize = bf.bfOffBits + bi.biSizeImage;
        fwrite(&bf, sizeof bf, 1, f); fwrite(&bi, sizeof bi, 1, f); fwrite(px, 1, bi.biSizeImage, f);
        fclose(f);
        printf("screenshot %ldx%ld -> %s\n", w, hh, file);
    }
    free(px);
    DeleteObject(bmp); DeleteDC(mem); ReleaseDC(NULL, screen);
}

#define WRITEISH 0x530D0156u   /* FILE_WRITE_DATA|APPEND|WRITE_EA|WRITE_ATTRIBUTES|DELETE|WRITE_DAC|WRITE_OWNER|
                                  GENERIC_WRITE|GENERIC_ALL|MAXIMUM_ALLOWED */

static void dump(const char *file, int with_data)
{
    IMAGE_DOS_HEADER d; IMAGE_NT_HEADERS nt; IMAGE_SECTION_HEADER sh;
    TADDR iat, sections; DWORD n, i, fnv = 2166136261u;
    char m[128];
    rd(exeBase, &d, sizeof d);
    rd(exeBase + d.e_lfanew, &nt, sizeof nt);
    iat = exeBase + nt.OptionalHeader.DataDirectory[12].VirtualAddress;
    n = nt.OptionalHeader.DataDirectory[12].Size / PTR;
    for (i = 0; i < n; i++) {
        TADDR v = rdp(iat + PTR * i); DWORD k;
        mod_of(v, m, sizeof m);
        if (v) printf("iat %04lx %s\n", (unsigned long)(PTR * i), m);
        for (k = 0; m[k]; k++) fnv = (fnv ^ (BYTE)m[k]) * 16777619u;
    }
    printf("IAT: %lu slots, fnv %08lx\n", (unsigned long)n, (unsigned long)fnv);
    if (!with_data) return;
    sections = exeBase + d.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    for (i = 0; i < nt.FileHeader.NumberOfSections; i++) {
        rd(sections + i * sizeof sh, &sh, sizeof sh);
        if (!memcmp(sh.Name, ".data", 6)) {
            DWORD size = sh.Misc.VirtualSize, k;
            BYTE *buf = malloc(size);
            FILE *f;
            fnv = 2166136261u;
            if (buf && rd(exeBase + sh.VirtualAddress, buf, size)) {
                for (k = 0; k < size; k++) fnv = (fnv ^ buf[k]) * 16777619u;
                if (file && (f = fopen(file, "wb"))) { fwrite(buf, 1, size, f); fclose(f); }
                printf(".data+.bss at WinMain: %lu bytes, fnv %08lx%s%s\n", (unsigned long)size, (unsigned long)fnv,
                       file ? " -> " : "", file ? file : "");
            }
            free(buf);
        }
    }
}

int main(int argc, char **argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    char path[MAX_PATH], cmd[MAX_PATH + 4];
    STARTUPINFOA si = {sizeof si};
    PROCESS_INFORMATION pi;
    DEBUG_EVENT ev;
    DWORD deadline, mode, winmain_rva = 0x140d73, last_event, last_diagnostic;
    int done = 0, option;
    if (argc < 5) {
        fprintf(stderr, "usage: exe_boot <dir> <exe> <oep|winmain|run> <timeout ms> [dump file]\n");
        return 2;
    }
    mode = !strcmp(argv[3], "oep") ? 1 : !strcmp(argv[3], "winmain") ? 2 : 0;
    for (option = 6; option < argc; option++) {
        if (!strcmp(argv[option], "--windowed")) windowed_test = 1;
        else if (!strcmp(argv[option], "--graphics-checkpoint")) graphics_checkpoint = 1;
        else if (!strcmp(argv[option], "--audio-checkpoint")) audio_checkpoint = 1;
        else if (!strcmp(argv[option], "--live-session")) live_session = 1;
        else if (!strcmp(argv[option], "--loopback-only")) loopback_test = 1;
        else if (!strcmp(argv[option], "--stack-scan") && option+1<argc) stack_scan = atoi(argv[++option]);
        else if (!strcmp(argv[option], "--audio-entry") && option+1<argc) audio_entry_rva = strtoul(argv[++option], NULL, 16);
        else if (!strcmp(argv[option], "--winmain-rva") && option+1<argc) winmain_rva = strtoul(argv[++option], NULL, 16);
        else if (!strcmp(argv[option], "--renderer-guards") && option+2<argc) {
            renderer_activate_rva = strtoul(argv[++option], NULL, 16); renderer_present_rva = strtoul(argv[++option], NULL, 16);
        }
        else if (!strcmp(argv[option], "--static-exe") && option+4<argc) {
            static_exe = 1;
            static_native_error_rva = strtoul(argv[++option], NULL, 16); static_abort_rva = strtoul(argv[++option], NULL, 16);
            static_generate_rva = strtoul(argv[++option], NULL, 16); static_next_rva = strtoul(argv[++option], NULL, 16);
        }
        else if (!strcmp(argv[option], "--trace-once")) trace_once=1;
        else if (!strcmp(argv[option], "--rng-trace") && option+1<argc) rng_trace_physics=(TADDR)strtoull(argv[++option],NULL,16);
        else if (!strcmp(argv[option], "--rng-offset") && option+1<argc) rng_trace_offset=strtoul(argv[++option],NULL,16);
        else if (!strcmp(argv[option], "--image-base") && option+1<argc) {va_base=(TADDR)strtoull(argv[++option],NULL,16);va_base_set=1;}
        else if (!strcmp(argv[option], "--reg-root") && option+1<argc) {   /* TEST ONLY: the self test's policy root */
            option++;
            if (_strnicmp(argv[option], "Software\\", 9) || strlen(argv[option]) >= sizeof regRootRel - 1 || strchr(argv[option] + 9, '\\')
                || !_stricmp(argv[option], "Software\\Mad Doc Software")) {
                fprintf(stderr, "--reg-root must be one key below Software (not the game's)\n"); return 2;
            }
            snprintf(regRootRel, sizeof regRootRel, "%s", argv[option]);
            snprintf(regGameRel, sizeof regGameRel, "%s\\EE-AOC", regRootRel);
        }
        else if (!strcmp(argv[option], "--seed-pin") && option+3<argc) {
            seed_pin=1;seed_pin_rva=strtoul(argv[++option],NULL,16);
            seed_pin_offset=strtoul(argv[++option],NULL,16);seed_pin_value=strtoul(argv[++option],NULL,0);
        }
        else if (!strcmp(argv[option], "--port-offset") && option+1<argc) port_offset=atoi(argv[++option]);
        else if (!strcmp(argv[option], "--mutex-bypass") && option+1<argc) mutex_bypass_rva=strtoul(argv[++option],NULL,16);
        else if (!strcmp(argv[option], "--no-human-defeat") && option+5<argc) {
            nhd_out_rva=strtoul(argv[++option],NULL,16);nhd_result_rva=strtoul(argv[++option],NULL,16);
            nhd_world=(TADDR)strtoull(argv[++option],NULL,16);nhd_players_off=strtoul(argv[++option],NULL,16);
            nhd_current_off=strtoul(argv[++option],NULL,16);
        }
        else if (!strcmp(argv[option], "--nhd-players") && option+1<argc) nhd_players_mask=strtoul(argv[++option],NULL,0);
        else if (!strcmp(argv[option], "--mp-skip") && option+1<argc) mp_skip_rva=strtoul(argv[++option],NULL,16);
        else if (!strcmp(argv[option], "--mp-checksum") && option+2<argc) {
            mp_checksum_rva=strtoul(argv[++option],NULL,16);mp_checksum_delta=strtoul(argv[++option],NULL,16);
        }
        else if (!strcmp(argv[option], "--mp-loopback") && option+2<argc) {
            DWORD self=strtoul(argv[++option],NULL,0),peer=strtoul(argv[++option],NULL,0);
            if(self<1 || self>254 || peer<1 || peer>254 || self==peer){fprintf(stderr,"--mp-loopback needs two different octets 1..254\n");return 2;}
            mp_self_ip=0x7f|(self<<24);mp_peer_ip=0x7f|(peer<<24);
        }
        else if (!strcmp(argv[option], "--mp-peers") && option+1<argc) {   /* TEST ONLY (final campaign, mp) */
            char* p=argv[++option];
            while(*p) {
                DWORD o=strtoul(p,&p,0);
                if(o<1 || o>254 || nmp_more_ip>=8){fprintf(stderr,"--mp-peers needs up to 8 octets 1..254\n");return 2;}
                mp_more_ip[nmp_more_ip++]=0x7f|(o<<24);
                if(*p==',')p++; else if(*p){fprintf(stderr,"--mp-peers: comma-separated octets\n");return 2;}
            }
        }
        else if (!strcmp(argv[option], "--registry-subtree") && option+1<argc) {
            option++;
            if (_strnicmp(argv[option], "windowed-test ", 14) || strlen(argv[option]) >= sizeof reg_subtree
                || strchr(argv[option], '\\')) {
                fprintf(stderr, "--registry-subtree must be a single \"windowed-test ...\" key name\n"); return 2;
            }
            strcpy(reg_subtree, argv[option]);
        }
        else if (ptsync_option(argc, argv, &option)) ;
        else if (ptwatch_option(argc, argv, &option)) ;
        else if (coverage_option(argc, argv, &option)) ;
        else if (!strcmp(argv[option], "--trace-functions") && option+1<argc) {
            FILE* trace=fopen(argv[++option],"r");unsigned long rva;
            if(!trace){fprintf(stderr,"cannot read experimental trace list\n");return 2;}
            while(fscanf(trace,"%lx",&rva)==1) {
                if(nexperimental_entries>=160){fclose(trace);fprintf(stderr,"too many experimental trace entries\n");return 2;}
                experimental_entries[nexperimental_entries++]=(DWORD)rva;
            }
            fclose(trace);
        }
        else { fprintf(stderr, "unknown harness option: %s\n", argv[option]); return 2; }
    }
    ptwatch_env(argv[1]);   /* TEST ONLY (dbg4): EE_PTWATCH="FROM TO CFG_ORIGINAL CFG_FULLLINK" */
    coverage_env(argv[1]);  /* TEST ONLY (cov): EE_COVERAGE="LIST_ORIGINAL LIST_FULLLINK OUTDIR" */
    if (nexperimental_entries > 8 && !trace_once) { fprintf(stderr, "too many experimental trace entries\n"); return 2; }
    if (windowed_test) printf("TEST native-windowed overlay; isolated DirectDraw bookkeeping; display guards retained\n");
    printf("harness " ARCH_NAME "\n");
    if (strcmp(regRootRel, "Software\\Mad Doc Software")) printf("TEST registry policy root HKCU\\%s\n", regRootRel);
    snprintf(path, sizeof path, "%s\\%s", argv[1], argv[2]);
    snprintf(cmd, sizeof cmd, "\"%s\"", argv[2]);
    if (!CreateProcessA(path, cmd, NULL, NULL, FALSE, DEBUG_ONLY_THIS_PROCESS, NULL, argv[1], &si, &pi)) {
        printf("CreateProcess failed: %lu\n", GetLastError());
        return 2;
    }
    hProc = pi.hProcess;
    targetPid = pi.dwProcessId;
    reg_init();
    {
        char full[MAX_PATH];
        GetFullPathNameA(argv[1], sizeof full, full, NULL);
        snprintf(sandbox, sizeof sandbox, "\\??\\%s%s", full, full[strlen(full) - 1] == 0x5c ? "" : "\\");
    }
    deadline = GetTickCount() + strtoul(argv[4], NULL, 0);
    last_event=last_diagnostic=GetTickCount();
    while (!done) {
        DWORD cont = DBG_CONTINUE, now = GetTickCount();
        coverage_tick();   /* TEST ONLY (cov): rewrite the hit set every 10 s */
        if(live_session) {
            char request[MAX_PATH];snprintf(request,sizeof request,"%s\\stop.request",argv[1]);
            if(rng_trace_physics && !rng_trace_armed && rng_trace_entries[0]) {
                char arm[MAX_PATH];snprintf(arm,sizeof arm,"%s\\rng.arm",argv[1]);
                if(GetFileAttributesA(arm)!=INVALID_FILE_ATTRIBUTES) {
                    rng_trace_armed=1;
                    if(!target_guard(rng_trace_entries[0],"URandom::Generate",G_RNG_TRACE)
                       || !target_guard(rng_trace_entries[1],"URandom::NextInSeries",G_RNG_TRACE))
                        printf("TEST rng trace: cannot guard URandom\n");
                    else printf("TEST rng trace armed\n");
                }
            }
            if(GetFileAttributesA(request)!=INVALID_FILE_ATTRIBUTES) {
                printf(frame_presented?"STOP <windowed session complete> reached\n":"STOP session stopped before frame\n");break;
            }
        }
        if ((LONG)(deadline - now) <= 0) {
            printf("STOP timeout: still running after %s ms\n", argv[4]);
            break;
        }
        if(!WaitForDebugEvent(&ev,live_session && deadline-now>250 ? (rearm ? 5 : 250) : deadline-now)) {
            if(live_session && GetLastError()==ERROR_SEM_TIMEOUT) {
                /* x86 emulation on ARM64: a held thread can be parked in the emulator's native code with a lock
                   the stepper needs (loading a DLL inside a guarded NtOpenFile), so the step never completes.
                   Release the held threads; the guard is re-armed when the step does complete. */
                if(rearm && now-last_event>20) {   /* a real step takes microseconds; each stall otherwise freezes the game */
                    int any=0;for(int t=0;t<nth;t++)any|=held[t];
                    if(any){printf("step-over stalled on thread %lu; held threads released\n",(unsigned long)stepper);hold_others(stepper,0);}
                }
                if(now-last_event>10000 && now-last_diagnostic>30000) {
                    last_diagnostic=now;
                    printf("diagnostic idle rearm=" AFMT " stepper=%lu\n",AV(rearm),(unsigned long)stepper);
                    for(int t=0;t<nth;t++) {
                        DWORD previous=SuspendThread(threads[t]);CONTEXT context;char module[128];
                        if(previous==(DWORD)-1)continue;
                        ZeroMemory(&context,sizeof context);
                        context.ContextFlags=CONTEXT_FULL;
                        if(GetThreadContext(threads[t],&context)) {
                            mod_of(R_IP(&context),module,sizeof module);
                            printf("diagnostic thread %lu suspended=%lu at %s\n",(unsigned long)tids[t],(unsigned long)previous,module);
                            /* x64 code keeps no frame-pointer chain: unwind with the modules' .pdata (dbghelp) */
                            static int sym_ready;
                            if(!sym_ready){SymSetOptions(SYMOPT_DEFERRED_LOADS);sym_ready=SymInitialize(hProc,NULL,TRUE)?1:-1;}
                            else if(sym_ready>0)SymRefreshModuleList(hProc);
                            if(sym_ready>0) {
                                STACKFRAME64 frame;ZeroMemory(&frame,sizeof frame);CONTEXT walk=context;
                                frame.AddrPC.Offset=walk.Rip;frame.AddrPC.Mode=AddrModeFlat;
                                frame.AddrStack.Offset=walk.Rsp;frame.AddrStack.Mode=AddrModeFlat;
                                frame.AddrFrame.Offset=walk.Rbp;frame.AddrFrame.Mode=AddrModeFlat;
                                for(int s=0;s<24 && StackWalk64(IMAGE_FILE_MACHINE_AMD64,hProc,threads[t],&frame,&walk,NULL,
                                        SymFunctionTableAccess64,SymGetModuleBase64,NULL) && frame.AddrPC.Offset;s++) {
                                    mod_of((TADDR)frame.AddrPC.Offset,module,sizeof module);printf("  frame %s\n",module);
                                }
                            } else
                            for(int s=0;s<4;s++){mod_of(rdp(R_SP(&context)+PTR*s),module,sizeof module);printf("  stack %s\n",module);}
                        }
                        ResumeThread(threads[t]);
                    }
                }
                continue;
            }
            printf("STOP timeout: still running after %s ms\n",argv[4]);break;
        }
        last_event=GetTickCount();
        switch (ev.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT:
            exeBase = (TADDR)ev.u.CreateProcessInfo.lpBaseOfImage;
            if(va_base_set && exeBase!=va_base) {   /* an ASLR image: move the absolute VAs by the load delta */
                TADDR delta=exeBase-va_base;
                if(rng_trace_physics)rng_trace_physics+=delta;
                if(nhd_world)nhd_world+=delta;
                printf("image base " AFMT " (map " AFMT "): absolute VAs relocated\n",AV(exeBase),AV(va_base));
            }
            mods[nmods].base = exeBase;
            mods[nmods].size = image_size(exeBase);
            strcpy(mods[nmods++].name, "EXE");
            {   /* the harness reads the target's structures in its own layout: the architectures must agree */
                IMAGE_DOS_HEADER d; IMAGE_FILE_HEADER fh;
                WORD want = PTR == 8 ? IMAGE_FILE_MACHINE_AMD64 : IMAGE_FILE_MACHINE_I386;
                if (!rd(exeBase, &d, sizeof d) || !rd(exeBase + d.e_lfanew + 4, &fh, sizeof fh) || fh.Machine != want) {
                    printf("STOP target machine %04x does not match the " ARCH_NAME " harness (exe_boot.exe: x86_64)\n",
                           (unsigned)fh.Machine);
                    done = 1;
                }
            }
            if(windowed_test && !target_guard(exeBase+audio_entry_rva,"EEInitializeSounds",G_AUDIO_INITIALIZE)) {
                printf("STOP cannot guard audio initialization\n");done=1;
            }
            if(static_exe && windowed_test) {
                if(!target_guard(exeBase+static_native_error_rva,"ESRecordNativeError",G_NATIVE_ERROR)
                    || !target_guard(exeBase+static_abort_rva,"abort",G_STOP)
                    || !target_guard(exeBase+renderer_activate_rva,"DX7Rasterizer::ActivateScreen",G_RENDER_ACTIVATE)
                    || !target_guard(exeBase+renderer_present_rva,"DX7Rasterizer::ShowScene",G_RENDER_PRESENT)) {
                    printf("STOP cannot guard the static exe's engine, runtime and renderer checkpoints\n");done=1;
                }
            }
            if(static_exe && rng_trace_physics) {
                rng_trace_entries[0]=exeBase+static_generate_rva;
                rng_trace_entries[1]=exeBase+static_next_rva;
                printf("TEST rng trace resolved " AFMT " " AFMT " (gPhysics " AFMT ")\n",AV(rng_trace_entries[0]),
                       AV(rng_trace_entries[1]),AV(rng_trace_physics));
            }
            for(int k=0;k<nexperimental_entries;k++) {
                if(experimental_entries[k]>=image_size(exeBase)
                    || !target_guard(exeBase+experimental_entries[k],"experimental candidate entry",G_EXPERIMENTAL_ENTRY)) {
                    printf("STOP cannot trace experimental candidate\n");done=1;break;
                }
            }
            if(seed_pin && (seed_pin_rva>=image_size(exeBase)
                || !target_guard(exeBase+seed_pin_rva,"seed pin entry",G_SEED_PIN))) {
                printf("STOP cannot guard seed pin\n");done=1;
            }
            if(mutex_bypass_rva && (mutex_bypass_rva>=image_size(exeBase)
                || !target_guard(exeBase+mutex_bypass_rva,"EECreateInstanceMutex GetLastError test",G_MUTEX_BYPASS))) {
                printf("STOP cannot guard mutex bypass\n");done=1;
            }
            if(mutex_bypass_rva)printf("TEST mutex bypass armed rva %08lx\n",(unsigned long)mutex_bypass_rva);
            if(nhd_out_rva) {
                if(nhd_out_rva>=image_size(exeBase) || nhd_result_rva>=image_size(exeBase) || !nhd_world
                    || !target_guard(exeBase+nhd_out_rva,"EEWorld::Fn1391F2 (player out)",G_NHD_OUT)
                    || !target_guard(exeBase+nhd_result_rva,"EEPlayer::Fn16AF5C (player result)",G_NHD_RESULT)) {
                    printf("STOP cannot guard no-human-defeat\n");done=1;
                } else printf("TEST no-human-defeat armed out rva %08lx result rva %08lx world " AFMT " players +%lx current +%lx\n",
                              (unsigned long)nhd_out_rva,(unsigned long)nhd_result_rva,AV(nhd_world),
                              (unsigned long)nhd_players_off,(unsigned long)nhd_current_off);
            }
            if(mp_skip_rva && (mp_skip_rva>=image_size(exeBase)
                || !target_guard(exeBase+mp_skip_rva,"mp skip (EEStartExternalLobby)",G_MP_SKIP))) {
                printf("STOP cannot guard mp skip\n");done=1;
            } else if(mp_skip_rva)printf("TEST mp skip armed rva %08lx\n",(unsigned long)mp_skip_rva);
            if(mp_checksum_rva && (mp_checksum_rva>=image_size(exeBase)
                || !target_guard(exeBase+mp_checksum_rva,"mp checksum (EEInitializeNetwork gBinaryChecksum store)",G_MP_CHECKSUM))) {
                printf("STOP cannot guard mp checksum\n");done=1;
            } else if(mp_checksum_rva)printf("TEST mp checksum armed rva %08lx delta %08lx\n",(unsigned long)mp_checksum_rva,
                                             (unsigned long)mp_checksum_delta);
            if(nhd_players_mask)printf("TEST no-human-defeat players mask %08lx\n",(unsigned long)nhd_players_mask);
            if(mp_self_ip)printf("TEST mp loopback self %08lx peer %08lx\n",(unsigned long)mp_self_ip,(unsigned long)mp_peer_ip);
            for(int k=0;k<nmp_more_ip;k++)printf("TEST mp loopback more peer %08lx\n",(unsigned long)mp_more_ip[k]);
            if(reg_subtree[0])printf("TEST registry subtree EE-AOC\\%s\n",reg_subtree);
            if(!ptsync_arm(exeBase,image_size(exeBase))) { printf("STOP cannot guard pt sync\n"); done=1; }
            if(seed_pin)printf("TEST seed pin armed rva %08lx [ecx+%lx] := %lu\n",(unsigned long)seed_pin_rva,
                               (unsigned long)seed_pin_offset,(unsigned long)seed_pin_value);
            if (mode) {
                IMAGE_DOS_HEADER d; IMAGE_NT_HEADERS nt;
                rd(exeBase, &d, sizeof d);
                rd(exeBase + d.e_lfanew, &nt, sizeof nt);
                stopAddr = exeBase + (mode == 1 ? nt.OptionalHeader.AddressOfEntryPoint : winmain_rva);
                guards[nguards].addr = stopAddr;
                guards[nguards].name = mode == 1 ? "<entry point>" : "<WinMain>";
                guards[nguards].kind = G_STOP;
                rd(stopAddr, &guards[nguards].orig, 1);
                wr8(stopAddr, 0xCC);
                nguards++;
            }
            coverage_arm(exeBase, image_size(exeBase), ev.dwProcessId);   /* TEST ONLY (cov): after the exe guards */
            threads[nth] = ev.u.CreateProcessInfo.hThread; tids[nth++] = ev.dwThreadId;
            if (ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile);
            break;
        case CREATE_THREAD_DEBUG_EVENT:
            if(nth>=256){printf("STOP thread tracking full\n");done=1;break;}
            threads[nth]=ev.u.CreateThread.hThread;tids[nth]=ev.dwThreadId;held[nth]=0;
            if(rearm && SuspendThread(threads[nth])!=(DWORD)-1)held[nth]=1;
            nth++;
            break;
        case EXIT_THREAD_DEBUG_EVENT: {
            int k;
            for (k = 0; k < nth; k++)
                if (tids[k] == ev.dwThreadId) { threads[k]=threads[nth-1];held[k]=held[nth-1];tids[k]=tids[--nth];break; }
            break;
        }
        case LOAD_DLL_DEBUG_EVENT: {
            char full[MAX_PATH] = "?", *b;
            TADDR at = (TADDR)ev.u.LoadDll.lpBaseOfDll;
            if (ev.u.LoadDll.hFile) {
                GetFinalPathNameByHandleA(ev.u.LoadDll.hFile, full, sizeof full, 0);
                CloseHandle(ev.u.LoadDll.hFile);
            }
            b = strrchr(full, '\\') ? strrchr(full, '\\') + 1 : full;
            if (nmods < MAXMOD) {
                mods[nmods].base = at;
                mods[nmods].size = image_size(at);
                snprintf(mods[nmods++].name, 64, "%s", b);
            }
            printf("load %s\n", b);
            guard_module(b, at, full);
            break;
        }
        case UNLOAD_DLL_DEBUG_EVENT: {
            TADDR base=(TADDR)ev.u.UnloadDll.lpBaseOfDll; DWORD size=0;
            int k;
            for(k=0;k<nmods;k++) if(mods[k].base==base) { size=mods[k].size;mods[k]=mods[--nmods];break; }
            for(k=0;k<nguards;) {
                if(guards[k].addr>=base && guards[k].addr<base+size) guards[k]=guards[--nguards];
                else k++;
            }
            break;
        }
        case EXCEPTION_DEBUG_EVENT: {
            EXCEPTION_RECORD *er = &ev.u.Exception.ExceptionRecord;
            TADDR a = (TADDR)er->ExceptionAddress; DWORD code = er->ExceptionCode;
            int g, k;
            HANDLE th = NULL;
            CONTEXT c;
            char m[128], s[700];
            for (k = 0; k < nth; k++) if (tids[k] == ev.dwThreadId) th = threads[k];
            if ((code == EXCEPTION_SINGLE_STEP || code == 0x4000001E) && ptwatch_single_step(th, ev.dwThreadId)) break;
            if ((code == EXCEPTION_SINGLE_STEP || code == 0x4000001E /* STATUS_WX86_SINGLE_STEP */) && rearm
                && ev.dwThreadId == stepper) {
                wr8(rearm, 0xCC);
                rearm = 0;
                hold_others(stepper, 0);
                CONTEXT flags;ZeroMemory(&flags,sizeof flags);flags.ContextFlags=CONTEXT_CONTROL;
                if(th && GetThreadContext(th,&flags)){flags.EFlags&=~0x100;SetThreadContext(th,&flags);}
                break;
            }
            if (code != EXCEPTION_BREAKPOINT && code != 0x4000001F /* STATUS_WX86_BREAKPOINT */) {
                mod_of(a, m, sizeof m);
                /* opt-in triage: EE_BOOT_MINIDUMP=<file> writes one full-memory minidump at the first access
                   violation (the faulting thread is still stopped at the instruction) */
                if (code == 0xC0000005 && ev.u.Exception.dwFirstChance && !av_dumped && getenv("EE_BOOT_MINIDUMP")) {
                    HANDLE f = CreateFileA(getenv("EE_BOOT_MINIDUMP"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                           FILE_ATTRIBUTE_NORMAL, NULL);
                    av_dumped = 1;
                    if (f != INVALID_HANDLE_VALUE) {
                        BOOL ok = MiniDumpWriteDump(hProc, targetPid, f, MiniDumpWithFullMemory, NULL, NULL, NULL);
                        CloseHandle(f);
                        printf("minidump %s: %s (thread %lu)\n", getenv("EE_BOOT_MINIDUMP"), ok ? "written" : "failed",
                               (unsigned long)ev.dwThreadId);
                    }
                }
                if (!ev.u.Exception.dwFirstChance) {
                    printf("STOP crash: exception %08lx at %s (second chance)\n", (unsigned long)code, m);
                    done = 1;
                    break;
                }
                if (nexc++ < 20 || (stack_scan && code != 0xE06D7363)) {   /* --stack-scan: every non-C++ exception */
                    printf("exception %08lx at %s (first chance)\n", (unsigned long)code, m);
                    if (th) {   /* the top of the stack, for locating the caller of a bad jump */
                        c.ContextFlags = CONTEXT_FULL;
                        GetThreadContext(th, &c);
                        printf("   rax %016llx rcx %016llx rdx %016llx rbx %016llx\n   rsi %016llx rdi %016llx r8 %016llx r9 %016llx\n",
                               AV(c.Rax), AV(c.Rcx), AV(c.Rdx), AV(c.Rbx), AV(c.Rsi), AV(c.Rdi), AV(c.R8), AV(c.R9));
                        for (k = 0; k < 12; k++) {
                            mod_of(rdp(R_SP(&c) + PTR * k), m, sizeof m);
                            printf("   [%s+%02x] %s\n", PTR == 8 ? "rsp" : "esp", k * PTR, m);
                        }
                        for (k = 12; k < 12 + stack_scan; k++) {
                            TADDR v = rdp(R_SP(&c) + PTR * k);
                            if (v >= exeBase && v < exeBase + image_size(exeBase)) {
                                mod_of(v, m, sizeof m);
                                printf("   scan [%s+%03x] %s\n", PTR == 8 ? "rsp" : "esp", k * PTR, m);
                            }
                        }
                        if (stack_scan) {   /* x64: only where the code keeps a frame-pointer chain */
                            TADDR fp = R_FP(&c);
                            for (k = 0; k < 24 && fp > R_SP(&c) && fp < R_SP(&c) + 0x100000; k++) {
                                TADDR ret = rdp(fp + PTR);
                                mod_of(ret, m, sizeof m);
                                printf("   %s-chain %d %s\n", PTR == 8 ? "rbp" : "ebp", k, m);
                                fp = rdp(fp);
                            }
                        }
                    }
                }
                cont = DBG_EXCEPTION_NOT_HANDLED;
                break;
            }
            if (coverage_hit(a, th, ev.dwThreadId)) break;   /* TEST ONLY (cov): one-shot entry int3, restored + rewound */
            for (g = 0; g < nguards && guards[g].addr != a; g++);
            if (g == nguards || !th) break;   /* the loader's own breakpoint */
            c.ContextFlags = CONTEXT_FULL;
            GetThreadContext(th, &c);
            if(rearm && ev.dwThreadId!=stepper) {
                /* Another thread may have hit int3 before the OS froze it for the
                   first debug event. Its queued event can arrive during step-over
                   even though we now hold it suspended. Rewind it for a fresh
                   guarded hit AFTER the existing step completes; do not overwrite
                   rearm/stepper or add another suspension layer. */
                for(int t=0;t<nth;t++)if(tids[t]==ev.dwThreadId && !held[t]) {
                    if(SuspendThread(th)==(DWORD)-1){printf("STOP cannot defer concurrent guard\n");done=1;break;}
                    held[t]=1;
                }
                if(done)break;
                R_IP(&c)=a;c.EFlags&=~0x100;SetThreadContext(th,&c);
                printf("deferred concurrent guard breakpoint on thread %lu\n",(unsigned long)ev.dwThreadId);
                break;
            }
            mod_of(rdp(R_SP(&c)), m, sizeof m);
            if (guards[g].kind == G_DD_RETURN) {
                int pending=-1, remains=0;
                int type;
                BYTE saved=guards[g].orig;
                for (k=ndd_returns-1;k>=0;k--) if(dd_returns[k].at==a && dd_returns[k].tid==ev.dwThreadId) {pending=k;break;}
                if (pending<0) { printf("STOP unmatched DirectDraw return\n"); done=1; break; }
                type=dd_returns[pending].type;
                if(type==11) {
                    int thread=net_thread(ev.dwThreadId);if(thread>=0)net_creating[thread].depth--;
                    if(R_AX(&c)!=(TADDR)~(TADDR)0) {   /* INVALID_SOCKET */
                        int socket=net_socket(R_AX(&c));
                        if(socket<0 && nnet_sockets<128)socket=nnet_sockets++;
                        if(socket<0){printf("STOP network handle tracking full\n");done=1;break;}
                        net_sockets[socket].fd=R_AX(&c);net_sockets[socket].bound=0;
                    }
                    printf("loopback socket result %08lx\n",(unsigned long)R_AX(&c));
                } else if(type==12) {
                    int socket=net_socket(dd_returns[pending].out);
                    if((DWORD)R_AX(&c)==0 && socket>=0)net_sockets[socket].bound=1;
                    printf("loopback bind result %08lx\n",(unsigned long)(DWORD)R_AX(&c));
                } else if(type==8) {
                    if((LONG)(DWORD)R_AX(&c)>=0) {
                        if(ninput_handles>=32){printf("STOP input handle tracking full\n");done=1;break;}
                        input_handles[ninput_handles].handle=rdp(dd_returns[pending].out);
                        target_handle_name(input_handles[ninput_handles].handle,input_handles[ninput_handles].name,600);
                        if(!input_handles[ninput_handles].name[0]){printf("STOP cannot identify virtual input handle\n");done=1;break;}
                        ninput_handles++;
                    }
                } else if(type==9) {
                    printf("audio result EEInitializeSounds: %s\n",(R_AX(&c)&255)?"true":"false");
                    if(audio_checkpoint) {
                        printf((R_AX(&c)&255)?"STOP <windowed audio ready> reached\n":"STOP audio initialization failed\n");done=1;
                    }
                } else if (type>=3) {
                    printf("graphics result %s: %08lx\n",type==3 ? "CreateSurface" : type==4 ? "CreateDevice" : type==5 ? "ActivateScreen" : type==7 ? "SetCooperativeLevel" : "ShowScene",(unsigned long)(DWORD)R_AX(&c));
                    if(type==5 && game_windowed_capture && graphics_checkpoint && (DWORD)R_AX(&c)==0) {
                        printf("STOP <windowed graphics ready> reached\n");done=1;
                    }
                    if(type==6 && game_windowed_capture) {
                        if((DWORD)R_AX(&c)==0 && live_session) {
                            frame_presented=1;printf("checkpoint windowed frame presented\n");
                            for(int j=0;j<nguards;j++)if(guards[j].kind==G_RENDER_PRESENT) {
                                BYTE current=0;if(rd(guards[j].addr,&current,1) && current==0xcc)wr8(guards[j].addr,guards[j].orig);
                            }
                        } else {printf((DWORD)R_AX(&c)==0 ? "STOP <windowed frame presented> reached\n" : "STOP render failure\n");done=1;}
                    }
                } else if ((LONG)(DWORD)R_AX(&c)>=0) {
                    TADDR iface=rdp(dd_returns[pending].out), vt=rdp(iface);
                    if (!iface || !vt || !target_guard(rdp(vt),"IDirectDraw::QueryInterface",G_DD_QUERY)
                        || (type==1 && (!target_guard(rdp(vt+20*PTR),"IDirectDraw::SetCooperativeLevel",G_DD_COOP)
                            || !target_guard(rdp(vt+21*PTR),"IDirectDraw::SetDisplayMode",G_STOP)
                            || !target_guard(rdp(vt+6*PTR),"IDirectDraw::CreateSurface",G_DD_SURFACE)))
                        || (type==2 && !target_guard(rdp(vt+4*PTR),"IDirect3D7::CreateDevice",G_D3D_DEVICE))) {
                        printf("STOP cannot guard target DirectDraw interface\n");done=1;break;
                    }
                    printf("graphics target interface guarded: %s\n",type==1 ? "DirectDraw" : "Direct3D7");
                }
                dd_returns[pending]=dd_returns[--ndd_returns];
                for(k=0;k<ndd_returns;k++) remains|=dd_returns[k].at==a;
                wr8(a,saved);R_IP(&c)=a;
                if(remains) { c.EFlags|=0x100;rearm=a;stepper=ev.dwThreadId;hold_others(stepper,1); }
                else guards[g]=guards[--nguards];
                SetThreadContext(th,&c);
                break;
            }
            if (a == stopAddr) {
                printf("STOP %s reached\n", guards[g].name);
                dump(argc > 5 ? argv[5] : NULL, mode == 2);
                done = 1;
                break;
            }
            {
                /* arguments 1..3 of the guarded entry (pointer-sized; a DWORD argument is compared truncated) */
                TADDR arg1 = arg_n(&c, 1), arg2 = arg_n(&c, 2), arg3 = arg_n(&c, 3);
                int allow = 0;
                s[0] = 0;
                if (guards[g].kind==G_DD_CREATE || guards[g].kind==G_DD_CREATE_EX || guards[g].kind==G_DD_QUERY) {
                    TADDR iid=guards[g].kind==G_DD_QUERY ? arg2 : arg3;
                    int requested=guards[g].kind==G_DD_CREATE || draw_iid(iid), type=1;
                    if(guards[g].kind==G_DD_QUERY && d3d_iid(iid)) {requested=1;type=2;}
                    allow=!requested || watch_dd_return(&c,ev.dwThreadId,guards[g].kind==G_DD_QUERY ? arg3 : arg2,type);
                    if (!allow) printf("STOP cannot watch DirectDraw creation/query return\n");
                } else if(guards[g].kind==G_NET_SOCKET) {
                    int thread=net_thread(ev.dwThreadId);
                    if(thread<0 && nnet_creating<32){thread=nnet_creating++;net_creating[thread].tid=ev.dwThreadId;net_creating[thread].depth=0;}
                    allow=loopback_socket_allowed((DWORD)arg1,(DWORD)arg2,(DWORD)arg3) && thread>=0;
                    if(allow)allow=watch_dd_return(&c,ev.dwThreadId,0,11);
                    if(allow){net_creating[thread].depth++;printf("loopback socket family %lu type %lu\n",(unsigned long)arg1,(unsigned long)arg2);}
                    else printf("STOP unsupported socket creation\n");
                } else if(guards[g].kind==G_MP_ACCEPT) {   /* TEST ONLY (round 40, mp): accept on a verified loopback bind */
                    int socket=net_socket(arg1),thread=net_thread(ev.dwThreadId);
                    if(thread<0 && nnet_creating<32){thread=nnet_creating++;net_creating[thread].tid=ev.dwThreadId;net_creating[thread].depth=0;}
                    allow=socket>=0 && net_sockets[socket].bound && thread>=0;
                    if(allow)allow=watch_dd_return(&c,ev.dwThreadId,0,11);
                    if(allow)net_creating[thread].depth++;
                    printf("%s mp %s on verified loopback bind\n",allow?"TEST":"STOP",guards[g].name);
                } else if(guards[g].kind==G_NET_IOCTL) {
                    allow=loopback_ioctl_allowed((DWORD)arg2);
                    printf("%s WSAIoctl %08lx\n",allow?"loopback":"STOP",(unsigned long)(DWORD)arg2);
                } else if(guards[g].kind==G_NET_CLOSE) {
                    int socket=net_socket(arg1);if(socket>=0)net_sockets[socket]=net_sockets[--nnet_sockets];allow=1;
                } else if(guards[g].kind==G_NET_LISTEN) {
                    int socket=net_socket(arg1);allow=socket>=0 && net_sockets[socket].bound;
                    printf("%s listen on verified loopback bind\n",allow?"loopback":"STOP");
                } else if(guards[g].kind==G_NET_ADDRESS) {
                    TADDR address=arg2;DWORD length=(DWORD)arg3;
                    if(!strcmp(guards[g].name,"sendto")){address=arg_n(&c,5);length=(DWORD)arg_n(&c,6);}
                    if(!strcmp(guards[g].name,"WSASendTo")){address=arg_n(&c,6);length=(DWORD)arg_n(&c,7);}
                    DWORD prior=0;allow=loopback_address(address,length,&prior);
                    if(allow) {
                        printf("loopback %s address %08lx -> 127.0.0.1\n",guards[g].name,(unsigned long)prior);
                        if(mp_self_ip) {   /* TEST ONLY (round 40, mp): this process's loopback address / its peer's */
                            DWORD to=!strcmp(guards[g].name,"bind") || prior==mp_self_ip ? mp_self_ip : mp_peer_ip;
                            for(int k=0;k<nmp_more_ip && strcmp(guards[g].name,"bind");k++)if(prior==mp_more_ip[k])to=prior;   /* --mp-peers */
                            allow=wr32(address+4,to);
                            printf("TEST mp %s address %08lx -> %08lx\n",guards[g].name,(unsigned long)prior,(unsigned long)to);
                        }
                        if(allow && !strcmp(guards[g].name,"bind"))allow=watch_dd_return(&c,ev.dwThreadId,arg1,12);
                    }
                    if(!allow)printf("STOP unsupported network destination\n");
                } else if(guards[g].kind==G_NATIVE_ERROR) {
                    static struct {DWORD code;unsigned count;} errors[64];static int nerrors;
                    DWORD code=(DWORD)arg1;   /* ESRecordNativeError(unsigned long) */
                    int e;for(e=0;e<nerrors && errors[e].code!=code;e++);
                    if(e==nerrors && nerrors<64){errors[e].code=code;errors[e].count=0;nerrors++;}
                    if(e<64){unsigned n=++errors[e].count;if(n<=3 || !(n&(n-1)))printf("native error %08lx count %u (from %s)\n",(unsigned long)code,n,m);}
                    allow=1;
                } else if(guards[g].kind==G_EXPERIMENTAL_ENTRY) {
                    printf("experimental hit rva %08lx\n",(unsigned long)(a-exeBase));allow=1;
                } else if(guards[g].kind==G_RNG_TRACE) {
                    TADDR physics=rdp(rng_trace_physics);
                    if(physics && R_THIS(&c)==physics+rng_trace_offset) {
                        /* no frame-pointer chain in x64 code: the caller frame's return and first argument are not logged (0) */
                        printf("rng %lu ret " AFMT " up %08lx tid %lu arg %08lx id %ld\n",(unsigned long)rd32(R_THIS(&c)),AV(rdp(R_SP(&c))),
                               0UL,(unsigned long)ev.dwThreadId,0UL,0L);
                    }
                    allow=1;
                } else if(guards[g].kind==G_PT_SYNC) {
                    ptsync_hit(); ptwatch_tick(pth.last_key); allow=1;
                } else if(guards[g].kind==G_PT_WATCH) {
                    ptwatch_hit(a,&c,ev.dwThreadId); allow=1;
                } else if(guards[g].kind==G_MUTEX_BYPASS) {
                    DWORD last=(DWORD)R_AX(&c);
                    if(last==183) R_AX(&c)=0;   /* ERROR_ALREADY_EXISTS: another (test) instance holds the mutex */
                    printf("TEST mutex bypass at rva %08lx: GetLastError %lu%s\n",(unsigned long)(a-exeBase),(unsigned long)last,
                           last==183?" (ERROR_ALREADY_EXISTS) -> 0, instance proceeds":last?" (unexpected; left as is)":" (first instance; not needed)");
                    allow=1;
                } else if(guards[g].kind==G_MP_SKIP) {   /* TEST ONLY (round 40, mp): return at the entry (cdecl, no args) */
                    TADDR ret=rdp(R_SP(&c));
                    printf("TEST mp skip at rva %08lx (EEStartExternalLobby: external lobby not opened) ret rva %08lx tid %lu\n",
                           (unsigned long)(a-exeBase),(unsigned long)(ret-exeBase),(unsigned long)ev.dwThreadId);
                    fake_return(&c,0,KEEP_RESULT);   /* cdecl, no arguments */
                    if(!SetThreadContext(th,&c)){printf("STOP mp skip failed\n");done=1;}
                    break;
                } else if(guards[g].kind==G_NHD_OUT || guards[g].kind==G_NHD_RESULT) {
                    int out=guards[g].kind==G_NHD_OUT;
                    /* Fn1391F2(bool victorious, player) / Fn16AF5C(state): member arguments after this */
                    DWORD m1=(DWORD)marg_n(&c,1),m2=(DWORD)marg_n(&c,2);
                    DWORD current=rd32(nhd_world+nhd_current_off);TADDR local=rdp(nhd_world+nhd_players_off+PTR*current),ret=rdp(R_SP(&c));
                    int skip=out ? (R_THIS(&c)==nhd_world && m2==current && !(m1&0xff)) : (local && R_THIS(&c)==local && m1==2);
                    if(nhd_players_mask) {   /* TEST ONLY (round 40, mp): the protected set instead of the local player */
                        skip=0;
                        if(out)skip=R_THIS(&c)==nhd_world && m2<32 && ((nhd_players_mask>>m2)&1) && !(m1&0xff);
                        else if(m1==2)for(DWORD p=0;p<17 && p<32;p++)if(((nhd_players_mask>>p)&1) && rdp(nhd_world+nhd_players_off+PTR*p)==R_THIS(&c))skip=1;
                    }
                    DWORD id=out ? m2 : 0xffffffff;
                    if(!out)for(DWORD p=0;p<17;p++)if(rdp(nhd_world+nhd_players_off+PTR*p)==R_THIS(&c)){id=p;break;}
                    printf("TEST no-human-defeat %s %s player %ld (%s %lu; local %lu) ret rva %08lx PT %lu%s\n",skip?"skip":"pass",
                           out?"out Fn1391F2":"result Fn16AF5C",(long)id,out?"victorious":"state",
                           (unsigned long)(out?(m1&0xff):m1),(unsigned long)current,(unsigned long)(ret-exeBase),
                           pth.keyed?(unsigned long)pth.last_key:0UL,skip?"":" (not the local player's defeat)");
                    if(skip) {   /* return at the entry (plain ret); the guard stays armed */
                        fake_return(&c,out?2:1,KEEP_RESULT);
                        if(!SetThreadContext(th,&c)){printf("STOP no-human-defeat skip failed\n");done=1;}
                        break;
                    }
                    allow=1;
                } else if(guards[g].kind==G_REG_OPEN || guards[g].kind==G_REG_OPEN_EX) {
                    /* NtOpenKey(&h, access, oa) / NtOpenKeyEx(&h, access, oa, options) */
                    char root[600] = "", nm[600] = "", full[1200];
                    int redirected;
                    target_handle_name(oa_root(arg3), root, sizeof root);
                    ustr(oa_name(arg3), nm, sizeof nm);
                    snprintf(full, sizeof full, "%s%s%s", root, root[0] ? "\\" : "", nm);
                    redirected = reg_instance_open(full, &c, arg1, guards[g].kind == G_REG_OPEN ? 3 : 4);
                    if (redirected) {
                        if (redirected < 0 || !SetThreadContext(th, &c)) { printf("STOP instance registry redirection failed\n"); done = 1; }
                        break;   /* handled the call; its entry guard stays armed */
                    }
                    allow = 1;   /* any other open: unchanged (opens are not policed) */
                } else if(guards[g].kind==G_MP_CHECKSUM) {   /* TEST ONLY (round 40, mpengine): eax += delta */
                    DWORD old=(DWORD)R_AX(&c);R_AX(&c)=(DWORD)(old+mp_checksum_delta);
                    printf("TEST mp checksum at rva %08lx: gBinaryChecksum %08lx -> %08lx (version suffix .%04lu -> .%04lu)\n",
                           (unsigned long)(a-exeBase),(unsigned long)old,(unsigned long)(DWORD)R_AX(&c),(unsigned long)(old&0x1fff),
                           (unsigned long)(R_AX(&c)&0x1fff));
                    allow=1;
                } else if(guards[g].kind==G_SEED_PIN) {
                    TADDR at=R_THIS(&c)+seed_pin_offset;DWORD old=rd32(at);
                    allow=R_THIS(&c) && wr32(at,seed_pin_value);
                    printf("%s seed pin write at " AFMT " (this " AFMT ") old %lu new %lu\n",allow?"TEST":"STOP",
                           AV(at),AV(R_THIS(&c)),(unsigned long)old,(unsigned long)seed_pin_value);
                } else if(guards[g].kind==G_AUDIO_INITIALIZE) {
                    allow=watch_dd_return(&c,ev.dwThreadId,0,9);
                    if(!allow)printf("STOP cannot watch audio initialization result\n");
                } else if(guards[g].kind==G_DD_SURFACE || guards[g].kind==G_D3D_DEVICE
                          || guards[g].kind==G_RENDER_ACTIVATE || guards[g].kind==G_RENDER_PRESENT) {
                    int type=guards[g].kind==G_DD_SURFACE ? 3 : guards[g].kind==G_D3D_DEVICE ? 4 : guards[g].kind==G_RENDER_ACTIVATE ? 5 : 6;
                    if(type==3) printf("ddsurface caps %08lx dimensions %lux%lu depth %lu\n",(unsigned long)rd32(arg2+offsetof(DDSURFACEDESC2,ddsCaps)),
                                       (unsigned long)rd32(arg2+offsetof(DDSURFACEDESC2,dwWidth)),(unsigned long)rd32(arg2+offsetof(DDSURFACEDESC2,dwHeight)),
                                       (unsigned long)rd32(arg2+offsetof(DDSURFACEDESC2,ddpfPixelFormat)+offsetof(DDPIXELFORMAT,dwRGBBitCount)));
                    if(type==4) printf("d3d device GUID first word %08lx\n",(unsigned long)rd32(arg2));
                    allow=watch_dd_return(&c,ev.dwThreadId,0,type);
                    if(!allow)printf("STOP cannot watch graphics result\n");
                } else if(guards[g].kind==G_DEVICE_CONTROL || guards[g].kind==G_WRITE_FILE) {
                    int tracked=-1;
                    for(k=0;k<ninput_handles;k++)if(input_handles[k].handle==arg1){
                        char current[600];target_handle_name(arg1,current,sizeof current);
                        if(!strcmp(current,input_handles[k].name))tracked=k;
                        else input_handles[k]=input_handles[--ninput_handles];
                        break;
                    }
                    allow=1;
                    if(tracked>=0 && guards[g].kind==G_WRITE_FILE)allow=0;
                    if(tracked>=0 && guards[g].kind==G_DEVICE_CONTROL) {
                        DWORD ctl=(DWORD)arg_n(&c,6);   /* NtDeviceIoControlFile IoControlCode */
                        /* Public hidclass.h GET operations, plus per-handle queue sizing/flush.
                           SET_FEATURE, output reports, device/driver settings stay denied. */
                        const DWORD gets[]={0xb0190,0xb0198,0xb01a0,0xb01a4,0xb01a8,0xb0193,0xb0197,
                                            0xb0192,0xb019a,0xb019e,0xb01a2,0xb01ba,0xb01be,0xb01c2,0xb01e2,0xb01e6};
                        unsigned q;allow=0;for(q=0;q<sizeof gets/sizeof *gets;q++)allow|=ctl==gets[q];
                        printf("%s read-only virtual input ioctl %08lx\n",allow?"input":"STOP",(unsigned long)ctl);
                    }
                    if(!allow && guards[g].kind==G_WRITE_FILE)printf("STOP write to virtual input device\n");
                } else if (guards[g].kind == G_FILE_CREATE || guards[g].kind == G_FILE_OPEN) {
                    /* NtCreateFile(.., CreateDisposition 8, CreateOptions 9, ..) / NtOpenFile(.., OpenOptions 6) */
                    DWORD disp = guards[g].kind == G_FILE_CREATE ? (DWORD)arg_n(&c, 8) : 1;
                    DWORD opts = (DWORD)arg_n(&c, guards[g].kind == G_FILE_CREATE ? 9 : 6);
                    objname(arg3, s, sizeof s);
                    if(windowed_test && disp==1 && !(opts&0x1000)
                        && !_strnicmp(s,"\\??\\hid#vid_0627&pid_0001#",strlen("\\??\\hid#vid_0627&pid_0001#"))) {
                        arg2 &= ~WRITEISH;
                        /* returning into ARM64 code (CHPE KernelBase), the handle cannot be watched: the
                           read-only access mask still holds, only the ioctl allow-list goes untracked */
                        int untracked=chpe_arm64(rdp(R_SP(&c)));
                        if(!set_arg(&c,2,arg2) || (!untracked && !watch_dd_return(&c,ev.dwThreadId,arg1,8))) {
                            printf("STOP cannot constrain virtual input access\n");done=1;break;
                        }
                        printf("input narrowed virtual tablet access to read-only%s\n",
                               untracked?" (handle untracked: returns into ARM64 code)":"");
                    }
                    int ro = !(arg2 & WRITEISH) && disp == 1 && !(opts & 0x1000 /* DELETE_ON_CLOSE */);
                    allow = ro || in_sandbox(s);
                    if(live_session && disp==1 && !(opts&0x1000) && local_uia_pipe(s))allow=1;
                    if(loopback_test && !_stricmp(s,"\\Device\\Afd\\Endpoint")) {
                        int thread=net_thread(ev.dwThreadId);allow=thread>=0 && net_creating[thread].depth>0;
                        printf("%s AFD endpoint within guarded socket creation\n",allow?"loopback":"STOP");
                    }
                    if(!allow && mp_self_ip && guards[g].kind==G_FILE_CREATE && !_stricmp(s,"\\Device\\RasAcd")) {
                        /* TEST ONLY (round 40, mp): refuse the autodial helper open of gethostbyname; logged */
                        fake_return(&c,11,0xC0000022);   /* NtCreateFile -> STATUS_ACCESS_DENIED */
                        printf("TEST mp refused NtCreateFile %s (gethostbyname autodial helper, from %s): STATUS_ACCESS_DENIED\n",s,m);
                        if(!SetThreadContext(th,&c)){printf("STOP mp refusal failed\n");done=1;}
                        break;
                    }
                    if(!allow && shader_cache(s)) {
                        /* the D3D12 runtime's shader cache (the renderer runs on wgpu's DX12 backend): refused, so
                           nothing persistent is written; D3D12 then works without a disk cache. Logged. */
                        fake_return(&c,guards[g].kind==G_FILE_CREATE?11:6,0xC0000022);   /* STATUS_ACCESS_DENIED */
                        printf("refused %s %s (D3D shader cache, from %s): STATUS_ACCESS_DENIED\n",guards[g].name,s,m);
                        if(!SetThreadContext(th,&c)){printf("STOP shader cache refusal failed\n");done=1;}
                        break;
                    }
                    printf("%s %s access %08lx disp %lu: %s (from %s)\n", ro ? "open" : allow ? "write" : "STOP",
                           guards[g].name,
                           (unsigned long)(DWORD)arg2, (unsigned long)disp, s, m);
                } else if (guards[g].kind == G_WINDOW) {
                    /* NtUserCreateWindowEx(exstyle, LARGE_STRING *class, *clsversion, *name, style, x, y, cx, cy,
                       parent, ...): message-only windows (parent HWND_MESSAGE, e.g. OLE's) are allowed and logged;
                       any other window stops the run (the boot milestone "first window") */
                    char cls[256] = "", name[256] = "";
                    DWORD parent = (DWORD)arg_n(&c, 10), style = (DWORD)arg_n(&c, 5);   /* an HWND's significant 32 bits */
                    int k2;
                    for (k2 = 0; k2 < 2; k2++) {
                        /* LARGE_STRING {ULONG Length; ULONG MaximumLength:31, bAnsi:1; PVOID Buffer} (native layout) */
                        TADDR ls = arg_n(&c, k2 ? 4 : 2), buf; DWORD len, flags;
                        char *out = k2 ? name : cls;
                        if (!ls) continue;
                        if (ls < 0x10000) { sprintf(out, "#%lu", (unsigned long)ls); continue; }
                        len = rd32(ls); flags = rd32(ls + 4); buf = rdp(ls + 8);
                        if (len > 254) len = 254;
                        if (flags & 0x80000000u) { rd(buf, out, len); out[len] = 0; }
                        else {
                            WCHAR w[128] = {0}; int q;
                            rd(buf, w, len > 254 ? 254 : len);
                            for (q = 0; q < (int)len / 2 && w[q]; q++) out[q] = w[q] < 128 ? (char)w[q] : '?';
                            out[q] = 0;
                        }
                    }
                    /* owner decision 2026-10-23: windows are allowed (a window is not persistent: it dies with the
                       process, and the harness checks that none is left) */
                    allow = 1;
                    printf("window CreateWindow class \"%s\" name \"%s\" style %08lx parent %08lx (from %s)\n",
                           cls, name, (unsigned long)style, (unsigned long)parent, m);
                    /* the renderer (GX on wgpu) never takes the display (no exclusive mode, no mode change; it
                       presents into the window), so the game window is the windowed capture */
                    if (windowed_test && !strcmp(cls, "Empire Earth Expansion") && !game_windowed_capture) {
                        game_windowed_capture = 1;
                        printf("checkpoint game window uses nonexclusive capture\n");
                    }
                } else if (guards[g].kind == G_REG_CREATE) {
                    /* NtCreateKey(&h, access, oa, titleindex, class, options, &disposition) */
                    char root[600] = "", nm[600] = "", full[1200];
                    int cls;
                    target_handle_name(oa_root(arg3), root, sizeof root);
                    ustr(oa_name(arg3), nm, sizeof nm);
                    snprintf(full, sizeof full, "%s%s%s", root, root[0] ? "\\" : "", nm);
                    {
                        int redirected = reg_instance_create(full, &c, arg1, arg_n(&c, 7), (DWORD)arg_n(&c, 6));
                        if (redirected) {
                            if (redirected < 0 || !SetThreadContext(th, &c)) {
                                printf("STOP instance registry redirection failed\n");
                                done = 1;
                            }
                            break;
                        }
                    }
                    {
                        int redirected = redirect_ddraw_key(full, &c, arg1, arg_n(&c, 7));
                        if (redirected) {
                            if (redirected < 0 || !SetThreadContext(th, &c)) {
                                printf("STOP DirectDraw bookkeeping redirection failed\n");
                                done = 1;
                            } else {
                                printf("regredirect %s -> snapshot-restored HKCU test subtree\n", full);
                            }
                            break; /* handled NtCreateKey return; keep its entry guard armed */
                        }
                    }
                    cls = reg_class(full);
                    if (cls) {
                        allow = 1;
                        printf("regcreate %s (access %08lx)\n", full, (unsigned long)(DWORD)arg2);
                    } else if (!(arg2 & KEY_WRITEISH) && reg_exists(full)) {
                        allow = 1;   /* an existing key opened read-only: nothing is created */
                        printf("regopen %s (from %s)\n", full, m);
                    } else
                        printf("STOP NtCreateKey %s access %08lx (from %s)\n", full, (unsigned long)(DWORD)arg2, m);
                } else if (guards[g].kind == G_REG_HANDLE) {
                    /* NtSetValueKey(h, name, ...) / NtDeleteValueKey(h, name) / NtDeleteKey(h) */
                    char key[600] = "", val[300] = "";
                    target_handle_name(arg1, key, sizeof key);
                    if (strcmp(guards[g].name, "NtDeleteKey")) ustr(arg2, val, sizeof val);
                    allow = reg_class(key) == 2;
                    if (allow && !strcmp(guards[g].name, "NtSetValueKey")) {
                        DWORD type = (DWORD)arg_n(&c, 4), size = (DWORD)arg_n(&c, 6); TADDR data = arg_n(&c, 5);
                        printf("regset %s [%s] type %lu size %lu%s", key, val, (unsigned long)type, (unsigned long)size,
                               type == REG_DWORD ? "" : "\n");
                        if (type == REG_DWORD) printf(" = %lu\n", (unsigned long)rd32(data));
                    } else
                        printf("%s %s %s [%s] (from %s)\n", allow ? "regwrite" : "STOP", guards[g].name, key, val, m);
                } else if (guards[g].kind == G_DD_COOP) {
                    /* SetCooperativeLevel(this, hwnd, flags): exclusive/fullscreen would take the display */
                    allow = !((DWORD)arg3 & (DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE));
                    if(windowed_test && allow) {
                        char cls[128]={0};GetClassNameA((HWND)arg2,cls,sizeof cls);
                        if(!strcmp(cls,"Empire Earth Expansion")) {game_windowed_capture=1;printf("checkpoint game window uses nonexclusive capture\n");}
                        allow=watch_dd_return(&c,ev.dwThreadId,0,7);
                    }
                    printf("%s IDirectDraw::SetCooperativeLevel flags %08lx (from %s)\n", allow ? "ddcoop" : "STOP",
                           (unsigned long)(DWORD)arg3, m);
                } else if (guards[g].kind == G_SETINFO) {
                    DWORD cls = (DWORD)arg_n(&c, 5);
                    /* only a handle opened with write/delete access can change a file, and those opens are
                       confined to the staging directory by the guard above */
                    allow = 1;
                    if (!allow) printf("STOP NtSetInformationFile class %lu (from %s)\n", (unsigned long)cls, m);
                } else if (guards[g].kind == G_SPI) {
                    /* the GET actions the CRT/USER init and games use; anything else may change system state */
                    static const DWORD gets[] = {0x03, 0x0A, 0x0E, 0x10, 0x16, 0x1F, 0x26, 0x29, 0x30, 0x32,
                                                 0x34, 0x35, 0x3A, 0x48, 0x4A, 0x5C, 0x68, 0x1002, 0x1012, 0x100A,
                                                 0x2000, 0x2002, 0x200C, 0x2012, 0x0068,
                                                 0x42};   /* SPI_GETHIGHCONTRAST (round 40, mpengine): user32 itself asks
                                                             in every running game at once on a desktop-wide settings change;
                                                             it stopped four MP sessions mid-game at the same instant */
                    unsigned i;
                    for (i = 0; i < sizeof gets / sizeof *gets; i++) allow |= (DWORD)arg1 == gets[i];
                    printf("%s SystemParametersInfo action %#lx (from %s)\n", allow ? "spi " : "STOP",
                           (unsigned long)(DWORD)arg1, m);
                } else if (guards[g].kind == G_MSGBOX_A || guards[g].kind == G_MSGBOX_W) {
                    char t[512] = "", cap[256] = "";
                    if (guards[g].kind == G_MSGBOX_A) { rd(arg2, t, sizeof t - 1); rd(arg3, cap, sizeof cap - 1); }
                    else {
                        WCHAR w[512] = {0}; int k2;
                        rd(arg2, w, sizeof w - 2);
                        for (k2 = 0; w[k2] && k2 < 511; k2++) t[k2] = w[k2] < 128 ? (char)w[k2] : '?';
                    }
                    printf("STOP %s: \"%s\" [%s] (from %s)\n", guards[g].name, t, cap, m);
                } else {
                    if(!strcmp(guards[g].name,"abort")) {
                        for(k=0;k<24;k++){char caller[128];mod_of(rdp(R_SP(&c)+PTR*k),caller,sizeof caller);printf("  abort stack+%02x %s\n",k*PTR,caller);}
                    }
                    if (!strncmp(guards[g].name, "NtCreateKey", 11) || !strcmp(guards[g].name, "NtCreateFile"))
                        objname(arg3, s, sizeof s);
                    else if (!strcmp(guards[g].name, "NtSetValueKey") || !strcmp(guards[g].name, "NtDeleteValueKey"))
                        ustr(arg2, s, sizeof s);
                    printf("STOP %s %s (from %s)\n", guards[g].name, s, m);
                }
                if (!allow) { done = 1; break; }
                if (trace_once && guards[g].kind == G_EXPERIMENTAL_ENTRY) {   /* opt-in: first entry only, no re-arm */
                    wr8(a, guards[g].orig); R_IP(&c) = a; SetThreadContext(th, &c);
                    guards[g] = guards[--nguards];
                    break;
                }
                /* step over: original byte back, eip back to the call, trap flag; re-arm on the single step */
                wr8(a, guards[g].orig);
                R_IP(&c) = a;
                c.EFlags |= 0x100;
                SetThreadContext(th, &c);
                rearm = a;
                stepper = ev.dwThreadId;
                hold_others(stepper, 1);
            }
            break;
        }
        case EXIT_PROCESS_DEBUG_EVENT:
            printf("STOP exit: process exited with %lu\n", (unsigned long)ev.u.ExitProcess.dwExitCode);
            done = 2;
            break;
        case OUTPUT_DEBUG_STRING_EVENT: {
            char t[256] = "";
            WORD n = ev.u.DebugString.nDebugStringLength;
            if (!ev.u.DebugString.fUnicode) {
                rd((TADDR)ev.u.DebugString.lpDebugStringData, t, n < sizeof t - 1 ? n : sizeof t - 1);
                printf("debugstring %s", t);
                if (!strchr(t, '\n')) printf("\n");
            }
            break;
        }
        default:
            break;
        }
        if (done == 2) {
            ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
            break;
        }
        if (done) {
            if (!mode) {   /* run: what is on screen when it stops */
                EnumWindows(list_win, 1);
                screenshot(bigWin, argc > 5 ? argv[5] : NULL);
            }
            TerminateProcess(hProc, 1);
            ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
            /* drain until the exit event so nothing of the process survives */
            while (WaitForDebugEvent(&ev, 5000)) {
                ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
                if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) break;
            }
            break;
        }
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, cont);
    }
    if (!done) {   /* hard timeout */
        if (!mode) {
            EnumWindows(list_win, 1);
            screenshot(bigWin, argc > 5 ? argv[5] : NULL);
        }
        TerminateProcess(hProc, 1);
        while (WaitForDebugEvent(&ev, 5000)) {
            ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
            if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) break;
        }
    }
    coverage_write("final");   /* TEST ONLY (cov) */
    WaitForSingleObject(hProc, 10000);
    nwin = 0;
    EnumWindows(list_win, 0);
    {
        int gone = WaitForSingleObject(hProc, 0) == WAIT_OBJECT_0;
        printf("process gone: %s; windows left: %d\n", gone ? "yes" : "NO", nwin);
        CloseHandle(pi.hThread);
        CloseHandle(hProc);
        return nwin || !gone ? 3 : 0;
    }
}
