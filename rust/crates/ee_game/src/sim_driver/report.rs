//! EE-SIM's samples and its JSON report (TEST ONLY; the format tools/sim.py, sim_bench.py and the harness read).

use std::fmt::Write as _;
use std::os::raw::{c_long, c_ulong};

use super::options::Options;

/// One player at a sample.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct PlayerSample {
    pub index: c_ulong,
    pub human: bool,
    pub epoch: c_long,
    /// `EEPlayer::mPopulationCount`
    pub population: c_long,
    /// `EEPlayer::mEndOfGameCount`
    pub victory_units: c_long,
    pub citizens: c_long,
    /// live units that are neither citizens nor buildings (priests and heroes too)
    pub army: c_long,
    pub buildings: c_long,
    /// of the army: warships, submarines and naval transports
    pub naval: c_long,
    /// of the army: planes, helicopters and helicopter transports
    pub air: c_long,
    /// of the buildings: the towers and space turrets
    pub towers: c_long,
    /// of the buildings: wall pieces and gates
    pub walls: c_long,
    pub wonders: c_long,
    /// the army's hit points x attack, summed
    pub strength: c_long,
    /// the cost of every live unit and building, all resources summed
    pub value: c_long,
    pub resources: [c_long; 5],
    /// `EEPlayer::mGameResult`: 1 victorious, 2 defeated
    pub result: c_ulong,
}

/// The world at a sample.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Sample {
    pub game_ms: c_long,
    pub pt: c_ulong,
    pub wall: f64,
    pub players: Vec<PlayerSample>,
}

/// A player's defeat.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Defeat {
    pub player: c_ulong,
    pub game_ms: c_long,
    pub pt: c_ulong,
}

/// A lobby combo set by the driver.
#[derive(Clone, Debug, PartialEq)]
pub struct Chosen {
    pub control: c_long,
    pub label: String,
    pub value: String,
    pub line: String,
    pub index: c_long,
}

/// A JSON string: `"` and `\` escaped, every byte below 0x20 or from 0x7f as `\u00XX`.
pub fn json_string(text: &str) -> String {
    let mut out = String::from("\"");
    for &c in text.as_bytes() {
        if c == b'"' || c == b'\\' {
            out.push('\\');
            out.push(c as char);
        } else if !(0x20..0x7f).contains(&c) {
            let _ = write!(out, "\\u{c:04x}");
        } else {
            out.push(c as char);
        }
    }
    out.push('"');
    out
}

/// Everything the report says.
pub struct ReportInput<'a> {
    pub options: &'a Options,
    pub chosen: &'a [Chosen],
    pub outcome: &'a str,
    pub error: &'a str,
    pub wall_seconds: f64,
    /// None: the game never started
    pub wall_seconds_game: Option<f64>,
    pub phases: &'a [(String, f64)],
    pub no_human_defeat_hits: c_long,
    pub samples: &'a [Sample],
    pub defeats: &'a [Defeat],
    /// per world player, the game ms each epoch was first reached (-1: not)
    pub epoch_ms: &'a [Vec<c_long>],
    pub exit_code: i32,
}

fn list(v: &[c_ulong]) -> String {
    let items: Vec<String> = v.iter().map(|x| x.to_string()).collect();
    format!("[{}]", items.join(", "))
}

fn players(s: &Sample) -> String {
    let mut o = String::from("[");
    for (k, p) in s.players.iter().enumerate() {
        let _ = write!(
            o,
            "{}{{\"index\": {}, \"human\": {}, \"result\": {}, \"epoch\": {}, \"population\": {}, \
             \"victory_units\": {}, \"citizens\": {}, \"army\": {}, \"buildings\": {}, \
             \"naval\": {}, \"air\": {}, \"towers\": {}, \"walls\": {}, \"wonders\": {}, \
             \"strength\": {}, \"value\": {}, \"resources\": {{\"food\": {}, \"wood\": {}, \"stone\": {}, \"gold\": {}, \"iron\": {}}}}}",
            if k > 0 { ", " } else { "" },
            p.index,
            p.human,
            p.result,
            p.epoch,
            p.population,
            p.victory_units,
            p.citizens,
            p.army,
            p.buildings,
            p.naval,
            p.air,
            p.towers,
            p.walls,
            p.wonders,
            p.strength,
            p.value,
            p.resources[0],
            p.resources[1],
            p.resources[2],
            p.resources[3],
            p.resources[4]
        );
    }
    o + "]"
}

/// The report's text.
pub fn report(r: &ReportInput) -> String {
    let o = r.options;
    let mut j = String::from("{\n");
    j += "  \"test_only\": true,\n  \"kind\": \"ee-sim-v1\",\n";
    let sets: Vec<String> = o.sets.iter().map(|s| json_string(s)).collect();
    let _ = write!(
        j,
        "  \"setup\": {{\"seed\": {}, \"computers\": {}, \"game_seconds\": {}, \"pt_rate\": {}, \"no_human_defeat\": {}, \
         \"human_out\": {}, \"cheats\": {}, \"personality\": {}, \"engine\": {}, \"sets\": [{}],\n    \"lobby\": [",
        o.seed,
        o.computers,
        o.game_seconds as c_long,
        o.pt_rate,
        o.no_human_defeat,
        o.human_out,
        json_string(if o.cheats.is_empty() { "all" } else { &o.cheats }),
        json_string(&o.personality),
        json_string(if o.engine.is_empty() { "classic" } else { &o.engine }),
        sets.join(", ")
    );
    for (k, c) in r.chosen.iter().enumerate() {
        let _ = write!(
            j,
            "{}{{\"control\": {}, \"value\": {}, \"index\": {}, \"line\": {}}}",
            if k > 0 { ", " } else { "" },
            c.control,
            json_string(&c.value),
            c.index,
            json_string(&c.line)
        );
    }
    j += "]},\n";
    let _ = writeln!(j, "  \"outcome\": {},", json_string(if r.error.is_empty() { r.outcome } else { "error" }));
    if !r.error.is_empty() {
        let _ = writeln!(j, "  \"error\": {},", json_string(r.error));
    }
    let _ = write!(
        j,
        "  \"wall_seconds\": {:.2},\n  \"wall_seconds_game\": {:.2},\n",
        r.wall_seconds,
        r.wall_seconds_game.unwrap_or(0.0)
    );
    j += "  \"wall_phases\": {";
    for (k, (name, seconds)) in r.phases.iter().enumerate() {
        let _ = write!(j, "{}\"{}\": {:.2}", if k > 0 { ", " } else { "" }, name, seconds);
    }
    j += "},\n";
    let last = r.samples.last();
    let _ = writeln!(j, "  \"game_ms\": {},", last.map_or(0, |s| s.game_ms));
    let _ = writeln!(j, "  \"pt\": {},", last.map_or(0, |s| s.pt));
    let _ = writeln!(j, "  \"no_human_defeat_hits\": {},", r.no_human_defeat_hits);
    let mut alive = Vec::new();
    let mut winners = Vec::new();
    for p in last.map_or(&[][..], |s| &s.players[..]) {
        if !p.human && p.victory_units > 0 {
            alive.push(p.index);
        }
        if p.result == 1 {
            winners.push(p.index);
        }
    }
    let _ = write!(j, "  \"alive_computers\": {},\n  \"winners\": {},\n", list(&alive), list(&winners));
    let winner = if r.outcome == "last-computer-standing" && alive.len() == 1 {
        alive[0].to_string()
    } else {
        "null".to_string()
    };
    let _ = writeln!(j, "  \"winner\": {winner},");
    j += "  \"defeat_order\": [";
    for (k, d) in r.defeats.iter().enumerate() {
        let _ = write!(
            j,
            "{}{{\"player\": {}, \"game_ms\": {}, \"pt\": {}}}",
            if k > 0 { ", " } else { "" },
            d.player,
            d.game_ms,
            d.pt
        );
    }
    j += "],\n";
    let _ = writeln!(j, "  \"players\": {},", last.map_or_else(|| "[]".to_string(), players));
    // per world player, the game ms each epoch was first reached (index: the epoch; an epoch never reached is missing
    // from the end)
    j += "  \"epoch_ms\": {";
    let mut first = true;
    for (i, times) in r.epoch_ms.iter().enumerate().skip(1) {
        if times.is_empty() {
            continue;
        }
        let items: Vec<String> = times.iter().map(|t| t.to_string()).collect();
        let _ = write!(j, "{}\"{}\": [{}]", if first { "" } else { ", " }, i, items.join(", "));
        first = false;
    }
    j += "},\n";
    j += "  \"samples\": [\n";
    for (k, s) in r.samples.iter().enumerate() {
        let _ = write!(
            j,
            "    {{\"game_ms\": {}, \"pt\": {}, \"wall\": {:.2}, \"players\": {}{}",
            s.game_ms,
            s.pt,
            s.wall,
            players(s),
            if k + 1 < r.samples.len() { "},\n" } else { "}\n" }
        );
    }
    let _ = write!(j, "  ],\n  \"exit_code\": {}\n}}\n", r.exit_code);
    j
}

/// The progress line of a sample (" p1 e1 c5 a0 b1 ..." per player).
pub fn sample_line(s: &Sample) -> String {
    let mut line = String::new();
    for p in &s.players {
        let _ = write!(line, " p{} e{} c{} a{} b{}", p.index, p.epoch, p.citizens, p.army, p.buildings);
    }
    line
}
