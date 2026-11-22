//! The scenario host's reads and the pending population write that were C++ statement sequences
//! (Scenario Manager/EEScenarioHost.cpp's `Read` and `Write`; the scenario editor lane's round 4): a tech node's
//! flags and availability, an object's evaluation, a trigger's flags, a unit set's test, an object's first unit's
//! type, the message tokens' objects, values and conditions, another condition's evaluation, the population effect
//! kept pending. Composed here of single C++ statements (`EE_SCENARIO_RX_*`, `SMEReadWorld` in EEScenarioHost.cpp,
//! reached through the world channel, which takes the objects' handles), in the C++'s order.

use std::os::raw::c_ulong;

use crate::scenario::ca_compose::{
    EE_SCENARIO_WK_EFFECT_ADD, EE_SCENARIO_WK_EFFECT_FIELDS, EE_SCENARIO_WK_EFFECT_FREE, EE_SCENARIO_WK_EFFECT_NEW,
};
use crate::scenario::fields::EeScenarioEffectFields;
use crate::scenario::host::*;
use crate::scenario::player::EE_SCENARIO_WRITE_PENDING_POPULATION;
use crate::scenario::tokens::{
    EE_SCENARIO_READ_TOKEN_CONDITION, EE_SCENARIO_READ_TOKEN_OBJECT, EE_SCENARIO_READ_TOKEN_VALUE,
};

/// Player a0's tech node a1 (`GetTechTree()->Get(a1)`); node a0's `mData->mMakesUnit`, `mIsResearched`,
/// `mIsResearched = a1 != 0`, `GetIsAvailable()`.
pub const EE_SCENARIO_RX_TECH_NODE: u32 = 770;
pub const EE_SCENARIO_RX_NODE_MAKES_UNIT: u32 = 771;
pub const EE_SCENARIO_RX_NODE_RESEARCHED: u32 = 772;
pub const EE_SCENARIO_RX_NODE_SET_RESEARCHED: u32 = 773;
pub const EE_SCENARIO_RX_NODE_AVAILABLE: u32 = 774;
/// The manager's `GetScenarioObject(a0)`, `FindObjectByID(a0)`; object a0's `Evaluate(0)`, `mHasAttributeCheck`,
/// `mUnits.size()`, `GetUnitAt(a1)`, `GetAttribute()`, `GetIsAttributePercent()`, `GetUnitAttribute(a1, a2 != 0,
/// unit a3, 0, 0)`, `GetUnits(the id list)`.
pub const EE_SCENARIO_RX_OBJECT: u32 = 775;
pub const EE_SCENARIO_RX_FIND_OBJECT: u32 = 776;
pub const EE_SCENARIO_RX_OBJECT_EVALUATE: u32 = 777;
pub const EE_SCENARIO_RX_OBJECT_HAS_ATTRIBUTE_CHECK: u32 = 778;
pub const EE_SCENARIO_RX_OBJECT_UNIT_COUNT: u32 = 779;
pub const EE_SCENARIO_RX_OBJECT_UNIT_AT: u32 = 780;
pub const EE_SCENARIO_RX_OBJECT_ATTRIBUTE: u32 = 781;
pub const EE_SCENARIO_RX_OBJECT_ATTRIBUTE_PERCENT: u32 = 782;
pub const EE_SCENARIO_RX_OBJECT_UNIT_ATTRIBUTE: u32 = 783;
pub const EE_SCENARIO_RX_OBJECT_GET_UNITS: u32 = 784;
/// The id list (the C++'s local vector) cleared, its size, its element a0.
pub const EE_SCENARIO_RX_IDS_CLEAR: u32 = 785;
pub const EE_SCENARIO_RX_IDS_COUNT: u32 = 786;
pub const EE_SCENARIO_RX_IDS_AT: u32 = 787;
/// `gWorld.GetUnit(a0)`; unit a0's `mUnitClass->GetUnitType()`.
pub const EE_SCENARIO_RX_UNIT: u32 = 788;
pub const EE_SCENARIO_RX_UNIT_TYPE: u32 = 789;
/// The manager's `GetTrigger(a0)`; trigger a0's `mHasFired`, `mIsOn`.
pub const EE_SCENARIO_RX_TRIGGER: u32 = 790;
pub const EE_SCENARIO_RX_TRIGGER_FIRED: u32 = 791;
pub const EE_SCENARIO_RX_TRIGGER_ON: u32 = 792;
/// `gUnitSetManager.GetSet(a0)`; set a0's `Contains(a1)`.
pub const EE_SCENARIO_RX_UNIT_SET: u32 = 793;
pub const EE_SCENARIO_RX_SET_CONTAINS: u32 = 794;
/// The manager's `GetCondition(a0)`, `FindConditionByID(a0)`; condition a0's `Evaluate()`, `mType`, `mPlayer`,
/// `mAttribute`.
pub const EE_SCENARIO_RX_CONDITION: u32 = 795;
pub const EE_SCENARIO_RX_FIND_CONDITION: u32 = 796;
pub const EE_SCENARIO_RX_CONDITION_EVALUATE: u32 = 797;
pub const EE_SCENARIO_RX_CONDITION_TYPE: u32 = 798;
pub const EE_SCENARIO_RX_CONDITION_PLAYER: u32 = 799;
pub const EE_SCENARIO_RX_CONDITION_ATTRIBUTE: u32 = 800;
/// Effect a0's `Copy(*the context's effect)`.
pub const EE_SCENARIO_RX_EFFECT_COPY: u32 = 801;

/// `kSMConditionPlayerAttribute`
const CONDITION_PLAYER_ATTRIBUTE: i64 = 2;

/// The read `op` composed of the world channel's statements `w`, or `None` for the C++'s.
pub fn compose(w: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, a: i32, b: i32) -> Option<i64> {
    let mut x = |op: u32, args: &[i64]| -> i64 {
        let mut v = [0i64; 6];
        v[..args.len()].copy_from_slice(args);
        w(op, v)
    };
    let (a, b) = (a as i64, b as i64);
    Some(match op {
        EE_SCENARIO_READ_TECH_NODE => {
            let node = x(EE_SCENARIO_RX_TECH_NODE, &[a, b]);
            if node == 0 {
                -1
            } else {
                (x(EE_SCENARIO_RX_NODE_MAKES_UNIT, &[node]) != 0) as i64
                    | ((x(EE_SCENARIO_RX_NODE_RESEARCHED, &[node]) != 0) as i64) << 1
            }
        }
        EE_SCENARIO_READ_TECH_AVAILABLE => {
            // available as if not researched: the flag dropped for the test and put back
            let node = x(EE_SCENARIO_RX_TECH_NODE, &[a, b]);
            let researched = x(EE_SCENARIO_RX_NODE_RESEARCHED, &[node]);
            x(EE_SCENARIO_RX_NODE_SET_RESEARCHED, &[node, 0]);
            let available = x(EE_SCENARIO_RX_NODE_AVAILABLE, &[node]);
            x(EE_SCENARIO_RX_NODE_SET_RESEARCHED, &[node, (researched != 0) as i64]);
            (available != 0) as i64
        }
        EE_SCENARIO_READ_OBJECT => {
            let object = x(EE_SCENARIO_RX_OBJECT, &[a]);
            if object == 0 {
                -1
            } else {
                x(EE_SCENARIO_RX_OBJECT_EVALUATE, &[object])
            }
        }
        EE_SCENARIO_READ_TRIGGER => {
            let trigger = x(EE_SCENARIO_RX_TRIGGER, &[a]);
            if trigger == 0 {
                -1
            } else {
                (x(EE_SCENARIO_RX_TRIGGER_FIRED, &[trigger]) != 0) as i64
                    | ((x(EE_SCENARIO_RX_TRIGGER_ON, &[trigger]) != 0) as i64) << 1
            }
        }
        EE_SCENARIO_READ_UNIT_SET_HAS => {
            let set = x(EE_SCENARIO_RX_UNIT_SET, &[a]);
            if set == 0 {
                -1
            } else {
                x(EE_SCENARIO_RX_SET_CONTAINS, &[set, b])
            }
        }
        EE_SCENARIO_READ_OBJECT_FIRST_UNIT_TYPE => {
            x(EE_SCENARIO_RX_IDS_CLEAR, &[]);
            let object = x(EE_SCENARIO_RX_OBJECT, &[a]);
            x(EE_SCENARIO_RX_OBJECT_GET_UNITS, &[object]);
            let unit = if x(EE_SCENARIO_RX_IDS_COUNT, &[]) > 0 {
                let id = x(EE_SCENARIO_RX_IDS_AT, &[0]);
                x(EE_SCENARIO_RX_UNIT, &[id])
            } else {
                0
            };
            if unit != 0 {
                x(EE_SCENARIO_RX_UNIT_TYPE, &[unit])
            } else {
                -1
            }
        }
        EE_SCENARIO_READ_TOKEN_OBJECT => {
            let object = x(EE_SCENARIO_RX_FIND_OBJECT, &[a]);
            if object == 0 || (b != 0 && x(EE_SCENARIO_RX_OBJECT_HAS_ATTRIBUTE_CHECK, &[object]) == 0) {
                return Some(-1);
            }
            x(EE_SCENARIO_RX_OBJECT_EVALUATE, &[object]);
            x(EE_SCENARIO_RX_OBJECT_UNIT_COUNT, &[object])
        }
        EE_SCENARIO_READ_TOKEN_VALUE => {
            let object = x(EE_SCENARIO_RX_FIND_OBJECT, &[a]);
            let attribute = x(EE_SCENARIO_RX_OBJECT_ATTRIBUTE, &[object]);
            let percent = x(EE_SCENARIO_RX_OBJECT_ATTRIBUTE_PERCENT, &[object]);
            let id = x(EE_SCENARIO_RX_OBJECT_UNIT_AT, &[object, 0]);
            let unit = x(EE_SCENARIO_RX_UNIT, &[id]);
            x(EE_SCENARIO_RX_OBJECT_UNIT_ATTRIBUTE, &[object, attribute, percent, unit])
        }
        EE_SCENARIO_READ_TOKEN_CONDITION => {
            let condition = x(EE_SCENARIO_RX_FIND_CONDITION, &[a]);
            if condition == 0 || x(EE_SCENARIO_RX_CONDITION_TYPE, &[condition]) != CONDITION_PLAYER_ATTRIBUTE {
                return Some(-1);
            }
            let field = if b == 0 { EE_SCENARIO_RX_CONDITION_PLAYER } else { EE_SCENARIO_RX_CONDITION_ATTRIBUTE };
            x(field, &[condition])
        }
        EE_SCENARIO_READ_CONDITION => {
            let condition = x(EE_SCENARIO_RX_CONDITION, &[a]);
            if condition == 0 {
                -1
            } else {
                x(EE_SCENARIO_RX_CONDITION_EVALUATE, &[condition])
            }
        }
        _ => return None,
    })
}

/// The write `op` composed (the pending population effect: a copy of the context's effect for player b, mode 0,
/// amount c, kept pending), or `false` for the C++'s.
pub fn compose_write(w: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, b: i64, c: i64) -> bool {
    if op != EE_SCENARIO_WRITE_PENDING_POPULATION {
        return false;
    }
    let mut x = |op: u32, a0: i64| w(op, [a0, 0, 0, 0, 0, 0]);
    let effect = x(EE_SCENARIO_WK_EFFECT_NEW, 0);
    x(EE_SCENARIO_RX_EFFECT_COPY, effect);
    let e = x(EE_SCENARIO_WK_EFFECT_FIELDS, effect) as usize as *mut EeScenarioEffectFields;
    // SAFETY: the new effect's record, written before the next C++ call
    unsafe {
        (*e).mPlayer = b as c_ulong;
        (*e).mPlayerAttributeMode = 0;
        (*e).mPlayerAttributeAmount = c as c_ulong;
    }
    x(EE_SCENARIO_WK_EFFECT_ADD, effect);
    x(EE_SCENARIO_WK_EFFECT_FREE, effect);
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A fake world: handles 100 (node), 200 (object), 300 (trigger), 400 (set), 500 (condition), 600 (unit); the
    /// statements in order.
    fn run(op: u32, a: i32, b: i32, answer: &dyn Fn(u32, &[i64; 6]) -> Option<i64>) -> (Option<i64>, Vec<(u32, i64)>) {
        let mut ops = Vec::new();
        let r = {
            let mut w = |op: u32, v: [i64; 6]| -> i64 {
                ops.push((op, v[0]));
                answer(op, &v).unwrap_or(match op {
                    EE_SCENARIO_RX_TECH_NODE => 100,
                    EE_SCENARIO_RX_OBJECT | EE_SCENARIO_RX_FIND_OBJECT => 200,
                    EE_SCENARIO_RX_TRIGGER => 300,
                    EE_SCENARIO_RX_UNIT_SET => 400,
                    EE_SCENARIO_RX_CONDITION | EE_SCENARIO_RX_FIND_CONDITION => 500,
                    EE_SCENARIO_RX_UNIT => 600,
                    _ => 1,
                })
            };
            compose(&mut w, op, a, b)
        };
        (r, ops)
    }

    fn none(_: u32, _: &[i64; 6]) -> Option<i64> {
        None
    }

    #[test]
    fn tech_reads() {
        assert_eq!(run(EE_SCENARIO_READ_TECH_NODE, 1, 7, &none).0, Some(3));
        assert_eq!(
            run(EE_SCENARIO_READ_TECH_NODE, 1, 7, &|op, _| (op == EE_SCENARIO_RX_TECH_NODE).then_some(0)).0,
            Some(-1)
        );
        let (r, ops) =
            run(EE_SCENARIO_READ_TECH_AVAILABLE, 1, 7, &|op, _| (op == EE_SCENARIO_RX_NODE_AVAILABLE).then_some(0));
        assert_eq!(r, Some(0));
        let order: Vec<u32> = ops.iter().map(|o| o.0).collect();
        assert_eq!(
            order,
            vec![
                EE_SCENARIO_RX_TECH_NODE,
                EE_SCENARIO_RX_NODE_RESEARCHED,
                EE_SCENARIO_RX_NODE_SET_RESEARCHED,
                EE_SCENARIO_RX_NODE_AVAILABLE,
                EE_SCENARIO_RX_NODE_SET_RESEARCHED
            ]
        );
    }

    #[test]
    fn objects_triggers_sets_conditions() {
        assert_eq!(
            run(EE_SCENARIO_READ_OBJECT, 2, 0, &|op, _| (op == EE_SCENARIO_RX_OBJECT_EVALUATE).then_some(4)).0,
            Some(4)
        );
        assert_eq!(run(EE_SCENARIO_READ_OBJECT, 2, 0, &|op, _| (op == EE_SCENARIO_RX_OBJECT).then_some(0)).0, Some(-1));
        assert_eq!(
            run(EE_SCENARIO_READ_TRIGGER, 2, 0, &|op, _| (op == EE_SCENARIO_RX_TRIGGER_FIRED).then_some(0)).0,
            Some(2)
        );
        assert_eq!(run(EE_SCENARIO_READ_UNIT_SET_HAS, 2, 9, &none).0, Some(1));
        assert_eq!(
            run(EE_SCENARIO_READ_CONDITION, 2, 0, &|op, _| (op == EE_SCENARIO_RX_CONDITION).then_some(0)).0,
            Some(-1)
        );
        assert_eq!(run(EE_SCENARIO_READ_CONDITION + 1000, 0, 0, &none).0, None);
    }

    #[test]
    fn first_unit_type() {
        let (r, ops) = run(EE_SCENARIO_READ_OBJECT_FIRST_UNIT_TYPE, 2, 0, &|op, _| match op {
            EE_SCENARIO_RX_IDS_COUNT => Some(2),
            EE_SCENARIO_RX_IDS_AT => Some(55),
            EE_SCENARIO_RX_UNIT_TYPE => Some(17),
            _ => None,
        });
        assert_eq!(r, Some(17));
        assert!(ops.contains(&(EE_SCENARIO_RX_UNIT, 55)));
        assert_eq!(ops[0].0, EE_SCENARIO_RX_IDS_CLEAR);
        let empty =
            run(EE_SCENARIO_READ_OBJECT_FIRST_UNIT_TYPE, 2, 0, &|op, _| (op == EE_SCENARIO_RX_IDS_COUNT).then_some(0));
        assert_eq!(empty.0, Some(-1));
    }

    #[test]
    fn tokens() {
        let (r, ops) = run(EE_SCENARIO_READ_TOKEN_OBJECT, 5, 1, &|op, _| match op {
            EE_SCENARIO_RX_OBJECT_UNIT_COUNT => Some(3),
            _ => None,
        });
        assert_eq!(r, Some(3));
        assert!(ops.iter().any(|o| o.0 == EE_SCENARIO_RX_OBJECT_EVALUATE));
        assert_eq!(
            run(EE_SCENARIO_READ_TOKEN_OBJECT, 5, 1, &|op, _| (op == EE_SCENARIO_RX_OBJECT_HAS_ATTRIBUTE_CHECK)
                .then_some(0))
            .0,
            Some(-1)
        );
        let (r, ops) = run(EE_SCENARIO_READ_TOKEN_VALUE, 5, 0, &|op, v| match op {
            EE_SCENARIO_RX_OBJECT_ATTRIBUTE => Some(12),
            EE_SCENARIO_RX_OBJECT_UNIT_ATTRIBUTE => Some(v[1] * 10 + v[2] + v[3]),
            _ => None,
        });
        assert_eq!(r, Some(120 + 1 + 600));
        assert_eq!(ops.last().unwrap().0, EE_SCENARIO_RX_OBJECT_UNIT_ATTRIBUTE);
        let cond = |op: u32, _: &[i64; 6]| match op {
            EE_SCENARIO_RX_CONDITION_TYPE => Some(CONDITION_PLAYER_ATTRIBUTE),
            EE_SCENARIO_RX_CONDITION_PLAYER => Some(4),
            EE_SCENARIO_RX_CONDITION_ATTRIBUTE => Some(9),
            _ => None,
        };
        assert_eq!(run(EE_SCENARIO_READ_TOKEN_CONDITION, 5, 0, &cond).0, Some(4));
        assert_eq!(run(EE_SCENARIO_READ_TOKEN_CONDITION, 5, 1, &cond).0, Some(9));
        assert_eq!(run(EE_SCENARIO_READ_TOKEN_CONDITION, 5, 1, &none).0, Some(-1));
    }

    #[test]
    fn pending_population_copies_the_effect() {
        let mut e = EeScenarioEffectFields { mPlayerAttributeMode: 3, ..Default::default() };
        let ep = &mut e as *mut EeScenarioEffectFields as usize as i64;
        let mut ops = Vec::new();
        let done = {
            let mut w = |op: u32, v: [i64; 6]| -> i64 {
                ops.push((op, v[0]));
                match op {
                    EE_SCENARIO_WK_EFFECT_NEW => 40,
                    EE_SCENARIO_WK_EFFECT_FIELDS => ep,
                    _ => 0,
                }
            };
            assert!(!compose_write(&mut w, EE_SCENARIO_WRITE_PENDING_POPULATION + 1, 0, 0));
            compose_write(&mut w, EE_SCENARIO_WRITE_PENDING_POPULATION, 2, 150)
        };
        assert!(done);
        assert_eq!((e.mPlayer, e.mPlayerAttributeMode, e.mPlayerAttributeAmount), (2, 0, 150));
        let order: Vec<u32> = ops.iter().map(|o| o.0).collect();
        assert_eq!(
            order,
            vec![
                EE_SCENARIO_WK_EFFECT_NEW,
                EE_SCENARIO_RX_EFFECT_COPY,
                EE_SCENARIO_WK_EFFECT_FIELDS,
                EE_SCENARIO_WK_EFFECT_ADD,
                EE_SCENARIO_WK_EFFECT_FREE
            ]
        );
    }
}
