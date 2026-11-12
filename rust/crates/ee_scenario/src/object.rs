//! A scenario object's unit class filter (`EESMObject::AcceptsClass`) and its choice of a class to create
//! (`PickUnitType`) over the host's reads of the unit class table and the unit sets.

use crate::host::*;

/// `kUnitCategory*` a scenario object never takes: projectiles, bombs, missiles, torpedoes.
const EE_SCENARIO_K_SHOTS: [u32; 4] = [9, 0xb, 0xc, 0x14];
/// The unit set `AcceptsClass` leaves out (by category).
const EE_SCENARIO_K_EXCLUDED_SET: i32 = 0x79;

/// The object's filter (`EESMObject`'s plain members).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioObjectFilter {
    pub class: u32,
    pub family: u32,
    pub parent: u32,
}

/// The family filter's part after the class's own tests: epoch (1..0xf), family (0x10..0x4b), AI kind
/// (0x4c..0x74), a unit set (past 0x74; `type_` is what the set must contain); `None` when the filter's value is
/// past the last set (the caller keeps its answer).
fn family_matches<H: Host>(host: &mut H, family: u32, class: i32, set_value: i32) -> Option<bool> {
    if family == 0 {
        Some(true)
    } else if family <= 0xf {
        Some(family == (host.read(EE_SCENARIO_READ_CLASS_EPOCH, class, 0) as u32).wrapping_add(1))
    } else if family <= 0x4b {
        Some(family - 0x10 == host.read(EE_SCENARIO_READ_CLASS_FAMILY, class, 0) as u32)
    } else if family <= 0x74 {
        Some(family - 0x4c == host.read(EE_SCENARIO_READ_CLASS_AI_KIND, class, 0) as u32)
    } else if family <= (host.read(EE_SCENARIO_READ_UNIT_SET_COUNT, 0, 0) as u32).wrapping_add(0x74) {
        let has = host.read(EE_SCENARIO_READ_UNIT_SET_HAS, (family - 0x75) as i32, set_value);
        if has < 0 {
            None
        } else {
            Some(has != 0)
        }
    } else {
        None
    }
}

fn is_shot(category: u32) -> bool {
    EE_SCENARIO_K_SHOTS.contains(&category)
}

/// `EESMObject::AcceptsClass`: whether a unit class passes the object's filter.
pub fn accepts_class<H: Host>(host: &mut H, o: &EeScenarioObjectFilter, class: u32) -> bool {
    let c = class as i32;
    let mut result =
        if o.class != 0 { o.class == class } else { host.read(EE_SCENARIO_READ_CLASS_SELECTABLE, c, 0) != 0 };
    if result && host.read(EE_SCENARIO_READ_CLASS_EXISTS, c, 0) != 0 {
        let category = host.read(EE_SCENARIO_READ_CLASS_CATEGORY, c, 0) as u32;
        if is_shot(category)
            || host.read(EE_SCENARIO_READ_CATEGORY_IN_SET, category as i32, EE_SCENARIO_K_EXCLUDED_SET) != 0
        {
            result = false;
        } else if let Some(m) = family_matches(host, o.family, c, c) {
            result = m;
        }
    }
    result
}

/// `EESMObject::PickUnitType`: the parent's first unit's class (a parent picking units on the map), the parent's
/// own pick, the object's class, or a random class of the filter; -1 for none.
pub fn pick_unit_type<H: Host>(host: &mut H, o: &EeScenarioObjectFilter) -> i32 {
    let parent = o.parent as i32;
    if host.read(EE_SCENARIO_READ_OBJECT_EXISTS, parent, 0) != 0 && o.parent != 0 {
        return if host.read(EE_SCENARIO_READ_OBJECT_SELECT_ON_MAP, parent, 0) != 0 {
            host.read(EE_SCENARIO_READ_OBJECT_FIRST_UNIT_TYPE, parent, 0) as i32
        } else {
            host.read(EE_SCENARIO_READ_OBJECT_PICK, parent, 0) as i32
        };
    }
    if o.class != 0 {
        return o.class as i32;
    }
    let mut classes = Vec::new();
    let count = host.read(EE_SCENARIO_READ_CLASS_COUNT, 0, 0) as i32;
    for i in 0..count {
        if host.read(EE_SCENARIO_READ_CLASS_EXISTS, i, 0) == 0 {
            continue;
        }
        if is_shot(host.read(EE_SCENARIO_READ_CLASS_CATEGORY, i, 0) as u32) {
            continue;
        }
        let kind = host.read(EE_SCENARIO_READ_CLASS_TYPE, i, 0) as i32;
        if family_matches(host, o.family, i, kind) == Some(true) {
            classes.push(kind);
        }
    }
    if classes.is_empty() {
        -1
    } else {
        classes[host.read(EE_SCENARIO_READ_RANDOM, 0, classes.len() as i32 - 1) as usize]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Classes: (exists, selectable, category, epoch, family, AI kind, type); one unit set {3, 5}; objects by
    /// index: (exists, picks on the map, first unit type, its own pick).
    struct World {
        classes: Vec<(bool, bool, u32, i32, u32, u32, i32)>,
        objects: Vec<(bool, bool, i64, i64)>,
        draws: Vec<i64>,
        log: Vec<(u32, i32, i32)>,
    }

    impl Host for World {
        fn read4(&mut self, op: u32, a: i32, b: i32, _c: i32, _d: i32) -> i64 {
            self.log.push((op, a, b));
            let c = self.classes.get(a as usize).copied().unwrap_or((false, false, 0, 0, 0, 0, 0));
            let o = self.objects.get(a as usize).copied().unwrap_or((false, false, -1, -1));
            match op {
                EE_SCENARIO_READ_CLASS_COUNT => self.classes.len() as i64,
                EE_SCENARIO_READ_CLASS_EXISTS => c.0 as i64,
                EE_SCENARIO_READ_CLASS_SELECTABLE => c.1 as i64,
                EE_SCENARIO_READ_CLASS_CATEGORY => c.2 as i64,
                EE_SCENARIO_READ_CLASS_EPOCH => c.3 as i64,
                EE_SCENARIO_READ_CLASS_FAMILY => c.4 as i64,
                EE_SCENARIO_READ_CLASS_AI_KIND => c.5 as i64,
                EE_SCENARIO_READ_CLASS_TYPE => c.6 as i64,
                EE_SCENARIO_READ_CATEGORY_IN_SET => (a == 7 && b == 0x79) as i64,
                EE_SCENARIO_READ_UNIT_SET_COUNT => 1,
                EE_SCENARIO_READ_UNIT_SET_HAS => {
                    if a == 0 {
                        (b == 3 || b == 5) as i64
                    } else {
                        -1
                    }
                }
                EE_SCENARIO_READ_OBJECT_EXISTS => o.0 as i64,
                EE_SCENARIO_READ_OBJECT_SELECT_ON_MAP => o.1 as i64,
                EE_SCENARIO_READ_OBJECT_FIRST_UNIT_TYPE => o.2,
                EE_SCENARIO_READ_OBJECT_PICK => o.3,
                EE_SCENARIO_READ_RANDOM => self.draws.remove(0),
                _ => panic!("unexpected read {op}"),
            }
        }
    }

    fn world() -> World {
        World {
            classes: vec![
                (true, true, 1, 0, 2, 3, 0),   // 0
                (true, false, 1, 4, 2, 3, 1),  // 1: not selectable
                (true, true, 9, 0, 2, 3, 2),   // 2: a projectile
                (true, true, 7, 0, 2, 3, 3),   // 3: its category in set 0x79
                (false, false, 0, 0, 0, 0, 4), // 4: none
                (true, true, 1, 5, 6, 8, 5),   // 5
            ],
            objects: vec![(true, true, -1, -1), (true, true, 42, -1), (true, false, -1, 17), (false, false, -1, -1)],
            draws: vec![],
            log: vec![],
        }
    }

    fn f(class: u32, family: u32, parent: u32) -> EeScenarioObjectFilter {
        EeScenarioObjectFilter { class, family, parent }
    }

    #[test]
    fn accepts_class_takes_the_class_or_the_selectable_then_the_family() {
        let mut w = world();
        assert!(accepts_class(&mut w, &f(0, 0, 0), 0));
        assert!(!accepts_class(&mut w, &f(0, 0, 0), 1));
        assert!(accepts_class(&mut w, &f(1, 0, 0), 1));
        assert!(!accepts_class(&mut w, &f(0, 0, 0), 2));
        assert!(!accepts_class(&mut w, &f(0, 0, 0), 3));
        // epoch 5 + 1, family 6 + 0x10, AI kind 8 + 0x4c, set 0 (classes 3 and 5)
        assert!(accepts_class(&mut w, &f(0, 6, 0), 5) && !accepts_class(&mut w, &f(0, 5, 0), 5));
        assert!(accepts_class(&mut w, &f(0, 0x16, 0), 5) && !accepts_class(&mut w, &f(0, 0x17, 0), 5));
        assert!(accepts_class(&mut w, &f(0, 0x54, 0), 5) && !accepts_class(&mut w, &f(0, 0x55, 0), 5));
        assert!(accepts_class(&mut w, &f(0, 0x75, 0), 5) && !accepts_class(&mut w, &f(0, 0x75, 0), 0));
        // past the sets: the class test stands
        assert!(accepts_class(&mut w, &f(0, 0x77, 0), 0));
        // a class the object names but the table lacks: accepted without the family tests
        assert!(accepts_class(&mut w, &f(4, 6, 0), 4));
    }

    #[test]
    fn pick_takes_the_parent_then_the_class_then_a_random_match() {
        let mut w = world();
        assert_eq!(pick_unit_type(&mut w, &f(0, 0, 1)), 42);
        assert_eq!(pick_unit_type(&mut w, &f(0, 0, 2)), 17);
        assert_eq!(pick_unit_type(&mut w, &f(9, 0, 0)), 9);
        // a missing parent, or parent 0: the object's own choice
        assert_eq!(pick_unit_type(&mut w, &f(9, 0, 3)), 9);
        // every class but the projectile and the missing one: 0, 1, 3, 5; draw 3 of 0..=3
        w.draws = vec![3];
        assert_eq!(pick_unit_type(&mut w, &f(0, 0, 0)), 5);
        assert_eq!(w.log.last(), Some(&(EE_SCENARIO_READ_RANDOM, 0, 3)));
        w.draws = vec![1];
        assert_eq!(pick_unit_type(&mut w, &f(0, 0x75, 0)), 5);
        assert_eq!(pick_unit_type(&mut w, &f(0, 0x30, 0)), -1);
        assert!(w.draws.is_empty());
    }
}
