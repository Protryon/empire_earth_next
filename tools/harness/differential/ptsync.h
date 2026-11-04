/* TEST ONLY (round 39, agent hash): synchronous per-PT sampling for the harness debugger exe_boot.c.
 *
 * Opt-in by `--pt-sync RVA NAME [--pt-hash CFG]`: exe_boot guards exeBase+RVA (the instruction right after
 * EEPhysics::Virtual20's `mUB8--`, i.e. the start of each physics tick, reached on every PT also after the engine end;
 * tools/boot/differential/layout.py finds RVA in either image) with the normal 0xCC guard. Each hit happens while every game thread is stopped by the debug event.
 *
 * Without --pt-hash: on every hit, if the Python sampler has attached (manual-reset event Local\ee_ptsync_<NAME>_on),
 *   SetEvent(..._req) and wait for ..._ack while sampler.py reads memory (all hashing in Python).
 * With --pt-hash CFG (written by session_hook.py from layout.py): the per-PT stream line is computed HERE (C, page
 *   cache over ReadProcessMemory; ~10x cheaper than Python) and appended to the stream file named in CFG; the Python
 *   sampler is only woken for the PTs that need a full snapshot (every N, the --full-at list, the stop PT).
 *
 * Read-only towards the game: nothing is written to the debuggee except the guard byte the harness already manages.
 * The image is not patched. Without the option nothing here runs. A wait longer than EE_PTSYNC_TIMEOUT_MS (default
 * 120000) for the sampler logs and detaches it.
 *
 * Wiring in exe_boot.c (done by par): #include after target_guard(); gkind G_PT_SYNC; option loop
 * `else if (ptsync_option(argc, argv, &option)) ;`; arming next to the seed pin `ptsync_arm(exeBase, image_size(exeBase))`;
 * guard dispatch `else if (guards[g].kind == G_PT_SYNC) { ptsync_hit(); allow = 1; }`.
 */
#ifndef EE_PTSYNC_H
#define EE_PTSYNC_H

static DWORD ptsync_rva;
static HANDLE ptsync_req, ptsync_ack, ptsync_on;
static unsigned long ptsync_hits, ptsync_sampled;

/* ---- C hasher configuration (--pt-hash) ---- */
#define PTH_UNIT 0x364
#define PTH_EUNIT 0x48
#define PTH_PLAYER 0xb80
static struct {
    int on;
    DWORD physics, world, inputs, vtc, vtu;
    unsigned char umask[PTH_UNIT], urmask[PTH_UNIT], emask[PTH_EUNIT], ermask[PTH_EUNIT], pmask[PTH_PLAYER];
    DWORD refs[32]; int nrefs;
    /* `uzero_if OFF VAL OFF2 LEN` (v6): a complex unit whose u32 at OFF equals VAL has LEN bytes at OFF2 zeroed before
       hashing (mU118 is stale/uninitialised while mF214 == -10000, its unset marker; dbg4 2026-11-01) */
    DWORD zoff[8], zval[8], zoff2[8], zlen[8]; int nzero;
    /* `uneg1 OFF` (v9, round 40 mp, opt-in): a complex unit's u32 at OFF is hashed as (value == -1 ? -1 : 0) */
    DWORD noff[4]; int nneg1;
    DWORD phys_off[16]; int nphys;
    unsigned long snapshot_every, stop_after, hash_every; int has_stop;
    unsigned long full_at[64]; int nfull;
    FILE *out;
    char header[8192];
    unsigned long lines, snaps_missed;
    unsigned long last_ub4, last_key; int keyed;
    char keyfile[MAX_PATH];
} pth;

static int pth_hex(const char *s, unsigned char *dst, int n)
{
    int i;
    for (i = 0; i < n; i++) { unsigned v; if (sscanf(s + 2 * i, "%2x", &v) != 1) return 0; dst[i] = (unsigned char)v; }
    return 1;
}

typedef unsigned long long u64;
static u64 pth_mix(u64 h, const void *data, unsigned n);
static DWORD pth_image, pth_image_size;   /* exe image range (ptsync_arm): AI vptrs are validated against it */

/* ---- AI column (agent hash2): computer-player object schema from tools/boot/differential/ai_layout.py ----
 * config: `ai_vt VT` (EEComputerPlayer vtable), `ai_every N`, `ai_root T`, `ai_type T SIZE POLY MASKHEX`,
 * `ai_op T KIND OFF ARG ARG2` with KIND U unit ptr (id), P player ptr (world index), N non-null, K keyed ref (u32 at
 * ptr+ARG, or with ARG2=1 the unit id of the pointer there), O owned ptr (walk type ARG), V vector / D deque / M tree
 * of element type ARG (Dinkum 2.33 layouts). Read-only; references hashed as ids, never raw pointers. */
#define AI_TYPES 160
#define AI_MAXSIZE 0x1000
typedef struct { char kind; unsigned short off; unsigned short arg, arg2, kref; } ai_op_t;   /* kref: K pointee type (0xffff none; ai_kref) */
static struct {
    int on, root, ntypes; DWORD vt; unsigned long every, objects, fails, maxobjects;
    /* vptr: 0 none checked, 1 any class (polymorphic, '*'), 2 the RTTI name hashing to vname (ai_vptr lines, v7) */
    struct { unsigned size, poly, nops, vptr; u64 vname; unsigned char *mask; ai_op_t ops[64]; } t[AI_TYPES];
} pai;

static int pai_config(const char *key, char *v)
{
    if (!strcmp(key, "ai_vt")) { pai.vt = strtoul(v, NULL, 16); pai.on = 1; }
    else if (!strcmp(key, "ai_every")) pai.every = strtoul(v, NULL, 0);
    else if (!strcmp(key, "ai_root")) pai.root = strtoul(v, NULL, 0);
    else if (!strcmp(key, "ai_type")) {
        char *e; unsigned id = strtoul(v, &e, 0), size = strtoul(e, &e, 16), poly = strtoul(e, &e, 0);
        while (*e == ' ') e++;
        if (id >= AI_TYPES || size > AI_MAXSIZE || strlen(e) != 2 * size) return -1;
        pai.t[id].size = size; pai.t[id].poly = poly; pai.t[id].mask = malloc(size ? size : 1);
        if (!pth_hex(e, pai.t[id].mask, size)) return -1;
        if ((int)id >= pai.ntypes) pai.ntypes = id + 1;
    } else if (!strcmp(key, "ai_op")) {
        char *e, kind; unsigned id = strtoul(v, &e, 0), n;
        while (*e == ' ') e++;
        kind = *e++;
        if (id >= AI_TYPES || (n = pai.t[id].nops) >= 64) return -1;
        pai.t[id].ops[n].kind = kind; pai.t[id].ops[n].off = strtoul(e, &e, 16);
        pai.t[id].ops[n].arg = strtoul(e, &e, 16); pai.t[id].ops[n].arg2 = strtoul(e, &e, 16);
        pai.t[id].ops[n].kref = 0xffff;
        pai.t[id].nops++;
    } else if (!strcmp(key, "ai_vptr")) {   /* ai_vptr T NAME|*: identity check before walking (hash3, v7) */
        char *e; unsigned id = strtoul(v, &e, 0);
        while (*e == ' ') e++;
        if (id >= AI_TYPES || !*e) return -1;
        if (!strcmp(e, "*")) pai.t[id].vptr = 1;
        else { pai.t[id].vptr = 2; pai.t[id].vname = pth_mix(0x243f6a8885a308d3ull, e, (unsigned)strlen(e)); }
    } else if (!strcmp(key, "ai_kref")) {   /* ai_kref T OFF PT: the K op at OFF checks its pointee as type PT */
        char *e; unsigned id = strtoul(v, &e, 0), off = strtoul(e, &e, 16), pt = strtoul(e, &e, 0), k;
        if (id >= AI_TYPES || pt >= AI_TYPES) return -1;
        for (k = 0; k < pai.t[id].nops; k++)
            if (pai.t[id].ops[k].kind == 'K' && pai.t[id].ops[k].off == off) { pai.t[id].ops[k].kref = pt; break; }
        if (k == pai.t[id].nops) return -1;
    } else return 0;
    return 1;
}

static void pth_config(const char *path)
{
    static char line[16384];
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot read pt-hash config %s\n", path); exit(2); }
    while (fgets(line, sizeof line, f)) {
        char key[32], *v; size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (sscanf(line, "%31s", key) != 1) continue;
        v = line + strlen(key); while (*v == ' ') v++;
        if (!strncmp(key, "ai_", 3)) { if (pai_config(key, v) != 1) goto bad; }
        else if (!strcmp(key, "physics")) pth.physics = strtoul(v, NULL, 16);
        else if (!strcmp(key, "world")) pth.world = strtoul(v, NULL, 16);
        else if (!strcmp(key, "inputs")) pth.inputs = strtoul(v, NULL, 16);
        else if (!strcmp(key, "vt_complex")) pth.vtc = strtoul(v, NULL, 16);
        else if (!strcmp(key, "vt_unit")) pth.vtu = strtoul(v, NULL, 16);
        else if (!strcmp(key, "uneg1")) {
            if (pth.nneg1 >= 4) goto bad;
            pth.noff[pth.nneg1] = strtoul(v, NULL, 16);
            if (pth.noff[pth.nneg1] + 4 > PTH_UNIT) goto bad;
            pth.nneg1++;
        }
        else if (!strcmp(key, "uzero_if")) {
            char *e = v; int z = pth.nzero;
            if (z >= 8) goto bad;
            pth.zoff[z] = strtoul(e, &e, 16); pth.zval[z] = strtoul(e, &e, 16);
            pth.zoff2[z] = strtoul(e, &e, 16); pth.zlen[z] = strtoul(e, &e, 16);
            if (pth.zoff[z] + 4 > PTH_UNIT || pth.zoff2[z] + pth.zlen[z] > PTH_UNIT) goto bad;
            pth.nzero++;
        }
        else if (!strcmp(key, "umask")) { if (!pth_hex(v, pth.umask, PTH_UNIT)) goto bad; }
        else if (!strcmp(key, "urmask")) { if (!pth_hex(v, pth.urmask, PTH_UNIT)) goto bad; }
        else if (!strcmp(key, "emask")) { if (!pth_hex(v, pth.emask, PTH_EUNIT)) goto bad; }
        else if (!strcmp(key, "ermask")) { if (!pth_hex(v, pth.ermask, PTH_EUNIT)) goto bad; }
        else if (!strcmp(key, "pmask")) { if (!pth_hex(v, pth.pmask, PTH_PLAYER)) goto bad; }
        else if (!strcmp(key, "refs")) { char *e = v; while (*e && pth.nrefs < 32) { pth.refs[pth.nrefs++] = strtoul(e, &e, 16); while (*e == ' ') e++; } }
        else if (!strcmp(key, "physics_fields")) { char *e = v; while (*e && pth.nphys < 16) { pth.phys_off[pth.nphys++] = strtoul(e, &e, 16); while (*e == ' ') e++; } }
        else if (!strcmp(key, "snapshot_every")) pth.snapshot_every = strtoul(v, NULL, 0);
        else if (!strcmp(key, "hash_every")) pth.hash_every = strtoul(v, NULL, 0);
        else if (!strcmp(key, "stop_after")) { pth.stop_after = strtoul(v, NULL, 0); pth.has_stop = 1; }
        else if (!strcmp(key, "full_at")) { char *e = v; while (*e && pth.nfull < 64) { pth.full_at[pth.nfull++] = strtoul(e, &e, 0); while (*e == ',' || *e == ' ') e++; } }
        else if (!strcmp(key, "header")) snprintf(pth.header, sizeof pth.header, "%s", v);
        else if (!strcmp(key, "keyfile")) snprintf(pth.keyfile, sizeof pth.keyfile, "%s", v);
        else if (!strcmp(key, "stream")) {
            pth.out = fopen(v, "w");
            if (!pth.out) { fprintf(stderr, "cannot write pt-hash stream %s\n", v); exit(2); }
            setvbuf(pth.out, NULL, _IOFBF, 1 << 16);
        }
    }
    fclose(f);
    if (!pth.out || !pth.physics || !pth.world) goto bad;
    fprintf(pth.out, "#%s\n", pth.header);
    pth.on = 1;
    return;
bad:
    fprintf(stderr, "bad pt-hash config %s\n", path); exit(2);
}

static int ptsync_option(int argc, char **argv, int *option)
{
    char name[200];
    if (!strcmp(argv[*option], "--pt-hash") && *option + 1 < argc) { pth_config(argv[++*option]); return 1; }
    if (strcmp(argv[*option], "--pt-sync") || *option + 2 >= argc) return 0;
    ptsync_rva = strtoul(argv[++*option], NULL, 16);
    ++*option;
    snprintf(name, sizeof name, "Local\\ee_ptsync_%s_req", argv[*option]); ptsync_req = CreateEventA(NULL, FALSE, FALSE, name);
    snprintf(name, sizeof name, "Local\\ee_ptsync_%s_ack", argv[*option]); ptsync_ack = CreateEventA(NULL, FALSE, FALSE, name);
    snprintf(name, sizeof name, "Local\\ee_ptsync_%s_on", argv[*option]);  ptsync_on = CreateEventA(NULL, TRUE, FALSE, name);
    if (!ptsync_req || !ptsync_ack || !ptsync_on) { fprintf(stderr, "cannot create pt sync events\n"); exit(2); }
    return 1;
}

/* returns 0 on failure; 1 when armed or not requested */
static int ptsync_arm(DWORD base, DWORD size)
{
    pth_image = base; pth_image_size = size;
    if (!ptsync_rva) return 1;
    if (ptsync_rva >= size || !target_guard(base + ptsync_rva, "pt sync (end of physics tick)", G_PT_SYNC)) return 0;
    printf("TEST pt sync armed rva %08lx%s\n", (unsigned long)ptsync_rva, pth.on ? " (C hasher)" : "");
    return 1;
}

/* ---- page cache over ReadProcessMemory (valid for one PT: the game is stopped) ---- */
#define PTH_PAGES 8192   /* 64 MB; hash3: was 2048, and a full 16-slot probe window failed the read (AI walk) */
#define PTH_CHUNK 0x2000   /* 8 KB reads (units come from memory pools); a failed chunk falls back to its 4 KB page */
static struct { DWORD tag; unsigned gen; unsigned char data[PTH_CHUNK]; } *pth_cache;
static unsigned pth_gen = 1;
static int pth_ok;
static unsigned long pth_reads, pth_evicts;

/* the cached block holding va: tag = base | 1 (chunk) or base | 2 (single page); returns data and sets base and size */
static const unsigned char *pth_block(DWORD va, DWORD *base, unsigned *size)
{
    int kind;
    for (kind = 1; kind <= 2; kind++) {
        DWORD b = kind == 1 ? va & ~(PTH_CHUNK - 1u) : va & ~0xfffu, tag = b | kind;
        unsigned n = kind == 1 ? PTH_CHUNK : 4096, slot = (b >> 12) * 2654435761u % PTH_PAGES, k;
        SIZE_T got;
        for (k = 0; k < 16; k++) {
            unsigned s = (slot + k) % PTH_PAGES;
            if (pth_cache[s].gen == pth_gen && pth_cache[s].tag == tag) { *base = b; *size = n; return pth_cache[s].data; }
            if (pth_cache[s].gen != pth_gen || k == 15) {   /* a free slot, or all 16 taken: evict the last one (a
                                                               block pointer is only used until the next pth_block) */
                if (pth_cache[s].gen == pth_gen) pth_evicts++;
                pth_cache[s].gen = 0;   /* stays invalid if the read fails */
                pth_reads++;
                if (ReadProcessMemory(hProc, (void *)b, pth_cache[s].data, n, &got) && got == n) {
                    pth_cache[s].tag = tag; pth_cache[s].gen = pth_gen; *base = b; *size = n; return pth_cache[s].data;
                }
                break;   /* chunk not readable as a whole: try the page */
            }
        }
    }
    pth_ok = 0;
    return NULL;
}

static void pth_read(DWORD va, void *dst, unsigned n)
{
    unsigned char *d = dst;
    while (n) {
        DWORD base; unsigned size, off, k;
        const unsigned char *p = pth_block(va, &base, &size);
        if (!p) { memset(d, 0, n); return; }
        off = va - base; k = size - off < n ? size - off : n;
        memcpy(d, p + off, k); d += k; va += k; n -= k;
    }
}

static DWORD pth_u32(DWORD va) { DWORD v = 0; if (va) pth_read(va, &v, 4); return v; }


static u64 pth_mix(u64 h, const void *data, unsigned n)
{
    const unsigned char *p = data; unsigned i;
    for (i = 0; i + 8 <= n; i += 8) { u64 w; memcpy(&w, p + i, 8); h ^= w; h *= 0x9E3779B97F4A7C15ull; h ^= h >> 29; }
    for (; i < n; i++) { h ^= p[i]; h *= 0x100000001B3ull; }
    h ^= n; h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 31;
    return h;
}

static void pth_masked(u64 *h, const unsigned char *b, const unsigned char *mask, unsigned n)
{
    unsigned char t[PTH_PLAYER]; unsigned i;
    for (i = 0; i < n; i++) t[i] = b[i] & mask[i];
    *h = pth_mix(*h, t, n);
}

static int pth_needs_snapshot(unsigned long pt)
{
    int i;
    if (pth.snapshot_every && pt % pth.snapshot_every == 0) return 1;
    if (pth.has_stop && pt >= pth.stop_after) return 1;
    for (i = 0; i < pth.nfull; i++) if (pth.full_at[i] == pt) return 1;
    return 0;
}

/* ---- AI walk ----
 * hash3 (stream version 7): the AI hash is a function of valid game state only.
 *  - every read is checked where it is made (pai_try): a failed read hashes a fixed sentinel for that object/element/
 *    reference and is counted (ai_read_failures); it never truncates or poisons the rest of the walk;
 *  - identity: an object of a class with a vptr (ai_vptr lines; ai_layout.VPTRLESS lists the exceptions) is walked only
 *    when its vptr lies in the exe image and its RTTI name is the expected one ('*': polymorphic, any .?AV..@@ class);
 *    a keyed reference (K) checks its pointee the same way (ai_kref) before reading the id. Anything else (a stale
 *    pointer into freed or reused memory) hashes the sentinel "I" without descending and is counted (ai_invalid);
 *  - the per-PT page cache never runs full (pth_block evicts), which was the old source of "read failures": with
 *    ~1,300 blocks per PT late in a 6-AI game, 16 occupied probe slots made pth_block fail, cleared pth_ok and cut
 *    the walk at a layout-dependent place (k7: bursts of AI-only differences, A 24,275 / B 10,697 "failures"). */
static DWORD *pai_players; static int pai_nplayers;
static long long pai_ticks, pai_objs; static unsigned long pai_lines;
static unsigned long pai_invalid, pai_diag;
#define PAI_DIAG_MAX 64   /* first identity/read problems written to the stream as '#ai-invalid' lines */

static int pai_try(DWORD va, void *dst, unsigned n)
{
    int save = pth_ok, ok;
    if (!va) { memset(dst, 0, n); return 0; }
    pth_ok = 1; pth_read(va, dst, n); ok = pth_ok; pth_ok = save;
    return ok;
}

static int pai_try_u32(DWORD va, DWORD *v) { *v = 0; return pai_try(va, v, 4); }

static void pai_note(const char *what, unsigned type, DWORD obj, DWORD vt, const char *name)
{
    if (pai_diag >= PAI_DIAG_MAX || !pth.out) return;
    pai_diag++;
    fprintf(pth.out, "#ai-invalid {\"pt\": %lu, \"what\": \"%s\", \"type\": %u, \"obj\": \"%08lx\", \"vt\": \"%08lx\", "
            "\"rtti\": \"%s\"}\n", pth.last_key, what, type, (unsigned long)obj, (unsigned long)vt, name ? name : "");
}

/* RTTI name of a vtable (cached for the run: vtables and their type descriptors are image constants). 0: not a vtable
   of this image (outside it, or no readable '.?AV...@@' name). */
static struct { DWORD vt; u64 h; char name[64]; } pai_vts[1024];
static int pai_vtinfo(DWORD vt, u64 *h, const char **name)
{
    unsigned s, k;
    if (vt < pth_image || vt - pth_image >= pth_image_size || (vt & 3)) return 0;
    s = (vt >> 2) * 2654435761u % 1024;
    for (k = 0; k < 1024; k++) {
        unsigned i = (s + k) % 1024;
        if (pai_vts[i].vt == vt) { *h = pai_vts[i].h; *name = pai_vts[i].name; return 1; }
        if (!pai_vts[i].vt) {   /* vtable[-1] = complete object locator, +0xc its type descriptor, +8 the name */
            DWORD col, td; char nm[64]; size_t len; int ok;
            memset(nm, 0, sizeof nm);
            ok = pai_try_u32(vt - 4, &col) && pai_try_u32(col + 0xc, &td) && pai_try(td + 8, nm, sizeof nm - 1);
            nm[sizeof nm - 1] = 0; len = strlen(nm);
            ok = ok && len > 6 && !strncmp(nm, ".?AV", 4) && !strcmp(nm + len - 2, "@@");
            if (!ok) return 0;   /* not cached: not a vtable (data that happens to point into the image) */
            pai_vts[i].vt = vt; memcpy(pai_vts[i].name, nm, sizeof nm);
            pai_vts[i].h = pth_mix(0x243f6a8885a308d3ull, nm, (unsigned)len);
            *h = pai_vts[i].h; *name = pai_vts[i].name;
            return 1;
        }
    }
    return 0;
}

/* identity of an object of AI type `type` whose vptr is vt: 1 valid (or the type has no vptr), 0 not; *rh = RTTI hash */
static int pai_valid(unsigned type, DWORD obj, DWORD vt, u64 *rh)
{
    const char *name = NULL; u64 h = 0;
    *rh = 0;
    if (!pai.t[type].vptr && !pai.t[type].poly) return 1;
    if (!pai_vtinfo(vt, &h, &name)) { pai_note("vptr", type, obj, vt, NULL); return 0; }
    if (pai.t[type].vptr == 2 && h != pai.t[type].vname) { pai_note("class", type, obj, vt, name); return 0; }
    *rh = h;
    return 1;
}

static DWORD pai_ref(const ai_op_t *op, DWORD ptr)
{
    int i; DWORD v;
    if (op->kind == 'N') return ptr != 0;
    if (!ptr) return 0xffffffff;
    if (op->kind == 'P') { for (i = 0; i < pai_nplayers; i++) if (pai_players[i] == ptr) return i; return 0xfffffffe; }
    if (op->kind == 'U') { if (!pai_try_u32(ptr + 0x24, &v)) { pai.fails++; return 0xfffffffd; } return v; }
    if (op->kind == 'K') {
        if (op->kref != 0xffff) {   /* the pointee's identity first */
            DWORD vt; u64 rh;
            if (!pai_try_u32(ptr, &vt)) { pai.fails++; return 0xfffffffd; }
            if (!pai_valid(op->kref, ptr, vt, &rh)) { pai_invalid++; return 0xfffffffc; }
        }
        if (!pai_try_u32(ptr + op->arg, &v)) { pai.fails++; return 0xfffffffd; }
        if (op->arg2 == 1) {
            DWORD id;
            if (!v) return 0xffffffff;
            if (!pai_try_u32(v + 0x24, &id)) { pai.fails++; return 0xfffffffd; }
            return id;
        }
        return v;
    }
    return 0;
}

static u64 pai_walk(u64 h, DWORD obj, unsigned type, int depth);

/* tree: in-order over the nodes below `node` (value at +0xc, _Isnil at +0xc + value size + 1). A node that cannot be
   read hashes the read sentinel and ends the walk of that tree (its count was hashed before). */
static u64 pai_tree(u64 h, DWORD node, unsigned el, int depth, unsigned *left, int wdepth)
{
    unsigned char nb[0xc], nil = 0;
    unsigned isnil = 0xc + pai.t[el].size + 1;
    DWORD l, r;
    if (!*left || !node) return h;
    if (depth > 64) { *left = 0; return pth_mix(h, "m!", 2); }
    if (!pai_try(node, nb, 0xc) || !pai_try(node + isnil, &nil, 1)) { pai.fails++; *left = 0; return pth_mix(h, "R", 1); }
    if (nil) return h;
    memcpy(&l, nb, 4); memcpy(&r, nb + 8, 4);
    h = pai_tree(h, l, el, depth + 1, left, wdepth);
    if (!*left) return h;
    --*left;
    h = pai_walk(h, node + 0xc, el, wdepth);
    return pai_tree(h, r, el, depth + 1, left, wdepth);
}

static u64 pai_walk(u64 h, DWORD obj, unsigned type, int depth)
{
    unsigned i, size = type < (unsigned)pai.ntypes ? pai.t[type].size : 0;
    u64 rh;
    if (depth > 16 || ++pai.objects > pai.maxobjects || !size) { h = pth_mix(h, "!", 1); return h; }
    {
    unsigned char b[size], t[size];
    DWORD vt;
    if (!pai_try(obj, b, size)) { pai.fails++; pai_note("read", type, obj, 0, NULL); return pth_mix(h, "R", 1); }
    memcpy(&vt, b, 4);
    if (!pai_valid(type, obj, vt, &rh)) { pai_invalid++; return pth_mix(h, "I", 1); }
    for (i = 0; i < size; i++) t[i] = b[i] & pai.t[type].mask[i];
    h = pth_mix(h, t, size);
    if (pai.t[type].poly) h = pth_mix(h, &rh, 8);
    for (i = 0; i < pai.t[type].nops; i++) {
        const ai_op_t *op = &pai.t[type].ops[i];
        DWORD w[5] = {0}, v; unsigned c = size - op->off;
        memcpy(w, b + op->off, c < 20 ? c : 20);
        switch (op->kind) {
        case 'U': case 'P': case 'N': case 'K':
            v = pai_ref(op, w[0]); h = pth_mix(h, &v, 4); break;
        case 'O':
            if (w[0]) h = pai_walk(h, w[0], op->arg, depth + 1); else h = pth_mix(h, "0", 1);
            break;
        case 'V': {   /* allocator, _First, _Last, _End */
            unsigned es = pai.t[op->arg].size, n = es && w[2] >= w[1] ? (w[2] - w[1]) / es : 0xffffffff, k;
            if (!w[1] && !w[2]) n = 0;
            if (n > 65536 || (n && (w[2] - w[1]) % es) || (n && !w[1]) || w[3] < w[2]) { h = pth_mix(h, "v?", 2); break; }
            h = pth_mix(h, &n, 4);
            for (k = 0; k < n; k++) h = pai_walk(h, w[1] + k * es, op->arg, depth + 1);
            break;
        }
        case 'D': {   /* allocator, _Map, _Mapsize, _Myoff, _Mysize */
            unsigned es = pai.t[op->arg].size, ds = es > 4096 ? 1 : 4096 / es, k, n;
            DWORD mp = w[1], mapsize = w[2], off = w[3];
            n = w[4];
            if (n > 65536 || mapsize > (1u << 20) || (n && (!mp || !mapsize))) { h = pth_mix(h, "d?", 2); break; }
            h = pth_mix(h, &n, 4);
            for (k = 0; k < n; k++) {
                unsigned x = off + k, blk = x / ds;
                DWORD bp;
                if (blk >= mapsize) blk -= mapsize;
                if (blk >= mapsize || !pai_try_u32(mp + 4 * blk, &bp) || !bp) { pai.fails++; h = pth_mix(h, "R", 1); continue; }
                h = pai_walk(h, bp + (x - (x / ds) * ds) * es, op->arg, depth + 1);
            }
            break;
        }
        case 'M': {   /* predicate+allocator, _Head, _Size */
            DWORD head = w[1], n = w[2], root; unsigned left = n;
            if (!head || n > 65536) { h = pth_mix(h, "m?", 2); break; }
            h = pth_mix(h, &n, 4);
            if (!pai_try_u32(head + 4, &root)) { pai.fails++; h = pth_mix(h, "R", 1); break; }
            h = pai_tree(h, root, op->arg, 0, &left, depth + 1);
            break;
        }
        }
    }
    }
    return h;
}

static unsigned long pth_line(void)
{
    unsigned char pb[0x100], wb[0xd8], b[PTH_UNIT], pl[PTH_PLAYER];
    DWORD players[17], phys, head, u, mp, maprng, inputs, rng, nplayers, link, vt, player;
    u64 ph[17], gaia = 0x6a09e667f3bcc908ull, render = 0x3c6ef372fe94f82bull, world = 0xbb67ae8584caa73bull;
    unsigned long pt, n = 0; int i, k;
    pth_gen++; pth_ok = 1;
    phys = pth_u32(pth.physics);
    pth_read(phys, pb, sizeof pb);
    memcpy(&pt, pb + 0xb4, 4); memcpy(&rng, pb + 0x18, 4);
    /* key rule (same in sampler.py): mUB4 (PTs completed) while it advances; once it freezes (engine end: mBE6 skips
       the world update and mUB4++, but PTs keep running) the previous key + 1 */
    if (pth.keyed && pt == pth.last_ub4) pt = pth.last_key + 1;
    else pth.last_ub4 = pt;
    pth.last_key = pt; pth.keyed = 1;
    /* stride (--diff-sync-every N): every PT still stops here (the key counts PTs), but only every Nth is hashed */
    if (pth.hash_every > 1 && pt % pth.hash_every && !pth_needs_snapshot(pt)) return pt;
    pth_read(pth.world, wb, sizeof wb);
    memcpy(&nplayers, wb + 0xcc, 4); if (nplayers > 17) nplayers = 17;
    memcpy(players, wb + 0x74, 17 * 4);
    memcpy(&mp, wb + 0xbc, 4);
    maprng = mp ? pth_u32(mp + 0x19562c) : 0;
    inputs = pth_u32(pth.inputs);
    for (i = 0; i < (int)nplayers; i++) {
        ph[i] = 0x510e527fade682d1ull;
        if (!players[i]) continue;
        pth_read(players[i], pl, PTH_PLAYER);
        pth_masked(&ph[i], pl, pth.pmask, PTH_PLAYER);
        for (k = 0; k < 31; k++) {   /* statistics series: count, last point, max, flag */
            DWORD s = players[i] + 0x4a8 + 0x18 + 0x18 * k, first = pth_u32(s + 4), last = pth_u32(s + 8), v[5] = {0};
            unsigned char flag = 0;
            v[0] = last >= first ? (last - first) / 8 : 0xffffffff;
            if (last > first) { v[1] = pth_u32(last - 8); v[2] = pth_u32(last - 4); }
            v[3] = pth_u32(s + 0x10); pth_read(s + 0x14, &flag, 1); v[4] = flag;
            ph[i] = pth_mix(ph[i], v, sizeof v);
        }
        { unsigned char civ[8]; pth_read(players[i] + 0x468 + 0x34, civ, 8); ph[i] = pth_mix(ph[i], civ, 8); }
    }
    head = pth.world + 0x14;
    u = pth_u32(pth.world + 0x18);
    while (u && u != head && n < 200000 && pth_ok) {
        int complex_, size, p = -1; DWORD extra[40]; int ne = 0;
        pth_read(u, b, 0x2c);
        memcpy(&vt, b, 4); memcpy(&link, b + 8, 4); memcpy(&player, b + 0x18, 4);
        complex_ = vt == pth.vtc; size = complex_ ? PTH_UNIT : PTH_EUNIT;
        pth_read(u, b, size);
        if (complex_) for (k = 0; k < pth.nzero; k++) {
            DWORD x; memcpy(&x, b + pth.zoff[k], 4);
            if (x == pth.zval[k]) memset(b + pth.zoff2[k], 0, pth.zlen[k]);
        }
        if (complex_) for (k = 0; k < pth.nneg1; k++) {
            DWORD x; memcpy(&x, b + pth.noff[k], 4); x = x == 0xffffffff ? 0xffffffff : 0; memcpy(b + pth.noff[k], &x, 4);
        }
        for (i = 0; i < (int)nplayers; i++) if (players[i] && players[i] == player) { p = i; break; }
        { DWORD cls; memcpy(&cls, b + 0x2c, 4); extra[ne++] = cls ? pth_u32(cls + 0x28) : 0xffffffff; }
        extra[ne++] = complex_ ? 0 : vt == pth.vtu ? 1 : 2;
        if (complex_) {
            DWORD g;
            for (k = 0; k < pth.nrefs; k++) { DWORD r; memcpy(&r, b + pth.refs[k], 4); extra[ne++] = r ? pth_u32(r + 0x24) : 0xffffffff; }
            memcpy(&g, b + 0x1f0, 4);
            if (g) { unsigned char kind = 0; pth_read(g + 5, &kind, 1); extra[ne++] = kind; extra[ne++] = pth_u32(g + 0x14); }
        }
        {
            u64 *h = p >= 0 ? &ph[p] : &gaia;
            pth_masked(h, b, complex_ ? pth.umask : pth.emask, size);
            *h = pth_mix(*h, extra, ne * 4);
            render = pth_mix(render, b + 0x24, 4);
            pth_masked(&render, b, complex_ ? pth.urmask : pth.ermask, size);
        }
        n++;
        if (!link) break;
        u = pth_u32(link);
    }
    for (k = 0; k < pth.nphys; k++) world = pth_mix(world, pb + pth.phys_off[k], 4);
    world = pth_mix(world, &n, 4);   /* inputs (gU531CF0) only in its own column: civ-timer messages are wall-timed */
    world = pth_mix(world, &gaia, 8);
    for (i = 0; i < (int)nplayers; i++) world = pth_mix(world, &ph[i], 8);
    if (!pth_ok) { fprintf(pth.out, "#read-failure pt %lu\n", pt); return pt; }
    fprintf(pth.out, "%lu %08lx%08lx %08lx %08lx %08lx %lu %lu", pt, (unsigned long)(world >> 32), (unsigned long)world,
            (unsigned long)(render >> 32), (unsigned long)rng,
            (unsigned long)maprng, n, (unsigned long)inputs);
    if (pai.on) {   /* ai column (version 5): one 8-hex hash per world player ('-' not a computer player), '=' not hashed this PT */
        if (pai.every > 1 && pt % pai.every && !pth_needs_snapshot(pt)) fputs(" =", pth.out);
        else {
            LARGE_INTEGER t0, t1; char tok[17 * 9 + 2]; int len = 0;
            QueryPerformanceCounter(&t0);
            pai_players = players; pai_nplayers = nplayers; pai.objects = 0;
            if (!pai.maxobjects) pai.maxobjects = 400000;
            for (i = 0; i < (int)nplayers; i++) {
                u64 a = 0x9b05688c2b3e6c1full;
                if (!players[i] || pth_u32(players[i]) != pai.vt) { len += sprintf(tok + len, "%s-", i ? "." : ""); continue; }
                pth_ok = 1;
                a = pai_walk(a, players[i], pai.root, 0);
                len += sprintf(tok + len, "%s%08lx", i ? "." : "", (unsigned long)(a >> 32));
            }
            pth_ok = 1;
            QueryPerformanceCounter(&t1);
            pai_ticks += t1.QuadPart - t0.QuadPart; pai_lines++; pai_objs += pai.objects;
            fprintf(pth.out, " %s", nplayers ? tok : "-");
        }
    }
    for (i = 0; i < (int)nplayers; i++) fprintf(pth.out, " %08lx", (unsigned long)(ph[i] >> 32));
    fputc('\n', pth.out);
    if (++pth.lines % 1024 == 0) {
        if (pai.on && pth.lines % 4096 == 0 && pai_lines) {   /* cost record (compare.py reads '#stat' lines) */
            LARGE_INTEGER f; QueryPerformanceFrequency(&f);
            fprintf(pth.out, "#stat {\"pt\": %lu, \"lines\": %lu, \"ai_lines\": %lu, \"ai_ms_per_line\": %.4f, "
                    "\"ai_objects_per_line\": %lu, \"ai_read_failures\": %lu, \"ai_invalid\": %lu, "
                    "\"block_reads_per_line\": %lu, \"cache_evictions\": %lu}\n",
                    pt, pth.lines, pai_lines, 1000.0 * pai_ticks / f.QuadPart / pai_lines,
                    (unsigned long)(pai_objs / pai_lines), pai.fails, pai_invalid, pth_reads / pth.lines, pth_evicts);
        }
        fflush(pth.out);
    }
    return pt;
}

static void ptsync_hit(void)
{
    static DWORD timeout;
    ptsync_hits++;
    if (!timeout) { const char *t = getenv("EE_PTSYNC_TIMEOUT_MS"); timeout = t ? strtoul(t, NULL, 0) : 120000; }
    if (pth.on) {
        unsigned long pt;
        if (!pth_cache) pth_cache = calloc(PTH_PAGES, sizeof *pth_cache);
        pt = pth_line();
        if (!pth_needs_snapshot(pt)) return;
        if (WaitForSingleObject(ptsync_on, 0) != WAIT_OBJECT_0) { pth.snaps_missed++; return; }
        if (pth.has_stop && pt >= pth.stop_after) fflush(pth.out);
        if (pth.keyfile[0]) { FILE *kf = fopen(pth.keyfile, "w"); if (kf) { fprintf(kf, "%lu\n", pt); fclose(kf); } }
    } else if (WaitForSingleObject(ptsync_on, 0) != WAIT_OBJECT_0) return;
    SetEvent(ptsync_req);
    if (WaitForSingleObject(ptsync_ack, timeout) != WAIT_OBJECT_0) {
        printf("TEST pt sync: sampler did not answer within %lu ms; detached\n", (unsigned long)timeout);
        ResetEvent(ptsync_on);
        return;
    }
    ptsync_sampled++;
    if (!(ptsync_sampled & (ptsync_sampled - 1)) || !(ptsync_sampled % 100000))
    {
        LARGE_INTEGER f; QueryPerformanceFrequency(&f);
        printf("TEST pt sync sampled %lu of %lu hits (C lines %lu, snapshots missed %lu, %lu block reads/line; ai %lu lines "
               "%.3f ms/line %lu objects/line %lu read failures %lu invalid; %lu cache evictions)\n",
               ptsync_sampled, ptsync_hits, pth.lines, pth.snaps_missed, pth.lines ? pth_reads / pth.lines : 0,
               pai_lines, pai_lines ? 1000.0 * pai_ticks / f.QuadPart / pai_lines : 0.0,
               pai_lines ? (unsigned long)(pai_objs / pai_lines) : 0, pai.fails, pai_invalid, pth_evicts);
    }
}

#endif
