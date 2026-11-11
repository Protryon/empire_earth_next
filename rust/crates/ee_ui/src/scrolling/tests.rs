//! The scrolling label against the real C++ (`golden_scroll.txt`): a scratch driver (`build/ui_scroll_golden`) ran
//! the unmodified `UITLScrolling.cpp` (with the real `UITextLabel.cpp`, `UIControl.cpp` and `UIForm.cpp`; the
//! driver's timers log every start, pause, resume and poll, and poll what the driver says) over 120 labels (then 30 on whole pixels scrolled a pixel a frame to the end, round 9) (three
//! font heights, borders, up to 29 lines, a finished trigger or none) of 40 random operations: both `ScrollText`s
//! (speeds, delays, offsets, times shorter and longer than the scroll), `CopyData` with each timer firing or not,
//! `PauseScrolling`, `ResumeScrolling`, `StopScrolling` and moves, printing the scroll's members, the timers'
//! states and periods, the snapshot and the triggers fired after each. The replay is the same label over this
//! module, the C++'s own steps (the control's copy and move, the timers) emulated; the driver relinked over the
//! forwarders prints the same file, and the same on two more seeds at ten times the size.

use super::*;
use crate::label_fit::location_rects;

const GOLDEN: &str = include_str!("../golden_scroll.txt");
const FONT_HEIGHTS: [c_ulong; 3] = [8, 12, 23];
const NAMES: [&str; 3] = ["scroll", "start", "end"];

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn rect_text(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

fn rect_at(t: &[&str], at: usize) -> UiRect {
    UiRect::new(fx(t[at]), fx(t[at + 1]), fx(t[at + 2]), fx(t[at + 3]))
}

#[derive(Clone, Copy, Default)]
struct Poll {
    running: bool,
    delay: c_ulong,
    fires: c_ulong,
}

struct S {
    font: c_ulong,
    border: c_ulong,
    trigger: bool,
    displayed: c_ulong,
    rect: UiRect,
    text: UiRect,
    drect: UiRect,
    lines: Vec<String>,
    draw: Vec<String>,
    fields: [c_ulong; 7],
    dirty: bool,
    polls: [Poll; 3],
    triggered: u32,
    log: Vec<String>,
}

impl S {
    fn dump(&mut self) -> String {
        let f = &self.fields;
        let p = &self.polls;
        let mut s = format!(
            "  state start {} dstart {} end {} dcount {} off {} doff {} scrolling {} dirty {} polls {}{}{} {} {} {} \
             lines {} draw {} trig {} rect {} drect {}",
            f[0],
            f[1],
            f[2],
            f[3],
            f[4] as c_long,
            f[5],
            f[6],
            self.dirty as u8,
            p[0].running as u8,
            p[1].running as u8,
            p[2].running as u8,
            p[0].delay,
            p[1].delay,
            p[2].delay,
            self.lines.len(),
            self.draw.len(),
            self.triggered,
            rect_text(&self.rect),
            rect_text(&self.drect)
        );
        for i in 0..(f[3] as usize).min(self.draw.len()) {
            s.push(' ');
            s.push_str(&self.draw[i]);
        }
        self.dirty = false;
        self.triggered = 0;
        s
    }
}

impl ScrollHost for S {
    fn font_height(&mut self) -> c_ulong {
        self.font
    }
    fn rect(&self) -> UiRect {
        self.rect
    }
    fn border(&self) -> c_ulong {
        self.border
    }
    fn text_rect(&self) -> UiRect {
        self.text
    }
    fn get(&self, f: ScrollField) -> c_ulong {
        self.fields[f as usize]
    }
    fn set(&mut self, f: ScrollField, v: c_ulong) {
        self.fields[f as usize] = if f == ScrollField::Scrolling { (v != 0) as c_ulong } else { v };
    }
    fn poll(&mut self, p: ScrollPoll, c: PollCall, arg: c_ulong) -> c_ulong {
        let name = NAMES[p as usize];
        let t = &mut self.polls[p as usize];
        match c {
            PollCall::Running => return t.running as c_ulong,
            PollCall::Poll => {
                let n = t.fires;
                self.log.push(format!("  {name} poll {n}"));
                return n;
            }
            PollCall::Start => {
                t.running = true;
                self.log.push(format!("  {name} start"));
            }
            PollCall::Stop => t.running = false,
            PollCall::Pause => self.log.push(format!("  {name} pause")),
            PollCall::Resume => self.log.push(format!("  {name} resume")),
            PollCall::SetTimeDelay => t.delay = if arg > 0 { arg } else { 1 },
        }
        0
    }
    fn line_count(&self) -> usize {
        self.lines.len()
    }
    fn displayed(&self) -> c_ulong {
        self.displayed
    }
    fn resize_draw_lines(&mut self, n: usize) {
        self.draw.resize(n, "-".to_string());
    }
    fn copy_draw_rect(&mut self) {
        self.drect = self.text;
    }
    fn copy_line(&mut self, i: usize, line: usize) {
        self.draw[i] = self.lines[line].clone();
    }
    fn control_copy_data(&mut self) -> c_long {
        self.dirty = false;
        0
    }
    fn fire_finished(&mut self) {
        if self.trigger {
            self.triggered += 1;
        }
    }
    fn set_dirty(&mut self) {
        self.dirty = true;
    }
}

#[test]
fn replays_the_cpp_golden() {
    let golden: Vec<&str> = GOLDEN.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut s: Option<S> = None;
    let mut labels = 0;
    let mut at = 0;
    while at < golden.len() {
        let line = golden[at];
        let t: Vec<&str> = line.split(' ').collect();
        out.push(line.to_string());
        if t[0] == "label" {
            let n: usize = t[23].parse().unwrap();
            let mut l = S {
                font: FONT_HEIGHTS[t[3].parse::<usize>().unwrap()],
                border: t[5].parse().unwrap(),
                trigger: t[7] == "1",
                displayed: t[9].parse().unwrap(),
                rect: rect_at(&t, 11),
                text: rect_at(&t, 16),
                drect: UiRect::default(),
                lines: t[24..24 + n].iter().map(|s| s.to_string()).collect(),
                draw: Vec::new(),
                fields: [0; 7],
                dirty: false,
                polls: [Poll { running: false, delay: 1000, fires: 0 }; 3],
                triggered: 0,
                log: Vec::new(),
            };
            l.fields[ScrollField::PixelStartingOffset as usize] = t[21].parse::<c_long>().unwrap() as c_ulong;
            out.push(l.dump());
            s = Some(l);
            labels += 1;
            at += 2;
            continue;
        }
        let l = s.as_mut().unwrap();
        match t[1] {
            "scroll" => scroll_text(
                l,
                fx(t[2]),
                t[3].parse().unwrap(),
                t[4].parse().unwrap(),
                t[5].parse().unwrap(),
                t[6].parse().unwrap(),
            ),
            "scrollfor" => scroll_text_for(l, t[2].parse().unwrap(), t[3].parse().unwrap()),
            "pause" => pause_or_resume(l, false),
            "resume" => pause_or_resume(l, true),
            "stop" => stop_scrolling(l),
            "loc" => {
                // UITextLabel::SetLocation: the control's move and dirty flag, then the label's rectangles (the
                // scrolling label has no scroll bar)
                let (x, y) = (fx(t[2]), fx(t[3]));
                let r = l.rect;
                let (dx, dy) = (x - r.left, y - r.top);
                l.rect = UiRect::new(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy);
                l.dirty = true;
                l.text = location_rects(l.rect, l.border, false).1;
            }
            "copy" => {
                for (i, f) in t[2..5].iter().enumerate() {
                    l.polls[i].fires = f.parse().unwrap();
                }
                let r = copy_data(l);
                l.log.push(format!("  = {r}"));
            }
            other => panic!("op {other}"),
        }
        out.append(&mut l.log);
        out.push(l.dump());
        at += 1;
        while at < golden.len() && golden[at].starts_with("  ") && !golden[at].starts_with("  state") {
            at += 1;
        }
        at += 1;
    }
    for (n, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", n + 1);
    }
    assert_eq!(out.len(), golden.len(), "line count");
    assert_eq!(labels, 150);
}

#[test]
fn draw_lines_are_a_font_height_apart() {
    assert_eq!(draw_line_at(5.5, 12, 0, 40), (5.5, 40.0));
    assert_eq!(draw_line_at(5.5, 12, 3, 40), (5.5, 76.0));
}
