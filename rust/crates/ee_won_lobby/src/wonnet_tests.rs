use super::*;
use std::sync::Mutex;

/// The ops a test host saw, and its answers.
struct Mock {
    ops: Vec<(EeWonNetOp, isize, isize)>,
    status: i32,
    group: u16,
    current_form: isize,
}

static MOCK: Mutex<Option<Mock>> = Mutex::new(None);
static SERIAL: Mutex<()> = Mutex::new(());

unsafe extern "C" fn host(_cpp: *mut c_void, op: EeWonNetOp, a: isize, b: isize, _c: isize, _d: isize) -> isize {
    let mut g = MOCK.lock().unwrap();
    let m = g.as_mut().unwrap();
    m.ops.push((op, a, b));
    match op {
        Op::OpStatus => m.status as isize,
        Op::OpNum => m.group as isize,
        Op::FormById => 100 + a,
        Op::CurrentForm => m.current_form,
        Op::TextLoad => {
            let t = format!("T{}", a);
            ee_won_wonnet_sink(b as *mut c_void, t.as_ptr() as *const c_void, t.len(), 1);
            0
        }
        _ => 0,
    }
}

fn with_mock<R>(
    routing: bool,
    status: i32,
    group: u16,
    f: impl FnOnce(&EeWonNet) -> R,
) -> (R, Vec<(EeWonNetOp, isize, isize)>) {
    let _s = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    *MOCK.lock().unwrap() = Some(Mock { ops: Vec::new(), status, group, current_form: 0 });
    ee_won_wonnet_set_host(Some(host));
    let net = EeWonNet::new(core::ptr::null_mut());
    // a routing connection in the slot (never called through: the mock answers its ops)
    if routing {
        // SAFETY: the object's slot
        unsafe { (*net.objects()).routing = 8 as *mut c_void };
    }
    let r = f(&net);
    let ops = MOCK.lock().unwrap().take().unwrap().ops;
    ee_won_wonnet_set_host(None);
    (r, ops)
}

#[test]
fn the_game_info_round_trips_and_reads_the_old_c_strings() {
    let info = GameInfo {
        name: b"My Game".to_vec(),
        address: b"10.0.0.1".to_vec(),
        password: b"pw".to_vec(),
        humans: 3,
        players: 8,
        host: 0x1234,
        flags: 3,
        room: 0x77,
    };
    let data = export_game_info(&info);
    // version, three texts (4 + bytes + 2 each), four words, the room
    assert_eq!(data.len(), 2 + (4 + 7 + 2) + (4 + 8 + 2) + (4 + 2 + 2) + 16 + 2);
    assert_eq!(&data[..2], &[1, 0]);
    assert_eq!(&data[2..6], &9u32.to_le_bytes());
    let mut back = GameInfo::default();
    import_game_info(&data, &mut back);
    assert_eq!(back, info);
    // an older record: C strings
    let mut old = vec![0u8, 0];
    old.extend_from_slice(b"\0");
    let mut raw = b"ab\0cd\0\0".to_vec();
    raw.extend_from_slice(&5u32.to_le_bytes());
    raw.extend_from_slice(&6u32.to_le_bytes());
    raw.extend_from_slice(&7u32.to_le_bytes());
    raw.extend_from_slice(&1u32.to_le_bytes());
    raw.extend_from_slice(&9u16.to_le_bytes());
    // version 0 is read as C strings from the start (the version's bytes are the name's)
    let mut rec = vec![b'x', b'y'];
    rec.extend_from_slice(&raw);
    let mut g = GameInfo::default();
    import_game_info(&rec, &mut g);
    assert_eq!(g.name, b"xyab");
    assert_eq!(g.address, b"cd");
    assert_eq!(g.password, b"");
    assert_eq!((g.humans, g.players, g.host, g.flags, g.room), (5, 6, 7, 1, 9));
    // a short buffer reads zeros past its end
    let mut z = GameInfo { flags: 9, ..GameInfo::default() };
    import_game_info(&[1, 0], &mut z);
    assert_eq!(z, GameInfo::default());
}

#[test]
fn the_texts_convert_as_the_front_end_did() {
    assert_eq!(bytes_to_wide(b""), Vec::<u32>::new());
    assert_eq!(bytes_to_wide(b"ab"), vec![0x61, 0x62]);
    assert_eq!(bytes_to_wide("é".as_bytes()), vec![0xe9]);
    assert_eq!(wide_to_bytes(&[0x61, 0xe9]), "aé".as_bytes());
    assert_eq!(wide_to_bytes(&[0x61, 0, 0x62]), b"a");
    assert_eq!(routing_op_type_name(0), "RoutingOp_Undefined");
    assert_eq!(routing_op_type_name(60), "RoutingOp_DerivedServerOp");
    assert_eq!(routing_op_type_name(61), "Unknown Routing Type");
    assert_eq!(routing_op_type_name(-1), "Unknown Routing Type");
}

#[test]
fn leave_groups_and_clear_games_send_their_requests_only_with_a_connection() {
    let (_, ops) = with_mock(true, 0, 0, |n| {
        n.join_room_id.set(5);
        n.game_list_port.set(6);
        n.join_group_id.set(7);
        n.game_group_id.set(8);
        n.leave_groups();
        n.clear_games();
        assert_eq!(
            (n.join_room_id.get(), n.game_list_port.get(), n.join_group_id.get(), n.game_group_id.get()),
            (NONE, NONE, NONE, NONE)
        );
    });
    let reqs: Vec<_> =
        ops.iter().filter(|o| matches!(o.0, Op::ReqCancelJoinGroup | Op::ReqLeaveGroup)).map(|o| (o.0, o.1)).collect();
    assert_eq!(
        reqs,
        vec![(Op::ReqCancelJoinGroup, 5), (Op::ReqLeaveGroup, 6), (Op::ReqCancelJoinGroup, 7), (Op::ReqLeaveGroup, 8)]
    );
    let (_, ops) = with_mock(false, 0, 0, |n| {
        n.join_room_id.set(5);
        n.leave_groups();
        assert_eq!(n.join_room_id.get(), NONE);
    });
    assert!(ops.is_empty());
}

#[test]
fn a_join_answer_for_another_group_is_left() {
    let (_, ops) = with_mock(true, WS_Success, 9, |n| {
        n.join_group_id.set(3);
        n.join_group_callback(1);
        assert_eq!(n.join_group_id.get(), 3);
    });
    assert!(ops.iter().any(|o| o.0 == Op::ReqLeaveGroup && o.1 == 9));
    // the awaited group with a bad password: the message, the id cleared
    let (_, ops) = with_mock(true, WS_RoutingServG2_InvalidPassword, 3, |n| {
        n.join_group_id.set(3);
        n.join_group_callback(1);
        assert_eq!(n.join_group_id.get(), NONE);
    });
    assert!(ops.iter().any(|o| o.0 == Op::TextLoad && o.1 == IDS_PASSWORD_IS_INCORRECT));
    assert!(ops.iter().any(|o| o.0 == Op::ShowMessage));
}

#[test]
fn the_game_list_filters_its_rows() {
    let (_, ops) = with_mock(true, 0, 0, |n| {
        n.game_list_port.set(4);
        let mut g = GameInfo { name: b"open".to_vec(), flags: 1, room: 4, ..GameInfo::default() };
        n.games.borrow_mut().insert(1, g.clone());
        g.name = b"pw".to_vec();
        g.password = b"x".to_vec();
        n.games.borrow_mut().insert(2, g.clone());
        g.name = b"elsewhere".to_vec();
        g.room = 5;
        n.games.borrow_mut().insert(3, g);
        n.games_changed.set(true);
        // the games form is current (FormById answers 100 + id)
        MOCK.lock().unwrap().as_mut().unwrap().current_form = 100 + FORM_AVAILABLE_GAMES;
        n.update_game_list();
        assert!(!n.games_changed.get());
    });
    // the checkboxes answer 0: only the open game without a password shows between the markers
    let rows = ops.iter().filter(|o| o.0 == Op::NewWonGame).count();
    assert_eq!(rows, 3);
    assert_eq!(ops.iter().filter(|o| o.0 == Op::PostAction).count(), 3);
}

#[test]
fn the_room_list_counts_games_and_players_per_room() {
    let (_, ops) = with_mock(true, 0, 0, |n| {
        n.games.borrow_mut().insert(1, GameInfo { humans: 2, flags: 1, room: 7, ..GameInfo::default() });
        n.games.borrow_mut().insert(2, GameInfo { humans: 3, flags: 1, room: 7, ..GameInfo::default() });
        n.games.borrow_mut().insert(3, GameInfo { humans: 4, flags: 0, room: 7, ..GameInfo::default() });
        n.rooms
            .borrow_mut()
            .insert(7, RoomInfo { name: b"R".to_vec(), description: b"D".to_vec(), max_players: 10, id: 7 });
        n.update_room_list(1, 2, 3);
    });
    let get = |op: Op| ops.iter().find(|o| o.0 == op).map(|o| o.1).unwrap();
    assert_eq!(get(Op::RoomLinePlayers), 15);
    assert_eq!(get(Op::RoomLineGames), 2);
}

#[test]
fn chat_commands_pick_their_requests() {
    // plain text: the room
    let (_, ops) = with_mock(true, 0, 0, |n| n.send_chat(b"xxhello".to_vec()));
    assert!(ops.iter().any(|o| o.0 == Op::ReqSendChat && o.2 == NONE as isize));
    // /r without a whisper yet: nothing sent
    let (_, ops) = with_mock(true, 0, 0, |n| n.send_chat(b"xx/r hi".to_vec()));
    assert!(!ops.iter().any(|o| matches!(o.0, Op::ReqSendChat | Op::ReqSendChatTo)));
    // /r after one: whispered to the sender
    let (_, ops) = with_mock(true, 0, 0, |n| {
        n.reply_client_id.set(12);
        n.send_chat(b"xx/r hi".to_vec())
    });
    assert!(ops.iter().any(|o| o.0 == Op::ReqSendChatTo));
    // the command names
    let t = Uw::new(b"xx/i \"a b\" rest");
    let (name, end) = EeWonNet::command_name(&t);
    assert_eq!((name.bytes(), end), (b"a b".to_vec(), 9));
    let t = Uw::new(b"xx/t bob hi");
    let (name, end) = EeWonNet::command_name(&t);
    assert_eq!((name.bytes(), end), (b"bob".to_vec(), 8));
}
