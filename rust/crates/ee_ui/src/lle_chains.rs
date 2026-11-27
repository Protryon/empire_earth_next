// The host ops that were a C++ chain of non-virtual forwarders (LLE/UI round 2, pass 2): `GetForm()->GetManager()->
// GetScreen()`, `theForm->GetTabStop()`, a control's or a background's member. Every link was a C++ forwarder of one
// inward call on a Rust object (a form's `ee_ui_form_call`, a manager's, a group's, a control's record), so the op
// makes those calls itself through [`crate::direct`], which finds each Rust object by the C++ pointer the op holds.
// The virtuals (a game class may override them) stay C++ shims. Included by lle_hosts.rs as `chains`.

use core::ffi::{c_ulong, c_void};

use crate::bgobj::EeUiBackgroundFields;
use crate::controlobj::{EeUiControlCall as C, EeUiControlFields};
use crate::direct::{self, Cpp, Kind};
use crate::formobj::EeUiFormCall as F;
use crate::groupobj::EeUiEntityGroupCall as G;
use crate::managerobj::EeUiManagerCall as M;
use crate::rect::{um_floor, UiRect};

/// A host op of the chains: `o` the host's object, `a`..`d` its numbers, `f` its floats.
macro_rules! ops {
    ($( $(#[$m:meta])* $name:ident => |$o:ident, $a:ident, $b:ident, $f:ident| $e:expr; )*) => {$(
        $(#[$m])*
        ///
        /// # Safety
        /// The host contract: `o` (and the objects `a` and `b` name) live, `f` the op's floats.
        #[allow(unused_variables, unused_unsafe, clippy::unnecessary_cast)]
        pub(crate) unsafe fn $name($o: *mut c_void, $a: isize, $b: isize, _c: isize, _d: isize, $f: *mut f32) -> isize {
            // SAFETY: the contract
            unsafe { $e }
        }
    )*};
}

fn p(x: isize) -> Cpp {
    x as Cpp
}
/// An `unsigned long` answer as the C++ case widened it.
fn ul(x: isize) -> isize {
    x as c_ulong as isize
}
/// A `bool` answer.
fn bl(x: isize) -> isize {
    (x != 0) as isize
}
/// The floats `f[0..4]` (a rectangle in GERectangle's order, a point, a colour), copied for the call as the C++
/// forwarder's `UIF4` was.
unsafe fn f4(f: *const f32) -> [f32; 4] {
    // SAFETY: the host's buffer holds four floats
    unsafe { [*f, *f.add(1), *f.add(2), *f.add(3)] }
}
/// `f[0..n] = v[0..n]`.
unsafe fn put(f: *mut f32, v: *const f32, n: usize) {
    // SAFETY: the host's buffer holds four floats; `v` the n floats of a live record
    unsafe { core::ptr::copy(v, f, n) }
}
unsafe fn put_rect(f: *mut f32, r: &UiRect) {
    // SAFETY: as put (UiRect is GERectangle's four floats)
    unsafe { put(f, &r.bottom, 4) }
}

/// The record of the control `c`, read by `g` (0 for an unknown control).
fn ctl<R: Default>(c: Cpp, g: impl FnOnce(&mut EeUiControlFields) -> R) -> R {
    direct::with_fields(c, g).unwrap_or_default()
}
/// The control `c`'s form (`GetForm()`).
fn form_of(c: Cpp) -> Cpp {
    ctl(c, |r| r.mForm as usize) as Cpp
}
/// The form's manager (`GetManager()`).
fn mgr(form: Cpp) -> Cpp {
    form_call(form, F::Manager, 0, 0) as Cpp
}
fn form_call(form: Cpp, call: F, a: isize, b: isize) -> isize {
    direct::form_call(form, call, a, b)
}
/// A form call with the op's floats (a copy: the forwarder's rectangle).
unsafe fn form_f(form: Cpp, call: F, a: isize, b: isize, f: *const f32) -> isize {
    // SAFETY: the contract
    let mut v = unsafe { f4(f) };
    direct::form_call_f(form, call, a, b, &mut v)
}
fn mgr_call(m: Cpp, call: M, a: isize, b: isize, c: isize) -> isize {
    direct::manager_call(m, call, a, b, c)
}
unsafe fn mgr_f(m: Cpp, call: M, a: isize, f: *const f32) -> isize {
    // SAFETY: the contract
    let mut v = unsafe { f4(f) };
    direct::manager_call_f(m, call, a, 0, 0, &mut v)
}
fn ctl_call(c: Cpp, call: C, a: isize) -> isize {
    direct::control_call(c, call, a, 0)
}
unsafe fn ctl_f(c: Cpp, call: C, f: *const f32) -> isize {
    // SAFETY: the contract
    let mut v = unsafe { f4(f) };
    direct::control_call_f(c, call, 0, 0, &mut v)
}
/// `f[0..4] = form->GetRectangle()`.
unsafe fn form_rect(form: Cpp, f: *mut f32) {
    let r = form_call(form, F::Rectangle, 0, 0) as *const f32;
    if !r.is_null() {
        // SAFETY: the form's rectangle (GERectangle's layout)
        unsafe { put(f, r, 4) }
    }
}
/// `*(GE2DPoint*)f = form->GetMouseLocation()`.
unsafe fn form_mouse(form: Cpp, f: *mut f32) {
    let r = form_call(form, F::MouseLocation, 0, 0) as *const f32;
    if !r.is_null() {
        // SAFETY: the form's mouse location (GE2DPoint's two floats)
        unsafe { put(f, r, 2) }
    }
}
/// The form's mouse location's x (`i` 0) or y (1).
pub(crate) fn form_mouse_at(form: Cpp, i: usize) -> f32 {
    let mut v = [0.0f32; 4];
    // SAFETY: a local buffer
    unsafe { form_mouse(form, v.as_mut_ptr()) };
    v[i]
}
/// `UMFloor(GetRectangle().GetWidth() + 1.0f)` of the control `c` (`GetWidth()`).
fn ctl_width(c: Cpp) -> c_ulong {
    ctl(c, |r| um_floor(r.mRectangle.width() + 1.0) as c_ulong)
}
/// The record of the background `bg` (a `UIBackground*`), read by `g`.
fn bg<R: Default>(bg: Cpp, g: impl FnOnce(&EeUiBackgroundFields) -> R) -> R {
    let r = direct::find(Kind::Background, bg) as *const EeUiBackgroundFields;
    // SAFETY: a live background record
    unsafe { r.as_ref() }.map_or_else(R::default, g)
}
/// `this->DrawTiledBackground(rect f, a, b, c, d, f[4], f[5])` (o the background record; the one body, Rust's).
///
/// # Safety
/// The host contract: `o` a live background record, `f` six floats.
pub(crate) unsafe fn bg_tiled(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    // SAFETY: the contract
    unsafe {
        let mut w = [
            *f,
            *f.add(1),
            *f.add(2),
            *f.add(3),
            f32::from_bits(c as u32),
            f32::from_bits(d as u32),
            *f.add(4),
            *f.add(5),
        ];
        crate::bgobj::ee_ui_bg_call(
            o as *mut EeUiBackgroundFields,
            crate::bgobj::EeUiBgCall::DrawTiled,
            ul(a),
            ul(b),
            w.as_mut_ptr(),
        ) as core::ffi::c_long as isize
    }
}
/// The form of the background record `o` (the host's object).
unsafe fn bg_form(o: *mut c_void) -> Cpp {
    // SAFETY: the host contract: `o` a live background record
    unsafe { (*(o as *const EeUiBackgroundFields)).mForm }
}

ops! {
    // ---- the backgrounds' (o the record)
    /// `theForm->AddBackground(static_cast<UIBackground*>(r))`.
    bg_add => |o, a, b, f| form_call(bg_form(o), F::AddBackground, direct::cpp_of(Kind::Background, o) as isize, 0) * 0;
    /// `theForm->GetUseExclusiveCoords()`.
    bg_exclusive => |o, a, b, f| bl(form_call(bg_form(o), F::UseExclusiveCoords, 0, 0));
    /// `theForm == m->GetCurrentForm()`.
    bg_form_current => |o, a, b, f| (mgr_call(mgr(bg_form(o)), M::CurrentForm, 0, 0, 0) == bg_form(o) as isize) as isize;
    /// `a ? theForm->LockDrawBackground() : theForm->UnlockDrawBackground()`.
    bg_lock => |o, a, b, f| form_call(bg_form(o), F::LockDraw, (a != 0) as isize, 0) * 0;
    /// `theForm->RedrawIntersectControls(rect f)`.
    bg_redraw => |o, a, b, f| form_f(bg_form(o), F::RedrawIntersectControls, 0, 0, f) * 0;
    /// `m->AddDirtyRectangle(rect f, theForm)`.
    bg_dirty => |o, a, b, f| mgr_f(mgr(bg_form(o)), M::AddDirtyRectangle, bg_form(o) as isize, f) * 0;
    /// `f = theForm->GetRectangle()`.
    bg_form_rect => |o, a, b, f| { form_rect(bg_form(o), f); 0 };
    /// `f[0] = theForm->GetFadeOpacity()`.
    bg_form_fade => |o, a, b, f| { let mut v = [0.0f32; 4]; direct::form_call_f(bg_form(o), F::FadeOpacity, 0, 0, &mut v); *f = v[0]; 0 };
    // ---- the background table's (o the manager, a the entry's record)
    /// `m->GetFormByID(a)`.
    bgt_form => |o, a, b, f| mgr_call(o, M::FormById, ul(a), 0, 0);
    /// `m->GetUITextureBitDepth()`.
    bgt_texture_depth => |o, a, b, f| ul(mgr_call(o, M::TextureBitDepth, 0, 0, 0));
    /// `t->GetHasAlpha()`.
    bgt_entry_alpha => |o, a, b, f| (*(a as *const crate::bgtableobj::EeUiBgTableFields)).mHasAlpha as isize;
    /// `t->GetFormID()`.
    bgt_entry_form_id => |o, a, b, f| (*(a as *const crate::bgtableobj::EeUiBgTableFields)).mFormID as isize;
    // ---- a slider part of a button (a the slider)
    /// `a->GetCurrentRangeValue()`.
    button_slider_value => |o, a, b, f| direct::slider_call(p(a), crate::sliderobj::EeUiSliderCall::CurrentRangeValue, 0, 0).unwrap_or(0) as core::ffi::c_long as isize;
    /// `a->SetCurrentRangeValue(b)`.
    button_slider_set_value => |o, a, b, f| { direct::slider_call(p(a), crate::sliderobj::EeUiSliderCall::SetCurrentRangeValue, b as core::ffi::c_long as isize, 0); 0 };
    // ---- UIControl's record's (o the control)
    /// `theForm->GetControl(a)`.
    ctl_form_control => |o, a, b, f| form_call(form_of(o), F::Control, ul(a), 0);
    /// `theForm->GetEntityGroupById(a)->GetIsEnabled()`.
    ctl_group_enabled => |o, a, b, f| bl(direct::group_call(form_call(form_of(o), F::EntityGroupById, ul(a), 0) as Cpp, G::IsEnabled, 0));
    /// `theForm->GetManager()->GetCurrentForm()`.
    ctl_current_form => |o, a, b, f| mgr_call(mgr(form_of(o)), M::CurrentForm, 0, 0, 0);
    /// `theForm->GetKeyboardCaptured()`.
    ctl_keyboard_captured => |o, a, b, f| form_call(form_of(o), F::KeyboardCaptured, 0, 0);
    /// `theForm->GetMouseCaptured()`.
    ctl_mouse_captured => |o, a, b, f| form_call(form_of(o), F::MouseCaptured, 0, 0);
    /// `theForm->SetKeyboardCaptured(a)`.
    ctl_set_keyboard_captured => |o, a, b, f| form_call(form_of(o), F::SetKeyboardCaptured, a, 0) * 0;
    /// `theForm->SetMouseCaptured(a)`.
    ctl_set_mouse_captured => |o, a, b, f| form_call(form_of(o), F::SetMouseCaptured, a, 0) * 0;
    /// `theForm->GetManager()->AddDirtyRectangle(rect f, theForm)`.
    ctl_add_dirty => |o, a, b, f| mgr_f(mgr(form_of(o)), M::AddDirtyRectangle, form_of(o) as isize, f) * 0;
    /// `theForm->Tab()`.
    ctl_tab => |o, a, b, f| form_call(form_of(o), F::Tab, 0, 0) * 0;
    /// `theForm->GetManager()->AddEventToTrigger(a, theForm, ctl)`.
    ctl_add_event => |o, a, b, f| mgr_call(mgr(form_of(o)), M::AddEventToTrigger, a, form_of(o) as isize, o as isize) * 0;
    /// `theForm->GetTabStop()`.
    ctl_tab_stop => |o, a, b, f| ul(form_call(form_of(o), F::TabStop, 0, 0));
    /// `theForm->GetTabbingOn()`.
    ctl_tabbing_on => |o, a, b, f| bl(form_call(form_of(o), F::TabbingOn, 0, 0));
    /// `theForm->GetManager()->GetNumberOfSurfaces()`.
    ctl_form_surfaces => |o, a, b, f| ul(mgr_call(mgr(form_of(o)), M::NumberOfSurfaces, 0, 0, 0));
    // ---- the virtuals no class overrides (LLE/UI round 3): the one body, Rust's
    /// `ctl->SetDisplayMode(a)`.
    ctl_set_display_mode => |o, a, b, f| ctl_call(o, C::SetDisplayMode, ul(a)) * 0;
    /// `a->SetDisplayMode(b)` (the form's control).
    form_ctl_set_display_mode => |o, a, b, f| ctl_call(p(a), C::SetDisplayMode, ul(b)) * 0;
    /// `ctl->ReleaseCapturedMouse()`.
    ctl_release_mouse => |o, a, b, f| ctl_call(o, C::ReleaseMouse, 0) * 0;
    /// `a->ReleaseCapturedMouse()` (the form's control).
    form_ctl_release_mouse => |o, a, b, f| ctl_call(p(a), C::ReleaseMouse, 0) * 0;
    /// `ctl->DrawBorder(rect f, a)`: the bevelled border over the record's copied opacity and type.
    ctl_draw_border => |o, a, b, f| {
        let m = mgr(form_of(o));
        let (opacity, kind) = ctl(o, |r| (r.mCopiedBorderOpacity, r.mCopiedBorderType));
        let rect = *(f as *const UiRect);
        let rasterizer = mgr_call(m, M::Rasterizer, 0, 0, 0) as *mut c_void;
        let screen = mgr_call(m, M::Screen, 0, 0, 0) as c_ulong;
        crate::lle_control::ee_ui_control_draw_border(rasterizer, screen, rect, ul(a) as c_ulong, opacity, kind) as isize
    };
    /// `a->RelayoutBackground()` (the form's background).
    form_bg_relayout => |o, a, b, f| direct::background_call(p(a), crate::bgobj::EeUiBgCall::Relayout, 0, 0) * 0;
    /// `a->FireEvents()` (the form's hot key).
    form_hot_key_fire => |o, a, b, f| bl(hot_key_call(p(a), crate::hotkeyobj::EeUiHotKeyCall::FireEvents, 0, 0));
    /// `this->DrawStrectchedBackground(rect f, rect f + 4)` (o the record).
    bg_stretched => |o, a, b, f| crate::bgobj::ee_ui_bg_call(o as *mut EeUiBackgroundFields, crate::bgobj::EeUiBgCall::BitmapStretched, 0, 0, f) as core::ffi::c_long as isize;
    /// `l->CheckScrollBar()` (o the label).
    label_check_scroll_bar => |o, a, b, f| direct::label_call(o, crate::labelobj::EeUiLabelCall::CheckScrollBar, 0, 0).unwrap_or(0) * 0;
    /// `l->RecomputeDisplayRange()` (o the label).
    label_recompute => |o, a, b, f| direct::label_call(o, crate::labelobj::EeUiLabelCall::RecomputeDisplayRange, 0, 0).unwrap_or(0) * 0;
    /// `l->ProcessHome()` (o the list box).
    list_process_home => |o, a, b, f| direct::list_call(o, crate::listobj::EeUiListCall::ProcessHome, 0, 0).unwrap_or(0) * 0;
    /// `l->ProcessEnd()` (o the list box).
    list_process_end => |o, a, b, f| direct::list_call(o, crate::listobj::EeUiListCall::ProcessEnd, 0, 0).unwrap_or(0) * 0;
    /// `CB->OpenListBox()` (o the combo box).
    combo_a_open_list => |o, a, b, f| direct::combo_call(o, crate::comboobj::EeUiComboCall::OpenListBox, 0, 0).unwrap_or(0) * 0;
    /// `CB->CloseListBox()` (o the combo box).
    combo_a_close_list => |o, a, b, f| direct::combo_call(o, crate::comboobj::EeUiComboCall::CloseListBox, 0, 0).unwrap_or(0) * 0;
    /// `CB->ReleaseCapturedMouse()` (o the combo box).
    combo_a_release_mouse => |o, a, b, f| ctl_call(o, C::ReleaseMouse, 0) * 0;
    // ---- every whole object's (o the control)
    /// `ctl->UIControl::CopyData()`.
    obj_base_copy_data => |o, a, b, f| ctl_call(o, C::CopyData, 0) as core::ffi::c_long as isize;
    /// `ctl->UIControl::DrawPolygons()`.
    obj_base_draw_polygons => |o, a, b, f| ctl_call(o, C::DrawPolygons, 0) as core::ffi::c_long as isize;
    /// `ctl->UIControl::DrawTextA()`: the draw count one less -> kErrorOK.
    obj_base_draw_text => |o, a, b, f| { ctl(o, |r| r.mDrawCount = r.mDrawCount.wrapping_sub(1)); 0 };
    /// `ctl->UIControl::SetLocation(point f)`.
    obj_base_set_location => |o, a, b, f| direct::control_call_f(o, C::SetLocation, 0, 0, &mut [*f, *f.add(1), 0.0, 0.0]) * 0;
    /// `ctl->UIControl::SetRectangle(rect f)`.
    obj_base_set_rectangle => |o, a, b, f| ctl_f(o, C::SetRectangle, f) * 0;
    /// `f = ctl->GetRectangle()`.
    obj_rect => |o, a, b, f| { ctl(o, |r| put_rect(f, &r.mRectangle)); 0 };
    /// `f = ctl->GetCopiedDrawRectangle()`.
    obj_copied_draw_rect => |o, a, b, f| { ctl(o, |r| put_rect(f, &r.mCopiedDrawRectangle)); 0 };
    /// `*(GE2DPoint*)f = ctl->GetMouseLocation()` (the form's).
    obj_mouse_location => |o, a, b, f| { form_mouse(form_of(o), f); 0 };
    /// `ctl->CaptureMouse()`.
    obj_capture_mouse => |o, a, b, f| ctl_call(o, C::CaptureMouse, 0) * 0;
    /// `ctl->CaptureKeyboard()`.
    obj_capture_keyboard => |o, a, b, f| ctl_call(o, C::CaptureKeyboard, 0) * 0;
    /// `ctl->FireTriggerEvent(a)`.
    obj_fire_trigger => |o, a, b, f| ctl_call(o, C::FireTrigger, ul(a)) * 0;
    /// `ctl->GetWidth()`.
    obj_width => |o, a, b, f| ctl_width(o) as isize;
    /// `ctl->GetForm()->GetTabStop()`.
    obj_tab_stop => |o, a, b, f| ul(form_call(form_of(o), F::TabStop, 0, 0));
    /// `ctl->GetForm()->GetTabbingOn()`.
    obj_tabbing_on => |o, a, b, f| bl(form_call(form_of(o), F::TabbingOn, 0, 0));
    /// `ctl->GetForm()->SetTabStop(a)`.
    obj_set_tab_stop => |o, a, b, f| form_call(form_of(o), F::SetTabStop, ul(a), 0) * 0;
    /// `ctl->GetHasMouseCapture()`.
    obj_has_mouse_capture => |o, a, b, f| bl(ctl_call(o, C::HasMouseCapture, 0));
    /// `ctl->GetRightMouseButtonDown()`.
    obj_right_button_down => |o, a, b, f| bl(ctl_call(o, C::RightButtonDown, 0));
    /// `ctl->GetForm()->GetManager()->GetBitmap(the manager bitmap a)`.
    obj_manager_bitmap => |o, a, b, f| mgr_call(mgr(form_of(o)), M::Bitmap, crate::lle_control::ee_ui_obj_manager_bitmap(a) as c_ulong as isize, 0, 0);
    /// `ctl->GetForm()->GetControlCount()`.
    obj_control_count => |o, a, b, f| ul(form_call(form_of(o), F::ControlCount, 0, 0));
    /// `f[0] = ctl->GetBgOpacity()`.
    obj_bg_opacity => |o, a, b, f| { let mut v = [0.0f32; 4]; direct::control_call_f(o, C::BgOpacity, 0, 0, &mut v); *f = v[0]; 0 };
    /// `ctl->GetForm()->DrawClippedText(rect f, a, ctl)`.
    obj_draw_clipped_text => |o, a, b, f| form_f(form_of(o), F::DrawClippedText, a, o as isize, f) as core::ffi::c_long as isize;
    /// `f = a->GetRectangle()` (a part).
    obj_part_rect => |o, a, b, f| { ctl(p(a), |r| put_rect(f, &r.mRectangle)); 0 };
    // ---- the controls table's (a the form, d the manager)
    /// `TF->GetUseExclusiveCoords()`.
    tbl_form_exclusive => |o, a, b, f| bl(form_call(p(a), F::UseExclusiveCoords, 0, 0));
    /// `TF->GetDesignedHeight()`.
    tbl_form_designed_height => |o, a, b, f| ul(form_call(p(a), F::DesignedHeight, 0, 0));
    /// `TF->GetUseGDIForText()`.
    tbl_form_gdi => |o, a, b, f| bl(form_call(p(a), F::UseGdiForText, 0, 0));
    /// `TF->ReserveControlIndex(b)`.
    tbl_reserve_index => |o, a, b, f| form_call(p(a), F::ReserveControlIndex, ul(b), 0) * 0;
    /// `TF->GetManager()->CreateControlEvent(TF, b)`.
    tbl_event_new => |o, a, b, f| mgr_call(mgr(p(a)), M::CreateControlEvent, a, ul(b), 0);
    /// `TF->GetControl(b)`.
    tbl_form_control => |o, a, b, f| form_call(p(a), F::Control, ul(b), 0);
    // ---- the form's (o the form, a the control, group or background)
    /// `a->GetDisplayMode()`.
    form_ctl_display_mode => |o, a, b, f| ctl(p(a), |r| r.mDisplayMode) as isize;
    /// `a->GetIsDirty()`.
    form_ctl_dirty => |o, a, b, f| ctl(p(a), |r| r.mIsDirty) as isize;
    /// `a->GetDrawCount()`.
    form_ctl_draw_count => |o, a, b, f| ctl(p(a), |r| r.mDrawCount) as isize;
    /// `a->GetIndex()`.
    form_ctl_index => |o, a, b, f| ctl(p(a), |r| r.mIndex) as isize;
    /// `a->GetId()`.
    form_ctl_id => |o, a, b, f| ctl(p(a), |r| r.mId) as isize;
    /// `f = a->GetRectangle()`.
    form_part_rect => |o, a, b, f| { ctl(p(a), |r| put_rect(f, &r.mRectangle)); 0 };
    /// `f = a->GetCopiedDrawRectangle()`.
    form_part_copied_draw_rect => |o, a, b, f| { ctl(p(a), |r| put_rect(f, &r.mCopiedDrawRectangle)); 0 };
    /// `a->SetStringTableID(b)`.
    form_ctl_set_string_id => |o, a, b, f| { ctl(p(a), |r| r.mStringTableID = b as c_ulong); 0 };
    /// `a->FireTriggerEvent(b)`.
    form_ctl_fire_trigger => |o, a, b, f| ctl_call(p(a), C::FireTrigger, ul(b)) * 0;
    /// `a->UpdateMouse(theForm->GetMouseLocation())` (ee_ui mouse.rs).
    form_ctl_update_mouse => |o, a, b, f| {
        let at = form_call(o, F::MouseLocation, 0, 0) as *const c_void;
        crate::mouse_ffi::ee_ui_control_update_mouse(p(a), at) as isize
    };
    /// `a->GetIsEnabled()` (a group).
    form_group_enabled => |o, a, b, f| bl(direct::group_call(p(a), G::IsEnabled, 0));
    /// `a->GetEntityCount()`.
    form_group_count => |o, a, b, f| ul(direct::group_call(p(a), G::Count, 0));
    /// `a->GetEntity(b)`.
    form_group_entity => |o, a, b, f| direct::group_call(p(a), G::Entity, ul(b));
    /// `a->AddEntity(b)`.
    form_group_add => |o, a, b, f| direct::group_call(p(a), G::Add, b) * 0;
    /// `a->RemoveEntity(b)`.
    form_group_remove => |o, a, b, f| direct::group_call(p(a), G::Remove, b) * 0;
    /// `a->GetDrawPriority()` (a background).
    form_bg_priority => |o, a, b, f| bg(p(a), |r| r.mDrawPriority) as isize;
    /// `a->GetBackgroundID()`.
    form_bg_id => |o, a, b, f| bg(p(a), |r| r.mBackgroundID) as isize;
    /// `a->GetIsVisible()`.
    form_bg_visible => |o, a, b, f| bg(p(a), |r| r.mIsVisible) as isize;
    /// `f = a->GetRectangle()`.
    form_bg_rect => |o, a, b, f| { bg(p(a), |r| put_rect(f, &r.mRectangle)); 0 };
    /// `theForm->GetManager()->GetNumberOfSurfaces()`.
    form_surfaces => |o, a, b, f| ul(mgr_call(mgr(o), M::NumberOfSurfaces, 0, 0, 0));
    /// `theForm->GetManager()->ProcessAllEventsForForm(theForm)`.
    form_manager_process_events => |o, a, b, f| mgr_call(mgr(o), M::ProcessAllEventsForForm, o as isize, 0, 0) * 0;
    /// `theForm->GetManager()->AddDirtyRectangle(rect f, theForm)`.
    form_manager_add_dirty => |o, a, b, f| mgr_f(mgr(o), M::AddDirtyRectangle, o as isize, f) * 0;
    /// `theForm->GetManager()->StopMusic()`.
    form_music_stop => |o, a, b, f| mgr_call(mgr(o), M::StopMusic, 0, 0, 0) * 0;
    /// `theForm->GetManager()->CreateFormEvent(a)`.
    form_create_form_event => |o, a, b, f| mgr_call(mgr(o), M::CreateFormEvent, ul(a), 0, 0);
    // ---- the entity groups' (a the entity, a control)
    /// `ctl->GetDisplayMode() == kDisplayModeShown`.
    group_shown => |o, a, b, f| (ctl(p(a), |r| r.mDisplayMode) == crate::ids::UIDisplayMode::kDisplayModeShown as c_ulong) as isize;
    /// `ctl->GetForm() == ctl->GetForm()->GetManager()->GetCurrentForm()`.
    group_on_current_form => |o, a, b, f| (mgr_call(mgr(form_of(p(a))), M::CurrentForm, 0, 0, 0) == form_of(p(a)) as isize) as isize;
    /// `ctl->GetForm()->GetManager()->AddDirtyRectangle(ctl->GetRectangle(), ctl->GetForm())`.
    group_add_dirty => |o, a, b, f| {
        let mut v = [0.0f32; 4];
        ctl(p(a), |r| put_rect(v.as_mut_ptr(), &r.mRectangle));
        direct::manager_call_f(mgr(form_of(p(a))), M::AddDirtyRectangle, form_of(p(a)) as isize, 0, 0, &mut v) * 0
    };
    /// `ctl->GetForm()->GetTabbingOn()`.
    group_tabbing => |o, a, b, f| bl(form_call(form_of(p(a)), F::TabbingOn, 0, 0));
    /// `ctl->GetForm()->GetTabStop() == ctl->GetIndex()`.
    group_is_tab_stop => |o, a, b, f| (form_call(form_of(p(a)), F::TabStop, 0, 0) as c_ulong == ctl(p(a), |r| r.mIndex)) as isize;
    /// `ctl->GetForm()->Tab()`.
    group_tab => |o, a, b, f| form_call(form_of(p(a)), F::Tab, 0, 0) * 0;
    // ---- the manager's (o the manager, a the form or control)
    /// `f = a->GetRectangle()` (a form).
    mgr_form_rect => |o, a, b, f| { form_rect(p(a), f); 0 };
    /// `a->GetFormID()`.
    mgr_form_id => |o, a, b, f| ul(form_call(p(a), F::FormId, 0, 0));
    /// `a->GetUseGDIForText()`.
    mgr_form_use_gdi => |o, a, b, f| bl(form_call(p(a), F::UseGdiForText, 0, 0));
    /// `a->GetKeyboardCaptured()`.
    mgr_form_keyboard => |o, a, b, f| form_call(p(a), F::KeyboardCaptured, 0, 0);
    /// `a->GetMouseCaptured()`.
    mgr_form_mouse => |o, a, b, f| form_call(p(a), F::MouseCaptured, 0, 0);
    /// `f = a->GetRectangle()` (a control).
    mgr_part_rect => |o, a, b, f| { ctl(p(a), |r| put_rect(f, &r.mRectangle)); 0 };
    /// `a->ClearAllClipRectangles()` (a form).
    mgr_form_clear_clips => |o, a, b, f| form_call(p(a), F::ClearAllClipRectangles, 0, 0) * 0;
    /// `a->SetAllControlsDirty()`.
    mgr_form_all_dirty => |o, a, b, f| form_call(p(a), F::SetAllControlsDirty, 0, 0) * 0;
    // ---- the labels', lists', edit boxes' and graphs' (o the control)
    /// `l->GetForm()->RedrawIntersectControls(rect f)`.
    label_redraw_intersect => |o, a, b, f| form_f(form_of(o), F::RedrawIntersectControls, 0, 0, f) * 0;
    /// `l->SetBgRectangle(rect f)`.
    set_bg_rectangle => |o, a, b, f| ctl_f(o, C::SetBgRectangle, f) * 0;
    /// `l->UIControl::MouseLButtonDoublePressed()`.
    base_mouse_double_pressed => |o, a, b, f| ctl_call(o, C::MouseDoublePressed, 0) as core::ffi::c_long as isize;
    /// `e->UIControl::ReleaseCapturedKeyboard()`.
    base_release_keyboard => |o, a, b, f| ctl_call(o, C::ReleaseKeyboard, 0) * 0;
    /// `e->GetForm()->GetManager()->UpdateTranslatedKey(a, 1)`.
    edit_translated_key => |o, a, b, f| mgr_call(mgr(form_of(o)), M::UpdateTranslatedKey, a, 1, 0) * 0;
    // ---- the slider's
    /// `theManager->CreateBitmapForScaledControl(theManager->GetDragBarBitmap(), rect f)`.
    slider_thumb_create => |o, a, b, f| {
        let m = mgr(form_of(o));
        mgr_f(m, M::ScaledBitmap, mgr_call(m, M::Bitmap, 10, 0, 0), f)
    };
    // ---- the combo box's (o the combo box)
    /// `CB->GetForm()->GetTabbingOn()`.
    combo_q_tabbing_on => |o, a, b, f| bl(form_call(form_of(o), F::TabbingOn, 0, 0));
    /// `CB->GetForm()->GetTabStop()`.
    combo_q_tab_stop => |o, a, b, f| ul(form_call(form_of(o), F::TabStop, 0, 0));
    /// `CB->GetHasMouseCapture()`.
    combo_q_has_mouse_capture => |o, a, b, f| bl(ctl_call(o, C::HasMouseCapture, 0));
    /// `CB->GetForm()->SetPopup(b ? CB : NULL)`.
    combo_a_set_popup => |o, a, b, f| form_call(form_of(o), F::SetPopup, if b != 0 { o as isize } else { 0 }, 0) * 0;
    /// `CB->SetAlwaysDirty(b != 0)`.
    combo_a_set_always_dirty => |o, a, b, f| { ctl(o, |r| r.mAlwaysDirty = b != 0); 0 };
    /// `CB->FireTriggerEvent(b)`.
    combo_a_fire => |o, a, b, f| ctl_call(o, C::FireTrigger, ul(b)) * 0;
    /// `CB->CaptureMouse()`.
    combo_a_capture_mouse => |o, a, b, f| ctl_call(o, C::CaptureMouse, 0) * 0;
    /// `CB->CaptureKeyboard()`.
    combo_a_capture_keyboard => |o, a, b, f| ctl_call(o, C::CaptureKeyboard, 0) * 0;
    /// `CB->mDrawCount--`.
    combo_draw_count_decrement => |o, a, b, f| { ctl(o, |r| r.mDrawCount = r.mDrawCount.wrapping_sub(1)); 0 };
}

/// The form `form`'s call `call` with the rectangle or point `f` (the manager's AddDirtyRectangle,
/// RedrawIntersectControls, AddClipRectangle, UpdateMouseLocation on a form).
///
/// # Safety
/// `f` holds four floats.
pub(crate) unsafe fn form_floats(form: isize, call: F, f: *const f32) {
    // SAFETY: the contract
    let mut v = unsafe { f4(f) };
    if call == F::UpdateMouseLocation {
        v[2] = 0.0;
        v[3] = 0.0;
    }
    direct::form_call_f(p(form), call, 0, 0, &mut v);
}

// ---- the typed shims' calls (lle_cpp.rs `shims!`'s `rust:` part)

/// `m->GetScreen()`.
pub(crate) fn mgr_screen(m: usize) -> u32 {
    mgr_call(m as Cpp, M::Screen, 0, 0, 0) as u32
}
/// `form->GetManager()->GetScreen()`.
pub(crate) fn form_screen(form: usize) -> u32 {
    mgr_screen(mgr(form as Cpp) as usize)
}
/// `m->GetUITextureBitDepth()`.
pub(crate) fn mgr_ui_depth(m: usize) -> u32 {
    mgr_call(m as Cpp, M::TextureBitDepth, 0, 0, 0) as u32
}
/// `m->GetFormByID(id)`.
pub(crate) fn mgr_form_by_id(m: usize, id: u32) -> usize {
    mgr_call(m as Cpp, M::FormById, id as c_ulong as isize, 0, 0) as usize
}
/// `m->AddForm(form)`.
pub(crate) fn mgr_add_form(m: usize, form: usize) {
    mgr_call(m as Cpp, M::AppendForm, form as isize, 0, 0);
}
/// `form->AddEvent(id, kind)`.
pub(crate) fn form_add_event(form: usize, id: u32, kind: u32) {
    form_call(form as Cpp, F::AddEvent, id as c_ulong as isize, kind as c_ulong as isize);
}
/// `form->GetKeyboardCaptured()`.
pub(crate) fn form_keyboard_captured(form: usize) -> usize {
    form_call(form as Cpp, F::KeyboardCaptured, 0, 0) as usize
}
/// `form->GetEntityGroup(0)->Enable()`.
pub(crate) fn form_enable_group0(form: usize) {
    direct::group_call(form_call(form as Cpp, F::EntityGroup, 0, 0) as Cpp, G::Enable, 0);
}
/// `form->AddDatabaseControl(index, NULL)`.
pub(crate) fn form_add_database_control(form: usize, index: u32) {
    form_call(form as Cpp, F::AddDatabaseControl, index as c_ulong as isize, 0);
}
/// `rect = control->GetRectangle()`.
///
/// # Safety
/// `rect` holds four floats.
pub(crate) unsafe fn control_rect(control: usize, rect: crate::lle_cpp::EeUiShimF32Out) {
    // SAFETY: the contract
    ctl(control as Cpp, |r| unsafe { put_rect(rect.0, &r.mRectangle) });
}
/// `control->GetMouseLocation().GetX()` (the form's).
pub(crate) fn control_mouse_x(control: usize) -> f32 {
    form_mouse_at(form_of(control as Cpp), 0)
}
/// `control->GetMouseLocation().GetY()`.
pub(crate) fn control_mouse_y(control: usize) -> f32 {
    form_mouse_at(form_of(control as Cpp), 1)
}
/// `control->GetWidth()`.
pub(crate) fn control_width(control: usize) -> u32 {
    ctl_width(control as Cpp) as u32
}
/// `form->GetManager()->CreateBitmapForScaledControl(form->GetManager()->GetBitmap(index), GERectangle(left, top,
/// right, bottom))`.
pub(crate) fn form_scaled_manager_bitmap(
    form: usize,
    index: u32,
    left: f32,
    top: f32,
    right: f32,
    bottom: f32,
) -> usize {
    let m = mgr(form as Cpp);
    let bitmap = mgr_call(m, M::Bitmap, index as c_ulong as isize, 0, 0);
    direct::manager_call_f(m, M::ScaledBitmap, bitmap, 0, 0, &mut [bottom, left, right, top]) as usize
}

// ---- the controls tables (a `UIControlsTable*`: a C++ shell over the Rust table, found by direct.rs)

/// The data of the table `t` (a `UIControlsTable*`), read by `g`.
fn table<R: Default>(t: Cpp, g: impl FnOnce(&crate::tableobj::EeUiControlsTableData) -> R) -> R {
    let o = direct::find(Kind::Table, t) as *const crate::tableobj::EeUiControlsTable;
    if o.is_null() {
        return R::default();
    }
    // SAFETY: a live table (registered between its wrap and its free)
    g(unsafe { &*crate::tableobj::ee_ui_controls_table_data(o) })
}
/// One of the table `t`'s methods.
fn table_call(t: Cpp, call: crate::tableobj::EeUiTableCall, a: isize, b: isize) -> isize {
    let o = direct::find(Kind::Table, t) as *const crate::tableobj::EeUiControlsTable;
    if o.is_null() {
        return 0;
    }
    // SAFETY: a live table
    unsafe { crate::tableobj::ee_ui_controls_table_call(o, call, a, b, core::ptr::null_mut()) }
}
/// `UIControlsTable::GetUIControlsTable(index)`.
fn table_at(index: isize) -> isize {
    // SAFETY: the static array's lookup (no host, no floats)
    unsafe {
        crate::tableobj::ee_ui_controls_tables(
            crate::tableobj::EeUiTablesCall::At,
            index,
            0,
            None,
            core::ptr::null_mut(),
        )
    }
}

// ---- the hot keys (a `UIHotKey*`: its record found by direct.rs)

/// The record of the hot key `k`, read or written by `g`.
fn hot_key<R: Default>(k: Cpp, g: impl FnOnce(&mut crate::hotkeyobj::EeUiHotKeyFields) -> R) -> R {
    let r = direct::find(Kind::HotKey, k) as *mut crate::hotkeyobj::EeUiHotKeyFields;
    // SAFETY: a live hot key record; no call out while the reference lives
    unsafe { r.as_mut() }.map_or_else(R::default, g)
}
/// One of the hot key `k`'s methods.
pub(crate) fn hot_key_call(k: Cpp, call: crate::hotkeyobj::EeUiHotKeyCall, a: isize, b: isize) -> isize {
    let r = direct::find(Kind::HotKey, k) as *mut crate::hotkeyobj::EeUiHotKeyFields;
    if r.is_null() {
        return 0;
    }
    // SAFETY: a live hot key record
    unsafe { crate::hotkeyobj::ee_ui_hotkey_call(r, call, a, b) }
}

// ---- UIControl's virtuals that only toolkit classes overrode (LLE/UI round 3): the override of the control's kind
// (direct.rs finds each Rust object by the C++ pointer), else UIControl's body

/// `c->SetBgOpacity(v[0])` (`color` false) or `c->SetBgColor(v[0..3])`: nothing for a combo box, the scroll bar's,
/// else UIControl's.
pub(crate) fn virt_bg(c: Cpp, color: bool, mut v: [f32; 4]) {
    if direct::is(Kind::Combo, c) {
        return;
    }
    if direct::is(Kind::ScrollBar, c) {
        let o = direct::find(Kind::ScrollBar, c) as *const crate::scrollbarobj::EeUiScrollBar;
        let call = if color { B::SetBgColor } else { B::SetBgOpacity };
        // SAFETY: a live scroll bar
        unsafe { crate::scrollbarobj::ee_ui_scroll_bar_call(o, call, 0, 0, v.as_mut_ptr()) };
        return;
    }
    direct::control_call_f(c, if color { C::SetBgColor } else { C::SetBgOpacity }, 0, 0, &mut v);
}
/// `c->ReleaseCapturedKeyboard()`: the edit box's, else UIControl's.
pub(crate) fn virt_release_keyboard(c: Cpp) {
    if direct::is(Kind::Edit, c) {
        direct::edit_call(c, crate::editobj::EeUiEditCall::ReleaseKeyboard, 0, 0);
    } else {
        ctl_call(c, C::ReleaseKeyboard, 0);
    }
}
/// `c->HideControl()`: the combo box's, else nothing.
fn virt_hide(c: Cpp) {
    if direct::is(Kind::Combo, c) {
        direct::combo_call(c, crate::comboobj::EeUiComboCall::HideControl, 0, 0);
    }
}
/// `c->GetIMEEnabled()`: the edit box's, else false.
fn virt_ime_enabled(c: Cpp) -> isize {
    if direct::is(Kind::Edit, c) {
        bl(direct::edit_call(c, crate::editobj::EeUiEditCall::ImeEnabled, 0, 0).unwrap_or(0))
    } else {
        0
    }
}
/// `c->OpenIMECompositionString()` (`open`) or `CloseIMECompositionString()`: the edit box's, else kErrorFailed.
fn virt_ime(c: Cpp, open: bool) -> isize {
    use crate::editobj::EeUiEditCall as E;
    if !direct::is(Kind::Edit, c) {
        return 9;
    }
    direct::edit_call(c, if open { E::ImeOpen } else { E::ImeClose }, 0, 0).unwrap_or(0) as core::ffi::c_long as isize
}
/// `c->OpenIMECompositionString()`.
pub(crate) fn control_ime_open(c: usize) -> core::ffi::c_long {
    virt_ime(c as Cpp, true) as core::ffi::c_long
}
/// `c->CloseIMECompositionString()`.
pub(crate) fn control_ime_close(c: usize) -> core::ffi::c_long {
    virt_ime(c as Cpp, false) as core::ffi::c_long
}
/// `c->SetBgOpacity(opacity)`.
pub(crate) fn control_set_bg_opacity(c: usize, opacity: f32) {
    virt_bg(c as Cpp, false, [opacity, 0.0, 0.0, 0.0]);
}
/// `c->SetBgColor(GEColor(red, green, blue))`.
pub(crate) fn control_set_bg_color(c: usize, red: f32, green: f32, blue: f32) {
    virt_bg(c as Cpp, true, [red, green, blue, 0.0]);
}
/// `c->ChangeFont(font)`: the button's, edit box's, status bar's or label's, else nothing.
fn virt_change_font(c: Cpp, font: isize) {
    if direct::is(Kind::Button, c) {
        direct::button_call(c, crate::buttonobj::EeUiButtonCall::ChangeFont, font, 0);
    } else if direct::is(Kind::Edit, c) {
        direct::edit_call(c, crate::editobj::EeUiEditCall::ChangeFont, font, 0);
    } else if direct::is(Kind::StatusBar, c) {
        direct::status_call(c, crate::statusobj::EeUiStatusBarCall::ChangeFont, font, 0, &mut [0.0; 4]);
    } else if direct::is(Kind::Label, c) {
        direct::label_call(c, crate::labelobj::EeUiLabelCall::ChangeFont, font, 0);
    }
}
/// `c->MouseWheelTurned(delta)`: the combo box's, list box's or label's, else kErrorFailed.
fn virt_wheel(c: Cpp, delta: isize) -> isize {
    let d = delta as core::ffi::c_long as isize;
    let r = if direct::is(Kind::Combo, c) {
        direct::combo_call(c, crate::comboobj::EeUiComboCall::MouseWheelTurned, d, 0)
    } else if direct::is(Kind::List, c) {
        direct::list_call(c, crate::listobj::EeUiListCall::MouseWheelTurned, d, 0)
    } else if direct::is(Kind::Label, c) {
        direct::label_call(c, crate::labelobj::EeUiLabelCall::MouseWheelTurned, d, 0)
    } else {
        Some(9)
    };
    lg(r.unwrap_or(0))
}

ops! {
    /// `ctl->SetBgOpacity(f[0])`.
    ctl_set_bg_opacity => |o, a, b, f| { virt_bg(o, false, f4(f)); 0 };
    /// `ctl->SetBgColor(f[0..3])`.
    ctl_set_bg_color => |o, a, b, f| { virt_bg(o, true, [*f, *f.add(1), *f.add(2), 0.0]); 0 };
    /// `a->SetBgOpacity(f[0])` (a part, a table's control).
    part_set_bg_opacity => |o, a, b, f| { virt_bg(p(a), false, [*f, 0.0, 0.0, 0.0]); 0 };
    /// `a->SetBgColor(f[0..3])` (a part, a table's control).
    part_set_bg_color => |o, a, b, f| { virt_bg(p(a), true, [*f, *f.add(1), *f.add(2), 0.0]); 0 };
    /// `ctl->ReleaseCapturedKeyboard()`.
    ctl_release_keyboard => |o, a, b, f| { virt_release_keyboard(o); 0 };
    /// `a->ReleaseCapturedKeyboard()` (the form's control).
    form_ctl_release_keyboard => |o, a, b, f| { virt_release_keyboard(p(a)); 0 };
    /// `ctl->HideControl()`.
    ctl_hide => |o, a, b, f| { virt_hide(o); 0 };
    /// `a->HideControl()` (the form's or group's control).
    part_hide => |o, a, b, f| { virt_hide(p(a)); 0 };
    /// `a->GetIMEEnabled()` (the form's or manager's control).
    part_ime_enabled => |o, a, b, f| virt_ime_enabled(p(a));
    /// `a->MouseWheelTurned(b)` (the form's control).
    part_wheel_turned => |o, a, b, f| virt_wheel(p(a), b);
    /// `CB->GetList()->MouseWheelTurned(b)` (o the combo box).
    combo_a_list_wheel => |o, a, b, f| virt_wheel(direct::combo_list(o), b);
}
/// `a->ChangeFont(c)` (a table's control).
///
/// # Safety
/// The host contract.
pub(crate) unsafe fn table_change_font(
    _o: *mut c_void,
    a: isize,
    _b: isize,
    c: isize,
    _d: isize,
    _f: *mut f32,
) -> isize {
    virt_change_font(p(a), c);
    0
}

// ---- the scroll bars (a `UIScrollBar*`) and sliders (a `UISlider*`) the controls own

use crate::scrollbarobj::EeUiScrollBarCall as B;
use crate::sliderobj::EeUiSliderCall as S;

fn bar_call(bar: Cpp, call: B, a: isize, b: isize) -> isize {
    let o = direct::find(Kind::ScrollBar, bar) as *const crate::scrollbarobj::EeUiScrollBar;
    if o.is_null() {
        return 0;
    }
    let mut v = [0.0f32; 4];
    // SAFETY: a live scroll bar
    unsafe { crate::scrollbarobj::ee_ui_scroll_bar_call(o, call, a, b, v.as_mut_ptr()) }
}
fn slider(s: Cpp, call: S, a: isize, b: isize) -> isize {
    direct::slider_call(s, call, a, b).unwrap_or(0)
}
/// `(long)` of a number answer.
fn lg(x: isize) -> isize {
    x as core::ffi::c_long as isize
}

ops! {
    // the form's table ops (o the form, a the table)
    /// `UIControlsTable::GetUIControlsTable(a)`.
    form_table => |o, a, b, f| table_at(ul(a));
    /// `a->GetFormIndex()`.
    form_table_form_index => |o, a, b, f| table(p(a), |t| t.fields.form_index) as isize;
    /// `a->GetGroup()`.
    form_table_group => |o, a, b, f| table(p(a), |t| t.fields.group) as isize;
    /// `a->GetIsVisible() == true`.
    form_table_visible => |o, a, b, f| table(p(a), |t| t.fields.is_visible) as isize;
    /// `a->GetControlType()`.
    form_table_type => |o, a, b, f| table(p(a), |t| t.fields.control_type) as isize;
    /// `a->GetStringTableID()`.
    form_table_string_id => |o, a, b, f| table(p(a), |t| t.fields.string_table_id) as isize;
    /// `a->CreateControl(theForm->GetManager())`.
    form_table_create_control => |o, a, b, f| table_call(p(a), crate::tableobj::EeUiTableCall::CreateControl, mgr(o) as isize, 0);
    /// `a->CreateControlEvent(theForm)`.
    form_table_create_event => |o, a, b, f| table_call(p(a), crate::tableobj::EeUiTableCall::CreateControlEvent, o as isize, 0) * 0;
    /// `a->RelayoutControl(b, theForm->GetManager())`.
    form_table_relayout => |o, a, b, f| table_call(p(a), crate::tableobj::EeUiTableCall::RelayoutControl, b, mgr(o) as isize) * 0;
    // the form's hot key ops (o the form, a the hot key)
    /// `a->GetAlwaysFire()`.
    form_hot_key_always => |o, a, b, f| hot_key(p(a), |k| k.mAlwaysFire) as isize;
    /// `a->GetKeyCount()`.
    form_hot_key_keys => |o, a, b, f| hot_key(p(a), |k| k.mKeyCount) as isize;
    /// `a->mKey`.
    form_hot_key_key => |o, a, b, f| hot_key(p(a), |k| k.mKey as usize) as isize;
    /// `a->mModifiers[0]`.
    form_hot_key_mod0 => |o, a, b, f| hot_key(p(a), |k| k.mModifiers[0] as usize) as isize;
    /// `a->mModifiers[1]`.
    form_hot_key_mod1 => |o, a, b, f| hot_key(p(a), |k| k.mModifiers[1] as usize) as isize;
    /// `a->mModifiers[2]`.
    form_hot_key_mod2 => |o, a, b, f| hot_key(p(a), |k| k.mModifiers[2] as usize) as isize;
    /// `a->mEnabled = b != 0`.
    form_hot_key_set_enabled => |o, a, b, f| { hot_key(p(a), |k| k.mEnabled = b != 0); 0 };
    /// `a->mForm = theForm`.
    form_hot_key_set_form => |o, a, b, f| { hot_key(p(a), |k| k.mForm = o); 0 };
    /// `a->mAlwaysFire = b != 0`.
    form_hot_key_set_always => |o, a, b, f| { hot_key(p(a), |k| k.mAlwaysFire = b != 0); 0 };
    /// `a->SetKey(b)`.
    form_hot_key_set_key => |o, a, b, f| hot_key_call(p(a), crate::hotkeyobj::EeUiHotKeyCall::SetKey, ul(b), 0) * 0;
    /// `static_cast<UIHotKey*>(a)->SetEnabled(b != 0)` (a group's hot key).
    group_set_enabled => |o, a, b, f| { hot_key(p(a), |k| k.mEnabled = b != 0); 0 };
    // a control's scroll bar (a the bar)
    /// `a->SetRange(0, b)`.
    bar_set_range => |o, a, b, f| bar_call(p(a), B::SetRange, 0, ul(b)) * 0;
    /// `a->SetPageSize(b)`.
    bar_set_page_size => |o, a, b, f| bar_call(p(a), B::SetPageSize, ul(b), 0) * 0;
    /// `a->GetSlider()->GetCurrentRangeValue()`.
    bar_value => |o, a, b, f| lg(slider(bar_call(p(a), B::Slider, 0, 0) as Cpp, S::CurrentRangeValue, 0, 0));
    /// `a->GetSlider()->GetMaxRange()`.
    bar_max => |o, a, b, f| lg(slider(bar_call(p(a), B::Slider, 0, 0) as Cpp, S::MaxRange, 0, 0));
    /// `a->SetCurrentRangeValue(b)`.
    bar_set_value => |o, a, b, f| bar_call(p(a), B::SetCurrentRangeValue, ul(b), 0) * 0;
    /// `a->IncrementBy(b)`.
    bar_increment_by => |o, a, b, f| bar_call(p(a), B::IncrementBy, lg(b), 0) * 0;
    /// `f = a->GetCopiedDrawRectangle()` (the bar's).
    bar_copied_draw_rect => |o, a, b, f| { ctl(p(a), |r| put_rect(f, &r.mCopiedDrawRectangle)); 0 };
    // a scroll bar's slider (a the slider)
    /// `a->GetSliderStyle()`.
    bar_slider_style => |o, a, b, f| ul(slider(p(a), S::Style, 0, 0));
    /// `a->GetCurrentRangeValue()`.
    bar_slider_value => |o, a, b, f| lg(slider(p(a), S::CurrentRangeValue, 0, 0));
    /// `a->SetCurrentRangeValue(b)`.
    bar_slider_set_value => |o, a, b, f| slider(p(a), S::SetCurrentRangeValue, lg(b), 0) * 0;
    /// `a->DecrementByPageSize()`.
    bar_slider_decrement_page => |o, a, b, f| slider(p(a), S::DecrementByPageSize, 0, 0) * 0;
    /// `a->IncrementByPageSize()`.
    bar_slider_increment_page => |o, a, b, f| slider(p(a), S::IncrementByPageSize, 0, 0) * 0;
    /// `a->SetPageSize(b)`.
    bar_slider_set_page_size => |o, a, b, f| slider(p(a), S::SetPageSize, ul(b), 0) * 0;
}

/// `a->SetRange(b, c)` (a slider).
///
/// # Safety
/// The host contract.
pub(crate) unsafe fn bar_slider_set_range(
    _o: *mut c_void,
    a: isize,
    b: isize,
    c: isize,
    _d: isize,
    _f: *mut f32,
) -> isize {
    slider(p(a), S::SetRange, ul(b), ul(c));
    0
}
/// `a->SetModifier(b, (unsigned char)c)` (a hot key).
///
/// # Safety
/// The host contract.
pub(crate) unsafe fn form_hot_key_set_modifier(
    _o: *mut c_void,
    a: isize,
    b: isize,
    c: isize,
    _d: isize,
    _f: *mut f32,
) -> isize {
    hot_key_call(p(a), crate::hotkeyobj::EeUiHotKeyCall::SetModifier, ul(b), c as u8 as isize);
    0
}
/// `t->SetFgColor(table->GetTextForeground())` (o the table, a the text): SetupGEText's colour.
///
/// # Safety
/// The host contract: `a` a live `GEText`.
pub(crate) unsafe fn tbl_text_fg(o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
    let c = table(o, |t| [t.text_foreground.red, t.text_foreground.green, t.text_foreground.blue]);
    // SAFETY: the contract (the text's one member is ee_render's text)
    unsafe {
        let text = *(a as *const *mut ee_render::ge::smallobj::EeGeText);
        core::ptr::copy(c.as_ptr(), ee_render::ge::smallobj::ee_ge_text_color(text, true), 3);
    }
    0
}
/// `t->GetStringTableBegin()`.
pub(crate) fn table_string_begin(t: usize) -> i32 {
    table(t as Cpp, |t| t.fields.string_table_begin) as i32
}
/// `t->GetStringTableEnd()`.
pub(crate) fn table_string_end(t: usize) -> i32 {
    table(t as Cpp, |t| t.fields.string_table_end) as i32
}
/// `UIControlsTable::GetUIControlsTable(index)`.
pub(crate) fn table_get(index: u32) -> usize {
    table_at(index as c_ulong as isize) as usize
}
/// `table->AddEvent(id, kind)`.
pub(crate) fn table_add_event(t: usize, id: u32, kind: u32) {
    table_call(t as Cpp, crate::tableobj::EeUiTableCall::AddEvent, id as c_ulong as isize, kind as c_ulong as isize);
}

// ---- the UI database's loaders and the shared tables' statics (each a static C++ member of one Rust call: the
// manager's LoadDatabase and RemoveDatabase, the controls table's fonts)

use crate::fonttableobj::{ee_ui_font_tables, EeUiFontsCall};
use crate::tableobj::{ee_ui_controls_tables, EeUiTablesCall};

/// `UIFontTable::Create(*file, m)`.
///
/// # Safety
/// `file` a live `FSFileSpec`, `m` the live manager.
pub(crate) unsafe fn db_fonts(file: usize, m: usize) -> core::ffi::c_long {
    // SAFETY: the contract
    unsafe {
        ee_ui_font_tables(
            EeUiFontsCall::Create,
            file as isize,
            m as isize,
            0,
            0,
            Some(crate::lle_hosts::ee_ui_font_table_host),
        ) as core::ffi::c_long
    }
}
/// `UIForm::Create(*file, m)`.
///
/// # Safety
/// As [`db_fonts`].
pub(crate) unsafe fn db_forms(file: usize, m: usize) -> core::ffi::c_long {
    // SAFETY: the contract
    unsafe { crate::lle_form::ee_ui_form_create(file as *mut c_void, m as *mut c_void) }
}
/// `UIBackgroundTable::Create(*file)`.
///
/// # Safety
/// `file` a live `FSFileSpec`.
pub(crate) unsafe fn db_backgrounds(file: usize) -> core::ffi::c_long {
    // SAFETY: the contract
    unsafe { crate::bgtableobj::ee_ui_bgtable_create(file as *mut c_void, Some(crate::lle_hosts::ee_ui_bg_table_host)) }
}
/// `UIManager::CreateBackgrounds`: the backgrounds of every table entry on their forms.
pub(crate) fn db_create_backgrounds(m: usize) {
    crate::bgtableobj::ee_ui_bgtable_create_backgrounds(m as *mut c_void, Some(crate::lle_hosts::ee_ui_bg_table_host));
}
/// `UIControlsTable::Create(*file, m)`.
///
/// # Safety
/// As [`db_fonts`].
pub(crate) unsafe fn db_controls(file: usize, m: usize) {
    // SAFETY: the contract
    unsafe {
        ee_ui_controls_tables(
            EeUiTablesCall::Create,
            file as isize,
            m as isize,
            Some(crate::lle_table::ee_ui_table_host),
            core::ptr::null_mut(),
        )
    };
}
/// `UIEvent::CreateControlEvents(*file)`.
///
/// # Safety
/// As [`db_backgrounds`].
pub(crate) unsafe fn db_control_events(file: usize) {
    // SAFETY: the contract
    unsafe {
        crate::eventobj::ee_ui_event_create(
            file as *mut c_void,
            Some(crate::lle_manager::ee_ui_event_host),
            0,
            core::ptr::null_mut(),
        )
    };
}
/// `UIEvent::CreateFormEvents(*file, m)`.
///
/// # Safety
/// As [`db_fonts`].
pub(crate) unsafe fn db_form_events(file: usize, m: usize) {
    // SAFETY: the contract
    unsafe {
        crate::eventobj::ee_ui_event_create(
            file as *mut c_void,
            Some(crate::lle_manager::ee_ui_event_host),
            1,
            m as *mut c_void,
        )
    };
}
/// `UIHotKey::CreateHotKeys(*file)`.
///
/// # Safety
/// As [`db_backgrounds`].
pub(crate) unsafe fn db_hot_keys(file: usize) {
    // SAFETY: the contract
    unsafe {
        crate::hotkeyobj::ee_ui_hotkey_create_all(file as *mut c_void, Some(crate::lle_hosts::ee_ui_hotkey_host))
    };
}
/// `UIControlsTable::DeleteAll()`.
pub(crate) fn db_remove_controls() {
    // SAFETY: no host, no floats
    unsafe { ee_ui_controls_tables(EeUiTablesCall::DeleteAll, 0, 0, None, core::ptr::null_mut()) };
}
/// `UIBackgroundTable::DeleteAll()`.
pub(crate) fn db_remove_backgrounds() {
    crate::bgtableobj::ee_ui_bgtable_delete_all(Some(crate::lle_hosts::ee_ui_bg_table_host));
}
/// `UIFontTable::DeleteAll()`.
pub(crate) fn db_remove_fonts() {
    // SAFETY: no arguments
    unsafe { ee_ui_font_tables(EeUiFontsCall::DeleteAll, 0, 0, 0, 0, Some(crate::lle_hosts::ee_ui_font_table_host)) };
}

ops! {
    /// `UIHotKey::DeleteHotKeys()` (the manager's).
    mgr_hot_keys_delete => |o, a, b, f| { crate::hotkeyobj::ee_ui_hotkey_delete_all(Some(crate::lle_hosts::ee_ui_hotkey_host)); 0 };
    /// `UIFontTable::GetFont(a)` (the controls table's).
    tbl_font_get => |o, a, b, f| ee_ui_font_tables(EeUiFontsCall::GetFont, ul(a), 0, 0, 0, Some(crate::lle_hosts::ee_ui_font_table_host));
}

/// `UIFontTable::CreateClosestFont(a, b)`.
///
/// # Safety
/// The host contract.
pub(crate) unsafe fn tbl_font_closest(
    _o: *mut c_void,
    a: isize,
    b: isize,
    _c: isize,
    _d: isize,
    _f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe {
        ee_ui_font_tables(
            EeUiFontsCall::CreateClosestFont,
            ul(a),
            ul(b),
            0,
            0,
            Some(crate::lle_hosts::ee_ui_font_table_host),
        )
    }
}

// ---- the combo box's parts (`PartOf(i)`: the Rust combo box's part `i`, a C++ control)

/// `combo->PartOf(i)`.
fn part_of(combo: Cpp, i: isize) -> Cpp {
    direct::combo_call(combo, crate::comboobj::EeUiComboCall::Part, i as core::ffi::c_int as isize, 0).unwrap_or(0)
        as Cpp
}

ops! {
    /// `f = CB->PartOf(a)->GetRectangle()`.
    combo_q_rect_of => |o, a, b, f| { ctl(part_of(o, a), |r| put_rect(f, &r.mRectangle)); 0 };
    /// `CB->PartOf(0)->GetBorderThickness()`.
    combo_q_text_box_border => |o, a, b, f| ctl(part_of(o, 0), |r| r.mBorderThickness) as isize;
    /// `CB->PartOf(3)->GetText(0)` (the edit box's first line).
    combo_q_edit_text => |o, a, b, f| direct::edit_call(part_of(o, 3), crate::editobj::EeUiEditCall::Line, 0, 0).unwrap_or(0);
    /// `CB->UIControl::CopyData()`.
    combo_a_base_copy_data => |o, a, b, f| ctl_call(o, C::CopyData, 0) as core::ffi::c_long as isize;
    /// `CB->UIControl::SetRectangle(rect f)`.
    combo_a_base_set_rectangle => |o, a, b, f| ctl_f(o, C::SetRectangle, f) * 0;
    /// `CB->UIControl::SetLocation(point f)`.
    combo_a_base_set_location => |o, a, b, f| direct::control_call_f(o, C::SetLocation, 0, 0, &mut [*f, *f.add(1), 0.0, 0.0]) * 0;
}

/// `combo->PartOf(part)` (lle_combo.rs).
pub(crate) fn combo_part(combo: usize, part: u32) -> usize {
    part_of(combo as Cpp, part as isize) as usize
}
/// `combo->UIControl::SetRectangle(GERectangle(text_box's top left, drop's bottom right))`.
pub(crate) fn combo_base_set_rect(combo: usize, text_box: usize, drop: usize) {
    let t = ctl(text_box as Cpp, |r| r.mRectangle);
    let d = ctl(drop as Cpp, |r| r.mRectangle);
    let r = UiRect::new(t.left, t.top, d.right, d.bottom);
    direct::control_call_f(combo as Cpp, C::SetRectangle, 0, 0, &mut [r.bottom, r.left, r.right, r.top]);
}

// ---- the controls' own non-virtual members (the controls table's setters, the combo box's list, the radio
// button's bitmaps): each the inward call of its C++ forwarder, on the Rust object direct.rs finds

/// The Rust object of `kind` the control `c` has.
fn obj<T>(kind: Kind, c: Cpp) -> *const T {
    direct::find(kind, c) as *const T
}
/// The forwarder's three-float colour (`CallF(r, g, b)` / `UIF4(GEColor)`: the fourth float 0).
unsafe fn rgb(f: *const f32) -> [f32; 4] {
    // SAFETY: the host's buffer holds four floats
    unsafe { [*f, *f.add(1), *f.add(2), 0.0] }
}
/// The forwarder's one float (`CallF(v)`).
unsafe fn one(f: *const f32) -> [f32; 4] {
    // SAFETY: as rgb
    unsafe { [*f, 0.0, 0.0, 0.0] }
}

macro_rules! sub {
    ($(#[$m:meta])* $name:ident, $kind:expr, $ty:ty, $call_ty:ty, $call:path) => {
        $(#[$m])*
        fn $name(c: Cpp, call: $call_ty, a: isize, b: isize, mut v: [f32; 4]) -> isize {
            let o = obj::<$ty>($kind, c);
            if o.is_null() {
                return 0;
            }
            // SAFETY: a live object of the kind (registered between its construction and destruction)
            unsafe { $call(o, call, a, b, v.as_mut_ptr()) }
        }
    };
}
sub!(
    /// One of `UIButton`'s (or a subclass's) methods with the floats `v`.
    button, Kind::Button, crate::buttonobj::EeUiButton, crate::buttonobj::EeUiButtonCall, crate::buttonobj::ee_ui_button_call
);
sub!(
    /// One of `UIListBox`'s methods with the floats `v`.
    list, Kind::List, crate::listobj::EeUiList, crate::listobj::EeUiListCall, crate::listobj::ee_ui_list_call
);
sub!(
    /// One of `UIStatusBar`'s methods with the floats `v`.
    status, Kind::StatusBar, crate::statusobj::EeUiStatusBar, crate::statusobj::EeUiStatusBarCall, crate::statusobj::ee_ui_status_bar_call
);
sub!(
    /// One of `UITextLabel`'s methods with the floats `v`.
    label, Kind::Label, crate::labelobj::EeUiLabel, crate::labelobj::EeUiLabelCall, crate::labelobj::ee_ui_label_call
);
sub!(
    /// One of `UIComboBox`'s methods with the floats `v`.
    combo, Kind::Combo, crate::comboobj::EeUiCombo, crate::comboobj::EeUiComboCall, crate::comboobj::ee_ui_combo_call
);
/// One of `UIGraph`'s methods (`a`, `b`, `c` and the floats `v`).
fn graph(g: Cpp, call: crate::graphobj::EeUiGraphCall, a: isize, mut v: [f32; 4]) -> isize {
    let o = obj::<crate::graphobj::EeUiGraph>(Kind::Graph, g);
    if o.is_null() {
        return 0;
    }
    // SAFETY: a live graph
    unsafe { crate::graphobj::ee_ui_graph_call(o, call, a, 0, 0, v.as_mut_ptr()) }
}

use crate::buttonobj::EeUiButtonCall as Bt;
use crate::comboobj::EeUiComboCall as Cb;
use crate::labelobj::EeUiLabelCall as Lb;
use crate::listobj::EeUiListCall as Ls;
use crate::statusobj::EeUiStatusBarCall as St;

/// A controls-table setter of a made control (`a` the control, `c`, `d` and `f` its values; lle_hosts.rs SET).
macro_rules! setters {
    ($( $(#[$m:meta])* $name:ident => |$a:ident, $c:ident, $d:ident, $f:ident| $e:expr; )*) => {$(
        $(#[$m])*
        ///
        /// # Safety
        /// The host contract: `a` a live control of the setter's class, `f` its floats.
        #[allow(unused_variables, unused_unsafe)]
        pub(crate) unsafe fn $name(_o: *mut c_void, $a: isize, _b: isize, $c: isize, $d: isize, $f: *mut f32) -> isize {
            // SAFETY: the contract
            unsafe { $e };
            0
        }
    )*};
}

setters! {
    /// `SetFocusedTextColor(GEColor(f))` (a button).
    tbl_set_button_focused_color => |a, c, d, f| button(p(a), Bt::SetFocusedTextColor, 0, 0, rgb(f));
    /// `SetChecked(c != 0)` (a check box).
    tbl_set_check_set_checked => |a, c, d, f| button(p(a), Bt::CheckSetChecked, (c != 0) as isize, 0, [0.0; 4]);
    /// `SetListBoxFocusedRectColor(GEColor(f))`: the combo box's list's `SetFocusedRectColor`.
    tbl_set_combo_focused_rect => |a, c, d, f| list(part_of(p(a), 2), Ls::SetFocusedRectColor, 0, 0, rgb(f));
    /// `SetCurrentSelection(c)`.
    tbl_set_combo_selection => |a, c, d, f| combo(p(a), Cb::SetCurrentSelection, ul(c), 0, [0.0; 4]);
    /// `SetListBoxAlwaysDirty(c != 0)`.
    tbl_set_combo_list_always_dirty => |a, c, d, f| combo(p(a), Cb::SetListBoxAlwaysDirty, (c != 0) as isize, 0, [0.0; 4]);
    /// `SetText((unsigned long)c)` (an edit box: its decimal).
    tbl_set_edit_number => |a, c, d, f| {
        let e = obj::<crate::editobj::EeUiEdit>(Kind::Edit, p(a));
        if !e.is_null() {
            crate::editobj::ee_ui_edit_set_number(e, c as c_ulong);
        }
    };
    /// `SetSelectedBgColor(GEColor(f))` (an edit box).
    tbl_set_edit_selected_bg => |a, c, d, f| {
        let e = obj::<crate::editobj::EeUiEdit>(Kind::Edit, p(a));
        if !e.is_null() {
            crate::editobj::ee_ui_edit_call(e, crate::editobj::EeUiEditCall::SetSelectedBgColor, 0, 0, 0, 0, rgb(f).as_mut_ptr());
        }
    };
    /// `SetFocusedRectColor(GEColor(f))` (a list box).
    tbl_set_list_focused_rect => |a, c, d, f| list(p(a), Ls::SetFocusedRectColor, 0, 0, rgb(f));
    /// `SetScrollBarBgOpacity(f[0])`.
    tbl_set_list_scroll_opacity => |a, c, d, f| list(p(a), Ls::SetScrollBarBgOpacity, 0, 0, one(f));
    /// `SetFont(c)` (a graph).
    tbl_set_graph_font => |a, c, d, f| graph(p(a), crate::graphobj::EeUiGraphCall::SetFont, c, [0.0; 4]);
    /// `SetFontColor(GEColor(f))`.
    tbl_set_graph_font_color => |a, c, d, f| graph(p(a), crate::graphobj::EeUiGraphCall::SetFontColor, 0, rgb(f));
    /// `SetUpUncheckedBitmap(c)` (a radio button).
    tbl_set_radio_up_unchecked => |a, c, d, f| button(p(a), Bt::RadioSetUpUnchecked, c, 0, [0.0; 4]);
    /// `SetRange(c, d)` (a scroll bar).
    tbl_set_scroll_range => |a, c, d, f| bar_call(p(a), B::SetRange, ul(c), ul(d));
    /// `SetCurrentRangeValue(c)`.
    tbl_set_scroll_value => |a, c, d, f| bar_call(p(a), B::SetCurrentRangeValue, ul(c), 0);
    /// `SetRange(c, d)` (a slider).
    tbl_set_slider_range => |a, c, d, f| slider(p(a), S::SetRange, ul(c), ul(d));
    /// `SetCurrentRangeValue(c)`.
    tbl_set_slider_value => |a, c, d, f| slider(p(a), S::SetCurrentRangeValue, lg(c), 0);
    /// `SetPageSize(c)`.
    tbl_set_slider_page => |a, c, d, f| slider(p(a), S::SetPageSize, ul(c), 0);
    /// `SetRange(c, d)` (a status bar).
    tbl_set_status_range => |a, c, d, f| status(p(a), St::SetRange, ul(c), ul(d), [0.0; 4]);
    /// `SetCurrentRangeValue(c)`.
    tbl_set_status_value => |a, c, d, f| status(p(a), St::SetCurrentRangeValue, lg(c), 0, [0.0; 4]);
    /// `SetStatusBarColor(GEColor(f))`.
    tbl_set_status_color => |a, c, d, f| status(p(a), St::SetStatusBarColor, 0, 0, rgb(f));
    /// `SetStatusBarOpacity(f[0])`.
    tbl_set_status_opacity => |a, c, d, f| status(p(a), St::SetStatusBarOpacity, 0, 0, one(f));
    /// `SetFocusedTextColor(GEColor(f))`.
    tbl_set_status_focused_color => |a, c, d, f| status(p(a), St::SetFocusedTextColor, 0, 0, rgb(f));
    /// `SetText((long)c)` (a label: its decimal).
    tbl_set_label_number => |a, c, d, f| {
        let l = obj::<crate::labelobj::EeUiLabel>(Kind::Label, p(a));
        if !l.is_null() {
            crate::labelobj::ee_ui_label_set_number(l, c as core::ffi::c_long);
        }
    };
    /// `SetAlignment(c)`.
    tbl_set_label_align => |a, c, d, f| label(p(a), Lb::SetAlignment, ul(c), 0, [0.0; 4]);
    /// `SetScrollBarBgOpacity(f[0])`.
    tbl_set_label_scroll_opacity => |a, c, d, f| label(p(a), Lb::SetScrollBarBgOpacity, 0, 0, one(f));
    /// `SetFitRectToText(c != 0)`.
    tbl_set_label_fit => |a, c, d, f| label(p(a), Lb::SetFitRectToText, (c != 0) as isize, 0, [0.0; 4]);
    /// `SetAlwaysDirty(c != 0)` (any control).
    tbl_set_always_dirty => |a, c, d, f| ctl(p(a), |r| r.mAlwaysDirty = c != 0);
    /// `SetTriggerEvent(c, d)`.
    tbl_set_set_trigger => |a, c, d, f| ctl_call2(p(a), C::SetTrigger, ul(c), d);
}

fn ctl_call2(c: Cpp, call: C, a: isize, b: isize) -> isize {
    direct::control_call(c, call, a, b)
}

ops! {
    /// `static_cast<UIRadioButton*>(a)->GetIsChecked()`.
    form_ctl_checked => |o, a, b, f| bl(button(p(a), Bt::RadioIsChecked, 0, 0, [0.0; 4]));
    /// `static_cast<UITextLabel*>(a)->GetFitRectToText()`.
    form_label_fit => |o, a, b, f| bl(label(p(a), Lb::FitRectToText, 0, 0, [0.0; 4]));
    /// `CB->GetList()->GetLineHeight()`.
    combo_q_line_height => |o, a, b, f| ul(list(part_of(o, 2), Ls::LineHeight, 0, 0, [0.0; 4]));
    /// `CB->GetList()->GetBorderThickness()`.
    combo_q_list_border => |o, a, b, f| ctl(part_of(o, 2), |r| r.mBorderThickness) as isize;
    /// `CB->GetList()->GetScrollBarCaptured()`.
    combo_q_list_scroll_bar_captured => |o, a, b, f| bl(list(part_of(o, 2), Ls::ScrollBarCaptured, 0, 0, [0.0; 4]));
    /// `CB->GetList()->ChangeWidth(b)`.
    combo_a_list_change_width => |o, a, b, f| list(part_of(o, 2), Ls::ChangeWidth, ul(b), 0, [0.0; 4]) * 0;
    /// `FILE_LIST->GetCurrentSelection(*(unsigned long*)f)`.
    fl_list_selection => |o, a, b, f| {
        let l = obj::<crate::listobj::EeUiList>(Kind::List, o);
        if l.is_null() { 0 } else { crate::listobj::ee_ui_list_current_selection(l, f as *mut c_ulong) as isize }
    };
    /// `FILE_COMBO->GetCurrentSelection(*(unsigned long*)f)`.
    fl_combo_selection => |o, a, b, f| {
        let c = obj::<crate::comboobj::EeUiCombo>(Kind::Combo, o);
        if c.is_null() { 0 } else { crate::comboobj::ee_ui_combo_current_selection(c, f as *mut c_ulong) as isize }
    };
}

/// `list->GetScrollBar()` (lle_combo.rs).
pub(crate) fn list_scroll_bar(l: usize) -> usize {
    list(l as Cpp, Ls::ScrollBar, 0, 0, [0.0; 4]) as usize
}
/// `list->GetCurrentSelection(*selection)`.
///
/// # Safety
/// `selection` is a live out number.
pub(crate) unsafe fn list_selection(l: usize, selection: crate::lle_cpp::EeUiShimULongOut) -> core::ffi::c_long {
    let o = obj::<crate::listobj::EeUiList>(Kind::List, l as Cpp);
    if o.is_null() {
        return 0;
    }
    // SAFETY: a live list; the contract
    unsafe { crate::listobj::ee_ui_list_current_selection(o, selection.0) }
}
/// `list->SetScrollBarBgOpacity(opacity)`.
pub(crate) fn list_set_scroll_bg_opacity(l: usize, opacity: f32) {
    list(l as Cpp, Ls::SetScrollBarBgOpacity, 0, 0, [opacity, 0.0, 0.0, 0.0]);
}
/// `list->SetScrollBarBgColor(GEColor(red, green, blue))`.
pub(crate) fn list_set_scroll_bg_color(l: usize, red: f32, green: f32, blue: f32) {
    list(l as Cpp, Ls::SetScrollBarBgColor, 0, 0, [red, green, blue, 0.0]);
}
/// `radio->SetDownUncheckedBitmap(bitmap)` (the table's).
pub(crate) fn radio_set_down_unchecked(control: usize, bitmap: usize) {
    button(control as Cpp, Bt::RadioSetDownUnchecked, bitmap as isize, 0, [0.0; 4]);
}
/// `radio->SetDownCheckedBitmap(bitmap)`.
pub(crate) fn radio_set_down_checked(control: usize, bitmap: usize) {
    button(control as Cpp, Bt::RadioSetDownChecked, bitmap as isize, 0, [0.0; 4]);
}
/// `radio->SetUpCheckedBitmap(bitmap)`.
pub(crate) fn radio_set_up_checked(control: usize, bitmap: usize) {
    button(control as Cpp, Bt::RadioSetUpChecked, bitmap as isize, 0, [0.0; 4]);
}

// ---- the made controls' texts (`SetupText`'s, lle_hosts.rs TEXT: `a` the control) and the label's, edit box's and
// graph's text calls

/// The text of the label `l` (`GetGEText()`).
fn label_text_of(l: Cpp) -> isize {
    label(l, Lb::Text, 0, 0, [0.0; 4])
}
/// The text of the edit box `e` (`GetGEText()`).
fn edit_text_of(e: Cpp) -> isize {
    direct::edit_call(e, crate::editobj::EeUiEditCall::Text, 0, 0).unwrap_or(0)
}

ops! {
    /// `static_cast<UIButton*>(a)->GetGEText()`.
    tbl_text_button => |o, a, b, f| button(p(a), Bt::Text, 0, 0, [0.0; 4]);
    /// `static_cast<UICheckBox*>(a)->GetTextLabel()->GetGEText()` (a radio button's alike).
    tbl_text_check => |o, a, b, f| label_text_of(button(p(a), Bt::TextLabel, 0, 0, [0.0; 4]) as Cpp);
    /// `static_cast<UIComboBox*>(a)->GetGEText()` (the label's or edit box's).
    tbl_text_combo => |o, a, b, f| {
        let c = obj::<crate::comboobj::EeUiCombo>(Kind::Combo, p(a));
        if c.is_null() { 0 } else { crate::comboobj::ee_ui_combo_ge_text(c) as isize }
    };
    /// `static_cast<UIEditBox*>(a)->GetGEText()`.
    tbl_text_edit => |o, a, b, f| edit_text_of(p(a));
    /// `static_cast<UITextLabel*>(a)->GetGEText()`.
    tbl_text_label => |o, a, b, f| label_text_of(p(a));
    /// `static_cast<UIStatusBar*>(a)->GetGEText()`.
    tbl_text_status => |o, a, b, f| status(p(a), St::Text, 0, 0, [0.0; 4]);
    /// `*(GEColor*)f = a->GetFgColor()` (a graph's text).
    graph_text_fg_color => |o, a, b, f| {
        let text = *(a as *const *mut ee_render::ge::smallobj::EeGeText);
        put(f, ee_render::ge::smallobj::ee_ge_text_color(text, true), 3);
        0
    };
}

/// `label->GetGEText()` (lle_combo.rs).
pub(crate) fn label_text(l: usize) -> usize {
    label_text_of(l as Cpp) as usize
}
/// `edit->GetGEText()`.
pub(crate) fn edit_text(e: usize) -> usize {
    edit_text_of(e as Cpp) as usize
}
