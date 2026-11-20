//! `EESMEffect::Reset` and `::Copy` (Scenario Manager/EESMEffect.cpp; the scenario editor lane's round 4): the
//! effect's eight texts emptied or copied, its numbers (the record the C++ class derives from) set to the defaults or
//! copied, its object list emptied or copied, in the C++'s order, over single C++ statements
//! (`EE_SCENARIO_EC_*`, `ee_scenariocpp_effect`).

use std::ffi::c_void;

use crate::scenario::fields::{effect_defaults, EeScenarioEffectFields};

/// Effect a's text k emptied (`= ""`); a's text k set to b's; a as its `EeScenarioEffectFields*`; a's `mObjects`
/// cleared; set to b's.
pub const EE_SCENARIO_EC_TEXT_CLEAR: u32 = 0;
pub const EE_SCENARIO_EC_TEXT_COPY: u32 = 1;
pub const EE_SCENARIO_EC_FIELDS: u32 = 2;
pub const EE_SCENARIO_EC_OBJECTS_CLEAR: u32 = 3;
pub const EE_SCENARIO_EC_OBJECTS_COPY: u32 = 4;

/// The effect's texts: `mName`, `mText`, `mMessage`, `mS64`, `mS78`, `mNewName`, `mFileName`, `mDescription`.
pub const EE_SCENARIO_EC_TEXTS: i64 = 8;

/// One statement: `x(op, a, b, k)`.
pub type Ec<'a> = dyn FnMut(u32, *mut c_void, *const c_void, i64) -> i64 + 'a;

fn fields(x: &mut Ec, e: *const c_void) -> *mut EeScenarioEffectFields {
    x(EE_SCENARIO_EC_FIELDS, e as *mut c_void, std::ptr::null(), 0) as usize as *mut EeScenarioEffectFields
}

/// `EESMEffect::Reset`.
pub fn reset(x: &mut Ec, e: *mut c_void) {
    for k in 0..EE_SCENARIO_EC_TEXTS {
        x(EE_SCENARIO_EC_TEXT_CLEAR, e, std::ptr::null(), k);
    }
    let f = fields(x, e);
    // SAFETY: the effect's live record, written before the next C++ call
    unsafe { *f = effect_defaults() };
    x(EE_SCENARIO_EC_OBJECTS_CLEAR, e, std::ptr::null(), 0);
}

/// `EESMEffect::Copy(src)` into `e`.
pub fn copy(x: &mut Ec, e: *mut c_void, src: *const c_void) {
    for k in 0..EE_SCENARIO_EC_TEXTS {
        x(EE_SCENARIO_EC_TEXT_COPY, e, src, k);
    }
    let (d, s) = (fields(x, e), fields(x, src));
    // SAFETY: both effects' live records (possibly the same one), copied before the next C++ call
    unsafe { *d = *s };
    x(EE_SCENARIO_EC_OBJECTS_COPY, e, src, 0);
}

#[cfg(not(test))]
extern "C" {
    /// One statement (EESMEffect.cpp).
    fn ee_scenariocpp_effect(op: u32, a: *mut c_void, b: *const c_void, k: i64) -> i64;
}

#[cfg(not(test))]
fn cpp(op: u32, a: *mut c_void, b: *const c_void, k: i64) -> i64 {
    // SAFETY: the statements take the live effects the C++ member passed
    unsafe { ee_scenariocpp_effect(op, a, b, k) }
}

/// `EESMEffect::Reset` of the effect at `e` (the C++ object).
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_scenario_effect_reset(e: *mut c_void) {
    reset(&mut cpp, e);
}

/// `EESMEffect::Copy`: `src` into `e` (the C++ objects).
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_scenario_effect_copy(e: *mut c_void, src: *const c_void) {
    copy(&mut cpp, e, src);
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Two fake effects: their texts and objects as values, their records.
    #[derive(Default)]
    struct Fake {
        texts: [Vec<u8>; 8],
        fields: EeScenarioEffectFields,
        objects: Vec<u32>,
    }

    fn run(f: impl FnOnce(&mut Ec)) -> Vec<(u32, i64)> {
        let mut ops = Vec::new();
        let mut x = |op: u32, a: *mut c_void, b: *const c_void, k: i64| -> i64 {
            ops.push((op, k));
            // SAFETY: the test's own fakes, alive through the call
            let (a, b) = unsafe { (&mut *(a as *mut Fake), (b as *const Fake).as_ref()) };
            match op {
                EE_SCENARIO_EC_TEXT_CLEAR => a.texts[k as usize].clear(),
                EE_SCENARIO_EC_TEXT_COPY => a.texts[k as usize] = b.unwrap().texts[k as usize].clone(),
                EE_SCENARIO_EC_FIELDS => return &mut a.fields as *mut EeScenarioEffectFields as usize as i64,
                EE_SCENARIO_EC_OBJECTS_CLEAR => a.objects.clear(),
                EE_SCENARIO_EC_OBJECTS_COPY => a.objects = b.unwrap().objects.clone(),
                _ => panic!("op {op}"),
            }
            0
        };
        f(&mut x);
        ops
    }

    #[test]
    fn reset_empties_and_defaults() {
        let mut e = Fake { objects: vec![1, 2], ..Default::default() };
        e.texts[7] = b"desc".to_vec();
        e.fields.mDuration = 9;
        let p = &mut e as *mut Fake as *mut c_void;
        let ops = run(|x| reset(x, p));
        assert_eq!(e.texts[7], b"");
        assert_eq!(e.fields, effect_defaults());
        assert!(e.objects.is_empty());
        let order: Vec<u32> = ops.iter().map(|o| o.0).collect();
        let mut want = vec![EE_SCENARIO_EC_TEXT_CLEAR; 8];
        want.extend([EE_SCENARIO_EC_FIELDS, EE_SCENARIO_EC_OBJECTS_CLEAR]);
        assert_eq!(order, want);
    }

    #[test]
    fn copy_takes_everything() {
        let mut src = Fake { objects: vec![4, 5, 6], ..Default::default() };
        src.texts[0] = b"name".to_vec();
        src.texts[6] = b"file".to_vec();
        src.fields.mObject = 3;
        let mut e = Fake::default();
        e.texts[1] = b"old".to_vec();
        let (p, s) = (&mut e as *mut Fake as *mut c_void, &src as *const Fake as *const c_void);
        run(|x| copy(x, p, s));
        assert_eq!(
            (e.texts[0].as_slice(), e.texts[1].as_slice(), e.texts[6].as_slice()),
            (&b"name"[..], &b""[..], &b"file"[..])
        );
        assert_eq!(e.fields, src.fields);
        assert_eq!(e.objects, vec![4, 5, 6]);
    }
}
