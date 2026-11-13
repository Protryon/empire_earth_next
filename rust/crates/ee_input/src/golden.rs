//! Replays the original C++ class's trace (`tests/input/is_device_golden.cpp`, format in its header) on the Rust
//! devices: every call is made again and every result line must be the golden's. `golden_input.txt` holds the first
//! 40 sequences; `EE_INPUT_GOLDEN=<file>` replays a longer run (20,000 sequences in the round's check).

use super::*;

fn num<T: std::str::FromStr>(s: &str) -> T
where
    T::Err: std::fmt::Debug,
{
    s.parse().unwrap()
}

/// Replays one trace -> (result lines checked, sequences with a differing line, the first difference). A sequence
/// stops being checked at its first difference.
fn replay(text: &str) -> (usize, usize, Option<String>) {
    let mut devices = [Device::new(), Device::new()];
    let mut system = (true, true);
    let mut checked = 0;
    let mut failed = 0;
    let mut skipping = false;
    let mut first = None;
    for (number, line) in text.lines().enumerate() {
        let f: Vec<&str> = line.split(' ').collect();
        let d = |i: usize| -> usize { num(f[i]) };
        if skipping && f[0] != "seq" {
            continue;
        }
        let got: Option<String> = match f[0] {
            "seq" => {
                devices = [Device::new(), Device::new()];
                system = (f[2] == "1", f[3] == "1");
                skipping = false;
                None
            }
            "acq" => {
                devices[d(1)].acquire();
                None
            }
            "unacq" => {
                devices[d(1)].unacquire();
                None
            }
            "isacq" => Some(format!("isacq {} = {}", f[1], devices[d(1)].is_acquired() as i32)),
            "data" => {
                let count: c_ulong = num(f[2]);
                let null = f[3] == "1";
                let mut out = vec![EeInputRecord::default(); 400];
                let (result, n) = devices[d(1)].get_data(if null { None } else { Some(&mut out) }, count);
                let mut s = format!("data {} {} {} = {} {}", f[1], count, f[3], result, n);
                if !null {
                    for r in &out[..n as usize] {
                        // the trace prints 32 bits; `unsigned long` is 64-bit on LP64 systems
                        #[allow(clippy::unnecessary_cast)]
                        let (ofs, data) = (r.dwOfs as u32, r.dwData as u32);
                        s += &format!(" {ofs:x}:{data:x}");
                    }
                }
                Some(s)
            }
            "keys" => {
                let mut keys = [0x55u8; 256];
                let result = devices[d(1)].get_key_state(&mut keys);
                let mut s = format!("keys {} = {}", f[1], result);
                if result == INPUT_OK {
                    for (k, &v) in keys.iter().enumerate() {
                        if v == 0x80 {
                            s += &format!(" {k}");
                        } else if v != 0 {
                            s += &format!(" !{k}={v}");
                        }
                    }
                }
                Some(s)
            }
            "mouse" => {
                let mut m = EeInputMouseState { lX: 0x5555, lY: 0x5555, lZ: 0x5555, rgbButtons: [0x55; 4] };
                let result = devices[d(1)].get_mouse_state(&mut m);
                Some(if result == INPUT_OK {
                    format!(
                        "mouse {} = {} {} {} {} {} {} {} {}",
                        f[1],
                        result,
                        m.lX,
                        m.lY,
                        m.lZ,
                        m.rgbButtons[0],
                        m.rgbButtons[1],
                        m.rgbButtons[2],
                        m.rgbButtons[3]
                    )
                } else {
                    format!("mouse {} = {}", f[1], result)
                })
            }
            "key" => {
                devices[d(1)].key(num(f[2]), f[3] == "1");
                None
            }
            "move" => {
                devices[d(1)].mouse_move(num(f[2]), num(f[3]));
                None
            }
            "wheel" => {
                devices[d(1)].mouse_wheel(num(f[2]));
                None
            }
            "btn" => {
                devices[d(1)].mouse_button(num(f[2]), f[3] == "1");
                None
            }
            "burst" => {
                for _ in 0..num::<u32>(f[2]) {
                    devices[d(1)].mouse_move(1, -1);
                }
                None
            }
            "ev" => {
                let wheel = f32::from_bits(u32::from_str_radix(f[6], 16).unwrap());
                let keyboard = if system.0 { Some(&devices[0]) } else { None };
                let mouse = if system.1 { Some(&devices[1]) } else { None };
                handle_event(keyboard, mouse, num(f[1]), num(f[2]), num(f[3]), num(f[4]), num(f[5]), wheel);
                None
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
fn golden_trace_replays() {
    let (checked, failed, first) = replay(include_str!("golden_input.txt"));
    assert_eq!(failed, 0, "{first:?}");
    assert!(checked > 500, "only {checked} lines checked");
}

#[test]
fn golden_trace_replays_long_run() {
    if let Ok(path) = std::env::var("EE_INPUT_GOLDEN") {
        let text = std::fs::read_to_string(&path).unwrap();
        let (checked, failed, first) = replay(&text);
        eprintln!("{path}: {checked} result lines checked, {failed} sequences differ");
        assert_eq!(failed, 0, "{first:?}");
    }
}
