//! routeop.rs's tests: the ops' constructors and widths, their requests against routemsg's message for the same
//! members, and their replies' members and records (the whole behaviour over a socket is golden_routeops.txt).

use super::*;
use crate::routemsg as rt;

/// A message body as the server sends it: little-endian numbers, WON's length-prefixed strings.
#[derive(Default, Clone)]
struct W(Vec<u8>);

impl W {
    fn msg(t: u8) -> W {
        W(vec![3, 7, t])
    }
    fn b(mut self, v: u8) -> W {
        self.0.push(v);
        self
    }
    fn s(mut self, v: i32) -> W {
        self.0.extend_from_slice(&(v as u16).to_le_bytes());
        self
    }
    fn l(mut self, v: u32) -> W {
        self.0.extend_from_slice(&v.to_le_bytes());
        self
    }
    fn w2(self, t: &str) -> W {
        let mut w = self.s(t.len() as i32);
        for c in t.bytes() {
            w = w.s(i32::from(c));
        }
        w
    }
    fn w1(self, t: &str) -> W {
        let mut w = self.b(t.len() as u8);
        for c in t.bytes() {
            w = w.s(i32::from(c));
        }
        w
    }
    fn n1(mut self, t: &str) -> W {
        self.0.push(t.len() as u8);
        self.0.extend_from_slice(t.as_bytes());
        self
    }
    fn buf2(mut self, d: &[u8]) -> W {
        self.0.extend_from_slice(&(d.len() as u16).to_le_bytes());
        self.0.extend_from_slice(d);
        self
    }
    fn entry(self, e: &W) -> W {
        let mut w = self.s(e.0.len() as i32);
        w.0.extend_from_slice(&e.0);
        w
    }
}

fn u(t: &str) -> Vec<u16> {
    t.encode_utf16().collect()
}

/// The reply as the connection hands it: the type, the reader past the header.
fn reply(op: &mut RouteOp, w: &W) -> Read {
    op.reply(w.0[2], &w.0, 3, true)
}

#[test]
fn constructors_and_widths() {
    let ban = RouteOp::new(T_BAN_CLIENT);
    assert_eq!(ban.num(EE_WON_RTF_GROUP_ID), 0xffff);
    assert_eq!(ban.num(EE_WON_RTF_ON), 1);
    assert_eq!(ban.num(EE_WON_RTF_BY_WON_ID), 1);
    assert_eq!(ban.num(EE_WON_RTF_IS_CDKEY), 0);
    let mute = RouteOp::new(T_MUTE_CLIENT);
    assert_eq!(mute.num(EE_WON_RTF_GROUP_ID), 0xffff);
    let create = RouteOp::new(T_CREATE_GROUP);
    assert_eq!((create.num(EE_WON_RTF_GROUP_ID), create.num(EE_WON_RTF_PARENT_ID)), (0xffff, 0xffff));
    assert_eq!(RouteOp::new(T_GROUP_LIST).num(EE_WON_RTF_PARENT_ID), 0xffff);
    assert_eq!(RouteOp::new(T_DETECT_FIREWALL).num(EE_WON_RTF_DO_LISTEN), 1);
    assert_eq!(RouteOp::new(T_RELINQUISH).num(EE_WON_RTF_CLIENT_ID), 0xffff);
    let mut op = RouteOp::new(T_SET_CLIENT_FLAGS);
    op.set(EE_WON_RTF_GROUP_ID, 0x12345);
    assert_eq!(op.num(EE_WON_RTF_GROUP_ID), 0x2345);
    op.set(EE_WON_RTF_FLAGS8, 0x1ff);
    assert_eq!(op.num(EE_WON_RTF_FLAGS8), 0xff);
    op.set(EE_WON_RTF_CLIENT_FLAGS, -1);
    assert_eq!(op.num(EE_WON_RTF_CLIENT_FLAGS), 0xffff_ffff);
    op.set(EE_WON_RTF_ON, 2);
    assert_eq!(op.num(EE_WON_RTF_ON), 1);
    op.set(EE_WON_RTF_LIST_TYPE, -2);
    assert_eq!(op.num(EE_WON_RTF_LIST_TYPE), -2);
    op.set(99, 5);
    assert_eq!(op.num(99), 0);
    assert_eq!(op.num(-1), 0);
}

#[test]
fn requests_are_routemsgs_messages() {
    let mut ban = RouteOp::new(T_BAN_CLIENT);
    ban.set(EE_WON_RTF_GROUP_ID, 100);
    ban.set(EE_WON_RTF_TIME, 60);
    ban.set_text(EE_WON_RTT_COMMENT, u("spam"));
    ban.set(EE_WON_RTF_BY_WON_ID, 0);
    ban.set(EE_WON_RTF_CLIENT_ID, 11);
    let want = rt::request(&Request {
        kind: rt::EE_WON_RT_REQ_BAN,
        group_id: 100,
        on: true,
        longs: [60, 0, 0, 0],
        text1: u("spam"),
        client_id: 11,
        ..Request::default()
    });
    assert_eq!(ban.request(), Some(Sent { message: want, add_op: true }));

    let mut chat = RouteOp::new(T_SEND_CHAT);
    chat.set(EE_WON_RTF_FLAGS16, 0x104);
    chat.set_text(EE_WON_RTT_TEXT, u("hi"));
    chat.ids = vec![11, 0x4001];
    let sent = chat.request().unwrap();
    assert!(!sent.add_op, "a chat without flag 1 waits for nothing");
    assert_eq!(
        sent.message,
        rt::request(&Request {
            kind: rt::EE_WON_RT_REQ_SEND_CHAT,
            flags16: 0x104,
            text1: u("hi"),
            ids: vec![11, 0x4001],
            ..Request::default()
        })
    );
    chat.set(EE_WON_RTF_FLAGS16, 0x105);
    assert!(chat.request().unwrap().add_op);

    let mut data = RouteOp::new(T_CREATE_DATA);
    data.set(EE_WON_RTF_FLAGS8, 2);
    data.set(EE_WON_RTF_LINK_ID, 300);
    data.data_type = b"Game".to_vec();
    data.set_text(EE_WON_RTT_NAME, u("g"));
    data.data = Some(vec![1, 2, 3]);
    assert_eq!(
        data.request().unwrap().message,
        rt::request(&Request {
            kind: rt::EE_WON_RT_REQ_CREATE_DATA,
            byte1: 2,
            link_id: 300,
            narrow: b"Game",
            text1: u("g"),
            data: Some(&[1, 2, 3]),
            ..Request::default()
        })
    );

    let mut gone = RouteOp::new(T_DISCONNECT);
    assert_eq!(gone.request().map(|s| s.add_op), Some(false));
    assert_eq!(RouteOp::new(T_PEER_CHAT).request(), None);
    assert_eq!(RouteOp::new(T_DERIVED).request(), None);
}

#[test]
fn register_waits_for_its_client_list() {
    let mut op = RouteOp::new(T_REGISTER);
    op.set_text(EE_WON_RTT_NAME, u("me"));
    op.set(EE_WON_RTF_REGISTER_FLAGS, 4);
    op.set(EE_WON_RTF_REPLY_COUNT, 7);
    op.request().unwrap();
    assert_eq!((op.num(EE_WON_RTF_REPLY_COUNT), op.num(EE_WON_RTF_REPLIES_NEEDED)), (0, 2));
    let r = reply(&mut op, &W::msg(51).s(0).s(10).w2("me!").l(77));
    assert_eq!(r.end, End::Status(rt::EE_WON_RT_NEED_MORE));
    assert_eq!(op.num(EE_WON_RTF_CLIENT_ID), 10);
    assert_eq!(op.text(EE_WON_RTT_NAME), u("me!").as_slice());
    assert_eq!(op.num(EE_WON_RTF_RECONNECT_ID), 77);
    let r = reply(&mut op, &W::msg(23).s(0).s(2).s(10).w2("me!").l(1).s(11).w2("Alice").l(0x20));
    assert_eq!(r.end, End::Status(0));
    assert_eq!(op.num(EE_WON_RTF_REPLY_COUNT), 2);
    let ids: Vec<u16> = op.clients.keys().copied().collect();
    assert_eq!(ids, vec![10, 11]);
    assert_eq!(op.clients[&11].borrow().name, u("Alice"));
    assert_eq!(op.clients[&11].borrow().flags, 0x20);
    // a reply of another type is not wanted
    assert_eq!(reply(&mut op, &W::msg(66).s(0)).end, End::Status(rt::EE_WON_RT_DONT_WANT));
}

#[test]
fn group_list_with_members() {
    let mut op = RouteOp::new(T_GROUP_LIST);
    op.set(EE_WON_RTF_FLAGS16, 0xff);
    op.groups.insert(9, shared(Group::default()));
    op.request().unwrap();
    assert!(op.groups.is_empty(), "the request clears the map");
    let g1 = W::default().s(100).w2("Lobby").s(7).s(50).l(0x800).l(0x10).s(3).s(2).s(10).b(1).s(11).b(0);
    let w = W::msg(25).s(0).s(0xff).s(1).entry(&g1);
    assert_eq!(reply(&mut op, &w).end, End::Status(0));
    let g = op.groups[&100].borrow();
    assert_eq!((g.id, g.captain, g.max_clients, g.flags, g.async_flags), (100, 7, 50, 0x800, 0x10));
    assert_eq!((g.observers, g.max_players, g.parent), (3, 2, 0xffff));
    assert_eq!(g.name, u("Lobby"));
    let m: Vec<(u16, u8)> = g.members.values().map(|m| (m.borrow().id, m.borrow().flags)).collect();
    assert_eq!(m, vec![(10, 1), (11, 0)]);
    assert_eq!(op.num(EE_WON_RTF_FLAGS16), 0xff);
}

#[test]
fn members_of_another_group_are_not_wanted() {
    let mut op = RouteOp::new(T_MEMBERS);
    op.set(EE_WON_RTF_GROUP_ID, 102);
    op.request().unwrap();
    let other = W::msg(29).s(0).s(55).b(0).s(0).s(0);
    assert_eq!(reply(&mut op, &other).end, End::Status(rt::EE_WON_RT_DONT_WANT));
    let m1 = W::default().s(12).b(1).w2("bob").l(4);
    let w = W::msg(29).s(0).s(102).b(3).s(4).s(1).entry(&m1);
    assert_eq!(reply(&mut op, &w).end, End::Status(0));
    assert_eq!((op.num(EE_WON_RTF_OBSERVERS), op.num(EE_WON_RTF_HAS_NAMES)), (4, 1));
    assert_eq!(op.num(EE_WON_RTF_HAS_CLIENT_FLAGS), 1);
    let m = op.members[&12].borrow();
    assert_eq!(m.flags, 1);
    let c = m.client.as_ref().unwrap().borrow();
    assert_eq!((c.name.clone(), c.flags, c.id), (u("bob"), 4, 0));
}

#[test]
fn join_commits_its_member_list() {
    let mut op = RouteOp::new(T_JOIN_GROUP);
    op.set(EE_WON_RTF_GROUP_ID, 100);
    op.set(EE_WON_RTF_JOIN_FLAGS, 1);
    let r = reply(&mut op, &W::msg(41).s(0).s(100).w2("welcome"));
    assert_eq!(r.end, End::Status(rt::EE_WON_RT_NEED_MORE));
    assert_eq!(op.text(EE_WON_RTT_COMMENT), u("welcome").as_slice());
    let e1 = W::default().s(10).b(1).w2("me");
    let w = W::msg(29).s(0).s(100).b(1).s(0).s(1).entry(&e1);
    assert_eq!(reply(&mut op, &w).end, End::Status(0));
    assert_eq!(op.members.len(), 1);
    assert_eq!(op.members[&10].borrow().client.as_ref().unwrap().borrow().name, u("me"));
    // a join for another group is not wanted; a wait (-2541) waits on
    assert_eq!(reply(&mut op, &W::msg(41).s(0).s(99).w2("")).end, End::Status(rt::EE_WON_RT_DONT_WANT));
    assert_eq!(reply(&mut op, &W::msg(41).s(-2541).s(100).w2("")).end, End::Status(rt::EE_WON_RT_NEED_MORE));
}

#[test]
fn create_group_leaves_when_the_op_already_ended() {
    let mut op = RouteOp::new(T_CREATE_GROUP);
    op.set(EE_WON_RTF_JOIN_FLAGS, 1);
    op.set(EE_WON_RTF_LEAVE_ON_CANCEL, 1);
    let w = W::msg(12).s(0).s(400).w2("MyGame");
    // pending: nothing
    let r = op.reply(12, &w.0, 3, true);
    assert_eq!((r.end, r.leave_group), (End::Status(rt::EE_WON_RT_NEED_MORE), None));
    assert_eq!(op.num(EE_WON_RTF_GROUP_ID), 400);
    // the op ended (killed) before the join status: leave the group, once
    let s = W::msg(41).s(0);
    let r = op.reply(41, &s.0, 3, false);
    assert_eq!(r.leave_group, Some(400));
    assert_eq!(op.num(EE_WON_RTF_LEAVE_ON_CANCEL), 0);
    assert_eq!(op.reply(41, &s.0, 3, false).leave_group, None);
    // a failed status leaves nothing
    let mut op2 = RouteOp::new(T_CREATE_GROUP);
    op2.set(EE_WON_RTF_JOIN_FLAGS, 1);
    op2.set(EE_WON_RTF_LEAVE_ON_CANCEL, 1);
    assert_eq!(op2.reply(41, &W::msg(41).s(-1).0, 3, false).leave_group, None);
    // without the join flag the parent id follows the name
    let mut op3 = RouteOp::new(T_CREATE_GROUP);
    assert_eq!(reply(&mut op3, &W::msg(12).s(0).s(301).w2("G2").s(7)).end, End::Status(0));
    assert_eq!((op3.num(EE_WON_RTF_GROUP_ID), op3.num(EE_WON_RTF_PARENT_ID)), (301, 7));
}

#[test]
fn notifications() {
    let mut chat = RouteOp::new(T_PEER_CHAT);
    assert_eq!(reply(&mut chat, &W::msg(47).s(12).s(0x104).w2("psst").s(2).s(10).s(11)).end, End::Status(0));
    assert_eq!((chat.num(EE_WON_RTF_CLIENT_ID), chat.num(EE_WON_RTF_FLAGS16)), (12, 0x104));
    assert_eq!(chat.text(EE_WON_RTT_TEXT), u("psst").as_slice());
    assert_eq!(chat.ids, vec![10, 11]);

    let mut joined = RouteOp::new(T_CLIENT_JOINED_SERVER);
    let w = W::msg(8).s(16).w2("Gina").l(0).b(2).b(1).b(4).l(31337).b(2).b(4).l(4242);
    assert_eq!(reply(&mut joined, &w).end, End::Status(0));
    let c = joined.op_client.as_ref().unwrap().borrow();
    assert_eq!((c.id, c.user_id, c.address), (16, 31337, 4242));

    let mut modified = RouteOp::new(T_DATA_MODIFIED);
    let w = W::msg(16).s(300).n1("Game").w1("g").s(2).b(1).buf2(&[9]);
    assert_eq!(reply(&mut modified, &w).end, End::Status(0));
    assert_eq!((modified.num(EE_WON_RTF_LINK_ID), modified.num(EE_WON_RTF_OFFSET)), (300, 2));
    assert_eq!(modified.num(EE_WON_RTF_ON), 1);
    assert_eq!(modified.data_type, b"Game".to_vec());
    assert_eq!(modified.text(EE_WON_RTT_NAME), u("g").as_slice());
    assert_eq!(modified.data, Some(vec![9]));

    let mut banned = RouteOp::new(T_BANNED);
    assert_eq!(reply(&mut banned, &W::msg(69).s(100).b(1).w2("bye").l(5)).end, End::Status(0));
    assert_eq!((banned.num(EE_WON_RTF_GROUP_ID), banned.num(EE_WON_RTF_TIME)), (100, 5));
    assert_eq!(banned.text(EE_WON_RTT_COMMENT), u("bye").as_slice());

    let mut joined_group = RouteOp::new(T_CLIENT_JOINED_GROUP);
    assert_eq!(reply(&mut joined_group, &W::msg(7).b(3).s(101).s(18).b(4).w2("Fred").l(9)).end, End::Status(0));
    assert_eq!(joined_group.num(EE_WON_RTF_HAS_CLIENT_FLAGS), 1);
    assert_eq!(joined_group.num(EE_WON_RTF_CLIENT_FLAGS), 9);

    let mut derived = RouteOp::new(T_DERIVED);
    let w = W(vec![3, 8, 9, 1, 2, 3]);
    assert_eq!(reply(&mut derived, &w).end, End::Status(0));
    assert_eq!((derived.num(EE_WON_RTF_MSG_TYPE), derived.data.clone()), (9, Some(vec![1, 2, 3])));

    // a notice cut short: the read throws
    let mut flags = RouteOp::new(T_CLIENT_FLAGS_CHANGED);
    assert_eq!(reply(&mut flags, &W::msg(6).s(11)).end, End::Cut);
    assert_eq!(flags.num(EE_WON_RTF_CLIENT_ID), 11);
    // the notice types
    assert_eq!(notice_type(rt::EE_WON_RT_N_MUTED), T_MUTED);
    assert_eq!(notice_type(rt::EE_WON_RT_N_MEMBER_COUNT), T_MEMBER_COUNT);
    assert_eq!(notice_type(-1), 0);
}

#[test]
fn data_objects_and_bad_users() {
    let mut read = RouteOp::new(T_READ_DATA);
    read.objects.push(shared(Object::default()));
    read.request().unwrap();
    assert!(read.objects.is_empty());
    let w = W::msg(49).s(0).s(2).s(300).n1("Game").w1("g").buf2(&[10, 11]).s(301).n1("Game").w1("").buf2(&[]);
    assert_eq!(reply(&mut read, &w).end, End::Status(0));
    let o: Vec<(u16, Option<Vec<u8>>)> =
        read.objects.iter().map(|o| (o.borrow().link, o.borrow().data.clone())).collect();
    assert_eq!(o, vec![(300, Some(vec![10, 11])), (301, Some(vec![]))]);

    let mut bad = RouteOp::new(T_BAD_USERS);
    let u1 = W::default().l(5001).w2("Mallory").l(3600).l(42).w2("Mod").w2("spam");
    let w = W::msg(21).s(0).b(0xfe).s(100).b(0x0f).s(1).entry(&u1);
    assert_eq!(reply(&mut bad, &w).end, End::Status(0));
    assert_eq!(bad.num(EE_WON_RTF_LIST_TYPE), -2, "the list type is a signed byte");
    assert_eq!(bad.bad_users.len(), 1);
    let b = &bad.bad_users[0];
    assert_eq!((b.won_id, b.expiration, b.moderator_id), (5001, 3600, 42));
    assert_eq!((b.moderator_name.clone(), b.comment.clone()), (u("Mod"), u("spam")));
}

#[test]
fn group_list_bodies() {
    let mut op = RouteOp::new(T_GROUP_LIST);
    let g1 = W::default().s(7).w2("Seven");
    let body = W::default().s(0x01).s(1).entry(&g1);
    assert!(op.parse_group_list(&body.0));
    assert_eq!(op.groups[&7].borrow().name, u("Seven"));
    // cut short: false, what was read stays
    let cut = W::default().s(0x01).s(2).entry(&W::default().s(9).w2("Nine")).s(40).s(10);
    assert!(!op.parse_group_list(&cut.0));
    assert_eq!(op.groups.keys().copied().collect::<Vec<_>>(), vec![9]);
    // a group list request op's replies: each read again (a bad header reads nothing)
    let mut inner = RouteOp::new(T_GROUP_LIST);
    let good = W::msg(25).s(0).s(1).s(1).entry(&W::default().s(5).w2("Five"));
    let bad = W(vec![3, 7, 24, 0, 0]);
    inner.read_group_list_replies(&[good.0.clone(), bad.0]);
    assert_eq!(inner.groups.keys().copied().collect::<Vec<_>>(), vec![5]);
    assert_eq!(inner.num(EE_WON_RTF_FLAGS16), 1);
}

unsafe extern "C" fn collect(context: *mut c_void, r: *const EeWonRouteRecord) {
    let v = unsafe { &mut *(context as *mut Vec<(i32, u16, u16, usize)>) };
    let r = unsafe { &*r };
    v.push((r.kind, r.key, r.id, r.name_len));
}

#[test]
fn c_abi() {
    let op = ee_won_routeop_new(T_GROUP_LIST);
    unsafe {
        assert_eq!(ee_won_routeop_type(op), T_GROUP_LIST);
        ee_won_routeop_set_num(op, EE_WON_RTF_FLAGS16, 0x81);
        assert_eq!(ee_won_routeop_num(op, EE_WON_RTF_FLAGS16), 0x81);
        let body = W::default().s(0x81).s(1).entry(&W::default().s(7).w2("Seven").s(1).s(3).b(1));
        assert!(ee_won_routeop_parse_group_list(op, body.0.as_ptr(), body.0.len()));
        let mut got: Vec<(i32, u16, u16, usize)> = Vec::new();
        ee_won_routeop_records(op, EE_WON_RTR_GROUP, collect, &mut got as *mut _ as *mut c_void);
        assert_eq!(got, vec![(EE_WON_RTR_GROUP, 7, 7, 5)]);
        let ids = [4u16, 5];
        ee_won_routeop_set_ids(op, ids.as_ptr(), 2);
        let mut out = [0u16; 1];
        assert_eq!(ee_won_routeop_ids(op, out.as_mut_ptr(), 1), 2);
        assert_eq!(out, [4]);
        let mut r = EeWonRouteOpRead::default();
        let w = W::msg(66).s(0);
        ee_won_routeop_reply(op, 66, w.0.as_ptr(), w.0.len(), 3, true, &mut r);
        assert_eq!((r.status, r.cut), (rt::EE_WON_RT_DONT_WANT, false));
        ee_won_routeop_free(op);
    }
    assert_eq!(ee_won_routeop_notice_type(rt::EE_WON_RT_N_PEER_CHAT), T_PEER_CHAT);
}
