# Lane: vendored graphics code out (branch rust-vendor-gfx)

State (2026-11-17): done, one round.

- libjpeg is gone (src/ThirdParty/libjpeg, include/jpeglib, the `jpeg` module). JPEG decoding is `ee_base::jpeg`
  (libjpeg 6b's decode in Rust, moved from ee_won_gui); the Graphics Engine's JPG/JPG2 surface files are Rust
  (ee_render openformats surface_file.rs), written with the `jpeg-encoder` crate.
- Movie playback is gone: pl_mpeg, DX7Movie, GEMovie, UIMovie, the mixer's movie source, the movie messages,
  tools/convert_movies.py, make_play_dir's `--movies`. Every movie call site goes on as without a movie file.
- Follow-up (owner): once the whole port is done, replace jpeg.rs with a library decoder (e.g. zune-jpeg) and
  re-record what depends on its pixels.
- Left: the scenario editor's movie file box and the scenario's movie name (a file-format field); CLAUDE.md's
  `make_play_dir.py ... [--movies DIR]` line (agents do not edit CLAUDE.md); tests/sound's recorders still name
  `SetMovieSource` (not built by the tools).

PORT_LEARNINGS: "Vendored graphics code out: libjpeg and the movie player".
