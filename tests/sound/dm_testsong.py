"""Writes the music engine's test songs (rust/crates/ee_audio/src/dmusic/testdata): two DirectMusic segments on one
style, our own content, made to reach every path of the performance that the golden replay compares
(golden_dmusic.txt, from tests/sound/dm_golden.c over dmusic):

- the style: patterns at groove ranges 1-10 (4 and 2 measures), 11-20 (8), a 1-measure groove (never chosen), the
  fill, intro, break and end embellishments; parts with notes in every play mode (fixed, chord root and chord
  intervals, scale intervals, the key root that does not convert, the part's own flags), time, duration and
  velocity ranges, a note before its pattern's start, variations by lock, sequence and random; curves of every
  shape on volume, pan and expression, a pitch bend, an out-of-range curve;
- the segments: tempo changes, commands with groove ranges and embellishments, chords with sub-chords and scales,
  a message at time -1, a loop, a band with general MIDI, GS and drum instruments, an instrument on a missing
  collection and one on a channel above 16.

    python -I tests/sound/dm_testsong.py <out folder>
"""
import struct
import sys
import pathlib


def chunk(cid, body, typ=None):
    if typ is not None:
        body = typ + body
    out = cid + struct.pack('<I', len(body)) + body
    return out + (b'\0' if len(body) % 2 else b'')


def lst(typ, *parts):
    return chunk(b'LIST', b''.join(parts), typ)


def riff(typ, *parts):
    return chunk(b'RIFF', b''.join(parts), typ)


def utf16(s):
    return s.encode('utf-16-le') + b'\0\0'


def guid(n):
    return bytes([n] * 16)


def unfo(name):
    return lst(b'UNFO', chunk(b'UNAM', utf16(name)))


def dmrf(file, g, name=''):
    return lst(b'DMRF', chunk(b'refh', guid(0x77) + struct.pack('<I', 0x13)), chunk(b'guid', g),
               chunk(b'name', utf16(name or file)), chunk(b'file', utf16(file)))


VALID_PATCH, VALID_PAN, VALID_VOLUME, VALID_TRANSPOSE, GM, GS = 1, 1 << 5, 1 << 6, 1 << 7, 1 << 8, 1 << 9


def band(name, instruments):
    ins = []
    for channel, patch, options, pan, volume, transpose, ref in instruments:
        bins = struct.pack('<IIIIIIIIBBhI', patch, 0, 0, 0, 0, 0, channel, options, pan, volume, transpose, 0)
        ins.append(lst(b'lbin', chunk(b'bins', bins), *( [dmrf(*ref)] if ref else [])))
    return riff(b'DMBD', chunk(b'guid', guid(0x40 + len(name))), unfo(name), lst(b'lbil', *ins))


def timesig(bpm, beat, grids):
    return struct.pack('<BBH', bpm, beat, grids)


def note(grid, var, dur, toff, value, vel, trange=0, drange=0, vrange=0, flags=16):
    return struct.pack('<IIIhHBBBBBB', grid, var, dur, toff, value, vel, trange, drange, vrange, 0, flags) + b'\0\0'


def curve(grid, var, dur, toff, start, end, reset, etype, shape, cc, flags=0):
    return struct.pack('<IIIIhhhhBBBB', grid, var, dur, 0, toff, start, end, reset, etype, shape, cc, flags)


def part(pid, flags, variations, notes, curves):
    choices = list(variations) + [0] * (32 - len(variations))
    prth = timesig(4, 4, 4) + struct.pack('<32I', *choices) + guid(pid) + struct.pack('<HBBBB', 1, flags, 0, 0, 0)
    out = [chunk(b'prth', prth), unfo(f'part{pid}')]
    if notes:
        out.append(chunk(b'note', struct.pack('<I', 24) + b''.join(notes)))
    if curves:
        out.append(chunk(b'crve', struct.pack('<I', 28) + b''.join(curves)))
    return lst(b'part', *out)


def pattern(name, bottom, top, emb, measures, prefs):
    ptnh = timesig(4, 4, 4) + struct.pack('<BBHH', bottom, top, emb, measures)
    refs = [lst(b'pref', chunk(b'prfc', guid(pid) + struct.pack('<HBBBB', ch, lock, level, 0, rv)))
            for pid, ch, lock, level, rv in prefs]
    return lst(b'pttn', chunk(b'ptnh', ptnh), unfo(name), chunk(b'rhtm', struct.pack('<I', 1)), *refs)


CHORD_ROOT, SCALE, CHORD = 2, 4, 8


def style():
    melody = part(1, CHORD_ROOT | CHORD, [1, 2, 4], [
        note(0, 1, 380, 0, 0x5000, 100),
        note(2, 1, 380, -3, 0x5100, 90, trange=4, drange=191, vrange=10),
        note(4, 2, 760, 0, 0x5200, 110, flags=CHORD_ROOT | SCALE),
        note(6, 3, 190, 12, 0x5011, 80),                       # scale position and an accidental
        note(8, 4, 190, 0, 0x5309, 70),                        # past the chord: by the scale
        note(9, 7, 190, 0, 0x4f20, 60, flags=SCALE),
        note(10, 1, 90, 0, 0xe080, 64),                        # a low octave, a scale carry
        note(12, 2, 190, 0, 0x5000, 50, flags=1),              # the key root: no note
        note(13, 1, 190, 0, 72, 127, flags=0),                 # fixed: the value is the note
        note(14, 4, 100, 0, 0x5101, 90, drange=230, vrange=200),
    ], [
        curve(0, 1, 120, 0, 20, 110, 100, 4, 0, 7, 1),          # volume, linear, reset
        curve(4, 2, 100, 0, 0, 127, 64, 4, 4, 10),              # pan, sine
        curve(8, 4, 80, 0, 127, 30, 0, 4, 2, 11),              # expression, exp
        curve(10, 1, 90, 0, 10, 90, 0, 4, 3, 7),               # volume, log
        curve(12, 7, 100, 0, 5, 70, 0, 4, 1, 7),                # instant
        curve(1, 1, 60, 0, 8192, 12000, 0, 3, 0, 0, 1),        # pitch bend, reset
        curve(3, 2, 300, 0, -2000, 127, 0, 4, 0, 7),            # out of range: none
        curve(5, 1, 200, 0, 0, 0, 0, 5, 0, 0),                  # aftertouch: none
    ])
    bass = part(2, CHORD_ROOT | SCALE, [1, 2], [
        note(0, 3, 760, -8, 0x3000, 100),                      # before the pattern's start
        note(8, 1, 760, 0, 0x3010, 100, trange=212),
        note(4, 2, 380, 0, 0x3100, 95),
    ], [])
    drums = part(3, 0, [1], [note(g, 1, 40, 0, 35 + g % 5, 100, flags=0) for g in range(0, 16, 2)], [])
    pad = part(4, CHORD_ROOT | CHORD, [1, 2, 4, 8, 16], [
        note(0, 31, 1500, 0, 0x5000, 70), note(0, 31, 1500, 0, 0x5100, 70), note(0, 31, 1500, 0, 0x5200, 70),
    ], [curve(0, 31, 150, 0, 64, 127, 64, 4, 0, 11)])
    patterns = [
        pattern('groove4', 1, 10, 0, 4, [(1, 0, 1, 0, 0), (2, 1, 1, 1, 1), (3, 9, 0, 0, 0), (4, 20, 2, 2, 3)]),
        pattern('groove2', 1, 10, 0, 2, [(1, 0, 1, 0, 1), (3, 9, 0, 0, 0)]),
        pattern('groove2b', 1, 10, 0, 2, [(2, 1, 0, 0, 2), (4, 20, 0, 1, 0)]),
        pattern('high8', 11, 20, 0, 8, [(1, 0, 3, 0, 1), (2, 1, 3, 0, 1), (3, 9, 0, 0, 0), (4, 20, 0, 0, 4)]),
        pattern('one', 1, 100, 0, 1, [(3, 9, 0, 0, 0)]),
        pattern('fill', 1, 100, 1, 1, [(3, 9, 0, 0, 0), (1, 0, 0, 0, 0)]),
        pattern('intro', 1, 100, 2, 2, [(4, 20, 0, 0, 0), (2, 1, 0, 0, 0)]),
        pattern('break', 1, 100, 4, 1, [(2, 1, 0, 0, 0)]),
        pattern('end', 1, 100, 8, 1, [(4, 20, 0, 0, 0), (1, 0, 0, 0, 1)]),
    ]
    sb = band('style band', [(0, 0x000000, VALID_PATCH | GM | VALID_PAN, 30, 0, 0, ('gm.dls', guid(9)))])
    styh = timesig(4, 4, 4) + struct.pack('<d', 120.0)
    return riff(b'DMST', chunk(b'styh', styh), chunk(b'guid', guid(0x51)), unfo('test style'), sb,
                melody, bass, drums, pad, *patterns)


MEASURE = 4 * 768


def segment(name, tempos, commands, chords, length, repeats, loop, instruments):
    tetr = struct.pack('<I', 16) + b''.join(struct.pack('<IId', t, 0, bpm) for t, bpm in tempos)
    cmnd = struct.pack('<I', 12) + b''.join(struct.pack('<IHBBBBBx', t, 0, 0, c, level, rng, 0)
                                             for t, c, level, rng in commands)
    cord = [chunk(b'crdh', struct.pack('<I', 0))]
    for t, subs in chords:
        head = utf16('chord').ljust(32, b'\0') + struct.pack('<IHBB', t, 0, 0, 0)
        body = struct.pack('<I', len(head)) + head + struct.pack('<II', len(subs), 20)
        body += b''.join(struct.pack('<IIIIBBxx', cp, sp, 0, lv, cr, sr) for cp, sp, lv, cr, sr in subs)
        cord.append(chunk(b'crdb', body))
    style_track = lst(b'sttr', lst(b'strf', chunk(b'stmp', struct.pack('<I', 0)), dmrf('test.sty', guid(0x51))))
    band_track = riff(b'DMBT', lst(b'lbdl', lst(b'lbnd', chunk(b'bdih', struct.pack('<I', 0xffffffff)),
                                               band(name + ' band', instruments))))
    tracks = [riff(b'DMTK', chunk(b'trkh', guid(1) + struct.pack('<IIII', 0, 1, 0, 0)), chunk(b'tetr', tetr)),
              riff(b'DMTK', chunk(b'cmnd', cmnd)), riff(b'DMTK', lst(b'cord', *cord)),
              riff(b'DMTK', style_track), riff(b'DMTK', band_track)]
    segh = struct.pack('<IIIIII', repeats, length, 0, loop[0], loop[1], 0)
    return riff(b'DMSG', chunk(b'segh', segh), chunk(b'guid', guid(0x60 + len(name))), unfo(name),
                lst(b'trkl', *tracks))


MAJOR = 0xab5ab5
INSTRUMENTS = [
    (0, 0x000000, VALID_PATCH | GM | VALID_VOLUME | VALID_PAN, 40, 100, 0, ('gm.dls', guid(9))),
    (1, 0x010200, VALID_PATCH | VALID_VOLUME, 64, 90, 0, ('test.dls', guid(10))),     # a missing collection
    (9, 0x80000000, VALID_PATCH | GM, 64, 127, 0, ('gm.dls', guid(9))),                 # the drum kit
    (20, 0x080000 | 46, VALID_PATCH | GS | VALID_TRANSPOSE | VALID_VOLUME, 100, 80, -12, ('gm.dls', guid(9))),
    (3, 0x000005, 0, 0, 0, 0, None),                                                    # no valid patch
]


def main():
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    (out / 'test.sty').write_bytes(style())
    one = segment('test1', [(0, 120.0), (16 * MEASURE, 90.0)],
                  [(0, 0, 5, 4), (8 * MEASURE, 1, 1, 0), (12 * MEASURE, 0, 15, 3), (20 * MEASURE, 3, 1, 0),
                   (22 * MEASURE, 2, 1, 0), (30 * MEASURE, 4, 1, 0)],
                  [(0, [(0x91, MAJOR, 1, 0, 0), (0x891, 0x6ad6ad, 6, 5, 2)]), (4 * MEASURE, [(0x49, 0, 3, 7, 7)]),
                   (10 * MEASURE, [(0x1, 0x111, 1, 14, 3)])],
                  32 * MEASURE, 3, (4 * MEASURE, 24 * MEASURE), INSTRUMENTS)
    two = segment('test2', [(0, 70.0)], [(0, 0, 12, 0), (6 * MEASURE, 3, 1, 0), (7 * MEASURE, 0, 2, 2)],
                  [(0, [(0x891, MAJOR, 0xffffffff, 2, 2)])], 9 * MEASURE, 2, (0, 0), INSTRUMENTS[:3])
    (out / 'test1.sgt').write_bytes(one)
    (out / 'test2.sgt').write_bytes(two)


if __name__ == '__main__':
    main()
