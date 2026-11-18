//! The sends' members: the C++ casts kept (`(long)` 32 bits on Windows, `(unsigned char)`, `!= 0`), the ops that
//! stay the C++'s.

use super::*;

fn ints(m: &Msg) -> Vec<i64> {
    m.values.iter().map(|v| v.i).collect()
}

#[test]
fn the_casts() {
    assert_eq!(byte(0x1ff).i, 0xff);
    assert_eq!(flag(-7).i, 1);
    assert_eq!(flag(0).i, 0);
    #[cfg(windows)]
    assert_eq!(long(0x1_8000_0001).i, -0x7fff_ffff);
    assert_eq!(long(-5).i, -5);
    assert_eq!(int(0xffff_ffff).i, 0xffff_ffff);
}

#[test]
fn the_screens_sends() {
    let m = screens(EE_SCREENS_GAME_HK_GROUP, 0, 0x1ff, 2, 0, || 0).unwrap();
    assert_eq!((m.kind, ints(&m)), (MSG_SAVED_GROUP_SELECT, vec![0xff, 1]));
    let m = screens(EE_SCREENS_GAME_HK_GROUP, 5, 3, 0, 0, || 0).unwrap();
    assert_eq!((m.kind, ints(&m)), (MSG_SAVED_GROUP_CREATE, vec![3]));
    // (an unsigned argument as is, a `(long)` of it 32 bits on Windows)
    let m = screens(EE_SCREENS_GAME_HK_SELECT_UNIT, 0xffff_ffff, 0, 0, 0, || 0).unwrap();
    assert_eq!(ints(&m), vec![0xffff_ffff]);
    let m = screens(EE_SCREENS_GAME_MG_ATTACK, 4, 0, 0, 0, || 2).unwrap();
    assert_eq!((m.kind, ints(&m)), (MSG_ATTACK_PLAYERS, vec![2, 1]));
    let m = screens(EE_SCREENS_GAME_MG_SEND_SPEED, 1.5f32.to_bits(), 0, 0, 0, || 0).unwrap();
    assert_eq!((m.values[0].kind, m.values[0].f[0]), (EE_SCREENS_GA_VALUE_FLOAT, 1.5));
    let m = screens(EE_SCREENS_GAME_DS_RESOURCES, 1, 2, 3, 0x104, || 0).unwrap();
    assert_eq!(ints(&m), vec![3, 1, 2, 4]);
    assert_eq!(screens(EE_SCREENS_GAME_GO_RESIGN, 0, 0, 0, 0, || 0).unwrap().values, vec![]);
    assert!(screens_answers(EE_SCREENS_GAME_MG_KILL) && !screens_answers(EE_SCREENS_GAME_MG_RALLY));
    // the C++'s own ops are not sends
    assert!(screens(EE_SCREENS_GAME_MG_SET_CLOCK, 0, 0, 0, 0, || 0).is_none());
}

#[test]
fn the_tribute_and_the_hud_sends() {
    let m = tribute(0x103, &[10, 11, 12, 13, 14]);
    assert_eq!((m.kind, ints(&m)), (MSG_PAY_TRIBUTE, vec![10, 12, 14, 13, 11, 3]));
    let m = icon_bar(EE_SCREENS_IB_OP_SCRAMBLE, 9, 0x5_ffff_fffe).unwrap();
    assert_eq!(ints(&m), vec![-2, 5, 9]);
    let m = icon_bar(EE_SCREENS_IB_OP_UNLOAD, 1, 2).unwrap();
    assert_eq!((m.kind, ints(&m)), (MSG_IMMEDIATE_CARGO_UNLOAD, vec![1, 2, 0]));
    assert!(icon_bar(EE_SCREENS_IB_OP_SELECT_THESE, 0, 0).is_none());
    let m = portrait(EE_SCREENS_PT_OP_REMOVE_TECHNOLOGY, || 77, 5).unwrap();
    assert_eq!((m.kind, ints(&m)), (MSG_REMOVE_TECHNOLOGY, vec![77, 5, 1, 0]));
    let m = status_button(EE_SCREENS_SB_SEND_EXCHANGE, 7, 0).unwrap();
    assert_eq!((m.kind, ints(&m)), (MSG_EXCHANGE_RESOURCES, vec![1, 3, 0]));
    assert_eq!(ints(&status_button(EE_SCREENS_SB_SEND_PATROL, 0, 0).unwrap()), vec![-1]);
    assert!(status_button(EE_SCREENS_SB_SEND_UNLOAD, 0, 0).is_none());
    assert!(status_button(EE_SCREENS_SB_SEND_RESEARCH, 0, 0).is_none());
    let io = EeScreensMainFormIo { f: [1.5, 2.5, 0.0, 0.0], n: [3, 0x1ff] };
    let m = mouse_mode(EE_SCREENS_MF_SEND_AIRPORT, 4, &io).unwrap();
    assert_eq!((m.values[0].kind, m.values[0].f), (EE_SCREENS_GA_VALUE_POINT, [1.5, 2.5]));
    assert_eq!(ints(&m)[1..], [4, 3, 0xff]);
    assert!(mouse_mode(EE_SCREENS_MF_SEND_RALLY, 0, &io).is_none());
}
