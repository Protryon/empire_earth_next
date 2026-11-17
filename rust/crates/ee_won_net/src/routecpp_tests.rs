//! The request helpers' members by kind, a request made and run over a stand-in core table (the new op, its
//! completion's and the local's references, its run), the notification ops, the chat input's requests and the
//! text conversion.

use super::*;
use crate::cppcore::tests::LOCK;
use crate::cppcore::{bind, EeWonCoreHooks};
use crate::opobj::{EeWonAsyncOp, EeWonAsyncOpHooks};
use std::sync::Mutex;

static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
/// the stand-in ops made: (C++ pointer, Rust routing op, Rust async op)
static MADE: Mutex<Vec<(usize, usize, usize)>> = Mutex::new(Vec::new());

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}
fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}

unsafe extern "C" fn r_add(o: *mut c_void) {
    log(format!("+{:x}", o as usize));
}
unsafe extern "C" fn r_release(o: *mut c_void) {
    log(format!("-{:x}", o as usize));
}
unsafe extern "C" fn object_of_stub(o: *mut c_void, kind: EeWonCoreObject) -> *mut c_void {
    let made = MADE.lock().unwrap();
    let e = made.iter().find(|e| e.0 == o as usize);
    match (e, kind) {
        (Some(e), EeWonCoreObject::RouteOp) => e.1 as *mut c_void,
        (Some(e), EeWonCoreObject::Op) => e.2 as *mut c_void,
        _ => null_mut(),
    }
}
static OP_HOOKS: EeWonAsyncOpHooks = EeWonAsyncOpHooks {
    run_hook: None,
    cleanup_hook: None,
    core_queue: Some(r_queue),
    timer_add: None,
    timer_remove: None,
    completion_add_ref: Some(r_add),
    completion_release: Some(r_release),
    completion_call: None,
    op_add_ref: None,
    op_release: None,
    op_kill: None,
    ref_add: None,
    ref_release: None,
    new_completion: None,
    callback_hook: None,
};
unsafe extern "C" fn r_queue(o: *mut c_void) {
    log(format!("queue {:x}", o as usize));
}
/// a new op: C++ pointer 0x100 + its index, its Rust objects kept (leaked: the test's)
unsafe extern "C" fn new_route_op_stub(conn: *mut c_void, ty: i32) -> *mut c_void {
    let mut made = MADE.lock().unwrap();
    let cpp = 0x100 + made.len();
    let r = ee_won_routeop_new_on(ty, conn);
    let o = Box::into_raw(Box::new(EeWonAsyncOp::new(cpp as *mut c_void, &OP_HOOKS)));
    made.push((cpp, r as usize, o as usize));
    log(format!("new {ty} on {:x}", conn as usize));
    cpp as *mut c_void
}
unsafe extern "C" fn route_virtual_stub(conn: *mut c_void, which: i32, o: *mut c_void, t: u8) -> *mut c_void {
    log(format!("virtual {which} {:x} {:x} {t}", conn as usize, o as usize));
    if which == 1 {
        0x900usize as *mut c_void
    } else {
        null_mut()
    }
}

static ROUTE_STUB: EeWonCoreHooks = EeWonCoreHooks {
    ref_add: Some(r_add),
    ref_release: Some(r_release),
    completion_call: None,
    object_of: Some(object_of_stub),
    socketop_duplicate: None,
    new_op: None,
    new_completion: None,
    new_socket_op: None,
    new_queue_socket: None,
    new_udp_recv_op: None,
    udp_check: None,
    udp_response_check: std::ptr::null_mut(),
    socket_bind_local: None,
    bytes_of: None,
    socketop_continue: None,
    socketop_start: None,
    socket_thread_add: None,
    bytes_adopt: None,
    transform: None,
    progress: None,
    accept: None,
    socket_hook: None,
    new_route_op: Some(new_route_op_stub),
    route_virtual: Some(route_virtual_stub),
    new_http_conn: None,
};

fn filled(kind: i32, values: &[Value]) -> RouteOp {
    let (ty, slots, extra) = plan(kind).unwrap();
    let mut o = RouteOp::new(ty);
    fill(&mut o, slots, extra, values);
    o
}
fn text(s: &str) -> Value {
    Value::units(&s.encode_utf16().collect::<Vec<_>>())
}

#[test]
fn every_kind_has_a_plan_and_its_type() {
    let types: Vec<i32> = (0..=36).map(|k| plan(k).unwrap().0).collect();
    assert_eq!(
        types,
        [
            12, 34, 3, 3, 3, 38, 38, 38, 42, 20, 20, 21, 22, 35, 5, 2, 33, 52, 47, 47, 56, 1, 51, 44, 46, 4, 11, 16,
            37, 41, 57, 18, 54, 53, 43, 17, 19
        ]
    );
    assert!(plan(37).is_none());
    assert!(plan(-1).is_none());
}

#[test]
fn members_as_the_helpers_set_them() {
    let o = filled(
        EE_WON_RTQ_CREATE_GROUP,
        &[Value::num(7), text("room"), text("pw"), Value::num(8), Value::num(0x800), Value::num(3), Value::num(9)],
    );
    assert_eq!(o.num(EE_WON_RTF_GROUP_ID), 7);
    assert_eq!(String::from_utf16(o.text(EE_WON_RTT_NAME)).unwrap(), "room");
    assert_eq!(String::from_utf16(o.text(EE_WON_RTT_PASSWORD)).unwrap(), "pw");
    assert_eq!(o.num(EE_WON_RTF_MAX_PLAYERS), 8);
    assert_eq!(o.num(EE_WON_RTF_GROUP_FLAGS), 0x800);
    assert_eq!(o.num(EE_WON_RTF_JOIN_FLAGS), 3);
    assert_eq!(o.num(EE_WON_RTF_ASYNC_FLAGS), 9);
    // the client list's extended flag
    assert_eq!(filled(EE_WON_RTQ_CLIENT_LIST, &[]).num(EE_WON_RTF_IS_EX), 0);
    assert_eq!(filled(EE_WON_RTQ_CLIENT_LIST_EX, &[]).num(EE_WON_RTF_IS_EX), 1);
    // the client mask cut to 16 bits
    let o = filled(EE_WON_RTQ_SET_CLIENT_FLAGS, &[Value::num(0x12345), Value::num(1), Value::num(2), Value::num(3)]);
    assert_eq!(o.num(EE_WON_RTF_CLIENT_MASK), 0x2345);
    assert_eq!(o.num(EE_WON_RTF_ASYNC_MSG_FLAGS), 3);
    // the data, its type and the recipients
    let o = filled(
        EE_WON_RTQ_CREATE_DATA,
        &[
            Value::num(1),
            Value::num(0xffff),
            Value { num: 0, bytes: b"_T".to_vec() },
            text("n"),
            Value { num: 1, bytes: vec![1, 2] },
        ],
    );
    assert_eq!(o.data_type, b"_T");
    assert_eq!(o.data, Some(vec![1, 2]));
    assert_eq!(o.num(EE_WON_RTF_LINK_ID), 0xffff);
    let o = filled(EE_WON_RTQ_DATA, &[Value::default(), Value::num(4), Value::units(&[5, 6])]);
    assert_eq!(o.data, None);
    assert_eq!(o.ids, [5, 6]);
    let o = filled(EE_WON_RTQ_READ_DATA, &[Value::num(1), Value::num(2), Value::default(), Value::num(0)]);
    assert_eq!((o.num(EE_WON_RTF_ON), o.num(EE_WON_RTF_LINK_ID)), (1, 2));
}

#[test]
fn bans_and_mutes_by_who() {
    let v = |id| [Value::num(3), Value::num(1), Value::num(60), text("x"), Value::num(id)];
    let o = filled(EE_WON_RTQ_BAN, &v(5));
    assert_eq!((o.num(EE_WON_RTF_CLIENT_ID), o.num(EE_WON_RTF_WON_ID)), (5, 0));
    assert_eq!((o.num(EE_WON_RTF_BY_WON_ID), o.num(EE_WON_RTF_IS_CDKEY)), (0, 0));
    let o = filled(EE_WON_RTQ_MUTE_WON_ID, &v(77));
    assert_eq!((o.num(EE_WON_RTF_CLIENT_ID), o.num(EE_WON_RTF_WON_ID)), (0, 77));
    assert_eq!((o.num(EE_WON_RTF_BY_WON_ID), o.num(EE_WON_RTF_IS_CDKEY)), (1, 0));
    let o = filled(EE_WON_RTQ_BAN_CDKEY_ID, &v(88));
    assert_eq!((o.num(EE_WON_RTF_WON_ID), o.num(EE_WON_RTF_BY_WON_ID), o.num(EE_WON_RTF_IS_CDKEY)), (88, 1, 1));
    assert_eq!((o.num(EE_WON_RTF_GROUP_ID), o.num(EE_WON_RTF_TIME), o.num(EE_WON_RTF_ON)), (3, 60, 1));
}

#[test]
fn an_alert_to_nobody_is_not_sent() {
    assert!(!sends(EE_WON_RTQ_ALERT_TO, &[text("a"), Value::default()]));
    assert!(sends(EE_WON_RTQ_ALERT_TO, &[text("a"), Value::units(&[1])]));
    assert!(sends(EE_WON_RTQ_ALERT, &[text("a")]));
}

#[test]
fn a_request_made_and_run() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    bind(&ROUTE_STUB);
    take();
    let conn = 0x50usize as *mut c_void;
    let completion = 0x60usize as *mut c_void;
    request(conn, EE_WON_RTQ_LEAVE, &[Value::num(12)], completion);
    let (cpp, r, _) = *MADE.lock().unwrap().last().unwrap();
    // the op made on the connection, its completion's reference, the local's around the run (queued: async)
    assert_eq!(
        take(),
        [
            "new 35 on 50",
            "+60",
            format!("+{cpp:x}").as_str(),
            format!("queue {cpp:x}").as_str(),
            format!("-{cpp:x}").as_str()
        ]
    );
    // SAFETY: the stub's live Rust op
    let o = unsafe { &*(r as *const EeWonRouteOp) };
    assert_eq!((o.0.num(EE_WON_RTF_GROUP_ID), o.1), (12, 0x50));
    // nothing for an alert to nobody or an unknown kind
    request(conn, EE_WON_RTQ_ALERT_TO, &[text("a"), Value::default()], null_mut());
    request(conn, 99, &[], null_mut());
    assert!(take().is_empty());
}

#[test]
fn notification_ops() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    bind(&ROUTE_STUB);
    take();
    let conn = 0x50usize as *mut c_void;
    // SAFETY: the stub table
    unsafe {
        // a derived server's message: the virtual's op
        assert_eq!(n_route_unsolicited(conn, -1, 9), 0x900usize as *mut c_void);
        assert_eq!(take(), ["virtual 1 50 0 9"]);
        // a kind without a notification op: none
        assert_eq!(n_route_unsolicited(conn, 999, 0), null_mut());
        // a notification: its op with the caller's reference, its type
        let kind = crate::routemsg::EE_WON_RT_N_SERVER_ALERT;
        let o = n_route_unsolicited(conn, kind, 0);
        assert_eq!(take(), ["new 48 on 50", format!("+{:x}", o as usize).as_str()]);
        assert_eq!(n_route_type(o), 48);
        assert!(!n_route_derived(o));
        n_route_complete_hook(conn, o);
        assert_eq!(take(), [format!("virtual 0 50 {:x} 0", o as usize)]);
    }
}

#[test]
fn chat_input_requests() {
    let text: Vec<u16> = "hi".encode_utf16().collect();
    let a = EeWonSmartAction { kind: EE_WON_SMART_ACT_CHAT, group: 4, flags: 0x10002, ..EeWonSmartAction::default() };
    let (k, v) = action_request(&a, &text).unwrap();
    assert_eq!(k, EE_WON_RTQ_CHAT);
    assert_eq!(v[1].num, 2);
    assert_eq!(v[2].u16s(), [4]);
    let a = EeWonSmartAction { kind: EE_WON_SMART_ACT_BAN_CDKEY, group: 1, value: 99, seconds: 5, on: true, ..a };
    let (k, v) = action_request(&a, &text).unwrap();
    assert_eq!(k, EE_WON_RTQ_BAN_CDKEY_ID);
    assert_eq!(v.iter().map(|x| x.num).collect::<Vec<_>>(), [1, 1, 5, 0, 99]);
    let a = EeWonSmartAction { kind: EE_WON_SMART_ACT_CLIENT_FLAGS, flags: 7, value: 8, ..a };
    let (k, v) = action_request(&a, &text).unwrap();
    assert_eq!(k, EE_WON_RTQ_SET_CLIENT_FLAGS);
    assert_eq!(v.iter().map(|x| x.num).collect::<Vec<_>>(), [7, 8, 0, 0]);
    let a = EeWonSmartAction { kind: EE_WON_SMART_ACT_INVITE, client: 3, ..a };
    assert_eq!(action_request(&a, &text).unwrap().0, EE_WON_RTQ_INVITE);
    let a = EeWonSmartAction { kind: EE_WON_SMART_ACT_MODERATOR, ..a };
    assert_eq!(action_request(&a, &text).unwrap().1.len(), 1);
    let a = EeWonSmartAction { kind: EE_WON_SMART_ACT_MUTE, ..a };
    assert_eq!(action_request(&a, &text).unwrap().0, EE_WON_RTQ_MUTE);
    let a = EeWonSmartAction { kind: EE_WON_SMART_ACT_NONE, ..a };
    assert!(action_request(&a, &text).is_none());
}

#[test]
fn units_past_the_bmp_become_surrogates() {
    assert_eq!(utf16(&[0x41, 0xffff, 0x1f600]), [0x41, 0xffff, 0xd83d, 0xde00]);
}
