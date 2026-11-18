//! EE-SIM's options (TEST ONLY): the command line, a `--games` line's words and the game options they share, with
//! the C library's number parsing the C++ driver used (`atol`, `atof`, `strtoul(.., 0)`).

use std::os::raw::{c_long, c_ulong};

/// One game's options (the C++ driver's `Options`; the defaults are the oracle's game).
#[derive(Clone, Debug, PartialEq)]
pub struct Options {
    pub seed: c_ulong,
    pub computers: c_long,
    pub map_size: String,
    pub map_type: String,
    pub difficulty: String,
    /// `--lobby CONTROL=VALUE`
    pub lobby: Vec<(c_long, String)>,
    pub game_seconds: f64,
    pub sample_seconds: f64,
    pub wall_timeout: f64,
    pub pt_rate: c_long,
    pub no_human_defeat: bool,
    /// `--keep-human`: off
    pub human_out: bool,
    /// the JSON report ("" or "-": stdout)
    pub report: String,
    /// the EE-AI lines ("" stdout)
    pub trace: String,
    /// every debug string ("" none)
    pub log: String,
    /// the per-CT checksum lines ("" none)
    pub checksum_trace: String,
    /// `--set doc.key=value`
    pub sets: Vec<String>,
    pub cheats: String,
    pub personality: String,
    /// `--engine`: the computer seats' engines ("" the lobby's Classic)
    pub engine: String,
    /// -1: no fixed ports
    pub port_offset: c_long,
    pub calamities: bool,
    pub missile_sweep: bool,
    pub effect_sweep: bool,
    pub unit_sweep: bool,
    pub app_sweep: bool,
    pub group_sweep: bool,
    /// the game's autosave period (0: none)
    pub autosave_ms: c_long,
    /// `--save-at MS=FILE`, in game-ms order
    pub saves: Vec<(c_long, String)>,
    pub load_save: String,
    pub scenario: String,
    /// learning, roman, pacific, asian ("" none)
    pub campaign: String,
    /// with `--campaign`; -1 the preselection
    pub scenario_index: c_long,
    pub editor_script: String,
    pub editor_out: String,
    pub quiet: bool,
    /// `--flare SECONDS:PLAYER`: -1 none
    pub flare_ms: c_long,
    pub flare_player: c_ulong,
    /// `--volcano SECONDS:PLAYER`: -1 none
    pub volcano_ms: c_long,
    pub volcano_player: c_ulong,
    /// `--games`: the game's stderr ("" the process's)
    pub err: String,
    /// `--games`: the game's name in the batch's markers
    pub id: String,
}

impl Default for Options {
    fn default() -> Self {
        Options {
            seed: 2,
            computers: 2,
            map_size: "Small".into(),
            map_type: String::new(),
            difficulty: "Hard".into(),
            lobby: Vec::new(),
            game_seconds: 2700.0,
            sample_seconds: 30.0,
            wall_timeout: 3600.0,
            pt_rate: 40,
            no_human_defeat: true,
            human_out: true,
            report: String::new(),
            trace: String::new(),
            log: String::new(),
            checksum_trace: String::new(),
            sets: Vec::new(),
            cheats: String::new(),
            personality: String::new(),
            engine: String::new(),
            port_offset: -1,
            calamities: false,
            missile_sweep: false,
            effect_sweep: false,
            unit_sweep: false,
            app_sweep: false,
            group_sweep: false,
            autosave_ms: 0,
            saves: Vec::new(),
            load_save: String::new(),
            scenario: String::new(),
            campaign: String::new(),
            scenario_index: -1,
            editor_script: String::new(),
            editor_out: String::new(),
            quiet: false,
            flare_ms: -1,
            flare_player: 0,
            volcano_ms: -1,
            volcano_player: 0,
            err: String::new(),
            id: String::new(),
        }
    }
}

fn skip_space(b: &[u8]) -> usize {
    b.iter().take_while(|&&c| c == b' ' || (b'\t'..=b'\r').contains(&c)).count()
}

/// The C library's `atol`: leading blanks, a sign, decimal digits; 0 for none (saturated at the type's range).
pub fn c_atol(text: &str) -> c_long {
    let b = text.as_bytes();
    let mut i = skip_space(b);
    let negative = i < b.len() && b[i] == b'-';
    if i < b.len() && (b[i] == b'-' || b[i] == b'+') {
        i += 1;
    }
    let mut v: i128 = 0;
    while i < b.len() && b[i].is_ascii_digit() {
        v = (v * 10 + i128::from(b[i] - b'0')).min(i128::from(c_long::MAX) + 1);
        i += 1;
    }
    let v = if negative { -v } else { v };
    v.clamp(i128::from(c_long::MIN), i128::from(c_long::MAX)) as c_long
}

/// The C library's `strtoul(text, NULL, 0)`: blanks, a sign (a minus negates, wrapping), then `0x` hex, a leading
/// `0` octal, else decimal; `ULONG_MAX` on overflow.
pub fn c_strtoul0(text: &str) -> c_ulong {
    let b = text.as_bytes();
    let mut i = skip_space(b);
    let negative = i < b.len() && b[i] == b'-';
    if i < b.len() && (b[i] == b'-' || b[i] == b'+') {
        i += 1;
    }
    let mut base = 10u32;
    if i + 1 < b.len()
        && b[i] == b'0'
        && (b[i + 1] == b'x' || b[i + 1] == b'X')
        && b.get(i + 2).is_some_and(|c| c.is_ascii_hexdigit())
    {
        base = 16;
        i += 2;
    } else if i < b.len() && b[i] == b'0' {
        base = 8;
    }
    let mut v: u128 = 0;
    let mut overflow = false;
    while i < b.len() {
        let Some(d) = (b[i] as char).to_digit(base) else { break };
        v = v * u128::from(base) + u128::from(d);
        if v > u128::from(c_ulong::MAX) {
            overflow = true;
            v = u128::from(c_ulong::MAX);
        }
        i += 1;
    }
    if overflow {
        return c_ulong::MAX;
    }
    let v = v as c_ulong;
    if negative {
        v.wrapping_neg()
    } else {
        v
    }
}

/// The C library's `atof` for decimal text: blanks, a sign, digits, a point, digits, an exponent (the longest such
/// prefix, correctly rounded); 0 for none.
pub fn c_atof(text: &str) -> f64 {
    let b = text.as_bytes();
    let start = skip_space(b);
    let mut i = start;
    if i < b.len() && (b[i] == b'-' || b[i] == b'+') {
        i += 1;
    }
    let digits_from = i;
    while i < b.len() && b[i].is_ascii_digit() {
        i += 1;
    }
    let mut mantissa = i - digits_from;
    if i < b.len() && b[i] == b'.' {
        i += 1;
        let f = i;
        while i < b.len() && b[i].is_ascii_digit() {
            i += 1;
        }
        mantissa += i - f;
    }
    if mantissa == 0 {
        return 0.0;
    }
    if i < b.len() && (b[i] == b'e' || b[i] == b'E') {
        let mut j = i + 1;
        if j < b.len() && (b[j] == b'-' || b[j] == b'+') {
            j += 1;
        }
        let e = j;
        while j < b.len() && b[j].is_ascii_digit() {
            j += 1;
        }
        if j > e {
            i = j;
        }
    }
    let mut prefix = text[start..i].to_string();
    if prefix.ends_with('.') {
        prefix.push('0');
    }
    if prefix.starts_with('.') || prefix.starts_with("-.") || prefix.starts_with("+.") {
        prefix = prefix.replacen('.', "0.", 1);
    }
    prefix.parse::<f64>().unwrap_or(0.0)
}

/// A path made absolute now (the game moves the current directory to its data root at start): `_fullpath` on
/// Windows, the current directory and the path on POSIX; "" and "-" stay.
pub fn absolute(path: &str) -> String {
    if path.is_empty() || path == "-" {
        return path.to_string();
    }
    #[cfg(windows)]
    {
        match std::path::absolute(path) {
            Ok(p) => p.to_string_lossy().into_owned(),
            Err(_) => path.to_string(),
        }
    }
    #[cfg(not(windows))]
    {
        if path.starts_with('/') {
            return path.to_string();
        }
        match std::env::current_dir() {
            Ok(d) => format!("{}/{}", d.to_string_lossy(), path),
            Err(_) => path.to_string(),
        }
    }
}

/// What [`game_option`] made of a word.
#[derive(Debug, PartialEq)]
pub enum Taken {
    /// a game option (the index at its last word)
    Yes,
    /// not a game option
    No,
    /// a bad one
    Bad(String),
}

const WITH_VALUE: [&str; 18] = [
    "--seed",
    "--ai",
    "--map-size",
    "--map-type",
    "--difficulty",
    "--lobby",
    "--game-seconds",
    "--sample-seconds",
    "--wall-timeout",
    "--flare",
    "--volcano",
    "--engine",
    "--report",
    "--trace",
    "--log",
    "--err",
    "--out",
    "--id",
];

fn seconds_player(a: &str, v: &str) -> Result<(c_long, c_ulong), String> {
    let Some(c) = v.find(':') else { return Err(format!("{a} SECONDS:PLAYER")) };
    Ok(((c_atof(&v[..c]) * 1000.0) as c_long, c_atol(&v[c + 1..]) as c_ulong))
}

/// A game's own option (the command line's, or a `--games` line's) at `args[*index]`; `make_dir` makes `--out`'s
/// folder.
pub fn game_option(args: &[String], index: &mut usize, o: &mut Options, make_dir: &mut dyn FnMut(&str)) -> Taken {
    let a = args[*index].as_str();
    let mut v = String::new();
    if WITH_VALUE.contains(&a) {
        if *index + 1 >= args.len() {
            return Taken::Bad(format!("{a} needs a value"));
        }
        *index += 1;
        v = args[*index].clone();
    }
    match a {
        "--seed" => o.seed = c_strtoul0(&v),
        "--ai" => o.computers = c_atol(&v),
        "--map-size" => o.map_size = v,
        "--map-type" => o.map_type = v,
        "--difficulty" => o.difficulty = v,
        "--lobby" => {
            let Some(eq) = v.find('=') else { return Taken::Bad("--lobby CONTROL=VALUE".into()) };
            o.lobby.push((c_atol(&v[..eq]), v[eq + 1..].to_string()));
        }
        "--game-seconds" => o.game_seconds = c_atof(&v),
        "--sample-seconds" => o.sample_seconds = c_atof(&v),
        "--wall-timeout" => o.wall_timeout = c_atof(&v),
        "--human-defeat" => o.no_human_defeat = false,
        "--no-human-defeat" => o.no_human_defeat = true,
        "--keep-human" => o.human_out = false,
        "--flare" => match seconds_player(a, &v) {
            Ok((ms, p)) => (o.flare_ms, o.flare_player) = (ms, p),
            Err(e) => return Taken::Bad(e),
        },
        "--volcano" => match seconds_player(a, &v) {
            Ok((ms, p)) => (o.volcano_ms, o.volcano_player) = (ms, p),
            Err(e) => return Taken::Bad(e),
        },
        "--engine" => o.engine = v,
        "--report" => o.report = absolute(&v),
        "--trace" => o.trace = absolute(&v),
        "--log" => o.log = absolute(&v),
        "--err" => o.err = absolute(&v),
        "--id" => o.id = v,
        "--out" => {
            let mut d = absolute(&v);
            make_dir(&d);
            d.push(if cfg!(windows) { '\\' } else { '/' });
            o.report = format!("{d}report.json");
            o.trace = format!("{d}trace.txt");
            o.err = format!("{d}err.txt");
        }
        _ => return Taken::No,
    }
    Taken::Yes
}

/// The options' ranges (the computers 1..7, positive game and sample seconds).
pub fn in_range(o: &Options) -> bool {
    o.computers >= 1 && o.computers <= 7 && o.game_seconds > 0.0 && o.sample_seconds > 0.0
}

/// A line of the `--games` file -> its words (blanks between them; "..." keeps blanks, \" a quote).
pub fn words(line: &str) -> Vec<String> {
    let b = line.as_bytes();
    let mut out = Vec::new();
    let mut word: Vec<u8> = Vec::new();
    let (mut inside, mut quoted) = (false, false);
    let mut k = 0;
    while k < b.len() {
        let c = b[k];
        if c == b'\\' && k + 1 < b.len() && b[k + 1] == b'"' {
            word.push(b'"');
            inside = true;
            k += 1;
        } else if c == b'"' {
            quoted = !quoted;
            inside = true;
        } else if !quoted && matches!(c, b' ' | b'\t' | b'\r' | b'\n') {
            if inside {
                out.push(String::from_utf8_lossy(&word).into_owned());
            }
            word.clear();
            inside = false;
        } else {
            word.push(c);
            inside = true;
        }
        k += 1;
    }
    if inside {
        out.push(String::from_utf8_lossy(&word).into_owned());
    }
    out
}

/// A `--games` line's game: its game options over the command line's (`base`; a game's files are its own) ->
/// `Ok(None)` a blank or comment line, `Ok(Some)` a game, `Err` a bad line.
pub fn game_of_line(
    line: &str,
    index: usize,
    base: &Options,
    make_dir: &mut dyn FnMut(&str),
) -> Result<Option<Options>, String> {
    let words = words(line);
    if words.is_empty() || words[0].starts_with('#') {
        return Ok(None);
    }
    let mut game = base.clone();
    game.report.clear();
    game.trace.clear();
    game.log.clear();
    game.err.clear();
    game.id = index.to_string();
    let mut i = 0;
    while i < words.len() {
        match game_option(&words, &mut i, &mut game, make_dir) {
            Taken::Yes => {}
            Taken::No => return Err(format!("{} is not a game's option (--help)", words[i])),
            Taken::Bad(e) => return Err(e),
        }
        i += 1;
    }
    if !in_range(&game) {
        return Err("an option is out of range".into());
    }
    Ok(Some(game))
}

/// The rest of the command line (the process's options beside the game options).
#[derive(Default, Debug, PartialEq)]
pub struct Process {
    pub data: String,
    pub config: String,
    pub games: String,
    pub ai_trace: bool,
    /// `--dump-at MS=FILE`
    pub dumps: Vec<(c_long, String)>,
    /// `--dump-load FILE`
    pub dump_load: String,
    /// `--dump-tick MS=FILE`
    pub tick_dumps: Vec<(c_long, String)>,
    /// `--rng-trace MS=FILE`
    pub rng_trace: Option<(c_long, String)>,
}

/// What the command line asks for.
#[derive(Debug, PartialEq)]
pub enum Command {
    /// a run
    Run,
    /// `--help`: the usage, exit 0
    Help,
    /// a bad command line: the message (after "EE-SIM: ") and the exit code
    Bad(String, i32),
}

fn ms_file(v: &str, form: &str) -> Result<(c_long, String), String> {
    match v.find('=') {
        Some(eq) => Ok((c_atol(&v[..eq]), absolute(&v[eq + 1..]))),
        None => Err(form.to_string()),
    }
}

/// The command line (without the program's name) into `o` and `p`; `add_set` applies a `--set` (`Err` its
/// problem), `make_dir` makes `--out`'s folder.
pub fn parse_command_line(
    args: &[String],
    o: &mut Options,
    p: &mut Process,
    add_set: &mut dyn FnMut(&str) -> Result<(), String>,
    make_dir: &mut dyn FnMut(&str),
) -> Command {
    let mut i = 0;
    while i < args.len() {
        let a = args[i].clone();
        match game_option(args, &mut i, o, make_dir) {
            Taken::Bad(e) => return Command::Bad(e, 64),
            Taken::Yes => {
                i += 1;
                continue;
            }
            Taken::No => {}
        }
        if a == "-h" || a == "--help" {
            return Command::Help;
        }
        let valued = [
            "--pt-rate",
            "--cheats",
            "--personality",
            "--set",
            "--data",
            "--config",
            "--games",
            "--checksum-trace",
            "--port-offset",
            "--autosave-ms",
            "--save-at",
            "--load-save",
            "--scenario",
            "--campaign",
            "--scenario-index",
            "--editor-script",
            "--editor-out",
            "--dump-at",
            "--dump-load",
            "--dump-tick",
            "--rng-trace",
        ];
        let mut v = String::new();
        if valued.contains(&a.as_str()) {
            if i + 1 >= args.len() {
                return Command::Bad(format!("{a} needs a value"), 64);
            }
            i += 1;
            v = args[i].clone();
        }
        match a.as_str() {
            "--pt-rate" => o.pt_rate = c_atol(&v),
            "--cheats" => o.cheats = v,
            "--personality" => o.personality = v,
            "--set" => {
                if let Err(e) = add_set(&v) {
                    return Command::Bad(format!("--set {e}"), 64);
                }
                o.sets.push(v);
            }
            "--data" => p.data = absolute(&v),
            "--config" => p.config = absolute(&v),
            "--games" => p.games = if v == "-" { v } else { absolute(&v) },
            "--ai-trace" => p.ai_trace = true,
            "--checksum-trace" => o.checksum_trace = absolute(&v),
            "--quiet" => o.quiet = true,
            "--calamities" => o.calamities = true,
            "--missile-sweep" => o.missile_sweep = true,
            "--unit-sweep" => o.unit_sweep = true,
            "--app-sweep" => o.app_sweep = true,
            "--effect-sweep" => o.effect_sweep = true,
            "--group-sweep" => o.group_sweep = true,
            "--port-offset" => o.port_offset = c_atol(&v),
            "--autosave-ms" => o.autosave_ms = c_atol(&v),
            "--save-at" => match v.find('=') {
                Some(eq) if eq != 0 && eq + 1 != v.len() => {
                    o.saves.push((c_atol(&v[..eq]), absolute(&v[eq + 1..])));
                }
                _ => return Command::Bad("--save-at MS=FILE".into(), 64),
            },
            "--load-save" => o.load_save = absolute(&v),
            "--scenario" => o.scenario = absolute(&v),
            "--campaign" => {
                if !["learning", "roman", "pacific", "asian"].contains(&v.as_str()) {
                    return Command::Bad("--campaign learning|roman|pacific|asian".into(), 64);
                }
                o.campaign = v;
            }
            "--scenario-index" => o.scenario_index = c_atol(&v),
            "--editor-script" => o.editor_script = absolute(&v),
            "--editor-out" => o.editor_out = absolute(&v),
            "--dump-at" => match ms_file(&v, "--dump-at MS=FILE") {
                Ok(d) => p.dumps.push(d),
                Err(e) => return Command::Bad(e, 64),
            },
            "--dump-load" => p.dump_load = absolute(&v),
            "--dump-tick" => match ms_file(&v, "--dump-tick MS=FILE") {
                Ok(d) => p.tick_dumps.push(d),
                Err(e) => return Command::Bad(e, 64),
            },
            "--rng-trace" => match ms_file(&v, "--rng-trace MS=FILE") {
                Ok(d) => p.rng_trace = Some(d),
                Err(e) => return Command::Bad(e, 64),
            },
            _ => return Command::Bad(format!("unknown option {a} (--help)"), 64),
        }
        i += 1;
    }
    if !in_range(o) || o.pt_rate < 1 || o.pt_rate > 250 {
        return Command::Bad("an option is out of range (--help)".into(), 64);
    }
    // (a stable sort: saves at the same time keep their order)
    o.saves.sort_by_key(|s| s.0);
    Command::Run
}

/// `--help`'s text.
pub const USAGE: &str =
    "EE-SIM.exe: the headless simulation (TEST ONLY). Runs a seeded computer-vs-computer random-map game of the game's
own code without window, rendering or sound, as fast as the CPU allows, and writes a JSON report.
  --seed N               the start seed (default 2)
  --ai N                 computer players, 1..7 (default 2)
  --map-size NAME        lobby map size line (default Small)
  --map-type NAME        lobby map type line (default Continental)
  --difficulty NAME      lobby difficulty line (default Hard)
  --lobby CONTROL=VALUE  any lobby combo of form 26 by line name or #index (e.g. 209=#2)
  --game-seconds S       stop at this game time (default 2700)
  --sample-seconds S     report sample interval in game seconds (default 30)
  --wall-timeout S       give up after this wall time (default 3600)
  --pt-rate N            Min = Max PTs per Second (default 40, the harness's pin)
  --human-defeat         let the idle human be defeated (default: kept in, the harness's --no-human-defeat)
  --flare SECONDS:PLAYER the idle human flares at the player's first building at that game time (TEST ONLY:
                         its allies, the computers, answer it as an ally's flare)
  --volcano SECONDS:PLAYER a volcano cast 6 tiles off the player's first building at that game time; logs the
                         boulders launched every game second (TEST ONLY)
  --keep-human           keep the idle human at war (default: allied with every computer from the start, so no
                         computer attacks it)
  --cheats SETS          EE_AI_CHEATS (e.g. none, all, none/all)
  --personality NAMES    EE_AI_PERSONALITY (e.g. balanced)
  --engine LIST          the computer seats' engine through the lobby's engine boxes: classic (the default: the
                         manifest's classic AI, classic_lua), random, an AI's
                         id from Data/ai/ais.json (e.g. boom) or next (its first AI); one for every seat or a
                         comma list by seat, taken in turn (e.g. boom,classic)
  --set DOC.KEY=VALUE    a rules document value (repeatable), e.g. aipersonalities.balanced.military.army_floor=0.2
  --data DIR             the data root (EE_DATA_ROOT; default the exe's directory)
  --config FILE          the settings file (EE_CONFIG; default <data root>/config.yaml)
  --report FILE          the JSON report (default stdout)
  --trace FILE           the EE-AI trace lines (with EE_AI_TRACE=1 or --ai-trace; default stdout)
  --ai-trace             EE_AI_TRACE=1
  --log FILE             every debug string of the game
  --checksum-trace FILE  one line of state hashes per communications tick (tick, random seed, out-of-synch
                         checksum, units, AI); reading them changes nothing (tools/sim_oracle.py ticks)
  --autosave-ms N        the game's autosave period in PT time (default 0: none; the game and the harness: 600000)
  --save-at MS=FILE      save the game at the first communications tick at or after game ms MS (repeatable); the
                         file is copied to FILE at the end of the run
  --load-save FILE       load this saved game (the game's Load Game screen) instead of starting from the lobby
  --scenario FILE        play this scenario (.scn) instead of a random map: the lobby's game type Scenario, the
                         Play Scenario screen, Load, Start (the scenario's own players and options)
  --campaign NAME        play a shipped campaign's scenario through the campaign screens (learning, roman,
                         pacific, asian: single player, the campaign, its scenario select form, the intro, Start;
                         the single-player scenarios the lobby refuses, the Learning Campaign's)
  --scenario-index K     with --campaign: the scenario list line (default: the screen's preselection; every
                         Learning Campaign scenario is open, of the others only the first)
  --dump-at MS=FILE      write the units' and players' state at the first tick at or after game ms MS (repeatable)
  --dump-load FILE       with --load-save: the same of the loaded state (at its first tick, before it runs)
  --dump-tick MS=FILE    the same after the first physics tick at or after game ms MS (repeatable; --dump-at
                         writes after a communications tick's last physics tick)
  --rng-trace MS=FILE    every draw of the physics random stream after game ms MS with its callers (exe-relative
                         addresses, tools/sim.py resolve), at most 50,000 draws
  --port-offset N        move the game's TCP ports 33334/33335 by N (default: none fixed, the system picks a
                         free port at each bind)
  --calamities           TEST ONLY: every five game minutes the computer players cast the next eighth of the
                         calamity classes (the capture's calamity game; ee/EETechCapture.h)
  --missile-sweep        TEST ONLY: with EE_SIM_CAPTURE, when the game ends the players' units fire every
                         missile class (the capture's missile game: CBMA, CBMH and the impact tests)
  --unit-sweep           TEST ONLY: every five game minutes every player gets aircraft, ships under water,
                         sea animals and spaceships sent at the next player (the capture's units game)
  --app-sweep            TEST ONLY: with EE_SIM_CAPTURE, cheat lines at five game minutes (CHET, CHEF, CHLS) and
                         one storm of each kind at ten (STRM; the capture's app game, ee/EEAppCapture.h); at
                         the game's end EEMap's sight and checksum (MAPS, MAPK, TSGT; ee/EESightCapture.h)
  --effect-sweep         TEST ONLY: with EE_SIM_CAPTURE, only the end of the game records: every swept 3D effect
                         kind made, stepped, saved and loaded (the capture's effects game: EFSW, EFST, EFMU)
  --group-sweep          TEST ONLY: with EE_SIM_CAPTURE, only the end of the game records: every player's units
                         get real group commands, each one's outcome recorded (the capture's groups game: AGPL)
  --editor-script FILE   TEST ONLY: instead of a game, the scenario editor (main menu -> Editors -> Scenario
                         Editor) and this script of its actions and digests (EESimEditor.cpp names the commands)
  --editor-out FILE      the script's steps and digests (default stdout)
  --quiet                no progress on stderr
  --games FILE           several games in a row in this process, one a line: the line's game options over the
                         command line's (# comments; \"...\" quotes). Game options: --seed --ai --map-size
                         --map-type --difficulty --lobby --engine --game-seconds --sample-seconds --wall-timeout
                         --human-defeat --keep-human --flare --volcano --report --trace --log and
                         --err FILE (the game's stderr), --out DIR (DIR/report.json, trace.txt, err.txt),
                         --id NAME (the markers' name). The rest (--set --cheats --personality --ai-trace
                         --pt-rate --autosave-ms --data --config --port-offset) holds for every game. stderr gets
                         \"EE-SIM GAME k BEGIN id\" / \"EE-SIM GAME k END code outcome\" per game and \"EE-SIM
                         BATCH END n\"; a failed game (code 2 or 3) ends the process (tools/sim.py play_games
                         goes on with the rest in a new one). Each game's report and trace equal its run alone
";
