//! The game log (ee/EEGameLog.h; the C++ class's bodies moved whole): `<data root>/Logs/game-<date>-<time>.log`, one
//! "EE-GAME <ms> p<player> <event> ..." line per event (tools/game_log.py reads it). Opened at a game's first physics
//! tick (and again when the clock goes back: a new game), when the settings' game log is on and the game is not the
//! headless simulation: the start line, a line per player (kind, name, colour, civilization, the computer's
//! personality, cheats and engine); then each epoch reached, every 30 s a sample per player (epoch, population, the
//! citizens by work, the army, the buildings, the stock, the income, the losses and by whom), the chat lines, the
//! lines the computer players' trace adds, and the end. It reads the world on the physics thread (the world views;
//! the C++ host for the objects' methods) and writes nothing back.
//!
//! The host (`ee_game_log_host`, EEGameLog.cpp): the file through the platform layer (PlatFOpen's C library text
//! mode, the data root), the lobby options, a player's texts (name, civilization, the computer's personality, cheats
//! and engine) and colour row, a gather goal's resource, a unit's killer, the physics' clock and game over.

use std::os::raw::{c_char, c_long, c_ulong};
use std::sync::atomic::{AtomicBool, AtomicI64, Ordering};
use std::sync::Mutex;

use ee_sim::world::{PlayerView, UnitView, WorldRead, WorldView};

/// The host's operations (`EeGameLogCall::op`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGameLogOp {
    /// the log wanted (the settings' game log on, not EE-SIM) -> 1 and `text` the data root; 0
    Wanted = 1,
    /// `text` (a path in the game's form) made a directory
    MakeDirectory,
    /// `text` opened for writing (PlatFOpen "w") -> 1, 0 failed (`text` then strerror)
    Open,
    /// `text` written and flushed
    Write,
    /// the file closed
    Close,
    /// the lobby options: `text` the map type, i0 the map size, i1 the difficulty
    Options,
    /// player `handle`: i0 the kind (0 human, 1 computer, 2 other), i1 its colour's row, f its rgb, `text` its
    /// name, its civilization's, the personality, the cheats and the engine, separated by 0x1f ("-" none)
    Player,
    /// player `handle`'s civilization's name in `text`
    Civilization,
    /// goal `handle`'s gather resource's type (0: none)
    GatherType,
    /// unit `handle`'s last attacker's owner: 0 a human, 1 a computer, 2 none or the world
    Killer,
    /// the physics: i0 its game time, i1 game over
    Physics,
    /// the computer players' decision trace into the log too (gCPTraceOn)
    TraceOn,
    /// `text` the file's name, "/game-%Y%m%d-%H%M%S.log" in local time
    FileName,
}

/// One host call.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGameLogCall {
    pub op: EeGameLogOp,
    pub handle: u64,
    pub i: [i64; 2],
    pub f: [f32; 3],
    /// a text in or out (NUL ended)
    pub text: [c_char; 1024],
}

#[cfg(not(test))]
extern "C" {
    fn ee_game_log_host(call: *mut EeGameLogCall) -> i64;
}

#[cfg(test)]
unsafe fn ee_game_log_host(_call: *mut EeGameLogCall) -> i64 {
    0
}

struct Call(EeGameLogCall);

impl Call {
    fn new(op: EeGameLogOp, handle: u64) -> Call {
        Call(EeGameLogCall { op, handle, i: [0; 2], f: [0.0; 3], text: [0; 1024] })
    }
    fn text(mut self, text: &str) -> Call {
        for (k, b) in text.bytes().take(1023).enumerate() {
            self.0.text[k] = b as c_char;
        }
        self
    }
    fn run(mut self) -> (i64, EeGameLogCall) {
        // SAFETY: the host's contract: the call lives through it
        let r = unsafe { ee_game_log_host(&mut self.0) };
        (r, self.0)
    }
}

fn text_of(call: &EeGameLogCall) -> String {
    let bytes: Vec<u8> = call.text.iter().take_while(|&&c| c != 0).map(|&c| c as u8).collect();
    String::from_utf8_lossy(&bytes).into_owned()
}

const EE_GAME_LOG_MAX_PLAYERS: usize = 64;
const EE_GAME_LOG_SAMPLE_MS: u64 = 30000;

// ee/EEUnitClassIDs.h
const EE_GAME_LOG_CATEGORY_AMBIENT: u64 = 0;
const EE_GAME_LOG_CATEGORY_RESOURCE: u64 = 1;
const EE_GAME_LOG_CATEGORY_PROJECTILE: u64 = 9;
const EE_GAME_LOG_CATEGORY_LAND_CITIZEN: u64 = 6;
const EE_GAME_LOG_CATEGORY_NAVAL_CITIZEN: u64 = 0x16;
const EE_GAME_LOG_BUILDINGS: [u64; 6] = [0x15, 0x18, 0x19, 0x1d, 0x1e, 0x20];
const EE_GAME_LOG_GOAL_BUILD_REPAIR: u8 = 4;
const EE_GAME_LOG_GOAL_GATHER: u8 = 6;

struct Log {
    open: bool,
    last_time: u64,
    started: bool,
    ended: bool,
    next_sample: u64,
    epoch: [i64; EE_GAME_LOG_MAX_PLAYERS],
    civ: Vec<String>,
}

static LOG: Mutex<Log> = Mutex::new(Log {
    open: false,
    last_time: 0,
    started: false,
    ended: false,
    next_sample: 0,
    epoch: [-1; EE_GAME_LOG_MAX_PLAYERS],
    civ: Vec::new(),
});
static OPEN: AtomicBool = AtomicBool::new(false);

// the income and the losses since the last sample (the physics thread adds them outside the log's lock)
static INCOME: [[AtomicI64; 7]; EE_GAME_LOG_MAX_PLAYERS] =
    [const { [const { AtomicI64::new(0) }; 7] }; EE_GAME_LOG_MAX_PLAYERS];
static LOST: [[AtomicI64; 3]; EE_GAME_LOG_MAX_PLAYERS] =
    [const { [const { AtomicI64::new(0) }; 3] }; EE_GAME_LOG_MAX_PLAYERS];
static LOST_BY: [[AtomicI64; 3]; EE_GAME_LOG_MAX_PLAYERS] =
    [const { [const { AtomicI64::new(0) }; 3] }; EE_GAME_LOG_MAX_PLAYERS];

fn log() -> std::sync::MutexGuard<'static, Log> {
    LOG.lock().unwrap_or_else(|e| e.into_inner())
}

fn write(text: &str) {
    if OPEN.load(Ordering::SeqCst) {
        Call::new(EeGameLogOp::Write, 0).text(text).run();
    }
}

/// A text as one word: blanks, tabs, '=' and new lines as '_'; "-" for none.
pub fn narrow(text: &str) -> String {
    let s: String = text.chars().map(|c| if matches!(c, ' ' | '\t' | '=' | '\n' | '\r') { '_' } else { c }).collect();
    if s.is_empty() {
        "-".into()
    } else {
        s
    }
}

/// The unit kinds the samples count.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Kind {
    pub category: u64,
    pub land_building: bool,
}

impl Kind {
    fn of(u: &UnitView) -> Option<Kind> {
        u.class().map(|c| Kind { category: c.category(), land_building: c.land_building() })
    }
    pub fn building(self) -> bool {
        self.land_building || EE_GAME_LOG_BUILDINGS.contains(&self.category)
    }
    pub fn citizen(self) -> bool {
        matches!(self.category, EE_GAME_LOG_CATEGORY_LAND_CITIZEN | EE_GAME_LOG_CATEGORY_NAVAL_CITIZEN)
    }
    /// a unit of the player's (not an AI path point, a resource or a shot)
    pub fn counted(self) -> bool {
        !matches!(
            self.category,
            EE_GAME_LOG_CATEGORY_AMBIENT | EE_GAME_LOG_CATEGORY_RESOURCE | EE_GAME_LOG_CATEGORY_PROJECTILE
        )
    }
}

fn close(l: &mut Log) {
    if l.open {
        if !l.ended {
            write(&format!("EE-GAME {} p0 end\n", l.last_time));
        }
        Call::new(EeGameLogOp::Close, 0).run();
        l.open = false;
        OPEN.store(false, Ordering::SeqCst);
    }
}

fn epoch_of(p: &PlayerView) -> i64 {
    p.tech_tree().map_or(-1, |t| t.current_epoch())
}

fn civilization(p: &PlayerView) -> String {
    narrow(&text_of(&Call::new(EeGameLogOp::Civilization, p.handle()).run().1))
}

fn open(l: &mut Log) {
    close(l);
    let (wanted, call) = Call::new(EeGameLogOp::Wanted, 0).run();
    if wanted == 0 {
        return; // off, or the headless simulation (its own report and trace)
    }
    let dir = format!("{}/Logs", text_of(&call));
    Call::new(EeGameLogOp::MakeDirectory, 0).text(&dir).run();
    let name = text_of(&Call::new(EeGameLogOp::FileName, 0).run().1);
    let path = format!("{dir}{name}");
    let (opened, call) = Call::new(EeGameLogOp::Open, 0).text(&path).run();
    if opened == 0 {
        crate::fixture::host::stderr(&format!("EE-GAME: cannot write the game log {path}: {}\n", text_of(&call)));
        return;
    }
    crate::fixture::host::stderr(&format!("EE-GAME: the game log is {path}\n"));
    l.open = true;
    OPEN.store(true, Ordering::SeqCst);
    l.ended = false;
    l.epoch = [-1; EE_GAME_LOG_MAX_PLAYERS];
    for a in INCOME.iter().flatten().chain(LOST.iter().flatten()).chain(LOST_BY.iter().flatten()) {
        a.store(0, Ordering::SeqCst);
    }
    l.next_sample = 0;
    // SAFETY: the physics thread reads the world it steps
    let t = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&t) else { return };
    let (_, o) = Call::new(EeGameLogOp::Options, 0).run();
    write(&format!(
        "EE-GAME 0 p0 start map={} size={} difficulty={} players={}\n",
        narrow(&text_of(&o)),
        o.i[0] as c_ulong,
        o.i[1] as c_ulong,
        world.num_players() as c_ulong
    ));
    l.civ = vec![String::new(); EE_GAME_LOG_MAX_PLAYERS];
    for i in 1..world.num_players() as usize {
        let Some(p) = world.player(i) else { continue };
        let (_, c) = Call::new(EeGameLogOp::Player, p.handle()).run();
        let texts = text_of(&c);
        let parts: Vec<String> = texts.split('\x1f').map(String::from).collect();
        let part = |k: usize| parts.get(k).cloned().unwrap_or_default();
        let kind = ["human", "computer", "other"][c.i[0].clamp(0, 2) as usize];
        // color: the dbcolortable row (tools/game_log.py names it from the data); rgb its colour
        write(&format!(
            "EE-GAME 0 p{i} player kind={kind} name={} color={} rgb={:.2}/{:.2}/{:.2} civ={} personality={} cheats={} engine={}\n",
            narrow(&part(0)),
            c.i[1] as c_ulong,
            c.f[0],
            c.f[1],
            c.f[2],
            narrow(&part(1)),
            part(2),
            part(3),
            part(4)
        ));
        if i < EE_GAME_LOG_MAX_PLAYERS {
            l.civ[i] = narrow(&part(1));
        }
    }
    Call::new(EeGameLogOp::TraceOn, 0).run();
}

/// A player's sample's counts (the units in its list).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Counts {
    pub citizens: c_long,
    pub boats: c_long,
    pub idle: c_long,
    pub builders: c_long,
    pub army: c_long,
    pub buildings: c_long,
    pub unfinished: c_long,
    pub towers: c_long,
    pub gather: [c_long; 7],
}

/// One living unit of the player's counted: its kind, whether it is built and a tower, its active goal's kind
/// (None: idle) and a gather goal's resource type.
pub fn count(
    c: &mut Counts,
    kind: Kind,
    built: bool,
    tower: bool,
    goal: Option<u8>,
    gather_type: impl FnOnce() -> u64,
) {
    if !kind.counted() {
        return;
    }
    if kind.citizen() {
        c.citizens += 1;
        if kind.category == EE_GAME_LOG_CATEGORY_NAVAL_CITIZEN {
            c.boats += 1;
        }
        match goal {
            None => c.idle += 1,
            Some(EE_GAME_LOG_GOAL_GATHER) => {
                let t = gather_type();
                c.gather[if t < 7 { t as usize } else { 0 }] += 1;
            }
            Some(EE_GAME_LOG_GOAL_BUILD_REPAIR) => c.builders += 1,
            _ => {}
        }
    } else if kind.building() {
        c.buildings += 1;
        if !built {
            c.unfinished += 1;
        } else if tower {
            c.towers += 1;
        }
    } else {
        c.army += 1;
    }
}

fn sample(l: &mut Log, world: &WorldView, time: u64) {
    for i in 1..(world.num_players() as usize).min(EE_GAME_LOG_MAX_PLAYERS) {
        let Some(p) = world.player(i) else { continue };
        let civ = civilization(&p);
        if civ != l.civ[i] {
            // chosen after the start (the civilization screen opens in the game)
            write(&format!("EE-GAME {time} p{i} civ name={civ}\n"));
            l.civ[i] = civ;
        }
        let mut c = Counts::default();
        for u in std::iter::successors(p.first_unit(), |u| u.next()) {
            let Some(x) = u.complex_unit() else { continue };
            if u.hit_points() <= 0 {
                continue;
            }
            let Some(kind) = Kind::of(&u) else { continue };
            let tower = u.class().is_some_and(|k| ee_sim::world_field!(k.fields(), mIsTower));
            let goal = x.active_goal_handle();
            let goal_kind = x.active_goal().map(|g| g.0);
            let goal_kind = if goal == 0 { None } else { goal_kind.or(Some(0xff)) };
            count(&mut c, kind, x.is_built(), tower, goal_kind, || {
                Call::new(EeGameLogOp::GatherType, goal).run().0 as u64
            });
        }
        let s: [c_long; 7] = ee_sim::world_field!(p.saved(), mResourceCaps);
        let population: c_long = ee_sim::world_field!(p.saved(), mPopulationCount);
        let take = |a: &AtomicI64| a.swap(0, Ordering::SeqCst);
        let got: Vec<i64> = INCOME[i].iter().map(take).collect();
        let lost: Vec<i64> = LOST[i].iter().map(take).collect();
        let by: Vec<i64> = LOST_BY[i].iter().map(take).collect();
        let g = c.gather;
        write(&format!(
            "EE-GAME {time} p{i} sample epoch={} pop={population} citizens={} boats={} idle={} gather={}/{}/{}/{}/{} build={} army={} \
             buildings={} unfinished={} towers={} stock={}/{}/{}/{}/{} got={}/{}/{}/{}/{} lost={}/{}/{} lost_by={}/{}/{}\n",
            epoch_of(&p),
            c.citizens,
            c.boats,
            c.idle,
            g[2],
            g[3],
            g[4],
            g[5],
            g[6],
            c.builders,
            c.army,
            c.buildings,
            c.unfinished,
            c.towers,
            s[2],
            s[3],
            s[4],
            s[5],
            s[6],
            got[2],
            got[3],
            got[4],
            got[5],
            got[6],
            lost[0],
            lost[1],
            lost[2],
            by[0],
            by[1],
            by[2]
        ));
    }
}

/// `EEGameLogOn`: a game log is open.
#[no_mangle]
pub extern "C" fn ee_game_log_on() -> bool {
    OPEN.load(Ordering::SeqCst)
}

/// `EEGameLogTick`: every physics tick (EEArtificialIntelligence::Update).
#[no_mangle]
pub extern "C" fn ee_game_log_tick(time: c_ulong) {
    let time = u64::from(time);
    let mut l = log();
    if !l.started || time + 1000 < l.last_time {
        // the first game, or a new one (the clock went back)
        open(&mut l);
        l.started = true;
    }
    l.last_time = time;
    if !l.open {
        return;
    }
    // SAFETY: the physics thread reads the world it steps
    let t = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&t) else { return };
    for i in 1..(world.num_players() as usize).min(EE_GAME_LOG_MAX_PLAYERS) {
        let e = world.player(i).map_or(-1, |p| epoch_of(&p));
        if e > l.epoch[i] {
            if l.epoch[i] >= 0 {
                write(&format!("EE-GAME {time} p{i} epoch to={e}\n"));
            }
            l.epoch[i] = e;
        }
    }
    let (_, physics) = Call::new(EeGameLogOp::Physics, 0).run();
    if physics.i[1] != 0 && !l.ended {
        // the game's result: a last sample and the end line
        sample(&mut l, &world, time);
        write(&format!("EE-GAME {time} p0 end result=over\n"));
        l.ended = true;
        return;
    }
    if l.ended {
        return;
    }
    if time >= l.next_sample {
        sample(&mut l, &world, time);
        l.next_sample = time - time % EE_GAME_LOG_SAMPLE_MS + EE_GAME_LOG_SAMPLE_MS;
    }
}

/// `EEGameLogLine`: a whole line, its new line included (the AI trace).
///
/// # Safety
/// `text` is NUL ended or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_game_log_line(text: *const c_char) {
    if text.is_null() {
        return;
    }
    let _l = log();
    // SAFETY: the contract
    let text = unsafe { std::ffi::CStr::from_ptr(text) }.to_string_lossy();
    write(&text);
}

/// `EEGameLogIncome`: EEPlayer::AddResource's amount for player `id`.
#[no_mangle]
pub extern "C" fn ee_game_log_income(id: c_ulong, resource: c_long, amount: c_long) {
    if !OPEN.load(Ordering::SeqCst) || amount <= 0 || !(0..=6).contains(&resource) {
        return;
    }
    if let Some(row) = INCOME.get(id as usize) {
        row[resource as usize].fetch_add(i64::from(amount), Ordering::SeqCst);
    }
}

/// `EEGameLogUnitLost`: player `player`'s unit `unit` taken (EEPlayer::UnitTaken, not swapped).
#[no_mangle]
pub extern "C" fn ee_game_log_unit_lost(player: u64, unit: u64) {
    if !OPEN.load(Ordering::SeqCst) || player == 0 || unit == 0 {
        return;
    }
    // SAFETY: the caller's live player and unit (the physics thread)
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let (Some(p), Some(u)) = (unsafe { PlayerView::of(&t, player) }, unsafe { UnitView::of(&t, unit) }) else {
        return;
    };
    let Some(kind) = Kind::of(&u) else { return };
    if u.complex_unit_handle() == 0 {
        return;
    }
    let id = p.id() as usize;
    if id == 0 || id >= EE_GAME_LOG_MAX_PLAYERS || !kind.counted() {
        return;
    }
    LOST[id][if kind.citizen() {
        0
    } else if kind.building() {
        2
    } else {
        1
    }]
    .fetch_add(1, Ordering::SeqCst);
    let killer = Call::new(EeGameLogOp::Killer, unit).run().0.clamp(0, 2) as usize;
    LOST_BY[id][killer].fetch_add(1, Ordering::SeqCst);
}

/// `EEGameLogChat`: a chat line (EEMInGameChat::Execute).
///
/// # Safety
/// `text` is NUL ended or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_game_log_chat(player: c_ulong, text: *const c_char) {
    let _l = log();
    if !OPEN.load(Ordering::SeqCst) || text.is_null() {
        return;
    }
    // SAFETY: the contract
    let text = unsafe { std::ffi::CStr::from_ptr(text) }.to_string_lossy().replace(['\n', '\r'], " ");
    let (_, physics) = Call::new(EeGameLogOp::Physics, 0).run();
    write(&format!("EE-GAME {} p{player} chat text={text}\n", physics.i[0] as c_ulong));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn narrow_words() {
        assert_eq!(narrow("Hard AI = x\n"), "Hard_AI___x_");
        assert_eq!(narrow(""), "-");
    }

    #[test]
    fn counts_by_kind_and_work() {
        let mut c = Counts::default();
        let citizen = Kind { category: EE_GAME_LOG_CATEGORY_LAND_CITIZEN, land_building: false };
        let boat = Kind { category: EE_GAME_LOG_CATEGORY_NAVAL_CITIZEN, land_building: false };
        let house = Kind { category: 2, land_building: true };
        let tower = Kind { category: 0x1d, land_building: false };
        count(&mut c, citizen, true, false, None, || 0);
        count(&mut c, citizen, true, false, Some(EE_GAME_LOG_GOAL_GATHER), || 3);
        count(&mut c, boat, true, false, Some(EE_GAME_LOG_GOAL_GATHER), || 99);
        count(&mut c, citizen, true, false, Some(EE_GAME_LOG_GOAL_BUILD_REPAIR), || 0);
        count(&mut c, house, false, false, None, || 0);
        count(&mut c, tower, true, true, None, || 0);
        count(&mut c, Kind { category: 4, land_building: false }, true, false, None, || 0);
        count(&mut c, Kind { category: EE_GAME_LOG_CATEGORY_RESOURCE, land_building: false }, true, false, None, || 0);
        assert_eq!(
            c,
            Counts {
                citizens: 4,
                boats: 1,
                idle: 1,
                builders: 1,
                army: 1,
                buildings: 2,
                unfinished: 1,
                towers: 1,
                gather: [1, 0, 0, 1, 0, 0, 0]
            }
        );
    }
}
