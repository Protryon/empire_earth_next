//! Replays the original keyboard's and mouse's trace (`tests/input/is_keyboard_golden.cpp`, format in its header) on
//! the Rust keyboard and mouse: every call is made again and every dump and result line must be the golden's.
//! `golden_keyboard.txt` holds the first 40 sequences; `EE_INPUT_KEYBOARD_GOLDEN=<file>` replays a longer run.

use super::devices::{KeyKind, Keyboard, Mouse};
use super::*;

fn num<T: std::str::FromStr>(s: &str) -> T
where
    T::Err: std::fmt::Debug,
{
    s.parse().unwrap()
}

fn dump_keyboard(k: &Keyboard) -> String {
    let mut s = format!(
        "kb = {} {} {} {} {} |",
        k.modifiers(),
        k.keys_down,
        k.last_keys_down,
        k.should_update() as i32,
        k.overflowed as i32
    );
    for i in 0..256 {
        let (st, l) = (k.status[i], k.last[i]);
        if k.kind(i) != KeyKind::Key {
            if st != 0 || l != 0 {
                s += &format!(" {i}:{st},{l}");
            }
            continue;
        }
        let (v, pa, ua) = (k.value(i), k.pressed_amount(i), k.unpressed_amount(i));
        let (d, t) = (k.double[i], k.press_time[i]);
        if st != 0 || l != 0 || v != 0 || pa != 0 || ua != 0 || d || t != 0 {
            s += &format!(" {i}:{st},{l},{v},{pa},{ua},{},{t}", d as i32);
        }
    }
    s += " |";
    for i in 0xe0..=0xe2 {
        s += &format!(
            " e{i}:{},{},{},{},{},{},{}",
            k.value(i),
            k.normalized_value(i),
            k.just_pressed(i) as i32,
            k.just_unpressed(i) as i32,
            k.just_double_pressed(i) as i32,
            k.pressed_amount(i),
            k.unpressed_amount(i)
        );
    }
    s
}

fn dump_mouse(m: &Mouse) -> String {
    let (c, l) = (&m.state, &m.last_state);
    let mut s = format!(
        "ms = {} {} {} {} {} {} {} {} {} | {} {} {} {} {} {} {} | {} {} {} |",
        m.should_update() as i32,
        m.overflowed as i32,
        c.lX,
        c.lY,
        c.lZ,
        c.rgbButtons[0],
        c.rgbButtons[1],
        c.rgbButtons[2],
        c.rgbButtons[3],
        l.lX,
        l.lY,
        l.lZ,
        l.rgbButtons[0],
        l.rgbButtons[1],
        l.rgbButtons[2],
        l.rgbButtons[3],
        m.normalized_value(0),
        m.normalized_value(1),
        m.normalized_value(2)
    );
    for b in 0..4 {
        s += &format!(
            " b{b}:{},{},{},{},{},{},{}",
            m.buttons[b],
            m.last_buttons[b],
            m.value(3 + b),
            m.pressed_amount(3 + b),
            m.unpressed_amount(3 + b),
            m.just_double_pressed(3 + b) as i32,
            m.press_time[b]
        );
    }
    s
}

/// Replays one trace -> (lines checked, sequences with a differing line, the first difference).
fn replay(text: &str) -> (usize, usize, Option<String>) {
    let names = Keyboard::new(None, None, 0, 250);
    let mut devices = [Device::new(), Device::new()];
    let mut window: c_ulong = 250;
    let mut time: c_ulong = 0;
    let mut k = Keyboard::new(None, None, 0, 250);
    let mut m = Mouse::new(None);
    let (mut checked, mut failed, mut skipping, mut first) = (0, 0, false, None);
    for (number, line) in text.lines().enumerate() {
        let f: Vec<&str> = line.split(' ').collect();
        if skipping && f[0] != "seq" {
            continue;
        }
        let got: Option<String> = match f[0] {
            "name" => {
                let i: usize = num(f[2]);
                let name = if f[1] == "k" { names.name(i) } else { Mouse::name(i) };
                Some(format!("name {} {i} {}", f[1], name.map_or("?".into(), |n| n.to_string_lossy())))
            }
            "seq" => {
                window = num(f[2]);
                time = 0;
                devices = [Device::new(), Device::new()];
                skipping = false;
                None
            }
            "pre" => {
                devices[0].key(num(f[1]), true);
                devices[1].mouse_button(num(f[2]), true);
                None
            }
            "new" => {
                k = Keyboard::new(None, Some(&devices[0]), 0, window);
                m = Mouse::new(Some(&devices[1]));
                None
            }
            "kb" if f.len() == 1 => None,
            "ms" if f.len() == 1 => None,
            "kb" => Some(dump_keyboard(&k)),
            "ms" => Some(dump_mouse(&m)),
            "time" => {
                time = num::<u32>(f[1]) as c_ulong;
                None
            }
            "ev" => {
                let wheel = f32::from_bits(u32::from_str_radix(f[6], 16).unwrap());
                let (kd, md) = (Some(&devices[0]), Some(&devices[1]));
                handle_event(kd, md, num(f[1]), num(f[2]), num(f[3]), num(f[4]), num(f[5]), wheel);
                None
            }
            "key" => {
                devices[0].key(num(f[1]), f[2] == "1");
                None
            }
            "burst" => {
                for _ in 0..num::<u32>(f[1]) {
                    devices[0].key(0x1e, true);
                    devices[0].key(0x1e, false);
                }
                None
            }
            "kref" => {
                k.refresh(Some(&devices[0]), time, window);
                None
            }
            "kres" => {
                k.resume(Some(&devices[0]), time, window);
                None
            }
            "ksus" => {
                k.suspend(Some(&devices[0]));
                None
            }
            "mref" => {
                m.refresh(Some(&devices[1]), time, window);
                None
            }
            "mxy" => {
                m.refresh_xy(Some(&devices[1]));
                None
            }
            "mres" => {
                m.resume(Some(&devices[1]));
                None
            }
            "msus" => {
                m.suspend(Some(&devices[1]));
                None
            }
            "mbuf" => {
                m.set_use_buffered_movement(f[1] == "1");
                None
            }
            "kent" => {
                let (r, present) = k.entity(num(f[1]));
                Some(format!("kent {} = {r} {}", f[1], !present as i32))
            }
            "ment" => {
                let (r, present) = Mouse::entity(num(f[1]));
                Some(format!("ment {} = {r} {}", f[1], !present as i32))
            }
            other => panic!("line {}: unknown op {other}", number + 1),
        };
        if let Some(got) = got {
            if got != line {
                failed += 1;
                skipping = true;
                first.get_or_insert_with(|| format!("line {}: got {got:?}, golden {line:?}", number + 1));
            }
            checked += 1;
        }
    }
    (checked, failed, first)
}

#[test]
fn golden_keyboard_trace_replays() {
    let (checked, failed, first) = replay(include_str!("golden_keyboard.txt"));
    assert_eq!(failed, 0, "{first:?}");
    assert!(checked > 1000, "only {checked} lines checked");
}

#[test]
fn golden_keyboard_trace_replays_long_run() {
    if let Ok(path) = std::env::var("EE_INPUT_KEYBOARD_GOLDEN") {
        let text = std::fs::read_to_string(&path).unwrap();
        let (checked, failed, first) = replay(&text);
        eprintln!("{path}: {checked} lines checked, {failed} sequences differ");
        assert_eq!(failed, 0, "{first:?}");
    }
}
