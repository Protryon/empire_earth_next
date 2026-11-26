//! TEST ONLY: the driver's reads of the whole state (was EESim.cpp's): the per-CT checksum line's fields
//! (--checksum-trace) and the state dump (--dump-at, --dump-load, --dump-tick), item by item, so two dumps name the
//! units and players that differ. Reads only; every C++ member or call is one host case ([`super::sim_ops`], the
//! `Dm*` operations of EESim.cpp's `ee_simcpp_sim` and `ee_simcpp_sim_float`). The C runtime's `%a`, `%lu` (a 32-bit
//! `unsigned long`), `%ld` and `%08lx` are written as the C++ wrote them ([`hexf`], [`lu`], [`ld`], [`x8`]).

use super::sim_ops::{h, hdd, EeSimDriverSim as O};
use crate::gamecpp as gc;
use ee_sim::units::patrol_group as pg;
use ee_sim::units::player_lists::{player_list, EE_UNITS_LISTS_PATROLS, EE_UNITS_LISTS_TOWNS};
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsUnitFields};

/// cbindgen:ignore
mod sys {
    extern "C" {
        // `EECPOutOfSynchChecksum` (ee_sim ai next_trace.rs)
        pub fn ee_ai_out_of_synch_checksum() -> std::ffi::c_ulong;
    }
}

/// A patrol group's id (`EEPPatrolGroup`: ee_sim units patrol_group.rs).
fn pg_id(group: usize) -> i64 {
    // SAFETY: the group the player or town holds
    unsafe { pg::ee_units_pg_id(group as *const _) as i64 }
}

/// A patrol group's town and loop flags.
fn pg_flags(group: usize) -> (i64, i64) {
    // SAFETY: as pg_id
    unsafe {
        (pg::ee_units_pg_is_town_patrol(group as *const _) as i64, pg::ee_units_pg_is_loop(group as *const _) as i64)
    }
}

/// A patrol group's points (complex units; 0: none).
fn pg_points(group: usize) -> Vec<usize> {
    // SAFETY: as pg_id
    unsafe {
        let n = pg::ee_units_pg_point_count(group as *const _) as usize;
        (0..n).map(|k| pg::ee_units_pg_point(group as *const _, k) as usize).collect()
    }
}

/// The out-of-synch checksum's parts read for the per-CT line: unit hit points and tiles (1), positions, heights,
/// commands and update counts (2), calamities (0x10), air traffic (0x40), the continent manager's checksum (0x100).
/// Left out because reading them is not free of effects: 4, the map's window checksum, advances the map's checksum
/// cursor (EEMap::ComputeChecksum); and 0x20, the goals, copies each goal queue and dereferences goal members the
/// multiplayer game (flag 1 only) never reads. The game's own OOS check uses flag 1.
const CHECKSUM_FLAGS: i64 = 0x1 | 0x2 | 0x10 | 0x40 | 0x100;

/// `%lu` of a 32-bit `unsigned long`.
fn lu(v: i64) -> u32 {
    v as u32
}

/// `%ld` of a 32-bit `long`.
fn ld(v: i64) -> i32 {
    v as i32
}

/// `%08lx` of a 32-bit value.
fn x8(v: i64) -> String {
    format!("{:08x}", v as u32)
}

/// The C runtime's (UCRT's) `%a` of a double: `[-]0x1.<13 hex digits>p<+|-><exponent>`, subnormals
/// `0x0.<13>p-1022`, zero `0x0.0000000000000p+0`; `inf`, `nan`, `-nan(ind)` (the default quiet NaN).
pub fn hexf(v: f64) -> String {
    let bits = v.to_bits();
    let sign = if bits >> 63 != 0 { "-" } else { "" };
    let exp = ((bits >> 52) & 0x7ff) as i64;
    let man = bits & 0x000f_ffff_ffff_ffff;
    if exp == 0x7ff {
        if man == 0 {
            return format!("{sign}inf");
        }
        if sign == "-" && man == 0x0008_0000_0000_0000 {
            return "-nan(ind)".to_string();
        }
        return format!("{sign}nan");
    }
    if exp == 0 {
        if man == 0 {
            return format!("{sign}0x0.0000000000000p+0");
        }
        return format!("{sign}0x0.{man:013x}p-1022");
    }
    let e = exp - 1023;
    let es = if e < 0 { "-" } else { "+" };
    format!("{sign}0x1.{man:013x}p{es}{}", e.abs())
}

/// --checksum-trace's fields (the driver writes "CT <n> " or "LOADED " before them; the physics thread, under the
/// world's write lock, after the CT's last physics tick). Only reads: no random draws, no caches filled;
/// EEPhysics::mLastOutOfSynchChecksum, the one member GetOutOfSynchChecksum writes (and nothing reads), is put back:
///   pt=<ticks done> tick=<current tick> ms=<game ms> rng=<physics random seed>
///   oos=<GetOutOfSynchChecksum(kChecksumFlags)> units=<count>/<sum of GetChecksum(3)> ai=<EECPOutOfSynchChecksum>
///   inputs=<user inputs executed>
pub fn checksum_text() -> String {
    let saved = h(O::DmPhysLastOos, 0, &[]);
    let checksum = h(O::DmPhysOos, 0, &[CHECKSUM_FLAGS]);
    h(O::DmPhysSetLastOos, 0, &[saved]);
    let mut units: u32 = 0;
    let mut count: u32 = 0;
    let mut u = h(O::DmWorldFirstUnit, 0, &[]);
    while u != 0 {
        units = units.wrapping_add(h(O::DmUnitChecksum, u as usize, &[3]) as u32);
        count = count.wrapping_add(1);
        u = gc::h(gc::O::UnTileNext, u as usize, &[]);
    }
    let physics = super::sim_host::physics_record();
    format!(
        "pt={} tick={} ms={} rng={} oos={} units={}/{} ai={} inputs={}\n",
        lu(physics.mNumTicksDone as i64),
        lu(physics.mCurrentTick as i64),
        ld(gc::h(gc::O::DwGameTime, 0, &[])),
        x8(h(O::DmPhysSeed, 0, &[])),
        x8(checksum),
        count,
        x8(i64::from(units)),
        // SAFETY: the simulation thread's read (the CT's line)
        x8(unsafe { sys::ee_ai_out_of_synch_checksum() } as i64),
        ld(h(O::DmPhysInputs, 0, &[])),
    )
}

/// The dump file: the Rust's lines go through the C++'s stream, so the C++ dumps (the computer players', the path
/// manager's, each goal's) write between them in order.
struct Out {
    file: usize,
    text: String,
}

impl Out {
    fn flush(&mut self) {
        if !self.text.is_empty() {
            let text = std::ffi::CString::new(std::mem::take(&mut self.text)).unwrap_or_default();
            // SAFETY: the stream dump_state opened, a NUL-ended text
            unsafe { libc::fputs(text.as_ptr(), self.file as *mut libc::FILE) };
        }
    }
    fn push(&mut self, s: &str) {
        self.text.push_str(s);
        if self.text.len() > 1 << 16 {
            self.flush();
        }
    }
    /// a C++ dump into the stream (`op` with the stream as its argument)
    fn cpp(&mut self, op: O, p: usize) {
        self.flush();
        h(op, p, &[self.file as i64]);
    }
}

/// The bytes of the object at `p`, as hex.
fn raw(p: usize, size: usize) -> String {
    // SAFETY: TEST ONLY: `p` is a live C++ object of `size` bytes (sizeof its class), read under the world's lock
    let bytes = unsafe { std::slice::from_raw_parts(p as *const u8, size) };
    let mut s = String::with_capacity(size * 2);
    for b in bytes {
        s.push_str(&format!("{b:02x}"));
    }
    s
}

fn f(op: O, p: usize, a: &[i64]) -> String {
    hexf(hdd(op, p, a))
}

/// --dump-at / --dump-load (TEST ONLY, the save/load fidelity hunt): the state the checksum line sums, item by item.
/// Reads only (the goals' own out-of-synch checksums are read, as the checksum's flag 0x20 would).
pub fn dump_state(path: &str) {
    let Ok(c_path) = std::ffi::CString::new(path) else { return };
    // SAFETY: NUL-ended path and mode; the C library's text mode, as the C++'s fopen (the C++ dumps write into it)
    let file = unsafe { libc::fopen(c_path.as_ptr(), c"w".as_ptr()) } as usize;
    if file == 0 {
        return;
    }
    let mut o = Out { file, text: String::new() };
    o.push("STATE ");
    o.push(&checksum_text());
    let physics = super::sim_host::physics_record();
    o.push(&format!(
        "physics clock={} tickrandom={} tickstoprocess={}\n",
        hexf(f64::from(physics.mGameClock)),
        ld(i64::from(physics.mTickRandom)),
        physics.mTicksToProcess as i32
    ));
    // the stepped units and the (de)activation queues, in order (EEPWorldHelper's unit order) (ee_sim app active.rs)
    for (l, name) in ["UPDATE", "ACTIVATE", "DEACTIVATE"].iter().enumerate() {
        // SAFETY: the counting call, then a buffer of that many ids
        let n = unsafe { ee_sim::app::active_ffi::ee_app_active_list(l as u32, std::ptr::null_mut(), 0) };
        let mut ids = vec![0u32; n];
        unsafe { ee_sim::app::active_ffi::ee_app_active_list(l as u32, ids.as_mut_ptr(), ids.len()) };
        let mut s = format!("{name} {}:", ids.len());
        for id in ids {
            s.push_str(&format!(" {}", lu(h(O::DmActiveUnitId, 0, &[i64::from(id)]))));
        }
        s.push('\n');
        o.push(&s);
    }
    dump_players(&mut o);
    o.cpp(O::DmCpDump, 0); // the "ai" parts per player and the computer players' timers
    o.cpp(O::DmPathDump, 0); // the searches in progress, the pending requests, the timers
    let mut u = h(O::DmWorldFirstUnit, 0, &[]) as usize;
    while u != 0 {
        dump_unit(&mut o, u);
        u = gc::h(gc::O::UnTileNext, u, &[]) as usize;
    }
    dump_calamities(&mut o);
    dump_regions(&mut o);
    o.flush();
    // SAFETY: the stream opened above
    unsafe { libc::fclose(file as *mut libc::FILE) };
}

fn dump_players(o: &mut Out) {
    let n = lu(h(O::NumPlayers, 0, &[]));
    for i in 0..n {
        let p = crate::tech_tree::objects::world_player(i64::from(i));
        if p == 0 {
            continue;
        }
        let mut s = format!(
            "P {i} pop={} eog={} epoch={} res=",
            ld(h(O::DmPlPop, p, &[])),
            ld(h(O::DmPlEog, p, &[])),
            ld(h(O::DmPlEpoch, p, &[]))
        );
        for r in 0..5 {
            if r != 0 {
                s.push(',');
            }
            s.push_str(&f(O::DmPlResource, p, &[r]));
        }
        s.push('\n');
        // the towns (hull, patrol group) and the patrol groups (their points as unit@tile)
        for town in player_list(p as u64, EE_UNITS_LISTS_TOWNS) {
            let town = town as usize;
            if town == 0 {
                continue;
            }
            let group = h(O::DmTownPatrol, town, &[]);
            s.push_str(&format!(
                "TOWN {i} {} units={} patrol={} hull=",
                ld(h(O::DmTownId, town, &[])),
                ld(h(O::DmTownUnits, town, &[])),
                if group != 0 { ld(pg_id(group as usize)) } else { -1 }
            ));
            for v in 0..lu(h(O::DmTownHullCount, town, &[])) {
                s.push_str(&format!(
                    "{},{} ",
                    ld(h(O::DmTownHullX, town, &[i64::from(v)])),
                    ld(h(O::DmTownHullY, town, &[i64::from(v)]))
                ));
            }
            s.push('\n');
        }
        for group in player_list(p as u64, EE_UNITS_LISTS_PATROLS) {
            let group = group as usize;
            if group == 0 {
                continue;
            }
            s.push_str(&format!(
                "PATROL {i} {} town={} loop={} points=",
                lu(pg_id(group)),
                pg_flags(group).0,
                pg_flags(group).1
            ));
            for point in pg_points(group) {
                if point != 0 {
                    s.push_str(&format!(
                        "{}@{},{} ",
                        lu(gc::h(gc::O::RoUnitId, point, &[])),
                        ld(gc::h(gc::O::DkTileX, point, &[])),
                        ld(gc::h(gc::O::DkTileY, point, &[]))
                    ));
                }
            }
            s.push('\n');
        }
        o.push(&s);
    }
    // each player's unit list in its order (save 0.270)
    for i in 0..lu(gc::h(gc::O::MmNumPlayers, 0, &[])) {
        let p = gc::h(gc::O::SsWorldPlayer, 0, &[i64::from(i)]) as usize;
        if p == 0 {
            continue;
        }
        let mut s = format!("PLAYERUNITS {i}:");
        let mut u = gc::h(gc::O::PlFirstUnit, p, &[]) as usize;
        while u != 0 {
            s.push_str(&format!(" {}", lu(gc::h(gc::O::RoUnitId, u, &[]))));
            u = gc::h(gc::O::UnNext, u, &[]) as usize;
        }
        s.push('\n');
        o.push(&s);
    }
}

/// The unit's Rust record (`static_cast<EeUnitsUnitFields*>`).
fn unit_fields(u: usize) -> *const EeUnitsUnitFields {
    gc::h(gc::O::UnFields, u, &[]) as usize as *const EeUnitsUnitFields
}

/// The complex unit's Rust record (`static_cast<EeUnitsComplexUnitFields*>`).
fn cx_fields(c: usize) -> *const EeUnitsComplexUnitFields {
    gc::h(gc::O::CxFields, c, &[]) as usize as *const EeUnitsComplexUnitFields
}

/// A unit's id (0: none).
fn unit_id(u: usize) -> u32 {
    if u == 0 {
        0
    } else {
        gc::h(gc::O::RoUnitId, u, &[]) as u32
    }
}

fn dump_unit(o: &mut Out, u: usize) {
    let player = h(O::DmUnitPlayerId, u, &[]);
    let mut s = format!(
        "U {} type={} player={} hp={} tile={},{}",
        lu(gc::h(gc::O::RoUnitId, u, &[])),
        ld(h(O::DmUnitTypeRaw, u, &[])),
        ld(player),
        f(O::DmUnitHp, u, &[]),
        ld(gc::h(gc::O::DkTileX, u, &[])),
        ld(gc::h(gc::O::DkTileY, u, &[]))
    );
    let c = gc::h(gc::O::FxDynComplex, u, &[]) as usize;
    // SAFETY (the unit records here): the live unit's (complex unit's) Rust record, read under the world lock
    let x = if c != 0 { (unsafe { (*unit_fields(c)).mComplexUnit }) as usize } else { 0 };
    if x == 0 {
        s.push_str("\n  rawu "); // a simple unit's bytes (layout.json "EEUnit")
        s.push_str(&raw(u, h(O::DmUnitSize, 0, &[]) as usize));
        s.push('\n');
        o.push(&s);
        return;
    }
    let r = cx_fields(x);
    let waypoints = h(O::DmCxWpCount, x, &[]);
    s.push_str(&format!(
        " upd={} cmd={} grp={} loc={},{},{} h={} c1={} c2={} wp={} pathdone={} arrived={} vel={} blocked={}",
        lu(unsafe { (*r).mTimesUpdated } as i64),
        ld(h(O::DmCxCommand, x, &[])),
        i64::from(!unsafe { (*r).mActiveGroup }.is_null()),
        f(O::DmCxLocX, x, &[]),
        f(O::DmCxLocY, x, &[]),
        f(O::DmCxLocZ, x, &[]),
        hexf(f64::from(unsafe { (*r).mHeight })),
        x8(h(O::DmComplexChecksum, c, &[1])),
        x8(h(O::DmComplexChecksum, c, &[2])),
        waypoints as u64,
        i64::from(unsafe { (*r).mIsPathDone }),
        i64::from(unsafe { (*r).mHasArrived }),
        hexf(f64::from(gc::hd(gc::O::EcVelocity, x, &[], &[]))),
        unit_id(unsafe { (*r).mBlockingUnit } as usize)
    ));
    s.push_str(&format!(
        " anim={}/{}/{}/{} reload={}",
        lu(unsafe { (*r).mAnimation } as i64),
        lu(h(O::DmCxFrame, x, &[])),
        lu(unsafe { (*r).mAnimationTime } as i64),
        i64::from(unsafe { (*r).mIsAnimationPaused }),
        ld(i64::from(unsafe { (*r).mReloadTimer }))
    ));
    s.push_str(" wps="); // the path's points (location, distance, start distance)
    for w in 0..waypoints {
        s.push_str(&format!(
            "{},{},{}/{}/{};",
            f(O::DmCxWpX, x, &[w]),
            f(O::DmCxWpY, x, &[w]),
            f(O::DmCxWpZ, x, &[w]),
            f(O::DmCxWpDistance, x, &[w]),
            f(O::DmCxWpStart, x, &[w])
        ));
    }
    let queues = ld(h(O::DmCxQueues, x, &[]));
    for q in 0..i64::from(queues) {
        for g in 0..h(O::DmCxQueueSize, x, &[q]) {
            let goal = h(O::DmCxGoal, x, &[q, g]) as usize;
            s.push_str(&format!(" g{q}:k{}:{}", h(O::DmGoalKind, goal, &[]) as u32, x8(h(O::DmGoalOos, goal, &[]))));
        }
    }
    // what the raw bytes hold only as pointers: the contacts, the last attacker, the area effects
    s.push_str("\n  refs contacts=");
    let mut n = h(O::DmCxContactHead, x, &[]) as usize;
    while n != 0 {
        s.push_str(&format!("{},", lu(h(O::DmNodeUnitId, n, &[]))));
        n = h(O::DmNodeNext, n, &[]) as usize;
    }
    let ae = h(O::DmCxAeCount, x, &[]);
    let affecting = h(O::DmCxAffectingCount, x, &[]);
    s.push_str(&format!(
        " attacker={} owner={} holder={} ae={}/{}",
        unit_id(unsafe { (*r).mLastAttacker } as usize),
        unit_id(unsafe { (*r).mOwner } as usize),
        unit_id(unsafe { (*r).mHolder } as usize),
        ae as u64,
        affecting as u64
    ));
    for a in 0..affecting {
        s.push_str(&format!(" {}", lu(h(O::DmCxAffectingUnitId, x, &[a]))));
    }
    for a in 0..ae {
        s.push_str(&format!(" own@{},{}", f(O::DmCxAeX, x, &[a]), f(O::DmCxAeY, x, &[a])));
    }
    s.push_str("\n  raw "); // the unit's bytes (pointers differ by run; build/clang/layout.json names offsets)
    s.push_str(&raw(c, h(O::DmCxSize, 0, &[]) as usize));
    s.push('\n');
    let id = lu(gc::h(gc::O::RoUnitId, u, &[]));
    o.push(&s);
    for q in 0..i64::from(queues) {
        // each goal's own out-of-synch dump
        for g in 0..h(O::DmCxQueueSize, x, &[q]) {
            o.push(&format!("  unit {id} goal {q}.{g}\n"));
            let goal = h(O::DmCxGoal, x, &[q, g]) as usize;
            o.cpp(O::DmGoalDump, goal);
        }
    }
}

/// the active calamities, in order: their checksum's fields (EECalamity::GetOutOfSynchChecksum) and their units
fn dump_calamities(o: &mut Out) {
    let manager = gc::h(gc::O::CalamityManager, 0, &[]) as usize;
    if manager == 0 {
        return;
    }
    for i in 0..h(O::DmCalCount, manager, &[]) {
        let k = gc::h(gc::O::MgrCalamityAt, manager, &[i]) as usize;
        if k == 0 {
            continue;
        }
        let b = |op| h(op, k, &[]);
        let line = format!(
            "CAL {i} class={} sum={} target={},{},{} tile={},{} int={},{} ids={},{} units={},{} player={} \
             bools={}{}{}{}{} floats={},{},{} epi={} alive={} discrete={} elapsed={}\n",
            ld(b(O::DmCalClass)),
            x8(b(O::DmCalOos)),
            f(O::DmCalTargetX, k, &[]),
            f(O::DmCalTargetY, k, &[]),
            f(O::DmCalTargetZ, k, &[]),
            lu(b(O::DmCalTileX)),
            lu(b(O::DmCalTileY)),
            ld(b(O::DmCalInt1)),
            ld(b(O::DmCalInt2)),
            lu(b(O::DmCalTargetId)),
            lu(b(O::DmCalOriginId)),
            lu(b(O::DmCalTargetUnitId)),
            lu(b(O::DmCalCasterId)),
            lu(b(O::DmCalPlayer)),
            b(O::DmCalBool1) as i32,
            b(O::DmCalBool2) as i32,
            b(O::DmCalInitialized) as i32,
            b(O::DmCalHasLocation) as i32,
            b(O::DmCalHasUnit) as i32,
            f(O::DmCalFloat1, k, &[]),
            f(O::DmCalFloat2, k, &[]),
            f(O::DmCalHeight, k, &[]),
            f(O::DmCalEpicenter, k, &[]),
            f(O::DmCalTimeAlive, k, &[]),
            f(O::DmCalDiscrete, k, &[]),
            f(O::DmCalElapsed, k, &[])
        );
        o.push(&line);
    }
}

/// the units of each map region in order (RTSRegionTable: FindImpactUnit and the target searches walk them)
/// (ee_combat regions.rs's saved order: the count, then each region's index, size and ids, -1 for none)
fn dump_regions(o: &mut Out) {
    let table = h(O::DmMapRegions, 0, &[]) as usize as *const ee_sim::combat::regions::EeCombatUnitRegions;
    if table.is_null() {
        return;
    }
    // SAFETY: the map's live region table, under the world's lock: the counting call, then a buffer of that size
    let n = unsafe { ee_sim::combat::regions::ee_combat_regions_order(table, std::ptr::null_mut(), 0) };
    let mut order = vec![0u32; n];
    if n != 0 {
        unsafe { ee_sim::combat::regions::ee_combat_regions_order(table, order.as_mut_ptr(), order.len()) };
    }
    let mut k = 1;
    while k < order.len() {
        let count = order[k + 1] as usize;
        let mut s = format!("REGION {}:", order[k]);
        for v in &order[k + 2..k + 2 + count] {
            s.push_str(&format!(" {}", if *v != u32::MAX { *v } else { 0 }));
        }
        s.push('\n');
        o.push(&s);
        k += 2 + count;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hexf_is_the_ucrt_form() {
        assert_eq!(hexf(1.0), "0x1.0000000000000p+0");
        assert_eq!(hexf(1.5), "0x1.8000000000000p+0");
        assert_eq!(hexf(-2.0), "-0x1.0000000000000p+1");
        assert_eq!(hexf(0.1), "0x1.999999999999ap-4");
        assert_eq!(hexf(f64::from(0.1f32)), "0x1.99999a0000000p-4");
        assert_eq!(hexf(0.0), "0x0.0000000000000p+0");
        assert_eq!(hexf(-0.0), "-0x0.0000000000000p+0");
        assert_eq!(hexf(f64::from_bits(1)), "0x0.0000000000001p-1022");
        assert_eq!(hexf(f64::INFINITY), "inf");
        assert_eq!(hexf(1024.0), "0x1.0000000000000p+10");
    }

    #[test]
    fn longs_are_32_bit() {
        assert_eq!(lu(-1), 4_294_967_295);
        assert_eq!(ld(0xffff_ffff), -1);
        assert_eq!(x8(0x1_0000_00ab), "000000ab");
    }

    #[test]
    fn raw_is_hex_bytes() {
        let b = [0u8, 0x1f, 0xa0];
        assert_eq!(raw(b.as_ptr() as usize, 3), "001fa0");
    }
}
