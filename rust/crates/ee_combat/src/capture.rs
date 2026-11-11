//! The simulation capture's combat records (`ee/EESimCapture.h`; `EECombatCapture` in `ee/EECombatFormulas.h`):
//! the tag, the input record's bytes, the output's bytes. `replay` recomputes each output here and compares it byte
//! for byte; the gate (`tools/sim_capture.py replay`) runs it over captures of the C++ formulas.

use crate::decide::*;
use crate::*;
use std::collections::BTreeMap;

/// The tags (the little-endian u32 of the four letters) and their formulas' names.
pub const TAGS: [(&[u8; 4], &str); 23] = [
    (b"CBAF", "attack factor"),
    (b"CBDM", "damage"),
    (b"CBEL", "elevation"),
    (b"CBMO", "morale"),
    (b"CBTH", "hit points"),
    (b"CBDE", "damage effect"),
    (b"CBRG", "regenerate"),
    (b"CBTM", "garrison timer"),
    (b"CBHL", "heal"),
    (b"CBHS", "heal step"),
    (b"CBCC", "conversion cost"),
    (b"CBCV", "conversion"),
    (b"CBBP", "ballistic pitch"),
    (b"CBPA", "projectile arc"),
    (b"CBRH", "range to hit"),
    (b"CBRC", "can reach"),
    (b"CBRW", "work reach"),
    (b"CBRT", "gather reach"),
    (b"CBHR", "heal range"),
    (b"CBDT", "damage row"),
    (b"CBCN", "can convert"),
    (b"CBCH", "can heal"),
    (b"CBDP", "drop point (model and tile)"),
];

pub fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

/// One record of a capture file: its tag and payload.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Record<'a> {
    pub tag: u32,
    pub payload: &'a [u8],
}

/// Splits a capture file into records; `Err` names the offset of a truncated one.
pub fn records(data: &[u8]) -> Result<Vec<Record<'_>>, String> {
    let mut out = Vec::new();
    let mut at = 0usize;
    while at < data.len() {
        if data.len() - at < 8 {
            return Err(format!("truncated record header at {at}"));
        }
        let tag = u32::from_le_bytes(data[at..at + 4].try_into().unwrap());
        let size = u32::from_le_bytes(data[at + 4..at + 8].try_into().unwrap()) as usize;
        at += 8;
        if data.len() - at < size {
            return Err(format!("truncated record payload at {at}"));
        }
        out.push(Record { tag, payload: &data[at..at + size] });
        at += size;
    }
    Ok(out)
}

/// The records here are 4-byte integer and float fields only.
///
/// # Safety
/// Every bit pattern of `size_of::<T>()` bytes must be a valid `T`.
pub unsafe trait Plain: Copy {}
unsafe impl Plain for f32 {}
unsafe impl Plain for i32 {}
unsafe impl Plain for [f32; 2] {}
unsafe impl Plain for EeCombatAttackFactorIn {}
unsafe impl Plain for EeCombatDamageIn {}
unsafe impl Plain for EeCombatDamageOut {}
unsafe impl Plain for EeCombatElevationIn {}
unsafe impl Plain for EeCombatMoraleIn {}
unsafe impl Plain for EeCombatHitPointsIn {}
unsafe impl Plain for EeCombatHitPointsOut {}
unsafe impl Plain for EeCombatDamageEffectIn {}
unsafe impl Plain for EeCombatRegenerateIn {}
unsafe impl Plain for EeCombatRegenerateOut {}
unsafe impl Plain for EeCombatTimerIn {}
unsafe impl Plain for EeCombatTimerOut {}
unsafe impl Plain for EeCombatHealIn {}
unsafe impl Plain for EeCombatHealOut {}
unsafe impl Plain for EeCombatHealStepIn {}
unsafe impl Plain for EeCombatHealStepOut {}
unsafe impl Plain for EeCombatConversionCostIn {}
unsafe impl Plain for EeCombatConversionIn {}
unsafe impl Plain for EeCombatConversionOut {}
unsafe impl Plain for EeCombatBallisticIn {}
unsafe impl Plain for EeCombatBallisticOut {}
unsafe impl Plain for EeCombatArcIn {}
unsafe impl Plain for EeCombatArcOut {}
unsafe impl Plain for EeCombatRangeIn {}
unsafe impl Plain for EeCombatReachIn {}
unsafe impl Plain for EeCombatDamageRowIn {}
unsafe impl Plain for EeCombatDamageRowOut {}
unsafe impl Plain for EeCombatCanConvertIn {}
unsafe impl Plain for EeCombatCanConvertOut {}
unsafe impl Plain for EeCombatCanHealIn {}
unsafe impl Plain for EeCombatDropIn {}
unsafe impl Plain for EeCombatDropOut {}

fn bytes_of<T: Plain>(value: &T) -> &[u8] {
    // SAFETY: a Plain value is size_of::<T>() initialized bytes (4-byte fields, no padding)
    unsafe { std::slice::from_raw_parts((value as *const T).cast::<u8>(), std::mem::size_of::<T>()) }
}

fn from_bytes<T: Plain>(bytes: &[u8]) -> T {
    assert_eq!(bytes.len(), std::mem::size_of::<T>());
    // SAFETY: the length is checked and every bit pattern is a valid T (Plain)
    unsafe { std::ptr::read_unaligned(bytes.as_ptr().cast::<T>()) }
}

/// Recomputes one record's output: `None` for a tag that is not a combat formula's, `Err` for a payload of the
/// wrong size, else whether the output is the captured one, byte for byte.
fn check<I: Plain, O: Plain>(payload: &[u8], formula: impl Fn(&I) -> O) -> Result<bool, String> {
    let (input_size, output_size) = (std::mem::size_of::<I>(), std::mem::size_of::<O>());
    if payload.len() != input_size + output_size {
        return Err(format!("payload of {} bytes, not {}", payload.len(), input_size + output_size));
    }
    let input: I = from_bytes(&payload[..input_size]);
    Ok(bytes_of(&formula(&input)) == &payload[input_size..])
}

/// Recomputes a record (see `check`).
pub fn replay(record: &Record) -> Option<Result<bool, String>> {
    let p = record.payload;
    let t = record.tag;
    Some(if t == tag(b"CBAF") {
        check(p, attack_factor)
    } else if t == tag(b"CBDM") {
        check(p, damage)
    } else if t == tag(b"CBEL") {
        check(p, elevation)
    } else if t == tag(b"CBMO") {
        check(p, morale)
    } else if t == tag(b"CBTH") {
        check(p, hit_points)
    } else if t == tag(b"CBDE") {
        check(p, damage_effect)
    } else if t == tag(b"CBRG") {
        check(p, regenerate)
    } else if t == tag(b"CBTM") {
        check(p, timer)
    } else if t == tag(b"CBHL") {
        check(p, heal)
    } else if t == tag(b"CBHS") {
        check(p, heal_step)
    } else if t == tag(b"CBCC") {
        check(p, conversion_cost)
    } else if t == tag(b"CBCV") {
        check(p, conversion)
    } else if t == tag(b"CBBP") {
        check(p, ballistic)
    } else if t == tag(b"CBPA") {
        check(p, arc)
    } else if t == tag(b"CBRH") {
        check(p, range)
    } else if t == tag(b"CBRC") {
        check(p, can_reach)
    } else if t == tag(b"CBRW") {
        check(p, work_reach)
    } else if t == tag(b"CBRT") {
        check(p, gather_reach)
    } else if t == tag(b"CBHR") {
        check(p, |v: &[f32; 2]| within_range(v[0], v[1]))
    } else if t == tag(b"CBDT") {
        check(p, damage_row)
    } else if t == tag(b"CBCN") {
        check(p, can_convert)
    } else if t == tag(b"CBCH") {
        check(p, can_heal)
    } else if t == tag(b"CBDP") {
        // the input, then the point and the target's id: only the model's and the tile's point are computed here
        let size = std::mem::size_of::<EeCombatDropIn>();
        if p.len() != size + 16 {
            return Some(Err(format!("payload of {} bytes, not {}", p.len(), size + 16)));
        }
        let input: EeCombatDropIn = from_bytes(&p[..size]);
        if input.kind != DROP_KIND_MODEL && input.kind != DROP_KIND_TILE {
            return None;
        }
        Ok(bytes_of(&drop_point(&input)) == &p[size..size + 12])
    } else {
        return None;
    })
}

/// Per tag: records, equal records, errors.
pub type Counts = BTreeMap<String, [u64; 3]>;

/// Replays every combat record of a capture file's bytes; the first few differences go to `report`.
pub fn replay_all(data: &[u8], counts: &mut Counts, report: &mut dyn FnMut(String)) -> Result<(), String> {
    for record in records(data)? {
        let Some(result) = replay(&record) else { continue };
        let name = String::from_utf8_lossy(&record.tag.to_le_bytes()).into_owned();
        let entry = counts.entry(name.clone()).or_default();
        entry[0] += 1;
        match result {
            Ok(true) => entry[1] += 1,
            Ok(false) => {
                if entry[0] - entry[1] - entry[2] <= 3 {
                    report(format!("{name} differs: payload {:02x?}", record.payload));
                }
            }
            Err(e) => {
                entry[2] += 1;
                report(format!("{name}: {e}"));
            }
        }
    }
    Ok(())
}

/// A record's bytes as the C++ capture writes them (for tests and tools).
pub fn encode<I: Plain, O: Plain>(name: &[u8; 4], input: &I, output: &O) -> Vec<u8> {
    let (input, output) = (bytes_of(input), bytes_of(output));
    let mut out = Vec::with_capacity(8 + input.len() + output.len());
    out.extend_from_slice(&tag(name).to_le_bytes());
    out.extend_from_slice(&((input.len() + output.len()) as u32).to_le_bytes());
    out.extend_from_slice(input);
    out.extend_from_slice(output);
    out
}
