/* TEST ONLY (round 39, agent dbg4): PT-windowed function-entry logging and a hardware write watch for exe_boot.c.
 *
 * Opt-in by `--pt-watch FROM TO CFG` (needs --pt-sync with --pt-hash: the window uses the C hasher's PT key).
 * CFG lines (rva hex, relative to the exe base):
 *   f RVA NAME        log each entry of RVA while FROM <= key <= TO: key, tid, ecx, first three stack args, the
 *                     return address and, for every one of ecx/args that looks like a unit, its id (+0x24)
 *   d RVA OFF NAME    the same, and set a 4-byte hardware write watch (DR0) on ecx+OFF in every thread
 *   u ID              also print 12 bytes at +0x118 for any logged unit with this id
 * DR0 hits print the writing thread's eip, the new 12 bytes at the watched address and the ebp chain.
 * Guards are armed at the first PT-sync hit with key >= FROM and stay armed (stop the run with --diff-stop-after).
 * Read-only towards the game apart from the harness's own 0xCC guard bytes and the debug registers.
 */
#ifndef EE_PTWATCH_H
#define EE_PTWATCH_H

static struct {
    int on, armed, n;
    unsigned long from, to, key;
    DWORD rva[32], off[32]; int kind[32]; char name[32][64];
    DWORD uid; int has_uid;
    DWORD dr_addr;
    DWORD ret[64]; int ret_of[64]; int nret;
} pw;

static void ptwatch_load(const char *path)
{
    FILE *f; char line[256];
    f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot read --pt-watch config %s\n", path); exit(2); }
    while (fgets(line, sizeof line, f) && pw.n < 32) {
        char k = 0, nm[64] = ""; unsigned long r = 0, o = 0;
        if (sscanf(line, " %c", &k) != 1) continue;
        if (k == 'f' && sscanf(line, " f %lx %63s", &r, nm) == 2) { pw.kind[pw.n] = 0; }
        else if (k == 'd' && sscanf(line, " d %lx %lx %63s", &r, &o, nm) == 3) { pw.kind[pw.n] = 1; }
        else if (k == 'r' && sscanf(line, " r %lx %63s", &r, nm) == 2) { pw.kind[pw.n] = 2; }
        else if (k == 'v' && sscanf(line, " v %lx %lx %63s", &r, &o, nm) == 3) { pw.kind[pw.n] = 3; }   /* dump the Dinkum vector at arg1 (element size OFF) */
        else if (k == 'u' && sscanf(line, " u %lu", &r) == 1) { pw.uid = r; pw.has_uid = 1; continue; }
        else continue;
        pw.rva[pw.n] = r; pw.off[pw.n] = o; strcpy(pw.name[pw.n], nm); pw.n++;
    }
    fclose(f);
    pw.on = 1;
    printf("TEST ptwatch %lu..%lu config %s (%d entries)\n", pw.from, pw.to, path, pw.n);
}

static int ptwatch_option(int argc, char **argv, int *option)
{
    if (strcmp(argv[*option], "--pt-watch") || *option + 3 >= argc) return 0;
    pw.from = strtoul(argv[++*option], NULL, 0);
    pw.to = strtoul(argv[++*option], NULL, 0);
    ptwatch_load(argv[++*option]);
    return 1;
}

/* EE_PTWATCH="FROM TO CFG_ORIGINAL CFG_FULLLINK": the stage directory name (windowed_f_* = full link) picks the config,
   so one environment serves a game_batch original/full-link pair */
static void ptwatch_env(const char *stage)
{
    const char *e = getenv("EE_PTWATCH"); char co[MAX_PATH], cf[MAX_PATH];
    if (pw.on || !e || sscanf(e, "%lu %lu %259s %259s", &pw.from, &pw.to, co, cf) != 4) return;
    ptwatch_load(strstr(stage, "windowed_f") ? cf : co);
}

static void ptwatch_tick(unsigned long key)
{
    int i;
    if (!pw.on) return;
    pw.key = key;
    if (key >= pw.from && key <= pw.to) printf("ptwatch key %lu\n", key);
    if (pw.armed || key < pw.from) return;
    pw.armed = 1;
    for (i = 0; i < pw.n; i++)
        printf("ptwatch arm %s rva %08lx: %s\n", pw.name[i], (unsigned long)pw.rva[i],
               target_guard(exeBase + pw.rva[i], pw.name[i], G_PT_WATCH) ? "ok" : "FAILED");
}

static void ptwatch_unit(const char *tag, DWORD p)
{
    DWORD vt, id; float v[3];
    if (p < 0x10000) return;
    vt = rd32(p);
    if (vt < exeBase || vt >= exeBase + image_size(exeBase)) return;
    id = rd32(p + 0x24);
    printf(" %s=%08lx(vt %06lx id %lu)", tag, (unsigned long)p, (unsigned long)(vt - exeBase), (unsigned long)id);
    if (pw.has_uid && id == pw.uid && rd(p + 0x118, v, sizeof v))
        printf("[u118 %.4f %.4f %.4f f214 %.1f]", v[0], v[1], v[2], (double)*(float *)&(DWORD){rd32(p + 0x214)});
}

static void ptwatch_set_dr(DWORD addr)
{
    int k;
    CONTEXT x;
    pw.dr_addr = addr;
    for (k = 0; k < nth; k++) {
        ZeroMemory(&x, sizeof x); x.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(threads[k], &x)) continue;
        x.Dr0 = addr; x.Dr6 = 0;
        x.Dr7 = (x.Dr7 & ~0x000f0003u) | 0x1u | (0x1u << 16) | (0x3u << 18);   /* L0, RW0=write, LEN0=4 */
        SetThreadContext(threads[k], &x);
    }
}

/* the guard dispatch for G_PT_WATCH (c = the stopped thread's full context) */
static void ptwatch_hit(DWORD a, CONTEXT *c, DWORD tid)
{
    int i; char m[128];
    for (i = 0; i < pw.n && exeBase + pw.rva[i] != a; i++);
    if (i == pw.n) {
        int k;
        for (k = 0; k < pw.nret; k++) if (pw.ret[k] == a) {
            mod_of(a, m, sizeof m);
            printf("ptwatch ret key %lu %s tid %lu at %s eax %08lx [eax] %08lx %08lx\n", pw.key, pw.name[pw.ret_of[k]], (unsigned long)tid, m,
                   (unsigned long)c->Eax, (unsigned long)rd32(c->Eax), (unsigned long)rd32(c->Eax + 4));
        }
        return;
    }
    mod_of(rd32(c->Esp), m, sizeof m);
    printf("ptwatch hit key %lu %s tid %lu ret %s args %08lx %08lx %08lx", pw.key, pw.name[i], (unsigned long)tid, m,
           (unsigned long)rd32(c->Esp + 4), (unsigned long)rd32(c->Esp + 8), (unsigned long)rd32(c->Esp + 12));
    printf(" *a1 %08lx %08lx *a2 %08lx %08lx", (unsigned long)rd32(rd32(c->Esp + 4)), (unsigned long)rd32(rd32(c->Esp + 4) + 4),
           (unsigned long)rd32(rd32(c->Esp + 8)), (unsigned long)rd32(rd32(c->Esp + 8) + 4));
    ptwatch_unit("ecx", c->Ecx);
    ptwatch_unit("a1", rd32(c->Esp + 4)); ptwatch_unit("a2", rd32(c->Esp + 8)); ptwatch_unit("a3", rd32(c->Esp + 12));
    printf("\n");
    if (pw.kind[i] == 3 && pw.off[i] >= 4 && pw.off[i] <= 64) {
        DWORD v = rd32(c->Esp + 4), b = rd32(v + 4), e = rd32(v + 8), p; int n = 0;
        for (p = b; p < e && n < 32; p += pw.off[i], n++) {
            DWORD q;
            printf("ptwatch   [%d]", n);
            for (q = 0; q < pw.off[i]; q += 4) printf(" %08lx", (unsigned long)rd32(p + q));
            printf("\n");
        }
    }
    if (pw.kind[i] == 2) {   /* r: also log the return value at this call's return address (guard stays armed) */
        DWORD r = rd32(c->Esp); int k;
        for (k = 0; k < pw.nret && pw.ret[k] != r; k++);
        if (k == pw.nret && pw.nret < 64 && target_guard(r, pw.name[i], G_PT_WATCH)) { pw.ret[pw.nret] = r; pw.ret_of[pw.nret++] = i; }
    }
    if (pw.kind[i] == 1 && c->Ecx) {
        ptwatch_set_dr(c->Ecx + pw.off[i]);
        printf("ptwatch DR0 on %08lx\n", (unsigned long)(c->Ecx + pw.off[i]));
    }
}

/* a single-step event: 1 if it was only our DR0 write watch (handled), 0 to let exe_boot handle it */
static int ptwatch_single_step(HANDLE th, DWORD tid)
{
    CONTEXT x; int k; DWORD fp; char m[128]; float v[3] = {0};
    if (!pw.on || !pw.dr_addr || !th) return 0;
    ZeroMemory(&x, sizeof x); x.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(th, &x) || !(x.Dr6 & 1)) return 0;
    rd(pw.dr_addr, v, sizeof v);
    mod_of(x.Eip, m, sizeof m);
    printf("ptwatch WRITE key %lu tid %lu at %s value %.4f %.4f %.4f (%08lx %08lx %08lx)\n", pw.key, (unsigned long)tid,
           m, v[0], v[1], v[2], (unsigned long)*(DWORD *)&v[0], (unsigned long)*(DWORD *)&v[1], (unsigned long)*(DWORD *)&v[2]);
    for (k = 0; k < 8; k++) { mod_of(rd32(x.Esp + 4 * k), m, sizeof m); printf("   [esp+%02x] %s\n", k * 4, m); }
    fp = x.Ebp;
    for (k = 0; k < 16 && fp > x.Esp && fp < x.Esp + 0x100000; k++) {
        mod_of(rd32(fp + 4), m, sizeof m); printf("   ebp-chain %d %s\n", k, m); fp = rd32(fp);
    }
    for (k = 0; k < 64; k++) {
        DWORD r = rd32(x.Esp + 4 * k);
        if (r >= exeBase && r < exeBase + image_size(exeBase)) { mod_of(r, m, sizeof m); printf("   scan [esp+%03x] %s\n", k * 4, m); }
    }
    x.Dr6 &= ~0xfu;
    x.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    SetThreadContext(th, &x);
    return !(x.Dr6 & 0x4000);   /* a trap-flag step at the same time: exe_boot's step-over handles it too */
}

#endif
