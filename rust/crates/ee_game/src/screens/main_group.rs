//! The main game form's main panel group (`EEUIFMGMainGroup`, `User Interface Thread/EEUIFMGMainGroup.cpp`): which
//! unit the bottom panel shows (`GetSelectedUnit`, `WantsRefresh`, `Refresh`), the new selection (`SetUnit`: the
//! mode, the market's cloak, the status box, the icon bar, the commands), the same selection again (`Update`: the
//! queue counts on the research buttons, the airport's timers), the form's controls bound (`Open`), one command
//! button (`ShowCommandButton`: the behaviour's or formation's face, its hot keys) and the airport's buttons
//! (`ShowSpecialButtons`). `ShowCommands` stays C++ over `command_panel.rs`' layout.
//!
//! Every call on the group, its form, the status box, the units and the world is one host op ([`GroupHost`],
//! `ee_screenscpp_main_group`), asked where and as often as the original asked it. Units are handles (`EEUnit*`, 0
//! none).

// ---- the host's ops (EE_SCREENS_MGR_OP_*)

/// The group's field a (`EE_SCREENS_MGR_FIELD_*`).
pub const EE_SCREENS_MGR_OP_FIELD: u32 = 1;
/// The group's field a = b.
pub const EE_SCREENS_MGR_OP_SET_FIELD: u32 = 2;
/// The form's `mUIState`.
pub const EE_SCREENS_MGR_OP_UI_STATE: u32 = 3;
/// `gWorld.mInCinematic`.
pub const EE_SCREENS_MGR_OP_IN_CINEMATIC: u32 = 4;
/// The form's control a's `SetDisplayMode(b)`.
pub const EE_SCREENS_MGR_OP_SET_MODE: u32 = 5;
/// The main game form's (`gUIManager->mMainGameForm`) unit panels off: entity group 9, background 0x97, entity
/// group 8, background 0x98.
pub const EE_SCREENS_MGR_OP_PANELS_OFF: u32 = 6;
/// The current player's selected group's `GetNumUnits()`.
pub const EE_SCREENS_MGR_OP_SELECTED_COUNT: u32 = 7;
/// The selected group's `GetUnit(a)`.
pub const EE_SCREENS_MGR_OP_SELECTED_UNIT: u32 = 8;
/// The current player's `mSelectedViewedUnit`.
pub const EE_SCREENS_MGR_OP_VIEWED_UNIT: u32 = 9;
/// The unit a's `mHitPoints`.
pub const EE_SCREENS_MGR_OP_UNIT_HP: u32 = 10;
/// The unit a's `mUnitID`.
pub const EE_SCREENS_MGR_OP_UNIT_ID: u32 = 11;
/// `gWorld.GetUnit(a)`.
pub const EE_SCREENS_MGR_OP_WORLD_UNIT: u32 = 12;
/// The current player's `GetUnit(a)`.
pub const EE_SCREENS_MGR_OP_PLAYER_UNIT: u32 = 13;
/// `EERemoveCloakEffect` (a 0) or `EEAddCloakEffect` (a 1) of the unit b.
pub const EE_SCREENS_MGR_OP_CLOAK: u32 = 14;
/// The unit a's kind: 1 a complex unit, 2 the current player's, 4 a land citizen, 8 a market (`kUnitTypeMarketCP`).
pub const EE_SCREENS_MGR_OP_UNIT_KIND: u32 = 15;
/// The unit a's class's `mL294`.
pub const EE_SCREENS_MGR_OP_UNIT_L294: u32 = 16;
/// The status box's call a (`EE_SCREENS_MGR_BOX_*`) of the unit b.
pub const EE_SCREENS_MGR_OP_BOX: u32 = 17;
/// The status box's control slot a (`mC8C`, `mC94`, `mC90`, `mC84`, `mC88`, `mC98`, `mCAC`, `mCA8`) = the form's
/// control b.
pub const EE_SCREENS_MGR_OP_BOX_BIND: u32 = 18;
/// The status box's `SetControls(a, the form's control a + 0x45, its control a + 0x20)`.
pub const EE_SCREENS_MGR_OP_BOX_PAIR: u32 = 19;
/// The icon bar's (0xde) `Update()` when it exists.
pub const EE_SCREENS_MGR_OP_ICON_BAR: u32 = 20;
/// `ShowCommands(unit a)`.
pub const EE_SCREENS_MGR_OP_SHOW_COMMANDS: u32 = 21;
/// The form's `RemoveAllHotKeysForAGroup(a)`.
pub const EE_SCREENS_MGR_OP_REMOVE_HOT_KEYS: u32 = 22;
/// `EEButtonObject::Get(a) != NULL`.
pub const EE_SCREENS_MGR_OP_BUTTON_OBJECT: u32 = 23;
/// The status button a's `SetCommandButton(b)`.
pub const EE_SCREENS_MGR_OP_SET_COMMAND: u32 = 24;
/// The unit a's complex unit's behaviour: k for `EEUnitBehavior::Get(k)`, k in 0, 1, 2, 5 (asked in that order);
/// else -1.
pub const EE_SCREENS_MGR_OP_BEHAVIOR: u32 = 25;
/// The current player's selected group's formation's `GetType()`.
pub const EE_SCREENS_MGR_OP_FORMATION: u32 = 26;
/// The button a's normal and pressed bitmaps = the button b's.
pub const EE_SCREENS_MGR_OP_COPY_BITMAPS: u32 = 27;
/// `ESVERIFY_AT(false, a)`.
pub const EE_SCREENS_MGR_OP_VERIFY: u32 = 28;
/// The button object a's hot key exists (`GetHotKey() != NULL`).
pub const EE_SCREENS_MGR_OP_HAS_HOT_KEY: u32 = 29;
/// The button object a's hot key: b 0 always fires and presses and releases the status button c
/// (`EEUIEMGHotKeyStatusButton`s); b 1 added to the form (group 2); b 2 a copy with Shift held, pressing and releasing
/// the status button c, added to the form.
pub const EE_SCREENS_MGR_OP_HOT_KEY: u32 = 30;
/// The unit a's complex unit is an airport (`kUnitTypeAirport`; 0 without a complex unit).
pub const EE_SCREENS_MGR_OP_AIRPORT: u32 = 31;
/// The complex unit a's goal queue b's `GetTimeLeft()`.
pub const EE_SCREENS_MGR_OP_TIME_LEFT: u32 = 32;
/// The button a's text = the time b as "m:ss" (`gPhysics->GetMinutesAndSeconds`), shown.
pub const EE_SCREENS_MGR_OP_TIMER_TEXT: u32 = 33;
/// The complex unit a's player is the current player.
pub const EE_SCREENS_MGR_OP_COMPLEX_LOCAL: u32 = 34;
/// The complex unit a's class's `mCommands[b]`.
pub const EE_SCREENS_MGR_OP_COMMAND: u32 = 35;
/// The complex unit a's player's tech tree's epoch is before `kTechEpochAtomicWW2`.
pub const EE_SCREENS_MGR_OP_BEFORE_WW2: u32 = 36;
/// The status button a's `mCount = 0` and `SetDirty(true)`.
pub const EE_SCREENS_MGR_OP_RESET_COUNT: u32 = 37;
/// The unit a is on the map (`GetTileNode()`) and the current player's.
pub const EE_SCREENS_MGR_OP_ON_MAP_LOCAL: u32 = 38;
/// The unit a's complex unit is a land or naval citizen.
pub const EE_SCREENS_MGR_OP_WORKER: u32 = 39;
/// The complex unit a's citizen build queue's size (b 1) or `GetBuildQueueSize()` (b 0).
pub const EE_SCREENS_MGR_OP_QUEUE_SIZE: u32 = 40;
/// The complex unit a's queued item c's technology: the citizen build target's class's (b 1) or the build queue's
/// (b 0).
pub const EE_SCREENS_MGR_OP_QUEUE_KEY: u32 = 41;
/// The status button a's technology, or [`EE_SCREENS_MGR_COMMAND`] for a command button.
pub const EE_SCREENS_MGR_OP_BUTTON_TECH: u32 = 42;
/// The status button a's `mCount += b`.
pub const EE_SCREENS_MGR_OP_ADD_COUNT: u32 = 43;
/// The group's form (`mOwner`, a = 0) or the UI manager's main game form (`gUIManager->mMainGameForm`, a = 1): the
/// toolkit's ops on them are `hud_views`' (the FFI adapter's, below the trait).
pub const EE_SCREENS_MGR_OP_FORMS: u32 = 44;

/// The group's fields: `mMode`, `mHitPoints`, `mUnitID`, `mPreviousID`, `mL1C`, `mL20`, `mHasUnit`, `mReset`.
pub const EE_SCREENS_MGR_FIELD_MODE: i64 = 0;
pub const EE_SCREENS_MGR_FIELD_HIT_POINTS: i64 = 1;
pub const EE_SCREENS_MGR_FIELD_UNIT_ID: i64 = 2;
pub const EE_SCREENS_MGR_FIELD_PREVIOUS_ID: i64 = 3;
pub const EE_SCREENS_MGR_FIELD_L1C: i64 = 4;
pub const EE_SCREENS_MGR_FIELD_L20: i64 = 5;
pub const EE_SCREENS_MGR_FIELD_HAS_UNIT: i64 = 6;
pub const EE_SCREENS_MGR_FIELD_RESET: i64 = 7;

/// The status box's calls: `Fill(unit)`, `Refresh(unit)`, `HideBackgrounds()`, `UpdateQueuedCounts(the unit's
/// complex unit, its player's researched improvement of its type)`.
pub const EE_SCREENS_MGR_BOX_FILL: i64 = 0;
pub const EE_SCREENS_MGR_BOX_REFRESH: i64 = 1;
pub const EE_SCREENS_MGR_BOX_HIDE: i64 = 2;
pub const EE_SCREENS_MGR_BOX_QUEUE: i64 = 3;

/// [`EE_SCREENS_MGR_OP_BUTTON_TECH`]'s answer for a command button.
pub const EE_SCREENS_MGR_COMMAND: i64 = i64::MIN;

/// The members `ee_screens_main_group_member` runs (its `which`; a the unit, or the button and control).
pub const EE_SCREENS_MGR_DO_REFRESH: u32 = 1;
pub const EE_SCREENS_MGR_DO_WANTS_REFRESH: u32 = 2;
pub const EE_SCREENS_MGR_DO_SELECTED_UNIT: u32 = 3;
pub const EE_SCREENS_MGR_DO_OPEN: u32 = 4;
pub const EE_SCREENS_MGR_DO_SET_UNIT: u32 = 5;
pub const EE_SCREENS_MGR_DO_COMMAND_BUTTON: u32 = 6;
pub const EE_SCREENS_MGR_DO_SPECIAL_BUTTONS: u32 = 7;
pub const EE_SCREENS_MGR_DO_UPDATE: u32 = 8;

/// The cancel-mode button, the first research button (0x6e..0xce), the first command slot (0x4e), the airport's
/// first timer button (0xd0).
const CANCEL_UI_MODE: i64 = 244;
const RESEARCH_BUTTON: i64 = 0x6e;
const ACTION_BUTTON: i64 = 0x4e;
const TIMER_BUTTON: i64 = 0xd0;
/// The behaviour and formation buttons whose faces a command button takes: scout, stand ground, guard, aggressive
/// (behaviours 0, 1, 2, 5); the eight formations in their types' order.
const BEHAVIOR_SCOUT: i64 = 232;
const BEHAVIOR_STAND_GROUND: i64 = 228;
const BEHAVIOR_GUARD: i64 = 233;
const BEHAVIOR_AGGRESSIVE: i64 = 229;
const FORMATION_FIRST: i64 = 236;
/// The behaviour and formation commands.
const COMMAND_BEHAVIOR: i64 = 0x1d;
const COMMAND_FORMATION: i64 = 0x1e;
/// The command an airport's slot hides before the WW2 epoch.
const COMMAND_EARLY_HIDDEN: u32 = 0xc;
/// The status box's control slots' controls, in `Open`'s order: the portrait, the civilization's name, the unit's
/// name, the left and middle status, the armour title, the small and middle status bars.
const BOX_CONTROLS: [i64; 8] = [42, 100, 43, 30, 31, 41, 50, 51];

pub trait GroupHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64;

    fn a(&mut self, op: u32, a: i64) -> i64 {
        self.op(op, a, 0, 0)
    }
    fn field(&mut self, field: i64) -> i64 {
        self.op(EE_SCREENS_MGR_OP_FIELD, field, 0, 0)
    }
    fn set_field(&mut self, field: i64, value: i64) {
        self.op(EE_SCREENS_MGR_OP_SET_FIELD, field, value, 0);
    }
}

/// Runs the member `which` (`EE_SCREENS_MGR_DO_*`): the unit for `GetSelectedUnit`, `WantsRefresh`'s answer, else 0.
pub fn member(h: &mut impl GroupHost, which: u32, a: i64, b: i64) -> i64 {
    match which {
        EE_SCREENS_MGR_DO_REFRESH => refresh(h),
        EE_SCREENS_MGR_DO_WANTS_REFRESH => return wants_refresh(h) as i64,
        EE_SCREENS_MGR_DO_SELECTED_UNIT => return selected_unit(h),
        EE_SCREENS_MGR_DO_OPEN => open(h),
        EE_SCREENS_MGR_DO_SET_UNIT => set_unit(h, a),
        EE_SCREENS_MGR_DO_COMMAND_BUTTON => show_command_button(h, a, b),
        EE_SCREENS_MGR_DO_SPECIAL_BUTTONS => show_special_buttons(h, a),
        EE_SCREENS_MGR_DO_UPDATE => update(h, a),
        _ => {}
    }
    0
}

/// `GetSelectedUnit` (0x268162): the first living selected unit (the last one when none lives), else none, else the
/// viewed unit when nothing is selected.
pub fn selected_unit(h: &mut impl GroupHost) -> i64 {
    let count = h.a(EE_SCREENS_MGR_OP_SELECTED_COUNT, 0) as u32;
    let mut unit = 0;
    for i in 0..count {
        unit = h.a(EE_SCREENS_MGR_OP_SELECTED_UNIT, i as i64);
        if h.a(EE_SCREENS_MGR_OP_UNIT_HP, unit) as i32 != 0 {
            break;
        }
    }
    if unit != 0 {
        return unit;
    }
    if count != 0 {
        h.a(EE_SCREENS_MGR_OP_SELECTED_UNIT, 0)
    } else {
        h.a(EE_SCREENS_MGR_OP_VIEWED_UNIT, 0)
    }
}

/// `WantsRefresh` (0x26915e): the selection is not the unit shown (or a unit is selected while none is shown).
pub fn wants_refresh(h: &mut impl GroupHost) -> bool {
    let unit = selected_unit(h);
    if h.field(EE_SCREENS_MGR_FIELD_HAS_UNIT) != 0 {
        let id = h.field(EE_SCREENS_MGR_FIELD_UNIT_ID);
        unit != h.a(EE_SCREENS_MGR_OP_WORLD_UNIT, id)
    } else {
        unit != 0
    }
}

/// `Refresh` (0x267f61): the cancel button shown in a mode (not in a cinematic); then a lost unit cleared, another
/// unit shown (the unit panels off for both), the same one updated (shown again first when reset), or a first unit
/// shown.
pub fn refresh(h: &mut impl GroupHost) {
    let unit = selected_unit(h);
    let shown = h.a(EE_SCREENS_MGR_OP_UI_STATE, 0) as i32 != 0 && h.a(EE_SCREENS_MGR_OP_IN_CINEMATIC, 0) == 0;
    h.op(EE_SCREENS_MGR_OP_SET_MODE, CANCEL_UI_MODE, shown as i64, 0);
    if h.field(EE_SCREENS_MGR_FIELD_HAS_UNIT) != 0 {
        if unit == 0 {
            h.set_field(EE_SCREENS_MGR_FIELD_HAS_UNIT, 0);
            set_unit(h, 0);
            h.a(EE_SCREENS_MGR_OP_PANELS_OFF, 0);
        } else if h.a(EE_SCREENS_MGR_OP_UNIT_ID, unit) as u32 != h.field(EE_SCREENS_MGR_FIELD_UNIT_ID) as u32 {
            let id = h.a(EE_SCREENS_MGR_OP_UNIT_ID, unit);
            h.set_field(EE_SCREENS_MGR_FIELD_UNIT_ID, id);
            set_unit(h, unit);
            h.a(EE_SCREENS_MGR_OP_PANELS_OFF, 0);
        } else {
            if h.field(EE_SCREENS_MGR_FIELD_RESET) != 0 {
                h.set_field(EE_SCREENS_MGR_FIELD_RESET, 0);
                set_unit(h, unit);
            }
            update(h, unit);
        }
    } else if unit != 0 {
        h.set_field(EE_SCREENS_MGR_FIELD_HAS_UNIT, 1);
        let id = h.a(EE_SCREENS_MGR_OP_UNIT_ID, unit);
        h.set_field(EE_SCREENS_MGR_FIELD_UNIT_ID, id);
        set_unit(h, unit);
    }
}

/// `SetUnit` (0x268070): the market's cloak off the previous unit, the mode (3 the local player's complex units, 4
/// a market, cloaked; else 0), the hit points and class value kept, the status box filled, the icon bar updated, the
/// commands shown.
pub fn set_unit(h: &mut impl GroupHost, unit: i64) {
    if h.field(EE_SCREENS_MGR_FIELD_MODE) as i32 == 4 {
        let previous = h.field(EE_SCREENS_MGR_FIELD_PREVIOUS_ID);
        if previous as u32 != 0 {
            let old = h.a(EE_SCREENS_MGR_OP_PLAYER_UNIT, previous);
            if old != 0 {
                h.op(EE_SCREENS_MGR_OP_CLOAK, 0, old, 0);
            }
        }
    }
    let kind = if unit != 0 { h.a(EE_SCREENS_MGR_OP_UNIT_KIND, unit) } else { 0 };
    if unit != 0 && kind & 3 == 3 {
        if kind & 4 != 0 {
            h.set_field(EE_SCREENS_MGR_FIELD_MODE, 3);
        } else if kind & 8 != 0 {
            h.set_field(EE_SCREENS_MGR_FIELD_MODE, 4);
            h.op(EE_SCREENS_MGR_OP_CLOAK, 1, unit, 0);
        } else {
            h.set_field(EE_SCREENS_MGR_FIELD_MODE, 3);
        }
    } else {
        h.set_field(EE_SCREENS_MGR_FIELD_MODE, 0);
    }
    let id = h.field(EE_SCREENS_MGR_FIELD_UNIT_ID);
    h.set_field(EE_SCREENS_MGR_FIELD_PREVIOUS_ID, id);
    if unit != 0 {
        let hp = h.a(EE_SCREENS_MGR_OP_UNIT_HP, unit);
        h.set_field(EE_SCREENS_MGR_FIELD_HIT_POINTS, hp);
        let l294 = h.a(EE_SCREENS_MGR_OP_UNIT_L294, unit);
        h.set_field(EE_SCREENS_MGR_FIELD_L20, l294);
    } else {
        h.set_field(EE_SCREENS_MGR_FIELD_HIT_POINTS, 0);
        h.set_field(EE_SCREENS_MGR_FIELD_L20, 0);
    }
    h.op(EE_SCREENS_MGR_OP_BOX, EE_SCREENS_MGR_BOX_FILL, unit, 0);
    h.set_field(EE_SCREENS_MGR_FIELD_L1C, 0);
    h.a(EE_SCREENS_MGR_OP_ICON_BAR, 0);
    h.a(EE_SCREENS_MGR_OP_SHOW_COMMANDS, unit);
}

/// `Open` (0x2681c2): the status box's backgrounds hidden and its controls bound to the form's, the group's hot keys
/// removed, no unit shown.
pub fn open(h: &mut impl GroupHost) {
    h.op(EE_SCREENS_MGR_OP_BOX, EE_SCREENS_MGR_BOX_HIDE, 0, 0);
    for (slot, control) in BOX_CONTROLS.iter().enumerate() {
        h.op(EE_SCREENS_MGR_OP_BOX_BIND, slot as i64, *control, 0);
    }
    for i in 0..9 {
        h.a(EE_SCREENS_MGR_OP_BOX_PAIR, i);
    }
    h.a(EE_SCREENS_MGR_OP_REMOVE_HOT_KEYS, 2);
    h.set_field(EE_SCREENS_MGR_FIELD_HAS_UNIT, 0);
}

/// `ShowCommandButton` (0x2682ea) of the unit's command `button` on the status button `control`: the behaviour's or
/// the formation's face for those two commands, shown; its hot key (always firing, pressing the button) added, with a
/// Shift copy in the market's mode.
pub fn show_command_button(h: &mut impl GroupHost, unit: i64, packed: i64) {
    let (button, control) = (packed & 0xffff_ffff, packed >> 32);
    if h.a(EE_SCREENS_MGR_OP_BUTTON_OBJECT, button) == 0 {
        return;
    }
    h.op(EE_SCREENS_MGR_OP_SET_COMMAND, control, button, 0);
    if button == COMMAND_BEHAVIOR {
        let source = match h.a(EE_SCREENS_MGR_OP_BEHAVIOR, unit) {
            0 => Some(BEHAVIOR_SCOUT),
            1 => Some(BEHAVIOR_STAND_GROUND),
            2 => Some(BEHAVIOR_GUARD),
            5 => Some(BEHAVIOR_AGGRESSIVE),
            _ => None,
        };
        match source {
            Some(s) => {
                h.op(EE_SCREENS_MGR_OP_COPY_BITMAPS, control, s, 0);
            }
            None => {
                h.a(EE_SCREENS_MGR_OP_VERIFY, 415);
            }
        }
    } else if button == COMMAND_FORMATION {
        let formation = h.a(EE_SCREENS_MGR_OP_FORMATION, 0) as i32;
        if (0..8).contains(&formation) {
            h.op(EE_SCREENS_MGR_OP_COPY_BITMAPS, control, FORMATION_FIRST + formation as i64, 0);
        } else {
            h.a(EE_SCREENS_MGR_OP_VERIFY, 462);
        }
    }
    h.op(EE_SCREENS_MGR_OP_SET_MODE, control, 1, 0);
    if h.a(EE_SCREENS_MGR_OP_HAS_HOT_KEY, button) == 0 {
        return;
    }
    h.op(EE_SCREENS_MGR_OP_HOT_KEY, button, 0, control);
    h.op(EE_SCREENS_MGR_OP_HOT_KEY, button, 1, control);
    if h.field(EE_SCREENS_MGR_FIELD_MODE) as i32 != 4 {
        return;
    }
    h.op(EE_SCREENS_MGR_OP_HOT_KEY, button, 2, control);
}

/// `ShowSpecialButtons` (0x268dee): an airport's three timed rally countdowns (shown while running), and for the
/// local player's its command 0xc's slot hidden before the WW2 epoch.
pub fn show_special_buttons(h: &mut impl GroupHost, unit: i64) {
    if h.a(EE_SCREENS_MGR_OP_AIRPORT, unit) == 0 {
        return;
    }
    for i in 1..4 {
        let left = h.op(EE_SCREENS_MGR_OP_TIME_LEFT, unit, i, 0) as u32;
        let button = TIMER_BUTTON + i - 1;
        if left > 0 {
            h.op(EE_SCREENS_MGR_OP_TIMER_TEXT, button, left as i64, 0);
        } else {
            h.op(EE_SCREENS_MGR_OP_SET_MODE, button, 0, 0);
        }
    }
    if h.a(EE_SCREENS_MGR_OP_COMPLEX_LOCAL, unit) == 0 {
        return;
    }
    for k in 0..9 {
        if h.op(EE_SCREENS_MGR_OP_COMMAND, unit, k, 0) as u32 != COMMAND_EARLY_HIDDEN {
            continue;
        }
        if h.a(EE_SCREENS_MGR_OP_BEFORE_WW2, unit) != 0 {
            h.op(EE_SCREENS_MGR_OP_SET_MODE, ACTION_BUTTON + k, 0, 0);
        }
    }
}

/// `Update` (0x268f3f) of the same unit: the research buttons' counts cleared; the commands shown again when the
/// unit just died, else for the local player's unit on the map each run of equal queued technologies counted on its
/// research button, the airport's timers, the status box's queued counts; then the status box refreshed.
pub fn update(h: &mut impl GroupHost, unit: i64) {
    for i in RESEARCH_BUTTON..RESEARCH_BUTTON + 0x60 {
        h.a(EE_SCREENS_MGR_OP_RESET_COUNT, i);
    }
    if h.a(EE_SCREENS_MGR_OP_UNIT_HP, unit) as i32 == 0 && h.field(EE_SCREENS_MGR_FIELD_HIT_POINTS) as i32 != 0 {
        h.a(EE_SCREENS_MGR_OP_SHOW_COMMANDS, unit);
    } else if h.a(EE_SCREENS_MGR_OP_ON_MAP_LOCAL, unit) != 0 {
        let worker = h.a(EE_SCREENS_MGR_OP_WORKER, unit);
        let timers = h.a(EE_SCREENS_MGR_OP_AIRPORT, unit) != 0;
        let count = h.op(EE_SCREENS_MGR_OP_QUEUE_SIZE, unit, worker, 0) as u32;
        let mut j = 0u32;
        while j < count {
            let key = h.op(EE_SCREENS_MGR_OP_QUEUE_KEY, unit, worker, j as i64) as i32;
            let mut run = 1i32;
            let mut k = j + 1;
            while k < count {
                if h.op(EE_SCREENS_MGR_OP_QUEUE_KEY, unit, worker, k as i64) as i32 != key {
                    break;
                }
                j += 1;
                run += 1;
                k += 1;
            }
            for n in 0..0x60 {
                let tech = h.a(EE_SCREENS_MGR_OP_BUTTON_TECH, n + RESEARCH_BUTTON);
                if tech != EE_SCREENS_MGR_COMMAND && tech as i32 == key {
                    h.op(EE_SCREENS_MGR_OP_ADD_COUNT, n + RESEARCH_BUTTON, run as i64, 0);
                    break;
                }
            }
            j += 1;
        }
        if timers {
            show_special_buttons(h, unit);
        }
        h.op(EE_SCREENS_MGR_OP_BOX, EE_SCREENS_MGR_BOX_QUEUE, unit, 0);
    }
    h.op(EE_SCREENS_MGR_OP_BOX, EE_SCREENS_MGR_BOX_REFRESH, unit, 0);
}

#[cfg(test)]
mod golden;
