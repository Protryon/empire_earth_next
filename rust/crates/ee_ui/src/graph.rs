//! `UIGraph` and `UILineGraph` (`User Interface/UIGraph.cpp`, `UILineGraph.cpp`), the post-game summary's graphs:
//! `CopyData`'s layout of the title, the axis labels and the grid values' margins around the plot area
//! ([`copy_layout`], [`copy_range`]), the zoom steps ([`zoom_in`], [`zoom_out`]), the grid lines and the inner border
//! ([`draw_grid`]), the grid values' places ([`draw_grid_text`]), which labels draw ([`draw_texts`]), and a line
//! graph's series: which draw ([`draw_series_list`]) and each segment's skip, clip and scale ([`draw_series`]).
//!
//! The C++ keeps the members, the texts (`FormatXValues`/`FormatYValues` are virtual: the post-game summary formats
//! times and counts), the line, every draw and the copies; each text, line and label goes back to it through
//! [`GraphHost`] / [`SeriesHost`] in the C++'s order, and the last draw's result is returned as the C++ returned it.

use core::ffi::{c_long, c_ulong};

use crate::rect::{float_to_long, um_floor, UiPoint, UiRect};

/// `kTextSpacing`: the gap around the title, the axis labels and the grid values.
pub const TEXT_SPACING: f32 = 5.0;
/// `kMaximumZoomScale`.
pub const MAXIMUM_ZOOM: f32 = 4.0;

/// `UMCeil`: round to nearest, then one more when that is below the input (compared as a float).
#[inline]
pub fn um_ceil(value: f32) -> c_long {
    let r = float_to_long(value);
    if (r as f32) < value {
        r.wrapping_add(1)
    } else {
        r
    }
}

/// `UMCeilFloat` (`UIGraph.h`): `UMCeil` as a float.
#[inline]
pub fn um_ceil_float(value: f32) -> f32 {
    um_ceil(value) as f32
}

/// A graph's value range (`UGraph`'s or the displayed one, `UIGraph::mMinX` ..).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GraphRange {
    pub min_x: c_long,
    pub max_x: c_long,
    pub min_y: c_long,
    pub max_y: c_long,
}

/// Which texts `CopyData` lays out: each label's flag (`mLabelGraph` ..) and whether its text exists (`SetFont`
/// made them).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GraphTexts {
    pub label_graph: bool,
    pub has_title: bool,
    pub label_grid: bool,
    pub has_grid: bool,
    pub label_y: bool,
    pub has_y: bool,
    pub label_x: bool,
    pub has_x: bool,
}

/// `CopyData`'s rectangles: the title's, the axis labels' (empty when off) and the plot area.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GraphLayout {
    pub title: UiRect,
    pub y_axis: UiRect,
    pub x_axis: UiRect,
    pub graph: UiRect,
}

/// The graph's texts.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GraphText {
    /// The title (`mTitleText`, the graph's `GetTitle`).
    Title = 0,
    /// The y axis label (`mYAxisText`, `GetYLabel`).
    YAxis = 1,
    /// The x axis label (`mXAxisText`, `GetXLabel`).
    XAxis = 2,
}

/// A grid value's axis.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GraphAxis {
    /// `FormatXValues`.
    X = 0,
    /// `FormatYValues`.
    Y = 1,
}

/// The graph's C++ side.
pub trait GraphHost {
    /// The text's `SetText` with the graph's string; its extent (width, height).
    fn text(&mut self, which: GraphText) -> (f32, f32);
    /// `FormatXValues`/`FormatYValues(value)` into the grid text, `SetText`; its extent (width, height).
    fn grid_value(&mut self, axis: GraphAxis, value: f32) -> (f32, f32);
    /// The grid text's `SetLocation(at)`, then the form's `DrawClippedText(rect, grid text)`.
    fn draw_grid_value(&mut self, rect: UiRect, at: UiPoint) -> c_long;
    /// The line's start and end, then the rasterizer's `Draw(line)`.
    fn line(&mut self, from: UiPoint, to: UiPoint) -> c_long;
    /// `DrawBorder(rect, thickness)`.
    fn border(&mut self, rect: UiRect, thickness: c_ulong) -> c_long;
    /// The virtual `DrawTitleText` (0), `DrawXAxisText` (1), `DrawYAxisText` (2), `DrawGridText` (3).
    fn draw_text(&mut self, which: u32) -> c_long;
}

/// `CopyData`'s layout of `rect` (the control's): the title centred at the top, the y label at the left, centred
/// down, the x label centred at the bottom, and the plot area inside them and the grid values' margins (measured
/// on the displayed range's maxima, `max_x` with the x format, `max_y` with the y one). The texts are set in the
/// C++'s order: title, the two grid values, the y label, the x label.
pub fn copy_layout(
    h: &mut impl GraphHost,
    rect: UiRect,
    texts: GraphTexts,
    max_x: c_long,
    max_y: c_long,
) -> GraphLayout {
    let mut value_height = 0.0f32;
    let mut value_width = 0.0f32;
    let center_x = rect.width() * 0.5 + rect.left;
    let center_y = rect.height() * 0.5 + rect.top;
    let mut title = UiRect::default();
    if texts.label_graph && texts.has_title {
        let (w, hh) = h.text(GraphText::Title);
        title.left = center_x - w * 0.5;
        title.right = title.left + w;
        title.top = rect.top + TEXT_SPACING;
        title.bottom = title.top + hh;
    }
    if texts.label_grid && texts.has_grid {
        value_height = h.grid_value(GraphAxis::X, max_x as f32).1;
        value_width = h.grid_value(GraphAxis::Y, max_y as f32).0 + TEXT_SPACING * 2.0;
    }
    let mut y_axis = UiRect::default();
    if texts.label_y && texts.has_y {
        let (w, hh) = h.text(GraphText::YAxis);
        y_axis.left = rect.left + TEXT_SPACING;
        y_axis.right = y_axis.left + w;
        y_axis.top = center_y - hh * 0.5;
        y_axis.bottom = y_axis.top + hh;
    }
    let mut x_axis = UiRect::default();
    if texts.label_x && texts.has_x {
        let (w, hh) = h.text(GraphText::XAxis);
        x_axis.left = center_x - w * 0.5;
        x_axis.right = x_axis.left + w;
        x_axis.bottom = rect.bottom - TEXT_SPACING;
        x_axis.top = x_axis.bottom - hh;
    }
    let graph = UiRect::new(
        rect.left + (y_axis.width() + TEXT_SPACING * 2.0 + value_width),
        rect.top + (title.height() + TEXT_SPACING * 2.0),
        rect.right - (TEXT_SPACING * 2.0 + value_width),
        rect.bottom - (x_axis.height() + TEXT_SPACING * 2.0 + value_height),
    );
    GraphLayout { title, y_axis, x_axis, graph }
}

/// `CopyData`'s range: unzoomed (the zoom's floor 1), the displayed range is the graph's.
pub fn copy_range(zoom: f32, displayed: GraphRange, graph: GraphRange) -> GraphRange {
    if um_floor(zoom) == 1 {
        graph
    } else {
        displayed
    }
}

/// The graph's range over the zoom scale, each bound floored.
fn zoomed(zoom: f32, graph: GraphRange) -> GraphRange {
    GraphRange {
        min_x: um_floor(graph.min_x as f32 / zoom),
        max_x: um_floor(graph.max_x as f32 / zoom),
        min_y: um_floor(graph.min_y as f32 / zoom),
        max_y: um_floor(graph.max_y as f32 / zoom),
    }
}

/// `ZoomIn`: one increment more, at most [`MAXIMUM_ZOOM`]; the displayed range shrinks with it.
pub fn zoom_in(zoom: f32, increment: f32, graph: GraphRange) -> (f32, GraphRange) {
    let mut z = increment + zoom;
    if z > MAXIMUM_ZOOM {
        z = MAXIMUM_ZOOM;
    }
    (z, zoomed(z, graph))
}

/// `ZoomOut`: one increment less, at least 1.
pub fn zoom_out(zoom: f32, increment: f32, graph: GraphRange) -> (f32, GraphRange) {
    let mut z = zoom - increment;
    if z < 1.0 {
        z = 1.0;
    }
    (z, zoomed(z, graph))
}

/// `UIGraph::DrawPolygons` after `UIControl::DrawPolygons` (`result`): an inner border one pixel thinner around
/// the plot area `rect` when the border is thicker than one, then (when `grid`) a line at each of the
/// `UMCeil(divisions / zoom)` steps across, edges included, and down.
pub fn draw_grid(
    h: &mut impl GraphHost,
    mut result: c_long,
    thickness: c_ulong,
    rect: UiRect,
    grid: bool,
    divisions: (c_ulong, c_ulong),
    zoom: f32,
) -> c_long {
    if thickness > 1 {
        result = h.border(rect, thickness - 1);
    }
    if grid {
        let across = um_ceil_float(divisions.0 as f32 / zoom);
        let step = rect.width() / across;
        let mut column: c_ulong = 0;
        while (column as f32) <= across {
            let x = column as f32 * step;
            result = h.line(UiPoint { x: rect.left + x, y: rect.top }, UiPoint { x: rect.left + x, y: rect.bottom });
            column = column.wrapping_add(1);
        }
        let down = um_ceil_float(divisions.1 as f32 / zoom);
        let step = rect.height() / down;
        let mut row: c_ulong = 0;
        while (row as f32) <= down {
            let y = rect.top + row as f32 * step;
            result = h.line(UiPoint { x: rect.left, y }, UiPoint { x: rect.right, y });
            row = row.wrapping_add(1);
        }
    }
    result
}

/// `DrawGridText`: the grid values of the displayed `range` on the plot area `rect`: below each column line
/// (edges included) the x value from the minimum up, capped at the maximum, and left of each row line but the
/// bottom one the y value from the maximum down. Returns the last draw's result (`kErrorOK` without one).
pub fn draw_grid_text(
    h: &mut impl GraphHost,
    rect: UiRect,
    divisions: (c_ulong, c_ulong),
    zoom: f32,
    range: GraphRange,
) -> c_long {
    let mut result = 0;
    let across = um_ceil_float(divisions.0 as f32 / zoom);
    let step = rect.width() / across;
    let value_step = range.max_x.wrapping_sub(range.min_x) as f32 / (divisions.0 as f32 / zoom);
    let mut value = 0.0f32;
    let mut column: c_ulong = 0;
    while (column as f32) <= across {
        let mut label = range.min_x as f32 + value;
        if label > range.max_x as f32 {
            label = range.max_x as f32;
        }
        value += value_step;
        let (w, hh) = h.grid_value(GraphAxis::X, label);
        // GERectangle(top left, width, height): right and bottom one pixel in
        let left = column as f32 * step + rect.left;
        let r = UiRect::new(left, rect.bottom, left + (w - 1.0), rect.bottom + (hh - 1.0));
        result = h.draw_grid_value(r, r.top_left());
        column = column.wrapping_add(1);
    }
    let down = um_ceil_float(divisions.1 as f32 / zoom);
    let step = rect.height() / down;
    let value_step = range.max_y.wrapping_sub(range.min_y) as f32 / (divisions.1 as f32 / zoom);
    let mut value = 0.0f32;
    let mut row: c_ulong = 0;
    while (row as f32) < down {
        let mut label = range.max_y as f32 - value;
        if label > range.max_y as f32 {
            label = range.max_y as f32;
        }
        value += value_step;
        let (w, hh) = h.grid_value(GraphAxis::Y, label);
        let x = rect.left - w;
        let offset = row as f32 * step;
        let y = rect.top + offset;
        let r = UiRect::new(x, y, rect.left, rect.top + hh + offset);
        result = h.draw_grid_value(r, UiPoint { x, y });
        row = row.wrapping_add(1);
    }
    result
}

/// `UIGraph::DrawTextA` after its draw count's decrement: the title, the x label, the y label and the grid values
/// that are on (`labels`: graph, x, y, grid), through the virtual draws; the last one's result.
pub fn draw_texts(h: &mut impl GraphHost, labels: [bool; 4]) -> c_long {
    let mut result = 0;
    for (which, on) in labels.iter().enumerate() {
        if *on {
            result = h.draw_text(which as u32);
        }
    }
    result
}

/// A line graph's series list (`mCopiedGraph`'s).
pub trait SeriesListHost {
    /// The series' `GetIsEnabled` and `GetDataPointCount`.
    fn enabled(&mut self, index: c_ulong) -> bool;
    fn point_count(&mut self, index: c_ulong) -> c_ulong;
    /// The virtual `DrawSeries(series)`.
    fn draw(&mut self, index: c_ulong) -> c_long;
}

/// `UILineGraph::DrawPolygons` after `UIGraph::DrawPolygons` (`result`): every enabled series of two points or more.
pub fn draw_series_list(h: &mut impl SeriesListHost, mut result: c_long, count: c_ulong) -> c_long {
    let mut i: c_ulong = 0;
    while i < count {
        if h.enabled(i) && h.point_count(i) > 1 {
            result = h.draw(i);
        }
        i += 1;
    }
    result
}

/// A series' points and the line.
pub trait SeriesHost {
    /// `GetDataPoint(index)`'s x and y.
    fn point(&mut self, index: c_ulong) -> (c_long, c_long);
    /// The line's start and end, then the rasterizer's `Draw(line)`.
    fn line(&mut self, from: UiPoint, to: UiPoint) -> c_long;
}

/// The segment clipped to the box (Liang-Barsky): `None` when nothing of it is inside.
#[allow(clippy::too_many_arguments)]
pub fn clip_segment(
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
    min_x: f32,
    max_x: f32,
    min_y: f32,
    max_y: f32,
) -> Option<(f32, f32, f32, f32)> {
    let dx = x1 - x0;
    let dy = y1 - y0;
    let p = [-dx, dx, -dy, dy];
    let q = [x0 - min_x, max_x - x0, y0 - min_y, max_y - y0];
    let mut t0 = 0.0f32;
    let mut t1 = 1.0f32;
    for i in 0..4 {
        if p[i] == 0.0 {
            if q[i] < 0.0 {
                return None;
            }
        } else {
            let t = q[i] / p[i];
            if p[i] < 0.0 {
                if t > t1 {
                    return None;
                }
                if t > t0 {
                    t0 = t;
                }
            } else {
                if t < t0 {
                    return None;
                }
                if t < t1 {
                    t1 = t;
                }
            }
        }
    }
    Some((x0 + t0 * dx, y0 + t0 * dy, x0 + t1 * dx, y0 + t1 * dy))
}

/// `UILineGraph::DrawSeries`: each segment of the `count` points, skipped when vertical at or left of the series'
/// minimum x (`series_min_x`, compared unsigned as the C++ did), clipped to the `displayed` range and scaled from
/// the copied `graph` range onto `rect` (the live `mGraphRectangle`, as the C++ read it): x from the left, y up
/// from the bottom. The last draw's result (`kErrorOK` without one).
pub fn draw_series(
    h: &mut impl SeriesHost,
    count: c_ulong,
    series_min_x: c_long,
    graph: GraphRange,
    displayed: GraphRange,
    rect: UiRect,
) -> c_long {
    let left = rect.left;
    let bottom = rect.bottom;
    let mut result = 0;
    let mut range_x = graph.max_x.wrapping_sub(graph.min_x) as c_ulong;
    if range_x == 0 {
        range_x = 1;
    }
    let mut range_y = graph.max_y.wrapping_sub(graph.min_y) as c_ulong;
    if range_y == 0 {
        range_y = 1;
    }
    let scale_x = rect.width() / range_x as f32;
    let scale_y = rect.height() / range_y as f32;
    let mut i: c_ulong = 1;
    while i < count {
        let (sx, sy) = h.point(i - 1);
        let (ex, ey) = h.point(i);
        let (start_x, start_y, end_x, end_y) = (sx as c_ulong, sy as c_ulong, ex as c_ulong, ey as c_ulong);
        i += 1;
        if start_x == end_x && start_x <= series_min_x as c_ulong {
            continue;
        }
        let Some((x0, y0, x1, y1)) = clip_segment(
            start_x as c_long as f32,
            start_y as c_long as f32,
            end_x as c_long as f32,
            end_y as c_long as f32,
            displayed.min_x as f32,
            displayed.max_x as f32,
            displayed.min_y as f32,
            displayed.max_y as f32,
        ) else {
            continue;
        };
        let min_x = graph.min_x as f32;
        let min_y = graph.min_y as f32;
        result = h.line(
            UiPoint { x: (x0 - min_x) * scale_x + left, y: bottom - (y0 - min_y) * scale_y },
            UiPoint { x: (x1 - min_x) * scale_x + left, y: bottom - (y1 - min_y) * scale_y },
        );
    }
    result
}

#[cfg(test)]
mod tests;
