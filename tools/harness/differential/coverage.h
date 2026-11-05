/* TEST ONLY (round 39, agent cov): one-shot function-entry coverage for exe_boot.c.
 *
 * Opt-in by `--coverage LIST OUT` or EE_COVERAGE="LIST_ORIGINAL LIST_FULLLINK OUTDIR" (the stage directory name,
 * windowed_f* = full link, picks the list; the output is OUTDIR\<stage name>.<target pid>.cov.tsv), so one
 * environment serves a game_batch run of either image. Nothing changes without it.
 * LIST lines: `RVA [LABEL]` (hex, relative to the exe base; LABEL defaults to the rva, e.g. the original VA a
 * full-link rva was mapped from). At process creation an int3 is written at every listed entry that carries no other
 * harness guard. The first hit records (label, hit order, PT key of the C hasher or -1, PT-sync hit count, ms since
 * arming, thread id), restores the original byte and re-executes the instruction: each breakpoint fires at most once
 * and the thread runs on unchanged (a concurrent hit of the same entry by a second thread is only rewound).
 * The hit set is rewritten every 10 s (a killed game still leaves data) and at the end of the run.
 * A later harness guard on a listed entry (target_guard) first takes the coverage int3 back (entry marked "guarded").
 * Read-only towards the game apart from these 0xCC bytes, each restored at its first execution.
 */
#ifndef EE_COVERAGE_H
#define EE_COVERAGE_H

typedef struct { DWORD rva; BYTE orig; signed char state; /* 0 armed, 1 hit, 2 not armed, 3 guarded */
                 char label[24]; unsigned long order, ptkey_valid, ptkey, pthits, ms, tid; } cov_entry;
static cov_entry *cov;
static int ncov, cov_on, cov_armed, cov_dirty;
static unsigned long cov_nhit, cov_t0, cov_last_write;
static char cov_list[MAX_PATH], cov_out[MAX_PATH], cov_outdir[MAX_PATH], cov_stage[MAX_PATH] = "run";

static int cov_cmp(const void *a, const void *b)
{
    DWORD x = ((const cov_entry *)a)->rva, y = ((const cov_entry *)b)->rva;
    return x < y ? -1 : x > y;
}

static void coverage_load(const char *path)
{
    FILE *f = fopen(path, "r"); char line[256]; int cap = 0, i, j;
    if (!f) { fprintf(stderr, "cannot read --coverage list %s\n", path); exit(2); }
    while (fgets(line, sizeof line, f)) {
        unsigned long r; char lab[64] = "";
        if (line[0] == '#' || sscanf(line, "%lx %63s", &r, lab) < 1) continue;
        if (ncov == cap) { cap = cap ? 2 * cap : 4096; cov = realloc(cov, cap * sizeof *cov); }
        ZeroMemory(&cov[ncov], sizeof *cov);
        cov[ncov].rva = r;
        if (lab[0]) snprintf(cov[ncov].label, sizeof cov[ncov].label, "%s", lab);
        else snprintf(cov[ncov].label, sizeof cov[ncov].label, "%lx", r);
        ncov++;
    }
    fclose(f);
    qsort(cov, ncov, sizeof *cov, cov_cmp);
    for (i = j = 0; i < ncov; i++) if (!j || cov[j - 1].rva != cov[i].rva) cov[j++] = cov[i];   /* duplicates */
    ncov = j;
    snprintf(cov_list, sizeof cov_list, "%s", path);
    cov_on = 1;
}

static int coverage_option(int argc, char **argv, int *option)
{
    if (strcmp(argv[*option], "--coverage") || *option + 2 >= argc) return 0;
    coverage_load(argv[++*option]);
    snprintf(cov_out, sizeof cov_out, "%s", argv[++*option]);
    return 1;
}

static void coverage_env(const char *stage)
{
    const char *e = getenv("EE_COVERAGE"); char co[MAX_PATH], cf[MAX_PATH];
    const char *b = strrchr(stage, '\\') ? strrchr(stage, '\\') + 1 : stage;
    if (cov_on || !e || sscanf(e, "%259s %259s %259s", co, cf, cov_outdir) != 3) return;
    snprintf(cov_stage, sizeof cov_stage, "%s", *b ? b : "run");
    coverage_load(strstr(stage, "windowed_f") ? cf : co);
}

static cov_entry *cov_find(DWORD rva)
{
    int lo = 0, hi = ncov - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (cov[m].rva == rva) return &cov[m];
        if (cov[m].rva < rva) lo = m + 1; else hi = m - 1;
    }
    return NULL;
}

static void coverage_write(const char *why)
{
    FILE *f; char tmp[MAX_PATH + 8]; int i; unsigned long armed = 0;
    if (!cov_on || !cov_out[0]) return;
    snprintf(tmp, sizeof tmp, "%s.tmp", cov_out);
    if (!(f = fopen(tmp, "w"))) return;
    for (i = 0; i < ncov; i++) armed += cov[i].state == 0 || cov[i].state == 1;
    fprintf(f, "# coverage list %s entries %d armed %lu hit %lu (%s) ms %lu\n", cov_list, ncov, armed, cov_nhit, why,
            (unsigned long)(GetTickCount() - cov_t0));
    fprintf(f, "#label\trva\tstate\torder\tptkey\tpthits\tms\ttid\n");
    for (i = 0; i < ncov; i++) {
        cov_entry *e = &cov[i];
        static const char *st[] = {"armed", "hit", "unarmed", "guarded"};
        if (e->state == 1)
            fprintf(f, "%s\t%lx\thit\t%lu\t%ld\t%lu\t%lu\t%lu\n", e->label, (unsigned long)e->rva, e->order,
                    e->ptkey_valid ? (long)e->ptkey : -1L, e->pthits, e->ms, e->tid);
        else fprintf(f, "%s\t%lx\t%s\n", e->label, (unsigned long)e->rva, st[(int)e->state]);
    }
    fclose(f);
    MoveFileExA(tmp, cov_out, MOVEFILE_REPLACE_EXISTING);
    cov_dirty = 0; cov_last_write = GetTickCount();
}

/* at CREATE_PROCESS, after the harness's own exe guards: an int3 on every listed entry without another guard */
static void coverage_arm(TADDR base, DWORD size, DWORD pid)
{
    int i, g; unsigned long n = 0, skipped = 0;
    if (!cov_on) return;
    if (!cov_out[0]) snprintf(cov_out, sizeof cov_out, "%s\\%s.%lu.cov.tsv", cov_outdir, cov_stage, (unsigned long)pid);
    for (i = 0; i < ncov; i++) {
        TADDR a = base + cov[i].rva;
        for (g = 0; g < nguards && guards[g].addr != a; g++);
        if (cov[i].rva >= size || g < nguards || !rd(a, &cov[i].orig, 1) || cov[i].orig == 0xCC) {
            cov[i].state = g < nguards ? 3 : 2; skipped++; continue;
        }
        wr8(a, 0xCC); n++;
    }
    cov_armed = 1; cov_t0 = cov_last_write = GetTickCount();
    printf("TEST coverage armed %lu of %d entries (%lu skipped: other guard or unreadable) list %s out %s\n",
           n, ncov, skipped, cov_list, cov_out);
    coverage_write("armed");
}

/* target_guard on an address: hand a still-armed coverage entry back first (its byte restored) */
static void coverage_release(TADDR address)
{
    cov_entry *e;
    if (!cov_armed || address < exeBase || address - exeBase > 0xffffffffu || !(e = cov_find((DWORD)(address - exeBase))) || e->state != 0) return;
    wr8(address, e->orig); e->state = 3; cov_dirty = 1;
}

/* a breakpoint at a listed entry: record the first hit, restore, rewind eip; 1 if handled */
static int coverage_hit(TADDR a, HANDLE th, DWORD tid)
{
    cov_entry *e; CONTEXT c;
    if (!cov_armed || !th || a < exeBase || a - exeBase > 0xffffffffu || !(e = cov_find((DWORD)(a - exeBase))) || (e->state != 0 && e->state != 1)) return 0;
    if (e->state == 0) {
        wr8(a, e->orig);
        e->state = 1; e->order = ++cov_nhit;
        e->ptkey_valid = pth.keyed; e->ptkey = pth.last_key; e->pthits = ptsync_hits;
        e->ms = GetTickCount() - cov_t0; e->tid = tid; cov_dirty = 1;
    }
    ZeroMemory(&c, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(th, &c)) return 0;
    R_IP(&c) = a;
    SetThreadContext(th, &c);
    return 1;
}

static void coverage_tick(void)
{
    if (cov_armed && cov_dirty && GetTickCount() - cov_last_write > 10000) coverage_write("periodic");
}

#endif
