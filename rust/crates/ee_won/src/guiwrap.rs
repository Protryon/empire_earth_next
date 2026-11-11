//! WONGUI's word wrap (WrappedText.cpp: every label and skinned button's text). The C++ keeps the text, the font
//! (it measures each character and the line height), the break list and the paint; these answer where the lines
//! break (`CalcWrap`) and whether a new width needs a new wrap (`SetMaxWidth`).

/// One line break: the line ends at `pos`, the next starts at `next`; the line is `width` wide.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonBreak {
    pub pos: i32,
    pub next: i32,
    pub width: i32,
}

/// `CalcWrap`'s loop over the text's units (`widths[i]` the font's width of unit i) for `max_width`: the breaks
/// (at the last space that fits, else inside the word; CR, LF and CR LF always break) and the text's width (the
/// widest line, or the last line's when no line had a width).
pub fn wrap(text: &[u32], widths: &[i32], max_width: i32) -> (Vec<EeWonBreak>, i32) {
    let len = text.len() as i32;
    // the original read the unit after a CR at the end of the text: the terminator
    let at = |i: i32| if i >= 0 && i < len { text[i as usize] } else { 0 };
    let mut breaks = Vec::new();
    let (mut break_pos, mut break_width) = (-1i32, 0i32);
    let (mut pos, mut width, mut max) = (0i32, 0i32, 0i32);
    while pos < len {
        let ch = at(pos);
        let w = widths[pos as usize];
        if ch == u32::from(b' ') {
            break_pos = pos;
            break_width = width;
        } else if ch == u32::from(b'\r') || ch == u32::from(b'\n') {
            break_pos = pos;
            break_width = width;
            if ch == u32::from(b'\r') && at(pos + 1) == u32::from(b'\n') {
                pos += 1;
            }
            pos += 1;
            breaks.push(EeWonBreak { pos: break_pos, next: pos, width: break_width });
            if break_width > max {
                max = break_width;
            }
            break_pos = -1;
            width = 0;
            break_width = 0;
            continue;
        }
        if width.wrapping_add(w) > max_width {
            if break_pos != -1 {
                breaks.push(EeWonBreak { pos: break_pos, next: break_pos + 1, width: break_width });
                pos = break_pos;
                if break_width > max {
                    max = break_width;
                }
            } else {
                breaks.push(EeWonBreak { pos, next: pos, width });
                if width > max {
                    max = width;
                }
                if width > 0 {
                    pos -= 1;
                }
            }
            break_pos = -1;
            width = 0;
            break_width = 0;
        } else {
            width = width.wrapping_add(w);
        }
        pos += 1;
    }
    if width > max {
        max = width;
    }
    (breaks, if max == 0 { width } else { max })
}

/// `SetMaxWidth`: (the width it keeps, whether to wrap again); `None` when the width is unchanged. A wrapping
/// text wraps again when it had no width, had breaks, or narrows.
pub fn max_width_change(new_width: i32, old_width: i32, wrapping: bool, has_breaks: bool) -> Option<(i32, bool)> {
    let w = new_width.max(0);
    if w == old_width {
        return None;
    }
    Some((w, wrapping && (old_width == 0 || has_breaks || w < old_width)))
}

// ---- the C ABI

/// `wrap` over a `GUIString`'s units with their widths: the first `room` breaks into `out` (`2 * len + 1` is
/// enough: a unit breaks at most twice, once inside a word and once more when it alone is wider than the line),
/// their count returned (all of them, even past the room), the width into `out_width`.
///
/// # Safety
/// `text` holds `len` units of `wchar_size` bytes and `widths` `len` ints (or both are NULL with 0); `out` has room
/// for `room` breaks; `out_width` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_wrap(
    text: *const std::ffi::c_void,
    len: u16,
    wchar_size: u32,
    widths: *const i32,
    max_width: i32,
    out: *mut EeWonBreak,
    room: usize,
    out_width: *mut i32,
) -> usize {
    let n = usize::from(len);
    let (t, w): (Vec<u32>, &[i32]) = if n == 0 || text.is_null() {
        (Vec::new(), &[])
    } else {
        // SAFETY: `len` units and widths (the contract)
        unsafe {
            let t = if wchar_size == 2 {
                std::slice::from_raw_parts(text.cast::<u16>(), n).iter().map(|&u| u32::from(u)).collect()
            } else {
                std::slice::from_raw_parts(text.cast::<u32>(), n).to_vec()
            };
            (t, std::slice::from_raw_parts(widths, n))
        }
    };
    let (b, width) = wrap(&t, w, max_width);
    for (i, x) in b.iter().take(room).enumerate() {
        // SAFETY: room for `room` (the contract)
        unsafe { *out.add(i) = *x };
    }
    // SAFETY: writable (the contract)
    unsafe { *out_width = width };
    b.len()
}

/// `max_width_change`: false when the width is unchanged, else true with the width and the rewrap in the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_max_width(
    new_width: i32,
    old_width: i32,
    wrapping: bool,
    has_breaks: bool,
    out_width: *mut i32,
    out_rewrap: *mut bool,
) -> bool {
    match max_width_change(new_width, old_width, wrapping, has_breaks) {
        Some((w, r)) => {
            // SAFETY: writable (the contract)
            unsafe {
                *out_width = w;
                *out_rewrap = r;
            }
            true
        }
        None => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn t(s: &str) -> Vec<u32> {
        s.chars().map(u32::from).collect()
    }

    #[test]
    fn wraps() {
        let b = |pos, next, width| EeWonBreak { pos, next, width };
        let s = t("ab cd ef");
        let w = vec![5; s.len()];
        assert_eq!(wrap(&s, &w, 100), (vec![], 40));
        assert_eq!(wrap(&s, &w, 26), (vec![b(5, 6, 25)], 25));
        assert_eq!(wrap(&s, &w, 12), (vec![b(2, 3, 10), b(5, 6, 10)], 10));
        // no space fits: the word breaks where it overflows
        let s = t("abcdef");
        assert_eq!(wrap(&s, &[5; 6], 12), (vec![b(2, 2, 10), b(4, 4, 10)], 10));
        // CR LF, a lone LF
        let s = t("ab\r\ncd\nx");
        assert_eq!(wrap(&s, &[5; 8], 100), (vec![b(2, 4, 10), b(6, 7, 10)], 10));
        // a character wider than the line on its own
        assert_eq!(wrap(&t("a"), &[50], 10), (vec![b(0, 0, 0)], 0));
        assert_eq!(wrap(&[], &[], 10), (vec![], 0));
    }

    #[test]
    fn max_width() {
        assert_eq!(max_width_change(-5, 0, true, false), None);
        assert_eq!(max_width_change(10, 0, true, false), Some((10, true)));
        assert_eq!(max_width_change(20, 10, true, false), Some((20, false)));
        assert_eq!(max_width_change(20, 10, true, true), Some((20, true)));
        assert_eq!(max_width_change(5, 10, true, false), Some((5, true)));
        assert_eq!(max_width_change(5, 10, false, true), Some((5, false)));
    }
}
