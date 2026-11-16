//! The graph's golden (`golden_graphobj.txt`): the first sequences the round-3 driver (`build/sb3/gr_driver.cpp`)
//! printed over the unmodified UIGraph.cpp, UILineGraph.cpp and EEUIPGSLineGraph.cpp (a world of the base control,
//! the texts and the lines), replayed op for op against the Rust object; and its first lines, the C runtime's
//! `"%.2f"` of 1,961 floats (ties, the edges, NaNs and infinities, random bits) against [`format_2f`].
//!
//! The driver's own lines: `new K` (the kind), `label G W HEX` (graph G's title, y or x label), `series G R G B N
//! x y ...` (`AddGraphSeries` into graph G), `enable G S ON`, `free`, `fmt BITS TEXT`. The summary's sequences use a
//! test format ("x" and the value times 3, "y" and the value times 7, truncated); its own is ee_screens' golden.

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_graphobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiGraphCall's (the golden's); the target is the live graph
    unsafe {
        ee_ui_graph_call(
            target as *const EeUiGraph,
            core::mem::transmute::<u32, EeUiGraphCall>(call),
            a,
            b,
            0,
            f,
        )
    }
}

/// The driver's test format of the summary's values.
unsafe extern "C" fn test_format(axis: c_int, value: f32, out: *mut u8, capacity: usize) -> usize {
    let text = if axis == 0 { format!("x{}", (value * 3.0) as i32) } else { format!("y{}", (value * 7.0) as i32) };
    let n = text.len().min(capacity);
    // SAFETY: `capacity` live bytes at `out`
    unsafe { core::ptr::copy_nonoverlapping(text.as_ptr(), out, n) };
    n
}

fn hex_bytes(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn graph<'a>(obj: *mut EeUiGraph) -> &'a EeUiGraph {
    // SAFETY: made by "new" and not yet freed
    unsafe { &*obj }
}

fn replay(golden: &str) -> (usize, usize) {
    let r = Replay::new(golden, dispatch);
    let mut obj: *mut EeUiGraph = core::ptr::null_mut();
    let mut formats = 0;
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            let (kind, format): (EeUiGraphKind, EeUiGraphFormatFn) =
                if w[1] == "0" { (EeUiGraphKind::Line, None) } else { (EeUiGraphKind::PostGame, Some(test_format)) };
            obj = ee_ui_graph_new(o, h, kind, format);
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_graph_free(obj) };
            obj = core::ptr::null_mut();
            true
        }
        "label" => {
            let text = hex_bytes(w[3]);
            graph(obj)
                .graph_at(w[1].parse().unwrap(), |g| match w[2] {
                    "0" => g.title = text,
                    "1" => g.y_label = text,
                    _ => g.x_label = text,
                })
                .expect("the label's graph");
            true
        }
        "series" => {
            let c: Vec<f32> = w[2..5].iter().map(|x| f32::from_bits(u32::from_str_radix(x, 16).unwrap())).collect();
            let n: usize = w[5].parse().unwrap();
            let v: Vec<c_long> = w[6..6 + 2 * n].iter().map(|x| x.parse().unwrap()).collect();
            let points: Vec<(c_long, c_long)> = v.chunks(2).map(|p| (p[0], p[1])).collect();
            assert!(graph(obj).graph_at(w[1].parse().unwrap(), |_| ()).is_some(), "the series' graph");
            graph(obj).add_graph_series(w[1].parse().unwrap(), &points, [c[0], c[1], c[2]]);
            true
        }
        "enable" => {
            let s: usize = w[2].parse().unwrap();
            graph(obj).graph_at(w[1].parse().unwrap(), |g| g.data.set_enabled(s, w[3] == "1")).expect("the graph");
            true
        }
        "fmt" => {
            let v = f32::from_bits(u32::from_str_radix(w[1], 16).unwrap());
            assert_eq!(String::from_utf8(format_2f(v)).unwrap(), w[2], "{w:?}");
            formats += 1;
            true
        }
        _ => false,
    });
    (r.checked.get(), formats)
}

#[test]
fn golden_replays() {
    let (checked, formats) = replay(GOLDEN);
    assert!(checked > 14_000, "{checked}");
    assert_eq!(formats, 1_961);
}

/// The driver's whole run (2,000 sequences, 13 MB: not committed): `EE_GRAPHOBJ_TRACE=<file> cargo test -- --ignored`.
#[test]
#[ignore]
fn the_driver_s_whole_trace_replays() {
    let Ok(path) = std::env::var("EE_GRAPHOBJ_TRACE") else {
        return;
    };
    let text = std::fs::read_to_string(path).unwrap();
    let (checked, _) = replay(&text);
    eprintln!("{checked} lines checked");
}

fn mutated(at: &str, word: usize, value: &str) -> String {
    let start = GOLDEN.find(at).expect("the mutation's line") + 1;
    let end = start + GOLDEN[start..].find('\n').unwrap();
    let mut w: Vec<&str> = GOLDEN[start..end].split(' ').collect();
    w[word] = value;
    format!("{}{}{}", &GOLDEN[..start], w.join(" "), &GOLDEN[end..])
}

#[test]
#[should_panic(expected = "golden line")]
fn a_moved_grid_value_fails() {
    // a grid value's location expected elsewhere
    replay(&mutated("\nop 83 ", 6, "7f7f7f7f"));
}

#[test]
#[should_panic(expected = "golden line")]
fn another_line_colour_fails() {
    replay(&mutated("\nop 184 ", 6, "3e800000"));
}

#[test]
#[should_panic]
fn another_format_fails() {
    replay(&mutated("\nfmt 3e000000 ", 2, "0.13"));
}

#[test]
fn format_2f_is_the_crt_s() {
    assert_eq!(format_2f(0.0), b"0.00");
    assert_eq!(format_2f(-0.0), b"-0.00");
    assert_eq!(format_2f(0.125), b"0.12");
    assert_eq!(format_2f(0.375), b"0.38");
    assert_eq!(format_2f(f32::INFINITY), b"inf");
    assert_eq!(format_2f(f32::from_bits(0xffc0_0000)), b"-nan(ind)");
}

#[test]
fn a_line_graph_has_only_the_shown_graph() {
    let g = EeUiGraph::new(Host { obj: core::ptr::null_mut(), f: None }, EeUiGraphKind::Line, None);
    assert!(g.graph_at(EE_UI_GRAPH_SHOWN, |_| ()).is_some());
    assert!(g.graph_at(0, |_| ()).is_none());
    assert!(g.graph_at(32, |_| ()).is_none());
}

#[test]
fn the_summary_s_graphs_are_numbered_as_the_host_did() {
    let g = EeUiGraph::new(Host { obj: core::ptr::null_mut(), f: None }, EeUiGraphKind::PostGame, None);
    for i in 0..40u32 {
        let name = format!("graph {i}").into_bytes();
        g.graph_at(i, |x| x.title = name).expect("a graph");
    }
    assert!(g.graph_at(40, |_| ()).is_none());
    assert_eq!(g.graph_at(30, |x| x.title.clone()).unwrap(), b"graph 30");
    assert_eq!(g.graph_at(32, |x| x.title.clone()).unwrap(), b"graph 32");
    assert_eq!(g.graph_at(39, |x| x.title.clone()).unwrap(), b"graph 39");
    g.reset_manager_graphs();
    assert_eq!(g.graph_at(32, |x| x.title.clone()).unwrap(), b"");
    assert_eq!(g.graph_at(30, |x| x.title.clone()).unwrap(), b"graph 30");
    g.show_graph(0x17);
    assert_eq!(g.graph_at(EE_UI_GRAPH_SHOWN, |x| x.title.clone()).unwrap(), b"graph 23");
}

#[test]
fn add_graph_series_keeps_the_points_sorted_in_the_colour() {
    let g = EeUiGraph::new(Host { obj: core::ptr::null_mut(), f: None }, EeUiGraphKind::PostGame, None);
    g.add_graph_series(3, &[(20, 5), (10, 7), (10, 9)], [0.5, 0.25, 1.0]);
    g.graph_at(3, |x| {
        assert_eq!(x.data.len(), 1);
        let s = x.data.series(0);
        let p: Vec<(c_long, c_long)> = s.points().iter().map(|p| (p.x, p.y)).collect();
        assert_eq!(p, vec![(10, 7), (10, 9), (20, 5)]);
        assert_eq!(s.color(), EeUtilColor { red: 0.5, green: 0.25, blue: 1.0 });
        assert_eq!(x.data.range().max_x, 20);
    });
}
