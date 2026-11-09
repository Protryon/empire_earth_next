"""Round 39 differential harness (TEST ONLY): object layouts and addresses for the per-PT sampler.

Field tables come from the reconstructed headers (include/ee/EEUnit.h, EEPlayer.h): every member line of the form
`<type> mName[;[N]];  // +0xOFF ...` of the listed classes. A member is HASHED when its type is a plain scalar (or a
known POD of scalars: GE3DPoint, U2DPoint<..>, GEColor, GEOrientation, EEEffectHandle) and NOT in NOISE; pointers,
containers, strings and unknown composites are not hashed (their bytes stay in the full snapshots, labelled, so a
diff still names them). A few pointer members are hashed through their pointee's stable identity (UNIT_REFS: the
target unit's id; the player's slot; the class's +0x28; the goal's kind/serial).

Addresses: the original by fixed VA, a full link by name from its LINK map (tools/boot/fulllink_locator.LinkMap).
"""
import pathlib, re, struct

ROOT = pathlib.Path(__file__).resolve().parents[3]
INCLUDE = ROOT / 'include/ee'

SCALARS = {'long': 4, 'unsigned long': 4, 'int': 4, 'unsigned int': 4, 'float': 4, 'bool': 1, 'char': 1,
           'unsigned char': 1, 'short': 2, 'unsigned short': 2, 'double': 8, 'DWORD': 4, 'BYTE': 1,
           'GE3DPoint': 12, 'U2DPoint<long>': 8, 'U2DPoint<float>': 8, 'GEColor': 12, 'GEOrientation': 12,
           'EEEffectHandle': 4, 'EETileCoordinate': 8}

MEMBER = re.compile(r'^\s+((?:const\s+)?(?:class\s+)?[A-Za-z_][\w:<>, *]*?[\w>*])\s*\*?\s*(m[A-Za-z0-9_]+)\s*(\[\s*(\w+)\s*\])?\s*;'
                    r'\s*(?://\s*\+0x([0-9a-fA-F]+))?')
COMMENT_OFF = re.compile(r'^\s*//\s*\+0x([0-9a-fA-F]+)\b')

# (header, class, end offset of the class's own members or None = next class's first member)
UNIT_CLASSES = [('RTSUnit.h', 'RTSUnit'), ('EEUnit.h', 'EEUnit'), ('EEUnit.h', 'EEComplexUnit')]
PLAYER_CLASSES = [('EEPlayer.h', 'RTSPlayer'), ('EEPlayer.h', 'EEPlayer')]
EEUNIT_SIZE = 0x48            # RTSUnit 0x2c + EEUnit members to +0x44 (byte) -> 0x48
COMPLEX_SIZE = 0x364          # EEComplexUnit: last member mF360 +0x360
PLAYER_SIZE = 0xb80           # EEPlayer (EEPlayer.h: sizeof 0xb80)
GOAL_SIZE = 0x48              # the largest goal object proven so far is 0x44
PHYSICS_SIZE = 0x100

# GEPhysicalModel at EEComplexUnit+0x130 (engine class, no header layout): 4x4 matrix +0x130..+0x16c, model pointer
# +0x170 (not hashed), scale +0x174, frame +0x178 (from raw dumps, round 36/39)
EXTRA_UNIT_FIELDS = [(0x130 + 4 * i, 4, 'mPhysicalModel.mMatrix[%d]' % i, 'float') for i in range(16)] + [
    (0x170, 4, 'mPhysicalModel.mModel', 'ptr'), (0x174, 4, 'mPhysicalModel.mScale', 'float'),
    (0x178, 4, 'mPhysicalModel.mFrame', 'unsigned long')]

# members measured as run noise (differ between two runs of the same image with the same pins); see notes.md
NOISE = {'mReferenceCount'}

# UI-WRITTEN player state (set on the UI thread at a wall-time-dependent PT): the local player's civilization-builder
# flags mBB49 (+0xb49) / mBB4A (+0xb4a) (setup's ai-game civ preempt opens/cancels the builder; EEMCivTimerExpired
# 0x4bd008 also sets mBB49 of the other players when it is not preempted). Not in the SIM hashes; class 'ui' in diffs.
UI_FIELDS = {'mCivilizationLocked', 'mCivilizationChosen'}
# OPT-IN (round 39, agent input; root-approved 2026-11-01): with --human-script the session also masks the local
# player's acknowledgement-sound throttle EEPlayer::mLAD8 (+0xad8, a TMGetTime() stamp) / mLADC (+0xadc, the active
# group's id at that time), written by EEPlayer::Virtual8C 0x567f10 when a human command executes; they only gate
# EEFn14831C (gSoundManager->Play). EXTRA_UI_FIELDS is empty unless enable_human_ui_mask() is called (session_hook
# for --human-script: stream version 8, header ui_extra), so the default hash and every existing stream are unchanged.
HUMAN_UI_FIELDS = {'mLastAcknowledgementTime', 'mLastAcknowledgedGroup'}
EXTRA_UI_FIELDS = set()


def enable_human_ui_mask():
    EXTRA_UI_FIELDS.update(HUMAN_UI_FIELDS)
    return sorted(EXTRA_UI_FIELDS)


# OPT-IN (fixes branch, 2026-11-03): EE_DIFF_ZEROFILL_MASK=1 (session_hook; stream version 8, header ui_extra) leaves
# out the unit fields that the mirror's `fixes` branch zero-initialises where the original reads uninitialised memory
# (docs/ORIGINAL_BUGS.md Fix = zerofill). EEComplexUnit::mDisabledReason (+0x358): the original's ctor never sets it,
# so under the harness (debug heap) every new unit holds 0xbaadf00d there and the fixed build 0; it is read only while
# mIsDisabled is set, and SetDisabled 0x5fe536 writes it first. Without the mask such a pair differs from PT 0.
ZEROFILL_FIELDS = {'mDisabledReason'}


def enable_zerofill_mask():
    EXTRA_UI_FIELDS.update(ZEROFILL_FIELDS)
    return sorted(EXTRA_UI_FIELDS)

# RENDER-WRITTEN unit state (written by the graphics thread, so its value depends on render timing and on whether
# the scene is built at all). Excluded from the SIM hashes, hashed separately (render column). member -> byte mask.
#  mB44 bits 0x20 + phase 0x03 (show-hidden-units marks: EEUnit::Virtual40 0x628f90 / EEComplexUnit::Virtual40
#    0x5f8357 from EERenderObject::Fn10F412; the sim's EEUnit::Fn229AC3 0x629ac3 also calls Virtual40 for buildings) and
#    0x10 (set by the sim's Fn229AC3 when a unit moves, cleared by EEComplexUnit::Virtual40 on the render thread)
#  mCloaked +0x320: EEE3DFarm (render) stores the farm tile variant into it; the sim uses the same member as the
#    cloak timer (EEComplexUnit 0x5fa906/0x5fa96c/0x5fa98c read it: a SIM READ OF A RENDER-WRITTEN FIELD for farms)
#  mL2B4 +0x2b4: area-effect count (sim Fn1FFCB2 0x5ffcb2 sets it), decremented by EEE2DMarchingIcons::Virtual5C
#    0x4e3d8e on the render thread
# Found by scanning src/Empire Earth/Graphics Thread/*.cpp for stores into unit/player members (only direct `->m` stores;
# render-called helpers in other folders were followed by hand for Virtual40/Virtual54/Virtual58/FnE123D).
RENDER_FIELDS = {'mHiddenFlags': 0x33, 'mCloaked': 0xff, 'mNumAreaEffects': 0xff}

# pointer members hashed by the pointee's unit id (+0x24)
# CONDITIONAL fields (stream v6, dbg4 2026-11-01): (marker offset, marker u32, field offset, length, field) - the field
# is stale/uninitialised while the marker holds its unset value and its readers ignore it then; hashed as zeros.
# mU118 (GE3DPoint +0x118) only means something when mF214 (+0x214) != -10000.0f (0xc61c4000): a new unit's mU118
# varied between two runs of the ORIGINAL (PT 71813, player 3).
UNIT_ZERO_IF = [(0x214, 0xc61c4000, 0x118, 12, 'mInitialContactLocation')]


# OPT-IN (round 40, agent mp; owner-approved 2026-11-02): in a multiplayer game the host and the client allocate
# EEEffectManager graphic-effect handles locally, so EEComplexUnit::mConversionEffect (an EEEffectHandle the sim only
# tests against -1: EEUCBasicPriest/EEUCBuildingPriest, ReleaseConversionEffect) differs between the two ends of
# one game. With --mp-role only (session_hook: stream version 9, header unit_neg1) it is hashed as (== -1).
# EEComplexUnit::mFireEffect (+0x310, the fire functor's effect handle on a burning unit; same allocator) likewise
# (realcheck, owner-approved 2026-11-02; stream version 10): seed-99 natural-end MP games differ only there, in the
# same PT window [54571, 55689] with the same values for orig+orig as for the cross-engine pair.
MP_NEG1_FIELDS = ['mConversionEffect', 'mFireEffect']
UNIT_NEG1 = []   # (offset, name); empty unless enable_mp_mask()


# Same opt-in, player side: EEPlayer::mEarthquakeShaken (+0xb4c) is set only on gWorld.GetCurrentPlayer() (the LOCAL
# player) by EECCEarthquake::ShakeCamera 0x4b0956 (camera shake) and read only there; host and client differ by design.
MP_UI_FIELDS = {'mEarthquakeShaken'}


def enable_mp_mask():
    if not UNIT_NEG1:
        UNIT_NEG1.extend((r[0], r[2]) for r in unit_table() if r[2] in MP_NEG1_FIELDS)
    EXTRA_UI_FIELDS.update(MP_UI_FIELDS)
    return [n for _, n in UNIT_NEG1] + sorted(MP_UI_FIELDS)


def unit_zero_if(raw):
    """raw complex-unit bytes with the CONDITIONAL fields zeroed where their marker is unset (bytes, same length)."""
    import struct as _s
    b = bytearray(raw)
    for off, val, off2, n, _ in UNIT_ZERO_IF:
        if len(b) >= off + 4 and _s.unpack_from('<I', b, off)[0] == val:
            b[off2:off2 + n] = bytes(n)
    for off, _ in UNIT_NEG1:   # opt-in (mp): only whether the handle is -1
        if len(b) >= off + 4:
            b[off:off + 4] = b'\xff\xff\xff\xff' if b[off:off + 4] == b'\xff\xff\xff\xff' else bytes(4)
    return bytes(b)


UNIT_REFS = ['mPreviousPart', 'mNextPart', 'mHomeBase', 'mContainer', 'mLastAttacker', 'mOwner', 'mHolder', 'mLastResource', 'mGatheredResource', 'mMine', 'mBlockingUnit']

PHYSICS_FIELDS = [(0x18, 'I', 'mRandom.mSeed'), (0x80, 'I', 'mU80 (CT tick)'), (0x94, 'f', 'mF94 (game ms)'),
                  (0x98, 'f', 'mF98 (ms/PT)'), (0xb0, 'i', 'mLB0 (last draw)'), (0xb4, 'I', 'mUB4 (PTs done)'),
                  (0xb8, 'I', 'mUB8 (PTs left in CT)'), (0xd0, 'i', 'mLD0 (game ms long)')]

ORIGINAL = dict(physics=0x9318f0, world=0x930d40, inputs=0x931cf0, vt_complex=0x846ddc, vt_unit=0x847a20,
                vt_rtsunit=0x849b10, virtual20=0x553082, effect_random=0x8e8dc8,
                vt_computer_player=0x83a57c)   # hash2: RTTI .?AVEEComputerPlayer@@ checked
NAMES = dict(physics='?gPhysics@@3PAVEEPhysics@@A', world='?gWorld@@3VEEWorld@@A', inputs='?gU531CF0@@3JA',
             vt_complex='??_7EEComplexUnit@@6B@', vt_unit='??_7EEUnit@@6B@', vt_rtsunit='??_7RTSUnit@@6B@',
             virtual20='?Tick@EEPhysics@@UAEXXZ', vt_computer_player='??_7EEComputerPlayer@@6B@')
MAP_RANDOM = 0x19562c          # EEMap::mRandom (EEMap.h), map = gWorld+0xbc
WORLD_UNITS = 0x14             # RTSWorld::mUnits (RTSDummyUnit head); first unit at +0x18 (head's link)
WORLD_PLAYERS, WORLD_NPLAYERS, WORLD_MAP = 0x74, 0xcc, 0xbc


def parse_class(header, cls):
    """[(offset, None, name, type, count)] of cls's own members that carry a `// +0x` offset comment."""
    text = (INCLUDE / header).read_text(encoding='latin-1').splitlines()
    start = next(i for i, l in enumerate(text) if re.match(r'^(class|struct) %s\b[^;]*$' % re.escape(cls), l))
    depth, seen, out, pending = 0, False, [], None
    for line in text[start:]:
        code = line.split('//')[0]
        m = MEMBER.match(line)
        c = COMMENT_OFF.match(line)
        if c and depth == 1:
            pending = int(c.group(1), 16)
        if m and depth == 1 and '(' not in code and (m.group(5) or pending is not None):
            decl = code[:code.index(m.group(2))]
            typ = re.sub(r'\s+', ' ', decl.replace('class ', '').replace('const ', '').replace('*', ' ')).strip()
            if '*' in decl:
                typ += '*'
            count = m.group(4)
            n = 1 if not count else (int(count, 0) if count[0].isdigit() else None)
            out.append([int(m.group(5), 16) if m.group(5) else pending, None, m.group(2), typ, n])
            pending = None
        depth += code.count('{') - code.count('}')
        seen = seen or '{' in code
        if seen and depth <= 0:
            break
    return out


def field_table(classes, end):
    rows = []
    for header, cls in classes:
        rows += parse_class(header, cls)
    rows.sort()
    table = []
    for i, (off, _, name, typ, count) in enumerate(rows):
        nxt = rows[i + 1][0] if i + 1 < len(rows) else end
        table.append((off, nxt - off, name, typ, count))
    return table


def hashed(typ):
    return '*' not in typ and typ in SCALARS


def unit_table():
    t = field_table(UNIT_CLASSES, COMPLEX_SIZE)
    t = [r for r in t if not (0x130 <= r[0] < 0x17c)] + [(o, s, n, ty, 1) for o, s, n, ty in EXTRA_UNIT_FIELDS]
    return sorted(t)


def player_table():
    return field_table(PLAYER_CLASSES, PLAYER_SIZE)


def mask_for(table, size, render=False):
    """Bit mask (as int over `size` little-endian bytes) of the hashed members' bytes: the SIM mask (render-written
    bits removed) or, with render=True, only the RENDER_FIELDS bits."""
    m = bytearray(size)
    for off, sz, name, typ, count in table:
        if off >= size or not hashed(typ) or name in NOISE or name in UI_FIELDS or name in EXTRA_UI_FIELDS:
            continue
        if render and name not in RENDER_FIELDS:
            continue
        width = SCALARS[typ] * (count or 1)
        width = min(width, sz, size - off)
        bits = RENDER_FIELDS.get(name, 0)
        for k in range(off, off + width):
            b = bits if (k == off or bits == 0xff) else 0   # this byte's render-written bits
            m[k] = b if render else 0xff & ~b
    return int.from_bytes(bytes(m), 'little')


def label(table, off):
    """Member name for a byte offset (name+delta)."""
    best = None
    for row in table:
        if row[0] <= off:
            best = row
        else:
            break
    if not best:
        return '+0x%x' % off, 'unknown'
    d = off - best[0]
    return (best[2] + ('+0x%x' % d if d else '')), best[3]


def resolve(image, map_path=None, exe_path=None):
    """Addresses for 'original' or a full link (map). Adds 'pt_sync': the instruction after `mUB8--` in
    EEPhysics::Virtual20 (orig 0x553137 -> 0x55313d), i.e. the START of every PT, after its clock/RNG pre-step
    (mF94/mLD0 advanced, mLB0 = mRandom.Generate()) and before the world update. Reached on every PT, also after the
    engine end (mBE6 set: the world update and mUB4++ are skipped, mUB4 freezes; setup agent, 2026-11-01), so the
    stream key is not mUB4 but the PT count (see sampler/ptsync.h key rule). The earlier point after mUB4++ (orig
    0x553340) is 'pt_end'."""
    if map_path:
        import sys
        sys.path.insert(0, str(ROOT / 'tools/boot'))
        from fulllink_locator import LinkMap
        lm = LinkMap(map_path)
        a = {k: lm.address(v) for k, v in NAMES.items()}
        a['effect_random'] = None
        start, end = lm.extent(NAMES['virtual20'])
        exe = exe_path or pathlib.Path(map_path).with_suffix('.exe')
    else:
        a = dict(ORIGINAL)
        start, end = a['virtual20'], a['virtual20'] + 0x381
        exe = exe_path or ROOT / 'orig-bin/aoc/EE-AOC.unwrapped.exe'
    a['pt_sync'] = find_after_rmw(exe, start, end, 1, 0xb8)   # dec dword [reg+0xb8]
    a['pt_end'] = find_after_rmw(exe, start, end, 0, 0xb4)    # inc dword [reg+0xb4]
    return a


def find_after_rmw(exe, start, end, op, disp):
    """VA of the instruction after the single `inc`(op 0)/`dec`(op 1) dword [reg+disp32] (ff /op, mod 10) in
    [start, end)."""
    import pefile
    pe = pefile.PE(str(exe), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    data = pe.get_data(start - base, end - start)
    hits = [m.start() for m in re.finditer(rb'\xff(.)' + re.escape(struct.pack('<I', disp)), data, re.S)
            if (data[m.start() + 1] & 0xf8) == (0x80 | op << 3) and (data[m.start() + 1] & 7) != 4]
    if len(hits) != 1:
        raise RuntimeError('expected one %s [reg+0x%x] in Virtual20 %x..%x of %s, found %d'
                           % ('dec' if op else 'inc', disp, start, end, exe, len(hits)))
    return start + hits[0] + 6
