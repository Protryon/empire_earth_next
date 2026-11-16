//! `EECheatCodes` (src/Empire Earth/Empire Earth/EECheatCodes.cpp): the cheat table built from the string
//! resources, the chat line's parse ("goto x y", "find [player] id", a code with a trailing number), the lookup,
//! the code list for the chat log and `ApplyCheatCode`'s dispatch, over a host of the C++ parts (the strings, the
//! chat log, the camera, the players and the world walks a cheat does).
//!
//! Texts are the game's UTF-8 bytes (D6); lowering and the character steps are ee_plat_core's `PlatMbs*`, as the
//! C++ called them.
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use ee_plat_core::string::{PlatMbsDec, PlatMbsLen, PlatMbsLwr, PlatMbsNInc};
use std::collections::BTreeMap;
use std::os::raw::{c_long, c_ulong};

/// The string resources' ids (ee/EEStringIDs.h).
const IDS_HEADSHOT: u32 = 30622;
const IDS_SOMEBODY_SET_UP_US_BOMB: u32 = 30623;
const IDS_COLUMBUS: u32 = 30624;
const IDS_ASUS_DRIVERS: u32 = 30625;
const IDS_ATM: u32 = 30626;
const IDS_YOU_SAID_WOOD: u32 = 30627;
const IDS_ROCK_ROLL: u32 = 30628;
const IDS_CREATINE: u32 = 30629;
const IDS_ALL_YOUR_BASE_ARE_BELONG_TO_US: u32 = 30631;
const IDS_BAM: u32 = 30632;
const IDS_BRAINSTORM: u32 = 30633;
const IDS_BOSTON_FOOD_SUCKS: u32 = 30635;
const IDS_BOSTON_RENT: u32 = 30636;
const IDS_UH_SMOKE: u32 = 30637;
const IDS_SLIMFAST: u32 = 30638;
const IDS_MINE_YOUR_OWN_BUSINESS: u32 = 30639;
const IDS_GIRLYMAN: u32 = 30640;
const IDS_BIG_DIG: u32 = 30642;
const IDS_MY_NAME_IS_METHOS: u32 = 30643;
const IDS_QUOTABLE_PATELLA: u32 = 30644;
const IDS_DISPLAY_CHEATS: u32 = 30645;
const IDS_I_HAVE_POWER: u32 = 30667;
const IDS_COFFEE_TRAIN: u32 = 30668;
const IDS_AHHHCOOL: u32 = 30669;
const IDS_FRIENDLY_SKIES: u32 = 30670;
const IDS_INGAME_CHEAT: u32 = 30671;

/// `Init`'s table: each cheat's string, its code and whether it is single-player only, in the original's order
/// (`atm` twice: the map keeps the first).
const TABLE: [(u32, c_long, bool); 26] = [
    (IDS_HEADSHOT, 1, true),
    (IDS_SOMEBODY_SET_UP_US_BOMB, 2, true),
    (IDS_COLUMBUS, 3, false),
    (IDS_ASUS_DRIVERS, 4, false),
    (IDS_ATM, 5, false),
    (IDS_BOSTON_FOOD_SUCKS, 6, false),
    (IDS_YOU_SAID_WOOD, 7, false),
    (IDS_ATM, 5, false),
    (IDS_ROCK_ROLL, 8, false),
    (IDS_CREATINE, 9, false),
    (IDS_ALL_YOUR_BASE_ARE_BELONG_TO_US, 0xb, false),
    (IDS_BAM, 0xc, false),
    (IDS_BRAINSTORM, 0xd, false),
    (IDS_BOSTON_RENT, 0xe, false),
    (IDS_UH_SMOKE, 0xf, false),
    (IDS_SLIMFAST, 0x10, false),
    (IDS_MINE_YOUR_OWN_BUSINESS, 0x11, false),
    (IDS_GIRLYMAN, 0x12, false),
    (IDS_BIG_DIG, 0x14, false),
    (IDS_MY_NAME_IS_METHOS, 0x15, false),
    (IDS_QUOTABLE_PATELLA, 0x16, false),
    (IDS_DISPLAY_CHEATS, 0x17, false),
    (IDS_I_HAVE_POWER, 0x18, false),
    (IDS_COFFEE_TRAIN, 0x19, false),
    (IDS_AHHHCOOL, 0x1a, false),
    (IDS_FRIENDLY_SKIES, 0x1b, false),
];

/// `ListCheatCodes`' lines: up to five strings each (0: an empty slot).
const LIST: [[u32; 5]; 6] = [
    [IDS_HEADSHOT, IDS_SOMEBODY_SET_UP_US_BOMB, IDS_COLUMBUS, IDS_ASUS_DRIVERS, IDS_ATM],
    [IDS_BOSTON_FOOD_SUCKS, IDS_YOU_SAID_WOOD, IDS_ATM, IDS_ROCK_ROLL, IDS_CREATINE],
    [0, IDS_ALL_YOUR_BASE_ARE_BELONG_TO_US, IDS_BAM, IDS_BRAINSTORM, IDS_BOSTON_RENT],
    [IDS_UH_SMOKE, IDS_SLIMFAST, IDS_MINE_YOUR_OWN_BUSINESS, IDS_GIRLYMAN, 0],
    [IDS_BIG_DIG, IDS_MY_NAME_IS_METHOS, IDS_QUOTABLE_PATELLA, IDS_DISPLAY_CHEATS, 0],
    [IDS_I_HAVE_POWER, IDS_COFFEE_TRAIN, IDS_AHHHCOOL, IDS_FRIENDLY_SKIES, 0],
];

/// The player's resources (ee/EEResourceType.h).
const FOOD: c_long = 2;
const WOOD: c_long = 3;
const STONE: c_long = 4;
const GOLD: c_long = 5;
const IRON: c_long = 6;
/// `all your base`' and `my name is methos`' order.
const ALL: [c_long; 5] = [STONE, GOLD, WOOD, FOOD, IRON];

/// The world walks a cheat does, by the C++ (each for a player below the player count, but `Kill`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CheatAction {
    /// `RevealMap`: every unit of the player killed (`all` false: the local player's selection and viewed unit
    /// spared)
    Kill {
        all: bool,
    },
    /// `ExploreMap`, `MakeWorldVisible` (the local player only: a message), `ResearchAll`
    Explore,
    MakeVisible,
    ResearchAll,
    /// `AdvanceEpoch(epoch)` (the epoch's checks are the C++'s)
    AdvanceEpoch(c_long),
    RestorePower,
    Heal,
    Refuel,
    /// `ToggleInstantBuild`
    ToggleInstantBuild,
    /// `SetPhysicsFlagForPlayer` (`win`: the game ends as a victory when the player is the local one) and
    /// `ClearPhysicsFlagForPlayer` (as a defeat)
    EndGame {
        win: bool,
    },
    /// `AddResource(resource, amount)`, `ClearResource(resource)`
    AddResource(c_long, c_long),
    ClearResource(c_long),
}

/// The C++ parts.
pub trait CheatHost {
    /// `gResource->GetString(id)`'s bytes
    fn string(&mut self, id: u32) -> Vec<u8>;
    /// `gCommunications->GetIsMultiplayerSession()`
    fn multiplayer(&mut self) -> bool;
    /// `gWorld.mCheatsEnabled`
    fn cheats_enabled(&mut self) -> bool;
    /// `gWorld.mNumPlayers`
    fn players(&mut self) -> c_ulong;
    /// `gUserInterface->AddChatLogEntry(line, 0)`
    fn chat(&mut self, line: &[u8]);
    /// "goto": the camera to the map's closest tile of (x, y), when there is a map and a tile
    fn goto(&mut self, x: f32, y: f32);
    /// "find": the camera to the unit's tile, when there is such a unit
    fn find(&mut self, id: c_ulong);
    /// a cheat's work for a player
    fn action(&mut self, action: CheatAction, player: c_ulong);
}

/// The cheat table (`mCodes`, `mIsSinglePlayerOnly`).
#[derive(Debug, Default)]
pub struct EeAppCheats {
    codes: BTreeMap<Vec<u8>, c_long>,
    single_player: BTreeMap<c_long, bool>,
}

/// `PlatMbsLwr` of a text.
fn lower(text: &[u8]) -> Vec<u8> {
    let mut z = text.to_vec();
    z.push(0);
    // SAFETY: a NUL-terminated buffer the call lowers in place
    unsafe { PlatMbsLwr(z.as_mut_ptr()) };
    z.pop();
    z
}

/// C's `atoi`: leading white space, a sign, the digits (wrapping).
fn atoi(text: &[u8]) -> c_long {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let negative = i < text.len() && text[i] == b'-';
    if i < text.len() && (text[i] == b'-' || text[i] == b'+') {
        i += 1;
    }
    let mut v: c_long = 0;
    while i < text.len() && text[i].is_ascii_digit() {
        v = v.wrapping_mul(10).wrapping_add((text[i] - b'0') as c_long);
        i += 1;
    }
    if negative {
        v.wrapping_neg()
    } else {
        v
    }
}

/// C's `atof` then `(float)`: leading white space, then the longest decimal number (sign, digits, a point, an
/// exponent), "inf"/"infinity"/"nan" in any case; 0 when none.
fn atof(text: &[u8]) -> f32 {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let rest = &text[i..];
    let sign_len = usize::from(rest.first().is_some_and(|&c| c == b'-' || c == b'+'));
    let word = rest[sign_len..].to_ascii_lowercase();
    let negative = sign_len == 1 && rest[0] == b'-';
    if word.starts_with(b"inf") || word.starts_with(b"nan") {
        let v = if word.starts_with(b"inf") { f64::INFINITY } else { f64::NAN };
        return (if negative { -v } else { v }) as f32;
    }
    let mut end = sign_len;
    let digits = |from: usize| rest[from..].iter().take_while(|c| c.is_ascii_digit()).count();
    let whole = digits(end);
    end += whole;
    let mut fraction = 0;
    if rest.get(end) == Some(&b'.') {
        fraction = digits(end + 1);
        if whole + fraction > 0 {
            end += 1 + fraction;
        }
    }
    if whole + fraction == 0 {
        return 0.0;
    }
    if matches!(rest.get(end), Some(b'e' | b'E')) {
        let mut e = end + 1;
        if matches!(rest.get(e), Some(b'-' | b'+')) {
            e += 1;
        }
        let n = digits(e);
        if n > 0 {
            end = e + n;
        }
    }
    let mut s = String::from_utf8_lossy(&rest[..end]).into_owned();
    if s.ends_with('.') {
        s.push('0');
    }
    s.parse::<f64>().unwrap_or(0.0) as f32
}

impl EeAppCheats {
    /// `Init`: every cheat's lowered text with its code, and whether it is single-player only.
    pub fn new(h: &mut impl CheatHost) -> Self {
        let mut t = EeAppCheats::default();
        for (id, code, single) in TABLE {
            let text = lower(&h.string(id));
            t.codes.entry(text).or_insert(code);
            t.single_player.entry(code).or_insert(single);
        }
        t
    }

    /// `LookUp`: the code of a lowered text, 0 when unknown or single-player only in a multiplayer game.
    pub fn look_up(&self, h: &mut impl CheatHost, text: &[u8]) -> c_long {
        let code = self.codes.get(text).copied().unwrap_or(0);
        if code != 0 && self.single_player.get(&code).is_none_or(|&s| s) && h.multiplayer() {
            return 0;
        }
        code
    }

    /// `ParseCheatCode`: "goto x y" and "find [player] id" move the camera (code 0); otherwise a trailing number is
    /// the parameter (`parameter` untouched when none) and the rest, lowered, is looked up.
    pub fn parse(&self, h: &mut impl CheatHost, text: &[u8], code: &mut c_long, parameter: &mut c_long) {
        let tokens: Vec<&[u8]> = text.split(|&c| c == b' ').filter(|t| !t.is_empty()).collect();
        if text.starts_with(b"goto") {
            if tokens.len() == 3 {
                h.goto(atof(tokens[1]), atof(tokens[2]));
            }
            *code = 0;
        } else if text.starts_with(b"find") {
            if tokens.len() >= 2 {
                let mut id = c_ulong::MAX;
                if tokens.len() == 2 {
                    id = atoi(tokens[1]) as c_ulong;
                } else if tokens.len() == 3 {
                    id = (atoi(tokens[1]).wrapping_shl(24)).wrapping_add(atoi(tokens[2])) as c_ulong;
                }
                if id as u32 != u32::MAX {
                    h.find(id);
                }
            }
            *code = 0;
        } else {
            let mut z = text.to_vec();
            z.push(0);
            // SAFETY: `z` is NUL-terminated; the steps stay inside it (the length is its characters')
            unsafe {
                let start = z.as_mut_ptr();
                let length = PlatMbsLen(start);
                if length > 1 {
                    let mut last = PlatMbsNInc(start, length - 2);
                    let tail = std::ffi::CStr::from_ptr(last.cast()).to_bytes();
                    *parameter = atoi(tail);
                    if *parameter != 0 || tail == b" 0" {
                        if *parameter as c_ulong > 9 && last != start {
                            last = PlatMbsDec(start, last);
                        }
                        *last = 0;
                    }
                }
                let end = std::ffi::CStr::from_ptr(start.cast()).to_bytes().len();
                z.truncate(end);
            }
            *code = self.look_up(h, &lower(&z));
        }
    }

    /// `ListCheatCodes`: the codes in the chat log, up to five a line after the "Cheat:" title.
    pub fn list(&self, h: &mut impl CheatHost) {
        let title = h.string(IDS_INGAME_CHEAT);
        for row in LIST {
            let mut line = title.clone();
            for (i, id) in row.into_iter().enumerate() {
                if i > 0 {
                    line.extend_from_slice(b"     ");
                }
                if id != 0 {
                    line.extend(h.string(id));
                }
            }
            h.chat(&line);
        }
    }

    /// `ApplyCheatCode(code, player, parameter)` when cheats are enabled.
    pub fn apply(&self, h: &mut impl CheatHost, code: c_long, player: c_long, parameter: c_long) {
        use CheatAction::*;
        if !h.cheats_enabled() {
            return;
        }
        let p = player as c_ulong;
        match code {
            1 => {
                let count = h.players();
                let (mut target, mut all) = (parameter as c_ulong, true);
                if target >= count {
                    target = target.wrapping_sub(10);
                    all = false;
                    if target >= count {
                        return;
                    }
                }
                h.action(Kill { all }, target);
            }
            2 => run(h, p, EndGame { win: true }),
            3 => run(h, p, Explore),
            4 => run(h, p, MakeVisible),
            5 => run(h, p, AddResource(GOLD, 1000)),
            7 => run(h, p, AddResource(WOOD, 1000)),
            6 => run(h, p, AddResource(FOOD, 1000)),
            8 => run(h, p, AddResource(STONE, 1000)),
            9 => run(h, p, AddResource(IRON, 1000)),
            0xb => ALL.iter().for_each(|&r| run(h, p, AddResource(r, 100000))),
            0xc => run(h, p, AdvanceEpoch(parameter.wrapping_sub(1))),
            0xd => run(h, p, ToggleInstantBuild),
            0xe => run(h, p, ClearResource(GOLD)),
            0xf => run(h, p, ClearResource(WOOD)),
            0x10 => run(h, p, ClearResource(FOOD)),
            0x11 => run(h, p, ClearResource(STONE)),
            0x12 => run(h, p, ClearResource(IRON)),
            0x14 => ALL.iter().for_each(|&r| run(h, p, ClearResource(r))),
            0x15 => {
                ALL.iter().for_each(|&r| run(h, p, AddResource(r, 100000)));
                for a in [Explore, MakeVisible, ToggleInstantBuild, ResearchAll] {
                    run(h, p, a);
                }
            }
            0x16 => run(h, p, ResearchAll),
            0x17 => self.list(h),
            0x1a => run(h, p, EndGame { win: false }),
            0x18 => run(h, p, RestorePower),
            0x19 => run(h, p, Heal),
            0x1b => run(h, p, Refuel),
            _ => {}
        }
    }
}

/// A cheat's work for a player below the player count (each action's own test: the original's was `<=`).
fn run(h: &mut impl CheatHost, player: c_ulong, action: CheatAction) {
    if player < h.players() {
        h.action(action, player);
    }
}
