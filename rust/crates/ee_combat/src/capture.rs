//! The simulation capture's combat records (`ee/EESimCapture.h`; `EECombatCapture` in `ee/EECombatFormulas.h`):
//! the tag, the input record's bytes, the output's bytes. `replay` recomputes each output here and compares it byte
//! for byte; the gate (`tools/sim_capture.py replay`) runs it over captures of the C++ formulas.

use crate::decide::*;
use crate::*;
use std::collections::BTreeMap;

/// The tags (the little-endian u32 of the four letters) and their formulas' names.
pub const TAGS: [(&[u8; 4], &str); 37] = [
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
    (b"CBTL", "tile line"),
    (b"CBWP", "weapon tag point height"),
    (b"CBLS", "shot launch"),
    (b"CBLT", "turret launch"),
    (b"CBAP", "attach point"),
    (b"CBPJ", "projectile launch step"),
    (b"CBPS", "projectile step"),
    (b"CBMA", "missile acceleration"),
    (b"CBMH", "missile homing"),
    (b"CBFS", "new target screen"),
    (b"CBHD", "hit distance"),
    (b"CBGI", "ground impact"),
    (b"CBWI", "water impact"),
    (b"CBEW", "effect weapon point"),
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
unsafe impl Plain for crate::geometry::EeCombatWeaponIn {}
unsafe impl Plain for [f32; 16] {}
unsafe impl Plain for crate::launch::EeCombatLaunchIn {}
unsafe impl Plain for crate::launch::EeCombatLaunchOut {}
unsafe impl Plain for crate::flight::EeCombatFlightIn {}
unsafe impl Plain for crate::flight::EeCombatFlightOut {}
unsafe impl Plain for crate::screen::EeCombatScreenIn {}
unsafe impl Plain for crate::screen::EeCombatHitDistanceIn {}
unsafe impl Plain for crate::impact::EeCombatImpactIn {}
unsafe impl Plain for crate::impact::EeCombatImpactOut {}

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
    } else if t == tag(b"CBTL") {
        // the ends and flags, the count, the tiles
        if p.len() < 28 || p.len() != 28 + 8 * i32_at(p, 24) as usize {
            return Some(Err(format!("payload of {} bytes", p.len())));
        }
        let v: Vec<i32> = (0..p.len() / 4).map(|n| i32_at(p, n * 4)).collect();
        let line = crate::geometry::tile_line(v[0], v[1], v[2], v[3], v[4] != 0, v[5] != 0);
        let tiles: Vec<(i32, i32)> = v[7..].chunks(2).map(|c| (c[0], c[1])).collect();
        Ok(line == tiles)
    } else if t == tag(b"CBWP") {
        // the input, its chain's matrices, the height
        let size = std::mem::size_of::<crate::geometry::EeCombatWeaponIn>();
        if p.len() < size + 4 {
            return Some(Err(format!("payload of {} bytes", p.len())));
        }
        let input: crate::geometry::EeCombatWeaponIn = from_bytes(&p[..size]);
        let chain = input.chain.max(0) as usize;
        if p.len() != size + 64 * chain + 4 {
            return Some(Err(format!("payload of {} bytes, not {}", p.len(), size + 64 * chain + 4)));
        }
        let links: Vec<[f32; 16]> = (0..chain).map(|n| from_bytes(&p[size + 64 * n..size + 64 * n + 64])).collect();
        let height = crate::geometry::weapon_height(&input, &links);
        Ok(height.to_bits().to_le_bytes() == p[p.len() - 4..])
    } else if t == tag(b"CBEW") {
        // the site, the input, its chain's matrices, the start
        let size = std::mem::size_of::<crate::geometry::EeCombatWeaponIn>();
        if p.len() < 4 + size + 12 {
            return Some(Err(format!("payload of {} bytes", p.len())));
        }
        let input: crate::geometry::EeCombatWeaponIn = from_bytes(&p[4..4 + size]);
        let chain = input.chain.max(0) as usize;
        if p.len() != 4 + size + 64 * chain + 12 {
            return Some(Err(format!("payload of {} bytes, not {}", p.len(), 4 + size + 64 * chain + 12)));
        }
        let at = 4 + size;
        let links: Vec<[f32; 16]> = (0..chain).map(|n| from_bytes(&p[at + 64 * n..at + 64 * n + 64])).collect();
        let point = crate::geometry::weapon_point(&input, &links);
        let bytes: Vec<u8> = [point.x, point.y, point.z].iter().flat_map(|v| v.to_bits().to_le_bytes()).collect();
        Ok(bytes == p[p.len() - 12..])
    } else if t == tag(b"CBFS") {
        check(p, crate::screen::screen)
    } else if t == tag(b"CBHD") {
        check(p, crate::screen::hit_distance)
    } else if t == tag(b"CBPJ") {
        check(p, crate::flight::projectile_launch)
    } else if t == tag(b"CBPS") {
        check(p, crate::flight::projectile_step)
    } else if t == tag(b"CBMA") {
        check(p, crate::flight::missile_accelerate)
    } else if t == tag(b"CBMH") {
        check(p, crate::flight::missile_home)
    } else if t == tag(b"CBGI") || t == tag(b"CBWI") {
        // the record as the test wrote it (its lazy reads), the outcome: the test over its eager part, the host
        // answering from the record, must write both again
        let (size, out) = (
            std::mem::size_of::<crate::impact::EeCombatImpactIn>(),
            std::mem::size_of::<crate::impact::EeCombatImpactOut>(),
        );
        if p.len() != size + out {
            return Some(Err(format!("payload of {} bytes, not {}", p.len(), size + out)));
        }
        let input: crate::impact::EeCombatImpactIn = from_bytes(&p[..size]);
        let (record, outcome) = crate::impact::replay(&input);
        Ok(bytes_of(&record) == &p[..size] && bytes_of(&outcome) == &p[size..])
    } else if t == tag(b"CBLS") || t == tag(b"CBLT") || t == tag(b"CBAP") {
        // the input, its chain's matrices, the output
        let size = std::mem::size_of::<crate::launch::EeCombatLaunchIn>();
        let out = std::mem::size_of::<crate::launch::EeCombatLaunchOut>();
        if p.len() < size + out {
            return Some(Err(format!("payload of {} bytes", p.len())));
        }
        let input: crate::launch::EeCombatLaunchIn = from_bytes(&p[..size]);
        let chain = input.chain.max(0) as usize;
        if p.len() != size + 64 * chain + out {
            return Some(Err(format!("payload of {} bytes, not {}", p.len(), size + 64 * chain + out)));
        }
        let links: Vec<[f32; 16]> = (0..chain).map(|n| from_bytes(&p[size + 64 * n..size + 64 * n + 64])).collect();
        Ok(bytes_of(&crate::launch::launch(&input, &links)) == &p[size + 64 * chain..])
    } else {
        return None;
    })
}

fn i32_at(p: &[u8], at: usize) -> i32 {
    i32::from_le_bytes(p[at..at + 4].try_into().unwrap())
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
