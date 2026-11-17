//! The key tables and key-state logic of the window layer: SDL scancodes (USB usage ids, layout-independent) map to
//! DirectInput scan codes by a table; virtual-key codes come from the scancode for the keys without a character and
//! from the layout's keycode for letters, digits and punctuation (as Windows derives VK_* from the active layout).
//! The numbers are `include/platform/PlatKeys.h`'s and SDL3's.

/// One row of the table: `(SDL_Scancode, DIK_*, VK_*)`.
#[derive(Clone, Copy)]
pub struct KeyMap {
    pub scan: i32,
    pub dik: u8,
    pub vk: u8,
}

const fn k(scan: i32, dik: u8, vk: u8) -> KeyMap {
    KeyMap { scan, dik, vk }
}

// Generated from the C++ table (PlatWindow.cpp kKeys), the SDL3 and PlatKeys.h values; the order is the C++'s.
#[rustfmt::skip]
pub const KEYS: [KeyMap; 118] = [
    k(41, 0x01, 0x1B), // ESCAPE, DIK_ESCAPE, VK_ESCAPE
    k(30, 0x02, 0x31), // 1, DIK_1, '1'
    k(31, 0x03, 0x32), // 2, DIK_2, '2'
    k(32, 0x04, 0x33), // 3, DIK_3, '3'
    k(33, 0x05, 0x34), // 4, DIK_4, '4'
    k(34, 0x06, 0x35), // 5, DIK_5, '5'
    k(35, 0x07, 0x36), // 6, DIK_6, '6'
    k(36, 0x08, 0x37), // 7, DIK_7, '7'
    k(37, 0x09, 0x38), // 8, DIK_8, '8'
    k(38, 0x0A, 0x39), // 9, DIK_9, '9'
    k(39, 0x0B, 0x30), // 0, DIK_0, '0'
    k(45, 0x0C, 0xBD), // MINUS, DIK_MINUS, VK_OEM_MINUS
    k(46, 0x0D, 0xBB), // EQUALS, DIK_EQUALS, VK_OEM_PLUS
    k(42, 0x0E, 0x08), // BACKSPACE, DIK_BACK, VK_BACK
    k(43, 0x0F, 0x09), // TAB, DIK_TAB, VK_TAB
    k(20, 0x10, 0x51), // Q, DIK_Q, 'Q'
    k(26, 0x11, 0x57), // W, DIK_W, 'W'
    k(8, 0x12, 0x45), // E, DIK_E, 'E'
    k(21, 0x13, 0x52), // R, DIK_R, 'R'
    k(23, 0x14, 0x54), // T, DIK_T, 'T'
    k(28, 0x15, 0x59), // Y, DIK_Y, 'Y'
    k(24, 0x16, 0x55), // U, DIK_U, 'U'
    k(12, 0x17, 0x49), // I, DIK_I, 'I'
    k(18, 0x18, 0x4F), // O, DIK_O, 'O'
    k(19, 0x19, 0x50), // P, DIK_P, 'P'
    k(47, 0x1A, 0xDB), // LEFTBRACKET, DIK_LBRACKET, VK_OEM_4
    k(48, 0x1B, 0xDD), // RIGHTBRACKET, DIK_RBRACKET, VK_OEM_6
    k(40, 0x1C, 0x0D), // RETURN, DIK_RETURN, VK_RETURN
    k(224, 0x1D, 0xA2), // LCTRL, DIK_LCONTROL, VK_LCONTROL
    k(4, 0x1E, 0x41), // A, DIK_A, 'A'
    k(22, 0x1F, 0x53), // S, DIK_S, 'S'
    k(7, 0x20, 0x44), // D, DIK_D, 'D'
    k(9, 0x21, 0x46), // F, DIK_F, 'F'
    k(10, 0x22, 0x47), // G, DIK_G, 'G'
    k(11, 0x23, 0x48), // H, DIK_H, 'H'
    k(13, 0x24, 0x4A), // J, DIK_J, 'J'
    k(14, 0x25, 0x4B), // K, DIK_K, 'K'
    k(15, 0x26, 0x4C), // L, DIK_L, 'L'
    k(51, 0x27, 0xBA), // SEMICOLON, DIK_SEMICOLON, VK_OEM_1
    k(52, 0x28, 0xDE), // APOSTROPHE, DIK_APOSTROPHE, VK_OEM_7
    k(53, 0x29, 0xC0), // GRAVE, DIK_GRAVE, VK_OEM_3
    k(225, 0x2A, 0xA0), // LSHIFT, DIK_LSHIFT, VK_LSHIFT
    k(49, 0x2B, 0xDC), // BACKSLASH, DIK_BACKSLASH, VK_OEM_5
    k(50, 0x2B, 0xDC), // NONUSHASH, DIK_BACKSLASH, VK_OEM_5
    k(29, 0x2C, 0x5A), // Z, DIK_Z, 'Z'
    k(27, 0x2D, 0x58), // X, DIK_X, 'X'
    k(6, 0x2E, 0x43), // C, DIK_C, 'C'
    k(25, 0x2F, 0x56), // V, DIK_V, 'V'
    k(5, 0x30, 0x42), // B, DIK_B, 'B'
    k(17, 0x31, 0x4E), // N, DIK_N, 'N'
    k(16, 0x32, 0x4D), // M, DIK_M, 'M'
    k(54, 0x33, 0xBC), // COMMA, DIK_COMMA, VK_OEM_COMMA
    k(55, 0x34, 0xBE), // PERIOD, DIK_PERIOD, VK_OEM_PERIOD
    k(56, 0x35, 0xBF), // SLASH, DIK_SLASH, VK_OEM_2
    k(229, 0x36, 0xA1), // RSHIFT, DIK_RSHIFT, VK_RSHIFT
    k(85, 0x37, 0x6A), // KP_MULTIPLY, DIK_MULTIPLY, VK_MULTIPLY
    k(226, 0x38, 0xA4), // LALT, DIK_LMENU, VK_LMENU
    k(44, 0x39, 0x20), // SPACE, DIK_SPACE, VK_SPACE
    k(57, 0x3A, 0x14), // CAPSLOCK, DIK_CAPITAL, VK_CAPITAL
    k(58, 0x3B, 0x70), // F1, DIK_F1, VK_F1
    k(59, 0x3C, 0x71), // F2, DIK_F2, VK_F2
    k(60, 0x3D, 0x72), // F3, DIK_F3, VK_F3
    k(61, 0x3E, 0x73), // F4, DIK_F4, VK_F4
    k(62, 0x3F, 0x74), // F5, DIK_F5, VK_F5
    k(63, 0x40, 0x75), // F6, DIK_F6, VK_F6
    k(64, 0x41, 0x76), // F7, DIK_F7, VK_F7
    k(65, 0x42, 0x77), // F8, DIK_F8, VK_F8
    k(66, 0x43, 0x78), // F9, DIK_F9, VK_F9
    k(67, 0x44, 0x79), // F10, DIK_F10, VK_F10
    k(83, 0x45, 0x90), // NUMLOCKCLEAR, DIK_NUMLOCK, VK_NUMLOCK
    k(71, 0x46, 0x91), // SCROLLLOCK, DIK_SCROLL, VK_SCROLL
    k(95, 0x47, 0x67), // KP_7, DIK_NUMPAD7, VK_NUMPAD7
    k(96, 0x48, 0x68), // KP_8, DIK_NUMPAD8, VK_NUMPAD8
    k(97, 0x49, 0x69), // KP_9, DIK_NUMPAD9, VK_NUMPAD9
    k(86, 0x4A, 0x6D), // KP_MINUS, DIK_SUBTRACT, VK_SUBTRACT
    k(92, 0x4B, 0x64), // KP_4, DIK_NUMPAD4, VK_NUMPAD4
    k(93, 0x4C, 0x65), // KP_5, DIK_NUMPAD5, VK_NUMPAD5
    k(94, 0x4D, 0x66), // KP_6, DIK_NUMPAD6, VK_NUMPAD6
    k(87, 0x4E, 0x6B), // KP_PLUS, DIK_ADD, VK_ADD
    k(89, 0x4F, 0x61), // KP_1, DIK_NUMPAD1, VK_NUMPAD1
    k(90, 0x50, 0x62), // KP_2, DIK_NUMPAD2, VK_NUMPAD2
    k(91, 0x51, 0x63), // KP_3, DIK_NUMPAD3, VK_NUMPAD3
    k(98, 0x52, 0x60), // KP_0, DIK_NUMPAD0, VK_NUMPAD0
    k(99, 0x53, 0x6E), // KP_PERIOD, DIK_DECIMAL, VK_DECIMAL
    k(100, 0x56, 0xE2), // NONUSBACKSLASH, DIK_OEM_102, VK_OEM_102
    k(68, 0x57, 0x7A), // F11, DIK_F11, VK_F11
    k(69, 0x58, 0x7B), // F12, DIK_F12, VK_F12
    k(104, 0x64, 0x7C), // F13, DIK_F13, VK_F13
    k(105, 0x65, 0x7D), // F14, DIK_F14, VK_F14
    k(106, 0x66, 0x7E), // F15, DIK_F15, VK_F15
    k(136, 0x70, 0x15), // INTERNATIONAL2, DIK_KANA, VK_KANA
    k(135, 0x73, 0x00), // INTERNATIONAL1, DIK_ABNT_C1, 0
    k(138, 0x79, 0x1C), // INTERNATIONAL4, DIK_CONVERT, VK_CONVERT
    k(139, 0x7B, 0x1D), // INTERNATIONAL5, DIK_NOCONVERT, VK_NONCONVERT
    k(137, 0x7D, 0x00), // INTERNATIONAL3, DIK_YEN, 0
    k(103, 0x8D, 0x00), // KP_EQUALS, DIK_NUMPADEQUALS, 0
    k(88, 0x9C, 0x0D), // KP_ENTER, DIK_NUMPADENTER, VK_RETURN
    k(228, 0x9D, 0xA3), // RCTRL, DIK_RCONTROL, VK_RCONTROL
    k(133, 0xB3, 0x6C), // KP_COMMA, DIK_NUMPADCOMMA, VK_SEPARATOR
    k(84, 0xB5, 0x6F), // KP_DIVIDE, DIK_DIVIDE, VK_DIVIDE
    k(70, 0xB7, 0x2C), // PRINTSCREEN, DIK_SYSRQ, VK_SNAPSHOT
    k(230, 0xB8, 0xA5), // RALT, DIK_RMENU, VK_RMENU
    k(72, 0xC5, 0x13), // PAUSE, DIK_PAUSE, VK_PAUSE
    k(74, 0xC7, 0x24), // HOME, DIK_HOME, VK_HOME
    k(82, 0xC8, 0x26), // UP, DIK_UP, VK_UP
    k(75, 0xC9, 0x21), // PAGEUP, DIK_PRIOR, VK_PRIOR
    k(80, 0xCB, 0x25), // LEFT, DIK_LEFT, VK_LEFT
    k(79, 0xCD, 0x27), // RIGHT, DIK_RIGHT, VK_RIGHT
    k(77, 0xCF, 0x23), // END, DIK_END, VK_END
    k(81, 0xD0, 0x28), // DOWN, DIK_DOWN, VK_DOWN
    k(78, 0xD1, 0x22), // PAGEDOWN, DIK_NEXT, VK_NEXT
    k(73, 0xD2, 0x2D), // INSERT, DIK_INSERT, VK_INSERT
    k(76, 0xD3, 0x2E), // DELETE, DIK_DELETE, VK_DELETE
    k(227, 0xDB, 0x5B), // LGUI, DIK_LWIN, VK_LWIN
    k(231, 0xDC, 0x5C), // RGUI, DIK_RWIN, VK_RWIN
    k(101, 0xDD, 0x5D), // APPLICATION, DIK_APPS, VK_APPS
    k(102, 0xDE, 0x00), // POWER, DIK_POWER, 0
    k(258, 0xDF, 0x00), // SLEEP, DIK_SLEEP, 0
];

// VK_*/SDL values the logic needs
const VK_LBUTTON: u32 = 0x01;
const VK_RBUTTON: u32 = 0x02;
const VK_CANCEL: u32 = 0x03;
const VK_MBUTTON: u32 = 0x04;
const VK_SHIFT: u32 = 0x10;
const VK_CONTROL: u32 = 0x11;
const VK_MENU: u32 = 0x12;
pub const VK_CAPITAL: u32 = 0x14;
pub const VK_NUMLOCK: u32 = 0x90;
pub const VK_SCROLL: u32 = 0x91;
const VK_OEM_1: u32 = 0xBA;
const VK_OEM_PLUS: u32 = 0xBB;
const VK_OEM_COMMA: u32 = 0xBC;
const VK_OEM_MINUS: u32 = 0xBD;
const VK_OEM_PERIOD: u32 = 0xBE;
const VK_OEM_2: u32 = 0xBF;
const VK_OEM_3: u32 = 0xC0;
const VK_OEM_4: u32 = 0xDB;
const VK_OEM_5: u32 = 0xDC;
const VK_OEM_6: u32 = 0xDD;
const VK_OEM_7: u32 = 0xDE;

const SCAN_LSHIFT: i32 = 225;
const SCAN_RSHIFT: i32 = 229;
const SCAN_LCTRL: i32 = 224;
const SCAN_RCTRL: i32 = 228;
const SCAN_LALT: i32 = 226;
const SCAN_RALT: i32 = 230;
const SCAN_KP_1: i32 = 89;
const SCAN_KP_0: i32 = 98;
pub const SCAN_UNKNOWN: i32 = 0;

// SDL_BUTTON_*MASK
const BUTTON_LMASK: u32 = 1;
const BUTTON_MMASK: u32 = 2;
const BUTTON_RMASK: u32 = 4;

// SDL_KMOD_*
const KMOD_SHIFT: u16 = 0x0003;
const KMOD_CTRL: u16 = 0x00C0;
const KMOD_ALT: u16 = 0x0300;
const KMOD_GUI: u16 = 0x0C00;
const KMOD_NUM: u16 = 0x1000;
const KMOD_CAPS: u16 = 0x2000;
const KMOD_SCROLL: u16 = 0x8000;

// PLAT_MOD_*
pub const PLAT_MOD_SHIFT: u32 = 0x1;
pub const PLAT_MOD_CONTROL: u32 = 0x2;
pub const PLAT_MOD_ALT: u32 = 0x4;
pub const PLAT_MOD_GUI: u32 = 0x8;
pub const PLAT_MOD_CAPS: u32 = 0x10;
pub const PLAT_MOD_NUM: u32 = 0x20;

/// The first row for a scancode.
pub fn find_scan_code(scan: i32) -> Option<&'static KeyMap> {
    KEYS.iter().find(|k| k.scan == scan)
}

/// The VK of a key: letters, digits and punctuation by the layout's character, the rest by position.
pub fn virtual_key_of(scan: i32, key: u32) -> u32 {
    let c = |b: u8| b as u32;
    if (c(b'a')..=c(b'z')).contains(&key) {
        return key - c(b'a') + c(b'A');
    }
    if (c(b'0')..=c(b'9')).contains(&key) && !(SCAN_KP_1..=SCAN_KP_0).contains(&scan) {
        return key;
    }
    match key {
        k if k == c(b';') => return VK_OEM_1,
        k if k == c(b'=') => return VK_OEM_PLUS,
        k if k == c(b',') => return VK_OEM_COMMA,
        k if k == c(b'-') => return VK_OEM_MINUS,
        k if k == c(b'.') => return VK_OEM_PERIOD,
        k if k == c(b'/') => return VK_OEM_2,
        k if k == c(b'`') => return VK_OEM_3,
        k if k == c(b'[') => return VK_OEM_4,
        k if k == c(b'\\') => return VK_OEM_5,
        k if k == c(b']') => return VK_OEM_6,
        k if k == c(b'\'') => return VK_OEM_7,
        _ => {}
    }
    find_scan_code(scan).map_or(0, |row| row.vk as u32)
}

/// `PLAT_MOD_*` of `SDL_Keymod` bits.
pub fn modifiers_of(modifiers: u16) -> u32 {
    let mut m = 0;
    if modifiers & KMOD_SHIFT != 0 {
        m |= PLAT_MOD_SHIFT;
    }
    if modifiers & KMOD_CTRL != 0 {
        m |= PLAT_MOD_CONTROL;
    }
    if modifiers & KMOD_ALT != 0 {
        m |= PLAT_MOD_ALT;
    }
    if modifiers & KMOD_GUI != 0 {
        m |= PLAT_MOD_GUI;
    }
    if modifiers & KMOD_CAPS != 0 {
        m |= PLAT_MOD_CAPS;
    }
    if modifiers & KMOD_NUM != 0 {
        m |= PLAT_MOD_NUM;
    }
    m
}

/// `PlatScanCodeIsDown`: any table row with the DIK whose scancode is held.
pub fn scan_code_is_down(dik: u32, held: &dyn Fn(i32) -> bool) -> i32 {
    i32::from(KEYS.iter().any(|k| k.dik as u32 == dik && held(k.scan)))
}

/// `PlatKeyIsDown`. `held`: the keyboard state by scancode; `buttons`: SDL_GetMouseState's flags;
/// `scan_of_key`: SDL_GetScancodeFromKey (0: unknown).
pub fn key_is_down(vk: u32, held: &dyn Fn(i32) -> bool, buttons: u32, scan_of_key: &dyn Fn(u32) -> i32) -> i32 {
    if (VK_LBUTTON..=VK_MBUTTON).contains(&vk) && vk != VK_CANCEL {
        let bit = if vk == VK_LBUTTON {
            BUTTON_LMASK
        } else if vk == VK_RBUTTON {
            BUTTON_RMASK
        } else {
            BUTTON_MMASK
        };
        return i32::from(buttons & bit != 0);
    }
    match vk {
        VK_SHIFT => return i32::from(held(SCAN_LSHIFT) || held(SCAN_RSHIFT)),
        VK_CONTROL => return i32::from(held(SCAN_LCTRL) || held(SCAN_RCTRL)),
        VK_MENU => return i32::from(held(SCAN_LALT) || held(SCAN_RALT)),
        _ => {}
    }
    let upper = (b'A' as u32)..=(b'Z' as u32);
    let digit = (b'0' as u32)..=(b'9' as u32);
    if upper.contains(&vk) || digit.contains(&vk) {
        let key = if vk >= b'A' as u32 { vk - b'A' as u32 + b'a' as u32 } else { vk };
        let scan = scan_of_key(key);
        return i32::from(scan != SCAN_UNKNOWN && held(scan));
    }
    i32::from(KEYS.iter().any(|k| k.vk as u32 == vk && held(k.scan)))
}

/// `PlatKeyIsToggled` of the SDL modifier bits.
pub fn key_is_toggled(vk: u32, modifiers: u16) -> i32 {
    match vk {
        VK_CAPITAL => i32::from(modifiers & KMOD_CAPS != 0),
        VK_NUMLOCK => i32::from(modifiers & KMOD_NUM != 0),
        VK_SCROLL => i32::from(modifiers & KMOD_SCROLL != 0),
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const GOLDEN: &str = include_str!("golden_window.txt");

    fn fnv_u32s(values: impl IntoIterator<Item = u32>) -> u64 {
        let mut h = 1469598103934665603u64;
        for v in values {
            for b in v.to_le_bytes() {
                h ^= b as u64;
                h = h.wrapping_mul(1099511628211);
            }
        }
        h
    }

    fn golden_hash(prefix: &str) -> u64 {
        let line = GOLDEN.lines().find(|l| l.starts_with(prefix)).unwrap();
        u64::from_str_radix(line.split_whitespace().nth(1).unwrap(), 16).unwrap()
    }

    #[test]
    fn the_table_is_the_cpp_table() {
        let rows: Vec<(i32, u8, u8)> = GOLDEN
            .lines()
            .filter(|l| l.starts_with("k "))
            .map(|l| {
                let p: Vec<&str> = l.split_whitespace().collect();
                (p[1].parse().unwrap(), p[2].parse().unwrap(), p[3].parse().unwrap())
            })
            .collect();
        assert_eq!(rows.len(), KEYS.len());
        for (a, b) in rows.iter().zip(KEYS.iter()) {
            assert_eq!(*a, (b.scan, b.dik, b.vk));
        }
    }

    const SWEEP_KEYS: [u32; 24] = [
        0,
        b'a' as u32,
        b'z' as u32,
        b'A' as u32,
        b'Z' as u32,
        b'0' as u32,
        b'5' as u32,
        b'9' as u32,
        b';' as u32,
        b'=' as u32,
        b',' as u32,
        b'-' as u32,
        b'.' as u32,
        b'/' as u32,
        b'`' as u32,
        b'[' as u32,
        b'\\' as u32,
        b']' as u32,
        b'\'' as u32,
        0x40000052,
        0x40000059,
        0x40000062,
        0x4000003A,
        0xFFFFFFFF,
    ];

    #[test]
    fn virtual_key_sweep_matches_the_cpp() {
        let mut out = Vec::new();
        for s in 0..512 {
            for k in SWEEP_KEYS {
                out.push(virtual_key_of(s, k));
            }
        }
        assert_eq!(fnv_u32s(out), golden_hash("vkhash "));
        let ex: Vec<u32> = GOLDEN
            .lines()
            .find(|l| l.starts_with("vkex"))
            .unwrap()
            .split_whitespace()
            .skip(1)
            .map(|t| t.parse().unwrap())
            .collect();
        assert_eq!(
            vec![
                virtual_key_of(20, b'a' as u32),
                virtual_key_of(91, b'3' as u32),
                virtual_key_of(58, 0x4000003A),
                virtual_key_of(51, b';' as u32),
                virtual_key_of(0, 0)
            ],
            ex
        );
    }

    #[test]
    fn modifiers_sweep_matches_the_cpp() {
        assert_eq!(fnv_u32s((0..=0xFFFFu32).map(|m| modifiers_of(m as u16))), golden_hash("modhash "));
    }

    #[test]
    fn key_state_sweep_matches_the_cpp() {
        const SCANS: [i32; 16] = [-1, 4, 225, 229, 224, 228, 226, 230, 98, 44, 40, 88, 39, 58, 50, 49];
        let scan_of_key = |k: u32| -> i32 {
            if (b'a' as u32..=b'z' as u32).contains(&k) {
                4 + (k - b'a' as u32) as i32
            } else if (b'1' as u32..=b'9' as u32).contains(&k) {
                30 + (k - b'1' as u32) as i32
            } else if k == b'0' as u32 {
                39
            } else {
                SCAN_UNKNOWN
            }
        };
        let mut out = Vec::new();
        for s in SCANS {
            let held = move |code: i32| s >= 0 && code == s;
            for vk in 0..300u32 {
                out.push(key_is_down(vk, &held, 0, &scan_of_key) as u32);
            }
            for dik in 0..300u32 {
                out.push(scan_code_is_down(dik, &held) as u32);
            }
        }
        assert_eq!(fnv_u32s(out), golden_hash("statehash "));

        let held = |_: i32| false;
        let mut mouse = Vec::new();
        for buttons in 0u32..32 {
            for vk in 1u32..=6 {
                mouse.push(key_is_down(vk, &held, buttons, &scan_of_key) as u32);
            }
        }
        assert_eq!(fnv_u32s(mouse), golden_hash("mousehash "));

        let mut toggles = Vec::new();
        for m in 0..=0xFFFFu32 {
            for vk in [VK_CAPITAL, VK_NUMLOCK, VK_SCROLL, 65] {
                toggles.push(key_is_toggled(vk, m as u16) as u32);
            }
        }
        assert_eq!(fnv_u32s(toggles), golden_hash("togglehash "));
    }

    #[test]
    fn duplicate_vk_rows_find_any_held_key() {
        // VK_RETURN is on both RETURN and KP_ENTER; the C++ scans every row
        assert_eq!(key_is_down(0x0D, &|c| c == 40, 0, &|_| 0), 1);
        assert_eq!(key_is_down(0x0D, &|c| c == 88, 0, &|_| 0), 1);
        assert_eq!(key_is_down(0x0D, &|_| false, 0, &|_| 0), 0);
    }
}
