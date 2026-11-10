//! A `UIForm`'s dirty-rectangle bookkeeping and its open, close and copy sequences (`User Interface/UIForm.cpp`:
//! `AddCopyDirtyRectangle`, `AddDirtyRectangle`, `CopyData`, `Open`, `Close`, `SetPopup`, `RedrawIntersectControls`,
//! `SetAllControlsDirty`, `GetToolTipShadowRectangle`).
//!
//! The form, its controls, backgrounds, events and database tables stay C++ objects. The two rectangle lists (the
//! background-redraw `std::vector<GERectangle>` and the per-surface `std::deque<UIDirtyRectangle>`) are reached a slot
//! at a time through [`Lists`]; everything else through [`LifeHost`], with the controls, tables and events as opaque
//! handles. A host call may come back into the form (a control's `SetDisplayMode` dirties the manager, which dirties
//! this form), so nothing is cached across one: each field is read where the C++ read it.

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::UiRect;

/// A C++ object the form holds (a control, a controls-table record, an event): opaque, NULL for none.
pub type Handle = *mut c_void;

/// `UIDirtyRectangle`: a rectangle and the surfaces it still has to be redrawn on.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DirtyRect {
    pub rect: UiRect,
    pub surface_count: c_ulong,
}

/// `kDisplayModeHidden` / `kDisplayModeShown`.
pub const HIDDEN: c_ulong = 0;
pub const SHOWN: c_ulong = 1;
/// `UIEntity::GetType` of a control (`kControlTypeButton`: every `UIControl` is entity type 1).
pub const ENTITY_CONTROL: c_ulong = 1;
/// `kDbControlHelpTextLabel`, `kDbControlMultiLineHelpTextLabel`.
pub const DB_HELP_TEXT_LABEL: c_ulong = 0x18;
pub const DB_MULTI_LINE_HELP_TEXT_LABEL: c_ulong = 0x19;
/// `kToolTipShadowOffset`.
pub const TOOL_TIP_SHADOW_OFFSET: f32 = 3.0;
/// `kFadeInRate`.
pub const FADE_IN_RATE: f32 = 1.0 / 700.0;

/// `GERectangle::Contains(const GERectangle&)`: `inner` inside `outer`, edges included.
pub fn contains_rect(outer: &UiRect, inner: &UiRect) -> bool {
    inner.left >= outer.left && inner.right <= outer.right && inner.top >= outer.top && inner.bottom <= outer.bottom
}

/// `GERectangle::Intersects`: the edges may touch.
pub fn intersects(a: &UiRect, b: &UiRect) -> bool {
    a.left <= b.right && a.right >= b.left && a.top <= b.bottom && a.bottom >= b.top
}

/// `GERectangle::And(const GERectangle&)`: clipped in place; unchanged (and `false`) when they do not overlap.
pub fn and_in_place(r: &mut UiRect, other: &UiRect) -> bool {
    let overlap = !(r.left > other.right || r.right < other.left || r.top > other.bottom || r.bottom < other.top);
    if overlap {
        if r.left < other.left {
            r.left = other.left;
        }
        if r.right > other.right {
            r.right = other.right;
        }
        if r.top < other.top {
            r.top = other.top;
        }
        if r.bottom > other.bottom {
            r.bottom = other.bottom;
        }
    }
    overlap
}

/// `GetToolTipShadowRectangle`: the rectangle moved 3 pixels right and down.
pub fn tool_tip_shadow(r: &UiRect) -> UiRect {
    UiRect::new(
        r.left + TOOL_TIP_SHADOW_OFFSET,
        r.top + TOOL_TIP_SHADOW_OFFSET,
        r.right + TOOL_TIP_SHADOW_OFFSET,
        r.bottom + TOOL_TIP_SHADOW_OFFSET,
    )
}

/// The form's two rectangle lists, a slot at a time.
pub trait Lists {
    /// `mCopyDirtyRectangles.size()`.
    fn copy_len(&self) -> usize;
    /// `mCopyDirtyRectangles[i]`.
    fn copy_at(&self, i: usize) -> UiRect;
    /// `mCopyDirtyRectangles.erase(begin() + i)`.
    fn copy_erase(&mut self, i: usize);
    /// `mCopyDirtyRectangles.push_back(r)`.
    fn copy_push(&mut self, r: UiRect);
    /// `mDirtyRectangles.size()`.
    fn dirty_len(&self) -> usize;
    /// `mDirtyRectangles.front()` then `pop_front()`.
    fn dirty_take_front(&mut self) -> DirtyRect;
    /// `mDirtyRectangles.push_back(d)`.
    fn dirty_push_back(&mut self, d: DirtyRect);
}

/// `AddCopyDirtyRectangle`: adds a rectangle to the background-redraw list so that no two listed rectangles overlap.
/// A listed rectangle inside the new one is dropped and the new one added again; a new one inside a listed one is
/// dropped; an overlapping one is split into the parts right of, left of, above and below the listed one (each added
/// the same way). The first listed rectangle that touches the new one decides.
pub fn add_copy_dirty_rectangle(h: &mut impl Lists, r: UiRect) {
    let mut add = true;
    let mut i = 0;
    while i < h.copy_len() {
        let old = h.copy_at(i);
        if contains_rect(&r, &old) {
            h.copy_erase(i);
            add_copy_dirty_rectangle(h, r);
            add = false;
            break;
        }
        if contains_rect(&old, &r) {
            add = false;
            break;
        }
        if !intersects(&old, &r) {
            i += 1;
            continue;
        }
        // each piece re-reads the listed rectangle: a recursive add may have moved the list's slots
        if r.right > h.copy_at(i).right + 1.0 {
            let old = h.copy_at(i);
            add_copy_dirty_rectangle(h, UiRect::new(old.right + 1.0, r.top, r.right, r.bottom));
        }
        if r.left < h.copy_at(i).left - 1.0 {
            let old = h.copy_at(i);
            add_copy_dirty_rectangle(h, UiRect::new(r.left, r.top, old.left - 1.0, r.bottom));
        }
        if r.top < h.copy_at(i).top - 1.0 {
            let old = h.copy_at(i);
            add_copy_dirty_rectangle(h, UiRect::new(r.left, r.top, r.right, old.top - 1.0));
        }
        if r.bottom > h.copy_at(i).bottom + 1.0 {
            let old = h.copy_at(i);
            add_copy_dirty_rectangle(h, UiRect::new(r.left, old.bottom + 1.0, r.right, r.bottom));
        }
        add = false;
        break;
    }
    if add {
        h.copy_push(r);
    }
}

/// `AddDirtyRectangle` after its popup test: queues the rectangle for every surface, dropping the queued rectangles
/// it covers; a queued rectangle that covers it with the same surface count keeps it out. The queue keeps its order.
pub fn add_dirty_rectangle(h: &mut impl Lists, r: UiRect, surfaces: c_ulong) {
    let new = DirtyRect { rect: r, surface_count: surfaces };
    let mut add = true;
    let mut i = h.dirty_len();
    while i > 0 {
        i -= 1;
        let old = h.dirty_take_front();
        if contains_rect(&old.rect, &new.rect) && old.surface_count == new.surface_count {
            h.dirty_push_back(old);
            add = false;
        } else if !contains_rect(&new.rect, &old.rect) {
            h.dirty_push_back(old);
        }
    }
    if add {
        h.dirty_push_back(new);
    }
}

/// `CopyData`'s last loop: each queued rectangle goes to the background-redraw list and stays queued while it has
/// surfaces left (the count drops by one; a count of 0 wraps and stays, as the C++'s `--` did).
pub fn age_dirty_rectangles(h: &mut impl Lists) {
    let mut k = h.dirty_len();
    while k > 0 {
        k -= 1;
        let mut d = h.dirty_take_front();
        add_copy_dirty_rectangle(h, d.rect);
        d.surface_count = d.surface_count.wrapping_sub(1);
        if d.surface_count != 0 {
            h.dirty_push_back(d);
        }
    }
}

/// The form events [`LifeHost::process_event`] fires.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FormEvent {
    Update = 0,
    Open = 1,
    Close = 2,
}

/// The form, its controls, backgrounds, database records and events as the sequences ask about them.
pub trait LifeHost: Lists {
    /// `mManager->GetNumberOfSurfaces()`.
    fn surfaces(&self) -> c_ulong;
    /// `mRectangle`.
    fn rect(&self) -> UiRect;
    /// `mLayout.CalculateRectangle(screen, mRectangle, false)`.
    fn layout(&mut self, screen: UiRect);
    /// The window of the manager's screen: `GetWidth()`, `GetHeight()`.
    fn window_size(&self) -> (c_ulong, c_ulong);
    /// `TMGetTime()`.
    fn now(&self) -> c_ulong;
    fn open_time(&self) -> c_ulong;
    fn set_open_time(&mut self, t: c_ulong);
    /// `GetShouldFadeIn()`.
    fn should_fade_in(&mut self) -> bool;
    fn set_fade_opacity(&mut self, v: f32);
    /// The event's `ProcessEvent(this, NULL)` when the form has one -> its result.
    fn process_event(&mut self, which: FormEvent) -> Option<c_long>;
    /// `mManager->AddDirtyRectangle(r, this)`.
    fn manager_add_dirty(&mut self, r: UiRect);
    /// `mManager->ProcessAllEventsForForm(this)`.
    fn manager_process_events(&mut self);
    /// `UpdateHelpText(force)`.
    fn update_help_text(&mut self, force: bool);
    /// `SetTabbing(mTabbingOn)`.
    fn reset_tabbing(&mut self);

    /// `mControlCount`.
    fn control_count(&self) -> c_long;
    /// `mControls[i]`.
    fn control(&self, i: c_long) -> Handle;
    /// `mDrawControls[i] = c`.
    fn set_draw_control(&mut self, i: c_long, c: Handle);
    /// `mControlGroupCount`.
    fn group_count(&self) -> c_long;
    fn group_enabled(&self, g: c_long) -> bool;
    fn entity_count(&self, g: c_long) -> c_ulong;
    /// `mEntityGroups[g]->GetEntity(j)`.
    fn entity(&self, g: c_long, j: c_ulong) -> Handle;

    fn popup(&self) -> Handle;
    fn set_popup_field(&mut self, c: Handle);
    fn set_copied_popup(&mut self, c: Handle);
    fn set_copied_popup_rect(&mut self, r: UiRect);
    fn popup_draw_count(&self) -> c_ulong;
    fn set_popup_draw_count(&mut self, n: c_ulong);
    fn help_label(&self) -> Handle;
    fn set_help_label(&mut self, c: Handle);
    fn mouse_over(&self) -> Handle;
    /// `mDrawToolTipShadow`.
    fn draw_shadow(&self) -> bool;

    /// `mBackgrounds.size()`; the `i`th background's `Open()` / `Close()`.
    fn background_count(&self) -> usize;
    fn background_open(&mut self, i: usize);
    fn background_close(&mut self, i: usize);

    /// `mDatabaseControls.size()`, the record's table (`GetUIControlsTable(mTableIndex)`, NULL for none), its control.
    fn db_count(&self) -> usize;
    fn db_table(&self, i: usize) -> Handle;
    fn db_control(&self, i: usize) -> Handle;
    fn set_db_control(&mut self, i: usize, c: Handle);
    /// The table's `CreateControl(mManager)`, `GetGroup()`, `GetIsVisible()`, `GetControlType()`,
    /// `GetStringTableID()`, `CreateControlEvent(this)`.
    fn table_create_control(&mut self, t: Handle) -> Handle;
    fn table_group(&self, t: Handle) -> c_ulong;
    fn table_visible(&self, t: Handle) -> bool;
    fn table_control_type(&self, t: Handle) -> c_ulong;
    fn table_string_id(&self, t: Handle) -> c_ulong;
    fn table_create_event(&mut self, t: Handle);
    /// `AddControl(c, group)`, `RemoveControl(c, group)`, `delete c` (NULL: nothing).
    fn add_control(&mut self, c: Handle, group: c_ulong);
    fn remove_control(&mut self, c: Handle, group: c_ulong);
    fn delete_control(&mut self, c: Handle);

    /// `mEventData.size()`, `mEventData[i]` (event ID, event type).
    fn event_data_count(&self) -> usize;
    fn event_data(&self, i: usize) -> (c_ulong, c_ulong);
    /// `mManager->CreateFormEvent(id)` (NULL: none); `SetFormEvent(type, e)` then `e->Dereference()`.
    fn create_form_event(&mut self, id: c_ulong) -> Handle;
    fn set_form_event(&mut self, kind: c_ulong, e: Handle);

    /// The control's `UIEntity::GetType()`, `GetDisplayMode()`, `GetIsDirty()`, `GetRectangle()`, `GetDrawCount()`,
    /// `GetIndex()`; its virtual `CopyData()`, `SetDirty(d)`, `SetDisplayMode(m)`, `HideControl()` and
    /// `SetStringTableID(id)`.
    fn ctl_entity_type(&self, c: Handle) -> c_ulong;
    fn ctl_mode(&self, c: Handle) -> c_ulong;
    fn ctl_dirty(&self, c: Handle) -> bool;
    fn ctl_rect(&self, c: Handle) -> UiRect;
    fn ctl_draw_count(&self, c: Handle) -> c_ulong;
    fn ctl_index(&self, c: Handle) -> c_ulong;
    fn ctl_copy_data(&mut self, c: Handle) -> c_long;
    fn ctl_set_dirty(&mut self, c: Handle, dirty: bool);
    fn ctl_set_display_mode(&mut self, c: Handle, mode: c_ulong);
    fn ctl_hide(&mut self, c: Handle);
    fn ctl_set_string_id(&mut self, c: Handle, id: c_ulong);
}

/// `SetAllControlsDirty`: every control slot's `SetDirty(true)`, last first.
pub fn set_all_controls_dirty(h: &mut impl LifeHost) {
    let mut i = h.control_count() - 1;
    while i >= 0 {
        let c = h.control(i);
        if !c.is_null() {
            h.ctl_set_dirty(c, true);
        }
        i -= 1;
    }
}

/// `RedrawIntersectControls`: dirties every control of an enabled group that is not hidden and overlaps `r`.
pub fn redraw_intersect_controls(h: &mut impl LifeHost, r: UiRect) {
    let mut g = h.group_count() - 1;
    while g >= 0 {
        if h.group_enabled(g) {
            let mut j = h.entity_count(g) as c_long - 1;
            while j >= 0 {
                let e = h.entity(g, j as c_ulong);
                if h.ctl_entity_type(e) == ENTITY_CONTROL && h.ctl_mode(e) != HIDDEN && intersects(&h.ctl_rect(e), &r) {
                    h.ctl_set_dirty(e, true);
                }
                j -= 1;
            }
        }
        g -= 1;
    }
}

/// `SetPopup`: the old popup hidden and what it (and a tool tip's shadow) covered redrawn; the new one dirtied.
pub fn set_popup(h: &mut impl LifeHost, control: Handle) {
    if !h.popup().is_null() {
        let mut r = h.ctl_rect(h.popup());
        if control.is_null() {
            if h.draw_shadow() && h.popup() == h.help_label() {
                r = tool_tip_shadow(&h.ctl_rect(h.popup()));
                h.manager_add_dirty(r);
                and_in_place(&mut r, &h.ctl_rect(h.popup()));
            }
            redraw_intersect_controls(h, r);
        } else if h.popup() != control && h.popup() == h.help_label() {
            if h.draw_shadow() {
                r = tool_tip_shadow(&h.ctl_rect(h.popup()));
                h.manager_add_dirty(r);
                and_in_place(&mut r, &h.ctl_rect(h.popup()));
            }
            redraw_intersect_controls(h, r);
            let label = h.help_label();
            h.ctl_set_display_mode(label, HIDDEN);
        }
        let popup = h.popup();
        h.ctl_hide(popup);
    }
    h.set_popup_field(control);
    if !control.is_null() {
        h.ctl_set_dirty(control, true);
    }
}

/// `CopyData`: fires the update event, fades in (never: `GetShouldFadeIn` is false), copies every dirty shown control
/// of the enabled groups (last group, last entity first), lists the controls to draw, ages the dirty rectangles and
/// updates the help text -> the last result.
pub fn copy_data(h: &mut impl LifeHost) -> c_long {
    let mut result = 0;
    if let Some(r) = h.process_event(FormEvent::Update) {
        result = r;
    }
    if h.should_fade_in() {
        let v = h.now().wrapping_sub(h.open_time()) as f32 * FADE_IN_RATE;
        let v = if v < 1.0 { v } else { 1.0 }; // min(v, 1.0f)
        let v = if v > 0.0 { v } else { 0.0 }; // max(.., 0.0f)
        h.set_fade_opacity(v);
        set_all_controls_dirty(h);
        let r = h.rect();
        h.manager_add_dirty(r);
    } else {
        h.set_fade_opacity(1.0);
    }
    let mut g = h.group_count() - 1;
    while g >= 0 {
        if h.group_enabled(g) {
            let mut j = h.entity_count(g) as c_long - 1;
            while j >= 0 {
                let c = h.entity(g, j as c_ulong);
                j -= 1;
                if h.ctl_mode(c) != SHOWN {
                    continue;
                }
                if h.ctl_dirty(c) {
                    let r = h.ctl_rect(c);
                    h.manager_add_dirty(r);
                    let popup = h.popup();
                    if !popup.is_null() && (c == popup || intersects(&h.ctl_rect(c), &h.ctl_rect(popup))) {
                        let pr = h.ctl_rect(popup);
                        h.set_copied_popup_rect(pr);
                        let n = h.surfaces();
                        h.set_popup_draw_count(n);
                    }
                    result = h.ctl_copy_data(c);
                }
                if h.ctl_draw_count(c) > 0 && c != h.popup() {
                    let i = h.ctl_index(c) as c_long;
                    let control = h.control(i);
                    h.set_draw_control(i, control);
                }
            }
        }
        g -= 1;
    }
    if !h.popup().is_null() && h.popup_draw_count() > 0 {
        let popup = h.popup();
        h.set_copied_popup(popup);
    }
    age_dirty_rectangles(h);
    if !h.help_label().is_null() && !h.mouse_over().is_null() {
        h.update_help_text(false);
    }
    result
}

/// `Open`: lays the form out on the window, opens the backgrounds, creates the database controls (the visible help
/// text label becomes the form's, hidden) and their events, the form events, fires the open event, records the open
/// time and dirties every control. `SetTabbing` runs once per database record, as in the C++.
pub fn open(h: &mut impl LifeHost) {
    let (w, ht) = h.window_size();
    let screen = UiRect::new(0.0, 0.0, w as f32 - 1.0, ht as f32 - 1.0);
    h.layout(screen);
    let mut j = 0;
    while j < h.background_count() {
        h.background_open(j);
        j += 1;
    }
    let mut i = 0;
    while i < h.db_count() {
        let t = h.db_table(i);
        if !t.is_null() {
            let c = h.table_create_control(t);
            h.set_db_control(i, c);
            if !h.db_control(i).is_null() {
                let group = h.table_group(t);
                h.add_control(h.db_control(i), group);
                let mode = h.table_visible(t) as c_ulong;
                h.ctl_set_display_mode(h.db_control(i), mode);
                let kind = h.table_control_type(t);
                if (kind == DB_HELP_TEXT_LABEL || kind == DB_MULTI_LINE_HELP_TEXT_LABEL) && h.table_visible(t) {
                    let c = h.db_control(i);
                    h.set_help_label(c);
                    h.ctl_set_display_mode(c, HIDDEN);
                }
                let id = h.table_string_id(t);
                if id > 0 {
                    h.ctl_set_string_id(h.db_control(i), id);
                }
            }
        }
        h.reset_tabbing();
        i += 1;
    }
    let mut j = 0;
    while j < h.db_count() {
        let t = h.db_table(j);
        if !t.is_null() {
            h.table_create_event(t);
        }
        j += 1;
    }
    let mut i = 0;
    while i < h.event_data_count() {
        let (id, kind) = h.event_data(i);
        let e = h.create_form_event(id);
        if !e.is_null() {
            h.set_form_event(kind, e);
        }
        i += 1;
    }
    h.process_event(FormEvent::Open);
    let t = h.now();
    h.set_open_time(t);
    set_all_controls_dirty(h);
}

/// `Close`: fires the close event, dirties every control, empties the draw list and the copied popup, closes the
/// backgrounds, flushes the form's queued events and deletes the controls created from the database.
pub fn close(h: &mut impl LifeHost) {
    h.process_event(FormEvent::Close);
    set_all_controls_dirty(h);
    let mut i = 0;
    while i < h.control_count() {
        h.set_draw_control(i, core::ptr::null_mut());
        i += 1;
    }
    h.set_copied_popup(core::ptr::null_mut());
    let mut i = 0;
    while i < h.background_count() {
        h.background_close(i);
        i += 1;
    }
    h.manager_process_events();
    let mut i = 0;
    while i < h.db_count() {
        let t = h.db_table(i);
        if !t.is_null() && !h.db_control(i).is_null() {
            let group = h.table_group(t);
            h.remove_control(h.db_control(i), group);
        }
        h.delete_control(h.db_control(i));
        h.set_db_control(i, core::ptr::null_mut());
        i += 1;
    }
}

#[cfg(test)]
mod tests;
