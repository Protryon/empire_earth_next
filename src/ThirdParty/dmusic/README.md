# dmusic (vendored)

[GothicKit/dmusic](https://github.com/GothicKit/dmusic), an implementation of DirectMusic (segments, styles,
bands, DLS, the composer, its own synthesizer on TinySoundFont), commit
`b73b270b94714b8ec8d9d0b2a9f3a5c445da52d1` (2026-03-02). MIT (Modern Variant), see `LICENSE.md`.

Taken: `include/`, `src/` (without `src/thread/Thread.Posix.c`), `vendor/TinySoundFont/tsf.{c,h}`.
`include_dirs.txt` lists the directories the build adds to `INCLUDE` for these sources.

Changes made here (each marked "empire_earth_next addition" in `dmusic.h`):

- `DmSegment_setRepeats`: the game loops a score by setting a large repeat count.
- `DmPerformance_setMasterGrooveLevel` (+ `groove_master` in `DmPerformance`): an offset added to the command
  track's groove level when a pattern is chosen, as DirectMusic's master groove level (the game's music
  intensity).
