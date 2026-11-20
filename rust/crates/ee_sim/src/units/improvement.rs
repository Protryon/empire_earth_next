//! `EEUnitClassImprovement`'s bodies and `EEUnitClassUpgrade`'s saved-game members (the groups lane, round 2; were
//! EEUnitClassImprovement.cpp and EEUnitClassUpgrade.cpp): the constructor's levels, `CopyLevelsTo`, `Reapply`,
//! `Load` and `Save` over the record's levels (`mLevels`, 16 4-byte words, saved as 0x40 raw bytes) and points
//! spent (`mTotalCost`), and the upgrade's `Load` and `Save` (its name, then the levels). The C++ they call is one
//! callback (the player save's [`EeUnitsPlayerSaveFn`]): `ChangeLevel` (improve.rs's records over the class's step
//! factors), the name's load and save, and the file's primitives.
//!
//! `ChangeLevel` writes the levels and the points: the bodies read and write them through the record's pointers,
//! never holding a reference across a callback.

use crate::units::player_save::{
    EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, EE_UNITS_PLAYER_SAVE_READ_LEVELS, EE_UNITS_PLAYER_SAVE_READ_LONG,
    EE_UNITS_PLAYER_SAVE_WRITE_LEVELS, EE_UNITS_PLAYER_SAVE_WRITE_LONG,
};
use std::ffi::c_void;
use std::os::raw::c_long;

/// The callback's `ChangeLevel(improvement index, delta io.i)` on the record the context names.
pub const EE_UNITS_IMPROVEMENT_CHANGE_LEVEL: i32 = 200;
/// The upgrade's `mName.Load(file)` (the answer: its read status)
pub const EE_UNITS_IMPROVEMENT_NAME_LOAD: i32 = 201;
/// The upgrade's `mName.Save(file)` (its write status)
pub const EE_UNITS_IMPROVEMENT_NAME_SAVE: i32 = 202;

/// `kImprovementLeadership` (old saves drop it back to level 1)
const LEADERSHIP: usize = 10;
const LEVELS: usize = 16;

/// The C++ a body calls.
pub trait ImprovementHost {
    /// one callback operation (the file's primitives, [`EE_UNITS_IMPROVEMENT_CHANGE_LEVEL`], the name's)
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32;
}

/// A record's levels and points (the C++ members' addresses).
#[derive(Clone, Copy)]
pub struct Record {
    pub levels: *mut i32,
    pub total: *mut c_long,
}

impl Record {
    fn level(self, i: usize) -> i32 {
        // SAFETY: the record's 16 levels (the C ABI's contract)
        unsafe { self.levels.add(i).read() }
    }
    fn set_level(self, i: usize, v: i32) {
        // SAFETY: as above
        unsafe { self.levels.add(i).write(v) }
    }
    fn set_total(self, v: c_long) {
        // SAFETY: the record's points
        unsafe { self.total.write(v) }
    }
}

fn change_level<H: ImprovementHost>(h: &mut H, i: usize, delta: i64) {
    h.call(EE_UNITS_IMPROVEMENT_CHANGE_LEVEL, i as u32, &mut EeUnitsPlayerSaveIo { i: delta, ..Default::default() });
}

/// The constructor's: every level 1, no points spent.
pub fn reset(r: Record) {
    for i in 0..LEVELS {
        r.set_level(i, 1);
    }
    r.set_total(0);
}

/// `CopyLevelsTo(upgrade, target)`: the host's record (the target) raised one level at a time by each of `r`'s
/// levels above 1.
pub fn copy_levels_to<H: ImprovementHost>(h: &mut H, r: Record) {
    for i in 0..LEVELS {
        let mut j: c_long = 0;
        while j < r.level(i) as c_long - 1 {
            change_level(h, i, 1);
            j += 1;
        }
    }
}

/// `Reapply(class, upgrade)`: the points recounted from 0, each raised level back to 1 and raised again at once.
pub fn reapply<H: ImprovementHost>(h: &mut H, r: Record) {
    r.set_total(0);
    for i in 0..LEVELS {
        if r.level(i) <= 1 {
            continue;
        }
        let delta = r.level(i) - 1;
        r.set_level(i, 1);
        change_level(h, i, delta.into());
    }
}

fn bytes<H: ImprovementHost>(h: &mut H, op: i32, data: *mut i32) -> i32 {
    h.call(op, 0, &mut EeUnitsPlayerSaveIo { i: data as usize as i64, ..Default::default() })
}

/// `Load(class, upgrade, major, minor, file)`: the levels (an old save's 16 old words after them, its points
/// otherwise, read and cleared), applied to the upgrade's stats; saves up to minor 0xd8 drop leadership to 1. The
/// last read's status.
pub fn load<H: ImprovementHost>(h: &mut H, r: Record, major: u32, minor: u32) -> i32 {
    let mut old = [0i32; LEVELS];
    let old_save = major == 0 && minor <= 0xd8;
    let result;
    if old_save && minor > 0x17 && minor <= 0x6a {
        bytes(h, EE_UNITS_PLAYER_SAVE_READ_LEVELS, r.levels);
        result = bytes(h, EE_UNITS_PLAYER_SAVE_READ_LEVELS, old.as_mut_ptr());
    } else {
        let mut first = bytes(h, EE_UNITS_PLAYER_SAVE_READ_LEVELS, r.levels);
        if !(old_save && minor <= 0xa0) && first == 0 {
            // SAFETY: the record's points
            let mut io = EeUnitsPlayerSaveIo { i: unsafe { r.total.read() } as i64, ..Default::default() };
            first = h.call(EE_UNITS_PLAYER_SAVE_READ_LONG, 0, &mut io);
            r.set_total(io.i as c_long);
            r.set_total(0);
        }
        result = first;
    }
    for i in 0..LEVELS {
        if old_save && i == LEADERSHIP {
            r.set_level(i, 1);
            continue;
        }
        let level = r.level(i);
        if level > 1 {
            r.set_level(i, 1);
            change_level(h, i, (level - 1).into());
        }
    }
    result
}

/// `Save(major, minor, file)`: the levels, then (written) the points; the first failed write's status.
pub fn save<H: ImprovementHost>(h: &mut H, r: Record) -> i32 {
    let mut result = bytes(h, EE_UNITS_PLAYER_SAVE_WRITE_LEVELS, r.levels);
    if result == 0 {
        // SAFETY: the record's points
        let mut io = EeUnitsPlayerSaveIo { i: unsafe { r.total.read() } as i64, ..Default::default() };
        result = h.call(EE_UNITS_PLAYER_SAVE_WRITE_LONG, 0, &mut io);
    }
    result
}

/// `EEUnitClassUpgrade::Load(major, minor, file)`: the name, then (read) the levels.
pub fn upgrade_load<H: ImprovementHost>(h: &mut H, r: Record, major: u32, minor: u32) -> i32 {
    let mut result = h.call(EE_UNITS_IMPROVEMENT_NAME_LOAD, 0, &mut EeUnitsPlayerSaveIo::default());
    if result == 0 {
        result = load(h, r, major, minor);
    }
    result
}

/// `EEUnitClassUpgrade::Save(major, minor, file)`: the name, then (written) the levels.
pub fn upgrade_save<H: ImprovementHost>(h: &mut H, r: Record) -> i32 {
    let mut result = h.call(EE_UNITS_IMPROVEMENT_NAME_SAVE, 0, &mut EeUnitsPlayerSaveIo::default());
    if result == 0 {
        result = save(h, r);
    }
    result
}

// ---- the C ABI
/// `ee_units_improvement`'s `what`
pub const EE_UNITS_IMPROVEMENT_DO_RESET: u32 = 0;
pub const EE_UNITS_IMPROVEMENT_DO_COPY_LEVELS: u32 = 1;
pub const EE_UNITS_IMPROVEMENT_DO_REAPPLY: u32 = 2;
pub const EE_UNITS_IMPROVEMENT_DO_LOAD: u32 = 3;
pub const EE_UNITS_IMPROVEMENT_DO_SAVE: u32 = 4;
pub const EE_UNITS_IMPROVEMENT_DO_UPGRADE_LOAD: u32 = 5;
pub const EE_UNITS_IMPROVEMENT_DO_UPGRADE_SAVE: u32 = 6;

struct FnHost {
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
}

impl ImprovementHost for FnHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the C++ callback over its context
        unsafe { (self.f)(self.ctx, op, index, io) }
    }
}

/// Member `what` (`EE_UNITS_IMPROVEMENT_DO_*`) on the record `levels`/`total` (16 words, the points), over the
/// callback `f` and its context (neither used by the reset); `major`, `minor` the saved game's version.
///
/// # Safety
/// `levels` 16 live words, `total` the record's points; `f` handles every operation the member makes.
#[no_mangle]
pub unsafe extern "C" fn ee_units_improvement(
    what: u32,
    levels: *mut i32,
    total: *mut c_long,
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    major: u32,
    minor: u32,
) -> i32 {
    let r = Record { levels, total };
    let h = &mut FnHost { ctx, f };
    match what {
        EE_UNITS_IMPROVEMENT_DO_RESET => reset(r),
        EE_UNITS_IMPROVEMENT_DO_COPY_LEVELS => copy_levels_to(h, r),
        EE_UNITS_IMPROVEMENT_DO_REAPPLY => reapply(h, r),
        EE_UNITS_IMPROVEMENT_DO_LOAD => return load(h, r, major, minor),
        EE_UNITS_IMPROVEMENT_DO_SAVE => return save(h, r),
        EE_UNITS_IMPROVEMENT_DO_UPGRADE_LOAD => return upgrade_load(h, r, major, minor),
        EE_UNITS_IMPROVEMENT_DO_UPGRADE_SAVE => return upgrade_save(h, r),
        _ => {}
    }
    0
}

#[cfg(test)]
#[path = "improvement_tests.rs"]
mod tests;
