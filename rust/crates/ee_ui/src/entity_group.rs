//! `UIEntityGroup::Disable` and `Enable` (`User Interface/UIEntityGroup.cpp`): a group of buttons and hot keys
//! switched off or on. The entities and their form stay C++ objects; every question and call is a host call, in the
//! C++'s order (the entities from the last to the first).

/// An entity of the group as `Disable`/`Enable` see it, by its index in the group.
pub trait GroupHost {
    /// `GetEntityCount()`.
    fn count(&self) -> usize;
    /// `mEntities[i]->GetType() == kControlTypeButton` (anything else is a hot key).
    fn is_button(&mut self, i: usize) -> bool;
    /// The button's `GetDisplayMode() == kDisplayModeShown`, and its form being its manager's current form.
    fn shown(&mut self, i: usize) -> bool;
    fn on_current_form(&mut self, i: usize) -> bool;
    /// The manager's `AddDirtyRectangle(its rectangle, its form)`, its `HideControl()`, its `SetDirty(true)`.
    fn add_dirty(&mut self, i: usize);
    fn hide(&mut self, i: usize);
    fn set_dirty(&mut self, i: usize);
    /// Its form's `GetTabbingOn()`, the form's `GetTabStop() == its GetIndex()`, the form's `Tab()`.
    fn tabbing(&mut self, i: usize) -> bool;
    fn is_tab_stop(&mut self, i: usize) -> bool;
    fn tab(&mut self, i: usize);
    /// The hot key's `SetEnabled(on)`.
    fn set_enabled(&mut self, i: usize, on: bool);
}

/// `Disable` (after `mDisableCount = 1`): a shown button on the current form is redrawn away and hidden; a button
/// holding the tab stop passes it on (shown or not); a hot key is disabled.
pub fn disable(h: &mut impl GroupHost) {
    for i in (0..h.count()).rev() {
        if h.is_button(i) {
            if h.shown(i) && h.on_current_form(i) {
                h.add_dirty(i);
                h.hide(i);
                h.set_dirty(i);
            }
            if h.tabbing(i) && h.is_tab_stop(i) {
                h.tab(i);
            }
        } else {
            h.set_enabled(i, false);
        }
    }
}

/// `Enable` (after `mDisableCount = 0`): buttons are redrawn, hot keys enabled.
pub fn enable(h: &mut impl GroupHost) {
    for i in (0..h.count()).rev() {
        if h.is_button(i) {
            h.set_dirty(i);
        } else {
            h.set_enabled(i, true);
        }
    }
}

#[cfg(test)]
mod tests;
