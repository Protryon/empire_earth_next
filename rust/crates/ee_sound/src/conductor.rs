//! `SEConductor`'s decisions: the score table (names, ids, load counts, the loaded scores), starting and stopping
//! the conductor, which score plays, the transitions' patterns and the volume curve.
//!
//! The C++ keeps the objects and the music library: dmusic's loader and performance (a vendored DirectMusic
//! reimplementation, `src/ThirdParty/dmusic`), the `SEScore` objects (a segment each), the mixer's music source and
//! the conductor's lock. They are reached through [`Host`]; a score is an opaque handle. The scalar members the
//! game reads inline (`GetVolume`, `GetConductorState`, ...) stay C++ members, handed to every call as
//! [`ConductorFields`] and written back after it, under the lock the C++ held (every write the C++ made was under it).

use std::ffi::c_void;

/// `kErrorOK`, `kErrorFailed`, `kErrorBusy`, `kErrorNotFound` (`lle/LowLevelEngine.h`); `1` is the conductor's
/// "dmusic said no".
pub const OK: i32 = 0;
pub const DM_ERROR: i32 = 1;
pub const BUSY: i32 = 5;
pub const NOT_FOUND: i32 = 3;
pub const FAILED: i32 = 9;

/// An `SEScore*` (never null).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Score(pub *mut c_void);

/// The conductor's scalar members (the C++ object's, read and written around each call).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ConductorFields {
    pub volume: f32,
    /// 0 stopped, 1 running.
    pub state: u32,
    pub reverb_state: u32,
    pub next_score_id: i32,
    pub playing_id: i32,
    pub intensity: u32,
    pub channel_group_count: u32,
}

impl ConductorFields {
    /// The constructor's values.
    pub fn new() -> Self {
        ConductorFields {
            volume: 1.0,
            state: 0,
            reverb_state: 1,
            next_score_id: 0,
            playing_id: -1,
            intensity: 0,
            channel_group_count: 1000,
        }
    }
}

impl Default for ConductorFields {
    fn default() -> Self {
        Self::new()
    }
}

/// dmusic's timing of a segment change (`DmTiming`'s values).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u32)]
pub enum Timing {
    Instant = 1,
    Measure = 4,
}

/// dmusic's transition pattern (`DmEmbellishmentType`'s values), from `TransitionTo`'s command.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u32)]
pub enum Embellishment {
    Groove = 1,
    Fill = 2,
    Intro = 3,
    Break = 4,
    End = 5,
}

/// `SETransitionCommand` -> the pattern (end-and-intro plays the end only; anything else is the groove).
pub fn embellishment(command: u32) -> Embellishment {
    match command {
        1 => Embellishment::Fill,
        2 => Embellishment::Break,
        3 => Embellishment::Intro,
        4 | 5 => Embellishment::End,
        _ => Embellishment::Groove,
    }
}

/// The performance's volume for a music volume: silent at 0.01 and below, else `volume^1.1512925` in double
/// precision (the original's port volume of 1000 * ln(volume) hundredths of a dB is that amplitude).
pub fn amplitude(volume: f32) -> f32 {
    if volume > 0.01f32 {
        (volume as f64).powf(1.1512925) as f32
    } else {
        0.0
    }
}

/// What the conductor asks of the C++ side: dmusic, the scores and the mixer. Nothing here reaches the conductor.
pub trait Host {
    /// `mMixer ? mMixer->GetOutputRate() : 0`.
    fn output_rate(&mut self) -> u32;
    /// Creates the loader (with the music folder's resolver) and the performance at the rate; on success the C++
    /// keeps both, on failure it has released what it made.
    fn start(&mut self, rate: u32) -> bool;
    /// Releases the performance and the loader.
    fn release(&mut self);
    /// `mMixer->SetMusicSource(Render, this)` (true) or `(NULL, NULL)` (false; only with a mixer).
    fn set_music_source(&mut self, on: bool);
    /// `DmLoader_getSegment(name)` and `new SEScore(segment)`; None on a dmusic failure or no segment.
    fn load(&mut self, name: &[u8]) -> Option<Score>;
    /// `delete score`.
    fn delete(&mut self, s: Score);
    /// `SEScore::mIsSecondaryScore`.
    fn is_secondary(&self, s: Score) -> bool;
    /// `DmPerformance_playSegment(score ? segment : NULL, timing)` succeeded.
    fn play(&mut self, s: Option<Score>, timing: Timing) -> bool;
    /// `DmPerformance_playTransition(segment, pattern, DmTiming_MEASURE)` succeeded.
    fn play_transition(&mut self, s: Score, pattern: Embellishment) -> bool;
    /// `DmPerformance_setVolume`.
    fn set_performance_volume(&mut self, amplitude: f32);
    /// `DmPerformance_setMasterGrooveLevel`.
    fn set_groove_level(&mut self, level: i8);
}

/// One entry of the score table (`SEConductorScore`).
#[derive(Clone, Debug, PartialEq)]
pub struct Entry {
    /// The file name as given (UTF-8 bytes, compared byte for byte as `PlatMbsCmp` does).
    pub name: Vec<u8>,
    /// None while the conductor is stopped (only the name is kept) or when its load failed.
    pub score: Option<Score>,
    pub id: i32,
    pub load_count: u32,
}

/// The score table. Every method is called with the C++ lock held and the C++ object's fields in `f`.
#[derive(Debug, Default)]
pub struct Conductor {
    pub scores: Vec<Entry>,
}

impl Conductor {
    pub fn new() -> Self {
        Conductor { scores: Vec::new() }
    }

    /// `GetCurrentlyPlayingScore`.
    pub fn currently_playing(f: &ConductorFields) -> i32 {
        if f.state == 1 {
            f.playing_id
        } else {
            -1
        }
    }

    /// `GetScore`: the first entry with the id (None: none, or not loaded).
    pub fn score(&self, id: i32) -> Option<Score> {
        self.scores.iter().find(|e| e.id == id).and_then(|e| e.score)
    }

    /// `GetScoreID`: the first entry holding the score; -1 for none.
    pub fn score_id(&self, s: Option<Score>) -> i32 {
        match s {
            Some(s) => self.scores.iter().find(|e| e.score == Some(s)).map_or(-1, |e| e.id),
            None => -1,
        }
    }

    fn next_id(f: &mut ConductorFields) -> i32 {
        let id = f.next_score_id;
        f.next_score_id = f.next_score_id.wrapping_add(1);
        if f.next_score_id == -1 {
            f.next_score_id = 0;
        }
        id
    }

    /// `LoadScore(name, outID, secondary)` -> (result, outID if it was written). A name already in the table only
    /// counts one more load; while stopped only the name is remembered. `secondary` (the start's reload) loads the
    /// score of the name's entry.
    pub fn load_score(
        &mut self,
        h: &mut impl Host,
        f: &mut ConductorFields,
        name: &[u8],
        secondary: bool,
    ) -> (i32, Option<i32>) {
        let found = self.scores.iter().position(|e| e.name == name);
        if let (Some(i), false) = (found, secondary) {
            self.scores[i].load_count = self.scores[i].load_count.wrapping_add(1);
            return (OK, Some(self.scores[i].id));
        }
        if f.state == 1 {
            let Some(s) = h.load(name) else {
                return (DM_ERROR, None);
            };
            if secondary {
                // the C++ wrote entry i - 1 of its search, which is the found one; with no match it was the last
                // (never reached: the start reloads names taken from the table itself)
                match found.or(self.scores.len().checked_sub(1)) {
                    Some(i) => {
                        self.scores[i].score = Some(s);
                        (OK, Some(self.scores[i].id))
                    }
                    None => {
                        h.delete(s);
                        (OK, None)
                    }
                }
            } else {
                let id = Self::next_id(f);
                self.scores.push(Entry { name: name.to_vec(), score: Some(s), id, load_count: 1 });
                (OK, Some(id))
            }
        } else if !secondary {
            let id = Self::next_id(f);
            self.scores.push(Entry { name: name.to_vec(), score: None, id, load_count: 1 });
            (OK, Some(id))
        } else {
            (OK, None)
        }
    }

    /// `SetVolume`.
    pub fn set_volume(h: &mut impl Host, f: &mut ConductorFields, volume: f32) -> i32 {
        f.volume = volume;
        if f.state == 1 {
            h.set_performance_volume(amplitude(volume));
        }
        OK
    }

    /// `SetIntensity`: kept while stopped; `immediate` reapplies an unchanged level.
    pub fn set_intensity(h: &mut impl Host, f: &mut ConductorFields, intensity: u32, immediate: bool) -> i32 {
        if intensity != f.intensity || immediate {
            f.intensity = intensity;
            if f.state == 1 {
                h.set_groove_level(intensity as i8);
            }
        }
        OK
    }

    /// `InitializeConductor`: 1 when it could not start; a failed reload stops the reloading (the conductor stays
    /// running, the later scores unloaded) and is the result.
    pub fn initialize(&mut self, h: &mut impl Host, f: &mut ConductorFields) -> i32 {
        let rate = h.output_rate();
        if rate == 0 || !h.start(rate) {
            return DM_ERROR;
        }
        f.state = 1;
        f.playing_id = -1;
        let v = f.volume;
        Self::set_volume(h, f, v);
        let i = f.intensity;
        Self::set_intensity(h, f, i, true);
        let mut result = OK;
        let mut k = 0;
        while k < self.scores.len() && result == OK {
            let name = self.scores[k].name.clone();
            result = self.load_score(h, f, &name, true).0;
            k += 1;
        }
        h.set_music_source(true);
        result
    }

    /// `UninitializeConductor`: off the mixer first, then every score deleted (the names kept), then dmusic.
    pub fn uninitialize(&mut self, h: &mut impl Host, f: &mut ConductorFields) -> i32 {
        if f.state == 1 {
            h.set_music_source(false);
            for e in &mut self.scores {
                if let Some(s) = e.score.take() {
                    h.delete(s);
                }
            }
            h.release();
            f.playing_id = -1;
            f.state = 0;
        }
        OK
    }

    /// `SetConductorState`: starts (non-zero) or stops (0) when it differs.
    pub fn set_state(&mut self, h: &mut impl Host, f: &mut ConductorFields, state: u32) -> i32 {
        if state == f.state {
            OK
        } else if state == 0 {
            self.uninitialize(h, f)
        } else {
            self.initialize(h, f)
        }
    }

    /// `Play`: now.
    pub fn play(&self, h: &mut impl Host, f: &mut ConductorFields, id: i32) -> i32 {
        let s = self.score(id);
        match s {
            Some(s) if f.state == 1 => {
                if h.play(Some(s), Timing::Instant) {
                    f.playing_id = id;
                    OK
                } else {
                    DM_ERROR
                }
            }
            _ => OK,
        }
    }

    /// `Queue`: at the next measure; a secondary score cannot be queued.
    pub fn queue(&self, h: &mut impl Host, f: &mut ConductorFields, id: i32) -> i32 {
        let s = self.score(id);
        match s {
            Some(s) if f.state == 1 => {
                if h.is_secondary(s) {
                    FAILED
                } else if h.play(Some(s), Timing::Measure) {
                    f.playing_id = id;
                    OK
                } else {
                    DM_ERROR
                }
            }
            _ => OK,
        }
    }

    /// `Stop`: silence now (the playing id is cleared even when dmusic fails).
    pub fn stop(h: &mut impl Host, f: &mut ConductorFields) -> i32 {
        let mut r = OK;
        if f.state == 1 {
            if !h.play(None, Timing::Instant) {
                r = DM_ERROR;
            }
            f.playing_id = -1;
        }
        r
    }

    /// `TransitionTo(id, command, flags)`: the command's pattern into the score at the next measure, or into
    /// silence when the id has no loaded score (`flags` is the original's composer flags, unused by dmusic).
    pub fn transition_to(&self, h: &mut impl Host, f: &mut ConductorFields, id: i32, command: u32) -> i32 {
        let s = self.score(id);
        if f.state != 1 {
            return OK;
        }
        let pattern = embellishment(command);
        let ok = match s {
            Some(s) => h.play_transition(s, pattern),
            None => h.play(None, Timing::Measure),
        };
        if ok {
            f.playing_id = if s.is_some() { id } else { -1 };
            OK
        } else {
            DM_ERROR
        }
    }

    /// `SetChannelGroupCount`: at most 1000, and only while stopped.
    pub fn set_channel_group_count(f: &mut ConductorFields, count: u32) -> i32 {
        if f.state == 0 {
            f.channel_group_count = count.min(1000);
            OK
        } else {
            BUSY
        }
    }

    /// `UnloadScore`: one load fewer, the entry and its score gone at the last; busy while it plays.
    pub fn unload_score(&mut self, h: &mut impl Host, f: &ConductorFields, id: i32) -> i32 {
        let Some(i) = self.scores.iter().position(|e| e.id == id) else {
            return NOT_FOUND;
        };
        if id == -1 {
            NOT_FOUND
        } else if id == Self::currently_playing(f) {
            BUSY
        } else if self.scores[i].load_count == 1 {
            let e = self.scores.remove(i);
            if let Some(s) = e.score {
                h.delete(s);
            }
            OK
        } else {
            self.scores[i].load_count -= 1;
            OK
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn amplitude_curve() {
        assert_eq!(amplitude(0.0), 0.0);
        assert_eq!(amplitude(0.01), 0.0);
        assert_eq!(amplitude(1.0), 1.0);
        assert_eq!(amplitude(0.5).to_bits(), (0.5f64.powf(1.1512925) as f32).to_bits());
    }

    #[test]
    fn patterns() {
        let p: Vec<_> = (0..8).map(embellishment).collect();
        use Embellishment::*;
        assert_eq!(p, [Groove, Fill, Break, Intro, End, End, Groove, Groove]);
    }

    #[test]
    fn ids_wrap_past_minus_one() {
        let mut f = ConductorFields { next_score_id: -2, ..ConductorFields::new() };
        assert_eq!(Conductor::next_id(&mut f), -2);
        assert_eq!(f.next_score_id, 0);
        f.next_score_id = i32::MAX;
        assert_eq!(Conductor::next_id(&mut f), i32::MAX);
        assert_eq!(f.next_score_id, i32::MIN);
    }
}
