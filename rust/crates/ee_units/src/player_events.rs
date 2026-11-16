//! A player's events (`EEPPlayerEventList` and `EEPPlayerEvent`, EEPPlayerEventList.cpp 0x56e5b9..0x56eea6,
//! EEPPlayerEvent.cpp 0x56e1a2..0x56e5b9): the alerts at a map location (1 under attack, 2 a garrison's message, 3 a
//! flare) with their time left, as one Rust object the C++ list holds (`mList`). Every decision is here: the event
//! made or folded into a recent one of its type near it, the newest first, the local player's sound, mini-map ping
//! and chat text, the ping's and the events' timers, the attack event's sight shared at its location while it is
//! active (on, then off five seconds after it started and when it ends), the trim of expired events past 15, the
//! next event to jump to, the saved game's Load and Save. The C++ host does the world's parts: the sight and the
//! local view at the location, whether the player is the local one, the alert sounds, the ping, the chat text.
//!
//! The current event (`mCurrent`, a pointer in the original) is an event's serial number here; the original could
//! compare it with a deleted event's address, which a serial does the same way.
//!
//! Kept: the duration (5 s for type 2, else 15 s), the fold distance (squared 110), the new event made and announced
//! before the old one is looked at, an old event replaced only once it has expired (else the new one stays
//! silently), the event saved counts, an event's failed load pushed anyway and the loop going on. Defined where the
//! original was not: a count or coordinates read from a short file start at 0 (the original's were uninitialised).
//!
//! Determinism: the original's `float` operations in order, no random draw. The host never calls back into the list.

#![allow(clippy::unnecessary_cast)]

use std::collections::VecDeque;
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::player_save::{
    EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_FLOAT,
    EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
    EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, EE_UNITS_PLAYER_SAVE_WRITE_LONG, EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};

/// One event (`EEPPlayerEvent`'s members).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsPlayerEvent {
    /// `mSource` (the tile the event is about: the camera jumps there)
    pub source_x: c_long,
    pub source_y: c_long,
    /// `mLocation` (an attack: the attacker's tile, where the sight is shared)
    pub x: c_long,
    pub y: c_long,
    /// `mType`
    pub kind: c_long,
    /// `mTimeRemaining` (ms)
    pub time: f32,
    /// `mTextID` (-1 none)
    pub text: c_long,
    /// `mActive`
    pub active: bool,
}

impl Default for EeUnitsPlayerEvent {
    /// `Reset`: no type, no time, both locations off the map, inactive, no text.
    fn default() -> Self {
        EeUnitsPlayerEvent {
            source_x: -10000,
            source_y: -10000,
            x: -10000,
            y: -10000,
            kind: 0,
            time: 0.0,
            text: -1,
            active: false,
        }
    }
}

// ---- the host's operations (the event passed with each)
/// the sight at the event's location on for the player (`AddSharedSightAt`), and the local player's mini-map cell and
/// terrain tile when it sees the player's sight
pub const EE_UNITS_EVENTS_SHOW: i32 = 1;
/// the same off (`RemoveSharedSightAt`, `CountCellOff`, `DereferenceTile`)
pub const EE_UNITS_EVENTS_HIDE: i32 = 2;
/// 1 when the player is the local player
pub const EE_UNITS_EVENTS_IS_LOCAL: i32 = 3;
/// 1 while the last alert sound plays
pub const EE_UNITS_EVENTS_PLAYING: i32 = 4;
/// the alert sound of the event's type played (and kept as the one playing)
pub const EE_UNITS_EVENTS_SOUND: i32 = 5;
/// the mini map pings the event's source
pub const EE_UNITS_EVENTS_PING: i32 = 6;
/// the event's text into the chat log
pub const EE_UNITS_EVENTS_TEXT: i32 = 7;
/// the local mini map's ping off
pub const EE_UNITS_EVENTS_STOP_PING: i32 = 8;

/// The list's C++ side: one call per operation.
pub trait EventHost {
    /// Whether the call has a player (the destructor's clear has none: the events are dropped without the world).
    fn has_player(&self) -> bool;
    fn op(&mut self, op: i32, event: &EeUnitsPlayerEvent) -> i32;
}

/// The duration of an event's type (`GetDuration`).
pub fn duration(kind: c_long) -> f32 {
    if kind == 2 {
        5000.0
    } else {
        15000.0
    }
}

/// The events (`EEPPlayerEventList`'s state).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeUnitsPlayerEvents {
    /// the events, the newest first, each with its serial number
    pub events: VecDeque<(u64, EeUnitsPlayerEvent)>,
    /// `mPingTimeLeft`
    pub ping: f32,
    /// `mCurrent`'s serial number
    pub current: Option<u64>,
    /// `mNextIndex`
    pub next_index: c_ulong,
    serial: u64,
}

/// `Activate`: an attack event shares its location's sight.
fn activate<H: EventHost + ?Sized>(h: &mut H, e: &mut EeUnitsPlayerEvent) {
    if e.kind != 1 {
        return;
    }
    h.op(EE_UNITS_EVENTS_SHOW, e);
    e.active = true;
}

/// `Deactivate`: an active attack event stops sharing it (without a player, the flag only).
fn deactivate<H: EventHost + ?Sized>(h: &mut H, e: &mut EeUnitsPlayerEvent) {
    if h.has_player() {
        if e.kind != 1 || !e.active {
            return;
        }
        h.op(EE_UNITS_EVENTS_HIDE, e);
    }
    e.active = false;
}

/// `Reactivate`.
fn reactivate<H: EventHost + ?Sized>(h: &mut H, e: &mut EeUnitsPlayerEvent) {
    e.active = false;
    activate(h, e);
}

/// `Update`: the time counted down; an attack event's sight ends five seconds after it started and when it expires.
fn update<H: EventHost + ?Sized>(h: &mut H, e: &mut EeUnitsPlayerEvent, elapsed: f32) {
    if e.kind == 1 {
        let warning = duration(1) - 5000.0;
        if warning < e.time && e.time - elapsed < warning {
            deactivate(h, e);
        }
    }
    if e.time >= 0.0 {
        e.time -= elapsed;
    }
    if e.kind == 1 && e.time <= 0.0 && e.active {
        deactivate(h, e);
    }
}

impl EeUnitsPlayerEvents {
    /// `MakeEvent`: an event at `source` about `location`, reactivated, at the front; its serial number.
    fn make<H: EventHost + ?Sized>(
        &mut self,
        h: &mut H,
        source: (c_long, c_long),
        location: (c_long, c_long),
        kind: c_long,
        text: c_long,
    ) -> u64 {
        let mut e = EeUnitsPlayerEvent {
            source_x: source.0,
            source_y: source.1,
            x: location.0,
            y: location.1,
            kind,
            time: duration(kind),
            text,
            active: false,
        };
        reactivate(h, &mut e);
        self.serial += 1;
        self.events.push_front((self.serial, e));
        self.serial
    }

    /// `RemoveEvent`: the event at `at` dropped (deactivated first when asked and active).
    fn remove<H: EventHost + ?Sized>(&mut self, h: &mut H, at: usize, deactivate_it: bool) {
        if let Some((_, mut e)) = self.events.remove(at) {
            if deactivate_it && e.active {
                deactivate(h, &mut e);
            }
        }
    }

    /// `StopPing`: the local player's ping off.
    fn stop_ping<H: EventHost + ?Sized>(h: &mut H) {
        if h.op(EE_UNITS_EVENTS_IS_LOCAL, &EeUnitsPlayerEvent::default()) != 0 {
            h.op(EE_UNITS_EVENTS_STOP_PING, &EeUnitsPlayerEvent::default());
        }
    }

    /// `Announce`: for the local player, the event's alert unless one plays, then the ping (attack, flare) or the
    /// text (garrison).
    fn announce<H: EventHost + ?Sized>(&mut self, h: &mut H, e: &EeUnitsPlayerEvent) {
        if h.op(EE_UNITS_EVENTS_IS_LOCAL, e) == 0 {
            return;
        }
        let playing = h.op(EE_UNITS_EVENTS_PLAYING, e) != 0;
        if !(1..=3).contains(&e.kind) {
            return;
        }
        if !playing {
            h.op(EE_UNITS_EVENTS_SOUND, e);
        }
        if e.kind == 2 {
            h.op(EE_UNITS_EVENTS_TEXT, e);
        } else {
            h.op(EE_UNITS_EVENTS_PING, e);
            self.ping = if e.kind == 3 { 20000.0 } else { 10000.0 };
        }
    }

    /// `AddPlayerEvent`: an event of `kind` at `tile` about `from`. Unless `force`, the first event of the type within
    /// squared 110 tiles of `tile` is looked at after the new one is made: replaced (dropped) once it has expired,
    /// else the new one is kept silently; otherwise the new event becomes the current one and is announced.
    pub fn add<H: EventHost + ?Sized>(
        &mut self,
        h: &mut H,
        tile: (c_long, c_long),
        from: (c_long, c_long),
        kind: c_long,
        text: c_long,
        force: bool,
    ) {
        let mut found = force;
        let mut i = 0usize;
        while !found && i < self.events.len() {
            let e = &self.events[i].1;
            found = e.kind == kind && {
                let dx = e.source_x.wrapping_sub(tile.0);
                let dy = e.source_y.wrapping_sub(tile.1);
                dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= 110
            };
            if !found {
                i += 1;
            }
        }
        let new = self.make(h, tile, from, kind, text);
        if found && !force {
            let at = i + 1;
            let (old, old_time) = (self.events[at].0, self.events[at].1.time);
            let mut removed = false;
            if old_time <= 0.0 {
                removed = true;
                self.remove(h, at, false);
            }
            if self.current == Some(old) {
                self.current = Some(new);
            }
            if !removed {
                self.next_index = 0;
                return;
            }
        }
        self.next_index = 0;
        self.current = Some(new);
        Self::stop_ping(h);
        let e = self.events[0].1;
        self.announce(h, &e);
    }

    /// `TrimEvents`: expired events dropped from the back while there are more than 15.
    fn trim<H: EventHost + ?Sized>(&mut self, h: &mut H) {
        while self.events.len() > 15 {
            let (serial, e) = *self.events.back().expect("more than 15");
            if e.time > 0.0 {
                break;
            }
            if self.current == Some(serial) {
                self.current = None;
            }
            self.remove(h, self.events.len() - 1, false);
        }
    }

    /// `Clear`: every event dropped from the back, deactivated.
    pub fn clear<H: EventHost + ?Sized>(&mut self, h: &mut H) {
        while let Some(&(serial, _)) = self.events.back() {
            if self.current == Some(serial) {
                self.current = None;
            }
            self.remove(h, self.events.len() - 1, true);
        }
        self.current = None;
    }

    /// `ShowEvents` (`show`) or `HideEvents`: every event activated, or every active one deactivated.
    pub fn show<H: EventHost + ?Sized>(&mut self, h: &mut H, show: bool) {
        if !h.has_player() {
            return;
        }
        for (_, e) in self.events.iter_mut() {
            if show {
                activate(h, e);
            } else if e.active {
                deactivate(h, e);
            }
        }
    }

    /// `RepairEvents`: the active events reactivated.
    pub fn repair<H: EventHost + ?Sized>(&mut self, h: &mut H) {
        for (_, e) in self.events.iter_mut() {
            if e.active {
                reactivate(h, e);
            }
        }
    }

    /// `TickEvents`: the ping and every event counted down, then the trim.
    pub fn tick<H: EventHost + ?Sized>(&mut self, h: &mut H, elapsed: f32) {
        self.ping -= elapsed;
        if self.ping <= 0.0 {
            Self::stop_ping(h);
        }
        for (_, e) in self.events.iter_mut() {
            update(h, e, elapsed);
        }
        self.trim(h);
    }

    /// `GetNextEvent`: the current event, else the next of the 15 newest in turn (none without events).
    pub fn next_event(&mut self) -> Option<EeUnitsPlayerEvent> {
        if self.next_index >= 15 || self.next_index as usize >= self.events.len() {
            self.next_index = 0;
        }
        let event = if self.events.is_empty() {
            None
        } else if let Some(serial) = self.current.take() {
            self.events.iter().find(|(s, _)| *s == serial).map(|&(_, e)| e)
        } else {
            Some(self.events[self.next_index as usize].1)
        };
        self.next_index = self.next_index.wrapping_add(1);
        event
    }

    /// The capture's digest (PLST): FNV-1a 64 of the count, each event's members as words, the current event's index
    /// (-1 none), the next index and the ping.
    pub fn digest(&self) -> u64 {
        let mut h: u64 = 0xcbf29ce484222325;
        let mut word = |w: u32| {
            for b in w.to_le_bytes() {
                h = (h ^ u64::from(b)).wrapping_mul(0x100000001b3);
            }
        };
        word(self.events.len() as u32);
        let mut current: i32 = if self.current.is_none() { -1 } else { -2 };
        for (i, (serial, e)) in self.events.iter().enumerate() {
            if Some(*serial) == self.current {
                current = i as i32;
            }
            for w in [e.source_x, e.source_y, e.x, e.y, e.kind] {
                word(w as u32);
            }
            word(e.time.to_bits());
            word(e.text as u32);
            word(u32::from(e.active));
        }
        word(current as u32);
        word(self.next_index as u32);
        word(self.ping.to_bits());
        h
    }
}

/// `EEPPlayerEvent::Load`: the source, type, time, flag, location and text, each read stopping at the first error.
fn load_event<F: FnMut(i32, &mut EeUnitsPlayerSaveIo) -> i32>(e: &mut EeUnitsPlayerEvent, prim: &mut F) -> i32 {
    let long = |v: &mut c_long, prim: &mut F| {
        let mut io = EeUnitsPlayerSaveIo { i: *v as i64, ..Default::default() };
        let r = prim(EE_UNITS_PLAYER_SAVE_READ_LONG, &mut io);
        *v = io.i as c_long;
        r
    };
    let (mut x, mut y): (c_long, c_long) = (0, 0);
    let mut r = long(&mut x, prim);
    if r != 0 {
        return r;
    }
    r = long(&mut y, prim);
    (e.source_x, e.source_y) = (x, y);
    if r != 0 {
        return r;
    }
    r = long(&mut e.kind, prim);
    if r != 0 {
        return r;
    }
    let mut io = EeUnitsPlayerSaveIo { f: e.time, ..Default::default() };
    r = prim(EE_UNITS_PLAYER_SAVE_READ_FLOAT, &mut io);
    e.time = io.f;
    if r != 0 {
        return r;
    }
    let mut io = EeUnitsPlayerSaveIo { b: u8::from(e.active), ..Default::default() };
    r = prim(EE_UNITS_PLAYER_SAVE_READ_BOOL, &mut io);
    e.active = io.b != 0;
    if r != 0 {
        return r;
    }
    r = long(&mut x, prim);
    if r != 0 {
        return r;
    }
    r = long(&mut y, prim);
    (e.x, e.y) = (x, y);
    if r == 0 {
        r = long(&mut e.text, prim);
    }
    r
}

/// `EEPPlayerEventList::Load`: the count, then each event (pushed at the back whatever its load gave).
pub fn load<F: FnMut(i32, &mut EeUnitsPlayerSaveIo) -> i32>(list: &mut EeUnitsPlayerEvents, mut prim: F) -> i32 {
    let mut io = EeUnitsPlayerSaveIo::default();
    let r = prim(EE_UNITS_PLAYER_SAVE_READ_ULONG, &mut io);
    if r == 0 {
        for _ in 0..io.i as c_ulong {
            let mut e = EeUnitsPlayerEvent::default();
            load_event(&mut e, &mut prim);
            list.serial += 1;
            list.events.push_back((list.serial, e));
        }
    }
    r
}

/// `EEPPlayerEventList::Save`: the count, then each event's words until a write fails.
pub fn save<F: FnMut(i32, &mut EeUnitsPlayerSaveIo) -> i32>(list: &EeUnitsPlayerEvents, mut prim: F) -> i32 {
    let long = |v: c_long, prim: &mut F| {
        prim(EE_UNITS_PLAYER_SAVE_WRITE_LONG, &mut EeUnitsPlayerSaveIo { i: v as i64, ..Default::default() })
    };
    let mut r = prim(
        EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
        &mut EeUnitsPlayerSaveIo { i: list.events.len() as i64, ..Default::default() },
    );
    for (_, e) in &list.events {
        if r != 0 {
            break;
        }
        r = long(e.source_x, &mut prim);
        if r == 0 {
            r = long(e.source_y, &mut prim);
        }
        if r == 0 {
            r = long(e.kind, &mut prim);
        }
        if r == 0 {
            r = prim(EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, &mut EeUnitsPlayerSaveIo { f: e.time, ..Default::default() });
        }
        if r == 0 {
            r = prim(
                EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
                &mut EeUnitsPlayerSaveIo { b: u8::from(e.active), ..Default::default() },
            );
        }
        if r == 0 {
            r = long(e.x, &mut prim);
        }
        if r == 0 {
            r = long(e.y, &mut prim);
        }
        if r == 0 {
            r = long(e.text, &mut prim);
        }
    }
    r
}

// ---- the C ABI
/// The C++ host: `(list, player, op, event) -> the answer`.
pub type EeUnitsEventsFn = unsafe extern "C" fn(*mut c_void, *mut c_void, i32, *const EeUnitsPlayerEvent) -> i32;

struct FnHost {
    list: *mut c_void,
    player: *mut c_void,
    f: EeUnitsEventsFn,
}

impl EventHost for FnHost {
    fn has_player(&self) -> bool {
        !self.player.is_null()
    }
    fn op(&mut self, op: i32, event: &EeUnitsPlayerEvent) -> i32 {
        // SAFETY: the C++ callback's contract
        unsafe { (self.f)(self.list, self.player, op, event) }
    }
}

/// The calls on a list: `what` 0 `Clear`, 1 `ShowEvents`, 2 `HideEvents`, 3 `RepairEvents`, 4 `TickEvents(elapsed)`.
pub const EE_UNITS_EVENTS_CLEAR: u32 = 0;
pub const EE_UNITS_EVENTS_SHOW_ALL: u32 = 1;
pub const EE_UNITS_EVENTS_HIDE_ALL: u32 = 2;
pub const EE_UNITS_EVENTS_REPAIR: u32 = 3;
pub const EE_UNITS_EVENTS_TICK: u32 = 4;

/// A new, empty list.
#[no_mangle]
pub extern "C" fn ee_units_events_new() -> *mut EeUnitsPlayerEvents {
    Box::into_raw(Box::default())
}

/// # Safety
/// `events` came from [`ee_units_events_new`] (or is null) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_units_events_free(events: *mut EeUnitsPlayerEvents) {
    if !events.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(events) });
    }
}

/// `AddPlayerEvent` (see [`EeUnitsPlayerEvents::add`]) with the C++ host on `list` and `player`.
///
/// # Safety
/// `events` is live; `f` handles every operation with `list` and `player`, never calling back into the events.
#[no_mangle]
pub unsafe extern "C" fn ee_units_events_add(
    events: *mut EeUnitsPlayerEvents,
    list: *mut c_void,
    player: *mut c_void,
    f: EeUnitsEventsFn,
    tile_x: c_long,
    tile_y: c_long,
    from_x: c_long,
    from_y: c_long,
    kind: c_long,
    text: c_long,
    force: bool,
) {
    // SAFETY: the caller's contract
    unsafe { (*events).add(&mut FnHost { list, player, f }, (tile_x, tile_y), (from_x, from_y), kind, text, force) };
}

/// The list's calls `EE_UNITS_EVENTS_CLEAR` .. `EE_UNITS_EVENTS_TICK` (`elapsed` the tick's).
///
/// # Safety
/// As [`ee_units_events_add`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_events_call(
    events: *mut EeUnitsPlayerEvents,
    list: *mut c_void,
    player: *mut c_void,
    f: EeUnitsEventsFn,
    what: u32,
    elapsed: f32,
) {
    let mut h = FnHost { list, player, f };
    // SAFETY: the caller's contract
    let events = unsafe { &mut *events };
    match what {
        EE_UNITS_EVENTS_CLEAR => events.clear(&mut h),
        EE_UNITS_EVENTS_SHOW_ALL => events.show(&mut h, true),
        EE_UNITS_EVENTS_HIDE_ALL => events.show(&mut h, false),
        EE_UNITS_EVENTS_REPAIR => events.repair(&mut h),
        _ => events.tick(&mut h, elapsed),
    }
}

/// `GetNextEvent`'s source into `out_x`/`out_y`; false without an event.
///
/// # Safety
/// `events`, `out_x` and `out_y` are live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_events_next(
    events: *mut EeUnitsPlayerEvents,
    out_x: *mut c_long,
    out_y: *mut c_long,
) -> bool {
    // SAFETY: the caller's contract
    let Some(e) = (unsafe { (*events).next_event() }) else { return false };
    // SAFETY: the caller's contract
    unsafe { (*out_x, *out_y) = (e.source_x, e.source_y) };
    true
}

/// `Load` (`save` false) or `Save` (true) of the list through the save's primitives `f` on `ctx`.
///
/// # Safety
/// `events` is live; `f` handles the primitives with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_events_file(
    events: *mut EeUnitsPlayerEvents,
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    save_it: bool,
) -> i32 {
    // SAFETY: the C++ callback's contract
    let prim = move |op, io: &mut EeUnitsPlayerSaveIo| unsafe { f(ctx, op, 0, io) };
    // SAFETY: the caller's contract
    unsafe {
        if save_it {
            save(&*events, prim)
        } else {
            load(&mut *events, prim)
        }
    }
}

/// TEST ONLY: the capture's digest of the list ([`EeUnitsPlayerEvents::digest`]).
///
/// # Safety
/// `events` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_events_digest(events: *const EeUnitsPlayerEvents) -> u64 {
    // SAFETY: the caller's contract
    unsafe { (*events).digest() }
}

#[cfg(test)]
#[path = "player_events_tests.rs"]
mod tests;
