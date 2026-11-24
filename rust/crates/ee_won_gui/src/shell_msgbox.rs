//! The Windows skin's message box (WONGUI's MSDialog and MSMessageBox, MSMessageBox.cpp and MSControlConstructors.cpp
//! deleted in WON round 22) as a kind over the Dialog shell (`WONShellDialog : Dialog`): MSDialog's background and
//! MSMessageBox's constructor, a centred wrapped label (the title when there is no message) and up to three same-size
//! buttons ("Ok" by default, at least 80 wide) centred below it, Escape ending with the last button's id. Showing it
//! in its own window (`MSMessageBox::Msg`: the native window, the modal run) is `ee_won_msgbox_run` over the shell
//! host's statements (GuiShell.cpp's MSMessageBoxMsg until R6 WON GUI round 36).

use std::cell::Cell;
use std::ffi::CStr;

use crate::btnobj::EeWonBtnCall as Bc;
use crate::compobj::EeWonCompCall as Cc;
use crate::ctobj::EeWonCtCall as Ct;
use crate::dialogobj::EeWonDlgCall as Dc;
use crate::msbtnobj::EeWonMsBtnCall as M;
use crate::shell::{
    gui_string_len, EeWonShellHostOp as H, EeWonShellKind as K, EeWonShellHostFn, EeWonShellOp as O, EeWonShellVirt as V, Sh,
    ShellKind,
};

const HALIGN_CENTER: isize = 1;

/// `MSMessageBox`'s members: the three buttons and the label (held).
#[derive(Default)]
pub struct MsMessageBox {
    parts: [Cell<isize>; 4],
}

/// The constructor's texts (`MSMessageBox(title, message, button1, button2, button3)`), each a `const GUIString*`,
/// and the C++ "Ok" for an empty first button: the C++ passes them as `intptr_t[6]` (this layout).
#[repr(C)]
pub struct EeWonMsgBoxTexts {
    pub title: isize,
    pub message: isize,
    pub buttons: [isize; 3],
    pub ok: isize,
}

impl MsMessageBox {
    fn hold(&self, sh: &Sh, at: usize, c: isize) -> isize {
        sh.host(H::Ref, c, 1, 0, 0);
        self.parts[at].set(c);
        c
    }
}

impl ShellKind for MsMessageBox {
    fn name(&self) -> &'static CStr {
        c"MSMessageBox"
    }
    fn overrides(&self) -> u64 {
        0
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        let me = sh.comp();
        // MSDialog(): a dialog background that fills it
        let back = sh.host(H::New, K::MsDlgBack as isize, 0, 0, 0);
        sh.host(H::Ref, back, 1, 0, 0);
        sh.layout(back, 3, me, [0; 5]);
        sh.ct_on(me, Ct::AddChild, back, 0);
        sh.host(H::Ref, back, -1, 0, 0);
        if args[0] == 0 {
            return;
        }
        // SAFETY: the C++ passes the texts' record
        let t = unsafe { &*(args[0] as *const EeWonMsgBoxTexts) };
        // SAFETY: live GUIStrings
        let len = |p: isize| unsafe { gui_string_len(p) };
        let text = if len(t.message) == 0 { t.title } else { t.message };
        let label = self.hold(sh, 3, sh.host(H::New, K::MsLabel as isize, text, 0, 0));
        sh.msb_on(label, M::SetHorzAlign, HALIGN_CENTER, 0);
        sh.msb_on(label, M::SetAdjustHeightToFitText, 1, 0);
        sh.host(H::SetWrap, label, 1, 0, 0);
        let text = if len(t.buttons[0]) == 0 { t.ok } else { t.buttons[0] };
        let b1 = self.hold(sh, 0, sh.host(H::New, K::MsButton as isize, text, 0, 0));
        let b2 = self.hold(sh, 1, sh.host(H::New, K::MsButton as isize, t.buttons[1], 0, 0));
        let b3 = self.hold(sh, 2, sh.host(H::New, K::MsButton as isize, t.buttons[2], 0, 0));
        let do2 = len(t.buttons[1]) != 0;
        let do3 = do2 && len(t.buttons[2]) != 0;
        sh.host(H::SameSize, b1, b2, b3, 0);

        let width1 = (sh.host(H::WrapWidthOf, label, 0, 0, 0) as i32).clamp(140, 400);
        let mut button_width = crate::compmem::width(b1);
        if button_width < 80 {
            button_width = 80;
            sh.comp_on(b1, Cc::SetSize, button_width as isize, crate::compmem::height(b1) as isize);
        }
        let mut width2 = button_width;
        if do2 {
            width2 += 20 + button_width;
        }
        if do3 {
            width2 += 20 + button_width;
        }
        let width1 = width1.max(width2);

        sh.dlg_on(me, Dc::SetDefaultButton, b1, 0);
        for (b, id) in [(b1, 1), (b2, 2), (b3, 3)] {
            sh.btn_on(b, Bc::SetButtonFlags, 0x100, 1);
            crate::compmem::set_control_id(b, id);
        }
        sh.dlg_on(
            me,
            Dc::SetEndOnEscape,
            if do3 {
                3
            } else if do2 {
                2
            } else {
                1
            },
            0,
        );

        sh.set_size(width1 + 20, sh.height());
        sh.comp_on(label, Cc::SetPos, 10, 10);
        sh.layout(label, 1, me, [0, 0, -20, 0, 0]);
        sh.layout(b1, 0x200, label, [0, 20, 0, 0, 0]);
        if do2 {
            sh.layout(b2, 0x4403, b1, [10, 0, 0, 0, 0]);
        }
        if do3 {
            sh.layout(b3, 0x4403, b2, [10, 0, 0, 0, 0]);
        }
        // the CenterLayout(this, button1, horizontal) with the shown buttons, added to the child layouts
        sh.host(H::CenterLayout, b1, if do2 { b2 } else { 0 }, if do3 { b3 } else { 0 }, 0);
        let height = crate::compmem::height(b1) + crate::compmem::top(b1) + 10;
        sh.set_size(sh.width(), height);
        sh.ct_on(me, Ct::AddChild, label, 0);
        sh.ct_on(me, Ct::AddChild, b1, 0);
        if do2 {
            sh.ct_on(me, Ct::AddChild, b2, 0);
        }
        if do3 {
            sh.ct_on(me, Ct::AddChild, b3, 0);
        }
    }
    fn virt(&self, _sh: &Sh, _v: V, _a: isize, _b: isize, _c: isize) -> isize {
        0
    }
    fn op(&self, _sh: &Sh, op: O, _a: isize, _b: isize) -> isize {
        match op {
            O::GetButton1 => self.parts[0].get(),
            _ => 0,
        }
    }
    /// The members released in reverse order (the label, then the third button .. the first).
    fn free(&self, sh: &Sh) {
        for at in [3, 2, 1, 0] {
            let c = self.parts[at].replace(0);
            if c != 0 {
                sh.host(H::Ref, c, -1, 0, 0);
            }
        }
    }
}


/// `MSMessageBox::Msg` (GuiShell.cpp's MSMessageBoxMsg until R6 WON GUI round 36): the box made from `texts`, a
/// native window with the parent, the title, style 0x59 and the box's size, the box added, its first button
/// focused, the modal run, the window closed; the box's and the window's references (the original's two SmartPtrs)
/// given back, the window's first. Returns the run's result.
///
/// # Safety
/// `texts` is the box's texts (`EeWonMsgBoxTexts`, live through the call); `host` the C++ shell host.
#[no_mangle]
pub unsafe extern "C" fn ee_won_msgbox_run(host: EeWonShellHostFn, parent: isize, texts: *const EeWonMsgBoxTexts) -> i32 {
    let sh = Sh::detached(host);
    let msg_box = sh.host(H::MsgBoxNew, texts as isize, 0, 0, 0);
    sh.host(H::Ref, msg_box, 1, 0, 0);
    let win = sh.host(H::MsgWinNew, 0, 0, 0, 0);
    sh.host(H::RefObj, win, 1, 0, 0);
    // SAFETY: the contract (the texts outlive the call)
    let title = unsafe { (*texts).title };
    sh.host(H::MsgWinParent, win, parent, 0, 0);
    sh.host(H::MsgWinTitle, win, title, 0, 0);
    sh.host(H::MsgWinStyle, win, 0x59, 0, 0);
    let size = crate::compmem::get(msg_box).bounds;
    sh.host(H::MsgWinCreate, win, size.width as isize, size.height as isize, 0);
    sh.host(H::MsgWinAdd, win, msg_box, 0, 0);
    let button = sh.host(H::MsgShellOp, msg_box, O::GetButton1 as isize, 0, 0);
    sh.host(H::MsgFocus, button, 0, 0, 0);
    let result = sh.host(H::MsgRunDialog, win, msg_box, 0, 0) as i32;
    sh.host(H::MsgWinClose, win, 0, 0, 0);
    sh.host(H::RefObj, win, 0, 0, 0);
    sh.host(H::Ref, msg_box, 0, 0, 0);
    result
}

#[cfg(test)]
#[path = "shell_msgbox_tests.rs"]
mod tests;
