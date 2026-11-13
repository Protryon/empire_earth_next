//! The draw plans against `golden_draw.txt`: every case of the golden driver (build/won_draw_golden: the original
//! WONGUI units over a fake Graphics that logs every call) rebuilt here from its inputs through the plans and a
//! model of the C++ executor and the fakes, and compared line count and FNV-1a 64 with the original's log.

use super::*;
use std::cell::RefCell;

const GOLDEN: &str = include_str!("../golden_draw.txt");

fn fnv(lines: &[String]) -> u64 {
    let mut x: u64 = 0xcbf29ce484222325;
    for l in lines {
        for b in l.bytes().chain(std::iter::once(b'\n')) {
            x ^= b as u64;
            x = x.wrapping_mul(0x100000001b3);
        }
    }
    x
}

struct Tok<'a> {
    it: std::str::SplitWhitespace<'a>,
}

impl<'a> Tok<'a> {
    fn s(&mut self) -> &'a str {
        self.it.next().expect("token")
    }
    fn i(&mut self) -> i32 {
        let t = self.s();
        t.parse::<i64>().unwrap_or_else(|_| panic!("int {t}")) as i32
    }
    fn b(&mut self) -> bool {
        self.i() != 0
    }
    fn u(&mut self) -> u32 {
        self.s().parse::<u64>().expect("uint") as u32
    }
    fn text(&mut self) -> Vec<u16> {
        let t = self.s();
        if t == "-" {
            return Vec::new();
        }
        t.bytes()
            .map(|c| match c {
                b'_' => b' ',
                b'|' => b'\n',
                c => c,
            } as u16)
            .collect()
    }
    fn img(&mut self) -> Img {
        let has = self.b();
        let id = self.i();
        let w = self.i();
        let h = self.i();
        Img { has, id: if has { id } else { -1 }, w, h }
    }
    fn sel(&mut self) -> EeWonSelColor {
        EeWonSelColor { background: self.u(), foreground: self.u(), mode: self.i() }
    }
}

#[derive(Clone, Copy, Debug)]
struct Img {
    has: bool,
    id: i32,
    w: i32,
    h: i32,
}

#[derive(Clone, Debug)]
struct Wt {
    has_font: bool,
    sid: i32,
    len: i32,
    centered: bool,
    width: i32,
    fsize: i32,
    breaks: Vec<EeWonLineBreak>,
    text: Vec<u16>,
}

fn wt(t: &mut Tok) -> Option<Wt> {
    if !t.b() {
        return None;
    }
    let has_font = t.b();
    let sid = t.i();
    let len = t.i();
    let centered = t.b();
    let width = t.i();
    let _height = t.i();
    let fsize = t.i();
    let n = t.i();
    let mut breaks = Vec::new();
    for _ in 0..n {
        breaks.push(EeWonLineBreak { break_pos: t.i() as u16, next_start: t.i() as u16, width: t.i() as u16 });
    }
    let text = t.text();
    Some(Wt { has_font, sid, len, centered, width, fsize, breaks, text })
}

fn unit_width(size: i32, unit: i32) -> i32 {
    (unit % 7) + 2 + size / 4
}

struct MeasureCtx<'a> {
    size: i32,
    text: &'a [u16],
}

unsafe extern "C" fn m_string(ctx: *mut c_void, off: i32, len: i32) -> i32 {
    let m = &*(ctx as *const MeasureCtx);
    let (o, l) = (off as u16 as i32, len as u16 as i32);
    MEAS.with(|v| v.borrow_mut().push(format!("m measure {} {} {}", m.size, o, l)));
    let mut w = 0;
    let mut i = o;
    while i < o + l && i < m.text.len() as i32 {
        w += unit_width(m.size, m.text[i as usize] as i32);
        i += 1;
    }
    w
}

unsafe extern "C" fn m_char(ctx: *mut c_void, index: i32) -> i32 {
    let m = &*(ctx as *const MeasureCtx);
    let c = if index < 0 { 'M' as i32 } else { m.text[index as usize] as i32 };
    MEAS.with(|v| v.borrow_mut().push(format!("m cw {} {}", m.size, c)));
    unit_width(m.size, c)
}

fn measure(ctx: &MeasureCtx) -> EeWonTextMeasure {
    EeWonTextMeasure {
        ctx: ctx as *const MeasureCtx as *mut c_void,
        string_width: Some(m_string),
        char_width: Some(m_char),
    }
}

/// What a plan's slots name in the log.
#[derive(Default, Clone)]
struct Slots {
    images: [i32; 4],
    font: i32,
    string: i32,
    text: Option<Wt>,
}

/// The C++ executor (WONGUIDraw.h) over the fake Graphics: each operation's log line; a call goes to `call`.
fn run(log: &RefCell<Vec<String>>, ops: &[EeWonDrawOp], s: &Slots, call: &mut dyn FnMut(&EeWonDrawOp)) {
    for o in ops {
        let line = match o.kind {
            EE_WON_DRAW_SET_COLOR => format!("color {:08x}", o.a as u32),
            EE_WON_DRAW_SET_SYS_COLOR => format!("sys {}", o.a),
            EE_WON_DRAW_FILL_RECT => format!("fill {} {} {} {}", o.a, o.b, o.c, o.d),
            EE_WON_DRAW_RECT => format!("rect {} {} {} {}", o.a, o.b, o.c, o.d),
            EE_WON_DRAW_DOTTED_RECT => format!("dotted {} {} {} {}", o.a, o.b, o.c, o.d),
            EE_WON_DRAW_LINE => format!("line {} {} {} {}", o.a, o.b, o.c, o.d),
            EE_WON_DRAW_IMAGE => format!("image {} {} {}", s.images[o.a as usize], o.b, o.c),
            EE_WON_DRAW_IMAGE_SIZE => format!("size {} {} {}", s.images[o.a as usize], o.b, o.c),
            EE_WON_DRAW_PUSH_CLIP => format!("clip {} {} {} {}", o.a, o.b, o.c, o.d),
            EE_WON_DRAW_POP_CLIP => "pop".to_string(),
            EE_WON_DRAW_TRANSLATE => format!("tr {} {}", o.a, o.b),
            EE_WON_DRAW_SET_FONT => format!("font {}", s.font),
            EE_WON_DRAW_STRING => format!("str {} {} {} {} {}", s.string, o.b as u16, o.c as u16, o.d, o.e),
            EE_WON_DRAW_STRING_ALL => format!("strall {} {} {}", s.string, o.b, o.c),
            EE_WON_DRAW_TEXT => {
                paint_wt(log, s.text.as_ref().expect("text"), o.b, o.c);
                continue;
            }
            EE_WON_DRAW_CALL => {
                call(o);
                continue;
            }
            k => panic!("kind {k}"),
        };
        log.borrow_mut().push(line);
    }
}

fn run0(log: &RefCell<Vec<String>>, ops: &[EeWonDrawOp], s: &Slots) {
    run(log, ops, s, &mut |o| panic!("call {o:?}"));
}

/// `WrappedText::Paint` through its plan.
fn paint_wt(log: &RefCell<Vec<String>>, t: &Wt, x: i32, y: i32) {
    let ctx = MeasureCtx { size: t.fsize, text: &t.text };
    let ops = wrapped_text(t.has_font, t.len, t.centered, t.width, t.fsize, &t.breaks, x, y, &measure(&ctx));
    run0(log, &ops, &Slots { font: t.fsize, string: t.sid, ..Default::default() });
}

fn frame_of(i: Img, w: i32, h: i32) -> EeWonFrame {
    EeWonFrame { has_image: i.has, image_width: i.w, image_height: i.h, width: w, height: h, ..Default::default() }
}

fn img_slots(ids: &[i32]) -> Slots {
    let mut s = Slots::default();
    for (k, id) in ids.iter().enumerate() {
        s.images[k] = *id;
    }
    s
}

struct Bg {
    b: EeWonBackground,
    id: i32,
    comp: i32,
}

/// bgdesc: stretch, offsets, component id, colour, border, 0, image, 0
fn bg(t: &mut Tok) -> Bg {
    let stretch = t.b();
    let offsets = t.b();
    let comp = t.i();
    let color = t.i();
    let border = t.i();
    t.i();
    let i = t.img();
    t.i();
    Bg {
        b: EeWonBackground {
            has_image: i.has,
            stretch_image: stretch,
            use_offsets: offsets,
            has_component: comp != -1,
            image_width: i.w,
            image_height: i.h,
            color,
            border_color: border,
            ..Default::default()
        },
        id: i.id,
        comp,
    }
}

#[allow(clippy::too_many_arguments)]
fn paint_bg(log: &RefCell<Vec<String>>, g: &Bg, x: i32, y: i32, w: i32, h: i32, xo: i32, yo: i32) {
    let mut b = g.b;
    b.x = x;
    b.y = y;
    b.width = w;
    b.height = h;
    b.x_offset = xo;
    b.y_offset = yo;
    run(log, &background(&b), &img_slots(&[g.id]), &mut |o| {
        assert_eq!(o.a, 0);
        log.borrow_mut().push(format!("comp {} {} {}", g.comp, o.b, o.c));
    });
}

fn pixel_filter(f: &EeWonStretchFilter) -> String {
    let (w, h, ops) = stretch_filter(f);
    let (sw, sh) = (f.src_width, f.src_height);
    let src: Vec<u32> =
        (0..sh).flat_map(|y| (0..sw).map(move |x| 0x0100_0000 | ((y as u32) << 12) | x as u32)).collect();
    let mut dst = vec![0xEEEE_EEEEu32; (w * h).max(0) as usize];
    for o in &ops {
        match o.kind {
            EE_WON_DRAW_COPY_PIXELS => {
                for i in 0..o.g {
                    for k in 0..o.f {
                        let v = if o.a != 0 {
                            dst[((o.e + i) * w + o.d + k) as usize]
                        } else {
                            src[((o.e + i) * sw + o.d + k) as usize]
                        };
                        dst[((o.c + i) * w + o.b + k) as usize] = v;
                    }
                }
            }
            EE_WON_DRAW_STRETCH_PIXELS => {
                let (x1, x2, y1, y2, tl, tt, tw, th) = (o.a, o.b, o.c, o.d, o.e, o.f, o.g, o.h);
                let (nw, nh) = (x2 - x1, y2 - y1);
                if nw <= 0 || nh <= 0 {
                    continue;
                }
                let (mut cy, mut oy) = (0, tt);
                for y in 0..nh {
                    let mut sp = (oy * sw + tl) as isize;
                    let read = |p: isize| src.get(p as usize).copied().unwrap_or(0);
                    let mut cur = read(sp);
                    let mut cx = 0;
                    for x in 0..nw {
                        dst[((y + y1) * w + x1 + x) as usize] = cur;
                        cx += tw;
                        if cx >= nw {
                            sp += (cx / nw) as isize;
                            cx %= nw;
                            cur = read(sp);
                        }
                    }
                    cy += th;
                    if cy >= nh {
                        oy += cy / nh;
                        cy %= nh;
                    }
                }
            }
            k => panic!("kind {k}"),
        }
    }
    let mut x: u64 = 0xcbf29ce484222325;
    for p in &dst {
        for b in p.to_le_bytes() {
            x ^= b as u64;
            x = x.wrapping_mul(0x100000001b3);
        }
    }
    format!("result {w} {h} {x:016x}")
}

struct Button {
    p: EeWonButtonPaint,
    image: i32,
    text: Option<Wt>,
}

fn button_in(t: &mut Tok) -> Button {
    let mut p =
        EeWonButtonPaint { width: t.i(), height: t.i(), transparent: t.b(), scale_image: t.b(), ..Default::default() };
    let i = t.img();
    p.has_image = i.has;
    p.image_width = i.w;
    p.image_height = i.h;
    p.v_align = t.i();
    p.h_align = t.i();
    p.pressed = t.b();
    p.disabled = t.b();
    p.draw_gray_text = t.b();
    p.focus_shown = t.b();
    p.color = t.i();
    p.left_text_pad = t.i();
    p.top_text_pad = t.i();
    p.right_text_pad = t.i();
    p.bottom_text_pad = t.i();
    p.left_focus_pad = t.i();
    p.top_focus_pad = t.i();
    p.right_focus_pad = t.i();
    p.bottom_focus_pad = t.i();
    p.push_dx = t.i();
    p.push_dy = t.i();
    p.text_width = t.i();
    p.text_height = t.i();
    p.has_text = t.b();
    let text = wt(t);
    Button { p, image: i.id, text }
}

fn paint_button(log: &RefCell<Vec<String>>, b: &Button) {
    let s = Slots { images: [b.image, 0, 0, 0], text: b.text.clone(), ..Default::default() };
    run0(log, &button(&b.p), &s);
}

struct TextItem {
    sid: i32,
    color: u32,
    width: i32,
    height: i32,
    img: Img,
    fsize: i32,
}

fn text_item(t: &mut Tok) -> TextItem {
    let sid = t.i();
    let color = t.u();
    let width = t.i();
    let height = t.i();
    let img = t.img();
    let fsize = t.i();
    t.text();
    TextItem { sid, color, width, height, img, fsize }
}

fn paint_text_item(
    log: &RefCell<Vec<String>>,
    it: &TextItem,
    selected: bool,
    flags: i32,
    sel: EeWonSelColor,
    paint_width: i32,
) {
    let r = EeWonTextRowPaint {
        selected,
        full_row_select: flags & 0x10 != 0,
        no_image_select: flags & 0x20 != 0,
        has_image: it.img.has,
        sel,
        color: it.color,
        paint_width,
        width: it.width,
        height: it.height,
        image_width: it.img.w,
        image_height: it.img.h,
        font_height: it.fsize,
    };
    run0(
        log,
        &text_row(&r),
        &Slots { images: [it.img.id, 0, 0, 0], font: it.fsize, string: it.sid, ..Default::default() },
    );
}

fn replay(kind: &str, t: &mut Tok, log: &RefCell<Vec<String>>) {
    match kind {
        "filter" => {
            let f = EeWonStretchFilter {
                width: t.i(),
                height: t.i(),
                left: t.i(),
                right: t.i(),
                top: t.i(),
                bottom: t.i(),
                src_width: t.i(),
                src_height: t.i(),
                tile_horz: t.b(),
                tile_vert: t.b(),
                skip_center: t.b(),
            };
            log.borrow_mut().push(pixel_filter(&f));
        }
        "spaint" => {
            let (sk, l, r, tp, b, w, h, id) = (t.b(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i());
            run0(log, &stretch_paint(sk, l, r, tp, b, w, h), &img_slots(&[id]));
        }
        "scenter" => {
            let (w, h) = (t.i(), t.i());
            let (a, c) = (t.img(), t.img());
            run0(log, &stretch_center_paint(w, h, c.w, c.h), &img_slots(&[a.id, c.id]));
        }
        "frame" => {
            let _v = t.i();
            let i = t.img();
            let (cw, ch) = (t.i(), t.i());
            let mut f = frame_of(i, cw, ch);
            f.left = t.i();
            f.top = t.i();
            f.right = t.i();
            f.bottom = t.i();
            run0(log, &frame_paint(&f), &img_slots(&[i.id]));
        }
        "combo" => {
            let (d, inf, bv, bw) = (t.b(), t.b(), t.b(), t.i());
            let i = t.img();
            let (cw, ch) = (t.i(), t.i());
            run0(log, &combo_frame(d, inf, bv, bw, &frame_of(i, cw, ch)), &img_slots(&[i.id]));
        }
        "scont" => {
            let (vv, hv, tr) = (t.b(), t.b(), t.b());
            let corner = t.img();
            let (vl, vb) = (t.i(), t.i());
            let i = t.img();
            let (cw, ch) = (t.i(), t.i());
            run0(
                log,
                &scroll_container(vv, hv, tr, corner.has, vl, vb, &frame_of(i, cw, ch)),
                &img_slots(&[i.id, corner.id]),
            );
        }
        "bg" => {
            let g = bg(t);
            let (x, y, w, h, xo, yo) = (t.i(), t.i(), t.i(), t.i(), t.i(), t.i());
            paint_bg(log, &g, x, y, w, h, xo, yo);
        }
        "button" => {
            let b = button_in(t);
            paint_button(log, &b);
        }
        "header" => {
            let sort = t.i();
            let b = button_in(t);
            paint_button(log, &b);
            let ops = header_arrow(
                sort,
                b.p.color,
                b.text.is_some(),
                b.p.text_width,
                b.p.left_text_pad,
                b.p.width,
                b.p.height,
                b.p.pressed,
            );
            run0(log, &ops, &Slots::default());
        }
        "simple" => {
            let mut b = EeWonSimpleButtonPaint {
                disabled: t.b(),
                check_mouse_down: t.b(),
                mouse_down_state: t.i() as u8,
                pressed: t.b(),
                mouse_over: t.b(),
                scale_image: t.b(),
                width: t.i(),
                height: t.i(),
                ..Default::default()
            };
            let mut ids = [0; 4];
            for (k, id) in ids.iter_mut().enumerate() {
                let i = t.img();
                b.has_image[k] = i.has;
                b.image_width[k] = i.w;
                b.image_height[k] = i.h;
                *id = i.id;
            }
            run0(log, &simple_button(&b), &img_slots(&ids));
        }
        "label" => {
            let (w, h) = (t.i(), t.i());
            let pads = [t.i(), t.i(), t.i(), t.i()];
            let (ha, va, tw, th, color) = (t.i(), t.i(), t.i(), t.i(), t.i());
            let g = bg(t);
            let text = wt(t);
            paint_bg(log, &g, 0, 0, w, h, 0, 0);
            run0(log, &label(w, h, pads, ha, va, tw, th, color), &Slots { text, ..Default::default() });
        }
        "wrapped" => {
            let (x, y) = (t.i(), t.i());
            let w = wt(t).expect("text");
            paint_wt(log, &w, x, y);
        }
        "list" => {
            let (tw, vw, pw, first, count, voff, vh, partial) =
                (t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.b());
            let rows: Vec<(i32, i32, bool)> = (0..count).map(|_| (t.i(), t.i(), t.b())).collect();
            let (pw, ops) = list_paint(tw, vw, pw, first as usize, count as usize, voff, vh, partial, &|i| {
                EeWonRowPlace { x: 0, top: rows[i].0, height: rows[i].1 }
            });
            run(log, &ops, &Slots::default(), &mut |o| {
                let r = rows[o.b as usize];
                log.borrow_mut().push(format!("item {} {} {} {}", o.b, pw, r.1, r.2 as i32));
            });
            log.borrow_mut().push("pw -1".to_string());
        }
        "multi" => {
            let (cols, border, total, voff, vw, vh, tw, flags, first) =
                (t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i());
            let sel = t.sel();
            let rows = t.i();
            t.i();
            let columns: Vec<(i32, u8)> = (0..cols).map(|_| (t.i(), t.i() as u8)).collect();
            struct Row {
                top: i32,
                height: i32,
                sel: bool,
                cells: Vec<Option<TextItem>>,
            }
            let mut rs = Vec::new();
            for _ in 0..rows {
                assert_eq!(t.s(), "r");
                let (top, height, sel, n) = (t.i(), t.i(), t.b(), t.i());
                let mut cells = Vec::new();
                for _ in 0..n {
                    cells.push(if t.s() == "c1" { Some(text_item(t)) } else { None });
                }
                rs.push(Row { top, height, sel, cells });
            }
            let widths: Vec<i32> = columns.iter().map(|c| c.0).collect();
            let ops = multi_list(&widths, border, total, voff, vh);
            let mut column = -1i32;
            let mut paint_width = -1i32;
            run(log, &ops, &Slots::default(), &mut |o| {
                if o.a == 0 {
                    column = o.b;
                    paint_width = o.c;
                    return;
                }
                let (pw, list_ops) =
                    list_paint(tw, vw, paint_width, first as usize, rs.len(), voff, vh, flags & 0x80 != 0, &|i| {
                        EeWonRowPlace { x: 0, top: rs[i].top, height: rs[i].height }
                    });
                run(log, &list_ops, &Slots::default(), &mut |o| {
                    let row = &rs[o.b as usize];
                    let cell = if column >= 0 && (column as usize) < row.cells.len() {
                        row.cells[column as usize].as_ref()
                    } else {
                        None
                    };
                    let mr = EeWonMultiRowPaint {
                        column,
                        selected: row.sel,
                        full_row_select: flags & 0x10 != 0,
                        has_cell: cell.is_some(),
                        cell_count: row.cells.len() as u32,
                        selection_color: sel.background,
                        total_width: total,
                        height: row.height,
                        align: if column >= 0 && (column as usize) < columns.len() {
                            columns[column as usize].1
                        } else {
                            0
                        },
                        paint_width: pw,
                        cell_width: cell.map(|c| c.width).unwrap_or(0),
                    };
                    let mut cell_sel = false;
                    run(log, &multi_row(&mr), &Slots::default(), &mut |o| {
                        if o.a == 0 {
                            cell_sel = o.c != 0;
                            return;
                        }
                        let c = cell.expect("cell");
                        let it = TextItem { height: row.height, img: c.img, ..*c };
                        paint_text_item(log, &it, cell_sel, flags, sel, pw);
                    });
                });
                paint_width = -1;
            });
        }
        "textrow" => {
            let (pw, flags, selected) = (t.i(), t.i(), t.b());
            let sel = t.sel();
            let it = text_item(t);
            paint_text_item(log, &it, selected, flags, sel, pw);
        }
        "input" => {
            let mut b = EeWonInputPaint {
                width: t.i(),
                height: t.i(),
                disabled_look: t.b(),
                has_focus: t.b(),
                cursor_on: t.b(),
                underline_cursor: t.b(),
                backwards_sel: t.b(),
                horz_offset: t.i(),
                ..Default::default()
            };
            let icon = t.img();
            b.has_icon = icon.has;
            b.icon_height = icon.h;
            b.text_color = t.i();
            b.disabled_text_color = t.i();
            b.text_x = t.i();
            b.text_y = t.i();
            b.text_width = t.i();
            b.text_height = t.i();
            b.sel_start = t.i();
            b.sel_end = t.i();
            b.length = t.i();
            b.font_height = t.i();
            let sid = t.i();
            t.i();
            t.i();
            b.sel = t.sel();
            let bgs = [bg(t), bg(t)];
            let fi = t.img();
            let text = t.text();
            let ctx = MeasureCtx { size: b.font_height, text: &text };
            let ops = input_box(&b, &measure(&ctx));
            run(
                log,
                &ops,
                &Slots { images: [icon.id, 0, 0, 0], font: b.font_height, string: sid, ..Default::default() },
                &mut |o| {
                    paint_bg(log, &bgs[o.b as usize], 0, 0, o.c, o.d, o.e, 0);
                },
            );
            run0(log, &frame_paint(&frame_of(fi, b.width, b.height)), &img_slots(&[fi.id]));
        }
        "tlines" => {
            let (first, count, voff, vh, partial) = (t.i(), t.i(), t.i(), t.i(), t.b());
            let lines: Vec<(i32, i32, i32, i32)> = (0..count).map(|_| (t.i(), t.i(), t.i(), t.i())).collect();
            let firsts: Vec<i32> = lines
                .iter()
                .scan(0, |a, l| {
                    let f = *a;
                    *a += l.3;
                    Some(f)
                })
                .collect();
            let ops = text_lines(first as usize, count as usize, voff, vh, partial, &|i| EeWonRowPlace {
                x: lines[i].0,
                top: lines[i].1,
                height: lines[i].2,
            });
            run(log, &ops, &Slots::default(), &mut |o| {
                let i = o.b as usize;
                for k in 0..lines[i].3 {
                    log.borrow_mut().push(format!("seg {}", firsts[i] + k));
                }
            });
        }
        "segment" => {
            let _v = t.i();
            let (ss, se, abs, start, end, x, y, lh) = (t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i());
            let color = t.u();
            let fsize = t.i();
            let sid = t.i();
            t.i();
            let sel = t.sel();
            let _len = t.i();
            let text = t.text();
            let s = EeWonSegmentPaint {
                area_sel_start: ss,
                area_sel_end: se,
                abs_start: abs,
                start,
                end,
                x,
                y,
                line_height: lh,
                color,
                sel,
            };
            let ctx = MeasureCtx { size: fsize, text: &text };
            let ops = segment(&s, &measure(&ctx));
            run0(log, &ops, &Slots { font: fsize, string: sid, ..Default::default() });
        }
        "timage" => {
            let (ss, se, a, e, x, y, w, lh) = (t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i(), t.i());
            let sel = t.sel();
            let i = t.img();
            run0(log, &text_image(ss, se, a, e, x, y, w, lh, sel.background), &img_slots(&[i.id]));
        }
        k => panic!("kind {k}"),
    }
}

thread_local! {
    /// the fake font's measures, which the driver writes after the case's draws
    static MEAS: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

#[test]
fn golden_replays() {
    let mut cases = 0;
    let mut by_kind = std::collections::BTreeMap::new();
    for line in GOLDEN.lines() {
        let (head, tail) = line.split_once(" = ").expect("case line");
        let mut tt = tail.split_whitespace();
        let count: usize = tt.next().unwrap().parse().unwrap();
        let digest = u64::from_str_radix(tt.next().unwrap(), 16).unwrap();
        let mut t = Tok { it: head.split_whitespace() };
        assert_eq!(t.s(), "case");
        let n = t.i();
        let kind = t.s();
        let log = RefCell::new(Vec::new());
        replay(kind, &mut t, &log);
        let mut got = log.into_inner();
        got.extend(MEAS.with(|v| std::mem::take(&mut *v.borrow_mut())));
        assert_eq!((got.len(), fnv(&got)), (count, digest), "case {n} {kind}: {got:#?}");
        cases += 1;
        *by_kind.entry(kind.to_string()).or_insert(0) += 1;
    }
    assert!(cases > 4000, "{cases} cases");
    assert_eq!(by_kind.len(), 19, "{by_kind:?}");
}

#[test]
fn sel_foreground_modes() {
    let s = |mode| EeWonSelColor { background: 1, foreground: 0x123456, mode };
    assert_eq!(sel_foreground(&s(0), 0xabcdef), 0xabcdef);
    assert_eq!(sel_foreground(&s(1), 0xabcdef), 0xabcdef ^ 0xffffff);
    assert_eq!(sel_foreground(&s(2), 0xabcdef), 0x123456);
    assert_eq!(sel_foreground(&s(3), 0xabcdef), 0xffffff);
}

#[test]
fn deliver_reports_the_whole_length() {
    let ops = stretch_paint(true, 1, 1, 1, 1, 10, 10);
    assert_eq!(ops.len(), 12);
    let mut buf = [EeWonDrawOp::default(); 4];
    let n = unsafe { ee_won_gui_draw_stretch_paint(true, 1, 1, 1, 1, 10, 10, buf.as_mut_ptr(), 4) };
    assert_eq!(n, 12);
    assert_eq!(buf[0], ops[0]);
    assert_eq!(buf[3], ops[3]);
}

/// TextListItem's and MenuItem's measure and MenuItem's paint against `golden_itemobj.txt` (printed by
/// `tests/won/won_itemobj_test.cpp --print` over the original statements and a recording Graphics).
#[test]
fn item_measures_and_menu_item_paint_are_the_originals() {
    let golden = include_str!("../golden_itemobj.txt");
    let mut lines = golden.lines().peekable();
    let mut cases = 0;
    while let Some(line) = lines.next() {
        let c: Vec<i64> = line.split_whitespace().skip(1).map(|w| w.parse().unwrap_or(0)).collect();
        assert!(line.starts_with("case ") && c.len() >= 20, "{line}");
        let (list_tw, list_h, own_tw, own_h, img_w, img_h) = (c[0], c[1], c[2], c[3], c[4], c[5]);
        let own_font = c[15] != 0;
        let has_image = c[16] != 0;
        let menu = c[19] != 0;
        let (tw, fh) = if own_font { (own_tw, own_h) } else { (list_tw, list_h) };
        let size: Vec<i32> =
            lines.next().unwrap_or("").split_whitespace().skip(1).map(|w| w.parse().unwrap_or(0)).collect();
        let got = text_item_size(tw as i32, fh as i32, has_image, img_w as i32, img_h as i32, menu);
        assert_eq!((got.0, got.1), (size[0], size[1]), "case {cases} size");
        let wh: Vec<i32> =
            lines.next().unwrap_or("").split_whitespace().skip(1).map(|w| w.parse().unwrap_or(0)).collect();
        let color = if c[12] == -1 { c[13] as u32 } else { c[12] as u32 };
        let r = EeWonMenuItemPaint {
            selected: c[14] != 0,
            full_row_select: c[6] & 0x10 != 0,
            has_image,
            disabled: c[18] != 0,
            has_submenu: c[17] != 0,
            sel: EeWonSelColor { background: c[10] as u32, foreground: c[11] as u32, mode: c[9] as i32 },
            color,
            paint_width: c[7] as i32,
            width: wh[0],
            height: wh[1],
            image_height: if has_image { img_h as i32 } else { 0 },
            font_height: fh as i32,
            view_width: c[8] as i32,
        };
        let mut want = Vec::new();
        while let Some(l) = lines.peek() {
            if !l.starts_with("op ") {
                break;
            }
            want.push(lines.next().unwrap_or("").to_string());
        }
        let ops: Vec<String> =
            menu_item(&r).iter().map(|o| format!("op {} {} {} {} {}", o.kind, o.a, o.b, o.c, o.d)).collect();
        assert_eq!(ops, want, "case {cases} paint");
        cases += 1;
    }
    assert_eq!(cases, 400);
}
