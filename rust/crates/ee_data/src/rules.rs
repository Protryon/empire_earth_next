//! The rules documents (docs/DATA_FORMATS.md 8.8): the game rules that were literals in the code, read from combat.yaml,
//! gameoptions.yaml, civbuilder.yaml, civpowers.yaml, aipersonalities.yaml and epochs.yaml's media. A key left out keeps
//! the default (the schema, tools/eedata/rules_schema.json, is the single source of the fields and defaults); a
//! document on the data path sets every value it holds. `rules_gen.rs` holds the generated structs, snapshot and dump.
//! The semantic checks here are the C++ `EERules.cpp`'s, message for message (`eedata.py cppgate` compares them).
#![allow(clippy::neg_cmp_op_on_partial_ord)] // a NaN must fail as it does in the C++ (`!(x >= 0.0)`)

use crate::content::Content;
use crate::fail;
use crate::generated::rules_gen::*;
use crate::msg;
use crate::msg::{At, Part};
use crate::node::{Kind, Node};
use crate::spec::table;
use crate::text::{self, node_bool, node_float, node_int, node_scalar, parse_yaml, Res};
use std::sync::Mutex;

/// A number that is a constant or one of dbworld's values (`EERuleNumber`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RuleNumber {
    pub float: f32,
    pub long: i32,
    pub world_row: i32,
    pub world_value: i32,
}

impl Default for RuleNumber {
    fn default() -> Self {
        RuleNumber { float: 0.0, long: 0, world_row: -1, world_value: 0 }
    }
}

/// `(long)f` on the x86-64 C++ builds: the 32-bit truncation, INT_MIN when out of range (Rust's `as` saturates).
fn x86_long(f: f32) -> i32 {
    if f.is_nan() || f <= -2147483904.0 || f >= 2147483648.0 {
        i32::MIN
    } else {
        f as i32
    }
}

impl RuleNumber {
    pub fn constant(v: f32) -> RuleNumber {
        RuleNumber { float: v, long: x86_long(v), ..RuleNumber::default() }
    }
    pub fn integer(v: i32) -> RuleNumber {
        RuleNumber { float: v as f32, long: v, ..RuleNumber::default() }
    }
    pub fn world(row: i32, value: i32) -> RuleNumber {
        RuleNumber { world_row: row, world_value: value, ..RuleNumber::default() }
    }
}

// the snapshot's primitives (rules_gen.rs; EERulesGenDecode.inl reads them back)
pub fn enc_int(out: &mut Vec<u8>, v: i32) {
    out.extend_from_slice(&v.to_le_bytes());
}
pub fn enc_float(out: &mut Vec<u8>, v: f32) {
    out.extend_from_slice(&v.to_bits().to_le_bytes());
}
pub fn enc_bool(out: &mut Vec<u8>, v: bool) {
    out.push(v as u8);
}
pub fn enc_len(out: &mut Vec<u8>, n: usize) {
    enc_int(out, n as i32);
}
pub fn enc_string(out: &mut Vec<u8>, v: &[u8]) {
    enc_len(out, v.len());
    out.extend_from_slice(v);
}
pub fn enc_number(out: &mut Vec<u8>, n: &RuleNumber) {
    enc_float(out, n.float);
    enc_int(out, n.long);
    enc_int(out, n.world_row);
    enc_int(out, n.world_value);
}

/// All the rules (`EERules`). Every field starts at its default (the schema); `load` applies the documents on the path.
#[derive(Clone, Debug, PartialEq)]
pub struct Rules {
    pub combat: CombatRules,
    pub game_options: GameOptionRules,
    pub civ_builder: CivBuilderRules,
    pub civ_powers: CivPowerRules,
    pub ai_personalities: AIPersonalityRules,
    pub epochs: Vec<EpochMedia>,
    pub epoch_music_from_text: bool,
}

impl Default for Rules {
    fn default() -> Self {
        Rules {
            combat: CombatRules::default(),
            game_options: GameOptionRules::default(),
            civ_builder: CivBuilderRules::default(),
            civ_powers: CivPowerRules::default(),
            ai_personalities: AIPersonalityRules::default(),
            epochs: (0..=K_TECH_EPOCH_SPACE).map(default_epoch_media).collect(),
            epoch_music_from_text: false,
        }
    }
}

/// The original's per-epoch citizen rules (EEPlayUnitEffect, EEUGGather, EEUnitClass::Attack); epoch positions
/// (kTechEpoch*); effects: lightning (unit) 0x15, dust 0xab, disintegration 0xec (dbgfxeffects rows).
pub fn default_epoch_media(epoch: i32) -> EpochMedia {
    let mut m = EpochMedia { high_tech_tools: epoch >= K_TECH_EPOCH_NANO, ..EpochMedia::default() };
    if epoch <= K_TECH_EPOCH_BRONZE {
        m.removal_effects = vec![0x15, 0xab, 0xab, 0xab];
        m.removal_thunder_every = 2;
    } else if epoch <= K_TECH_EPOCH_INDUSTRIAL {
        m.removal_effects = vec![0xec, 0xab];
        m.removal_thunder_every = 3;
    } else if epoch <= K_TECH_EPOCH_ATOMIC_MODERN {
        m.removal_effects = vec![0xec, 0xab];
        m.removal_pitch_offset = -0.2;
    } else {
        m.removal_effects = vec![0xec, 0xab];
    }
    m
}

fn to_i32(v: i64) -> i32 {
    v as i32
}

// ------------------------------------------------------------------------------------------------ reading

/// A mapping read key by key; a key nobody asked for is an error.
struct Map<'a> {
    node: &'a Node,
    doc: &'a str,
    used: Vec<bool>,
}

impl<'a> Map<'a> {
    fn new(node: &'a Node, doc: &'a str, what: &str) -> Res<Map<'a>> {
        if node.kind != Kind::Map {
            fail!(where_of(doc, node), ": ", what, " must be a mapping");
        }
        Ok(Map { node, doc, used: vec![false; node.keys.len()] })
    }
    /// The place of a value of this mapping, written only into a message.
    fn where_at(&self, at: &Node) -> At<'a> {
        where_of(self.doc, at)
    }
    fn where_(&self) -> At<'a> {
        where_of(self.doc, self.node)
    }
    fn get(&mut self, key: &str) -> Option<&'a Node> {
        for i in 0..self.node.keys.len() {
            if self.node.keys[i] == key.as_bytes() {
                self.used[i] = true;
                let v = &self.node.items[i];
                return if v.is_missing() { None } else { Some(v) };
            }
        }
        None
    }
    fn done(&self) -> Res<()> {
        for i in 0..self.used.len() {
            if !self.used[i] {
                fail!(self.where_at(&self.node.items[i]), ": unknown key '", &self.node.keys[i], "'");
            }
        }
        Ok(())
    }
    fn int(&mut self, key: &str, value: &mut i32) -> Res<()> {
        if let Some(v) = self.get(key) {
            *value = to_i32(node_int(Some(v), &self.where_at(v), key)?);
        }
        Ok(())
    }
    fn float(&mut self, key: &str, value: &mut f32) -> Res<()> {
        if let Some(v) = self.get(key) {
            *value = node_float(Some(v), &self.where_at(v), key)?;
        }
        Ok(())
    }
    fn bool(&mut self, key: &str, value: &mut bool) -> Res<()> {
        if let Some(v) = self.get(key) {
            *value = node_bool(Some(v), &self.where_at(v), key)?;
        }
        Ok(())
    }
    fn number(&mut self, key: &str, value: &mut RuleNumber) -> Res<()> {
        let Some(v) = self.get(key) else { return Ok(()) };
        let w = self.where_at(v);
        let t = node_scalar(Some(v), &w, key)?;
        if !t.is_empty() && t[0].is_ascii_lowercase() && t != b"inf" && t != b"nan" {
            let (row, world_value) = world_value(t, &w, key)?;
            *value = RuleNumber::world(row, world_value);
        } else if !t.iter().any(|c| matches!(c, b'.' | b'e' | b'E')) || t.starts_with(b"0x") {
            *value = RuleNumber::integer(to_i32(node_int(Some(v), &w, key)?));
        } else {
            *value = RuleNumber::constant(node_float(Some(v), &w, key)?);
        }
        Ok(())
    }
    fn list(&mut self, key: &str) -> Res<Option<&'a Node>> {
        let v = self.get(key);
        if let Some(n) = v {
            if n.kind != Kind::List {
                fail!(self.where_at(n), ": ", key, " must be a list");
            }
        }
        Ok(v)
    }
    fn sub(&mut self, key: &str) -> Res<Option<&'a Node>> {
        let v = self.get(key);
        if let Some(n) = v {
            if n.kind != Kind::Map {
                fail!(self.where_at(n), ": ", key, " must be a mapping");
            }
        }
        Ok(v)
    }
}

fn where_of<'a>(doc: &'a str, at: &Node) -> At<'a> {
    At { what: doc, line: at.line }
}

fn named<W: Part + ?Sized>(value: Option<&Node>, where_: &W, key: &str, names: &[&str], what: &str) -> Res<i32> {
    let t = node_scalar(value, where_, key)?;
    if let Some(i) = names.iter().position(|n| n.as_bytes() == t) {
        return Ok(i as i32);
    }
    fail!(where_, ": ", key, ": '", t, "' is not ", what);
}

fn category<W: Part + ?Sized>(value: Option<&Node>, where_: &W, key: &str) -> Res<i32> {
    named(value, where_, key, UNIT_CATEGORY_NAMES, "a unit category")
}

fn categories(m: &mut Map, key: &str, current: &[i32]) -> Res<Vec<i32>> {
    let Some(v) = m.list(key)? else { return Ok(current.to_vec()) };
    let mut out = Vec::with_capacity(v.items.len());
    for x in &v.items {
        out.push(category(Some(x), &m.where_at(x), key)?);
    }
    Ok(out)
}

fn refs(m: &mut Map, c: &mut Content, key: &str, table_name: &str, current: &[i32]) -> Res<Vec<i32>> {
    let Some(v) = m.list(key)? else { return Ok(current.to_vec()) };
    let mut out = Vec::with_capacity(v.items.len());
    for x in &v.items {
        out.push(to_i32(c.key_of(table_name, Some(x), &m.where_at(x), key)?));
    }
    Ok(out)
}

fn reference(m: &mut Map, c: &mut Content, key: &str, table_name: &str, default: i32) -> Res<i32> {
    match m.get(key) {
        Some(v) => Ok(to_i32(c.key_of(table_name, Some(v), &m.where_at(v), key)?)),
        None => Ok(default),
    }
}

fn entry_id(m: &mut Map) -> Res<Vec<u8>> {
    let v = m.get("id");
    let w = m.where_();
    let t = node_scalar(v, &w, "id")?;
    if !text::is_id(t) {
        fail!(&w, ": id '", t, "': an id is a lowercase letter, then a-z, 0-9 or _");
    }
    Ok(t.to_vec())
}

fn default_of<T>(m: &mut Map, key: &str, list: &[T], id: impl Fn(&T) -> &[u8], default: i32) -> Res<i32> {
    let Some(v) = m.get(key) else { return Ok(default) };
    let w = m.where_at(v);
    let t = node_scalar(Some(v), &w, key)?;
    if let Some(i) = list.iter().position(|e| id(e) == t) {
        return Ok(i as i32);
    }
    fail!(&w, ": ", key, ": '", t, "' is not an entry of the list");
}

fn check_unique<T, W: Part + ?Sized>(list: &[T], id: impl Fn(&T) -> &[u8], where_: &W, what: &str) -> Res<()> {
    for i in 0..list.len() {
        for j in 0..i {
            if id(&list[i]) == id(&list[j]) {
                fail!(where_, ": duplicate ", what, " id '", id(&list[i]), "'");
            }
        }
    }
    Ok(())
}

fn head(m: &mut Map) {
    m.get("eedata");
    m.get("format");
}

fn load_combat(c: &mut Content, d: &Node, r: &mut CombatRules) -> Res<()> {
    let mut m = Map::new(d, "combat", "the document")?;
    head(&mut m);
    if let Some(s) = m.sub("damage")? {
        let mut x = Map::new(s, "combat", "damage")?;
        x.int("minimum", &mut r.minimum_damage)?;
        x.number("fanatic_factor", &mut r.fanatic_factor)?;
        x.float("elevation_step", &mut r.elevation_step)?;
        x.float("ally_factor", &mut r.ally_factor)?;
        x.float("herd_ally_factor", &mut r.herd_ally_factor)?;
        x.float("dead_food_share", &mut r.dead_food_share)?;
        x.int("trample_damage", &mut r.trample_damage)?;
        x.float("trample_threshold", &mut r.trample_threshold)?;
        x.done()?;
    }
    if let Some(s) = m.sub("elevation")? {
        let mut x = Map::new(s, "combat", "elevation")?;
        r.elevation_attackers = categories(&mut x, "attackers", &r.elevation_attackers)?;
        r.elevation_launchers = categories(&mut x, "launchers", &r.elevation_launchers)?;
        r.elevation_targets = categories(&mut x, "targets", &r.elevation_targets)?;
        x.done()?;
    }
    if let Some(s) = m.sub("morale")? {
        let mut x = Map::new(s, "combat", "morale")?;
        x.float("positive_factor", &mut r.positive_morale)?;
        x.float("negative_factor", &mut r.negative_morale)?;
        r.morale_buildings = refs(&mut x, c, "buildings", "dbobjects", &r.morale_buildings)?;
        x.done()?;
    }
    if let Some(s) = m.sub("ticks")? {
        let mut x = Map::new(s, "combat", "ticks")?;
        x.int("regeneration", &mut r.regeneration_interval)?;
        x.int("power_regeneration", &mut r.power_regeneration_interval)?;
        x.done()?;
    }
    m.done()
}

fn load_game_options(d: &Node, r: &mut GameOptionRules) -> Res<()> {
    const DOC: &str = "gameoptions";
    let mut m = Map::new(d, DOC, "the document")?;
    head(&mut m);
    if let Some(v) = m.list("map_sizes")? {
        r.map_sizes.clear();
        for e in &v.items {
            let mut x = Map::new(e, DOC, "a map size")?;
            let mut s = MapSizeRule { id: entry_id(&mut x)?, ..MapSizeRule::default() };
            x.int("name", &mut s.name_id)?;
            x.int("tiles", &mut s.tiles)?;
            x.int("script_size", &mut s.script_size)?;
            if s.tiles < 1 {
                fail!(x.where_(), ": tiles must be at least 1");
            }
            if s.script_size < 0 || s.script_size > 5 {
                fail!(x.where_(), ": script_size is a random map script size class, 0 .. 5");
            }
            x.done()?;
            r.map_sizes.push(s);
        }
        check_unique(&r.map_sizes, |s| &s.id, &m.where_at(v), "map size")?;
        if r.map_sizes.is_empty() {
            fail!(m.where_at(v), ": map_sizes is empty");
        }
    }
    r.map_size_default = default_of(&mut m, "map_size_default", &r.map_sizes, |s| &s.id, r.map_size_default)?;
    if r.map_size_default as usize >= r.map_sizes.len() {
        fail!(m.where_(), ": map_size_default: name one of the map_sizes");
    }
    if let Some(v) = m.list("speeds")? {
        r.speeds.clear();
        for e in &v.items {
            let mut x = Map::new(e, DOC, "a speed")?;
            let mut s = SpeedRule { id: entry_id(&mut x)?, ..SpeedRule::default() };
            x.int("name", &mut s.name_id)?;
            x.number("factor", &mut s.factor)?;
            x.done()?;
            r.speeds.push(s);
        }
        check_unique(&r.speeds, |s| &s.id, &m.where_at(v), "speed")?;
        if r.speeds.is_empty() {
            fail!(m.where_at(v), ": speeds is empty");
        }
    }
    r.speed_default = default_of(&mut m, "speed_default", &r.speeds, |s| &s.id, r.speed_default)?;
    if r.speed_default as usize >= r.speeds.len() {
        fail!(m.where_(), ": speed_default: name one of the speeds");
    }
    if let Some(v) = m.list("difficulties")? {
        r.difficulties.clear();
        for e in &v.items {
            let mut x = Map::new(e, DOC, "a difficulty")?;
            let mut s = DifficultyRule { id: entry_id(&mut x)?, ..DifficultyRule::default() };
            x.int("name", &mut s.name_id)?;
            x.int("ai_level", &mut s.ai_level)?;
            if s.ai_level < 0 || s.ai_level > 2 {
                fail!(x.where_(), ": ai_level is 0 (easy), 1 (medium) or 2 (hard): the computer players' records");
            }
            x.number("damage_scale_human_target", &mut s.scale_human_target)?;
            x.number("damage_scale_human_attacker", &mut s.scale_human_attacker)?;
            if let Some(c) = x.list("ai_cheats")? {
                s.ai_cheats = 0;
                for n in &c.items {
                    let w = x.where_at(n);
                    let t = node_scalar(Some(n), &w, "ai_cheats")?;
                    let bit = ai_cheat_bit(t);
                    if bit == 0 {
                        fail!(
                            &w,
                            ": ai_cheats: '",
                            t,
                            "' is not cost, reveal, epochs, extra, popband, catchup or retire"
                        );
                    }
                    s.ai_cheats |= bit;
                }
            }
            x.done()?;
            r.difficulties.push(s);
        }
        check_unique(&r.difficulties, |s| &s.id, &m.where_at(v), "difficulty")?;
        if r.difficulties.is_empty() {
            fail!(m.where_at(v), ": difficulties is empty");
        }
    }
    if let Some(s) = m.sub("unit_limit")? {
        let mut x = Map::new(s, DOC, "unit_limit")?;
        x.number("minimum", &mut r.unit_limit_minimum)?;
        x.number("maximum", &mut r.unit_limit_maximum)?;
        x.int("step", &mut r.unit_limit_step)?;
        if r.unit_limit_step < 1 {
            fail!(x.where_(), ": step must be at least 1");
        }
        x.number("default", &mut r.unit_limit_default)?;
        x.done()?;
    }
    if let Some(s) = m.sub("wonders_for_victory")? {
        let mut x = Map::new(s, DOC, "wonders_for_victory")?;
        x.number("maximum", &mut r.wonders_maximum)?;
        x.int("default", &mut r.wonders_default)?;
        x.done()?;
    }
    m.bool("defeat_without_recovery", &mut r.defeat_without_recovery)?;
    m.done()
}

/// The bit of an AI cheat name; 0: not a cheat name.
pub fn ai_cheat_bit(name: &[u8]) -> i32 {
    match AI_CHEAT_NAMES.iter().position(|n| n.as_bytes() == name) {
        Some(i) => 1 << i,
        None => 0,
    }
}

fn load_civ_builder(c: &mut Content, d: &Node, r: &mut CivBuilderRules) -> Res<()> {
    const DOC: &str = "civbuilder";
    let mut m = Map::new(d, DOC, "the document")?;
    head(&mut m);
    if let Some(v) = m.list("attributes")? {
        r.attributes.clear();
        for e in &v.items {
            let mut x = Map::new(e, DOC, "an attribute")?;
            let mut a = CivAttribute { id: entry_id(&mut x)?, ..CivAttribute::default() };
            a.level = 0;
            x.int("level", &mut a.level)?;
            if a.level < 1 || a.level > 0xffff {
                fail!(x.where_(), ": level must be 1 .. 65535");
            }
            a.name_id = 0;
            x.int("name", &mut a.name_id)?;
            for b in &r.attributes {
                if b.id == a.id || b.level == a.level {
                    fail!(x.where_(), ": attribute '", &a.id, "': its id or level ", a.level, " is also '", &b.id, "'");
                }
            }
            if let Some(l) = x.list("applies")? {
                for ap in &l.items {
                    let mut y = Map::new(ap, DOC, "an apply")?;
                    let mut p = CivApply::default();
                    let stat = y.get("stat");
                    p.stat = named(stat, &y.where_(), "stat", STAT_NAMES, "a stat")?;
                    if let Some(mode) = y.get("mode") {
                        p.mode = named(Some(mode), &y.where_at(mode), "mode", MODE_NAMES, "a mode")?;
                    }
                    if p.mode != K_CIV_MODE_SHARE
                        && (p.stat == K_CIV_STAT_HIT_POINTS || p.stat == K_CIV_STAT_ARMOR || p.stat == K_CIV_STAT_POWER)
                    {
                        fail!(y.where_(), ": ", STAT_NAMES[p.stat as usize], " has only the share mode");
                    }
                    y.int("slot", &mut p.slot)?;
                    if (p.stat == K_CIV_STAT_GATHER_TIME) != (p.slot >= 0) {
                        fail!(y.where_(), ": slot belongs to gather_time (0 .. 6), and only there");
                    }
                    if p.slot > 6 {
                        fail!(y.where_(), ": slot is a gather time slot, 0 .. 6");
                    }
                    if let Some(only) = y.get("only_category") {
                        p.only_category = category(Some(only), &y.where_at(only), "only_category")?;
                    }
                    if let Some(except) = y.get("except_category") {
                        p.except_category = category(Some(except), &y.where_at(except), "except_category")?;
                    }
                    p.power = reference(&mut y, c, "power", "dbcivpowers", -1)?;
                    if (p.stat == K_CIV_STAT_POWER) != (p.power >= 0) {
                        fail!(y.where_(), ": power names the civ power of stat power, and only there");
                    }
                    y.done()?;
                    a.applies.push(p);
                }
            }
            x.done()?;
            r.attributes.push(a);
        }
    }
    if let Some(s) = m.sub("powers_category")? {
        let mut x = Map::new(s, DOC, "powers_category")?;
        if x.get("id").is_some() {
            r.powers_id = entry_id(&mut x)?;
        }
        x.int("name", &mut r.powers_name_id)?;
        r.powers_unit_set = reference(&mut x, c, "unit_set", "dbunitset", r.powers_unit_set)?;
        x.number("cost", &mut r.powers_cost)?;
        x.float("level_value", &mut r.powers_level_value)?;
        x.done()?;
    }
    m.done()
}

fn load_civ_categories(d: &Node, r: &mut CivBuilderRules) -> Res<()> {
    let Some(recs) = d.find(b"records").filter(|n| n.kind == Kind::List) else { return Ok(()) };
    r.categories.clear();
    let mut next: i64 = 0;
    let rows = d.find(b"rows").filter(|n| !n.is_missing());
    if let Some(rows) = rows {
        next = node_int(Some(rows), b"dbcivilization", "rows")?;
    }
    for e in &recs.items {
        if e.kind != Kind::Map {
            continue;
        }
        let where_ = At { what: "dbcivilization", line: e.line };
        let mut t = CivCategoryText::default();
        let row = e.find(b"row").filter(|n| !n.is_missing());
        let row_value = match row {
            Some(n) => node_int(Some(n), &where_, "row")?,
            None => next,
        };
        t.row = to_i32(row_value);
        next = next.max(row_value + 1);
        t.id = e.find(b"id").filter(|n| n.kind == Kind::Scalar).map_or(Vec::new(), |n| n.text.to_vec());
        if let Some(levels) = e.find(b"levels") {
            if levels.kind != Kind::List && !levels.is_missing() {
                fail!(&where_, ": levels must be a list");
            }
            for l in &levels.items {
                let mut x = Map::new(l, "dbcivilization", "a level")?;
                let attribute = x.get("attribute");
                let name = node_scalar(attribute, &x.where_(), "attribute")?;
                let Some(a) = r.attributes.iter().find(|a| a.id == name) else {
                    fail!(x.where_(), ": attribute: '", name, "' is not an attribute of civbuilder");
                };
                let level = a.level as usize;
                let mut value = 0.0f32;
                let mut bonus = 0.0f32;
                x.float("value", &mut value)?;
                x.float("bonus", &mut bonus)?;
                x.done()?;
                if t.values.len() < level {
                    t.values.resize(level, 0.0);
                    t.bonuses.resize(level, 0.0);
                }
                t.values[level - 1] = value;
                t.bonuses[level - 1] = bonus;
            }
        }
        r.categories.push(t);
    }
    Ok(())
}

fn load_civ_powers(c: &mut Content, d: &Node, r: &mut CivPowerRules) -> Res<()> {
    const DOC: &str = "civpowers";
    let mut m = Map::new(d, DOC, "the document")?;
    head(&mut m);
    if let Some(v) = m.list("powers")? {
        r.powers.clear();
        for e in &v.items {
            let mut x = Map::new(e, DOC, "a power")?;
            let mut p =
                CivPowerRule { power: reference(&mut x, c, "power", "dbcivpowers", -1)?, ..CivPowerRule::default() };
            if p.power < 0 {
                fail!(x.where_(), ": power: missing");
            }
            if r.powers.iter().any(|q| q.power == p.power) {
                fail!(x.where_(), ": a second entry for the same power");
            }
            if let Some(l) = x.list("effects")? {
                for ef in &l.items {
                    let mut y = Map::new(ef, DOC, "an effect")?;
                    let mut f = CivPowerEffect::default();
                    let act = y.get("do");
                    f.action = named(act, &y.where_(), "do", ACTION_NAMES, "a power effect")?;
                    f.tech = reference(&mut y, c, "tech", "dbtechtree", -1)?;
                    f.if_tech = reference(&mut y, c, "if_researched", "dbtechtree", -1)?;
                    f.sound = reference(&mut y, c, "sound", "dbsounds", -1)?;
                    f.effect = reference(&mut y, c, "effect", "dbgfxeffects", -1)?;
                    f.unit_type = reference(&mut y, c, "unit_type", "dbobjects", -1)?;
                    let need_tech = f.action == K_POWER_RESEARCH_IF_RESEARCHED
                        || f.action == K_POWER_ALLOW_TECH
                        || f.action == K_POWER_DISALLOW_TECH;
                    if need_tech != (f.tech >= 0)
                        || (f.action == K_POWER_RESEARCH_IF_RESEARCHED) != (f.if_tech >= 0)
                        || (f.action == K_POWER_LOAD_SOUND) != (f.sound >= 0)
                        || (f.action == K_POWER_LOAD_EFFECT) != (f.effect >= 0)
                        || (f.action == K_POWER_PULSE_SIGHT) != (f.unit_type >= 0)
                    {
                        let takes = if f.action == K_POWER_RESEARCH_IF_RESEARCHED {
                            "if_researched and tech"
                        } else if need_tech {
                            "tech"
                        } else if f.action == K_POWER_LOAD_SOUND {
                            "sound"
                        } else if f.action == K_POWER_LOAD_EFFECT {
                            "effect"
                        } else if f.action == K_POWER_PULSE_SIGHT {
                            "unit_type"
                        } else {
                            "nothing"
                        };
                        fail!(y.where_(), ": ", ACTION_NAMES[f.action as usize], " takes ", takes);
                    }
                    y.done()?;
                    p.effects.push(f);
                }
            }
            x.done()?;
            r.powers.push(p);
        }
    }
    m.done()
}

fn load_epochs(c: &mut Content, d: &Node, r: &mut Rules) -> Res<()> {
    let Some(recs) = d.find(b"records").filter(|n| n.kind == Kind::List) else { return Ok(()) };
    let mut out = Vec::with_capacity(recs.items.len());
    for (k, e) in recs.items.iter().enumerate() {
        let where_ = At { what: "epochs", line: e.line };
        let mut m = default_epoch_media(k as i32);
        if e.kind != Kind::Map {
            fail!(&where_, ": an epoch must be a mapping");
        }
        if let Some(music) = e.find(b"music") {
            if !music.is_missing() {
                if music.kind != Kind::List {
                    fail!(&where_, ": music must be a list");
                }
                for s in &music.items {
                    m.music.push(to_i32(c.key_of(
                        "dbmusic",
                        Some(s),
                        &At { what: "epochs", line: s.line },
                        "music",
                    )?));
                }
            }
        }
        if let Some(cit) = e.find(b"citizen") {
            if !cit.is_missing() {
                let mut x = Map::new(cit, "epochs", "citizen")?;
                x.bool("high_tech_tools", &mut m.high_tech_tools)?;
                m.removal_effects = refs(&mut x, c, "removal_effects", "dbgfxeffects", &m.removal_effects)?;
                m.removal_sound = reference(&mut x, c, "removal_sound", "dbsounds", m.removal_sound)?;
                x.float("removal_pitch_offset", &mut m.removal_pitch_offset)?;
                m.removal_thunder = reference(&mut x, c, "removal_thunder", "dbsounds", m.removal_thunder)?;
                x.int("removal_thunder_every", &mut m.removal_thunder_every)?;
                if m.removal_thunder_every < 0 {
                    fail!(x.where_(), ": removal_thunder_every must be 0 (never) or more");
                }
                x.done()?;
            }
        }
        out.push(m);
    }
    r.epochs = out;
    r.epoch_music_from_text = true;
    Ok(())
}

fn load_ai_personalities(d: &Node, r: &mut AIPersonalityRules) -> Res<()> {
    const DOC: &str = "aipersonalities";
    let mut m = Map::new(d, DOC, "the document")?;
    head(&mut m);
    if let Some(v) = m.list("personalities")? {
        r.personalities.clear();
        for e in &v.items {
            let mut x = Map::new(e, DOC, "a personality")?;
            let mut p = AIPersonalityRule { id: entry_id(&mut x)?, ..AIPersonalityRule::default() };
            // next_script: obsolete (the AI manifest Data/ai/ais.json names the next engine's scripts);
            // read and ignored, so the documents of older data releases still load
            let _ = x.get("next_script");
            if let Some(s) = x.sub("epoch_pressure")? {
                let mut y = Map::new(s, DOC, "epoch_pressure")?;
                y.float("start", &mut p.epoch_pressure_start)?;
                y.float("full", &mut p.epoch_pressure_full)?;
                y.float("stone_span", &mut p.stone_span)?;
                y.float("safe_bonus", &mut p.safe_bonus)?;
                y.done()?;
                if !(p.epoch_pressure_start >= 0.0 && p.epoch_pressure_full > p.epoch_pressure_start) {
                    fail!(y.where_(), ": epoch_pressure: 0 <= start < full");
                }
                if !(p.stone_span >= 0.0) {
                    fail!(y.where_(), ": epoch_pressure: stone_span must be 0 or more");
                }
                if !(p.safe_bonus >= 0.0) {
                    fail!(y.where_(), ": epoch_pressure: safe_bonus must be 0 or more");
                }
            }
            if let Some(s) = x.sub("economy")? {
                let mut y = Map::new(s, DOC, "economy")?;
                y.int("surplus", &mut p.surplus)?;
                y.float("surplus_cut", &mut p.surplus_cut)?;
                y.int("low_stock", &mut p.low_stock)?;
                if let Some(f) = y.sub("floor")? {
                    let mut z = Map::new(f, DOC, "floor")?;
                    const NAMES: [&str; 5] = ["food", "wood", "stone", "gold", "iron"];
                    for (i, name) in NAMES.iter().enumerate() {
                        z.float(name, &mut p.floor[i])?;
                    }
                    z.done()?;
                }
                y.float("max_citizen_share", &mut p.max_citizen_share)?;
                y.int("wood_slots", &mut p.wood_slots)?;
                y.int("moves_per_cycle", &mut p.moves_per_cycle)?;
                y.bool("demand_split", &mut p.demand_split)?;
                y.bool("coast_fishing", &mut p.coast_fishing)?;
                y.bool("forage_patches", &mut p.forage_patches)?;
                y.bool("claim_needed", &mut p.claim_needed)?;
                y.int("early_town_centers", &mut p.early_town_centers)?;
                y.int("early_trainers", &mut p.early_trainers)?;
                y.bool("patch_expand", &mut p.patch_expand)?;
                y.done()?;
                if p.early_town_centers < 0 || p.early_trainers < 0 {
                    fail!(y.where_(), ": economy: early_town_centers and early_trainers must be 0 or more");
                }
            }
            if let Some(s) = x.sub("military")? {
                let mut y = Map::new(s, DOC, "military")?;
                y.float("standing_army", &mut p.standing_army)?;
                y.float("army_floor", &mut p.army_floor)?;
                y.bool("militia", &mut p.militia)?;
                y.bool("militia_towers", &mut p.militia_towers)?;
                y.int("prehistoric_army", &mut p.prehistoric_army)?;
                y.bool("prehistoric_attacks", &mut p.prehistoric_attacks)?;
                y.bool("stone_attacks", &mut p.stone_attacks)?;
                y.bool("naval_flee", &mut p.naval_flee)?;
                y.done()?;
                if !(p.standing_army >= 0.0) {
                    fail!(y.where_(), ": military: standing_army must be 0 or more");
                }
                if p.prehistoric_army < 0 {
                    fail!(y.where_(), ": military: prehistoric_army must be 0 or more");
                }
            }
            x.done()?;
            r.personalities.push(p);
        }
        check_unique(&r.personalities, |p| &p.id, &m.where_at(v), "personality")?;
        if r.personalities.is_empty() {
            fail!(m.where_at(v), ": personalities is empty");
        }
    }
    if let Some(v) = m.list("assign")? {
        r.assign.clear();
        for e in &v.items {
            let w = m.where_at(e);
            let t = node_scalar(Some(e), &w, "assign")?;
            let Some(k) = r.personalities.iter().position(|p| p.id == t) else {
                fail!(&w, ": assign: '", t, "' is not a personality");
            };
            r.assign.push(k as i32);
        }
    }
    m.done()
}

/// A dbworld value named `<id>.<F|F2|L|L2>` -> its row and value (0 F .. 3 L2).
pub fn world_value<W: Part + ?Sized>(text_value: &[u8], where_: &W, name: &str) -> Res<(i32, i32)> {
    const VALUES: [&[u8]; 4] = [b"F", b"F2", b"L", b"L2"];
    const FIELDS: [&str; 4] = ["mFloatValue", "mFloatValue2", "mLongValue", "mLongValue2"];
    let dot = text_value.iter().rposition(|&c| c == b'.');
    let id = &text_value[..dot.unwrap_or(text_value.len())];
    let value: &[u8] = dot.map_or(&[], |d| &text_value[d + 1..]);
    for rn in table("dbworld").row_names {
        if id != rn.name.as_bytes() {
            continue;
        }
        for (v, want) in VALUES.iter().enumerate() {
            if value != *want {
                continue;
            }
            if !rn.fields.split(' ').any(|f| f == FIELDS[v]) {
                fail!(
                    where_,
                    ": ",
                    name,
                    ": '",
                    text_value,
                    "': dbworld's ",
                    id,
                    " has no ",
                    value,
                    " (its readers use ",
                    rn.fields,
                    ")"
                );
            }
            return Ok((rn.row, v as i32));
        }
        fail!(where_, ": ", name, ": '", text_value, "': a world value is <id>.F, .F2, .L or .L2");
    }
    fail!(where_, ": ", name, ": '", text_value, "' is neither a number nor a dbworld value (<id>.F ...)");
}

impl Rules {
    /// Reads the rules documents and epochs.yaml's media (references by id, through the Content). A document the
    /// Content has no text for keeps its defaults.
    pub fn load(&mut self, c: &mut Content) -> Res<()> {
        if let Some(d) = c.document("combat")? {
            load_combat(c, &d, &mut self.combat)?;
        }
        if let Some(d) = c.document("gameoptions")? {
            load_game_options(&d, &mut self.game_options)?;
        }
        if let Some(d) = c.document("civbuilder")? {
            load_civ_builder(c, &d, &mut self.civ_builder)?;
        }
        if let Some(d) = c.document("dbcivilization")? {
            load_civ_categories(&d, &mut self.civ_builder)?;
        }
        if let Some(d) = c.document("civpowers")? {
            load_civ_powers(c, &d, &mut self.civ_powers)?;
        }
        if let Some(d) = c.document("epochs")? {
            load_epochs(c, &d, self)?;
        }
        if let Some(d) = c.document("aipersonalities")? {
            load_ai_personalities(&d, &mut self.ai_personalities)?;
        }
        Ok(())
    }

    /// The songs from dbmusic's per-epoch flags (the binary load).
    pub fn set_music_from_flags(&mut self, song: i32, flags: &[bool]) {
        if self.epoch_music_from_text {
            return;
        }
        let count = flags.len();
        for k in 0..self.epochs.len() {
            // the space age (past the flags) plays the last flag's songs (the original's EEMusicObject ctor)
            if count > 0 && k <= count && flags[if k < count { k } else { count - 1 }] {
                self.epochs[k].music.push(song);
            }
        }
    }

    /// The snapshot the C++ `EERules` reads (ee/EERulesGen.h, EERulesGenDecode.inl).
    pub fn encode(&self) -> Vec<u8> {
        let mut out = Vec::new();
        encode_top(self, &mut out);
        out
    }

    /// The content dump of the rules (`"rules.<doc>":{...}` lines, values resolved).
    pub fn dump(&self) -> Vec<u8> {
        let mut s: Vec<u8> = Vec::new();
        {
            let mut j = Json::new(&mut s);
            j.raw(b"\"rules.combat\":{");
            self.combat.dump(&mut j);
            j.raw(b"}\n");
        }
        {
            let mut j = Json::new(&mut s);
            j.raw(b"\"rules.gameoptions\":{");
            self.game_options.dump(&mut j);
            j.raw(b"}\n");
        }
        {
            let mut j = Json::new(&mut s);
            j.raw(b"\"rules.civbuilder\":{");
            self.civ_builder.dump(&mut j);
            // the levels past the record's 60 (only dbcivilization.yaml can have them; the rest are in its dump)
            j.open(Some(b"levels_past_60"), b'[');
            for c in &self.civ_builder.categories {
                for k in 60..c.values.len() {
                    if c.values[k] == 0.0 && c.bonuses[k] == 0.0 {
                        continue;
                    }
                    j.open(None, b'{');
                    j.int(b"row", c.row);
                    j.int(b"level", k as i32 + 1);
                    j.float(b"value", c.values[k]);
                    j.float(b"bonus", c.bonuses[k]);
                    j.close(b'}');
                }
            }
            j.close(b']');
            j.raw(b"}\n");
        }
        {
            let mut j = Json::new(&mut s);
            j.raw(b"\"rules.civpowers\":{");
            self.civ_powers.dump(&mut j);
            j.raw(b"}\n");
        }
        {
            let mut j = Json::new(&mut s);
            j.raw(b"\"rules.epochs\":[");
            for m in &self.epochs {
                j.open(None, b'{');
                m.dump(&mut j);
                j.close(b'}');
            }
            j.raw(b"]\n");
        }
        {
            let mut j = Json::new(&mut s);
            j.raw(b"\"rules.aipersonalities\":{");
            self.ai_personalities.dump(&mut j);
            j.raw(b"}\n");
        }
        s
    }
}

/// The dump's JSON writer (the C++ `Json` struct, byte for byte).
pub struct Json<'a> {
    out: &'a mut Vec<u8>,
    first: bool,
}

impl<'a> Json<'a> {
    pub fn new(out: &'a mut Vec<u8>) -> Json<'a> {
        Json { out, first: true }
    }
    pub fn raw(&mut self, text: &[u8]) {
        self.out.extend_from_slice(text);
    }
    fn key(&mut self, key: &[u8]) {
        if !self.first {
            self.out.push(b',');
        }
        self.first = false;
        self.out.push(b'"');
        self.out.extend_from_slice(key);
        self.out.extend_from_slice(b"\":");
    }
    pub fn int(&mut self, key: &[u8], value: i32) {
        self.key(key);
        self.out.extend_from_slice(value.to_string().as_bytes());
    }
    /// A float as its bits (as a C++ 32-bit `long`: the dump is signed on the Windows builds this gate pins).
    pub fn float(&mut self, key: &[u8], value: f32) {
        self.int(key, value.to_bits() as i32);
    }
    pub fn string(&mut self, key: &[u8], value: &[u8]) {
        self.key(key);
        self.out.push(b'"');
        self.out.extend_from_slice(value);
        self.out.push(b'"');
    }
    pub fn number(&mut self, key: &[u8], n: &RuleNumber) {
        self.key(key);
        const VALUES: [&str; 4] = ["F", "F2", "L", "L2"];
        if n.world_row >= 0 {
            self.out.extend_from_slice(
                format!("{{\"world\":{},\"value\":\"{}\"}}", n.world_row, VALUES[(n.world_value & 3) as usize])
                    .as_bytes(),
            );
        } else {
            self.out.extend_from_slice(format!("{{\"float\":{},\"long\":{}}}", n.float.to_bits(), n.long).as_bytes());
        }
    }
    pub fn list(&mut self, key: &[u8], values: &[i32]) {
        self.key(key);
        self.out.push(b'[');
        for (i, v) in values.iter().enumerate() {
            if i > 0 {
                self.out.push(b',');
            }
            self.out.extend_from_slice(v.to_string().as_bytes());
        }
        self.out.push(b']');
    }
    pub fn open(&mut self, key: Option<&[u8]>, bracket: u8) {
        if let Some(k) = key {
            self.key(k);
        } else if !self.first {
            self.out.push(b',');
        }
        self.out.push(bracket);
        self.first = true;
    }
    pub fn close(&mut self, bracket: u8) {
        self.out.push(bracket);
        self.first = false;
    }
}

// ------------------------------------------------------------------------------------- the test overrides

/// TEST ONLY (EE-SIM.exe --set): a rules document's value changed as the document is read.
struct RuleOverride {
    doc: Vec<u8>,
    path: Vec<Vec<u8>>,
    value: Vec<u8>,
    problem: Vec<u8>,
    applied: bool,
}

static OVERRIDES: Mutex<Vec<RuleOverride>> = Mutex::new(Vec::new());

/// "doc.key.key=value"; false with the reason when the spec is malformed.
pub fn add_override(spec: &[u8]) -> Result<(), Vec<u8>> {
    let eq = spec.iter().position(|&c| c == b'=');
    let eq = match eq {
        Some(e) if e != 0 => e,
        _ => return Err(msg!("'", spec, "': expected doc.key...=value")),
    };
    let value = spec[eq + 1..].to_vec();
    let parts: Vec<Vec<u8>> = spec[..eq].split(|&c| c == b'.').map(<[u8]>::to_vec).collect();
    if parts.iter().any(Vec::is_empty) {
        return Err(msg!("'", spec, "': an empty key"));
    }
    if parts.len() < 2 {
        return Err(msg!("'", spec, "': expected doc.key...=value"));
    }
    OVERRIDES.lock().expect("the override list").push(RuleOverride {
        doc: parts[0].clone(),
        path: parts[1..].to_vec(),
        value,
        problem: b"its document was not read".to_vec(),
        applied: false,
    });
    Ok(())
}

/// The overrides that changed nothing (document not read, key absent), each with the reason.
pub fn override_problems() -> Vec<Vec<u8>> {
    OVERRIDES
        .lock()
        .expect("the override list")
        .iter()
        .filter(|o| !o.applied)
        .map(|o| msg!(&o.doc, ": ", &o.problem))
        .collect()
}

#[cfg(test)]
pub fn clear_overrides() {
    OVERRIDES.lock().expect("the override list").clear();
}

/// a map's child by key, or a list's (or a map's list child's) entry by id
fn override_step<'a>(node: &'a mut Node, key: &[u8]) -> Option<&'a mut Node> {
    if node.kind == Kind::Map {
        if let Some(i) = node.keys.iter().position(|k| k == key) {
            return Some(&mut node.items[i]);
        }
    }
    fn by_id<'a>(list: &'a mut Node, key: &[u8]) -> Option<&'a mut Node> {
        let found = list.items.iter().position(|e| {
            e.kind == Kind::Map && e.find(b"id").is_some_and(|id| id.kind == Kind::Scalar && id.text == key)
        })?;
        Some(&mut list.items[found])
    }
    if node.kind == Kind::List {
        return by_id(node, key);
    }
    if node.kind == Kind::Map {
        let list_index = (0..node.items.len()).find(|&i| {
            node.items[i].kind == Kind::List
                && node.items[i].items.iter().any(|e| {
                    e.kind == Kind::Map && e.find(b"id").is_some_and(|id| id.kind == Kind::Scalar && id.text == key)
                })
        });
        if let Some(i) = list_index {
            return by_id(&mut node.items[i], key);
        }
    }
    None
}

/// Applies the overrides of this document, as it is read.
pub fn apply_overrides(name: &str, doc: &mut Node) {
    let mut overrides = OVERRIDES.lock().expect("the override list");
    for o in overrides.iter_mut() {
        if o.doc != name.as_bytes() {
            continue;
        }
        let mut where_ = o.doc.clone();
        let mut node: Option<&mut Node> = Some(doc);
        for k in &o.path {
            where_.push(b'.');
            where_.extend_from_slice(k);
            node = node.and_then(|n| override_step(n, k));
            if node.is_none() {
                break;
            }
        }
        let Some(n) = node else {
            o.problem = msg!(&where_, ": not in the document");
            continue;
        };
        if n.kind == Kind::List && o.value.first() == Some(&b'[') {
            // a list value ("[a, b]", "[]") replaces a list (e.g. a difficulty's ai_cheats)
            let parsed = parse_yaml(&msg!("value: ", &o.value, "\n"));
            let replacement = match &parsed {
                Ok(p) => p.find(b"value").filter(|v| v.kind == Kind::List).cloned(),
                Err(_) => None,
            };
            let Some(v) = replacement else {
                o.problem = msg!(&where_, ": not a list: ", &o.value);
                continue;
            };
            *n = v;
            o.applied = true;
            o.problem.clear();
            continue;
        }
        if n.kind != Kind::Scalar {
            o.problem = msg!(&where_, ": not a single value");
            continue;
        }
        n.text = o.value.clone().into();
        n.quoted = false;
        o.applied = true;
        o.problem.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::content::Content;
    use std::collections::HashMap;

    fn content(files: &[(&str, String)]) -> Content {
        let map: HashMap<String, Vec<u8>> =
            files.iter().map(|(n, t)| (n.to_string(), t.clone().into_bytes())).collect();
        Content::new(Box::new(move |n: &str| map.get(n).cloned()))
    }

    fn fail_of(r: Res<()>) -> String {
        String::from_utf8_lossy(&r.unwrap_err().text).into_owned()
    }

    #[test]
    fn defaults_are_the_schemas() {
        let r = Rules::default();
        assert_eq!(r.combat.minimum_damage, 1);
        assert_eq!(r.combat.fanatic_factor, RuleNumber::world(88, 1));
        assert_eq!(r.combat.elevation_attackers, vec![5, 6]);
        assert_eq!(r.game_options.map_sizes.len(), 6);
        assert_eq!(r.game_options.map_sizes[1].tiles, 58);
        assert_eq!(r.game_options.speeds[1].factor, RuleNumber::world(81, 1));
        assert_eq!(r.civ_builder.attributes.len(), 45);
        assert_eq!(r.civ_builder.attributes[0].id, b"attack");
        assert_eq!(r.civ_builder.attributes[22].level, 0x26); // flaming_arrows
        assert_eq!(r.civ_powers.powers.len(), 9);
        assert_eq!(r.ai_personalities.personalities.len(), 5);
        assert_eq!(r.epochs.len(), 15);
        assert_eq!(r.epochs[0].removal_effects, vec![0x15, 0xab, 0xab, 0xab]);
        assert_eq!(r.epochs[13].removal_effects, vec![0xec, 0xab]);
        assert!(r.epochs[13].high_tech_tools);
        assert_eq!(r.epochs[9].removal_pitch_offset, -0.2);
    }

    #[test]
    fn default_epoch_brackets() {
        assert_eq!(default_epoch_media(3).removal_thunder_every, 2);
        assert_eq!(default_epoch_media(4).removal_thunder_every, 3);
        assert_eq!(default_epoch_media(8).removal_thunder_every, 3);
        assert_eq!(default_epoch_media(9).removal_thunder_every, 0);
        assert!(default_epoch_media(99).high_tech_tools);
    }

    #[test]
    fn rule_numbers() {
        assert_eq!(RuleNumber::constant(1.5).long, 1);
        assert_eq!(RuleNumber::constant(-1.5).long, -1);
        assert_eq!(RuleNumber::constant(3e9).long, i32::MIN);
        assert_eq!(RuleNumber::constant(f32::NAN).long, i32::MIN);
        assert_eq!(RuleNumber::integer(5).float, 5.0);
        assert_eq!(RuleNumber::world(1, 2).world_row, 1);
    }

    #[test]
    fn combat_document_sets_values_and_keeps_defaults() {
        let doc =
            "eedata: combat\nformat: 3\ndamage:\n  minimum: 3\n  fanatic_factor: 1.5\nticks:\n  regeneration: 500\n"
                .to_string();
        let mut c = content(&[("combat", doc)]);
        let mut r = Rules::default();
        r.load(&mut c).unwrap();
        assert_eq!(r.combat.minimum_damage, 3);
        assert_eq!(r.combat.fanatic_factor, RuleNumber::constant(1.5));
        assert_eq!(r.combat.regeneration_interval, 500);
        assert_eq!(r.combat.power_regeneration_interval, 5000);
        assert_eq!(r.combat.elevation_attackers, vec![5, 6]);
    }

    #[test]
    fn number_forms() {
        let doc = "damage:\n  fanatic_factor: 2\nelevation:\n  attackers: [land]\n".to_string();
        let mut c = content(&[("combat", doc)]);
        let mut r = Rules::default();
        r.load(&mut c).unwrap();
        assert_eq!(r.combat.fanatic_factor, RuleNumber::integer(2));
        assert_eq!(r.combat.elevation_attackers, vec![5]);
    }

    #[test]
    fn errors_name_the_place() {
        for (doc, want) in [
            ("damage:\n  colour: 1\n", "combat line 2: unknown key 'colour'"),
            ("damage:\n  minimum: x\n", "combat line 2: minimum: 'x' is not an integer"),
            ("damage: 3\n", "combat line 1: damage must be a mapping"),
            ("elevation:\n  attackers: [nope]\n", "combat line 2: attackers: 'nope' is not a unit category"),
            ("nosuch: 1\n", "combat line 1: unknown key 'nosuch'"),
        ] {
            let mut c = content(&[("combat", doc.to_string())]);
            assert_eq!(fail_of(Rules::default().load(&mut c)), want);
        }
    }

    #[test]
    fn wrong_document_name_and_format() {
        let mut c = content(&[("combat", "eedata: nope\n".to_string())]);
        assert_eq!(fail_of(Rules::default().load(&mut c)), "combat: the file says it is 'nope'");
        let mut c = content(&[("combat", "format: 2\n".to_string())]);
        assert_eq!(fail_of(Rules::default().load(&mut c)), "combat: format 2, the game reads format 3");
    }

    #[test]
    fn game_options_validation() {
        for (doc, want) in [
            ("map_sizes:\n  - id: a\n    tiles: 0\n", "gameoptions line 2: tiles must be at least 1"),
            (
                "map_sizes:\n  - id: a\n    tiles: 3\n    script_size: 9\n",
                "gameoptions line 2: script_size is a random map script size class, 0 .. 5",
            ),
            (
                "map_size_default: gigantic2\n",
                "gameoptions line 1: map_size_default: 'gigantic2' is not an entry of the list",
            ),
            (
                "difficulties:\n  - id: a\n    ai_level: 7\n",
                "gameoptions line 2: ai_level is 0 (easy), 1 (medium) or 2 (hard): the computer players' records",
            ),
            (
                "difficulties:\n  - id: a\n    ai_cheats: [nope]\n",
                "gameoptions line 3: ai_cheats: 'nope' is not cost, reveal, epochs, extra, popband, catchup or retire",
            ),
            ("speeds:\n  - id: a\n  - id: a\n", "gameoptions line 2: duplicate speed id 'a'"),
            ("unit_limit:\n  step: 0\n", "gameoptions line 2: step must be at least 1"),
            ("map_sizes: []\n", "gameoptions line 1: map_sizes is empty"),
        ] {
            let mut c = content(&[("gameoptions", doc.to_string())]);
            assert_eq!(fail_of(Rules::default().load(&mut c)), want, "{doc}");
        }
    }

    #[test]
    fn ai_cheats_become_bits() {
        let doc = "difficulties:\n  - id: a\n    ai_level: 1\n    ai_cheats: [cost, retire]\n".to_string();
        let mut c = content(&[("gameoptions", doc)]);
        let mut r = Rules::default();
        r.load(&mut c).unwrap();
        assert_eq!(r.game_options.difficulties[0].ai_cheats, 1 | 1 << 6);
        assert_eq!(ai_cheat_bit(b"reveal"), 2);
        assert_eq!(ai_cheat_bit(b"nope"), 0);
    }

    #[test]
    fn personalities_validation() {
        for (doc, want) in [
            ("personalities:\n  - id: a\n    epoch_pressure:\n      start: 0.5\n      full: 0.2\n", "aipersonalities line 4: epoch_pressure: 0 <= start < full"),
            ("personalities:\n  - id: a\n    economy:\n      early_trainers: -1\n", "aipersonalities line 4: economy: early_town_centers and early_trainers must be 0 or more"),
            ("personalities:\n  - id: a\n    military:\n      prehistoric_army: -1\n", "aipersonalities line 4: military: prehistoric_army must be 0 or more"),
            ("assign: [nope]\n", "aipersonalities line 1: assign: 'nope' is not a personality"),
        ] {
            let mut c = content(&[("aipersonalities", doc.to_string())]);
            assert_eq!(fail_of(Rules::default().load(&mut c)), want, "{doc}");
        }
    }

    #[test]
    fn next_script_is_obsolete_and_ignored() {
        // the documents of older data releases name a script per personality; it loads and is ignored
        let doc = "personalities:\n  - id: a\n    next_script: a..b\n".to_string();
        let mut c = content(&[("aipersonalities", doc)]);
        let mut r = Rules::default();
        r.load(&mut c).unwrap();
        assert_eq!(r.ai_personalities.personalities[0].id, b"a");
    }

    #[test]
    fn world_values() {
        // row names come from the specs: a value the row's readers do not use is rejected
        let (row, v) = world_value(b"fanaticism.F2", b"w", "n").unwrap();
        assert_eq!(v, 1);
        assert!(row >= 0);
        assert_eq!(
            world_value(b"no_such.F", b"w", "n").unwrap_err().text,
            b"w: n: 'no_such.F' is neither a number nor a dbworld value (<id>.F ...)".to_vec()
        );
        assert_eq!(
            world_value(b"fanaticism.X", b"w", "n").unwrap_err().text,
            b"w: n: 'fanaticism.X': a world value is <id>.F, .F2, .L or .L2".to_vec()
        );
    }

    #[test]
    fn music_from_flags() {
        let mut r = Rules::default();
        let mut flags = vec![false; 14];
        flags[13] = true;
        r.set_music_from_flags(7, &flags);
        assert_eq!(r.epochs[13].music, vec![7]);
        assert_eq!(r.epochs[14].music, vec![7]); // the space age plays the last flag's songs
        assert!(r.epochs[12].music.is_empty());
        let mut r = Rules { epoch_music_from_text: true, ..Rules::default() };
        r.set_music_from_flags(7, &flags);
        assert!(r.epochs[13].music.is_empty());
    }

    #[test]
    fn json_writer_commas_and_brackets() {
        let mut s = Vec::new();
        let mut j = Json::new(&mut s);
        j.raw(b"{");
        j.int(b"a", 1);
        j.open(Some(b"b"), b'[');
        j.open(None, b'{');
        j.int(b"x", 2);
        j.close(b'}');
        j.open(None, b'{');
        j.close(b'}');
        j.close(b']');
        j.float(b"f", -0.0);
        j.string(b"s", b"t");
        j.number(b"n", &RuleNumber::world(3, 1));
        j.number(b"m", &RuleNumber::constant(1.5));
        j.list(b"l", &[1, 2]);
        j.raw(b"}");
        assert_eq!(
            String::from_utf8(s).unwrap(),
            "{\"a\":1,\"b\":[{\"x\":2},{}],\"f\":-2147483648,\"s\":\"t\",\"n\":{\"world\":3,\"value\":\"F2\"},\"m\":{\"float\":1069547520,\"long\":1},\"l\":[1,2]}"
        );
    }

    #[test]
    fn dump_shape() {
        let r = Rules::default();
        let d = String::from_utf8(r.dump()).unwrap();
        let lines: Vec<&str> = d.lines().collect();
        assert_eq!(lines.len(), 6);
        assert!(lines[0].starts_with("\"rules.combat\":{\"minimum_damage\":1,"));
        assert!(lines[2].contains("\"levels_past_60\":[]"));
        assert!(lines[4].starts_with("\"rules.epochs\":[{\"music\":[],"));
        assert!(lines[5].ends_with("\"assign\":[0]}"));
    }

    #[test]
    fn snapshot_encodes_every_section() {
        let r = Rules::default();
        let a = r.encode();
        let mut r2 = r.clone();
        r2.combat.minimum_damage = 9;
        assert_ne!(a, r2.encode());
        assert!(a.len() > 1000);
    }

    #[test]
    fn overrides_apply_and_report() {
        clear_overrides();
        assert_eq!(add_override(b"combat").unwrap_err(), b"'combat': expected doc.key...=value".to_vec());
        assert_eq!(add_override(b"combat.=1").unwrap_err(), b"'combat.=1': an empty key".to_vec());
        add_override(b"combat.damage.minimum=8").unwrap();
        add_override(b"combat.damage.nosuch=8").unwrap();
        add_override(b"gameoptions.easy.ai_cheats=[cost]").unwrap();
        let mut c = content(&[
            ("combat", "damage:\n  minimum: 1\n".to_string()),
            ("gameoptions", "difficulties:\n  - id: easy\n    ai_level: 1\n    ai_cheats: [retire]\n".to_string()),
        ]);
        let mut r = Rules::default();
        r.load(&mut c).unwrap();
        assert_eq!(r.combat.minimum_damage, 8);
        assert_eq!(r.game_options.difficulties[0].ai_cheats, 1);
        assert_eq!(override_problems(), vec![b"combat: combat.damage.nosuch: not in the document".to_vec()]);
        clear_overrides();
    }
}
