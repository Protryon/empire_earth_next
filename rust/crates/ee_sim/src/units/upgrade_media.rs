//! A player's unit class copy's media (`EEUnitClassUpgrade`, EEUnitClassUpgrade.cpp 0x23b61a..0x23c332; the groups
//! lane, round 1): the media and button load counts, the loads and unloads of its sounds, model, button and
//! area-effect templates and of its class's media and buttons, the media setters, and the two sounds' round robins.
//! The C++ object keeps its members (the eight media pointers `mButton` .. `mSelectionSounds[1]`, contiguous, and the
//! two busy flags); its counts are the Rust record's (`EeUnitsUnitClassUpgradeFields::mMediaCount`,
//! `mButtonLoadCount`). Each C++ call is one operation of [`MediaHost`]: the objects' loads and unloads, the two
//! database locks, `PlatSleep(1)`.

use std::os::raw::{c_long, c_ulong};
use std::sync::atomic::{AtomicU32, Ordering};

/// The media slots (the order of the C++ members).
pub const EE_UNITS_MEDIA_BUTTON: usize = 0;
pub const EE_UNITS_MEDIA_MODEL: usize = 1;
pub const EE_UNITS_MEDIA_CREATION: usize = 2;
pub const EE_UNITS_MEDIA_DEATH: usize = 3;
pub const EE_UNITS_MEDIA_ACKNOWLEDGE: usize = 4;
pub const EE_UNITS_MEDIA_SELECTION: usize = 6;

/// What a media's load or unload is (the object's kind).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Media {
    /// a sound (`EESoundObject::LoadMedia`, `UnloadMedia`)
    Sound,
    /// the model (`EEModel::LoadMedia`, `UnloadMedia`)
    Model,
    /// the button (`EEButtonObject::Acquire`, `Release`)
    Button,
}

/// The two locks (`sDatabaseLock`, the unit's own `sUnitClassUpgradeLock`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Lock {
    Database,
    Upgrade,
}

/// The C++ the media bodies call.
pub trait MediaHost {
    /// the media slot's pointer and its store
    fn slot(&mut self, slot: usize) -> u64;
    fn set_slot(&mut self, slot: usize, value: u64);
    /// `mMediaCount`, `mButtonLoadCount` and their stores
    fn counts(&mut self) -> (c_long, c_long);
    fn set_counts(&mut self, media: c_long, buttons: c_long);
    /// the volatile busy flags `mIsLoadingMedia` (0), `mIsLoadingButtons` (1) and their stores
    fn busy(&mut self, which: u32) -> bool;
    fn set_busy(&mut self, which: u32, value: bool);
    /// the object's load (the sound's result: nonzero failed) and unload
    fn load(&mut self, kind: Media, object: u64) -> c_long;
    fn unload(&mut self, kind: Media, object: u64);
    /// `gAEManager.LoadTemplateMedia(t)` / `UnloadTemplateMedia(t)` of the four templates
    fn templates(&mut self, load: bool);
    /// the class's `LoadMedia(player)`, `UnloadMedia(player)`, `LoadButtons(player)`, `UnloadButtons(player)`
    fn class_media(&mut self, load: bool, buttons: bool);
    fn enter(&mut self, lock: Lock);
    fn leave(&mut self, lock: Lock);
    /// `PlatSleep(1)`
    fn sleep(&mut self);
}

/// `kMedia*` (ee/EEUnitClassIDs.h)
const ACKNOWLEDGE_1: c_long = 1;
const ACKNOWLEDGE_2: c_long = 2;
const CREATION: c_long = 4;
const DEATH: c_long = 8;
const SELECTION_1: c_long = 0x10;
const SELECTION_2: c_long = 0x20;
const MODEL: c_long = 0x40;

const BUSY_MEDIA: u32 = 0;
const BUSY_BUTTONS: u32 = 1;

fn kind_of(slot: usize) -> Media {
    match slot {
        EE_UNITS_MEDIA_BUTTON => Media::Button,
        EE_UNITS_MEDIA_MODEL => Media::Model,
        _ => Media::Sound,
    }
}

fn wait_while<H: MediaHost + ?Sized>(h: &mut H, which: u32) {
    while h.busy(which) {
        h.sleep();
    }
}

/// The setters (`SetButton`, `SetModel`, `SetCreationSound`, `SetDeathSound`, `SetAcknowledgeSound(s, i)`,
/// `SetSelectionSound(s, i)`: `slot` is the member): after the media load, under the database lock, while media is
/// loaded the old one unloaded and the new one loaded.
pub fn set<H: MediaHost + ?Sized>(h: &mut H, slot: usize, value: u64) {
    wait_while(h, BUSY_MEDIA);
    h.enter(Lock::Database);
    let kind = kind_of(slot);
    let old = h.slot(slot);
    if h.counts().0 > 0 && old != 0 {
        h.unload(kind, old);
    }
    h.set_slot(slot, value);
    if h.counts().0 > 0 && value != 0 {
        h.load(kind, value);
    }
    h.leave(Lock::Database);
}

/// `LoadMedia(player, flags)`: the first request loads the chosen sounds and model (a sound that fails is dropped),
/// the button, the templates and the class's media; later requests are counted and wait for the first.
pub fn load_media<H: MediaHost + ?Sized>(h: &mut H, flags: c_long) {
    h.enter(Lock::Database);
    let (media, buttons) = h.counts();
    if media == 0 {
        h.set_counts(1, buttons);
        h.set_busy(BUSY_MEDIA, true);
        h.leave(Lock::Database);
        let sounds = [
            (ACKNOWLEDGE_1, EE_UNITS_MEDIA_ACKNOWLEDGE),
            (ACKNOWLEDGE_2, EE_UNITS_MEDIA_ACKNOWLEDGE + 1),
            (SELECTION_1, EE_UNITS_MEDIA_SELECTION),
            (SELECTION_2, EE_UNITS_MEDIA_SELECTION + 1),
            (DEATH, EE_UNITS_MEDIA_DEATH),
            (CREATION, EE_UNITS_MEDIA_CREATION),
        ];
        for (flag, slot) in sounds {
            if flags & flag != 0 {
                let s = h.slot(slot);
                if s != 0 && h.load(Media::Sound, s) != 0 {
                    h.set_slot(slot, 0);
                }
            }
        }
        if flags & MODEL != 0 {
            let m = h.slot(EE_UNITS_MEDIA_MODEL);
            if m != 0 {
                h.load(Media::Model, m);
            }
        }
        let button = h.slot(EE_UNITS_MEDIA_BUTTON);
        h.load(Media::Button, button);
        h.templates(true);
        h.class_media(true, false);
        h.enter(Lock::Database);
        h.set_busy(BUSY_MEDIA, false);
    } else {
        h.set_counts(media + 1, buttons);
        h.leave(Lock::Database);
        wait_while(h, BUSY_MEDIA);
        h.enter(Lock::Database);
    }
    h.leave(Lock::Database);
}

/// The unloads of every media member (sounds, then the model), as the destructor and the last UnloadMedia make them.
fn unload_members<H: MediaHost + ?Sized>(h: &mut H) {
    for slot in [
        EE_UNITS_MEDIA_ACKNOWLEDGE,
        EE_UNITS_MEDIA_ACKNOWLEDGE + 1,
        EE_UNITS_MEDIA_SELECTION,
        EE_UNITS_MEDIA_SELECTION + 1,
        EE_UNITS_MEDIA_DEATH,
        EE_UNITS_MEDIA_CREATION,
        EE_UNITS_MEDIA_MODEL,
    ] {
        let s = h.slot(slot);
        if s != 0 {
            h.unload(kind_of(slot), s);
        }
    }
}

/// `UnloadMedia(player)`: the last request unloads the sounds, the model, the button, the templates and the class's
/// media; the others are counted down.
pub fn unload_media<H: MediaHost + ?Sized>(h: &mut H) {
    wait_while(h, BUSY_MEDIA);
    h.enter(Lock::Database);
    let (media, buttons) = h.counts();
    if media != 0 {
        if media == 1 {
            h.set_counts(0, buttons);
            unload_members(h);
            let button = h.slot(EE_UNITS_MEDIA_BUTTON);
            h.unload(Media::Button, button);
            h.templates(false);
            h.class_media(false, false);
        } else {
            h.set_counts(media - 1, buttons);
        }
    }
    h.leave(Lock::Database);
}

/// `~EEUnitClassUpgrade`'s: the sounds and model unloaded while media is loaded.
pub fn destroy<H: MediaHost + ?Sized>(h: &mut H) {
    if h.counts().0 > 0 {
        unload_members(h);
    }
}

/// `LoadButtons(player)`: the first request loads the class's buttons; later ones are counted and wait.
pub fn load_buttons<H: MediaHost + ?Sized>(h: &mut H) {
    h.enter(Lock::Upgrade);
    let (media, buttons) = h.counts();
    if buttons == 0 {
        h.set_counts(media, 1);
        h.set_busy(BUSY_BUTTONS, true);
        h.leave(Lock::Upgrade);
        h.class_media(true, true);
        h.enter(Lock::Upgrade);
        h.set_busy(BUSY_BUTTONS, false);
    } else {
        h.set_counts(media, buttons + 1);
        h.leave(Lock::Upgrade);
        wait_while(h, BUSY_BUTTONS);
        h.enter(Lock::Upgrade);
    }
    h.leave(Lock::Upgrade);
}

/// `UnloadButtons(player)`: the last request unloads the class's buttons.
pub fn unload_buttons<H: MediaHost + ?Sized>(h: &mut H) {
    wait_while(h, BUSY_BUTTONS);
    h.enter(Lock::Upgrade);
    let (media, buttons) = h.counts();
    if buttons == 1 {
        h.set_counts(media, 0);
        h.class_media(false, true);
    } else {
        h.set_counts(media, buttons - 1);
    }
    h.leave(Lock::Upgrade);
}

/// The sounds' turns (`0x564460`, `0x564464`): one shared by every class each.
static ACKNOWLEDGE_TURN: AtomicU32 = AtomicU32::new(0);
static SELECTION_TURN: AtomicU32 = AtomicU32::new(0);

/// `GetAcknowledgeSound(index)` (`selection` false) and `GetSelectionSound(index)`: the slot of sound `index`; -1
/// takes the turn and alternates it (`turn = (turn - 1) & 1`).
pub fn sound_slot(selection: bool, index: c_long) -> usize {
    let index = if index == -1 {
        let turn = if selection { &SELECTION_TURN } else { &ACKNOWLEDGE_TURN };
        let i = turn.load(Ordering::Relaxed);
        turn.store(i.wrapping_sub(1) & 1, Ordering::Relaxed);
        i as c_ulong as usize
    } else {
        index as usize
    };
    (if selection { EE_UNITS_MEDIA_SELECTION } else { EE_UNITS_MEDIA_ACKNOWLEDGE }) + index
}

// ---- the game's host (EEUnitClassUpgrade.cpp)

/// What one entry point call sees of the C++ object.
#[repr(C)]
pub struct EeUnitsUpgradeMedia {
    /// `&mButton`: the eight media members
    pub slots: *mut *mut std::ffi::c_void,
    /// `&mIsLoadingMedia` (volatile; `mIsLoadingButtons` follows it)
    pub busy: *mut bool,
    /// the record (`EeUnitsUnitClassUpgradeFields`: the counts, the templates)
    pub fields: *mut crate::world::stats_fields::EeUnitsUnitClassUpgradeFields,
    /// the object and the call's player
    pub upgrade: *mut std::ffi::c_void,
    pub player: *mut std::ffi::c_void,
}

/// The entry point's operations.
pub const EE_UNITS_UPGRADE_MEDIA_SET: i32 = 0;
pub const EE_UNITS_UPGRADE_MEDIA_LOAD: i32 = 1;
pub const EE_UNITS_UPGRADE_MEDIA_UNLOAD: i32 = 2;
pub const EE_UNITS_UPGRADE_MEDIA_DESTROY: i32 = 3;
pub const EE_UNITS_UPGRADE_MEDIA_LOAD_BUTTONS: i32 = 4;
pub const EE_UNITS_UPGRADE_MEDIA_UNLOAD_BUTTONS: i32 = 5;
pub const EE_UNITS_UPGRADE_MEDIA_ACKNOWLEDGE_SOUND: i32 = 6;
pub const EE_UNITS_UPGRADE_MEDIA_SELECTION_SOUND: i32 = 7;

/// The host's operations (`ee_units_upgrade_host`).
pub const EE_UNITS_UPGRADE_OP_SOUND_LOAD: i32 = 1;
pub const EE_UNITS_UPGRADE_OP_SOUND_UNLOAD: i32 = 2;
pub const EE_UNITS_UPGRADE_OP_MODEL_LOAD: i32 = 3;
pub const EE_UNITS_UPGRADE_OP_MODEL_UNLOAD: i32 = 4;
pub const EE_UNITS_UPGRADE_OP_BUTTON_ACQUIRE: i32 = 5;
pub const EE_UNITS_UPGRADE_OP_BUTTON_RELEASE: i32 = 6;
pub const EE_UNITS_UPGRADE_OP_TEMPLATE_LOAD: i32 = 7;
pub const EE_UNITS_UPGRADE_OP_TEMPLATE_UNLOAD: i32 = 8;
pub const EE_UNITS_UPGRADE_OP_CLASS_LOAD_MEDIA: i32 = 9;
pub const EE_UNITS_UPGRADE_OP_CLASS_UNLOAD_MEDIA: i32 = 10;
pub const EE_UNITS_UPGRADE_OP_CLASS_LOAD_BUTTONS: i32 = 11;
pub const EE_UNITS_UPGRADE_OP_CLASS_UNLOAD_BUTTONS: i32 = 12;
pub const EE_UNITS_UPGRADE_OP_DATABASE_ENTER: i32 = 13;
pub const EE_UNITS_UPGRADE_OP_DATABASE_LEAVE: i32 = 14;
pub const EE_UNITS_UPGRADE_OP_UPGRADE_ENTER: i32 = 15;
pub const EE_UNITS_UPGRADE_OP_UPGRADE_LEAVE: i32 = 16;
pub const EE_UNITS_UPGRADE_OP_SLEEP: i32 = 17;

#[cfg(not(test))]
mod game {
    use super::*;
    use std::ffi::c_void;

    extern "C" {
        fn ee_units_upgrade_host(op: i32, upgrade: *mut c_void, object: *mut c_void, player: *mut c_void) -> c_long;
    }

    struct Game<'a>(&'a EeUnitsUpgradeMedia);

    impl Game<'_> {
        fn op(&mut self, op: i32, object: u64) -> c_long {
            // SAFETY: the C++ switch over the live object the entry point was handed
            unsafe { ee_units_upgrade_host(op, self.0.upgrade, object as usize as *mut c_void, self.0.player) }
        }
        fn fields(&mut self) -> &mut crate::world::stats_fields::EeUnitsUnitClassUpgradeFields {
            // SAFETY: the live object's record; the borrow ends before any host call
            unsafe { &mut *self.0.fields }
        }
    }

    impl MediaHost for Game<'_> {
        fn slot(&mut self, slot: usize) -> u64 {
            // SAFETY: one of the eight members
            unsafe { *self.0.slots.add(slot) as u64 }
        }
        fn set_slot(&mut self, slot: usize, value: u64) {
            // SAFETY: as above
            unsafe { *self.0.slots.add(slot) = value as usize as *mut c_void }
        }
        fn counts(&mut self) -> (c_long, c_long) {
            let f = self.fields();
            (f.mMediaCount, f.mButtonLoadCount)
        }
        fn set_counts(&mut self, media: c_long, buttons: c_long) {
            let f = self.fields();
            f.mMediaCount = media;
            f.mButtonLoadCount = buttons;
        }
        fn busy(&mut self, which: u32) -> bool {
            // SAFETY: the volatile flags (another loader thread writes them)
            unsafe { std::ptr::read_volatile(self.0.busy.add(which as usize)) }
        }
        fn set_busy(&mut self, which: u32, value: bool) {
            // SAFETY: as above
            unsafe { std::ptr::write_volatile(self.0.busy.add(which as usize), value) }
        }
        fn load(&mut self, kind: Media, object: u64) -> c_long {
            match kind {
                Media::Sound => self.op(EE_UNITS_UPGRADE_OP_SOUND_LOAD, object),
                Media::Model => self.op(EE_UNITS_UPGRADE_OP_MODEL_LOAD, object),
                Media::Button => self.op(EE_UNITS_UPGRADE_OP_BUTTON_ACQUIRE, object),
            }
        }
        fn unload(&mut self, kind: Media, object: u64) {
            match kind {
                Media::Sound => self.op(EE_UNITS_UPGRADE_OP_SOUND_UNLOAD, object),
                Media::Model => self.op(EE_UNITS_UPGRADE_OP_MODEL_UNLOAD, object),
                Media::Button => self.op(EE_UNITS_UPGRADE_OP_BUTTON_RELEASE, object),
            };
        }
        fn templates(&mut self, load: bool) {
            let templates = self.fields().mAreaEffectTemplates;
            for t in templates {
                let op = if load { EE_UNITS_UPGRADE_OP_TEMPLATE_LOAD } else { EE_UNITS_UPGRADE_OP_TEMPLATE_UNLOAD };
                self.op(op, t as c_ulong as u64);
            }
        }
        fn class_media(&mut self, load: bool, buttons: bool) {
            let op = match (load, buttons) {
                (true, false) => EE_UNITS_UPGRADE_OP_CLASS_LOAD_MEDIA,
                (false, false) => EE_UNITS_UPGRADE_OP_CLASS_UNLOAD_MEDIA,
                (true, true) => EE_UNITS_UPGRADE_OP_CLASS_LOAD_BUTTONS,
                (false, true) => EE_UNITS_UPGRADE_OP_CLASS_UNLOAD_BUTTONS,
            };
            self.op(op, 0);
        }
        fn enter(&mut self, lock: Lock) {
            let op = if lock == Lock::Database {
                EE_UNITS_UPGRADE_OP_DATABASE_ENTER
            } else {
                EE_UNITS_UPGRADE_OP_UPGRADE_ENTER
            };
            self.op(op, 0);
        }
        fn leave(&mut self, lock: Lock) {
            let op = if lock == Lock::Database {
                EE_UNITS_UPGRADE_OP_DATABASE_LEAVE
            } else {
                EE_UNITS_UPGRADE_OP_UPGRADE_LEAVE
            };
            self.op(op, 0);
        }
        fn sleep(&mut self) {
            self.op(EE_UNITS_UPGRADE_OP_SLEEP, 0);
        }
    }

    /// `EEUnitClassUpgrade`'s media members (`op` an `EE_UNITS_UPGRADE_MEDIA_*`; `arg` the slot, the flags or the
    /// sound's index; `value` the setter's object): the sounds' getters answer the sound.
    ///
    /// # Safety
    /// `media` describes a live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_upgrade_media(
        media: *const EeUnitsUpgradeMedia,
        op: i32,
        arg: c_long,
        value: *mut c_void,
    ) -> u64 {
        // SAFETY: the caller's contract
        let mut h = Game(unsafe { &*media });
        match op {
            EE_UNITS_UPGRADE_MEDIA_SET => set(&mut h, arg as usize, value as u64),
            EE_UNITS_UPGRADE_MEDIA_LOAD => load_media(&mut h, arg),
            EE_UNITS_UPGRADE_MEDIA_UNLOAD => unload_media(&mut h),
            EE_UNITS_UPGRADE_MEDIA_DESTROY => destroy(&mut h),
            EE_UNITS_UPGRADE_MEDIA_LOAD_BUTTONS => load_buttons(&mut h),
            EE_UNITS_UPGRADE_MEDIA_UNLOAD_BUTTONS => unload_buttons(&mut h),
            EE_UNITS_UPGRADE_MEDIA_ACKNOWLEDGE_SOUND | EE_UNITS_UPGRADE_MEDIA_SELECTION_SOUND => {
                let slot = sound_slot(op == EE_UNITS_UPGRADE_MEDIA_SELECTION_SOUND, arg);
                return h.slot(slot);
            }
            _ => {}
        }
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Fake {
        slots: [u64; 8],
        counts: (c_long, c_long),
        busy: [bool; 2],
        log: Vec<String>,
        failing: u64,
    }

    impl MediaHost for Fake {
        fn slot(&mut self, slot: usize) -> u64 {
            self.slots[slot]
        }
        fn set_slot(&mut self, slot: usize, value: u64) {
            self.slots[slot] = value;
        }
        fn counts(&mut self) -> (c_long, c_long) {
            self.counts
        }
        fn set_counts(&mut self, media: c_long, buttons: c_long) {
            self.counts = (media, buttons);
        }
        fn busy(&mut self, which: u32) -> bool {
            self.busy[which as usize]
        }
        fn set_busy(&mut self, which: u32, value: bool) {
            self.busy[which as usize] = value;
        }
        fn load(&mut self, kind: Media, object: u64) -> c_long {
            self.log.push(format!("load {kind:?} {object}"));
            c_long::from(object == self.failing)
        }
        fn unload(&mut self, kind: Media, object: u64) {
            self.log.push(format!("unload {kind:?} {object}"));
        }
        fn templates(&mut self, load: bool) {
            self.log.push(format!("templates {load}"));
        }
        fn class_media(&mut self, load: bool, buttons: bool) {
            self.log.push(format!("class {load} {buttons}"));
        }
        fn enter(&mut self, _lock: Lock) {}
        fn leave(&mut self, _lock: Lock) {}
        fn sleep(&mut self) {
            self.busy = [false; 2];
        }
    }

    #[test]
    fn the_first_load_loads_and_the_last_unload_unloads() {
        let mut f = Fake { slots: [1, 2, 3, 4, 5, 6, 7, 8], failing: 5, ..Default::default() };
        load_media(&mut f, 0x7f);
        assert_eq!(f.counts.0, 1);
        assert_eq!(f.slots[4], 0, "the failed acknowledge sound dropped");
        assert_eq!(
            f.log,
            vec![
                "load Sound 5",
                "load Sound 6",
                "load Sound 7",
                "load Sound 8",
                "load Sound 4",
                "load Sound 3",
                "load Model 2",
                "load Button 1",
                "templates true",
                "class true false"
            ]
        );
        load_media(&mut f, 0x7f);
        assert_eq!(f.counts.0, 2);
        f.log.clear();
        unload_media(&mut f);
        assert!(f.log.is_empty());
        unload_media(&mut f);
        assert_eq!(f.counts.0, 0);
        assert_eq!(f.log[0], "unload Sound 6", "the dropped sound is not unloaded");
        assert_eq!(f.log.last().unwrap(), "class false false");
        // a setter swaps the loads only while media is loaded
        f.log.clear();
        set(&mut f, EE_UNITS_MEDIA_DEATH, 9);
        assert!(f.log.is_empty());
        f.counts.0 = 1;
        set(&mut f, EE_UNITS_MEDIA_BUTTON, 10);
        assert_eq!(f.log, vec!["unload Button 1", "load Button 10"]);
    }

    #[test]
    fn buttons_count_and_sounds_alternate() {
        let mut f = Fake::default();
        load_buttons(&mut f);
        load_buttons(&mut f);
        assert_eq!(f.counts.1, 2);
        unload_buttons(&mut f);
        unload_buttons(&mut f);
        assert_eq!(f.counts.1, 0);
        assert_eq!(f.log, vec!["class true true", "class false true"]);
        assert_eq!(sound_slot(false, 1), 5);
        let a = sound_slot(true, -1);
        let b = sound_slot(true, -1);
        assert_ne!(a, b);
        assert!((6..8).contains(&a) && (6..8).contains(&b));
    }
}
