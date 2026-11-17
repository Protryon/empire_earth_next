//! The main game form's mouse cursor over a unit (`EEUIFMainGame::UpdateCursorKind` and the five decisions it calls:
//! `GetAttackStance`, `GetTransportCommand`, `GetRepairCommand`, `GetCommandOnUnit`, `Fn265D7A`, with the cdecl
//! helpers `EEGetAttackRangeKind` and `EECanEnterUnit`; `User Interface Thread/EEUIFMainGame.cpp`).
//!
//! Every read of the world is one host op ([`CursorHost`]), asked lazily in the original's order: the picked unit
//! (the target), the local player's selected group (by index), an air base's cargo (by selected index and cargo
//! index). The predicates with a cost of their own (the pick, `EEIsSeenBy`, `EEUUGetIsUnitClassTargetable`,
//! `CanGather`, `CanAttackClass`, `EECanHeal`, `EECanConvert`, the release) are asked exactly when the original asked
//! them; plain field reads are asked where the original read them. The returned value is the cursor kind written to
//! `gMouse->mPointer`.

use ee_render::ge::types::Ge3DPoint;

/// Whether the mouse is over no control, or over the command units control.
pub const EE_SCREENS_CURSOR_OP_MOUSE_OK: u32 = 1;
/// The selection's first unit: 0 none, 1 alive (hit points > 0), 2 not alive.
pub const EE_SCREENS_CURSOR_OP_FIRST: u32 = 2;
/// The selected group's kind flags.
pub const EE_SCREENS_CURSOR_OP_FLAGS: u32 = 3;
/// Whether the mouse is in the viewport's view rectangle.
pub const EE_SCREENS_CURSOR_OP_IN_VIEW: u32 = 4;
/// `gUnitPicker.PickUnit(mouse, a)`: 1 when a unit was picked (the host holds it as the target).
pub const EE_SCREENS_CURSOR_OP_PICK: u32 = 5;
/// The target's `Dereference`.
pub const EE_SCREENS_CURSOR_OP_RELEASE: u32 = 6;
/// `EEIsSeenBy(target, player, true)`.
pub const EE_SCREENS_CURSOR_OP_SEEN: u32 = 7;
/// The target class's kind flags (the "command").
pub const EE_SCREENS_CURSOR_OP_T_KIND: u32 = 8;
/// The target is on the map, disabled and not protected (`mTileNode`, `mIsDisabled`, `!IsProtected()`).
pub const EE_SCREENS_CURSOR_OP_T_DISABLED: u32 = 9;
/// The selection's first unit's class is a medic.
pub const EE_SCREENS_CURSOR_OP_FIRST_MEDIC: u32 = 10;
/// `EEUUGetIsUnitClassTargetable(NULL, target class)`.
pub const EE_SCREENS_CURSOR_OP_T_TARGETABLE: u32 = 11;
/// The target's owner: 0 the local player, 1 an enemy of it, 2 another player.
pub const EE_SCREENS_CURSOR_OP_T_OWNER: u32 = 12;
/// The target's `mHitPoints`.
pub const EE_SCREENS_CURSOR_OP_T_HP: u32 = 13;
/// The target's stats' `mMaxHitPoints`.
pub const EE_SCREENS_CURSOR_OP_T_MAX_HP: u32 = 14;
/// The target's stats' attack > 0.
pub const EE_SCREENS_CURSOR_OP_T_ATTACK: u32 = 15;
/// The target class's unit type.
pub const EE_SCREENS_CURSOR_OP_T_TYPE: u32 = 16;
/// The target class is a tower.
pub const EE_SCREENS_CURSOR_OP_T_TOWER: u32 = 17;
/// The target class's category.
pub const EE_SCREENS_CURSOR_OP_T_CATEGORY: u32 = 18;
/// The target is on the map and built (`mTileNode`, `mComplexUnit->mIsBuilt`).
pub const EE_SCREENS_CURSOR_OP_T_ENTERABLE: u32 = 19;
/// The target has a complex unit.
pub const EE_SCREENS_CURSOR_OP_T_COMPLEX: u32 = 20;
/// The target's aim point (`EEGetAttackRangeKind`'s), written to the three floats.
pub const EE_SCREENS_CURSOR_OP_AIM: u32 = 21;
/// `theGroup->FindUnitWithFlags(a)->CanGather(target, its tile, false, false)`.
pub const EE_SCREENS_CURSOR_OP_CAN_GATHER: u32 = 22;
/// `EECanHeal(the selection's first unit, the target's complex unit)`.
pub const EE_SCREENS_CURSOR_OP_CAN_HEAL: u32 = 23;
/// The selected group's unit count.
pub const EE_SCREENS_CURSOR_OP_SEL_COUNT: u32 = 24;
/// Selected unit a's class kind flags.
pub const EE_SCREENS_CURSOR_OP_SEL_KIND: u32 = 25;
/// Selected unit a's stats' attack > 0.
pub const EE_SCREENS_CURSOR_OP_SEL_ATTACK: u32 = 26;
/// Selected unit a's complex unit is built.
pub const EE_SCREENS_CURSOR_OP_SEL_BUILT: u32 = 27;
/// Selected unit a is on the map (`mTileNode`).
pub const EE_SCREENS_CURSOR_OP_SEL_ON_MAP: u32 = 28;
/// Selected unit a's complex unit's cargo count.
pub const EE_SCREENS_CURSOR_OP_SEL_CARGO: u32 = 29;
/// Selected unit a's class's unit type.
pub const EE_SCREENS_CURSOR_OP_SEL_TYPE: u32 = 30;
/// Selected unit a's class `IsAirBase()`.
pub const EE_SCREENS_CURSOR_OP_SEL_AIR_BASE: u32 = 31;
/// Selected unit a's class `IsCrusader()`.
pub const EE_SCREENS_CURSOR_OP_SEL_CRUSADER: u32 = 32;
/// Selected unit a's class `IsPriest()`.
pub const EE_SCREENS_CURSOR_OP_SEL_PRIEST: u32 = 33;
/// Selected unit a has a complex unit.
pub const EE_SCREENS_CURSOR_OP_SEL_COMPLEX: u32 = 34;
/// Selected unit a's complex unit's position (three floats).
pub const EE_SCREENS_CURSOR_OP_SEL_POSITION: u32 = 35;
/// Selected unit a's class's minimum range (the first float).
pub const EE_SCREENS_CURSOR_OP_SEL_MIN_RANGE: u32 = 36;
/// Selected unit a's complex unit `CanAttackClass(target class)`.
pub const EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK: u32 = 37;
/// `EECanConvert(selected unit a, the target's complex unit, shielded)` with shielded in b: bit 0 the answer, bit 1
/// shielded after the call.
pub const EE_SCREENS_CURSOR_OP_SEL_CONVERT: u32 = 38;
/// Cargo b of selected unit a: `CanAttackClass(target class)`.
pub const EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK: u32 = 39;
/// Cargo b of selected unit a: its stats' attack > 0.
pub const EE_SCREENS_CURSOR_OP_CARGO_ATTACK: u32 = 40;
/// Cargo b of selected unit a: its position (three floats).
pub const EE_SCREENS_CURSOR_OP_CARGO_POSITION: u32 = 41;
/// Cargo b of selected unit a: its class's minimum range (the first float).
pub const EE_SCREENS_CURSOR_OP_CARGO_MIN_RANGE: u32 = 42;

// The kind flags (`EEUnitClassIDs.h`).
const KIND_CITIZEN: u32 = 1;
const KIND_MILITARY: u32 = 2;
const KIND_PRIEST: u32 = 4;
const KIND_BUILDING: u32 = 0x10;
const KIND_NAVAL_CITIZEN: u32 = 0x40;
const KIND_HELICOPTER_TRANSPORT: u32 = 0x80;
const KIND_LAND_TRANSPORT: u32 = 0x100;
const KIND_NAVAL_TRANSPORT: u32 = 0x200;
const KIND_CAN_ENTER_AIR: u32 = 0x400;
const KIND_CAN_ENTER_LAND: u32 = 0x800;
const KIND_CAN_ENTER_SEA: u32 = 0x1000;
const KIND_CAN_ENTER_FORTRESSES: u32 = 0x2000;
const KIND_FORTRESS: u32 = 0x4000;
const KIND_LONG_RANGE: u32 = 0x8000;
const KIND_SPACE_TRANSPORT: u32 = 0x10000;
const KIND_CAN_ENTER_SPACE: u32 = 0x20000;

const TYPE_AIRPORT: i32 = 0x42;
const TYPE_MISSILE_BASE: i32 = 0x43;
const TYPE_CP_MISSILE_BASE: i32 = 0x2ed;
const TYPE_TELEPORTER: i32 = 0x313;
const CATEGORY_FORTRESS: i32 = 0x1d;

/// The world's reads for the cursor. `a` and `b` are the op's arguments (a selected unit's index, a cargo index, a
/// flag); `f` receives the floats of the position ops.
pub trait CursorHost {
    fn op(&mut self, op: u32, a: u32, b: u32, f: &mut [f32; 3]) -> i32;
}

struct Q<'a, H: CursorHost + ?Sized>(&'a mut H);

impl<H: CursorHost + ?Sized> Q<'_, H> {
    fn int(&mut self, op: u32, a: u32, b: u32) -> i32 {
        self.0.op(op, a, b, &mut [0.0; 3])
    }
    fn flag(&mut self, op: u32, a: u32) -> bool {
        self.int(op, a, 0) != 0
    }
    fn t(&mut self, op: u32) -> bool {
        self.int(op, 0, 0) != 0
    }
    fn point(&mut self, op: u32, a: u32, b: u32) -> Ge3DPoint {
        let mut f = [0.0; 3];
        self.0.op(op, a, b, &mut f);
        Ge3DPoint { x: f[0], y: f[1], z: f[2] }
    }
    fn float(&mut self, op: u32, a: u32, b: u32) -> f32 {
        let mut f = [0.0; 3];
        self.0.op(op, a, b, &mut f);
        f[0]
    }
}

/// `EECanEnterUnit` (0x63f5a3): whether units of the kind flags `flags` can enter a unit of the kind flags `other`.
pub fn can_enter_unit(flags: u32, other: u32) -> bool {
    let pair = |o: u32, f: u32| other & o == o && flags & f == f;
    pair(KIND_HELICOPTER_TRANSPORT, KIND_CAN_ENTER_AIR)
        || pair(KIND_LAND_TRANSPORT, KIND_CAN_ENTER_LAND)
        || pair(KIND_NAVAL_TRANSPORT, KIND_CAN_ENTER_SEA)
        || pair(KIND_SPACE_TRANSPORT, KIND_CAN_ENTER_SPACE)
        || pair(KIND_FORTRESS, KIND_CAN_ENTER_FORTRESSES)
}

/// `UpdateCursorKind` (0x666f8b): the cursor kind for the unit under the mouse.
pub fn cursor_kind(h: &mut (impl CursorHost + ?Sized)) -> i32 {
    let mut q = Q(h);
    let mut result = 0;
    let first = q.int(EE_SCREENS_CURSOR_OP_FIRST, 0, 0);
    let flags = q.int(EE_SCREENS_CURSOR_OP_FLAGS, 0, 0) as u32;
    if q.t(EE_SCREENS_CURSOR_OP_MOUSE_OK)
        && first == 1
        && flags != 0
        && q.t(EE_SCREENS_CURSOR_OP_IN_VIEW)
        && q.flag(EE_SCREENS_CURSOR_OP_PICK, flags & KIND_CITIZEN)
    {
        let allow_dead = !q.t(EE_SCREENS_CURSOR_OP_SEEN);
        let command = q.int(EE_SCREENS_CURSOR_OP_T_KIND, 0, 0) as u32;
        if q.t(EE_SCREENS_CURSOR_OP_T_DISABLED) {
            result = 2;
        } else if command != 0 {
            if flags & KIND_BUILDING != 0 {
                let next = attack_stance(&mut q, command, allow_dead);
                if next != 0 {
                    result = next;
                }
            }
            if flags & KIND_PRIEST != 0 || q.t(EE_SCREENS_CURSOR_OP_FIRST_MEDIC) {
                let next = transport_command(&mut q, command, flags, allow_dead);
                if next != 0 {
                    result = next;
                }
            }
            if flags & KIND_NAVAL_CITIZEN == KIND_NAVAL_CITIZEN {
                let next = repair_command(&mut q);
                if next != 0 {
                    result = next;
                }
            }
            if flags & KIND_CITIZEN == KIND_CITIZEN {
                let next = command_on_unit(&mut q, command, flags, allow_dead);
                if next != 0 {
                    result = next;
                }
            }
            if flags & KIND_MILITARY == KIND_MILITARY {
                let next = military_command(&mut q, command, flags, allow_dead);
                if next != 0 {
                    result = next;
                }
            }
        }
        q.t(EE_SCREENS_CURSOR_OP_RELEASE);
    }
    result
}

/// `GetAttackStance` (0x665791): the attack cursor for a selection holding armed buildings over an enemy.
fn attack_stance<H: CursorHost + ?Sized>(q: &mut Q<'_, H>, command: u32, _allow_dead: bool) -> i32 {
    let count = q.int(EE_SCREENS_CURSOR_OP_SEL_COUNT, 0, 0) as u32;
    let (mut any, mut first, mut healthy) = (false, false, false);
    for i in 0..count {
        if (q.int(EE_SCREENS_CURSOR_OP_SEL_KIND, i, 0) as u32) & KIND_BUILDING == 0
            || !q.flag(EE_SCREENS_CURSOR_OP_SEL_ATTACK, i)
        {
            continue;
        }
        // the target's attack (as the original: not the building's)
        if q.t(EE_SCREENS_CURSOR_OP_T_ATTACK) {
            healthy = true;
        }
        any = true;
        if !first {
            first = q.flag(EE_SCREENS_CURSOR_OP_SEL_BUILT, i);
        }
    }
    if any && q.int(EE_SCREENS_CURSOR_OP_T_OWNER, 0, 0) == 1 && command != 0 && command != 8 {
        if first {
            1
        } else if healthy {
            2
        } else {
            0
        }
    } else {
        0
    }
}

/// `GetCommandOnUnit` (0x66588c): a citizen selection's cursor: gather, repair, enter or attack.
fn command_on_unit<H: CursorHost + ?Sized>(q: &mut Q<'_, H>, command: u32, flags: u32, allow_dead: bool) -> i32 {
    let mut result = 0;
    let count = q.int(EE_SCREENS_CURSOR_OP_SEL_COUNT, 0, 0) as u32;
    if q.t(EE_SCREENS_CURSOR_OP_T_TARGETABLE) {
        let owner = q.int(EE_SCREENS_CURSOR_OP_T_OWNER, 0, 0);
        if q.flag(EE_SCREENS_CURSOR_OP_CAN_GATHER, KIND_CITIZEN) {
            result = 4;
        } else if owner == 0 {
            let t = q.int(EE_SCREENS_CURSOR_OP_T_TYPE, 0, 0);
            let repairable = command & KIND_BUILDING != 0
                || t == TYPE_MISSILE_BASE
                || t == TYPE_CP_MISSILE_BASE
                || t == TYPE_AIRPORT
                || t == TYPE_TELEPORTER
                || q.t(EE_SCREENS_CURSOR_OP_T_TOWER);
            let hp = q.int(EE_SCREENS_CURSOR_OP_T_HP, 0, 0);
            if repairable && (allow_dead || hp > 0 && hp < q.int(EE_SCREENS_CURSOR_OP_T_MAX_HP, 0, 0)) {
                result = 5;
            } else if can_enter_unit(flags, command) && q.t(EE_SCREENS_CURSOR_OP_T_ENTERABLE) {
                if q.int(EE_SCREENS_CURSOR_OP_T_CATEGORY, 0, 0) == CATEGORY_FORTRESS
                    && hp != q.int(EE_SCREENS_CURSOR_OP_T_MAX_HP, 0, 0)
                {
                    result = 5;
                } else {
                    result = 0xb;
                }
            }
        } else if owner == 1 && q.int(EE_SCREENS_CURSOR_OP_T_HP, 0, 0) > 0 {
            for i in 0..count {
                if q.int(EE_SCREENS_CURSOR_OP_SEL_KIND, i, 0) as u32 & KIND_CITIZEN == 0 {
                    continue;
                }
                if q.flag(EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK, i) {
                    result = 1;
                }
                break;
            }
        }
    }
    result
}

/// `EEGetAttackRangeKind` (0x665a4f): 1 the selection (or an air base's aircraft) can attack the target from beyond
/// its minimum range, 3 only too close, 2 only an armed unit that cannot attack it, 0 none.
fn attack_range_kind<H: CursorHost + ?Sized>(q: &mut Q<'_, H>, command: u32, allow_dead: bool) -> i32 {
    let count = q.int(EE_SCREENS_CURSOR_OP_SEL_COUNT, 0, 0) as u32;
    if command == 8 || !allow_dead && q.int(EE_SCREENS_CURSOR_OP_T_HP, 0, 0) < 1 {
        return 0;
    }
    let aim = q.point(EE_SCREENS_CURSOR_OP_AIM, 0, 0);
    let (mut can_attack, mut in_range, mut healthy) = (false, false, false);
    for i in 0..count {
        if can_attack && in_range {
            break;
        }
        if q.flag(EE_SCREENS_CURSOR_OP_SEL_ON_MAP, i)
            && (q.int(EE_SCREENS_CURSOR_OP_SEL_CARGO, i, 0) == 0
                || q.int(EE_SCREENS_CURSOR_OP_SEL_TYPE, i, 0) == TYPE_CP_MISSILE_BASE)
        {
            if q.flag(EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK, i)
                || q.int(EE_SCREENS_CURSOR_OP_SEL_TYPE, i, 0) == TYPE_CP_MISSILE_BASE
            {
                can_attack = true;
                if q.flag(EE_SCREENS_CURSOR_OP_SEL_ATTACK, i) {
                    healthy = true;
                }
                if q.flag(EE_SCREENS_CURSOR_OP_SEL_COMPLEX, i) {
                    let at = q.point(EE_SCREENS_CURSOR_OP_SEL_POSITION, i, 0);
                    let range = q.float(EE_SCREENS_CURSOR_OP_SEL_MIN_RANGE, i, 0);
                    if at.distance_to_squared(aim) >= range * range {
                        in_range = true;
                    }
                }
            }
        } else if q.flag(EE_SCREENS_CURSOR_OP_SEL_AIR_BASE, i) {
            let cargo = q.int(EE_SCREENS_CURSOR_OP_SEL_CARGO, i, 0) as u32;
            for j in 0..cargo {
                if q.int(EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK, i, j) == 0 {
                    continue;
                }
                can_attack = true;
                if q.int(EE_SCREENS_CURSOR_OP_CARGO_ATTACK, i, j) != 0 {
                    healthy = true;
                }
                let at = q.point(EE_SCREENS_CURSOR_OP_CARGO_POSITION, i, j);
                let range = q.float(EE_SCREENS_CURSOR_OP_CARGO_MIN_RANGE, i, j);
                if at.distance_to_squared(aim) >= range * range {
                    in_range = true;
                }
            }
        }
    }
    if can_attack {
        if in_range {
            1
        } else {
            3
        }
    } else if healthy {
        2
    } else {
        0
    }
}

/// `Fn265D7A` (0x665d7a): a military selection's cursor: enter, patrol point or the attack range kind.
fn military_command<H: CursorHost + ?Sized>(q: &mut Q<'_, H>, command: u32, flags: u32, allow_dead: bool) -> i32 {
    let mut result = 0;
    if q.t(EE_SCREENS_CURSOR_OP_T_TARGETABLE) {
        match q.int(EE_SCREENS_CURSOR_OP_T_OWNER, 0, 0) {
            0 => {
                if can_enter_unit(flags, command) && q.t(EE_SCREENS_CURSOR_OP_T_ENTERABLE) {
                    result = 0xb;
                } else if command == 0x20 {
                    result = 0x14;
                }
            }
            1 => {
                result = attack_range_kind(q, command, allow_dead);
                if result == 1 && allow_dead && flags & KIND_LONG_RANGE == KIND_LONG_RANGE {
                    result = 7;
                }
            }
            _ => {}
        }
    }
    result
}

/// `GetRepairCommand` (0x665e35): a fishing boat selection's gather cursor.
fn repair_command<H: CursorHost + ?Sized>(q: &mut Q<'_, H>) -> i32 {
    if q.t(EE_SCREENS_CURSOR_OP_T_TARGETABLE) && q.flag(EE_SCREENS_CURSOR_OP_CAN_GATHER, KIND_NAVAL_CITIZEN) {
        4
    } else {
        0
    }
}

/// `GetTransportCommand` (0x665e89): a priest or medic selection's cursor: heal, enter, convert, shielded or attack.
fn transport_command<H: CursorHost + ?Sized>(q: &mut Q<'_, H>, command: u32, flags: u32, allow_dead: bool) -> i32 {
    let mut result = 0;
    if q.t(EE_SCREENS_CURSOR_OP_T_TARGETABLE) {
        let owner = q.int(EE_SCREENS_CURSOR_OP_T_OWNER, 0, 0);
        if owner != 1 {
            if q.t(EE_SCREENS_CURSOR_OP_CAN_HEAL) {
                result = 0x12;
            }
            if owner == 0 && can_enter_unit(flags, command) && q.t(EE_SCREENS_CURSOR_OP_T_ENTERABLE) {
                result = 0xb;
            }
        } else if q.t(EE_SCREENS_CURSOR_OP_T_COMPLEX) {
            let (mut convert, mut shielded) = (false, false);
            let mut i = 0;
            while i < q.int(EE_SCREENS_CURSOR_OP_SEL_COUNT, 0, 0) as u32 {
                if convert || shielded {
                    break;
                }
                if !q.flag(EE_SCREENS_CURSOR_OP_SEL_CRUSADER, i) {
                    convert = if q.flag(EE_SCREENS_CURSOR_OP_SEL_PRIEST, i) {
                        let r = q.int(EE_SCREENS_CURSOR_OP_SEL_CONVERT, i, shielded as u32);
                        shielded = r & 2 != 0;
                        r & 1 != 0
                    } else {
                        false
                    };
                }
                i += 1;
            }
            result = if convert {
                10
            } else if shielded {
                0x10
            } else {
                attack_range_kind(q, command, allow_dead)
            };
        }
    }
    result
}

#[cfg(test)]
mod tests;
