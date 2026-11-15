use super::*;
use std::cell::RefCell;

thread_local! {
    static LOG: RefCell<Vec<(u32, isize)>> = const { RefCell::new(Vec::new()) };
    static MANAGERS: Cell<bool> = const { Cell::new(true) };
}

unsafe extern "C" fn host(_obj: *mut c_void, op: EeWonEeSoundOp, a: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((op as u32, a)));
    match op {
        EeWonEeSoundOp::HasUiManager | EeWonEeSoundOp::HasSoundManager => MANAGERS.with(|m| m.get()) as isize,
        EeWonEeSoundOp::Play => 77,
        EeWonEeSoundOp::Load => 0xFFFF_FFF0,
        _ => 0,
    }
}

fn take() -> Vec<(u32, isize)> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

#[test]
fn an_effect_plays_stops_and_unloads() {
    MANAGERS.with(|m| m.set(true));
    let p = ee_won_eesound_new(std::ptr::null_mut(), EeWonEeSoundKind::Effect, Some(host), 9);
    unsafe {
        assert_eq!(ee_won_eesound_call(p, EeWonEeSoundCall::Field, 1), u32::MAX as isize);
        ee_won_eesound_call(p, EeWonEeSoundCall::Play, 0);
        assert_eq!(ee_won_eesound_call(p, EeWonEeSoundCall::Field, 1), 77);
        ee_won_eesound_call(p, EeWonEeSoundCall::Resume, 0); // the base's empty one
        ee_won_eesound_call(p, EeWonEeSoundCall::Stop, 0);
        ee_won_eesound_call(p, EeWonEeSoundCall::Stop, 0); // not playing: no Stop op
        ee_won_eesound_free(p);
    }
    let h = EeWonEeSoundOp::HasSoundManager as u32;
    assert_eq!(
        take(),
        vec![
            (h, 0),
            (EeWonEeSoundOp::Play as u32, 9),
            (h, 0),
            (EeWonEeSoundOp::Stop as u32, 77),
            (h, 0),
            (h, 0),
            (EeWonEeSoundOp::Unload as u32, 9)
        ]
    );
}

#[test]
fn the_music_turns_the_volume() {
    MANAGERS.with(|m| m.set(true));
    let p = ee_won_eesound_new(std::ptr::null_mut(), EeWonEeSoundKind::Music, Some(host), 0);
    unsafe {
        ee_won_eesound_call(p, EeWonEeSoundCall::Play, 0);
        ee_won_eesound_call(p, EeWonEeSoundCall::Stop, 0);
        ee_won_eesound_call(p, EeWonEeSoundCall::Resume, 0);
        ee_won_eesound_free(p);
    }
    let h = EeWonEeSoundOp::HasUiManager as u32;
    assert_eq!(
        take(),
        vec![
            (h, 0),
            (EeWonEeSoundOp::MusicVolumeOn as u32, 0),
            (h, 0),
            (EeWonEeSoundOp::MusicVolumeOff as u32, 0),
            (h, 0),
            (EeWonEeSoundOp::MusicVolumeOn as u32, 0)
        ]
    );
}

#[test]
fn no_managers() {
    MANAGERS.with(|m| m.set(false));
    let p = ee_won_eesound_new(std::ptr::null_mut(), EeWonEeSoundKind::Effect, Some(host), 3);
    unsafe {
        ee_won_eesound_call(p, EeWonEeSoundCall::Play, 0);
        assert_eq!(ee_won_eesound_call(p, EeWonEeSoundCall::Field, 1), u32::MAX as isize);
        ee_won_eesound_free(p);
    }
    assert_eq!(ee_won_eesound_load(Some(host), c"a.wav".as_ptr().cast()), -1);
    MANAGERS.with(|m| m.set(true));
    assert_eq!(ee_won_eesound_load(Some(host), c"a.wav".as_ptr().cast()), 0xFFFF_FFF0);
    take();
}
