//! The data pre-loading thread's bodies (`EEDataPreLoading`, world lane round 1): its loop, its messages, the queue it
//! holds while queueing, the media loads and the game's load. The C++ class keeps its thread and events (the shutdown
//! in Empire Earth.cpp reads them) and its one-statement posts; the queue and the two flags are Rust's (one
//! pre-loader per game). Each C++ call is one operation ([`EePreloadCpp`]) answered by a one-statement case of
//! `ee_preloadcpp` (`src/Empire Earth/Data Pre-Loading Thread/EEDataPreLoading.cpp`).

use std::ffi::{c_long, c_ulong, c_void};
use std::sync::Mutex;

use ee_base::util::fp;

use crate::gamecpp::globals;

/// The pre-loader's calls (p: the `EEDataPreLoading`, or the object a call names).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EePreloadCpp {
    /// `mThread->Receive(*a0, *a1, *a2)`
    Receive = 1,
    ThreadCreatedSet,
    ThreadDoneSet,
    QueueDoneSet,
    QueueDoneWait,
    QueueDoneClear,
    ThreadPost,
    ThreadPriority,
    SetThreadPriority,
    /// `gWorld.mPlayers[a0]`
    Player,
    /// `gUnitClassTable[a0]`
    Class,
    /// `mTechnologies->Get(a0)` of the player p
    Technology,
    ClassUnitType,
    SetUnitClassMedia,
    LoadUnitClassButtons,
    UnloadUnitClassMedia,
    UnloadUnitClassButtons,
    TechLoadMedia,
    TechUnloadMedia,
    // ---- the game's load
    NewOptions,
    DeleteOptions,
    NewError,
    DeleteError,
    LoadType,
    SetLoadingSavedGame,
    GameStateCreate,
    GameStateReset,
    ReleaseScene,
    WorldWriteLock,
    WorldWriteUnlock,
    WorldClear,
    StartGame,
    IsMultiplayer,
    IsClient,
    ProcessLocalInput,
    SetHasLoaded,
    DisplayError,
    // ---- the constructor and destructor (round 12)
    /// `TSScheduler::CreateEvent(<a0: 0 mThreadCreated, 1 mThreadDone, 2 mMessageQueueDone>) == keseOK`
    CreateEvent,
    /// `TSScheduler::DestroyEvent(<a0 as CreateEvent>)`
    DestroyEvent,
    /// `TSScheduler::CreateThread(mThread, EEDataPreLoading::Main, p) == keseOK`
    CreateThread,
    DestroyThread,
    /// `mThread->SetPriority(a0) == keseOK`
    SetThreadPriorityOk,
    ThreadCreatedWait,
    /// `mThread->SetName(gDataPreLoadingThreadName)`
    SetThreadName,
    /// `ESVERIFY_AT(a0 != 0, a1)`
    Verify,
}

use EePreloadCpp as O;

/// The load's texts (ee/EEStringIDs.h): IDS_LOADING_GLOBAL_MEDIA, IDS_POPULATING_WORLD, IDS_LOADING_COMPLETE,
/// IDS_WAITING_FOR_PLAYERS.
const IDS_LOADING_GLOBAL_MEDIA: c_long = 30291;
const IDS_POPULATING_WORLD: c_long = 30294;
const IDS_LOADING_COMPLETE: c_long = 30296;
const IDS_WAITING_FOR_PLAYERS: c_long = 30606;

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use super::EePreloadCpp;
    extern "C" {
        // EEDataPreLoading.cpp
        pub fn ee_preloadcpp(op: EePreloadCpp, p: usize, a: *const i64, f: *const f32) -> i64;
    }
}

/// The tests run no C++: a call is a bug of the test.
#[cfg(test)]
mod sys {
    use super::EePreloadCpp;
    pub unsafe fn ee_preloadcpp(op: EePreloadCpp, _: usize, _: *const i64, _: *const f32) -> i64 {
        panic!("no C++ in the tests ({op:?})")
    }
}

type P = usize;

fn hf(op: O, p: P, a: &[i64], f: &[f32]) -> i64 {
    let mut ai = [0i64; 4];
    ai[..a.len()].copy_from_slice(a);
    let mut fi = [0f32; 2];
    fi[..f.len()].copy_from_slice(f);
    // SAFETY: the host's operation over the C++ objects the pre-loading thread holds
    unsafe { sys::ee_preloadcpp(op, p, ai.as_ptr(), fi.as_ptr()) }
}

fn h(op: O, p: P, a: &[i64]) -> i64 {
    hf(op, p, a, &[])
}

fn h0(op: O) -> i64 {
    hf(op, 0, &[], &[])
}

/// `kThreadMessageQuit` and the pre-loader's messages.
const MESSAGE_QUIT: u32 = 0x401;
const MESSAGE_LOAD_GAME: u32 = 0x402;
const MESSAGE_LOAD_CLASS: u32 = 0x404;
const MESSAGE_START_QUEUEING: u32 = 0x407;
const MESSAGE_FLUSH_QUEUE: u32 = 0x408;
const MESSAGE_UNLOAD_CLASS: u32 = 0x409;
const MESSAGE_LOAD_TECHNOLOGY: u32 = 0x40a;
const MESSAGE_UNLOAD_TECHNOLOGY: u32 = 0x40b;
const MESSAGE_QUEUE_DONE: u32 = 0x40c;
/// `PLAT_PRIORITY_ABOVE_NORMAL`
const PRIORITY_ABOVE_NORMAL: i64 = 1;
/// `kFPPrecision53`.
const FP_PRECISION_53: c_ulong = ee_base::lle_enums::UMFloatingPointPrecision::kFPPrecision53 as c_ulong;

/// The pre-loader's state that was its `mQuit`, `mQueueing` and `mQueue`.
struct State {
    quit: bool,
    queueing: bool,
    queue: Vec<(u32, usize, usize)>,
}

static STATE: Mutex<State> = Mutex::new(State { quit: false, queueing: false, queue: Vec::new() });

fn state() -> std::sync::MutexGuard<'static, State> {
    STATE.lock().unwrap_or_else(|e| e.into_inner())
}

/// `HandleMessage`: quit, queueing on and the flush, a unit class's or a technology's media (un)loaded, the game
/// loaded, the queue's end reported.
fn handle_message(p: P, message: u32, parameter1: usize, parameter2: usize) {
    match message {
        MESSAGE_QUIT => state().quit = true,
        MESSAGE_START_QUEUEING => state().queueing = true,
        MESSAGE_FLUSH_QUEUE => flush_queue(p),
        MESSAGE_LOAD_CLASS | MESSAGE_UNLOAD_CLASS => {
            let player = h(O::Player, 0, &[parameter1 as isize as i64]) as P;
            if player != 0 {
                let class = h(O::Class, 0, &[parameter2 as isize as i64]) as P;
                if class != 0 {
                    let unit_type = h(O::ClassUnitType, class, &[]);
                    if message == MESSAGE_LOAD_CLASS {
                        h(O::SetUnitClassMedia, player, &[unit_type]);
                        h(O::LoadUnitClassButtons, player, &[unit_type]);
                    } else {
                        h(O::UnloadUnitClassMedia, player, &[unit_type]);
                        h(O::UnloadUnitClassButtons, player, &[unit_type]);
                    }
                }
            }
        }
        MESSAGE_LOAD_TECHNOLOGY | MESSAGE_UNLOAD_TECHNOLOGY => {
            let player = h(O::Player, 0, &[parameter1 as isize as i64]) as P;
            if player != 0 {
                let node = h(O::Technology, player, &[parameter2 as isize as i64]) as P;
                if node != 0 {
                    h(
                        if message == MESSAGE_LOAD_TECHNOLOGY { O::TechLoadMedia } else { O::TechUnloadMedia },
                        node,
                        &[player as i64],
                    );
                }
            }
        }
        MESSAGE_LOAD_GAME => load_game(p, parameter1 != 0, parameter2 != 0),
        MESSAGE_QUEUE_DONE => {
            h(O::QueueDoneSet, p, &[]);
        }
        _ => {}
    }
}

/// `LoadGame`: the game world built from the current game options with the load progress, then loaded reported to
/// the host; on failure the game exits and shows the error.
fn load_game(p: P, skip_game_media: bool, clear_world: bool) {
    let options = h0(O::NewOptions);
    let error = h0(O::NewError);
    let priority = h(O::ThreadPriority, p, &[]);
    h(O::SetThreadPriority, p, &[PRIORITY_ABOVE_NORMAL]);
    crate::screens::ui_objects::ui_set_load_progress(0.0);
    crate::screens::ui_objects::ui_set_loading_text(IDS_LOADING_GLOBAL_MEDIA);
    if !skip_game_media {
        crate::gamecpp::startup::ee_gamecpp_startup_load_media();
    }
    let load_type = h0(O::LoadType);
    let saved = load_type == 4 || load_type == 1 || load_type == 2;
    if saved {
        h(O::SetLoadingSavedGame, 0, &[1]);
    }
    let created = h(O::GameStateCreate, 0, &[options, error]) != 0;
    if created {
        if clear_world {
            h0(O::ReleaseScene);
            // SAFETY: gGraphics, set for the game's whole run
            crate::gamecpp::render_to::ee_gamecpp_gfx_post_50a_and_wait(unsafe { globals::gGraphics });
            h0(O::WorldWriteLock);
            h0(O::WorldClear);
            // SAFETY: gPhysics, set for the game's whole run
            crate::physics::ee_physics_clear_server_messages(unsafe { globals::gPhysics });
            h0(O::WorldWriteUnlock);
        }
        crate::screens::ui_objects::ui_set_load_progress(0.6);
        crate::screens::ui_objects::ui_set_loading_text(IDS_POPULATING_WORLD);
        h0(O::StartGame);
        crate::screens::ui_objects::ui_resume_graphics();
    }
    h0(O::GameStateReset);
    if saved {
        h(O::SetLoadingSavedGame, 0, &[0]);
    }
    if created {
        if h0(O::IsMultiplayer) == 0 {
            crate::screens::ui_objects::ui_set_loading_text(IDS_LOADING_COMPLETE);
        } else {
            crate::screens::ui_objects::ui_set_loading_text(IDS_WAITING_FOR_PLAYERS);
        }
        crate::screens::ui_objects::ui_set_load_progress(1.0);
        // kMessageStartLoadingResponse {the client's send-to slot (the server's 0xfe), the local player's id, 0}
        let slot = if h0(O::IsClient) != 0 {
            ee_net::net::ffi_client::ee_net_cl_my_send_to_slot(crate::gamecpp::net::global().client) as i64
        } else {
            0xfe
        };
        let player = crate::gamecpp::hp(crate::gamecpp::O::WorldCurrentPlayer, 0, &[]);
        let id = crate::gamecpp::h(crate::gamecpp::O::RoPlayerId, player, &[]);
        let values = [slot, id, 0].map(crate::gamecpp::mrecord::EeGamecppMrValue::integer);
        let response = crate::gamecpp::mrecord::new_record(
            ee_sim::message_ids::EEMessageID::kMessageStartLoadingResponse as u8,
            &values,
        );
        h(O::ProcessLocalInput, response as P, &[]);
        if h0(O::IsClient) != 0 {
            h0(O::SetHasLoaded);
        }
    } else {
        crate::gamecpp::utilities::ee_gamecpp_exit_game();
        h(O::DisplayError, error as P, &[]);
    }
    h(O::SetThreadPriority, p, &[priority]);
    h(O::DeleteError, error as P, &[]);
    h(O::DeleteOptions, options as P, &[]);
}

/// `FlushQueue`: queueing off, every queued message handled in order, the queue emptied.
fn flush_queue(p: P) {
    let queue = {
        let mut s = state();
        s.queueing = false;
        std::mem::take(&mut s.queue)
    };
    for (message, parameter1, parameter2) in queue {
        handle_message(p, message, parameter1, parameter2);
    }
}

/// `QueueMessage`: a message held for later; the flush message handles the queue now.
fn queue_message(p: P, message: u32, parameter1: usize, parameter2: usize) {
    if message == MESSAGE_FLUSH_QUEUE {
        flush_queue(p);
    } else {
        state().queue.push((message, parameter1, parameter2));
    }
}

/// One `Receive` of the thread: its result, the message and the two parameters.
fn receive(p: P) -> (i64, u32, usize, usize) {
    let mut message: c_ulong = 0;
    let mut parameter1: usize = 0;
    let mut parameter2: usize = 0;
    let result = h(
        O::Receive,
        p,
        &[
            &mut message as *mut c_ulong as usize as i64,
            &mut parameter1 as *mut usize as usize as i64,
            &mut parameter2 as *mut usize as usize as i64,
        ],
    );
    (result, message as u32, parameter1, parameter2)
}

/// `Main`: the thread drains its messages (queueing or handling each, 1 ms apart) until told to quit.
#[no_mangle]
pub extern "C" fn ee_preload_main(p: *mut c_void) -> c_ulong {
    let p = p as P;
    receive(p);
    h(O::ThreadCreatedSet, p, &[]);
    fp::ee_util_fp_set_precision(FP_PRECISION_53);
    while !state().quit {
        loop {
            let (result, message, parameter1, parameter2) = receive(p);
            if fp::ee_util_fp_precision() != FP_PRECISION_53 {
                fp::ee_util_fp_set_precision(FP_PRECISION_53);
            }
            if fp::ee_util_fp_rounding() != 0 {
                fp::ee_util_fp_set_rounding(0);
            }
            if result == 0 {
                if state().queueing {
                    queue_message(p, message, parameter1, parameter2);
                } else {
                    handle_message(p, message, parameter1, parameter2);
                }
            }
            ee_base::plat_core::time::PlatSleep(1);
            if result != 0 {
                break;
            }
        }
    }
    h(O::ThreadDoneSet, p, &[]);
    0
}

#[no_mangle]
pub extern "C" fn ee_preload_start_queueing() {
    state().queueing = true;
}

/// The pre-loader made (`fresh`) or destroyed: the flags cleared, the queued messages dropped.
/// The constructor's body: the three events, the queue reset, the thread made at `priority` and waited for, then
/// named.
#[no_mangle]
pub extern "C" fn ee_preload_construct(p: *mut c_void, priority: c_long) {
    let p = p as P;
    for (event, line) in [(0, 55), (1, 56), (2, 57)] {
        h(O::Verify, 0, &[h(O::CreateEvent, p, &[event]), line]);
    }
    ee_preload_reset();
    h(O::Verify, 0, &[h(O::CreateThread, p, &[]), 65]);
    h(O::Verify, 0, &[h(O::SetThreadPriorityOk, p, &[i64::from(priority)]), 66]);
    h(O::ThreadCreatedWait, p, &[]);
    h(O::SetThreadName, p, &[]);
}

/// The destructor's body: the events destroyed, the queue reset, the thread destroyed.
#[no_mangle]
pub extern "C" fn ee_preload_destruct(p: *mut c_void) {
    let p = p as P;
    for event in 0..3 {
        h(O::DestroyEvent, p, &[event]);
    }
    ee_preload_reset();
    h(O::DestroyThread, p, &[]);
}

#[no_mangle]
pub extern "C" fn ee_preload_reset() {
    let mut s = state();
    s.quit = false;
    s.queueing = false;
    s.queue.clear();
}

/// `WaitForMessageQueue`: waits until the thread has handled every message posted so far -> the post's result.
#[no_mangle]
pub extern "C" fn ee_preload_wait_for_queue(p: *mut c_void) -> i64 {
    let p = p as P;
    let result = h(O::ThreadPost, p, &[MESSAGE_QUEUE_DONE as i64]);
    if result == 0 {
        h(O::QueueDoneWait, p, &[]);
        h(O::QueueDoneClear, p, &[]);
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_load_texts_are_the_resource_strings() {
        let text = |id: c_long| ee_base::util::strings::resource_string(id as u16).unwrap();
        assert!(text(IDS_LOADING_GLOBAL_MEDIA).starts_with("Loading Global Media"));
        assert!(text(IDS_POPULATING_WORLD).starts_with("Populating World"));
        assert!(text(IDS_LOADING_COMPLETE).starts_with("Loading Complete"));
        assert!(text(IDS_WAITING_FOR_PLAYERS).starts_with("Waiting for players"));
    }

    /// One test: the state is the process's (tests run in parallel).
    #[test]
    fn queueing_holds_messages_and_quit_touches_no_cpp() {
        ee_preload_reset();
        ee_preload_start_queueing();
        assert!(state().queueing);
        queue_message(0, 0x500, 1, 2);
        assert_eq!(state().queue, vec![(0x500, 1, 2)]);
        ee_preload_reset();
        assert!(state().queue.is_empty() && !state().queueing && !state().quit);
        handle_message(0, 0x555, 0, 0);
        handle_message(0, MESSAGE_QUIT, 0, 0);
        assert!(state().quit);
        ee_preload_reset();
    }
}
