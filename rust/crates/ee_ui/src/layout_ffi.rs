//! `extern "C"` for `UILayoutUtility`'s members (`UITrampolines.cpp`): `layout` is the C++ object itself (the pinned
//! record [`Layout`]), `parent`/`out` are `GERectangle`s.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong};

use crate::layout::{Edge, Layout};
use crate::rect::UiRect;

/// `UILayoutUtility::UILayoutUtility`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_layout_init(layout: *mut Layout) {
    layout.write(Layout::default());
}

/// `UILayoutUtility::CalculateRectangle`. The parent is read whole before `out` is written (no caller passes the same
/// rectangle twice).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_layout_calculate(
    layout: *const Layout,
    parent: *const UiRect,
    out: *mut UiRect,
    exclusive: bool,
) {
    let rect = (*layout).calculate(&parent.read(), exclusive);
    out.write(rect);
}

/// `UILayoutUtility::OffsetLeft`/`OffsetRight`/`OffsetTop`/`OffsetBottom`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_layout_offset(layout: *mut Layout, edge: Edge, amount: c_long, size: c_ulong) {
    (*layout).offset(edge, amount, size);
}

/// `UILayoutUtility::SetLeft`/`SetRight`/`SetTop`/`SetBottom`: an edge's offset type and offset.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_layout_set(layout: *mut Layout, edge: Edge, kind: c_ulong, offset: f32) {
    let l = &mut *layout;
    let (o, k) = match edge {
        Edge::Left => (&mut l.left_offset, &mut l.left_type),
        Edge::Right => (&mut l.right_offset, &mut l.right_type),
        Edge::Top => (&mut l.top_offset, &mut l.top_type),
        Edge::Bottom => (&mut l.bottom_offset, &mut l.bottom_type),
    };
    *o = offset;
    *k = kind;
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::layout::{FROM_END, FROM_START};

    #[test]
    fn the_c_entry_points_reach_the_layout() {
        let mut l = Layout { left_offset: 1.0, ..Layout::default() };
        unsafe { ee_ui_layout_init(&mut l) };
        assert_eq!(l, Layout::default());
        assert_eq!((l.left_type, l.right_type), (FROM_START, FROM_END));
        let parent = UiRect::new(0.0, 0.0, 99.0, 49.0);
        let mut out = UiRect::default();
        unsafe {
            ee_ui_layout_offset(&mut l, Edge::Right, 9, 100);
            ee_ui_layout_calculate(&l, &parent, &mut out, true);
        }
        assert_eq!(out, UiRect::new(0.0, 0.0, 107.0, 48.0));
        unsafe {
            ee_ui_layout_set(&mut l, Edge::Top, 4, 0.5);
            ee_ui_layout_set(&mut l, Edge::Bottom, 1, 2.0);
        }
        assert_eq!((l.top_type, l.top_offset, l.bottom_type, l.bottom_offset), (4, 0.5, 1, 2.0));
        assert_eq!((l.left_type, l.right_offset), (FROM_START, -9.0));
    }
}
