//! WONGUI's TextArea as a whole Rust object (R6 WON round 21): the lobby's chat output, its MOTD and help pages.
//! The original's TextArea.cpp, TextSegments.cpp, TextAreaLines.cpp, TextAreaInstructions.cpp and the segments'
//! layout virtuals, with their state: the segment list, the laid-out lines and the wrapped pieces, the selection,
//! the links under the mouse, the text flags, the limits and the delayed wrap.
//!
//! The C++ `TextArea` stays the type the lobby holds (a `ScrollArea` in the component tree, its vtable, the fonts,
//! colours and selection colours its config and the chat read); its virtuals are one-line forwarders to
//! `ee_won_text_call`, and every call out is an op of `ee_won_guicpp_call` (`guihost`). The C++ segments (a
//! `TextSegment`: text, colour, font; a `TextLink` and the chat's `ChatLink` over it) stay data the lobby makes and
//! changes (a disabled chat link's colour): Rust holds a reference to each and asks it for its measures, its colour
//! and its link virtuals. The line-break and alignment segments, the lines and the pieces a wrap makes are Rust
//! data only.
//!
//! Every method is the original's statements in order with the calls out at the same points (`guitext`, `guilist`
//! and `guidraw` keep the decisions, as before). The state is behind a `RefCell` that is never borrowed across a
//! call out: the base class's size change calls the virtual AdjustPosition back, and a delivered link event runs
//! the chat's handler, which can add text or reset the area while MouseUp runs.
//!
//! One deliberate difference: when a trim removes a segment the hovered or pressed link is a piece of, the original
//! kept the piece (its own segment then freed); here the link is dropped with its segment.

use std::cell::RefCell;
use std::collections::VecDeque;
use std::ffi::c_void;

use crate::guidraw::{self, EeWonDrawOp, EeWonRowPlace, EeWonSegmentPaint, EeWonSelColor, EeWonTextMeasure};
use crate::guihost::{EeWonGuiCursor as Cursor, EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guilist;
use crate::guitext;

/// The text flags (`TextFlags`).
const WORD_WRAP: i32 = 0x01;
const ALLOW_SELECTION: i32 = 0x02;
const SCROLL_ON_BOTTOM: i32 = 0x04;
const SHOW_PARTIAL_LINES: i32 = 0x08;
const SCROLL_FULL_LINE: i32 = 0x10;

/// The inward calls (the C++ shell's virtuals and methods). `a`..`d` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTextCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `MouseEnter(a, b)`.
    MouseEnter,
    MouseExit,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    /// `MouseMove(a, b)`.
    MouseMove,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    /// `TimerEvent()` (0 or 1).
    TimerEvent,
    /// `KeyChar(a)` (0 or 1).
    KeyChar,
    /// `KeyDown(a)` (0 or 1).
    KeyDown,
    /// `NotifyRootVisibility(a != 0)`.
    NotifyRootVisibility,
    /// `AdjustPosition(a != 0)`.
    AdjustPosition,
    /// `ScrollPaint(*a)` (`a` the Graphics).
    ScrollPaint,
    /// `CheckSel(a, b, c != 0)`.
    CheckSel,
    /// `Reset(a != 0)`.
    Reset,
    /// `AddSegment(a)` (a C++ `TextSegment` or one derived from it).
    AddSegment,
    /// `LineBreak(a)`.
    LineBreak,
    /// `SetLineAlignment(a)` (`HorzAlign`).
    SetLineAlignment,
    /// `SetTextFlags(a, b != 0)`.
    SetTextFlags,
    /// `SetMaxChars(a)`.
    SetMaxChars,
    /// `SetMaxLines(a)`.
    SetMaxLines,
    /// `SetWrapIndentSize(a)`.
    SetWrapIndentSize,
    /// `GetNumLines()`.
    GetNumLines,
    /// `mTextFlags`.
    TextFlags,
}

/// What a segment is: the original's classes.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Kind {
    /// `TextSegmentBase`: a line break (`LineBreak`).
    Base,
    /// `TextAlignInstruction`.
    Align(i32),
    /// `TextSegment` (or a class over it: `TextLink`, the chat's `ChatLink`): the C++ object.
    Text,
}

/// A segment of the segment list (`TextSegmentBase`'s fields).
#[derive(Clone, Debug)]
struct Seg {
    id: u64,
    kind: Kind,
    /// the C++ object (0 unless `Text`)
    cpp: isize,
    abs_start: i32,
    abs_end: i32,
    width: i32,
    height: i32,
    ascent: i32,
    x: i32,
    y: i32,
    /// `mLine` (a line's id)
    line: Option<u64>,
    line_break: bool,
}

/// A wrapped part of a text segment (`TextSegmentPiece`).
#[derive(Clone, Copy, Debug)]
struct Piece {
    id: u64,
    seg: u64,
    cpp: isize,
    start: i32,
    end: i32,
    width: i32,
    height: i32,
    ascent: i32,
    x: i32,
    y: i32,
    line: u64,
}

/// A line's entry: a whole segment (its geometry is the segment's) or a piece.
#[derive(Clone, Copy, Debug)]
enum Item {
    Seg(u64),
    Piece(Piece),
}

/// `TextLine`.
#[derive(Clone, Debug, Default)]
struct Line {
    id: u64,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    baseline: i32,
    descent: i32,
    items: Vec<Item>,
}

/// A `TextSegmentBase*` the original kept (`mCurLink`, `mLastFindSeg`) or found: a segment or a piece, with the
/// segment it stands for (`GetActual`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct NodeRef {
    id: u64,
    seg: u64,
    kind: Kind,
    cpp: isize,
    piece: bool,
}

/// A node's geometry and range (read at once).
#[derive(Clone, Copy, Debug)]
struct NodeGeo {
    x: i32,
    y: i32,
    width: i32,
    line: Option<u64>,
    start: i32,
    end: i32,
    abs_start: i32,
    abs_end: i32,
}

#[derive(Debug)]
struct State {
    segs: VecDeque<Seg>,
    lines: VecDeque<Line>,
    next_id: u64,
    top_line: i32,
    max_chars: i32,
    max_lines: i32,
    num_lines: i32,
    need_recalc: bool,
    delay_accum: i32,
    cur_link: Option<NodeRef>,
    last_find: Option<NodeRef>,
    selecting: bool,
    backwards: bool,
    been_visible: bool,
    sel_start: i32,
    sel_end: i32,
    flags: i32,
    wrap_indent: i32,
    line_align: i32,
}

impl State {
    fn seg_index(&self, id: u64) -> Option<usize> {
        self.segs.binary_search_by_key(&id, |s| s.id).ok()
    }
    fn seg(&self, id: u64) -> Option<&Seg> {
        self.seg_index(id).map(|i| &self.segs[i])
    }
    fn seg_mut(&mut self, id: u64) -> Option<&mut Seg> {
        self.seg_index(id).map(move |i| &mut self.segs[i])
    }
    fn line_index(&self, id: u64) -> Option<usize> {
        self.lines.binary_search_by_key(&id, |l| l.id).ok()
    }
    fn line(&self, id: u64) -> Option<&Line> {
        self.line_index(id).map(|i| &self.lines[i])
    }
    fn flag(&self, f: i32) -> bool {
        self.flags & f != 0
    }
    fn new_id(&mut self) -> u64 {
        self.next_id += 1;
        self.next_id
    }
    fn node_of(&self, item: &Item) -> NodeRef {
        match item {
            Item::Seg(id) => {
                let s = self.seg(*id);
                NodeRef {
                    id: *id,
                    seg: *id,
                    kind: s.map_or(Kind::Base, |s| s.kind),
                    cpp: s.map_or(0, |s| s.cpp),
                    piece: false,
                }
            }
            Item::Piece(p) => NodeRef { id: p.id, seg: p.seg, kind: Kind::Text, cpp: p.cpp, piece: true },
        }
    }
    /// a node's geometry: a segment's own, or its piece's on the piece's line
    fn geo(&self, n: &NodeRef) -> Option<NodeGeo> {
        let s = self.seg(n.seg);
        if !n.piece {
            let s = s?;
            return Some(NodeGeo {
                x: s.x,
                y: s.y,
                width: s.width,
                line: s.line,
                start: 0,
                end: 0,
                abs_start: s.abs_start,
                abs_end: s.abs_end,
            });
        }
        for l in &self.lines {
            for it in &l.items {
                if let Item::Piece(p) = it {
                    if p.id == n.id {
                        return Some(NodeGeo {
                            x: p.x,
                            y: p.y,
                            width: p.width,
                            line: Some(p.line),
                            start: p.start,
                            end: p.end,
                            abs_start: s.map_or(0, |s| s.abs_start),
                            abs_end: s.map_or(0, |s| s.abs_end),
                        });
                    }
                }
            }
        }
        None
    }
}

/// The Rust TextArea.
pub struct EeWonText {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonText {
    /// The constructor's scalar state (the C++ constructor sets the fonts, the colours and the padding, then calls
    /// `Reset(false)`).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonText {
            comp,
            host,
            st: RefCell::new(State {
                segs: VecDeque::new(),
                lines: VecDeque::new(),
                next_id: 0,
                top_line: 0,
                max_chars: 80 * 2000,
                max_lines: 0,
                num_lines: 0,
                need_recalc: false,
                delay_accum: 0,
                cur_link: None,
                last_find: None,
                selecting: false,
                backwards: false,
                been_visible: false,
                sel_start: 0,
                sel_end: 0,
                flags: WORD_WRAP | ALLOW_SELECTION | SHOW_PARTIAL_LINES | SCROLL_ON_BOTTOM,
                wrap_indent: 0,
                line_align: 0,
            }),
        }
    }

    // ---- calls out

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn op0(&self, op: Op) -> isize {
        self.op(op, 0, 0, 0, 0)
    }
    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }
    fn get(&self, f: F) -> i32 {
        self.op(Op::Get, f as isize, 0, 0, 0) as i32
    }
    fn set(&self, f: F, v: i32) {
        self.op(Op::Set, f as isize, v as isize, 0, 0);
    }
    fn invalidate(&self) {
        self.op0(Op::Invalidate);
    }
    fn scroll_translate(&self, x: &mut i32, y: &mut i32) {
        self.op(Op::ScrollTranslate, x as *mut i32 as isize, y as *mut i32 as isize, 0, 0);
    }
    fn flag(&self, f: i32) -> bool {
        self.st.borrow().flag(f)
    }
    fn select_cursor(&self) -> Cursor {
        if self.flag(ALLOW_SELECTION) {
            Cursor::IBeam
        } else {
            Cursor::None
        }
    }

    // ---- a segment's virtuals

    fn seg_is_link(&self, n: &NodeRef) -> bool {
        n.kind == Kind::Text && self.op1(Op::SegIsLink, n.cpp) != 0
    }
    fn seg_pressed(&self, n: &NodeRef) {
        if n.kind == Kind::Text {
            self.op1(Op::SegPressed, n.cpp);
        }
    }
    fn seg_released(&self, n: &NodeRef) -> bool {
        n.kind == Kind::Text && self.op1(Op::SegReleased, n.cpp) != 0
    }

    /// `CalcDimensions` (only a text segment measures itself)
    fn calc_dimensions(&self, id: u64) {
        let (kind, cpp) = {
            let st = self.st.borrow();
            let Some(s) = st.seg(id) else { return };
            (s.kind, s.cpp)
        };
        if kind != Kind::Text {
            return;
        }
        let mut m = [0i32; 3];
        self.op(Op::SegMeasure, cpp, m.as_mut_ptr() as isize, 0, 0);
        if let Some(s) = self.st.borrow_mut().seg_mut(id) {
            s.width = m[0];
            s.height = m[1];
            s.ascent = m[2];
        }
    }

    /// `CharAt(x)` of a node on its line
    fn char_at(&self, n: &NodeRef, x: i32) -> i32 {
        let Some(g) = self.st.borrow().geo(n) else { return 0 };
        match (n.kind, n.piece) {
            (Kind::Text, true) => {
                let cpp = n.cpp;
                guitext::char_at(x.wrapping_sub(g.x), g.start, g.end, g.abs_start, |pos| {
                    self.op(Op::SegCharWidth, cpp, pos as isize, 0, 0) as i32
                })
            }
            (Kind::Text, false) => {
                let cpp = n.cpp;
                let len = self.op1(Op::SegLength, cpp) as i32;
                guitext::char_at(x.wrapping_sub(g.x), 0, len, g.abs_start, |pos| {
                    self.op(Op::SegCharWidth, cpp, pos as isize, 0, 0) as i32
                })
            }
            _ => guitext::base_char_at(x, g.x, g.width, g.abs_start, g.abs_end),
        }
    }

    /// `TextSegment::Paint(g, area, start, end, x, y, lineHeight)`
    #[allow(clippy::too_many_arguments)]
    fn paint_text(&self, g: isize, cpp: isize, abs_start: i32, start: i32, end: i32, x: i32, y: i32, height: i32) {
        let (ss, se) = {
            let st = self.st.borrow();
            (st.sel_start, st.sel_end)
        };
        let color = self.op1(Op::SegColor, cpp) as u32;
        let mut sel = EeWonSelColor::default();
        self.op1(Op::SelColor, &mut sel as *mut EeWonSelColor as isize);
        let s = EeWonSegmentPaint {
            area_sel_start: ss,
            area_sel_end: se,
            abs_start,
            start,
            end,
            x,
            y,
            line_height: height,
            color,
            sel,
        };
        let mut ctx = MeasureCtx { text: self, cpp };
        let m = EeWonTextMeasure {
            ctx: &mut ctx as *mut MeasureCtx as *mut c_void,
            string_width: Some(measure_string),
            char_width: Some(measure_char),
        };
        let ops = guidraw::segment(&s, &m);
        for o in &ops {
            self.op(Op::Draw, g, o as *const EeWonDrawOp as isize, cpp, 0);
        }
    }

    /// a line's entry's `Paint(g, area)`
    fn paint_node(&self, g: isize, n: &NodeRef) {
        if n.kind != Kind::Text {
            return;
        }
        let (geo, height) = {
            let st = self.st.borrow();
            let Some(geo) = st.geo(n) else { return };
            let h = geo.line.and_then(|l| st.line(l)).map_or(0, |l| l.height);
            (geo, h)
        };
        if n.piece {
            self.paint_text(g, n.cpp, geo.abs_start, geo.start, geo.end, geo.x, geo.y, height);
        } else {
            let len = self.op1(Op::SegLength, n.cpp) as i32;
            self.paint_text(g, n.cpp, geo.abs_start, 0, len, geo.x, geo.y, height);
        }
    }

    /// `GetSelText(area, text)` of a segment of the list
    fn get_sel_text(&self, id: u64) {
        let (kind, cpp, line_break, abs_start, abs_end, ss, se) = {
            let st = self.st.borrow();
            let Some(s) = st.seg(id) else { return };
            (s.kind, s.cpp, s.line_break, s.abs_start, s.abs_end, st.sel_start, st.sel_end)
        };
        if kind == Kind::Text {
            let Some((off, len)) = guitext::sel_part(ss, se, abs_start, abs_end) else { return };
            self.op(Op::SelTextAppend, cpp, off as isize, len as isize, 0);
            if guitext::sel_break(true, line_break, ss, se, abs_start, abs_end) {
                self.op0(Op::SelTextBreak);
            }
        } else if guitext::sel_break(false, line_break, ss, se, abs_start, abs_end) {
            self.op0(Op::SelTextBreak);
        }
    }

    // ---- TextArea

    /// `Reset(justDisplay)`
    pub fn reset(&self, just_display: bool) {
        let mut released = Vec::new();
        {
            let mut st = self.st.borrow_mut();
            st.lines.clear();
            if !just_display {
                released = st.segs.drain(..).filter(|s| s.cpp != 0).map(|s| s.cpp).collect();
            }
        }
        if !just_display {
            for cpp in released {
                self.op1(Op::SegRelease, cpp);
            }
            {
                let mut st = self.st.borrow_mut();
                st.selecting = false;
                st.sel_start = 0;
                st.sel_end = 0;
                st.backwards = false;
            }
            self.set(F::HorzOffset, 0);
            self.set(F::VertOffset, 0);
            self.set(F::TotalHeight, 0);
            self.set(F::TotalWidth, 0);
            self.adjust_position(false);
        }
        {
            let mut st = self.st.borrow_mut();
            st.num_lines = 0;
            st.top_line = 0;
        }
        self.set(F::TotalWidth, 0);
        self.set(F::TotalHeight, 0);
        let mut st = self.st.borrow_mut();
        st.cur_link = None;
        st.last_find = None;
        st.need_recalc = false;
        st.delay_accum = 0;
    }

    /// `RecalcWordWrap(wasDelayed)`
    fn recalc_word_wrap(&self, was_delayed: bool) {
        self.reset(true);
        let ids: Vec<u64> = self.st.borrow().segs.iter().map(|s| s.id).collect();
        for id in ids {
            self.add_segment_prv(id);
        }
        if self.should_scroll_on_bottom() {
            let h = self.get(F::TotalHeight);
            self.set(F::VertOffset, h);
        }
        if was_delayed {
            self.adjust_position(false);
        }
    }

    fn last_end(&self) -> Option<i32> {
        self.st.borrow().segs.back().map(|s| s.abs_end)
    }

    fn scroll_to_bottom(&self) {
        let th = self.get(F::TotalHeight);
        let eh = self.get(F::ExtraHeight);
        let vh = self.get(F::ScrollHeight);
        self.set(F::VertOffset, th.wrapping_add(eh).wrapping_sub(vh));
    }

    /// `Virtual08` (SizeChanged)
    pub fn size_changed(&self) {
        let old_width = self.get(F::ScrollWidth);
        let bottom = self.should_scroll_on_bottom();
        self.op0(Op::BaseSizeChanged);
        let (ww, flags, been) = {
            let st = self.st.borrow();
            (st.flag(WORD_WRAP), st.flags, st.been_visible)
        };
        if let Some(end) = self.last_end() {
            if ww && self.get(F::ScrollWidth) != old_width && been {
                if guitext::delay_wrap(flags, end) {
                    {
                        let mut st = self.st.borrow_mut();
                        st.delay_accum = 0;
                        st.need_recalc = true;
                    }
                    self.op1(Op::RequestTimer, 1);
                } else {
                    self.recalc_word_wrap(false);
                }
            }
        }
        if bottom {
            self.scroll_to_bottom();
        }
        self.adjust_position(false);
    }

    fn find_line_at(&self, y: i32) -> usize {
        let st = self.st.borrow();
        guilist::find_line_at(st.lines.len(), y, |i| st.lines[i].y)
    }

    /// `AdjustPosition(becauseOfScrollbar)`
    pub fn adjust_position(&self, because_of_scrollbar: bool) {
        self.set(F::ExtraHeight, 0);
        let full_line = self.flag(SCROLL_FULL_LINE);
        let th = self.get(F::TotalHeight);
        let vh = self.get(F::ScrollHeight);
        if full_line && th > vh {
            let i = self.find_line_at(th.wrapping_sub(vh));
            let place = self.st.borrow().lines.get(i).map(|l| (l.y, l.height));
            if let Some((y, h)) = place {
                self.set(F::ExtraHeight, guilist::full_line_extra(th, vh, y, h));
            }
        }
        self.op1(Op::BaseAdjustPosition, because_of_scrollbar as isize);
        self.find_top_line();
        if !self.flag(SCROLL_FULL_LINE) {
            return;
        }
        let (top, cur, prev) = {
            let st = self.st.borrow();
            let top = st.top_line;
            let cur = st.lines.get(top as u32 as usize).map(|l| (l.y, l.height));
            let prev = if top > 0 { st.lines.get(top as usize - 1).map(|l| l.height) } else { None };
            (top, cur, prev)
        };
        let Some((y, h)) = cur else { return };
        self.set(F::VertOffset, y);
        self.set(F::DownScrollAmount, h);
        if top > 0 {
            if let Some(ph) = prev {
                self.set(F::UpScrollAmount, ph);
            }
        }
        self.op1(Op::BaseAdjustPosition, 1);
    }

    /// `ShouldScrollOnBottom`
    fn should_scroll_on_bottom(&self) -> bool {
        let (flags, count, last, prev) = {
            let st = self.st.borrow();
            let n = st.lines.len();
            let mut last = guitext::EeWonTextLineY { y: 0, height: 0 };
            let mut prev = guitext::EeWonTextLineY { y: 0, height: 0 };
            if n > 0 {
                last = guitext::EeWonTextLineY { y: st.lines[n - 1].y, height: st.lines[n - 1].height };
            }
            if n > 1 {
                prev = guitext::EeWonTextLineY { y: st.lines[n - 2].y, height: st.lines[n - 2].height };
            }
            (st.flags, n, last, prev)
        };
        let user = self.get(F::UserHasScrolled) != 0;
        let off = self.get(F::VertOffset);
        let vh = self.get(F::ScrollHeight);
        guitext::should_scroll_on_bottom(flags, user, count, last, prev, off, vh)
    }

    /// `NotifyRootVisibility(visible)`
    pub fn notify_root_visibility(&self, visible: bool) {
        self.op1(Op::BaseNotifyRootVisibility, visible as isize);
        let rewrap = {
            let mut st = self.st.borrow_mut();
            if visible && !st.been_visible {
                st.been_visible = true;
                Some(st.flag(WORD_WRAP) && !st.segs.is_empty())
            } else {
                None
            }
        };
        if rewrap == Some(true) {
            self.recalc_word_wrap(true);
        }
    }

    /// `ScrollPaint(g)`
    pub fn scroll_paint(&self, g: isize) {
        let off = self.get(F::VertOffset);
        let vh = self.get(F::ScrollHeight);
        let ops = {
            let st = self.st.borrow();
            guidraw::text_lines(st.top_line as usize, st.lines.len(), off, vh, st.flag(SHOW_PARTIAL_LINES), &|i| {
                EeWonRowPlace { x: st.lines[i].x, top: st.lines[i].y, height: st.lines[i].height }
            })
        };
        for o in &ops {
            if o.kind == guidraw::EE_WON_DRAW_CALL {
                let nodes: Vec<NodeRef> = {
                    let st = self.st.borrow();
                    match st.lines.get(o.b as usize) {
                        Some(l) => l.items.iter().map(|it| st.node_of(it)).collect(),
                        None => Vec::new(),
                    }
                };
                for n in &nodes {
                    self.paint_node(g, n);
                }
            } else {
                self.op(Op::Draw, g, o as *const EeWonDrawOp as isize, 0, 0);
            }
        }
    }

    /// `FindTopLine`
    fn find_top_line(&self) {
        let off = self.get(F::VertOffset);
        let i = self.find_line_at(off);
        self.st.borrow_mut().top_line = i as i32;
    }

    /// `FindSegment(x, y, lenient)`
    fn find_segment(&self, x: i32, y: i32, lenient: bool) -> Option<NodeRef> {
        if self.st.borrow().lines.is_empty() {
            return None;
        }
        let (mut x, mut y) = (x, y);
        if lenient {
            let tw = self.get(F::TotalWidth);
            let th = self.get(F::TotalHeight);
            (x, y) = guitext::find_clamp(x, y, tw, th);
        }
        let i = self.find_line_at(y);
        let st = self.st.borrow();
        if let Some(lf) = st.last_find {
            if let Some(g) = st.geo(&lf) {
                if let Some(l) = g.line.and_then(|l| st.line(l)) {
                    if guitext::find_cached(x, y, l.x, l.y, l.height, g.x, g.width) {
                        return Some(lf);
                    }
                }
            }
        }
        let line = &st.lines[i];
        let mut xs = Vec::with_capacity(line.items.len());
        let mut ws = Vec::with_capacity(line.items.len());
        let mut nodes = Vec::with_capacity(line.items.len());
        for it in &line.items {
            let n = st.node_of(it);
            let g = st.geo(&n);
            xs.push(g.map_or(0, |g| g.x));
            ws.push(g.map_or(0, |g| g.width));
            nodes.push(n);
        }
        let pick = guitext::find_pick(x.wrapping_sub(line.x), y, line.y, line.height, &xs, &ws, lenient);
        drop(st);
        let found = pick?.map(|i| nodes[i]);
        self.st.borrow_mut().last_find = found;
        found
    }

    /// `CheckSel(x, y, newStart)`
    pub fn check_sel(&self, x: i32, y: i32, new_start: bool) {
        let Some(n) = self.find_segment(x, y, true) else { return };
        let line_x = {
            let st = self.st.borrow();
            st.geo(&n).and_then(|g| g.line).and_then(|l| st.line(l)).map_or(0, |l| l.x)
        };
        let at = self.char_at(&n, x.wrapping_sub(line_x));
        {
            let mut st = self.st.borrow_mut();
            let (s, e, b) = guitext::check_sel(st.sel_start, st.sel_end, st.backwards, at, new_start);
            st.sel_start = s;
            st.sel_end = e;
            st.backwards = b;
        }
        self.invalidate();
    }

    /// `TimerEvent()`
    pub fn timer_event(&self) -> bool {
        let need = self.st.borrow().need_recalc;
        if need {
            let ticks = {
                let mut st = self.st.borrow_mut();
                st.delay_accum = st.delay_accum.wrapping_add(1);
                st.delay_accum
            };
            let chars = self.last_end().unwrap_or(0);
            if guitext::delayed_wrap_due(ticks, chars) {
                self.recalc_word_wrap(true);
            }
        }
        let (selecting, need) = {
            let st = self.st.borrow();
            (st.selecting, st.need_recalc)
        };
        if !selecting {
            return need;
        }
        let (mut x, mut y) = (0i32, 0i32);
        self.op(Op::GetMousePos, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0, 0);
        let w = self.get(F::Width);
        let h = self.get(F::Height);
        let full = self.flag(SCROLL_FULL_LINE);
        let down = self.get(F::DownScrollAmount);
        let up = self.get(F::UpScrollAmount);
        let vert = self.get(F::VertOffset);
        let horz = self.get(F::HorzOffset);
        let Some((v, hz)) = guitext::drag_scroll(x, y, w, h, full, down, up, vert, horz) else { return true };
        self.set(F::VertOffset, v);
        self.set(F::HorzOffset, hz);
        self.adjust_position(false);
        self.scroll_translate(&mut x, &mut y);
        self.check_sel(x, y, false);
        true
    }

    /// `MouseDown(x, y, button)`
    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        self.op(Op::BaseMouseDown, x as isize, y as isize, button as isize, 0);
        if button != 0 {
            return;
        }
        let (mut x, mut y) = (x, y);
        self.scroll_translate(&mut x, &mut y);
        self.op0(Op::RequestFocus);
        let cur = self.st.borrow().cur_link;
        if let Some(cl) = cur {
            self.seg_pressed(&cl);
            self.invalidate();
            self.st.borrow_mut().selecting = false;
            return;
        }
        if !self.flag(ALLOW_SELECTION) {
            return;
        }
        self.st.borrow_mut().selecting = true;
        self.op1(Op::RequestTimer, 1);
        let new_start = self.op0(Op::KeyMod) & 1 == 0;
        self.check_sel(x, y, new_start);
    }

    /// `MouseExit()`
    pub fn mouse_exit(&self) {
        self.op0(Op::BaseMouseExit);
        let cur = self.st.borrow().cur_link;
        if let Some(cl) = cur {
            self.seg_released(&cl);
            self.st.borrow_mut().cur_link = None;
            self.invalidate();
        }
        self.st.borrow_mut().selecting = false;
    }

    /// `MouseMove(x, y)`
    pub fn mouse_move(&self, x: i32, y: i32) {
        self.op(Op::BaseMouseMove, x as isize, y as isize, 0, 0);
        let (mut x, mut y) = (x, y);
        self.scroll_translate(&mut x, &mut y);
        let seg = self.find_segment(x, y, false);
        if seg.map(|n| n.id) == self.st.borrow().cur_link.map(|n| n.id) {
            return;
        }
        if let Some(n) = seg.filter(|n| self.seg_is_link(n)) {
            self.st.borrow_mut().cur_link = Some(n);
            self.op1(Op::SetParentCursor, Cursor::Hand as isize);
        } else {
            self.st.borrow_mut().cur_link = None;
            self.op1(Op::SetParentCursor, self.select_cursor() as isize);
        }
    }

    /// `MouseEnter(x, y)`
    pub fn mouse_enter(&self, x: i32, y: i32) {
        self.op(Op::BaseMouseEnter, x as isize, y as isize, 0, 0);
        self.op1(Op::SetParentCursor, self.select_cursor() as isize);
        self.mouse_move(x, y);
    }

    /// `MouseDrag(x, y)`
    pub fn mouse_drag(&self, x: i32, y: i32) {
        self.op(Op::BaseMouseDrag, x as isize, y as isize, 0, 0);
        let (mut x, mut y) = (x, y);
        self.scroll_translate(&mut x, &mut y);
        if self.st.borrow().selecting {
            self.check_sel(x, y, false);
        }
    }

    /// `KeyDown(key)`
    pub fn key_down(&self, key: i32) -> bool {
        if self.op1(Op::BaseKeyDown, key as isize) != 0 {
            return true;
        }
        let vert = self.get(F::VertOffset);
        let horz = self.get(F::HorzOffset);
        let down = self.get(F::DownScrollAmount);
        let up = self.get(F::UpScrollAmount);
        let vh = self.get(F::ScrollHeight);
        let Some((v, h)) = guitext::key_scroll(key, vert, horz, down, up, vh) else { return false };
        self.set(F::VertOffset, v);
        self.set(F::HorzOffset, h);
        self.set(F::UserHasScrolled, 1);
        self.adjust_position(false);
        true
    }

    /// `CopySelection()`
    fn copy_selection(&self) {
        let ids: Vec<u64> = self.st.borrow().segs.iter().map(|s| s.id).collect();
        for id in ids {
            self.get_sel_text(id);
        }
        self.op0(Op::SelTextCopy);
    }

    /// `KeyChar(key)`
    pub fn key_char(&self, key: i32) -> bool {
        if self.op1(Op::BaseKeyChar, key as isize) != 0 {
            return true;
        }
        if key == 3 {
            self.copy_selection();
            return true;
        }
        false
    }

    /// `SetTextFlags(flags, on)`
    pub fn set_text_flags(&self, flags: i32, on: bool) {
        let (was, now) = {
            let mut st = self.st.borrow_mut();
            let was = st.flag(WORD_WRAP);
            if on {
                st.flags |= flags;
            } else {
                st.flags &= !flags;
            }
            (was, st.flag(WORD_WRAP))
        };
        if !was && now {
            self.size_changed();
        }
        {
            let mut st = self.st.borrow_mut();
            if st.flag(ALLOW_SELECTION) || st.sel_start == st.sel_end {
                return;
            }
            st.selecting = false;
            st.sel_start = 0;
            st.sel_end = 0;
        }
        self.invalidate();
    }

    /// `SetMaxChars(chars)`
    pub fn set_max_chars(&self, chars: i32) {
        self.st.borrow_mut().max_chars = chars;
        self.enforce_max_chars();
        self.adjust_position(false);
    }

    /// `SetMaxLines(lines)`
    pub fn set_max_lines(&self, lines: i32) {
        self.st.borrow_mut().max_lines = lines;
        self.enforce_max_lines();
        self.adjust_position(false);
    }

    /// `AddSeg(segment)`: a whole segment or a piece onto the last line
    fn add_seg(&self, item: Item) {
        let (h, asc) = {
            let st = self.st.borrow();
            match &item {
                Item::Seg(id) => st.seg(*id).map_or((0, 0), |s| (s.height, s.ascent)),
                Item::Piece(p) => (p.height, p.ascent),
            }
        };
        let th0 = self.get(F::TotalHeight);
        let mut th = th0;
        let align = {
            let mut guard = self.st.borrow_mut();
            let st = &mut *guard;
            let Some(line) = st.lines.back_mut() else { return };
            let mut m = guitext::EeWonTextLineM { height: line.height, baseline: line.baseline, descent: line.descent };
            let adjust = guitext::add_seg_metrics(&mut m, h, asc, &mut th);
            line.height = m.height;
            line.baseline = m.baseline;
            line.descent = m.descent;
            let baseline = line.baseline;
            let line_id = line.id;
            let line_width = line.width;
            if adjust {
                // the line's segments placed again on the new baseline
                let mut seg_ids = Vec::new();
                for it in line.items.iter_mut() {
                    match it {
                        Item::Seg(id) => seg_ids.push(*id),
                        Item::Piece(p) => p.y = baseline.wrapping_sub(p.ascent),
                    }
                }
                for id in seg_ids {
                    if let Some(s) = st.seg_mut(id) {
                        s.y = baseline.wrapping_sub(s.ascent);
                    }
                }
            }
            let item = match item {
                Item::Seg(id) => {
                    if let Some(s) = st.seg_mut(id) {
                        s.x = line_width;
                        s.y = baseline.wrapping_sub(s.ascent);
                        s.line = Some(line_id);
                    }
                    Item::Seg(id)
                }
                Item::Piece(mut p) => {
                    p.x = line_width;
                    p.y = baseline.wrapping_sub(p.ascent);
                    p.line = line_id;
                    Item::Piece(p)
                }
            };
            let w = match &item {
                Item::Seg(id) => st.seg(*id).map_or(0, |s| s.width),
                Item::Piece(p) => p.width,
            };
            let line = st.lines.back_mut().expect("the line is there");
            line.items.push(item);
            line.width = line.width.wrapping_add(w);
            st.line_align
        };
        if th != th0 {
            self.set(F::TotalHeight, th);
        }
        self.align_last_line(align);
    }

    /// `AlignLine(line, align)` of the last line
    fn align_last_line(&self, align: i32) {
        let vw = self.get(F::ScrollWidth);
        let mut st = self.st.borrow_mut();
        if let Some(line) = st.lines.back_mut() {
            line.x = guitext::align_x(vw, line.width, align);
        }
    }

    /// `AddLine(height, baseline)`
    fn add_line(&self, height: i32, baseline: i32) -> u64 {
        let id = {
            let mut st = self.st.borrow_mut();
            let id = st.new_id();
            let y = st.lines.back().map_or(0, |p| p.y.wrapping_add(p.height));
            st.lines.push_back(Line { id, y, height, baseline, ..Line::default() });
            id
        };
        let th = self.get(F::TotalHeight);
        self.set(F::TotalHeight, th.wrapping_add(height));
        id
    }

    /// `AddSegment(segment)`: a new segment after the last one, laid out, the limits enforced
    fn add_segment(&self, kind: Kind, cpp: isize, height: i32, line_break: bool) {
        let (line_break, length) = if kind == Kind::Text {
            (self.op1(Op::SegLineBreak, cpp) != 0, self.op1(Op::SegLength, cpp) as i32)
        } else {
            (line_break, 1)
        };
        let id = {
            let mut st = self.st.borrow_mut();
            let id = st.new_id();
            let start = st.segs.back().map_or(0, |s| s.abs_end);
            st.segs.push_back(Seg {
                id,
                kind,
                cpp,
                abs_start: start,
                abs_end: start.wrapping_add(length),
                width: 0,
                height,
                ascent: height,
                x: 0,
                y: 0,
                line: None,
                line_break,
            });
            id
        };
        // (ShouldScrollOnBottom reads only the lines: the segment's early place in the list changes nothing; the
        // list's reference is taken where the original's push_back took it)
        let bottom = self.should_scroll_on_bottom();
        if cpp != 0 {
            self.op1(Op::SegHold, cpp);
        }
        self.calc_dimensions(id);
        self.add_segment_prv(id);
        self.enforce_max_chars();
        self.enforce_max_lines();
        if bottom {
            self.scroll_to_bottom();
        }
        self.adjust_position(false);
    }

    /// `AddSegment(TextSegment*)` from the C++
    pub fn add_text_segment(&self, cpp: isize) {
        self.add_segment(Kind::Text, cpp, 0, false);
    }

    /// `LineBreak(height)`
    pub fn line_break(&self, height: i32) {
        self.add_segment(Kind::Base, 0, height, true);
    }

    /// `SetLineAlignment(alignment)`
    pub fn set_line_alignment(&self, align: i32) {
        if align == self.st.borrow().line_align {
            return;
        }
        self.add_segment(Kind::Align(align), 0, 0, false);
    }

    /// `AddSegmentPrv(segment)`
    fn add_segment_prv(&self, id: u64) {
        if self.st.borrow().lines.is_empty() {
            self.add_line(0, 0);
        }
        let (line_width, seg_width) = {
            let mut st = self.st.borrow_mut();
            let st = &mut *st;
            let line = st.lines.back().expect("a line");
            let (lid, lw) = (line.id, line.width);
            let Some(s) = st.seg_mut(id) else { return };
            s.line = Some(lid);
            let (kind, w) = (s.kind, s.width);
            // PerformInstructions
            if let Kind::Align(a) = kind {
                st.line_align = a;
            }
            (lw, w)
        };
        let width = self.get(F::Width);
        let vw = self.get(F::ScrollWidth);
        let (ww, been) = {
            let st = self.st.borrow();
            (st.flag(WORD_WRAP), st.been_visible)
        };
        if guitext::seg_fits(width, line_width, seg_width, vw, ww, been) {
            self.add_seg(Item::Seg(id));
            let lw = self.st.borrow().lines.back().map_or(0, |l| l.width);
            if lw > self.get(F::TotalWidth) && !self.flag(WORD_WRAP) {
                self.set(F::TotalWidth, lw);
            }
        } else {
            self.wrap(id);
        }
        let lb = self.st.borrow().seg(id).is_some_and(|s| s.line_break);
        if lb {
            {
                let mut st = self.st.borrow_mut();
                st.num_lines = st.num_lines.wrapping_add(1);
            }
            self.add_line(0, 0);
        }
    }

    /// `Wrap(area, line)` of the segment on the last line
    fn wrap(&self, id: u64) {
        let Some((kind, cpp, height, ascent)) = self.st.borrow().seg(id).map(|s| (s.kind, s.cpp, s.height, s.ascent))
        else {
            return;
        };
        if kind != Kind::Text {
            // TextSegmentBase::Wrap
            let empty = self.st.borrow().lines.back().is_none_or(|l| l.items.is_empty());
            if !empty {
                self.add_line(0, 0);
            }
            {
                let mut st = self.st.borrow_mut();
                let indent = st.wrap_indent;
                if let Some(l) = st.lines.back_mut() {
                    l.width = indent;
                }
            }
            self.add_seg(Item::Seg(id));
            return;
        }
        // TextSegment::Wrap
        if let Some(s) = self.st.borrow_mut().seg_mut(id) {
            s.line = None;
        }
        let len = self.op1(Op::SegLength, cpp) as i32;
        let mut widths = Vec::with_capacity(len.max(0) as usize);
        for i in 0..len {
            widths.push(self.op(Op::SegCharWidth, cpp, i as isize, 0, 0) as i32);
        }
        let mut text: Vec<u32> = Vec::new();
        self.op(Op::SegTextTake, cpp, &mut text as *mut Vec<u32> as isize, 0, 0);
        text.resize(len.max(0) as usize, 0);
        let vw = self.get(F::ScrollWidth);
        let (line_width, line_empty, align_left, indent) = {
            let st = self.st.borrow();
            let l = st.lines.back();
            (l.map_or(0, |l| l.width), l.is_none_or(|l| l.items.is_empty()), st.line_align == 0, st.wrap_indent)
        };
        let ops = guitext::wrap_segment(&text, &widths, vw, line_width, line_empty, align_left, indent, |start| {
            self.op(Op::SegStringWidth, cpp, start as isize, len.wrapping_sub(start) as isize, 0) as i32
        });
        let mut line_id = self.st.borrow().lines.back().map_or(0, |l| l.id);
        for o in &ops {
            if o.kind == 1 {
                line_id = self.add_line(height, ascent);
                if align_left {
                    if let Some(l) = self.st.borrow_mut().lines.back_mut() {
                        l.width = o.width;
                    }
                }
                continue;
            }
            let pid = self.st.borrow_mut().new_id();
            let piece = Piece {
                id: pid,
                seg: id,
                cpp,
                start: o.start,
                end: o.end,
                width: o.width,
                height,
                ascent,
                x: 0,
                y: 0,
                line: 0,
            };
            self.add_seg(Item::Piece(piece));
            if let Some(s) = self.st.borrow_mut().seg_mut(id) {
                if s.line.is_none() {
                    s.line = Some(line_id);
                }
            }
        }
    }

    /// `EnforceInvariants()`
    fn enforce_invariants(&self) {
        self.set(F::TotalWidth, 0);
        self.set(F::TotalHeight, 0);
        let empty = {
            let mut st = self.st.borrow_mut();
            st.num_lines = 0;
            st.lines.is_empty() || st.segs.is_empty()
        };
        if empty {
            return;
        }
        let mut off = self.get(F::VertOffset);
        let totals = {
            let mut st = self.st.borrow_mut();
            let st = &mut *st;
            let widths: Vec<i32> = st.lines.iter().map(|l| l.width).collect();
            let heights: Vec<i32> = st.lines.iter().map(|l| l.height).collect();
            let mut ys: Vec<i32> = st.lines.iter().map(|l| l.y).collect();
            let mut starts: Vec<i32> = st.segs.iter().map(|s| s.abs_start).collect();
            let mut ends: Vec<i32> = st.segs.iter().map(|s| s.abs_end).collect();
            let breaks: Vec<bool> = st.segs.iter().map(|s| s.line_break).collect();
            let (mut ss, mut se) = (st.sel_start, st.sel_end);
            let t = guitext::renumber(guitext::Renumber {
                line_widths: &widths,
                line_heights: &heights,
                line_ys: &mut ys,
                seg_starts: &mut starts,
                seg_ends: &mut ends,
                seg_breaks: &breaks,
                sel_start: &mut ss,
                sel_end: &mut se,
                offset: &mut off,
            });
            st.sel_start = ss;
            st.sel_end = se;
            for (l, y) in st.lines.iter_mut().zip(ys) {
                l.y = y;
            }
            for (s, (a, e)) in st.segs.iter_mut().zip(starts.into_iter().zip(ends)) {
                s.abs_start = a;
                s.abs_end = e;
            }
            st.num_lines = t.2;
            t
        };
        self.set(F::VertOffset, off);
        self.set(F::TotalWidth, totals.0);
        self.set(F::TotalHeight, totals.1);
    }

    /// `RemoveSegmentsUpTo(segment)`
    fn remove_segments_up_to(&self, id: u64) {
        let released: Vec<isize> = {
            let mut st = self.st.borrow_mut();
            let st = &mut *st;
            let find_line = st.seg(id).and_then(|s| s.line);
            st.last_find = None;
            let stop_line = guitext::remove_lines_stop(st.lines.len(), |i| {
                let l = &st.lines[i];
                let mut f = 0u32;
                if Some(l.id) == find_line {
                    f |= 1;
                }
                if let Some(last) = l.items.last() {
                    f |= 2;
                    let actual = match last {
                        Item::Seg(s) => *s,
                        Item::Piece(p) => p.seg,
                    };
                    if actual == id {
                        f |= 4;
                    }
                }
                f
            });
            if let Some(lid) = st.lines.get(stop_line).map(|l| l.id) {
                if let Some(s) = st.seg_mut(id) {
                    s.line = Some(lid);
                }
            }
            st.lines.drain(..stop_line);
            let stop = st.lines.front().and_then(|l| l.items.first()).map(|it| match it {
                Item::Seg(s) => *s,
                Item::Piece(p) => p.seg,
            });
            let mut count = 0usize;
            for s in &st.segs {
                if Some(s.id) == stop {
                    break;
                }
                if st.cur_link.is_some_and(|c| c.id == s.id) {
                    st.cur_link = None;
                }
                count += 1;
            }
            let removed: Vec<Seg> = st.segs.drain(..count).collect();
            // the one deliberate difference: a hovered piece of a removed segment goes with it
            if st.cur_link.is_some_and(|c| removed.iter().any(|s| s.id == c.seg)) {
                st.cur_link = None;
            }
            removed.into_iter().filter(|s| s.cpp != 0).map(|s| s.cpp).collect()
        };
        for cpp in released {
            self.op1(Op::SegRelease, cpp);
        }
        self.enforce_invariants();
        self.adjust_position(false);
    }

    /// `EnforceMaxChars()`
    fn enforce_max_chars(&self) {
        let cut = {
            let st = self.st.borrow();
            let Some(last) = st.segs.back() else { return };
            let Some(excess) = guitext::max_chars_excess(st.max_chars, last.abs_end) else { return };
            let ends: Vec<i32> = st.segs.iter().map(|s| s.abs_end).collect();
            st.segs[guitext::max_chars_cut(&ends, excess)].id
        };
        self.remove_segments_up_to(cut);
    }

    /// `EnforceMaxLines()`
    fn enforce_max_lines(&self) {
        let cut = {
            let st = self.st.borrow();
            if st.segs.is_empty() {
                return;
            }
            let Some(excess) = guitext::max_lines_excess(st.max_lines, st.num_lines) else { return };
            let breaks: Vec<bool> = st.segs.iter().map(|s| s.line_break).collect();
            st.segs[guitext::max_lines_cut(&breaks, excess)].id
        };
        self.remove_segments_up_to(cut);
    }

    /// `MouseUp(x, y, button)`
    pub fn mouse_up(&self, x: i32, y: i32, button: i32) {
        self.op(Op::BaseMouseUp, x as isize, y as isize, button as isize, 0);
        if button != 0 {
            return;
        }
        let (mut x, mut y) = (x, y);
        self.scroll_translate(&mut x, &mut y);
        let seg = self.find_segment(x, y, false);
        let mut pressed = false;
        let cur = self.st.borrow().cur_link;
        if let Some(cl) = cur {
            pressed = self.seg_released(&cl);
        }
        let cur = self.st.borrow().cur_link;
        if let Some(cl) = cur {
            if seg.map(|n| n.id) == Some(cl.id) && pressed {
                self.op1(Op::LinkActivated, cl.cpp);
            }
        }
        // the event's handler may have changed the area
        let cur = self.st.borrow().cur_link;
        if cur.is_some() {
            if let Some(n) = seg.filter(|n| self.seg_is_link(n)) {
                self.st.borrow_mut().cur_link = Some(n);
            } else {
                self.st.borrow_mut().cur_link = None;
                self.op1(Op::SetParentCursor, self.select_cursor() as isize);
            }
            self.invalidate();
        }
        self.st.borrow_mut().selecting = false;
    }

    /// `GetNumLines()`
    pub fn num_lines(&self) -> i32 {
        self.st.borrow().lines.len() as i32
    }

    /// One inward call.
    pub fn call(&self, call: EeWonTextCall, a: isize, b: isize, c: isize) -> isize {
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            EeWonTextCall::SizeChanged => self.size_changed(),
            EeWonTextCall::MouseEnter => self.mouse_enter(ai, bi),
            EeWonTextCall::MouseExit => self.mouse_exit(),
            EeWonTextCall::MouseDown => self.mouse_down(ai, bi, ci),
            EeWonTextCall::MouseUp => self.mouse_up(ai, bi, ci),
            EeWonTextCall::MouseMove => self.mouse_move(ai, bi),
            EeWonTextCall::MouseDrag => self.mouse_drag(ai, bi),
            EeWonTextCall::TimerEvent => return self.timer_event() as isize,
            EeWonTextCall::KeyChar => return self.key_char(ai) as isize,
            EeWonTextCall::KeyDown => return self.key_down(ai) as isize,
            EeWonTextCall::NotifyRootVisibility => self.notify_root_visibility(a != 0),
            EeWonTextCall::AdjustPosition => self.adjust_position(a != 0),
            EeWonTextCall::ScrollPaint => self.scroll_paint(a),
            EeWonTextCall::CheckSel => self.check_sel(ai, bi, c != 0),
            EeWonTextCall::Reset => self.reset(a != 0),
            EeWonTextCall::AddSegment => self.add_text_segment(a),
            EeWonTextCall::LineBreak => self.line_break(ai),
            EeWonTextCall::SetLineAlignment => self.set_line_alignment(ai),
            EeWonTextCall::SetTextFlags => self.set_text_flags(ai, b != 0),
            EeWonTextCall::SetMaxChars => self.set_max_chars(ai),
            EeWonTextCall::SetMaxLines => self.set_max_lines(ai),
            EeWonTextCall::SetWrapIndentSize => self.st.borrow_mut().wrap_indent = ai,
            EeWonTextCall::GetNumLines => return self.num_lines() as isize,
            EeWonTextCall::TextFlags => return self.st.borrow().flags as isize,
        }
        0
    }

    /// The segments' references go back (the C++ destructor's list clear).
    fn release_all(&self) {
        let released: Vec<isize> = self.st.borrow_mut().segs.drain(..).filter(|s| s.cpp != 0).map(|s| s.cpp).collect();
        for cpp in released {
            self.op1(Op::SegRelease, cpp);
        }
    }
}

impl Drop for EeWonText {
    fn drop(&mut self) {
        self.release_all();
    }
}

struct MeasureCtx<'a> {
    text: &'a EeWonText,
    cpp: isize,
}

unsafe extern "C" fn measure_string(ctx: *mut c_void, offset: i32, length: i32) -> i32 {
    // SAFETY: the ctx is the MeasureCtx of the paint running
    let m = unsafe { &*(ctx as *const MeasureCtx) };
    m.text.op(Op::SegStringWidth, m.cpp, offset as isize, length as isize, 0) as i32
}

unsafe extern "C" fn measure_char(ctx: *mut c_void, index: i32) -> i32 {
    // SAFETY: as above
    let m = unsafe { &*(ctx as *const MeasureCtx) };
    m.text.op(Op::SegCharWidth, m.cpp, index as isize, 0, 0) as i32
}

// ---- the C ABI

/// A new Rust TextArea for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_text_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonText {
    Box::into_raw(Box::new(EeWonText::new(component, host)))
}

/// # Safety
/// `text` came from `ee_won_text_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_text_free(text: *mut EeWonText) {
    if !text.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(text) });
    }
}

/// One of TextArea's methods (`EeWonTextCall`).
///
/// # Safety
/// `text` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_text_call(
    text: *const EeWonText,
    call: EeWonTextCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*text).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "textobj_tests.rs"]
mod tests;
