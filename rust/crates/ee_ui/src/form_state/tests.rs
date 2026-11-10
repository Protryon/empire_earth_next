//! The form's bookkeeping against the real C++ (`golden_form_state.txt`): a scratch driver built over the original
//! `UIForm.cpp` (the form `calloc`ed with its lists constructed in place, fake controls, backgrounds, events,
//! controls-table records and a fake manager whose `AddDirtyRectangle` comes back into the form, every call a trace
//! line) printed 24 random forms x 30 operations (`AddDirtyRectangle`, `AddCopyDirtyRectangle`, `CopyData`, `Open`,
//! `Close`, `SetPopup`, `RedrawIntersectControls`, `SetAllControlsDirty`, `GetToolTipShadowRectangle`, pokes at the
//! controls' state, the help label), each followed by the form's whole state. The replay below is the same world
//! over this module; the same driver over the forwarders prints the same file.

use std::collections::VecDeque;

use super::*;

const GOLDEN: &str = include_str!("../golden_form_state.txt");

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn rect_text(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}
fn parse_rect(t: &[&str]) -> UiRect {
    UiRect::new(fx(t[0]), fx(t[1]), fx(t[2]), fx(t[3]))
}

/// A handle: the arena index + 1 (0 is NULL).
fn handle(i: usize) -> Handle {
    i as Handle
}
fn index(h: Handle) -> usize {
    h as usize
}

#[derive(Clone)]
struct Ctl {
    index: c_ulong,
    rect: UiRect,
    mode: c_ulong,
    dirty: bool,
    draws: c_ulong,
    string_id: c_ulong,
}

struct Table {
    form_index: c_long,
    group: c_ulong,
    visible: bool,
    kind: c_ulong,
    string_id: c_ulong,
}

struct Event {
    id: c_ulong,
    refs: u32,
}

#[derive(Default)]
struct World {
    ctls: Vec<Ctl>,    // the arena (handle - 1)
    slots: Vec<usize>, // mControls (0: NULL)
    draw: Vec<usize>,  // mDrawControls
    groups: Vec<(bool, Vec<usize>)>,
    tables: Vec<Option<Table>>,
    db: Vec<(usize, usize)>, // (table index, control handle)
    event_data: Vec<(c_ulong, c_ulong)>,
    events: Vec<Event>,
    update_event: usize,
    open_event: usize,
    close_event: usize,
    backgrounds: Vec<c_ulong>,
    copies: Vec<UiRect>,
    dirty: VecDeque<DirtyRect>,
    rect: UiRect,
    fade: f32,
    open_time: c_ulong,
    popup: usize,
    copied: usize,
    popup_count: c_ulong,
    copied_rect: UiRect,
    help: usize,
    tab_stop: usize,
    tabbing: bool,
    shadow: bool,
    surfaces: c_ulong,
    window: (c_ulong, c_ulong),
    time: std::cell::Cell<c_ulong>,
    log: Vec<String>,
}

impl World {
    fn ctl(&self, c: Handle) -> &Ctl {
        &self.ctls[index(c) - 1]
    }
    fn ctl_mut(&mut self, c: Handle) -> &mut Ctl {
        &mut self.ctls[index(c) - 1]
    }
    fn new_control(&mut self, i: c_ulong, rect: UiRect) -> usize {
        self.ctls.push(Ctl { index: i, rect, mode: 1, dirty: true, draws: 0, string_id: 0 });
        self.ctls.len()
    }
    fn tabable(&self, c: usize) -> bool {
        self.ctls[c - 1].index % 4 != 1
    }
    /// The C++ form's `AddDirtyRectangle` (the fake manager's call back into the form).
    fn form_add_dirty(&mut self, r: UiRect) {
        if self.popup != 0 && intersects(&r, &self.ctl(handle(self.popup)).rect) {
            self.ctl_set_dirty(handle(self.popup), true);
        }
        let s = self.surfaces;
        add_dirty_rectangle(self, r, s);
    }
    fn dereference(&mut self, e: usize) {
        let ev = &mut self.events[e - 1];
        ev.refs -= 1;
        if ev.refs == 0 {
            let id = ev.id;
            self.log.push(format!("  event-delete {id}"));
        }
    }
    fn set_event_slot(&mut self, kind: c_ulong, e: usize) {
        let old = if kind == 0 { self.open_event } else { self.close_event };
        if old != 0 {
            self.dereference(old);
        }
        if kind == 0 {
            self.open_event = e;
        } else {
            self.close_event = e;
        }
        if e != 0 {
            self.events[e - 1].refs += 1;
        }
    }
    fn dump(&mut self) {
        let id = |x: usize, w: &World| if x == 0 { -1 } else { w.ctls[x - 1].index as c_long };
        let line = format!(
            "  state rect {} fade {} open {} popup {} copied {} count {} help {}",
            rect_text(&self.rect),
            h(self.fade),
            self.open_time,
            id(self.popup, self),
            id(self.copied, self),
            self.popup_count,
            id(self.help, self)
        );
        self.log.push(line);
        self.log.push(format!("  copiedrect {}", rect_text(&self.copied_rect)));
        for r in self.copies.clone() {
            self.log.push(format!("  copy {}", rect_text(&r)));
        }
        for d in self.dirty.clone() {
            self.log.push(format!("  dirty {} {}", d.surface_count, rect_text(&d.rect)));
        }
        let mut s = String::from("  draw");
        for &d in &self.draw {
            s.push_str(&format!(" {}", id(d, self)));
        }
        self.log.push(s);
        let mut s = String::from("  controls");
        for &c in &self.slots {
            if c == 0 {
                s.push_str(" -");
            } else {
                let k = &self.ctls[c - 1];
                s.push_str(&format!(" {}:{}{}{}:{}", k.index, k.dirty as u8, k.mode, k.draws, k.string_id));
            }
        }
        self.log.push(s);
        let mut s = String::from("  groups");
        for (_, list) in &self.groups {
            s.push_str(" [");
            for &c in list {
                s.push_str(&format!(" {}", self.ctls[c - 1].index));
            }
            s.push_str(" ]");
        }
        self.log.push(s);
    }
}

impl Lists for World {
    fn copy_len(&self) -> usize {
        self.copies.len()
    }
    fn copy_at(&self, i: usize) -> UiRect {
        self.copies[i]
    }
    fn copy_erase(&mut self, i: usize) {
        self.copies.remove(i);
    }
    fn copy_push(&mut self, r: UiRect) {
        self.copies.push(r);
    }
    fn dirty_len(&self) -> usize {
        self.dirty.len()
    }
    fn dirty_take_front(&mut self) -> DirtyRect {
        self.dirty.pop_front().unwrap()
    }
    fn dirty_push_back(&mut self, d: DirtyRect) {
        self.dirty.push_back(d);
    }
}

impl LifeHost for World {
    fn surfaces(&self) -> c_ulong {
        self.surfaces
    }
    fn rect(&self) -> UiRect {
        self.rect
    }
    fn layout(&mut self, screen: UiRect) {
        self.log.push(format!("  layout {}", rect_text(&screen)));
        self.rect = UiRect::new(screen.left + 10.0, screen.top + 5.0, screen.right - 10.0, screen.bottom - 5.0);
    }
    fn window_size(&self) -> (c_ulong, c_ulong) {
        self.window
    }
    fn now(&self) -> c_ulong {
        self.time.set(self.time.get() + 7);
        self.time.get()
    }
    fn open_time(&self) -> c_ulong {
        self.open_time
    }
    fn set_open_time(&mut self, t: c_ulong) {
        self.open_time = t;
    }
    fn should_fade_in(&mut self) -> bool {
        false
    }
    fn set_fade_opacity(&mut self, v: f32) {
        self.fade = v;
    }
    fn process_event(&mut self, which: FormEvent) -> Option<c_long> {
        let e = match which {
            FormEvent::Update => self.update_event,
            FormEvent::Open => self.open_event,
            FormEvent::Close => self.close_event,
        };
        if e == 0 {
            return None;
        }
        let id = self.events[e - 1].id;
        self.log.push(format!("  event {id}"));
        Some((id % 5) as c_long)
    }
    fn manager_add_dirty(&mut self, r: UiRect) {
        self.log.push(format!("  mdirty {}", rect_text(&r)));
        self.form_add_dirty(r);
    }
    fn manager_process_events(&mut self) {
        self.log.push("  process-events".into());
    }
    fn update_help_text(&mut self, _force: bool) {
        unreachable!("the driver's forms have no control under the mouse");
    }
    fn reset_tabbing(&mut self) {
        let old = self.slots[self.tab_stop];
        if old != 0 {
            let i = self.ctls[old - 1].index;
            self.log.push(format!("  from {i}"));
        }
        if self.tabbing {
            for i in 0..self.slots.len() {
                let c = self.slots[i];
                if c != 0 && self.tabable(c) {
                    self.tab_stop = i;
                    let k = self.ctls[c - 1].index;
                    self.log.push(format!("  to {k}"));
                    break;
                }
            }
        }
    }
    fn control_count(&self) -> c_long {
        self.slots.len() as c_long
    }
    fn control(&self, i: c_long) -> Handle {
        handle(self.slots[i as usize])
    }
    fn set_draw_control(&mut self, i: c_long, c: Handle) {
        self.draw[i as usize] = index(c);
    }
    fn group_count(&self) -> c_long {
        self.groups.len() as c_long
    }
    fn group_enabled(&self, g: c_long) -> bool {
        self.groups[g as usize].0
    }
    fn entity_count(&self, g: c_long) -> c_ulong {
        self.groups[g as usize].1.len() as c_ulong
    }
    fn entity(&self, g: c_long, j: c_ulong) -> Handle {
        handle(self.groups[g as usize].1[j as usize])
    }
    fn popup(&self) -> Handle {
        handle(self.popup)
    }
    fn set_popup_field(&mut self, c: Handle) {
        self.popup = index(c);
    }
    fn set_copied_popup(&mut self, c: Handle) {
        self.copied = index(c);
    }
    fn set_copied_popup_rect(&mut self, r: UiRect) {
        self.copied_rect = r;
    }
    fn popup_draw_count(&self) -> c_ulong {
        self.popup_count
    }
    fn set_popup_draw_count(&mut self, n: c_ulong) {
        self.popup_count = n;
    }
    fn help_label(&self) -> Handle {
        handle(self.help)
    }
    fn set_help_label(&mut self, c: Handle) {
        self.help = index(c);
    }
    fn mouse_over(&self) -> Handle {
        core::ptr::null_mut()
    }
    fn draw_shadow(&self) -> bool {
        self.shadow
    }
    fn background_count(&self) -> usize {
        self.backgrounds.len()
    }
    fn background_open(&mut self, i: usize) {
        let id = self.backgrounds[i];
        self.log.push(format!("  bg-open {id}"));
    }
    fn background_close(&mut self, i: usize) {
        let id = self.backgrounds[i];
        self.log.push(format!("  bg-close {id}"));
    }
    fn db_count(&self) -> usize {
        self.db.len()
    }
    fn db_table(&self, i: usize) -> Handle {
        let t = self.db[i].0;
        if self.tables[t].is_some() {
            handle(t + 1)
        } else {
            core::ptr::null_mut()
        }
    }
    fn db_control(&self, i: usize) -> Handle {
        handle(self.db[i].1)
    }
    fn set_db_control(&mut self, i: usize, c: Handle) {
        self.db[i].1 = index(c);
    }
    fn table_create_control(&mut self, t: Handle) -> Handle {
        let x = self.tables[index(t) - 1].as_ref().unwrap().form_index;
        self.log.push(format!("  create-control {x}"));
        if x < 0 {
            return core::ptr::null_mut();
        }
        let (fx, fy) = ((x * 10) as f32, (x * 5) as f32);
        handle(self.new_control(x as c_ulong, UiRect::new(fx, fy, fx + 30.0, fy + 20.0)))
    }
    fn table_group(&self, t: Handle) -> c_ulong {
        self.tables[index(t) - 1].as_ref().unwrap().group
    }
    fn table_visible(&self, t: Handle) -> bool {
        self.tables[index(t) - 1].as_ref().unwrap().visible
    }
    fn table_control_type(&self, t: Handle) -> c_ulong {
        self.tables[index(t) - 1].as_ref().unwrap().kind
    }
    fn table_string_id(&self, t: Handle) -> c_ulong {
        self.tables[index(t) - 1].as_ref().unwrap().string_id
    }
    fn table_create_event(&mut self, t: Handle) {
        let x = self.tables[index(t) - 1].as_ref().unwrap().form_index;
        self.log.push(format!("  control-event {x}"));
    }
    fn add_control(&mut self, c: Handle, group: c_ulong) {
        let i = self.ctl(c).index as usize;
        if self.slots[i] == 0 {
            self.slots[i] = index(c);
        }
        self.groups[group as usize].1.push(index(c));
    }
    fn remove_control(&mut self, c: Handle, group: c_ulong) {
        let i = self.ctl(c).index as usize;
        self.slots[i] = 0;
        let list = &mut self.groups[group as usize].1;
        if let Some(at) = list.iter().position(|&e| e == index(c)) {
            list.remove(at);
        }
    }
    fn delete_control(&mut self, c: Handle) {
        if !c.is_null() {
            let i = self.ctl(c).index;
            self.log.push(format!("  delete {i}"));
        }
    }
    fn event_data_count(&self) -> usize {
        self.event_data.len()
    }
    fn event_data(&self, i: usize) -> (c_ulong, c_ulong) {
        self.event_data[i]
    }
    fn create_form_event(&mut self, id: c_ulong) -> Handle {
        self.log.push(format!("  create-event {id}"));
        if id.is_multiple_of(3) {
            return core::ptr::null_mut();
        }
        self.events.push(Event { id, refs: 1 });
        handle(self.events.len())
    }
    fn set_form_event(&mut self, kind: c_ulong, e: Handle) {
        self.set_event_slot(kind, index(e));
        self.dereference(index(e));
    }
    fn ctl_entity_type(&self, _c: Handle) -> c_ulong {
        ENTITY_CONTROL
    }
    fn ctl_mode(&self, c: Handle) -> c_ulong {
        self.ctl(c).mode
    }
    fn ctl_dirty(&self, c: Handle) -> bool {
        self.ctl(c).dirty
    }
    fn ctl_rect(&self, c: Handle) -> UiRect {
        self.ctl(c).rect
    }
    fn ctl_draw_count(&self, c: Handle) -> c_ulong {
        self.ctl(c).draws
    }
    fn ctl_index(&self, c: Handle) -> c_ulong {
        self.ctl(c).index
    }
    fn ctl_copy_data(&mut self, c: Handle) -> c_long {
        let s = self.surfaces;
        let k = self.ctl_mut(c);
        k.dirty = false;
        k.draws = s;
        let i = k.index;
        self.log.push(format!("  copydata {i}"));
        (i % 3) as c_long
    }
    fn ctl_set_dirty(&mut self, c: Handle, dirty: bool) {
        self.ctl_mut(c).dirty = dirty;
        let i = self.ctl(c).index;
        self.log.push(format!("  setdirty {i} {}", dirty as u8));
    }
    fn ctl_set_display_mode(&mut self, c: Handle, mode: c_ulong) {
        self.ctl_mut(c).mode = mode;
        let i = self.ctl(c).index;
        self.log.push(format!("  mode {i} {mode}"));
    }
    fn ctl_hide(&mut self, c: Handle) {
        let i = self.ctl(c).index;
        self.log.push(format!("  hide {i}"));
    }
    fn ctl_set_string_id(&mut self, c: Handle, id: c_ulong) {
        self.ctl_mut(c).string_id = id;
    }
}

#[test]
fn replays_the_cpp_form_bookkeeping() {
    let mut out: Vec<String> = Vec::new();
    let mut w: Option<World> = None;
    let flush = |w: &mut Option<World>, out: &mut Vec<String>| {
        if let Some(w) = w.as_mut() {
            out.append(&mut w.log);
        }
    };
    for line in GOLDEN.lines() {
        if line.starts_with("  ") {
            continue; // traces and state: the replay prints its own
        }
        let t: Vec<&str> = line.split(' ').collect();
        flush(&mut w, &mut out);
        out.push(line.to_string());
        if t[0] == "form" {
            let n: usize = t[8].parse().unwrap();
            // the driver's clock runs on across the forms
            let now = w.as_ref().map_or(1000, |w| w.time.get());
            w = Some(World {
                window: (t[3].parse().unwrap(), t[4].parse().unwrap()),
                surfaces: t[6].parse().unwrap(),
                slots: vec![0; n],
                draw: vec![0; n],
                shadow: t[12] == "1",
                tabbing: t[14] == "1",
                time: std::cell::Cell::new(now),
                ..World::default()
            });
            continue;
        }
        let w = w.as_mut().unwrap();
        match t[0] {
            "group" => w.groups.push((t[2] == "1", Vec::new())),
            "control" => {
                if t[2] == "free" {
                    continue;
                }
                let i: c_ulong = t[1].parse().unwrap();
                let c = w.new_control(i, parse_rect(&t[10..14]));
                let k = &mut w.ctls[c - 1];
                k.mode = t[5].parse().unwrap();
                k.dirty = t[7] == "1";
                k.draws = t[9].parse().unwrap();
                w.slots[i as usize] = c;
                let g: usize = t[3].parse().unwrap();
                w.groups[g].1.push(c);
            }
            "record" => {
                let table = if t[2] == "none" {
                    None
                } else {
                    Some(Table {
                        form_index: t[3].parse().unwrap(),
                        group: t[5].parse().unwrap(),
                        visible: t[7] == "1",
                        kind: t[9].parse().unwrap(),
                        string_id: t[11].parse().unwrap(),
                    })
                };
                w.tables.push(table);
                w.db.push((w.tables.len() - 1, 0));
            }
            "eventdata" => w.event_data.push((t[1].parse().unwrap(), t[2].parse().unwrap())),
            "background" => {
                // UIForm::AddBackground: before the first one of an equal or higher draw priority (id % 3)
                let id: c_ulong = t[1].parse().unwrap();
                let at = w.backgrounds.iter().position(|&b| id % 3 <= b % 3).unwrap_or(w.backgrounds.len());
                w.backgrounds.insert(at, id);
            }
            "update-event" => {
                w.events.push(Event { id: 1, refs: 1 });
                w.update_event = w.events.len();
            }
            "close-event" => {
                w.events.push(Event { id: 2, refs: 1 });
                w.close_event = w.events.len();
            }
            op => {
                match op {
                    "dirty" => w.form_add_dirty(parse_rect(&t[1..5])),
                    "copydirty" => add_copy_dirty_rectangle(w, parse_rect(&t[1..5])),
                    "copy" => {
                        let r = copy_data(w);
                        w.log.push(format!("  = {r}"));
                    }
                    "open" => open(w),
                    "close" => close(w),
                    "popup" => {
                        let c: c_long = t[1].parse().unwrap();
                        let ctl = if c < 0 { 0 } else { w.slots[c as usize] };
                        set_popup(w, handle(ctl));
                    }
                    "redraw" => redraw_intersect_controls(w, parse_rect(&t[1..5])),
                    "alldirty" => set_all_controls_dirty(w),
                    "shadow" => {
                        let r = tool_tip_shadow(&parse_rect(&t[1..5]));
                        w.log.push(format!("  = {}", rect_text(&r)));
                    }
                    "poke" => {
                        if t[2] != "none" {
                            let c = w.slots[t[1].parse::<usize>().unwrap()];
                            w.ctls[c - 1].dirty = t[2] == "1";
                            w.ctls[c - 1].mode = t[3].parse().unwrap();
                        }
                    }
                    "help" => {
                        let c: c_long = t[1].parse().unwrap();
                        w.help = if c < 0 { 0 } else { w.slots[c as usize] };
                    }
                    other => panic!("unknown operation {other}"),
                }
                w.dump();
            }
        }
    }
    flush(&mut w, &mut out);
    let golden: Vec<&str> = GOLDEN.lines().collect();
    assert!(golden.len() > 10_000, "the golden is the driver's output");
    for (i, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
    assert_eq!(out.len(), golden.len());
}

#[test]
fn copy_dirty_rectangles_never_overlap() {
    struct L(Vec<UiRect>, VecDeque<DirtyRect>);
    impl Lists for L {
        fn copy_len(&self) -> usize {
            self.0.len()
        }
        fn copy_at(&self, i: usize) -> UiRect {
            self.0[i]
        }
        fn copy_erase(&mut self, i: usize) {
            self.0.remove(i);
        }
        fn copy_push(&mut self, r: UiRect) {
            self.0.push(r);
        }
        fn dirty_len(&self) -> usize {
            self.1.len()
        }
        fn dirty_take_front(&mut self) -> DirtyRect {
            self.1.pop_front().unwrap()
        }
        fn dirty_push_back(&mut self, d: DirtyRect) {
            self.1.push_back(d);
        }
    }
    let mut l = L(Vec::new(), VecDeque::new());
    add_copy_dirty_rectangle(&mut l, UiRect::new(0.0, 0.0, 10.0, 10.0));
    // a covering rectangle replaces it
    add_copy_dirty_rectangle(&mut l, UiRect::new(-5.0, -5.0, 20.0, 20.0));
    assert_eq!(l.0, vec![UiRect::new(-5.0, -5.0, 20.0, 20.0)]);
    // an overlapping one is split into the parts outside it: right of it, then below it (which is split again
    // around the right part)
    add_copy_dirty_rectangle(&mut l, UiRect::new(10.0, 10.0, 30.0, 30.0));
    assert_eq!(
        l.0,
        vec![
            UiRect::new(-5.0, -5.0, 20.0, 20.0),
            UiRect::new(21.0, 10.0, 30.0, 30.0),
            UiRect::new(10.0, 21.0, 20.0, 30.0)
        ]
    );
    // the queue: two surfaces, then aged twice
    add_dirty_rectangle(&mut l, UiRect::new(0.0, 0.0, 5.0, 5.0), 2);
    add_dirty_rectangle(&mut l, UiRect::new(1.0, 1.0, 2.0, 2.0), 2);
    assert_eq!(l.1.len(), 1);
    age_dirty_rectangles(&mut l);
    assert_eq!(l.1[0].surface_count, 1);
    age_dirty_rectangles(&mut l);
    assert!(l.1.is_empty());
}
