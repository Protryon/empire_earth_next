"""TEST ONLY: locate the windowed/software/session overlay sites in a relaid full-link EE-AOC.exe.

python -I tools/boot/fulllink_locator.py [build/exe_fulllink/EE-AOC.exe] [--map build/exe_fulllink/EE-AOC.map]

The overlays in windowed_overlay.py, exe_boot.c (audio observer), presentation_bridge.c and game_state_probe.py
name fixed ORIGINAL addresses. tools/exe_fulllink.py relays the image, so those addresses are wrong there. This
module finds each patched function and each telemetry global by NAME in the full link's LINK map and proves the
body before any byte is changed:

  body check  the located function is disassembled next to the original one (capstone). Every instruction must
              have the same length and mnemonic. A byte may differ only inside a 4-byte address field: an absolute
              field that tools/xref_noreloc.py lists for the original, or the rel32 of a call/jmp leaving the
              function. For each differing field the original's target name (names.txt, nearest name + offset)
              and ours (map, nearest public + offset) must agree; a field whose original target carries no name
              within 0x40 bytes is counted as `unnamed` and must point to the same kind (code/data) on both sides.
              Any other difference, or a function whose extent is shorter than the original's, refuses.
  patches     the same preimage/replacement as the original-image overlay, at the same offset in the function.
              An address field inside a preimage is checked through the body check, never copied.
  near bodies EEInitializeDisplay (software profile) and EEGamePreferences::Load (a changed body, e.g. a fixes
              branch) that fail the body check are located by map name and patched by meaning
              (near_display_patches, near_load_patches): each refuses unless it finds exactly the expected
              instructions.

The original-image paths (windowed_overlay.build_windowed_image etc.) are not changed by this module.
Images it writes are test artifacts: never verifier inputs, never matching credit.
"""
import argparse
import bisect
import itertools
import hashlib
import json
import pathlib
import re
import sys

import capstone
import pefile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import windowed_overlay as overlay  # noqa: E402

ROOT = overlay.ROOT
FULLLINK = ROOT / 'build/exe_fulllink'
NAMES = ROOT / 'analysis/exe/names.txt'
XREF = ROOT / 'build/exe/xref_noreloc.txt'
BASE = 0x400000

# original function RVA -> its symbol (names.txt). Extents are windowed_overlay.FUNCTIONS'.
FUNCTION_NAMES = {
    0x1324F8: '??0EEGameSettings@@QAE@XZ',
    0x1328DD: '?Load@EEGamePreferences@@QAEJXZ',
    0x143557: '?EEInitializeSounds@@YA_NXZ',
    0x1444D0: '?EEInitializeDisplay@@YA_NXZ',
}
# telemetry/UI globals the session helpers read (original VA -> symbol)
GLOBAL_NAMES = {
    0x93191C: '?gUIManager@@3PAVEEUIManager@@A',
    0x9319C8: '?gCommunications@@3PAVNESynchronousCommunications@@A',
    0x930D40: '?gWorld@@3VEEWorld@@A',
    0x931924: '?gMouse@@3PAVEEMouse@@A',   # play.py's mouse shim (presentation_bridge.c, EE_MOUSE_VA)
}
SOFTWARE_EXE_PATCHES = [
    (0x14460A, bytes.fromhex('6a10'), bytes.fromhex('6a20'),
     'software test policy: keep requested 32-bit render/texture format despite legacy HAL capability heuristic'),
    # the mov's address field (bytes 2..5) is checked by the body check (same target name), only byte 6 changes
    (0x144715, bytes.fromhex('c7057408930010000000'), bytes.fromhex('c7057408930020000000'),
     'software test policy: 32-bit fallback mode matches the native desktop catalog'),
]
AL_ZERO = 0x132659   # windowed_overlay's constructor precondition (xor eax,eax)
OBSERVED_ONLY = {0x143557}
PREFS_LOAD = 0x1328DD       # EEGamePreferences::Load: a near (changed) body gets near_load_patches
DISPLAY = 0x1444D0          # EEInitializeDisplay: software patches; near bodies get near_display_patches   # EEInitializeSounds: entry guard + result observer (exe_boot.c), never patched


def sha(data):
    return hashlib.sha256(data).hexdigest()


class LinkMap:
    """Publics and statics of a LINK 6 / lld-link map: name -> VA, plus sorted VAs for nearest-name lookups.
    An x64 map also answers for i686 decorated names (x64names.py): the harness names the game's symbols as the 32-bit
    build decorates them. x64: the image next to the map is PE32+ (else: 16-digit addresses, or x64 member-function
    decorations; the fork's x64 exe is /FIXED at 0x400000, so its map has 8-digit addresses). base: the map's preferred
    load address (rva = address - base)."""
    LINE = re.compile(r'^ ([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{16}|[0-9a-f]{8})\s+(f\s+)?(i\s+)?(\S+)?', re.I)
    PREFERRED = re.compile(r'^\s*Preferred load address is ([0-9a-f]+)', re.I)

    def __init__(self, path):
        self.path = pathlib.Path(path)
        self.by_name, self.rows = {}, []
        self.base, self.x64, self._resolver = BASE, False, None
        for line in self.path.read_text(encoding='latin-1').splitlines():
            p = self.PREFERRED.match(line)
            if p:
                self.base = int(p.group(1), 16)
                continue
            m = self.LINE.match(line)
            if not m:
                continue
            va, name = int(m.group(4), 16), m.group(3)
            self.x64 |= len(m.group(4)) == 16
            if va < self.base:
                continue
            self.by_name.setdefault(name, va)
            self.rows.append((va, name, bool(m.group(5)), m.group(7) or ''))
        image = self.path.with_suffix('.exe')
        if image.exists():
            data = image.read_bytes()[:0x400]
            pe = int.from_bytes(data[0x3c:0x40], 'little')
            self.x64 = data[pe:pe + 4] == b'PE\0\0' and int.from_bytes(data[pe + 4:pe + 6], 'little') == 0x8664
        elif not self.x64:
            self.x64 = any(re.search(r'@@[QAIUEM]E[AB]A', n) for n in itertools.islice(self.by_name, 0, 20000))
        self.rows.sort()
        self.vas = [r[0] for r in self.rows]

    def name(self, name):
        """The map's own name for name (an i686 decorated name is translated for an x64 map)."""
        if name in self.by_name or not self.x64:
            return name
        if self._resolver is None:
            import x64names
            self._resolver = x64names.Resolver(self.by_name)
        try:
            return self._resolver.resolve(name)
        except KeyError as error:
            raise ValueError(f'{name} is not in {self.path}: {error}') from None

    def address(self, name):
        name = self.name(name)
        if name not in self.by_name:
            raise ValueError(f'{name} is not in {self.path}')
        return self.by_name[name]

    def rva(self, name):
        return self.address(name) - self.base

    def names_at(self, va):
        i = bisect.bisect_left(self.vas, va)
        out = []
        while i < len(self.rows) and self.rows[i][0] == va:
            out.append(self.rows[i][1]); i += 1
        return out

    def nearest(self, va):
        i = bisect.bisect_right(self.vas, va) - 1
        if i < 0:
            return None, 0
        at = self.vas[i]
        return set(self.names_at(at)), va - at

    def extent(self, name):
        """Start VA and the next higher symbol VA (the function's maximal extent)."""
        start = self.address(name)
        i = bisect.bisect_right(self.vas, start)
        return start, (self.vas[i] if i < len(self.vas) else start)


def original_names():
    by = {}
    for line in NAMES.read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):
            continue
        parts = line.split()
        if len(parts) >= 2:
            by.setdefault(int(parts[0], 16), []).append(parts[1])
    return by


def original_fields():
    fields = {}
    for line in XREF.read_text(encoding='utf-8').splitlines():
        if line.startswith('#') or not line.strip():
            continue
        f, t, _kind = line.split()[:3]
        fields[int(f, 16)] = int(t, 16)
    return fields


class Prover:
    def __init__(self, image_path, map_path):
        self.image_path = pathlib.Path(image_path)
        self.data = self.image_path.read_bytes()
        self.pe = pefile.PE(data=self.data, fast_load=True)
        if self.pe.OPTIONAL_HEADER.ImageBase != BASE:
            raise ValueError('Only the fixed-layout 0x400000 image is supported')
        self.map = LinkMap(map_path)
        self.ref_data = overlay.REFERENCE.read_bytes()
        self.ref = pefile.PE(data=self.ref_data, fast_load=True)
        self.onames = original_names()
        self.onames_sorted = sorted(self.onames)
        self.ofields = original_fields()
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.oimports = {}
        ref = pefile.PE(data=self.ref_data, fast_load=True)
        ref.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
        for dll in getattr(ref, 'DIRECTORY_ENTRY_IMPORT', []):
            for imp in dll.imports:
                if imp.name:
                    self.oimports[imp.address - BASE] = imp.name.decode('latin-1')

    @staticmethod
    def is_code(pe, rva):
        s = pe.get_section_by_rva(rva)
        return bool(s and s.Characteristics & 0x20000000)

    def onearest(self, rva):
        i = bisect.bisect_right(self.onames_sorted, rva) - 1
        if i < 0 or rva - self.onames_sorted[i] > 0x40:
            return None, 0
        at = self.onames_sorted[i]
        return set(self.onames[at]), rva - at

    def field_agrees(self, orva, ova):
        """Original target RVA vs our target VA: same name+offset, else `unnamed` with the same kind."""
        if not 0 <= ova - BASE < self.pe.OPTIONAL_HEADER.SizeOfImage:
            return 'refuse', f'our target {ova:#x} is outside the image'
        onames, ooff = self.onearest(orva)
        mnames, moff = self.map.nearest(ova)
        # our symbol+offset names the original's symbol at the same offset (any distance)
        if mnames and set(self.onames.get(orva - moff, ())) & mnames:
            return 'named', sorted(set(self.onames[orva - moff]) & mnames)[0] + (f'+{moff:#x}' if moff else '')
        if onames:
            if mnames and onames & mnames and ooff == moff:
                return 'named', sorted(onames & mnames)[0] + (f'+{ooff:#x}' if ooff else '')
            # a compiler string literal (??_C@) where names.txt carries a placeholder for the same bytes: compare
            # the NUL-terminated contents at both targets
            if mnames and moff == 0 and all(n.startswith('??_C@') for n in mnames):
                ours = self.pe.get_data(ova - BASE, 0x400)
                text = ours[:ours.index(b'\0') + 1] if b'\0' in ours else None
                if text is not None and self.ref.get_data(orva, len(text)) == text:
                    return 'unnamed', f'string literal {text[:24]!r} ({sorted(onames)[0]})'
            return 'refuse', f'original {sorted(onames)[0]}+{ooff:#x} vs ours {sorted(mnames or ["?"])[0]}+{moff:#x}'
        # an import slot: the original's IAT entry name vs our map's __imp_ name
        if orva in self.oimports:
            want = self.oimports[orva]
            plain = {re.sub(r'^_|@\d+$', '', n[6:]) for n in (mnames or ()) if n.startswith('__imp_')}
            if mnames and moff == 0 and (('__imp_' + want) in mnames or want in plain):
                return 'named', '__imp_' + want
            return 'refuse', f'original import {self.oimports[orva]} vs ours {sorted(mnames or ["?"])[0]}+{moff:#x}'
        # an unnamed original string: our compiler literal must hold the same NUL-terminated bytes
        if mnames and moff == 0 and all(n.startswith('??_C@') for n in mnames):
            ours = self.pe.get_data(ova - BASE, 0x400)
            text = ours[:ours.index(b'\0') + 1] if b'\0' in ours else None
            if text is None or self.ref.get_data(orva, len(text)) != text:
                return 'refuse', f'string literal at {ova:#x} differs from the original {orva:#x}'
            return 'named', f'string literal {text[:24]!r}'
        if self.is_code(self.ref, orva) != self.is_code(self.pe, ova - BASE):
            return 'refuse', f'original target {orva:#x} and ours {ova:#x} differ in kind (code/data)'
        return 'unnamed', f'{orva:#x}->{ova:#x} (ours {sorted(mnames or ["?"])[0]}+{moff:#x})'

    def prove(self, orva, oend, name):
        """Instruction-level proof that our `name` is the original [orva, oend). Returns (our rva, report)."""
        size = oend - orva
        start, limit = self.map.extent(name)
        nrva = start - BASE
        if limit - start < size:
            raise ValueError(f'{name}: our extent {limit - start:#x} is shorter than the original {size:#x}')
        ob, nb = self.ref.get_data(orva, size), self.pe.get_data(nrva, size)
        if len(ob) != size or len(nb) != size:
            raise ValueError(f'{name}: function bytes unavailable')
        oins = list(self.md.disasm(ob, BASE + orva))
        nins = list(self.md.disasm(nb, BASE + nrva))
        if sum(i.size for i in oins) != size or len(oins) != len(nins):
            raise ValueError(f'{name}: instruction streams differ in count or do not decode to the end')
        counts = dict(identical=0, relocated=0)
        named, unnamed = [], []
        for oi, ni in zip(oins, nins):
            if oi.size != ni.size or oi.mnemonic != ni.mnemonic:
                raise ValueError(f'{name}+{oi.address - BASE - orva:#x}: {oi.mnemonic} {oi.op_str} vs {ni.mnemonic} {ni.op_str}')
            if oi.bytes == ni.bytes:
                counts['identical'] += 1
                continue
            off = oi.address - BASE - orva
            diff = [k for k in range(oi.size) if oi.bytes[k] != ni.bytes[k]]
            covered = set()
            # absolute fields the original carries (xref_noreloc)
            for k in range(oi.size - 3):
                frva = orva + off + k
                if frva in self.ofields:
                    ova = int.from_bytes(ni.bytes[k:k + 4], 'little')
                    verdict, text = self.field_agrees(self.ofields[frva], ova)
                    if verdict == 'refuse':
                        raise ValueError(f'{name}+{off + k:#x}: {text}')
                    (named if verdict == 'named' else unnamed).append(text)
                    covered.update(range(k, k + 4))
            # rel32 of a call/jmp that leaves the function (internal targets must be byte-identical)
            if oi.mnemonic in ('call', 'jmp') and oi.size == 5 and oi.bytes[0] in (0xE8, 0xE9):
                ot = (oi.address + 5 + int.from_bytes(oi.bytes[1:5], 'little', signed=True)) & 0xFFFFFFFF
                nt = (ni.address + 5 + int.from_bytes(ni.bytes[1:5], 'little', signed=True)) & 0xFFFFFFFF
                if BASE + orva <= ot < BASE + oend:
                    raise ValueError(f'{name}+{off:#x}: internal branch differs')
                verdict, text = self.field_agrees(ot - BASE, nt)
                if verdict == 'refuse':
                    raise ValueError(f'{name}+{off:#x}: call/jmp {text}')
                (named if verdict == 'named' else unnamed).append(text)
                covered.update(range(1, 5))
            if any(k not in covered for k in diff):
                raise ValueError(f'{name}+{off:#x}: non-address bytes differ ({oi.bytes.hex()} vs {ni.bytes.hex()})')
            counts['relocated'] = counts.get('relocated', 0) + 1
        return nrva, dict(name=name, original_rva=hex(orva), rva=hex(nrva), size=size,
                          instructions=len(oins), identical_instructions=counts['identical'],
                          address_fields_named=len(named), address_fields_unnamed=len(unnamed),
                          unnamed_fields=unnamed, original_sha256=sha(ob), located_sha256=sha(nb))

    def global_va(self, original_va):
        return self.map.address(GLOBAL_NAMES[original_va])


def locate(image, map_path, software_display=False):
    """Prove every overlay function in the full-link image; return the relocation of each fixed address."""
    prover = Prover(image, map_path)
    functions, moved, near = [], {}, {}
    ext = overlay.FUNCTIONS + ([(0x1444D0, 0x14494D)] if software_display else [])
    for start, end in ext:
        name = FUNCTION_NAMES[start]
        if start in OBSERVED_ONLY:
            # no byte of it is patched: the harness only guards its entry and reads its bool result. A near body
            # is located by name only (it must be a code symbol); its proof result is recorded, never required.
            nrva = prover.map.address(name) - BASE
            if not prover.is_code(prover.pe, nrva):
                raise ValueError(f'{name} is not in a code section')
            try:
                report = dict(prover.prove(start, end, name)[1], status='proven')
            except ValueError as why:
                report = dict(name=name, original_rva=hex(start), rva=hex(nrva), status='near (observed only)',
                              proof_failure=str(why))
            functions.append(report)
            moved[start] = (nrva, 1)   # only the entry is meaningful
            continue
        try:
            nrva, report = prover.prove(start, end, name)
        except ValueError as why:
            if start not in (DISPLAY, PREFS_LOAD):
                raise
            nrva = prover.map.address(name) - BASE
            report = dict(name=name, original_rva=hex(start), rva=hex(nrva), status='near (semantic patch)',
                          proof_failure=str(why))
            near[start] = nrva
            functions.append(report)
            continue
        functions.append(dict(report, status='proven'))
        moved[start] = (nrva, end - start)
    return prover, functions, moved, near


def near_display_patches(prover, nrva):
    """The software policy for a NEAR EEInitializeDisplay, by meaning instead of by offset.

    In the original, `push 0x10; pop esi` (0x14460a) holds the one constant 16 that the two bit-depth checks
    compare with and store (EEPreferences mSettings.mGameBitDepth = gGamePreferences+0x56c, mTextureBitDepth
    +0x5ac): `if ((d != 32 && d != 16) || (d == 32 && !support)) d = 16;`, twice. Its overlay turns that 16 into
    32, and 0x144715 (the no-matching-mode fallback `mGameBitDepth = 16`) into 32. Our near body has the 16 as
    immediates: per depth field `mov eax,[field]; cmp eax,0x20; je; cmp eax,0x10` and `mov [field],0x10`, plus
    the fallback store. Exactly 2 compares and 3 stores must be found (else refuse); each 0x10 becomes 0x20,
    which is the original overlay's meaning (5 one-byte changes instead of 2)."""
    if 'gGamePreferences' not in ' '.join(prover.onames.get(0x530308, [])) or 0x930874 - 0x930308 != 0x56c:
        raise ValueError('gGamePreferences evidence changed')
    prefs = prover.map.address('?gGamePreferences@@3VEEGamePreferences@@A')
    fields = {prefs + 0x56c: 'mGameBitDepth', prefs + 0x5ac: 'mTextureBitDepth'}
    start, limit = prover.map.extent('?EEInitializeDisplay@@YA_NXZ')
    code = prover.pe.get_data(nrva, limit - start)
    ins = list(prover.md.disasm(code, start))
    patches, cmps, stores = [], 0, 0
    for k, i in enumerate(ins):
        b = i.bytes
        if (b[:2] == bytes.fromhex('c705') and len(b) == 10 and int.from_bytes(b[2:6], 'little') in fields
                and b[6:] == bytes.fromhex('10000000')):
            stores += 1
            patches.append((i.address - BASE + 6, bytes.fromhex('10'), bytes.fromhex('20'),
                            f'near-body software policy: {fields[int.from_bytes(b[2:6], "little")]} = 32 (was 16)'))
        if (b == bytes.fromhex('83f810') and k >= 3 and ins[k - 3].bytes[:1] == bytes.fromhex('a1')
                and int.from_bytes(ins[k - 3].bytes[1:5], 'little') in fields
                and ins[k - 2].bytes == bytes.fromhex('83f820') and ins[k - 1].mnemonic == 'je'):
            cmps += 1
            patches.append((i.address - BASE + 2, bytes.fromhex('10'), bytes.fromhex('20'),
                            f'near-body software policy: accepted {fields[int.from_bytes(ins[k - 3].bytes[1:5], "little")]} 16 -> 32'))
    if (cmps, stores) != (2, 3):
        raise ValueError(f'near EEInitializeDisplay: found {cmps} depth compares and {stores} stores, expected 2 and 3')
    return patches


def near_load_patches(prover, nrva):
    """The windowed overlay's preferences-Load patch for a NEAR (changed) EEGamePreferences::Load, by meaning.

    In the original, 0x132fbf `mov byte ptr [eax+0x5d3], 1` (this = [ebp-4]; +4 mSettings, +0x5cf) sets the settings
    flag the overlay keeps false; the overlay turns its immediate 1 into 0. A near body (a source change elsewhere in
    Load, e.g. a fixes branch) is located by map name only; its store is found by meaning: exactly one
    `mov byte ptr [r32 + 0x5d3], 1` in the function's map extent (else refuse), and its immediate becomes 0
    (the same one-byte change)."""
    start, limit = prover.map.extent(FUNCTION_NAMES[PREFS_LOAD])
    code = prover.pe.get_data(nrva, limit - start)
    hits = [i for i in prover.md.disasm(code, start)
            if i.mnemonic == 'mov' and re.fullmatch(r'byte ptr \[e[a-ds][xip] \+ 0x5d3\], 1', i.op_str)]
    if len(hits) != 1:
        raise ValueError(f'near EEGamePreferences::Load: found {len(hits)} `mov byte [reg+0x5d3], 1` stores, expected 1')
    i = hits[0]
    return [(i.address - BASE + i.size - 1, bytes.fromhex('01'), bytes.fromhex('00'),
             'near-body preferences Load: keep settings +4+0x5cf false')]


def relocate(rva, moved):
    for start, (nrva, size) in moved.items():
        if start <= rva < start + size:
            return nrva + rva - start
    raise ValueError(f'{rva:#x} is outside every proven function')


def original_backed_hotfix(prover, spec):
    """TEST ONLY triage aid, opt-in: 'SYMBOL+0xOFF' -> put the ORIGINAL's instruction at that offset into our body.

    Allowed only when the function exists on both sides at the same instruction boundaries up to and including that
    instruction (same lengths/mnemonic class), the instruction has no address field in the original (xref_noreloc)
    and is not a call/jmp rel32. So a source bug that a running session hits (e.g. a signed `jge` where the original
    has `jae`) can be stepped over in a staged copy to find the next one. Never source, never credit."""
    name, off = spec.rsplit('+', 1)
    off = int(off, 16)
    oname = [r for r, ns in prover.onames.items() if name in ns]
    if len(oname) != 1:
        raise ValueError(f'hotfix {spec}: {name} has {len(oname)} original addresses')
    orva = oname[0]
    nrva = prover.map.address(name) - BASE
    # body check: the same function length (original: up to the next names.txt address; ours: up to the next map
    # symbol, alignment padding allowed), the same instruction lengths throughout, and the same mnemonics except
    # at the hotfix instruction
    k = bisect.bisect_right(prover.onames_sorted, orva)
    size = prover.onames_sorted[k] - orva if k < len(prover.onames_sorted) else 0
    start, limit = prover.map.extent(name)
    if not 0 < size <= 0x4000 or limit - start < size or \
            any(b not in (0xCC, 0x90) for b in prover.pe.get_data(nrva + size, limit - start - size)):
        raise ValueError(f'hotfix {spec}: function lengths differ (original {size:#x}, ours {limit - start:#x})')
    ob = prover.ref.get_data(orva, size)
    nb = prover.pe.get_data(nrva, size)
    oi = list(prover.md.disasm(ob, BASE + orva))
    ni = list(prover.md.disasm(nb, BASE + nrva))
    if sum(i.size for i in oi) != size or [i.size for i in oi] != [i.size for i in ni] or \
            any(a.mnemonic != b.mnemonic for a, b in zip(oi, ni) if a.address - BASE - orva != off):
        raise ValueError(f'hotfix {spec}: instruction streams differ beyond the hotfix instruction')
    ok = [i for i in oi if i.address - BASE - orva == off]
    nk = [i for i in ni if i.address - BASE - nrva == off]
    if not ok or not nk:
        raise ValueError(f'hotfix {spec}: offset is not an instruction boundary on both sides')
    o, n = ok[0], nk[0]
    if [i.size for i in oi if i.address - BASE - orva <= off] != [i.size for i in ni if i.address - BASE - nrva <= off]:
        raise ValueError(f'hotfix {spec}: instruction lengths differ before the offset')
    if o.size != n.size or o.bytes[0] in (0xE8, 0xE9) or any(orva + off + k in prover.ofields for k in range(o.size)):
        raise ValueError(f'hotfix {spec}: not a same-size, address-free instruction')
    if o.bytes == n.bytes:
        raise ValueError(f'hotfix {spec}: already equal to the original')
    return dict(spec=spec, original_rva=hex(orva + off), rva=nrva + off, before=bytes(n.bytes), after=bytes(o.bytes),
                ours=f'{n.mnemonic} {n.op_str}', original=f'{o.mnemonic} {o.op_str}')


def build_fulllink_windowed_image(source, output, map_path, software_display=False, hotfixes=()):
    """windowed_overlay.build_windowed_image for a relaid full-link image (by map name + body proof).
    hotfixes: opt-in original_backed_hotfix specs (triage only)."""
    source, output = pathlib.Path(source).resolve(), pathlib.Path(output).resolve()
    overlay.test_output(source, output)
    original = source.read_bytes()
    prover, functions, moved, near = locate(source, map_path, software_display)
    pe = prover.pe
    al = relocate(AL_ZERO, moved)
    if pe.get_data(al, 2) != bytes.fromhex('33c0'):
        raise ValueError('Constructor AL-zero precondition failed')
    result = bytearray(original)
    changes = []
    load_end = dict(overlay.FUNCTIONS)[PREFS_LOAD]
    plan = [(rva, relocate(rva, moved), before, after, reason) for rva, before, after, reason in overlay.PATCHES
            if not (PREFS_LOAD in near and PREFS_LOAD <= rva < load_end)]
    if PREFS_LOAD in near:
        if sum(PREFS_LOAD <= p[0] < load_end for p in overlay.PATCHES) != 1:
            raise ValueError('windowed_overlay patches in EEGamePreferences::Load changed: near Load unsupported')
        plan += [(None, nrva, before, after, reason) for nrva, before, after, reason in near_load_patches(prover, near[PREFS_LOAD])]
    if software_display and DISPLAY in near:
        plan += [(None, nrva, before, after, reason) for nrva, before, after, reason in near_display_patches(prover, near[DISPLAY])]
    elif software_display:
        plan += [(rva, relocate(rva, moved), before, after, reason) for rva, before, after, reason in SOFTWARE_EXE_PATCHES]
    for rva, nrva, before, after, reason in plan:
        offset = pe.get_offset_from_rva(nrva)
        cur = bytes(result[offset:offset + len(before)])
        # bytes equal in before/after outside an address field must match; an address field (already proven by
        # the body check) is kept as ours
        fields = {k for f in range(len(before) - 3) if rva is not None and rva + f in prover.ofields
                  for k in range(f, f + 4)}
        if any(cur[k] != before[k] for k in range(len(before)) if k not in fields):
            raise ValueError(f'Unexpected patch preimage at {rva} (located {nrva:#x})')
        if any(before[k] != after[k] for k in fields):
            raise ValueError(f'Patch at {rva} would change an address field')
        new = bytes(after[k] if k not in fields else cur[k] for k in range(len(before)))
        result[offset:offset + len(before)] = new
        changes.append(dict(original_rva=hex(rva) if rva is not None else None, rva=hex(nrva), file_offset=offset, before=cur.hex(),
                            after=new.hex(), reason=reason))
    expected = len(plan)   # one byte per patch entry (2, +2 software, or +5 for a near EEInitializeDisplay)
    if any(sum(a != b for a, b in zip(p[2], p[3])) != 1 for p in plan):
        raise ValueError('every overlay patch changes exactly one byte')
    hot = []
    for spec in hotfixes:
        h = original_backed_hotfix(prover, spec)
        offset = pe.get_offset_from_rva(h['rva'])
        if result[offset:offset + len(h['before'])] != h['before']:
            raise ValueError(f'hotfix {spec}: overlaps another patch')
        result[offset:offset + len(h['before'])] = h['after']
        expected += sum(a != b for a, b in zip(h['before'], h['after']))
        hot.append(dict(h, rva=hex(h['rva']), before=h['before'].hex(), after=h['after'].hex()))
    differences = [i for i, (a, b) in enumerate(zip(original, result)) if a != b]
    if len(result) != len(original) or len(differences) != expected:
        raise ValueError(f'Overlay must change exactly {expected} bytes without changing file size')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(result)
    globals_ = {GLOBAL_NAMES[va]: dict(original_va=hex(va), va=hex(prover.global_va(va))) for va in GLOBAL_NAMES}
    manifest = dict(test_only=True, kind='native-windowed-preferences-fulllink-v1', source=str(source),
                    output=str(output), map=str(pathlib.Path(map_path).resolve()),
                    map_sha256=sha(pathlib.Path(map_path).read_bytes()),
                    input_sha256=sha(original), output_sha256=sha(bytes(result)), changed_bytes=expected,
                    software_display=software_display, changed_file_offsets=differences, functions=functions,
                    patches=changes, audio_entry_rva=hex(moved[0x143557][0]), globals=globals_,
                    original_backed_hotfixes=hot,
                    proof='instruction-level: same lengths and mnemonics; differences only in address fields whose '
                          'targets agree by name+offset (names.txt vs map), or unnamed with the same kind',
                    source_purity='No src/, include/, orig-bin/, or canonical build changes; not matching credit')
    output.with_suffix('.windowed.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    if source.read_bytes() != original:
        raise RuntimeError('Input changed during overlay generation')
    return manifest


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('image', nargs='?', type=pathlib.Path, default=FULLLINK / 'EE-AOC.exe')
    ap.add_argument('--map', type=pathlib.Path)
    ap.add_argument('--software-display', action='store_true')
    ap.add_argument('--output', type=pathlib.Path, help='also write the windowed overlay (below build/windowed/)')
    a = ap.parse_args()
    mp = a.map or a.image.with_suffix('.map')
    if a.output:
        m = build_fulllink_windowed_image(a.image, a.output, mp, a.software_display)
    else:
        prover, functions, moved, near = locate(a.image, mp, a.software_display)
        m = dict(functions=functions, globals={GLOBAL_NAMES[v]: hex(prover.global_va(v)) for v in GLOBAL_NAMES})
    print(json.dumps(m, indent=2))


if __name__ == '__main__':
    main()
