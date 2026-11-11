//! `extern "C"` for `UIGraph` and `UILineGraph` (`User Interface/UIGraph.cpp`, `UILineGraph.cpp`): the texts, the
//! lines, the border and the virtual draws go back to the C++ (`ee_uicpp_graph_*`, `ee_uicpp_linegraph_*`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_ulong, c_void};

use crate::graph::{
    self, GraphAxis, GraphHost, GraphLayout, GraphRange, GraphText, GraphTexts, SeriesHost, SeriesListHost,
};
use crate::rect::{UiPoint, UiRect};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    use crate::rect::UiRect;

    extern "C" {
        // which: 0 the title, 1 the y label, 2 the x label
        pub fn ee_uicpp_graph_text(graph: *mut c_void, which: c_int, width: *mut f32, height: *mut f32);
        // axis: 0 FormatXValues, 1 FormatYValues
        pub fn ee_uicpp_graph_grid_value(
            graph: *mut c_void,
            axis: c_int,
            value: f32,
            width: *mut f32,
            height: *mut f32,
        );
        pub fn ee_uicpp_graph_draw_grid_value(graph: *mut c_void, rect: *const UiRect, x: f32, y: f32) -> c_long;
        pub fn ee_uicpp_graph_line(graph: *mut c_void, line: *mut c_void, x0: f32, y0: f32, x1: f32, y1: f32)
            -> c_long;
        pub fn ee_uicpp_graph_border(graph: *mut c_void, rect: *const UiRect, thickness: c_ulong) -> c_long;
        // which: 0 DrawTitleText, 1 DrawXAxisText, 2 DrawYAxisText, 3 DrawGridText
        pub fn ee_uicpp_graph_draw_text(graph: *mut c_void, which: c_int) -> c_long;
        // which: 0 the copied graph's series' GetIsEnabled, 1 its GetDataPointCount
        pub fn ee_uicpp_linegraph_series(graph: *mut c_void, index: c_ulong, which: c_int) -> c_ulong;
        pub fn ee_uicpp_linegraph_draw_series(graph: *mut c_void, index: c_ulong) -> c_long;
        pub fn ee_uicpp_linegraph_point(series: *const c_void, index: c_ulong, x: *mut c_long, y: *mut c_long);
    }
}

/// A graph (`UIGraph*`) and the line its draws use (`GELine*`, NULL where none draws).
struct Raw {
    graph: *mut c_void,
    line: *mut c_void,
}

impl GraphHost for Raw {
    fn text(&mut self, which: GraphText) -> (f32, f32) {
        let (mut w, mut h) = (0.0, 0.0);
        unsafe { sys::ee_uicpp_graph_text(self.graph, which as c_int, &mut w, &mut h) };
        (w, h)
    }
    fn grid_value(&mut self, axis: GraphAxis, value: f32) -> (f32, f32) {
        let (mut w, mut h) = (0.0, 0.0);
        unsafe { sys::ee_uicpp_graph_grid_value(self.graph, axis as c_int, value, &mut w, &mut h) };
        (w, h)
    }
    fn draw_grid_value(&mut self, rect: UiRect, at: UiPoint) -> c_long {
        unsafe { sys::ee_uicpp_graph_draw_grid_value(self.graph, &rect, at.x, at.y) }
    }
    fn line(&mut self, from: UiPoint, to: UiPoint) -> c_long {
        unsafe { sys::ee_uicpp_graph_line(self.graph, self.line, from.x, from.y, to.x, to.y) }
    }
    fn border(&mut self, rect: UiRect, thickness: c_ulong) -> c_long {
        unsafe { sys::ee_uicpp_graph_border(self.graph, &rect, thickness) }
    }
    fn draw_text(&mut self, which: u32) -> c_long {
        unsafe { sys::ee_uicpp_graph_draw_text(self.graph, which as c_int) }
    }
}

impl SeriesListHost for Raw {
    fn enabled(&mut self, index: c_ulong) -> bool {
        unsafe { sys::ee_uicpp_linegraph_series(self.graph, index, 0) != 0 }
    }
    fn point_count(&mut self, index: c_ulong) -> c_ulong {
        unsafe { sys::ee_uicpp_linegraph_series(self.graph, index, 1) }
    }
    fn draw(&mut self, index: c_ulong) -> c_long {
        unsafe { sys::ee_uicpp_linegraph_draw_series(self.graph, index) }
    }
}

/// A series (`const UGraphSeries*`) and the graph and line that draw it.
struct Series {
    series: *const c_void,
    raw: Raw,
}

impl SeriesHost for Series {
    fn point(&mut self, index: c_ulong) -> (c_long, c_long) {
        let (mut x, mut y) = (0, 0);
        unsafe { sys::ee_uicpp_linegraph_point(self.series, index, &mut x, &mut y) };
        (x, y)
    }
    fn line(&mut self, from: UiPoint, to: UiPoint) -> c_long {
        GraphHost::line(&mut self.raw, from, to)
    }
}

/// `UIGraph::CopyData`'s rectangles for the control's `rect` (the texts set through the host).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_graph_copy_layout(
    graph: *mut c_void,
    rect: UiRect,
    texts: GraphTexts,
    max_x: c_long,
    max_y: c_long,
) -> GraphLayout {
    graph::copy_layout(&mut Raw { graph, line: core::ptr::null_mut() }, rect, texts, max_x, max_y)
}

/// `UIGraph::CopyData`'s displayed range.
#[no_mangle]
pub extern "C" fn ee_ui_graph_copy_range(zoom: f32, displayed: GraphRange, graph: GraphRange) -> GraphRange {
    graph::copy_range(zoom, displayed, graph)
}

/// `ZoomIn` (`zoom_in`) or `ZoomOut`: the new zoom; `out` the displayed range.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_graph_zoom(
    zoom_in: bool,
    zoom: f32,
    increment: f32,
    graph: GraphRange,
    out: *mut GraphRange,
) -> f32 {
    let (z, r) = if zoom_in { graph::zoom_in(zoom, increment, graph) } else { graph::zoom_out(zoom, increment, graph) };
    out.write(r);
    z
}

/// `UIGraph::DrawPolygons` after `UIControl::DrawPolygons` (`result`), the grid lines drawn with `line`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_graph_draw_grid(
    graph: *mut c_void,
    line: *mut c_void,
    result: c_long,
    thickness: c_ulong,
    rect: UiRect,
    grid: bool,
    divisions_x: c_ulong,
    divisions_y: c_ulong,
    zoom: f32,
) -> c_long {
    graph::draw_grid(&mut Raw { graph, line }, result, thickness, rect, grid, (divisions_x, divisions_y), zoom)
}

/// `UIGraph::DrawGridText`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_graph_draw_grid_text(
    graph: *mut c_void,
    rect: UiRect,
    divisions_x: c_ulong,
    divisions_y: c_ulong,
    zoom: f32,
    range: GraphRange,
) -> c_long {
    graph::draw_grid_text(
        &mut Raw { graph, line: core::ptr::null_mut() },
        rect,
        (divisions_x, divisions_y),
        zoom,
        range,
    )
}

/// `UIGraph::DrawTextA` after the draw count's decrement.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_graph_draw_texts(
    graph: *mut c_void,
    title: bool,
    x: bool,
    y: bool,
    grid: bool,
) -> c_long {
    graph::draw_texts(&mut Raw { graph, line: core::ptr::null_mut() }, [title, x, y, grid])
}

/// `UILineGraph::DrawPolygons` after `UIGraph::DrawPolygons` (`result`), over the copied graph's `count` series.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_linegraph_draw_list(graph: *mut c_void, result: c_long, count: c_ulong) -> c_long {
    graph::draw_series_list(&mut Raw { graph, line: core::ptr::null_mut() }, result, count)
}

/// `UILineGraph::DrawSeries` of `series` (`count` points), drawn with `line`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_linegraph_draw_series(
    graph: *mut c_void,
    line: *mut c_void,
    series: *const c_void,
    count: c_ulong,
    series_min_x: c_long,
    copied: GraphRange,
    displayed: GraphRange,
    rect: UiRect,
) -> c_long {
    graph::draw_series(&mut Series { series, raw: Raw { graph, line } }, count, series_min_x, copied, displayed, rect)
}
