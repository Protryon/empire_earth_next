//! `UIBackground.cpp`'s and `UIEditBox.cpp`'s multi-statement host cases in Rust over one-statement shims (UI lane
//! round 12): a background's texture size and load (the UI textures' stages, `lle_manager.rs`'), an edit box's form
//! mouse location, its scroll bar, its lines, its text colour and the keyboard's A, L and Return keys. The other
//! cases stay the C++ hosts' (`UIBackgroundHost::Op`, `UIEditBoxRust::Op`: one statement each).

// the logic is the FFI hosts' (absent with no_cpp: a crate's tests that call the toolkit)
#![cfg_attr(feature = "no_cpp", allow(dead_code))]

use crate::lle_cpp::{lle_host, Cpp, EeUiShimPtrOut, EeUiShimText, HostArgs};
use crate::lle_manager::ui_texture_stages;
use crate::obj::EeUiOp;

/// A background's moved cases (`h.obj` the background's record).
pub(crate) fn background_call<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs) -> Option<isize> {
    match h.op {
        EeUiOp::BgTextureSize => {
            let texture = h.a as usize;
            h.set_f(0, c.texture_height(texture) as f32);
            Some(c.texture_width(texture) as isize)
        }
        EeUiOp::BgTextureLoad => {
            // SAFETY: the op's NUL-terminated name, live for the call
            let name = unsafe { core::ffi::CStr::from_ptr(h.a as *const core::ffi::c_char) }.to_bytes();
            let texture = c.bg_texture_load(h.obj, EeUiShimText::of(name), h.b as usize);
            ui_texture_stages(c, texture);
            Some(texture as isize)
        }
        _ => None,
    }
}

lle_host!(
    /// The backgrounds' host: [`background_call`], then `UIBackgroundHost::Op`.
    ee_ui_background_host => background_call, [crate::lle_hosts::background]
);

/// The keyboard keys of `EditKey` 14..16 (`kKeyA`, `kKeyL`, `kKeyReturn` just pressed).
const EDIT_KEY_A: isize = 14;
const EDIT_KEY_RETURN: isize = 16;

/// An edit box's moved cases (`h.obj` the `UIEditBox*`).
pub(crate) fn edit_call<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs) -> Option<isize> {
    let e = h.obj;
    Some(match h.op {
        EeUiOp::EditKey if (EDIT_KEY_A..=EDIT_KEY_RETURN).contains(&h.a) => {
            let mut key = 0usize;
            use ee_base::lle_enums::ISKeyCode as K;
            let code = [K::kKeyA, K::kKeyL, K::kKeyReturn][(h.a - EDIT_KEY_A) as usize];
            c.keyboard_entity(code as u32, EeUiShimPtrOut::of(&mut key));
            c.entity_just_pressed(key) as isize
        }
        EeUiOp::CreateScrollBar => c.edit_scroll_bar_new(e, h.f(0), h.f(1), h.f(2)) as isize,
        EeUiOp::FormMouseLocation => {
            h.set_f(0, c.edit_form_mouse_x(e));
            h.set_f(1, c.edit_form_mouse_y(e));
            0
        }
        EeUiOp::LineCreate => {
            if h.a == 1 {
                c.line_new_black() as isize
            } else {
                c.line_new() as isize
            }
        }
        EeUiOp::TextFgColor => {
            let text = h.a as usize;
            h.set_f(0, c.text_fg_red(text));
            h.set_f(1, c.text_fg_green(text));
            h.set_f(2, c.text_fg_blue(text));
            0
        }
        _ => return None,
    })
}

lle_host!(
    /// The edit boxes' host: [`edit_call`], then `UIEditBoxRust::Op`.
    ee_ui_edit_host => edit_call, [crate::lle_hosts::base, crate::lle_hosts::media, crate::lle_hosts::part, crate::lle_hosts::edit]
);

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lle_cpp::Fake;

    #[test]
    fn a_background_texture_is_loaded_with_the_ui_stages() {
        let mut fake = Fake::default();
        fake.answer("bg_texture_load", &[70]).answer("texture_height", &[32]).answer("texture_width", &[64]);
        let name = c"textures/x";
        let mut f = [0.0f32; 4];
        let h = HostArgs::new(5, EeUiOp::BgTextureLoad, [name.as_ptr() as isize, 9, 0, 0], &mut f);
        assert_eq!(background_call(&mut fake, &h), Some(70));
        let h = HostArgs::new(5, EeUiOp::BgTextureSize, [70, 0, 0, 0], &mut f);
        assert_eq!(background_call(&mut fake, &h), Some(64));
        assert_eq!(f[0], 32.0);
        let calls = fake.calls();
        assert!(calls.starts_with("bg_texture_load(5, \"textures/x\", 9)\ntexture_stage_alpha(70, "), "{calls}");
        let h = HostArgs::new(5, EeUiOp::BgAdd, [0; 4], &mut f);
        assert_eq!(background_call(&mut fake, &h), None);
    }

    #[test]
    fn an_edit_box_answers_its_moved_cases() {
        let mut fake = Fake::default();
        fake.out("keyboard_entity", 9).answer("entity_just_pressed", &[1]);
        fake.answer("line_new_black", &[40]).answer("line_new", &[41]);
        fake.answer("edit_form_mouse_x", &[3]).answer("text_fg_green", &[1]);
        let mut f = [0.0f32; 4];
        assert_eq!(edit_call(&mut fake, &HostArgs::new(5, EeUiOp::EditKey, [15, 0, 0, 0], &mut f)), Some(1));
        assert_eq!(edit_call(&mut fake, &HostArgs::new(5, EeUiOp::EditKey, [13, 0, 0, 0], &mut f)), None);
        assert_eq!(edit_call(&mut fake, &HostArgs::new(5, EeUiOp::LineCreate, [1, 0, 0, 0], &mut f)), Some(40));
        assert_eq!(edit_call(&mut fake, &HostArgs::new(5, EeUiOp::LineCreate, [0, 0, 0, 0], &mut f)), Some(41));
        edit_call(&mut fake, &HostArgs::new(5, EeUiOp::FormMouseLocation, [0; 4], &mut f));
        assert_eq!((f[0], f[1]), (3.0, 0.0));
        edit_call(&mut fake, &HostArgs::new(5, EeUiOp::TextFgColor, [8, 0, 0, 0], &mut f));
        assert_eq!(&f[..3], &[0.0, 1.0, 0.0]);
        assert!(fake.calls().starts_with(
            "keyboard_entity(38, out)\nentity_just_pressed(9)\nline_new_black()\nline_new()\nedit_form_mouse_x(5)"
        ));
    }
}
