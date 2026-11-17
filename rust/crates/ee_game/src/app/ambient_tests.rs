//! ambient.rs over a mock sound system: 28 slots, a scripted random sequence, the plays and instances logged.

use crate::app::ambient::*;

struct Sounds {
    slots: Vec<EeAppAmbientSoundFields>,
    instances: Vec<bool>,
    objects: Vec<bool>,
    randoms: Vec<f32>,
    next: usize,
    log: Vec<String>,
    tod: f32,
    time: u32,
    tiles: u32,
    water: f32,
    space: f32,
}

impl Sounds {
    fn new() -> Self {
        let mut slots = vec![EeAppAmbientSoundFields::default(); 28];
        for (i, s) in slots.iter_mut().enumerate() {
            init(
                s,
                &EeAppAmbientRecord {
                    sound_id: i as i32,
                    maximum_volume: 0.8,
                    start_volume: 0.1,
                    fade_up: 2.0,
                    fade_down: 0.5,
                    probability: 0.5,
                    interval_seconds: 10,
                },
            );
        }
        Sounds {
            slots,
            instances: vec![false; 28],
            objects: vec![false; 28],
            randoms: vec![0.25],
            next: 0,
            log: Vec::new(),
            tod: 0.3,
            time: 100_000,
            tiles: 100,
            water: 0.0,
            space: 0.0,
        }
    }
}

impl AmbientHost for Sounds {
    fn sounds(&mut self) -> usize {
        self.slots.len()
    }
    fn sound(&mut self, slot: usize) -> *mut EeAppAmbientSoundFields {
        &mut self.slots[slot]
    }
    fn random(&mut self) -> f32 {
        let r = self.randoms[self.next % self.randoms.len()];
        self.next += 1;
        r
    }
    fn play(&mut self, slot: usize, volume: f32, pan: f32, pitch: f32) {
        self.log.push(format!("play {slot} {volume:.3} {pan:.3} {pitch:.3}"));
    }
    fn has_instance(&mut self, slot: usize) -> bool {
        self.instances[slot]
    }
    fn start(&mut self, slot: usize, volume: f32, _pan: f32, _pitch: f32, looping: bool) {
        self.instances[slot] = true;
        self.log.push(format!("start {slot} {volume:.3} {looping}"));
    }
    fn set_parameters(&mut self, slot: usize, volume: f32, _pan: f32, _pitch: f32) {
        self.log.push(format!("set {slot} {volume:.3}"));
    }
    fn stop_instance(&mut self, slot: usize) {
        self.instances[slot] = false;
        self.log.push(format!("stop {slot}"));
    }
    fn load_sound(&mut self, slot: usize) -> (bool, Option<bool>) {
        if self.objects[slot] {
            return (true, None);
        }
        self.objects[slot] = true;
        (true, Some(slot == 7))
    }
    fn unload_sound(&mut self, slot: usize) {
        self.objects[slot] = false;
    }
    fn time_of_day(&mut self) -> f32 {
        self.tod
    }
    fn time(&mut self) -> u32 {
        self.time
    }
    fn visible_tiles(&mut self) -> u32 {
        self.tiles
    }
    fn visible_water(&mut self) -> f32 {
        self.water
    }
    fn visible_space(&mut self) -> f32 {
        self.space
    }
}

#[test]
fn init_and_the_day_phases() {
    let s = Sounds::new();
    assert_eq!((s.slots[3].mInterval, s.slots[3].mSoundIndex, s.slots[3].mProbability), (10000, 3, 0.5));
    assert_eq!((phase(0.1), phase(0.46), phase(0.6), phase(0.96)), (3, 2, 4, 1));
}

#[test]
fn enabling_loads_every_sound_but_the_first_and_takes_the_loop_flag() {
    let mut s = Sounds::new();
    let mut m = EeAppAmbientFields::default();
    set_enabled(&mut m, &mut s, true);
    assert!(m.mIsEnabled);
    assert!(!s.objects[0] && s.objects[1] && s.objects[27]);
    assert!(s.slots[7].mIsLooping && !s.slots[8].mIsLooping);
    // disabling silences and unloads
    s.instances[5] = true;
    set_enabled(&mut m, &mut s, false);
    assert!(!s.instances[5] && !s.objects[5]);
    assert!(s.log.contains(&"stop 5".to_string()));
}

#[test]
fn fades_step_toward_the_target() {
    let mut s = Sounds::new();
    set_target_volume(&mut s, 3, 5.0);
    assert_eq!(s.slots[3].mTargetVolume, 0.8);
    set_target_volume(&mut s, 3, -1.0);
    assert_eq!(s.slots[3].mTargetVolume, 0.0);
    // over water the wind's target is its maximum
    s.water = 0.8;
    let mut m = EeAppAmbientFields { mIsEnabled: true, ..Default::default() };
    let mut c = EeAppAmbientCounts::default();
    // the start volume first, then doubling up to the target
    update(&mut m, &mut c, &mut s);
    assert_eq!(s.slots[3].mVolume, 0.1);
    update(&mut m, &mut c, &mut s);
    assert_eq!(s.slots[3].mVolume, 0.2);
    for _ in 0..3 {
        update(&mut m, &mut c, &mut s);
    }
    assert_eq!(s.slots[3].mVolume, 0.8);
    // out of view, the target falls to 0 and the sound fades out
    s.water = 0.0;
    for _ in 0..10 {
        update(&mut m, &mut c, &mut s);
    }
    assert_eq!(s.slots[3].mVolume, 0.0);
    assert!(!s.instances[3]);
}

#[test]
fn the_day_plays_farm_calls_with_their_chance() {
    let mut s = Sounds::new();
    let mut m = EeAppAmbientFields::default();
    let mut c = EeAppAmbientCounts { mNumFarms: 2, ..Default::default() };
    update(&mut m, &mut c, &mut s);
    assert_eq!(m.mState, 3);
    assert!(s.log.iter().any(|l| l.starts_with("play 11 ")));
    assert_eq!(s.slots[11].mNextTime, 110_000);
    // the counts are cleared after the update
    assert_eq!(c.mNumFarms, 0);
    assert_eq!(m.mNextUpdateFrame, 10);
    // a draw above the chance plays nothing
    let mut s = Sounds::new();
    s.randoms = vec![0.9];
    let mut c = EeAppAmbientCounts { mNumFarms: 2, ..Default::default() };
    update(&mut m, &mut c, &mut s);
    assert!(!s.log.iter().any(|l| l.starts_with("play 11 ")));
}

#[test]
fn forests_pick_their_sound_by_kind() {
    let mut s = Sounds::new();
    let mut m = EeAppAmbientFields::default();
    let mut c = EeAppAmbientCounts { mNumTrees: 50, mForestType: 3, ..Default::default() };
    update(&mut m, &mut c, &mut s);
    assert!(s.slots[22].mTargetVolume > 0.0);
    assert_eq!(s.slots[20].mTargetVolume, 0.0);
    // the quiet background is off while a forest sound plays
    assert_eq!(s.slots[13].mTargetVolume, 0.0);
    let mut c = EeAppAmbientCounts::default();
    update(&mut m, &mut c, &mut s);
    assert_eq!(s.slots[13].mTargetVolume, 0.8);
}

#[test]
fn stop_sounds_and_pause() {
    let mut s = Sounds::new();
    let mut m = EeAppAmbientFields { mNextUpdateFrame: 40, ..Default::default() };
    s.instances[0] = true;
    s.instances[27] = true;
    s.slots[27].mVolume = 0.5;
    pause(&mut m, &mut s);
    assert!(m.mIsPaused && !s.instances[0] && !s.instances[27]);
    assert_eq!(s.slots[27].mVolume, 0.0);
    restart(&mut m);
    assert!(!m.mIsPaused);
    assert_eq!(m.mFrameCount, 40);
}
