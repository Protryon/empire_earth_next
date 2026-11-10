//! The controls table against the real C++ (`golden_controls.txt`): a scratch driver built over the original
//! `UIControlsTable.cpp` (traced fakes for the control classes, the manager, forms, fonts, textures, bitmaps and the
//! string table) printed 48 random database records made into tables, 24 `AddControlLike` calls, 10
//! `MoveControlsHorizontally` calls and, per table, `SetupFont`, `SetupStringArray`, `CreateControl`,
//! `CreateControlEvent` and `RelayoutControl`. The replay below runs the arithmetic that moved here and checks every
//! value it produces in that trace: the tables' fields, layouts, colours and texture rectangles, the searches'
//! results, the font sizes, the string counts, and the geometry each control is constructed with (as an ordered
//! subsequence of each `create`/`relayout` block; the rest of a block is the C++ glue, which the same driver over the
//! forwarders prints identically).

use super::*;

const GOLDEN: &str = include_str!("../golden_controls.txt");

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn rect(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}
fn colour(c: &UiColor) -> String {
    format!("{} {} {}", h(c.red), h(c.green), h(c.blue))
}

/// The driver's `Dump`, up to the `UString` names (C++'s).
fn dump(i: usize, t: &Table) -> String {
    let f = &t.fields;
    let l = &t.layout;
    let bools: String = [
        f.is_visible,
        f.start_checked,
        f.list_box_always_dirty,
        f.sort_list_box,
        f.focused_has_alpha,
        f.pressed_has_alpha,
        f.normal_has_alpha,
        f.always_dirty,
        f.fit_rect_to_text,
        f.scale_to_control,
        f.loop_movie,
    ]
    .iter()
    .map(|&b| if b { '1' } else { '0' })
    .collect();
    format!(
        "table {i} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {bools} layout {} {} {} {} {} {} {} {} \
         colors {} | {} | {} | {} rects {} | {} | {}",
        h(f.background_opacity),
        h(f.text_box_opacity),
        h(f.scroll_bar_bg_opacity),
        f.unknown100,
        f.id,
        f.form_id,
        f.group,
        f.control_type,
        f.form_index,
        f.border_thickness,
        f.string_table_id,
        f.font_table_index,
        f.blink_rate,
        f.string_table_begin,
        f.string_table_end,
        f.text_alignment,
        f.max_chars,
        f.start_current_selection,
        f.start_range,
        f.end_range,
        f.current_range,
        f.page_size,
        h(l.left_offset),
        h(l.right_offset),
        h(l.top_offset),
        h(l.bottom_offset),
        l.left_type,
        l.right_type,
        l.top_type,
        l.bottom_type,
        colour(&t.background),
        colour(&t.text_foreground),
        colour(&t.selected_list_box),
        colour(&t.focused_line),
        rect(&t.normal_rect),
        rect(&t.pressed_rect),
        rect(&t.focused_rect),
    )
}

fn record(hex: &str) -> DbControl {
    let bytes: Vec<u8> = (0..hex.len() / 2).map(|k| u8::from_str_radix(&hex[2 * k..2 * k + 2], 16).unwrap()).collect();
    assert_eq!(bytes.len(), core::mem::size_of::<DbControl>());
    // SAFETY: the record is plain integers, floats and bytes; any bit pattern is a value
    unsafe { core::ptr::read_unaligned(bytes.as_ptr() as *const DbControl) }
}

/// The static array, as the driver's.
struct Tables(Vec<Option<Table>>);

impl TablesHost for Tables {
    fn len(&self) -> usize {
        self.0.len()
    }
    fn key(&self, i: usize) -> Option<TableKey> {
        self.0[i].map(|t| TableKey { id: t.fields.id, form_id: t.fields.form_id, form_index: t.fields.form_index })
    }
    fn layout(&self, i: usize) -> Layout {
        self.0[i].unwrap().layout
    }
    fn set_layout(&mut self, i: usize, layout: &Layout) {
        self.0[i].as_mut().unwrap().layout = *layout;
    }
}

/// The driver's forms: exclusive coordinates, GDI text, the designed height.
fn form(id: c_long) -> (bool, bool, c_ulong) {
    match id {
        1 => (false, false, 768),
        2 => (true, true, 600),
        _ => (true, false, 480),
    }
}

/// The driver's fake font table: `GetFont(i)` a font 8 + 3i tall with a bitmap when i is odd; `CreateClosestFont`
/// one `size` tall with a bitmap when bit 1 of i is set. A font is one (height, has bitmap).
type Font = Option<(c_long, bool)>;

/// `SetupFont` as the C++ runs it over the fakes: the trace lines, the font kept.
fn setup_font(
    t: &Table,
    font: &mut Font,
    r: &UiRect,
    scale_to_window: bool,
    window_height: c_ulong,
    out: &mut Vec<String>,
) {
    let f = &t.fields;
    let (_, gdi, designed) = form(f.form_id);
    if let Some((old, _)) = font.take() {
        out.push(format!("  font-free {old}"));
    }
    if f.font_table_index > 0 {
        let idx = f.font_table_index;
        if f.scale_to_control {
            let size = if !font_scales_to_window(f.control_type, scale_to_window) {
                control_font_size(r.height(), f.border_thickness)
            } else {
                out.push(format!("  getfont {idx}"));
                let fh = 8 + 3 * idx as c_long;
                let size = window_font_size(fh, window_height, designed);
                out.push(format!("  font-free {fh}"));
                size
            };
            out.push(format!("  closest {idx} {}", size as u32));
            *font = Some((size, idx & 2 != 0));
        } else {
            out.push(format!("  getfont {idx}"));
            *font = Some((8 + 3 * idx as c_long, idx & 1 != 0));
        }
    } else {
        *font = None;
    }
    if !gdi {
        if let Some((fh, false)) = *font {
            out.push(format!("  fontbitmap {fh}"));
        }
    }
}

/// `CreateControl` as the C++ runs it over the fakes: the lines carrying the geometry (the control's rectangle, a
/// combo box's list height, a file combo box's size, a scroll bar's length, a slider's page size); the class the
/// driver's fakes name.
fn create(t: &Table, font: &mut Font, window: (c_ulong, c_ulong), out: &mut Vec<String>) {
    let f = &t.fields;
    let (exclusive, _, _) = form(f.form_id);
    let r = t.layout.calculate(&parent_rect(window.0, window.1, exclusive), exclusive);
    setup_font(t, font, &font_rect(&r, exclusive), false, window.1, out);
    let ctl =
        |class: &str, r: &UiRect| format!("  ctl {class} {} {} index {}", f.id as u32, rect(r), f.form_index as u32);
    let class = match f.control_type {
        0 => "UIBitmap",
        1 => "UIButton",
        27 => "UIBlinkingButton",
        28 => "UIScalableButton",
        31 => "UIDualButton",
        2 | 11 | 12 => "UICheckBox",
        3 | 4 => {
            let (list, height) = combo_list(&r, f.scale_to_control, font.map_or(0, |f| f.0), f.border_thickness);
            out.push(format!("{}|| list-height {height} ", ctl("UIComboBox", &list)));
            ""
        }
        30 => "UILineGraph",
        5..=8 => "UIEditBox",
        9 | 10 => "UILBString",
        13 => "UIStatusBar",
        14 | 19 => {
            let len = scroll_bar_length(&r, f.control_type == 19);
            let fake = UiRect::new(r.left, r.top, r.left + len, r.top + len);
            out.push(format!(
                "{} at {} {} length {} style {}",
                ctl("UIScrollBar", &fake),
                h(r.left),
                h(r.top),
                h(len),
                if f.control_type == 19 { 1 } else { 0 }
            ));
            ""
        }
        15 | 16 | 20 | 21 | 24 | 25 => "UITextLabel",
        17 | 18 => {
            out.push(ctl("UISlider", &r));
            out.push(format!("  page {}", slider_page_size(f.start_range, f.end_range, f.page_size)));
            ""
        }
        22 => "UIFileListBox",
        32 => {
            let (w, height) = file_combo_size(&r);
            let fake = UiRect::new(r.left, r.top, r.left + w, r.top + height as f32);
            out.push(ctl("UIFileComboBox", &fake));
            out.push(format!("  file-combo at {} {} width {} height {height}", h(r.left), h(r.top), h(w)));
            ""
        }
        26 => "UITLScrolling",
        kind if kind >= 1000 => {
            out.push(format!("  user {kind}"));
            out.push(ctl("user", &UiRect::default()));
            ""
        }
        _ => {
            out.push("  = null font -1".to_string());
            return;
        }
    };
    if !class.is_empty() {
        out.push(ctl(class, &r));
    }
    let shown = match f.control_type {
        3 | 4 => "UIComboBox",
        14 | 19 => "UIScrollBar",
        17 | 18 => "UISlider",
        32 => "UIFileComboBox",
        kind if kind >= 1000 => "user",
        _ => class,
    };
    out.push(format!("  = {shown} font -1"));
}

/// `RelayoutControl` as the C++ runs it over the fakes.
fn relayout(t: &Table, font: &mut Font, window: (c_ulong, c_ulong), out: &mut Vec<String>) {
    let (exclusive, _, _) = form(t.fields.form_id);
    let r = t.layout.calculate(&parent_rect(window.0, window.1, exclusive), exclusive);
    out.push(format!("  setrect {}", rect(&r)));
    setup_font(t, font, &r, false, window.1, out);
    out.push(format!("  changefont {}", font.map_or(-1, |(fh, _)| fh)));
    out.push("  = font -1".to_string());
}

/// `expected` appears in `block` in order; a `ctl` line matches as a prefix (the fakes print the controls' other
/// arguments after the geometry), and what follows a `||` in it must appear later in the same line.
fn subsequence(block: &[&str], expected: &[String], what: &str) {
    let mut k = 0;
    for line in block {
        if k == expected.len() {
            break;
        }
        let e = &expected[k];
        let hit = if let Some((prefix, inside)) = e.split_once("||") {
            line.starts_with(prefix) && line.contains(inside)
        } else if e.starts_with("  ctl ") {
            line.starts_with(e.as_str())
        } else {
            *line == e
        };
        if hit {
            k += 1;
        }
    }
    assert_eq!(k, expected.len(), "{what}: missing {:?} in {:#?}", expected.get(k), block);
}

#[test]
fn replays_the_cpp_controls_table() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    // (header line, its indented body)
    let mut blocks: Vec<(&str, Vec<&str>)> = Vec::new();
    for line in &lines {
        if line.starts_with("  ") {
            if let Some(b) = blocks.last_mut() {
                b.1.push(line);
            }
        } else {
            blocks.push((line, Vec::new()));
        }
    }
    let mut tables = Tables(Vec::new());
    let mut fonts: Vec<Font> = Vec::new();
    let mut window = (0, 0);
    let mut expect_table: Option<usize> = None;
    let mut checked = [0usize; 8];
    for (header, body) in &blocks {
        let t: Vec<&str> = header.split(' ').collect();
        match t[0] {
            "record" => {
                let r = record(t[2]);
                let i: usize = t[1].parse().unwrap();
                assert_eq!(i, tables.0.len());
                if record_is_control(r.id) {
                    tables.0.push(Some(from_record(&r)));
                    expect_table = Some(i);
                } else {
                    tables.0.push(None);
                }
                fonts.push(None);
            }
            "skipped" => {
                let i: usize = t[1].parse().unwrap();
                assert!(tables.0[i].is_none(), "record {i} kept");
                checked[0] += 1;
            }
            "table" => {
                let i: usize = t[1].parse().unwrap();
                assert_eq!(expect_table.take(), Some(i), "an unexpected table dump");
                let want = &header[..header.find(" names ").unwrap()];
                assert_eq!(dump(i, tables.0[i].as_ref().unwrap()), want, "table {i}");
                checked[1] += 1;
            }
            "like" => {
                let n: Vec<c_long> = [1, 2, 3, 9, 10].iter().map(|&k| t[k].parse().unwrap()).collect();
                let group: c_ulong = t[4].parse().unwrap();
                let help: c_ulong = t[11].parse().unwrap();
                let found = find_template(&tables, n[0], n[1], n[2]);
                let result = if found.is_some() { 0 } else { 3 };
                if let Some(ti) = found {
                    let mut copy = tables.0[ti].unwrap();
                    like(
                        &mut copy.fields,
                        &mut copy.layout,
                        n[1],
                        n[2],
                        group,
                        fx(t[5]),
                        fx(t[6]),
                        fx(t[7]),
                        fx(t[8]),
                        n[3],
                        n[4],
                        help,
                    );
                    tables.0.push(Some(copy));
                    fonts.push(None);
                    expect_table = Some(tables.0.len() - 1);
                }
                let shown = body.iter().find(|l| l.starts_with("  = ")).unwrap();
                assert_eq!(*shown, format!("  = {result} size {}", tables.0.len()), "{header}");
                checked[2] += 1;
            }
            "move" => {
                let n: Vec<c_long> = (1..=3).map(|k| t[k].parse().unwrap()).collect();
                let count = move_horizontally(&mut tables, n[0], n[1], n[2], fx(t[4]), fx(t[5]));
                let mut want = vec![format!("  = {count}")];
                for (i, table) in tables.0.iter().enumerate() {
                    if let Some(table) = table {
                        let l = &table.layout;
                        want.push(format!(
                            "  layout {i} {} {} {} {}",
                            h(l.left_offset),
                            h(l.right_offset),
                            l.left_type,
                            l.right_type
                        ));
                    }
                }
                assert_eq!(*body, want, "{header}");
                checked[3] += 1;
            }
            "window" => window = (t[1].parse().unwrap(), t[2].parse().unwrap()),
            "font" => {
                let i: usize = t[1].parse().unwrap();
                let r = UiRect { left: fx(t[2]), top: fx(t[3]), right: fx(t[4]), bottom: fx(t[5]) };
                let mut want = Vec::new();
                setup_font(tables.0[i].as_ref().unwrap(), &mut fonts[i], &r, t[6] == "1", window.1, &mut want);
                want.push(format!("  = {}", fonts[i].map_or(-1, |(fh, _)| fh)));
                assert_eq!(*body, want, "{header}");
                checked[4] += 1;
            }
            "strings" => {
                let i: usize = t[1].parse().unwrap();
                let f = &tables.0[i].unwrap().fields;
                let count = string_count(f.string_table_begin, f.string_table_end);
                let mut want = format!("strings {i} = {count}");
                for k in 0..count {
                    want.push_str(&format!(" s{}", f.string_table_begin + k as c_long));
                }
                assert_eq!(*header, want);
                checked[5] += 1;
            }
            "create" => {
                let i: usize = t[1].parse().unwrap();
                let mut want = Vec::new();
                create(tables.0[i].as_ref().unwrap(), &mut fonts[i], window, &mut want);
                // CreateControl releases the font
                fonts[i] = None;
                subsequence(body, &want, header);
                checked[6] += 1;
            }
            "events" => {}
            "relayout" => {
                let i: usize = t[1].parse().unwrap();
                window = (t[3].parse().unwrap(), t[4].parse().unwrap());
                let mut want = Vec::new();
                relayout(tables.0[i].as_ref().unwrap(), &mut fonts[i], window, &mut want);
                fonts[i] = None;
                subsequence(body, &want, header);
                checked[7] += 1;
            }
            "end" => assert_eq!(t[1], "0"),
            _ => panic!("unknown line {header}"),
        }
    }
    assert_eq!(checked, [8, 48, 24, 10, 48, 48, 48, 43], "the golden's shape");
}

#[test]
fn colours_scale_by_one_255th() {
    let c = color(255, 0, 128);
    assert_eq!(c.red, 1.0);
    assert_eq!(c.green, 0.0);
    assert_eq!(c.blue, 128.0 * (1.0f32 / 255.0));
}

#[test]
fn the_default_table_is_an_invisible_bitmap() {
    let f = init();
    assert_eq!(f.control_type, BITMAP);
    assert!(!f.is_visible);
    assert_eq!(f.string_table_id, 0);
}

#[test]
fn add_control_like_refuses_a_taken_id_or_index() {
    let mut a = from_record(&record(&"00".repeat(860)));
    a.fields.id = 10;
    a.fields.form_id = 1;
    a.fields.form_index = 3;
    let mut b = a;
    b.fields.id = 11;
    b.fields.form_index = 4;
    let tables = Tables(vec![Some(a), None, Some(b)]);
    assert_eq!(find_template(&tables, 10, 20, 7), Some(0));
    assert_eq!(find_template(&tables, 11, 20, 7), Some(2));
    assert_eq!(find_template(&tables, 10, 11, 7), None, "the id is taken");
    assert_eq!(find_template(&tables, 10, 20, 4), None, "the form index is taken");
    assert_eq!(find_template(&tables, 12, 20, 7), None, "no template");
}

#[test]
fn like_sets_a_proportional_rectangle_and_clears_the_selection() {
    let mut f = init();
    f.start_current_selection = 3;
    let mut l = Layout::default();
    like(&mut f, &mut l, 5, 6, 7, 0.1, 0.2, 0.3, 0.4, 8, 9, 10);
    assert_eq!(
        (f.id, f.form_index, f.group, f.string_table_begin, f.string_table_end, f.string_table_id),
        (5, 6, 7, 8, 9, 10)
    );
    assert_eq!(f.start_current_selection, 0);
    assert_eq!(l.left_type, PROPORTIONAL);
    assert_eq!((l.left_offset, l.right_offset, l.top_offset, l.bottom_offset), (0.1, 0.2, 0.3, 0.4));
}

#[test]
fn move_horizontally_takes_proportional_edges_of_the_form_range() {
    let mut a = from_record(&record(&"00".repeat(860)));
    a.fields.form_id = 2;
    a.fields.form_index = 5;
    a.layout =
        Layout { left_type: PROPORTIONAL, right_type: PROPORTIONAL, left_offset: 0.5, right_offset: 0.6, ..a.layout };
    let mut b = a;
    b.layout.left_type = 0;
    let mut c = a;
    c.fields.form_index = 9;
    let mut tables = Tables(vec![Some(a), Some(b), None, Some(c)]);
    assert_eq!(move_horizontally(&mut tables, 2, 4, 6, 0.1, -0.1), 1);
    assert_eq!(tables.layout(0).left_offset, 0.6);
    assert_eq!(tables.layout(0).right_offset, 0.5);
    assert_eq!(tables.layout(1).left_offset, 0.5, "a pixel edge stays");
    assert_eq!(tables.layout(3).left_offset, 0.5, "outside the index range");
    assert_eq!(move_horizontally(&mut tables, 3, 0, 99, 1.0, 1.0), 0, "another form");
}

#[test]
fn font_sizes_and_the_window_rule() {
    assert!(font_scales_to_window(COMBO_BOX, false));
    assert!(font_scales_to_window(1, true));
    assert!(!font_scales_to_window(1, false));
    assert_eq!(control_font_size(30.0, 2), 26);
    assert_eq!(control_font_size(-1.5, 0), -2);
    assert_eq!(window_font_size(17, 720, 768), 15);
    assert_eq!(window_font_size(11, 480, 480), 11);
}

#[test]
fn the_geometry_helpers() {
    assert_eq!(string_count(2, 4), 3);
    assert_eq!(string_count(0, 4), 0);
    assert_eq!(string_count(5, 4), 0);
    assert_eq!(parent_rect(640, 480, false), UiRect::new(0.0, 0.0, 639.0, 479.0));
    assert_eq!(parent_rect(640, 480, true), UiRect::new(0.0, 0.0, 640.0, 480.0));
    let r = UiRect::new(10.0, 20.0, 110.0, 120.0);
    assert_eq!(font_rect(&r, true), UiRect::new(10.0, 20.0, 111.0, 121.0));
    assert_eq!(font_rect(&r, false), r);
    assert_eq!(combo_list(&r, false, 0, 0), (UiRect::new(10.0, 20.0, 110.0, 40.0), 80));
    assert_eq!(combo_list(&r, true, 14, 2), (UiRect::new(10.0, 20.0, 110.0, 38.0), 82));
    assert_eq!(combo_list(&UiRect::new(0.0, 0.0, 50.0, 10.0), false, 0, 0).1, 1, "a short control: one line");
    assert_eq!(file_combo_size(&r), (101.0, 101));
    assert_eq!(scroll_bar_length(&r, true), 101.0);
    assert_eq!(scroll_bar_length(&r, false), 101.0);
    assert_eq!(slider_page_size(0, 10, 4), 4);
    assert_eq!(slider_page_size(0, 10, 10), 9);
    assert_eq!(slider_page_size(5, 5, 0), -1);
}
