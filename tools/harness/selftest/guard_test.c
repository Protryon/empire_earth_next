/* TEST ONLY: the target of the harness self test (selftest.py). Built for x86_64 with mingw64 gcc; run only
 * under exe_boot.exe in build/boot/selftest_stage. EE_SELFTEST_CASE names the case: each calls the guarded
 * APIs the case is about and reports what came back with OutputDebugString ("SELFTEST ..."), which the harness logs.
 * Stand-ins for the game functions the harness hooks by RVA (seed pin, no-human-defeat, mutex bypass, mp skip/checksum,
 * rng trace, the static exe's engine/runtime/renderer checkpoints) are defined here; selftest.py finds them with nm.
 * Paths and keys outside the stage are the self test's own (EE-harness-selftest*), never the game's.
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ddraw.h>

#define HOOK __attribute__((noinline, used))

static void say(const char *fmt, ...)
{
    char text[600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    char line[640];
    snprintf(line, sizeof line, "SELFTEST %s\n", text);
    OutputDebugStringA(line);
}

/* ---- stand-ins for the hooked game functions ---- */
typedef struct { void *pad[5]; unsigned long seed; } Loading;                 /* seed at 5 pointers */
typedef struct { void *vt; long state; } Player;
typedef struct { void *pad[2]; Player *players[17]; unsigned long current; } World;   /* players at 2 pointers */
typedef struct { unsigned long seed; } Random;
typedef struct { void *pad[6]; Random random; } Physics;                       /* random at 6 pointers */
World selftest_world;
Player selftest_players[4];
Physics selftest_physics_object;
Physics *selftest_physics = &selftest_physics_object;
volatile long selftest_out_calls, selftest_result_calls, selftest_skip_calls;

HOOK void selftest_execute(Loading *self) { say("seed %lu", self->seed); }
HOOK void selftest_out(World *self, unsigned char victorious, unsigned long player)
{
    (void)self;
    selftest_out_calls++;
    say("out ran (victorious %u player %lu)", victorious, player);
}
HOOK void selftest_result(Player *self, long state) { selftest_result_calls++; self->state = state; say("result ran (state %ld)", state); }
HOOK float selftest_generate(Random *self) { self->seed = self->seed * 1103515245u + 12345u; return (float)(self->seed >> 8); }
HOOK unsigned long selftest_next(Random *self) { return ++self->seed; }
HOOK void selftest_skip(void) { selftest_skip_calls++; say("skip target ran"); }
HOOK unsigned char selftest_sounds(void) { return 1; }                          /* EEInitializeSounds */
HOOK long selftest_activate(void *self, unsigned long a, void *b, void *c, unsigned long d, unsigned char e, unsigned long f)
{ (void)self; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return 0; }   /* ActivateScreen */
HOOK long selftest_show(void *self, unsigned long a) { (void)self; (void)a; return 0; }   /* ShowScene */
HOOK unsigned long selftest_native_error(unsigned long code) { return code; }   /* ESRecordNativeError */
HOOK void selftest_abort(void) { say("abort ran"); ExitProcess(3); }
HOOK int selftest_traced(int x) { return x + 1; }
HOOK int selftest_covered(int x) { return x * 2; }
HOOK int selftest_main_stand_in(void) { return 0; }                            /* --winmain-rva */
/* never called: game_session.mutex_bypass_rva's x64 search runs on its body (call [rip+GetLastError]; test eax,eax) */
HOOK int selftest_instance_mutex(void)
{
    HANDLE m = CreateMutexA(NULL, FALSE, NULL);
    int first = GetLastError() == 0;
    CloseHandle(m);
    return first;
}
/* the mutex bypass guards the `test eax,eax` after GetLastError; the mp checksum guard the store of eax */
unsigned long selftest_mutex_probe(unsigned long last);
unsigned long selftest_checksum_probe(unsigned long value);
__asm__(".text\n.globl selftest_mutex_probe\nselftest_mutex_probe:\n mov %ecx,%eax\n.globl selftest_mutex_at\n"
        "selftest_mutex_at:\n test %eax,%eax\n ret\n"
        ".globl selftest_checksum_probe\nselftest_checksum_probe:\n mov %ecx,%eax\n.globl selftest_checksum_at\n"
        "selftest_checksum_at:\n ret\n");
/* calls through volatile pointers: never inlined or folded */
static void (*volatile pExecute)(Loading *) = selftest_execute;
static void (*volatile pOut)(World *, unsigned char, unsigned long) = selftest_out;
static void (*volatile pResult)(Player *, long) = selftest_result;
static float (*volatile pGenerate)(Random *) = selftest_generate;
static unsigned long (*volatile pNext)(Random *) = selftest_next;
static void (*volatile pSkip)(void) = selftest_skip;
static unsigned char (*volatile pSounds)(void) = selftest_sounds;
static long (*volatile pActivate)(void *, unsigned long, void *, void *, unsigned long, unsigned char, unsigned long) = selftest_activate;
static long (*volatile pShow)(void *, unsigned long) = selftest_show;
static unsigned long (*volatile pNative)(unsigned long) = selftest_native_error;
static void (*volatile pAbort)(void) = selftest_abort;
static int (*volatile pTraced)(int) = selftest_traced;
static int (*volatile pCovered)(int) = selftest_covered;
static unsigned long (*volatile pMutex)(unsigned long) = selftest_mutex_probe;
static unsigned long (*volatile pChecksum)(unsigned long) = selftest_checksum_probe;

/* ---- helpers ---- */
static char stage[MAX_PATH], outside[MAX_PATH], victim[MAX_PATH];

static void paths(void)
{
    GetCurrentDirectoryA(sizeof stage, stage);
    snprintf(outside, sizeof outside, "%s\\..\\selftest_outside.txt", stage);
    snprintf(victim, sizeof victim, "%s\\..\\selftest_victim.txt", stage);
}

typedef struct { USHORT Length, MaximumLength; PWSTR Buffer; } USTR;
typedef struct { ULONG Length; HANDLE RootDirectory; USTR *ObjectName; ULONG Attributes; PVOID sd, sqos; } OA;
typedef struct { union { LONG Status; PVOID Pointer; }; ULONG_PTR Information; } IOSB;
typedef LONG (WINAPI *NtOpenFile_t)(PHANDLE, ACCESS_MASK, OA *, IOSB *, ULONG, ULONG);
typedef LONG (WINAPI *NtDeleteFile_t)(OA *);
typedef LONG (WINAPI *NtCreateFile_t)(PHANDLE, ACCESS_MASK, OA *, IOSB *, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);

static void native_path(const char *path, WCHAR *w, USTR *u, OA *oa)
{
    char full[MAX_PATH];
    int k;
    GetFullPathNameA(path, sizeof full, full, NULL);
    wcscpy(w, L"\\??\\");
    for (k = 0; full[k]; k++) w[4 + k] = (BYTE)full[k];
    w[4 + k] = 0;
    u->Length = u->MaximumLength = (USHORT)(2 * wcslen(w)); u->Buffer = w;
    memset(oa, 0, sizeof *oa); oa->Length = sizeof *oa; oa->ObjectName = u; oa->Attributes = 0x40;
}

static FARPROC nt(const char *name) { return GetProcAddress(GetModuleHandleA("ntdll.dll"), name); }

static int port_of(SOCKET s)
{
    struct sockaddr_in a; int n = sizeof a;
    return getsockname(s, (struct sockaddr *)&a, &n) ? -1 : ntohs(a.sin_port);
}

static const char *ip_of(SOCKET s)
{
    static char text[32];
    struct sockaddr_in a; int n = sizeof a;
    if (getsockname(s, (struct sockaddr *)&a, &n)) return "?";
    snprintf(text, sizeof text, "%u.%u.%u.%u", a.sin_addr.S_un.S_un_b.s_b1, a.sin_addr.S_un.S_un_b.s_b2,
             a.sin_addr.S_un.S_un_b.s_b3, a.sin_addr.S_un.S_un_b.s_b4);
    return text;
}

static void wsa(void) { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }

static HWND game_window(const char *cls)
{
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = cls;
    RegisterClassA(&wc);
    return CreateWindowExA(0, cls, "selftest window", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, NULL, NULL, wc.hInstance, NULL);
}

static const char *REG_ROOT = "Software\\EE-harness-selftest";

/* ---- cases ---- */
static int run(const char *c)
{
    say("begin %s", c);
    if (!strcmp(c, "file_read")) {
        HANDLE h = CreateFileA("C:\\Windows\\win.ini", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        say("file_read %s", h != INVALID_HANDLE_VALUE ? "opened" : "failed");
        CloseHandle(h);
    } else if (!strcmp(c, "file_write_in")) {
        DWORD n = 0;
        char in1[MAX_PATH + 16], in2[MAX_PATH + 16];   /* absolute: a handle-relative open is not "below the stage" */
        snprintf(in1, sizeof in1, "%s\\inside.txt", stage); snprintf(in2, sizeof in2, "%s\\inside2.txt", stage);
        HANDLE h = CreateFileA(in1, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        BOOL w = WriteFile(h, "selftest\r\n", 10, &n, NULL);
        CloseHandle(h);
        BOOL moved = MoveFileExA(in1, in2, MOVEFILE_REPLACE_EXISTING);
        BOOL del = DeleteFileA(in2);
        say("file_write_in create %d write %d moved %d deleted %d", h != INVALID_HANDLE_VALUE, w, moved, del);
    } else if (!strcmp(c, "file_write_out")) {
        HANDLE h = CreateFileA(outside, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        say("file_write_out NOT STOPPED %p", (void *)h);
    } else if (!strcmp(c, "file_delete_on_close")) {
        HANDLE h = CreateFileA(victim, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                               FILE_FLAG_DELETE_ON_CLOSE, NULL);
        say("file_delete_on_close NOT STOPPED %p", (void *)h);
    } else if (!strcmp(c, "ntopen_write_out")) {
        WCHAR w[MAX_PATH + 8]; USTR u; OA oa; IOSB io; HANDLE h = NULL;
        native_path(victim, w, &u, &oa);
        LONG s = ((NtOpenFile_t)nt("NtOpenFile"))(&h, FILE_WRITE_DATA | SYNCHRONIZE, &oa, &io, 0, 0x20);
        say("ntopen_write_out NOT STOPPED %08lx", (unsigned long)s);
    } else if (!strcmp(c, "nt_delete_file")) {
        WCHAR w[MAX_PATH + 8]; USTR u; OA oa;
        native_path(victim, w, &u, &oa);
        LONG s = ((NtDeleteFile_t)nt("NtDeleteFile"))(&oa);
        say("nt_delete_file NOT STOPPED %08lx", (unsigned long)s);
    } else if (!strcmp(c, "shader_cache")) {
        char path[MAX_PATH];
        snprintf(path, sizeof path, "%s\\D3DSCache\\ee_selftest\\cache.bin", getenv("LOCALAPPDATA"));
        HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        say("shader_cache handle %s error %lu", h == INVALID_HANDLE_VALUE ? "invalid" : "VALID", GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    } else if (!strcmp(c, "reg_write_in")) {
        HKEY k; DWORD v = 42; char key[200];
        snprintf(key, sizeof key, "%s\\EE-AOC\\selftest", REG_ROOT);
        LONG r = RegCreateKeyExA(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
        LONG s = r ? r : RegSetValueExA(k, "Value", 0, REG_DWORD, (BYTE *)&v, 4);
        LONG d = r ? r : RegDeleteValueA(k, "Value");
        LONG s2 = r ? r : RegSetValueExA(k, "Kept", 0, REG_DWORD, (BYTE *)&v, 4);
        if (!r) RegCloseKey(k);
        say("reg_write_in create %ld set %ld delete %ld set %ld", r, s, d, s2);
    } else if (!strcmp(c, "reg_write_out")) {
        HKEY k;
        LONG r = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\EE-harness-selftest-outside\\x", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL);
        say("reg_write_out NOT STOPPED %ld", r);
    } else if (!strcmp(c, "reg_open_ro")) {
        HKEY k;
        LONG r = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft", 0, NULL, 0, KEY_READ, NULL, &k, NULL);
        if (!r) RegCloseKey(k);
        say("reg_open_ro %ld", r);
    } else if (!strcmp(c, "reg_set_out") || !strcmp(c, "reg_delete_out") || !strcmp(c, "reg_save")) {
        HKEY k;
        LONG r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\EE-harness-selftest-victim", 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &k);
        say("victim open %ld", r);
        if (!strcmp(c, "reg_set_out")) { DWORD v = 1; r = RegSetValueExA(k, "Injected", 0, REG_DWORD, (BYTE *)&v, 4); }
        else if (!strcmp(c, "reg_delete_out")) r = RegDeleteValueA(k, "Keep");
        else { char hive[MAX_PATH + 16]; snprintf(hive, sizeof hive, "%s\\saved.hiv", stage); r = RegSaveKeyA(k, hive, NULL); }
        say("%s NOT STOPPED %ld", c, r);
    } else if (!strcmp(c, "reg_subtree")) {
        HKEY k, k2; DWORD v = 7, got = 0, n = 4; char key[200];
        snprintf(key, sizeof key, "%s\\EE-AOC", REG_ROOT);
        LONG r = RegCreateKeyExA(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
        LONG s = r ? r : RegSetValueExA(k, "Instance", 0, REG_DWORD, (BYTE *)&v, 4);
        if (!r) RegCloseKey(k);
        LONG o = RegOpenKeyExA(HKEY_CURRENT_USER, key, 0, KEY_READ, &k2);
        LONG q = o ? o : RegQueryValueExA(k2, "Instance", NULL, NULL, (BYTE *)&got, &n);
        if (!o) RegCloseKey(k2);
        LONG a = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\EE-harness-selftest\\EE-AOC\\absent", 0, KEY_READ, &k2);
        say("reg_subtree create %ld set %ld open %ld query %ld value %lu absent %ld", r, s, o, q, got, a);
    } else if (!strcmp(c, "ddraw_key_redirect")) {
        HKEY k; DWORD v = 5;
        LONG r = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\DirectDraw\\MostRecentApplication", 0, NULL, 0,
                                 KEY_ALL_ACCESS, NULL, &k, NULL);
        LONG s = r ? r : RegSetValueExA(k, "ID", 0, REG_DWORD, (BYTE *)&v, 4);
        if (!r) RegCloseKey(k);
        say("ddraw_key_redirect create %ld set %ld", r, s);
    } else if (!strcmp(c, "window")) {
        HWND h = CreateWindowExA(0, "STATIC", "selftest", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
        HWND m = CreateWindowExA(0, "STATIC", "selftest message", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
        say("window %d message %d", h != NULL, m != NULL);
        DestroyWindow(m); DestroyWindow(h);
    } else if (!strcmp(c, "msgbox")) {
        MessageBoxA(NULL, "selftest box", "selftest", MB_OK);
        say("msgbox NOT STOPPED");
    } else if (!strcmp(c, "create_process")) {
        STARTUPINFOA si = {sizeof si}; PROCESS_INFORMATION pi; char cmd[] = "cmd.exe /c exit 0";
        BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        say("create_process NOT STOPPED %d", ok);
    } else if (!strcmp(c, "display")) {
        LONG r = ChangeDisplaySettingsA(NULL, 0);
        say("display NOT STOPPED %ld", r);
    } else if (!strcmp(c, "gamma")) {
        WORD ramp[3][256]; HDC dc = GetDC(NULL);
        GetDeviceGammaRamp(dc, ramp);
        BOOL r = SetDeviceGammaRamp(dc, ramp);
        say("gamma NOT STOPPED %d", r);
    } else if (!strcmp(c, "spi_get")) {
        RECT r; BOOL ok = SystemParametersInfoA(SPI_GETWORKAREA, 0, &r, 0);
        say("spi_get %d", ok);
    } else if (!strcmp(c, "spi_set")) {
        BOOL ok = SystemParametersInfoA(SPI_SETBEEP, TRUE, NULL, 0);   /* stopped before it runs; never persisted */
        say("spi_set NOT STOPPED %d", ok);
    } else if (!strcmp(c, "net_stop")) {
        wsa();
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(9); a.sin_addr.s_addr = htonl(0x7f000001);
        int r = connect(s, (struct sockaddr *)&a, sizeof a);
        say("net_stop NOT STOPPED %d", r);
    } else if (!strcmp(c, "net_loopback")) {
        wsa();
        SOCKET l = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), t = socket(AF_INET, SOCK_STREAM, 0), u = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in a = {0}; a.sin_family = AF_INET;
        int b = bind(l, (struct sockaddr *)&a, sizeof a);
        int li = listen(l, 1);
        int port = port_of(l);
        struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons((u_short)port);
        to.sin_addr.s_addr = inet_addr("8.8.8.8");   /* rewritten to 127.0.0.1: reaches the listener */
        int cn = connect(t, (struct sockaddr *)&to, sizeof to);
        u_long on = 1; DWORD got = 0;
        int io = WSAIoctl(t, FIONBIO, &on, sizeof on, NULL, 0, &got, NULL, NULL);
        struct sockaddr_in ud = {0}; ud.sin_family = AF_INET; ud.sin_port = htons(9); ud.sin_addr.s_addr = inet_addr("8.8.4.4");
        int st = sendto(u, "x", 1, 0, (struct sockaddr *)&ud, sizeof ud);
        say("net_loopback bind %d %s listen %d connect %d ioctl %d sendto %d", b, ip_of(l), li, cn, io, st);
        closesocket(t); closesocket(l); closesocket(u);   /* (accept's own AFD endpoint is not allowed: --mp-loopback only) */
    } else if (!strcmp(c, "net_raw")) {
        wsa();
        SOCKET s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        say("net_raw NOT STOPPED %d", s != INVALID_SOCKET);
    } else if (!strcmp(c, "net_ioctl_bad")) {
        wsa();
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        GUID g = WSAID_CONNECTEX; void *fn = NULL; DWORD got = 0;
        int r = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &g, sizeof g, &fn, sizeof fn, &got, NULL, NULL);
        say("net_ioctl_bad NOT STOPPED %d", r);
    } else if (!strcmp(c, "net_port_offset")) {
        wsa();
        SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(33334);
        int b = bind(s, (struct sockaddr *)&a, sizeof a);
        say("net_port_offset bind %d port %d", b, port_of(s));
        closesocket(s);
    } else if (!strcmp(c, "mp_loopback")) {
        wsa();
        SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in a = {0}; a.sin_family = AF_INET;
        int b = bind(s, (struct sockaddr *)&a, sizeof a);
        struct sockaddr_in to = {0}; to.sin_family = AF_INET; to.sin_port = htons(9); to.sin_addr.s_addr = inet_addr("1.2.3.4");
        int st = sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof to);
        WCHAR w[] = L"\\Device\\RasAcd"; USTR u = {sizeof w - 2, sizeof w, w}; OA oa = {sizeof oa, NULL, &u, 0x40, NULL, NULL};
        IOSB io; HANDLE h = NULL;
        LONG r = ((NtCreateFile_t)nt("NtCreateFile"))(&h, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &oa, &io, NULL, 0,
                                                       FILE_SHARE_READ | FILE_SHARE_WRITE, 1 /* FILE_OPEN */, 0x20, NULL, 0);
        say("mp_loopback bind %d %s sendto %d rasacd %08lx", b, ip_of(s), st, (unsigned long)r);
        closesocket(s);
    } else if (!strcmp(c, "seed_pin")) {
        Loading l; memset(&l, 0, sizeof l); l.seed = 1;
        pExecute(&l);
        pExecute(&l);
    } else if (!strcmp(c, "nhd")) {
        int i;
        for (i = 0; i < 4; i++) selftest_world.players[i] = &selftest_players[i];
        selftest_world.current = 1;
        pOut(&selftest_world, 0, 1);   /* the local player out, not victorious: skipped */
        pOut(&selftest_world, 0, 2);   /* another player: runs */
        pOut(&selftest_world, 1, 1);   /* victorious: runs */
        pResult(&selftest_players[1], 2);   /* the local player defeated: skipped */
        pResult(&selftest_players[1], 1);   /* won: runs */
        pResult(&selftest_players[2], 2);   /* another player: runs */
        say("nhd out calls %ld result calls %ld local state %ld", selftest_out_calls, selftest_result_calls,
            selftest_players[1].state);
    } else if (!strcmp(c, "mutex_bypass")) {
        unsigned long a = pMutex(183), b = pMutex(5);
        say("mutex 183 -> %lu, 5 -> %lu", a, b);
    } else if (!strcmp(c, "mp_skip")) {
        pSkip();
        say("mp_skip calls %ld", selftest_skip_calls);
    } else if (!strcmp(c, "mp_checksum")) {
        say("checksum %lu", pChecksum(1000));
    } else if (!strcmp(c, "rng_trace")) {
        int i;
        Sleep(1500);   /* the harness arms the trace when it sees rng.arm (its idle loop) */
        selftest_physics_object.random.seed = 77;
        for (i = 0; i < 2; i++) pGenerate(&selftest_physics_object.random);
        pNext(&selftest_physics_object.random);
        say("rng seed %lu", selftest_physics_object.random.seed);
    } else if (!strcmp(c, "static_exe") || !strcmp(c, "abort")) {
        HWND h = game_window("Empire Earth Expansion");
        say("sounds %u", pSounds());
        say("activate %ld", pActivate(NULL, 0, NULL, NULL, 0, 0, 0));
        pNative(0x1234); pNative(0x1234); pNative(0x99);
        say("show %ld", pShow(NULL, 0));
        if (!strcmp(c, "abort")) pAbort();
        say("traced %d covered %d", pTraced(1), pCovered(2));
        char request[MAX_PATH + 16]; snprintf(request, sizeof request, "%s\\stop.request", stage);   /* absolute: a handle-relative open is not "below the stage" */
        HANDLE f = CreateFileA(request, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(f);
        Sleep(3000);   /* the harness stops the session at its next poll */
        DestroyWindow(h);
    } else if (!strcmp(c, "ddraw")) {
        HWND h = game_window("Empire Earth Expansion");
        LPDIRECTDRAW7 dd = NULL;
        /* loaded here only: ddraw.dll in a non-windowed run makes the harness resolve its vtables (scratch key) */
        HMODULE m = LoadLibraryA("ddraw.dll");
        typedef HRESULT (WINAPI *create_t)(GUID *, void **, REFIID, IUnknown *);
        create_t create = m ? (create_t)GetProcAddress(m, "DirectDrawCreateEx") : NULL;
        HRESULT r = create ? create(NULL, (void **)&dd, &IID_IDirectDraw7, NULL) : E_FAIL;
        say("ddraw create %08lx", (unsigned long)r);
        if (dd) {
            r = IDirectDraw7_SetCooperativeLevel(dd, h, DDSCL_NORMAL);
            say("ddraw normal %08lx", (unsigned long)r);
            r = IDirectDraw7_SetCooperativeLevel(dd, h, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
            say("ddraw exclusive NOT STOPPED %08lx", (unsigned long)r);
        }
    } else if (!strcmp(c, "crash")) {
        volatile int *p = (volatile int *)(ULONG_PTR)0x10;
        *p = 1;
    } else {
        say("unknown case %s", c);
        return 2;
    }
    say("end %s", c);
    return 0;
}

int main(int argc, char **argv)
{
    const char *c = getenv("EE_SELFTEST_CASE");
    (void)argc; (void)argv;
    paths();
    if (!strcmp(c ? c : "", "--symbols")) return 0;
    return selftest_main_stand_in() + run(c ? c : "");
}
