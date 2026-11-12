//! The in-game command panel's buttons (`User Interface Thread/EEUIFMGStatusButton.cpp`): whether a button is
//! available (`CopyData`: a command by its own test, a technology by the player's `CanProduce` or, for a calamity
//! with a cost, by a selected unit that can pay it), which icon it draws, what its tooltip is made of
//! (`GetHelpString`), and the targeting modes its commands start (`Command`); and the panel's layout
//! (`EEUIFMGMainGroup::ShowCommands`: which command, trade, civ power and research buttons show, and where).
//!
//! The C++ keeps the buttons, the selected group, the tech nodes, the strings and every call into the simulation;
//! it hands over what the tests read (per selected unit only what the button's test needs) and applies the answer.

use core::ffi::{c_long, c_ulong};

/// A command button that is always available.
pub const EE_SCREENS_BUTTON_ALWAYS: u32 = 0;
/// Go fanatic (0x28): a selected unit in unit set 0x2f that is not cloaked.
pub const EE_SCREENS_BUTTON_FANATIC: u32 = 1;
/// Unload (2): a selected unit that is not a teleporter short of the fortress unload power.
pub const EE_SCREENS_BUTTON_UNLOAD: u32 = 2;
/// A market trade (0x5dc..0x5e3): the player can afford it.
pub const EE_SCREENS_BUTTON_TRADE: u32 = 3;
/// Just-in-time manufacturing (0x29, 0x2a): not in the deathmatch delay.
pub const EE_SCREENS_BUTTON_JIT: u32 = 4;
/// A technology: the player's `CanProduce`.
pub const EE_SCREENS_BUTTON_PRODUCE: u32 = 5;
/// A calamity with a cost: a selected unit of the node's types with the power to pay it.
pub const EE_SCREENS_BUTTON_POWER: u32 = 6;

/// The teleporter (`kUnitTypeTeleporter`): its unload costs power.
const TELEPORTER: c_ulong = 0x313;

/// The test a command button's availability takes.
pub fn command_test(button: c_ulong) -> u32 {
    match button {
        0x28 => EE_SCREENS_BUTTON_FANATIC,
        2 => EE_SCREENS_BUTTON_UNLOAD,
        0x5dc..=0x5e3 => EE_SCREENS_BUTTON_TRADE,
        0x29 | 0x2a => EE_SCREENS_BUTTON_JIT,
        _ => EE_SCREENS_BUTTON_ALWAYS,
    }
}

/// Whether a technology button is a calamity's (`mKind` 5 with a calamity): its cost then decides the test.
pub fn tech_is_calamity(kind: c_long, calamity: c_ulong) -> bool {
    kind == 5 && calamity != 0
}

/// A technology button's test: a calamity with a cost by the selected units' power, anything else `CanProduce`.
pub fn tech_test(calamity: bool, cost: c_long) -> u32 {
    if calamity && cost != 0 {
        EE_SCREENS_BUTTON_POWER
    } else {
        EE_SCREENS_BUTTON_PRODUCE
    }
}

/// What a button's test reads of one selected unit (`present` false: the group's entry is empty).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensButtonUnit {
    pub present: bool,
    pub unit_type: c_ulong,
    pub power: c_long,
    /// The fanatic test: in unit set 0x2f, cloaked.
    pub in_set: bool,
    pub cloaked: bool,
    /// The calamity test: the tech node holds the unit's type.
    pub in_node: bool,
}

/// What a button's test reads besides the units.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensButtonFacts {
    /// The player has a selected group (else no unit test passes).
    pub has_group: bool,
    pub affordable: bool,
    pub can_produce: bool,
    /// `gGameResources`, the game clock and the deathmatch JIT delay (seconds: `kWorldDeathmatchJitManufacturingDelay`).
    pub game_resources: c_long,
    pub clock: f32,
    pub jit_delay: c_ulong,
    pub unload_cost: c_long,
    pub calamity_cost: c_long,
}

/// Whether the button is available under its test (the original's "found" loops: any unit that passes).
pub fn available(test: u32, facts: &EeScreensButtonFacts, units: &[EeScreensButtonUnit]) -> bool {
    let any = |f: &dyn Fn(&EeScreensButtonUnit) -> bool| facts.has_group && units.iter().any(f);
    match test {
        EE_SCREENS_BUTTON_FANATIC => any(&|u| u.present && u.in_set && !u.cloaked),
        EE_SCREENS_BUTTON_UNLOAD => any(&|u| u.present && !(u.unit_type == TELEPORTER && u.power < facts.unload_cost)),
        EE_SCREENS_BUTTON_TRADE => facts.affordable,
        // `gGameResources == 4 && clock < (float)(long)(delay * 1000)`: unavailable
        EE_SCREENS_BUTTON_JIT => {
            let delay = facts.jit_delay.wrapping_mul(1000) as c_long as f32;
            !(facts.game_resources == 4 && facts.clock < delay)
        }
        EE_SCREENS_BUTTON_PRODUCE => facts.can_produce,
        EE_SCREENS_BUTTON_POWER => any(&|u| u.present && u.power >= facts.calamity_cost && u.in_node),
        _ => true,
    }
}

/// [`icon`]: no icon.
pub const EE_SCREENS_BUTTON_ICON_NONE: u32 = 0;
/// [`icon`]: the icon as the face bitmap.
pub const EE_SCREENS_BUTTON_ICON_FACE: u32 = 1;
/// [`icon`]: the icon as the pressed bitmap.
pub const EE_SCREENS_BUTTON_ICON_PRESSED: u32 = 2;

/// The icon `CopyData` draws: none without a button object or for the group buttons (0x1d, 0x1e: `mButton` as it
/// was last set, a technology button's included), else pressed when the state's bitmap is the pressed one (a state
/// past the three draws none, which is the pressed one only when there is none).
pub fn icon(
    has_object: bool,
    button: c_ulong,
    state: c_ulong,
    normal_is_pressed: bool,
    focused_is_pressed: bool,
    pressed_is_none: bool,
) -> u32 {
    if !has_object || button == 0x1d || button == 0x1e {
        return EE_SCREENS_BUTTON_ICON_NONE;
    }
    let pressed = match state {
        0 => normal_is_pressed,
        1 => focused_is_pressed,
        2 => true,
        _ => pressed_is_none,
    };
    if pressed {
        EE_SCREENS_BUTTON_ICON_PRESSED
    } else {
        EE_SCREENS_BUTTON_ICON_FACE
    }
}

/// The first string of a command button's tooltip (`IDS_ACTION_BUTTON_HELP_FIRST` + the button).
pub fn command_help_string(button: c_ulong) -> c_long {
    (button as c_long).wrapping_add(6001)
}

/// Whether a command's tooltip adds the selected priest's conversion cost (the flares, 0x14 and 0x23).
pub fn command_help_priest(button: c_ulong) -> bool {
    button == 0x14 || button == 0x23
}

/// A technology button's tooltip: where the name comes from, the cost, the population cost, the unit's name.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensTechHelp {
    /// 0 the class's name string, 1 the player's upgrade name, 2 the node's name string.
    pub name: u32,
    /// The cost as "Power: <cost>" (a calamity with a cost), else the node's cost string.
    pub power: bool,
    /// " Pop Cost: <n>" (a class costing more than one).
    pub population: bool,
    /// The node's name after the cost (a node that makes a unit and has a name).
    pub unit_name: bool,
}

/// `GetHelpString`'s plan for a technology button: `has_class` its class index is a class, `upgrade_named` the
/// player's upgrade of that class has a name, `calamity_cost` the calamity's cost of a kind 5 node.
pub fn tech_help(
    has_class: bool,
    upgrade_named: bool,
    population: c_long,
    kind: c_long,
    calamity_cost: c_long,
    makes_unit: bool,
    name_id: c_long,
) -> EeScreensTechHelp {
    let count = if has_class { population as c_ulong } else { 0 };
    EeScreensTechHelp {
        name: if !has_class {
            2
        } else if upgrade_named {
            1
        } else {
            0
        },
        power: kind == 5 && calamity_cost != 0,
        population: count > 1,
        unit_name: makes_unit && name_id > 0,
    }
}

/// A targeting mode a command starts (`Command`): the main game form's UI state, and for the attack ground and
/// rally commands the cursor locked to a pointer first. The form then shows its cancel button and refreshes.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensCommandMode {
    pub ui_state: c_long,
    pub lock_cursor: bool,
    pub pointer: c_ulong,
}

/// The targeting mode of command `button`, if it starts one.
pub fn command_mode(button: c_ulong) -> Option<EeScreensCommandMode> {
    let mode = |ui_state| EeScreensCommandMode { ui_state, lock_cursor: false, pointer: 0 };
    let locked = |ui_state, pointer| EeScreensCommandMode { ui_state, lock_cursor: true, pointer };
    Some(match button {
        5 => mode(0xc),
        6 => locked(0x13, 0x17),
        7 => mode(2),
        9 => mode(8),
        10 => mode(9),
        0x26 => mode(0x14),
        0xc => mode(10),
        0xd => mode(0xf),
        0xe => mode(0xe),
        0xf => mode(0xd),
        0x18 => locked(7, 0x15),
        _ => return None,
    })
}

/// What `EEUIFMGMainGroup::ShowCommands` reads before it lays the panel out: whether it shows at all (a built,
/// living, commandable complex unit of the local human, non-observing player, no targeting mode on) and what the
/// mode's buttons need.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensPanelView {
    pub unit: bool,
    pub complex: bool,
    pub built: bool,
    pub alive: bool,
    pub commandable: bool,
    /// The unit's owner is the local player (not observing).
    pub owned: bool,
    pub cancel_hidden: bool,
    pub player: bool,
    pub human: bool,
    /// The main group's mode: 1 and 3 the commands, 2 and 3 the research, 2 the stop button, 4 the market.
    pub mode: c_long,
    /// The fanaticism power and a fanatic-capable unit; the just-in-time power and a rally point; its state.
    pub fanatic: bool,
    pub jit: bool,
    pub jit_on: bool,
    /// The button objects' positions: go fanatic (0x28), JIT on and off (0x29, 0x2a), the eight trades.
    pub fanatic_position: c_long,
    pub jit_positions: [c_long; 2],
    pub trade_positions: [c_long; 8],
    /// The class's nine command slots.
    pub commands: [c_ulong; 9],
}

/// One research technology of the class: its button object's position and id, its node's kind, and whether it is
/// shown (a position in 0..10000, available, allowed or already queued).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensPanelResearch {
    pub position: c_long,
    pub command: c_long,
    pub kind: c_long,
    pub eligible: bool,
}

/// [`EeScreensPanelButton`]: a command button (`ShowCommandButton(button, control)`).
pub const EE_SCREENS_PANEL_COMMAND: u32 = 0;
/// [`EeScreensPanelButton`]: research technology `research` on `control` (`SetTechButton`).
pub const EE_SCREENS_PANEL_TECH: u32 = 1;
/// [`EeScreensPanelButton`]: the stop command on the first action button.
pub const EE_SCREENS_PANEL_STOP: u32 = 2;

/// One button the panel shows, in the order the original shows them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensPanelButton {
    pub kind: u32,
    pub button: c_ulong,
    pub research: u32,
    pub control: c_ulong,
}

/// The first command slot control (`kMainGameActionButton1`) and the first research control.
const ACTION_BUTTON: c_ulong = 0x4e;
const RESEARCH_BUTTON: c_ulong = 0x6e;

/// The panel's gate but for the player's `IsHuman` (the original asks it last).
pub fn panel_gate(v: &EeScreensPanelView) -> bool {
    v.unit && v.complex && v.built && v.alive && v.commandable && v.owned && v.cancel_hidden && v.player
}

/// Whether the panel shows any button for the selection.
pub fn panel_shown(v: &EeScreensPanelView) -> bool {
    panel_gate(v) && v.human
}

/// `ShowCommands`' layout: the buttons the panel shows and where (a command slot is free when its command is not
/// in 2..0x32; a research command of kind 3 takes the next free slot from the first, the original's slot search
/// never running in mode 2, where no slot was counted).
pub fn panel_buttons(v: &EeScreensPanelView, research: &[EeScreensPanelResearch]) -> Vec<EeScreensPanelButton> {
    let mut out = Vec::new();
    if !panel_shown(v) {
        return out;
    }
    let command = |button: c_ulong, control: c_ulong| EeScreensPanelButton {
        kind: EE_SCREENS_PANEL_COMMAND,
        button,
        research: 0,
        control,
    };
    let free = |c: c_ulong| !(2..0x32).contains(&c);
    let mut free_slot: c_long = -1;
    match v.mode {
        4 => {
            for (j, &position) in v.trade_positions.iter().enumerate() {
                out.push(command(0x5dc + j as c_ulong, (position as c_ulong).wrapping_add(RESEARCH_BUTTON)));
            }
        }
        1 | 3 => {
            if v.fanatic {
                out.push(command(0x28, (v.fanatic_position as c_ulong).wrapping_add(RESEARCH_BUTTON)));
            }
            if v.jit {
                let (button, position) = if v.jit_on { (0x29, v.jit_positions[0]) } else { (0x2a, v.jit_positions[1]) };
                out.push(command(button, (position as c_ulong).wrapping_add(RESEARCH_BUTTON)));
            }
            for (k, &c) in v.commands.iter().enumerate() {
                if !free(c) {
                    out.push(command(c, k as c_ulong + ACTION_BUTTON));
                } else if free_slot < 0 {
                    free_slot = k as c_long;
                }
            }
        }
        2 => out.push(EeScreensPanelButton {
            kind: EE_SCREENS_PANEL_STOP,
            button: 3,
            research: 0,
            control: ACTION_BUTTON,
        }),
        _ => {}
    }
    if v.mode == 2 || v.mode == 3 {
        for (m, r) in research.iter().enumerate() {
            if !r.eligible {
                continue;
            }
            let index = if r.position < 0 || r.position > 0x5f { 0x5f } else { r.position };
            if r.command < 0x32 && r.kind == 3 {
                // `for (unsigned long n = theFreeSlot; ...)`: a slot of -1 searches nothing
                let mut n = free_slot as c_ulong;
                while n < 9 {
                    if free(v.commands[n as usize]) {
                        out.push(command(r.command as c_ulong, n + ACTION_BUTTON));
                        free_slot += 1;
                        break;
                    }
                    n += 1;
                }
            } else {
                out.push(EeScreensPanelButton {
                    kind: EE_SCREENS_PANEL_TECH,
                    button: 0,
                    research: m as u32,
                    control: index as c_ulong + RESEARCH_BUTTON,
                });
            }
        }
    }
    out
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden_layout;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_tests() {
        assert_eq!(command_test(0x28), EE_SCREENS_BUTTON_FANATIC);
        assert_eq!(command_test(0x5e3), EE_SCREENS_BUTTON_TRADE);
        assert_eq!(command_test(0x5e4), EE_SCREENS_BUTTON_ALWAYS);
        let facts = EeScreensButtonFacts { has_group: true, unload_cost: 100, ..Default::default() };
        let teleporter = EeScreensButtonUnit { present: true, unit_type: TELEPORTER, power: 99, ..Default::default() };
        assert!(!available(EE_SCREENS_BUTTON_UNLOAD, &facts, &[teleporter]));
        assert!(available(
            EE_SCREENS_BUTTON_UNLOAD,
            &facts,
            &[teleporter, EeScreensButtonUnit { present: true, ..teleporter }.with_type(1)]
        ));
        assert!(!available(
            EE_SCREENS_BUTTON_UNLOAD,
            &EeScreensButtonFacts { has_group: false, ..facts },
            &[teleporter.with_type(1)]
        ));
        let jit = EeScreensButtonFacts { game_resources: 4, clock: 59_999.0, jit_delay: 60, ..facts };
        assert!(!available(EE_SCREENS_BUTTON_JIT, &jit, &[]));
        assert!(available(EE_SCREENS_BUTTON_JIT, &EeScreensButtonFacts { clock: 60_000.0, ..jit }, &[]));
        assert_eq!(tech_test(true, 0), EE_SCREENS_BUTTON_PRODUCE);
        assert_eq!(tech_test(true, 5), EE_SCREENS_BUTTON_POWER);
    }

    impl EeScreensButtonUnit {
        fn with_type(self, unit_type: c_ulong) -> Self {
            EeScreensButtonUnit { unit_type, ..self }
        }
    }

    #[test]
    fn icons_help_and_modes() {
        assert_eq!(icon(true, 0x1d, 0, false, false, false), EE_SCREENS_BUTTON_ICON_NONE);
        assert_eq!(icon(true, 3, 2, false, false, false), EE_SCREENS_BUTTON_ICON_PRESSED);
        assert_eq!(icon(true, 3, 3, false, false, true), EE_SCREENS_BUTTON_ICON_PRESSED);
        assert_eq!(icon(true, 3, 0, false, true, false), EE_SCREENS_BUTTON_ICON_FACE);
        assert_eq!(command_help_string(3), 6004);
        let help = tech_help(true, false, 2, 5, 40, true, 7);
        assert_eq!((help.name, help.power, help.population, help.unit_name), (0, true, true, true));
        assert_eq!(tech_help(false, true, 9, 3, 0, false, 0).name, 2);
        assert!(!tech_help(false, true, 9, 3, 0, false, 0).population);
        assert_eq!(command_mode(6), Some(EeScreensCommandMode { ui_state: 0x13, lock_cursor: true, pointer: 0x17 }));
        assert_eq!(command_mode(3), None);
    }
}
