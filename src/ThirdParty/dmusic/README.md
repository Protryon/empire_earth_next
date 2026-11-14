# dmusic (vendored)

[GothicKit/dmusic](https://github.com/GothicKit/dmusic), an implementation of DirectMusic (segments, styles,
bands, DLS, the composer, its own synthesizer on TinySoundFont), commit
`b73b270b94714b8ec8d9d0b2a9f3a5c445da52d1` (2026-03-02). MIT (Modern Variant), see `LICENSE.md`.

Taken: `include/`, `src/` (without upstream's `src/thread/Thread.Posix.c`; this copy has its own: the four C11 mutex functions dmusic uses, over pthreads, for macOS), `vendor/TinySoundFont/tsf.{c,h}`.
`include_dirs.txt` lists the directories the build adds to `INCLUDE` for these sources.

Changes made here (each marked "(EE patch)" at its place in the source):

- `DmSegment_setRepeats`: the game loops a score by setting a large repeat count.
- `DmPerformance_setGrooveLevel` (`groove_master`, `groove_base`): the game's music intensity, added once to each
  groove command's level; the command track's own level is what the next pattern's command carries. (It replaces the
  earlier `DmPerformance_setMasterGrooveLevel`.)
- Instrument bank select (`Band.c`, `Synth.c`): the bank's high and low parts and the drum flag are all compared.
  Upstream compared the low part alone, so every instrument of the game's eight songs played as preset 0.
- General MIDI, GS and XG instruments (`Band.c`): all from `gm.dls`; upstream asked for collections that do not exist
  and the parts stayed silent. The game's conductor finds `gm.dls` in the system's directory on Windows
  (`SEConductor::Resolve`); elsewhere those parts stay silent until one is supplied.
- Volume curves (`Synth.c`, `_Internal.h`, `tsf.h`): velocity, volume and expression each at 40 * log10, volume and
  expression multiplied, channels reset to volume 100.
- Master volume on sounding notes (`tsf.h` `tsf_set_volume`): the game fades each song in from 0, and upstream left
  the notes started during the fade silent to their end.
- Reverb (`Reverb.c`, `DmPerformance_setReverb`): on the finished stereo mix, with each song's own settings.
- Stop (`DmPerformance_stop`): what is due within the given time still sounds, then every note is released.
- Note order at the same time (`Message.c`): a note's end before a note's start.
- Silence before the first band (`Synth.c`): with no instruments loaded the output is cleared.
- A SoundFont in a collection's place (`io/Dls.c`, `Loader.c`, `Band.c`, `Synth.c`, `util/Tsf.c`): a SoundFont 2 file
  handed over where a DLS collection is asked for (the general MIDI set) is kept whole and given to the synthesizer,
  which reads that format itself; its presets are found by bank (the variation's MSB) and patch, a missing variation
  or drum kit falling back to the capital tone or the standard kit.
- Pattern choice (`Style.c`): of the patterns whose groove range holds the level, the longest that fits the measures
  left before the song ends or starts over (random only among equals), else the longest, cut short; with none in
  range, the nearest range's. Upstream drew any at random, or fell silent.
- Note timing (`Performance.c`): a note placed before its pattern's start plays at the start; the random ranges of
  duration and velocity vary the duration and the velocity.
- The clock (`Performance.c` `DmPerformance_renderPcm`, `time_carry`): the fraction of a tick is carried from one
  render call to the next. Upstream rounded to whole ticks in every call, so the tempo depended on the size of the
  blocks asked for (at 48 kHz: 10 % slow in 128-frame blocks, 1 to 2.6 % fast in 512-frame ones).
