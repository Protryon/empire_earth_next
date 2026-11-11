//! The drawing against the real C++ (`golden_draw.txt`): a scratch golden driver built over the unmodified UI units
//! (`UIControl.cpp`, `UIForm.cpp`, `UITextLabel.cpp`, `UILBString.cpp` with `UIListBox.h`, `UIButton.cpp`,
//! `UICheckBox.cpp`, `UIRadioButton.cpp`, the scroll bar's units, `UIEditBox.cpp`), the real `GELine`, `GERhombus`,
//! `GERectangle` and strings, calloc'ed controls with their classes' vtables, a fake rasterizer whose every draw is a
//! log line with its arguments (the line's ends, colour and opacity; the rhombus's rectangle, colour and opacities;
//! the bitmap's location and scale; the text's bytes, location, colour and clip rectangle) and a fake text measure
//! (a hash advance per code point times the font's height / 8 + 1). Each case is its inputs (the header) and its
//! log as a line count and an FNV-1a 64 digest. The replay below rebuilds each log from the header over this
//! module, with the same fakes; the same driver relinked over the forwarders prints the same file.

use super::*;

const GOLDEN: &str = include_str!("../golden_draw.txt");

const FONT_HEIGHTS: [c_ulong; 4] = [8, 12, 16, 23];

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

/// The fake measure: 3 + (c * 7) % 9 per code point (a stray byte is one), times height / 8 + 1.
fn width(text: &[u8], height: c_ulong) -> f32 {
    let mut w = 0.0f32;
    let mut at = 0usize;
    let mut z = text.to_vec();
    z.push(0);
    while at < text.len() {
        let mut l = 0u32;
        let c = unsafe { ee_plat_core::string::PlatUtf8Decode(z.as_ptr().add(at).cast(), &mut l) };
        let l = if l == 0 { 1 } else { l as usize };
        w += (3u32.wrapping_add(c.wrapping_mul(7) % 9)) as f32 * (height / 8 + 1) as f32;
        at += l;
    }
    w
}

fn fnv(lines: &[String]) -> u64 {
    let mut x: u64 = 0xcbf2_9ce4_8422_2325;
    for l in lines {
        for b in l.bytes().chain(core::iter::once(b'\n')) {
            x ^= b as u64;
            x = x.wrapping_mul(0x0000_0100_0000_01b3);
        }
    }
    x
}

fn hex_text(t: &str) -> Vec<u8> {
    if t == "-" {
        return Vec::new();
    }
    (0..t.len()).step_by(2).map(|i| u8::from_str_radix(&t[i..i + 2], 16).unwrap()).collect()
}
fn text_hex(t: &[u8]) -> String {
    if t.is_empty() {
        "-".into()
    } else {
        t.iter().map(|b| format!("{b:02x}")).collect()
    }
}

/// A header's tokens.
struct Cursor<'a> {
    t: Vec<&'a str>,
    at: usize,
}

impl<'a> Cursor<'a> {
    fn next(&mut self) -> &'a str {
        self.at += 1;
        self.t[self.at - 1]
    }
    fn key(&mut self, k: &str) {
        let got = self.next();
        assert_eq!(got, k, "header {:?}", self.t);
    }
    fn f(&mut self) -> f32 {
        f32::from_bits(u32::from_str_radix(self.next(), 16).unwrap())
    }
    fn u(&mut self) -> c_ulong {
        self.next().parse().unwrap()
    }
    fn i(&mut self) -> c_long {
        self.next().parse().unwrap()
    }
    fn b(&mut self) -> bool {
        self.u() != 0
    }
    fn rect(&mut self, k: &str) -> UiRect {
        self.key(k);
        let (l, t, r, b) = (self.f(), self.f(), self.f(), self.f());
        UiRect::new(l, t, r, b)
    }
    fn color(&mut self, k: &str) -> [f32; 3] {
        self.key(k);
        [self.f(), self.f(), self.f()]
    }
    fn fk(&mut self, k: &str) -> f32 {
        self.key(k);
        self.f()
    }
    fn uk(&mut self, k: &str) -> c_ulong {
        self.key(k);
        self.u()
    }
    fn text(&mut self) -> Vec<u8> {
        hex_text(self.next())
    }
}

fn col(c: &[f32; 3]) -> String {
    format!("{} {} {}", h(c[0]), h(c[1]), h(c[2]))
}
fn rect_hex(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

const WHITE: [f32; 3] = [1.0, 1.0, 1.0];

/// A control's base (`UIControl`): its rectangle, border and background.
struct Base {
    rect: UiRect,
    thickness: c_ulong,
    border_type: c_ulong,
    border_opacity: f32,
    bg: bool,
    bg_color: [f32; 3],
    bg_opacity: f32,
}

fn base(c: &mut Cursor) -> Base {
    let rect = c.rect("rect");
    c.key("border");
    let (thickness, border_type) = (c.u(), c.u());
    let border_opacity = c.fk("bop");
    c.key("bg");
    let bg = c.b();
    let bg_color = c.color("bgc");
    let bg_opacity = c.fk("bgo");
    Base { rect, thickness, border_type, border_opacity, bg, bg_color, bg_opacity }
}

/// The form's clipping, as `random_clipping` printed it.
struct Clip {
    mode: c_ulong,
    rects: Vec<UiRect>,
    popup: (UiRect, UiRect),
    ime: UiRect,
    dr: c_long,
}

fn clip(c: &mut Cursor) -> Clip {
    c.key("clip");
    let mode = c.u();
    let mut k = Clip { mode, rects: Vec::new(), popup: Default::default(), ime: UiRect::default(), dr: 0 };
    match mode {
        1 | 2 => {
            let n = c.u();
            for _ in 0..n {
                k.rects.push(c.rect("c"));
            }
        }
        3 => k.popup = (c.rect("pr"), c.rect("pd")),
        5 => k.ime = c.rect("ime"),
        _ => {}
    }
    c.key("dr");
    k.dr = c.i();
    k
}

impl Clip {
    /// The clipping a control's text is drawn under: `own` is whether the control is the case's own one (the
    /// popup in mode 4, its rectangle and copied draw rectangle `own_rects`).
    fn clipping(&self, own: bool, own_rects: (UiRect, UiRect)) -> Clipping {
        match self.mode {
            1 | 2 => Clipping::Rectangles(self.rects.clone()),
            3 => Clipping::Popup { is_control: false, rect: self.popup.0, draw_rect: self.popup.1 },
            4 => Clipping::Popup { is_control: own, rect: own_rects.0, draw_rect: own_rects.1 },
            5 => Clipping::Plain { candidate: Some(self.ime) },
            _ => Clipping::Plain { candidate: None },
        }
    }
}

/// The replayed log.
struct Log {
    lines: Vec<String>,
    dr: c_long,
}

impl Log {
    fn rhombus(&mut self, r: &UiRect, c: &[f32; 3], o: f32) -> c_long {
        let o = h(o);
        self.lines.push(format!("  rhombus {} col {} op {o} {o} {o} {o} scr 0", rect_hex(r), col(c)));
        self.dr
    }
    fn line(&mut self, s: UiPoint, e: UiPoint, c: &[f32; 3], o: f32) -> c_long {
        self.lines.push(format!(
            "  line {} {} {} {} col {} op {} n 1 scr 0",
            h(s.x),
            h(s.y),
            h(e.x),
            h(e.y),
            col(c),
            h(o)
        ));
        self.dr
    }
    fn bitmap(&mut self, id: u32, at: UiPoint, xs: f32, ys: f32) -> c_long {
        self.lines.push(format!("  bitmap {id} {} {} scale {} {} scr 0", h(at.x), h(at.y), h(xs), h(ys)));
        self.dr
    }
    /// `UIControl::DrawPolygons`: the copied background, then the border's lines in Rust's plan.
    fn control(&mut self, b: &Base) -> c_long {
        if b.bg {
            self.rhombus(&b.rect, &b.bg_color, b.bg_opacity);
        }
        let mut result = 0;
        if b.thickness > 0 {
            for side in 0..4 {
                for i in 0..b.thickness {
                    let l = border_line(&b.rect, side, i, b.border_type);
                    let c = if l.light { WHITE } else { [0.0, 0.0, 0.0] };
                    result = self.line(l.start, l.end, &c, b.border_opacity);
                }
            }
        }
        result
    }
    /// `UIForm::DrawClippedText` of a text at `at` in colour `fg`.
    fn text(&mut self, text: &[u8], at: UiPoint, fg: &[f32; 3], r: UiRect, clipping: &Clipping) -> c_long {
        struct T<'a> {
            log: &'a mut Log,
            prefix: String,
        }
        impl TextDraw for T<'_> {
            fn draw(&mut self, r: UiRect) -> c_long {
                self.log.lines.push(format!("{} rect {}", self.prefix, rect_hex(&r)));
                self.log.dr
            }
        }
        let prefix = format!("  text {} {} {} fg {}", text_hex(text), h(at.x), h(at.y), col(fg));
        draw_clipped_text(&mut T { log: self, prefix }, r, clipping)
    }
}

/// A one-line label (the check box's and radio button's, `small_label`): rectangle, alignment, font, text.
struct SmallLabel {
    rect: UiRect,
    alignment: c_ulong,
    font: usize,
    text: Vec<u8>,
}

fn small_label(c: &mut Cursor) -> SmallLabel {
    c.key("label");
    let rect = c.rect("lr");
    c.key("la");
    let alignment = c.u();
    c.key("lf");
    let font = c.u() as usize;
    c.key("lt");
    SmallLabel { rect, alignment, font, text: c.text() }
}

impl SmallLabel {
    fn location(&self) -> UiPoint {
        let hgt = FONT_HEIGHTS[self.font];
        label_one_line_location(self.alignment, false, &self.rect, width(&self.text, hgt), hgt as f32)
    }
}

fn replay(kind: &str, c: &mut Cursor, log: &mut Log) {
    match kind {
        "border" => {
            let mut b = base(c);
            b.thickness = c.uk("thick");
            c.key("dr");
            log.dr = c.i();
            let r = log.control(&b);
            log.lines.push(format!("  result {r}"));
        }
        "clip" => {
            let r = c.rect("r");
            c.key("t");
            let text = c.text();
            c.key("null");
            let null = c.b();
            let k = clip(c);
            log.dr = k.dr;
            let clipping = k.clipping(!null, (UiRect::default(), UiRect::default()));
            let res = log.text(&text, r.top_left(), &WHITE, r, &clipping);
            log.lines.push(format!("  result {res}"));
        }
        "label1" | "labelN" => {
            let b = base(c);
            let alignment = c.uk("align");
            c.key("fit");
            let fit = c.b();
            let font = c.uk("font") as usize;
            let tr = c.rect("tr");
            let fg = c.color("fg");
            let lines = c.uk("lines");
            let shown = c.uk("shown");
            let texts: Vec<Vec<u8>> = (0..lines).map(|_| c.text()).collect();
            c.key("sb");
            let sb = match c.next() {
                "-" => None,
                v => {
                    let value: c_long = v.parse().unwrap();
                    Some((value, c.rect("sbr"), c.rect("thumb")))
                }
            };
            let k = clip(c);
            log.dr = k.dr;
            let clipping = k.clipping(true, (b.rect, b.rect));
            let hgt = FONT_HEIGHTS[font];
            log.lines.push("  copy 0".into());
            if kind == "label1" {
                let text = texts.first().cloned().unwrap_or_default();
                let at = label_one_line_location(alignment, fit, &tr, width(&text, hgt), hgt as f32);
                let p = log.control(&b);
                log.lines.push(format!("  polygons {p}"));
                let res = log.text(&text, at, &fg, b.rect, &clipping);
                log.lines.push(format!("  result {res}"));
            } else {
                let range = label_line_range(sb.map(|s| s.0), shown, lines);
                let shown_lines: Vec<Vec<u8>> = (range.first..range.end).map(|i| texts[i as usize].clone()).collect();
                let mut p = log.control(&b);
                if let Some((_, _, thumb)) = sb {
                    log.bitmap(100, thumb.top_left(), 1.0, 1.0);
                    p = 0;
                }
                log.lines.push(format!("  polygons {p}"));
                let mut res = 0;
                let mut offset: c_ulong = 0;
                let mut i: c_long = 0;
                while i <= range.last {
                    let text = &shown_lines[i as usize];
                    let (at, r) = label_line(
                        alignment,
                        fit,
                        &b.rect,
                        &tr,
                        width(text, hgt),
                        sb.map(|s| s.1.width()),
                        offset,
                        hgt,
                    );
                    res = log.text(text, at, &fg, r, &clipping);
                    i += 1;
                    offset = offset.wrapping_add(hgt);
                }
                if sb.is_some() {
                    res = 0;
                }
                log.lines.push(format!("  result {res}"));
            }
        }
        "list" => {
            let b = base(c);
            let font = c.uk("font") as usize;
            let lh = c.uk("lh");
            let tr = c.rect("tr");
            let fg = c.color("fg");
            let sel = c.color("sel");
            let selo = c.fk("selo");
            let foc = c.color("foc");
            c.key("hassel");
            let has_selection = c.b();
            let cur = c.uk("cur");
            c.key("tab");
            let tab = c.b();
            let stop = c.uk("stop");
            let lines = c.uk("lines");
            let shown = c.uk("shown");
            let texts: Vec<Vec<u8>> = (0..lines).map(|_| c.text()).collect();
            c.key("sb");
            let sb = match c.next() {
                "-" => None,
                v => {
                    let value: c_long = v.parse().unwrap();
                    Some((value, c.rect("thumb")))
                }
            };
            let k = clip(c);
            log.dr = k.dr;
            let clipping = k.clipping(true, (b.rect, b.rect));
            let _ = font;
            let hgt = FONT_HEIGHTS[font];
            let _ = hgt;
            log.lines.push("  copy 0".into());
            let (first, end, count) = list_line_range(sb.map(|s| s.0), shown, lines);
            let selection = list_selection(lines, has_selection, cur, first, end, &tr, lh);
            let mut p = log.control(&b);
            if let Some((_, thumb)) = sb {
                log.bitmap(100, thumb.top_left(), 1.0, 1.0);
                p = 0;
            }
            if let Some(s) = selection {
                p = log.rhombus(&s, &sel, selo);
                if tab && stop == 4 {
                    for (a, e) in list_focus_lines(&s) {
                        p = log.line(a, e, &foc, 1.0);
                    }
                }
            }
            log.lines.push(format!("  polygons {p}"));
            let mut res = 0;
            for i in 0..count {
                let row = string_list_row(&tr, i, lh);
                let text = &texts[(first + i) as usize];
                res = log.text(text, row.top_left(), &fg, row, &clipping);
            }
            log.lines.push(format!("  result {res}"));
        }
        "button" | "check" | "radio" => {
            let b = base(c);
            c.key("bitmap");
            let bitmap = if c.b() { Some((c.f(), c.f())) } else { None };
            c.key("at");
            let at = UiPoint { x: c.f(), y: c.f() };
            c.key("text");
            let has_text = c.b();
            let text = c.text();
            let tfg = c.color("tfg");
            let ctr = c.rect("ctr");
            let mut scale = false;
            let mut label = None;
            if kind != "button" {
                c.key("scale");
                scale = c.b();
                if c.at < c.t.len() && c.t[c.at] == "label" {
                    label = Some(small_label(c));
                }
            }
            let k = clip(c);
            log.dr = k.dr;
            if let Some(l) = &label {
                let _ = l.location();
                log.lines.push("  copy 0".into());
            }
            // DrawPolygons: the control, the (scaled) bitmap, the label's (no background, no border)
            let mut p = log.control(&b);
            if let Some((xs, ys)) = bitmap {
                let (xs, ys) = if scale {
                    box_bitmap_scale(xs, ys, b.rect.height(), (kind == "check").then_some(b.thickness))
                } else {
                    (xs, ys)
                };
                p = log.bitmap(7, at, xs, ys);
            }
            if label.is_some() {
                p = 0;
            }
            log.lines.push(format!("  polygons {p}"));
            // DrawTextA: the button's copied text (not the check box's), the label's
            let mut res = 0;
            if kind != "check" && has_text {
                res = log.text(&text, ctr.top_left(), &tfg, ctr, &k.clipping(true, (b.rect, b.rect)));
            }
            if let Some(l) = &label {
                res = log.text(&l.text, l.location(), &WHITE, l.rect, &k.clipping(false, (b.rect, b.rect)));
            }
            log.lines.push(format!("  result {res}"));
        }
        _ => unreachable!(),
    }
}

#[test]
fn golden_draws_replay() {
    let mut counts = std::collections::BTreeMap::new();
    for line in GOLDEN.lines() {
        // "case <n> <kind> <inputs...> = <lines> <digest>"
        let (head, tail) = line.split_once(" = ").expect(line);
        let t: Vec<&str> = head.split(' ').collect();
        let kind = t[2];
        let (want_lines, want_digest) = tail.split_once(' ').unwrap();
        *counts.entry(kind.to_string()).or_insert(0) += 1;
        if kind == "form" || kind == "edit" {
            continue; // C++ sequences (their clipped texts and borders go through the functions above)
        }
        let mut c = Cursor { t, at: 3 };
        let mut log = Log { lines: Vec::new(), dr: 0 };
        replay(kind, &mut c, &mut log);
        assert_eq!(c.at, c.t.len(), "header not consumed: {line}");
        assert_eq!(
            (log.lines.len().to_string(), format!("{:016x}", fnv(&log.lines))),
            (want_lines.to_string(), want_digest.to_string()),
            "{line}\n{}",
            log.lines.join("\n")
        );
    }
    for (kind, want) in
        [("border", 200), ("clip", 400), ("label1", 200), ("labelN", 200), ("list", 200), ("form", 100), ("edit", 400)]
    {
        assert_eq!(counts.get(kind), Some(&want), "{kind}");
    }
}

#[test]
fn border_lines_bevel() {
    let r = UiRect::new(10.0, 20.0, 50.0, 40.0);
    let top = border_line(&r, 0, 1, 0);
    assert_eq!((top.start, top.end, top.light), (UiPoint { x: 10.0, y: 21.0 }, UiPoint { x: 50.0, y: 21.0 }, false));
    let bottom = border_line(&r, 1, 0, 1);
    assert_eq!(
        (bottom.start, bottom.end, bottom.light),
        (UiPoint { x: 10.0, y: 40.0 }, UiPoint { x: 51.0, y: 40.0 }, false)
    );
    let right = border_line(&r, 3, 2, 0);
    assert_eq!(
        (right.start, right.end, right.light),
        (UiPoint { x: 48.0, y: 22.0 }, UiPoint { x: 48.0, y: 41.0 }, true)
    );
}

#[test]
fn clipped_text_pieces() {
    struct D(Vec<UiRect>, c_long);
    impl TextDraw for D {
        fn draw(&mut self, r: UiRect) -> c_long {
            self.0.push(r);
            self.1
        }
    }
    let r = UiRect::new(0.0, 0.0, 100.0, 10.0);
    // a clip covering the middle: the left and right pieces
    let mut d = D(Vec::new(), 0);
    draw_clipped_text(&mut d, r, &Clipping::Rectangles(vec![UiRect::new(40.0, -5.0, 60.0, 20.0)]));
    assert_eq!(d.0, vec![UiRect::new(0.0, 0.0, 40.0, 10.0), UiRect::new(60.0, 0.0, 100.0, 10.0)]);
    // covered whole: nothing drawn
    let mut d = D(Vec::new(), 0);
    draw_clipped_text(&mut d, r, &Clipping::Rectangles(vec![UiRect::new(-1.0, -1.0, 101.0, 11.0)]));
    assert!(d.0.is_empty());
    // a failing piece stops the pieces; a whole draw's result is returned
    let mut d = D(Vec::new(), 9);
    assert_eq!(draw_clipped_text(&mut d, r, &Clipping::Rectangles(vec![UiRect::new(40.0, -5.0, 60.0, 20.0)])), 9);
    assert_eq!(d.0.len(), 1);
    let mut d = D(Vec::new(), 9);
    assert_eq!(draw_clipped_text(&mut d, r, &Clipping::Plain { candidate: None }), 9);
    // the popup's own text is never clipped by it
    let mut d = D(Vec::new(), 0);
    let p = Clipping::Popup { is_control: true, rect: r, draw_rect: r };
    draw_clipped_text(&mut d, r, &p);
    assert_eq!(d.0, vec![r]);
}

#[test]
fn rows_wrap_as_unsigned_32_bit_products() {
    let ctr = UiRect::new(0.0, 5.0, 10.0, 100.0);
    let big: c_ulong = 0x1000_0000;
    assert_eq!(string_list_row(&ctr, 16, big).top, (16 as c_ulong).wrapping_mul(big) as f32 + 5.0);
    assert_eq!(string_list_row(&ctr, 1, 12).right, 11.0);
}
