//! The controls table object's tests: its golden (the round's base UIControlsTable over random call sequences,
//! `build/ui18`), the first sequences replayed, and changed answers that must fail. Its two movie controls (type 0x1d:
//! op 491, the bitmap colour and opacity sets on the control, its address returned) became no control when movies
//! left the game (2026-11-17): the ops taken out, the result 0.

use super::*;
use crate::obj::replay::{host_current, Replay};
use std::collections::BTreeMap;

const GOLDEN: &str = include_str!("golden_tableobj.txt");

/// The driver's manager.
const MANAGER: isize = 0x7100;

/// The replays share the static array: one at a time.
static SERIAL: Mutex<()> = Mutex::new(());

std::thread_local! {
    /// The C++ tables the golden named -> the Rust tables.
    static TABLE_OF: core::cell::RefCell<BTreeMap<isize, usize>> = const { core::cell::RefCell::new(BTreeMap::new()) };
}

/// Notes every table in the array by its C++ table.
fn refresh() {
    let all: Vec<usize> = tables().clone();
    TABLE_OF.with(|m| {
        let mut m = m.borrow_mut();
        for p in all.into_iter().filter(|&p| p != 0) {
            m.insert(at(p).cpp(), p);
        }
    });
}

fn table(cpp: isize) -> &'static EeUiControlsTable {
    at(TABLE_OF.with(|m| *m.borrow().get(&cpp).expect("a table the golden made")))
}

fn fnv(bytes: &[u8]) -> isize {
    let mut h: u32 = 2166136261;
    for &b in bytes {
        h = (h ^ b as u32).wrapping_mul(16777619);
    }
    h as isize
}

/// The driver's digest of a table's data (its `Digest`: the printf line hashed).
fn digest(t: &EeUiControlsTable) -> isize {
    let d = t.data();
    let x = d.fields;
    let l = d.layout;
    let h = |v: f32| format!("{:08x}", v.to_bits());
    let b = |v: bool| if v { "1" } else { "0" };
    let mut s = format!(
        "{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}{}{}{}{}{}{}{}{}{}{} {} {} {} {} {} {} {} {}",
        h(x.background_opacity),
        h(x.text_box_opacity),
        h(x.scroll_bar_bg_opacity),
        x.unknown100,
        x.id,
        x.form_id,
        x.group,
        x.control_type,
        x.form_index,
        x.border_thickness,
        x.string_table_id,
        x.font_table_index,
        x.blink_rate,
        x.string_table_begin,
        x.string_table_end,
        x.text_alignment,
        x.max_chars,
        x.start_current_selection,
        x.start_range,
        x.end_range,
        x.current_range,
        x.page_size,
        b(x.is_visible),
        b(x.start_checked),
        b(x.list_box_always_dirty),
        b(x.sort_list_box),
        b(x.focused_has_alpha),
        b(x.pressed_has_alpha),
        b(x.normal_has_alpha),
        b(x.always_dirty),
        b(x.fit_rect_to_text),
        b(x.scale_to_control),
        b(x.loop_movie),
        h(l.left_offset),
        h(l.right_offset),
        h(l.top_offset),
        h(l.bottom_offset),
        l.left_type,
        l.right_type,
        l.top_type,
        l.bottom_type
    );
    for r in [d.normal_rect, d.pressed_rect, d.focused_rect] {
        s += &format!(" {} {} {} {}", h(r.bottom), h(r.left), h(r.right), h(r.top));
    }
    for c in [d.background, d.text_foreground, d.selected_list_box, d.focused_line] {
        s += &format!(" {} {} {}", h(c.red), h(c.green), h(c.blue));
    }
    let mut bytes = s.into_bytes();
    for i in 0..5 {
        bytes.extend_from_slice(b" [");
        bytes.extend_from_slice(t.with(|s| s.names[i].clone()).split_last().expect("NUL-terminated").1);
        bytes.push(b']');
    }
    bytes.extend_from_slice(format!(" {}", d.font as isize).as_bytes());
    fnv(&bytes)
}

unsafe fn dispatch(_target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    refresh();
    let host: EeUiHostFn = Some(host_current);
    // SAFETY: four live floats (the replay's buffer)
    unsafe {
        match call {
            1 => table(a).call(EeUiTableCall::CreateControl, MANAGER, 0, f),
            2 => table(a).call(EeUiTableCall::CreateControlEvent, b, 0, f),
            3 => table(a).call(EeUiTableCall::RelayoutControl, b, MANAGER, f),
            4 => table(a).call(EeUiTableCall::SetupFont, MANAGER, b, f),
            5 => table(a).call(EeUiTableCall::AddEvent, b as u64 as u32 as isize, (b as u64 >> 32) as isize, f),
            11 => ee_ui_controls_tables(EeUiTablesCall::Create, a, MANAGER, host, f),
            12 => {
                let v = read_buf(f);
                let l = EeUiTableLike {
                    template_id: (a & 0xffff) as c_long,
                    id: ((a >> 16) & 0xffff) as c_long,
                    form_index: ((a >> 32) & 0xffff) as c_long,
                    group: ((a >> 48) & 0xffff) as c_ulong,
                    left: v[0],
                    right: v[1],
                    top: v[2],
                    bottom: v[3],
                    string_table_begin: (b & 0xffff) as u16 as i16 as c_long,
                    string_table_end: ((b >> 16) & 0xffff) as u16 as i16 as c_long,
                    help_string_id: ((b >> 32) & 0xffff) as c_ulong,
                };
                ee_ui_controls_tables(
                    EeUiTablesCall::AddControlLike,
                    MANAGER,
                    &l as *const EeUiTableLike as isize,
                    None,
                    f,
                )
            }
            13 => ee_ui_controls_tables(EeUiTablesCall::MoveControlsHorizontally, a, b, None, f),
            14 => ee_ui_controls_tables(EeUiTablesCall::DeleteAll, 0, 0, None, f),
            15 => ee_ui_controls_tables(EeUiTablesCall::At, a, 0, None, f),
            20 => digest(table(a)),
            21 => {
                let p = TABLE_OF.with(|m| m.borrow_mut().remove(&a).expect("a table the golden made"));
                ee_ui_controls_table_free(p as *mut EeUiControlsTable);
                0
            }
            _ => panic!("call {call}"),
        }
    }
}

/// Replays a golden -> the lines checked.
fn run(golden: &str) -> usize {
    let _serial = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    // a failed replay before this one may have left tables (leaked)
    tables().clear();
    TABLE_OF.with(|m| m.borrow_mut().clear());
    let replay = Replay::new(golden, dispatch);
    replay.run(|_, _| false);
    assert!(tables().is_empty(), "DeleteAll left tables");
    replay.checked.get()
}

#[test]
fn replays_the_tables_golden() {
    let checked = run(GOLDEN);
    assert!(checked > 3_000, "{checked} lines checked");
}

#[test]
#[should_panic]
fn a_changed_record_count_fails() {
    // the first database file's record count, one more
    let i = GOLDEN.find("\nop 471 ").expect("a record count") + 1;
    let i = i + GOLDEN[i..].find("\n= ").expect("its answer") + 1;
    let line_end = i + GOLDEN[i..].find('\n').unwrap();
    let w: Vec<&str> = GOLDEN[i..line_end].split(' ').collect();
    let packed: i64 = w[1].parse().unwrap();
    let changed = format!("= {} {}", packed + (1 << 32), w[2..].join(" "));
    run(&format!("{}{}{}", &GOLDEN[..i], changed, &GOLDEN[line_end..]));
}

#[test]
#[should_panic]
fn a_changed_window_fails() {
    // every window width the world gave, one pixel narrower (the controls' rectangles follow)
    let changed: Vec<String> = GOLDEN
        .lines()
        .map(|l| match l.strip_prefix("set 460 0 ") {
            Some(rest) => {
                let w: Vec<&str> = rest.split(' ').collect();
                format!("set 460 0 {} {}", w[0].parse::<i64>().unwrap() - 1, w[1..].join(" "))
            }
            None => l.to_string(),
        })
        .collect();
    run(&changed.join("\n"));
}

#[test]
fn the_default_table_is_an_empty_bitmap_description() {
    let t = EeUiControlsTable::blank(Host { obj: core::ptr::null_mut(), f: None });
    let d = t.data();
    assert_eq!(d.fields.control_type, ty::BITMAP);
    assert!(d.font.is_null());
    for i in 0..5 {
        assert!(!t.has_name(i));
        // SAFETY: the names are NUL-terminated
        assert_eq!(unsafe { core::ffi::CStr::from_ptr(d.names[i]) }.to_bytes(), b"");
    }
}

#[test]
fn names_stop_at_the_nul_or_the_field() {
    assert_eq!(name(b"ui/a.tga\0junk"), b"ui/a.tga\0");
    assert_eq!(name(&[b'x'; 4]), b"xxxx\0");
}
