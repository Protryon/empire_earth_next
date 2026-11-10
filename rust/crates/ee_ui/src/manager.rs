//! `UIManager`'s form stack (`User Interface/UIManager.cpp`): which forms are on the stack and which of them are
//! visible (a form fully covered by one above it is closed; it reopens when the cover goes), the dirty rectangles a
//! redraw of one form puts on the forms above, the clip rectangles each visible form gets from the forms above it,
//! `Push`, `Pop`, `PopTo`, `SwapForm`, `RedrawAll`, `RelayoutAllForms`, `GetFormByID`, the queued events'
//! dispatch and the mouse wheel's step.
//!
//! The forms are C++ objects seen as opaque handles ([`Form`]); the stack, the visible list and the forms list stay
//! the manager's `std::vector`s, read and edited through [`StackHost`] a slot at a time (never cached across a host
//! call: `Open` and `Close` may change what a form covers). The locks, the drawing, the IME, the music and the input
//! tests stay C++.
//!
//! Faithful to the C++ with one guard: where the original indexed an empty list (`AddClippedForms` with no visible
//! form, `AddToVisibleForms` with a form not on the stack) it read before the vector's start; here those steps are
//! skipped.

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::UiRect;

/// A C++ `UIForm*`, compared by address and never dereferenced here.
pub type Form = *mut c_void;

/// The manager's lists and the forms' members the stack logic touches.
pub trait StackHost {
    /// `mFormStack.size()`.
    fn stack_len(&self) -> usize;
    /// `mFormStack[i]`.
    fn stack(&self, i: usize) -> Form;
    /// `mFormStack.push_back(form)`.
    fn stack_push(&mut self, form: Form);
    /// `mFormStack.pop_back()`.
    fn stack_pop(&mut self);
    /// `mVisibleForms.size()`.
    fn visible_len(&self) -> usize;
    /// `mVisibleForms[i]`.
    fn visible(&self, i: usize) -> Form;
    /// `mVisibleForms.insert(begin() + i, form)`.
    fn visible_insert(&mut self, i: usize, form: Form);
    /// `mVisibleForms.erase(begin() + i)`.
    fn visible_erase(&mut self, i: usize);
    /// `mVisibleForms.push_back(form)`.
    fn visible_push(&mut self, form: Form);
    /// `mVisibleForms.pop_back()`.
    fn visible_pop(&mut self);
    /// `mForms.size()`.
    fn forms_len(&self) -> usize;
    /// `mForms[i]`.
    fn forms(&self, i: usize) -> Form;
    /// `mCopiedForms.clear()`.
    fn copied_clear(&mut self);
    /// `form->GetRectangle()`.
    fn rect(&self, form: Form) -> UiRect;
    /// `form->GetFormID()`.
    fn form_id(&self, form: Form) -> c_ulong;
    /// The virtual `Open()`.
    fn open(&mut self, form: Form);
    /// The virtual `Close()`.
    fn close(&mut self, form: Form);
    /// The virtual `Activate()`.
    fn activate(&mut self, form: Form);
    /// The virtual `Deactivate()`.
    fn deactivate(&mut self, form: Form);
    /// `form->AddDirtyRectangle(rect)`.
    fn add_dirty(&mut self, form: Form, rect: UiRect);
    /// `form->RedrawIntersectControls(rect)`.
    fn redraw_intersect(&mut self, form: Form, rect: UiRect);
    /// `form->ClearAllClipRectangles()`.
    fn clear_clips(&mut self, form: Form);
    /// `form->AddClipRectangle(rect)`.
    fn add_clip(&mut self, form: Form, rect: UiRect);
    /// The virtual `RelayoutForm(rect)`.
    fn relayout(&mut self, form: Form, rect: UiRect);
    /// `form->SetAllControlsDirty()`.
    fn set_all_dirty(&mut self, form: Form);
}

/// `GERectangle::Contains(const GERectangle&)`: `inner` inside `outer`, edges included.
fn contains(outer: &UiRect, inner: &UiRect) -> bool {
    inner.left >= outer.left && inner.right <= outer.right && inner.top >= outer.top && inner.bottom <= outer.bottom
}

/// `GERectangle::And(rect, out)`: the overlap, `None` when the rectangles are apart (touching edges overlap).
fn intersection(a: &UiRect, b: &UiRect) -> Option<UiRect> {
    if a.left > b.right || a.right < b.left || a.top > b.bottom || a.bottom < b.top {
        return None;
    }
    Some(UiRect {
        left: if a.left < b.left { b.left } else { a.left },
        right: if a.right > b.right { b.right } else { a.right },
        top: if a.top < b.top { b.top } else { a.top },
        bottom: if a.bottom > b.bottom { b.bottom } else { a.bottom },
    })
}

fn stack_top(h: &impl StackHost) -> Option<Form> {
    let n = h.stack_len();
    (n > 0).then(|| h.stack(n - 1))
}

fn on_stack(h: &impl StackHost, form: Form) -> bool {
    (0..h.stack_len()).any(|i| h.stack(i) == form)
}

/// `AddClippedForms(index)`: every stacked form below the visible form `index` that only that form covers (no
/// form between them covers it) is made visible again, in stack order.
pub fn add_clipped_forms(h: &mut impl StackHost, index: usize) {
    let target = h.visible(index);
    let mut forms = Vec::new();
    let mut i = 0;
    while i < h.stack_len() && h.stack(i) != target {
        let below = h.stack(i);
        if contains(&h.rect(target), &h.rect(below)) {
            let mut j = i + 1;
            while j < h.stack_len() {
                if h.stack(j) == target {
                    forms.push(below);
                    break;
                }
                if contains(&h.rect(h.stack(j)), &h.rect(below)) {
                    break;
                }
                j += 1;
            }
        }
        i += 1;
    }
    for form in forms {
        add_to_visible_forms(h, form);
    }
}

/// `AddToVisibleForms(form)`: a stacked form is inserted into the visible list at its stack position (and opened)
/// unless already there; returns the index. A form not on the stack is not added (the index reached is returned).
pub fn add_to_visible_forms(h: &mut impl StackHost, form: Form) -> usize {
    let mut index = 0;
    // the C++ compared the empty list's back() here (a read before its start); an empty list never holds the form
    if !(h.visible_len() == 0 && stack_top(h) == Some(form)) {
        let mut i = 0;
        while i < h.stack_len() {
            let visible_here = (index < h.visible_len()).then(|| h.visible(index));
            if h.stack(i) == form {
                if visible_here != Some(form) {
                    h.visible_insert(index, form);
                    h.open(form);
                }
                return index;
            }
            if Some(h.stack(i)) != visible_here {
                i += 1;
                continue;
            }
            index += 1;
            if index == h.visible_len() {
                break;
            }
            i += 1;
        }
        if i == h.stack_len() {
            return index;
        }
    }
    index = h.visible_len();
    h.visible_push(form);
    h.open(form);
    index
}

/// `AddDirtyRectangle(rect, form)`: on the top form directly; on any other visible form, the overlap of the
/// rectangle with it and with every visible form above it (those redraw the controls it cuts).
pub fn add_dirty_rectangle(h: &mut impl StackHost, rect: UiRect, form: Form) {
    if h.stack_len() == 0 {
        return;
    }
    if stack_top(h) == Some(form) {
        h.add_dirty(form, rect);
        return;
    }
    let mut i = 0;
    while i < h.visible_len() {
        if h.visible(i) == form {
            while i < h.visible_len() {
                let above = h.visible(i);
                if let Some(cut) = intersection(&h.rect(above), &rect) {
                    h.add_dirty(above, cut);
                    h.redraw_intersect(above, cut);
                }
                i += 1;
            }
            break;
        }
        i += 1;
    }
}

/// `RemoveClippedForms(index)`: closes the first visible form above `index` that covers it (else marks the form's
/// rectangle dirty), then every visible form below it that it covers.
pub fn remove_clipped_forms(h: &mut impl StackHost, index: usize) {
    let own = h.visible(index);
    let rect = h.rect(own);
    let mut removed = false;
    let mut i = index + 1;
    while i < h.visible_len() {
        let above = h.visible(i);
        if contains(&h.rect(above), &rect) {
            h.close(above);
            h.visible_erase(i);
            removed = true;
            break;
        }
        i += 1;
    }
    if !removed {
        add_dirty_rectangle(h, rect, own);
    }
    let mut i = index;
    while i > 0 {
        i -= 1;
        let below = h.visible(i);
        if contains(&rect, &h.rect(below)) {
            h.close(below);
            h.visible_erase(i);
        }
    }
}

/// `ResetClippedForms`: each visible form's clip rectangles are the rectangles of the visible forms above it.
pub fn reset_clipped_forms(h: &mut impl StackHost) {
    let mut i = 0;
    while i < h.visible_len() {
        let form = h.visible(i);
        h.clear_clips(form);
        let mut j = i + 1;
        while j < h.visible_len() {
            let r = h.rect(h.visible(j));
            h.add_clip(form, r);
            j += 1;
        }
        i += 1;
    }
}

/// `Pop`: closes the top form, shows the forms it covered again, dirties its area on the bottom visible form, and
/// activates the form below.
pub fn pop(h: &mut impl StackHost) {
    if let Some(form) = stack_top(h) {
        h.copied_clear();
        h.deactivate(form);
        h.close(form);
        // the C++ indexed the visible list's last slot without checking it has one
        if h.visible_len() > 0 {
            let last = h.visible_len() - 1;
            add_clipped_forms(h, last);
            let bottom = h.visible(0);
            let rect = h.rect(form);
            add_dirty_rectangle(h, rect, bottom);
        }
        h.stack_pop();
        h.visible_pop();
        if let Some(below) = stack_top(h) {
            h.activate(below);
        }
    }
    reset_clipped_forms(h);
}

/// `PopTo(form)`: pops until the form is on top, when it is on the stack.
pub fn pop_to(h: &mut impl StackHost, form: Form) {
    if on_stack(h, form) {
        while stack_top(h) != Some(form) {
            pop(h);
        }
    }
}

/// `Push(form)`: opens the form on top of the stack (unless it is already stacked), deactivating the old top.
pub fn push(h: &mut impl StackHost, form: Form) {
    if on_stack(h, form) {
        return;
    }
    h.copied_clear();
    if let Some(top) = stack_top(h) {
        h.deactivate(top);
    }
    h.stack_push(form);
    h.visible_push(form);
    h.open(form);
    h.activate(form);
    let last = h.visible_len() - 1;
    remove_clipped_forms(h, last);
    let rect = h.rect(form);
    add_dirty_rectangle(h, rect, form);
    reset_clipped_forms(h);
}

/// `SwapForm(form)`: replaces the top form with another (unless already stacked), showing again what the old one
/// covered when the new one does not cover the old top.
pub fn swap_form(h: &mut impl StackHost, form: Form) {
    if on_stack(h, form) {
        return;
    }
    h.copied_clear();
    if let Some(old) = stack_top(h) {
        h.deactivate(old);
        h.close(old);
        h.open(form);
        h.activate(form);
        let last = h.visible_len() - 1;
        let top_visible = h.visible(last);
        if !contains(&h.rect(form), &h.rect(top_visible)) {
            add_clipped_forms(h, last);
            let bottom = h.visible(0);
            let rect = h.rect(old);
            add_dirty_rectangle(h, rect, bottom);
        }
        h.stack_pop();
        h.visible_pop();
    } else {
        h.open(form);
        h.activate(form);
    }
    h.stack_push(form);
    h.visible_push(form);
    let last = h.visible_len() - 1;
    remove_clipped_forms(h, last);
    let rect = h.rect(form);
    add_dirty_rectangle(h, rect, form);
    reset_clipped_forms(h);
}

/// `RedrawAll`: every visible form and all its controls dirty.
pub fn redraw_all(h: &mut impl StackHost) {
    let mut i = 0;
    while i < h.visible_len() {
        let form = h.visible(i);
        let rect = h.rect(form);
        h.add_dirty(form, rect);
        h.set_all_dirty(form);
        i += 1;
    }
}

/// `RelayoutAllForms`: every stacked form laid out to the window (one pixel short on the right and bottom), then
/// the clip rectangles rebuilt.
pub fn relayout_all_forms(h: &mut impl StackHost, window_width: c_ulong, window_height: c_ulong) {
    let rect = UiRect::new(0.0, 0.0, window_width as f32 - 1.0, window_height as f32 - 1.0);
    let mut i = 0;
    while i < h.stack_len() {
        let form = h.stack(i);
        h.relayout(form, rect);
        i += 1;
    }
    reset_clipped_forms(h);
}

/// `GetFormByID`: the first form with the id.
pub fn form_by_id(h: &impl StackHost, id: c_ulong) -> Option<Form> {
    (0..h.forms_len()).map(|i| h.forms(i)).find(|&f| h.form_id(f) == id)
}

/// `GetMouseWheelScrollAmount`: -1 rolled forward, 1 rolled back, 0 still.
pub fn wheel_scroll_amount(value: c_long) -> c_long {
    match value {
        0 => 0,
        v if v > 0 => -1,
        _ => 1,
    }
}

/// One queued event (`UIEventTrigger`): the referenced event, its form and entity, C++'s.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Trigger {
    pub event: *mut c_void,
    pub form: *mut c_void,
    pub entity: *mut c_void,
}

/// The queue of events to trigger (`mEventsToTrigger`, a `std::deque`): processing an event may queue more.
pub trait EventHost {
    /// The front trigger, removed; `None` when the queue is empty.
    fn take_front(&mut self) -> Option<Trigger>;
    /// `event->ProcessEvent(form, entity)`, then `event->Dereference()`.
    fn process(&mut self, trigger: Trigger);
    /// `push_back(trigger)` (the other forms' events, kept in order).
    fn push_back(&mut self, trigger: Trigger);
}

/// `UpdateUI`'s event loop: every queued event processed in order, those queued meanwhile included.
pub fn process_all_events(h: &mut impl EventHost) {
    while let Some(t) = h.take_front() {
        h.process(t);
    }
}

/// `ProcessAllEventsForForm(form)`: the form's queued events processed in order (those queued meanwhile
/// included); the other forms' stay queued, in order, after them.
pub fn process_all_events_for_form(h: &mut impl EventHost, form: *mut c_void) {
    let mut others = Vec::new();
    while let Some(t) = h.take_front() {
        if t.form != form {
            others.push(t);
        } else {
            h.process(t);
        }
    }
    for t in others {
        h.push_back(t);
    }
}

#[cfg(test)]
mod tests;
