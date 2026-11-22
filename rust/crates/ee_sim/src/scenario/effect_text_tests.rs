use super::*;

/// Runs `op` over a fake world: the effect record `e`, the answers `answer` gives (else 1 for handles), recording the
/// statements and their first two arguments.
fn run(
    op: u32,
    a: [i64; 6],
    e: &mut EeScenarioEffectFields,
    answer: &dyn Fn(u32, &[i64; 6]) -> Option<i64>,
) -> (Option<i64>, Vec<(u32, i64, i64)>) {
    let ep = e as *mut EeScenarioEffectFields as usize as i64;
    let mut ops = Vec::new();
    let r = {
        let mut cpp = |op: u32, v: [i64; 6]| -> i64 {
            ops.push((op, v[0], v[1]));
            if let Some(x) = answer(op, &v) {
                return x;
            }
            match op {
                EE_SCENARIO_WY_EFFECT_FIELDS | EE_SCENARIO_WK_EFFECT_FIELDS => ep,
                _ => 1,
            }
        };
        compose(&mut cpp, op, a)
    };
    (r, ops)
}

fn none(_: u32, _: &[i64; 6]) -> Option<i64> {
    None
}

fn ops(v: &[(u32, i64, i64)]) -> Vec<u32> {
    v.iter().map(|o| o.0).collect()
}

#[test]
fn other_ops_are_the_cpps() {
    let mut e = EeScenarioEffectFields::default();
    assert_eq!(run(EE_SCENARIO_WORLD_AREA_CENTER, [0; 6], &mut e, &none).0, None);
    assert_eq!(run(EE_SCENARIO_WORLD_CU_SET, [3, 0, 0, 0, 0, 0], &mut e, &none).0, None);
}

#[test]
fn flare_and_sound() {
    let mut e = EeScenarioEffectFields::default();
    let (_, o) = run(EE_SCENARIO_WORLD_FLARE, [2, 10, 11, 0, 0, 0], &mut e, &|op, _| {
        (op == EE_SCENARIO_WK_PLAYER).then_some(77)
    });
    assert_eq!(o, vec![(EE_SCENARIO_WK_PLAYER, 2, 0), (EE_SCENARIO_WY_FLARE, 77, 10)]);
    // the instance is "none": no registration
    let none_instance = |op: u32, v: &[i64; 6]| {
        if op == EE_SCENARIO_WY_SOUND_LOAD {
            // SAFETY: the composer's own two out values
            unsafe {
                *(v[0] as usize as *mut c_ulong) = 5;
                *(v[1] as usize as *mut c_ulong) = 0xffff_ffff;
            }
            Some(0)
        } else {
            None
        }
    };
    let (_, o) = run(EE_SCENARIO_WORLD_PLAY_SOUND, [0; 6], &mut e, &none_instance);
    assert_eq!(ops(&o), vec![EE_SCENARIO_WY_SOUND_LOAD]);
    let some_instance = |op: u32, v: &[i64; 6]| {
        if op == EE_SCENARIO_WY_SOUND_LOAD {
            // SAFETY: as above
            unsafe {
                *(v[0] as usize as *mut c_ulong) = 5;
                *(v[1] as usize as *mut c_ulong) = 9;
            }
            Some(0)
        } else {
            None
        }
    };
    let (_, o) = run(EE_SCENARIO_WORLD_PLAY_SOUND, [0; 6], &mut e, &some_instance);
    assert_eq!(o[1], (EE_SCENARIO_WY_SOUND_REGISTER, 5, 9));
}

#[test]
fn change_text_by_section() {
    let mut e = EeScenarioEffectFields::default();
    let (_, o) = run(EE_SCENARIO_WORLD_CHANGE_TEXT, [1, 0, 0, 0, 0, 0], &mut e, &|op, _| {
        (op == EE_SCENARIO_WY_SCI_FORM).then_some(40)
    });
    assert_eq!(
        o,
        vec![
            (EE_SCENARIO_WY_SCI_FORM, 0, 0),
            (EE_SCENARIO_WY_SECTION_APPEND, 3, 0),
            (EE_SCENARIO_WY_SOUND_OBJECT, SOUND_TRUMPETS, 0),
            (EE_SCENARIO_WY_SOUND_PLAY, 1, 0),
            (EE_SCENARIO_WY_FORM_BLINK, 40, 0)
        ]
    );
    let (_, o) = run(EE_SCENARIO_WORLD_CHANGE_TEXT, [2, 1, 0, 0, 0, 0], &mut e, &|op, _| {
        (op == EE_SCENARIO_WY_SOUND_OBJECT).then_some(0)
    });
    assert_eq!(
        ops(&o),
        vec![
            EE_SCENARIO_WY_SCI_FORM,
            EE_SCENARIO_WY_SECTION_CLEAR,
            EE_SCENARIO_WY_SOUND_OBJECT,
            EE_SCENARIO_WY_FORM_BLINK
        ]
    );
    assert_eq!(o[1].1, 2);
    // out of range: no section
    let (_, o) = run(EE_SCENARIO_WORLD_CHANGE_TEXT, [3, 0, 0, 0, 0, 0], &mut e, &none);
    assert!(!ops(&o).contains(&EE_SCENARIO_WY_SECTION_APPEND));
}

#[test]
fn dialogue_with_and_without_a_unit() {
    let mut e = EeScenarioEffectFields { mDuration: 4, mObject: 6, mIntValue1: -2, ..Default::default() };
    let with = |op: u32, _: &[i64; 6]| match op {
        EE_SCENARIO_WY_MSG_NEW => Some(50),
        EE_SCENARIO_RX_OBJECT => Some(60),
        EE_SCENARIO_RX_OBJECT_UNIT_COUNT => Some(2),
        EE_SCENARIO_RX_OBJECT_UNIT_AT => Some(123),
        EE_SCENARIO_RX_UNIT => Some(70),
        EE_SCENARIO_RX_UNIT_TYPE => Some(17),
        EE_SCENARIO_WY_UNIT_ID => Some(123),
        _ => None,
    };
    let (_, o) = run(EE_SCENARIO_WORLD_DIALOGUE, [1, 0, 0, 0, 0, 0], &mut e, &with);
    assert!(o.contains(&(EE_SCENARIO_WY_MSG_DURATION, 50, 4000)));
    assert!(o.contains(&(EE_SCENARIO_WY_MSG_CINEMATIC, 50, 1)));
    assert!(o.contains(&(EE_SCENARIO_RX_OBJECT, 6, 0)));
    assert!(o.contains(&(EE_SCENARIO_WY_MSG_UNIT_TYPE, 50, 17)));
    assert!(o.contains(&(EE_SCENARIO_WY_MSG_SCROLL, 50, -2)));
    assert!(o.contains(&(EE_SCENARIO_WY_MSG_UNIT_ID, 50, 123)));
    assert_eq!(
        ops(&o)[ops(&o).len() - 3..],
        [EE_SCENARIO_WY_MSG_SOUND, EE_SCENARIO_WY_MSG_ADD, EE_SCENARIO_WY_MSG_FREE]
    );
    let evaluate = ops(&o).iter().position(|&x| x == EE_SCENARIO_RX_OBJECT_EVALUATE).unwrap();
    let count = ops(&o).iter().position(|&x| x == EE_SCENARIO_RX_OBJECT_UNIT_COUNT).unwrap();
    assert!(evaluate < count);
    let without = |op: u32, v: &[i64; 6]| match op {
        EE_SCENARIO_RX_OBJECT_UNIT_COUNT => Some(0),
        _ => with(op, v),
    };
    let (_, o) = run(EE_SCENARIO_WORLD_DIALOGUE, [0; 6], &mut e, &without);
    assert!(o.contains(&(EE_SCENARIO_WY_MSG_UNIT_TYPE, 50, 0)));
    assert!(!ops(&o).contains(&EE_SCENARIO_WY_MSG_SCROLL));
}

#[test]
fn chat_calamity_and_pending() {
    let mut e = EeScenarioEffectFields { mDuration: 3, ..Default::default() };
    let (_, o) = run(EE_SCENARIO_WORLD_CHAT_LOG, [0; 6], &mut e, &none);
    assert_eq!(o.last().unwrap(), &(EE_SCENARIO_WY_CHAT_LOG, 3000, 0));
    let (_, o) = run(EE_SCENARIO_WORLD_CHAT_BOX, [0; 6], &mut e, &none);
    assert_eq!(ops(&o), vec![EE_SCENARIO_WY_CHAT_TEXT, EE_SCENARIO_WY_CHAT_BOX]);
    let typed = |op: u32, v: &[i64; 6]| {
        if op == EE_SCENARIO_WY_CAL_TYPE {
            // SAFETY: the composer's own out value
            unsafe { *(v[1] as usize as *mut c_long) = 8 };
            Some(1)
        } else {
            None
        }
    };
    assert_eq!(run(EE_SCENARIO_WORLD_CAL_TYPE, [4, 0, 0, 0, 0, 0], &mut e, &typed).0, Some(8));
    let untyped = |op: u32, _: &[i64; 6]| (op == EE_SCENARIO_WY_CAL_TYPE).then_some(0);
    assert_eq!(run(EE_SCENARIO_WORLD_CAL_TYPE, [4, 0, 0, 0, 0, 0], &mut e, &untyped).0, Some(-1));
    let kind = |op: u32, _: &[i64; 6]| (op == EE_SCENARIO_WY_CAL_TARGET_KIND).then_some(2);
    assert_eq!(run(EE_SCENARIO_WORLD_CAL_TARGET_KIND, [4, 0, 0, 0, 0, 0], &mut e, &kind).0, Some(2));
    let no_class = |op: u32, _: &[i64; 6]| (op == EE_SCENARIO_WY_CAL_CLASS).then_some(0);
    assert_eq!(run(EE_SCENARIO_WORLD_CAL_TARGET_KIND, [4, 0, 0, 0, 0, 0], &mut e, &no_class).0, Some(-1));
    let mut copy = EeScenarioEffectFields::default();
    let (_, o) = run(EE_SCENARIO_WORLD_ADD_PENDING_PLAYER, [5, 0, 0, 0, 0, 0], &mut copy, &none);
    assert_eq!(copy.mPlayer, 5);
    assert_eq!(
        ops(&o),
        vec![
            EE_SCENARIO_WK_EFFECT_NEW,
            EE_SCENARIO_RX_EFFECT_COPY,
            EE_SCENARIO_WK_EFFECT_FIELDS,
            EE_SCENARIO_WK_EFFECT_ADD,
            EE_SCENARIO_WK_EFFECT_FREE
        ]
    );
}

#[test]
fn camera_key_rate() {
    let mut e = EeScenarioEffectFields::default();
    let (_, o) = run(EE_SCENARIO_WORLD_CAMERA_KEY, [9, 4, 0, 0, 0, 0], &mut e, &|op, _| match op {
        EE_SCENARIO_RX_UNIT => Some(70),
        EE_SCENARIO_WY_KEY_NEW => Some(80),
        _ => None,
    });
    assert_eq!(o[0], (EE_SCENARIO_RX_UNIT, 9, 0));
    assert!(o.contains(&(EE_SCENARIO_WY_KEY_FOCUS, 80, 70)));
    assert!(o.contains(&(EE_SCENARIO_WY_KEY_CHANGED, 80, 0)));
    assert!(o.contains(&(EE_SCENARIO_WY_KEY_RATE, 80, bits(0.25))));
    let (_, o) = run(EE_SCENARIO_WORLD_CAMERA_KEY, [9, 0, 0, 0, 0, 0], &mut e, &none);
    assert!(o.contains(&(EE_SCENARIO_WY_KEY_CHANGED, 1, 1)));
    assert!(o.contains(&(EE_SCENARIO_WY_KEY_RATE, 1, bits(0.0))));
    assert_eq!(ops(&o)[ops(&o).len() - 2..], [EE_SCENARIO_WY_KEY_ADD, EE_SCENARIO_WY_KEY_FREE]);
}

#[test]
fn change_unit_garrison_and_kill() {
    let mut e = EeScenarioEffectFields::default();
    let (_, o) = run(EE_SCENARIO_WORLD_CU_SET, [EE_SCENARIO_CU_GARRISON_WAIT, 0, 0, 0, 0, 0], &mut e, &none);
    assert_eq!(
        o,
        vec![
            (EE_SCENARIO_WY_CU_UNITS_CLEAR, 0, 0),
            (EE_SCENARIO_WY_CU_QUEUE_GARRISON, 0, 0),
            (EE_SCENARIO_WY_CU_START_ACTION, ACTION_WAIT_FOR_GARRISON_COST, 0),
            (EE_SCENARIO_WY_CU_ACTION_CHANGED, 0, 0)
        ]
    );
    let (_, o) = run(EE_SCENARIO_WORLD_CU_SET, [EE_SCENARIO_CU_KILL, 0, 0, 0, 0, 0], &mut e, &none);
    assert_eq!(ops(&o), vec![EE_SCENARIO_WY_CU_KILL]);
    let (_, o) = run(EE_SCENARIO_WORLD_CU_SET, [EE_SCENARIO_CU_KILL, 1, 0, 0, 0, 0], &mut e, &none);
    assert_eq!(ops(&o), vec![EE_SCENARIO_WY_CU_RESOURCE_ZERO, EE_SCENARIO_WY_CU_TAKEN, EE_SCENARIO_WY_CU_KILL_UNIT]);
}
