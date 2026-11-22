use super::*;

/// A fake trigger: its texts, numbers and two entry lists of (first, second, operator).
#[derive(Default, Clone)]
struct Trig {
    name: String,
    text: String,
    f: EeScenarioTriggerFields,
    lists: [Vec<[i64; 3]>; 2],
    deleted: usize,
}

/// Triggers 1.. (handle t is index t - 1); the timed effects added; the statements in order.
#[derive(Default)]
struct World {
    t: Vec<Trig>,
    timed: Vec<(i64, i64, bool)>,
    ops: Vec<u32>,
    cinematic: bool,
}

impl World {
    fn run<R>(&mut self, f: impl FnOnce(&mut TrX) -> R) -> R {
        let me = self as *mut World;
        let mut x = |op: u32, t: i64, i: i64, v: i64| -> i64 {
            // SAFETY: the test's own world, alive through the call
            let w = unsafe { &mut *me };
            w.ops.push(op);
            let ti = (t - 1) as usize;
            if op >= EE_SCENARIO_TR_NAME_EMPTY {
                return match op {
                    EE_SCENARIO_TR_NAME_EMPTY => {
                        w.t[ti].name.clear();
                        0
                    }
                    EE_SCENARIO_TR_TEXT_EMPTY => {
                        w.t[ti].text.clear();
                        0
                    }
                    EE_SCENARIO_TR_COPY_NAME => {
                        w.t[ti].name = w.t[(i - 1) as usize].name.clone();
                        0
                    }
                    EE_SCENARIO_TR_COPY_TEXT => {
                        w.t[ti].text = w.t[(i - 1) as usize].text.clone();
                        0
                    }
                    EE_SCENARIO_TR_FIELDS => &mut w.t[ti].f as *mut _ as usize as i64,
                    EE_SCENARIO_TR_ADD_TIMED => {
                        let c = w.t[ti].f.mIsInCinematic;
                        w.timed.push((i, v, c));
                        0
                    }
                    EE_SCENARIO_TR_WORLD_CINEMATIC => w.cinematic as i64,
                    _ => panic!("op {op}"),
                };
            }
            let k = (op >= EE_SCENARIO_TR_EFFECTS) as usize;
            let n = op - if k == 1 { EE_SCENARIO_TR_EFFECTS } else { EE_SCENARIO_TR_CONDITIONS };
            let tr = &mut w.t[ti];
            let l = &mut tr.lists[k];
            let iu = i as usize;
            match n {
                EE_SCENARIO_TR_COUNT => l.len() as i64,
                EE_SCENARIO_TR_PUSH_NEW => {
                    l.push([0, 0, 0]);
                    0
                }
                EE_SCENARIO_TR_BACK_EXISTS => 1,
                EE_SCENARIO_TR_DELETE_BACK | EE_SCENARIO_TR_DELETE_AT => {
                    tr.deleted += 1;
                    0
                }
                EE_SCENARIO_TR_POP => {
                    l.pop();
                    0
                }
                EE_SCENARIO_TR_ERASE => {
                    l.remove(iu);
                    0
                }
                EE_SCENARIO_TR_SET_FIRST => {
                    l[iu][0] = if k == 0 { (v != 0) as i64 } else { v };
                    0
                }
                EE_SCENARIO_TR_SET_SECOND => {
                    l[iu][1] = v;
                    0
                }
                EE_SCENARIO_TR_SET_OPERATOR => {
                    l[iu][2] = v;
                    0
                }
                EE_SCENARIO_TR_FIRST => l[iu][0],
                EE_SCENARIO_TR_SECOND => l[iu][1],
                EE_SCENARIO_TR_OPERATOR => l[iu][2],
                EE_SCENARIO_TR_AT => 1000 * (k as i64 + 1) + i,
                _ => panic!("op {op}"),
            }
        };
        f(&mut x)
    }
}

fn world(n: usize) -> World {
    World { t: vec![Trig::default(); n], ..Default::default() }
}

#[test]
fn construct_makes_one_entry_of_each_kind_and_destroy_frees_them() {
    let mut w = world(1);
    w.t[0].name = "x".into();
    w.t[0].f.mID = 9;
    w.t[0].f.mHasFired = true;
    w.run(|x| member(x, 1, EE_SCENARIO_TRM_CONSTRUCT, 0, 0, 0, 0));
    let t = &w.t[0];
    assert_eq!((t.name.as_str(), t.f.mIsOn, t.f.mHasFired, t.f.mID), ("", true, false, 0));
    assert_eq!(t.lists, [vec![[0, 0, 0]], vec![[0, 0, 0]]]);
    w.run(|x| member(x, 1, EE_SCENARIO_TRM_DESTROY, 0, 0, 0, 0));
    assert_eq!((w.t[0].lists[0].len(), w.t[0].lists[1].len(), w.t[0].deleted), (0, 0, 2));
}

#[test]
fn an_operator_on_the_last_entry_appends_one_and_deleting_the_last_ends_the_chain() {
    let mut w = world(1);
    w.run(|x| construct(x, 1));
    w.run(|x| {
        set_operator(x, 1, 0, 0, 1);
        set_entry(x, 1, 0, 1, 1, 7);
        set_operator(x, 1, 0, 1, 2);
    });
    assert_eq!(w.t[0].lists[0], vec![[0, 0, 1], [1, 7, 2], [0, 0, 0]]);
    // an operator 0 on the last appends nothing
    w.run(|x| set_operator(x, 1, 0, 2, 0));
    assert_eq!(w.t[0].lists[0].len(), 3);
    w.run(|x| delete_entry(x, 1, 0, 2));
    assert_eq!(w.t[0].lists[0], vec![[0, 0, 1], [1, 7, 0]]);
    // deleting a middle entry leaves the last's operator
    w.run(|x| delete_entry(x, 1, 0, 0));
    assert_eq!(w.t[0].lists[0], vec![[1, 7, 0]]);
    assert_eq!(w.run(|x| get_entry(x, 1, 1, 0)), 2000);
    assert_eq!(w.run(|x| get_entry(x, 1, 1, 1)), 0);
}

#[test]
fn copy_takes_texts_entries_and_numbers_but_the_start_time() {
    let mut w = world(2);
    w.run(|x| {
        construct(x, 1);
        construct(x, 2);
        set_entry(x, 2, 1, 0, 30, 4);
        set_operator(x, 2, 1, 0, 1);
        set_entry(x, 2, 1, 1, 0, 5);
        set_operator(x, 2, 0, 0, 0);
        // trigger 1 has three conditions to drop
        set_operator(x, 1, 0, 0, 1);
        set_operator(x, 1, 0, 1, 1);
    });
    w.t[1].name = "src".into();
    w.t[1].text = "txt".into();
    w.t[1].f = EeScenarioTriggerFields {
        mConditionDuration: 6,
        mConditionStartTime: 77,
        mID: 3,
        mIsOn: false,
        mHasFired: true,
        mIsLooping: true,
        mIsInCinematic: true,
    };
    w.run(|x| member(x, 1, EE_SCENARIO_TRM_COPY, 0, 2, 0, 0));
    let t = &w.t[0];
    assert_eq!((t.name.as_str(), t.text.as_str()), ("src", "txt"));
    assert_eq!(t.lists[0], vec![[0, 0, 0]]);
    assert_eq!(t.lists[1], vec![[30, 4, 1], [0, 5, 0]]);
    assert_eq!((t.f.mConditionDuration, t.f.mConditionStartTime, t.f.mID), (6, 0, 3));
    assert_eq!((t.f.mIsOn, t.f.mHasFired, t.f.mIsLooping, t.f.mIsInCinematic), (false, true, true, true));
}

/// A host whose condition reads answer 1 (true).
struct Conditions;

impl Host for Conditions {
    fn read4(&mut self, _op: u32, _a: i32, _b: i32, _c: i32, _d: i32) -> i64 {
        1
    }
}

#[test]
fn evaluate_fires_and_adds_the_timed_effects() {
    let mut w = world(1);
    w.run(|x| {
        construct(x, 1);
        set_entry(x, 1, 1, 0, 2, 8);
    });
    w.t[0].f.mIsInCinematic = true;
    w.run(|x| evaluate(&mut Conditions, x, 1, 10.0));
    assert!(w.t[0].f.mHasFired);
    assert_eq!(w.timed.len(), 1);
    assert_eq!((w.timed[0].0, w.timed[0].2), (8, true));
    // the state is read before the entries and written after them
    let first_fields = w.ops.iter().position(|&o| o == EE_SCENARIO_TR_FIELDS).unwrap();
    let cinematic = w.ops.iter().position(|&o| o == EE_SCENARIO_TR_WORLD_CINEMATIC).unwrap();
    assert!(first_fields < cinematic);
    // an off trigger does nothing
    let mut w = world(1);
    w.run(|x| construct(x, 1));
    w.t[0].f.mIsOn = false;
    w.run(|x| evaluate(&mut Conditions, x, 1, 10.0));
    assert!(w.timed.is_empty() && !w.t[0].f.mHasFired);
}
