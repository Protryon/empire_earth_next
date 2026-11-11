//! `UIEditBox`'s key dispatch (`User Interface/UIEditBox.cpp`): `UpdateKeyboard`'s repeat (a 500 ms delay, then
//! 50 ms steps), `ProcessInput` and its Shift and Ctrl halves (which keys move, select, delete, cut, copy, paste,
//! clear a line, scroll or fire Enter), the clipboard's `Cut`/`Copy`/`Paste`, `AddIMEResultString`'s typing of an
//! IME result, and `MouseMove`'s drag selection with its auto-scroll. The text model they call is [`crate::edit`]'s.
//!
//! Every key is a host question ([`EditKey`]): the C++ keeps `UIControl`'s key predicates (`DeleteKeyPressed` reads
//! the keypad's Del when Num Lock is off, `CutKeyPressed` is Ctrl+X or Shift+Del, ...) and the input system's
//! entities, and each is asked where and as often as the C++ asked it. The four timers stay `TMPoll`s
//! ([`EditPoll`]). `ProcessEnterKey` is virtual (the scenario editor's boxes override it) and the IME result's
//! characters go through `UIManager::UpdateTranslatedKey` back into the focused control: both are host calls that
//! may come back into the box, so the members go out before them and come back after ([`Edit::call`]).
//!
//! Game text is UTF-8 (decision D6): the caret moves and deletes by the model's whole characters, and an IME result
//! is typed a whole character at a time.

use core::ffi::{c_long, c_ulong};

use crate::edit::{Edit, EditHost, ERROR_OK, TRIGGER_ENTER_PRESSED};
use crate::rect::UiPoint;
use crate::text::{empty, Mbcs};

/// What the dispatch asks of the keyboard, as `UIEditBox.cpp` asked it.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EditKey {
    /// `UIControl::ShiftKeyPressed()` (Shift just pressed or held).
    Shift = 0,
    /// `UIControl::sControlKey->GetValue() != 0`.
    Control = 1,
    /// `UIControl::sBackSpaceKey->GetValue() != 0`.
    BackSpace = 2,
    /// `UIControl::DeleteKeyPressed()`.
    Delete = 3,
    /// `UIControl::RightKeyPressed()`.
    Right = 4,
    /// `UIControl::LeftKeyPressed()`.
    Left = 5,
    /// `UIControl::HomeKeyPressed()`.
    Home = 6,
    /// `UIControl::EndKeyPressed()`.
    End = 7,
    /// `UIControl::UpKeyPressed()`.
    Up = 8,
    /// `UIControl::DownKeyPressed()`.
    Down = 9,
    /// `UIControl::EnterKeyPressed()` (Enter or the keypad's Enter just released).
    EnterReleased = 10,
    /// `UIControl::CutKeyPressed()`.
    Cut = 11,
    /// `UIControl::CopyKeyPressed()`.
    Copy = 12,
    /// `UIControl::PasteKeyPressed()`.
    Paste = 13,
    /// The keyboard's `kKeyA` entity `JustPressed()` (Ctrl+A).
    SelectAll = 14,
    /// The keyboard's `kKeyL` entity `JustPressed()` (Ctrl+L).
    ClearLine = 15,
    /// The keyboard's `kKeyReturn` entity `JustPressed()` (Ctrl+Enter).
    Return = 16,
    /// `UIControl::sEnterKey->JustPressed()`.
    EnterPressed = 17,
    /// `UIControl::sNumEnterKey->JustPressed()`.
    NumEnterPressed = 18,
}

/// The box's timers: `mRepeatPoll` (50 ms), `mRepeatDelayPoll` (500 ms), `mCursorPoll` (the caret's blink),
/// `mScrollPoll` (a drag's auto-scroll, 25 ms).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EditPoll {
    Repeat = 0,
    RepeatDelay = 1,
    Cursor = 2,
    Scroll = 3,
}

/// What is done to a timer: `GetIsRunning`, `Poll` (the steps elapsed), `Start`, `Stop` (the answer is the first
/// two's).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PollOp {
    Running = 0,
    Poll = 1,
    Start = 2,
    Stop = 3,
}

/// The C++ side of the key dispatch, beside [`EditHost`]. Texts cross NUL-terminated.
pub trait EditKeysHost {
    /// A key question.
    fn key(&mut self, k: EditKey) -> bool;
    /// A timer operation.
    fn poll(&mut self, p: EditPoll, op: PollOp) -> c_ulong;
    /// The virtual `ProcessEnterKey` (may come back into the box).
    fn process_enter_key(&mut self);
    /// `PlatClipboardSetText`.
    fn clipboard_set(&mut self, text: &[u8]);
    /// `PlatClipboardGetText` (`None`: nothing to paste).
    fn clipboard_get(&mut self) -> Option<Vec<u8>>;
    /// `mForm->GetManager()->UpdateTranslatedKey(text, 1)`: the first character of `text` typed into the focused
    /// control (this box; comes back into it).
    fn translated_key(&mut self, text: &[u8]);
}

impl<'a, M: Mbcs, H: EditHost + EditKeysHost> Edit<'a, M, H> {
    fn key(&mut self, k: EditKey) -> bool {
        self.h.key(k)
    }

    fn running(&mut self, p: EditPoll) -> bool {
        self.h.poll(p, PollOp::Running) != 0
    }

    fn fired(&mut self, p: EditPoll) -> c_ulong {
        self.h.poll(p, PollOp::Poll)
    }

    fn start(&mut self, p: EditPoll) {
        self.h.poll(p, PollOp::Start);
    }

    fn stop(&mut self, p: EditPoll) {
        self.h.poll(p, PollOp::Stop);
    }

    fn process_enter_key(&mut self) {
        self.call(|h| h.process_enter_key());
    }

    /// After a deletion an anchor that is no longer a place in the text (its line gone, or its index past its
    /// line's end) becomes the caret. A fix, the owner's decision: the original kept it, so a drag's next
    /// `MouseMove`, or a Shift move in the same frame, selected from past the text and read past the lines
    /// (PORT_LEARNINGS "R6: ... round 8"). An anchor still inside the text stays where it was, as in the original.
    pub(crate) fn keep_anchor_in_text(&mut self) {
        let (line, index) = (self.s.f.selection_start_line, self.s.f.selection_start_index);
        if line as usize >= self.h.line_count() || index < 0 || index as c_ulong > self.len(line) {
            self.s.f.selection_start_index = self.s.f.caret_index;
            self.s.f.selection_start_line = self.s.f.caret_line;
        }
    }

    /// `UpdateKeyboard`: Enter in a single-line box fires at once; otherwise a held key acts on its first frame,
    /// again after the 500 ms delay, then every 50 ms step, and shows the caret while it is held.
    pub fn update_keyboard(&mut self) -> c_long {
        let mut repeat = false;
        if !self.s.f.ime_active
            && self.s.f.is_single_line
            && (self.key(EditKey::EnterPressed) || self.key(EditKey::NumEnterPressed))
        {
            self.process_enter_key();
            self.s.f.key_repeating = false;
            self.stop(EditPoll::RepeatDelay);
            self.stop(EditPoll::Repeat);
            self.start(EditPoll::Cursor);
        } else {
            if self.running(EditPoll::RepeatDelay) {
                if self.fired(EditPoll::RepeatDelay) != 0 {
                    self.stop(EditPoll::RepeatDelay);
                    self.start(EditPoll::Repeat);
                    repeat = true;
                }
            } else if !self.running(EditPoll::Repeat) || self.fired(EditPoll::Repeat) != 0 {
                repeat = true;
            }
            if self.process_input(repeat) {
                if !self.s.f.key_repeating {
                    self.start(EditPoll::RepeatDelay);
                }
                self.s.f.key_repeating = true;
                if !self.s.f.cursor_on {
                    self.set_dirty();
                }
                self.stop(EditPoll::Cursor);
                self.s.f.cursor_on = true;
            } else {
                self.s.f.key_repeating = false;
                self.stop(EditPoll::RepeatDelay);
                self.stop(EditPoll::Repeat);
                self.start(EditPoll::Cursor);
            }
        }
        ERROR_OK
    }

    /// `ProcessInput`: the edit keys, or the Shift or Ctrl combinations (none while an IME composes); `repeat` is
    /// whether a held key acts this frame. Returns whether a key was handled.
    pub fn process_input(&mut self, repeat: bool) -> bool {
        let mut handled = false;
        if self.key(EditKey::Shift) {
            if !self.s.f.ime_active {
                handled = self.process_shift_input(repeat);
            }
        } else if self.key(EditKey::Control) {
            if !self.s.f.ime_active {
                handled = self.process_control_input(repeat);
            }
        } else {
            if self.key(EditKey::BackSpace) {
                handled = true;
                if !self.s.f.ime_active && repeat {
                    if self.s.f.is_text_selected {
                        self.delete_selected_text();
                    } else {
                        self.back_space_char();
                    }
                    self.keep_anchor_in_text();
                }
            }
            if self.key(EditKey::Delete) {
                handled = true;
                if !self.s.f.ime_active && repeat {
                    if self.s.f.is_text_selected {
                        self.delete_selected_text();
                    } else {
                        self.delete_char();
                    }
                    self.keep_anchor_in_text();
                }
            }
            if self.key(EditKey::Right) {
                handled = true;
                if !self.s.f.ime_active {
                    self.s.f.is_text_selected = false;
                    if repeat {
                        self.increment_caret_pos(1);
                    }
                }
            }
            if self.key(EditKey::Left) {
                handled = true;
                if !self.s.f.ime_active {
                    self.s.f.is_text_selected = false;
                    if repeat {
                        self.decrement_caret_pos();
                    }
                }
            }
            if self.key(EditKey::Home) {
                handled = true;
                if !self.s.f.ime_active {
                    self.s.f.is_text_selected = false;
                    self.home_key();
                }
            }
            if self.key(EditKey::End) {
                handled = true;
                if !self.s.f.ime_active {
                    self.s.f.is_text_selected = false;
                    self.end_key();
                }
            }
            if !self.s.f.is_single_line {
                if self.key(EditKey::Up) {
                    handled = true;
                    if !self.s.f.ime_active {
                        self.s.f.is_text_selected = false;
                        if repeat {
                            self.up_key();
                        }
                    }
                } else if self.key(EditKey::Down) {
                    handled = true;
                    if !self.s.f.ime_active {
                        self.s.f.is_text_selected = false;
                        if repeat {
                            self.down_key();
                        }
                    }
                }
                if self.key(EditKey::EnterReleased) {
                    handled = true;
                    if !self.s.f.ime_active && repeat {
                        self.process_enter_key();
                    }
                }
            }
        }
        handled
    }

    /// `ProcessShiftInput`: the clipboard keys (alone), or deletion and the moves that extend the selection from
    /// its anchor (set at the caret when nothing is selected).
    pub fn process_shift_input(&mut self, repeat: bool) -> bool {
        let mut handled = false;
        if !self.s.f.is_text_selected {
            self.s.f.selection_start_index = self.s.f.caret_index;
            self.s.f.selection_start_line = self.s.f.caret_line;
        }
        if self.key(EditKey::Cut) {
            handled = true;
            if repeat {
                self.cut();
            }
        }
        if self.key(EditKey::Copy) {
            handled = true;
            if repeat {
                self.copy();
            }
        }
        if self.key(EditKey::Paste) {
            handled = true;
            if repeat {
                self.paste();
            }
        }
        if handled {
            return handled;
        }
        if self.key(EditKey::BackSpace) {
            handled = true;
            if repeat {
                if self.s.f.is_text_selected {
                    self.delete_selected_text();
                } else {
                    self.back_space_char();
                }
                self.keep_anchor_in_text();
            }
        }
        if self.key(EditKey::Delete) {
            // kept: Shift+Del without a selection deletes backwards (the keypad's Del with Num Lock off; the main
            // Del is Cut above)
            handled = true;
            if repeat {
                if self.s.f.is_text_selected {
                    self.delete_selected_text();
                } else {
                    self.back_space_char();
                }
                self.keep_anchor_in_text();
            }
        }
        if self.key(EditKey::Right) {
            handled = true;
            if repeat {
                self.increment_caret_pos(self.s.f.char_scroll as c_long);
            }
            self.selection_changed();
        }
        if self.key(EditKey::Left) {
            handled = true;
            if repeat {
                self.decrement_caret_pos();
            }
            self.selection_changed();
        }
        if self.key(EditKey::Home) {
            handled = true;
            self.home_key();
            self.selection_changed();
        }
        if self.key(EditKey::End) {
            handled = true;
            self.end_key();
            self.selection_changed();
        }
        if !self.s.f.is_single_line {
            if self.key(EditKey::Down) {
                handled = true;
                if repeat && (self.s.f.caret_line as usize) < self.h.line_count().wrapping_sub(1) {
                    self.s.f.caret_line += 1;
                    self.index_from_up_down_offset();
                    self.compute_caret_location(false);
                }
                self.selection_changed();
            }
            if self.key(EditKey::Up) {
                handled = true;
                if repeat && self.s.f.caret_line > 0 {
                    self.s.f.caret_line -= 1;
                    self.index_from_up_down_offset();
                    self.compute_caret_location(false);
                }
                self.selection_changed();
            }
        }
        handled
    }

    /// `ProcessControlInput`: cut, copy, paste, Home and End of the whole text, Ctrl+A (select all), Ctrl+L (clear
    /// the caret's line), and in a multi-line box Up and Down scroll and Ctrl+Enter fires Enter. Ctrl+A, Ctrl+L and
    /// Ctrl+Enter do not count as handled (kept).
    pub fn process_control_input(&mut self, repeat: bool) -> bool {
        let mut handled = false;
        if self.key(EditKey::Cut) {
            handled = true;
            if repeat {
                self.cut();
            }
        }
        if self.key(EditKey::Copy) {
            handled = true;
            if repeat {
                self.copy();
            }
        }
        if self.key(EditKey::Paste) {
            handled = true;
            if repeat {
                self.paste();
            }
        }
        if self.key(EditKey::Home) {
            self.s.f.is_text_selected = false;
            handled = true;
            self.s.f.caret_line = 0;
            self.home_key();
        }
        if self.key(EditKey::End) {
            self.s.f.is_text_selected = false;
            handled = true;
            self.s.f.caret_line = self.h.line_count().wrapping_sub(1) as c_ulong;
            self.end_key();
        }
        if self.key(EditKey::SelectAll) {
            self.select_all();
        }
        if self.key(EditKey::ClearLine) {
            self.s.f.is_text_selected = false;
            self.s.f.caret_index = 0;
            self.s.f.drawing_index = 0;
            if self.s.f.caret_line as usize == self.h.line_count().wrapping_sub(1) {
                self.h.set_line(self.s.f.caret_line as usize, &empty());
                self.compute_caret_location(true);
            } else {
                self.delete_line(self.s.f.caret_line);
            }
            self.keep_anchor_in_text();
        }
        if self.s.f.is_single_line {
            return handled;
        }
        if self.key(EditKey::Down) {
            self.s.f.is_text_selected = false;
            handled = true;
            if repeat && self.h.scroll_value() < self.h.scroll_max() {
                let v = self.h.scroll_value().wrapping_add(1);
                self.set_scroll_value(v as c_ulong);
            }
        }
        if self.key(EditKey::Up) {
            self.s.f.is_text_selected = false;
            handled = true;
            if repeat && self.h.scroll_value() > 0 {
                let v = self.h.scroll_value().wrapping_sub(1);
                self.set_scroll_value(v as c_ulong);
            }
        }
        if self.key(EditKey::Return) {
            self.call(|h| h.fire(TRIGGER_ENTER_PRESSED));
        }
        handled
    }

    /// `Copy`: the selected text to the clipboard.
    pub fn copy(&mut self) {
        if !self.s.f.is_text_selected {
            return;
        }
        let text = self.selected_text().unwrap_or_else(empty);
        self.h.clipboard_set(&text);
    }

    /// `Cut`: copies the selection, then deletes it.
    pub fn cut(&mut self) {
        if self.s.f.is_text_selected {
            self.copy();
            self.delete_selected_text();
            self.keep_anchor_in_text();
        }
    }

    /// `Paste`: the clipboard's text typed at the caret (`AddChar`).
    pub fn paste(&mut self) {
        if let Some(text) = self.h.clipboard_get() {
            // typing over a selection deletes it first
            let deletes = self.s.f.is_text_selected;
            self.add_char(&text);
            if deletes {
                self.keep_anchor_in_text();
            }
        }
    }

    /// `AddIMEResultString`: removes the composition, types the result a character at a time through the manager
    /// (as typed keys), then restarts the composition at the caret.
    pub fn add_ime_result(&mut self, text: &[u8]) {
        let active = self.s.f.ime_active;
        self.ime_remove();
        self.s.f.ime_active = false;
        let mut at = 0;
        while text[at] != 0 {
            let rest = text[at..].to_vec();
            self.call(|h| h.translated_key(&rest));
            at = self.m.inc(text, at);
        }
        self.s.f.ime_start_index = self.s.f.caret_index as c_ulong;
        self.s.f.ime_start_line = self.s.f.caret_line;
        self.s.f.ime_active = active;
    }

    /// `MouseMove` while selecting: the caret follows the mouse and the selection runs from its anchor; outside the
    /// text the box auto-scrolls by the scroll timer's steps (the first move one), sideways in a single-line box,
    /// a line at a time up or down in a multi-line one.
    pub fn drag_to(&mut self, at: UiPoint) {
        let (line, index) = self.caret_from_mouse(at);
        self.s.f.caret_line = line;
        self.s.f.caret_index = index;
        self.selection_changed();
        if self.s.text.contains(at) {
            return;
        }
        let count = if self.running(EditPoll::Scroll) {
            self.fired(EditPoll::Scroll)
        } else {
            self.start(EditPoll::Scroll);
            1
        };
        if self.s.f.is_single_line {
            if count > 0 {
                if at.x < self.s.text.left {
                    for _ in 0..count {
                        self.decrement_caret_pos();
                    }
                } else if at.x > self.s.text.right {
                    self.increment_caret_pos(count as c_long);
                } else {
                    self.stop(EditPoll::Scroll);
                }
            }
        } else if count > 0 {
            let lines = self.h.line_count();
            if at.y < self.s.text.top && self.s.f.caret_line > 0 {
                if (self.s.f.caret_line.wrapping_sub(count) as c_long) < 0 {
                    self.s.f.caret_line = 0;
                } else {
                    self.s.f.caret_line -= count;
                }
                self.index_from_up_down_offset();
                self.compute_caret_location(false);
            } else if at.y > self.s.text.bottom && (self.s.f.caret_line as usize) < lines.wrapping_sub(1) {
                if self.s.f.caret_line.wrapping_add(count) as usize >= lines {
                    self.s.f.caret_line = lines.wrapping_sub(1) as c_ulong;
                } else {
                    self.s.f.caret_line += count;
                }
                self.index_from_up_down_offset();
                self.compute_caret_location(false);
            } else {
                self.stop(EditPoll::Scroll);
            }
        }
    }
}

#[cfg(test)]
mod tests;
