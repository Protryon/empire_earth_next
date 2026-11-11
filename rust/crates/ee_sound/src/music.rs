//! `EEMusicManager`'s decisions: the music thread's step (the intensity delay's countdown, what a wake-up does),
//! starting, pausing and stopping, the song changes (a random song of the current epoch, one retry against the
//! same song), the fades, the intensity rules (a drop waits five seconds) and the time to the next song (five to
//! seven minutes).
//!
//! Everything stays C++ but the decisions: the manager's members (the class layout is proven), the thread and its
//! events, the conductor, the song objects (`EEMusicObject`), the world and the preferences are reached through
//! [`Host`]. The manager has no Rust state, so the callers' threads (the intensity request comes from the render
//! and simulation threads) and the music thread call in as they called the C++, reading and writing the members in
//! the original's order.

use ee_random::float_to_long;

use crate::conductor::Score;

/// An `EEMusicObject*` (never null).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Song(pub *mut std::ffi::c_void);

/// `mState`.
pub(crate) const STOPPED: i32 = 0;
pub(crate) const PAUSED: i32 = 1;
pub(crate) const PLAYING: i32 = 2;

/// `TSEvent::WaitAny`'s results: the events' indices (exit, pause, intensity, start, stop) and these.
pub(crate) const WAIT_TIMEOUT: i32 = -1;
pub(crate) const WAIT_FAILED: i32 = -2;

/// A drop of the intensity waits this long (ms).
pub(crate) const INTENSITY_DROP_DELAY: u32 = 5000;
/// `SEScore::SetRepeatCount` of a song: it loops.
pub(crate) const SONG_REPEATS: u32 = 100000;
/// `EEMusicObject::UnloadScore`: busy, try again.
pub(crate) const UNLOAD_AGAIN: i32 = 5;

const FADE_STEPS: f32 = 3.0;
const FADE_TIME: f32 = 0.1;
/// `sFadeSleep`: `(long)(kFadeTime * 1000.0f)`.
pub(crate) const FADE_SLEEP: u32 = 100;
const MINIMUM_SONG_DELAY: f32 = 300000.0;
const SONG_DELAY_RANGE: f32 = 120001.0;

/// What the music manager asks of the C++ side. Nothing here calls back into this module.
pub trait Host {
    // the manager's members
    fn music(&self) -> Option<Song>;
    fn set_music(&mut self, s: Option<Song>);
    fn score_id(&self) -> i32;
    fn set_score_id(&mut self, id: i32);
    fn state(&self) -> i32;
    fn set_state(&mut self, s: i32);
    fn intensity_delay(&self) -> u32;
    fn set_intensity_delay(&mut self, d: u32);
    fn intensity(&self) -> u32;
    fn set_intensity(&mut self, i: u32);
    fn time_to_next_song(&self) -> u32;
    fn set_time_to_next_song(&mut self, t: u32);
    fn requested_intensity(&self) -> u32;
    fn set_requested_intensity(&mut self, i: u32);
    /// `mRandom.Generate()`.
    fn random(&mut self) -> f32;
    /// `this->Stop()`: sets the stop event (the thread stops on its next wake-up).
    fn request_stop(&mut self);
    /// `PlatGetLastError()` (a failed wait).
    fn last_error(&mut self);

    /// The current player's tech tree's epoch; None without a current player or tech tree.
    fn current_epoch(&mut self) -> Option<i32>;
    /// `gGamePreferences.mSettings.GetMusicVolume()`.
    fn music_volume(&self) -> f32;
    fn sleep(&mut self, ms: u32);

    /// `EEMusicObject::GetRandom(epoch)`.
    fn random_song(&mut self, epoch: i32) -> Option<Song>;
    /// `song->LoadScore()` (its result is not looked at).
    fn song_load(&mut self, s: Song);
    /// `song->mScoreID`.
    fn song_score_id(&self, s: Song) -> i32;
    /// `song->UnloadScore()`.
    fn song_unload(&mut self, s: Song) -> i32;
    /// The song's reverb: gain, high-frequency RT ratio, mix, time.
    fn song_reverb(&self, s: Song) -> [f32; 4];

    // mConductor
    fn volume(&self) -> f32;
    fn set_volume(&mut self, v: f32);
    fn stop(&mut self);
    fn score(&mut self, id: i32) -> Option<Score>;
    fn set_repeat_count(&mut self, s: Score, count: u32);
    fn conductor_set_intensity(&mut self, intensity: u32, immediate: bool);
    fn set_reverb_style(&mut self, reverb: [f32; 4]);
    fn play(&mut self, id: i32);
    fn transition_to(&mut self, id: i32, command: u32, flags: u32);
    /// `gConductor->GetConductor().SetIntensity` (the intensity step goes through the global mixer's conductor,
    /// the same object in the game).
    fn global_set_intensity(&mut self, intensity: u32, immediate: bool);
}

/// The time to the next song: `UMFloatToLong(random * 120001 + 300000)` (f32), 420001 taken as 420000.
pub fn song_delay(random: f32) -> u32 {
    let d = float_to_long(random * SONG_DELAY_RANGE + MINIMUM_SONG_DELAY);
    (if d == 420001 { 420000 } else { d }) as u32
}

/// Whether the music is audible enough to fade (`|volume| > 0.0001`).
fn audible(v: f32) -> bool {
    v < -0.0001f32 || v > 0.0001f32
}

/// The fade's step per sleep (`sFadeStep * kFadeTime`).
fn fade_step() -> f32 {
    (1.0f32 / FADE_STEPS) * FADE_TIME
}

/// `FadeIn`: from silence up to the music volume setting, a step per 100 ms.
pub fn fade_in(h: &mut impl Host) {
    let mut v = 0.0f32;
    while v < h.music_volume() {
        h.set_volume(v);
        v += fade_step();
        h.sleep(FADE_SLEEP);
    }
    let m = h.music_volume();
    h.set_volume(m);
}

/// `FadeOut`: from the music volume setting down to silence.
pub fn fade_out(h: &mut impl Host) {
    let mut v = h.music_volume();
    while v > 0.0f32 {
        h.set_volume(v);
        v -= fade_step();
        h.sleep(FADE_SLEEP);
    }
    h.set_volume(0.0);
}

/// One wake-up of the music thread (`MusicManagerMain`'s loop body after the wait): counts the intensity delay
/// down by the elapsed time and acts on the wait's result. True when the thread must end (the exit event).
pub fn step(h: &mut impl Host, wait: i32, elapsed: u32) -> bool {
    let d = h.intensity_delay();
    if d <= elapsed {
        h.set_intensity_delay(0);
    } else {
        h.set_intensity_delay(d - elapsed);
    }
    match wait {
        WAIT_FAILED => h.last_error(),
        WAIT_TIMEOUT => {
            if h.state() == PLAYING {
                next_song(h);
            }
        }
        4 => do_stop(h),
        3 => {
            if h.state() == PLAYING {
                update(h, elapsed);
            } else {
                do_start(h);
            }
        }
        2 => {
            do_set_intensity(h);
            if h.state() == PLAYING {
                update(h, elapsed);
            }
        }
        1 => do_pause(h),
        0 => {
            do_stop(h);
            return true;
        }
        _ => {}
    }
    false
}

/// `DoPause`.
pub fn do_pause(h: &mut impl Host) {
    if h.state() == PLAYING {
        h.set_state(PAUSED);
        h.stop();
    }
}

/// `SetIntensity`'s rule (before the event is set): a drop starts the five-second delay unless one runs, a rise
/// cancels it; the request is kept.
pub fn request_intensity(h: &mut impl Host, intensity: u32) {
    if h.intensity_delay() == 0 && intensity < h.intensity() {
        h.set_intensity_delay(INTENSITY_DROP_DELAY);
    } else if intensity > h.intensity() {
        h.set_intensity_delay(0);
    }
    h.set_requested_intensity(intensity);
}

/// `DoSetIntensity`: the request applies unless it is not a rise and the delay runs; while playing a change
/// transitions the score (a groove on the measure either way).
pub fn do_set_intensity(h: &mut impl Host) {
    let old = h.intensity();
    if h.intensity_delay() != 0 && h.requested_intensity() <= old {
        return;
    }
    let i = h.requested_intensity();
    h.set_intensity(i);
    h.global_set_intensity(i, false);
    if i == old || h.state() != PLAYING {
        return;
    }
    let id = h.score_id();
    h.transition_to(id, 0, 0);
}

/// The common part of a song's start: loop it, set intensity and reverb, play, fade in, schedule the next song.
/// False when the conductor has no score of the id.
fn play_song(h: &mut impl Host, song: Option<Song>, fade: bool) -> bool {
    let id = h.score_id();
    let Some(score) = h.score(id) else {
        return false;
    };
    h.set_repeat_count(score, SONG_REPEATS);
    let i = h.intensity();
    h.conductor_set_intensity(i, false);
    // (a song is always set here: the score id came from it)
    let reverb = song.map_or([0.0; 4], |s| h.song_reverb(s));
    h.set_reverb_style(reverb);
    let id = h.score_id();
    h.play(id);
    if fade {
        fade_in(h);
    }
    true
}

/// `DoStart`: a random song of the current epoch (epoch 0 without a tech tree), or the paused one, from silence;
/// fades in when the music was audible; the next song in five to seven minutes. Without a score it asks to stop.
pub fn do_start(h: &mut impl Host) {
    let epoch = h.current_epoch().unwrap_or(0);
    let fade = audible(h.volume());
    let state = h.state();
    if state != STOPPED && state != PAUSED {
        return;
    }
    if state == STOPPED {
        let song = h.random_song(epoch);
        h.set_music(song);
        // the C++ called LoadScore on a null song (no song for the epoch) and crashed; here it is a missing score
        match song {
            Some(s) => {
                h.song_load(s);
                let id = h.song_score_id(s);
                h.set_score_id(id);
            }
            None => {
                h.set_volume(0.0);
                h.request_stop();
                return;
            }
        }
    }
    h.set_volume(0.0);
    let song = h.music();
    if play_song(h, song, fade) {
        let r = h.random();
        h.set_time_to_next_song(song_delay(r));
        h.set_state(PLAYING);
    } else {
        h.request_stop();
    }
}

/// Unloads a song's score, retrying while it is busy (a millisecond apart).
fn unload(h: &mut impl Host, s: Song) {
    loop {
        let r = h.song_unload(s);
        h.sleep(1);
        if r != UNLOAD_AGAIN {
            break;
        }
    }
}

/// `DoStop`: stopped, the music silenced and the song's score unloaded.
pub fn do_stop(h: &mut impl Host) {
    h.set_state(STOPPED);
    h.stop();
    let Some(s) = h.music() else {
        return;
    };
    unload(h, s);
    h.set_music(None);
}

/// `NextSong`: another random song of the current epoch (a second draw when the first is the same song), faded
/// out and in when audible; the next change in five to seven minutes. Without a tech tree, or when both draws give
/// the same song, only the time is drawn. A missing score leaves the state and the time as they were.
pub fn next_song(h: &mut impl Host) {
    let old = h.music();
    let epoch = h.current_epoch();
    let fade = audible(h.volume());
    if let Some(epoch) = epoch {
        let mut song = h.random_song(epoch);
        h.set_music(song);
        if song == old {
            song = h.random_song(epoch);
            h.set_music(song);
        }
        if song != old {
            // the C++ called LoadScore on a null song (no song for the epoch) and crashed; here the score id stays
            if let Some(s) = song {
                h.song_load(s);
                let id = h.song_score_id(s);
                h.set_score_id(id);
            }
            if fade {
                fade_out(h);
            }
            h.stop();
            if let Some(o) = old {
                unload(h, o);
            }
            if !play_song(h, song, fade) {
                return;
            }
        }
    }
    let r = h.random();
    h.set_time_to_next_song(song_delay(r));
    h.set_state(PLAYING);
}

/// `Update`: the time to the next song counts down; at its end the song changes.
pub fn update(h: &mut impl Host, elapsed: u32) {
    let t = h.time_to_next_song();
    if t > elapsed {
        h.set_time_to_next_song(t - elapsed);
    } else {
        next_song(h);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn song_delays() {
        assert_eq!(song_delay(0.0), 300000);
        assert_eq!(song_delay(1.0), 420000);
        assert_eq!(song_delay(0.5), 360000);
        assert_eq!(song_delay(0.99999994), 420000);
    }

    #[test]
    fn fade_step_is_the_cpp_product() {
        assert_eq!(fade_step().to_bits(), ((1.0f32 / 3.0f32) * 0.1f32).to_bits());
        assert_eq!((0.1f32 * 1000.0f32) as i32 as u32, FADE_SLEEP);
    }
}
