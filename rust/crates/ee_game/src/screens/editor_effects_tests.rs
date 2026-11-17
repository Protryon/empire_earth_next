//! Unit tests of the effect types (`editor_effects*.rs`) over a recording host: no game, data or window.

use std::collections::HashMap;
use std::ffi::c_ulong;

use ee_sim::scenario::fields::EeScenarioEffectFields;

use crate::screens::editor_effects::*;
use crate::screens::editor_effects_more::AiTypes;
use crate::screens::editor_players::SeplHost;

/// A recording host: the controls' rectangles, selections, checks, sliders, players and texts as maps; strings are
/// "S<id>", a formatted number "F<id>:<n>".
#[derive(Default)]
struct Mock {
    log: Vec<(u32, [i32; 4], Vec<u8>)>,
    rects: HashMap<(i32, i32), [f32; 4]>,
    selections: HashMap<(i32, i32), i32>,
    checked: HashMap<(i32, i32), bool>,
    sliders: HashMap<i32, i32>,
    players: HashMap<i32, i32>,
    texts: HashMap<(i32, i32), Vec<u8>>,
    effect_texts: HashMap<i32, Vec<u8>>,
    events: HashMap<(i32, i32, i32), usize>,
    next_event: usize,
    behaviors: Vec<[i32; 8]>,
}

fn write(p: usize, cap: i32, text: &[u8]) -> i32 {
    let n = text.len().min(cap as usize);
    unsafe { core::ptr::copy_nonoverlapping(text.as_ptr(), p as *mut u8, n) };
    text.len() as i32
}

fn bytes(p: usize, n: i32) -> Vec<u8> {
    unsafe { core::slice::from_raw_parts(p as *const u8, n as usize) }.to_vec()
}

impl SeplHost for Mock {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let key = (i[0], i[1]);
        let mut text = Vec::new();
        let r = match op {
            EE_SCREENS_SEET_OP_RECT => {
                *f = *self.rects.get(&key).unwrap_or(&[0.0, 0.0, 100.0, 20.0]);
                0
            }
            EE_SCREENS_SEET_OP_SET_RECT => {
                self.rects.insert(key, *f);
                text = format!("{:?}", f).into_bytes();
                0
            }
            EE_SCREENS_SEET_OP_LOCATION => {
                let r = self.rects.entry(key).or_insert([0.0, 0.0, 100.0, 20.0]);
                let (w, h) = (r[2] - r[0], r[3] - r[1]);
                *r = [f[0], f[1], f[0] + w, f[1] + h];
                text = format!("{:?}", &f[..2]).into_bytes();
                0
            }
            EE_SCREENS_SEET_OP_TEXT | EE_SCREENS_SEET_OP_APPEND => {
                text = bytes(p[2], i[2]);
                if op == EE_SCREENS_SEET_OP_TEXT {
                    self.texts.insert(key, text.clone());
                }
                0
            }
            EE_SCREENS_SEET_OP_GET_TEXT | EE_SCREENS_SEET_OP_GET_ALL_TEXT => {
                let t = self.texts.get(&key).cloned().unwrap_or_default();
                write(p[2], i[3], &t)
            }
            EE_SCREENS_SEET_OP_LIST_DATA => write(p[2], i[3], format!("line{}", i[2]).as_bytes()),
            EE_SCREENS_SEET_OP_SELECTION => *self.selections.get(&key).unwrap_or(&-1),
            EE_SCREENS_SEET_OP_SET_SELECTION => {
                self.selections.insert(key, i[2]);
                0
            }
            EE_SCREENS_SEET_OP_LINE_COUNT => 3,
            EE_SCREENS_SEET_OP_CHECKED => *self.checked.get(&key).unwrap_or(&false) as i32,
            EE_SCREENS_SEET_OP_SLIDER => *self.sliders.get(&i[1]).unwrap_or(&0),
            EE_SCREENS_SEET_OP_SET_SLIDER => {
                self.sliders.insert(i[1], i[2]);
                0
            }
            EE_SCREENS_SEET_OP_PLAYER => *self.players.get(&i[1]).unwrap_or(&0),
            EE_SCREENS_SEET_OP_SET_PLAYER => {
                self.players.insert(i[1], i[2]);
                0
            }
            EE_SCREENS_SEET_OP_EVENT_NEW => {
                self.next_event += 1;
                unsafe { *(p[2] as *mut usize) = self.next_event };
                0
            }
            EE_SCREENS_SEET_OP_EVENT_SET => {
                self.events.insert((i[0], i[1], i[2]), p[1]);
                0
            }
            EE_SCREENS_SEET_OP_EVENT_NULL => {
                self.events.remove(&(i[0], i[1], i[2]));
                0
            }
            EE_SCREENS_SEET_OP_EVENT_HOLD => {
                let e = self.events.remove(&(i[0], i[1], i[2])).unwrap_or(0);
                unsafe { *(p[2] as *mut usize) = e };
                0
            }
            EE_SCREENS_SEET_OP_EVENT_RESTORE => {
                self.events.insert((i[0], i[1], i[2]), p[1]);
                0
            }
            EE_SCREENS_SEET_OP_STRING => write(p[2], i[1], format!("S{}", i[2]).as_bytes()),
            EE_SCREENS_SEET_OP_FORMAT_NUMBER => write(p[2], i[1], format!("F{}:{}", i[2], i[3]).as_bytes()),
            EE_SCREENS_SEET_OP_FORMAT_FLOAT => write(p[2], i[1], format!("{}", f[0]).as_bytes()),
            EE_SCREENS_SEET_OP_EFFECT_TEXT => {
                let t = self.effect_texts.get(&i[2]).cloned().unwrap_or_default();
                write(p[2], i[1], &t)
            }
            EE_SCREENS_SEET_OP_SET_EFFECT_TEXT => {
                text = bytes(p[2], i[3]);
                self.effect_texts.insert(i[2], text.clone());
                0
            }
            EE_SCREENS_SEET_OP_SET_NAME_BOX => {
                text = bytes(p[2], i[2]);
                0
            }
            EE_SCREENS_SEET_OP_CALAMITY_COUNT => 2,
            EE_SCREENS_SEET_OP_CALAMITY => {
                // class 1 missing
                if i[2] == 1 {
                    0
                } else {
                    i[3] = 900 + i[2];
                    i[2] += 10;
                    1
                }
            }
            EE_SCREENS_SEET_OP_BEHAVIOR_COUNT => self.behaviors.len() as i32,
            EE_SCREENS_SEET_OP_BEHAVIOR => self.behaviors[i[2] as usize][i[3] as usize],
            _ => 0,
        };
        self.log.push((op, *i, text));
        r
    }
}

fn run<R>(m: &mut Mock, e: &mut EeScenarioEffectFields, f: impl FnOnce(&mut Fx<Mock>) -> R) -> R {
    let mut c = Fx { h: m, controls: 1, effect: 2, fields: e as *mut _, me: 3 };
    f(&mut c)
}

fn ops(m: &Mock, op: u32) -> Vec<[i32; 4]> {
    m.log.iter().filter(|x| x.0 == op).map(|x| x.1).collect()
}

#[test]
fn no_effect_hides_and_shows_the_entry_controls() {
    let (mut m, mut e) = (Mock::default(), EeScenarioEffectFields::default());
    let types = run(&mut m, &mut e, EffectTypes::new);
    m.log.clear();
    run(&mut m, &mut e, |c| types.call(c, 0, EE_SCREENS_SEET_SHOW));
    let shown: Vec<_> = ops(&m, EE_SCREENS_SEET_OP_FORM_DISPLAY).iter().map(|i| (i[2], i[3])).collect();
    assert_eq!(shown, EE_SCREENS_SEET_NO_EFFECT_CONTROLS.iter().map(|&id| (id, 0)).collect::<Vec<_>>());
    m.log.clear();
    run(&mut m, &mut e, |c| types.call(c, 0, EE_SCREENS_SEET_HIDE));
    assert!(ops(&m, EE_SCREENS_SEET_OP_FORM_DISPLAY).iter().all(|i| i[3] == 1));
    // the six families' events (the object family two)
    assert_eq!(m.next_event, 6);
}

#[test]
fn trigger_store_keeps_fields_without_a_selection() {
    let (mut m, mut e) =
        (Mock::default(), EeScenarioEffectFields { mTrigger: 7, mTriggerState: 2, ..Default::default() });
    let types = run(&mut m, &mut e, EffectTypes::new);
    m.selections.insert((SCEN as i32, 0), 4);
    run(&mut m, &mut e, |c| types.call(c, 3, EE_SCREENS_SEET_STORE));
    assert_eq!((e.mTrigger, e.mTriggerState), (4, 2));
}

#[test]
fn player_diplomacy_names_both_players_and_the_stance() {
    let (mut m, mut e) = (Mock::default(), EeScenarioEffectFields::default());
    let types = run(&mut m, &mut e, EffectTypes::new);
    m.effect_texts.insert(EE_SCREENS_SEET_S_NAME, b"<Effect1>".to_vec());
    m.players.insert(0, 2);
    m.players.insert(1, 8);
    m.texts.insert((ECOMBO as i32, 1), b"Allied".to_vec());
    types.player.family.current.set(3);
    run(&mut m, &mut e, |c| types.call(c, 2, EE_SCREENS_SEET_UPDATE_NAME));
    let first = IDS_TRIGGER_PLAYER_EFFECT_FIRST + 3;
    let want = format!("<S{first} S{} F{}:2 Allied>", 2 + IDS_TRIGGER_PLAYER_FIRST, IDS_TRIGGER_PLAYER);
    assert_eq!(m.effect_texts[&EE_SCREENS_SEET_S_NAME], want.as_bytes());
    let boxed = m.log.iter().rfind(|x| x.0 == EE_SCREENS_SEET_OP_SET_NAME_BOX).unwrap();
    assert_eq!(boxed.2, want.as_bytes());
}

#[test]
fn a_named_effect_is_not_renamed() {
    let (mut m, mut e) = (Mock::default(), EeScenarioEffectFields::default());
    let types = run(&mut m, &mut e, EffectTypes::new);
    m.effect_texts.insert(EE_SCREENS_SEET_S_NAME, b"Mine".to_vec());
    for ty in 1..=6 {
        run(&mut m, &mut e, |c| types.call(c, ty, EE_SCREENS_SEET_UPDATE_NAME));
    }
    assert_eq!(m.effect_texts[&EE_SCREENS_SEET_S_NAME], b"Mine");
    assert!(ops(&m, EE_SCREENS_SEET_OP_SET_NAME_BOX).is_empty());
}

#[test]
fn task_area_lists_the_actions_then_the_calamities() {
    let (mut m, mut e) =
        (Mock::default(), EeScenarioEffectFields { mObjectAction: 6, mTaskAction: 18, ..Default::default() });
    let types = run(&mut m, &mut e, EffectTypes::new);
    run(&mut m, &mut e, |c| types.call(c, 1, EE_SCREENS_SEET_LOAD));
    let mut want: Vec<i32> = (IDS_NORMAL..=IDS_TRIGGER_GARRISON).map(|id| (id - 0x72bf) as i32).collect();
    want.extend([16, 18]);
    assert_eq!(*types.object.to_area.borrow(), want);
    // the stored action 18 (the calamity class 2, id 12) is the last line
    assert_eq!(m.selections[&(ECOMBO as i32, 0)], want.len() as i32 - 1);
    m.selections.insert((ECOMBO as i32, 0), 0);
    m.selections.insert((COMBO as i32, 0), 6);
    run(&mut m, &mut e, |c| types.call(c, 1, EE_SCREENS_SEET_STORE));
    assert_eq!(e.mTaskAction, want[0] as c_ulong);
}

#[test]
fn military_group_moves_keep_the_added_list_sorted() {
    let (mut m, mut e) = (Mock::default(), EeScenarioEffectFields::default());
    let ai = AiTypes::default();
    *ai.to_add.borrow_mut() = vec![1, 3, 5];
    *ai.added.borrow_mut() = vec![2, 4];
    run(&mut m, &mut e, |c| ai.move_object(c, true, 1));
    assert_eq!(*ai.added.borrow(), vec![2, 3, 4]);
    assert_eq!(*ai.to_add.borrow(), vec![1, 5]);
    let inserted = ops(&m, EE_SCREENS_SEET_OP_LIST_INSERT);
    assert_eq!((inserted[0][1], inserted[0][3]), (1, 1));
    run(&mut m, &mut e, |c| ai.move_object(c, false, 0));
    assert_eq!(*ai.to_add.borrow(), vec![1, 2, 5]);
    // a line past the list moves nothing
    run(&mut m, &mut e, |c| ai.move_object(c, true, 9));
    assert_eq!(ai.to_add.borrow().len(), 3);
}

#[test]
fn attribute_ranges_hide_an_empty_range_and_scale_the_float() {
    let (mut m, mut e) = (Mock::default(), EeScenarioEffectFields::default());
    m.behaviors = vec![[1, 0, 10, 5, 5, 0, 50, 0]];
    let ai = AiTypes::default();
    *ai.attributes.borrow_mut() = vec![0];
    run(&mut m, &mut e, |c| ai.show_ranges(c, 0));
    let texts: Vec<_> =
        m.log.iter().filter(|x| x.0 == EE_SCREENS_SEET_OP_TEXT).map(|x| (x.1[1], x.2.clone())).collect();
    assert_eq!(texts[0], (5, b"0:10".to_vec()));
    assert_eq!(texts[1], (11, b"0:50".to_vec()));
    // the second slider's range is empty: its four controls hidden
    let hidden: Vec<_> =
        ops(&m, EE_SCREENS_SEET_OP_DISPLAY).iter().filter(|i| i[2] == 0).map(|i| (i[0], i[1])).collect();
    assert_eq!(hidden, vec![(LABEL as i32, 7), (LABEL as i32, 9), (SLIDER as i32, 1), (LABEL as i32, 8)]);
    // a scale of 0 reads as 1
    let modifier = m.log.iter().find(|x| x.0 == EE_SCREENS_SEET_OP_SLIDER_MODIFIER);
    assert!(modifier.is_some());
    // out of range: nothing
    let n = m.log.len();
    run(&mut m, &mut e, |c| ai.show_ranges(c, u32::MAX));
    assert_eq!(m.log.len(), n);
}

#[test]
fn silent_selection_holds_the_event_and_gives_it_back() {
    let (mut m, mut e) =
        (Mock::default(), EeScenarioEffectFields { mStance: 2, mObjectAction: 7, ..Default::default() });
    m.events.insert((ECOMBO as i32, 1, 0x3eb), 77);
    let types = run(&mut m, &mut e, EffectTypes::new);
    m.log.clear();
    run(&mut m, &mut e, |c| types.call(c, 1, EE_SCREENS_SEET_LOAD));
    let at = |op| m.log.iter().position(|x| x.0 == op && x.1[0] == ECOMBO as i32 && x.1[1] == 1).unwrap();
    assert!(at(EE_SCREENS_SEET_OP_EVENT_HOLD) < at(EE_SCREENS_SEET_OP_SET_SELECTION));
    assert!(at(EE_SCREENS_SEET_OP_SET_SELECTION) < at(EE_SCREENS_SEET_OP_EVENT_RESTORE));
    assert_eq!(m.events[&(ECOMBO as i32, 1, 0x3eb)], 77);
    assert_eq!(m.selections[&(ECOMBO as i32, 1)], 2);
}

#[test]
fn the_action_combo_takes_the_familys_event_on_load() {
    let (mut m, mut e) = (Mock::default(), EeScenarioEffectFields { mGameAction: 6, ..Default::default() });
    let types = run(&mut m, &mut e, EffectTypes::new);
    run(&mut m, &mut e, |c| types.call(c, 5, EE_SCREENS_SEET_LOAD));
    assert_eq!(types.game.family.current.get(), 6);
    assert_eq!(m.events[&(COMBO as i32, 0, 0x3eb)], types.game.family.event);
    // Store removes it and keeps the action
    m.selections.insert((COMBO as i32, 0), 1);
    run(&mut m, &mut e, |c| types.call(c, 5, EE_SCREENS_SEET_STORE));
    assert_eq!((e.mGameAction, types.game.family.current.get()), (1, 1));
    assert!(!m.events.contains_key(&(COMBO as i32, 0, 0x3eb)));
}
