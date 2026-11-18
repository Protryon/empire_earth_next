"""DLS (DirectMusic Downloadable Sounds, level 1/2) -> SoundFont 2.01, once, for the open data tree.

The game's music played its DLS banks through dmusic, which converted each bank in memory into TinySoundFont's
"hydra" (src/ThirdParty/dmusic/src/util/Tsf.c, Dm_createHydra; the parse in src/ThirdParty/dmusic/src/io/Dls.c).
This writes the same hydra as a standard SoundFont 2.01 file, so the Rust synthesizer (rustysynth) plays what dmusic
played. Tsf.c's mapping is kept as it is, quirks included where they change the sound:

- a preset per DLS instrument: bank 128 for a drum kit (bank bit 31), else the raw DLS bank (MSB << 8 | LSB, e.g.
  0x602 = 1538), the DLS patch; one preset zone with the instrument generator;
- an instrument zone per region: keyRange, velRange (0-127 when its high is not above its low), exclusiveClass (a
  key group), overridingRootKey (the region's unity note), fineTune (Tsf.c's `fine_tune >> 16` of a uint16: always
  0), initialAttenuation (-(gain >> 16) cB, x 2.5 for rustysynth's 0.4 factor), the loop offsets and sampleModes (1, 3 for loop-with-release), the
  articulators' generators (the region's, else the instrument's), sampleID (the region's wave-link table index
  used directly as the wave's index, as Tsf.c does) last;
- the region's own wsmp only (a region without one plays unity note 0, no gain, no loop, as in dmusic);
- the articulators' modulators as Tsf.c builds them (the Rust synthesizer, like TinySoundFont, ignores them);
- the samples: 16-bit PCM as stored, MS-ADPCM decoded as Dls.c decodes it, each followed by 46 zero samples; the
  sample headers' loop points at the sample's start (the loops are the generators' offsets).

  python -I tools/dls2sf2.py IN.dls OUT.sf2       convert one bank (and check the result)
  python -I tools/dls2sf2.py --check FILE.sf2      re-parse a SoundFont and check every index
  python -I tools/dls2sf2.py --report IN.dls ...   the banks' gains, fine tunes, regions without wsmp, pool tables
"""
import struct
import sys

TOOL = 'dls2sf2 (empire_earth_next)'
DRUM_KIT = 0x80000000
PADDING = 46
ATTENUATION_SCALE = 2.5   # see region_generators

# SF2 generators (Tsf.c's names)
kStartLoopOffset, kEndLoopOffset = 2, 3
kModLfoToPitch, kVibLfoToPitch, kModEnvToPitch = 5, 6, 7
kInitialFilterFc, kInitialFilterQ = 8, 9
kModLfoToVolume = 13
kChorusSend = 16    # (Tsf.c: both 16, reverbEffectsSend)
kReverbSend = 16
kPan = 17
kDelayModLFO, kFreqModLFO, kDelayVibLFO, kFreqVibLFO = 21, 22, 23, 24
kDelayModEnv, kAttackModEnv, kHoldModEnv, kDecayModEnv, kSustainModEnv, kReleaseModEnv = 25, 26, 27, 28, 29, 30
kDelayVolEnv, kAttackVolEnv, kHoldVolEnv, kDecayVolEnv, kSustainVolEnv, kReleaseVolEnv = 33, 34, 35, 36, 37, 38
kInstrument, kKeyRange, kVelRange = 41, 43, 44
kStartLoopOffsetCoarse, kInitialAttenuation, kEndLoopOffsetCoarse = 45, 48, 50
kFineTune, kSampleID, kSampleModes, kScaleTuning, kExclusiveClass, kOverridingRootKey = 52, 53, 54, 56, 57, 58
kNoteOnVelocity, kNoteOnKey = 2, 3

# DLS connection sources and destinations (_Dls.h)
SRC_NONE, SRC_LFO, SRC_VELOCITY, SRC_KEY, SRC_EG2, SRC_VIBRATO = 0, 1, 2, 3, 5, 9
DST_PITCH = 3
DST_GENERATORS = {   # destination -> (generator, how the amount is made)
    0x004: (kPan, 's'), 0x080: (kChorusSend, 's'), 0x001: (kReverbSend, 's'),
    0x206: (kAttackVolEnv, 's'), 0x30a: (kAttackModEnv, 's'), 0x207: (kDecayVolEnv, 's'),
    0x30b: (kDecayModEnv, 's'), 0x209: (kReleaseVolEnv, 's'), 0x30d: (kReleaseModEnv, 's'),
    0x20b: (kDelayVolEnv, 's'), 0x30f: (kDelayModEnv, 's'), 0x20c: (kHoldVolEnv, 's'), 0x310: (kHoldModEnv, 's'),
    0x20a: (kSustainVolEnv, 'sustain'), 0x30e: (kSustainModEnv, 'sustain'),
    0x500: (kInitialFilterFc, 's'), 0x501: (kInitialFilterQ, 's'),
    0x104: (kFreqModLFO, 's'), 0x105: (kDelayModLFO, 's'), 0x114: (kFreqVibLFO, 's'), 0x115: (kDelayVibLFO, 's'),
}
EG1_DECAY, EG2_DECAY, EG1_ATTACK, EG2_ATTACK, EG1_HOLD, EG2_HOLD = 0x207, 0x30b, 0x206, 0x30a, 0x20c, 0x310

ADPCM_COEFF1 = [256, 512, 0, 192, 240, 460, 392]
ADPCM_COEFF2 = [0, -256, 0, 64, 0, -208, -232]
ADPCM_ADAPT = [230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230]


def s16(v):
    """C's conversion to a 16-bit field (wraps)."""
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def cdiv(a, b):
    """C's integer division (toward zero)."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


# ---- the RIFF reader (dmusic's DmRiff)
def chunks(b, start, end):
    """(id, type or None, data start, data end) of each chunk in [start, end)."""
    pos = start
    while end - pos >= 8:
        cid = b[pos:pos + 4]
        size = struct.unpack_from('<I', b, pos + 4)[0]
        if cid in (b'RIFF', b'LIST'):
            typ = b[pos + 8:pos + 12]
            yield cid, typ, pos + 12, min(pos + 8 + size, end)
        else:
            yield cid, None, pos + 8, min(pos + 8 + size, end)
        pos += 8 + size + (size & 1)


def zstr(b, s, e):
    return b[s:e].split(b'\0', 1)[0].decode('latin-1')


def parse_info(b, s, e):
    for cid, typ, cs, ce in chunks(b, s, e):
        if cid == b'INAM':
            return zstr(b, cs, ce)
    return ''


def parse_wsmp(b, s):
    unity, fine, gain, flags, loops = struct.unpack_from('<HHiII', b, s + 4)
    w = {'unity': unity, 'fine': fine, 'gain': gain, 'looping': False, 'release': False, 'loop_start': 0,
         'loop_length': 0}
    if loops == 0:
        return w
    cb = struct.unpack_from('<I', b, s)[0]
    _, ltype, lstart, llen = struct.unpack_from('<IIII', b, s + cb)
    w.update(looping=True, release=ltype == 1, loop_start=lstart, loop_length=llen)
    return w


def parse_articulators(b, s, e):
    arts = []
    for cid, typ, cs, ce in chunks(b, s, e):
        if cid not in (b'art1', b'art2'):
            raise ValueError('articulator list holds %r' % cid)
        cb, n = struct.unpack_from('<II', b, cs)
        conns = []
        for i in range(n):
            src, ctl, dst, _tr, scale = struct.unpack_from('<HHHHi', b, cs + cb + 12 * i)
            conns.append((src, ctl, dst, scale))
        arts.append(conns)
    return arts


def parse_region(b, s, e):
    r = {'lo': 0, 'hi': 0, 'vlo': 0, 'vhi': 0, 'group': 0, 'wsmp': None, 'link': 0, 'arts': []}
    have_arts = False
    for cid, typ, cs, ce in chunks(b, s, e):
        if cid == b'rgnh':
            r['lo'], r['hi'], r['vlo'], r['vhi'], _opt, r['group'] = struct.unpack_from('<HHHHHH', b, cs)
        elif cid == b'wsmp':
            r['wsmp'] = parse_wsmp(b, cs)
        elif cid == b'wlnk':
            r['link'] = struct.unpack_from('<I', b, cs + 8)[0]
        elif cid == b'LIST' and typ in (b'lart', b'lar2'):
            if have_arts and typ != b'lar2':
                continue
            r['arts'] = parse_articulators(b, cs, ce)
            have_arts = len(r['arts']) != 0
    return r


def parse_instrument(b, s, e):
    ins = {'bank': 0, 'patch': 0, 'name': '', 'regions': [], 'arts': [], 'count': 0}
    have_arts = False
    for cid, typ, cs, ce in chunks(b, s, e):
        if cid == b'insh':
            ins['count'], ins['bank'], ins['patch'] = struct.unpack_from('<III', b, cs)
        elif cid == b'LIST' and typ == b'INFO':
            ins['name'] = parse_info(b, cs, ce)
        elif cid == b'LIST' and typ == b'lrgn':
            regions = [c for c in chunks(b, cs, ce)]
            if len(regions) < ins['count']:
                raise ValueError('an instrument lists fewer regions than its header')
            for i in range(ins['count']):
                rid, rtyp, rs, re_ = regions[i]
                if rid != b'LIST' or rtyp not in (b'rgn ', b'rgn2'):
                    raise ValueError('a region list holds %r' % rtyp)
                ins['regions'].append(parse_region(b, rs, re_))
        elif cid == b'LIST' and typ in (b'lart', b'lar2'):
            if have_arts and typ != b'lar2':
                continue
            ins['arts'] = parse_articulators(b, cs, ce)
            have_arts = len(ins['arts']) != 0
    return ins


def parse_wave(b, s, e):
    w = {'name': '', 'format': 0, 'channels': 0, 'rate': 0, 'align': 0, 'bits': 0, 'pcm': b'', 'wsmp': None,
         'coeff1': ADPCM_COEFF1, 'coeff2': ADPCM_COEFF2}
    for cid, typ, cs, ce in chunks(b, s, e):
        if cid == b'LIST' and typ == b'INFO':
            w['name'] = parse_info(b, cs, ce)
        elif cid == b'data':
            w['pcm'] = b[cs:ce]
        elif cid == b'wsmp':
            w['wsmp'] = parse_wsmp(b, cs)
        elif cid == b'fmt ':
            w['format'], w['channels'], w['rate'], _avg, w['align'], w['bits'] = struct.unpack_from('<HHIIHH', b, cs)
            if w['format'] == 2:
                count = struct.unpack_from('<H', b, cs + 20)[0]
                if count == 7:
                    pairs = struct.unpack_from('<14h', b, cs + 22)
                    w['coeff1'], w['coeff2'] = list(pairs[0::2]), list(pairs[1::2])
    return w


def parse_dls(b):
    if len(b) < 12 or b[:4] != b'RIFF' or b[8:12] != b'DLS ':
        raise ValueError('not a DLS collection')
    end = min(8 + struct.unpack_from('<I', b, 4)[0], len(b))
    dls = {'name': '', 'count': 0, 'instruments': [], 'pool': [], 'waves': []}
    for cid, typ, cs, ce in chunks(b, 12, end):
        if cid == b'colh':
            dls['count'] = struct.unpack_from('<I', b, cs)[0]
        elif cid == b'LIST' and typ == b'INFO':
            dls['name'] = parse_info(b, cs, ce)
        elif cid == b'ptbl':
            cb, n = struct.unpack_from('<II', b, cs)
            dls['pool'] = list(struct.unpack_from('<%dI' % n, b, cs + cb))
        elif cid == b'LIST' and typ == b'lins':
            items = [c for c in chunks(b, cs, ce)]
            if len(items) < dls['count']:
                raise ValueError('the collection lists fewer instruments than its header')
            for i in range(dls['count']):
                iid, ityp, i_s, i_e = items[i]
                if iid != b'LIST' or ityp != b'ins ':
                    raise ValueError('an instrument list holds %r' % ityp)
                dls['instruments'].append(parse_instrument(b, i_s, i_e))
        elif cid == b'LIST' and typ == b'wvpl':
            for wid, wtyp, ws, we in chunks(b, cs, ce):   # every chunk is a slot (an empty one if not a wave)
                dls['waves'].append(parse_wave(b, ws, we) if wid == b'LIST' and wtyp == b'wave' else None)
    return dls


def decode(w):
    """The wave's samples as int16 (DmDls_decodeSamples: 16-bit PCM, or mono MS-ADPCM)."""
    if w is None:
        return []
    if w['format'] == 1:
        n = len(w['pcm']) // 2
        return list(struct.unpack_from('<%dh' % n, w['pcm']))
    if w['format'] == 2:
        if w['channels'] != 1 or w['align'] <= 7:
            return []
        out = []
        pcm, align = w['pcm'], w['align']
        for blk in range(len(pcm) // align):
            p = blk * align
            pred, delta, a, bb = struct.unpack_from('<Bhhh', pcm, p)
            out += [bb, a]
            c1, c2 = w['coeff1'][pred], w['coeff2'][pred]
            for byte in pcm[p + 7:p + align]:
                for nib in (byte >> 4, byte & 0xF):
                    signed = nib - 16 if nib & 8 else nib
                    v = clamp(cdiv(c1 * a + c2 * bb, 256) + signed * delta, -32768, 32767)
                    out.append(v)
                    bb, a = a, v
                    delta = max(cdiv(ADPCM_ADAPT[nib] * delta, 256), 16)
        return out
    return []


# ---- Tsf.c's generators and modulators
def generators(conns):
    gens = []
    for src, ctl, dst, scale in conns:
        if ctl != SRC_NONE:
            continue
        hi = scale >> 16
        if dst == DST_PITCH and src == SRC_LFO:
            gens.append((kModLfoToPitch, clamp(hi, -1200, 1200)))
        elif dst == 1 and src == SRC_LFO:
            gens.append((kModLfoToVolume, clamp(hi, 0, 120)))
        elif dst == DST_PITCH and src == SRC_VIBRATO:
            gens.append((kVibLfoToPitch, clamp(hi, -1200, 1200)))
        elif dst == DST_PITCH and src == SRC_EG2:
            gens.append((kModEnvToPitch, clamp(hi, -1200, 1200)))
        elif dst == DST_PITCH and src == SRC_KEY:
            gens.append((kScaleTuning, cdiv(scale, 12800) * 100))
        elif src != SRC_NONE:
            continue
        elif dst == DST_PITCH:
            continue   # (a modulator: the fine tune already carries the sample's tuning)
        elif dst in DST_GENERATORS:
            gen, how = DST_GENERATORS[dst]
            gens.append((gen, 1000 - clamp(hi, 0, 1000) if how == 'sustain' else hi))
    return [(g, s16(v)) for g, v in gens]


def modulators(conns):
    """(src, dest, amount, amount source, transform) records, as Tsf.c builds them."""
    mods = []
    for src, ctl, dst, scale in conns:
        if ctl != SRC_NONE:
            continue
        hi = scale >> 16
        if src == SRC_NONE and dst == DST_PITCH:
            mods.append((0, kFineTune, -hi, 0, 0))
        elif dst == EG1_DECAY and src == SRC_KEY:
            mods.append((kNoteOnKey, kDecayVolEnv, hi, 0, 0))
        elif dst == EG2_DECAY and src == SRC_KEY:
            mods.append((kNoteOnKey, kDecayModEnv, hi, 0, 0))
        elif dst == EG1_ATTACK and src == SRC_VELOCITY:
            mods.append((kNoteOnVelocity, kAttackVolEnv, hi, 0, 0))
        elif dst == EG2_ATTACK and src == SRC_VELOCITY:
            mods.append((kNoteOnVelocity, kAttackModEnv, hi, 0, 0))
        elif dst == EG1_HOLD and src == SRC_KEY:
            mods.append((kNoteOnKey, kHoldVolEnv, hi, 0, 0))
        elif dst == EG2_HOLD and src == SRC_KEY:
            mods.append((kNoteOnKey, kHoldModEnv, hi, 0, 0))
    return [(a, b, s16(c), d, e) for a, b, c, d, e in mods]


NO_WSMP = {'unity': 0, 'fine': 0, 'gain': 0, 'looping': False, 'release': False, 'loop_start': 0, 'loop_length': 0}


def region_generators(ins, reg, sample_start=0, data_length=1 << 62):
    w = reg['wsmp'] or NO_WSMP
    gens = [(kKeyRange, (reg['hi'] & 0xFF) << 8 | (reg['lo'] & 0xFF))]
    if reg['vhi'] <= reg['vlo']:
        gens.append((kVelRange, 127 << 8 | 0))
    else:
        gens.append((kVelRange, (reg['vhi'] & 0xFF) << 8 | (reg['vlo'] & 0xFF)))
    if reg['group'] != 0:
        gens.append((kExclusiveClass, reg['group']))
    gens.append((kOverridingRootKey, w['unity']))
    gens.append((kFineTune, s16(w['fine'] >> 16)))   # (Tsf.c: a uint16 shifted by 16, always 0)
    # the DLS attenuation in cB, x 2.5: rustysynth (as EMU's hardware, FluidSynth and Polyphone) applies 0.4 of a
    # SoundFont's initialAttenuation, TinySoundFont and DirectMusic all of it; so the regions keep dmusic's levels
    gens.append((kInitialAttenuation, s16(round(-(w['gain'] >> 16) * ATTENUATION_SCALE))))
    loop = 0
    if w['looping']:
        start = w['loop_start']
        stop = w['loop_start'] + w['loop_length']
        # (TinySoundFont clamped a loop's end to the font's samples; a SoundFont's must lie before the data's end)
        stop = min(stop, data_length - 1 - sample_start)
        gens.append((kStartLoopOffset, (start % 32768) & 0xFFFF))
        gens.append((kStartLoopOffsetCoarse, (start // 32768) & 0xFFFF))
        gens.append((kEndLoopOffset, (stop % 32768) & 0xFFFF))
        gens.append((kEndLoopOffsetCoarse, (stop // 32768) & 0xFFFF))
        loop = 3 if w['release'] else 1
    gens.append((kSampleModes, loop))
    mods = []
    for conns in (ins['arts'] if not reg['arts'] else reg['arts']):
        gens += generators(conns)
        mods += modulators(conns)
    gens.append((kSampleID, reg['link'] & 0xFFFF))
    return gens, mods


# ---- the SoundFont writer
def name20(s):
    return s.encode('latin-1', 'replace')[:19].ljust(20, b'\0')


def chunk(cid, data):
    return cid + struct.pack('<I', len(data)) + data + (b'\0' if len(data) & 1 else b'')


def lst(typ, data):
    return chunk(b'LIST', typ + data)


def info_str(s):
    v = s.encode('latin-1', 'replace') + b'\0'
    return v + (b'\0' if len(v) & 1 else b'')


def gen_record(oper, amount):
    return struct.pack('<HH', oper, amount & 0xFFFF)


def convert(dls_bytes, name=None):
    """The SoundFont 2.01 file for a DLS collection's bytes."""
    dls = parse_dls(dls_bytes)
    # samples and their headers (one per wave-pool slot, 46 zeros after each)
    smpl = []
    shdr = b''
    starts = []
    for i, w in enumerate(dls['waves']):
        data = decode(w)
        start = len(smpl)
        starts.append(start)
        smpl += data
        end = len(smpl)
        smpl += [0] * PADDING
        rate = w['rate'] if w else 0
        unity = w['wsmp']['unity'] if w and w['wsmp'] else 60
        shdr += name20(w['name'] if w else '') + struct.pack('<IIIIIBbHH', start, end, start, start, rate,
                                                             unity & 0xFF if unity <= 127 else 60, 0, 0, 1)
    shdr += name20('EOS') + struct.pack('<IIIIIBbHH', 0, 0, 0, 0, 0, 0, 0, 0, 0)
    # presets: one zone each, its instrument
    phdr = pbag = pgen = b''
    for i, ins in enumerate(dls['instruments']):
        bank = 128 if ins['bank'] & DRUM_KIT else ins['bank'] & 0xFFFF
        phdr += name20(ins['name']) + struct.pack('<HHHIII', ins['patch'] & 0xFFFF, bank, i, 0, 0, 0)
        pbag += struct.pack('<HH', i, 0)
        pgen += gen_record(kInstrument, i)
    n = len(dls['instruments'])
    phdr += name20('EOP') + struct.pack('<HHHIII', 0, 0, n, 0, 0, 0)
    pbag += struct.pack('<HH', n, 0)
    pgen += gen_record(0, 0)
    pmod = struct.pack('<HHhHH', 0, 0, 0, 0, 0)
    # instruments: a zone per region
    inst = ibag = igen = imod = b''
    ngen = nmod = nbag = 0
    for ins in dls['instruments']:
        inst += name20(ins['name']) + struct.pack('<H', nbag)
        for reg in ins['regions']:
            ibag += struct.pack('<HH', ngen, nmod)
            nbag += 1
            sample_start = starts[reg['link']] if reg['link'] < len(starts) else 0
            gens, mods = region_generators(ins, reg, sample_start, len(smpl))
            for oper, amount in gens:
                igen += gen_record(oper, amount)
            for m in mods:
                imod += struct.pack('<HHhHH', *m)
            ngen += len(gens)
            nmod += len(mods)
    inst += name20('EOI') + struct.pack('<H', nbag)
    ibag += struct.pack('<HH', ngen, nmod)
    igen += gen_record(0, 0)
    imod += struct.pack('<HHhHH', 0, 0, 0, 0, 0)
    if ngen > 0xFFFF or nmod > 0xFFFF or nbag > 0xFFFF:
        raise ValueError('too many generators for SoundFont 2 indices')

    title = name or dls['name'] or 'DLS'
    info = lst(b'INFO', chunk(b'ifil', struct.pack('<HH', 2, 1)) + chunk(b'isng', info_str('EMU8000'))
               + chunk(b'INAM', info_str(title)) + chunk(b'ISFT', info_str(TOOL)))
    sdta = lst(b'sdta', chunk(b'smpl', struct.pack('<%dh' % len(smpl), *smpl)))
    pdta = lst(b'pdta', chunk(b'phdr', phdr) + chunk(b'pbag', pbag) + chunk(b'pmod', pmod) + chunk(b'pgen', pgen)
               + chunk(b'inst', inst) + chunk(b'ibag', ibag) + chunk(b'imod', imod) + chunk(b'igen', igen)
               + chunk(b'shdr', shdr))
    return chunk(b'RIFF', b'sfbk' + info + sdta + pdta)


# ---- the check: a SoundFont re-parsed, every index and size checked
def check(sf2):
    """A list of problems (empty: the file is consistent)."""
    p = []
    if sf2[:4] != b'RIFF' or sf2[8:12] != b'sfbk':
        return ['not a SoundFont 2 file']
    if struct.unpack_from('<I', sf2, 4)[0] + 8 != len(sf2):
        p.append('the RIFF size is not the file size')
    lists = {typ: (cs, ce) for cid, typ, cs, ce in chunks(sf2, 12, len(sf2)) if cid == b'LIST'}
    for need in (b'INFO', b'sdta', b'pdta'):
        if need not in lists:
            return p + ['no %s list' % need.decode()]
    info = {cid: sf2[cs:ce] for cid, typ, cs, ce in chunks(sf2, *lists[b'INFO'])}
    if info.get(b'ifil', b'')[:4] != struct.pack('<HH', 2, 1):
        p.append('ifil is not 2.01')
    sdta = {cid: (cs, ce) for cid, typ, cs, ce in chunks(sf2, *lists[b'sdta'])}
    if b'smpl' not in sdta:
        return p + ['no smpl']
    nsmp = (sdta[b'smpl'][1] - sdta[b'smpl'][0]) // 2
    sizes = {b'phdr': 38, b'pbag': 4, b'pmod': 10, b'pgen': 4, b'inst': 22, b'ibag': 4, b'imod': 10, b'igen': 4,
             b'shdr': 46}
    pd = {}
    for cid, typ, cs, ce in chunks(sf2, *lists[b'pdta']):
        if cid not in sizes:
            p.append('pdta holds %r' % cid)
            continue
        if (ce - cs) % sizes[cid] or ce - cs == 0:
            p.append('%s is not a whole number of records' % cid.decode())
        pd[cid] = [sf2[cs + i:cs + i + sizes[cid]] for i in range(0, ce - cs, sizes[cid])]
    if set(pd) != set(sizes):
        return p + ['pdta lacks %s' % sorted(k.decode() for k in set(sizes) - set(pd))]
    phdr = [struct.unpack_from('<20sHHHIII', r) for r in pd[b'phdr']]
    pbag = [struct.unpack_from('<HH', r) for r in pd[b'pbag']]
    pgen = [struct.unpack_from('<HH', r) for r in pd[b'pgen']]
    inst = [struct.unpack_from('<20sH', r) for r in pd[b'inst']]
    ibag = [struct.unpack_from('<HH', r) for r in pd[b'ibag']]
    igen = [struct.unpack_from('<HH', r) for r in pd[b'igen']]
    shdr = [struct.unpack_from('<20sIIIIIBbHH', r) for r in pd[b'shdr']]
    for what, recs, term in ((b'phdr', phdr, b'EOP'), (b'inst', inst, b'EOI'), (b'shdr', shdr, b'EOS')):
        if recs[-1][0].split(b'\0', 1)[0] != term:
            p.append('%s has no %s record' % (what.decode(), term.decode()))
    # the bag indices rise and end at their terminal records
    def rising(name, idx, count):
        if any(b < a for a, b in zip(idx, idx[1:])):
            p.append('%s indices fall' % name)
        if idx[-1] != count:
            p.append('%s ends at %d, not %d' % (name, idx[-1], count))
    rising('phdr bag', [r[3] for r in phdr], len(pbag) - 1)
    rising('pbag gen', [r[0] for r in pbag], len(pgen) - 1)
    rising('pbag mod', [r[1] for r in pbag], len(pd[b'pmod']) - 1)
    rising('inst bag', [r[1] for r in inst], len(ibag) - 1)
    rising('ibag gen', [r[0] for r in ibag], len(igen) - 1)
    rising('ibag mod', [r[1] for r in ibag], len(pd[b'imod']) - 1)
    for g in pgen[:-1]:
        if g[0] == kInstrument and g[1] >= len(inst) - 1:
            p.append('a preset names instrument %d of %d' % (g[1], len(inst) - 1))
    for z in range(len(ibag) - 1):
        gens = igen[ibag[z][0]:ibag[z + 1][0]]
        ops = [g[0] for g in gens]
        if not ops or ops[-1] != kSampleID:
            p.append('instrument zone %d does not end with its sample' % z)
            continue
        if ops[0] != kKeyRange or (len(ops) > 1 and ops[1] != kVelRange):
            p.append('instrument zone %d: keyRange, velRange not first' % z)
        sid = gens[-1][1]
        if sid >= len(shdr) - 1:
            p.append('instrument zone %d names sample %d of %d' % (z, sid, len(shdr) - 1))
            continue
        s = shdr[sid]
        if not (s[1] <= s[2] and s[2] + PADDING <= nsmp):
            p.append('sample %d lies outside the sample data' % sid)
        loop = {g[0]: g[1] for g in gens}
        if loop.get(kSampleModes, 0):
            ls = s[3] + loop.get(kStartLoopOffset, 0) + 32768 * loop.get(kStartLoopOffsetCoarse, 0)
            le = s[4] + loop.get(kEndLoopOffset, 0) + 32768 * loop.get(kEndLoopOffsetCoarse, 0)
            # (a loop may run past its sample's end, as some of the game's regions' do: TinySoundFont played on into
            # the next samples, and so does rustysynth; only the sample data's end is a limit)
            if not (s[1] <= ls < le < nsmp):
                p.append('instrument zone %d: its loop %d-%d is outside the sample data' % (z, ls, le))
        if s[2] >= nsmp:
            p.append('sample %d ends at the data end (rustysynth needs end < data length)' % sid)
    return p


def report(paths):
    """What decides the conversion's open questions: the gains, the fine tunes, the regions without wsmp, the pool."""
    for path in paths:
        dls = parse_dls(open(path, 'rb').read())
        gains, fines, no_wsmp, regions, overruns = set(), set(), 0, 0, 0
        for ins in dls['instruments']:
            for reg in ins['regions']:
                regions += 1
                w = dls['waves'][reg['link']] if reg['link'] < len(dls['waves']) else None
                if reg['wsmp'] and reg['wsmp']['looping'] and w is not None:
                    overruns += reg['wsmp']['loop_start'] + reg['wsmp']['loop_length'] > len(decode(w))
                if reg['wsmp'] is None:
                    no_wsmp += 1
                else:
                    gains.add(reg['wsmp']['gain'])
                    fines.add(reg['wsmp']['fine'])
        wave_fines = {w['wsmp']['fine'] for w in dls['waves'] if w and w['wsmp']}
        # the pool table holds each wave's offset in the wave pool: Tsf.c's direct index is right when they rise
        identity = dls['pool'] == sorted(dls['pool']) and len(dls['pool']) == len(dls['waves'])
        print('%s: %d instruments, %d regions (%d without wsmp), gains %s, region fine tunes %s, wave fine tunes %s, '
              'pool %d %s, %d loops past their sample' % (path, len(dls['instruments']), regions, no_wsmp,
                              sorted(g / 65536.0 for g in gains), sorted(fines), sorted(wave_fines),
                              len(dls['pool']), 'in wave order' if identity else 'NOT in wave order', overruns))


def main(argv):
    if len(argv) >= 2 and argv[0] == '--check':
        bad = 0
        for f in argv[1:]:
            problems = check(open(f, 'rb').read())
            print('%s: %s' % (f, 'OK' if not problems else '; '.join(problems)))
            bad += bool(problems)
        return 1 if bad else 0
    if len(argv) >= 2 and argv[0] == '--report':
        report(argv[1:])
        return 0
    if len(argv) != 2:
        print(__doc__)
        return 2
    import os
    data = convert(open(argv[0], 'rb').read(), os.path.splitext(os.path.basename(argv[0]))[0])
    problems = check(data)
    if problems:
        print('%s: %s' % (argv[0], '; '.join(problems)))
        return 1
    with open(argv[1], 'wb') as f:
        f.write(data)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
