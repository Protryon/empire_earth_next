//! `UIComboBox.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 10): the combo box's host's
//! queries and actions of more than one call ([`combo_call`], plugged in as `ee_ui_combo_host` before
//! `UIComboBox::Rust::Op`) and its constructor's part making ([`construct`]). The combo box's state and methods were
//! already Rust (`comboobj.rs`).

use core::ffi::c_ulong;

use crate::comboobj::{
    ee_ui_combo_drop_rect, ee_ui_combo_list_rect, ee_ui_combo_text_rect, EeUiComboAct as Act, EeUiComboQuery as Query,
};
use crate::lle_cpp::{lle_host, Cpp, EeUiShimF32Out as F32Out, EeUiShimULongOut as ULongOut, HostArgs};
use crate::obj::EeUiOp as Op;
use crate::rect::UiRect;

/// The combo box's parts (`PartOf`): 0 the text box, 2 the list, 3 the edit box, 4 the label.
const PART_LIST: u32 = 2;
const PART_EDIT: u32 = 3;
const PART_LABEL: u32 = 4;
/// The manager's scroll-bar down bitmaps (`EeUiManagerCall::Bitmap`): normal, pressed.
const SB_DOWN_NORMAL: u32 = 2;
const SB_DOWN_PRESSED: u32 = 3;

/// A control's rectangle (`GetRectangle()`).
fn rect_of<C: Cpp + ?Sized>(c: &mut C, control: usize) -> UiRect {
    let mut f = [0.0f32; 4];
    c.control_rect(control, F32Out(f.as_mut_ptr()));
    UiRect { bottom: f[0], left: f[1], right: f[2], top: f[3] }
}

/// The combo box's host's moved cases; `None` for the C++ host's.
pub(crate) fn combo_call<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs) -> Option<isize> {
    let combo = h.obj;
    match h.op {
        Op::ComboQuery => query(c, h, combo, (h.a >> 24) as u32, h.a & 0xff_ffff),
        Op::ComboAct => act(c, h, combo),
        _ => None,
    }
}

fn query<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs, combo: usize, q: u32, arg: isize) -> Option<isize> {
    Some(match q {
        q if q == Query::ListScrollBarRect as u32 => {
            let list = c.combo_part(combo, PART_LIST);
            let bar = c.list_scroll_bar(list);
            if bar == 0 {
                return Some(0);
            }
            let r = rect_of(c, bar);
            for (i, v) in [r.bottom, r.left, r.right, r.top].into_iter().enumerate() {
                h.set_f(i, v);
            }
            1
        }
        q if q == Query::ListSelection as u32 => {
            let list = c.combo_part(combo, PART_LIST);
            let mut selection: c_ulong = 0;
            let result = c.list_selection(list, ULongOut::of(&mut selection));
            h.set_f_bits(0, selection as u32);
            result as isize
        }
        q if q == Query::MouseLocation as u32 => {
            h.set_f(0, c.control_mouse_x(combo));
            h.set_f(1, c.control_mouse_y(combo));
            0
        }
        q if q == Query::Key as u32 => {
            (match arg {
                0 => c.key_down_pressed(),
                1 => c.key_up_pressed(),
                2 => c.key_home_pressed(),
                3 => c.key_end_pressed(),
                _ => c.key_enter_value(),
            }) as isize
        }
        _ => return None,
    })
}

fn act<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs, combo: usize) -> Option<isize> {
    let a = h.a as u32;
    if a == Act::ShowLine as u32 || a == Act::ClearText as u32 || a == Act::DeleteText as u32 {
        let label = c.combo_part(combo, PART_LABEL);
        if a == Act::ShowLine as u32 {
            if label != 0 {
                let list = c.combo_part(combo, PART_LIST);
                c.label_set_text_line(label, list, h.b as u32);
            } else {
                let edit = c.combo_part(combo, PART_EDIT);
                let list = c.combo_part(combo, PART_LIST);
                c.edit_set_text_line(edit, list, h.b as u32);
            }
        } else if a == Act::ClearText as u32 {
            if label != 0 {
                c.label_clear_text(label);
            } else {
                let edit = c.combo_part(combo, PART_EDIT);
                c.edit_clear_text(edit);
            }
        } else if label != 0 {
            c.label_delete_first_line(label);
        } else {
            let edit = c.combo_part(combo, PART_EDIT);
            c.edit_delete_text(edit);
        }
    } else if a == Act::PartTabbed as u32 {
        let part = c.combo_part(combo, h.b as u32);
        if h.c != 0 {
            c.control_tabbed_to(part);
        } else {
            c.control_tabbed_from(part);
        }
    } else if a == Act::CopyListColours as u32 {
        let label = c.combo_part(combo, PART_LABEL);
        let text = if label != 0 {
            c.label_text(label)
        } else {
            let edit = c.combo_part(combo, PART_EDIT);
            c.edit_text(edit)
        };
        let list = c.combo_part(combo, PART_LIST);
        let list_text = c.list_text(list);
        c.text_copy_fg(list_text, text);
        c.text_copy_bg(list_text, text);
        c.text_copy_transparent(list_text, text);
    } else if a == Act::ProcessArrow as u32 {
        if h.b != 0 {
            c.combo_process_down(combo);
        } else {
            c.combo_process_up(combo);
        }
    } else {
        return None;
    }
    Some(0)
}

lle_host!(
    /// The combo box's host: [`combo_call`], then `UIComboBox::Rust::Op`.
    ee_ui_combo_host => combo_call, ee_uicpp_combo_host
);

/// The constructor's arguments past the combo box (`UIComboBox::UIComboBox`).
pub(crate) struct ComboArgs {
    pub text_box_type: c_ulong,
    /// The lines (`std::vector<UWideString>*`).
    pub data: usize,
    pub font: usize,
    pub rect: UiRect,
    pub max_list_height: c_ulong,
    pub form: usize,
    pub max_chars: c_ulong,
    pub border: c_ulong,
}

/// `UIComboBox::UIComboBox`'s body after the control's: the drop button over the scroll bar's down bitmaps, the
/// label or edit box, the list of the lines, the Rust object (made by `new`, handed to the C++ by `set_rust`; the
/// list's selected colour, no selection for no lines), the list's first line selected and shown, black opaque
/// backgrounds, the combo box spanning its text box and drop button.
pub(crate) fn construct<C: Cpp + ?Sized>(
    c: &mut C,
    combo: usize,
    args: &ComboArgs,
    new: &mut dyn FnMut(usize, usize, usize, usize, [f32; 3], c_ulong) -> usize,
) {
    let own = rect_of(c, combo);
    let drop_rect = ee_ui_combo_drop_rect(own, args.rect);
    let normal = c.form_scaled_manager_bitmap(
        args.form,
        SB_DOWN_NORMAL,
        drop_rect.left,
        drop_rect.top,
        drop_rect.right,
        drop_rect.bottom,
    );
    let pressed = c.form_scaled_manager_bitmap(
        args.form,
        SB_DOWN_PRESSED,
        drop_rect.left,
        drop_rect.top,
        drop_rect.right,
        drop_rect.bottom,
    );
    let drop =
        c.combo_new_drop(combo, normal, pressed, drop_rect.left, drop_rect.top, drop_rect.right, drop_rect.bottom);
    c.bitmap_dereference(normal);
    c.bitmap_dereference(pressed);
    let own = rect_of(c, combo);
    let drop_width = c.control_width(drop);
    let t = ee_ui_combo_text_rect(own, args.rect, drop_width as c_ulong);
    let (mut label, mut edit) = (0usize, 0usize);
    let text_box = if args.text_box_type == 1 {
        label = c.combo_new_label(combo, args.font, t.left, t.top, t.right, t.bottom, args.border as u32);
        label
    } else {
        edit = c.combo_new_edit(
            combo,
            args.font,
            args.max_chars as u32,
            t.left,
            t.top,
            t.right,
            t.bottom,
            args.border as u32,
        );
        edit
    };
    let own = rect_of(c, combo);
    let text_bottom = rect_of(c, text_box).bottom;
    let l = ee_ui_combo_list_rect(own, text_bottom, args.max_list_height);
    let list = c.combo_new_list(combo, args.data, args.font, l.left, l.top, l.right, l.bottom, args.border as u32);
    let mut rgb = [0.0f32; 4];
    c.list_selected_color(list, F32Out(rgb.as_mut_ptr()));
    let selection = if c.list_line_count(list) > 0 { 0 } else { c_ulong::MAX };
    let rust = new(label, drop, list, edit, [rgb[0], rgb[1], rgb[2]], selection);
    c.combo_set_rust(combo, rust);
    c.list_change_selection(list, 0, true, false);
    c.control_set_bg_opacity(list, 1.0);
    c.control_set_bg_color(list, 0.0, 0.0, 0.0);
    c.list_set_scroll_bg_opacity(list, 1.0);
    c.list_set_scroll_bg_color(list, 0.0, 0.0, 0.0);
    if c.list_line_count(list) > 0 {
        if label != 0 {
            c.label_set_text_line(label, list, 0);
        } else {
            c.edit_set_text_line(edit, list, 0);
        }
    }
    c.combo_base_set_rect(combo, text_box, drop);
}

/// `UIComboBox::UIComboBox`'s body ([`construct`]).
///
/// # Safety
/// `combo` is the C++ combo box being constructed (its control part made), `data` its lines, `rect` the given
/// `const GERectangle*`, `font` and `form` live.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_construct(
    combo: *mut core::ffi::c_void,
    text_box_type: c_ulong,
    data: *const core::ffi::c_void,
    font: *mut core::ffi::c_void,
    rect: *const UiRect,
    max_list_height: c_ulong,
    form: *mut core::ffi::c_void,
    max_chars: c_ulong,
    border: c_ulong,
) {
    // SAFETY: the contract (GERectangle is UiRect's layout)
    let rect = unsafe { *rect };
    let args = ComboArgs {
        text_box_type,
        data: data as usize,
        font: font as usize,
        rect,
        max_list_height,
        form: form as usize,
        max_chars,
        border,
    };
    let mut new = |label: usize, drop: usize, list: usize, edit: usize, rgb: [f32; 3], selection: c_ulong| {
        // SAFETY: the parts just made, the colour's three floats
        unsafe {
            crate::comboobj::ee_ui_combo_new(
                combo,
                Some(ee_ui_combo_host),
                label as *mut _,
                drop as *mut _,
                list as *mut _,
                edit as *mut _,
                text_box_type,
                max_list_height,
                selection,
                rgb.as_ptr(),
            ) as usize
        }
    };
    construct(&mut crate::lle_cpp::Ffi, combo as usize, &args, &mut new);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lle_cpp::Fake;

    fn run(fake: &mut Fake, op: Op, abcd: [isize; 4], f: &mut [f32; 4]) -> Option<isize> {
        combo_call(fake, &HostArgs::new(0x100, op, abcd, f))
    }

    fn q(query: Query, arg: isize) -> isize {
        (query as isize) << 24 | arg
    }

    #[test]
    fn queries() {
        let mut fake = Fake::default();
        fake.answer("combo_part", &[0x20, 0x20]).answer("list_scroll_bar", &[0, 0x30]);
        let mut f = [0.0; 4];
        assert_eq!(run(&mut fake, Op::ComboQuery, [q(Query::ListScrollBarRect, 0), 0, 0, 0], &mut f), Some(0));
        assert_eq!(run(&mut fake, Op::ComboQuery, [q(Query::ListScrollBarRect, 0), 0, 0, 0], &mut f), Some(1));
        fake.out("list_selection", 5).answer("list_selection", &[0]);
        assert_eq!(run(&mut fake, Op::ComboQuery, [q(Query::ListSelection, 0), 0, 0, 0], &mut f), Some(0));
        assert_eq!(f[0].to_bits(), 5);
        fake.answer("control_mouse_x", &[3]).answer("control_mouse_y", &[4]);
        run(&mut fake, Op::ComboQuery, [q(Query::MouseLocation, 0), 0, 0, 0], &mut f);
        assert_eq!(f[..2], [3.0, 4.0]);
        for k in 0..5 {
            run(&mut fake, Op::ComboQuery, [q(Query::Key, k), 0, 0, 0], &mut f);
        }
        assert_eq!(run(&mut fake, Op::ComboQuery, [q(Query::LineCount, 0), 0, 0, 0], &mut f), None);
        assert_eq!(
            fake.calls(),
            "combo_part(256, 2)\nlist_scroll_bar(32)\ncombo_part(256, 2)\nlist_scroll_bar(32)\ncontrol_rect(48, out)\n\
             combo_part(256, 2)\nlist_selection(0, out)\ncontrol_mouse_x(256)\ncontrol_mouse_y(256)\nkey_down_pressed()\n\
             key_up_pressed()\nkey_home_pressed()\nkey_end_pressed()\nkey_enter_value()"
        );
    }

    #[test]
    fn actions_on_the_label_or_the_edit_box() {
        let mut fake = Fake::default();
        let mut f = [0.0; 4];
        // label 0: the edit box
        run(&mut fake, Op::ComboAct, [Act::ShowLine as isize, 2, 0, 0], &mut f);
        run(&mut fake, Op::ComboAct, [Act::ClearText as isize, 0, 0, 0], &mut f);
        run(&mut fake, Op::ComboAct, [Act::DeleteText as isize, 0, 0, 0], &mut f);
        assert_eq!(
            fake.calls(),
            "combo_part(256, 4)\ncombo_part(256, 3)\ncombo_part(256, 2)\nedit_set_text_line(0, 0, 2)\ncombo_part(256, 4)\n\
             combo_part(256, 3)\nedit_clear_text(0)\ncombo_part(256, 4)\ncombo_part(256, 3)\nedit_delete_text(0)"
        );
        let mut fake = Fake::default();
        fake.answer("combo_part", &[9, 7, 9, 9, 9, 7]).answer("label_text", &[0x40]).answer("list_text", &[0x41]);
        run(&mut fake, Op::ComboAct, [Act::ShowLine as isize, 2, 0, 0], &mut f);
        run(&mut fake, Op::ComboAct, [Act::ClearText as isize, 0, 0, 0], &mut f);
        run(&mut fake, Op::ComboAct, [Act::DeleteText as isize, 0, 0, 0], &mut f);
        run(&mut fake, Op::ComboAct, [Act::CopyListColours as isize, 0, 0, 0], &mut f);
        assert_eq!(
            fake.calls(),
            "combo_part(256, 4)\ncombo_part(256, 2)\nlabel_set_text_line(9, 7, 2)\ncombo_part(256, 4)\nlabel_clear_text(9)\n\
             combo_part(256, 4)\nlabel_delete_first_line(9)\ncombo_part(256, 4)\nlabel_text(9)\ncombo_part(256, 2)\n\
             list_text(7)\ntext_copy_fg(65, 64)\ntext_copy_bg(65, 64)\ntext_copy_transparent(65, 64)"
        );
    }

    #[test]
    fn tabs_and_arrows() {
        let mut fake = Fake::default();
        fake.answer("combo_part", &[5, 5]);
        let mut f = [0.0; 4];
        run(&mut fake, Op::ComboAct, [Act::PartTabbed as isize, 1, 1, 0], &mut f);
        run(&mut fake, Op::ComboAct, [Act::PartTabbed as isize, 1, 0, 0], &mut f);
        run(&mut fake, Op::ComboAct, [Act::ProcessArrow as isize, 1, 0, 0], &mut f);
        run(&mut fake, Op::ComboAct, [Act::ProcessArrow as isize, 0, 0, 0], &mut f);
        assert_eq!(run(&mut fake, Op::ComboAct, [Act::OpenList as isize, 0, 0, 0], &mut f), None);
        assert_eq!(
            fake.calls(),
            "combo_part(256, 1)\ncontrol_tabbed_to(5)\ncombo_part(256, 1)\ncontrol_tabbed_from(5)\n\
             combo_process_down(256)\ncombo_process_up(256)"
        );
    }

    #[test]
    fn constructor_makes_the_parts_in_order() {
        let mut fake = Fake::default();
        fake.answer("form_scaled_manager_bitmap", &[0x10, 0x11]).answer("combo_new_drop", &[0x20]);
        fake.answer("combo_new_label", &[0x30]).answer("combo_new_list", &[0x40]).answer("list_line_count", &[2, 2]);
        let args = ComboArgs {
            text_box_type: 1,
            data: 0x50,
            font: 0x60,
            rect: UiRect { bottom: 20.0, left: 0.0, right: 100.0, top: 0.0 },
            max_list_height: 80,
            form: 0x70,
            max_chars: 10,
            border: 1,
        };
        let mut made = Vec::new();
        construct(&mut fake, 0x100, &args, &mut |l, d, li, e, _, s| {
            made.push((l, d, li, e, s));
            0x99
        });
        assert_eq!(made, [(0x30, 0x20, 0x40, 0, 0)]);
        let calls = fake.calls();
        assert!(calls.contains("combo_new_drop(256, 16, 17,"));
        assert!(calls.contains("bitmap_dereference(16)\nbitmap_dereference(17)\n"));
        assert!(calls.contains("combo_set_rust(256, 153)\nlist_change_selection(64, 0, true, false)\n"));
        assert!(calls.ends_with("label_set_text_line(48, 64, 0)\ncombo_base_set_rect(256, 48, 32)"));
    }
}
