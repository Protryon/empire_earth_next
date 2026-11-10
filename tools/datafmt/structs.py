"""Parse the on-disk record structs out of the empire_earth_next sources and compute their layouts."""
import re, os, json, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = {
    'EEUnitClassRecord': r'include\ee\EEUnitClassRecord.h',
    'EEGraphicObjectRecord': r'src\Empire Earth\Data Pre-Loading Thread\EEGraphicObject.cpp',
    'EESoundRecord': r'src\Empire Earth\Data Pre-Loading Thread\EESoundObject.cpp',
    'EEGraphicEffectRecord': r'src\Empire Earth\Data Pre-Loading Thread\EEGraphicEffect.cpp',
    'EEMusicRecord': r'src\Empire Earth\Data Pre-Loading Thread\EEMusicObject.cpp',
    'EEWorldTableEntry': r'include\ee\EEDatabaseTables.h',
    'EEDamageTableRecord': r'src\Empire Earth\Database\EETerrainTypeTable.cpp',
    'EETerrainTypeRecord': r'src\Empire Earth\Database\EETerrainTypeTable.cpp',
    'EEButtonRecord': r'src\Empire Earth\Database\EEButtonObject.cpp',
    'EETechTreeRecord': r'include\ee\EETechTreeRecord.h',
    'EEEventEffect': r'include\ee\EEEvent.h',
    'EEAnimalRecord': r'src\Empire Earth\Database\EEAnimal.cpp',
    'EEColorTableRecord': r'src\Empire Earth\Database\EEColorTable.cpp',
    'EEAIUnitTargetingRecord': r'src\Empire Earth\Database\EEAIUnitTargetingTable.cpp',
    'EETerrainRecord': r'src\Empire Earth\Database\EETerrainTable.cpp',
    'EECliffTerrainRecord': r'src\Empire Earth\Database\EETerrainTable.cpp',
    'EECivPower': r'include\ee\EECivPowersManager.h',
    'EECivilizationCategoryEntry': r'include\ee\EECivilizationManager.h',
    'EECivilizationEntry': r'include\ee\EECivilizationManager.h',
    'EEToHitRecord': r'src\Empire Earth\Database\EEUnitBehavior.cpp',
    'EEUnitBehaviorRecord': r'src\Empire Earth\Database\EEUnitBehavior.cpp',
    'EECalamityData': r'include\ee\EECalamityClass.h',
    'EEUnitSetRecord': r'src\Empire Earth\Empire Earth\EEUnitSetManager.cpp',
    'EEAreaEffectData': r'include\ee\EEAreaEffectTemplate.h',
    'EEUCUpgradeRecord': r'src\Empire Earth\Database\EEUCUpgradeTable.cpp',
    'EECPBehavior': r'include\ee\EECPBehavior.h',
    'EEGameVariantData': r'include\ee\EEGameVariant.h',
    'EEStartingResourcesRecord': r'src\Empire Earth\Database\EEStartingResources.cpp',
    'EEAmbientSoundRecord': r'include\ee\EEAmbientSound.h',
    'UIDbControl': r'src\Low-Level Engine\User Interface\UIControlsTable.cpp',
    'UIDbDataBackgrounds': r'include\lle\UIBackgroundTable.h',
    'UIDbDataForms': r'include\lle\UIForm.h',
    'UIDbDataEvents': r'src\Low-Level Engine\User Interface\UIEvent.cpp',
    'UIDbDataFonts': r'src\Low-Level Engine\User Interface\UIFontTable.cpp',
    'UIDbDataHotKeys': r'src\Low-Level Engine\User Interface\UIHotKey.cpp',
}
TYPES = {'char': ('s8', 1), 'unsigned char': ('u8', 1), 'bool': ('bool', 1), 'short': ('s16', 2),
         'unsigned short': ('u16', 2), 'long': ('s32', 4), 'unsigned long': ('u32', 4), 'int': ('s32', 4),
         'unsigned int': ('u32', 4), 'float': ('f32', 4), 'EECalamityKind': ('s32', 4), 'int32': ('s32', 4),
         'EECivilizationUpgradeValue': ('pair_f32', 8), 'WORD': ('u16', 2), 'DWORD': ('u32', 4), 'BYTE': ('u8', 1)}
FIELD = re.compile(r'^\s*(?:volatile\s+)?((?:unsigned\s+)?\w+)\s+(\w+)\s*(\[[^\]]+\])?\s*;\s*(?://\s*(.*))?$')


def evalsize(expr):
    return int(eval(expr.replace('/', '//')))


def body_of(text, name):
    m = re.search(r'struct\s+' + name + r'\s*\{', text)
    if not m:
        return None
    i = m.end(); depth = 1; j = i
    while depth:
        c = text[j]
        if c == '{': depth += 1
        elif c == '}': depth -= 1
        j += 1
    return text[i:j - 1]


def parse(name):
    text = open(os.path.join(ROOT, SRC[name]), encoding='latin-1').read()
    body = body_of(text, name)
    fields = []
    off = 0
    maxal = 1
    lines = body.split('\n')
    depth = 0
    for ln in lines:
        s = ln.strip()
        # skip inline function bodies
        if depth:
            depth += s.count('{') - s.count('}')
            continue
        if '(' in s.split('//')[0]:
            if s.count('{') > s.count('}'):
                depth = s.count('{') - s.count('}')
            continue
        m = FIELD.match(ln)
        if not m:
            continue
        t, n, arr, cmt = m.groups()
        if t in ('static', 'return', 'class', 'struct', 'virtual'):
            continue
        if t not in TYPES:
            print('UNKNOWN TYPE', name, t, n, file=sys.stderr)
            continue
        code, sz = TYPES[t]
        cnt = evalsize(arr[1:-1]) if arr else 1
        al = 4 if code == 'pair_f32' else sz
        maxal = max(maxal, al)
        if off % al:
            pad = al - off % al
            fields.append(dict(off=off, type='pad', name='(pad)', count=pad, size=pad, comment='alignment padding'))
            off += pad
        fields.append(dict(off=off, type=code, name=n, count=cnt, size=sz * cnt, comment=(cmt or '').strip()))
        off += sz * cnt
    if off % maxal:
        pad = maxal - off % maxal
        fields.append(dict(off=off, type='pad', name='(pad)', count=pad, size=pad, comment='tail padding'))
        off += pad
    return dict(name=name, size=off, fields=fields)


GAPNAME = re.compile(r'^(_gap\w*|_u\w*|m[LUCBF][0-9A-F]{2,3}|mU[0-9a-f]{3}|mUnknown\w*|mUndecoded\w*|mBoolPlaceholder\d|mPad|mIntParam\d|mFloatParam\d|mParam\d+|mL\w{2,3}|mB\w{2,3}|mC\w{2,3}|mF\w{2,3})$')


def is_undecoded(f):
    return f['type'] == 'pad' or bool(GAPNAME.match(f['name']))


if __name__ == '__main__':
    out = {}
    for n in SRC:
        r = parse(n)
        dec = sum(f['size'] for f in r['fields'] if not is_undecoded(f))
        r['decoded_bytes'] = dec
        out[n] = r
        print(f"{n:30s} size={r['size']:5d} decoded={dec:5d} ({100*dec/r['size']:.0f}%)")
    json.dump(out, open(os.path.join(os.path.dirname(__file__), 'structs.json'), 'w'), indent=1)
