"""Round 39 differential harness (TEST ONLY, agent hash2): the computer-player (AI) object schema for the `ai` column.

The AI state of each computer player (EEComputerPlayer, include/ee/EEComputerPlayer.h, recognised by its vtable) is
walked from the player object through the managers it OWNS (OWNED below: combat manager -> attack commander -> attack
groups -> sub groups -> sub group units, defense commander -> aid packages, intelligence -> player files -> armies,
personnel -> requests, civilization manager -> resource/build/research/unit/wall managers, resource centers,
exploration and interaction managers). Members and offsets come from the reconstructed headers (include/ee/*.h,
`// +0x` comments, else sequential with the member type's size). Every member is classified by its declared type:

  scalar (long, float, bool, U2DPoint<long>, ...)  hashed by value (its bytes)
  EEComplexUnit* / EEUnit* / RTSUnit*               the unit's id (+0x24), -1 for null          op U
  EEPlayer* / EEComputerPlayer* / RTSPlayer*        the world player index, -1 null, -2 other   op P
  owned AI object pointer (OWNED)                   walked (recursively)                       op O
  referenced AI object with an id (KEYED)           its id (sub group mID, group serial, ...)   op K
  any other pointer                                 null / non-null only                        op N
  std::vector / std::deque / std::map / set         size, then every element (Dinkum 2.33 layouts) ops V D M
  inline struct (EECPAGTask, EECPILocation, ...)    flattened into the owner
  strings, engine containers                        not hashed (skipped)
Polymorphic owned objects (sub groups, sub group units, armies) also hash their RTTI class name (the vtable's
complete object locator), so two images with different vtable addresses compare.

Never raw pointers. compile_schema() is shared by the C hasher (ptsync.h `ai_*` config lines, via config_lines) and
the Python snapshot walker (Walker, used by sampler.py for the AI records of full snapshots and by compare.py/dump.py
for object paths: P<index>/mCombatManager/mAttackCommander/mGroups[#serial]/mSubGroups[#id]/mUnits[#unit id]/...).
"""
import hashlib, pathlib, re, struct

ROOT = pathlib.Path(__file__).resolve().parents[3]
INCLUDE = ROOT / 'include/ee'
VERSION = 2   # 2 (hash3): object identity checks (vptr RTTI per class, keyed pointees), see VPTRLESS / ptsync.h

# (class, header, sizeof, bases whose members are included, polymorphic: hash the RTTI class name)
CLASSES = {
    'EEComputerPlayer': ('EEComputerPlayer.h', 0xf58, [], False),   # own members from +0xb80 (EEPlayer part: player column)
    'EECPCombatManager': ('EECPCombatManager.h', 0x68, [], False),
    'EECPAttackCommander': ('EECPAttackCommander.h', 0x3c, [], False),
    'EECPAttackGroup': ('EECPAttackGroup.h', 0x6c, [], False),
    'EECPAttackSubGroup': ('EECPAttackSubGroup.h', 0x1e8, [], True),
    'EECPAttackSubGroupUnit': ('EECPAttackSubGroupUnit.h', 0x84, [], True),
    'EECPASTargetUnit': ('EECPASTargetUnit.h', 0x14, [], False),
    'EECPDefenseCommander': ('EECPDefenseCommander.h', 0x54, [], False),
    'EECPMilitaryAidPackage': ('EECPMilitaryAidPackage.h', 0x3c, [], False),
    'EECPIntelligence': ('EECPIntelligence.h', 0x14, [], False),
    'EECPIntelligencePlayerFile': ('EECPIntelligence.h', 0xb4, [], False),
    'EECPIArmy': ('EECPIArmy.h', 0x88, [('EEAIArea.h', 'EEAIArea')], True),
    'EECPPersonnel': ('EECPPersonnel.h', 0x38, [], False),
    'EECPPersonnelRequest': ('EECPPersonnel.h', 0x40, [], False),
    'EECPCivilizationManager': ('EECPCivilizationManager.h', 0x84, [], False),
    'EECPResourceManager': ('EECPResourceManager.h', 0x18c, [], False),
    'EECPBuildManager': ('EECPBuildManager.h', 0x170, [], False),
    'EECPResearchManager': ('EECPResearchManager.h', 0x12c, [], False),
    'EECPUnitManager': ('EECPUnitManager.h', 0x158, [], False),
    'EECPWallManager': ('EECPWallManager.h', 0x88, [], False),
    'EECPResourceCenter': ('EECPResourceCenter.h', 0x1c, [], False),
    'EECPExplorationManager': ('EECPExplorationManager.h', 0x30, [], False),
    'EECPPlayerInteractionManager': ('EECPPlayerInteractionManager.h', 0x2c, [], False),
}
# inline structs (flattened into their owner or used as container elements): (header, sizeof)
STRUCTS = {
    'EECPILocation': ('EECPILocation.h', 0x18), 'EECPAGTask': ('EECPAGTask.h', 0x34), 'EECPAGOrder': ('EECPAGOrder.h', 0x3c),
    'EECPASGStats': ('EECPASGStats.h', 0x48), 'EECPASGUnitCounts': ('EECPAttackSubGroup.h', 0x34),
    'EECPASGFlags': ('EECPAttackSubGroup.h', 0x18), 'EECPASGPathPoint': ('EECPAttackSubGroup.h', 0xc),
    'EECPAttackSubGroupUnitPrejudice': ('EECPAttackSubGroupUnit.h', 0x10), 'EECPIUnitTypeCount': ('EECPIntelligence.h', 0xc),
    'EECPIFamilyCount': ('EECPIntelligence.h', 8), 'EECPWMWall': ('EECPWallManager.h', 8), 'EECPUMRequest': ('EECPUnitManager.h', 0x10),
    'EECPRMHistoryEntry': ('EECPResearchManager.h', 8), 'EECPPIMEntry': ('EECPPlayerInteractionManager.h', 0x24),
    'RTSMapTileRun': ('EEMap.h', 0x10), 'Unit': ('EECPPersonnel.h', 0x10),   # EECPPersonnelRequest::Unit
}
# owning edges (class, member): followed. Every other AI-object pointer is a back/side reference (KEYED or N).
OWNED = {
    ('EEComputerPlayer', 'mResourceCenters'), ('EEComputerPlayer', 'mCombatManager'),
    ('EEComputerPlayer', 'mCivilizationManager'), ('EEComputerPlayer', 'mExplorationManager'),
    ('EEComputerPlayer', 'mInteractionManager'),
    ('EECPCombatManager', 'mAttackCommander'), ('EECPCombatManager', 'mDefenseCommander'),
    ('EECPCombatManager', 'mIntelligence'), ('EECPCombatManager', 'mPersonnel'),
    ('EECPAttackCommander', 'mGroups'), ('EECPAttackGroup', 'mSubGroups'),
    ('EECPAttackSubGroup', 'mUnits'), ('EECPAttackSubGroup', 'mTargets'), ('EECPAttackSubGroup', 'mSiegeTargets'),
    ('EECPDefenseCommander', 'mPackages'), ('EECPDefenseCommander', 'mLoadedPackages'),
    ('EECPIntelligence', 'mFiles'), ('EECPIntelligencePlayerFile', 'mArmies'), ('EECPPersonnel', 'mRequests'),
    ('EECPCivilizationManager', 'mResourceManager'), ('EECPCivilizationManager', 'mBuildManager'),
    ('EECPCivilizationManager', 'mResearchManager'), ('EECPCivilizationManager', 'mUnitManager'),
    ('EECPCivilizationManager', 'mWallManager'), ('EECPUnitManager', 'mUnits'),
}
# referenced AI objects hashed by their id: pointee -> (offset, how): 0 the u32 there, 1 a unit pointer there (unit id)
KEYED = {'EECPAttackSubGroup': (0x1a4, 0), 'EECPAttackGroup': (0x60, 0), 'EECPPersonnelRequest': (0x3c, 0),
         'EECPAttackSubGroupUnit': (0x44, 1)}
# path keys of owned objects in containers: member (scalar or U op) -> '[#value]' instead of '[index]'
PATH_KEYS = {'EECPAttackGroup': 'mID', 'EECPAttackSubGroup': 'mID', 'EECPAttackSubGroupUnit': 'mUnit',
             'EECPPersonnelRequest': 'mID', 'EECPResourceCenter': 'mUnit', 'EECPIntelligencePlayerFile': 'mPlayer',
             'EECPIArmy': 'mSerial', 'EECPASTargetUnit': 'mUnit'}
# classes without a vptr at +0 (every other CLASSES entry has one: its RTTI name is checked before the object is walked,
# so a stale pointer into freed or reused memory is a sentinel, not hashed bytes; hash3 2026-11-01)
VPTRLESS = {'EECPResourceCenter', 'EECPUnitManager'}   # UnitManager: non-virtual dtor, mUnits at +0
SKIP = {('EECPAttackSubGroupUnit', 'mU08')}   # std::set<UString>: strings are not hashed (count only would need a type)
UNIT_T = {'EEComplexUnit', 'EEUnit', 'RTSUnit'}
PLAYER_T = {'EEPlayer', 'EEComputerPlayer', 'RTSPlayer'}
SCALARS = {'long': 4, 'unsigned long': 4, 'int': 4, 'unsigned int': 4, 'float': 4, 'bool': 1, 'char': 1,
           'unsigned char': 1, 'short': 2, 'unsigned short': 2, 'double': 8, 'DWORD': 4, 'BYTE': 1,
           'U2DPoint<long>': 8, 'U2DPoint<float>': 8, 'U2DRectangle<long>': 16, 'GE3DPoint': 12}
TYPEDEFS = {'EETileLocation': 'U2DPoint<long>', 'EETileCoordinate': 'U2DPoint<long>', 'EETileArea': 'U2DRectangle<long>',
            'EEAIAStarBounds': 'U2DRectangle<long>', 'EECPBuildRequest': 'std::pair<unsigned long,U2DPoint<long>>',
            'EECPBuildQueue': 'std::deque<EECPBuildRequest>', 'const long': 'long', 'const unsigned long': 'unsigned long'}
CONTAINER_SIZE = {'std::vector': 0x10, 'std::deque': 0x14, 'std::map': 0xc, 'std::multimap': 0xc, 'std::set': 0xc,
                  'std::multiset': 0xc, 'std::list': 0xc}
OPAQUE = {'UString': 0x10, 'UWideString': 0x10, 'U2DSparseArrayPointContainer': 0x1c, 'U2DConvexHull<long>': 0x30}

MEMBER = re.compile(r'^\s*(.+?)\s*\b(m[A-Za-z0-9_]+|_gap\w*)\s*(?:\[\s*(\w+)\s*\])?\s*;\s*$')
OFFC = re.compile(r'\+0x([0-9a-fA-F]+)\b')


def norm(t):
    t = re.sub(r'\b(class|struct|const)\s+', '', t)
    t = re.sub(r'\s*([<>,*])\s*', r'\1', t).strip()
    t = re.sub(r'\bstd::(\w+)<', r'std::\1<', t)
    return TYPEDEFS.get(t, t)


def split_args(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == '<': depth += 1
        elif ch == '>': depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur); cur = ''
        else:
            cur += ch
    out.append(cur)
    return [norm(x) for x in out]


def parse_members(header, cls, start_off=0):
    """[(offset, name, type, count)] of cls's own data members (nested `struct Unit` too)."""
    text = (INCLUDE / header).read_text(encoding='latin-1').splitlines()
    start = next(i for i, l in enumerate(text) if re.match(r'^\s*(class|struct) %s\b[^;]*$' % re.escape(cls), l))
    depth, seen, out, pending, off = 0, False, [], None, start_off
    nxt = start_off
    for line in text[start:]:
        code, _, comment = line.partition('//')
        if depth == 1 and not code.strip() and comment:
            m = re.match(r'\s*\+0x([0-9a-fA-F]+)\b', comment)
            if m:
                pending = int(m.group(1), 16)
        m = MEMBER.match(code) if depth == 1 else None
        if m and '(' not in code and not re.match(r'\s*(static|typedef|friend|enum|return|using|virtual)\b', code):
            typ = norm(m.group(1))
            mo = OFFC.match(comment.strip()) if comment else None
            o = int(mo.group(1), 16) if mo else pending if pending is not None else nxt
            if o is None:
                raise ValueError('%s::%s: no offset' % (cls, m.group(2)))
            cnt = m.group(3)
            n = 1 if not cnt else int(cnt, 0) if cnt[0].isdigit() else None
            if cnt and n is None:
                n = 0   # e.g. _gapF56[0xf58 - 0xf56]: sized by the next member/the class size
            sz = type_size(typ)
            if sz is not None and sz:
                al = min(4, align_of(typ))
                o = (o + al - 1) // al * al if not mo and pending is None else o
            out.append((o, m.group(2), typ, n))
            nxt = o + sz * (n or 1) if sz is not None and n else None
            pending = None
        depth += code.count('{') - code.count('}')
        seen = seen or '{' in code
        if seen and depth <= 0:
            break
    return out


def align_of(t):
    if t in SCALARS:
        return {1: 1, 2: 2}.get(SCALARS[t], 4)
    return 4


def type_size(t):
    t = norm(t)
    if t.endswith('*'):
        return 4
    if t in SCALARS:
        return SCALARS[t]
    if t in OPAQUE:
        return OPAQUE[t]
    base = t.split('<')[0]
    if base in CONTAINER_SIZE:
        return CONTAINER_SIZE[base]
    if base == 'std::pair':
        k, v = split_args(t[t.index('<') + 1:-1])
        ks, vs = type_size(k), type_size(v)
        if ks is None or vs is None:
            return None
        vo = (ks + align_of(v) - 1) // align_of(v) * align_of(v)
        return (vo + vs + 3) // 4 * 4 if max(align_of(k), align_of(v)) >= 4 else vo + vs
    if t in STRUCTS:
        return STRUCTS[t][1]
    if t in CLASSES:
        return CLASSES[t][1]
    return None


class Schema:
    """Compiled types: name -> dict(size, poly, fields=[(off, size, name, typ)], ops=[(kind, off, arg, arg2, name)],
    key=path key member). Ops: U unit ptr, P player ptr, N non-null, K keyed ref (arg off, arg2 how), O owned ptr (arg
    type), V vector, D deque, M tree (arg element type)."""
    def __init__(self):
        self.types = {}
        self.order = []

    def add(self, name, size, poly=False):
        t = self.types[name] = dict(name=name, size=size, poly=poly, fields=[], ops=[], key=PATH_KEYS.get(name),
                                    vptr=None, kref={})
        if name in CLASSES and name not in VPTRLESS and name != 'EEComputerPlayer':   # root: checked by its vtable
            t['vptr'] = '' if poly else '.?AV%s@@' % name   # '': polymorphic, any '.?AV..@@' class
        self.order.append(name)
        return t

    def element(self, typ, owner, member):
        """Type name of a container element / pointee of declared type typ (compiled on demand)."""
        typ = norm(typ)
        name = 'elem:%s|%s' % (typ, 'own' if (owner, member) in OWNED else '')
        if name in self.types:
            return name
        if typ in CLASSES and not typ.endswith('*'):
            self.compile_class(typ)
            return typ
        if typ in STRUCTS:
            self.compile_struct(typ)
            return typ
        size = type_size(typ)
        if size is None:
            raise ValueError('element type %s (%s::%s) has no size' % (typ, owner, member))
        t = self.add(name, size)
        self.member(t, 0, 'v', typ, 1, owner, member)
        return name

    def member(self, t, off, name, typ, count, owner, mname):
        typ = norm(typ)
        size = type_size(typ)
        if (owner, mname) in SKIP:
            return
        for i in range(count or 1):
            o = off + i * (size or 0)
            nm = name if (count or 1) == 1 else '%s[%d]' % (name, i)
            if typ.endswith('*'):
                p = typ[:-1]
                if p in UNIT_T:
                    t['ops'].append(('U', o, 0, 0, nm))
                elif p in PLAYER_T:
                    t['ops'].append(('P', o, 0, 0, nm))
                elif (owner, mname) in OWNED and (p in CLASSES or p.split('<')[0] in CONTAINER_SIZE):
                    t['ops'].append(('O', o, self.element(p, owner, mname), 0, nm))
                elif p in KEYED:
                    t['ops'].append(('K', o, KEYED[p][0], KEYED[p][1], nm))
                    if p in CLASSES:   # the pointee's identity is checked before its id is read
                        self.compile_class(p)
                        t['kref'][o] = p
                else:
                    t['ops'].append(('N', o, 0, 0, nm))
            elif typ in SCALARS:
                t['fields'].append((o, SCALARS[typ], nm, typ))
            elif typ in STRUCTS:
                self.compile_struct(typ)
                self.flatten(t, o, nm, self.types[typ])
            elif typ.split('<')[0] == 'std::pair':
                k, v = split_args(typ[typ.index('<') + 1:-1])
                self.member(t, o, nm + '.first', k, 1, owner, mname)
                vo = (type_size(k) + align_of(v) - 1) // align_of(v) * align_of(v)
                self.member(t, o + vo, nm + '.second', v, 1, owner, mname)
            elif typ.split('<')[0] in CONTAINER_SIZE:
                base = typ.split('<')[0]
                args = split_args(typ[typ.index('<') + 1:-1])
                if base in ('std::map', 'std::multimap'):
                    el = self.element('std::pair<%s,%s>' % (args[0], args[1]), owner, mname)
                else:
                    el = self.element(args[0], owner, mname)
                kind = {'std::vector': 'V', 'std::deque': 'D', 'std::list': 'L'}.get(base, 'M')
                t['ops'].append((kind, o, el, 0, nm))
            elif typ in OPAQUE or size is None:
                pass   # strings / engine containers / unknown: not hashed
            else:
                raise ValueError('unclassified %s %s::%s' % (typ, owner, mname))

    def flatten(self, t, off, prefix, s):
        for o, sz, n, ty in s['fields']:
            t['fields'].append((off + o, sz, prefix + '.' + n, ty))
        for k, o, a, a2, n in s['ops']:
            t['ops'].append((k, off + o, a, a2, prefix + '.' + n))
        for o, ty in s['kref'].items():
            t['kref'][off + o] = ty

    def compile_struct(self, name):
        if name in self.types:
            return
        header, size = STRUCTS[name]
        t = self.add(name, size)
        owner = name
        for off, mname, typ, n in parse_members(header, name, 0):
            if mname.startswith('_gap'):
                continue
            self.member(t, off, mname, typ, n, owner, mname)
        check(t)

    def compile_class(self, name):
        if name in self.types:
            return
        header, size, bases, poly = CLASSES[name]
        t = self.add(name, size, poly)
        rows = []
        for bh, bc in bases:
            rows += [(r, bc) for r in parse_members(bh, bc, 4)]
        start = 0xb80 if name == 'EEComputerPlayer' else 4
        rows += [(r, name) for r in parse_members(header, name, start)]
        for (off, mname, typ, n), owner in rows:
            if mname.startswith('_gap'):
                continue
            self.member(t, off, mname, typ, n, name, mname)
        check(t)


def check(t):
    for o, sz, n, ty in t['fields']:
        if o + sz > t['size']:
            raise ValueError('%s.%s at +0x%x+%d beyond sizeof 0x%x' % (t['name'], n, o, sz, t['size']))
    for k, o, a, a2, n in t['ops']:
        if o + 4 > t['size']:
            raise ValueError('%s.%s op at +0x%x beyond sizeof 0x%x' % (t['name'], n, o, t['size']))


_SCHEMA = None


def compile_schema():
    global _SCHEMA
    if _SCHEMA is None:
        s = Schema()
        s.compile_class('EEComputerPlayer')
        for t in s.types.values():   # mask: hashed scalar bytes
            m = bytearray(t['size'])
            for o, sz, n, ty in t['fields']:
                m[o:o + sz] = b'\xff' * sz
            t['mask'] = bytes(m)
        _SCHEMA = s
    return _SCHEMA


def digest():
    s = compile_schema()
    return hashlib.sha1(repr([VERSION] + [(n, s.types[n]['size'], s.types[n]['fields'], s.types[n]['ops'], s.types[n]['vptr'],
                                          sorted(s.types[n]['kref'].items())) for n in s.order]).encode()).hexdigest()[:12]


def config_lines(vt_computer_player, every=1):
    """ptsync.h config: ai_vt, ai_every, ai_type <id> <size> <poly> <mask hex>, ai_op <type id> <kind> <off> <arg> <arg2>."""
    s = compile_schema()
    ids = {n: i for i, n in enumerate(s.order)}
    rows = ['ai_vt %x' % vt_computer_player, 'ai_every %d' % every, 'ai_root %d' % ids['EEComputerPlayer']]
    for n in s.order:
        t = s.types[n]
        rows.append('ai_type %d %x %d %s' % (ids[n], t['size'], 1 if t['poly'] else 0, t['mask'].hex()))
        if t['vptr'] is not None:
            rows.append('ai_vptr %d %s' % (ids[n], t['vptr'] or '*'))
        for k, o, a, a2, nm in t['ops']:
            arg = ids[a] if k == 'O' or k in 'VDML' else a
            rows.append('ai_op %d %s %x %x %x' % (ids[n], k, o, arg, a2))
        for o, p in sorted(t['kref'].items()):
            rows.append('ai_kref %d %x %d' % (ids[n], o, ids[p]))
    return rows


# ---- Python walker (snapshots) ----
DEQ = lambda size: 1 if size > 4096 else 4096 // size


class Walker:
    """Reads one computer player's AI tree from a sampler.Proc into records
    {'path', 'type', 'cls'?, 'v': {field: value}}: scalars as hex of their bytes, refs as ids, containers as their
    size ('#name') plus inline values for scalar/ref elements, child records for struct/object elements."""
    LIMIT = 200000

    def __init__(self, proc, players):
        self.p, self.s = proc, compile_schema()
        self.index = {ptr: i for i, ptr in enumerate(players) if ptr}
        self.rtti = {}
        self.count = 0

    def u32(self, va):
        return struct.unpack('<I', self.p.read(va, 4))[0]

    def valid(self, tname, addr):
        """Identity check (ptsync.h pai_valid): the object's vptr has the expected RTTI name ('' expected: any class)."""
        want = self.s.types[tname]['vptr']
        if want is None:
            return True
        try:
            name = self.cls_name(addr)
        except OSError:
            return False
        return name == want if want else (name.startswith('.?AV') and name.endswith('@@'))

    def cls_name(self, obj):
        vt = self.u32(obj)
        lo, hi = getattr(self.p, 'image_range', (0, 1 << 32))
        if not lo <= vt < hi:
            return '?'
        if vt not in self.rtti:
            try:
                td = self.u32(self.u32(vt - 4) + 0xc)
                raw = self.p.read(td + 8, 64)
                self.rtti[vt] = raw.split(b'\0')[0].decode('latin-1')
            except OSError:
                self.rtti[vt] = '?'
        return self.rtti[vt]

    def ref(self, kind, b, o, a, a2, owner=None):
        ptr = struct.unpack_from('<I', b, o)[0]
        if kind == 'N':
            return int(ptr != 0)
        if not ptr:
            return -1
        try:
            if kind == 'U':
                return self.u32(ptr + 0x24)
            if kind == 'P':
                return self.index.get(ptr, -2)
            if kind == 'K':
                p = self.s.types[owner]['kref'].get(o) if owner else None
                if p and not self.valid(p, ptr):
                    return -4   # stale / reused pointee
                v = self.u32(ptr + a)
                return (self.u32(v + 0x24) if v else -1) if a2 == 1 else v
        except OSError:
            return -3
        return None

    def elements(self, kind, b, o, el):
        size = self.s.types[el]['size']
        if kind == 'V':
            first, last = struct.unpack_from('<II', b, o + 4)
            if not first or last < first or (last - first) % size or (last - first) // size > 65536:
                return [] if not first and not last else None
            return [first + i * size for i in range((last - first) // size)]
        if kind == 'D':
            mp, mapsize, off, n = struct.unpack_from('<IIII', b, o + 4)
            if n > 65536 or mapsize > 1 << 20 or (n and not mp):
                return None
            ds = DEQ(size); out = []
            for i in range(n):
                k = off + i; blk = k // ds
                if blk >= mapsize: blk -= mapsize
                out.append(self.u32(mp + 4 * blk) + (k - (k // ds) * ds) * size)
            return out
        if kind == 'M':
            head, n = struct.unpack_from('<II', b, o + 4)
            if not head or n > 65536:
                return None
            isnil = 0xc + size + 1
            out = []

            def walk(node, depth):
                if depth > 64 or len(out) > n:
                    return
                nb = self.p.read(node, isnil + 1)
                if nb[isnil]:
                    return
                walk(struct.unpack_from('<I', nb, 0)[0], depth + 1)
                out.append(node + 0xc)
                walk(struct.unpack_from('<I', nb, 8)[0], depth + 1)
            walk(self.u32(head + 4), 0)
            return out
        return None

    def leaf(self, el):
        t = self.s.types[el]
        return el.startswith('elem:') and not any(k in 'OVDML' for k, *_ in t['ops'])

    def values(self, el, addr):
        """Inline value of a leaf element: hex scalars and ref ids, joined."""
        t = self.s.types[el]
        b = self.p.read(addr, t['size'])
        parts = [b[o:o + sz].hex() for o, sz, n, ty in t['fields']]
        parts += [str(self.ref(k, b, o, a, a2, el)) for k, o, a, a2, n in t['ops']]
        return ':'.join(parts)

    def walk(self, addr, tname, path, out, depth=0):
        self.count += 1
        if depth > 16 or self.count > self.LIMIT:
            return
        t = self.s.types[tname]
        try:
            b = self.p.read(addr, t['size'])
        except OSError:
            out.append(dict(path=path, type=tname, v={'read': 'failed'}))
            return
        if not self.valid(tname, addr):
            out.append(dict(path=path, type=tname, v={'invalid': 'identity'}))
            return
        rec = dict(path=path, type=tname, v={})
        if t['poly']:
            rec['cls'] = self.cls_name(addr)
        for o, sz, n, ty in t['fields']:
            rec['v'][n] = b[o:o + sz].hex()
        out.append(rec)
        for k, o, a, a2, n in t['ops']:
            if k in 'UPNK':
                rec['v'][n] = self.ref(k, b, o, a, a2, tname)
            elif k == 'O':
                ptr = struct.unpack_from('<I', b, o)[0]
                rec['v'][n] = int(ptr != 0)
                if ptr:
                    self.walk(ptr, a, path + '/' + n, out, depth + 1)
            elif k in 'VDM':
                try:
                    els = self.elements(k, b, o, a)
                except OSError:
                    els = None
                if els is None:
                    rec['v']['#' + n] = 'bad'
                    continue
                rec['v']['#' + n] = len(els)
                if self.leaf(a):
                    et = self.s.types[a]
                    own = [op for op in et['ops'] if op[0] == 'O']
                    rec['v'][n] = [self.values(a, e) for e in els]
                    continue
                for i, e in enumerate(els):
                    sub, key = a, None
                    et = self.s.types[a]
                    if a.startswith('elem:'):   # pointer or pair element: the owned object behind it
                        eb = self.p.read(e, et['size'])
                        own = [op for op in et['ops'] if op[0] == 'O']
                        kf = [eb[o2:o2 + sz].hex() for o2, sz, n2, ty in et['fields']]
                        if own:
                            ptr = struct.unpack_from('<I', eb, own[0][1])[0]
                            if not ptr:
                                continue
                            key = self.path_key(ptr, own[0][2]) or (kf[0] if kf else None)
                            self.walk(ptr, own[0][2], '%s/%s[%s]' % (path, n, '#%s' % key if key is not None else i), out, depth + 1)
                            continue
                    key = self.path_key(e, sub)
                    self.walk(e, sub, '%s/%s[%s]' % (path, n, '#%s' % key if key is not None else i), out, depth + 1)

    def path_key(self, addr, tname):
        t = self.s.types.get(tname)
        if not t or not t.get('key'):
            return None
        try:
            b = self.p.read(addr, t['size'])
        except OSError:
            return None
        for o, sz, n, ty in t['fields']:
            if n == t['key']:
                return int.from_bytes(b[o:o + sz], 'little')
        for k, o, a, a2, n in t['ops']:
            if n == t['key']:
                return self.ref(k, b, o, a, a2)
        return None


if __name__ == '__main__':   # print the schema
    s = compile_schema()
    for n in s.order:
        t = s.types[n]
        print('%-60s size %3x poly %d fields %3d ops %s' % (n, t['size'], t['poly'], len(t['fields']),
              ' '.join('%s%x:%s' % (k, o, nm) for k, o, a, a2, nm in t['ops'])))
    print('digest', digest())
