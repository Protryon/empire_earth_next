use super::*;
use std::cell::RefCell;
use std::rc::Rc;

struct Log(Rc<RefCell<Vec<&'static str>>>);
impl SoundHook for Log {
    fn play(&self) {
        self.0.borrow_mut().push("play");
    }
    fn stop(&self) {
        self.0.borrow_mut().push("stop");
    }
    fn resume(&self) {
        self.0.borrow_mut().push("resume");
    }
}
impl Drop for Log {
    fn drop(&mut self) {
        self.0.borrow_mut().push("drop");
    }
}

#[test]
fn muting_stops_and_silences() {
    let log = Rc::new(RefCell::new(Vec::new()));
    let s = new_sound(1, b"a.wav", Some(Box::new(Log(log.clone()))));
    unsafe {
        assert!(is_sound(s as *const c_void));
        ee_won_sound_call(s, EeWonSoundCall::Play, 0);
        ee_won_sound_call(s, EeWonSoundCall::SetMuted, 1);
        ee_won_sound_call(s, EeWonSoundCall::SetMuted, 1); // already muted: no stop
        assert_eq!(ee_won_sound_call(s, EeWonSoundCall::GetMuted, 0), 1);
        assert_eq!(ee_won_sound_call(s, EeWonSoundCall::Flags, 0), 5);
        ee_won_sound_call(s, EeWonSoundCall::Play, 0); // muted: nothing
        ee_won_sound_call(s, EeWonSoundCall::Resume, 0); // the hook's, muted or not
        ee_won_sound_call(s, EeWonSoundCall::SetMuted, 0);
        ee_won_sound_call(s, EeWonSoundCall::SetLooping, 1);
        assert_eq!(ee_won_sound_call(s, EeWonSoundCall::Looping, 0), 1);
        assert_eq!((*s).path(), b"a.wav");
        free_sound(s as *mut c_void);
    }
    assert_eq!(*log.borrow(), ["play", "stop", "resume", "drop"]);
}

#[test]
fn decoding() {
    fn none(_: &[u8]) -> Option<Box<dyn SoundHook>> {
        None
    }
    set_decoder(None);
    let s = decode(0, b"x");
    assert!(!s.is_null());
    unsafe {
        ee_won_sound_call(s, EeWonSoundCall::Play, 0); // no hook: nothing
        free_sound(s as *mut c_void);
    }
    set_decoder(Some(none));
    assert!(decode(0, b"x").is_null());
    set_decoder(None);
    assert_eq!(unsafe { ee_won_sound_call(std::ptr::null(), EeWonSoundCall::GetMuted, 0) }, 0);
}
