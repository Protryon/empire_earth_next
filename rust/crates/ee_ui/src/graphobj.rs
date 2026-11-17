//! `UIGraph`, `UILineGraph` and the post-game summary's `EEUIPGSLineGraph` as one whole Rust object (R6 game
//! screens B, round 3).
//!
//! The three C++ classes were a control that draws a `UGraph` (a grid, its values, a title and axis labels, zoomed
//! by a scale), the line graph that draws each enabled series as segments, and the summary's line graph with 31
//! stored graphs and its own value formats. They are one class now (`lle/UIGraph.h`): a C++ shell whose methods
//! forward to [`ee_ui_graph_call`] and whose host ([`EeUiOp`]s: the base control's, the texts', a line's) is the
//! only way out. This object holds every member: the texts (C++ `GEText`s by address), the rectangles, the zoom, the
//! flags, the shown graph and its copy for drawing, and for the summary the stored graphs (its 31, then the 8 its
//! form manager kept: `EEPGSFormManager`'s graphs live here now). A graph is a [`UiGraph`]: `ee_util`'s series and
//! range and the three strings `UGraph` kept in C++.
//!
//! The arithmetic (the layout, the zoom, the grid, the clipped series) is `graph.rs`'s, pinned by its golden; the
//! order of the calls out is the C++'s, pinned by `golden_graphobj.txt` (`graphobj_tests.rs`).
//!
//! The screens' Rust reaches the summary's graphs through [`EeUiGraph::graph_at`] (`direct::graph`), where the C++
//! host's cases used to.

use core::cell::RefCell;
use core::ffi::{c_int, c_long, c_ulong, c_void};

use ee_base::util::graph::{EeUtilColor, EeUtilGraph, EeUtilPoint, EeUtilSeries};

use crate::graph::{self, GraphAxis, GraphHost, GraphRange, GraphText, GraphTexts, SeriesHost, SeriesListHost};
use crate::obj::{read_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{UiPoint, UiRect};

/// The inward calls (the C++ shell's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiGraphCall {
    /// `CopyData()` -> the base's result.
    CopyData = 1,
    /// `DrawPolygons()`: the frame, the grid, then the series.
    DrawPolygons = 2,
    /// `DrawTextA()`.
    DrawText = 3,
    /// `SetFont(font a)`.
    SetFont = 4,
    /// `SetFontColor(f[0..3])`.
    SetFontColor = 5,
    /// `TabbedTo()`.
    TabbedTo = 6,
    /// `TabbedFrom()`.
    TabbedFrom = 7,
    /// `ZoomIn((f[0], f[1]))`.
    ZoomIn = 8,
    /// `ZoomOut()`.
    ZoomOut = 9,
    /// `SetLabelXAxis(a != 0)`.
    SetLabelXAxis = 10,
    /// `SetLabelYAxis(a != 0)`.
    SetLabelYAxis = 11,
    /// The summary's `AddGraphSeries`: a series of the `c` (time, value) pairs of `long`s at `b`, in the colour
    /// `f[0..3]`, added to the stored graph `a`.
    AddGraphSeries = 12,
    /// The summary's `ShowGraph(a)` ([`EeUiGraph::show_graph`]).
    ShowGraph = 13,
}

/// What the graph is: which values it formats and whether it stores graphs.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiGraphKind {
    /// `UILineGraph`: values as `"%.2f"`, no stored graphs.
    Line = 0,
    /// `EEUIPGSLineGraph`: values through the format the game gave, the summary's 31 + 8 stored graphs.
    PostGame = 1,
}

/// A graph's value format (the summary's: `axis` 0 x, 1 y): the text of `value` into `out` (`capacity` bytes);
/// its length.
pub type EeUiGraphFormatFn =
    Option<unsafe extern "C" fn(axis: c_int, value: f32, out: *mut u8, capacity: usize) -> usize>;

/// A `UGraph`: the series and range (`ee_util`'s) and the title and axis labels (UTF-8, no NUL).
#[derive(Clone, Debug, Default)]
pub struct UiGraph {
    pub data: EeUtilGraph,
    pub title: Vec<u8>,
    pub x_label: Vec<u8>,
    pub y_label: Vec<u8>,
}

/// The summary's stored graphs: its line graph's 31, then its form manager's 8.
const STORED_GRAPHS: usize = 31;
const MANAGER_GRAPHS: usize = 8;
/// The index of the shown graph in [`EeUiGraph::graph_at`] (the summary's numbering: 0..30 stored, 31 shown,
/// 32..39 the form manager's).
pub const EE_UI_GRAPH_SHOWN: u32 = 31;

/// The texts, by slot.
const TITLE: usize = 0;
const COPIED_TITLE: usize = 1;
const X_AXIS: usize = 2;
const COPIED_X_AXIS: usize = 3;
const Y_AXIS: usize = 4;
const COPIED_Y_AXIS: usize = 5;
const GRID: usize = 6;

#[derive(Clone, Copy, Debug)]
struct State {
    /// The `GEText*`s by slot (0 until `SetFont`).
    texts: [isize; 7],
    zoom: f32,
    copied_zoom: f32,
    divisions: (c_ulong, c_ulong),
    /// The displayed range (the graph's over the zoom scale).
    displayed: GraphRange,
    zoom_location: UiPoint,
    /// The title's foreground colour as of `CopyData`.
    grid_color: [f32; 3],
    graph_rect: UiRect,
    copied_graph_rect: UiRect,
    title_rect: UiRect,
    x_rect: UiRect,
    y_rect: UiRect,
    line_opacity: f32,
    copied_line_opacity: f32,
    zoom_increment: f32,
    /// graph, x, y, grid: drawn when on (and their copies)
    draw_grid: bool,
    copied_draw_grid: bool,
    label_grid: bool,
    copied_label_grid: bool,
    label_x: bool,
    copied_label_x: bool,
    label_y: bool,
    copied_label_y: bool,
    label_graph: bool,
    copied_label_graph: bool,
}

/// The Rust graph.
pub struct EeUiGraph {
    host: Host,
    kind: EeUiGraphKind,
    format: EeUiGraphFormatFn,
    s: RefCell<State>,
    graph: RefCell<UiGraph>,
    copied: RefCell<UiGraph>,
    stored: RefCell<Vec<UiGraph>>,
}

/// `kDefaultLineOpacity`, `kDefaultZoomIncrement`.
const DEFAULT_LINE_OPACITY: f32 = 0.25;
const DEFAULT_ZOOM_INCREMENT: f32 = 1.0;

fn range(g: &EeUtilGraph) -> GraphRange {
    let r = g.range();
    GraphRange { min_x: r.min_x, max_x: r.max_x, min_y: r.min_y, max_y: r.max_y }
}

fn nul(text: &[u8]) -> Vec<u8> {
    let mut t = text.to_vec();
    t.push(0);
    t
}

/// The C runtime's `"%.2f"` of a float (promoted to double): the exact value rounded half to even at two places,
/// as the Universal CRT prints it (`inf`, `-inf`, `nan`, `-nan(ind)`).
pub fn format_2f(value: f32) -> Vec<u8> {
    let v = value as f64;
    if v.is_nan() {
        return if v.is_sign_negative() { b"-nan(ind)".to_vec() } else { b"nan".to_vec() };
    }
    if v.is_infinite() {
        return if v < 0.0 { b"-inf".to_vec() } else { b"inf".to_vec() };
    }
    format!("{v:.2}").into_bytes()
}

impl EeUiGraph {
    /// The constructor's part after `UIControl`'s: a 15 x 10 grid, every label on, zoom 1, the displayed range
    /// 0..10, the plot area the control's rectangle, no texts yet.
    pub fn new(host: Host, kind: EeUiGraphKind, format: EeUiGraphFormatFn) -> Self {
        let rect = host.rect();
        let stored = match kind {
            EeUiGraphKind::Line => Vec::new(),
            EeUiGraphKind::PostGame => vec![UiGraph::default(); STORED_GRAPHS + MANAGER_GRAPHS],
        };
        EeUiGraph {
            host,
            kind,
            format,
            s: RefCell::new(State {
                texts: [0; 7],
                zoom: 1.0,
                copied_zoom: 1.0,
                divisions: (15, 10),
                displayed: GraphRange { min_x: 0, max_x: 10, min_y: 0, max_y: 10 },
                zoom_location: UiPoint::default(),
                grid_color: [1.0, 1.0, 1.0],
                graph_rect: rect,
                copied_graph_rect: rect,
                title_rect: UiRect::default(),
                x_rect: UiRect::default(),
                y_rect: UiRect::default(),
                line_opacity: DEFAULT_LINE_OPACITY,
                copied_line_opacity: DEFAULT_LINE_OPACITY,
                zoom_increment: DEFAULT_ZOOM_INCREMENT,
                draw_grid: true,
                copied_draw_grid: true,
                label_grid: true,
                copied_label_grid: true,
                label_x: true,
                copied_label_x: true,
                label_y: true,
                copied_label_y: true,
                label_graph: true,
                copied_label_graph: true,
            }),
            graph: RefCell::new(UiGraph::default()),
            copied: RefCell::new(UiGraph::default()),
            stored: RefCell::new(stored),
        }
    }

    fn st(&self) -> State {
        *self.s.borrow()
    }

    /// The destructor: the texts, when made.
    pub fn release(&self) {
        let t = self.st().texts;
        for (one, copy) in [(TITLE, COPIED_TITLE), (X_AXIS, COPIED_X_AXIS), (Y_AXIS, COPIED_Y_AXIS)] {
            if t[one] != 0 {
                self.host.op(Op::TextDelete, t[one], 0, 0, 0);
                self.host.op(Op::TextDelete, t[copy], 0, 0, 0);
            }
        }
        if t[GRID] != 0 {
            self.host.op(Op::TextDelete, t[GRID], 0, 0, 0);
        }
    }

    /// The kind the graph was made as.
    pub fn kind(&self) -> EeUiGraphKind {
        self.kind
    }

    /// The graph `index` in the summary's numbering (0..30 stored, [`EE_UI_GRAPH_SHOWN`] the shown one, 32..39 the
    /// form manager's; a line graph has only the shown one) handed to `f`; `None` for another index. `f` must not
    /// call into the graph.
    pub fn graph_at<R>(&self, index: u32, f: impl FnOnce(&mut UiGraph) -> R) -> Option<R> {
        let index = index as usize;
        if index == EE_UI_GRAPH_SHOWN as usize {
            return Some(f(&mut self.graph.borrow_mut()));
        }
        let slot = if index < STORED_GRAPHS { index } else { index.checked_sub(1)? };
        let mut stored = self.stored.borrow_mut();
        stored.get_mut(slot).map(f)
    }

    /// The summary's `ShowGraph(index)`: the stored graph shown, the control dirty.
    pub fn show_graph(&self, index: u32) {
        let Some(g) = self.graph_at(index, |g| g.clone()) else {
            return;
        };
        *self.graph.borrow_mut() = g;
        self.host.op(Op::SetDirty, 1, 0, 0, 0);
    }

    /// `SetLabelXAxis` / `SetLabelYAxis`.
    pub fn set_label_x(&self, on: bool) {
        self.s.borrow_mut().label_x = on;
    }
    pub fn set_label_y(&self, on: bool) {
        self.s.borrow_mut().label_y = on;
    }

    /// The summary's form manager's graphs back to empty ones (a new `EEPGSFormManager` came with them).
    pub fn reset_manager_graphs(&self) {
        let mut stored = self.stored.borrow_mut();
        for g in stored.iter_mut().skip(STORED_GRAPHS) {
            *g = UiGraph::default();
        }
    }

    fn format(&self, axis: GraphAxis, value: f32) -> Vec<u8> {
        if let Some(f) = self.format {
            let mut out = vec![0u8; 64];
            // SAFETY: the game's format, 64 live bytes
            let n = unsafe { f(axis as c_int, value, out.as_mut_ptr(), out.len()) };
            out.truncate(n.min(64));
            return out;
        }
        format_2f(value)
    }

    fn set_text(&self, text: isize, bytes: &[u8]) {
        let t = nul(bytes);
        self.host.op(Op::TextSetBytes, text, t.as_ptr() as isize, 0, 0);
    }

    fn extent(&self, text: isize) -> (f32, f32) {
        let mut f = [0.0f32; 4];
        self.host.op_f(Op::TextExtent, text, 0, 0, 0, &mut f);
        (f[0], f[1])
    }

    fn draw_clipped(&self, text: isize, rect: UiRect, at: UiPoint) -> c_long {
        self.host.op_point(Op::TextSetLocation, text, at);
        self.host.op_rect(Op::DrawClippedText, text, 0, rect) as c_long
    }

    /// `CopyData`: the layout of the title, the axis labels and the grid values' margins around the plot area, the
    /// displayed range, and the copies for drawing.
    pub fn copy_data(&self) -> isize {
        let h = &self.host;
        let rect = h.rect();
        self.s.borrow_mut().graph_rect = rect;
        let s = self.st();
        let texts = GraphTexts {
            label_graph: s.label_graph,
            has_title: s.texts[TITLE] != 0,
            label_grid: s.label_grid,
            has_grid: s.texts[GRID] != 0,
            label_y: s.label_y,
            has_y: s.texts[Y_AXIS] != 0,
            label_x: s.label_x,
            has_x: s.texts[X_AXIS] != 0,
        };
        let layout =
            graph::copy_layout(&mut Draw { g: self, line: 0 }, rect, texts, s.displayed.max_x, s.displayed.max_y);
        let mut color = [1.0f32, 1.0, 1.0, 0.0];
        if s.texts[TITLE] != 0 {
            h.op_f(Op::TextFgColor, s.texts[TITLE], 0, 0, 0, &mut color);
        }
        h.op_rect(Op::SetBgRectangle, 0, 0, layout.graph);
        let shown = range(&self.graph.borrow().data);
        {
            let mut m = self.s.borrow_mut();
            m.title_rect = layout.title;
            m.y_rect = layout.y_axis;
            m.x_rect = layout.x_axis;
            m.grid_color = [color[0], color[1], color[2]];
            m.graph_rect = layout.graph;
            m.displayed = graph::copy_range(m.zoom, m.displayed, shown);
        }
        for (copy, one) in [(COPIED_TITLE, TITLE), (COPIED_X_AXIS, X_AXIS), (COPIED_Y_AXIS, Y_AXIS)] {
            if s.texts[one] != 0 {
                h.op(Op::TextCopy, s.texts[copy], s.texts[one], 0, 0);
            }
        }
        *self.copied.borrow_mut() = self.graph.borrow().clone();
        {
            let mut m = self.s.borrow_mut();
            m.copied_graph_rect = m.graph_rect;
            m.copied_zoom = m.zoom;
            m.copied_line_opacity = m.line_opacity;
            m.copied_draw_grid = m.draw_grid;
            m.copied_label_grid = m.label_grid;
            m.copied_label_x = m.label_x;
            m.copied_label_y = m.label_y;
            m.copied_label_graph = m.label_graph;
        }
        h.op(Op::BaseCopyData, 0, 0, 0, 0)
    }

    /// `DrawXAxisText`, `DrawYAxisText`, `DrawTitleText`: the copied text at its rectangle's top left, clipped.
    fn draw_label(&self, which: usize) -> c_long {
        let s = self.st();
        let (text, rect) = match which {
            TITLE => (s.texts[COPIED_TITLE], s.title_rect),
            X_AXIS => (s.texts[COPIED_X_AXIS], s.x_rect),
            _ => (s.texts[COPIED_Y_AXIS], s.y_rect),
        };
        self.draw_clipped(text, rect, rect.top_left())
    }

    /// `DrawGridText`.
    fn draw_grid_text(&self) -> c_long {
        let s = self.st();
        graph::draw_grid_text(
            &mut Draw { g: self, line: 0 },
            s.copied_graph_rect,
            s.divisions,
            s.copied_zoom,
            s.displayed,
        )
    }

    /// `DrawPolygons`: the control, an inner border around the plot area, the grid lines, then every enabled series
    /// of the copied graph with two points or more.
    pub fn draw_polygons(&self) -> c_long {
        let h = &self.host;
        let line = h.op(Op::LineCreate, 0, 0, 0, 0);
        let thickness = h.op(Op::BorderThickness, 0, 0, 0, 0) as c_ulong;
        let result = h.op(Op::BaseDrawPolygons, 0, 0, 0, 0) as c_long;
        let s = self.st();
        let c = s.grid_color;
        h.op_f(Op::LineSetColor, line, 0, 0, 0, &mut [c[0], c[1], c[2], 0.0]);
        h.op_f(Op::LineSetOpacity, line, 0, 0, 0, &mut [s.copied_line_opacity, 0.0, 0.0, 0.0]);
        let result = graph::draw_grid(
            &mut Draw { g: self, line },
            result,
            thickness,
            s.copied_graph_rect,
            s.copied_draw_grid,
            s.divisions,
            s.copied_zoom,
        );
        h.op(Op::LineDelete, line, 0, 0, 0);
        let count = self.copied.borrow().data.len() as c_ulong;
        graph::draw_series_list(&mut Draw { g: self, line: 0 }, result, count)
    }

    /// `DrawSeries`: the copied graph's series `index` as segments scaled into the plot area, in its colour.
    fn draw_series(&self, index: c_ulong) -> c_long {
        let (points, color, min_x, copied) = {
            let g = self.copied.borrow();
            let series = g.data.series(index as usize);
            (series.points().to_vec(), series.color(), series.range().min_x, range(&g.data))
        };
        let h = &self.host;
        let line = h.op(Op::LineCreate, 0, 0, 0, 0);
        h.op_f(Op::LineSetColor, line, 0, 0, 0, &mut [color.red, color.green, color.blue, 0.0]);
        let s = self.st();
        let result = graph::draw_series(
            &mut Points { g: self, points: &points, line },
            points.len() as c_ulong,
            min_x,
            copied,
            s.displayed,
            s.graph_rect,
        );
        h.op(Op::LineDelete, line, 0, 0, 0);
        result
    }

    /// `DrawTextA`: the base's, then the title, the axis labels and the grid values that are on.
    pub fn draw_text(&self) -> c_long {
        self.host.op(Op::BaseDrawText, 0, 0, 0, 0);
        let s = self.st();
        graph::draw_texts(
            &mut Draw { g: self, line: 0 },
            [s.copied_label_graph, s.copied_label_x, s.copied_label_y, s.copied_label_grid],
        )
    }

    /// `SetFont`: every text's font, the texts made on first use; the control dirty.
    pub fn set_font(&self, font: isize) {
        let h = &self.host;
        let mut t = self.st().texts;
        for (one, copy) in [(TITLE, COPIED_TITLE), (X_AXIS, COPIED_X_AXIS), (Y_AXIS, COPIED_Y_AXIS)] {
            if t[one] == 0 {
                t[one] = h.op(Op::TextCreate, font, 0, 0, 0);
                t[copy] = h.op(Op::TextCreate, font, 0, 0, 0);
            } else {
                h.op(Op::TextSetFont, t[one], font, 0, 0);
                h.op(Op::TextSetFont, t[copy], font, 0, 0);
            }
            self.s.borrow_mut().texts = t;
        }
        if t[GRID] == 0 {
            t[GRID] = h.op(Op::TextCreate, font, 0, 0, 0);
        } else {
            h.op(Op::TextSetFont, t[GRID], font, 0, 0);
        }
        self.s.borrow_mut().texts = t;
        h.op(Op::SetDirty, 1, 0, 0, 0);
    }

    /// `SetFontColor`: the title's, the axis labels' and the grid's colour, on a transparent background.
    pub fn set_font_color(&self, color: [f32; 3]) {
        let t = self.st().texts;
        for which in [TITLE, X_AXIS, Y_AXIS, GRID] {
            if t[which] == 0 {
                continue;
            }
            self.host.op(Op::TextSetTransparent, t[which], 1, 0, 0);
            self.host.op_f(Op::TextSetFgColor, t[which], 0, 0, 0, &mut [color[0], color[1], color[2], 0.0]);
        }
    }

    fn zoom(&self, zoom_in: bool) {
        let shown = range(&self.graph.borrow().data);
        {
            let mut m = self.s.borrow_mut();
            let (z, r) = if zoom_in {
                graph::zoom_in(m.zoom, m.zoom_increment, shown)
            } else {
                graph::zoom_out(m.zoom, m.zoom_increment, shown)
            };
            m.zoom = z;
            m.displayed = r;
        }
        self.host.op(Op::SetDirty, 1, 0, 0, 0);
    }

    /// The summary's `AddGraphSeries`: the points (time, value) as a series in `color`, added to stored graph `index`.
    pub fn add_graph_series(&self, index: u32, points: &[(c_long, c_long)], color: [f32; 3]) {
        let mut series = EeUtilSeries::default();
        for &(x, y) in points {
            series.add_point(EeUtilPoint { x, y });
        }
        series.set_color(EeUtilColor { red: color[0], green: color[1], blue: color[2] });
        self.graph_at(index, |g| g.data.add_series(&series));
    }

    /// One inward call.
    ///
    /// # Safety
    /// `f` is NULL or four live floats; for [`EeUiGraphCall::AddGraphSeries`], `b` points at `c` pairs of `long`.
    pub unsafe fn call(&self, call: EeUiGraphCall, a: isize, b: isize, c: isize, f: *mut f32) -> isize {
        use EeUiGraphCall as C;
        // SAFETY: the contract
        let v = unsafe { read_buf(f) };
        match call {
            C::CopyData => return self.copy_data(),
            C::DrawPolygons => return self.draw_polygons() as isize,
            C::DrawText => return self.draw_text() as isize,
            C::SetFont => self.set_font(a),
            C::SetFontColor => self.set_font_color([v[0], v[1], v[2]]),
            C::TabbedTo => {
                self.host.op(Op::SetDirty, 1, 0, 0, 0);
            }
            C::TabbedFrom => {
                self.host.op(Op::ReleaseMouse, 0, 0, 0, 0);
                self.host.op(Op::ReleaseKeyboard, 0, 0, 0, 0);
                self.host.op(Op::SetDirty, 1, 0, 0, 0);
            }
            C::ZoomIn => {
                self.s.borrow_mut().zoom_location = UiPoint { x: v[0], y: v[1] };
                self.zoom(true);
            }
            C::ZoomOut => self.zoom(false),
            C::SetLabelXAxis => self.set_label_x(a != 0),
            C::SetLabelYAxis => self.set_label_y(a != 0),
            C::ShowGraph => self.show_graph(a as u32),
            C::AddGraphSeries => {
                let points: Vec<(c_long, c_long)> = if b == 0 || c <= 0 {
                    Vec::new()
                } else {
                    // SAFETY: the contract: c pairs of longs at b
                    let raw = unsafe { core::slice::from_raw_parts(b as *const c_long, c as usize * 2) };
                    raw.chunks(2).map(|p| (p[0], p[1])).collect()
                };
                self.add_graph_series(a as u32, &points, [v[0], v[1], v[2]]);
            }
        }
        0
    }
}

/// The graph's draws and texts (`graph.rs`'s host) over the C++, with the line they draw with (0 for none).
struct Draw<'a> {
    g: &'a EeUiGraph,
    line: isize,
}

impl GraphHost for Draw<'_> {
    fn text(&mut self, which: GraphText) -> (f32, f32) {
        let (slot, label) = {
            let g = self.g.graph.borrow();
            match which {
                GraphText::Title => (TITLE, g.title.clone()),
                GraphText::YAxis => (Y_AXIS, g.y_label.clone()),
                GraphText::XAxis => (X_AXIS, g.x_label.clone()),
            }
        };
        let text = self.g.st().texts[slot];
        self.g.set_text(text, &label);
        self.g.extent(text)
    }
    fn grid_value(&mut self, axis: GraphAxis, value: f32) -> (f32, f32) {
        let bytes = self.g.format(axis, value);
        let text = self.g.st().texts[GRID];
        self.g.set_text(text, &bytes);
        self.g.extent(text)
    }
    fn draw_grid_value(&mut self, rect: UiRect, at: UiPoint) -> c_long {
        let text = self.g.st().texts[GRID];
        self.g.draw_clipped(text, rect, at)
    }
    fn line(&mut self, from: UiPoint, to: UiPoint) -> c_long {
        let h = &self.g.host;
        h.op_f(Op::LineSetPoints, self.line, 0, 0, 0, &mut [from.x, from.y, to.x, to.y]);
        h.op(Op::LineDraw, self.line, 0, 0, 0) as c_long
    }
    fn border(&mut self, rect: UiRect, thickness: c_ulong) -> c_long {
        self.g.host.op_rect(Op::DrawBorder, thickness as isize, 0, rect) as c_long
    }
    fn draw_text(&mut self, which: u32) -> c_long {
        match which {
            0 => self.g.draw_label(TITLE),
            1 => self.g.draw_label(X_AXIS),
            2 => self.g.draw_label(Y_AXIS),
            _ => self.g.draw_grid_text(),
        }
    }
}

impl SeriesListHost for Draw<'_> {
    fn enabled(&mut self, index: c_ulong) -> bool {
        self.g.copied.borrow().data.series(index as usize).enabled()
    }
    fn point_count(&mut self, index: c_ulong) -> c_ulong {
        self.g.copied.borrow().data.series(index as usize).points().len() as c_ulong
    }
    fn draw(&mut self, index: c_ulong) -> c_long {
        self.g.draw_series(index)
    }
}

/// A series' points (copied out of the graph) and the line drawing it.
struct Points<'a> {
    g: &'a EeUiGraph,
    points: &'a [EeUtilPoint],
    line: isize,
}

impl SeriesHost for Points<'_> {
    fn point(&mut self, index: c_ulong) -> (c_long, c_long) {
        let p = self.points[index as usize];
        (p.x, p.y)
    }
    fn line(&mut self, from: UiPoint, to: UiPoint) -> c_long {
        GraphHost::line(&mut Draw { g: self.g, line: self.line }, from, to)
    }
}

// ---- the C interface (lle/UIGraph.h)

/// The constructor's part after `UIControl`'s ([`EeUiGraph::new`]); the graph recorded for the direct path.
#[no_mangle]
pub extern "C" fn ee_ui_graph_new(
    graph: *mut c_void,
    host: EeUiHostFn,
    kind: EeUiGraphKind,
    format: EeUiGraphFormatFn,
) -> *mut EeUiGraph {
    let obj = Box::into_raw(Box::new(EeUiGraph::new(Host { obj: graph, f: host }, kind, format)));
    crate::direct::made(crate::direct::Kind::Graph, graph, obj)
}

/// The destructor: deletes the texts and frees the object.
///
/// # Safety
/// `graph` came from `ee_ui_graph_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_graph_free(graph: *mut EeUiGraph) {
    if graph.is_null() {
        return;
    }
    crate::direct::forget(crate::direct::Kind::Graph, graph as *const c_void);
    // SAFETY: the contract
    unsafe { (*graph).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(graph) });
}

/// One of the graph's methods ([`EeUiGraphCall`]).
///
/// # Safety
/// `graph` is live; `f` is NULL or four live floats; see [`EeUiGraph::call`].
#[no_mangle]
pub unsafe extern "C" fn ee_ui_graph_call(
    graph: *const EeUiGraph,
    call: EeUiGraphCall,
    a: isize,
    b: isize,
    c: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*graph).call(call, a, b, c, f) }
}

#[cfg(test)]
#[path = "graphobj_tests.rs"]
mod tests;
