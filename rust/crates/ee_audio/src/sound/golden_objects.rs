//! The Sound Engine objects' golden (`golden_objects.txt`): what the unmodified C++ units did, printed by the
//! driver `tests/sound/se_golden.cpp` (random sequences of the public calls of SESoundManager, its nodes, SESample,
//! SEMixer, SEConductor and SEScore over fakes at the engine's seams). Here the same lines run over the Rust objects:
//! this module defines those seams (`sys.rs`'s C names: the Thread System's C ABI, dmusic, the platform, the C++
//! trampolines) as the driver's fakes, executes each line's call the way the C++ forwarders make it, and rebuilds
//! the line. The C++ evaluates a call's arguments right to left (clang, MSVC's ABI), so a line whose result has
//! several calls makes them last first, as the driver did.
//!
//! `EE_SOUND_OBJECTS_GOLDEN=<file> cargo test -p ee_sound --lib golden_objects -- --nocapture` replays a long run.

// the C ABI's `long`/`unsigned long` are 32-bit on Windows and 64-bit on LP64: the casts are not no-ops there
#![allow(clippy::unnecessary_cast, clippy::manual_dangling_ptr)]

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::{c_char, c_int, c_long, c_ulong, c_void, CStr};
use std::fmt::Write as _;
use std::ptr::null_mut;

use crate::sound::conductorobj::*;
use crate::sound::managerobj::*;
use crate::sound::mixerobj::*;
use crate::sound::objects::*;
use crate::sound::sys::DmResolver;

// ---- the fakes' state (the driver's globals)
#[derive(Default)]
struct World {
    line: String,
    next_ts: i32,
    wait_script: Vec<i32>,
    last_thread: Option<(usize, usize)>, // (function, parameter)
    precision: u32,
    // the plug-in
    samples: BTreeMap<usize, i32>,
    next_sample: i32,
    channels: u32,
    max_channels: u32,
    busy: Vec<i32>,
    fail_next: i32,
    play_time: i32,
    volume: f32,
    output_rate: u32,
    music_source: Option<(usize, usize)>,
    module_valid: [bool; 4],
    module_index: i32,
    module_count: i32,
    module_next: u32,
    load_fail: bool,
    // dmusic
    next_dm: i32,
    fail_loader: bool,
    fail_performance: bool,
    fail_play: bool,
    // the paths
    path_refs: [u32; 2],
    finds: i32,
}

thread_local! {
    static W: RefCell<World> = RefCell::new(World { max_channels: 32, volume: 1.0, path_refs: [1, 1], busy: vec![0; 64], ..Default::default() });
}

fn w<R>(f: impl FnOnce(&mut World) -> R) -> R {
    W.with(|w| f(&mut w.borrow_mut()))
}

fn t(s: String) {
    w(|w| {
        w.line.push(' ');
        w.line.push_str(&s);
    })
}

fn bits(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

// ---- the Thread System's C ABI
struct Section {
    id: i32,
    depth: i32,
}
struct Event {
    id: i32,
}
struct Thread {
    id: i32,
}

fn next_ts() -> i32 {
    w(|w| {
        w.next_ts += 1;
        w.next_ts
    })
}

/// ee_util's shutdown list: `ee_sound_initialize` is not part of the golden.
#[export_name = "ee_sound_fake_ee_util_register_shutdown"]
extern "C" fn ee_util_register_shutdown(_function: unsafe extern "C" fn(bool)) -> std::ffi::c_long {
    0
}
#[export_name = "ee_sound_fake_ee_ts_section_new"]
extern "C" fn ee_ts_section_new() -> *mut c_void {
    let id = next_ts();
    t(format!("cs+{id}"));
    Box::into_raw(Box::new(Section { id, depth: 0 })).cast()
}
#[export_name = "ee_sound_fake_ee_ts_section_free"]
unsafe extern "C" fn ee_ts_section_free(s: *mut c_void) {
    if s.is_null() {
        return;
    }
    let s = unsafe { Box::from_raw(s as *mut Section) };
    t(format!("cs-{}/{}", s.id, s.depth));
}
#[export_name = "ee_sound_fake_ee_ts_section_enter"]
unsafe extern "C" fn ee_ts_section_enter(s: *const c_void) {
    let s = unsafe { &mut *(s as *mut Section) };
    s.depth += 1;
    t(format!("E{}", s.id));
}
#[export_name = "ee_sound_fake_ee_ts_section_leave"]
unsafe extern "C" fn ee_ts_section_leave(s: *const c_void) {
    let s = unsafe { &mut *(s as *mut Section) };
    s.depth -= 1;
    t(format!("L{}", s.id));
}
#[export_name = "ee_sound_fake_ee_ts_event_new"]
unsafe extern "C" fn ee_ts_event_new(out: *mut *mut c_void, manual: bool) -> c_long {
    let id = next_ts();
    t(format!("ev+{id}{}", if manual { "m" } else { "" }));
    unsafe { *out = Box::into_raw(Box::new(Event { id })).cast() };
    0
}
#[export_name = "ee_sound_fake_ee_ts_event_free"]
unsafe extern "C" fn ee_ts_event_free(e: *mut c_void) {
    if e.is_null() {
        return;
    }
    let e = unsafe { Box::from_raw(e as *mut Event) };
    t(format!("ev-{}", e.id));
}
#[export_name = "ee_sound_fake_ee_ts_event_set"]
unsafe extern "C" fn ee_ts_event_set(e: *const c_void) -> c_long {
    t(format!("set{}", unsafe { (*(e as *const Event)).id }));
    0
}
#[export_name = "ee_sound_fake_ee_ts_event_clear"]
unsafe extern "C" fn ee_ts_event_clear(e: *const c_void) -> c_long {
    t(format!("clr{}", unsafe { (*(e as *const Event)).id }));
    0
}
#[export_name = "ee_sound_fake_ee_ts_event_wait"]
unsafe extern "C" fn ee_ts_event_wait(e: *const c_void, timeout: u32) -> c_long {
    t(format!("wait{}/{timeout}", unsafe { (*(e as *const Event)).id }));
    0
}
#[export_name = "ee_sound_fake_ee_ts_event_wait_any"]
unsafe extern "C" fn ee_ts_event_wait_any(events: *const *const c_void, count: u32, timeout: u32) -> i32 {
    let r = w(|w| if w.wait_script.is_empty() { 0 } else { w.wait_script.remove(0) });
    let id = unsafe { (*(*events as *const Event)).id };
    t(format!("any{id}/{count}/{timeout}>{r}"));
    r
}
#[export_name = "ee_sound_fake_ee_ts_thread_new"]
unsafe extern "C" fn ee_ts_thread_new(
    out: *mut *mut c_void,
    function: crate::sound::sys::EeSoundThreadFunction,
    parameter: *mut c_void,
    stack: u32,
    suspended: bool,
) -> c_long {
    let id = next_ts();
    t(format!("th+{id}/{stack}/{}", suspended as i32));
    w(|w| w.last_thread = Some((function.unwrap() as usize, parameter as usize)));
    unsafe { *out = Box::into_raw(Box::new(Thread { id })).cast() };
    0
}
#[export_name = "ee_sound_fake_ee_ts_thread_free"]
unsafe extern "C" fn ee_ts_thread_free(th: *mut c_void) -> c_long {
    if th.is_null() {
        return 0;
    }
    let th = unsafe { Box::from_raw(th as *mut Thread) };
    t(format!("th-{}", th.id));
    0
}
#[export_name = "ee_sound_fake_ee_ts_thread_set_name"]
unsafe extern "C" fn ee_ts_thread_set_name(th: *const c_void, name: *const c_char) {
    let name = unsafe { CStr::from_ptr(name) }.to_str().unwrap();
    t(format!("name{}={name}", unsafe { (*(th as *const Thread)).id }));
}

// ---- the platform, the engine's leaves, the C++ trampolines
#[allow(non_snake_case)]
#[export_name = "ee_sound_fake_PlatSleep"]
extern "C" fn PlatSleep(ms: u32) {
    t(format!("sleep{ms}"));
}
#[allow(non_snake_case)]
#[export_name = "ee_sound_fake_PlatGetLastError"]
extern "C" fn PlatGetLastError() -> u32 {
    0
}
extern "C" {
    fn fopen(path: *const c_char, mode: *const c_char) -> *mut c_void;
}
#[allow(non_snake_case)]
#[export_name = "ee_sound_fake_PlatFOpen"]
unsafe extern "C" fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void {
    let p = unsafe { CStr::from_ptr(path) }.to_bytes().to_vec();
    #[cfg(not(windows))]
    let p: Vec<u8> = p.into_iter().map(|b| if b == b'\\' { b'/' } else { b }).collect();
    let mut z = p;
    z.push(0);
    unsafe { fopen(z.as_ptr() as *const c_char, mode) }
}
#[no_mangle]
unsafe extern "C" fn ee_soundcpp_verify(_expression: *const c_char, line: u32) {
    t(format!("VERIFY {line}"));
}
#[no_mangle]
extern "C" fn ee_soundcpp_fp_precision53() {
    t("fpp=10000".into());
    w(|w| w.precision = 0x10000);
}
#[no_mangle]
extern "C" fn ee_soundcpp_fp_check() {
    if w(|w| w.precision) != 0x10000 {
        ee_soundcpp_fp_precision53();
    }
}
/// A path is the address of `W.path_refs[i]`'s index + 1.
#[no_mangle]
extern "C" fn ee_soundcpp_path_reference(path: *mut c_void) {
    w(|w| w.path_refs[path as usize - 1] += 1);
}
#[no_mangle]
extern "C" fn ee_soundcpp_path_dereference(path: *mut c_void) {
    let zero = w(|w| {
        w.path_refs[path as usize - 1] -= 1;
        w.path_refs[path as usize - 1] == 0
    });
    if zero {
        t("path-deleted".into());
    }
}
/// A file spec: its name.
struct Spec(Vec<u8>);
#[no_mangle]
unsafe extern "C" fn ee_soundcpp_find_file(_path: *mut c_void, name: *const c_char) -> *mut c_void {
    let name = unsafe { CStr::from_ptr(name) }.to_bytes().to_vec();
    t(format!("find:{}", String::from_utf8_lossy(&name)));
    w(|w| w.finds += 1);
    if name[0] == b'm' {
        return null_mut();
    }
    Box::into_raw(Box::new(Spec(name))).cast()
}
#[no_mangle]
unsafe extern "C" fn ee_soundcpp_file_delete(f: *mut c_void) {
    drop(unsafe { Box::from_raw(f as *mut Spec) });
}
#[no_mangle]
unsafe extern "C" fn ee_soundcpp_module_entry(_module: *mut c_void, name: *const c_char) -> *mut c_void {
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    match entry(name) {
        Some(p) => p,
        None => {
            t(format!("noentry:{}", String::from_utf8_lossy(name)));
            null_mut()
        }
    }
}
fn is_valid() -> bool {
    t("isvalid".into());
    w(|w| w.module_valid[w.module_index as usize])
}
#[no_mangle]
extern "C" fn ee_soundcpp_module_is_valid(_module: *mut c_void) -> bool {
    is_valid()
}
#[no_mangle]
extern "C" fn ee_soundcpp_module_unload(_module: *mut c_void) {
    t("unload".into());
}
fn get_module(start: *mut c_void, first: bool) -> i32 {
    let (next, count) = w(|w| {
        if first {
            w.module_next = 0;
        }
        (w.module_next, w.module_count)
    });
    t(format!("module{next}/{count}"));
    if next as i32 >= count {
        return 3;
    }
    w(|w| {
        w.module_index = w.module_next as i32;
        w.module_next += 1;
    });
    unsafe { (*(start as *mut EeSoundMixerStart)).module = 1 as *mut c_void };
    0
}
#[no_mangle]
extern "C" fn ee_soundcpp_first_module(start: *mut c_void) -> i32 {
    get_module(start, true)
}
#[no_mangle]
extern "C" fn ee_soundcpp_next_module(start: *mut c_void) -> i32 {
    get_module(start, false)
}

// ---- the mixer plug-in
fn label(s: *const c_void) -> i32 {
    w(|w| w.samples.get(&(s as usize)).copied().unwrap_or(-1))
}
fn fail() -> c_long {
    w(|w| std::mem::take(&mut w.fail_next)) as c_long
}
fn new_label(s: *mut c_void) -> i32 {
    w(|w| {
        w.next_sample += 1;
        w.samples.insert(s as usize, w.next_sample);
        w.next_sample
    })
}
unsafe extern "C" fn activate_mixer() -> c_long {
    t("activate".into());
    fail()
}
unsafe extern "C" fn deactivate_mixer() -> c_long {
    t("deactivate".into());
    fail()
}
unsafe extern "C" fn create_channels(count: c_ulong, _out: *mut c_void) -> c_long {
    t(format!("create{count}"));
    w(|w| w.channels = count as u32);
    0
}
unsafe extern "C" fn destroy_channels() -> c_long {
    t("destroy".into());
    w(|w| w.channels = 0);
    0
}
unsafe extern "C" fn delete_wave_data(s: *mut c_void) {
    t(format!("delwave:S{}", label(s)));
    w(|w| w.samples.remove(&(s as usize)));
}
unsafe extern "C" fn duplicate_sample(s: *const c_void, out: *mut *mut c_void) -> c_long {
    let copy = unsafe { ee_sound_sample_duplicate(s.cast()) };
    unsafe { ee_sound_sample_set_data(copy, ee_sound_sample_data(s as *mut _)) };
    let from = label(s);
    let n = new_label(copy.cast());
    t(format!("dup:S{from}>S{n}"));
    unsafe { *out = copy.cast() };
    0
}
unsafe extern "C" fn get_channel_count() -> c_ulong {
    t("chcount".into());
    w(|w| w.channels) as c_ulong
}
unsafe extern "C" fn get_cpu_usage() -> f32 {
    t("cpu".into());
    0.25
}
unsafe extern "C" fn get_direct_sound_interface() -> *mut c_void {
    t("ds".into());
    null_mut()
}
unsafe extern "C" fn get_highest_timer_delay() -> c_ulong {
    t("delay".into());
    7
}
unsafe extern "C" fn get_maximum_channel_count() -> c_ulong {
    t("maxch".into());
    w(|w| w.max_channels) as c_ulong
}
unsafe extern "C" fn get_sample_playback_time(s: *mut c_void, total: *mut c_long, remaining: *mut c_long) -> c_long {
    let l = label(s);
    let p = w(|w| w.play_time);
    unsafe {
        *total = (1000 + l * 10 + p) as c_long;
        *remaining = (l * 7 + p) as c_long;
    }
    t(format!("ptime:S{l}"));
    0
}
unsafe extern "C" fn get_sample_offset(s: *mut c_void) -> c_ulong {
    t(format!("offset:S{}", label(s)));
    3
}
unsafe extern "C" fn get_volume() -> f32 {
    t("getvol".into());
    w(|w| w.volume)
}
unsafe extern "C" fn initialize_mixer(window: *mut c_void) {
    t(format!("init{}", !window.is_null() as i32));
}
unsafe extern "C" fn is_channel_busy_playing(channel: c_ulong) -> c_long {
    t(format!("busy{channel}"));
    if (channel as u64) < 64 {
        w(|w| w.busy[channel as usize]) as c_long
    } else {
        0
    }
}
unsafe extern "C" fn load_wave_data(spec: *mut c_void, s: *mut c_void) -> c_long {
    let name = unsafe { &(*(spec as *const Spec)).0 }.clone();
    let n = new_label(s);
    t(format!("loadwave:{}>S{n}", String::from_utf8_lossy(&name)));
    if w(|w| w.load_fail) && name[0] == b'b' {
        return 9;
    }
    let len = name.len() as u32;
    let s = s as *mut EeSoundSample;
    let mut z = name.clone();
    z.push(0);
    unsafe {
        ee_sound_sample_set_filename(s, z.as_ptr() as *const c_char);
        ee_sound_sample_set_data(s, (0x1000 + n) as usize as *mut c_void);
        ee_sound_sample_set_length_in_bytes(s, len * 1000);
        ee_sound_sample_set_bits(s, 8);
        ee_sound_sample_set_length_in_samples(s, len * 500 + if name[1] == b'2' { 0 } else { 17 });
        ee_sound_sample_set_rate(s, if name[1] == b'3' { 0 } else { 22050 + len });
    }
    0
}
unsafe extern "C" fn mix_ahead() -> c_long {
    t("mixahead".into());
    4
}
unsafe extern "C" fn play_channel(channel: c_ulong) -> c_long {
    t(format!("playch{channel}"));
    fail()
}
unsafe extern "C" fn process_message(message: c_ulong, _p1: *mut c_void, _p2: *mut c_void) -> bool {
    t(format!("msg{message}"));
    true
}
unsafe extern "C" fn set_channel_sample(channel: c_ulong, s: *mut c_void) -> c_long {
    t(format!("setch{channel}:S{}", label(s)));
    let r = fail();
    if r == 0 {
        unsafe { ee_sound_sample_set_mixer_channel(s.cast(), channel as u32) };
    }
    r
}
unsafe extern "C" fn set_latency(ms: c_ulong) -> c_long {
    t(format!("latency{ms}"));
    0
}
unsafe extern "C" fn set_sample_looping(s: *mut c_void, looping: bool) {
    t(format!("loop:S{}={}", label(s), looping as i32));
    unsafe { ee_sound_sample_set_the_is_looping(s.cast(), looping) };
}
unsafe extern "C" fn set_sample_offset(s: *mut c_void, offset: c_ulong) -> c_long {
    t(format!("setoff:S{}={offset}", label(s)));
    0
}
unsafe extern "C" fn set_sample_pan(s: *mut c_void, v: f32) -> c_long {
    t(format!("pan:S{}={}", label(s), bits(v)));
    let r = fail();
    unsafe { ee_sound_sample_set_the_pan(s.cast(), v) };
    r
}
unsafe extern "C" fn set_sample_pitch(s: *mut c_void, v: f32) -> c_long {
    t(format!("pitch:S{}={}", label(s), bits(v)));
    let r = fail();
    unsafe { ee_sound_sample_set_the_pitch(s.cast(), v) };
    r
}
unsafe extern "C" fn set_sample_volume(s: *mut c_void, v: f32) -> c_long {
    t(format!("vol:S{}={}", label(s), bits(v)));
    let r = fail();
    unsafe { ee_sound_sample_set_the_volume(s.cast(), v) };
    r
}
unsafe extern "C" fn set_volume(v: f32) -> c_long {
    t(format!("mastervol={}", bits(v)));
    w(|w| w.volume = v);
    0
}
unsafe extern "C" fn stop_all_channels() -> c_long {
    t("stopall".into());
    0
}
unsafe extern "C" fn stop_channel(channel: c_ulong) -> c_long {
    t(format!("stopch{}", channel as u32));
    0
}
unsafe extern "C" fn get_output_rate() -> c_ulong {
    t("rate".into());
    w(|w| w.output_rate) as c_ulong
}
unsafe extern "C" fn set_music_source(source: EeSoundSource, context: *mut c_void) {
    t(format!("musicsrc{}", source.is_some() as i32));
    w(|w| w.music_source = source.map(|f| (f as usize, context as usize)));
}
unsafe extern "C" fn set_movie_source(source: EeSoundSource, _context: *mut c_void) {
    t(format!("moviesrc{}", source.is_some() as i32));
}

fn entry(name: &[u8]) -> Option<*mut c_void> {
    let name = &name[..name.len() - usize::from(name.last() == Some(&0))];
    let p: *mut c_void = match name {
        b"SPIMActivateMixer" => activate_mixer as *mut c_void,
        b"SPIMCreateChannels" => create_channels as *mut c_void,
        b"SPIMDeactivateMixer" => deactivate_mixer as *mut c_void,
        b"SPIMDeleteWaveData" => delete_wave_data as *mut c_void,
        b"SPIMDestroyChannels" => destroy_channels as *mut c_void,
        b"SPIMDuplicateSample" => duplicate_sample as *mut c_void,
        b"SPIMGetChannelCount" => get_channel_count as *mut c_void,
        b"SPIMGetCPUUsage" => get_cpu_usage as *mut c_void,
        b"SPIMGetDirectSoundInterface" => get_direct_sound_interface as *mut c_void,
        b"SPIMGetHighestTimerDelay" => get_highest_timer_delay as *mut c_void,
        b"SPIMGetMaximumChannelCount" => get_maximum_channel_count as *mut c_void,
        b"SPIMGetSamplePlayBackTime" => get_sample_playback_time as *mut c_void,
        b"SPIMGetSampleOffset" => get_sample_offset as *mut c_void,
        b"SPIMGetVolume" => get_volume as *mut c_void,
        b"SPIMInitializeMixer" => initialize_mixer as *mut c_void,
        b"SPIMIsChannelBusyPlaying" => is_channel_busy_playing as *mut c_void,
        b"SPIMLoadWaveData" => load_wave_data as *mut c_void,
        b"SPIMMixAhead" => mix_ahead as *mut c_void,
        b"SPIMPlayChannel" => play_channel as *mut c_void,
        b"SPIMProcessMessage" => process_message as *mut c_void,
        b"SPIMSetChannelSample" => set_channel_sample as *mut c_void,
        b"SPIMSetLatency" => set_latency as *mut c_void,
        b"SPIMSetSampleLooping" => set_sample_looping as *mut c_void,
        b"SPIMSetSampleOffset" => set_sample_offset as *mut c_void,
        b"SPIMSetSamplePan" => set_sample_pan as *mut c_void,
        b"SPIMSetSamplePitch" => set_sample_pitch as *mut c_void,
        b"SPIMSetSampleVolume" => set_sample_volume as *mut c_void,
        b"SPIMSetVolume" => set_volume as *mut c_void,
        b"SPIMStopAllChannels" => stop_all_channels as *mut c_void,
        b"SPIMStopChannel" => stop_channel as *mut c_void,
        b"SPIMGetOutputRate" => get_output_rate as *mut c_void,
        b"SPIMSetMusicSource" => set_music_source as *mut c_void,
        b"SPIMSetMovieSource" => set_movie_source as *mut c_void,
        _ => return None,
    };
    Some(p)
}

// ---- dmusic
struct Loader {
    id: i32,
    resolve: Option<DmResolver>,
    context: *mut c_void,
}
struct Segment {
    id: i32,
    repeats: u32,
}
fn next_dm() -> i32 {
    w(|w| {
        w.next_dm += 1;
        w.next_dm
    })
}
fn mark(fail: bool) -> &'static str {
    if fail {
        "!"
    } else {
        ""
    }
}
#[no_mangle]
unsafe extern "C" fn DmLoader_create(out: *mut *mut c_void, options: c_int) -> c_int {
    let f = w(|w| w.fail_loader);
    t(format!("dmloader{options}{}", mark(f)));
    if f {
        return 1;
    }
    let id = next_dm();
    unsafe { *out = Box::into_raw(Box::new(Loader { id, resolve: None, context: null_mut() })).cast() };
    0
}
#[no_mangle]
unsafe extern "C" fn DmLoader_release(l: *mut c_void) {
    let id = if l.is_null() { 0 } else { unsafe { Box::from_raw(l as *mut Loader) }.id };
    t(format!("dmloader-{id}"));
}
#[no_mangle]
unsafe extern "C" fn DmLoader_addResolver(l: *mut c_void, resolve: DmResolver, context: *mut c_void) -> c_int {
    t("resolver".into());
    let l = unsafe { &mut *(l as *mut Loader) };
    l.resolve = Some(resolve);
    l.context = context;
    0
}
extern "C" {
    fn free(p: *mut c_void);
}
#[no_mangle]
unsafe extern "C" fn DmLoader_getSegment(l: *mut c_void, name: *const c_char, out: *mut *mut c_void) -> c_int {
    let l = unsafe { &*(l as *const Loader) };
    let mut len: usize = 99;
    let data = unsafe { l.resolve.unwrap()(l.context, name, &mut len) };
    let mut text = if data.is_null() {
        b"-".to_vec()
    } else {
        unsafe { std::slice::from_raw_parts(data as *const u8, len) }.to_vec()
    };
    for b in &mut text {
        if *b == b' ' || *b == b'\r' || *b == b'\n' {
            *b = b'_';
        }
    }
    let n = unsafe { CStr::from_ptr(name) }.to_str().unwrap();
    t(format!("getseg:{n}={len}:{}", String::from_utf8_lossy(&text)));
    unsafe { free(data) };
    if data.is_null() || text[0] == b'F' {
        return 2;
    }
    let id = next_dm();
    unsafe { *out = Box::into_raw(Box::new(Segment { id, repeats: 0 })).cast() };
    0
}
#[no_mangle]
unsafe extern "C" fn DmSegment_release(s: *mut c_void) {
    let s = unsafe { Box::from_raw(s as *mut Segment) };
    t(format!("seg-{}", s.id));
}
#[no_mangle]
unsafe extern "C" fn DmSegment_getRepeats(s: *const c_void) -> u32 {
    let s = unsafe { &*(s as *const Segment) };
    t(format!("getrep{}", s.id));
    s.repeats
}
#[no_mangle]
unsafe extern "C" fn DmSegment_setRepeats(s: *mut c_void, r: u32) {
    let s = unsafe { &mut *(s as *mut Segment) };
    t(format!("setrep{}={r}", s.id));
    s.repeats = r;
}
#[no_mangle]
unsafe extern "C" fn DmPerformance_create(out: *mut *mut c_void, rate: u32) -> c_int {
    let f = w(|w| w.fail_performance);
    t(format!("dmperf{rate}{}", mark(f)));
    if f {
        return 1;
    }
    let id = next_dm();
    unsafe { *out = Box::into_raw(Box::new(id)).cast() };
    0
}
#[no_mangle]
unsafe extern "C" fn DmPerformance_release(p: *mut c_void) {
    let id = if p.is_null() { 0 } else { *unsafe { Box::from_raw(p as *mut i32) } };
    t(format!("dmperf-{id}"));
}
fn seg_id(s: *mut c_void) -> i32 {
    if s.is_null() {
        0
    } else {
        unsafe { (*(s as *const Segment)).id }
    }
}
#[no_mangle]
unsafe extern "C" fn DmPerformance_playSegment(_p: *mut c_void, s: *mut c_void, timing: c_int) -> c_int {
    let f = w(|w| w.fail_play);
    t(format!("playseg{}/{timing}{}", seg_id(s), mark(f)));
    if f {
        3
    } else {
        0
    }
}
#[no_mangle]
unsafe extern "C" fn DmPerformance_playTransition(_p: *mut c_void, s: *mut c_void, e: c_int, timing: c_int) -> c_int {
    let f = w(|w| w.fail_play);
    t(format!("trans{}/{e}/{timing}{}", seg_id(s), mark(f)));
    if f {
        3
    } else {
        0
    }
}
#[no_mangle]
extern "C" fn DmPerformance_setVolume(_p: *mut c_void, v: f32) {
    t(format!("dmvol={}", bits(v)));
}
#[no_mangle]
extern "C" fn DmPerformance_setGrooveLevel(_p: *mut c_void, level: u8) {
    t(format!("groove={level}"));
}
#[no_mangle]
extern "C" fn DmPerformance_stop(_p: *mut c_void, lookahead_ms: u32) {
    t(format!("dmstop{lookahead_ms}"));
}
#[no_mangle]
extern "C" fn DmPerformance_setReverb(_p: *mut c_void, enabled: c_int, gain: f32, mix: f32, time: f32, hf: f32) {
    t(format!("reverb{enabled}/{}/{}/{}/{}", bits(gain), bits(mix), bits(time), bits(hf)));
}
/// The built-in general MIDI set (SEGeneralMIDI.c): four bytes here.
#[no_mangle]
extern "C" fn SEGeneralMIDISet(length: *mut usize) -> *const u8 {
    unsafe { *length = 4 };
    b"RIFF".as_ptr()
}
#[no_mangle]
unsafe extern "C" fn DmPerformance_renderPcm(
    _p: *mut c_void,
    buffer: *mut c_void,
    count: usize,
    options: c_int,
) -> c_int {
    t(format!("render{count}/{options}"));
    for i in 0..count {
        unsafe { *(buffer as *mut f32).add(i) = 0.5 };
    }
    0
}

// ---- the replay
struct Replay {
    music: String,
    manager: *mut EeSoundManager,
    nodes: [*mut EeSoundNode; 8],
    seed: u64,
}

fn mixer_at(i: i32) -> *mut EeSoundMixer {
    if i < 0 {
        return null_mut();
    }
    let mut m = null_mut();
    unsafe { ee_sound_mixer_get(i as u32, &mut m) };
    m
}

fn h(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

/// The number after `key` in the expected line (a value the driver drew and printed only in a fake call).
fn drawn(expected: &str, key: &str) -> u32 {
    let i = expected.find(key).unwrap_or_else(|| panic!("{key} not in {expected}")) + key.len();
    expected[i..].split(|c: char| !c.is_ascii_digit()).next().unwrap().parse().unwrap()
}

fn dump_samples() {
    let order: Vec<(i32, usize)> = w(|w| w.samples.iter().map(|(&p, &l)| (l, p)).collect());
    let mut order = order;
    order.sort();
    for (l, p) in order {
        let s = p as *mut EeSoundSample;
        unsafe {
            let file = CStr::from_ptr(ee_sound_sample_filename(s)).to_str().unwrap();
            t(format!(
                "S{l}[{},{:x},{},{},{},{},{},{},{},{},{}]",
                if file.is_empty() { "-" } else { file },
                ee_sound_sample_data(s) as usize,
                bits(ee_sound_sample_volume(s)),
                bits(ee_sound_sample_pan(s)),
                bits(ee_sound_sample_pitch(s)),
                ee_sound_sample_mixer_channel(s) as i32,
                ee_sound_sample_length_in_bytes(s),
                ee_sound_sample_length_in_samples(s),
                ee_sound_sample_bits(s),
                ee_sound_sample_rate(s),
                ee_sound_sample_is_looping(s) as i32
            ));
        }
    }
}

fn path(k: i32) -> *mut c_void {
    if k == 2 {
        null_mut()
    } else {
        (k as usize + 1) as *mut c_void
    }
}

fn b(v: bool) -> i32 {
    v as i32
}

impl Replay {
    fn set_node(&mut self, k: usize, n: *mut EeSoundNode) {
        if !self.nodes[k].is_null() {
            unsafe { ee_sound_node_dereference(self.nodes[k]) };
        }
        self.nodes[k] = n;
    }

    /// Runs one line's call (its head is `head`) and returns the result text; `expected` is the driver's line.
    fn run(&mut self, head: &str, expected: &str) -> String {
        let a: Vec<&str> = head.split(' ').collect();
        let i = |k: usize| -> i64 { a[k].parse().unwrap() };
        let u = |k: usize| -> u32 { a[k].parse::<i64>().unwrap() as u32 };
        let m = self.manager;
        // conductor ops: "m<idx> c..."
        if a[0].starts_with('m') && a[0][1..].parse::<i32>().is_ok() && a.len() > 1 && a[1].starts_with('c') {
            let c = unsafe { ee_sound_mixer_conductor(mixer_at(a[0][1..].parse().unwrap())) };
            let a = &a[1..];
            let i = |k: usize| -> i64 { a[k].parse().unwrap() };
            return unsafe { self.conductor(c, a, i, expected) };
        }
        unsafe {
            match a[0] {
                "seq" => unreachable!(),
                "init" => {
                    w(|w| {
                        w.channels = a[1].parse().unwrap();
                        w.module_count = a[2].parse().unwrap();
                        for (k, ch) in a[3].bytes().enumerate() {
                            w.module_valid[k] = ch == b'1';
                        }
                    });
                    let seed = self.seed;
                    if seed.is_multiple_of(5) {
                        t("shutdown-registered".into());
                    }
                    let mut start = EeSoundMixerStart {
                        directory: null_mut(),
                        window: if seed.is_multiple_of(5) || seed % 2 == 1 { 8 as *mut c_void } else { null_mut() },
                        module: null_mut(),
                    };
                    let r = ee_sound_mixers_initialize(&mut start);
                    format!("{r} {}", ee_sound_mixer_count())
                }
                "use0" => format!("{}", ee_sound_mixer_use(mixer_at(0))),
                "cfolder0" => {
                    let z = format!("{}\0", self.music);
                    format!(
                        "{}",
                        ee_sound_conductor_set_music_folder(ee_sound_mixer_conductor(mixer_at(0)), z.as_ptr().cast())
                    )
                }
                "use" => {
                    let mx = mixer_at(i(1) as i32);
                    let r = ee_sound_mixer_use(mx);
                    format!("{r} {} {}", b(ee_sound_mixer_active() == mx), b(!ee_sound_mixer_active().is_null()))
                }
                "getmixer" => {
                    let mut out = 1 as *mut EeSoundMixer;
                    let r = ee_sound_mixer_get(u(1), &mut out);
                    let v = if out as usize == 1 { 2 } else { b(!out.is_null()) };
                    format!("{r} {v} {}", ee_sound_mixer_count())
                }
                "mixer" => {
                    let mx = mixer_at(i(1) as i32);
                    let cpu = ee_sound_mixer_cpu_usage(mx);
                    let ds = ee_sound_mixer_direct_sound(mx);
                    t("getname".into());
                    let v = is_valid();
                    let p = ee_sound_mixer_process_message(mx, 5, null_mut(), null_mut());
                    let rate = ee_sound_mixer_output_rate(mx);
                    format!("{} {} FakeMixer {} {} {rate}", bits(cpu), b(!ds.is_null()), b(v), b(p))
                }
                "moviesrc" => {
                    ee_sound_mixer_set_movie_source(mixer_at(i(1) as i32), None, null_mut());
                    "-".into()
                }
                "newmgr" => {
                    let mx = ee_sound_mixer_active();
                    self.manager = ee_sound_manager_new(path(i(3) as i32), mx, u(1), u(2));
                    let (p1, p0) = w(|w| (w.path_refs[1], w.path_refs[0]));
                    let av = ee_sound_manager_available_channels(self.manager);
                    format!("{av} {p0} {p1}")
                }
                "delmgr" => {
                    ee_sound_manager_free(m);
                    self.manager = null_mut();
                    "-".into()
                }
                "pool" => "-".into(),
                "nodes" => {
                    for k in 0..8 {
                        self.set_node(k, null_mut());
                    }
                    "-".into()
                }
                "shutdown" => {
                    ee_sound_mixers_delete_all();
                    format!("{} {}", ee_sound_mixer_count(), b(!ee_sound_mixer_active().is_null()))
                }
                "cleanup" => {
                    ee_sound_pool_cleanup();
                    ee_sound_pool_cleanup();
                    "-".into()
                }
                "samples" => {
                    dump_samples();
                    format!("{}", w(|w| w.samples.len()))
                }
                "load" => {
                    let z = format!("{}\0", a[1]);
                    format!("{}", ee_sound_manager_load(m, z.as_ptr().cast(), u(2), u(3)) as i32)
                }
                "play" => format!("{}", ee_sound_manager_play(m, u(1), h(a[2]), h(a[3]), h(a[4]), a[5] == "1") as i32),
                "playn" => {
                    let k = i(1) as usize;
                    let n = ee_sound_manager_play_node(m, u(2), h(a[3]), 0.0, 1.0, a[4] == "1");
                    if !n.is_null() {
                        self.set_node(k, n);
                        ee_sound_manager_node_instance_id(m, n);
                    }
                    let id = if n.is_null() { -9 } else { ee_sound_manager_node_instance_id(m, n) as i32 };
                    format!("{} {id}", b(!n.is_null()))
                }
                "deref" => {
                    self.set_node(i(1) as usize, null_mut());
                    "-".into()
                }
                "refcopy" => {
                    let (k, j) = (i(1) as usize, i(2) as usize);
                    if !self.nodes[k].is_null() && k != j {
                        ee_sound_node_reference(self.nodes[k]);
                        let n = self.nodes[k];
                        self.set_node(j, n);
                    }
                    "-".into()
                }
                "getref" => {
                    let n = ee_sound_manager_get_and_reference(m, u(1));
                    if !n.is_null() {
                        self.set_node(i(2) as usize, n);
                    }
                    format!("{}", b(!n.is_null()))
                }
                "stop" => format!("{}", ee_sound_manager_stop(m, u(1), a[2] == "1")),
                "stopn" => format!("{}", ee_sound_manager_stop_node(m, self.nodes[i(1) as usize], a[2] == "1")),
                "get" => {
                    let id = u(1);
                    let left = ee_sound_manager_time_left(m, id);
                    let rate = ee_sound_manager_sample_rate(m, id);
                    let sid = ee_sound_manager_sample_id(m, id);
                    let vol = ee_sound_manager_volume(m, id);
                    let pitch = ee_sound_manager_pitch(m, id);
                    let pan = ee_sound_manager_pan(m, id);
                    let playing = ee_sound_manager_is_playing(m, id);
                    let looping = ee_sound_manager_is_looping(m, id);
                    format!(
                        "{} {} {} {} {} {} {rate} {left}",
                        b(looping),
                        b(playing),
                        bits(pan),
                        bits(pitch),
                        bits(vol),
                        sid as i32
                    )
                }
                "getn" => {
                    let n = self.nodes[i(1) as usize];
                    let sid = ee_sound_manager_node_sample_id(m, n);
                    let pitch = ee_sound_manager_node_pitch(m, n);
                    let pan = ee_sound_manager_node_pan(m, n);
                    let playing = ee_sound_manager_node_is_playing(m, n);
                    let looping = ee_sound_manager_node_is_looping(m, n);
                    let id = ee_sound_manager_node_instance_id(m, n);
                    format!("{} {} {} {} {} {}", id as i32, b(looping), b(playing), bits(pan), bits(pitch), sid as i32)
                }
                "total" => format!("{}", ee_sound_manager_total_play_time(m, u(1))),
                op if op.starts_with("setn") => {
                    let which: u32 = op[4..].parse().unwrap();
                    let n = self.nodes[i(1) as usize];
                    let r = if which == 3 {
                        ee_sound_manager_node_set_parameters(m, n, h(a[2]), h(a[3]), h(a[4]))
                    } else {
                        ee_sound_manager_node_set(m, n, [0, 1, 2][which as usize], h(a[2]))
                    };
                    format!("{r}")
                }
                op if op.starts_with("set") && op.len() == 4 => {
                    let id = u(1);
                    let v = h(a[2]);
                    let r = match &op[3..] {
                        "0" => ee_sound_manager_set_volume(m, id, v),
                        "1" => ee_sound_manager_set_pan(m, id, v),
                        "2" => ee_sound_manager_set_pitch(m, id, v),
                        _ => ee_sound_manager_set_parameters(m, id, v, h(a[3]), h(a[4])),
                    };
                    format!("{r}")
                }
                "oneshot" => format!("{}", ee_sound_manager_register_one_shot(m, u(1), u(2))),
                "stopos" => format!("{}", ee_sound_manager_stop_all_one_shots(m)),
                "unload" => format!("{}", ee_sound_manager_unload(m, u(1))),
                "maxch" => {
                    let av = ee_sound_manager_available_channels(m);
                    let r = ee_sound_manager_set_max_channels(m, u(1));
                    format!("{r} {av}")
                }
                "pct" => format!("{}", ee_sound_manager_set_category_percentage(m, u(1), h(a[2]))),
                "prio" => format!("{}", ee_sound_manager_set_category_priority(m, u(1), u(2))),
                "setpath" => {
                    let k = i(1) as i32;
                    ee_sound_manager_set_path(m, path(k));
                    let same = ee_sound_manager_path(m) == path(k);
                    let (p0, p1) = w(|w| (w.path_refs[0], w.path_refs[1]));
                    format!("{p0} {p1} {}", b(same))
                }
                "master" => {
                    let r = ee_sound_manager_set_master_volume(m, h(a[1]));
                    format!("{r} {}", bits(ee_sound_manager_master_volume(m)))
                }
                "misc" => {
                    let lock = !ee_sound_manager_lock(m).is_null();
                    let av = ee_sound_manager_available_channels(m);
                    let delay = ee_sound_manager_mixer_value(m, 0);
                    let max = ee_sound_manager_mixer_value(m, 1);
                    let ahead = ee_sound_manager_mix_ahead(m);
                    let r = ee_sound_manager_set_latency(m, drawn(expected, " latency"));
                    format!("{r} {ahead} {max} {delay} {av} {}", b(lock))
                }
                "rate" => format!("{}", ee_sound_manager_sample_rate(m, u(1))),
                "scav" => {
                    ee_sound_manager_set_muted(m, a[1] == "1");
                    let script: Vec<i32> = a[2]
                        .bytes()
                        .map(|c| match c {
                            b'0' => 0,
                            b't' => -1,
                            _ => -2,
                        })
                        .collect();
                    w(|w| w.wait_script = script);
                    let (f, p) = w(|w| w.last_thread.unwrap());
                    let f: unsafe extern "C" fn(*mut c_void) -> c_ulong = std::mem::transmute(f);
                    let r = f(p as *mut c_void);
                    format!("{r} {}", b(ee_sound_manager_muted(m)))
                }
                "busy" => {
                    w(|w| {
                        for (k, ch) in a[1].bytes().enumerate() {
                            w.busy[k] = (ch - b'0') as i32;
                        }
                    });
                    "-".into()
                }
                "ptime" => {
                    w(|w| w.play_time = i(1) as i32);
                    "-".into()
                }
                "failnext" => {
                    w(|w| w.fail_next = i(1) as i32);
                    "-".into()
                }
                "dump" => {
                    dump_samples();
                    let av = ee_sound_manager_available_channels(m);
                    format!("{av} {}", w(|w| w.finds))
                }
                "loadfail" => {
                    w(|w| w.load_fail = a[1] == "1");
                    "-".into()
                }
                "vol" => bits(ee_sound_manager_volume(m, u(1))),
                "idn" => {
                    let n = self.nodes[i(1) as usize];
                    let sid = ee_sound_manager_node_sample_id(m, n);
                    let id = ee_sound_manager_node_instance_id(m, n);
                    format!("{} {}", id as i32, sid as i32)
                }
                "maxmix" => {
                    w(|w| w.max_channels = u(1));
                    "-".into()
                }
                "left" => format!("{}", ee_sound_manager_time_left(m, u(1))),
                op => panic!("unknown op {op}"),
            }
        }
    }

    unsafe fn conductor(
        &mut self,
        c: *mut EeSoundConductor,
        a: &[&str],
        i: impl Fn(usize) -> i64,
        expected: &str,
    ) -> String {
        unsafe {
            match a[0] {
                "cload" => {
                    let z = format!("{}\0", a[1]);
                    let mut id = -7;
                    let r = ee_sound_conductor_load_score(c, z.as_ptr().cast(), &mut id);
                    format!("{r} {id}")
                }
                "cplay" => format!("{}", ee_sound_conductor_play(c, i(1) as i32)),
                "cqueue" => format!("{}", ee_sound_conductor_queue(c, i(1) as i32)),
                "cstop" => format!("{}", ee_sound_conductor_stop(c)),
                "ctrans" => format!("{}", ee_sound_conductor_transition_to(c, i(1) as i32, i(2) as u32)),
                "cunload" => format!("{}", ee_sound_conductor_unload_score(c, i(1) as i32)),
                "cstate" => format!("{}", ee_sound_conductor_set_state(c, i(1) as u32)),
                "cint" => format!("{}", ee_sound_conductor_set_intensity(c, i(1) as u32, a[2] == "1")),
                "cvol" => format!("{}", ee_sound_conductor_set_volume(c, h(a[1]))),
                "cfolder" => {
                    let k = i(1);
                    let folder = match k {
                        0 => String::new(),
                        1 => "x\\".into(),
                        _ => self.music.clone(),
                    };
                    let z = format!("{folder}\0");
                    let r = ee_sound_conductor_set_music_folder(c, z.as_ptr().cast());
                    let now = CStr::from_ptr(ee_sound_conductor_music_folder(c)).to_str().unwrap().to_string();
                    let shown = if k == 2 {
                        "@".to_string()
                    } else if now.is_empty() {
                        "-".into()
                    } else {
                        now
                    };
                    format!("{r} {shown}")
                }
                "creverb" => {
                    let state = ee_sound_conductor_fields(c).reverb_state;
                    let r = ee_sound_conductor_set_reverb(c, i(1) as u32);
                    format!("{r} {state}")
                }
                "cstyle" => "0".into(),
                "cgroups" => {
                    let r = ee_sound_conductor_set_channel_group_count(c, i(1) as u32);
                    format!("{r} {}", ee_sound_conductor_fields(c).channel_group_count)
                }
                "cscore" => {
                    let s = ee_sound_conductor_score(c, i(1) as i32);
                    if s.is_null() {
                        return "null".into();
                    }
                    let id = ee_sound_conductor_score_id(c, s);
                    let rep = ee_sound_score_repeat_count(s);
                    let n = {
                        let k = expected.find("setrep").unwrap();
                        let e = expected[k..].find('=').unwrap() + k + 1;
                        expected[e..].split(' ').next().unwrap().parse::<u32>().unwrap()
                    };
                    ee_sound_score_set_repeat_count(s, n);
                    let now = ee_sound_score_repeat_count(s);
                    let secondary = ee_sound_score_is_secondary(s);
                    format!("{id} {} {rep} {now} 0", b(secondary))
                }
                "cscoreid0" => format!("{}", ee_sound_conductor_score_id(c, null_mut())),
                "cplaying" => format!("{}", ee_sound_conductor_playing(c)),
                "cget" => {
                    let f = ee_sound_conductor_fields(c);
                    format!(
                        "{} {} {} {} {}",
                        f.state,
                        f.intensity,
                        bits(f.volume),
                        f.reverb_state,
                        f.channel_group_count
                    )
                }
                "crender" => match w(|w| w.music_source) {
                    Some((f, ctx)) => {
                        let f: unsafe extern "C" fn(*mut c_void, *mut f32, c_ulong) = std::mem::transmute(f);
                        let mut frames = [0f32; 16];
                        f(ctx as *mut c_void, frames.as_mut_ptr(), 4);
                        bits(frames[0])
                    }
                    None => "none".into(),
                },
                "cfail" => {
                    w(|w| {
                        w.fail_play = a[1] == "1";
                        w.fail_performance = a[2] == "1";
                        w.fail_loader = a[3] == "1";
                    });
                    "-".into()
                }
                "crate" => {
                    w(|w| w.output_rate = i(1) as u32);
                    "-".into()
                }
                op => panic!("unknown conductor op {op}"),
            }
        }
    }
}

/// Replays the golden text; panics at the first line that differs. Returns the number of lines.
fn replay(text: &str, music: &str) -> usize {
    let mut r = Replay { music: music.to_string(), manager: null_mut(), nodes: [null_mut(); 8], seed: 0 };
    let mut n = 0;
    for line in text.lines() {
        n += 1;
        if let Some(seed) = line.strip_prefix("seq ") {
            r.seed = seed.parse().unwrap();
            w(|w| {
                w.fail_play = false;
                w.fail_performance = false;
                w.fail_loader = false;
                w.fail_next = 0;
                w.output_rate = 22050;
                w.load_fail = true;
                w.play_time = 0;
                w.busy = vec![0; 64];
            });
            continue;
        }
        let k = line.find(" :").unwrap_or_else(|| panic!("line {n}: {line}"));
        let head = &line[..k];
        w(|w| w.line = format!("{head} :"));
        let result = r.run(head, line);
        let mut got = w(|w| std::mem::take(&mut w.line));
        let _ = write!(got, " = {result}");
        assert_eq!(got, line, "golden line {n} differs");
    }
    n
}

fn music_folder() -> String {
    let d = std::env::temp_dir().join(format!("ee_sound_objects_music_{}", std::process::id()));
    std::fs::create_dir_all(&d).unwrap();
    for (f, c) in [("a.sgt", &b"AAA"[..]), ("b.sgt", b"BB"), ("c.sgt", b"C C\r\n"), ("f.sgt", b"Fail")] {
        std::fs::write(d.join(f), c).unwrap();
    }
    let mut s = d.to_str().unwrap().to_string();
    s.push(if cfg!(windows) { '\\' } else { '/' });
    s
}

#[test]
fn golden_objects_replay() {
    let music = music_folder();
    let n = replay(include_str!("golden_objects.txt"), &music);
    println!("golden_objects: {n} lines replayed");
    if let Ok(file) = std::env::var("EE_SOUND_OBJECTS_GOLDEN") {
        // a long run continues the same process state only if it starts fresh: run it alone
        let text = std::fs::read_to_string(file).unwrap();
        let n = replay(&text[include_str!("golden_objects.txt").len()..], &music);
        println!("golden_objects: {n} more lines replayed");
    }
    let _ = std::fs::remove_dir_all(music.trim_end_matches(['\\', '/']));
}
