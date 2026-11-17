//! WONGUI's layout arithmetic (Rectangle.cpp, Component.cpp's sizes, ChildLayouts.cpp, ChildLayoutsAdvanced.cpp,
//! CenterLayoutExecute.cpp, ContainerConfigInstructions.cpp's LAYOUT rule). The C++ keeps the components, the
//! layout objects and their lists, and every `SetPos` / `SetSize` call (and so the position and size hooks they
//! run); these answer the numbers:
//!
//! - `WONRectangle`'s intersection, union and overlap test;
//! - `Component::SetSize`'s clamp and no-change test, `SetSizeAtLeast`'s grow test;
//! - a child layout instruction's rectangle from its flags, offsets and reference (`ChildLayoutInstruction`);
//! - the centre, same-size, clear-right, space-evenly, no-overlap, size-at-least, percentage and vertical flow
//!   layouts;
//! - the LAYOUT rule's instruction names and which of its four offsets a chained or repeated rule zeroes.
//!
//! Every sum is the 32-bit `int` of the original, wrapping (the game is built with `-fwrapv`).

/// A `WONRectangle` (x, y, width, height).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonRect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

impl EeWonRect {
    pub const fn new(x: i32, y: i32, width: i32, height: i32) -> Self {
        EeWonRect { x, y, width, height }
    }
    fn right(&self) -> i32 {
        self.x.wrapping_add(self.width)
    }
    fn bottom(&self) -> i32 {
        self.y.wrapping_add(self.height)
    }
}

/// `DoIntersect`: the overlap, an empty size (0) on an axis where they do not overlap.
pub fn intersect(a: EeWonRect, b: EeWonRect) -> EeWonRect {
    let mut right = a.right();
    let mut bottom = a.bottom();
    let mut r = a;
    if b.x > r.x {
        r.x = b.x;
    }
    if b.y > r.y {
        r.y = b.y;
    }
    if b.right() < right {
        right = b.right();
    }
    if b.bottom() < bottom {
        bottom = b.bottom();
    }
    r.width = right.wrapping_sub(r.x).max(0);
    r.height = bottom.wrapping_sub(r.y).max(0);
    r
}

/// `DoUnion`: both covered; a rectangle with a zero width or height becomes the other.
pub fn union(a: EeWonRect, b: EeWonRect) -> EeWonRect {
    if a.width == 0 || a.height == 0 {
        return b;
    }
    let mut right = a.right();
    let mut bottom = a.bottom();
    let mut r = a;
    if b.x < r.x {
        r.x = b.x;
    }
    if b.y < r.y {
        r.y = b.y;
    }
    if b.right() > right {
        right = b.right();
    }
    if b.bottom() > bottom {
        bottom = b.bottom();
    }
    r.width = right.wrapping_sub(r.x);
    r.height = bottom.wrapping_sub(r.y);
    r
}

/// `Intersects`: touching this rectangle's right or bottom edge counts, touching the other's does not.
pub fn intersects(a: EeWonRect, b: EeWonRect) -> bool {
    a.right() >= b.x && a.x < b.right() && a.bottom() >= b.y && a.y < b.bottom()
}

/// `Component::SetSize`: the size it takes (negative sizes 0), or `None` when it is the current one.
pub fn set_size(cur_width: i32, cur_height: i32, width: i32, height: i32) -> Option<(i32, i32)> {
    let (w, h) = (width.max(0), height.max(0));
    if w == cur_width && h == cur_height {
        None
    } else {
        Some((w, h))
    }
}

/// `Component::SetSizeAtLeast`: the size to set (`SetSize`), or `None` when it already is at least that big.
pub fn size_at_least(cur_width: i32, cur_height: i32, width: i32, height: i32) -> Option<(i32, i32)> {
    let w = width.max(cur_width);
    let h = height.max(cur_height);
    if w > cur_width || h > cur_height {
        Some((w, h))
    } else {
        None
    }
}

/// A child layout instruction (`ChildLayoutInstruction`): its flags and the four offsets.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonChildLayout {
    pub flags: i32,
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

/// `ChildLayoutInstruction::Execute`: the child's new rectangle. `reference` is the reference's bounds; when the
/// reference is the child's parent its position counts as (0, 0). The flags apply from the lowest bit up, each
/// seeing the earlier ones' results.
pub fn child_layout(child: EeWonRect, reference: EeWonRect, ref_is_parent: bool, l: EeWonChildLayout) -> EeWonRect {
    let (left, top) = if ref_is_parent { (0, 0) } else { (reference.x, reference.y) };
    let right = left.wrapping_add(reference.width);
    let bottom = top.wrapping_add(reference.height);
    let EeWonRect { mut x, mut y, mut width, mut height } = child;
    let mut flag = 1i32;
    while flag < 0x100000 {
        if l.flags & flag != 0 {
            match flag {
                0x00001 => width = l.width.wrapping_add(reference.width),
                0x00002 => height = l.height.wrapping_add(reference.height),
                0x00100 => y = l.y.wrapping_sub(height).wrapping_add(top),
                0x00200 => y = l.y.wrapping_add(bottom),
                0x00400 => x = l.x.wrapping_add(right),
                0x00800 => x = l.x.wrapping_sub(width).wrapping_add(left),
                0x01000 => x = l.x.wrapping_add(left),
                0x02000 => x = l.x.wrapping_sub(width).wrapping_add(right),
                0x04000 => y = l.y.wrapping_add(top),
                0x08000 => y = l.y.wrapping_sub(height).wrapping_add(bottom),
                0x10000 => width = l.width.wrapping_sub(x).wrapping_add(right),
                0x20000 => width = l.width.wrapping_sub(x).wrapping_add(left),
                0x40000 => height = l.height.wrapping_sub(y).wrapping_add(top),
                0x80000 => height = l.height.wrapping_sub(y).wrapping_add(bottom),
                _ => {}
            }
        }
        flag <<= 1;
    }
    EeWonRect { x, y, width, height }
}

/// `CenterLayout::Execute`'s shift (dx, dy) of the components (their bounds in `bounds`) to centre their union in
/// the centre rectangle: the parent's size (`center_is_box` false: the parent, or no centre box) or the centre
/// box's bounds. `None`: no components, or no shift.
pub fn center_shift(
    bounds: &[EeWonRect],
    center: EeWonRect,
    center_is_box: bool,
    horz: bool,
    vert: bool,
) -> Option<(i32, i32)> {
    if bounds.is_empty() {
        return None;
    }
    let mut u = EeWonRect::default();
    for b in bounds {
        u = union(u, *b);
    }
    let (mut dx, mut dy) = (0i32, 0i32);
    if center_is_box {
        if horz {
            dx = center.x;
        }
        if vert {
            dy = center.y;
        }
    }
    if horz {
        dx = dx.wrapping_add(center.width.wrapping_sub(u.width) / 2).wrapping_sub(u.x);
    }
    if vert {
        dy = dy.wrapping_add(center.height.wrapping_sub(u.height) / 2).wrapping_sub(u.y);
    }
    if dx == 0 && dy == 0 {
        None
    } else {
        Some((dx, dy))
    }
}

/// `SameSizeLayout::Execute`'s size: the largest width (`horz`) and height (`vert`) of the sizes, 0 for an axis
/// not asked for. Each component then takes (width or its own, height or its own).
pub fn same_size(sizes: &[(i32, i32)], horz: bool, vert: bool) -> (i32, i32) {
    let (mut w, mut h) = (0, 0);
    for &(cw, ch) in sizes {
        if horz && cw > w {
            w = cw;
        }
        if vert && ch > h {
            h = ch;
        }
    }
    (w, h)
}

/// `ClearRightLayout::Execute`'s x: the largest right edge (at least 0) plus the padding.
pub fn clear_right(rights: &[i32], pad: i32) -> i32 {
    rights.iter().fold(0, |m, &r| if r > m { r } else { m }).wrapping_add(pad)
}

/// `SpaceEvenlyLayout::Execute`'s positions on its axis for the components of `extents` (their widths or
/// heights) across the space box (`box_pos`, `box_extent`; position 0 when the box is the parent): equal gaps,
/// the leftover pixels one each to the first gaps. Empty for no components.
pub fn space_evenly(
    extents: &[i32],
    box_pos: i32,
    box_extent: i32,
    box_is_parent: bool,
    left_pad: i32,
    right_pad: i32,
) -> Vec<i32> {
    if extents.is_empty() {
        return Vec::new();
    }
    let total = extents.iter().fold(0i32, |s, &e| s.wrapping_add(e));
    let available = box_extent.wrapping_sub(right_pad).wrapping_sub(left_pad).wrapping_sub(total).max(0);
    let (spacing, mut remainder) = if extents.len() > 1 {
        let gaps = (extents.len() - 1) as i64;
        ((i64::from(available) / gaps) as i32, (i64::from(available) % gaps) as i32)
    } else {
        (0, 0)
    };
    let mut position = if box_is_parent { 0 } else { box_pos };
    position = position.wrapping_add(left_pad);
    let mut out = Vec::with_capacity(extents.len());
    for &e in extents {
        out.push(position);
        position = position.wrapping_add(e).wrapping_add(spacing);
        if remainder > 0 {
            position = position.wrapping_add(1);
            remainder -= 1;
        }
    }
    out
}

/// `NoOverlapLayout::Execute`'s shift (dx, dy) of the target (and the components that move with it): clear of
/// a sibling reference, or back inside the parent (`ref_is_parent`: only the reference's size counts), along the
/// sides of `kind` (0x800 left, else 0x400 right; 0x100 top, else 0x200 bottom). `None`: no shift.
pub fn no_overlap(
    target: EeWonRect,
    reference: EeWonRect,
    ref_is_parent: bool,
    kind: i32,
    horz_pad: i32,
    vert_pad: i32,
) -> Option<(i32, i32)> {
    let t = target;
    let r = reference;
    let (mut dx, mut dy) = (0i32, 0i32);
    if ref_is_parent {
        if kind & 0x800 != 0 {
            dx = horz_pad.wrapping_sub(t.x).max(0);
        } else if kind & 0x400 != 0 {
            dx = r.width.wrapping_sub(t.width).wrapping_add(horz_pad).wrapping_sub(t.x).min(0);
        }
        if kind & 0x100 != 0 {
            dy = vert_pad.wrapping_sub(t.y).max(0);
        } else if kind & 0x200 != 0 {
            dy = r.height.wrapping_sub(t.height).wrapping_sub(t.y).wrapping_add(vert_pad).min(0);
        }
    } else {
        if kind & 0x800 != 0 {
            dx = r.x.wrapping_sub(t.width).wrapping_add(horz_pad).wrapping_sub(t.x).min(0);
        } else if kind & 0x400 != 0 {
            dx = r.right().wrapping_sub(t.x).wrapping_add(horz_pad).max(0);
        }
        if kind & 0x100 != 0 {
            dy = r.y.wrapping_sub(t.height).wrapping_sub(t.y).wrapping_add(vert_pad).min(0);
        } else if kind & 0x200 != 0 {
            dy = r.height.wrapping_sub(t.y).wrapping_add(r.y).wrapping_add(vert_pad).max(0);
        }
    }
    if dx == 0 && dy == 0 {
        None
    } else {
        Some((dx, dy))
    }
}

/// `SizeAtLeastLayout::Execute`'s size for `SetSizeAtLeast`: the largest width and height (at least 0) plus the
/// padding.
pub fn size_at_least_layout(sizes: &[(i32, i32)], horz_pad: i32, vert_pad: i32) -> (i32, i32) {
    let (w, h) =
        sizes.iter().fold((0, 0), |(w, h), &(cw, ch)| (if cw > w { cw } else { w }, if ch > h { ch } else { h }));
    (w.wrapping_add(horz_pad), h.wrapping_add(vert_pad))
}

/// `PercentLayoutParam::SetBorders`' anchors as indices 0-3 (left, top, right, bottom; -1 none) of the four
/// given ones: a missing edge takes the opposite edge on its axis, then the last anchor given.
pub fn percent_borders(given: [bool; 4]) -> [i32; 4] {
    // the original's statements over the slots' identities: each slot holds the index of the anchor it took
    let id = |i: usize| if given[i] { i as i32 } else { -1 };
    let (mut l, mut t, mut r, mut b) = (id(0), id(1), id(2), id(3));
    let mut last = -1;
    if given[0] {
        last = 0;
    } else {
        l = r;
    }
    if given[2] {
        last = 2;
    } else {
        r = l;
    }
    if given[1] {
        last = 1;
    } else {
        t = b;
    }
    if given[3] {
        last = 3;
    } else {
        b = t;
    }
    if l < 0 {
        l = last;
    }
    if r < 0 {
        r = last;
    }
    if t < 0 {
        t = last;
    }
    if b < 0 {
        b = last;
    }
    [l, t, r, b]
}

/// A percentage layout's numbers (`PercentLayoutParam`): percentages (negative: leave that coordinate) and the
/// padding inside each anchor edge.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonPercent {
    pub px: i32,
    pub py: i32,
    pub pw: i32,
    pub ph: i32,
    pub lp: i32,
    pub tp: i32,
    pub rp: i32,
    pub bp: i32,
}

/// `PercentLayoutParam::DoCalc`: the target's rectangle between its anchors' edges. `edges` are the left
/// anchor's left, the right anchor's right, the top anchor's top and the bottom anchor's bottom, each already
/// 0 or the parent's width / height where that anchor is the parent.
pub fn percent_layout(target: EeWonRect, edges: [i32; 4], p: EeWonPercent) -> EeWonRect {
    let EeWonRect { mut x, mut y, mut width, mut height } = target;
    if p.px >= 0 || p.pw >= 0 {
        let l = edges[0].wrapping_add(p.lp);
        let r = edges[1].wrapping_sub(p.rp);
        let span = r.wrapping_sub(l);
        if p.px >= 0 {
            x = l.wrapping_add(span.wrapping_mul(p.px) / 100);
        }
        if p.pw >= 0 {
            width = span.wrapping_mul(p.pw) / 100;
        }
    }
    if p.py >= 0 || p.ph >= 0 {
        let t = edges[2].wrapping_add(p.tp);
        let b = edges[3].wrapping_sub(p.bp);
        let span = b.wrapping_sub(t);
        if p.py >= 0 {
            y = t.wrapping_add(span.wrapping_mul(p.py) / 100);
        }
        if p.ph >= 0 {
            height = span.wrapping_mul(p.ph) / 100;
        }
    }
    EeWonRect { x, y, width, height }
}

/// One component of a vertical flow layout: its paddings, its x and its height.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonFlowItem {
    pub top_pad: i32,
    pub left_pad: i32,
    pub left: i32,
    pub height: i32,
}

/// `VerticalFlowLayout::Execute`'s positions: stacked from (`left`, `top`) (the top component's left and bottom,
/// or (0, 0) when it is the first item's parent), each item below the one before plus its top padding, at the
/// left plus its left padding (a negative padding keeps its x).
pub fn vertical_flow(items: &[EeWonFlowItem], left: i32, top: i32) -> Vec<(i32, i32)> {
    let mut y = top;
    items
        .iter()
        .map(|it| {
            y = y.wrapping_add(it.top_pad);
            let x = if it.left_pad >= 0 { left.wrapping_add(it.left_pad) } else { it.left };
            let p = (x, y);
            y = y.wrapping_add(it.height);
            p
        })
        .collect()
}

/// The LAYOUT rule's instruction names and their flags, in the order the original tested them.
pub const LAYOUT_KEYWORDS: [(&str, i32); 15] = [
    ("SameWidth", 0x1),
    ("SameHeight", 0x2),
    ("Above", 0x100),
    ("Below", 0x200),
    ("Right", 0x400),
    ("Left", 0x800),
    ("SameLeft", 0x1000),
    ("SameRight", 0x2000),
    ("SameTop", 0x4000),
    ("SameBottom", 0x8000),
    ("GrowToRight", 0x10000),
    ("GrowToLeft", 0x20000),
    ("GrowToTop", 0x40000),
    ("GrowToBottom", 0x80000),
    ("SameSize", 0x3),
];

/// The LAYOUT rule's offsets: whether the ones it did not give (from `read`, of 4) are zeroed. A chained or
/// repeated rule that gave none keeps the previous rule's.
pub fn layout_zero_rest(chain: bool, repeat: bool, read: i32) -> bool {
    (!chain && !repeat) || read != 0
}

// ---- the C ABI

/// The LAYOUT keyword `index` (0-14) as a NUL-terminated ASCII name and its flag; false past the table.
///
/// # Safety
/// Both outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_layout_keyword(index: i32, out_name: *mut *const u8, out_flag: *mut i32) -> bool {
    const NAMES: [&[u8]; 15] = [
        b"SameWidth\0",
        b"SameHeight\0",
        b"Above\0",
        b"Below\0",
        b"Right\0",
        b"Left\0",
        b"SameLeft\0",
        b"SameRight\0",
        b"SameTop\0",
        b"SameBottom\0",
        b"GrowToRight\0",
        b"GrowToLeft\0",
        b"GrowToTop\0",
        b"GrowToBottom\0",
        b"SameSize\0",
    ];
    if index < 0 || index as usize >= LAYOUT_KEYWORDS.len() {
        return false;
    }
    let i = index as usize;
    // SAFETY: writable (the contract)
    unsafe {
        *out_name = NAMES[i].as_ptr();
        *out_flag = LAYOUT_KEYWORDS[i].1;
    }
    true
}

#[no_mangle]
pub extern "C" fn ee_won_gui_layout_zero_rest(chain: bool, repeat: bool, read: i32) -> bool {
    layout_zero_rest(chain, repeat, read)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_rect_intersect(a: EeWonRect, b: EeWonRect) -> EeWonRect {
    intersect(a, b)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_rect_union(a: EeWonRect, b: EeWonRect) -> EeWonRect {
    union(a, b)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_rect_intersects(a: EeWonRect, b: EeWonRect) -> bool {
    intersects(a, b)
}

/// `set_size`: true with the size to set in the outs, false when nothing changes.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_set_size(
    cur_width: i32,
    cur_height: i32,
    width: i32,
    height: i32,
    out_w: *mut i32,
    out_h: *mut i32,
) -> bool {
    write_size(set_size(cur_width, cur_height, width, height), out_w, out_h)
}

/// `size_at_least`: true with the size to set in the outs, false when nothing changes.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_size_at_least(
    cur_width: i32,
    cur_height: i32,
    width: i32,
    height: i32,
    out_w: *mut i32,
    out_h: *mut i32,
) -> bool {
    write_size(size_at_least(cur_width, cur_height, width, height), out_w, out_h)
}

unsafe fn write_size(s: Option<(i32, i32)>, out_a: *mut i32, out_b: *mut i32) -> bool {
    match s {
        Some((a, b)) => {
            // SAFETY: writable (the callers' contract)
            unsafe {
                *out_a = a;
                *out_b = b;
            }
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_child_layout(
    child: EeWonRect,
    reference: EeWonRect,
    ref_is_parent: bool,
    layout: EeWonChildLayout,
) -> EeWonRect {
    child_layout(child, reference, ref_is_parent, layout)
}

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: `n` entries (the callers' contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// `center_shift` over `count` bounds: true with the shift in the outs.
///
/// # Safety
/// `bounds` holds `count` rectangles (or is NULL with 0); the outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_center_shift(
    bounds: *const EeWonRect,
    count: usize,
    center: EeWonRect,
    center_is_box: bool,
    horz: bool,
    vert: bool,
    out_dx: *mut i32,
    out_dy: *mut i32,
) -> bool {
    // SAFETY: the contract
    unsafe { write_size(center_shift(slice(bounds, count), center, center_is_box, horz, vert), out_dx, out_dy) }
}

/// `same_size` (`which` 0) or `size_at_least_layout` (1, with the paddings) over `count` (width, height) pairs
/// as `2 * count` ints, into the outs.
///
/// # Safety
/// `sizes` holds `2 * count` ints (or is NULL with 0); the outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_max_size(
    which: i32,
    sizes: *const i32,
    count: usize,
    horz: bool,
    vert: bool,
    horz_pad: i32,
    vert_pad: i32,
    out_w: *mut i32,
    out_h: *mut i32,
) {
    // SAFETY: the contract
    let s = unsafe { slice(sizes, count * 2) };
    let pairs: Vec<(i32, i32)> = s.as_chunks::<2>().0.iter().map(|c| (c[0], c[1])).collect();
    let (w, h) =
        if which == 0 { same_size(&pairs, horz, vert) } else { size_at_least_layout(&pairs, horz_pad, vert_pad) };
    // SAFETY: writable (the contract)
    unsafe {
        *out_w = w;
        *out_h = h;
    }
}

/// `clear_right` over `count` right edges.
///
/// # Safety
/// `rights` holds `count` ints (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_clear_right(rights: *const i32, count: usize, pad: i32) -> i32 {
    // SAFETY: the contract
    clear_right(unsafe { slice(rights, count) }, pad)
}

/// `space_evenly` over `count` extents, the positions into `out` (`count` ints).
///
/// # Safety
/// `extents` holds and `out` has room for `count` ints (or both are NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_space_evenly(
    extents: *const i32,
    count: usize,
    box_pos: i32,
    box_extent: i32,
    box_is_parent: bool,
    left_pad: i32,
    right_pad: i32,
    out: *mut i32,
) {
    // SAFETY: the contract
    let p = space_evenly(unsafe { slice(extents, count) }, box_pos, box_extent, box_is_parent, left_pad, right_pad);
    for (i, v) in p.into_iter().enumerate() {
        // SAFETY: room for `count` (the contract)
        unsafe { *out.add(i) = v };
    }
}

/// `no_overlap`: true with the shift in the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_no_overlap(
    target: EeWonRect,
    reference: EeWonRect,
    ref_is_parent: bool,
    kind: i32,
    horz_pad: i32,
    vert_pad: i32,
    out_dx: *mut i32,
    out_dy: *mut i32,
) -> bool {
    // SAFETY: the contract
    unsafe { write_size(no_overlap(target, reference, ref_is_parent, kind, horz_pad, vert_pad), out_dx, out_dy) }
}

/// `percent_borders`: bit i of `given` set when anchor i (left, top, right, bottom) was given; the chosen
/// indices into `out` (4 ints).
///
/// # Safety
/// `out` has room for 4 ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_percent_borders(given: u32, out: *mut i32) {
    let r = percent_borders([given & 1 != 0, given & 2 != 0, given & 4 != 0, given & 8 != 0]);
    for (i, v) in r.into_iter().enumerate() {
        // SAFETY: room for 4 (the contract)
        unsafe { *out.add(i) = v };
    }
}

/// `percent_layout` with the four edges (left, right, top, bottom).
///
/// # Safety
/// `edges` holds 4 ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_percent_layout(target: EeWonRect, edges: *const i32, p: EeWonPercent) -> EeWonRect {
    // SAFETY: 4 ints (the contract)
    let e = unsafe { [*edges, *edges.add(1), *edges.add(2), *edges.add(3)] };
    percent_layout(target, e, p)
}

/// `vertical_flow` over `count` items, the positions into `out` (`2 * count` ints: x, y).
///
/// # Safety
/// `items` holds `count` items and `out` has room for `2 * count` ints (or both are NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_vertical_flow(
    items: *const EeWonFlowItem,
    count: usize,
    left: i32,
    top: i32,
    out: *mut i32,
) {
    // SAFETY: the contract
    let p = vertical_flow(unsafe { slice(items, count) }, left, top);
    for (i, (x, y)) in p.into_iter().enumerate() {
        // SAFETY: room for `2 * count` (the contract)
        unsafe {
            *out.add(2 * i) = x;
            *out.add(2 * i + 1) = y;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const fn r(x: i32, y: i32, w: i32, h: i32) -> EeWonRect {
        EeWonRect::new(x, y, w, h)
    }

    #[test]
    fn rectangles() {
        assert_eq!(intersect(r(0, 0, 10, 10), r(5, 5, 10, 10)), r(5, 5, 5, 5));
        assert_eq!(intersect(r(0, 0, 10, 10), r(20, 20, 5, 5)), r(20, 20, 0, 0));
        assert_eq!(union(r(0, 0, 0, 10), r(5, 5, 1, 1)), r(5, 5, 1, 1));
        assert_eq!(union(r(0, 0, 10, 10), r(5, -5, 20, 1)), r(0, -5, 25, 15));
        assert!(intersects(r(0, 0, 10, 10), r(10, 10, 5, 5)));
        assert!(!intersects(r(10, 10, 5, 5), r(0, 0, 10, 10)));
        assert_eq!(set_size(3, 4, -1, 4), Some((0, 4)));
        assert_eq!(set_size(0, 4, -1, 4), None);
        assert_eq!(size_at_least(10, 10, 5, 12), Some((10, 12)));
        assert_eq!(size_at_least(10, 10, 5, 10), None);
    }

    #[test]
    fn child_layouts() {
        let l = |flags, x, y, width, height| EeWonChildLayout { flags, x, y, width, height };
        let child = r(1, 2, 30, 40);
        let refr = r(100, 200, 50, 60);
        assert_eq!(child_layout(child, refr, false, l(0x3, 1, 1, 2, 3)), r(1, 2, 52, 63));
        assert_eq!(child_layout(child, refr, false, l(0x200 | 0x1000, 5, 6, 0, 0)), r(105, 266, 30, 40));
        assert_eq!(child_layout(child, refr, true, l(0x2000 | 0x8000, -5, -6, 0, 0)), r(15, 14, 30, 40));
        // GrowToRight after Right: the width from the new x
        assert_eq!(child_layout(child, refr, true, l(0x400 | 0x10000, 2, 0, -3, 0)), r(52, 2, -5, 40));
    }

    #[test]
    fn layouts() {
        assert_eq!(center_shift(&[], r(0, 0, 100, 100), false, true, true), None);
        assert_eq!(
            center_shift(&[r(0, 0, 10, 10), r(20, 0, 10, 10)], r(0, 0, 100, 50), false, true, true),
            Some((35, 20))
        );
        assert_eq!(center_shift(&[r(35, 20, 30, 10)], r(0, 0, 100, 50), false, true, true), None);
        assert_eq!(center_shift(&[r(0, 0, 10, 10)], r(7, 9, 20, 20), true, true, false), Some((12, 0)));
        assert_eq!(same_size(&[(1, 9), (5, 2)], true, false), (5, 0));
        assert_eq!(clear_right(&[-5, -2], 3), 3);
        assert_eq!(clear_right(&[10, 30, 20], 3), 33);
        assert_eq!(space_evenly(&[10, 10, 10], 0, 100, true, 0, 0), vec![0, 45, 90]);
        assert_eq!(space_evenly(&[10, 10, 10], 0, 101, true, 0, 0), vec![0, 46, 91]);
        assert_eq!(space_evenly(&[10], 5, 100, false, 2, 0), vec![7]);
        assert_eq!(space_evenly(&[60, 60], 0, 100, true, 0, 0), vec![0, 60]);
        assert_eq!(no_overlap(r(-4, 3, 10, 10), r(0, 0, 50, 50), true, 0x800 | 0x200, 2, 0), Some((6, 0)));
        assert_eq!(no_overlap(r(45, 45, 10, 10), r(0, 0, 50, 50), true, 0x400 | 0x200, 0, 0), Some((-5, -5)));
        assert_eq!(no_overlap(r(10, 10, 10, 10), r(15, 0, 10, 50), false, 0x400, 1, 0), Some((16, 0)));
        assert_eq!(size_at_least_layout(&[(3, 4), (-1, 9)], 1, 2), (4, 11));
        assert_eq!(percent_borders([true, false, false, false]), [0, 0, 0, 0]);
        assert_eq!(percent_borders([false, true, true, false]), [2, 1, 2, 1]);
        assert_eq!(percent_borders([false, false, false, true]), [3, 3, 3, 3]);
        let p = EeWonPercent { px: 50, py: -1, pw: 25, ph: -1, lp: 10, tp: 0, rp: 10, bp: 0 };
        assert_eq!(percent_layout(r(1, 2, 3, 4), [0, 220, 0, 0], p), r(110, 2, 50, 4));
        let it = |top_pad, left_pad, left, height| EeWonFlowItem { top_pad, left_pad, left, height };
        assert_eq!(vertical_flow(&[it(2, 3, 9, 10), it(1, -1, 9, 5)], 4, 20), vec![(7, 22), (9, 33)]);
        assert!(
            layout_zero_rest(false, false, 0) && !layout_zero_rest(true, false, 0) && layout_zero_rest(false, true, 2)
        );
    }

    #[test]
    fn keywords() {
        let mut name = std::ptr::null();
        let mut flag = 0;
        // SAFETY: writable outs
        assert!(unsafe { ee_won_gui_layout_keyword(14, &mut name, &mut flag) });
        assert_eq!(flag, 3);
        // SAFETY: a NUL-terminated name from the table
        assert_eq!(unsafe { std::ffi::CStr::from_ptr(name.cast()) }.to_str().unwrap(), "SameSize");
        // SAFETY: writable outs
        assert!(!unsafe { ee_won_gui_layout_keyword(15, &mut name, &mut flag) });
    }
}
