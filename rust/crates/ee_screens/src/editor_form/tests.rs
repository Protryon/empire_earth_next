use super::*;

/// A trigger: its numbers, name, description, condition and effect wrappers.
type Trigger = (EeScenarioTriggerFields, &'static str, &'static str, Vec<[i32; 3]>, Vec<[i32; 3]>);

/// A scenario as plain records.
#[derive(Default)]
struct Scenario {
    triggers: Vec<Trigger>,
    conditions: Vec<(EeScenarioConditionFields, &'static str)>,
    effects: Vec<(EeScenarioEffectFields, &'static str, Vec<i32>)>,
    objects: Vec<(EeScenarioObjectFields, &'static str)>,
    areas: Vec<&'static str>,
}

impl ScenarioRecords for Scenario {
    fn count(&mut self, list: i32) -> usize {
        match list {
            EE_SCREENS_SEFM_LIST_TRIGGERS => self.triggers.len(),
            EE_SCREENS_SEFM_LIST_CONDITIONS => self.conditions.len(),
            EE_SCREENS_SEFM_LIST_EFFECTS => self.effects.len(),
            EE_SCREENS_SEFM_LIST_OBJECTS => self.objects.len(),
            _ => self.areas.len(),
        }
    }
    fn trigger(&mut self, index: usize) -> Option<EeScenarioTriggerFields> {
        self.triggers.get(index).map(|t| t.0)
    }
    fn condition(&mut self, index: usize) -> Option<EeScenarioConditionFields> {
        self.conditions.get(index).map(|t| t.0)
    }
    fn effect(&mut self, index: usize) -> Option<EeScenarioEffectFields> {
        self.effects.get(index).map(|t| t.0)
    }
    fn object(&mut self, index: usize) -> Option<EeScenarioObjectFields> {
        self.objects.get(index).map(|t| t.0)
    }
    fn text(&mut self, list: i32, index: usize, which: i32) -> Vec<u8> {
        let s = match list {
            EE_SCREENS_SEFM_LIST_TRIGGERS => self.triggers.get(index).map(|t| if which == 0 { t.1 } else { t.2 }),
            EE_SCREENS_SEFM_LIST_CONDITIONS => self.conditions.get(index).map(|t| t.1),
            EE_SCREENS_SEFM_LIST_EFFECTS => self.effects.get(index).map(|t| t.1),
            EE_SCREENS_SEFM_LIST_OBJECTS => self.objects.get(index).map(|t| t.1),
            _ => self.areas.get(index).copied(),
        };
        s.unwrap_or("").as_bytes().to_vec()
    }
    fn vec(&mut self, which: i32, index: usize) -> Vec<[i32; 3]> {
        match which {
            EE_SCREENS_SEFM_VEC_CONDITIONS => self.triggers[index].3.clone(),
            EE_SCREENS_SEFM_VEC_EFFECTS => self.triggers[index].4.clone(),
            _ => self.effects[index].2.iter().map(|&o| [o, 0, 0]).collect(),
        }
    }
}

fn trigger(on: bool, duration: u32, looping: bool) -> EeScenarioTriggerFields {
    EeScenarioTriggerFields { mIsOn: on, mConditionDuration: duration as c_ulong, mIsLooping: looping, ..Default::default() }
}

fn condition(ty: u32, object: u32) -> EeScenarioConditionFields {
    EeScenarioConditionFields { mType: ty as c_ulong, mObjectIndex: object as c_ulong, ..Default::default() }
}

fn effect(ty: u32, object_action: u32) -> EeScenarioEffectFields {
    EeScenarioEffectFields { mType: ty as c_ulong, mObjectAction: object_action as c_ulong, ..Default::default() }
}

fn object(parent: u32, area: u32) -> EeScenarioObjectFields {
    EeScenarioObjectFields { mParent: parent as c_ulong, mArea: area as c_ulong, ..Default::default() }
}

/// The original's report over a scenario with a described, off, timed and looping trigger (a NOT, an OR, a delay, an
/// AND, a condition and an effect past their lists), a plain trigger, an unused effect, condition, two objects and an
/// area, and a looping trigger's effect naming the class.
#[test]
fn trigger_report_matches_the_original() {
    let mut e3 = effect(2, 0);
    e3.mTaskArea = 3;
    let mut s = Scenario {
        triggers: vec![
            (trigger(false, 7, true), "Alpha", "Desc", vec![[1, 1, 2], [0, 9, 0]], vec![[4, 1, 1], [0, 9, 0]]),
            (trigger(true, 0, false), "Beta", "", vec![[0, 0, 0]], vec![[0, 2, 0]]),
        ],
        conditions: vec![(condition(0, 0), "Always"), (condition(2, 0), "C1"), (condition(2, 0), "C2"), (condition(1, 2), "C3")],
        effects: vec![
            (effect(0, 0), "None", vec![]),
            (effect(1, 10), "E1", vec![1]),
            (effect(0, 0), "E2", vec![]),
            (e3, "E3", vec![]),
        ],
        objects: vec![(object(0, 0), "O0"), (object(0, 2), "O1"), (object(0, 0), "O2"), (object(1, 0), "O3")],
        areas: vec!["Off", "Previous", "A2", "A3", "A4"],
    };
    let want = "Alpha\nDescription:\n\"Desc\"\n\tTrigger starts OFF\n\tConditions True for 7\n\tLooping\n\
                \tIf  NOT (C1)  OR (BAD CONDITION) \n\tThen  (Delay 4 sec)  (E1)  AND (BAD EFFECT) \n\n\
                Beta\n\tIf  (Always) \n\tThen  (E2) \n\n\
                Effect: (E3) is not used in any trigger\n\
                Condition: (C2) is not used in any trigger\n\
                Object: (O2) is not used in any condition, effect or other object\n\
                Object: (O3) is not used in any condition, effect or other object\n\
                Area: (A4) is not used in any condition, effect or Object\n\
                WARNING: Potential performance issue with looping Trigger (Alpha) calling effect (E1) which repeatedly \
                sets the objects name, class attribute or player tech\n";
    assert_eq!(String::from_utf8(trigger_report(&mut s)).unwrap(), want);
}

/// An object only a condition names is used when a trigger uses that condition; an empty scenario reports nothing.
#[test]
fn trigger_report_object_conditions_and_empty() {
    let mut s = Scenario::default();
    assert!(trigger_report(&mut s).is_empty());
    let mut s = Scenario {
        triggers: vec![(trigger(true, 0, false), "T", "", vec![[0, 1, 0]], vec![])],
        conditions: vec![(condition(0, 0), "Always"), (condition(1, 1), "Obj")],
        effects: vec![(effect(0, 0), "None", vec![])],
        objects: vec![(object(0, 0), "O0"), (object(0, 0), "O1")],
        areas: vec!["Off", "Previous"],
    };
    assert_eq!(String::from_utf8(trigger_report(&mut s)).unwrap(), "T\n\tIf  (Obj) \n\tThen \n\n");
}

/// A recording host: every op logged with its numbers; made objects are counted addresses; the units are
/// `0x1000 + id` with cargo for ids at least 100.
#[derive(Default)]
struct Rec {
    log: Vec<String>,
    next: usize,
    captured: [bool; 2],
    keys: Vec<(i32, i32)>,
    shown: bool,
    players: i32,
}

impl SeplHost for Rec {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let out = |v: usize| unsafe { *(p[0] as *mut usize) = v };
        match op {
            EE_SCREENS_SEFM_OP_BASICS => {
                let words = unsafe { core::slice::from_raw_parts_mut(p[0] as *mut usize, 5) };
                for (k, w) in words.iter_mut().enumerate() {
                    *w = 0x100 + k;
                }
                self.log.push("basics".into());
                return 0;
            }
            EE_SCREENS_SEFM_OP_BRUSH_GROUP | EE_SCREENS_SEFM_OP_TAB_GROUP => {
                let index = unsafe { &mut *(p[2] as *mut u32) };
                self.log.push(format!("{op} {} at {}", i[0], *index));
                *index += 3;
                self.next += 1;
                out(0x200 + self.next);
                return 0;
            }
            EE_SCREENS_SEFM_OP_BUTTON => {
                let b = unsafe { *(p[2] as *const EeScreensSefmButton) };
                self.log.push(format!("button {} id {} ix {} help {:#x} ev {:#x} {:?} {:?}", b.kind, b.id, b.index, b.help, b.event, b.dest, b.rect));
                self.next += 1;
                out(0x200 + self.next);
                return 0;
            }
            EE_SCREENS_SEFM_OP_MINI_MAP
            | EE_SCREENS_SEFM_OP_PLAYER_COMBO
            | EE_SCREENS_SEFM_OP_LABEL
            | EE_SCREENS_SEFM_OP_UNDO_REDO
            | EE_SCREENS_SEFM_OP_QUICK_ACTION
            | EE_SCREENS_SEFM_OP_HIDE_UI
            | EE_SCREENS_SEFM_OP_SCREEN_EVENT
            | EE_SCREENS_SEFM_OP_SEE_EVENT
            | EE_SCREENS_SEFM_OP_GRAB_SCROLL
            | EE_SCREENS_SEFM_OP_LOAD_THREAD
                if p[0] != 0 =>
            {
                self.next += 1;
                self.log.push(format!("{op} {:?} -> {:#x}", i, 0x200 + self.next));
                out(0x200 + self.next);
                return 0;
            }
            EE_SCREENS_SEFM_OP_RESOLVE => {
                out(if i[0] as u32 == u32::MAX { 0 } else { 0x1000 + i[0] as usize });
                return 0;
            }
            EE_SCREENS_SEFM_OP_UNIT_ID => return (p[0] - 0x1000) as i32,
            EE_SCREENS_SEFM_OP_UNIT_CARGO => return i32::from(p[0] >= 0x1000 + 100),
            EE_SCREENS_SEFM_OP_CAPTURED => return i32::from(self.captured[i[0] as usize]),
            EE_SCREENS_SEFM_OP_KEY => return self.keys.iter().find(|k| k.0 == i[0]).map_or(0, |k| k.1),
            EE_SCREENS_SEFM_OP_PLAYER_COUNT => return self.players,
            crate::editor_players::EE_SCREENS_SEPL_OP_DISPLAY => {
                return if self.shown { crate::editor_players::EE_SCREENS_SEPL_SHOWN } else { 0 }
            }
            crate::editor_players::EE_SCREENS_SEPL_OP_RECT => {
                *f = [264.0, 735.0, 295.0, 766.0];
                return 0;
            }
            _ => {}
        }
        self.log.push(format!("{op} {:#x},{:#x},{:#x} {:?} {:?}", p[0], p[1], p[2], i, f));
        0
    }
}

/// The constructor: the controls' indices in the original's order (the brush group and the groups take three
/// each here), the tab buttons' and tool bar buttons' records, the hot keys, the map tab checked last.
#[test]
fn build_makes_the_controls_in_order() {
    let form = EditorForm::new(0x50);
    let mut h = Rec::default();
    form.build(&mut h);
    let log = h.log.join("\n");
    // the mini map 0, the combo 1, the labels 2 and 3, the brush group from 4 (to 7), undo 7, redo 8, the seven
    // groups from 9 (to 30), the tab buttons 30 .. 36, the civilization builder 37, the tool bar 38 .. 43, hide UI 44
    assert!(log.contains("801 [0, 0, 0, 0]"), "{log}");
    assert!(log.contains("802 [1, 0, 0, 0]"));
    assert!(log.contains("803 [0, 2, 0, 0]") && log.contains("803 [1, 3, 0, 0]"));
    assert!(log.contains("804 0 at 4"));
    assert!(log.contains("805 [0, 7, 0, 0]") && log.contains("805 [1, 8, 0, 0]"));
    assert!(log.contains("806 0 at 9") && log.contains("806 6 at 27"));
    assert!(log.contains("button 0 id 220 ix 30 help 0x213a"));
    assert!(log.contains("button 0 id 226 ix 36 help 0x2143"));
    assert!(log.contains("button 1 id 17 ix 37 help 0x2159 ev 0x2422 [307.0, 735.0, 338.0, 766.0] [307.0, 735.0, 338.0, 766.0]"));
    assert!(log.contains("button 1 id 12 ix 38 help 0x2140 ev 0x3 [647.0, 736.0, 678.0, 767.0]"));
    assert!(log.contains("button 1 id 15 ix 43 help 0x213f ev 0x242b"));
    assert!(log.contains("809 [44, 0, 0, 0]"));
    assert_eq!(form.get(EE_SCREENS_SEFM_ENTITY_GROUP), 1);
    assert_eq!(form.get(EE_SCREENS_SEFM_CURRENT_TAB), 0);
    assert_ne!(form.get(EE_SCREENS_SEFM_QUICK_SAVE_BUTTON), 0);
    assert_eq!(form.get(EE_SCREENS_SEFM_BUTTON_TEXTURE), 0x100);
    assert_eq!(form.get(EE_SCREENS_SEFM_FULL_BACKGROUND), 0x104);
    // the last op: the map tab's button checked
    assert!(h.log.last().unwrap().starts_with(&format!("818 {:#x}", form.get(EE_SCREENS_SEFM_TAB_BUTTON))));
    // eight hot keys and the Ctrl+S save as an EESEERust of kind 17
    assert_eq!(h.log.iter().filter(|l| l.starts_with("814 ") || l.starts_with("815 ")).count(), 9);
    assert!(log.contains("812 [17, 0, 0, 0]"));
}

/// `SelectTab`: the full view entered for the player tab, the old group left and its controls disabled, the new
/// group enabled, entered and loaded; the brush highlight for the terrain tab.
#[test]
fn select_tab_switches_groups() {
    let form = EditorForm::new(0x50);
    for g in 0..7 {
        form.set(EE_SCREENS_SEFM_GROUP + g, 0x300 + g as usize);
    }
    form.set(EE_SCREENS_SEFM_BRUSH_GROUP, 0x400);
    form.set(EE_SCREENS_SEFM_MINI_MAP, 0x401);
    form.set(EE_SCREENS_SEFM_ENTITY_GROUP, 1);
    form.set(EE_SCREENS_SEFM_ENTITY_GROUP_ENABLED, 1);
    let mut h = Rec::default();
    form.select_tab(&mut h, 4, 5);
    assert_eq!(
        h.log,
        [
            "822 0x0,0x0,0x0 [1, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]",
            "4 0x401,0x0,0x0 [0, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]",
            "823 0x0,0x0,0x0 [0, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]",
            "824 0x400,0x0,0x0 [0, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]",
            "821 0x300,0x0,0x0 [1, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]",
            "810 0x0,0x50,0x0 [1, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]",
            "810 0x0,0x50,0x0 [4, 1, 0, 0] [0.0, 0.0, 0.0, 0.0]",
            "821 0x305,0x0,0x0 [5, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]",
            "821 0x305,0x0,0x0 [4, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]",
        ]
    );
    assert_eq!((form.get(EE_SCREENS_SEFM_CURRENT_TAB), form.get(EE_SCREENS_SEFM_ENTITY_GROUP)), (5, 4));
    h.log.clear();
    form.set(EE_SCREENS_SEFM_SWITCHING, 1);
    form.select_tab(&mut h, 3, 2);
    assert_eq!(h.log[0], "822 0x0,0x0,0x0 [0, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]");
    assert!(!h.log.iter().any(|l| l.starts_with("821 0x305")), "switching: the old group not left");
    assert_eq!(h.log.last().unwrap(), "824 0x400,0x0,0x0 [1, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]");
}

/// The selection: carriers counted (the cargo panel shown with the first, hidden with the last), the status of the
/// first unit, duplicates kept, a removal of the first showing the next one's status.
#[test]
fn selection_counts_cargo() {
    let form = EditorForm::new(0x50);
    form.set(EE_SCREENS_SEFM_GROUP + 3, 0x303);
    let mut h = Rec::default();
    let (carrier, citizen) = (0x1000 + 150, 0x1000 + 7);
    form.select_unit(&mut h, carrier);
    form.select_unit(&mut h, citizen);
    form.select_unit(&mut h, carrier);
    assert_eq!(form.get(EE_SCREENS_SEFM_NUM_CARGO_UNITS), 2);
    assert_eq!((0..3).map(|k| form.selected_id(k)).collect::<Vec<_>>(), [150, 7, 150]);
    assert_eq!(h.log.iter().filter(|l| l.starts_with("830 ")).count(), 1, "the status for the first only");
    h.log.clear();
    form.deselect_unit(&mut h, citizen);
    assert_eq!(form.selected_count(), 2);
    assert!(!h.log.iter().any(|l| l.starts_with("830 ")), "not the first: no status");
    form.deselect_first(&mut h);
    assert_eq!(form.get(EE_SCREENS_SEFM_NUM_CARGO_UNITS), 1);
    assert_eq!(h.log.last().unwrap(), "830 0x303,0x1096,0x0 [0, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]");
    h.log.clear();
    form.deselect_unit(&mut h, carrier);
    assert_eq!(form.selected_count(), 0);
    assert!(h.log.iter().any(|l| l == "829 0x303,0x0,0x0 [0, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]"), "{:?}", h.log);
    assert_eq!(h.log.last().unwrap(), "830 0x303,0x0,0x0 [0, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]");
    form.select_unit(&mut h, citizen);
    form.deselect_unit(&mut h, carrier);
    assert_eq!(form.selected_count(), 1);
    form.clear_selection(&mut h);
    assert_eq!((form.selected_count(), form.get(EE_SCREENS_SEFM_NUM_CARGO_UNITS)), (0, 0));
}

/// The keys: `;` quick saves (control 13), Alt+3 picks player 11 when there are as many, the group's keys only
/// when nothing was handled; nothing while suspended.
#[test]
fn update_keyboard_keys() {
    let form = EditorForm::new(0x50);
    form.set(EE_SCREENS_SEFM_PLAYER_COMBO, 0x500);
    let mut h = Rec { keys: vec![(EE_SCREENS_SEFM_KEY_SEMICOLON, 1)], ..Default::default() };
    assert!(!form.update_keyboard(&mut h));
    assert!(h.log.iter().any(|l| l.starts_with("846 0x0,0x50,0x0 [13,")));
    let mut h = Rec {
        keys: vec![(EE_SCREENS_SEFM_KEY_1 + 2, 1), (EE_SCREENS_SEFM_KEY_ALT, 2)],
        shown: true,
        players: 11,
        ..Default::default()
    };
    assert!(!form.update_keyboard(&mut h));
    assert!(h.log.iter().any(|l| l.starts_with("849 0x500,0x0,0x0 [10,")), "{:?}", h.log);
    let mut h = Rec { shown: true, players: 2, ..Default::default() };
    assert!(form.update_keyboard(&mut h));
    assert!(h.log.iter().any(|l| l.starts_with("821 ") && l.ends_with("[9, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]")));
    form.set(EE_SCREENS_SEFM_SUSPENDED, 1);
    let mut h = Rec { keys: vec![(EE_SCREENS_SEFM_KEY_SEMICOLON, 1)], ..Default::default() };
    assert!(!form.update_keyboard(&mut h));
    assert!(h.log.is_empty());
}

/// `DrawBackground`: the panel clipped to the rectangle (none when it misses the panel), the group drawn after;
/// the full view's background and its result for the player tab.
#[test]
fn draw_background_clips_the_panel() {
    let form = EditorForm::new(0x50);
    form.set(EE_SCREENS_SEFM_PANEL_BACKGROUND, 0x600);
    let mut h = Rec::default();
    form.draw_background(&mut h, [100.0, 500.0, 300.0, 700.0]);
    assert!(h.log[0].starts_with("844 0x600,0x50,0x") && h.log[0].ends_with("[0, 0, 0, 0] [100.0, 606.0, 300.0, 700.0]"));
    assert!(h.log[1].starts_with("821 ") && h.log[1].contains("[2, 0, 0, 0] [100.0, 500.0, 300.0, 700.0]"));
    h.log.clear();
    form.draw_background(&mut h, [0.0, 0.0, 100.0, 100.0]);
    assert_eq!(h.log.len(), 1);
    form.set(EE_SCREENS_SEFM_CURRENT_TAB, 5);
    h.log.clear();
    form.draw_background(&mut h, [0.0, 0.0, 100.0, 100.0]);
    assert!(h.log[0].starts_with("843 "));
}

/// `ExitEditor`: the map button checked directly, the load thread asked to test from the editor (Post404) and
/// dropped, the form suspended.
#[test]
fn exit_editor_posts_and_suspends() {
    let form = EditorForm::new(0x50);
    form.set(EE_SCREENS_SEFM_TAB_BUTTON, 0x700);
    form.set(EE_SCREENS_SEFM_LOAD_THREAD, 0x701);
    let mut h = Rec::default();
    form.exit_editor(&mut h, false);
    assert_eq!(h.log[0], "818 0x700,0x0,0x0 [1, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]");
    assert_eq!(h.log[1], "832 0x0,0x701,0x0 [1, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]");
    assert_eq!((form.get(EE_SCREENS_SEFM_LOAD_THREAD), form.get(EE_SCREENS_SEFM_SUSPENDED)), (0, 1));
}

/// The tables keep the original's values (0x4c46d0, 0x4c47b0).
#[test]
fn tables() {
    assert_eq!(EDITOR_TABS.iter().map(|t| t.flag).collect::<Vec<_>>(), [1, 2, 3, 6, 14, 4, 7]);
    assert_eq!(EDITOR_TABS.iter().filter(|t| t.full_view).count(), 2);
    assert_eq!(EDITOR_TABS.iter().filter(|t| t.brush).map(|t| t.group).collect::<Vec<_>>(), [1, 2]);
    assert_eq!(TOOLBAR_BUTTONS.iter().map(|t| t.id).collect::<Vec<_>>(), [12, 13, 11, 10, 14, 15]);
}
