//! The mixer's output device (cpal: WASAPI, Core Audio, ALSA) and its decoder (symphonia: WAV, MP3, FLAC), which
//! replaced miniaudio. The device thread pulls the mix from [`Mixer::mix`] (the channels, then the music), stereo
//! float at the device's own rate; a device of another format or channel count gets the same frames converted.
//! Without a device (the headless simulation, a machine with no output) a null device pulls the mix at the
//! rate of a 48 kHz device in real time, as miniaudio's null backend did, so the game always has its mixer.
//!
//! The decoder keeps miniaudio's sample conversions (dr_wav's: 8-bit `x * (2 / 255) - 1`, 16-bit `x / 32768`, 24-bit
//! and 32-bit through double), so a WAV file's samples, and so the effects' mix, are bit for bit what they were.

use std::io::Cursor;
use std::sync::mpsc;
use std::sync::Mutex;
use std::thread::JoinHandle;
use std::time::{Duration, Instant};

use symphonia::core::audio::{Audio, GenericAudioBufferRef};
use symphonia::core::codecs::audio::AudioDecoderOptions;
use symphonia::core::codecs::CodecParameters;
use symphonia::core::formats::probe::Hint;
use symphonia::core::formats::{FormatOptions, TrackType};
use symphonia::core::io::MediaSourceStream;
use symphonia::core::meta::MetadataOptions;

/// A decoded file: interleaved float frames, the channels and the rate.
pub struct Decoded {
    pub pcm: Vec<f32>,
    pub channels: u32,
    pub rate: u32,
}

/// miniaudio's (dr_wav's) conversions to float.
fn u8_to_f32(x: u8) -> f32 {
    f32::from(x) * f32::from_bits(0x3c00_8081) - 1.0 // 0.00784313725490196078f (2 / 255)
}
fn s16_to_f32(x: i16) -> f32 {
    f32::from(x) * (1.0 / 32768.0)
}

fn append(buf: &GenericAudioBufferRef, out: &mut Vec<f32>) {
    fn interleave<S: symphonia::core::audio::sample::Sample>(
        b: &symphonia::core::audio::AudioBuffer<S>,
        out: &mut Vec<f32>,
        f: impl Fn(S) -> f32,
    ) {
        let planes: Vec<&[S]> = (0..b.num_planes()).filter_map(|c| b.plane(c)).collect();
        for i in 0..b.frames() {
            for p in &planes {
                out.push(f(p[i]));
            }
        }
    }
    match buf {
        GenericAudioBufferRef::U8(b) => interleave(b, out, u8_to_f32),
        GenericAudioBufferRef::S16(b) => interleave(b, out, s16_to_f32),
        GenericAudioBufferRef::S24(b) => interleave(b, out, |x| (f64::from(x.inner()) * (1.0 / 8_388_608.0)) as f32),
        GenericAudioBufferRef::S32(b) => interleave(b, out, |x| (f64::from(x) / 2_147_483_648.0) as f32),
        GenericAudioBufferRef::F32(b) => interleave(b, out, |x| x),
        GenericAudioBufferRef::F64(b) => interleave(b, out, |x| x as f32),
        other => other.copy_to_vec_interleaved(out),
    }
}

/// Decodes a whole file image (WAV, MP3, FLAC); None when it is none of them or holds no frames.
pub fn decode(bytes: &[u8]) -> Option<Decoded> {
    let mss = MediaSourceStream::new(Box::new(Cursor::new(bytes.to_vec())), Default::default());
    let mut format = symphonia::default::get_probe()
        .probe(&Hint::new(), mss, FormatOptions::default(), MetadataOptions::default())
        .ok()?;
    let track = format.default_track(TrackType::Audio)?;
    let id = track.id;
    let Some(CodecParameters::Audio(params)) = track.codec_params.clone() else { return None };
    let mut decoder =
        symphonia::default::get_codecs().make_audio_decoder(&params, &AudioDecoderOptions::default()).ok()?;
    let mut pcm = Vec::new();
    let (mut channels, mut rate) =
        (params.channels.as_ref().map_or(0, |c| c.count() as u32), params.sample_rate.unwrap_or(0));
    while let Ok(Some(packet)) = format.next_packet() {
        if packet.track_id != id {
            continue;
        }
        match decoder.decode(&packet) {
            Ok(buf) => {
                channels = buf.spec().channels().count() as u32;
                rate = buf.spec().rate();
                append(&buf, &mut pcm);
            }
            Err(symphonia::core::errors::Error::DecodeError(_)) => continue,
            Err(_) => break,
        }
    }
    (channels != 0 && rate != 0 && !pcm.is_empty()).then_some(Decoded { pcm, channels, rate })
}

// ---- the device

/// The rate of the null device (miniaudio's default: a device of its own rate asked nothing else).
pub const NULL_RATE: u32 = 48000;
/// The null device's period: 10 ms, miniaudio's low-latency default.
const NULL_PERIOD_FRAMES: usize = 480;

/// The open device: its thread (which owns the stream) and the way to stop it.
struct Open {
    stop: mpsc::Sender<()>,
    thread: JoinHandle<()>,
}

static DEVICE: Mutex<Option<Open>> = Mutex::new(None);

/// The device's name as the settings keep it.
fn device_name(d: &cpal::Device) -> String {
    use cpal::traits::DeviceTrait;
    d.description().map(|x| x.name().to_string()).unwrap_or_else(|_| d.to_string())
}

/// The names of the system's playback devices.
pub fn output_devices() -> Vec<String> {
    use cpal::traits::HostTrait;
    match cpal::default_host().output_devices() {
        Ok(devices) => devices.map(|d| device_name(&d)).collect(),
        Err(_) => Vec::new(),
    }
}

/// Writes the stereo mix into a device buffer of `channels` per frame (a mono device gets the two sides' mean, the
/// sides past the second are silent).
fn spread<T: cpal::SizedSample + cpal::FromSample<f32>>(mix: &[f32], out: &mut [T], channels: usize) {
    for (f, frame) in out.chunks_mut(channels).enumerate() {
        let (l, r) = (mix[2 * f], mix[2 * f + 1]);
        for (c, s) in frame.iter_mut().enumerate() {
            let v = match (channels, c) {
                (1, _) => (l + r) * 0.5,
                (_, 0) => l,
                (_, 1) => r,
                _ => 0.0,
            };
            *s = T::from_sample(v);
        }
    }
}

fn stream<T, F>(device: &cpal::Device, config: &cpal::StreamConfig, mut mix: F) -> Option<cpal::Stream>
where
    T: cpal::SizedSample + cpal::FromSample<f32>,
    F: FnMut(&mut [f32]) + Send + 'static,
{
    use cpal::traits::DeviceTrait;
    let channels = usize::from(config.channels).max(1);
    let mut scratch: Vec<f32> = Vec::new();
    device
        .build_output_stream(
            *config,
            move |out: &mut [T], _: &cpal::OutputCallbackInfo| {
                let frames = out.len() / channels;
                scratch.resize(2 * frames, 0.0);
                mix(&mut scratch);
                spread(&scratch, out, channels);
            },
            |_| {},
            None,
        )
        .ok()
}

/// Opens a cpal stream on the named device (the system's default when there is none of that name): its rate.
fn open_cpal<F: FnMut(&mut [f32]) + Send + 'static>(name: &str, mix: F) -> Option<(cpal::Stream, u32)> {
    use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
    let host = cpal::default_host();
    let named = (!name.is_empty()).then(|| host.output_devices().ok()?.find(|d| device_name(d) == name)).flatten();
    let device = named.or_else(|| host.default_output_device())?;
    let supported = device.default_output_config().ok()?;
    let config = supported.config();
    let s = match supported.sample_format() {
        cpal::SampleFormat::F32 => stream::<f32, F>(&device, &config, mix),
        cpal::SampleFormat::I16 => stream::<i16, F>(&device, &config, mix),
        cpal::SampleFormat::U16 => stream::<u16, F>(&device, &config, mix),
        cpal::SampleFormat::I32 => stream::<i32, F>(&device, &config, mix),
        cpal::SampleFormat::F64 => stream::<f64, F>(&device, &config, mix),
        cpal::SampleFormat::U8 => stream::<u8, F>(&device, &config, mix),
        cpal::SampleFormat::I8 => stream::<i8, F>(&device, &config, mix),
        _ => None,
    }?;
    s.play().ok()?;
    Some((s, config.sample_rate))
}

/// The null device: the mix pulled in 10 ms periods at 48 kHz, paced by the clock.
fn run_null<F: FnMut(&mut [f32])>(mut mix: F, stop: &mpsc::Receiver<()>) {
    let mut buf = vec![0f32; 2 * NULL_PERIOD_FRAMES];
    let start = Instant::now();
    let mut done: u64 = 0;
    loop {
        let due = start.elapsed().as_secs_f64() * f64::from(NULL_RATE);
        while (done as f64) + NULL_PERIOD_FRAMES as f64 <= due {
            mix(&mut buf);
            done += NULL_PERIOD_FRAMES as u64;
        }
        match stop.recv_timeout(Duration::from_millis(10)) {
            Err(mpsc::RecvTimeoutError::Timeout) => {}
            _ => return,
        }
    }
}

/// Opens the output: the named device (or the default) through cpal, else (or with `null_only`) the null device. The
/// device's rate; `mix` fills interleaved stereo frames on the device's thread until [`close`].
pub fn open<F: FnMut(&mut [f32]) + Send + 'static>(name: &str, null_only: bool, mix: F) -> u32 {
    close();
    let (stop_tx, stop_rx) = mpsc::channel::<()>();
    let (rate_tx, rate_rx) = mpsc::channel::<u32>();
    let name = name.to_string();
    let thread = std::thread::Builder::new()
        .name("Mixer device".into())
        .spawn(move || {
            let mut mix = Some(mix);
            if !null_only {
                // the stream lives on this thread (it is not Send on every platform) until the stop
                let shared = std::sync::Arc::new(Mutex::new(mix.take()));
                let s2 = shared.clone();
                let opened = open_cpal(&name, move |buf: &mut [f32]| {
                    if let Some(m) = s2.lock().unwrap_or_else(|e| e.into_inner()).as_mut() {
                        m(buf)
                    }
                });
                if let Some((stream, rate)) = opened {
                    let _ = rate_tx.send(rate);
                    let _ = stop_rx.recv();
                    drop(stream);
                    return;
                }
                mix = shared.lock().unwrap_or_else(|e| e.into_inner()).take();
            }
            let _ = rate_tx.send(NULL_RATE);
            if let Some(m) = mix {
                run_null(m, &stop_rx);
            }
        })
        .expect("the mixer's device thread starts");
    let rate = rate_rx.recv().unwrap_or(NULL_RATE);
    *DEVICE.lock().unwrap_or_else(|e| e.into_inner()) = Some(Open { stop: stop_tx, thread });
    rate
}

/// Stops the device: when this returns its thread no longer mixes.
pub fn close() {
    let open = DEVICE.lock().unwrap_or_else(|e| e.into_inner()).take();
    if let Some(o) = open {
        let _ = o.stop.send(());
        let _ = o.thread.join();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::sync::Arc;

    fn wav(channels: u16, rate: u32, bits: u16, data: &[u8]) -> Vec<u8> {
        let align = channels * bits / 8;
        let mut v = b"RIFF".to_vec();
        v.extend_from_slice(&(36 + data.len() as u32).to_le_bytes());
        v.extend_from_slice(b"WAVEfmt ");
        v.extend_from_slice(&16u32.to_le_bytes());
        v.extend_from_slice(&1u16.to_le_bytes());
        v.extend_from_slice(&channels.to_le_bytes());
        v.extend_from_slice(&rate.to_le_bytes());
        v.extend_from_slice(&(rate * u32::from(align)).to_le_bytes());
        v.extend_from_slice(&align.to_le_bytes());
        v.extend_from_slice(&bits.to_le_bytes());
        v.extend_from_slice(b"data");
        v.extend_from_slice(&(data.len() as u32).to_le_bytes());
        v.extend_from_slice(data);
        v
    }

    #[test]
    fn wav_samples_are_minaudios_bit_for_bit() {
        // 8-bit: miniaudio's x * 0.00784313725490196078f - 1
        let d = decode(&wav(1, 22050, 8, &[0, 1, 127, 128, 200, 255])).unwrap();
        assert_eq!((d.channels, d.rate, d.pcm.len()), (1, 22050, 6));
        let want: Vec<u32> = [0u8, 1, 127, 128, 200, 255]
            .iter()
            .map(|&x| (x as f32 * f32::from_bits(0x3c00_8081) - 1.0).to_bits())
            .collect();
        assert_eq!(d.pcm.iter().map(|v| v.to_bits()).collect::<Vec<_>>(), want);
        assert_eq!(d.pcm[0], -1.0);
        // 16-bit stereo: x / 32768
        let s: Vec<i16> = vec![-32768, 32767, 0, -1, 1234, -4321];
        let bytes: Vec<u8> = s.iter().flat_map(|x| x.to_le_bytes()).collect();
        let d = decode(&wav(2, 44100, 16, &bytes)).unwrap();
        assert_eq!((d.channels, d.rate), (2, 44100));
        assert_eq!(d.pcm, s.iter().map(|&x| f32::from(x) / 32768.0).collect::<Vec<_>>());
    }

    #[test]
    fn not_a_sound() {
        assert!(decode(b"not a sound file at all").is_none());
        assert!(decode(&wav(1, 22050, 16, &[])).is_none(), "no frames");
    }

    #[test]
    fn the_mix_spreads_over_the_devices_channels() {
        let mix = [0.5f32, -0.25, 1.0, 0.0];
        let mut mono = [0f32; 2];
        spread(&mix, &mut mono, 1);
        assert_eq!(mono, [0.125, 0.5]);
        let mut quad = [9i16; 8];
        spread(&mix, &mut quad, 4);
        assert_eq!(quad[2..4], [0, 0]);
        assert_eq!(quad[0], <i16 as cpal::FromSample<f32>>::from_sample_(0.5f32));
    }

    #[test]
    fn the_null_device_pulls_the_mix_in_real_time() {
        let frames = Arc::new(AtomicU64::new(0));
        let f = frames.clone();
        let rate = open("", true, move |buf: &mut [f32]| {
            f.fetch_add(buf.len() as u64 / 2, Ordering::SeqCst);
        });
        assert_eq!(rate, NULL_RATE);
        std::thread::sleep(Duration::from_millis(200));
        close();
        let n = frames.load(Ordering::SeqCst);
        assert!((4800..=48000).contains(&n), "{n} frames in 0.2 s");
        let after = frames.load(Ordering::SeqCst);
        std::thread::sleep(Duration::from_millis(50));
        assert_eq!(frames.load(Ordering::SeqCst), after, "closed: no more mixing");
        close(); // twice is harmless
    }

    /// Not a check here: the decoder digest of the game's sounds (`EE_DECODE_DIGEST=<a file of paths>
    /// EE_DECODE_OUT=<out file>`): name, frames, channels, rate and an FNV-1a hash of the floats' bits per file, as
    /// the miniaudio driver printed it, to compare before and after.
    #[test]
    fn decoder_digest() {
        let (Ok(list), Ok(out)) = (std::env::var("EE_DECODE_DIGEST"), std::env::var("EE_DECODE_OUT")) else { return };
        let mut text = String::new();
        for path in std::fs::read_to_string(list).unwrap().lines().filter(|l| !l.is_empty()) {
            let Some(d) = std::fs::read(path).ok().and_then(|b| decode(&b)) else {
                text += &format!(
                    "{path} fail
"
                );
                continue;
            };
            if std::env::var("EE_DECODE_RAW").is_ok() {
                let raw: Vec<u8> = d.pcm.iter().flat_map(|v| v.to_le_bytes()).collect();
                std::fs::write(format!("{path}.sy.raw"), raw).unwrap();
            }
            let mut h: u32 = 2166136261;
            for v in &d.pcm {
                h = (h ^ v.to_bits()).wrapping_mul(16777619);
            }
            text += &format!(
                "{path} {} {} {} {h:08x}
",
                d.pcm.len() / d.channels as usize,
                d.channels,
                d.rate
            );
        }
        std::fs::write(out, text).unwrap();
    }
}
