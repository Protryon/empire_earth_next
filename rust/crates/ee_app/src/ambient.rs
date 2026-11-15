//! The ambient sounds (`EEAmbientSoundManager`, `EEAmbientSound`; ee/EEAmbientSoundManager.h, ee/EEAmbientSound.h)
//! whole in Rust: the manager's and each sound's plain members are records the C++ classes derive from
//! ([`EeAppAmbientFields`], [`EeAppAmbientSoundFields`]; the world analyzer's counts [`EeAppAmbientCounts`]), and
//! every decision is here: the day phase and its handler, the terrain and animal sounds, the forest sounds, the fades,
//! enabling, stopping and restarting. The C++ keeps the sound objects and instances, the manager's random generator,
//! the world's reads and the analyzer's counting of the rendered units, reached through [`AmbientHost`].
//!
//! Ambient sounds are local (the render thread's): no simulation state is read or written, and the generator is the
//! manager's own (seeded from the clock at load).

#![allow(non_snake_case)]
// the original's negated float tests are kept as written (a NaN takes the same branch)
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use std::os::raw::c_long;

/// The day phases' limits (ee/EEAmbientSoundManager.h).
const DAWN_TIME: f32 = 0.955;
const DUSK_TIME: f32 = 0.452;
/// The unit's .rdata floats (0x441e14..0x441e3c).
const NIGHT_TIME: f32 = 0.07;
const BIRD_PITCH_HIGH: f32 = 1.062;
const CRICKET_PITCH_LOW: f32 = 0.795;
const CRICKET_PITCH_HIGH: f32 = 1.125;
const ANIMAL_PITCH_HIGH: f32 = 1.065;
const TERRAIN_PITCH_LOW1: f32 = 0.825;
const TERRAIN_PITCH_HIGH1: f32 = 1.11;
const TERRAIN_PITCH_LOW2: f32 = 0.805;
const TERRAIN_PITCH_HIGH2: f32 = 1.096;
const TERRAIN_PITCH_LOW3: f32 = 0.865;
const TERRAIN_PITCH_HIGH3: f32 = 1.066;
/// `kHeightEpsilon` (ee/EEGlobalConstants.h; EECPAttackSubGroup.cpp defines it)
const HEIGHT_EPSILON: f32 = 0.01;
/// FadeOut's silence (.rdata 0x43f924)
const SILENCE: f32 = 0.005;

/// The world analyzer's counts (`EEAmbientWorldAnalyzer`, after its vtable), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppAmbientCounts {
    pub mNumAirRaiders: c_long,
    pub mNumTrees: c_long,
    pub mNumCitizens: c_long,
    pub mNumAttackers: c_long,
    pub mNumFarms: c_long,
    pub mNumGoats: c_long,
    pub mNumEagles: c_long,
    pub mL20: c_long,
    pub mL24: c_long,
    pub mL28: c_long,
    /// the first visible forest's resource kind (0: none yet)
    pub mForestType: c_long,
}

/// `EEAmbientSoundManager`'s plain members (the analyzer, the generator and the sounds stay C++).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeAppAmbientFields {
    /// the day phase: 1 night .. 4 dawn (see [`phase`])
    pub mState: c_long,
    pub mFrameCount: c_long,
    pub mNextUpdateFrame: c_long,
    /// 10 frames
    pub mUpdateInterval: c_long,
    pub mIsEnabled: bool,
    pub mIsPaused: bool,
}

impl Default for EeAppAmbientFields {
    /// The constructor (0x124ee2): disabled, unpaused, updating every 10 frames (`mState` untouched there: 0 here).
    fn default() -> Self {
        EeAppAmbientFields {
            mState: 0,
            mFrameCount: 0,
            mNextUpdateFrame: 0,
            mUpdateInterval: 10,
            mIsEnabled: false,
            mIsPaused: false,
        }
    }
}

/// `EEAmbientSound`'s plain members (the sound object and the instance stay C++).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeAppAmbientSoundFields {
    pub mSoundID: c_long,
    pub mSoundIndex: c_long,
    pub mProbability: f32,
    pub mVolume: f32,
    pub mTargetVolume: f32,
    pub mMaximumVolume: f32,
    pub mStartVolume: f32,
    /// FadeIn multiplies the volume by it
    pub mFadeUp: f32,
    pub mFadeDown: f32,
    pub mPan: f32,
    pub mPitch: f32,
    pub mF38: f32,
    pub mF3C: f32,
    /// ms
    pub mInterval: c_long,
    pub mNextTime: c_long,
    pub mIsLooping: bool,
}

impl Default for EeAppAmbientSoundFields {
    /// The constructor (0x124b34): silent, no sound, full probability, normal pitch.
    fn default() -> Self {
        EeAppAmbientSoundFields {
            mSoundID: 0,
            mSoundIndex: 0,
            mProbability: 1.0,
            mVolume: 0.0,
            mTargetVolume: 0.0,
            mMaximumVolume: 0.0,
            mStartVolume: 0.0,
            mFadeUp: 0.0,
            mFadeDown: 0.0,
            mPan: 0.0,
            mPitch: 1.0,
            mF38: 0.0,
            mF3C: 1.0,
            mInterval: 0,
            mNextTime: 0,
            mIsLooping: false,
        }
    }
}

/// The database record's fields `Init` copies (EEAmbientSoundRecord).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppAmbientRecord {
    pub sound_id: i32,
    pub maximum_volume: f32,
    pub start_volume: f32,
    pub fade_up: f32,
    pub fade_down: f32,
    pub probability: f32,
    pub interval_seconds: i32,
}

/// The ambient sounds' C++ side. A slot is the index of a sound in the manager's vector; no call touches the
/// records.
pub trait AmbientHost {
    fn sounds(&mut self) -> usize;
    /// the slot's record (the C++ object's base)
    fn sound(&mut self, slot: usize) -> *mut EeAppAmbientSoundFields;
    /// `mRandom.Generate()`
    fn random(&mut self) -> f32;
    /// `gSoundManager->Play(the slot's sample, volume, pan, pitch, false, NULL)`
    fn play(&mut self, slot: usize, volume: f32, pan: f32, pitch: f32);
    /// whether the slot has a playing instance
    fn has_instance(&mut self, slot: usize) -> bool;
    /// `mInstance = gSoundManager->PlayAndReturnReferencedNode(sample, volume, pan, pitch, looping, NULL)`
    fn start(&mut self, slot: usize, volume: f32, pan: f32, pitch: f32, looping: bool);
    /// `gSoundManager->SetSoundParameters(mInstance, volume, pan, pitch)`
    fn set_parameters(&mut self, slot: usize, volume: f32, pan: f32, pitch: f32);
    /// `gSoundManager->Stop(mInstance, true)` and the instance released
    fn stop_instance(&mut self, slot: usize);
    /// `LoadSound`'s object: true when the slot has (or now gets) a sound object; `looping` its loop flag when it was
    /// loaded now (None when it had one already)
    fn load_sound(&mut self, slot: usize) -> (bool, Option<bool>);
    /// `UnloadSound`: the media unloaded, the object forgotten
    fn unload_sound(&mut self, slot: usize);
    /// `gWorld.GetTimeOfDay()`, `TMGetTime()`
    fn time_of_day(&mut self) -> f32;
    fn time(&mut self) -> u32;
    /// the terrain mesh's visible tiles, water and space percentages
    fn visible_tiles(&mut self) -> u32;
    fn visible_water(&mut self) -> f32;
    fn visible_space(&mut self) -> f32;
}

/// One slot's record behind its pointer: copied out, changed, stored back, never held across a host call.
fn get<H: AmbientHost + ?Sized>(h: &mut H, slot: usize) -> EeAppAmbientSoundFields {
    // SAFETY: the host's pointer is a live record for the call
    unsafe { *h.sound(slot) }
}
fn put<H: AmbientHost + ?Sized>(h: &mut H, slot: usize, s: &EeAppAmbientSoundFields) {
    // SAFETY: as get
    unsafe { *h.sound(slot) = *s }
}

/// `EEAmbientSound::Init`: the slot from its database record, silent.
pub fn init(s: &mut EeAppAmbientSoundFields, r: &EeAppAmbientRecord) {
    s.mSoundIndex = r.sound_id as c_long;
    s.mFadeUp = r.fade_up;
    s.mFadeDown = r.fade_down;
    s.mSoundID = r.sound_id as c_long;
    s.mStartVolume = r.start_volume;
    s.mMaximumVolume = r.maximum_volume;
    s.mProbability = r.probability;
    s.mTargetVolume = 0.0;
    s.mVolume = 0.0;
    s.mIsLooping = false;
    s.mNextTime = 0;
    s.mInterval = if r.interval_seconds != 0 { (r.interval_seconds as c_long).wrapping_mul(1000) } else { 0 };
}

/// `SetTargetVolume`: kept within 0 .. the maximum.
pub fn set_target_volume<H: AmbientHost + ?Sized>(h: &mut H, slot: usize, volume: f32) {
    let mut s = get(h, slot);
    let mut v = volume;
    if v <= s.mMaximumVolume {
        if v < 0.0 {
            v = 0.0;
        }
    } else {
        v = s.mMaximumVolume;
    }
    s.mTargetVolume = v;
    put(h, slot, &s);
}

/// `PlayAtVolume`: plays at a volume (starting at the start volume, or updating the playing instance); 0 or less
/// stops the sound.
pub fn play_at_volume<H: AmbientHost + ?Sized>(h: &mut H, slot: usize, volume: f32) {
    let mut s = get(h, slot);
    if volume > 0.0 {
        if h.has_instance(slot) {
            s.mVolume = volume;
            put(h, slot, &s);
            h.set_parameters(slot, volume, s.mPan, s.mPitch);
        } else {
            s.mVolume = s.mStartVolume;
            put(h, slot, &s);
            h.start(slot, s.mVolume, s.mPan, s.mPitch, s.mIsLooping);
        }
    } else {
        if h.has_instance(slot) {
            h.stop_instance(slot);
        }
        s.mTargetVolume = 0.0;
        s.mVolume = 0.0;
        put(h, slot, &s);
    }
}

/// `FadeIn`: one step up toward the target (from silence straight to the start volume).
fn fade_in<H: AmbientHost + ?Sized>(h: &mut H, slot: usize) {
    let s = get(h, slot);
    let mut v = if s.mVolume != 0.0 { s.mFadeUp * s.mVolume } else { s.mStartVolume };
    if v > s.mTargetVolume {
        v = s.mTargetVolume;
    }
    play_at_volume(h, slot, v);
}

/// `FadeOut`: one step down toward the target, silent at 0.005 or below.
fn fade_out<H: AmbientHost + ?Sized>(h: &mut H, slot: usize) {
    let s = get(h, slot);
    let mut v = s.mFadeDown * s.mVolume;
    if v < s.mTargetVolume {
        v = s.mTargetVolume;
    }
    if v <= SILENCE {
        v = 0.0;
    }
    play_at_volume(h, slot, v);
}

/// `UpdateFade`.
fn update_fade<H: AmbientHost + ?Sized>(h: &mut H, slot: usize) {
    let s = get(h, slot);
    if s.mVolume != s.mTargetVolume {
        if s.mVolume < s.mTargetVolume {
            fade_in(h, slot);
        } else {
            fade_out(h, slot);
        }
    }
}

/// `EEAmbientSound::Stop`.
pub fn stop<H: AmbientHost + ?Sized>(h: &mut H, slot: usize) {
    if h.has_instance(slot) {
        h.stop_instance(slot);
    }
    let mut s = get(h, slot);
    s.mVolume = 0.0;
    s.mTargetVolume = 0.0;
    put(h, slot, &s);
}

/// `LoadSound`: the sound object on first use (its loop flag taken); whether the slot has one.
pub fn load_sound<H: AmbientHost + ?Sized>(h: &mut H, slot: usize) -> bool {
    let (has, looping) = h.load_sound(slot);
    if let Some(l) = looping {
        let mut s = get(h, slot);
        s.mIsLooping = l;
        put(h, slot, &s);
    }
    has
}

fn random_range<H: AmbientHost + ?Sized>(h: &mut H, low: f32, high: f32) -> f32 {
    h.random() * (high - low) + low
}

/// `RandomInteger`: between two bounds, inclusive (the C cast's truncation).
fn random_integer<H: AmbientHost + ?Sized>(h: &mut H, low: c_long, high: c_long) -> c_long {
    let limit = high.wrapping_add(1);
    let r = h.random() * (limit.wrapping_sub(low) as f32) + low as f32;
    let mut result = r as c_long;
    if result == limit {
        result = high;
    }
    result
}

/// The day phase of a time of day: 3 day (before dusk), 2 dusk, 4 night (before dawn), 1 dawn.
pub fn phase(time_of_day: f32) -> c_long {
    if time_of_day < 0.5 {
        if time_of_day < DUSK_TIME {
            3
        } else {
            2
        }
    } else if time_of_day < DAWN_TIME {
        4
    } else {
        1
    }
}

/// `UpdateState`: the phase taken and its handler run (1 dawn's wind up, 2 dusk's, 3 the day's birds, 4 the
/// night's). The original's handler names were swapped against their phases: the bodies are kept with their cases.
pub fn update_state<H: AmbientHost + ?Sized>(
    m: &mut EeAppAmbientFields,
    c: &EeAppAmbientCounts,
    h: &mut H,
    tod: f32,
    time: c_long,
) {
    m.mState = phase(tod);
    match m.mState {
        1 => update_night(m, c, h, tod, time),
        2 => update_dusk(h, tod),
        3 => update_day(c, h, tod, time),
        4 => update_dawn(h),
        _ => {}
    }
}

/// `CheckState`: the phase's handler when the phase changed.
fn check_state<H: AmbientHost + ?Sized>(
    m: &mut EeAppAmbientFields,
    c: &EeAppAmbientCounts,
    h: &mut H,
    tod: f32,
    time: c_long,
) {
    if phase(tod) != m.mState {
        update_state(m, c, h, tod, time);
    }
}

/// A bird call's volume and pan: full and centred when farms are visible, else random.
fn bird_mix<H: AmbientHost + ?Sized>(c: &EeAppAmbientCounts, h: &mut H) -> (f32, f32) {
    if c.mNumFarms == 0 {
        let v = random_range(h, 0.35, 0.6);
        let p = random_range(h, -1.0, 1.0);
        (v, p)
    } else {
        (1.0, 0.0)
    }
}

/// `UpdateDay` (state 3): the wind silenced; early in the day, when due and the chance allows, a bird call.
fn update_day<H: AmbientHost + ?Sized>(c: &EeAppAmbientCounts, h: &mut H, tod: f32, time: c_long) {
    set_target_volume(h, 7, 0.0);
    let bird = get(h, 9);
    if !(tod <= NIGHT_TIME) || !(bird.mNextTime <= time) {
        return;
    }
    let r = h.random();
    if !(bird.mProbability >= r) {
        return;
    }
    let (volume, pan) = bird_mix(c, h);
    let pitch = random_range(h, 0.9, BIRD_PITCH_HIGH);
    h.play(9, bird.mMaximumVolume * volume, pan, pitch);
    let mut bird = get(h, 9);
    bird.mNextTime = bird.mInterval.wrapping_add(time);
    put(h, 9, &bird);
}

/// `UpdateDawn` (state 4): the wind up to its maximum.
fn update_dawn<H: AmbientHost + ?Sized>(h: &mut H) {
    let wind = get(h, 7);
    set_target_volume(h, 7, wind.mMaximumVolume);
}

/// `UpdateNight` (state 1): the wind with the time of night; when due and the chance allows, a bird call, the next
/// after a random part of its interval.
fn update_night<H: AmbientHost + ?Sized>(
    _m: &mut EeAppAmbientFields,
    c: &EeAppAmbientCounts,
    h: &mut H,
    tod: f32,
    time: c_long,
) {
    let wind = get(h, 7);
    let mut volume = tod * wind.mMaximumVolume;
    // the CRT's max(0.0f, v)
    volume = if 0.0 > volume { 0.0 } else { volume };
    if wind.mTargetVolume != volume {
        set_target_volume(h, 7, volume);
    }
    let bird = get(h, 9);
    if !(bird.mNextTime <= time) {
        return;
    }
    let r = h.random();
    if !(r <= bird.mProbability) {
        return;
    }
    let (scale, pan) = bird_mix(c, h);
    let pitch = random_range(h, 0.9, BIRD_PITCH_HIGH);
    h.play(9, bird.mMaximumVolume * scale, pan, pitch);
    let mut bird = get(h, 9);
    bird.mNextTime = ((h.random() * bird.mInterval as f32) as c_long).wrapping_add(time);
    put(h, 9, &bird);
}

/// `UpdateDusk` (state 2): the wind rising with the time of day between its start and maximum volume.
fn update_dusk<H: AmbientHost + ?Sized>(h: &mut H, tod: f32) {
    let wind = get(h, 7);
    let factor = ((tod + tod) - 0.9) * 10.0;
    // the CRT's min(maximum, max(start, maximum * factor))
    let inner =
        if wind.mStartVolume > wind.mMaximumVolume * factor { wind.mStartVolume } else { wind.mMaximumVolume * factor };
    let volume = if wind.mMaximumVolume < inner { wind.mMaximumVolume } else { inner };
    if wind.mTargetVolume != volume {
        set_target_volume(h, 7, volume);
    }
}

/// The trees' share of the visible tiles.
fn tree_ratio<H: AmbientHost + ?Sized>(c: &EeAppAmbientCounts, h: &mut H) -> f32 {
    let tiles = h.visible_tiles();
    if tiles != 0 {
        c.mNumTrees as f32 / tiles as f32
    } else {
        0.0
    }
}

/// A call when due and the chance allows: the next time set first, then the sound at a random part of its maximum
/// (the farm's, goat's, eagle's and the space sounds').
fn chance_call<H: AmbientHost + ?Sized>(h: &mut H, slot: usize, time: c_long, low: f32, high: f32) {
    let s = get(h, slot);
    if !(s.mNextTime <= time) {
        return;
    }
    let r = h.random();
    if r <= s.mProbability {
        let mut s = s;
        s.mNextTime = s.mInterval.wrapping_add(time);
        put(h, slot, &s);
        let volume = s.mMaximumVolume;
        let pitch = random_range(h, low, high);
        let pan = random_range(h, -1.0, 1.0);
        let v = h.random() * volume;
        h.play(slot, v, pan, pitch);
    }
}

/// `UpdateAnimals`: the dawn and dusk animal calls and crickets, the citizens' chatter (more than 3 visible) when due.
fn update_animals<H: AmbientHost + ?Sized>(m: &EeAppAmbientFields, c: &EeAppAmbientCounts, h: &mut H, time: c_long) {
    let ratio = tree_ratio(c, h);
    if m.mState == 4 || m.mState == 2 {
        let s = get(h, 10);
        if s.mNextTime <= time {
            let r = h.random();
            // play with chance p, as every other ambient sound (the test was reversed: chance 1 - p)
            if r <= s.mProbability {
                let (volume, pan) = if ratio >= 0.11 {
                    (s.mMaximumVolume * random_range(h, 1.0, 1.3), 0.0)
                } else {
                    let v = s.mMaximumVolume * random_range(h, 0.25, 0.65);
                    (v, random_range(h, -1.0, 1.0))
                };
                let pitch = random_range(h, 0.9, ANIMAL_PITCH_HIGH);
                h.play(10, volume, pan, pitch);
                let mut s = get(h, 10);
                s.mNextTime = s.mInterval.wrapping_add(time);
                put(h, 10, &s);
            }
        }
    }
    if m.mState == 4 || m.mState == 2 {
        let s = get(h, 14);
        if s.mNextTime <= time {
            let r = h.random();
            // as above
            if r <= s.mProbability {
                let pan = random_range(h, -1.0, 1.0);
                let pitch = random_range(h, CRICKET_PITCH_LOW, CRICKET_PITCH_HIGH);
                h.play(14, s.mMaximumVolume, pan, pitch);
                let mut s = get(h, 14);
                s.mNextTime = s.mInterval.wrapping_add(time);
                put(h, 14, &s);
            }
        }
    }
    if !(c.mNumCitizens > 3) {
        return;
    }
    let s = get(h, 18);
    if !(s.mNextTime <= time) {
        return;
    }
    if s.mInterval.wrapping_add(12000) < time.wrapping_sub(s.mNextTime) {
        let next = random_integer(h, 100, s.mInterval).wrapping_add(time);
        let mut s = get(h, 18);
        s.mNextTime = next;
        put(h, 18, &s);
    } else {
        let r = h.random();
        // as above
        if r <= s.mProbability {
            let pan = random_range(h, -1.0, 1.0);
            let pitch = random_range(h, 0.6, 1.2);
            h.play(18, s.mMaximumVolume, pan, pitch);
            let mut s = get(h, 18);
            s.mNextTime = s.mInterval.wrapping_add(time);
            put(h, 18, &s);
        }
    }
}

/// The target volume set to 0 when the slot has an instance.
fn quiet_if_playing<H: AmbientHost + ?Sized>(h: &mut H, slot: usize) {
    if h.has_instance(slot) {
        set_target_volume(h, slot, 0.0);
    }
}

/// `UpdateWater`: one forest sound (by the forest type) fades in when trees fill the view by day or night, the others
/// out; whether a forest sound plays.
fn update_forest<H: AmbientHost + ?Sized>(
    m: &EeAppAmbientFields,
    c: &EeAppAmbientCounts,
    h: &mut H,
    time: c_long,
    ratio: f32,
) -> bool {
    if (m.mState == 3 || m.mState == 1) && (ratio >= 0.11 || c.mNumTrees >= 160) {
        let (sound, other1, other2) = match c.mForestType {
            3 => (22, 21, 20),
            2 => (21, 20, 22),
            _ => (20, 21, 22),
        };
        let s = get(h, sound);
        let mut volume = s.mTargetVolume;
        if volume == 0.0 || s.mNextTime < time {
            let mut s = s;
            s.mNextTime = s.mInterval.wrapping_add(time);
            put(h, sound, &s);
            volume = random_range(h, 0.05, s.mMaximumVolume + HEIGHT_EPSILON);
        }
        set_target_volume(h, sound, volume);
        quiet_if_playing(h, other1);
        quiet_if_playing(h, other2);
        true
    } else {
        quiet_if_playing(h, 20);
        quiet_if_playing(h, 21);
        quiet_if_playing(h, 22);
        false
    }
}

/// `UpdateTerrain`: the crowd and battle noise, the farm, goat and eagle calls, the air raid sirens, the wind over
/// water, the space sounds, the forest sounds and the quiet background, from the visible units and tiles.
fn update_terrain<H: AmbientHost + ?Sized>(m: &EeAppAmbientFields, c: &EeAppAmbientCounts, h: &mut H, time: c_long) {
    let water = h.visible_water();
    let space = h.visible_space();
    let ratio = tree_ratio(c, h);
    if c.mNumCitizens >= 4 {
        let base = get(h, 1).mStartVolume;
        set_target_volume(h, 1, c.mNumCitizens as f32 * 0.02 + base);
    } else {
        quiet_if_playing(h, 1);
    }
    if c.mNumAttackers >= 5 {
        set_target_volume(h, 1, 0.0);
        let base = get(h, 2).mStartVolume;
        set_target_volume(h, 2, c.mNumAttackers as f32 * 0.02 + base);
    } else {
        quiet_if_playing(h, 2);
    }
    let day = m.mState == 3 || m.mState == 1;
    if c.mNumFarms != 0 && day {
        chance_call(h, 11, time, TERRAIN_PITCH_LOW3, TERRAIN_PITCH_HIGH3);
    }
    if c.mNumGoats != 0 && day {
        chance_call(h, 19, time, TERRAIN_PITCH_LOW2, TERRAIN_PITCH_HIGH2);
    }
    if c.mNumEagles != 0 && day {
        chance_call(h, 15, time, TERRAIN_PITCH_LOW1, TERRAIN_PITCH_HIGH1);
    }
    if c.mNumAirRaiders == 0 {
        if get(h, 8).mTargetVolume != 0.0 {
            set_target_volume(h, 8, 0.0);
        }
    } else {
        let v = get(h, 8).mMaximumVolume;
        set_target_volume(h, 8, v);
    }
    let windy = water > 0.7 || (water > 0.2 && water > ratio * 1.5);
    if windy {
        let v = get(h, 3).mMaximumVolume;
        set_target_volume(h, 3, v);
    } else {
        quiet_if_playing(h, 3);
    }
    let spacey = space > 0.7 || (space > 0.2 && space > ratio * 1.5);
    if spacey {
        let (a, b) = (get(h, 26), get(h, 27));
        if a.mTargetVolume == 0.0 && b.mTargetVolume == 0.0 {
            let r = h.random();
            if r <= a.mProbability {
                set_target_volume(h, 26, a.mMaximumVolume);
            } else {
                set_target_volume(h, 27, b.mMaximumVolume);
            }
        }
        for slot in 23..=25 {
            chance_call(h, slot, time, TERRAIN_PITCH_LOW1, TERRAIN_PITCH_HIGH1);
        }
    } else if day {
        let s = get(h, 12);
        if s.mNextTime <= time {
            let r = h.random();
            // play with chance p, as every other ambient sound (the test was reversed: chance 1 - p)
            if r <= s.mProbability {
                let volume = s.mMaximumVolume;
                let pitch = random_range(h, 0.8, ANIMAL_PITCH_HIGH);
                let pan = random_range(h, -1.0, 1.0);
                let v = h.random() * volume;
                h.play(12, v, pan, pitch);
                let mut s = get(h, 12);
                s.mNextTime = s.mInterval.wrapping_add(time);
                put(h, 12, &s);
            }
        }
    }
    let quiet = update_forest(m, c, h, time, ratio) || windy || spacey || m.mState == 4 || m.mState == 2;
    let v = if quiet { 0.0 } else { get(h, 13).mMaximumVolume };
    set_target_volume(h, 13, v);
}

/// `Update`: one ambient update (the phase, the terrain and animal sounds, the fades), then the world counts cleared.
pub fn update<H: AmbientHost + ?Sized>(m: &mut EeAppAmbientFields, c: &mut EeAppAmbientCounts, h: &mut H) {
    let tod = h.time_of_day();
    let time = h.time() as c_long;
    check_state(m, c, h, tod, time);
    update_terrain(m, c, h, time);
    update_animals(m, c, h, time);
    let mut i = h.sounds() as isize - 1;
    while i > 0 {
        update_fade(h, i as usize);
        i -= 1;
    }
    clear(c);
    c.mForestType = 0;
    if m.mState == 2 {
        m.mState = 0;
    }
    m.mNextUpdateFrame = m.mUpdateInterval.wrapping_add(m.mFrameCount);
}

/// `EEAmbientWorldAnalyzer::Clear`: the counts zeroed for the next update (the forest type kept).
pub fn clear(c: &mut EeAppAmbientCounts) {
    let forest = c.mForestType;
    *c = EeAppAmbientCounts { mForestType: forest, ..Default::default() };
}

/// `SetEnabled`: every sound object loaded, or the sounds silenced and unloaded (slot 0 is skipped).
pub fn set_enabled<H: AmbientHost + ?Sized>(m: &mut EeAppAmbientFields, h: &mut H, enabled: bool) {
    if m.mIsEnabled == enabled {
        return;
    }
    m.mIsEnabled = enabled;
    let mut i = h.sounds() as isize - 1;
    while i > 0 {
        if !enabled {
            play_at_volume(h, i as usize, 0.0);
            h.unload_sound(i as usize);
        } else {
            load_sound(h, i as usize);
        }
        i -= 1;
    }
}

/// `StopSounds`: every sound stopped.
pub fn stop_sounds<H: AmbientHost + ?Sized>(h: &mut H) {
    let mut i = h.sounds() as isize - 1;
    while i >= 0 {
        stop(h, i as usize);
        i -= 1;
    }
}

/// `Pause`: every sound stopped, the manager paused.
pub fn pause<H: AmbientHost + ?Sized>(m: &mut EeAppAmbientFields, h: &mut H) {
    stop_sounds(h);
    m.mIsPaused = true;
}

/// `Restart`: unpaused, the next update straight away.
pub fn restart(m: &mut EeAppAmbientFields) {
    m.mIsPaused = false;
    m.mFrameCount = m.mNextUpdateFrame;
}
