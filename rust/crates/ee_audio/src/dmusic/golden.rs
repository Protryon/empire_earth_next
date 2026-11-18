//! The music engine against dmusic: `tests/sound/dm_golden.c` ran dmusic's own performance with its synthesizer
//! replaced by a logger (every call it made into its synthesizer, at the frame it landed on) over a scenario; here
//! the same scenario runs on the Rust performance with [`Log`] as its synthesizer, and the lines must be the same.
//!
//! - `golden_dmusic.txt`: the scenario over the synthetic test songs in `testdata/` (made by
//!   `tests/sound/dm_testsong.py`; our own content).
//! - The game's songs (retail, not in git): `EE_DMUSIC_GOLDEN=<the C driver's output folder>
//!   EE_DMUSIC_MUSIC=<the folders with the .sgt/.sty and the .sf2, ';'-separated> cargo test -p ee_audio --lib
//!   dmusic::golden -- --nocapture` compares `c_songs.txt` and each `c_<Song>.txt` there.

use std::fmt::Write as _;
use std::sync::{Arc, Mutex};

use super::files::{Band, Segment};
use super::loader::Loader;
use super::perf::{Performance, TIMING_INSTANT, TIMING_MEASURE};
use super::synth::Sink;

/// The synthesizer as a log, as the C driver printed it.
struct Log {
    frame: Arc<Mutex<u64>>,
    out: Arc<Mutex<String>>,
}

impl Log {
    fn line(&self, s: std::fmt::Arguments) {
        let frame = *self.frame.lock().unwrap();
        let mut out = self.out.lock().unwrap();
        let _ = writeln!(out, "{frame} {s}");
    }
    fn band(&self, what: &str, band: &Band) {
        let mut s = format!("{what} {}", band.instruments.len());
        for i in &band.instruments {
            let _ = write!(
                s,
                " [{} {:08x} {:x} {} {} {} {} {}]",
                i.channel,
                i.patch,
                i.options,
                i.pan,
                i.volume,
                i.transpose,
                i.file().unwrap_or("-"),
                i32::from(i.collection().is_some())
            );
        }
        self.line(format_args!("{s}"));
    }
}

fn bits(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

impl Sink for Log {
    fn reset(&mut self) {
        self.line(format_args!("reset"));
    }
    fn set_volume(&mut self, volume: f32) {
        self.line(format_args!("volume {}", bits(volume)));
    }
    fn prepare_fonts(&mut self, band: &Band) {
        self.band("fonts", band);
    }
    fn band_update(&mut self, band: &Band) {
        self.band("band", band);
    }
    fn control(&mut self, channel: u32, control: u8, value: f32) {
        self.line(format_args!("cc {channel} {control} {}", bits(value)));
    }
    fn control_reset(&mut self, channel: u32, control: u8, reset: f32) {
        self.line(format_args!("ccreset {channel} {control} {}", bits(reset)));
    }
    fn pitch_bend(&mut self, channel: u32, bend: i32) {
        self.line(format_args!("bend {channel} {bend}"));
    }
    fn pitch_bend_reset(&mut self, channel: u32, reset: i32) {
        self.line(format_args!("bendreset {channel} {reset}"));
    }
    fn note_on(&mut self, channel: u32, note: u8, velocity: u8) {
        self.line(format_args!("on {channel} {note} {velocity}"));
    }
    fn note_off(&mut self, channel: u32, note: u8) {
        self.line(format_args!("off {channel} {note}"));
    }
    fn note_off_everything(&mut self) {
        self.line(format_args!("offeverything"));
    }
    fn render(&mut self, out: &mut [f32]) {
        out.fill(0.0);
        *self.frame.lock().unwrap() += out.len() as u64 / 2;
    }
}

struct Run {
    frame: Arc<Mutex<u64>>,
    out: Arc<Mutex<String>>,
    perf: Performance<Log>,
    loader: Loader,
    block: usize,
}

impl Run {
    fn new(folders: Vec<std::path::PathBuf>) -> Run {
        let frame = Arc::new(Mutex::new(0u64));
        let out = Arc::new(Mutex::new(String::new()));
        let log = Log { frame: frame.clone(), out: out.clone() };
        log.line(format_args!("init 48000"));
        let loader = Loader::default();
        let (f, o) = (frame.clone(), out.clone());
        loader.add_resolver(Box::new(move |name: &str| {
            let shown = super::loader::collection_file(name); // (a no-op; the .sf2 is shown as the .dls it was)
            let shown = match shown.strip_suffix(".sf2") {
                Some(stem) => format!("{stem}.dls"),
                None => shown,
            };
            let bytes = folders
                .iter()
                .find_map(|d| std::fs::read(d.join(name)).ok())
                .or_else(|| name.eq_ignore_ascii_case("gm.sf2").then(|| super::GENERAL_MIDI.to_vec()));
            let frame = *f.lock().unwrap();
            let _ = match &bytes {
                Some(_) => writeln!(o.lock().unwrap(), "{frame} resolve {shown}"),
                None => writeln!(o.lock().unwrap(), "{frame} resolve {shown} missing"),
            };
            bytes
        }));
        let mut perf = Performance::new(48000, log);
        perf.set_reverb(true, 0.0, -10.0, 1000.0, 0.001);
        Run { frame, out, perf, loader, block: 0 }
    }

    fn frame(&self) -> u64 {
        *self.frame.lock().unwrap()
    }

    fn render_seconds(&mut self, seconds: f64) {
        const BLOCKS: [usize; 4] = [480, 512, 128, 1031];
        let mut buf = vec![0f32; 2 * 1031];
        let end = self.frame() + (seconds * 48000.0) as u64;
        while self.frame() < end {
            let n = BLOCKS[self.block % 4];
            self.block += 1;
            self.perf.render(&mut buf[..2 * n]);
        }
    }

    fn load(&mut self, name: &str) -> Option<Arc<Segment>> {
        let r = self.loader.segment(name);
        let code = match &r {
            Ok(_) => 0,
            Err(super::loader::Error::InvalidArgument) => 1,
            Err(super::loader::Error::NotFound) => 2,
            Err(super::loader::Error::FileCorrupt) => 3,
        };
        let frame = self.frame();
        let _ = writeln!(self.out.lock().unwrap(), "{frame} load {name} {code}");
        let s = r.ok()?;
        s.repeats.store(1000, std::sync::atomic::Ordering::SeqCst);
        Some(s)
    }

    /// dm_golden.c's one-song scenario.
    fn one_song(&mut self, name: &str) {
        let a = self.load(name);
        self.perf.set_volume(0.0);
        self.perf.set_groove_level(0);
        self.perf.play_segment(a, TIMING_MEASURE);
        for i in 1..=20 {
            self.perf.set_volume(i as f32 / 20.0);
            self.render_seconds(0.1);
        }
        self.render_seconds(60.0);
        self.perf.set_groove_level(2);
        self.render_seconds(60.0);
        self.perf.set_groove_level(5);
        self.render_seconds(60.0);
        self.perf.set_groove_level(0);
        self.render_seconds(30.0);
        self.perf.stop(200);
        self.render_seconds(3.0);
    }

    /// dm_golden.c's every-song scenario over `songs`.
    fn songs(&mut self, songs: &[&str]) {
        let s: Vec<Option<Arc<Segment>>> = songs.iter().map(|n| self.load(n)).collect();
        self.perf.set_volume(0.75);
        self.perf.play_segment(s[0].clone(), TIMING_MEASURE);
        self.render_seconds(25.0);
        for (i, si) in s.iter().enumerate().skip(1) {
            self.perf.set_groove_level((i % 4) as u8);
            if i % 3 == 0 {
                self.perf.stop(200);
                self.render_seconds(1.5);
                self.perf.play_segment(si.clone(), TIMING_INSTANT);
            } else if let Some(si) = si {
                self.perf.play_transition(si.clone(), (1 + i % 6) as i32, TIMING_MEASURE);
            }
            self.render_seconds(20.0 + i as f64);
        }
        self.perf.play_segment(None, TIMING_MEASURE);
        self.render_seconds(4.0);
        self.perf.play_segment(s[4 % s.len()].clone(), TIMING_MEASURE);
        self.perf.stop(200);
        self.render_seconds(4.0);
    }

    fn text(&self) -> String {
        self.out.lock().unwrap().clone()
    }
}

/// The lines of a C log as this replay prints them: a resolve line without its size.
fn normalized(c: &str) -> Vec<String> {
    c.lines()
        .map(|l| {
            let t: Vec<&str> = l.split(' ').collect();
            if t.len() >= 2 && t[1] == "resolve" && t.last() != Some(&"missing") {
                // "<frame> resolve <name with spaces> <size>"
                t[..t.len() - 1].join(" ")
            } else {
                l.to_string()
            }
        })
        .collect()
}

fn compare(what: &str, want: &str, got: &str) {
    if let Ok(dir) = std::env::var("EE_DMUSIC_DUMP") {
        // the replay's lines, to diff against the C's
        let _ = std::fs::write(std::path::Path::new(&dir).join(format!("rust_{what}.txt")), got);
    }
    let want = normalized(want);
    let got: Vec<&str> = got.lines().collect();
    for (i, (w, g)) in want.iter().zip(got.iter()).enumerate() {
        assert_eq!(w, g, "{what}: line {} differs", i + 1);
    }
    assert_eq!(want.len(), got.len(), "{what}: line counts");
}

const SONGS: [&str; 8] =
    ["Honor.sgt", "Balance.sgt", "Enmity.sgt", "Fallen.sgt", "Pawns.sgt", "Rivers.sgt", "Shadow2.sgt", "Time.sgt"];

#[test]
fn the_test_songs_play_as_dmusic_played_them() {
    let dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("src/dmusic/testdata");
    let golden = include_str!("golden_dmusic.txt");
    let mut sections = golden.split("== ");
    let _ = sections.next();
    for section in sections {
        let (head, body) = section.split_once('\n').unwrap();
        let mut run = Run::new(vec![dir.clone()]);
        if head == "songs" {
            run.songs(&["test1.sgt", "test2.sgt", "test1.sgt", "test2.sgt", "test1.sgt"]);
        } else {
            run.one_song(head);
        }
        compare(head, body, &run.text());
    }
}

#[test]
fn the_games_songs_play_as_dmusic_played_them() {
    let (Ok(golden), Ok(music)) = (std::env::var("EE_DMUSIC_GOLDEN"), std::env::var("EE_DMUSIC_MUSIC")) else {
        eprintln!("EE_DMUSIC_GOLDEN / EE_DMUSIC_MUSIC not set: the game's songs are not compared");
        return;
    };
    let folders: Vec<std::path::PathBuf> = music.split(';').map(Into::into).collect();
    let golden = std::path::PathBuf::from(golden);
    let want = std::fs::read_to_string(golden.join("c_songs.txt")).unwrap();
    let mut run = Run::new(folders.clone());
    run.songs(&SONGS);
    compare("songs", &want, &run.text());
    for song in SONGS {
        let stem = song.trim_end_matches(".sgt");
        let want = std::fs::read_to_string(golden.join(format!("c_{stem}.txt"))).unwrap();
        let mut run = Run::new(folders.clone());
        run.one_song(song);
        compare(song, &want, &run.text());
        eprintln!("{song}: {} lines equal", want.lines().count());
    }
}

/// Not a comparison: the game's songs through rustysynth into WAV files, for listening (`EE_DMUSIC_RENDER=<out folder>`
/// with `EE_DMUSIC_MUSIC` as above): 40 s each, groove level 2 from the middle, the reverb at its defaults; the peak
/// and RMS are printed.
#[test]
fn render_the_games_songs() {
    let (Ok(out), Ok(music)) = (std::env::var("EE_DMUSIC_RENDER"), std::env::var("EE_DMUSIC_MUSIC")) else {
        return;
    };
    let folders: Vec<std::path::PathBuf> = music.split(';').map(Into::into).collect();
    for song in ["Honor.sgt", "Pawns.sgt", "Enmity.sgt"] {
        let loader = Loader::default();
        let f = folders.clone();
        loader.add_resolver(Box::new(move |name: &str| {
            f.iter()
                .find_map(|d| std::fs::read(d.join(name)).ok())
                .or_else(|| name.eq_ignore_ascii_case("gm.sf2").then(|| super::GENERAL_MIDI.to_vec()))
        }));
        let mut p = Performance::new(48000, super::synth::Synth::new(48000));
        p.set_reverb(true, 0.0, -10.0, 1000.0, 0.001);
        let s = loader.segment(song).unwrap();
        s.repeats.store(100, std::sync::atomic::Ordering::SeqCst);
        p.set_volume(1.0);
        p.play_segment(Some(s), TIMING_MEASURE);
        let frames = 40 * 48000;
        let mut buf = vec![0f32; 2 * frames];
        for at in (0..frames).step_by(512) {
            if at == frames / 2 {
                p.set_groove_level(2);
            }
            let n = (frames - at).min(512);
            p.render(&mut buf[2 * at..2 * (at + n)]);
        }
        let mut wav = Vec::new();
        let data = (frames * 4) as u32;
        wav.extend_from_slice(b"RIFF");
        wav.extend_from_slice(&(36 + data).to_le_bytes());
        wav.extend_from_slice(b"WAVEfmt ");
        for v in [16u32, 0x0002_0001, 48000, 48000 * 4, 0x0010_0004] {
            wav.extend_from_slice(&v.to_le_bytes());
        }
        wav.extend_from_slice(b"data");
        wav.extend_from_slice(&data.to_le_bytes());
        let (mut sum, mut peak) = (0f64, 0f32);
        for v in &buf {
            sum += f64::from(v * v);
            peak = peak.max(v.abs());
            wav.extend_from_slice(&((v.clamp(-1.0, 1.0) * 32767.0) as i16).to_le_bytes());
        }
        let stem = song.trim_end_matches(".sgt");
        std::fs::write(std::path::Path::new(&out).join(format!("after_{stem}.wav")), wav).unwrap();
        eprintln!("{song} peak {peak:.3} rms {:.4}", (sum / buf.len() as f64).sqrt());
    }
}
