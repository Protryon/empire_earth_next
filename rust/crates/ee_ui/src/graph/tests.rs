//! `UIGraph` and `UILineGraph` against the real C++ (`golden_graph.txt`): a scratch driver (`build/u9_golden`,
//! `driver_graph.cpp`) ran the unmodified `UIGraph.cpp` and `UILineGraph.cpp` (with the real `UGraph.cpp`,
//! `GELine.cpp` and `GERectangle.cpp`; `UIControl`'s members and `UIForm::DrawClippedText` stubbed, each logging its
//! arguments and returning the next number of a counter) over 120 line graphs of 30 random operations: random
//! graphs (series inside and outside the range, vertical segments, empty graphs and ranges set by hand), fonts, the
//! label and grid flags, divisions (0 included), `ZoomIn`, `ZoomOut`, `SetZoomScale` and the increment, moves,
//! borders and opacities, then `CopyData`, `DrawPolygons` and `DrawTextA`, printing every draw and the graph's
//! state after each. The replay below is the same graph over this module; the driver relinked over the forwarders
//! prints the same file, and the same on two more seeds at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_graph.txt");
const FONT_HEIGHTS: [c_long; 3] = [8, 12, 23];

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn rect_text(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn hex(b: &[u8]) -> String {
    if b.is_empty() {
        return "-".into();
    }
    b.iter().map(|c| format!("{c:02x}")).collect()
}

/// The driver's text measure (`fakes.cpp`): a hash advance per code point times the font's height / 8 + 1.
fn extent(text: &[u8], height: c_long) -> (f32, f32) {
    let mut w = 0.0f32;
    let mut at = 0;
    while at < text.len() {
        let (c, l) = ee_plat_core::utf8::decode(&text[at..]);
        w += (3 + c.wrapping_mul(7) % 9) as f32 * (height / 8 + 1) as f32;
        at += l.max(1);
    }
    (w, height as f32)
}

/// `UILineGraph::FormatXValues`/`FormatYValues`: `sprintf("%.2f")` (the UCRT's spelling of the non-numbers).
fn format_value(v: f32) -> Vec<u8> {
    if v.is_nan() {
        return if v.is_sign_negative() { b"-nan(ind)".to_vec() } else { b"nan".to_vec() };
    }
    if v.is_infinite() {
        return if v < 0.0 { b"-inf".to_vec() } else { b"inf".to_vec() };
    }
    format!("{:.2}", v as f64).into_bytes()
}

#[derive(Clone, Default)]
struct Series {
    points: Vec<(c_long, c_long)>,
    color: [f32; 3],
    enabled: bool,
    min_x: c_long,
}

#[derive(Clone, Default)]
struct Graph {
    title: Vec<u8>,
    x_label: Vec<u8>,
    y_label: Vec<u8>,
    series: Vec<Series>,
    range: GraphRange,
}

#[derive(Default)]
struct World {
    rect: UiRect,
    border: c_ulong,
    font: c_long,
    zoom: f32,
    copied_zoom: f32,
    increment: f32,
    range: GraphRange,
    divisions: (c_ulong, c_ulong),
    draws: u32,
    graph_rect: UiRect,
    copied_rect: UiRect,
    layout: GraphLayout,
    flags: [bool; 5], // grid, label grid, x, y, graph
    copied_flags: [bool; 5],
    opacity: f32,
    copied_opacity: f32,
    graph: Graph,
    copied: Graph,
    grid_color: [f32; 3],
    // the texts' contents (mTitleText, mXAxisText, mYAxisText) and their copies
    texts: [Vec<u8>; 3],
    copied_texts: [Vec<u8>; 3],
    counter: c_long,
    draw_result: c_long,
    out: Vec<String>,
}

impl World {
    fn next(&mut self) -> c_long {
        self.counter += 1;
        self.counter
    }
    fn state(&mut self) {
        let r = self.range;
        self.out.push(format!(
            "  state zoom {} {} inc {} range {} {} {} {} div {} {} draws {}",
            h(self.zoom),
            h(self.copied_zoom),
            h(self.increment),
            r.min_x,
            r.max_x,
            r.min_y,
            r.max_y,
            self.divisions.0,
            self.divisions.1,
            self.draws
        ));
        self.out.push(format!(
            "    graph {} copied {} title {} x {} y {}",
            rect_text(&self.graph_rect),
            rect_text(&self.copied_rect),
            rect_text(&self.layout.title),
            rect_text(&self.layout.x_axis),
            rect_text(&self.layout.y_axis)
        ));
        let f = |a: &[bool; 5]| a.iter().map(|b| if *b { '1' } else { '0' }).collect::<String>();
        self.out.push(format!(
            "   flags {} copied {} opacity {} {}",
            f(&self.flags),
            f(&self.copied_flags),
            h(self.opacity),
            h(self.copied_opacity)
        ));
    }
    fn line(&mut self, from: UiPoint, to: UiPoint, color: [f32; 3], opacity: f32) -> c_long {
        self.out.push(format!(
            "  line {} {} {} {} col {} {} {} op {} n 1 scr 1",
            h(from.x),
            h(from.y),
            h(to.x),
            h(to.y),
            h(color[0]),
            h(color[1]),
            h(color[2]),
            h(opacity)
        ));
        self.draw_result
    }
    fn label(&mut self, text: &[u8], at: UiPoint, rect: UiRect) -> c_long {
        let r = self.next();
        self.out.push(format!("  label {} at {} {}  rect {} -> {}", hex(text), h(at.x), h(at.y), rect_text(&rect), r));
        r
    }
}

/// The graph's host as the driver's world: the texts measured by the fake, the draws logged as the stubs log them.
struct Host<'a> {
    w: &'a mut World,
    grid_text: Vec<u8>,
}

impl GraphHost for Host<'_> {
    fn text(&mut self, which: GraphText) -> (f32, f32) {
        let (i, s) = match which {
            GraphText::Title => (0, self.w.graph.title.clone()),
            GraphText::YAxis => (2, self.w.graph.y_label.clone()),
            GraphText::XAxis => (1, self.w.graph.x_label.clone()),
        };
        self.w.texts[i] = s;
        extent(&self.w.texts[i], self.w.font)
    }
    fn grid_value(&mut self, _axis: GraphAxis, value: f32) -> (f32, f32) {
        self.grid_text = format_value(value);
        extent(&self.grid_text, self.w.font)
    }
    fn draw_grid_value(&mut self, rect: UiRect, at: UiPoint) -> c_long {
        let t = self.grid_text.clone();
        self.w.label(&t, at, rect)
    }
    fn line(&mut self, from: UiPoint, to: UiPoint) -> c_long {
        let (c, o) = (self.w.grid_color, self.w.copied_opacity);
        self.w.line(from, to, c, o)
    }
    fn border(&mut self, rect: UiRect, thickness: c_ulong) -> c_long {
        let r = self.w.next();
        self.w.out.push(format!("  border {} t {} -> {}", rect_text(&rect), thickness, r));
        r
    }
    fn draw_text(&mut self, which: u32) -> c_long {
        let l = self.w.layout;
        match which {
            0 => {
                let t = self.w.copied_texts[0].clone();
                self.w.label(&t, l.title.top_left(), l.title)
            }
            1 => {
                let t = self.w.copied_texts[1].clone();
                self.w.label(&t, l.x_axis.top_left(), l.x_axis)
            }
            2 => {
                let t = self.w.copied_texts[2].clone();
                self.w.label(&t, l.y_axis.top_left(), l.y_axis)
            }
            _ => {
                let (rect, div, zoom, range) = (self.w.copied_rect, self.w.divisions, self.w.copied_zoom, self.w.range);
                draw_grid_text(self, rect, div, zoom, range)
            }
        }
    }
}

struct List<'a>(&'a mut World);

impl SeriesListHost for List<'_> {
    fn enabled(&mut self, index: c_ulong) -> bool {
        self.0.copied.series[index as usize].enabled
    }
    fn point_count(&mut self, index: c_ulong) -> c_ulong {
        self.0.copied.series[index as usize].points.len() as c_ulong
    }
    fn draw(&mut self, index: c_ulong) -> c_long {
        let s = self.0.copied.series[index as usize].clone();
        let (graph, displayed, rect) = (self.0.copied.range, self.0.range, self.0.graph_rect);
        let mut one = One { w: self.0, s: &s };
        draw_series(&mut one, s.points.len() as c_ulong, s.min_x, graph, displayed, rect)
    }
}

struct One<'a> {
    w: &'a mut World,
    s: &'a Series,
}

impl SeriesHost for One<'_> {
    fn point(&mut self, index: c_ulong) -> (c_long, c_long) {
        self.s.points[index as usize]
    }
    fn line(&mut self, from: UiPoint, to: UiPoint) -> c_long {
        let c = self.s.color;
        self.w.line(from, to, c, 1.0)
    }
}

#[test]
fn replays_the_cpp_golden() {
    assert_eq!(replay(GOLDEN), 120);
}

/// Another seed's golden (the driver's seeds 2 and 3, ten times the size): `EE_UI_GOLDEN=<file> cargo test --
/// --ignored graph`.
#[test]
#[ignore]
fn replays_another_seed() {
    let path = std::env::var("EE_UI_GOLDEN").expect("EE_UI_GOLDEN");
    assert_eq!(replay(&std::fs::read_to_string(path).unwrap()), 1200);
}

fn replay(golden: &str) -> usize {
    let lines: Vec<&str> = golden.lines().collect();
    let mut w = World { counter: 1000, ..World::default() };
    let mut graphs = 0;
    let mut i = 0;
    while i < lines.len() {
        let line = lines[i];
        i += 1;
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "graph" {
            graphs += 1;
            w.out.push(line.to_string());
            let rect = UiRect::new(fx(t[3]), fx(t[4]), fx(t[5]), fx(t[6]));
            let out = core::mem::take(&mut w.out);
            let counter = w.counter;
            w = World {
                rect,
                border: t[8].parse().unwrap(),
                font: FONT_HEIGHTS[t[10].parse::<usize>().unwrap()],
                zoom: 1.0,
                copied_zoom: 1.0,
                increment: 1.0,
                range: GraphRange { min_x: 0, max_x: 10, min_y: 0, max_y: 10 },
                divisions: (15, 10),
                graph_rect: rect,
                copied_rect: rect,
                flags: [true; 5],
                copied_flags: [true; 5],
                opacity: 0.25,
                copied_opacity: 0.25,
                counter,
                out,
                ..World::default()
            };
            w.out.push("  dirty 1".into()); // SetFont
            continue;
        }
        if t[0] != "op" {
            continue;
        }
        w.out.push(line.to_string());
        match t[1] {
            "graph" => {
                let mut g =
                    Graph { title: unhex(t[3]), x_label: unhex(t[5]), y_label: unhex(t[7]), ..Graph::default() };
                // UGraph::ClearAllSeries' empty range, then each AddSeries widening it
                g.range = GraphRange {
                    min_x: 0x7fffffff,
                    max_x: i32::MIN as c_long,
                    min_y: 0x7fffffff,
                    max_y: i32::MIN as c_long,
                };
                let n: usize = t[9].parse().unwrap();
                for _ in 0..n {
                    let s: Vec<&str> = lines[i].split(' ').collect();
                    w.out.push(lines[i].to_string());
                    i += 1;
                    let count: usize = s[3].parse().unwrap();
                    let mut one = Series::default();
                    for p in &s[4..4 + count] {
                        let (x, y) = p.split_once(',').unwrap();
                        one.points.push((x.parse().unwrap(), y.parse().unwrap()));
                    }
                    let k = 4 + count;
                    one.color = [fx(s[k + 1]), fx(s[k + 2]), fx(s[k + 3])];
                    one.enabled = s[k + 5] == "1";
                    let r = one.points.iter().fold(
                        GraphRange {
                            min_x: 0x7fffffff,
                            max_x: i32::MIN as c_long,
                            min_y: 0x7fffffff,
                            max_y: i32::MIN as c_long,
                        },
                        |r, &(x, y)| GraphRange {
                            min_x: r.min_x.min(x),
                            max_x: r.max_x.max(x),
                            min_y: r.min_y.min(y),
                            max_y: r.max_y.max(y),
                        },
                    );
                    one.min_x = r.min_x;
                    assert_eq!(one.min_x.to_string(), s[k + 7], "series minimum, line {i}");
                    g.range = GraphRange {
                        min_x: g.range.min_x.min(r.min_x),
                        max_x: g.range.max_x.max(r.max_x),
                        min_y: g.range.min_y.min(r.min_y),
                        max_y: g.range.max_y.max(r.max_y),
                    };
                    g.series.push(one);
                }
                // the range line: the driver's hand-set range, or AddSeries' (checked)
                let s: Vec<&str> = lines[i].split(' ').collect();
                w.out.push(lines[i].to_string());
                i += 1;
                let r = GraphRange {
                    min_x: s[4].parse().unwrap(),
                    max_x: s[5].parse().unwrap(),
                    min_y: s[6].parse().unwrap(),
                    max_y: s[7].parse().unwrap(),
                };
                g.range = r;
                w.graph = g;
            }
            "font" => {
                w.font = FONT_HEIGHTS[t[2].parse::<usize>().unwrap()];
                w.out.push("  dirty 1".into());
            }
            "flags" => {
                let m: u32 = t[2].parse().unwrap();
                for (k, f) in w.flags.iter_mut().enumerate() {
                    *f = m & (1 << k) != 0;
                }
            }
            "div" => w.divisions = (t[2].parse().unwrap(), t[3].parse().unwrap()),
            "zoomin" => {
                let (z, r) = zoom_in(w.zoom, w.increment, w.graph.range);
                w.zoom = z;
                w.range = r;
                w.out.push("  dirty 1".into());
                w.out.push(format!("  at {} {}", t[2], t[3]));
            }
            "zoomout" => {
                let (z, r) = zoom_out(w.zoom, w.increment, w.graph.range);
                w.zoom = z;
                w.range = r;
                w.out.push("  dirty 1".into());
            }
            "zoom" => w.zoom = fx(t[2]),
            "inc" => w.increment = fx(t[2]),
            "rect" => w.rect = UiRect::new(fx(t[3]), fx(t[4]), fx(t[5]), fx(t[6])),
            "border" => {
                w.border = t[2].parse().unwrap();
                w.opacity = fx(t[4]);
            }
            "copy" => {
                let texts = GraphTexts {
                    label_graph: w.flags[4],
                    has_title: true,
                    label_grid: w.flags[1],
                    has_grid: true,
                    label_y: w.flags[3],
                    has_y: true,
                    label_x: w.flags[2],
                    has_x: true,
                };
                let (rect, max_x, max_y) = (w.rect, w.range.max_x, w.range.max_y);
                let layout = copy_layout(&mut Host { w: &mut w, grid_text: Vec::new() }, rect, texts, max_x, max_y);
                w.layout = layout;
                w.grid_color = [0.25, 0.5, 1.0]; // the title's foreground colour (SetFontColor)
                w.graph_rect = layout.graph;
                w.out.push(format!("  bgrect {}", rect_text(&layout.graph)));
                w.range = copy_range(w.zoom, w.range, w.graph.range);
                w.copied_texts = w.texts.clone();
                w.copied = w.graph.clone();
                w.copied_rect = w.graph_rect;
                w.copied_zoom = w.zoom;
                w.copied_opacity = w.opacity;
                w.copied_flags = w.flags;
                let r = w.next();
                w.out.push(format!("  base copy -> {r}"));
                let c = w.grid_color;
                w.out.push(format!("  copy -> {r} grid {} {} {}", h(c[0]), h(c[1]), h(c[2])));
            }
            "polygons" => {
                w.draw_result = t[3].parse().unwrap();
                let base = w.next();
                w.out.push(format!("  base polygons -> {base}"));
                let (border, rect, grid, div, zoom) =
                    (w.border, w.copied_rect, w.copied_flags[0], w.divisions, w.copied_zoom);
                let r = draw_grid(&mut Host { w: &mut w, grid_text: Vec::new() }, base, border, rect, grid, div, zoom);
                let n = w.copied.series.len() as c_ulong;
                let r = draw_series_list(&mut List(&mut w), r, n);
                w.out.push(format!("  polygons -> {r}"));
            }
            "text" => {
                w.draws = w.draws.wrapping_sub(1);
                let f = w.copied_flags;
                let r = draw_texts(&mut Host { w: &mut w, grid_text: Vec::new() }, [f[4], f[2], f[3], f[1]]);
                w.out.push(format!("  text -> {r}"));
            }
            other => panic!("op {other}"),
        }
        w.state();
    }
    let out = w.out;
    for (n, (a, g)) in out.iter().zip(lines.iter()).enumerate() {
        assert_eq!(a, g, "line {}", n + 1);
    }
    assert_eq!(out.len(), lines.len());
    graphs
}

#[test]
fn the_ceiling_rounds_up_and_the_zoom_clamps() {
    assert_eq!(um_ceil(1.2), 2);
    assert_eq!(um_ceil(2.0), 2);
    assert_eq!(um_ceil(-1.5), -1);
    assert_eq!(um_ceil_float(15.0 / 4.0), 4.0);
    let g = GraphRange { min_x: 0, max_x: 100, min_y: -9, max_y: 50 };
    assert_eq!(zoom_in(3.5, 1.0, g), (4.0, GraphRange { min_x: 0, max_x: 25, min_y: -3, max_y: 12 }));
    assert_eq!(zoom_out(1.5, 1.0, g), (1.0, g));
    // unzoomed (the floor is 1) the displayed range follows the graph's
    let d = GraphRange { min_x: 1, max_x: 2, min_y: 3, max_y: 4 };
    assert_eq!(copy_range(1.9, d, g), g);
    assert_eq!(copy_range(2.0, d, g), d);
}

#[test]
fn a_segment_clips_to_the_range() {
    assert_eq!(clip_segment(-10.0, 0.0, 10.0, 0.0, 0.0, 5.0, -1.0, 1.0), Some((0.0, 0.0, 5.0, 0.0)));
    assert_eq!(clip_segment(-10.0, 5.0, 10.0, 5.0, 0.0, 5.0, -1.0, 1.0), None);
    assert_eq!(clip_segment(1.0, 0.0, 2.0, 0.0, 0.0, 5.0, -1.0, 1.0), Some((1.0, 0.0, 2.0, 0.0)));
}
