# Lane: the vendored audio libraries replaced by Rust crates (branch rust-vendor-audio)

State (2026-11-17): done, pending the lead's gates and the media republish.

- **Music**: `rust/crates/ee_audio/src/dmusic`, a port of dmusic (DirectMusic segments, styles, bands, the
  performance, the composer's transitions, the reverb) playing on rustysynth. Golden: `tests/sound/dm_golden.c`
  (dmusic with a logging synthesizer) -> `golden_dmusic.txt` (synthetic songs from `tests/sound/dm_testsong.py`) and
  `golden_reverb.txt`; the game's eight songs compared locally (`EE_DMUSIC_GOLDEN`, 176k lines equal).
  `src/ThirdParty/dmusic` (with TinySoundFont) and `SEGeneralMIDI.c` deleted; `EE.sf2` moved into the crate.
- **Banks**: `tools/dls2sf2.py` converts each DLS bank to a SoundFont at data build time (`open_data.py build`);
  the tree's Music has `X.sf2` and no `.dls` (the D9 check rejects `.dls`). The media release part must be
  republished (the lead, at the merge to main).
- **Mixer**: the device (cpal) and the decoder (symphonia) in `rust/crates/ee_audio/src/mixer/device.rs`;
  `include/miniaudio` and `src/ThirdParty/miniaudio` deleted. Every WAV decodes bit for bit as miniaudio decoded it;
  the ten MP3s differ by at most 2.7e-5 (and five 22 kHz ones gain the last 576 frames).
- Not done here: the harness gates (the lead), a listening test (the owner), a Linux link (`libasound2-dev` needed)
  and a macOS link (the audio frameworks were added to xbuild.py's link lines, unbuilt here).
