//! The three subclasses against the real C++ (`golden_buttons2.txt`): a scratch driver built over the original
//! `UITriStateCheckBox.cpp`, `UIBlinkingButton.cpp` and `UIScalableButton.cpp` (a fake `UIButton`/`UIControl` base
//! tracing every member they reach, a scripted `TMPoll`, traced bitmaps, label and rasterizer) printed two tri-state
//! boxes, two blinking buttons and two scalable buttons under 600 seeded operations, each followed by the control's
//! record. The replay below is the same world: the fakes as the driver's, the C++ members that stay C++
//! (`SetIsBlinking`, `DrawPolygons`, `DrawTextA`, `UIButton::CopyData`'s bitmap choice) as the C++ does them, and
//! everything else through this module. The same driver over the forwarders prints the same file.
#![allow(clippy::manual_dangling_ptr)]

use super::*;

const GOLDEN: &str = include_str!("../golden_buttons2.txt");

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn rect(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}
fn moved(r: &UiRect, at: UiPoint) -> UiRect {
    UiRect::new(at.x, at.y, at.x + r.width(), at.y + r.height())
}

const CHECK_NAMES: [(&str, &str); 3] = [("COFF", "CDOFF"), ("CON", "CDON"), ("CNOT", "CDNOT")];
const MARK: *mut c_void = 1 as *mut c_void;

/// A tri-state box as the fakes see it.
struct Tri {
    id: u32,
    index: u32,
    f: TriStateFields,
    rect: UiRect,
    border: c_ulong,
    label: Option<(UiRect, String)>,
    bitmaps: (&'static str, &'static str),
    pending_text: String,
    log: Vec<String>,
}

impl TriStateHost for Tri {
    fn load(&self) -> TriStateFields {
        self.f
    }
    fn store(&mut self, f: &TriStateFields) {
        self.f = *f;
    }
    fn rect(&self) -> UiRect {
        self.rect
    }
    fn border(&self) -> c_ulong {
        self.border
    }
    fn label_rect(&self) -> UiRect {
        self.label.as_ref().unwrap().0
    }
    fn label_set_rect(&mut self, r: UiRect) {
        self.label.as_mut().unwrap().0 = r;
        self.log.push(format!("  label-rect {}", rect(&r)));
    }
    fn label_set_location(&mut self, at: UiPoint) {
        let l = self.label.as_mut().unwrap();
        l.0 = moved(&l.0, at);
        self.log.push(format!("  label-location {} {}", h(at.x), h(at.y)));
    }
    fn label_set_text(&mut self) {
        let t = self.pending_text.clone();
        self.label.as_mut().unwrap().1 = t.clone();
        self.log.push(format!("  label-text '{t}'"));
    }
    fn label_create(&mut self, r: UiRect) {
        let t = self.pending_text.clone();
        self.log.push(format!("  label-new {} '{t}' {} type 0 index {} border 0", self.id, rect(&r), self.index));
        self.label = Some((r, t));
        self.f.text_label = MARK;
    }
    fn label_delete(&mut self) {
        let (_, t) = self.label.take().unwrap();
        self.log.push(format!("  label-delete '{t}'"));
    }
    fn base_set_rectangle(&mut self, r: UiRect) {
        self.rect = r;
        self.log.push(format!("  base-rect {}", rect(&r)));
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.rect = moved(&self.rect, at);
        self.log.push(format!("  base-location {}", rect(&self.rect)));
    }
    fn set_dirty(&mut self) {
        self.log.push("  dirty 1".to_string());
    }
    fn set_check_bitmaps(&mut self, state: c_ulong) {
        let (n, p) = CHECK_NAMES[state as usize];
        self.bitmaps = (n, p);
        self.log.push("  dirty 1".to_string());
        self.log.push(format!("  normal {n}"));
        self.log.push("  dirty 1".to_string());
        self.log.push(format!("  pressed {p}"));
    }
    fn fire(&mut self, event: c_ulong) {
        self.log.push(format!("  fire {event:x}"));
    }
    fn base_copy_data(&mut self) -> c_long {
        self.log.push(format!("  button-copy state 0 bitmap {}", self.bitmaps.0));
        self.log.push("  base-copy".to_string());
        1
    }
    fn label_copy_data(&mut self) -> c_long {
        self.log.push("  label-copy".to_string());
        11
    }
}

impl Tri {
    /// The constructor's trace over the fakes.
    fn new(id: u32, text: &str, r: UiRect, index: u32, border: c_ulong) -> Tri {
        let mut t = Tri {
            id,
            index,
            f: TriStateFields { text_label: core::ptr::null_mut(), check_state: 0, has_text: false },
            rect: tri_state_rect(&r),
            border,
            label: None,
            bitmaps: CHECK_NAMES[0],
            pending_text: text.to_string(),
            log: Vec::new(),
        };
        t.log.push(format!("  control {id} {} border {border}", rect(&t.rect)));
        t.log.push("  button COFF - CDOFF".to_string());
        if let Some(o) = tri_state_bitmap_offset(r.height(), border) {
            t.log.push(format!("  draw-offset {} {}", h(o.x), h(o.y)));
        }
        let bytes = crate::text::from_bytes(text.as_bytes());
        tri_state_set_text(&mut t, &bytes);
        t
    }
    fn dump(&self) -> String {
        let mut s = format!(
            "  tri state {} text {} label {} rect {}",
            self.f.check_state,
            self.f.has_text as u8,
            (!self.f.text_label.is_null()) as u8,
            rect(&self.rect)
        );
        if let Some((r, _)) = &self.label {
            s.push_str(&format!(" label-rect {}", rect(r)));
        }
        s.push_str(&format!(" normal {} pressed {}", self.bitmaps.0, self.bitmaps.1));
        s
    }
}

/// A blinking button as the fakes see it.
struct Blink {
    f: BlinkFields,
    /// The control's own background and border opacities (the fake `UIControl`'s 0.75 and 0.5).
    bg: f32,
    border: f32,
    state: c_ulong,
    rect: UiRect,
    bitmaps: [Option<&'static str>; 3],
    always_dirty: bool,
    copied: Option<&'static str>,
    log: Vec<String>,
}

impl Blink {
    fn new(id: u32, bitmaps: [Option<&'static str>; 3], r: UiRect, border: c_ulong, rate: c_ulong) -> Blink {
        let mut b = Blink {
            f: blink_init(rate, false),
            bg: 0.75,
            border: 0.5,
            state: 0,
            rect: r,
            bitmaps,
            always_dirty: false,
            copied: None,
            log: Vec::new(),
        };
        b.log.push(format!("  control {id} {} border {border}", rect(&r)));
        let n = |x: Option<&str>| x.unwrap_or("-").to_string();
        b.log.push(format!("  button {} {} {}", n(bitmaps[0]), n(bitmaps[1]), n(bitmaps[2])));
        b.log.push(format!("  poll-delay {}", b.f.blink_rate));
        b
    }
    fn dump(&self) -> String {
        let f = &self.f;
        format!(
            "  blink phase {} rate {} opacities {} {} {} {} saved {} needs {} blinking {} state {}",
            f.blink_phase,
            f.blink_rate,
            h(f.pressed_opacity),
            h(f.normal_opacity),
            h(f.bg_opacity),
            h(f.full_border_opacity),
            f.saved_always_dirty as u8,
            f.needs_opacities as u8,
            f.is_blinking as u8,
            self.state
        )
    }
    /// `SetIsBlinking` (C++).
    fn set_is_blinking(&mut self, on: bool) {
        if self.f.is_blinking == on {
            return;
        }
        self.f.is_blinking = on;
        self.log.push("  poll-stop".to_string());
        if on {
            self.log.push("  poll-start".to_string());
            self.always_dirty = true;
            self.log.push("  always-dirty 1".to_string());
            self.log.push("  dirty 1".to_string());
            self.log.push("  display 1".to_string());
        } else {
            self.always_dirty = self.f.saved_always_dirty;
            self.log.push(format!("  always-dirty {}", self.always_dirty as u8));
            self.log.push("  dirty 1".to_string());
        }
    }
    /// `UIButton::CopyData` (the fake): the bitmap for the state.
    fn button_copy(&mut self) {
        self.copied = self.bitmaps[self.state as usize];
        self.log.push(format!("  button-copy state {} bitmap {}", self.state, self.copied.unwrap_or("-")));
        self.log.push("  base-copy".to_string());
    }
}

/// A scalable button as the fakes see it.
struct Scalable {
    rects: [UiRect; 3],
    copied_rect: UiRect,
    draw_rect: UiRect,
    rect: UiRect,
    state: c_ulong,
    bitmaps: [Option<&'static str>; 3],
    texture_width: c_ulong,
    copied: Option<&'static str>,
    log: Vec<String>,
}

impl Scalable {
    fn dump(&self) -> String {
        format!("  scalable state {} copied {} draw {}", self.state, rect(&self.copied_rect), rect(&self.draw_rect))
    }
}

#[test]
fn replays_the_cpp_three_subclasses() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let mut blocks: Vec<(&str, Vec<&str>)> = Vec::new();
    for line in &lines {
        if line.starts_with("  ") {
            blocks.last_mut().unwrap().1.push(line);
        } else {
            blocks.push((line, Vec::new()));
        }
    }
    let r = UiRect::new;
    let mut tris =
        [Tri::new(10, "Alpha", r(10.0, 20.0, 25.0, 30.0), 3, 1), Tri::new(11, "", r(100.0, 50.0, 300.0, 110.0), 4, 2)];
    let mut blinks = [
        Blink::new(20, [Some("N0"), Some("F0"), Some("P0")], r(5.0, 5.0, 69.0, 37.0), 1, 0),
        Blink::new(21, [Some("N1"), None, None], r(200.0, 200.0, 296.0, 232.0), 0, 7000),
        Blink::new(22, [None, None, Some("P1")], r(0.0, 0.0, 10.0, 10.0), 0, 30),
    ];
    let mut scalables = [
        Scalable {
            rects: [r(4.0, 0.0, 59.0, 31.0), r(6.0, 32.0, 57.0, 63.0), r(8.0, 64.0, 55.0, 95.0)],
            copied_rect: UiRect::default(),
            draw_rect: UiRect::default(),
            rect: r(100.0, 300.0, 400.0, 331.0),
            state: 0,
            bitmaps: [Some("N0"), Some("F0"), Some("P0")],
            texture_width: 64,
            copied: None,
            log: vec!["  control 30 42c80000 43960000 43c80000 43a58000 border 0".into(), "  button N0 F0 P0".into()],
        },
        Scalable {
            rects: [r(10.0, 0.0, 85.0, 31.0), r(0.0, 0.0, 1.0, 1.0), r(12.0, 32.0, 83.0, 63.0)],
            copied_rect: UiRect::default(),
            draw_rect: UiRect::default(),
            rect: r(50.0, 50.0, 149.0, 81.0),
            state: 0,
            bitmaps: [Some("N1"), None, Some("P1")],
            texture_width: 96,
            copied: None,
            log: vec!["  control 31 42480000 42480000 43150000 42a20000 border 0".into(), "  button N1 - P1".into()],
        },
    ];
    let mut ops = [0usize; 3];
    for (header, body) in &blocks {
        let t: Vec<&str> = header.split(' ').collect();
        let mut want: Vec<String>;
        match t[0] {
            "new" => {
                let k: usize = t.get(2).map_or(0, |k| k.parse().unwrap());
                want = match t[1] {
                    "tri" => {
                        let x = &mut tris[k];
                        let mut w = core::mem::take(&mut x.log);
                        w.push(x.dump());
                        w
                    }
                    "blink" => {
                        let x = &mut blinks[k];
                        let mut w = core::mem::take(&mut x.log);
                        w.push(x.dump());
                        w
                    }
                    _ => {
                        let x = &mut scalables[if t.len() > 2 { 1 } else { 0 }];
                        let mut w = core::mem::take(&mut x.log);
                        w.push(x.dump());
                        w
                    }
                };
            }
            "tri" => {
                let x = &mut tris[(t[1].parse::<u32>().unwrap() - 10) as usize];
                x.log.clear();
                match t[2] {
                    "setstate" => tri_state_set_check_state(x, t[3].parse().unwrap()),
                    "buttonup" => {
                        tri_state_cycle(x);
                        x.log.push("  button-up".into());
                    }
                    "width" => tri_state_change_width(x, t[3].parse().unwrap()),
                    "location" => tri_state_set_location(x, UiPoint { x: fx(t[3]), y: fx(t[4]) }),
                    "text" => {
                        let s = header.split('\'').nth(1).unwrap().to_string();
                        x.pending_text = s.clone();
                        let bytes = crate::text::from_bytes(s.as_bytes());
                        tri_state_set_text(x, &bytes);
                    }
                    "copy" => {
                        let res = tri_state_copy_data(x);
                        x.log.push(format!("  = {res}"));
                    }
                    "polygons" => {
                        x.log.push("  button-draw-polygons".into());
                        let mut res = 4;
                        if x.label.is_some() {
                            x.log.push("  label-draw-polygons".into());
                            res = 12;
                        }
                        x.log.push(format!("  = {res}"));
                    }
                    "drawtext" => {
                        x.log.push("  base-draw-text".into());
                        let mut res = 3;
                        if x.label.is_some() {
                            x.log.push("  label-draw-text".into());
                            res = 13;
                        }
                        x.log.push(format!("  = {res}"));
                    }
                    _ => x.log.extend(["  = 12".to_string(), "  tabbed-to".into(), "  tabbed-from".into()]),
                }
                want = core::mem::take(&mut x.log);
                want.push(x.dump());
                ops[0] += 1;
            }
            "blink" => {
                let x = &mut blinks[(t[1].parse::<u32>().unwrap() - 20) as usize];
                x.log.clear();
                match t[2] {
                    "set" => x.set_is_blinking(t[3] == "1"),
                    "state" => x.state = t[3].parse().unwrap(),
                    "polygons" => {
                        // printf's arguments: DrawTextA runs first (MSVC: right to left)
                        x.log.push("  button-draw-text".into());
                        x.log.push("  base-draw-polygons".into());
                        let mut res = 2;
                        if let Some(name) = x.copied {
                            let at = x.rect.top_left();
                            x.log.push(format!("  bitmap-at {name} {} {}", h(at.x), h(at.y)));
                            x.log.push(format!("  draw {name}"));
                            res = 7;
                        }
                        x.log.push(format!("  = {res} 5"));
                    }
                    _ => {
                        // "copy poll P left L bg B border Bo": the driver's `b->mBgOpacity` is the blinking button's
                        // own member (it shadows the control's), `b->mBorderOpacity` the control's
                        x.f.bg_opacity = fx(t[8]);
                        x.border = fx(t[10]);
                        let show =
                            blink_copy_data(&mut x.f, t[4] == "1", t[6].parse().unwrap(), x.state, x.bg, x.border);
                        x.bg = show.background;
                        x.border = show.border;
                        x.log.push(format!("  bg-opacity {}", h(show.background)));
                        x.log.push(format!("  border-opacity {}", h(show.border)));
                        if let Some(p) = x.bitmaps[2] {
                            x.log.push(format!("  opacity {p} {}", h(show.pressed)));
                        }
                        if let Some(n) = x.bitmaps[0] {
                            x.log.push(format!("  opacity {n} {}", h(show.normal)));
                        }
                        x.button_copy();
                        x.log.push("  = 1".into());
                    }
                }
                want = core::mem::take(&mut x.log);
                want.push(x.dump());
                ops[1] += 1;
            }
            "scalable" => {
                let x = &mut scalables[(t[1].parse::<u32>().unwrap() - 30) as usize];
                x.log.clear();
                match t[2] {
                    "state" => x.state = t[3].parse().unwrap(),
                    "copy" => {
                        x.copied_rect = scalable_copied_rect(
                            x.state,
                            x.bitmaps[1].is_some(),
                            &x.rects[0],
                            &x.rects[1],
                            &x.rects[2],
                            &x.copied_rect,
                        );
                        x.copied = x.bitmaps[x.state as usize];
                        x.log.push(format!("  button-copy state {} bitmap {}", x.state, x.copied.unwrap_or("-")));
                        x.draw_rect = x.rect;
                        x.log.push("  base-copy".into());
                        x.log.push("  = 1".into());
                    }
                    "polygons" => {
                        let mut res = 0;
                        if let Some(name) = x.copied {
                            for s in scalable_slices(&x.copied_rect, &x.draw_rect, x.texture_width) {
                                x.log.push(format!(
                                    "  stretch {name} {} | {} | {}",
                                    rect(&s.destination),
                                    rect(&s.destination),
                                    rect(&s.source)
                                ));
                                x.log.push(format!("  bitmap-at {name} {} {}", h(s.location.x), h(s.location.y)));
                                x.log.push(format!("  draw {name}"));
                            }
                            res = 7;
                        }
                        x.log.push(format!("  = {res}"));
                    }
                    _ => {
                        x.log.push("  button-location".into());
                        x.rect = moved(&x.rect, UiPoint { x: fx(t[3]), y: fx(t[4]) });
                        x.log.push(format!("  base-location {}", rect(&x.rect)));
                    }
                }
                want = core::mem::take(&mut x.log);
                want.push(x.dump());
                ops[2] += 1;
            }
            "end" => break,
            _ => panic!("unknown line {header}"),
        }
        assert_eq!(*body, want, "{header}");
    }
    assert_eq!(ops.iter().sum::<usize>(), 600, "the golden's shape");
    assert!(ops.iter().all(|&n| n > 100), "{ops:?}");
}

#[test]
fn the_tri_state_box_cycles_and_keeps_an_unknown_state() {
    let mut t = Tri::new(1, "x", UiRect::new(0.0, 0.0, 100.0, 40.0), 0, 0);
    t.log.clear();
    tri_state_cycle(&mut t);
    assert_eq!(t.f.check_state, CHECK_ON);
    assert_eq!(t.bitmaps, ("CON", "CDON"));
    tri_state_cycle(&mut t);
    tri_state_cycle(&mut t);
    assert_eq!(t.f.check_state, CHECK_OFF);
    t.log.clear();
    tri_state_set_check_state(&mut t, 7);
    assert_eq!(t.f.check_state, 7);
    assert!(t.log.is_empty(), "no bitmaps, no trigger");
    assert_eq!(tri_state_rect(&UiRect::new(0.0, 0.0, 10.0, 10.0)), UiRect::new(0.0, 0.0, 19.0, 19.0));
    assert_eq!(tri_state_bitmap_offset(10.0, 0), None);
    assert_eq!(tri_state_bitmap_offset(40.0, 1), Some(UiPoint { x: 0.0, y: 9.0 }));
}

#[test]
fn the_blink_rate_clamps_and_the_fade_is_full_when_not_blinking() {
    assert_eq!((blink_rate(0), blink_rate(10), blink_rate(9000), blink_rate(300)), (250, 50, 5000, 300));
    let mut f = blink_init(0, true);
    assert!(f.saved_always_dirty && f.needs_opacities && !f.is_blinking);
    let show = blink_copy_data(&mut f, false, 100, STATE_NORMAL, 0.5, 0.25);
    assert_eq!(show, BlinkShow { background: 0.5, border: 0.25, pressed: 1.0, normal: 1.0 });
    assert!(!f.needs_opacities);
    f.is_blinking = true;
    let full = blink_copy_data(&mut f, false, 0, STATE_NORMAL, 9.0, 9.0);
    assert_eq!(full, show, "fading out, no time left: full");
    let gone = blink_copy_data(&mut f, false, 250, STATE_NORMAL, 9.0, 9.0);
    assert!(gone.background.abs() < 1e-6, "{gone:?}");
    assert_eq!(blink_copy_data(&mut f, true, 0, STATE_PRESSED, 9.0, 9.0), show, "pressed: full");
    assert_eq!(f.blink_phase, 0, "the poll flipped the phase");
}

#[test]
fn the_scalable_slices_cut_the_caps_and_stretch_the_middle() {
    let copied = UiRect::new(4.0, 0.0, 59.0, 31.0);
    let draw = UiRect::new(100.0, 300.0, 400.0, 331.0);
    let [left, right, middle] = scalable_slices(&copied, &draw, 64);
    assert_eq!(left.source, UiRect::new(0.0, 0.0, 5.0, 31.0));
    assert_eq!(left.destination, UiRect::new(100.0, 300.0, 104.0, 331.0));
    assert_eq!(left.location, UiPoint { x: 100.0, y: 300.0 });
    assert_eq!(right.source, UiRect::new(60.0, 0.0, 64.0, 31.0));
    assert_eq!(right.destination, UiRect::new(397.0, 300.0, 400.0, 331.0));
    assert_eq!(middle.source, copied);
    assert_eq!(middle.destination, UiRect::new(105.0, 300.0, 396.0, 331.0));
    let n = UiRect::new(1.0, 1.0, 2.0, 2.0);
    let f = UiRect::new(3.0, 3.0, 4.0, 4.0);
    let p = UiRect::new(5.0, 5.0, 6.0, 6.0);
    let c = UiRect::new(7.0, 7.0, 8.0, 8.0);
    assert_eq!(scalable_copied_rect(STATE_FOCUSED, false, &n, &f, &p, &c), n);
    assert_eq!(scalable_copied_rect(STATE_FOCUSED, true, &n, &f, &p, &c), f);
    assert_eq!(scalable_copied_rect(STATE_PRESSED, true, &n, &f, &p, &c), p);
    assert_eq!(scalable_copied_rect(9, true, &n, &f, &p, &c), c);
}
