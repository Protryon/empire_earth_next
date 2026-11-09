//! Channels and mixing, as the C++ `Mixer.cpp` (linear interpolation, per-side gains, loop by `fmod`, the master
//! volume over the effects, the music and the movie added after the effects' gain).

use std::ffi::{c_ulong, c_void};
use std::sync::{Arc, Mutex};

/// A channel's "no sample" value of `SESample::GetMixerChannel` (`-1`).
pub const CHANNEL_NONE: u32 = u32::MAX;

/// A sample as the channel layer sees it (the C++ `SESample*`).
pub type SampleHandle = *mut c_void;

/// The audio source callbacks of the conductor and the movie player: add interleaved stereo floats.
pub type SourceFn = unsafe extern "C" fn(context: *mut c_void, frames: *mut f32, count: c_ulong);

/// The `SESample` accessors the mixer uses.
pub trait SampleHost: Send + Sync + 'static {
    fn mixer_channel(&self, sample: SampleHandle) -> u32;
    fn set_mixer_channel(&self, sample: SampleHandle, channel: u32);
    fn data(&self, sample: SampleHandle) -> *const SampleData;
    fn is_looping(&self, sample: SampleHandle) -> bool;
    fn pan(&self, sample: SampleHandle) -> f32;
    fn pitch(&self, sample: SampleHandle) -> f32;
    fn volume(&self, sample: SampleHandle) -> f32;
    fn set_is_looping(&self, sample: SampleHandle, looping: bool);
    fn set_pan(&self, sample: SampleHandle, pan: f32);
    fn set_pitch(&self, sample: SampleHandle, pitch: f32);
    fn set_volume(&self, sample: SampleHandle, volume: f32);
}

/// Result codes of the SPIM entry points.
pub const OK: i32 = 0;
pub const NATIVE_ERROR: i32 = 1;
pub const CHANNEL_BUSY: i32 = 2;
pub const NO_SAMPLE: i32 = 3;
pub const CHANNEL_PLAYING: i32 = 5;
pub const CHANNEL_IDLE: i32 = 6;
pub const BAD_PARAMETER: i32 = 10;
pub const SAMPLE_ON_OTHER_CHANNEL: i32 = 13;

/// The most channels the game may create.
pub const MAXIMUM_CHANNELS: u32 = 32;

/// A loaded sample's decoded audio (reference counted across a sample and its duplicates).
pub struct SampleData {
    /// Interleaved, `channels` (1 or 2) per frame.
    pcm: Vec<f32>,
    pub frames: u32,
    pub channels: u32,
    pub rate: u32,
    /// Bytes per frame of the file's own format (offsets, granularity).
    pub frame_bytes: u32,
}

impl SampleData {
    /// Builds a sample from an interleaved decoded block of `file_channels` per frame, keeping the first two
    /// channels. `None` when there are no frames (the C++ `Decode`'s failure).
    pub fn new(file: &[u8], pcm: &[f32], file_channels: u32, rate: u32) -> Option<Arc<SampleData>> {
        let channels = file_channels.min(2);
        if channels == 0 {
            return None;
        }
        let frames = pcm.len() / file_channels as usize;
        let mut kept = Vec::with_capacity(frames * channels as usize);
        for frame in pcm.chunks_exact(file_channels as usize) {
            kept.extend_from_slice(&frame[..channels as usize]);
        }
        if kept.is_empty() {
            return None;
        }
        Some(Arc::new(SampleData {
            frames: frames as u32,
            channels,
            rate,
            frame_bytes: file_frame_bytes(file, file_channels),
            pcm: kept,
        }))
    }

    #[cfg(test)]
    fn tone(frames: u32, channels: u32, rate: u32) -> Arc<SampleData> {
        let pcm = (0..frames * channels).map(|i| (i64::from(i) * 7 % 200 - 100) as f32 / 128.0).collect::<Vec<f32>>();
        Arc::new(SampleData { pcm, frames, channels, rate, frame_bytes: channels * 2 })
    }
}

/// Bytes per frame of the file's own format: a WAV's block alignment, 2 bytes per channel otherwise.
pub fn file_frame_bytes(data: &[u8], channels: u32) -> u32 {
    if data.len() >= 12 && &data[..4] == b"RIFF" && &data[8..12] == b"WAVE" {
        let mut i = 12usize;
        while i + 8 <= data.len() {
            let size = u32::from_le_bytes([data[i + 4], data[i + 5], data[i + 6], data[i + 7]]);
            if &data[i..i + 4] == b"fmt " && i + 22 <= data.len() {
                return u32::from(data[i + 20]) | u32::from(data[i + 21]) << 8;
            }
            i = i.saturating_add(8usize.saturating_add(size as usize).saturating_add(size as usize & 1));
        }
    }
    2 * channels
}

#[derive(Clone, Default)]
struct Channel {
    sample: Option<usize>, // the SESample*, Copy across the device thread; never dereferenced here
    data: Option<Arc<SampleData>>,
    position: f64,
    step: f64,
    left: f32,
    right: f32,
    playing: bool,
    looping: bool,
}

fn gains(channel: &mut Channel, volume: f32, pan: f32) {
    let pan = pan.clamp(-1.0, 1.0);
    channel.left = volume * if pan > 0.0 { 1.0 - pan } else { 1.0 };
    channel.right = volume * if pan < 0.0 { 1.0 + pan } else { 1.0 };
}

fn step(channel: &mut Channel, pitch: f32, device_rate: u32) {
    channel.step = match &channel.data {
        Some(data) if device_rate != 0 => f64::from(data.rate) * f64::from(pitch) / f64::from(device_rate),
        _ => 0.0,
    };
}

struct State {
    channels: Vec<Channel>,
    volume: f32,
    master_volume: f32,
    muted: bool,
    maximum_channels: u32,
    device_rate: u32,
    active: bool,
}

struct Sources {
    music: Option<(SourceFn, usize)>,
    movie: Option<(SourceFn, usize)>,
    buffer: Vec<f32>,
}

/// The mixer: the channels and the volumes under one lock, the audio sources under their own (so that clearing a
/// source waits for the device thread to be out of it, while a source may call back into the mixer).
pub struct Mixer<H: SampleHost> {
    pub host: H,
    state: Mutex<State>,
    sources: Mutex<Sources>,
}

impl<H: SampleHost> Mixer<H> {
    pub fn new(host: H) -> Self {
        Mixer {
            host,
            state: Mutex::new(State {
                channels: Vec::new(),
                volume: 1.0,
                master_volume: 1.0,
                muted: false,
                maximum_channels: 0,
                device_rate: 0,
                active: false,
            }),
            sources: Mutex::new(Sources { music: None, movie: None, buffer: Vec::new() }),
        }
    }

    fn state(&self) -> std::sync::MutexGuard<'_, State> {
        self.state.lock().expect("no panics while locked")
    }

    /// The device opened at `rate`: the channel limit applies and the channels are created.
    pub fn activated(&self, rate: u32) {
        {
            let mut state = self.state();
            state.device_rate = rate;
            state.maximum_channels = MAXIMUM_CHANNELS;
            state.active = true;
        }
        self.create_channels(MAXIMUM_CHANNELS);
    }

    /// The device closed.
    pub fn deactivated(&self) {
        let mut state = self.state();
        state.maximum_channels = 0;
        state.active = false;
    }

    pub fn is_active(&self) -> bool {
        self.state().active
    }

    pub fn output_rate(&self) -> u32 {
        let state = self.state();
        if state.active {
            state.device_rate
        } else {
            0
        }
    }

    pub fn create_channels(&self, count: u32) -> i32 {
        let mut state = self.state();
        if !state.channels.is_empty() {
            return CHANNEL_BUSY;
        }
        let count = count.min(state.maximum_channels);
        state.channels = vec![Channel::default(); count as usize];
        OK
    }

    pub fn destroy_channels(&self) -> i32 {
        let mut state = self.state();
        let channels = std::mem::take(&mut state.channels);
        drop(state);
        for channel in &channels {
            if let Some(sample) = channel.sample {
                self.host.set_mixer_channel(sample as SampleHandle, CHANNEL_NONE);
            }
        }
        OK
    }

    pub fn channel_count(&self) -> u32 {
        self.state().channels.len() as u32
    }

    pub fn maximum_channel_count(&self) -> u32 {
        self.state().maximum_channels
    }

    pub fn is_channel_busy_playing(&self, channel: u32) -> i32 {
        let state = self.state();
        let Some(c) = state.channels.get(channel as usize) else {
            return BAD_PARAMETER;
        };
        if c.sample.is_some() && c.playing {
            CHANNEL_PLAYING
        } else {
            CHANNEL_IDLE
        }
    }

    pub fn play_channel(&self, channel: u32) -> i32 {
        let mut state = self.state();
        let Some(c) = state.channels.get_mut(channel as usize) else {
            return BAD_PARAMETER;
        };
        if c.sample.is_none() {
            return NO_SAMPLE;
        }
        if c.playing {
            return CHANNEL_BUSY;
        }
        if c.data.as_ref().is_some_and(|d| c.position >= f64::from(d.frames)) {
            c.position = 0.0;
        }
        c.playing = true;
        OK
    }

    /// Stops the channel and detaches its sample; `CHANNEL_BUSY` when the channel is empty.
    pub fn stop_channel(&self, channel: u32) -> i32 {
        let mut state = self.state();
        let Some(c) = state.channels.get_mut(channel as usize) else {
            return BAD_PARAMETER;
        };
        let Some(sample) = c.sample.take() else {
            return CHANNEL_BUSY;
        };
        c.data = None;
        c.playing = false;
        self.host.set_mixer_channel(sample as SampleHandle, CHANNEL_NONE);
        OK
    }

    /// Stops every channel; returns the last channel's result.
    pub fn stop_all_channels(&self) -> i32 {
        let mut result = OK;
        for channel in 0..self.channel_count() {
            result = self.stop_channel(channel);
        }
        result
    }

    /// Puts a sample on an idle channel and applies its loop, pan, pitch and volume; a NULL sample stops the channel.
    pub fn set_channel_sample(&self, channel: u32, sample: SampleHandle) -> i32 {
        let busy = self.is_channel_busy_playing(channel);
        if busy != CHANNEL_IDLE {
            return busy;
        }
        if sample.is_null() {
            self.stop_channel(channel);
            return OK;
        }
        let bound = self.host.mixer_channel(sample);
        if bound == channel {
            return OK;
        }
        if bound != CHANNEL_NONE {
            return SAMPLE_ON_OTHER_CHANNEL;
        }
        self.stop_channel(channel);
        {
            let mut state = self.state();
            let c = &mut state.channels[channel as usize];
            c.sample = Some(sample as usize);
            let data = self.host.data(sample);
            c.data = if data.is_null() {
                None
            } else {
                // SAFETY: the sample's data pointer is a live `Arc<SampleData>` raw pointer (see `sample_create`)
                unsafe {
                    Arc::increment_strong_count(data);
                    Some(Arc::from_raw(data))
                }
            };
            c.position = 0.0;
            c.playing = false;
        }
        self.host.set_mixer_channel(sample, channel);
        self.set_sample_looping(sample, self.host.is_looping(sample));
        self.set_sample_pan(sample, self.host.pan(sample));
        self.set_sample_pitch(sample, self.host.pitch(sample));
        self.set_sample_volume(sample, self.host.volume(sample));
        OK
    }

    fn channel_of<'a>(&self, state: &'a mut State, sample: SampleHandle) -> Option<&'a mut Channel> {
        let index = self.host.mixer_channel(sample);
        state.channels.get_mut(index as usize).filter(|c| c.sample == Some(sample as usize))
    }

    pub fn set_sample_looping(&self, sample: SampleHandle, looping: bool) {
        self.host.set_is_looping(sample, looping);
        let mut state = self.state();
        if let Some(c) = self.channel_of(&mut state, sample) {
            c.looping = looping;
        }
    }

    pub fn set_sample_pan(&self, sample: SampleHandle, pan: f32) -> i32 {
        self.host.set_pan(sample, pan);
        let volume = self.host.volume(sample);
        let mut state = self.state();
        if let Some(c) = self.channel_of(&mut state, sample) {
            gains(c, volume, pan);
        }
        OK
    }

    pub fn set_sample_pitch(&self, sample: SampleHandle, pitch: f32) -> i32 {
        self.host.set_pitch(sample, pitch);
        let mut state = self.state();
        let device_rate = state.device_rate;
        if let Some(c) = self.channel_of(&mut state, sample) {
            step(c, pitch, device_rate);
        }
        OK
    }

    pub fn set_sample_volume(&self, sample: SampleHandle, volume: f32) -> i32 {
        self.host.set_volume(sample, volume);
        let pan = self.host.pan(sample);
        let mut state = self.state();
        if let Some(c) = self.channel_of(&mut state, sample) {
            gains(c, volume, pan);
        }
        OK
    }

    /// The playback position in bytes of the file's format (0 when it has no channel).
    pub fn sample_offset(&self, sample: SampleHandle) -> u32 {
        let mut state = self.state();
        match self.channel_of(&mut state, sample) {
            Some(c) if c.data.is_some() => {
                let data = c.data.as_ref().expect("checked just above");
                (c.position as u32).wrapping_mul(data.frame_bytes)
            }
            _ => 0,
        }
    }

    pub fn set_sample_offset(&self, sample: SampleHandle, offset: u32) -> i32 {
        let mut state = self.state();
        if let Some(c) = self.channel_of(&mut state, sample) {
            if let Some(data) = c.data.as_ref().filter(|d| d.frame_bytes != 0) {
                c.position = f64::from(offset / data.frame_bytes);
            }
        }
        OK
    }

    /// The sample's total and remaining play time in ms, `NATIVE_ERROR` and zeros when it has no channel.
    pub fn sample_play_back_time(&self, sample: SampleHandle) -> (i32, i32, i32) {
        let mut state = self.state();
        match self.channel_of(&mut state, sample) {
            Some(c) => match c.data.as_ref().filter(|d| d.rate != 0) {
                Some(data) => {
                    let rate = f64::from(data.rate);
                    let total = (f64::from(data.frames) * 1000.0 / rate) as i32;
                    let position = (c.position * 1000.0 / rate) as i32;
                    (OK, total, if position < total { total - position } else { 0 })
                }
                None => (NATIVE_ERROR, 0, 0),
            },
            None => (NATIVE_ERROR, 0, 0),
        }
    }

    pub fn volume(&self) -> f32 {
        self.state().volume
    }

    pub fn set_volume(&self, volume: f32) -> i32 {
        if !(0.0..=1.0).contains(&volume) {
            return BAD_PARAMETER;
        }
        self.state().volume = volume;
        OK
    }

    pub fn set_master_volume(&self, volume: f32) {
        self.state().master_volume = volume.clamp(0.0, 1.0);
    }

    pub fn set_muted(&self, muted: bool) {
        self.state().muted = muted;
    }

    pub fn set_music_source(&self, source: Option<SourceFn>, context: *mut c_void) {
        self.sources.lock().expect("no panics while locked").music = source.map(|s| (s, context as usize));
    }

    pub fn set_movie_source(&self, source: Option<SourceFn>, context: *mut c_void) {
        self.sources.lock().expect("no panics while locked").movie = source.map(|s| (s, context as usize));
    }

    /// The device thread: mixes every playing channel (linear interpolation), then the music and the movie.
    pub fn mix(&self, out: &mut [f32]) {
        out.fill(0.0);
        let frames = out.len() / 2;
        let (volume, master);
        {
            let mut state = self.state();
            for channel in state.channels.iter_mut() {
                let Some(data) = channel.data.clone() else {
                    continue;
                };
                if !channel.playing || data.frames == 0 || channel.step <= 0.0 {
                    continue;
                }
                render_channel(channel, &data, out, frames);
            }
            volume = state.volume;
            master = if state.muted { 0.0 } else { state.master_volume };
        }
        let mut sources = self.sources.lock().expect("no panics while locked");
        let has_music = sources.music.is_some() || sources.movie.is_some();
        if has_music {
            let sources = &mut *sources;
            if sources.buffer.len() < out.len() {
                sources.buffer.resize(out.len(), 0.0);
            }
            sources.buffer[..out.len()].fill(0.0);
            if let Some((source, context)) = sources.music {
                // SAFETY: the conductor's contract: its render callback adds `frames` stereo frames
                unsafe { source(context as *mut c_void, sources.buffer.as_mut_ptr(), frames as c_ulong) };
            }
            if let Some((source, context)) = sources.movie {
                // SAFETY: the movie player's contract, as above
                unsafe { source(context as *mut c_void, sources.buffer.as_mut_ptr(), frames as c_ulong) };
            }
        }
        for (i, sample) in out.iter_mut().enumerate() {
            let music = if has_music { sources.buffer[i] } else { 0.0 };
            let v = (*sample * volume + music) * master;
            *sample = v.clamp(-1.0, 1.0);
        }
    }
}

fn render_channel(channel: &mut Channel, data: &SampleData, out: &mut [f32], frames: usize) {
    let width = data.channels as usize;
    let total = f64::from(data.frames);
    for i in 0..frames {
        if channel.position >= total {
            if !channel.looping {
                channel.playing = false;
                break;
            }
            channel.position %= total;
        }
        let index = channel.position as usize;
        let fraction = (channel.position - index as f64) as f32;
        let next = if index + 1 < data.frames as usize {
            index + 1
        } else if channel.looping {
            0
        } else {
            index
        };
        let a = &data.pcm[index * width..];
        let b = &data.pcm[next * width..];
        let left = a[0] + (b[0] - a[0]) * fraction;
        let right = if width > 1 { a[1] + (b[1] - a[1]) * fraction } else { left };
        out[2 * i] += left * channel.left;
        out[2 * i + 1] += right * channel.right;
        channel.position += channel.step;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;
    use std::sync::Mutex as StdMutex;

    #[derive(Default)]
    struct SampleState {
        channel: u32,
        data: usize,
        looping: bool,
        pan: f32,
        pitch: f32,
        volume: f32,
    }

    #[derive(Default)]
    struct Mock {
        samples: StdMutex<HashMap<usize, SampleState>>,
    }

    impl SampleHost for Mock {
        fn mixer_channel(&self, s: SampleHandle) -> u32 {
            self.samples.lock().unwrap()[&(s as usize)].channel
        }
        fn set_mixer_channel(&self, s: SampleHandle, channel: u32) {
            self.samples.lock().unwrap().get_mut(&(s as usize)).unwrap().channel = channel;
        }
        fn data(&self, s: SampleHandle) -> *const SampleData {
            self.samples.lock().unwrap()[&(s as usize)].data as *const SampleData
        }
        fn is_looping(&self, s: SampleHandle) -> bool {
            self.samples.lock().unwrap()[&(s as usize)].looping
        }
        fn pan(&self, s: SampleHandle) -> f32 {
            self.samples.lock().unwrap()[&(s as usize)].pan
        }
        fn pitch(&self, s: SampleHandle) -> f32 {
            self.samples.lock().unwrap()[&(s as usize)].pitch
        }
        fn volume(&self, s: SampleHandle) -> f32 {
            self.samples.lock().unwrap()[&(s as usize)].volume
        }
        fn set_is_looping(&self, s: SampleHandle, looping: bool) {
            self.samples.lock().unwrap().get_mut(&(s as usize)).unwrap().looping = looping;
        }
        fn set_pan(&self, s: SampleHandle, pan: f32) {
            self.samples.lock().unwrap().get_mut(&(s as usize)).unwrap().pan = pan;
        }
        fn set_pitch(&self, s: SampleHandle, pitch: f32) {
            self.samples.lock().unwrap().get_mut(&(s as usize)).unwrap().pitch = pitch;
        }
        fn set_volume(&self, s: SampleHandle, volume: f32) {
            self.samples.lock().unwrap().get_mut(&(s as usize)).unwrap().volume = volume;
        }
    }

    fn mixer() -> Mixer<Mock> {
        let m = Mixer::new(Mock::default());
        m.activated(44100);
        m
    }

    fn sample(m: &Mixer<Mock>, id: usize, data: &Arc<SampleData>) -> SampleHandle {
        m.host.samples.lock().unwrap().insert(
            id,
            SampleState {
                channel: CHANNEL_NONE,
                data: Arc::as_ptr(data) as usize,
                looping: false,
                pan: 0.0,
                pitch: 1.0,
                volume: 1.0,
            },
        );
        id as SampleHandle
    }

    const GOLDEN: &str = include_str!("golden.txt");

    fn fnv(data: &[f32]) -> u64 {
        let mut h = 0xcbf29ce484222325u64;
        for value in data {
            for byte in value.to_le_bytes() {
                h = (h ^ u64::from(byte)).wrapping_mul(0x100000001b3);
            }
        }
        h
    }

    fn check(m: &Mixer<Mock>, tag: &str) {
        let mut out = vec![0.0f32; 512];
        m.mix(&mut out);
        let t: Vec<&str> =
            GOLDEN.lines().find(|l| l.split('\t').next() == Some(tag)).unwrap().split('\t').collect::<Vec<&str>>();
        assert_eq!(format!("{:016x}", fnv(&out)), t[1], "{tag}");
        let state = m.state();
        for (c, field) in state.channels.iter().zip(&t[10..]) {
            let (position, playing) = field.split_once(',').unwrap();
            assert_eq!(format!("{:.6}", c.position), position, "{tag}");
            assert_eq!(c.playing, playing == "1", "{tag}");
        }
        for (i, field) in t[2..10].iter().enumerate() {
            assert_eq!(format!("{:08x}", out[i].to_bits()), *field, "{tag} sample {i}");
        }
    }

    fn bind(m: &Mixer<Mock>, c: usize, data: Arc<SampleData>, volume: f32, pan: f32, pitch: f32, looping: bool) {
        let mut state = m.state();
        let device_rate = state.device_rate;
        let channel = &mut state.channels[c];
        channel.sample = Some(1000 + c);
        channel.data = Some(data);
        channel.position = 0.0;
        channel.playing = true;
        channel.looping = looping;
        gains(channel, volume, pan);
        step(channel, pitch, device_rate);
    }

    fn golden_setup() -> Mixer<Mock> {
        let m = Mixer::new(Mock::default());
        m.activated(44100);
        // the tool mixes 4 channels; bind() replaces the first ones, the rest stay empty
        m.state().channels.truncate(4);
        m
    }

    #[test]
    fn mix_cases_match_the_cpp() {
        let m = golden_setup();
        bind(&m, 0, SampleData::tone(1000, 1, 22050), 1.0, 0.0, 1.0, false);
        check(&m, "mono-half-rate");

        let m = golden_setup();
        bind(&m, 0, SampleData::tone(1000, 2, 44100), 0.7, -0.5, 1.0, false);
        check(&m, "stereo-pan-left");

        let m = golden_setup();
        bind(&m, 0, SampleData::tone(50, 1, 44100), 1.0, 0.5, 1.25, true);
        check(&m, "mono-loop-short");

        let m = golden_setup();
        bind(&m, 0, SampleData::tone(50, 1, 44100), 1.0, 2.0, 1.0, false);
        check(&m, "mono-clamp-pan-ends-early");

        let m = golden_setup();
        bind(&m, 0, SampleData::tone(1000, 2, 48000), 1.0, -1.5, 0.5, true);
        bind(&m, 1, SampleData::tone(333, 1, 11025), 0.5, 0.0, 3.0, true);
        check(&m, "two-channels");

        let m = golden_setup();
        bind(&m, 0, SampleData::tone(1000, 1, 44100), 1.0, 0.0, 1.0, false);
        m.set_volume(0.5);
        m.set_master_volume(0.8);
        check(&m, "volumes");

        let m = golden_setup();
        bind(&m, 0, SampleData::tone(1000, 1, 44100), 1.0, 0.0, 1.0, false);
        m.set_muted(true);
        check(&m, "muted");

        let m = golden_setup();
        bind(&m, 0, SampleData::tone(20, 1, 44100), 0.25, 0.0, 1.0, false);
        m.state().channels[0].position = 19.5;
        check(&m, "end-interp");
    }

    #[test]
    fn file_frame_bytes_matches_the_cpp() {
        let t: Vec<&str> = GOLDEN.lines().find(|l| l.starts_with("fileframebytes")).unwrap().split('\t').collect();
        let mut wav = [0u8; 44];
        wav[..4].copy_from_slice(b"RIFF");
        wav[8..12].copy_from_slice(b"WAVE");
        wav[12..16].copy_from_slice(b"fmt ");
        wav[16] = 16;
        wav[20] = 1;
        wav[22] = 2;
        wav[32] = 4;
        wav[34] = 16;
        wav[36..40].copy_from_slice(b"data");
        assert_eq!(file_frame_bytes(&wav, 7).to_string(), t[1]);
        wav[32] = 6;
        assert_eq!(file_frame_bytes(&wav, 7).to_string(), t[2]);
        assert_eq!(file_frame_bytes(&wav[..11], 7).to_string(), t[3]);
        assert_eq!(file_frame_bytes(b"OGGSsomething", 3).to_string(), t[4]);
        let mut wav2 = [0u8; 40];
        wav2[..4].copy_from_slice(b"RIFF");
        wav2[8..12].copy_from_slice(b"WAVE");
        wav2[12..16].copy_from_slice(b"junk");
        wav2[16] = 3;
        wav2[20..23].copy_from_slice(&[9, 9, 9]);
        wav2[24..28].copy_from_slice(b"fmt ");
        wav2[28] = 16;
        wav2[32] = 0;
        assert_eq!(file_frame_bytes(&wav2, 2).to_string(), t[5]);
    }

    #[test]
    fn set_channel_sample_binds_and_applies_the_sample_settings() {
        let m = mixer();
        let data = SampleData::tone(100, 1, 22050);
        let s = sample(&m, 1, &data);
        m.host.samples.lock().unwrap().get_mut(&1).unwrap().pitch = 2.0;
        assert_eq!(m.set_channel_sample(3, s), OK);
        assert_eq!(m.host.mixer_channel(s), 3);
        let state = m.state();
        let c = &state.channels[3];
        assert!(!c.playing);
        assert_eq!(c.step, f64::from(22050u32) * 2.0 / 44100.0);
        drop(state);
        assert_eq!(m.set_channel_sample(3, s), OK, "already on this channel");
        assert_eq!(m.set_channel_sample(4, s), SAMPLE_ON_OTHER_CHANNEL);
        assert_eq!(m.set_channel_sample(99, s), BAD_PARAMETER);
    }

    #[test]
    fn null_sample_stops_and_busy_channels_refuse() {
        let m = mixer();
        let data = SampleData::tone(100, 1, 44100);
        let s = sample(&m, 2, &data);
        assert_eq!(m.set_channel_sample(0, s), OK);
        assert_eq!(m.play_channel(0), OK);
        assert_eq!(m.set_channel_sample(0, s), CHANNEL_PLAYING);
        assert_eq!(m.stop_channel(0), OK);
        assert_eq!(m.stop_channel(0), CHANNEL_BUSY, "already empty");
        assert_eq!(m.host.mixer_channel(s), CHANNEL_NONE);
        assert_eq!(m.set_channel_sample(0, std::ptr::null_mut()), OK);
    }

    #[test]
    fn play_channel_codes() {
        let m = mixer();
        assert_eq!(m.play_channel(99), BAD_PARAMETER);
        assert_eq!(m.play_channel(0), NO_SAMPLE);
        let data = SampleData::tone(10, 1, 44100);
        let s = sample(&m, 3, &data);
        m.set_channel_sample(0, s);
        assert_eq!(m.play_channel(0), OK);
        assert_eq!(m.play_channel(0), CHANNEL_BUSY);
        // a finished channel restarts from the start
        m.stop_channel(0);
        let m = mixer();
        let s = sample(&m, 3, &data);
        m.set_channel_sample(0, s);
        m.state().channels[0].position = 11.0;
        m.state().channels[0].playing = false;
        assert_eq!(m.play_channel(0), OK);
        assert_eq!(m.state().channels[0].position, 0.0);
    }

    #[test]
    fn busy_state_of_a_channel() {
        let m = mixer();
        assert_eq!(m.is_channel_busy_playing(99), BAD_PARAMETER);
        assert_eq!(m.is_channel_busy_playing(0), CHANNEL_IDLE);
        let data = SampleData::tone(10, 1, 44100);
        let s = sample(&m, 4, &data);
        m.set_channel_sample(0, s);
        assert_eq!(m.is_channel_busy_playing(0), CHANNEL_IDLE);
        m.play_channel(0);
        assert_eq!(m.is_channel_busy_playing(0), CHANNEL_PLAYING);
    }

    #[test]
    fn sample_setters_update_their_channel() {
        let m = mixer();
        let data = SampleData::tone(10, 1, 44100);
        let s = sample(&m, 5, &data);
        m.set_channel_sample(1, s);
        assert_eq!(m.set_sample_pan(s, 0.5), OK);
        assert_eq!(m.host.pan(s), 0.5);
        m.set_sample_volume(s, 0.5);
        let state = m.state();
        let c = &state.channels[1];
        assert_eq!((c.left, c.right), (0.25, 0.5));
        drop(state);
        m.set_sample_looping(s, true);
        assert!(m.host.is_looping(s) && m.state().channels[1].looping);
        m.set_sample_pitch(s, 0.5);
        assert_eq!(m.state().channels[1].step, 0.5);

        // a sample not on any channel only updates itself
        let s2 = sample(&m, 6, &data);
        m.set_sample_pan(s2, 0.1);
        assert_eq!(m.host.pan(s2), 0.1);
    }

    #[test]
    fn offsets_and_play_times() {
        let m = mixer();
        let data = SampleData::tone(44100, 1, 44100);
        let s = sample(&m, 7, &data);
        m.set_channel_sample(0, s);
        assert_eq!(m.set_sample_offset(s, 4 * 2 + 1), OK);
        assert_eq!(m.sample_offset(s), 4 * 2, "the offset is whole frames of the file's format");
        assert_eq!(m.sample_play_back_time(s), (OK, 1000, 1000)); // 4 frames at 44.1 kHz round to 0 ms
                                                                  // no channel: zeros and an error
        let s2 = sample(&m, 8, &data);
        assert_eq!(m.sample_offset(s2), 0);
        assert_eq!(m.sample_play_back_time(s2), (NATIVE_ERROR, 0, 0));
    }

    #[test]
    fn volumes_and_output_rate() {
        let m = Mixer::new(Mock::default());
        assert_eq!(m.output_rate(), 0);
        assert_eq!(m.create_channels(4), OK);
        assert_eq!(m.channel_count(), 0, "no maximum yet");
        let m = mixer();
        assert_eq!(m.output_rate(), 44100);
        assert_eq!(m.set_volume(1.5), BAD_PARAMETER);
        assert_eq!(m.set_volume(0.5), OK);
        assert_eq!(m.volume(), 0.5);
        m.set_master_volume(2.0);
        assert_eq!(m.state().master_volume, 1.0);
        m.set_master_volume(-1.0);
        assert_eq!(m.state().master_volume, 0.0);
    }

    #[test]
    fn create_destroy_and_stop_all() {
        let m = mixer();
        assert_eq!(m.channel_count(), MAXIMUM_CHANNELS);
        assert_eq!(m.create_channels(4), CHANNEL_BUSY);
        let data = SampleData::tone(10, 1, 44100);
        let s = sample(&m, 9, &data);
        m.set_channel_sample(0, s);
        assert_eq!(m.stop_all_channels(), CHANNEL_BUSY, "the last channel was empty");
        m.set_channel_sample(0, s);
        assert_eq!(m.destroy_channels(), OK);
        assert_eq!(m.host.mixer_channel(s), CHANNEL_NONE);
        assert_eq!(m.channel_count(), 0);
        assert_eq!(m.create_channels(100), OK);
        assert_eq!(m.channel_count(), MAXIMUM_CHANNELS, "capped");
        m.deactivated();
        assert_eq!(m.maximum_channel_count(), 0);
        assert!(!m.is_active());
    }

    #[test]
    fn sources_add_after_the_effects_volume() {
        unsafe extern "C" fn tone(context: *mut c_void, frames: *mut f32, count: c_ulong) {
            let value = unsafe { *(context as *const f32) };
            for i in 0..count as usize * 2 {
                unsafe { *frames.add(i) += value };
            }
        }
        let m = mixer();
        let mut music = 0.25f32;
        m.set_music_source(Some(tone), &mut music as *mut f32 as *mut c_void);
        m.set_volume(0.0); // the music is not subject to the effects volume
        let mut out = [0.0f32; 4];
        m.mix(&mut out);
        assert_eq!(out, [0.25; 4]);
        m.set_music_source(None, std::ptr::null_mut());
        m.mix(&mut out);
        assert_eq!(out, [0.0; 4]);
    }

    #[test]
    fn sample_data_keeps_two_channels_and_refuses_empty_audio() {
        let frames: Vec<f32> = (0..9).map(|i| i as f32).collect();
        let data = SampleData::new(b"", &frames, 3, 8000).unwrap();
        assert_eq!((data.frames, data.channels, data.rate), (3, 2, 8000));
        assert_eq!(data.pcm, vec![0.0, 1.0, 3.0, 4.0, 6.0, 7.0]);
        assert!(SampleData::new(b"", &[], 2, 8000).is_none());
        assert!(SampleData::new(b"", &frames, 0, 8000).is_none());
    }
}
