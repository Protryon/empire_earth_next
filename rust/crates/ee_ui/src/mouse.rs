//! The mouse's way into a form and its controls (`User Interface/UIControl.cpp`, `UIForm.cpp`):
//! `UIControl::UpdateMouse` (which of a control's press, release and double-click handlers a frame calls, and the
//! button-down members they keep) and `UIForm::UpdateMouse` (the captured control or the one under the mouse gets
//! the mouse, the wheel goes to it, then, unless a control holds the mouse or failed, the form's own mouse events).
//!
//! The controls, the input system's entities and the events stay C++ objects: every read and call is a host call,
//! made where and as often as the C++ made it (short-circuits included), and the captured control is read again
//! after a control's handlers, which may capture or release the mouse.

use core::ffi::c_long;

/// `kErrorOK`.
pub const ERROR_OK: c_long = 0;

/// A question to a mouse button's entity (`ISEntity`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ButtonQuery {
    /// `JustDoublePressed()`.
    JustDoublePressed = 0,
    /// `JustUnpressed()`.
    JustUnpressed = 1,
    /// `GetValue() != 0`.
    Value = 2,
    /// `JustPressed()`.
    JustPressed = 3,
}

/// A control's mouse handler (`UIControl`'s virtuals).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ControlMouseCall {
    /// `MouseLButtonDoublePressed()`.
    LeftDouble = 0,
    /// `MouseLButtonUp()`.
    LeftUp = 1,
    /// `MouseLButtonDown()`.
    LeftDown = 2,
    /// `MouseRButtonUp()`.
    RightUp = 3,
    /// `MouseRButtonDown()`.
    RightDown = 4,
    /// `MouseMove(inLocation)`.
    Move = 5,
}

/// A control as `UIControl::UpdateMouse` sees it.
pub trait ControlMouseHost {
    /// `UIManager::GetLMouseButton()` (`right` false) or `GetRMouseButton()`, asked `q`.
    fn button(&mut self, right: bool, q: ButtonQuery) -> bool;
    /// `mLeftButtonDown` / `mRightButtonDown`.
    fn down(&self, right: bool) -> bool;
    fn set_down(&mut self, right: bool, down: bool);
    /// The virtual handler -> its result.
    fn call(&mut self, c: ControlMouseCall) -> c_long;
}

/// `UIControl::UpdateMouse`: a double click, else the left button's release (the control thought it down and the
/// button is released or up) or press; then the right button's release or press (no double click); then the move,
/// whose result is the result.
pub fn control_update_mouse(h: &mut impl ControlMouseHost) -> c_long {
    if h.button(false, ButtonQuery::JustDoublePressed) {
        h.call(ControlMouseCall::LeftDouble);
    } else if h.down(false) && (h.button(false, ButtonQuery::JustUnpressed) || !h.button(false, ButtonQuery::Value)) {
        h.set_down(false, false);
        h.call(ControlMouseCall::LeftUp);
    } else if h.button(false, ButtonQuery::JustPressed) {
        h.set_down(false, true);
        h.call(ControlMouseCall::LeftDown);
    }
    if h.down(true) && (h.button(true, ButtonQuery::JustUnpressed) || !h.button(true, ButtonQuery::Value)) {
        h.set_down(true, false);
        h.call(ControlMouseCall::RightUp);
    } else if h.button(true, ButtonQuery::JustPressed) {
        h.set_down(true, true);
        h.call(ControlMouseCall::RightDown);
    }
    h.call(ControlMouseCall::Move)
}

/// The mouse entities `UIForm::UpdateMouse` takes from `gISMouse->GetEntity`, in its order.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MouseEntity {
    XAxis = 0,
    YAxis = 1,
    Wheel = 2,
    Left = 3,
    Right = 4,
    Middle = 5,
}

/// The form's own mouse events, `UIForm`'s members.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FormMouseEvent {
    Move = 0,
    WheelMove = 1,
    LeftPressed = 2,
    LeftUnpressed = 3,
    LeftDoublePressed = 4,
    MiddlePressed = 5,
    MiddleUnpressed = 6,
    MiddleDoublePressed = 7,
    RightPressed = 8,
    RightUnpressed = 9,
    RightDoublePressed = 10,
}

/// A question to one of the form's mouse entities.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EntityQuery {
    /// `GetValue() != 0`.
    Value = 0,
    JustPressed = 1,
    JustUnpressed = 2,
    JustDoublePressed = 3,
}

/// A control, as the host names it (never null).
pub type Control = usize;

/// The form as `UIForm::UpdateMouse` sees it.
pub trait FormMouseHost {
    /// `mMouseCaptured` (read each time: a control's handlers may capture or release the mouse).
    fn captured(&self) -> Option<Control>;
    /// `mMouseOverControl`.
    fn mouse_over(&self) -> Option<Control>;
    /// `c->UpdateMouse(mMouseLocation)`.
    fn control_update(&mut self, c: Control) -> c_long;
    /// `UIManager::GetMouseWheel() != NULL`, then `UIManager::GetMouseWheelScrollAmount()`.
    fn wheel(&self) -> bool;
    fn wheel_amount(&mut self) -> c_long;
    /// `c->MouseWheelTurned(amount)`.
    fn wheel_turned(&mut self, c: Control, amount: c_long) -> c_long;
    /// `gISMouse->GetEntity(e)`: whether it is not NULL (the C++ checks only the wheel and the middle button).
    fn entity(&mut self, e: MouseEntity) -> bool;
    /// That entity asked `q` (it was not NULL, or the C++ reads through NULL as the original did).
    fn ask(&mut self, e: MouseEntity, q: EntityQuery) -> bool;
    /// Whether the event member is set; its `ProcessEvent(this, NULL)` -> its result.
    fn has_event(&self, ev: FormMouseEvent) -> bool;
    fn process(&mut self, ev: FormMouseEvent) -> c_long;
}

/// `UIForm::UpdateMouse`: the captured control, else the control under the mouse (and then the wheel, when there
/// is one and it turned: a control that does not take it is forgotten, the result OK) gets the mouse. When no
/// control holds the mouse after that and the result is OK, the form's events fire (move, the wheel unless a control
/// took it, the left, middle and right buttons' presses, releases and double clicks); the last one's result is the
/// result.
pub fn form_update_mouse(h: &mut impl FormMouseHost) -> c_long {
    let mut result = ERROR_OK;
    let mut control = None;
    if let Some(c) = h.captured() {
        result = h.control_update(c);
    } else {
        control = h.mouse_over();
        if let Some(c) = control {
            result = h.control_update(c);
            if result == ERROR_OK && h.wheel() {
                let amount = h.wheel_amount();
                if amount != 0 {
                    result = h.wheel_turned(c, amount);
                    if result != ERROR_OK {
                        result = ERROR_OK;
                        control = None;
                    }
                }
            }
        }
    }
    if h.captured().is_some() || result != ERROR_OK {
        return result;
    }
    h.entity(MouseEntity::XAxis);
    h.entity(MouseEntity::YAxis);
    let wheel = h.entity(MouseEntity::Wheel);
    h.entity(MouseEntity::Left);
    h.entity(MouseEntity::Right);
    let middle = h.entity(MouseEntity::Middle);
    use EntityQuery as Q;
    use FormMouseEvent as E;
    use MouseEntity as M;
    if h.has_event(E::Move) && (h.ask(M::XAxis, Q::Value) || h.ask(M::YAxis, Q::Value)) {
        result = h.process(E::Move);
    }
    if h.has_event(E::WheelMove) && control.is_none() && wheel && h.ask(M::Wheel, Q::Value) {
        result = h.process(E::WheelMove);
    }
    let buttons: [(M, [E; 3]); 3] = [
        (M::Left, [E::LeftPressed, E::LeftUnpressed, E::LeftDoublePressed]),
        (M::Middle, [E::MiddlePressed, E::MiddleUnpressed, E::MiddleDoublePressed]),
        (M::Right, [E::RightPressed, E::RightUnpressed, E::RightDoublePressed]),
    ];
    for (m, events) in buttons {
        if m == M::Middle && !middle {
            continue;
        }
        for (ev, q) in events.into_iter().zip([Q::JustPressed, Q::JustUnpressed, Q::JustDoublePressed]) {
            if h.has_event(ev) && h.ask(m, q) {
                result = h.process(ev);
            }
        }
    }
    result
}

#[cfg(test)]
mod tests;
