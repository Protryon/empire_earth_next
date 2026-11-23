//! `UIESliderChange` whole in Rust (UI round 15; lle/UIESliderChange.h is its C++ shell, every member one call): the
//! slider's range event that writes the slider's value, scaled by the modifier and plus the offset, into a text
//! label, or the text added for that value.

use core::cell::{Cell, RefCell};
use core::ffi::{c_char, c_long, c_void, CStr};

use crate::direct::{self, Cpp};
use crate::labelobj::EeUiLabelCall;
use crate::sliderobj::EeUiSliderCall;

/// The event's state (`mStrings`, `mLabel`, `mOffset`, `mModifier`).
pub struct EeUiSliderChange {
    /// The C++ `UITextLabel` written to.
    label: Cpp,
    modifier: Cell<f32>,
    offset: Cell<c_long>,
    /// `std::map<float, UWideString>`: a value's text (the first added for a value stays).
    strings: RefCell<Vec<(f32, Vec<u8>)>>,
}

impl EeUiSliderChange {
    /// The label's text for the slider value `value`: the text added for the scaled value, else the value "%g".
    pub fn text(&self, value: c_long) -> Vec<u8> {
        let v = value as f32 * self.modifier.get() + self.offset.get() as f32;
        match self.strings.borrow().iter().find(|(k, _)| *k == v) {
            Some((_, t)) => t.clone(),
            None => ee_render::gx::cformat::format_g(v as f64).into_bytes(),
        }
    }

    /// `AddString(value, text)`: the map's insert (an existing value keeps its text).
    pub fn add(&self, value: f32, text: &[u8]) {
        let mut s = self.strings.borrow_mut();
        if !s.iter().any(|(k, _)| *k == value) {
            s.push((value, text.to_vec()));
        }
    }
}

/// `new UIESliderChange(label, modifier, offset)`.
#[no_mangle]
pub extern "C" fn ee_ui_slider_change_new(label: *mut c_void, modifier: f32, offset: c_long) -> *mut EeUiSliderChange {
    let o = EeUiSliderChange {
        label,
        modifier: Cell::new(modifier),
        offset: Cell::new(offset),
        strings: RefCell::new(Vec::new()),
    };
    Box::into_raw(Box::new(o))
}

/// The destructor.
///
/// # Safety
/// `event` came from `ee_ui_slider_change_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_change_free(event: *mut EeUiSliderChange) {
    if !event.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(event) });
    }
}

/// `AddString(value, text)`.
///
/// # Safety
/// `event` is live; `text` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_change_add(event: *const EeUiSliderChange, value: f32, text: *const c_char) {
    // SAFETY: the contract
    unsafe { (*event).add(value, CStr::from_ptr(text).to_bytes()) }
}

/// `SetModifier(m)` (`which` 0) or `SetOffset(o)` (1, the offset in `value`'s integer part).
///
/// # Safety
/// `event` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_change_set(event: *const EeUiSliderChange, which: u32, value: f32, offset: c_long) {
    // SAFETY: the contract
    let e = unsafe { &*event };
    if which == 0 {
        e.modifier.set(value);
    } else {
        e.offset.set(offset);
    }
}

/// `ProcessEvent(form, slider)`: the label's text set from the slider's value; OK.
///
/// # Safety
/// `event` is live; `slider` is the C++ `UISlider` the event fired for.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_change_process(event: *const EeUiSliderChange, slider: *mut c_void) -> c_long {
    // SAFETY: the contract
    let e = unsafe { &*event };
    let value = direct::slider_call(slider, EeUiSliderCall::CurrentRangeValue, 0, 0).unwrap_or(0) as c_long;
    let mut text = e.text(value);
    text.push(0);
    direct::label_call(e.label, EeUiLabelCall::SetText, text.as_ptr() as isize, 1);
    0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_value_is_scaled_and_printed_g_unless_a_text_was_added() {
        let e = ee_ui_slider_change_new(core::ptr::null_mut(), -4.0, 0x28);
        // SAFETY: made above
        let r = unsafe { &*e };
        assert_eq!(r.text(3), b"28");
        r.add(28.0, b"twenty-eight");
        r.add(28.0, b"kept the first");
        assert_eq!(r.text(3), b"twenty-eight");
        // SAFETY: made above
        unsafe { ee_ui_slider_change_set(e, 0, 0.5, 0) };
        assert_eq!(r.text(3), b"41.5");
        // SAFETY: made above
        unsafe { ee_ui_slider_change_set(e, 1, 0.0, 0) };
        assert_eq!(r.text(1), b"0.5");
        // SAFETY: made above
        unsafe { ee_ui_slider_change_free(e) };
    }
}
