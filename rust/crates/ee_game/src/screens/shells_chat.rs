//! One line of the in-game chat display as a kind over the control shell (UI lane round 6; once `EEUICChatLine`):
//! its logic is `chat_line.rs`' over its record ([`EeScreensChatLineFields`], owned here, not a C++ base any more)
//! and the chat line's host (`hud_ctl_ffi.rs` `ChatCpp`: the timer, the texts, then `EEUIRustControl`'s host). The
//! timer (a `TMPoll` is its [`EeTimePoll`]) is owned here too; the four `GEText`s are C++ objects the control
//! factory makes (`EEUIShellNewChatLine`, the shell's slots) after the kind's constructor, and the host deletes them.

use core::cell::UnsafeCell;
use core::ffi::CStr;

use ee_base::time::EeTimePoll;

use crate::screens::chat_line::EeScreensChatLineFields;
use crate::screens::shells::*;

/// `EEUICChatLine` (a control; args: the table's background opacity, its `f32` bits).
pub const EE_SCREENS_SK_CHAT_LINE: u32 = 0x83;

/// The texts made: `a` the shell's slots (`void*[4]`: the bold name, its drawn copy, the message, its drawn copy).
pub const EE_SCREENS_SKOP_CHAT_PARTS: u32 = 0x84;
/// -> the line's record (an `EeScreensChatLineFields*`).
pub const EE_SCREENS_SKOP_CHAT_RECORD: u32 = 0x85;

/// The line's four texts (its slots) deleted.
pub const EE_SCREENS_SH_CHAT_FREE: u32 = 0xc3;

/// The line's calls into `hud_ctl_ffi.rs` (the chat line's members on its record and control) and the timer's
/// constructor.
pub(crate) trait ChatFfi {
    /// `which`: [`EE_SCREENS_SV_COPY_DATA`], [`EE_SCREENS_SV_DRAW_POLYGONS`] or [`EE_SCREENS_SV_DRAW_TEXT`].
    fn member(&self, which: u32, rec: *mut EeScreensChatLineFields, ctl: Cpp) -> i64;
    /// `new TMPoll(1, 0, false)`, then `Stop()`.
    fn timer(&self, poll: *mut EeTimePoll);
}

#[cfg(not(test))]
pub(crate) struct Ffi;

#[cfg(not(test))]
impl ChatFfi for Ffi {
    fn member(&self, which: u32, rec: *mut EeScreensChatLineFields, ctl: Cpp) -> i64 {
        use crate::screens::hud_ctl_ffi::*;
        // SAFETY: the line's live record and control
        unsafe {
            match which {
                EE_SCREENS_SV_COPY_DATA => ee_screens_chat_line_copy_data(rec, ctl),
                EE_SCREENS_SV_DRAW_POLYGONS => ee_screens_chat_line_draw(rec, ctl),
                _ => ee_screens_chat_line_draw_text(rec, ctl),
            }
        }
    }
    fn timer(&self, poll: *mut EeTimePoll) {
        // SAFETY: the kind's own poll
        unsafe {
            ee_base::time::ee_time_poll_init(poll, 1, 0, false);
            ee_base::time::ee_time_poll_stop(poll);
        }
    }
}

pub(crate) struct ChatLineKind<F: ChatFfi> {
    rec: Box<UnsafeCell<EeScreensChatLineFields>>,
    timer: Box<UnsafeCell<EeTimePoll>>,
    opacity: f32,
    ffi: F,
}

impl<F: ChatFfi> ChatLineKind<F> {
    pub(crate) fn new(ffi: F, opacity: f32) -> Self {
        ChatLineKind {
            rec: Box::new(UnsafeCell::new(EeScreensChatLineFields::default())),
            timer: Box::new(UnsafeCell::new(EeTimePoll::default())),
            opacity,
            ffi,
        }
    }
}

impl<F: ChatFfi> Kind for ChatLineKind<F> {
    fn name(&self) -> &'static CStr {
        c"EEUICChatLine"
    }
    fn overrides(&self) -> u64 {
        (1 << EE_SCREENS_SV_COPY_DATA) | (1 << EE_SCREENS_SV_DRAW_POLYGONS) | (1 << EE_SCREENS_SV_DRAW_TEXT)
    }
    /// The constructor's record part (`ee_screens_chat_line_init`): the opacity, no shadow, showing; the timer made.
    fn init(&self, _c: &Ctx) {
        // SAFETY: the kind's own record
        crate::screens::chat_line::init(unsafe { &crate::screens::hud_ctl::Rec::new(self.rec.get()) }, self.opacity);
        self.ffi.timer(self.timer.get());
        // SAFETY: as above
        unsafe { (*self.rec.get()).mTimer = self.timer.get().cast() };
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_COPY_DATA | EE_SCREENS_SV_DRAW_POLYGONS | EE_SCREENS_SV_DRAW_TEXT => {
                self.ffi.member(v, self.rec.get(), c.ctl)
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, _c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_CHAT_PARTS if a != 0 => {
                // SAFETY: the factory's four slots; the kind's own record
                unsafe {
                    let s = core::slice::from_raw_parts(a as *const *mut core::ffi::c_void, 4);
                    let r = &mut *self.rec.get();
                    r.mName = s[0];
                    r.mDrawnName = s[1];
                    r.mMessage = s[2];
                    r.mDrawnMessage = s[3];
                }
                0
            }
            EE_SCREENS_SKOP_CHAT_RECORD => self.rec.get() as i64,
            _ => 0,
        }
    }
    /// The destructor: the texts deleted (the timer goes with the kind).
    fn free(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_CHAT_FREE, 0, 0);
    }
}

pub(crate) fn make(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    match kind {
        #[cfg(not(test))]
        EE_SCREENS_SK_CHAT_LINE => {
            let opacity = f32::from_bits(args.first().copied().unwrap_or(0) as u32);
            Some(Box::new(ChatLineKind::new(Ffi, opacity)))
        }
        _ => crate::screens::shells_files::make(kind, args),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::RefCell;

    #[derive(Default)]
    struct Fake {
        members: RefCell<Vec<(u32, usize)>>,
        timers: RefCell<u32>,
    }

    impl ChatFfi for &Fake {
        fn member(&self, which: u32, rec: *mut EeScreensChatLineFields, _ctl: Cpp) -> i64 {
            self.members.borrow_mut().push((which, rec as usize));
            7
        }
        fn timer(&self, poll: *mut EeTimePoll) {
            *self.timers.borrow_mut() += 1;
            // SAFETY: the kind's poll
            unsafe { (*poll).time_delay = 1 };
        }
    }

    struct Host(RefCell<Vec<(u32, i64, i64)>>);

    impl ShellHost for Host {
        fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, _f: &mut [f32; 4]) -> i64 {
            self.0.borrow_mut().push((op, a, b));
            11
        }
    }

    #[test]
    fn the_constructor_fills_the_record_and_makes_the_timer() {
        let fake = Fake::default();
        let k = ChatLineKind::new(&fake, 0.75);
        assert_eq!(k.name().to_str().unwrap(), "EEUICChatLine");
        let h = Host(RefCell::new(Vec::new()));
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        k.init(&c);
        let rec = k.op(&c, EE_SCREENS_SKOP_CHAT_RECORD, 0, 0, &mut [0.0; 4]) as *const EeScreensChatLineFields;
        // SAFETY: the kind's live record
        let r = unsafe { &*rec };
        assert_eq!((r.mOpacity, r.mShadowOffset, r.mShowing), (0.75, 0, true));
        assert_eq!(*fake.timers.borrow(), 1);
        assert!(!r.mTimer.is_null());
        // SAFETY: the record's timer is the kind's poll
        assert_eq!(unsafe { (*(r.mTimer as *const EeTimePoll)).time_delay }, 1);
        assert!(r.mName.is_null() && r.mMessage.is_null());
        let slots: [usize; 4] = [0x10, 0x20, 0x30, 0x40];
        k.op(&c, EE_SCREENS_SKOP_CHAT_PARTS, slots.as_ptr() as i64, 0, &mut [0.0; 4]);
        // SAFETY: as above
        let r = unsafe { &*rec };
        assert_eq!([r.mName, r.mDrawnName, r.mMessage, r.mDrawnMessage].map(|p| p as usize), [0x10, 0x20, 0x30, 0x40]);
        assert!(h.0.borrow().is_empty());
    }

    #[test]
    fn the_three_draw_members_are_the_lines_and_the_rest_the_bases() {
        let fake = Fake::default();
        let k = ChatLineKind::new(&fake, 1.0);
        let h = Host(RefCell::new(Vec::new()));
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        let rec = k.op(&c, EE_SCREENS_SKOP_CHAT_RECORD, 0, 0, &mut [0.0; 4]) as usize;
        assert_eq!(
            k.overrides(),
            (1 << EE_SCREENS_SV_COPY_DATA) | (1 << EE_SCREENS_SV_DRAW_POLYGONS) | (1 << EE_SCREENS_SV_DRAW_TEXT)
        );
        for v in [EE_SCREENS_SV_COPY_DATA, EE_SCREENS_SV_DRAW_POLYGONS, EE_SCREENS_SV_DRAW_TEXT] {
            assert_eq!(k.virt(&c, v, 0, 0, &mut [0.0; 4]), 7);
        }
        assert_eq!(
            *fake.members.borrow(),
            vec![(EE_SCREENS_SV_COPY_DATA, rec), (EE_SCREENS_SV_DRAW_POLYGONS, rec), (EE_SCREENS_SV_DRAW_TEXT, rec)]
        );
        assert_eq!(k.virt(&c, EE_SCREENS_SV_MOUSE_MOVE, 0, 0, &mut [0.0; 4]), 11);
        assert_eq!(h.0.borrow()[0].0, EE_SCREENS_SH_BASE + EE_SCREENS_SV_MOUSE_MOVE);
        k.free(&c);
        assert_eq!(*h.0.borrow().last().unwrap(), (EE_SCREENS_SH_CHAT_FREE, 0, 0));
    }
}
