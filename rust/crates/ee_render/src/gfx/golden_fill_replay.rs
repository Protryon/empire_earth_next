//! `fill.rs` replayed against the unmodified C++ (`golden_fill.txt`, see golden_fill.rs): every frame's world is
//! rebuilt as fake objects laid out like the game's classes (the records the views read, at offsets this test
//! registers as the world's layout), and `fill_unit_fields` / `fill_object_fields` run for every render unit and
//! object the C++'s `FillUnit` / `FillObject` filled, over a host answering what the driver's stand-in world
//! answered. Each entry starts from its pool slot's previous dump (the fill keeps the stale members the original kept)
//! and must end equal to the C++'s, member for member, with its health bars, name, selected and pointed-at lists and
//! debug label.
#![allow(clippy::too_many_lines)]

use std::collections::BTreeMap;
use std::mem::offset_of;
use std::os::raw::{c_long, c_ulong};

use ee_base::math::types::GeTransformation;
use ee_sim::world::map_fields::{EeUnitsMapFields, EeUnitsRtsMapFields};
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use ee_sim::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use ee_sim::world::{EeWorldLayout, UnitView, WorldRead};

use crate::gfx::effects::EeGfxColor;
use crate::gfx::fill::{
    fill_object_fields, fill_unit_fields, EeGfxFillFrame, EeGfxFillMorale, EeGfxFillObject, EeGfxFillPoint,
    EeGfxFillUnit, FillHost,
};

const GOLDEN: &str = include_str!("golden_fill.txt");

fn hex(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s.trim_end_matches(']'), 16).expect("a hex word"))
}

fn num<T: std::str::FromStr>(s: &str, prefix: &str) -> T
where
    T::Err: std::fmt::Debug,
{
    s.strip_prefix(prefix).unwrap_or(s).trim_end_matches(']').parse().expect("a number")
}

// ---- the fake objects (any layout: the test registers it)

#[repr(C)]
#[derive(Default)]
struct FUnit {
    rts: EeUnitsRtsUnitFields,
    fields: EeUnitsUnitFields,
    complex: EeUnitsComplexUnitFields,
    kind: u8,
    tile_node: usize,
    next: usize,
    tile: usize,
    player: usize,
    class: usize,
    stats: usize,
    complex_unit: usize,
    transform: [[f32; 4]; 4],
    command: usize,
    goal: usize,
    active_goal: usize,
    cargo: [usize; 3],
    previous: usize,
    next_part: usize,
}

#[repr(C)]
#[derive(Default)]
struct FGoal {
    kind: u8,
    id: c_ulong,
}

#[repr(C)]
#[derive(Default)]
struct FCommand {
    kind: u32,
}

#[repr(C)]
#[derive(Default)]
struct FClass {
    fields: EeUnitsUnitClassFields,
}

#[repr(C)]
#[derive(Default)]
struct FStats {
    fields: EeUnitsUnitClassUpgradeFields,
}

#[repr(C)]
#[derive(Default)]
struct FPlayer {
    rts: EeUnitsRtsPlayerFields,
    saved: EeUnitsPlayerSaved,
    fields: EeUnitsPlayerFields,
    color: [f32; 3],
    first: usize,
    group: usize,
    viewed: usize,
    selected_viewed: usize,
}

#[repr(C)]
#[derive(Default)]
struct FTile {
    terrain: c_long,
    units: usize,
    visibility: usize,
    height: f32,
    ai: i8,
    words: [u32; 2],
}

#[repr(C)]
#[derive(Default)]
struct FNode {
    unit: usize,
    next: usize,
}

#[repr(C)]
#[derive(Default)]
struct FMap {
    rts: EeUnitsRtsMapFields,
    fields: EeUnitsMapFields,
    tiles: usize,
}

fn register_layout() {
    static ONCE: std::sync::Once = std::sync::Once::new();
    ONCE.call_once(|| {
        let l = EeWorldLayout {
            size: std::mem::size_of::<EeWorldLayout>() as u64,
            rts_unit_fields: offset_of!(FUnit, rts) as u64,
            unit_fields: offset_of!(FUnit, fields) as u64,
            complex_unit_fields: offset_of!(FUnit, complex) as u64,
            unit_class_fields: offset_of!(FClass, fields) as u64,
            stats_fields: offset_of!(FStats, fields) as u64,
            rts_player_fields: offset_of!(FPlayer, rts) as u64,
            player_saved: offset_of!(FPlayer, saved) as u64,
            player_fields: offset_of!(FPlayer, fields) as u64,
            rts_map_fields: offset_of!(FMap, rts) as u64,
            map_fields: offset_of!(FMap, fields) as u64,
            unit_object_kind: offset_of!(FUnit, kind) as u64,
            unit_tile_node: offset_of!(FUnit, tile_node) as u64,
            unit_next: offset_of!(FUnit, next) as u64,
            unit_tile: offset_of!(FUnit, tile) as u64,
            unit_player: offset_of!(FUnit, player) as u64,
            unit_class: offset_of!(FUnit, class) as u64,
            unit_complex_unit: offset_of!(FUnit, complex_unit) as u64,
            unit_stats: offset_of!(FUnit, stats) as u64,
            complex_location: offset_of!(FUnit, transform) as u64,
            complex_command: offset_of!(FUnit, command) as u64,
            complex_goal_state: offset_of!(FUnit, goal) as u64,
            complex_active_goal: offset_of!(FUnit, active_goal) as u64,
            complex_cargo: offset_of!(FUnit, cargo) as u64,
            complex_previous_part: offset_of!(FUnit, previous) as u64,
            complex_next_part: offset_of!(FUnit, next_part) as u64,
            goal_kind: offset_of!(FGoal, kind) as u64,
            goal_id: offset_of!(FGoal, id) as u64,
            action_type: offset_of!(FCommand, kind) as u64,
            player_color: offset_of!(FPlayer, color) as u64,
            player_first_unit: offset_of!(FPlayer, first) as u64,
            player_selected_group: offset_of!(FPlayer, group) as u64,
            map_tiles: offset_of!(FMap, tiles) as u64,
            tile_stride: std::mem::size_of::<FTile>() as u64,
            tile_terrain_type: offset_of!(FTile, terrain) as u64,
            tile_units: offset_of!(FTile, units) as u64,
            tile_visibility: offset_of!(FTile, visibility) as u64,
            tile_height: offset_of!(FTile, height) as u64,
            tile_ai_value: offset_of!(FTile, ai) as u64,
            unit_node_unit: offset_of!(FNode, unit) as u64,
            unit_node_next: offset_of!(FNode, next) as u64,
            player_viewed_unit: offset_of!(FPlayer, viewed) as u64,
            player_selected_viewed_unit: offset_of!(FPlayer, selected_viewed) as u64,
            ..EeWorldLayout::default()
        };
        assert!(ee_sim::world::layout::set(l), "the test's layout");
    });
}

fn addr<T>(b: &T) -> usize {
    b as *const T as usize
}

// ---- the golden's tables and frames

#[derive(Clone, Default)]
struct KClass {
    category: c_ulong,
    tile: c_long,
    family: c_ulong,
    kind: c_ulong,
    selection: f32,
    scale: f32,
    shadow: f32,
    l170: f32,
    ground: f32,
    tree: bool,
    flare: bool,
    obstacle: bool,
    faces: bool,
    land: bool,
    scaffolding: i64,
}

#[derive(Clone, Default)]
struct KStats {
    scale: f32,
    range: f32,
    max: c_long,
    power: c_long,
    model: u64,
    name: String,
}

#[derive(Clone, Default)]
struct KPlayer {
    color: [f32; 3],
    enemies: [c_ulong; 4],
    human: bool,
    unchanged: bool,
    civ: String,
    name: String,
}

/// A unit's W line (and a complex unit's tail).
#[derive(Clone, Default)]
struct KUnit {
    id: u32,
    class: usize,
    stats: usize,
    player: usize,
    tile: (c_long, c_long),
    resource: (c_ulong, c_long),
    hp: c_long,
    editor: bool,
    hidden: u8,
    complex: Option<KComplex>,
}

#[derive(Clone, Default)]
struct KComplex {
    transform: [[f32; 4]; 4],
    scale: f32,
    frame: c_ulong,
    tinted: bool,
    color: [f32; 3],
    built: bool,
    foundation: bool,
    open: f32,
    fraction: f32,
    goal: i64,
    turrets: Vec<f32>,
    orbitals: usize,
    wheels: usize,
    cargo: Vec<u32>,
    power: c_long,
    research: c_long,
    production: c_long,
    next_frame: c_long,
    command: i64,
    animation: c_ulong,
    effects: Vec<(i64, c_ulong)>,
    previous: i64,
    next: i64,
    side: bool,
    waypoint: Option<[f32; 3]>,
}

#[derive(Default)]
struct KFrame {
    free: bool,
    skip: bool,
    music: bool,
    spheres: bool,
    sea: f32,
    current: usize,
    camera: [[f32; 4]; 4],
    units: Vec<KUnit>,
    tiles: Vec<((c_long, c_long), u32, Vec<u32>)>,
    viewed: i64,
    selected_viewed: i64,
    group: Vec<u32>,
    pointed: i64,
    rect: Vec<c_ulong>,
    sway: u32,
    entries: Vec<Vec<String>>,
    objects: Vec<Vec<String>>,
    selected_units: Vec<usize>,
    selected_objects: Vec<usize>,
    selected_unit: i64,
    debug_unit: i64,
    debug_text: String,
}

struct Golden {
    model_frames: Vec<u64>,
    object_model0: Vec<usize>,
    classes: Vec<KClass>,
    stats: Vec<KStats>,
    players: Vec<KPlayer>,
    frames: Vec<KFrame>,
}

fn quoted(t: &[&str], at: usize) -> (String, usize) {
    // a 'quoted' token (no spaces in the golden's names)
    (t[at].trim_matches('\'').to_string(), at + 1)
}

fn parse() -> Golden {
    let mut g = Golden {
        model_frames: vec![],
        object_model0: vec![],
        classes: vec![],
        stats: vec![],
        players: vec![],
        frames: vec![],
    };
    let mut cur: Option<KFrame> = None;
    let mut pending = KFrame::default();
    for line in GOLDEN.lines() {
        let t: Vec<&str> = line.split_whitespace().collect();
        let Some(&head) = t.first() else { continue };
        if head.starts_with("MO") {
            g.model_frames.push(t[1].parse().unwrap());
        } else if head.starts_with("OB") {
            g.object_model0.push(num(t[1], "m"));
        } else if head.starts_with('K') {
            let f = t[10].as_bytes();
            g.classes.push(KClass {
                category: t[1].parse().unwrap(),
                tile: t[2].parse().unwrap(),
                family: t[3].parse().unwrap(),
                kind: t[4].parse().unwrap(),
                selection: hex(t[5]),
                scale: hex(t[6]),
                shadow: hex(t[7]),
                l170: hex(t[8]),
                ground: hex(t[9]),
                tree: f[0] == b'1',
                flare: f[1] == b'1',
                obstacle: f[2] == b'1',
                faces: f[3] == b'1',
                land: f[4] == b'1',
                scaffolding: t[11].parse().unwrap(),
            });
        } else if head.starts_with('Q') {
            g.stats.push(KStats {
                scale: hex(t[1]),
                range: hex(t[2]),
                max: t[3].parse().unwrap(),
                power: t[4].parse().unwrap(),
                model: num(t[5], "o"),
                name: t[6].trim_matches('\'').to_string(),
            });
        } else if head.starts_with("PL") {
            g.players.push(KPlayer {
                color: [hex(t[1]), hex(t[2]), hex(t[3])],
                enemies: [t[5].parse().unwrap(), t[6].parse().unwrap(), t[7].parse().unwrap(), t[8].parse().unwrap()],
                human: t[9] == "h1",
                unchanged: t[10] == "u1",
                civ: t[11].trim_matches('\'').to_string(),
                name: t[12].trim_matches('\'').to_string(),
            });
        } else if head == "SEL" {
            pending.viewed = num(t[1], "v");
            pending.selected_viewed = num(t[2], "sv");
            pending.group = t[4..].iter().map(|s| s.parse().unwrap()).collect();
        } else if head == "E" {
            if let Some(f) = cur.take() {
                g.frames.push(f);
            }
            let mut f = std::mem::take(&mut pending);
            f.free = t[1] == "free1";
            f.music = t[3] == "music1";
            f.spheres = t[4] != "spheres0";
            f.skip = t[6] == "skip1";
            f.sea = hex(&t[10][3..]);
            f.current = num(t[14], "current");
            cur = Some(f);
        } else if head == "CAM" {
            let f = cur.as_mut().unwrap();
            for i in 0..16 {
                f.camera[i / 4][i % 4] = hex(t[1 + i]);
            }
        } else if head.starts_with('W') {
            cur.as_mut().unwrap().units.push(parse_unit(&t));
        } else if let Some(at) = head.strip_prefix('T') {
            let (x, y) = at.split_once(',').unwrap();
            let count = if t[2] == "s1" {
                2
            } else if t[1] == "v1" {
                1
            } else {
                0
            };
            let ids = t[4..].iter().map(|s| s.parse().unwrap()).collect();
            cur.as_mut().unwrap().tiles.push(((x.parse().unwrap(), y.parse().unwrap()), count, ids));
        } else if head.starts_with('F') && head[1..].chars().all(|c| c.is_ascii_digit()) {
            let f = cur.as_mut().unwrap();
            f.pointed = t.iter().find_map(|s| s.strip_prefix("picked").map(|v| v.parse().unwrap())).unwrap_or(-1);
        } else if head.starts_with('U') && head[1..].chars().all(|c| c.is_ascii_digit()) {
            cur.as_mut().unwrap().entries.push(t.iter().map(|s| s.to_string()).collect());
        } else if head.starts_with('S') && head[1..].chars().all(|c| c.is_ascii_digit()) {
            cur.as_mut().unwrap().objects.push(t.iter().map(|s| s.to_string()).collect());
        } else if head == "SU" {
            cur.as_mut().unwrap().selected_units = t[1..].iter().map(|s| s.parse().unwrap()).collect();
        } else if head == "SO" {
            cur.as_mut().unwrap().selected_objects = t[1..].iter().map(|s| s.parse().unwrap()).collect();
        } else if head == "X" {
            let f = cur.as_mut().unwrap();
            f.selected_unit = t[1].parse().unwrap();
            f.debug_unit = t[3].parse().unwrap();
            let (text, at) = quoted(&t, 4);
            f.debug_text = text;
            let sway = t.iter().position(|s| *s == "sway").unwrap();
            f.rect = t[at + 1..sway].iter().map(|s| s.parse().unwrap()).collect();
            f.sway = u32::from_str_radix(t[sway + 1], 16).unwrap();
        }
    }
    if let Some(f) = cur {
        g.frames.push(f);
    }
    g
}

fn parse_unit(t: &[&str]) -> KUnit {
    let (x, y) = t[4][1..].split_once(',').unwrap();
    let (kind, amount) = t[5][1..].split_once(',').unwrap();
    let mut u = KUnit {
        id: num(t[0], "W"),
        class: num(t[1], "c"),
        stats: num(t[2], "s"),
        player: num(t[3], "p"),
        tile: (x.parse().unwrap(), y.parse().unwrap()),
        resource: (kind.parse().unwrap(), amount.parse().unwrap()),
        hp: num(t[6], "hp"),
        editor: t[7] == "e1",
        hidden: num(t[8], "h"),
        complex: None,
    };
    if t.len() > 12 && t[12] == "C" {
        let mut c = KComplex::default();
        for i in 0..16 {
            c.transform[i / 4][i % 4] = hex(t[13 + i]);
        }
        let mut i = 29;
        c.scale = hex(t[i]);
        c.frame = t[i + 1].parse().unwrap();
        c.tinted = t[i + 2] == "tint1";
        c.color = [hex(t[i + 3]), hex(t[i + 4]), hex(t[i + 5])];
        c.built = t[i + 6] == "built1";
        c.foundation = t[i + 7] == "fnd1";
        c.open = hex(&t[i + 8][4..]);
        c.fraction = hex(&t[i + 9][2..]);
        c.goal = num(t[i + 10], "goal");
        i += 12; // past "tur"
        while !t[i].starts_with("orb") {
            c.turrets.push(hex(t[i]));
            i += 1;
        }
        c.orbitals = num(t[i], "orb");
        c.wheels = num(t[i + 1], "wh");
        i += 3; // past "cargo"
        while !t[i].starts_with("pw") {
            c.cargo.push(t[i].parse().unwrap());
            i += 1;
        }
        c.power = num(t[i], "pw");
        c.research = num(t[i + 1], "res");
        c.production = num(t[i + 2], "pt");
        c.next_frame = num(t[i + 3], "nf");
        c.command = num(t[i + 4], "cmd");
        c.animation = num(t[i + 5], "an");
        i += 7; // past "ae"
        while !t[i].starts_with("prev") {
            let (k, s) = t[i].split_once(':').unwrap();
            c.effects.push((k.parse().unwrap(), s.parse().unwrap()));
            i += 1;
        }
        c.previous = num(t[i], "prev");
        c.next = num(t[i + 1], "next");
        c.side = t[i + 2] == "side1";
        if t[i + 3] == "wp1" {
            c.waypoint = Some([hex(t[i + 4]), hex(t[i + 5]), hex(t[i + 6])]);
        }
        u.complex = Some(c);
    }
    u
}

// ---- a frame's fake world

struct World {
    units: Vec<Box<FUnit>>,
    classes: Vec<Box<FClass>>,
    stats: Vec<Box<FStats>>,
    players: Vec<Box<FPlayer>>,
    tiles: Vec<Box<FTile>>,
    tile_pointers: Vec<usize>,
    nodes: Vec<Box<FNode>>,
    goals: Vec<Box<FGoal>>,
    commands: Vec<Box<FCommand>>,
    cargo_lists: Vec<Vec<usize>>,
    map: Box<FMap>,
    by_id: BTreeMap<u32, usize>,
}

fn build(g: &Golden, f: &KFrame) -> World {
    let mut w = World {
        units: vec![],
        classes: vec![],
        stats: vec![],
        players: vec![],
        tiles: vec![],
        tile_pointers: vec![],
        nodes: vec![],
        goals: vec![],
        commands: vec![],
        cargo_lists: vec![],
        map: Box::default(),
        by_id: BTreeMap::new(),
    };
    for k in &g.classes {
        let mut c = FClass::default();
        c.fields.mCategory = k.category;
        c.fields.mTileSize = k.tile;
        c.fields.mFamily = k.family;
        c.fields.mUnitType = k.kind;
        c.fields.mSelectionSize = k.selection;
        c.fields.mScale = k.scale;
        c.fields.mShadowScale = k.shadow;
        c.fields.mL170 = k.l170;
        c.fields.mIsTree = k.tree;
        c.fields.mIsFlare = k.flare;
        c.fields.mIsObstacle = k.obstacle;
        c.fields.mFacesCamera = k.faces;
        c.fields.mIsLandBuildingClass = k.land;
        w.classes.push(Box::new(c));
    }
    for k in &g.stats {
        let mut s = FStats::default();
        s.fields.mScale = k.scale;
        s.fields.mRange = k.range;
        s.fields.mMaxHitPoints = k.max;
        s.fields.mPower = k.power;
        w.stats.push(Box::new(s));
    }
    for (i, k) in g.players.iter().enumerate() {
        let mut p = FPlayer::default();
        p.rts.mPlayerID = i as c_ulong;
        p.color = k.color;
        p.saved.mEnemies[..4].copy_from_slice(&k.enemies);
        w.players.push(Box::new(p));
    }
    for _ in 0..64 * 64 {
        w.tiles.push(Box::default());
    }
    for ((x, y), count, _) in &f.tiles {
        let tile = &mut w.tiles[(*y * 64 + *x) as usize];
        tile.words = [count | count << 16; 2];
    }
    for (i, t) in w.tiles.iter_mut().enumerate() {
        t.visibility = addr(&t.words);
        t.height = map_z((i % 64) as f32, (i / 64) as f32);
    }
    w.tile_pointers = w.tiles.iter().map(|t| addr(&**t)).collect();
    w.map.rts.mSize = 64;
    w.map.tiles = w.tile_pointers.as_ptr() as usize;
    for k in &f.units {
        let mut u = FUnit::default();
        u.rts.mUnitID = k.id as c_ulong;
        u.rts.mTileX = k.tile.0;
        u.rts.mTileY = k.tile.1;
        u.rts.mIsEditorSelected = k.editor;
        u.fields.mResourceType = k.resource.0;
        u.fields.mResourceAmount = k.resource.1;
        u.fields.mHitPoints = k.hp;
        u.fields.mHiddenFlags = k.hidden;
        u.kind = 1;
        u.tile = if (0..64).contains(&k.tile.0) && (0..64).contains(&k.tile.1) {
            w.tile_pointers[(k.tile.1 * 64 + k.tile.0) as usize]
        } else {
            0
        };
        u.player = addr(&*w.players[k.player]);
        u.class = addr(&*w.classes[k.class]);
        u.stats = addr(&*w.stats[k.stats]);
        if let Some(c) = &k.complex {
            u.kind = 2;
            u.transform = c.transform;
            u.complex.mIsTinted = c.tinted;
            u.complex.mIsBuilt = c.built;
            u.complex.mIsFoundation = c.foundation;
            u.complex.mOpenFraction = c.open;
            u.complex.mAnimationFraction = c.fraction;
            u.complex.mPower = c.power;
            u.complex.mCurrentResearch = c.research;
            u.complex.mProductionTime = c.production;
            u.complex.mNextFrame = c.next_frame;
            u.complex.mAnimation = c.animation;
            if c.goal >= 0 {
                w.goals.push(Box::new(FGoal { kind: c.goal as u8, id: 0 }));
                u.active_goal = addr(&**w.goals.last().unwrap());
            }
            if c.command >= 0 {
                w.commands.push(Box::new(FCommand { kind: c.command as u32 }));
                u.command = addr(&**w.commands.last().unwrap());
            }
        }
        w.by_id.insert(k.id, w.units.len());
        w.units.push(Box::new(u));
    }
    // the pointers between units: a complex unit is its own, its parts, its cargo
    let handles: Vec<usize> = w.units.iter().map(|u| addr(&**u)).collect();
    for (i, k) in f.units.iter().enumerate() {
        if let Some(c) = &k.complex {
            let list: Vec<usize> = c.cargo.iter().map(|id| handles[w.by_id[id]]).collect();
            let u = &mut w.units[i];
            u.complex_unit = handles[i];
            u.previous = if c.previous >= 0 { handles[w.by_id[&(c.previous as u32)]] } else { 0 };
            u.next_part = if c.next >= 0 { handles[w.by_id[&(c.next as u32)]] } else { 0 };
            w.cargo_lists.push(list);
            let l = w.cargo_lists.last().unwrap();
            let begin = l.as_ptr() as usize;
            w.units[i].cargo = [begin, begin + l.len() * std::mem::size_of::<usize>(), 0];
        }
    }
    for ((x, y), _, ids) in &f.tiles {
        let tile = &mut w.tiles[(*y * 64 + *x) as usize];
        // the list as printed, first node first
        let mut next = 0;
        for id in ids.iter().rev() {
            w.nodes.push(Box::new(FNode { unit: handles[w.by_id[id]], next }));
            next = addr(&**w.nodes.last().unwrap());
        }
        tile.units = next;
    }
    let current = &mut w.players[f.current];
    current.viewed = if f.viewed >= 0 { handles[w.by_id[&(f.viewed as u32)]] } else { 0 };
    current.selected_viewed = if f.selected_viewed >= 0 { handles[w.by_id[&(f.selected_viewed as u32)]] } else { 0 };
    w
}

fn map_z(x: f32, y: f32) -> f32 {
    x * 0.25 - y * 0.125 + 1.5
}

// ---- the host: the driver's stand-in answers

#[derive(Default)]
struct Out {
    bars: Vec<f32>,
    name: String,
    selected: bool,
    pointed: bool,
    label: Option<String>,
    parts: Option<u64>,
}

struct Host<'a> {
    g: &'a Golden,
    f: &'a KFrame,
    w: &'a World,
    out: Out,
}

impl Host<'_> {
    fn unit_of(&self, handle: u64) -> &KUnit {
        let i = self.w.units.iter().position(|u| addr(&**u) as u64 == handle).expect("a fake unit");
        &self.f.units[i]
    }
    fn class_of(&self, handle: u64) -> &KClass {
        let i = self.w.classes.iter().position(|c| addr(&**c) as u64 == handle).expect("a fake class");
        &self.g.classes[i]
    }
}

impl FillHost for Host<'_> {
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        map_z(x, y)
    }
    fn model_frames(&mut self, object: u64, index: u64) -> u64 {
        assert_eq!(index, 0);
        self.g.model_frames[self.g.object_model0[object as usize - 1]]
    }
    fn water_tile(&mut self, x: c_long, y: c_long) -> bool {
        ((x + 2 * y) % 7 + 7) % 7 == 0
    }
    fn valid_tile(&mut self, x: c_long, y: c_long) -> bool {
        (0..64).contains(&x) && (0..64).contains(&y)
    }
    fn ground_z(&mut self, p: [f32; 3]) -> f32 {
        map_z(p[0], p[1]) - 0.25
    }
    fn physical_scale(&mut self, unit: u64) -> f32 {
        self.unit_of(unit).complex.as_ref().unwrap().scale
    }
    fn physical_frame(&mut self, unit: u64) -> c_ulong {
        self.unit_of(unit).complex.as_ref().unwrap().frame
    }
    fn tint(&mut self, unit: u64) -> EeGfxColor {
        let c = self.unit_of(unit).complex.as_ref().unwrap().color;
        EeGfxColor { red: c[0], green: c[1], blue: c[2] }
    }
    fn side_part(&mut self, unit: u64) -> bool {
        self.unit_of(unit).complex.as_ref().unwrap().side
    }
    fn land_building(&mut self, class: u64) -> bool {
        self.class_of(class).land
    }
    fn ground_location(&mut self, class: u64, p: [f32; 3]) -> [f32; 3] {
        [p[0], p[1], p[2] + self.class_of(class).ground]
    }
    fn group_contains(&mut self, _player: u64, unit: u64) -> bool {
        let id = self.unit_of(unit).id;
        self.f.group.contains(&id)
    }
    fn parts(&mut self, _out: *mut EeGfxFillUnit, unit: u64) {
        self.out.parts = Some(unit);
    }
    fn bars_resize(&mut self, _out: *mut EeGfxFillUnit, count: usize) {
        self.out.bars.resize(count, 0.0);
    }
    fn bars_set(&mut self, _out: *mut EeGfxFillUnit, index: usize, value: f32) {
        self.out.bars[index] = value;
    }
    fn bars_clear(&mut self, _out: *mut EeGfxFillUnit) {
        self.out.bars.clear();
    }
    fn name_clear(&mut self, _out: *mut EeGfxFillUnit) {
        self.out.name.clear();
    }
    fn name_from_stats(&mut self, _out: *mut EeGfxFillUnit, unit: u64) {
        let stats = self.unit_of(unit).stats;
        self.out.name = self.g.stats[stats].name.clone();
    }
    fn select(&mut self, _out: *mut EeGfxFillUnit) {
        self.out.selected = true;
    }
    fn point_at(&mut self, _out: *mut EeGfxFillUnit) {
        self.out.pointed = true;
    }
    fn label(&mut self, _out: *mut EeGfxFillUnit, player: u64) {
        let i = self.w.players.iter().position(|p| addr(&**p) as u64 == player).unwrap();
        let p = &self.g.players[i];
        self.out.label = Some(if !p.human && !p.unchanged { p.civ.clone() } else { p.name.clone() });
    }
    fn morale(&mut self, unit: u64) -> EeGfxFillMorale {
        let effects = &self.unit_of(unit).complex.as_ref().unwrap().effects;
        match effects.iter().find(|(k, _)| *k == 2 || *k == 5 || *k == 6) {
            Some(&(_, s)) => EeGfxFillMorale { found: true, strength: s },
            None => EeGfxFillMorale::default(),
        }
    }
    fn scaffolding(&mut self, player: c_long, class: u64) -> u64 {
        let s = self.class_of(class).scaffolding;
        if s < 0 {
            return 0;
        }
        // the upgrade table by type: the last class of the type wins (as the driver filled it)
        let kind = self.g.classes[s as usize].kind;
        let last = self.g.classes.iter().rposition(|c| c.kind == kind).unwrap();
        self.g.stats[player as usize * 24 + last].model + 1
    }
    fn unit(&mut self, id: c_long) -> u64 {
        u32::try_from(id).ok().and_then(|id| self.w.by_id.get(&id)).map_or(0, |&i| addr(&*self.w.units[i]) as u64)
    }
    fn waypoint(&mut self, carrier: u64, _x: c_long, _y: c_long) -> EeGfxFillPoint {
        match self.unit_of(carrier).complex.as_ref().unwrap().waypoint {
            Some(p) => EeGfxFillPoint { found: true, point: p },
            None => EeGfxFillPoint::default(),
        }
    }
}

// ---- the dumps

/// A U line's render unit: its head and the C++ members the host keeps.
#[derive(Clone)]
struct Entry {
    head: EeGfxFillUnit,
    bars: Vec<f32>,
    turrets: Vec<f32>,
    name: String,
    source_unit: i64,
}

fn zero_head() -> EeGfxFillUnit {
    // SAFETY: a plain record of numbers
    let mut h: EeGfxFillUnit = unsafe { std::mem::zeroed() };
    h.transformation = GeTransformation::default();
    h.transformation.m = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];
    h.player_color = [1.0; 3];
    h.tint_color = [1.0; 3];
    h
}

fn parse_entry(t: &[String], classes: &[usize]) -> Entry {
    let mut h = zero_head();
    for i in 0..16 {
        h.transformation.m[i / 4][i % 4] = hex(&t[2 + i]);
    }
    h.player_color = [hex(&t[18]), hex(&t[19]), hex(&t[20])];
    h.tint_color = [hex(&t[21]), hex(&t[22]), hex(&t[23])];
    let mut i = 24;
    let mut vectors: Vec<Vec<String>> = vec![];
    for _ in 0..7 {
        let count: usize = t[i].trim_start_matches('[').trim_end_matches(']').parse().unwrap();
        vectors.push(t[i + 1..i + 1 + count].to_vec());
        i += 1 + count;
    }
    let name = t[i].trim_matches('\'').to_string();
    let class: i64 = num(&t[i + 1], "c");
    h.info = if class < 0 { 0 } else { classes[class as usize] as u64 };
    let source: i64 = num(&t[i + 2], "o");
    let source2: i64 = num(&t[i + 3], "o");
    h.source = (source + 1) as u64;
    h.source2 = (source2 + 1) as u64;
    let f: Vec<f32> = t[i + 4..i + 14].iter().map(|s| hex(s)).collect();
    (h.scale, h.selection_size, h.shadow_scale, h.ff4, h.ground_z) = (f[0], f[1], f[2], f[3], f[4]);
    (h.opacity, h.health, h.fraction, h.sub_model_offset_scale, h.fog_scale) = (f[5], f[6], f[7], f[8], f[9]);
    let n: Vec<i64> = t[i + 14..i + 27].iter().map(|s| s.parse().unwrap()).collect();
    h.category = n[0] as c_long;
    h.unit_class_index = n[1] as c_ulong;
    h.l118 = n[2] as c_long;
    h.command_type = n[3] as c_long;
    h.current_research = n[4] as c_long;
    h.animation = n[5] as c_long;
    h.grid_size = n[6] as c_long;
    h.current_frame = n[7] as c_ulong;
    h.next_frame = n[8] as c_ulong;
    h.l134 = n[9] as c_long;
    h.unit_id = n[10] as c_ulong;
    h.player_id = n[11] as c_long;
    h.area_effect_strength = n[12] as c_ulong;
    let b = t[i + 27].as_bytes();
    h.can_place = b[0] == b'1';
    h.is_fogged = b[1] == b'1';
    h.is_tinted = b[2] == b'1';
    h.has_sub_model_offset = b[3] == b'1';
    h.second_full_opacity = b[4] == b'1';
    Entry {
        head: h,
        bars: vectors[0].iter().map(|s| hex(s)).collect(),
        turrets: vectors[1].iter().map(|s| hex(s)).collect(),
        name,
        source_unit: num(&t[1], "u"),
    }
}

fn head_words(h: &EeGfxFillUnit) -> Vec<String> {
    let mut v: Vec<String> = h.transformation.m.iter().flatten().map(|f| format!("{:08x}", f.to_bits())).collect();
    v.extend(h.player_color.iter().chain(h.tint_color.iter()).map(|f| format!("{:08x}", f.to_bits())));
    v.push(format!("info {:x} source {:x} source2 {:x}", h.info, h.source, h.source2));
    for f in [
        h.scale,
        h.selection_size,
        h.shadow_scale,
        h.ff4,
        h.ground_z,
        h.opacity,
        h.health,
        h.fraction,
        h.sub_model_offset_scale,
        h.fog_scale,
    ] {
        v.push(format!("{:08x}", f.to_bits()));
    }
    for n in [
        h.category as i64,
        h.unit_class_index as i64,
        h.l118 as i64,
        h.command_type as i64,
        h.current_research as i64,
        h.animation as i64,
        h.grid_size as i64,
        h.current_frame as i64,
        h.next_frame as i64,
        h.l134 as i64,
        h.unit_id as i64,
        h.player_id as i64,
        h.area_effect_strength as i64,
    ] {
        v.push(n.to_string());
    }
    v.push(format!(
        "{} {} {} {} {}",
        h.can_place, h.is_fogged, h.is_tinted, h.has_sub_model_offset, h.second_full_opacity
    ));
    v
}

fn frame_of(f: &KFrame, w: &World) -> EeGfxFillFrame {
    // the yaw: the camera's forward (0, 1, 0), as FillRenderLists turns it
    let m = &f.camera;
    let dx = 0.0 * m[0][2] + 1.0 * m[0][1] + 0.0 * m[0][0];
    let dy = 0.0 * m[1][0] + 0.0 * m[1][2] + 1.0 * m[1][1];
    let yaw = ee_base::math::um::um_arctan2(-dx, dy) - 0.785_398_2;
    let pointed = if f.pointed >= 0 {
        let i = w.by_id[&(f.pointed as u32)];
        if f.units[i].hp > 0 {
            addr(&*w.units[i]) as u64
        } else {
            0
        }
    } else {
        0
    };
    EeGfxFillFrame {
        yaw,
        free_camera: f.free,
        sway_clock: f.sway,
        player: addr(&*w.players[f.current]) as u64,
        world_player: addr(&*w.players[0]) as u64,
        map: addr(&*w.map) as u64,
        picked: f.rect.as_ptr(),
        picked_count: f.rect.len(),
        pointed,
        music_follows_battle: f.music,
        collision_spheres: f.spheres,
        sea_level: f.sea,
        space_turret_height: 0.7,
        space_station_height: 1.5,
    }
}

#[test]
fn the_unit_and_object_fields_replay_the_cpp_bit_for_bit() {
    register_layout();
    let g = parse();
    assert_eq!(g.frames.len(), 40);
    let class_handles_of = |w: &World| -> Vec<usize> { w.classes.iter().map(|c| addr(&**c)).collect() };
    // the pools' slots as last dumped (the C++'s pool keeps a slot's members between frames)
    let mut pool: Vec<Option<Entry>> = vec![None; 0x200];
    let mut objects: Vec<Option<Vec<String>>> = vec![None; 0x600];
    let (mut units_checked, mut objects_checked) = (0, 0);
    for (k, f) in g.frames.iter().enumerate() {
        let w = build(&g, f);
        let classes = class_handles_of(&w);
        let frame = frame_of(f, &w);
        // SAFETY: the fake world lives through the frame
        let t = unsafe { WorldRead::begin() };
        for (i, line) in f.entries.iter().enumerate() {
            let want = parse_entry(line, &classes);
            let filled = !f.skip && want.source_unit >= 0 && want.head.unit_id as i64 == want.source_unit;
            if filled {
                let ki = w.by_id[&(want.source_unit as u32)];
                let unit = unsafe { UnitView::of(&t, addr(&*w.units[ki]) as u64) }.unwrap();
                let mut head = pool[i].as_ref().map_or_else(zero_head, |e| {
                    let mut h = e.head;
                    // the class pointer of the earlier frame's world: the same class
                    h.info = e.head.info;
                    h
                });
                if let Some(prev) = &pool[i] {
                    let class = classes.iter().position(|&c| c as u64 == prev.head.info);
                    head.info = class.map_or(0, |c| classes[c] as u64);
                }
                head.source = want.head.source;
                head.opacity = want.head.opacity;
                // set after the fill (FillRenderLists' robotic farm pass)
                head.second_full_opacity = want.head.second_full_opacity;
                let bars = pool[i].as_ref().map_or(vec![0.0], |e| {
                    let mut b = e.bars.clone();
                    b.resize(1, 0.0);
                    b
                });
                let mut host = Host { g: &g, f, w: &w, out: Out { bars, ..Out::default() } };
                fill_unit_fields(&t, unit, &mut head, &frame, &mut host);
                assert_eq!(head_words(&head), head_words(&want.head), "frame {k} unit entry {i}: {line:?}");
                assert_eq!(
                    host.out.bars.iter().map(|b| b.to_bits()).collect::<Vec<_>>(),
                    want.bars.iter().map(|b| b.to_bits()).collect::<Vec<_>>(),
                    "frame {k} unit {i}'s bars"
                );
                assert_eq!(host.out.name, want.name, "frame {k} unit {i}'s name");
                assert_eq!(host.out.selected, f.selected_units.contains(&i), "frame {k} unit {i} selected");
                assert_eq!(host.out.pointed, f.selected_unit == i as i64, "frame {k} unit {i} pointed at");
                assert_eq!(host.out.label.is_some(), f.debug_unit == i as i64, "frame {k} unit {i} labelled");
                if let Some(text) = &host.out.label {
                    assert_eq!(text, &f.debug_text);
                }
                let c = f.units[ki].complex.as_ref();
                if host.out.parts.is_some() {
                    assert_eq!(&want.turrets, &c.unwrap().turrets, "frame {k} unit {i}'s turrets");
                }
                units_checked += 1;
            }
            pool[i] = Some(want);
        }
        for (i, line) in f.objects.iter().enumerate() {
            let source: i64 = num(&line[1], "u");
            let fogged = line[16] == "1";
            if !f.skip && source >= 0 && !fogged {
                let ki = w.by_id[&(source as u32)];
                let unit = unsafe { UnitView::of(&t, addr(&*w.units[ki]) as u64) }.unwrap();
                let mut out = EeGfxFillObject {
                    location: [0.0; 3],
                    object: num::<u64>(&line[5], "o") + 1,
                    model_scale: 0.0,
                    selection_size: 0.0,
                    scale: 0.0,
                    f1c: 0.0,
                    health: 0.0,
                    opacity: hex(&line[11]),
                    fraction: 0.0,
                    model_index: 0,
                    current_frame: 0,
                    next_frame: 0,
                    is_fogged: true,
                    gap: [0; 3],
                };
                // the slot's earlier selection size (written only for a selected object)
                out.selection_size = objects[i].as_ref().map_or(0.0, |p| hex(&p[7]));
                let mut transformation = GeTransformation::default();
                let mut host = Host { g: &g, f, w: &w, out: Out::default() };
                let selected = fill_object_fields(&t, unit, &mut out, &frame, &mut transformation, &mut host);
                let got = format!(
                    "{:08x} {:08x} {:08x} o{} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {} {} {} {}",
                    out.location[0].to_bits(),
                    out.location[1].to_bits(),
                    out.location[2].to_bits(),
                    out.object - 1,
                    out.model_scale.to_bits(),
                    out.selection_size.to_bits(),
                    out.scale.to_bits(),
                    out.f1c.to_bits(),
                    out.health.to_bits(),
                    out.opacity.to_bits(),
                    out.fraction.to_bits(),
                    out.model_index,
                    out.current_frame,
                    out.next_frame,
                    out.is_fogged as i32
                );
                assert_eq!(got, line[2..].join(" "), "frame {k} object {i}");
                assert_eq!(selected, f.selected_objects.contains(&i), "frame {k} object {i} selected");
                objects_checked += 1;
            }
            objects[i] = Some(line.clone());
        }
    }
    assert!(units_checked > 200 && objects_checked > 40, "{units_checked} units, {objects_checked} objects");
}

#[test]
fn the_effect_readers_and_the_drawn_facts_read_the_golden_world() {
    use crate::gfx::effect_views;
    use crate::gfx::fill::unit_drawn_facts;
    use ee_sim::world::{PlayerView, TileView};
    register_layout();
    let g = parse();
    let mut checked = 0;
    for f in g.frames.iter().take(12) {
        let w = build(&g, f);
        let current = addr(&*w.players[f.current]) as u64;
        for (i, k) in f.units.iter().enumerate() {
            let h = addr(&*w.units[i]) as u64;
            let class = &g.classes[k.class];
            let stats = &g.stats[k.stats];
            // SAFETY: the fake world lives through the test
            unsafe {
                assert_eq!(effect_views::unit_scale(h).to_bits(), stats.scale.to_bits());
                assert_eq!(effect_views::selection_size(h).to_bits(), class.selection.to_bits());
                assert_eq!(effect_views::max_hit_points(h), stats.max as i32);
                assert_eq!(effect_views::unit_class_scale(h).to_bits(), class.scale.to_bits());
                assert_eq!(effect_views::has_complex(h), k.complex.is_some());
                let tile = effect_views::unit_tile(h);
                assert_eq!((tile.size, tile.x, tile.y), (class.tile as f32, k.tile.0 as f32, k.tile.1 as f32));
                let owner = &g.players[k.player];
                let enemy = k.player != f.current && g.players[f.current].enemies[k.player] != 0 && k.player != 0;
                assert_eq!(effect_views::confirm_enemy(h, current), enemy);
                if let Some(c) = &k.complex {
                    assert_eq!(effect_views::unit_location(h).m, c.transform);
                    let color = effect_views::player_color(h);
                    assert_eq!([color.red, color.green, color.blue], owner.color);
                    assert_eq!(effect_views::icons_owner(h, current), k.player == f.current && c.built);
                    let vent = effect_views::vent_unit(h);
                    assert_eq!((vent.dead, vent.max_hit_points), (c.command == 6003, stats.max as f32));
                    let farm = effect_views::farm_unit(h);
                    assert_eq!(
                        (farm.tile_x, farm.tile_y, farm.tile_size, farm.built),
                        (k.tile.0 as i32, k.tile.1 as i32, class.tile as i32, c.built)
                    );
                    assert_eq!(effect_views::market_unbuilt(h), class.kind == 0x321 && !c.built && !c.foundation);
                }
                // the drawn facts of a unit on its tile
                let t = WorldRead::begin();
                let Some(tile_handle) = (w.units[i].tile != 0).then_some(w.units[i].tile as u64) else { continue };
                let unit = UnitView::of(&t, h).unwrap();
                let tile = TileView::of(&t, tile_handle).unwrap();
                let player = PlayerView::of(&t, current).unwrap();
                let facts = unit_drawn_facts(unit, tile, player, f.free, 7, 9);
                let words = w.tiles[(k.tile.1 * 64 + k.tile.0) as usize].words[0];
                assert_eq!(facts.seen, (words >> ((f.current & 1) << 4) & 0xff) > 1);
                assert_eq!(
                    (facts.food, facts.has_complex, facts.category, facts.owner_id, facts.unit_type, facts.tile_size),
                    (
                        k.resource.0 == 2 && k.resource.1 > 0,
                        k.complex.is_some(),
                        class.category as i32,
                        k.player as i32,
                        class.kind as i32,
                        class.tile as i32
                    )
                );
                assert_eq!(
                    (facts.mine, facts.id, facts.unit_count, facts.object_count),
                    (k.player == f.current, k.id as i32, 7, 9)
                );
                assert_eq!(facts.path_marker, class.kind == 0x104 || class.kind == 0xdf);
            }
            checked += 1;
        }
    }
    assert!(checked > 200, "{checked}");
}
