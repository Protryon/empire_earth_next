# Lane: dependencies (hand-written Rust replaced by established crates)

The lane's score is Rust lines removed. A swap is taken only when every golden, capture, oracle, saveload row, model
or font dump and WON test vector stays as it was (the owner's rule for round 1, 2026-11-24): nothing is
re-baselined. A swap that would move one is reverted and listed under "Deferred".

Crates must be established (at least a few months old), build for x86_64-pc-windows-msvc and stay POSIX-capable.

## Round 1 (branch rust-deps1, forks rust-deps1-a and rust-deps1-b, 2026-11-24)

State: done. Rust: +1,775 / -5,718 lines in `.rs` files, **3,943 removed**. `rust/Cargo.lock`: 230 -> 258 packages.

| Package | Files | Rust lines (+ / -, net) | Crates added |
|---|---|---|---|
| glTF + JSON (ee_render) | `openformats/cgltf.rs` and `jsmn.rs` deleted, `json.rs`, `import.rs`, `mod.rs`, new `document.rs` | +1,334 / -3,456, **-2,122** | gltf 1.4.1 (no default features; extras, extensions, names) with gltf-json, gltf-derive, inflections, byteorder; serde_json 1.0.143 (`float_roundtrip`, already in the lock) |
| PNG (ee_base) | `png.rs` 1,628 -> 619 lines | +162 / -1,171, **-1,009** | png 0.18.1 with fdeflate, flate2, miniz_oxide, crc32fast, simd-adler32, adler2, zlib-rs |
| TrueType parsing (ee_base) | `plat_ui/ttf.rs`, `font.rs` | +112 / -523, **-411** | ttf-parser 0.25.1 (no default features, `std`) |
| WON crypto (ee_won_net) | `md5.rs`, `blowfish.rs`, `blowfish_tables.rs` deleted, `crc16.rs`, `bigint.rs` | +154 / -497, **-343** | md-5, blowfish (RustCrypto), crc, num-bigint |
| UTF-8 (ee_base) | `plat_core/utf8.rs` | +12 / -70, **-58** | none (std) |

(`gx/modes.rs` +1/-1: a test's type annotation that serde_json's `PartialEq` impls made ambiguous.)

### glTF and JSON

- `document.rs` reads a model's document with the gltf crate's `gltf-json` structures and GLB container (none of
  its importer: the game resolves buffers and textures itself) and keeps what cgltf did beyond parsing: the range
  checks, the buffer loads, the validation and the bounds-checked accessor readers.
- `json.rs` is serde_json's `Value` for the EE_model extension and a material's extras, plus `JsonNumber`, the
  C++ `JParser`'s rule (an index only from an integer token, a float from any number). `float_roundtrip`: a number
  is the nearest double of its text, as the C++ `from_chars` and the Python converter read it.
- Proof: `eemodel.py cppgate` against a game build of the fork's tree: the store gate 25,140 lines equal (2,583
  model loads, 17,535 texture loads), the decoder gate 7,104 of 7,104 golden digests (861 models, 861 without
  EE_model, 2,568 edited models of which 639 refused, 34 `.glb`, 2,505 textures, 275 PNG variants).
- Malformed documents no gate case has are judged differently: a document without its `asset` member is refused
  (cgltf accepted it), and a name or URI that is not UTF-8 or holds a lone surrogate escape is refused as not JSON.

### PNG

- `png.rs` keeps the C interface and the game's conversions (texture and picture forms); inflate, unfiltering and
  de-interlacing are the png crate's. `scan_png` walks the chunks as stb did over memory, so a file cut short
  gets stb's message (the decoder gate's `t1400_truncated` caught the first version's `outofdata`).
- Proof: a test-only copy of the old `png.rs` against the new, then deleted. Every PNG of the open tree equal byte
  for byte as texture and as picture (2,705 files, 102 MB: 1,396 8-bit RGB, 32 palette, 1,277 RGBA); 8,115
  truncations (three cuts of each) with the same result and refusal text; 5,410 one-byte changes of image data:
  the 76 that decode are equal, the refusals now carry the crate's inflate text instead of stb's ("bad huffman
  code"), and 3 files the old decoder refused decode now. No golden has a damaged PNG of that kind.

### TrueType

- `ttf-parser`'s `Face` parses the font; code points go through `Face::glyph_index`; the horizontal metrics are
  read from hhea and hmtx directly (`Face::ascender` may switch to OS/2's typo metrics); simple glyphs come from
  `outline_glyph`, converted to stb's vertices with `floor` (ttf-parser walks a contour as stb does, and its
  midpoints are exact halves, as stb's `>> 1`).
- Kept: the rasterizer (pixels unchanged) and stb's walk of compound glyphs (stb scales a scaled component twice,
  ttf-parser does not: the test font's composite glyph would change).
- Five synthetic cmap and font-collection tests of stb's reader were dropped; the golden font tests are unchanged.
- Proof: old against new over the 12 Liberation fonts the game embeds and the test font: 14,456,832 code points,
  the metrics, box and vertices of all 30,631 glyphs, 609,740 bitmaps (U+0020..U+024F, the euro, trade mark and
  U+FFFD at the scales `font.rs` computes for heights 6 to 48), all identical.

### WON crypto and UTF-8

- `md5.rs`: the crate's `Md5Core` does the blocks and the padding; the C++ embeds the state by value, so `Md5`
  keeps its pending bytes and the core in storage of its size.
- `blowfish.rs`: RustCrypto's cipher; the embedded state is the key (64 bytes, was 4,232 with the schedule) and
  each Encrypt/Decrypt runs the key schedule. WON's framing (CBC from 0xff, the length, zero padding) stays.
- `crc16.rs`: `crc`'s CRC-16/UMTS (poly 0x8005, not reflected) with the caller's initial value and final XOR.
- `bigint.rs`: the magnitudes are `BigUint`; WON's sign rules and errors stay. Compared with the old code over
  80,000 random operand triples (every operation, signs and zeros) before it was deleted. `elgamal.rs` is kept.
- `utf8.rs`: decode, encode and the UTF-16 conversions through `str::from_utf8`, `char::encode_utf8`,
  `encode_utf16` and `char::decode_utf16`; the stray-byte model stays. `lossy()` keeps its loop (std replaces a
  truncated sequence with one U+FFFD, the game one per byte). Compared exhaustively before the old forms went.

### Looked at and not swapped in round 1

- **`ee_sim/src/ai/json.rs` -> serde_json (348 lines)**: the shipped `data/ai/ais.json` parses to the same tree,
  but the `ai::golden::manifest_and_seats` golden moves (its manifest section 1,758 -> 1,813 lines): serde's error
  texts instead of "line N: ...", and what it accepts (`5.`, nesting 32 vs 128, duplicate keys). Reverted; the diff
  is kept in the fork's `build/dp1a/ee_sim_json_deferred.diff`. Needs a re-baseline of that golden.
- **Thread wrappers -> std::thread**: the C interface does not allow it. `PlatThread*` is the Win32 thread handle
  (suspend, resume, terminate and priority use it), `PlatEvent*` the event handle (PlatTimerStartEvent signals
  it), the harness's time shim patches the Win32 wait imports, and `PlatMutex` is a recursive critical section the
  harness casts to.
- **Sockets -> std::net or socket2**: ee_won_net and ee_net make no raw socket calls of their own; all goes through
  `PlatSocket`. `socket_win32.rs` is one-line Winsock wrappers socket2 would only wrap again, and `socket_posix.rs`
  must keep its WSA error mapping and its event emulation.
- **`util/pool.rs` -> Vec or slab**: the pool reproduces the C++ chunk and free-index order, so objects land where
  the C++ put them, and the units' and contact lists' orders follow those addresses: a slab could move the oracle.

## Deferred until the port is done

Each of these changes a golden, a capture or behaviour on some platform, so it waits until the C++ is gone and a
one-time re-baseline is cheap.

- **The glyph rasterizer -> fontdue or ab_glyph** (the stb port left in ee_base `plat_ui/ttf.rs`): a new font
  golden and new pixels in every text capture.
- **jpeg.rs -> zune-jpeg**: another decoder's IDCT and upsampling need not match ours bit for bit: new decoder goldens.
- **SDL3 -> winit**: the window, input, full screen and the macOS behaviour (PORT_LEARNINGS "macOS: sound, full
  screen...") would all need proving again on three platforms.
- **`ee_sim/src/ai/json.rs` -> serde_json**: one golden re-baseline (above).
- **`util/pool.rs`**: only once nothing observes the allocation order (the units' and lists' orders).
- **Threads and sockets** over std: only once the harness no longer patches the Win32 imports and nothing casts to
  the platform objects.

Gates on the merged tip: PORT_LEARNINGS "The dependencies lane, round 1" (all pass; no golden re-baselined).
