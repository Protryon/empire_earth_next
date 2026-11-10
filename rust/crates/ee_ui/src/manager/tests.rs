//! The form stack against the real C++ (`golden_manager.txt`): a scratch driver built over the original
//! `UIManager.cpp` (the manager `calloc`ed with its lists constructed in place, seven traced fake forms whose
//! rectangles nest, overlap and coincide) printed 400 random `Push`, `Pop`, `PopTo`, `SwapForm`,
//! `AddDirtyRectangle`, `RedrawAll`, `RelayoutAllForms`, `GetFormByID` and `ProcessAllEventsForForm` calls: every
//! call on a form, then the stack and the visible list. The replay below is the same world over this module; the
//! same driver over the forwarders prints the same file.

use std::collections::VecDeque;

use super::*;

const GOLDEN: &str = include_str!("../golden_manager.txt");

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn rect(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

/// A form handle: its index + 1.
fn handle(i: usize) -> Form {
    (i + 1) as Form
}
fn index(f: Form) -> usize {
    f as usize - 1
}

struct World {
    forms: Vec<(c_ulong, UiRect)>,
    stack: Vec<Form>,
    visible: Vec<Form>,
    queue: VecDeque<Trigger>,
    log: Vec<String>,
}

impl World {
    fn id(&self, f: Form) -> c_ulong {
        self.forms[index(f)].0
    }
    fn say(&mut self, what: &str, f: Form) {
        let id = self.id(f);
        self.log.push(format!("  {what} {id}"));
    }
    fn say_rect(&mut self, what: &str, f: Form, r: UiRect) {
        let id = self.id(f);
        self.log.push(format!("  {what} {id} {}", rect(&r)));
    }
    fn ids(&self, name: &str, list: &[Form]) -> String {
        let mut s = format!("  {name}");
        for &f in list {
            s.push_str(&format!(" {}", self.id(f)));
        }
        s
    }
}

impl StackHost for World {
    fn stack_len(&self) -> usize {
        self.stack.len()
    }
    fn stack(&self, i: usize) -> Form {
        self.stack[i]
    }
    fn stack_push(&mut self, form: Form) {
        self.stack.push(form);
    }
    fn stack_pop(&mut self) {
        self.stack.pop();
    }
    fn visible_len(&self) -> usize {
        self.visible.len()
    }
    fn visible(&self, i: usize) -> Form {
        self.visible[i]
    }
    fn visible_insert(&mut self, i: usize, form: Form) {
        self.visible.insert(i, form);
    }
    fn visible_erase(&mut self, i: usize) {
        self.visible.remove(i);
    }
    fn visible_push(&mut self, form: Form) {
        self.visible.push(form);
    }
    fn visible_pop(&mut self) {
        self.visible.pop();
    }
    fn forms_len(&self) -> usize {
        self.forms.len()
    }
    fn forms(&self, i: usize) -> Form {
        handle(i)
    }
    fn copied_clear(&mut self) {}
    fn rect(&self, form: Form) -> UiRect {
        self.forms[index(form)].1
    }
    fn form_id(&self, form: Form) -> c_ulong {
        self.id(form)
    }
    fn open(&mut self, form: Form) {
        self.say("open", form);
    }
    fn close(&mut self, form: Form) {
        self.say("close", form);
    }
    fn activate(&mut self, form: Form) {
        self.say("activate", form);
    }
    fn deactivate(&mut self, form: Form) {
        self.say("deactivate", form);
    }
    fn add_dirty(&mut self, form: Form, r: UiRect) {
        self.say_rect("dirty", form, r);
    }
    fn redraw_intersect(&mut self, form: Form, r: UiRect) {
        self.say_rect("redraw", form, r);
    }
    fn clear_clips(&mut self, form: Form) {
        self.say("clear-clips", form);
    }
    fn add_clip(&mut self, form: Form, r: UiRect) {
        self.say_rect("clip", form, r);
    }
    fn relayout(&mut self, form: Form, r: UiRect) {
        self.say_rect("relayout", form, r);
    }
    fn set_all_dirty(&mut self, form: Form) {
        self.say("set-all-dirty", form);
    }
}

impl EventHost for World {
    fn take_front(&mut self) -> Option<Trigger> {
        self.queue.pop_front()
    }
    fn process(&mut self, t: Trigger) {
        let id = t.event as usize;
        let form = self.id(t.form);
        self.log.push(format!("  process {id} form {form} entity {}", if t.entity.is_null() { 0 } else { 1 }));
        self.log.push(format!("  event-free {id}"));
    }
    fn push_back(&mut self, t: Trigger) {
        self.queue.push_back(t);
    }
}

#[test]
fn replays_the_cpp_form_stack() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let mut blocks: Vec<(&str, Vec<&str>)> = Vec::new();
    for line in &lines {
        if line.starts_with("  ") {
            blocks.last_mut().unwrap().1.push(line);
        } else {
            blocks.push((line, Vec::new()));
        }
    }
    let mut w =
        World { forms: Vec::new(), stack: Vec::new(), visible: Vec::new(), queue: VecDeque::new(), log: Vec::new() };
    let by_id = |w: &World, id: c_ulong| handle(w.forms.iter().position(|f| f.0 == id).unwrap());
    let mut ops = [0usize; 10];
    for (header, body) in &blocks {
        let t: Vec<&str> = header.split(' ').collect();
        w.log.clear();
        let mut lists = true;
        match t[0] {
            "form" => {
                w.forms.push((
                    t[1].parse().unwrap(),
                    UiRect { left: fx(t[2]), top: fx(t[3]), right: fx(t[4]), bottom: fx(t[5]) },
                ));
                continue;
            }
            "push" => {
                let f = by_id(&w, t[1].parse().unwrap());
                push(&mut w, f);
                ops[0] += 1;
            }
            "pop" => {
                pop(&mut w);
                ops[1] += 1;
            }
            "popto" => {
                let f = by_id(&w, t[1].parse().unwrap());
                pop_to(&mut w, f);
                ops[2] += 1;
            }
            "swap" => {
                let f = by_id(&w, t[1].parse().unwrap());
                swap_form(&mut w, f);
                ops[3] += 1;
            }
            "dirty" => {
                let f = by_id(&w, t[1].parse().unwrap());
                let r = UiRect { left: fx(t[2]), top: fx(t[3]), right: fx(t[4]), bottom: fx(t[5]) };
                add_dirty_rectangle(&mut w, r, f);
                ops[4] += 1;
            }
            "redrawall" => {
                redraw_all(&mut w);
                ops[5] += 1;
            }
            "relayoutall" => {
                relayout_all_forms(&mut w, t[1].parse().unwrap(), t[2].parse().unwrap());
                ops[6] += 1;
            }
            "events" => {
                // "events <id>:<form> ... for <form>"
                let n = t.len() - 2;
                for (k, spec) in t[1..n].iter().enumerate() {
                    let (id, form) = spec.split_once(':').unwrap();
                    let form = by_id(&w, form.parse().unwrap());
                    w.queue.push_back(Trigger {
                        event: id.parse::<usize>().unwrap() as *mut c_void,
                        form,
                        entity: (k & 1) as *mut c_void,
                    });
                }
                let f = by_id(&w, t[n + 1].parse().unwrap());
                process_all_events_for_form(&mut w, f);
                let queued = w.queue.len();
                w.log.push(format!("  queued {queued}"));
                ops[7] += 1;
            }
            "byid" => {
                let id: c_ulong = t[1].parse().unwrap();
                let found = form_by_id(&w, id).map_or(-1, |f| w.id(f) as i64);
                assert_eq!(*header, format!("byid {id} = {found}"));
                ops[8] += 1;
                continue;
            }
            "drain" => {
                let f = by_id(&w, t[1].parse().unwrap());
                process_all_events_for_form(&mut w, f);
                let queued = w.queue.len();
                w.log.push(format!("  queued {queued}"));
                lists = false;
                ops[9] += 1;
            }
            "end" => break,
            _ => panic!("unknown line {header}"),
        }
        if lists {
            let s = w.ids("stack", &w.stack);
            let v = w.ids("visible", &w.visible);
            w.log.push(s);
            w.log.push(v);
        }
        assert_eq!(w.log, *body, "{header}");
    }
    assert_eq!(ops.iter().sum::<usize>(), 400 + 58 + 7, "the golden's shape");
    assert!(ops.iter().all(|&n| n > 0), "every operation ran: {ops:?}");
}

fn world(rects: &[UiRect]) -> World {
    World {
        forms: rects.iter().enumerate().map(|(i, &r)| (i as c_ulong + 1, r)).collect(),
        stack: Vec::new(),
        visible: Vec::new(),
        queue: VecDeque::new(),
        log: Vec::new(),
    }
}

#[test]
fn a_covering_form_hides_the_forms_below_until_it_goes() {
    let big = UiRect::new(0.0, 0.0, 100.0, 100.0);
    let small = UiRect::new(10.0, 10.0, 20.0, 20.0);
    let mut w = world(&[small, big, small]);
    push(&mut w, handle(0));
    push(&mut w, handle(1));
    assert_eq!(w.stack, vec![handle(0), handle(1)]);
    assert_eq!(w.visible, vec![handle(1)], "the big form covers the small one");
    assert!(w.log.contains(&"  close 1".to_string()));
    push(&mut w, handle(2));
    assert_eq!(w.visible, vec![handle(1), handle(2)]);
    w.log.clear();
    pop(&mut w);
    assert_eq!(w.visible, vec![handle(1)]);
    pop(&mut w);
    assert_eq!(w.visible, vec![handle(0)], "the covered form reopens");
    assert!(w.log.contains(&"  open 1".to_string()));
    assert!(w.log.contains(&"  activate 1".to_string()));
}

#[test]
fn pop_to_and_swap() {
    let r = UiRect::new(0.0, 0.0, 10.0, 10.0);
    let mut w = world(&[r, UiRect::new(20.0, 20.0, 30.0, 30.0), UiRect::new(40.0, 40.0, 50.0, 50.0), r]);
    for i in 0..3 {
        push(&mut w, handle(i));
    }
    pop_to(&mut w, handle(9)); // not stacked: nothing
    assert_eq!(w.stack.len(), 3);
    pop_to(&mut w, handle(0));
    assert_eq!(w.stack, vec![handle(0)]);
    swap_form(&mut w, handle(3));
    assert_eq!(w.stack, vec![handle(3)]);
    assert_eq!(w.visible, vec![handle(3)]);
    swap_form(&mut w, handle(3)); // already stacked: nothing
    assert_eq!(w.stack, vec![handle(3)]);
}

#[test]
fn dirty_rectangles_reach_the_forms_above_only_where_they_overlap() {
    let mut w = world(&[
        UiRect::new(0.0, 0.0, 100.0, 100.0),
        UiRect::new(50.0, 50.0, 80.0, 80.0),
        UiRect::new(200.0, 0.0, 300.0, 50.0),
    ]);
    for i in 0..3 {
        push(&mut w, handle(i));
    }
    w.log.clear();
    add_dirty_rectangle(&mut w, UiRect::new(60.0, 60.0, 120.0, 120.0), handle(0));
    assert_eq!(
        w.log,
        vec![
            "  dirty 1 42700000 42700000 42c80000 42c80000",
            "  redraw 1 42700000 42700000 42c80000 42c80000",
            "  dirty 2 42700000 42700000 42a00000 42a00000",
            "  redraw 2 42700000 42700000 42a00000 42a00000",
        ]
    );
    w.log.clear();
    add_dirty_rectangle(&mut w, UiRect::new(0.0, 0.0, 1.0, 1.0), handle(2));
    assert_eq!(w.log, vec!["  dirty 3 00000000 00000000 3f800000 3f800000"], "the top form takes it as is");
    assert_eq!(
        intersection(&UiRect::new(0.0, 0.0, 1.0, 1.0), &UiRect::new(1.0, 1.0, 2.0, 2.0)),
        Some(UiRect::new(1.0, 1.0, 1.0, 1.0))
    );
    assert_eq!(intersection(&UiRect::new(0.0, 0.0, 1.0, 1.0), &UiRect::new(1.5, 0.0, 2.0, 2.0)), None);
    assert!(contains(&UiRect::new(0.0, 0.0, 1.0, 1.0), &UiRect::new(0.0, 0.0, 1.0, 1.0)));
}

#[test]
fn events_of_one_form_run_in_order_and_the_rest_wait() {
    let mut w = world(&[UiRect::default(), UiRect::default()]);
    let t = |e: usize, f: usize| Trigger { event: e as *mut c_void, form: handle(f), entity: core::ptr::null_mut() };
    w.queue.extend([t(1, 0), t(2, 1), t(3, 0)]);
    process_all_events_for_form(&mut w, handle(0));
    assert_eq!(
        w.log,
        vec!["  process 1 form 1 entity 0", "  event-free 1", "  process 3 form 1 entity 0", "  event-free 3"]
    );
    assert_eq!(w.queue.len(), 1);
    w.log.clear();
    process_all_events(&mut w);
    assert_eq!(w.log, vec!["  process 2 form 2 entity 0", "  event-free 2"]);
    assert!(w.queue.is_empty());
    assert_eq!((wheel_scroll_amount(5), wheel_scroll_amount(-3), wheel_scroll_amount(0)), (-1, 1, 0));
}
