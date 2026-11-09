//! `DX7Movie` (`Common/DX7Movie.cpp`): the playback thread's orchestration, the frame conversion loop, the mixer's sound
//! ring with linear resampling, the movie file name. The class stays C++ (`GEMovie`, the critical sections and events
//! of the thread system, `plm_t`, the frame buffer `GESurface`); every call out goes through [`MovieHost`].
//!
//! Golden: `golden_movie.txt`, printed by a scratch copy of the C++ ring functions (`DecodedSamples`, `MixSound`) and the
//! `.mpg` path substitution, compiled with clang-cl; the play-loop order is asserted against traces read off the C++.

/// `kSoundRingFloats`: about 0.7 s of stereo at 44.1 kHz.
pub(crate) const SOUND_RING_FLOATS: u32 = 2 * 32768;

/// `MAX_PATH` as the playback thread's `char thePath[MAX_PATH]` used it.
pub(crate) const MOVIE_MAX_PATH: usize = 260;

/// `PLM_AUDIO_SAMPLES_PER_FRAME` (`pl_mpeg.h`).
pub(crate) const PLM_AUDIO_SAMPLES_PER_FRAME: u32 = 1152;

/// `UMSetFloatingPointPrecision(0x10000)` / rounding 0 (kept while the movie runs).
pub(crate) const FP_PRECISION_DOUBLE: u32 = 0x10000;

/// The decoded sound waiting for the mixer (`mSound`, `mSoundRead`, `mSoundCount`, `mSoundPosition`, `mSoundStep`):
/// interleaved stereo at the movie's rate. C++ calls it under `mSoundAccess`.
#[derive(Debug)]
pub struct SoundRing {
    ring: Vec<f32>,
    read: u32,
    count: u32,
    position: f64,
    step: f64,
}

impl Default for SoundRing {
    fn default() -> Self {
        Self::new()
    }
}

impl SoundRing {
    pub fn new() -> Self {
        SoundRing { ring: vec![0.0; SOUND_RING_FLOATS as usize], read: 0, count: 0, position: 0.0, step: 0.0 }
    }

    /// `mSoundStep`: movie frames per device frame.
    pub fn set_step(&mut self, step: f64) {
        self.step = step;
    }

    pub fn count(&self) -> u32 {
        self.count
    }

    /// `DecodedSamples`: queue `inCount` interleaved stereo frames (`2 * inCount` floats); dropped when full.
    pub fn push(&mut self, samples: &[f32]) {
        let size = self.ring.len() as u32;
        for &sample in samples {
            if self.count >= size {
                break;
            }
            let index = (self.read + self.count) % size;
            self.ring[index as usize] = sample;
            self.count += 1;
        }
    }

    /// `MixSound`: add `frames` device frames to `out` (linear resampling); `out.len() >= 2 * frames`.
    pub fn mix(&mut self, out: &mut [f32], frames: usize) {
        let size = self.ring.len() as u32;
        let mut i = 0;
        while i < frames && self.count >= 4 {
            let a = self.read;
            let b = (a + 2) % size;
            let f = self.position as f32;
            let ring = &self.ring;
            out[2 * i] += ring[a as usize] + (ring[b as usize] - ring[a as usize]) * f;
            out[2 * i + 1] += ring[a as usize + 1] + (ring[b as usize + 1] - ring[a as usize + 1]) * f;
            self.position += self.step;
            while self.position >= 1.0 && self.count >= 2 {
                self.position -= 1.0;
                self.read = (self.read + 2) % size;
                self.count -= 2;
            }
            i += 1;
        }
    }
}

/// The converted movie's path: the game's name with its `.ext` replaced by `.mpg`, when the last dot is in the final
/// path part (no `\` after it) and the result fits `MAX_PATH`, as `PlayMovie` did with its fixed buffer.
pub fn mpg_path(name: &[u8]) -> Vec<u8> {
    let mut path: Vec<u8> = name.iter().copied().take(MOVIE_MAX_PATH - 1).collect();
    let dot = match path.iter().rposition(|&b| b == b'.') {
        Some(dot) => dot,
        None => return path,
    };
    if path[dot..].contains(&b'\\') || dot + 5 > MOVIE_MAX_PATH {
        return path;
    }
    path.truncate(dot);
    path.extend_from_slice(b".mpg");
    path
}

/// The C services the playback thread uses. In the game the `ee_dx7cpp_movie_*` trampolines around a `DX7Movie*`.
pub trait MovieHost {
    fn thread_begin(&mut self); // sMovieCriticalSection->Enter, SetPriority(0), UMSetFloatingPointPrecision(0x10000)
    fn open(&mut self, path: &[u8]) -> bool; // PlatPathGetAttributes, PlatPathToNative, plm_create_with_filename
    fn video_stream_count(&mut self) -> i32;
    fn width(&mut self) -> i32;
    fn height(&mut self) -> i32;
    fn create_frame_buffer(&mut self, width: i32, height: i32) -> i32;
    fn set_finished(&mut self);
    fn set_loop(&mut self, looping: bool);
    fn install_video_callback(&mut self);
    fn mixer_rate(&mut self) -> u32; // SEMixer::GetActiveMixer() ? GetOutputRate() : 0
    fn audio_stream_count(&mut self) -> i32;
    fn sample_rate(&mut self) -> i32;
    fn set_audio_enabled(&mut self, enabled: bool);
    fn install_audio_callback(&mut self, lead_time: f64);
    fn set_sound_step(&mut self, step: f64);
    fn set_mixer_source(&mut self, attach: bool);
    fn set_playing_started(&mut self); // mIsPlaying = true; mJustStarted = false
    fn counter_frequency(&mut self) -> i64;
    fn counter_ticks(&mut self) -> i64;
    fn should_stop(&mut self) -> bool;
    fn fp_precision(&mut self) -> u32;
    fn set_fp_precision(&mut self, precision: u32);
    fn fp_rounding(&mut self) -> u32;
    fn set_fp_rounding(&mut self, mode: u32);
    fn decode(&mut self, elapsed: f64);
    fn has_ended(&mut self) -> bool;
    fn sleep(&mut self, milliseconds: u32);
    fn frame_enter(&mut self); // mFrameAccess->Enter
    fn frame_leave(&mut self);
    fn clear_playing(&mut self); // mIsPlaying = false
    fn frame_ready_set(&mut self); // mFrameReadyEvent->Set
    fn release_frame_buffer(&mut self); // Dereference + NULL
    fn clear_just_started(&mut self);
    fn destroy(&mut self); // plm_destroy
    fn dereference(&mut self); // theMovie->Dereference
    fn thread_end(&mut self); // sMovieCriticalSection->Leave, mMovieStopped->Set
}

/// The playback thread (`DX7Movie::PlayMovie`): decodes in real time until stopped or finished, tears down the frame
/// buffer and the movie's reference, signals that it stopped.
pub fn play_movie<H: MovieHost>(host: &mut H, filename: &[u8], looping: bool) {
    host.thread_begin();
    let path = mpg_path(filename);
    let opened = host.open(&path);
    if opened && host.video_stream_count() > 0 {
        let (width, height) = (host.width(), host.height());
        if host.create_frame_buffer(width, height) != 0 {
            host.set_finished();
        }
        host.set_loop(looping);
        host.install_video_callback();
        let rate = host.mixer_rate();
        let sound = rate != 0 && host.audio_stream_count() > 0 && host.sample_rate() > 0;
        host.set_audio_enabled(sound);
        if sound {
            let samplerate = host.sample_rate();
            host.install_audio_callback(f64::from(PLM_AUDIO_SAMPLES_PER_FRAME) / f64::from(samplerate));
            host.set_sound_step(f64::from(samplerate) / f64::from(rate));
            host.set_mixer_source(true);
        }
        host.set_playing_started();
        let frequency = host.counter_frequency();
        let mut last = host.counter_ticks();
        while !host.should_stop() {
            if host.fp_precision() != FP_PRECISION_DOUBLE {
                host.set_fp_precision(FP_PRECISION_DOUBLE);
            }
            if host.fp_rounding() != 0 {
                host.set_fp_rounding(0);
            }
            let now = host.counter_ticks();
            let elapsed = (now - last) as f64 / frequency as f64;
            last = now;
            host.decode(if elapsed < 0.1 { elapsed } else { 0.1 });
            if !looping && host.has_ended() {
                host.set_finished();
            }
            host.sleep(1);
        }
        if sound {
            host.set_mixer_source(false); // the device thread no longer pulls after this returns
        }
        host.frame_enter();
        host.clear_playing();
        host.frame_ready_set();
        host.release_frame_buffer();
        host.frame_leave();
        host.sleep(100);
    }
    if opened {
        host.destroy();
    }
    host.clear_just_started();
    host.dereference();
    host.thread_end();
}

/// `DX7Movie::DecodedFrame`'s row loop: `pack_row(source row, destination row)` for `height` rows of `width` pixels
/// (`GEPackBGRXRow`, C++).
///
/// # Safety
/// `source` is `height` rows of `stride` bytes and `destination` `height` rows of `pitch` bytes.
pub unsafe fn convert_frame(
    source: *const u8,
    stride: u32,
    destination: *mut u8,
    pitch: u32,
    height: u32,
    mut pack_row: impl FnMut(*const u8, *mut u8),
) {
    for y in 0..height as usize {
        let (src, dst) = unsafe { (source.add(y * stride as usize), destination.add(y * pitch as usize)) };
        pack_row(src, dst);
    }
}

/// What `RenderFrame` asks of the frame buffer side.
pub trait RenderHost {
    fn just_started(&mut self) -> bool;
    fn is_playing(&mut self) -> bool;
    fn wait_frame_ready(&mut self);
    fn frame_enter(&mut self);
    fn frame_leave(&mut self);
    fn frame_buffer_present(&mut self) -> bool;
    fn has_rectangle(&mut self) -> bool;
    fn destination_rectangle(&mut self) -> [f32; 4];
    fn target_size(&mut self) -> (u32, u32);
    fn copy_frame(&mut self, destination: [f32; 4]) -> i32;
}

/// `DX7Movie::RenderFrame`: 7 once playback has ended, otherwise waits for a frame and copies it (stretched).
pub fn render_frame<H: RenderHost>(host: &mut H) -> i32 {
    if !host.just_started() && !host.is_playing() {
        return 7;
    }
    host.wait_frame_ready();
    host.frame_enter();
    let mut result = 0;
    if host.is_playing() && host.frame_buffer_present() {
        let destination = if host.has_rectangle() {
            host.destination_rectangle()
        } else {
            let (width, height) = host.target_size();
            [0.0, 0.0, width as i32 as f32, height as i32 as f32]
        };
        result = host.copy_frame(destination);
    }
    host.frame_leave();
    result
}

/// `GESurfaceDescription` for the movie's system-memory frame buffer (kSurfaceSystemMemory, the back buffer's format).
pub fn frame_buffer_description(
    pixel_format: u32,
    pixel_depth: u32,
    width: u32,
    height: u32,
) -> crate::api::SurfaceDescription {
    crate::api::SurfaceDescription {
        mType: crate::api::SURFACE_SYSTEM_MEMORY,
        mPixelFormat: core::ffi::c_ulong::from(pixel_format),
        mSurfaceVolatility: 0,
        mWidth: core::ffi::c_ulong::from(width),
        mHeight: core::ffi::c_ulong::from(height),
        m14: 0,
        mMipLevelCount: 0,
        mPixelDepth: core::ffi::c_ulong::from(pixel_depth),
        mIs3DRenderable: false,
        mIsTransparencyEnabled: false,
        mNoAlpha: false,
    }
}

#[cfg(test)]
#[allow(clippy::unnecessary_cast)]
mod tests {
    use super::*;

    fn fh(text: &str) -> f32 {
        f32::from_bits(u32::from_str_radix(text, 16).unwrap())
    }

    // golden_movie.txt: "mix" block with floats in hex bits, "path" lines.
    #[test]
    fn matches_cpp_golden() {
        let text = include_str!("golden_movie.txt");
        let mut lines = text.lines();
        let mut mix_cases = 0;
        let mut path_cases = 0;
        while let Some(line) = lines.next() {
            let mut parts = line.split(' ');
            match parts.next().unwrap() {
                "mix" => {
                    // mix step count_pushed frames : samples..
                    let step: f64 = f64::from_bits(u64::from_str_radix(parts.next().unwrap(), 16).unwrap());
                    let frames: usize = parts.next().unwrap().parse().unwrap();
                    let input: Vec<f32> = lines.next().unwrap().split(' ').map(fh).collect();
                    let expected: Vec<f32> = lines.next().unwrap().split(' ').map(fh).collect();
                    let state: Vec<&str> = lines.next().unwrap().split(' ').collect();
                    let mut ring = SoundRing::new();
                    ring.set_step(step);
                    ring.push(&input);
                    let mut out = vec![0.25f32; 2 * frames];
                    ring.mix(&mut out, frames);
                    let got: Vec<u32> = out.iter().map(|v| v.to_bits()).collect();
                    let want: Vec<u32> = expected.iter().map(|v| v.to_bits()).collect();
                    assert_eq!(got, want, "mix {mix_cases}");
                    assert_eq!(ring.read.to_string(), state[0], "read {mix_cases}");
                    assert_eq!(ring.count.to_string(), state[1], "count {mix_cases}");
                    assert_eq!(
                        ring.position.to_bits(),
                        u64::from_str_radix(state[2], 16).unwrap(),
                        "position {mix_cases}"
                    );
                    mix_cases += 1;
                }
                "path" => {
                    let rest = line.strip_prefix("path ").unwrap();
                    let (from, to) = rest.split_once(" => ").unwrap();
                    assert_eq!(String::from_utf8(mpg_path(from.as_bytes())).unwrap(), to, "{from}");
                    path_cases += 1;
                }
                _ => {}
            }
        }
        assert!(mix_cases >= 3, "mix cases {mix_cases}");
        assert!(path_cases >= 4, "path cases {path_cases}");
    }

    #[test]
    fn push_drops_when_full() {
        let mut ring = SoundRing::new();
        ring.push(&vec![1.0; SOUND_RING_FLOATS as usize + 10]);
        assert_eq!(ring.count(), SOUND_RING_FLOATS);
    }

    #[test]
    fn mix_stops_below_two_frames() {
        let mut ring = SoundRing::new();
        ring.set_step(1.0);
        ring.push(&[1.0, 2.0]);
        let mut out = [0.0f32; 4];
        ring.mix(&mut out, 2);
        assert_eq!(out, [0.0, 0.0, 0.0, 0.0]);
    }

    #[test]
    fn mpg_path_cases() {
        assert_eq!(mpg_path(b"C:\\dir\\intro.bik"), b"C:\\dir\\intro.mpg");
        assert_eq!(mpg_path(b"intro"), b"intro");
        assert_eq!(mpg_path(b"dir.v1\\intro"), b"dir.v1\\intro");
    }

    struct Tracer {
        log: Vec<String>,
        stops: u32,
        ticks: Vec<i64>,
        ended: bool,
        rate: u32,
        audio_streams: i32,
        open_ok: bool,
        frame_result: i32,
        fp_bad: bool,
    }

    impl Tracer {
        fn new() -> Self {
            Tracer {
                log: Vec::new(),
                stops: 1,
                ticks: vec![0, 1000],
                ended: false,
                rate: 44100,
                audio_streams: 1,
                open_ok: true,
                frame_result: 0,
                fp_bad: false,
            }
        }
        fn l(&mut self, s: &str) {
            self.log.push(s.to_string());
        }
    }

    impl MovieHost for Tracer {
        fn thread_begin(&mut self) {
            self.l("begin");
        }
        fn open(&mut self, path: &[u8]) -> bool {
            self.l(&format!("open {}", String::from_utf8_lossy(path)));
            self.open_ok
        }
        fn video_stream_count(&mut self) -> i32 {
            self.l("video_streams");
            1
        }
        fn width(&mut self) -> i32 {
            320
        }
        fn height(&mut self) -> i32 {
            240
        }
        fn create_frame_buffer(&mut self, w: i32, h: i32) -> i32 {
            self.l(&format!("create_fb {w} {h}"));
            self.frame_result
        }
        fn set_finished(&mut self) {
            self.l("finished");
        }
        fn set_loop(&mut self, l: bool) {
            self.l(&format!("loop {l}"));
        }
        fn install_video_callback(&mut self) {
            self.l("video_cb");
        }
        fn mixer_rate(&mut self) -> u32 {
            self.rate
        }
        fn audio_stream_count(&mut self) -> i32 {
            self.audio_streams
        }
        fn sample_rate(&mut self) -> i32 {
            22050
        }
        fn set_audio_enabled(&mut self, e: bool) {
            self.l(&format!("audio {e}"));
        }
        fn install_audio_callback(&mut self, lead: f64) {
            self.l(&format!("audio_cb {lead}"));
        }
        fn set_sound_step(&mut self, s: f64) {
            self.l(&format!("step {s}"));
        }
        fn set_mixer_source(&mut self, a: bool) {
            self.l(&format!("mixer {a}"));
        }
        fn set_playing_started(&mut self) {
            self.l("playing");
        }
        fn counter_frequency(&mut self) -> i64 {
            1000
        }
        fn counter_ticks(&mut self) -> i64 {
            if self.ticks.len() > 1 {
                self.ticks.remove(0)
            } else {
                self.ticks[0]
            }
        }
        fn should_stop(&mut self) -> bool {
            if self.stops == 0 {
                true
            } else {
                self.stops -= 1;
                false
            }
        }
        fn fp_precision(&mut self) -> u32 {
            if self.fp_bad {
                0
            } else {
                FP_PRECISION_DOUBLE
            }
        }
        fn set_fp_precision(&mut self, p: u32) {
            self.l(&format!("set_prec {p:x}"));
        }
        fn fp_rounding(&mut self) -> u32 {
            if self.fp_bad {
                1
            } else {
                0
            }
        }
        fn set_fp_rounding(&mut self, m: u32) {
            self.l(&format!("set_round {m}"));
        }
        fn decode(&mut self, e: f64) {
            self.l(&format!("decode {e}"));
        }
        fn has_ended(&mut self) -> bool {
            self.ended
        }
        fn sleep(&mut self, ms: u32) {
            self.l(&format!("sleep {ms}"));
        }
        fn frame_enter(&mut self) {
            self.l("frame_enter");
        }
        fn frame_leave(&mut self) {
            self.l("frame_leave");
        }
        fn clear_playing(&mut self) {
            self.l("not_playing");
        }
        fn frame_ready_set(&mut self) {
            self.l("frame_ready");
        }
        fn release_frame_buffer(&mut self) {
            self.l("release_fb");
        }
        fn clear_just_started(&mut self) {
            self.l("just_started_false");
        }
        fn destroy(&mut self) {
            self.l("destroy");
        }
        fn dereference(&mut self) {
            self.l("deref");
        }
        fn thread_end(&mut self) {
            self.l("end");
        }
    }

    #[test]
    fn play_movie_order_with_sound_one_iteration() {
        let mut t = Tracer::new();
        t.fp_bad = true;
        play_movie(&mut t, b"x.bik", false);
        assert_eq!(
            t.log,
            vec![
                "begin",
                "open x.mpg",
                "video_streams",
                "create_fb 320 240",
                "loop false",
                "video_cb",
                "audio true",
                "audio_cb 0.052244897959183675",
                "step 0.5",
                "mixer true",
                "playing",
                "set_prec 10000",
                "set_round 0",
                "decode 0.1",
                "sleep 1",
                "mixer false",
                "frame_enter",
                "not_playing",
                "frame_ready",
                "release_fb",
                "frame_leave",
                "sleep 100",
                "destroy",
                "just_started_false",
                "deref",
                "end",
            ]
        );
    }

    #[test]
    fn play_movie_open_failure_skips_the_loop_but_still_signals() {
        let mut t = Tracer::new();
        t.open_ok = false;
        play_movie(&mut t, b"x.bik", true);
        assert_eq!(t.log, vec!["begin", "open x.mpg", "just_started_false", "deref", "end"]);
    }

    #[test]
    fn play_movie_finishes_when_frame_buffer_fails_and_on_end() {
        let mut t = Tracer::new();
        t.frame_result = 1;
        t.audio_streams = 0;
        t.ended = true;
        t.stops = 1;
        play_movie(&mut t, b"x.bik", false);
        assert_eq!(t.log[3], "create_fb 320 240");
        assert_eq!(t.log[4], "finished");
        assert!(t.log.iter().any(|l| l == "audio false"));
        assert!(!t.log.iter().any(|l| l == "mixer true"));
        assert!(t.log.iter().any(|l| l == "finished"));
    }

    struct Render {
        log: Vec<String>,
        just: bool,
        playing: bool,
        fb: bool,
        rect: bool,
    }
    impl RenderHost for Render {
        fn just_started(&mut self) -> bool {
            self.just
        }
        fn is_playing(&mut self) -> bool {
            self.playing
        }
        fn wait_frame_ready(&mut self) {
            self.log.push("wait".into());
        }
        fn frame_enter(&mut self) {
            self.log.push("enter".into());
        }
        fn frame_leave(&mut self) {
            self.log.push("leave".into());
        }
        fn frame_buffer_present(&mut self) -> bool {
            self.fb
        }
        fn has_rectangle(&mut self) -> bool {
            self.rect
        }
        fn destination_rectangle(&mut self) -> [f32; 4] {
            [1.0, 2.0, 3.0, 4.0]
        }
        fn target_size(&mut self) -> (u32, u32) {
            (640, 480)
        }
        fn copy_frame(&mut self, d: [f32; 4]) -> i32 {
            self.log.push(format!("copy {d:?}"));
            5
        }
    }

    #[test]
    fn render_frame_paths() {
        let mut r = Render { log: vec![], just: false, playing: false, fb: true, rect: false };
        assert_eq!(render_frame(&mut r), 7);
        assert!(r.log.is_empty());
        let mut r = Render { log: vec![], just: true, playing: true, fb: true, rect: false };
        assert_eq!(render_frame(&mut r), 5);
        assert_eq!(r.log, vec!["wait", "enter", "copy [0.0, 0.0, 640.0, 480.0]", "leave"]);
        let mut r = Render { log: vec![], just: false, playing: true, fb: true, rect: true };
        assert_eq!(render_frame(&mut r), 5);
        assert_eq!(r.log[2], "copy [1.0, 2.0, 3.0, 4.0]");
        let mut r = Render { log: vec![], just: false, playing: false, fb: false, rect: false };
        r.just = true;
        assert_eq!(render_frame(&mut r), 0);
        assert_eq!(r.log, vec!["wait", "enter", "leave"]);
    }

    #[test]
    fn convert_frame_rows_step_by_stride_and_pitch() {
        let src = [0u8; 100];
        let mut dst = [0u8; 200];
        let mut rows = Vec::new();
        unsafe {
            convert_frame(src.as_ptr(), 10, dst.as_mut_ptr(), 20, 3, |s, d| {
                rows.push(((s as usize) - (src.as_ptr() as usize), (d as usize) - (dst.as_ptr() as usize)));
            })
        };
        assert_eq!(rows, vec![(0, 0), (10, 20), (20, 40)]);
    }

    #[test]
    fn frame_buffer_description_is_system_memory() {
        let d = frame_buffer_description(7, 16, 320, 240);
        assert_eq!(d.mType, crate::api::SURFACE_SYSTEM_MEMORY);
        assert_eq!((d.mWidth as u64, d.mHeight as u64, d.mPixelFormat as u64, d.mPixelDepth as u64), (320, 240, 7, 16));
        assert!(!d.mIs3DRenderable);
    }
}
