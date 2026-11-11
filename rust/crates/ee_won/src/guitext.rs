//! WONGUI's rich text area (TextArea.cpp, TextSegments.cpp, TextFormat.cpp, TextAreaInstructions.cpp: the lobby's
//! chat, its MOTD and help pages). The C++ keeps the area, its lines (`SmartPtr<TextLine>`s in a `std::deque`),
//! its segments (a `std::list`), the fonts (every width is measured there and handed over, or read through a
//! callback where the original measured only what its loop reached), the links, cursors, timers, events and the
//! paint; these answer:
//!
//! - a segment's place on its line and the line's new height and baseline (`AddSeg`), the line's x for its
//!   alignment (`AlignLine`), whether a segment fits its line or wraps (`AddSegmentPrv`), where a text segment
//!   wraps over new lines (`TextSegment::Wrap`);
//! - whether new text keeps the view at the bottom (`ShouldScrollOnBottom`), the bottom offset, whether a width
//!   change waits for the timer (`Virtual08`) and when the waiting wrap is due (`TimerEvent`);
//! - which segment the character and line limits trim up to (`EnforceMaxChars`, `EnforceMaxLines`), which lines
//!   a trim removes (`RemoveSegmentsUpTo`) and the renumbering after it (`EnforceInvariants`);
//! - the segment under a point (`FindSegment`), the character under it (`CharAt`), the selection it starts or
//!   extends (`CheckSel`), the drag scroll (`TimerEvent`) and the keys' scroll (`KeyDown`), a segment's part of
//!   the selection (`GetSelText`);
//! - how a text splits at CR, LF and CR LF into line-broken segments (`AddFormatedText`), how a lobby format
//!   text expands its `<n>`, `<cN>`, `<fN>`, `<sN>`, `<lN>` and `<<` commands (`LobbyAppendFormattedText`), the
//!   line an anchor goes to (`GotoAnchor`).
//!
//! The line at a y (`FindLineAt`), a line's visibility (`IsLineVisible`) and the full-line scroll's extra height
//! (`AdjustPosition`) are the list's (`guilist`): the same statements. Ints wrap (the game builds with -fwrapv).

use std::ffi::c_void;

/// The text flags (TextFlags in the C++).
pub const WORD_WRAP: i32 = 0x01;
pub const SCROLL_ON_BOTTOM: i32 = 0x04;
pub const SHOW_PARTIAL_LINES: i32 = 0x08;
pub const SCROLL_FULL_LINE: i32 = 0x10;
pub const DELAY_WORD_WRAP: i32 = 0x20;

/// A line's vertical metrics (`TextLine`'s height, baseline and descent).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonTextLineM {
    pub height: i32,
    pub baseline: i32,
    pub descent: i32,
}

/// `AddSeg`'s metrics: the line grows to the segment's ascent, descent and height (the area's total height
/// following its height); true when the line's segments must be placed again on the new baseline. The segment's
/// y is then `baseline - ascent`.
pub fn add_seg_metrics(line: &mut EeWonTextLineM, seg_height: i32, seg_ascent: i32, total_height: &mut i32) -> bool {
    let mut adjust = false;
    if seg_ascent > line.baseline {
        line.baseline = seg_ascent;
        adjust = true;
    }
    let descent = seg_height.wrapping_sub(seg_ascent);
    if descent > line.descent {
        line.descent = descent;
    }
    if seg_height > line.height {
        *total_height = total_height.wrapping_sub(line.height);
        line.height = seg_height;
        *total_height = total_height.wrapping_add(line.height);
        adjust = true;
    }
    if line.descent.wrapping_add(line.baseline) > line.height {
        *total_height = total_height.wrapping_sub(line.height);
        line.height = line.baseline.wrapping_add(line.descent);
        *total_height = total_height.wrapping_add(line.height);
        adjust = true;
    }
    adjust
}

/// `AlignLine`: the line's x for `align` (0 left, 1 centre, 2 right; anything else left), never negative.
pub fn align_x(view_width: i32, line_width: i32, align: i32) -> i32 {
    let x = match align {
        1 => view_width.wrapping_sub(line_width) / 2,
        2 => view_width.wrapping_sub(line_width),
        _ => 0,
    };
    x.max(0)
}

/// `AddSegmentPrv`'s test: the segment goes on its line as it is (else it wraps).
pub fn seg_fits(
    area_width: i32,
    line_width: i32,
    seg_width: i32,
    view_width: i32,
    word_wrap: bool,
    been_visible: bool,
) -> bool {
    area_width <= 0 || line_width.wrapping_add(seg_width) <= view_width || !word_wrap || !been_visible
}

/// A line's place: its y and height.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonTextLineY {
    pub y: i32,
    pub height: i32,
}

/// `ShouldScrollOnBottom`: the flag is set and the user has not scrolled, or the last line (the one before
/// when the last is empty and not alone) is still in view. `last` and `prev` are read only when there are that
/// many lines.
pub fn should_scroll_on_bottom(
    flags: i32,
    user_scrolled: bool,
    count: usize,
    last: EeWonTextLineY,
    prev: EeWonTextLineY,
    offset: i32,
    view_height: i32,
) -> bool {
    if flags & SCROLL_ON_BOTTOM == 0 {
        return false;
    }
    if !user_scrolled {
        return true;
    }
    if count > 0 {
        let line = if last.height == 0 && count > 1 { prev } else { last };
        if crate::guilist::is_item_visible(line.y, line.height, offset, view_height, flags & SHOW_PARTIAL_LINES != 0) {
            return true;
        }
    }
    false
}

/// `Virtual08`: a width change waits for the timer (delayed wrap over more than 5000 characters).
pub fn delay_wrap(flags: i32, chars: i32) -> bool {
    flags & DELAY_WORD_WRAP != 0 && chars > 5000
}

/// `TimerEvent`: the waiting wrap is due at this tick count (already counted up).
pub fn delayed_wrap_due(ticks: i32, chars: i32) -> bool {
    ticks > 1 && (ticks >= 10 || ticks >= chars / 16000)
}

/// `EnforceMaxChars`: how many characters to trim (none when no limit or within it).
pub fn max_chars_excess(max_chars: i32, last_end: i32) -> Option<i32> {
    if max_chars <= 0 {
        return None;
    }
    let n = last_end.wrapping_sub(max_chars);
    (n > 0).then_some(n)
}

/// `EnforceMaxChars`' walk: the first segment ending at or past `excess - 1` (the last one when none does).
pub fn max_chars_cut(ends: &[i32], excess: i32) -> usize {
    ends.iter().position(|&e| e.wrapping_add(1) >= excess).unwrap_or(ends.len().saturating_sub(1))
}

/// `EnforceMaxLines`: how many line breaks to trim.
pub fn max_lines_excess(max_lines: i32, num_lines: i32) -> Option<i32> {
    if max_lines <= 0 {
        return None;
    }
    let n = num_lines.wrapping_sub(max_lines);
    (n > 0).then_some(n)
}

/// `EnforceMaxLines`' walk: the segment holding the `excess`th line break (the last one when there are fewer).
pub fn max_lines_cut(breaks: &[bool], excess: i32) -> usize {
    let mut count = 0i32;
    for (i, &b) in breaks.iter().enumerate() {
        if b {
            count = count.wrapping_add(1);
        }
        if count >= excess {
            return i;
        }
    }
    breaks.len().saturating_sub(1)
}

/// A line's part in `RemoveSegmentsUpTo`'s search (bits): it is the trimmed segment's line, it has segments,
/// its last segment stands for the trimmed one.
pub const LINE_IS_SEG_LINE: u32 = 1;
pub const LINE_HAS_SEGS: u32 = 2;
pub const LINE_LAST_IS_SEG: u32 = 4;
/// A line's flags (`i` below the count), read from the C++ area.
pub type EeWonLineFlagsOf = extern "C" fn(ctx: *const c_void, i: usize) -> u32;

/// `RemoveSegmentsUpTo`'s search: the first line from the trimmed segment's on whose last segment is another
/// one; the lines before it go (all of them when there is none).
pub fn remove_lines_stop(count: usize, flags: impl Fn(usize) -> u32) -> usize {
    let mut found = false;
    for i in 0..count {
        let f = flags(i);
        if f & LINE_IS_SEG_LINE != 0 {
            found = true;
        }
        if found && f & LINE_HAS_SEGS != 0 && f & LINE_LAST_IS_SEG == 0 {
            return i;
        }
    }
    count
}

/// `EnforceInvariants`' sums and renumbering: the lines moved up to start at 0, the segments' characters to
/// start at 0, the selection and the offset with them; (total width, total height, line count). Nothing moves
/// when either list is empty.
pub struct Renumber<'a> {
    pub line_widths: &'a [i32],
    pub line_heights: &'a [i32],
    pub line_ys: &'a mut [i32],
    pub seg_starts: &'a mut [i32],
    pub seg_ends: &'a mut [i32],
    pub seg_breaks: &'a [bool],
    pub sel_start: &'a mut i32,
    pub sel_end: &'a mut i32,
    pub offset: &'a mut i32,
}

pub fn renumber(r: Renumber<'_>) -> (i32, i32, i32) {
    let (mut tw, mut th, mut lines) = (0i32, 0i32, 0i32);
    if r.line_ys.is_empty() || r.seg_starts.is_empty() {
        return (0, 0, 0);
    }
    let dy = r.line_ys[0];
    for i in 0..r.line_ys.len() {
        if r.line_widths[i] > tw {
            tw = r.line_widths[i];
        }
        r.line_ys[i] = r.line_ys[i].wrapping_sub(dy);
        th = th.wrapping_add(r.line_heights[i]);
    }
    let dc = r.seg_starts[0];
    for i in 0..r.seg_starts.len() {
        r.seg_starts[i] = r.seg_starts[i].wrapping_sub(dc);
        r.seg_ends[i] = r.seg_ends[i].wrapping_sub(dc);
        if r.seg_breaks[i] {
            lines = lines.wrapping_add(1);
        }
    }
    *r.sel_start = r.sel_start.wrapping_sub(dc).max(0);
    *r.sel_end = r.sel_end.wrapping_sub(dc).max(0);
    *r.offset = r.offset.wrapping_sub(dy);
    (tw, th, lines)
}

/// `FindSegment`'s lenient clamp of a point into the text.
pub fn find_clamp(x: i32, y: i32, total_width: i32, total_height: i32) -> (i32, i32) {
    let mut x = x;
    let mut y = y;
    if x >= total_width && total_width != 0 {
        x = total_width.wrapping_sub(1);
    }
    if x < 0 {
        x = 0;
    }
    if y >= total_height {
        y = total_height.wrapping_sub(1);
    }
    if y < 0 {
        y = 0;
    }
    (x, y)
}

/// `FindSegment`'s cache test: the last segment found is still under the point.
pub fn find_cached(x: i32, y: i32, line_x: i32, line_y: i32, line_height: i32, seg_x: i32, seg_width: i32) -> bool {
    let xp = x.wrapping_sub(line_x);
    line_y <= y && line_y.wrapping_add(line_height) >= y && xp >= seg_x && xp <= seg_x.wrapping_add(seg_width)
}

/// `FindSegment`'s pick on the line found: the first segment whose right edge is not left of `x` (relative to
/// the line), the last when none; strictly, none left of the segment picked. `None`: strictly, the point is
/// below the line (the original returned there, keeping its cached segment); `Some(None)`: no segment (cached).
pub fn find_pick(
    x: i32,
    y: i32,
    line_y: i32,
    line_height: i32,
    xs: &[i32],
    widths: &[i32],
    lenient: bool,
) -> Option<Option<usize>> {
    if !lenient && line_y.wrapping_add(line_height) < y {
        return None;
    }
    if xs.is_empty() {
        return Some(None);
    }
    let i = (0..xs.len()).find(|&i| x <= xs[i].wrapping_add(widths[i])).unwrap_or(xs.len() - 1);
    if !lenient && x < xs[i] {
        return Some(None);
    }
    Some(Some(i))
}

/// `TextSegment::CharAt`: the character nearest `width` pixels into [start, end) (rounding at half a
/// character), as an absolute position; `char_width(i)` is the font's width of unit `i`, asked only for the units
/// the walk reaches.
pub fn char_at(width: i32, start: i32, end: i32, abs_start: i32, mut char_width: impl FnMut(i32) -> i32) -> i32 {
    let mut pos = start;
    let (mut w, mut cw) = (0i32, 0i32);
    while pos < end && w < width {
        cw = char_width(pos);
        w = w.wrapping_add(cw);
        pos += 1;
    }
    if width >= 0 && width < w.wrapping_sub(cw / 2) {
        pos -= 1;
    }
    pos.wrapping_add(abs_start)
}

/// `TextSegmentBase::CharAt`: its start left of its middle, else its end.
pub fn base_char_at(x: i32, seg_x: i32, seg_width: i32, abs_start: i32, abs_end: i32) -> i32 {
    if x < seg_x.wrapping_add(seg_width / 2) {
        abs_start
    } else {
        abs_end
    }
}

/// `CheckSel`: the selection (start, end, backwards) after the character `at` starts it or extends it.
pub fn check_sel(start: i32, end: i32, backwards: bool, at: i32, new_start: bool) -> (i32, i32, bool) {
    if new_start {
        return (at, at, false);
    }
    let (mut s, mut e, mut b) = (start, end, backwards);
    if b {
        s = at;
    } else {
        e = at;
    }
    if s > e {
        b = !backwards;
        std::mem::swap(&mut s, &mut e);
    }
    (s, e, b)
}

/// `TimerEvent`'s drag scroll: `None` while the mouse is inside the area, else the new (vertical, horizontal)
/// offsets.
#[allow(clippy::too_many_arguments)]
pub fn drag_scroll(
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    full_line: bool,
    down: i32,
    up: i32,
    vert: i32,
    horz: i32,
) -> Option<(i32, i32)> {
    if x >= 0 && x <= width && y >= 0 && y <= height {
        return None;
    }
    let (mut v, mut h) = (vert, horz);
    if y > height {
        let mut d = y.wrapping_sub(height);
        if full_line && d < down {
            d = down;
        }
        v = v.wrapping_add(d);
    } else if y < 0 {
        let mut d = y.wrapping_neg();
        if full_line && d < up {
            d = up;
        }
        v = v.wrapping_sub(d);
    }
    if x > width {
        h = h.wrapping_add(x.wrapping_sub(width));
    } else if x < 0 {
        h = h.wrapping_add(x);
    }
    Some((v, h))
}

/// `KeyDown`'s scroll keys (the Win32 virtual keys: Right, Left, Down, Up, Page Down, Page Up): the new
/// (vertical, horizontal) offsets, `None` for any other key.
pub fn key_scroll(key: i32, vert: i32, horz: i32, down: i32, up: i32, view_height: i32) -> Option<(i32, i32)> {
    Some(match key {
        0x27 => (vert, horz.wrapping_add(10)),
        0x25 => (vert, horz.wrapping_sub(10)),
        0x28 => (vert.wrapping_add(down), horz),
        0x26 => (vert.wrapping_sub(up), horz),
        0x22 => (vert.wrapping_add(view_height), horz),
        0x21 => (vert.wrapping_sub(view_height), horz),
        _ => return None,
    })
}

/// `TextSegment::GetSelText`: the part of the segment's text the selection covers (offset, length).
pub fn sel_part(sel_start: i32, sel_end: i32, abs_start: i32, abs_end: i32) -> Option<(i32, i32)> {
    let s = sel_start.max(abs_start);
    let e = sel_end.min(abs_end);
    if s >= e {
        return None;
    }
    Some((s.wrapping_sub(abs_start), e.wrapping_sub(s)))
}

/// Whether the copied text gets the segment's line break: a text segment's when the selection runs past its
/// end (and covers some of it), a plain segment's when the selection covers it whole and runs past it.
pub fn sel_break(text: bool, line_break: bool, sel_start: i32, sel_end: i32, abs_start: i32, abs_end: i32) -> bool {
    if text {
        sel_part(sel_start, sel_end, abs_start, abs_end).is_some() && line_break && sel_end > abs_end
    } else {
        line_break && sel_start <= abs_start && sel_end > abs_end
    }
}

/// `GotoAnchor`'s line: the anchor's line clamped to the lines there are.
pub fn anchor_line(line: i32, count: usize) -> i32 {
    // the original compared the int with the deque's size as unsigned: a negative line counts as past the end
    let mut l = line;
    if l as isize as usize >= count {
        l = (count as i32).wrapping_sub(1);
    }
    l.max(0)
}

/// One step of a segment's wrap (`TextSegment::Wrap`): a piece of the text on the current line
/// (`kind` 0: units [start, end), `width` wide), or a new line (`kind` 1: `width` its starting width, the wrap
/// indent on a left-aligned area).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonTextWrapOp {
    pub kind: i32,
    pub start: i32,
    pub end: i32,
    pub width: i32,
}

/// `TextSegment::Wrap`'s loop over the text's units (`widths[i]` the font's width of unit i) in a view
/// `box_width` wide, from a line `line_width` wide (`line_empty`: no segment on it yet): at the last space that
/// fits (the piece's width includes it), else inside a word on an empty line (the piece as wide as the view, at
/// least one unit), else the whole text again on a new line. After each break the rest goes whole on the new
/// line when it fits (`rest_width(start)`, the font's width of the units from `start` to the end).
#[allow(clippy::too_many_arguments)]
pub fn wrap_segment(
    text: &[u32],
    widths: &[i32],
    box_width: i32,
    line_width: i32,
    line_empty: bool,
    align_left: bool,
    indent: i32,
    mut rest_width: impl FnMut(i32) -> i32,
) -> Vec<EeWonTextWrapOp> {
    let len = text.len() as i32;
    let mut ops = Vec::new();
    let (mut lw, mut empty) = (line_width, line_empty);
    let mut cur = 0i32;
    while cur < len {
        let mut width = lw;
        let mut break_width = lw;
        let start = cur;
        let mut break_pos = start - 1;
        while cur < len {
            let ch = text[cur as usize];
            width = width.wrapping_add(widths[cur as usize]);
            if ch == u32::from(b' ') {
                break_pos = cur;
                break_width = width;
            }
            if width > box_width {
                break;
            }
            cur += 1;
        }
        let mut force = false;
        if break_pos < start {
            if width < box_width {
                break_pos = len;
            } else if empty {
                break_pos = cur;
                break_width = box_width;
                if break_pos == start {
                    break_pos += 1;
                }
                force = true;
            } else {
                cur = 0;
            }
        }
        if break_pos >= start {
            if break_pos != start {
                // (the line is left behind next: a new one follows, or the text ends)
                ops.push(EeWonTextWrapOp { kind: 0, start, end: break_pos, width: break_width });
            }
            cur = break_pos;
            if !force {
                cur += 1;
            }
        }
        if cur >= len {
            break;
        }
        lw = if align_left { indent } else { 0 };
        empty = true;
        ops.push(EeWonTextWrapOp { kind: 1, start: 0, end: 0, width: lw });
        let w = rest_width(cur);
        if w > box_width.wrapping_sub(lw) {
            continue;
        }
        ops.push(EeWonTextWrapOp { kind: 0, start: cur, end: len, width: w });
        break;
    }
    ops
}

/// One line of `AddFormatedText`: units [start, start + len), with a line break after it.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonTextPart {
    pub start: i32,
    pub len: i32,
    pub line_break: bool,
}

/// `AddFormatedText`: the text split at CR, LF and CR LF, each line breaking, the last only with `line_break`.
pub fn split_lines(text: &[u32], line_break: bool) -> Vec<EeWonTextPart> {
    let len = text.len() as i32;
    let mut parts = Vec::new();
    let (mut start, mut next) = (0i32, 0i32);
    while start < len {
        let mut end = start;
        while end < len {
            let ch = text[end as usize];
            if ch == u32::from(b'\r') {
                next = if end + 1 < len && text[(end + 1) as usize] == u32::from(b'\n') { end + 2 } else { end + 1 };
                break;
            } else if ch == u32::from(b'\n') {
                next = end + 1;
                break;
            }
            end += 1;
        }
        let mut found = true;
        if end >= len {
            next = len;
            found = line_break;
        }
        parts.push(EeWonTextPart { start, len: end - start, line_break: found });
        start = next;
    }
    parts
}

/// One step of `LobbyAppendFormattedText`: `kind` 0 appends units [a, a + b) to the pending text, 1 appends
/// '<', 2 adds the pending text as a segment in the current colour and font and empties it, 3 a line break,
/// 4 the colour becomes colour `a`, 5 the font becomes font `a` (when not NULL), 6 adds string `a` as a segment
/// (when not NULL), 7 adds segment `a` (when not NULL), 8 adds the pending text as the last segment, breaking
/// the line when `b` is 1.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonFormatOp {
    pub kind: i32,
    pub a: i32,
    pub b: i32,
}

/// `LobbyAppendFormattedText`'s parse of `format` (indices 1-based in the text, checked against the counts:
/// strings, colours, fonts, segments); the C++ checks the NULL entries.
pub fn format_ops(format: &[u32], counts: [i32; 4], line_break: bool) -> Vec<EeWonFormatOp> {
    let len = format.len() as i32;
    // the original read the terminator past the end
    let at = |i: i32| if i >= 0 && i < len { format[i as usize] } else { 0 };
    let find = |ch: u32, from: i32| (from.max(0)..len).find(|&i| format[i as usize] == ch);
    let op = |kind, a, b| EeWonFormatOp { kind, a, b };
    let mut ops = Vec::new();
    let mut pending = 0i32;
    let mut start = 0i32;
    while let Some(open) = find(u32::from(b'<'), start) {
        ops.push(op(0, start, open - start));
        pending += open - start;
        let next = open + 1;
        let command = at(next);
        if command == u32::from(b'<') {
            ops.push(op(1, 0, 0));
            pending += 1;
            start = open + 2;
            continue;
        }
        if next < len {
            if pending != 0 {
                ops.push(op(2, 0, 0));
                pending = 0;
            }
            let index = (at(open + 2) as i32).wrapping_sub(i32::from(b'1'));
            // the CRT's tolower: only the ASCII capitals become the command letters
            let lower = if (u32::from(b'A')..=u32::from(b'Z')).contains(&command) { command + 32 } else { command };
            let slot = |k: usize| index >= 0 && index < counts[k];
            match char::from_u32(lower) {
                Some('n') => ops.push(op(3, 0, 0)),
                Some('c') if slot(1) => ops.push(op(4, index, 0)),
                Some('f') if slot(2) => ops.push(op(5, index, 0)),
                Some('s') if slot(0) => ops.push(op(6, index, 0)),
                Some('l') if slot(3) => ops.push(op(7, index, 0)),
                _ => {}
            }
        }
        start = open + 1;
        let Some(close) = find(u32::from(b'>'), start) else { break };
        start = close + 1;
    }
    if start < len {
        ops.push(op(0, start, len - start));
        pending += len - start;
    }
    if pending != 0 || line_break {
        ops.push(op(8, 0, i32::from(line_break)));
    }
    ops
}

// ---- the C ABI

/// A `GUIString`'s units (`wchar_size` bytes each) widened.
unsafe fn wide(text: *const c_void, len: usize, wchar_size: u32) -> Vec<u32> {
    if len == 0 || text.is_null() {
        return Vec::new();
    }
    // SAFETY: `len` units (the callers' contract)
    unsafe {
        if wchar_size == 2 {
            std::slice::from_raw_parts(text.cast::<u16>(), len).iter().map(|&u| u32::from(u)).collect()
        } else {
            std::slice::from_raw_parts(text.cast::<u32>(), len).to_vec()
        }
    }
}

/// A slice of `n` from a pointer that may be NULL when `n` is 0.
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        // SAFETY: `n` entries (the callers' contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

unsafe fn slice_mut<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if n == 0 || p.is_null() {
        &mut []
    } else {
        // SAFETY: `n` entries (the callers' contract)
        unsafe { std::slice::from_raw_parts_mut(p, n) }
    }
}

/// Copies `items` into `out` (room for `room`), returning the count of all of them.
unsafe fn emit<T: Copy>(items: &[T], out: *mut T, room: usize) -> usize {
    for (i, x) in items.iter().take(room).enumerate() {
        // SAFETY: room for `room` (the callers' contract)
        unsafe { *out.add(i) = *x };
    }
    items.len()
}

/// `add_seg_metrics` on the line and the total height in place.
///
/// # Safety
/// `line` and `total_height` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_add_seg(
    line: *mut EeWonTextLineM,
    seg_height: i32,
    seg_ascent: i32,
    total_height: *mut i32,
) -> bool {
    // SAFETY: writable (the contract)
    unsafe { add_seg_metrics(&mut *line, seg_height, seg_ascent, &mut *total_height) }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_align_x(view_width: i32, line_width: i32, align: i32) -> i32 {
    align_x(view_width, line_width, align)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_seg_fits(
    area_width: i32,
    line_width: i32,
    seg_width: i32,
    view_width: i32,
    word_wrap: bool,
    been_visible: bool,
) -> bool {
    seg_fits(area_width, line_width, seg_width, view_width, word_wrap, been_visible)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_scroll_on_bottom(
    flags: i32,
    user_scrolled: bool,
    count: usize,
    last: EeWonTextLineY,
    prev: EeWonTextLineY,
    offset: i32,
    view_height: i32,
) -> bool {
    should_scroll_on_bottom(flags, user_scrolled, count, last, prev, offset, view_height)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_delay_wrap(flags: i32, chars: i32) -> bool {
    delay_wrap(flags, chars)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_wrap_due(ticks: i32, chars: i32) -> bool {
    delayed_wrap_due(ticks, chars)
}

/// `max_chars_excess`: 0 when nothing is trimmed.
#[no_mangle]
pub extern "C" fn ee_won_gui_text_chars_excess(max_chars: i32, last_end: i32) -> i32 {
    max_chars_excess(max_chars, last_end).unwrap_or(0)
}

/// `max_chars_cut` over `count` segment ends.
///
/// # Safety
/// `ends` holds `count` ints (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_chars_cut(ends: *const i32, count: usize, excess: i32) -> usize {
    // SAFETY: `count` ints (the contract)
    max_chars_cut(unsafe { slice(ends, count) }, excess)
}

/// `max_lines_excess`: 0 when nothing is trimmed.
#[no_mangle]
pub extern "C" fn ee_won_gui_text_lines_excess(max_lines: i32, num_lines: i32) -> i32 {
    max_lines_excess(max_lines, num_lines).unwrap_or(0)
}

/// `max_lines_cut` over `count` segments' line-break flags.
///
/// # Safety
/// `breaks` holds `count` bools (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_lines_cut(breaks: *const bool, count: usize, excess: i32) -> usize {
    // SAFETY: `count` bools (the contract)
    max_lines_cut(unsafe { slice(breaks, count) }, excess)
}

/// `remove_lines_stop` over `count` lines whose flags `flags_of(ctx, i)` gives.
#[no_mangle]
pub extern "C" fn ee_won_gui_text_remove_stop(count: usize, flags_of: EeWonLineFlagsOf, ctx: *const c_void) -> usize {
    remove_lines_stop(count, |i| flags_of(ctx, i))
}

/// `renumber` over `lines` lines and `segs` segments in place; the totals and the line count into `out` (3
/// ints).
///
/// # Safety
/// Each array holds its count of entries (or is NULL with 0); the scalars and `out` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_renumber(
    line_widths: *const i32,
    line_heights: *const i32,
    line_ys: *mut i32,
    lines: usize,
    seg_starts: *mut i32,
    seg_ends: *mut i32,
    seg_breaks: *const bool,
    segs: usize,
    sel_start: *mut i32,
    sel_end: *mut i32,
    offset: *mut i32,
    out: *mut i32,
) {
    // SAFETY: the counts and writable scalars (the contract)
    unsafe {
        let (tw, th, n) = renumber(Renumber {
            line_widths: slice(line_widths, lines),
            line_heights: slice(line_heights, lines),
            line_ys: slice_mut(line_ys, lines),
            seg_starts: slice_mut(seg_starts, segs),
            seg_ends: slice_mut(seg_ends, segs),
            seg_breaks: slice(seg_breaks, segs),
            sel_start: &mut *sel_start,
            sel_end: &mut *sel_end,
            offset: &mut *offset,
        });
        *out = tw;
        *out.add(1) = th;
        *out.add(2) = n;
    }
}

/// `find_clamp` in place.
///
/// # Safety
/// `x` and `y` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_find_clamp(x: *mut i32, y: *mut i32, total_width: i32, total_height: i32) {
    // SAFETY: writable (the contract)
    unsafe {
        let (a, b) = find_clamp(*x, *y, total_width, total_height);
        *x = a;
        *y = b;
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_find_cached(
    x: i32,
    y: i32,
    line_x: i32,
    line_y: i32,
    line_height: i32,
    seg_x: i32,
    seg_width: i32,
) -> bool {
    find_cached(x, y, line_x, line_y, line_height, seg_x, seg_width)
}

/// `find_pick` over the line's `count` segments: the index, -1 for no segment, -2 below the line.
///
/// # Safety
/// `xs` and `widths` hold `count` ints (or are NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_find_pick(
    x: i32,
    y: i32,
    line_y: i32,
    line_height: i32,
    xs: *const i32,
    widths: *const i32,
    count: usize,
    lenient: bool,
) -> i64 {
    // SAFETY: `count` ints each (the contract)
    let (a, b) = unsafe { (slice(xs, count), slice(widths, count)) };
    match find_pick(x, y, line_y, line_height, a, b, lenient) {
        None => -2,
        Some(None) => -1,
        Some(Some(i)) => i as i64,
    }
}

/// A unit's width (`i` a position in the segment's text), read from the C++ font.
pub type EeWonCharWidthOf = extern "C" fn(ctx: *const c_void, i: i32) -> i32;

#[no_mangle]
pub extern "C" fn ee_won_gui_text_char_at(
    width: i32,
    start: i32,
    end: i32,
    abs_start: i32,
    char_width: EeWonCharWidthOf,
    ctx: *const c_void,
) -> i32 {
    char_at(width, start, end, abs_start, |i| char_width(ctx, i))
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_base_char_at(
    x: i32,
    seg_x: i32,
    seg_width: i32,
    abs_start: i32,
    abs_end: i32,
) -> i32 {
    base_char_at(x, seg_x, seg_width, abs_start, abs_end)
}

/// `check_sel` in place.
///
/// # Safety
/// The three are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_check_sel(
    start: *mut i32,
    end: *mut i32,
    backwards: *mut bool,
    at: i32,
    new_start: bool,
) {
    // SAFETY: writable (the contract)
    unsafe {
        let (s, e, b) = check_sel(*start, *end, *backwards, at, new_start);
        *start = s;
        *end = e;
        *backwards = b;
    }
}

/// `drag_scroll`: false while inside, else true with the offsets in place.
///
/// # Safety
/// `vert` and `horz` are writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_text_drag_scroll(
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    full_line: bool,
    down: i32,
    up: i32,
    vert: *mut i32,
    horz: *mut i32,
) -> bool {
    // SAFETY: writable (the contract)
    unsafe {
        match drag_scroll(x, y, width, height, full_line, down, up, *vert, *horz) {
            Some((v, h)) => {
                *vert = v;
                *horz = h;
                true
            }
            None => false,
        }
    }
}

/// `key_scroll`: false for another key, else true with the offsets in place.
///
/// # Safety
/// `vert` and `horz` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_key_scroll(
    key: i32,
    vert: *mut i32,
    horz: *mut i32,
    down: i32,
    up: i32,
    view_height: i32,
) -> bool {
    // SAFETY: writable (the contract)
    unsafe {
        match key_scroll(key, *vert, *horz, down, up, view_height) {
            Some((v, h)) => {
                *vert = v;
                *horz = h;
                true
            }
            None => false,
        }
    }
}

/// `sel_part`: false when nothing is selected, else true with the offset and length in the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_sel_part(
    sel_start: i32,
    sel_end: i32,
    abs_start: i32,
    abs_end: i32,
    out_offset: *mut i32,
    out_len: *mut i32,
) -> bool {
    match sel_part(sel_start, sel_end, abs_start, abs_end) {
        Some((o, l)) => {
            // SAFETY: writable (the contract)
            unsafe {
                *out_offset = o;
                *out_len = l;
            }
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_sel_break(
    text: bool,
    line_break: bool,
    sel_start: i32,
    sel_end: i32,
    abs_start: i32,
    abs_end: i32,
) -> bool {
    sel_break(text, line_break, sel_start, sel_end, abs_start, abs_end)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_text_anchor_line(line: i32, count: usize) -> i32 {
    anchor_line(line, count)
}

/// The font's width of a text's units from `start` to its end, read from the C++ font.
pub type EeWonRestWidthOf = extern "C" fn(ctx: *const c_void, start: i32) -> i32;

/// `wrap_segment` over a `GUIString`'s units with their widths: the first `room` steps into `out` (`3 * len + 8`
/// is enough), their count returned (all of them, even past the room).
///
/// # Safety
/// `text` holds `len` units of `wchar_size` bytes and `widths` `len` ints (or both are NULL with 0); `out` has room
/// for `room` steps.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_text_wrap(
    text: *const c_void,
    len: u16,
    wchar_size: u32,
    widths: *const i32,
    box_width: i32,
    line_width: i32,
    line_empty: bool,
    align_left: bool,
    indent: i32,
    rest_width: EeWonRestWidthOf,
    ctx: *const c_void,
    out: *mut EeWonTextWrapOp,
    room: usize,
) -> usize {
    let n = usize::from(len);
    // SAFETY: `len` units and widths (the contract)
    let (t, w) = unsafe { (wide(text, n, wchar_size), slice(widths, n)) };
    let ops = wrap_segment(&t, w, box_width, line_width, line_empty, align_left, indent, |s| rest_width(ctx, s));
    // SAFETY: room for `room` (the contract)
    unsafe { emit(&ops, out, room) }
}

/// `split_lines` over a `GUIString`'s units: the first `room` parts into `out` (`len` is enough), their count.
///
/// # Safety
/// `text` holds `len` units of `wchar_size` bytes (or is NULL with 0); `out` has room for `room` parts.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_split(
    text: *const c_void,
    len: u16,
    wchar_size: u32,
    line_break: bool,
    out: *mut EeWonTextPart,
    room: usize,
) -> usize {
    // SAFETY: `len` units (the contract)
    let t = unsafe { wide(text, usize::from(len), wchar_size) };
    // SAFETY: room for `room` (the contract)
    unsafe { emit(&split_lines(&t, line_break), out, room) }
}

/// `format_ops` over a `GUIString`'s units: the first `room` steps into `out` (`2 * len + 2` is enough), their
/// count.
///
/// # Safety
/// `text` holds `len` units of `wchar_size` bytes (or is NULL with 0); `out` has room for `room` steps.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_text_format(
    text: *const c_void,
    len: u16,
    wchar_size: u32,
    num_strings: i32,
    num_colors: i32,
    num_fonts: i32,
    num_segments: i32,
    line_break: bool,
    out: *mut EeWonFormatOp,
    room: usize,
) -> usize {
    // SAFETY: `len` units (the contract)
    let t = unsafe { wide(text, usize::from(len), wchar_size) };
    let ops = format_ops(&t, [num_strings, num_colors, num_fonts, num_segments], line_break);
    // SAFETY: room for `room` (the contract)
    unsafe { emit(&ops, out, room) }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn t(s: &str) -> Vec<u32> {
        s.chars().map(u32::from).collect()
    }

    #[test]
    fn metrics() {
        let mut l = EeWonTextLineM::default();
        let mut total = 0;
        assert!(add_seg_metrics(&mut l, 14, 11, &mut total));
        assert_eq!((l, total), (EeWonTextLineM { height: 14, baseline: 11, descent: 3 }, 14));
        // a taller ascent with a smaller descent: height = baseline + descent
        assert!(add_seg_metrics(&mut l, 14, 13, &mut total));
        assert_eq!((l, total), (EeWonTextLineM { height: 16, baseline: 13, descent: 3 }, 16));
        assert!(!add_seg_metrics(&mut l, 10, 8, &mut total));
        assert_eq!(align_x(100, 40, 1), 30);
        assert_eq!(align_x(100, 40, 2), 60);
        assert_eq!(align_x(30, 40, 2), 0);
        assert_eq!(align_x(100, 40, 0), 0);
        assert!(seg_fits(0, 90, 20, 100, true, true));
        assert!(!seg_fits(10, 90, 20, 100, true, true));
        assert!(seg_fits(10, 90, 20, 100, true, false));
    }

    #[test]
    fn bottom_and_limits() {
        let y = |y, height| EeWonTextLineY { y, height };
        assert!(!should_scroll_on_bottom(0, false, 0, y(0, 0), y(0, 0), 0, 10));
        assert!(should_scroll_on_bottom(4, false, 0, y(0, 0), y(0, 0), 0, 10));
        assert!(!should_scroll_on_bottom(4, true, 0, y(0, 0), y(0, 0), 0, 10));
        // the empty last line: the one before decides
        assert!(should_scroll_on_bottom(4, true, 2, y(100, 0), y(5, 5), 0, 10));
        assert!(!should_scroll_on_bottom(4, true, 2, y(100, 0), y(50, 5), 0, 10));
        assert!(delay_wrap(0x20, 5001) && !delay_wrap(0x20, 5000) && !delay_wrap(0, 9000));
        assert!(
            !delayed_wrap_due(1, 0)
                && delayed_wrap_due(2, 0)
                && !delayed_wrap_due(2, 48000)
                && delayed_wrap_due(10, 1 << 30)
        );
        assert_eq!(max_chars_excess(10, 15), Some(5));
        assert_eq!(max_chars_excess(0, 15), None);
        assert_eq!(max_chars_cut(&[3, 6, 9], 5), 1);
        assert_eq!(max_chars_cut(&[3, 6, 9], 50), 2);
        assert_eq!(max_lines_cut(&[false, true, true], 2), 2);
        assert_eq!(max_lines_cut(&[false, true, false], 1), 1);
        assert_eq!(max_lines_cut(&[false, false], 1), 1);
        let f = [0, LINE_IS_SEG_LINE | LINE_HAS_SEGS | LINE_LAST_IS_SEG, LINE_HAS_SEGS, LINE_HAS_SEGS];
        assert_eq!(remove_lines_stop(4, |i| f[i]), 2);
        assert_eq!(remove_lines_stop(1, |i| f[i]), 1);
    }

    #[test]
    fn renumbers() {
        let (w, h) = ([10, 30, 20], [5, 6, 7]);
        let mut ys = [11, 16, 22];
        let (mut s, mut e) = ([4, 6, 9], [6, 9, 12]);
        let (mut a, mut b, mut o) = (2, 8, 20);
        let r = renumber(Renumber {
            line_widths: &w,
            line_heights: &h,
            line_ys: &mut ys,
            seg_starts: &mut s,
            seg_ends: &mut e,
            seg_breaks: &[true, false, true],
            sel_start: &mut a,
            sel_end: &mut b,
            offset: &mut o,
        });
        assert_eq!(r, (30, 18, 2));
        assert_eq!((ys, s, e, a, b, o), ([0, 5, 11], [0, 2, 5], [2, 5, 8], 0, 4, 9));
    }

    #[test]
    fn find_and_select() {
        assert_eq!(find_clamp(50, -3, 40, 20), (39, 0));
        assert_eq!(find_clamp(50, 30, 0, 20), (50, 19));
        assert!(find_cached(15, 5, 5, 0, 10, 0, 10));
        assert!(!find_cached(16, 5, 5, 0, 10, 0, 10));
        assert_eq!(find_pick(12, 0, 0, 10, &[0, 10, 20], &[10, 10, 10], false), Some(Some(1)));
        assert_eq!(find_pick(50, 0, 0, 10, &[0, 10, 20], &[10, 10, 10], true), Some(Some(2)));
        assert_eq!(find_pick(5, 0, 0, 10, &[10], &[10], false), Some(None));
        assert_eq!(find_pick(5, 20, 0, 10, &[0], &[10], false), None);
        assert_eq!(find_pick(5, 20, 0, 10, &[0], &[10], true), Some(Some(0)));
        assert_eq!(find_pick(5, 0, 0, 10, &[], &[], true), Some(None));
        // 10 wide units: 14 rounds down to 1, 16 up to 2
        assert_eq!(char_at(14, 0, 5, 100, |_| 10), 101);
        assert_eq!(char_at(16, 0, 5, 100, |_| 10), 102);
        assert_eq!(char_at(500, 0, 5, 0, |_| 10), 5);
        assert_eq!(char_at(-5, 2, 5, 0, |_| 10), 2);
        assert_eq!(base_char_at(4, 0, 10, 7, 8), 7);
        assert_eq!(base_char_at(5, 0, 10, 7, 8), 8);
        assert_eq!(check_sel(1, 2, false, 5, true), (5, 5, false));
        assert_eq!(check_sel(3, 6, false, 1, false), (1, 3, true));
        assert_eq!(check_sel(1, 3, true, 2, false), (2, 3, true));
        assert_eq!(drag_scroll(5, 5, 10, 10, false, 0, 0, 0, 0), None);
        assert_eq!(drag_scroll(-3, 14, 10, 10, true, 12, 0, 0, 0), Some((12, -3)));
        assert_eq!(drag_scroll(14, -2, 10, 10, true, 0, 9, 20, 0), Some((11, 4)));
        assert_eq!(key_scroll(0x22, 0, 0, 0, 0, 50), Some((50, 0)));
        assert_eq!(key_scroll(0x41, 0, 0, 0, 0, 50), None);
        assert_eq!(sel_part(2, 20, 5, 10), Some((0, 5)));
        assert_eq!(sel_part(7, 8, 5, 10), Some((2, 1)));
        assert_eq!(sel_part(10, 20, 5, 10), None);
        assert!(sel_break(true, true, 6, 11, 5, 10) && !sel_break(true, true, 6, 10, 5, 10));
        assert!(sel_break(false, true, 5, 11, 5, 5) && !sel_break(false, true, 6, 11, 5, 5));
        assert_eq!(anchor_line(5, 3), 2);
        assert_eq!(anchor_line(-4, 3), 2);
        assert_eq!(anchor_line(-4, 0), 0);
        assert_eq!(anchor_line(1, 3), 1);
    }

    #[test]
    fn wraps() {
        let p = |start, end, width| EeWonTextWrapOp { kind: 0, start, end, width };
        let nl = |width| EeWonTextWrapOp { kind: 1, start: 0, end: 0, width };
        let s = t("ab cd ef");
        let w = vec![5; s.len()];
        let rest = |s: i32| (8 - s) * 5;
        // the first break at the space after "cd" (its width included), the rest whole on the next line
        assert_eq!(wrap_segment(&s, &w, 32, 0, true, true, 0, rest), vec![p(0, 5, 30), nl(0), p(6, 8, 10)]);
        // an indent on the wrapped lines: the rest no longer fits, a forced break as wide as the view
        assert_eq!(
            wrap_segment(&s, &w, 32, 0, true, true, 25, rest),
            vec![p(0, 5, 30), nl(25), p(6, 7, 32), nl(25), p(7, 8, 5)]
        );
        // a word longer than the line: forced breaks as wide as the view
        let s = t("abcdefgh");
        let rest = |s: i32| (8 - s) * 5;
        assert_eq!(
            wrap_segment(&s, &[5; 8], 12, 0, true, false, 0, rest),
            vec![p(0, 2, 12), nl(0), p(2, 4, 12), nl(0), p(4, 6, 12), nl(0), p(6, 8, 10)]
        );
        // a line with segments: the whole text moves to a new line
        assert_eq!(wrap_segment(&s, &[5; 8], 50, 30, false, true, 0, rest), vec![nl(0), p(0, 8, 40)]);
        assert_eq!(wrap_segment(&[], &[], 10, 0, true, true, 0, rest), vec![]);
    }

    #[test]
    fn splits_and_formats() {
        let p = |start, len, line_break| EeWonTextPart { start, len, line_break };
        assert_eq!(
            split_lines(&t("ab\r\ncd\ne\rf"), false),
            vec![p(0, 2, true), p(4, 2, true), p(7, 1, true), p(9, 1, false)]
        );
        assert_eq!(split_lines(&t("ab\n"), false), vec![p(0, 2, true)]);
        assert_eq!(split_lines(&t("ab"), true), vec![p(0, 2, true)]);
        assert_eq!(split_lines(&[], true), vec![]);
        let o = |kind, a, b| EeWonFormatOp { kind, a, b };
        assert_eq!(
            format_ops(&t("hi <c2>x<<y<N><s1>"), [1, 2, 0, 0], false),
            vec![
                o(0, 0, 3),
                o(2, 0, 0),
                o(4, 1, 0),
                o(0, 7, 1),
                o(1, 0, 0),
                o(0, 10, 1),
                o(2, 0, 0),
                o(3, 0, 0),
                o(0, 14, 0),
                o(6, 0, 0),
            ]
        );
        // an index out of range does nothing; a trailing '<' ends the parse
        assert_eq!(format_ops(&t("<f3>a<"), [0, 0, 2, 0], true), vec![o(0, 0, 0), o(0, 4, 1), o(8, 0, 1)]);
        assert_eq!(format_ops(&[], [0; 4], false), vec![]);
    }
}
