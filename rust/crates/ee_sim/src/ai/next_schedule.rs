//! The computer players' schedule (was EEArtificialIntelligence.cpp, the AI Thread cluster's round 8): the game's
//! computer players collected after a game is set up, and the physics tick's AI step: the game log's tick, the sim
//! hash (EE_AI_TRACE), the next engine's micro step, then one computer player's Update per interval (1000 ms over
//! the players), timed for the EE-AIPROF lines while the trace is on. A child module of `next_game`. What only C++
//! holds is one `ee_ai_next_host` case each (EE_AI_NEXT_HOST_* from 66).

use super::*;

/// `EEGameLogTick(a)`; `gCPTraceHashOn`; the computer player's `Update(a)`
pub const EE_AI_NEXT_HOST_GAME_LOG_TICK: i32 = 66;
pub const EE_AI_NEXT_HOST_TRACE_HASH_ON: i32 = 67;
pub const EE_AI_NEXT_HOST_CP_UPDATE: i32 = 68;

/// `EEArtificialIntelligence`'s members: the players (`EEComputerPlayer*`), the interval, the next time and player.
struct Schedule {
    players: Vec<u64>,
    interval: c_ulong,
    next_time: c_ulong,
    next_player: c_ulong,
}

static SCHEDULE: Mutex<Schedule> =
    Mutex::new(Schedule { players: Vec::new(), interval: 1000, next_time: 0, next_player: 0 });

fn schedule() -> std::sync::MutexGuard<'static, Schedule> {
    SCHEDULE.lock().unwrap_or_else(|e| e.into_inner())
}

/// `CollectComputerPlayers` (after a game is set up): the players' computer players in slot order.
#[no_mangle]
pub extern "C" fn ee_ai_collect_computer_players() {
    let players: Vec<u64> = {
        let t = unsafe { WorldRead::begin() };
        WorldView::global(&t).map_or(Vec::new(), |w| {
            (0..w.num_players())
                .filter_map(|i| w.player(i as usize))
                .map(|p| computer(&p))
                .filter(|&c| c != 0)
                .collect()
        })
    };
    let mut s = schedule();
    if !players.is_empty() {
        s.interval = 1000 / players.len() as c_ulong;
    }
    s.players = players;
    s.next_player = 0;
    s.next_time = 0;
}

fn computer(p: &PlayerView<'_>) -> u64 {
    host(EE_AI_NEXT_HOST_COMPUTER, std::ptr::null_mut(), p.handle() as i64, 0, 0) as u64
}

/// `Update` (the physics tick): the game log's tick, the sim hash, the micro step, then the computer player whose
/// turn it is.
///
/// # Safety
/// The simulation thread, the game set up.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_update() {
    let time = now();
    host(EE_AI_NEXT_HOST_GAME_LOG_TICK, std::ptr::null_mut(), time as i64, 0, 0);
    let count = schedule().players.len();
    if host(EE_AI_NEXT_HOST_TRACE_HASH_ON, std::ptr::null_mut(), 0, 0, 0) != 0 && count > 0 {
        trace::ee_ai_trace_hash(time);
    }
    // SAFETY: the caller's contract
    unsafe { ee_ai_next_micro_tick(time) };
    let (player, ready) = {
        let s = schedule();
        let ready = time >= s.next_time && !s.players.is_empty();
        (s.players.get(s.next_player as usize).copied().unwrap_or(0), ready)
    };
    if !ready {
        return;
    }
    // SAFETY: a collected computer player (an `EEPlayer*` too)
    let active = player != 0 && unsafe { (*crate::cp::player_game::ctx_of(player).fields).mActive };
    if active {
        let me = player as usize as *mut c_void;
        if trace_on() {
            // the AI's real-time cost (EE-AIPROF lines; never fed back into the simulation)
            let start = std::time::Instant::now();
            host(EE_AI_NEXT_HOST_CP_UPDATE, me, time as i64, 0, 0);
            let microseconds = start.elapsed().as_micros() as i64;
            trace::ee_ai_trace_profile(player_id(me), time, microseconds);
        } else {
            host(EE_AI_NEXT_HOST_CP_UPDATE, me, time as i64, 0, 0);
        }
    }
    let mut s = schedule();
    s.next_player += 1;
    if s.next_player as usize == s.players.len() {
        s.next_player = 0;
    }
    s.next_time = s.interval.wrapping_add(time);
}

/// The save's part (EEPWorldHelper: `EE_UNITS_WORLD_AI_SCHEDULE`): the next player and time -> the player count.
///
/// # Safety
/// `next_player` and `next_time` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_schedule_get(next_player: *mut i64, next_time: *mut i64) -> i64 {
    let s = schedule();
    // SAFETY: the caller's contract
    unsafe {
        *next_player = s.next_player as i64;
        *next_time = s.next_time as i64;
    }
    s.players.len() as i64
}

/// A load's schedule (`EE_UNITS_WORLD_AI_SET_SCHEDULE`) -> 0.
#[no_mangle]
pub extern "C" fn ee_ai_schedule_set(next_player: i64, next_time: i64) -> i64 {
    let mut s = schedule();
    s.next_player = next_player as c_ulong;
    s.next_time = next_time as c_ulong;
    0
}
