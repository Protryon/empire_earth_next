//! Replays `tests/util/util_golden.cpp`'s trace of the original `UGraph.cpp` and `U2DFractal.cpp` over the C interface
//! of the Rust objects: every operation line is applied, and the state lines the driver printed after it must be
//! what the Rust objects print. `golden_util.txt` is the head of the trace (checked in);
//! `EE_UTIL_GOLDEN=<file> cargo test -p ee_util --lib golden -- --nocapture` replays a whole one.

use crate::util::*;
use std::ffi::c_ulong;

fn series_state(s: *const EeUtilSeries) -> String {
    unsafe {
        let c = &*ee_util_series_color(s);
        let r = &*ee_util_series_range(s);
        let n = ee_util_series_count(s);
        let mut o = format!(
            "n{} c{:08x},{:08x},{:08x} e{} r{},{},{},{} p",
            n,
            c.red.to_bits(),
            c.green.to_bits(),
            c.blue.to_bits(),
            ee_util_series_enabled(s) as i32,
            r.min_x,
            r.max_x,
            r.min_y,
            r.max_y
        );
        for i in 0..n {
            let p = &*ee_util_series_point(s, i);
            o += &format!(" {},{}", p.x, p.y);
        }
        o
    }
}

struct Graph {
    graph: *mut EeUtilGraph,
    strings: [String; 3],
}

fn graph_state(slot: usize, g: &Graph, out: &mut Vec<String>) {
    unsafe {
        let r = &*ee_util_graph_range(g.graph);
        let n = ee_util_graph_count(g.graph);
        out.push(format!(
            "G{} n{} r{},{},{},{} t{}|{}|{}",
            slot, n, r.min_x, r.max_x, r.min_y, r.max_y, g.strings[0], g.strings[1], g.strings[2]
        ));
        for i in 0..n {
            out.push(format!(" {}", series_state(*ee_util_graph_series(g.graph, i))));
        }
    }
}

fn fractal_rows(f: &EeUtilFractal, d: u32, out: &mut Vec<String>) {
    for x in 0..d {
        let mut row = String::from(" ");
        for y in 0..d {
            row += &format!("{:08x}", f.value(x, y).to_bits());
        }
        out.push(row);
    }
}

/// Replays a trace; returns the number of operations checked.
fn replay(text: &str) -> usize {
    let lines: Vec<&str> = text.lines().collect();
    let mut series: Vec<*mut EeUtilSeries> = Vec::new();
    let mut graphs: Vec<Graph> = Vec::new();
    let mut fractal: Option<(EeUtilFractal, u32)> = None;
    let mut checked = 0;
    let mut at = 0;
    let free = |series: &mut Vec<*mut EeUtilSeries>, graphs: &mut Vec<Graph>| unsafe {
        for s in series.drain(..) {
            ee_util_series_free(s);
        }
        for g in graphs.drain(..) {
            ee_util_graph_free(g.graph);
        }
    };
    while at < lines.len() {
        let line = lines[at];
        at += 1;
        let w: Vec<&str> = line.split(' ').collect();
        let n = |k: usize| -> i64 { w[k].parse().unwrap_or_else(|_| panic!("line {at}: {line}")) };
        let u = |k: usize| n(k) as usize;
        let hex = |k: usize| f32::from_bits(u32::from_str_radix(w[k], 16).unwrap());
        let mut out = Vec::new();
        let mut series_slot = None;
        unsafe {
            match w[0] {
                "seq" => {
                    free(&mut series, &mut graphs);
                    series = (0..6).map(|_| ee_util_series_new()).collect();
                    graphs =
                        (0..4).map(|_| Graph { graph: ee_util_graph_new(), strings: Default::default() }).collect();
                    continue;
                }
                "snew" => {
                    ee_util_series_free(series[u(1)]);
                    series[u(1)] = ee_util_series_new();
                    series_slot = Some(u(1));
                }
                "scopy" => {
                    ee_util_series_free(series[u(1)]);
                    series[u(1)] = ee_util_series_clone(series[u(2)]);
                    series_slot = Some(u(1));
                }
                "sasg" => {
                    ee_util_series_assign(series[u(1)], series[u(2)]);
                    series_slot = Some(u(1));
                }
                "sadd" => {
                    ee_util_series_add_point(series[u(1)], n(2) as _, n(3) as _);
                    series_slot = Some(u(1));
                }
                "sclr" => {
                    ee_util_series_clear(series[u(1)]);
                    series_slot = Some(u(1));
                }
                "srem" => {
                    ee_util_series_remove_point(series[u(1)], n(2) as c_ulong);
                    series_slot = Some(u(1));
                }
                "scol" => {
                    let c = EeUtilColor { red: hex(2), green: hex(3), blue: hex(4) };
                    ee_util_series_set_color(series[u(1)], &c);
                    series_slot = Some(u(1));
                }
                "sen" => {
                    ee_util_series_set_enabled(series[u(1)], n(2) != 0);
                    series_slot = Some(u(1));
                }
                "sfromg" => {
                    let copy = ee_util_series_clone(*ee_util_graph_series(graphs[u(2)].graph, n(3) as c_ulong));
                    ee_util_series_free(series[u(1)]);
                    series[u(1)] = copy;
                    series_slot = Some(u(1));
                }
                "gnew" => {
                    ee_util_graph_free(graphs[u(1)].graph);
                    graphs[u(1)] = Graph { graph: ee_util_graph_new(), strings: Default::default() };
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "gcopy" => {
                    let copy =
                        Graph { graph: ee_util_graph_clone(graphs[u(2)].graph), strings: graphs[u(2)].strings.clone() };
                    ee_util_graph_free(graphs[u(1)].graph);
                    graphs[u(1)] = copy;
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "gasg" => {
                    ee_util_graph_assign(graphs[u(1)].graph, graphs[u(2)].graph);
                    graphs[u(1)].strings = graphs[u(2)].strings.clone();
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "gadd" => {
                    ee_util_graph_add_series(graphs[u(1)].graph, series[u(2)]);
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "gclr" => {
                    ee_util_graph_clear(graphs[u(1)].graph);
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "gprune" => {
                    ee_util_graph_prune(graphs[u(1)].graph);
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "grem" => {
                    ee_util_graph_remove_series(graphs[u(1)].graph, n(2) as c_ulong);
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "gen" => {
                    ee_util_graph_set_enabled(graphs[u(1)].graph, n(2) as c_ulong, n(3) != 0);
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "gtitle" => {
                    let k = n(2);
                    graphs[u(1)].strings = [format!("title {k}"), format!("x {}", k + 1), format!("y {}", k + 2)];
                    graph_state(u(1), &graphs[u(1)], &mut out);
                }
                "gcomb" => {
                    ee_util_graph_combine(graphs[u(1)].graph, graphs[u(2)].graph, graphs[u(3)].graph);
                    graph_state(u(3), &graphs[u(3)], &mut out);
                }
                "frac" => {
                    let d = n(1) as u32;
                    let mut f = EeUtilFractal::new(d);
                    f.compute(hex(2), hex(3), n(4) as u32);
                    fractal_rows(&f, d, &mut out);
                    fractal = Some((f, d));
                }
                "frecomp" => {
                    let (f, d) = fractal.as_mut().expect("frecomp after frac");
                    f.compute(hex(1), hex(2), n(3) as u32);
                    let d = *d;
                    fractal_rows(f, d, &mut out);
                }
                "progress" | "register" | "shutdown" | "end" => continue,
                other => panic!("line {at}: unexpected {other:?}: {line}"),
            }
            if let Some(slot) = series_slot {
                out.push(format!("S{} {}", slot, series_state(series[slot])));
            }
        }
        for expected in out {
            assert_eq!(lines.get(at).copied(), Some(expected.as_str()), "after line {at}: {line}");
            at += 1;
        }
        checked += 1;
    }
    free(&mut series, &mut graphs);
    checked
}

#[test]
fn golden_util() {
    let n = replay(include_str!("golden_util.txt"));
    assert!(n > 1000, "{n} operations");
    if let Ok(file) = std::env::var("EE_UTIL_GOLDEN") {
        let text = std::fs::read_to_string(file).expect("the golden file");
        let n = replay(&text);
        println!("{n} operations replayed");
    }
}
