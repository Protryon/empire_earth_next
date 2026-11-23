use super::*;
use std::cell::RefCell;
use std::sync::Mutex;

/// The host is global: one test at a time.
static LOCK: Mutex<()> = Mutex::new(());

/// The test host's answer.
type Answer = Box<dyn Fn(Op, isize, isize, isize) -> isize>;

thread_local! {
    static TRACE: RefCell<Vec<(Op, isize, isize, isize)>> = const { RefCell::new(Vec::new()) };
    static ANSWER: RefCell<Option<Answer>> = const { RefCell::new(None) };
}

unsafe extern "C" fn test_host(op: Op, a: isize, b: isize, c: isize) -> isize {
    TRACE.with(|t| t.borrow_mut().push((op, a, b, c)));
    ANSWER.with(|f| f.borrow().as_ref().map_or(0, |f| f(op, a, b, c)))
}

fn run(
    answer: impl Fn(Op, isize, isize, isize) -> isize + 'static,
    f: impl FnOnce(),
) -> Vec<(Op, isize, isize, isize)> {
    ANSWER.with(|a| *a.borrow_mut() = Some(Box::new(answer)));
    TRACE.with(|t| t.borrow_mut().clear());
    ee_won_front_set_host(Some(test_host));
    f();
    ee_won_front_set_host(None);
    TRACE.with(|t| t.borrow_mut().drain(..).collect())
}

fn ops(trace: &[(Op, isize, isize, isize)]) -> Vec<Op> {
    trace.iter().map(|t| t.0).collect()
}

fn header(name: &str) -> String {
    let p = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../include").join(name);
    std::fs::read_to_string(p).unwrap_or_default()
}

fn value_in(text: &str, name: &str) -> Option<isize> {
    let at = text.find(&format!("{name} "))?;
    let rest = text[at + name.len()..].trim_start();
    let rest = rest.strip_prefix('=').unwrap_or(rest).trim_start();
    let end = rest.find(|c: char| !c.is_ascii_alphanumeric()).unwrap_or(rest.len());
    let v = &rest[..end];
    v.strip_prefix("0x").map_or_else(|| v.parse().ok(), |h| isize::from_str_radix(h, 16).ok())
}

#[test]
fn the_constants_are_the_headers() {
    let ui = header("ee/EEUIManager.h");
    let ids = header("ee/EEStringIDs.h");
    let control = header("lle/UIControl.h");
    if ui.is_empty() || ids.is_empty() {
        return; // a checkout without the headers (the crate alone)
    }
    let checks: &[(&str, &str, isize)] = &[
        (&ui, "kFormAvailableGames", WF_FORM_AVAILABLE_GAMES),
        (&ui, "kFormUpdateSierraAccount", WF_FORM_UPDATE_SIERRA_ACCOUNT),
        (&ui, "kAvailableGamesPlayersNameLabel", WF_AG_PLAYERS_NAME_LABEL),
        (&ui, "kAvailableGamesRoomsNameLabel", WF_AG_ROOMS_NAME_LABEL),
        (&ui, "kAvailableGamesJoinGameTab", WF_AG_JOIN_GAME_TAB),
        (&ui, "kAvailableGamesHostGameTab", WF_AG_HOST_GAME_TAB),
        (&ui, "kAvailableGamesPlayersInRoomListBox", WF_AG_PLAYERS_IN_ROOM_LIST_BOX),
        (&ui, "kAvailableGamesAvalGamesListBox", WF_AG_AVAL_GAMES_LIST_BOX),
        (&ui, "kAvailableGamesChatEditBox", WF_AG_CHAT_EDIT_BOX),
        (&ui, "kAvailableGamesJoinHostGameButton", WF_AG_JOIN_HOST_GAME_BUTTON),
        (&ui, "kAvailableGamesBackButton", WF_AG_BACK_BUTTON),
        (&ui, "kAvailableGamesHostGameNameEditBox", WF_AG_HOST_GAME_NAME_EDIT_BOX),
        (&ui, "kAvailableGamesPWForHostedGameEditBox", WF_AG_PW_EDIT_BOX),
        (&ui, "kAvailableGamesPWForHostedGameCheckbox", WF_AG_PW_CHECKBOX),
        (&ui, "kAvailableGamesPWRetypeForHostedGameEditBox", WF_AG_PW_RETYPE_EDIT_BOX),
        (&ui, "kAvailableGamesPWForHostedGameLabel", WF_AG_PW_LABEL),
        (&ui, "kAvailableGamesPWRetypeForHostedGameLabel", WF_AG_PW_RETYPE_LABEL),
        (&ui, "kAvailableGamesInProgressToggle", WF_AG_IN_PROGRESS_TOGGLE),
        (&ui, "kAvailableGamesPasswordToggle", WF_AG_PASSWORD_TOGGLE),
        (&ui, "kAvailableGamesPrivateToggle", WF_AG_PRIVATE_TOGGLE),
        (&ui, "kAvailableRoomsPlayersNameLabel", WF_AR_PLAYERS_NAME_LABEL),
        (&ui, "kAvailableRoomsAvailRoomsListBox", WF_AR_AVAIL_ROOMS_LIST_BOX),
        (&ui, "kAvailableRoomsJoinRoomButton", WF_AR_JOIN_ROOM_BUTTON),
        (&ui, "kAvailableRoomsBackButton", WF_AR_BACK_BUTTON),
        (&ui, "kAvailableRoomsUpdateMyAccount", WF_AR_UPDATE_MY_ACCOUNT),
        (&ui, "kAvailableRoomsTotalGamesNumber", WF_AR_TOTAL_GAMES_NUMBER),
        (&ui, "kAvailableRoomsServersNameLabel", WF_AR_SERVERS_NAME_LABEL),
        (&ui, "kUpdateAccountNewPasswordEditBox", WF_UA_NEW_PASSWORD_EDIT_BOX),
        (&ui, "kUpdateAccountConfirmPasswordEditBox", WF_UA_CONFIRM_PASSWORD_EDIT_BOX),
        (&ui, "kUpdateAccountEnterEmailEditBox", WF_UA_ENTER_EMAIL_EDIT_BOX),
        (&ui, "kUpdateAccountSendMeEmailCheckbox", WF_UA_SEND_ME_EMAIL_CHECKBOX),
        (&ui, "kUpdateAccountUpdateButton", WF_UA_UPDATE_BUTTON),
        (&ui, "kUpdateAccountDontUpdateButton", WF_UA_DONT_UPDATE_BUTTON),
        (&ui, "kSelectServerListBox", WF_SS_LIST_BOX),
        (&ui, "kSelectServerOKButton", WF_SS_OK_BUTTON),
        (&ui, "kSelectServerCancelButton", WF_SS_CANCEL_BUTTON),
        (&ui, "kWonStatsStatBox", WF_WS_STAT_BOX),
        (&ui, "kWonStatsMyStatsLine", WF_WS_MY_STATS_LINE),
        (&ids, "IDS_GAME_NAME_IS_INVALID", WF_IDS_GAME_NAME_IS_INVALID),
        (&ids, "IDS_LOBBY_PASSWORD", WF_IDS_LOBBY_PASSWORD),
        (&ids, "IDS_HOST", WF_IDS_HOST),
        (&ids, "IDS_JOIN", WF_IDS_JOIN),
        (&ids, "IDS_YOUR_TWO_PASSWORDS_DONT_MATCH", WF_IDS_YOUR_TWO_PASSWORDS_DONT_MATCH),
        (&ids, "IDS_CIV_POWER_PASSWORD", WF_IDS_CIV_POWER_PASSWORD),
        (&ids, "IDS_PRIVATE", WF_IDS_PRIVATE),
        (&ids, "IDS_IN_PROGRESS", WF_IDS_IN_PROGRESS),
        (&ids, "IDS_YOU_CANT_JOIN_THAT_GAME", WF_IDS_YOU_CANT_JOIN_THAT_GAME),
        (&control, "kControlTriggerChecked", WF_TRIGGER_CHECKED),
        (&control, "kControlTriggerUnchecked", WF_TRIGGER_UNCHECKED),
        (&control, "kControlTriggerDoubleClicked", WF_TRIGGER_DOUBLE_CLICKED),
        (&control, "kControlTriggerButtonUp", WF_TRIGGER_BUTTON_UP),
        (&control, "kControlTriggerEnterPressed", WF_TRIGGER_ENTER_PRESSED),
    ];
    for (text, name, v) in checks {
        if text.is_empty() {
            continue;
        }
        assert_eq!(value_in(text, name), Some(*v), "{name}");
    }
}

#[test]
fn host_or_join_waits_five_seconds_and_an_in_progress_game_lifts_the_wait() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    ee_won_front_reset();
    // the join tab, a selected game in progress
    let answer = |t: u32| {
        move |op: Op, _a: isize, b: isize, _c: isize| match op {
            Op::Time => t as isize,
            Op::GamesSelection => {
                // SAFETY: the op's out argument
                unsafe { *(b as *mut c_ulong) = 1 };
                0
            }
            Op::GamesInProgress => 1,
            _ => 0,
        }
    };
    let first = run(answer(6000), || {
        ee_won_front_call(EeWonFrontCall::AgHostOrJoinGame, std::ptr::null_mut(), 0);
    });
    assert!(ops(&first).contains(&Op::DisplayMessage));
    // the in-progress message reset the time: at once again
    let second = run(answer(6001), || {
        ee_won_front_call(EeWonFrontCall::AgHostOrJoinGame, std::ptr::null_mut(), 0);
    });
    assert!(ops(&second).contains(&Op::DisplayMessage));
    ee_won_front_reset();
}

#[test]
fn the_join_room_wait() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    ee_won_front_reset();
    let at = |t: isize| move |op: Op, _: isize, _: isize, _: isize| if op == Op::Time { t } else { 0 };
    let a = run(at(5001), || {
        ee_won_front_call(EeWonFrontCall::ArJoinRoom, std::ptr::null_mut(), 0);
    });
    assert!(ops(&a).contains(&Op::RoomsSelection));
    let b = run(at(9000), || {
        ee_won_front_call(EeWonFrontCall::ArJoinRoom, std::ptr::null_mut(), 0);
    });
    assert!(!ops(&b).contains(&Op::RoomsSelection));
    ee_won_front_reset();
}

#[test]
fn the_stat_numbers() {
    assert_eq!(percent(324, 464), b"69".to_vec());
    assert_eq!(percent(500, 464), b"100".to_vec());
    assert_eq!(ld(-6isize as c_ulong as isize), b"-6".to_vec());
    // no games: NaN, the integer indefinite
    assert_eq!(percent(0, 0), c_long::MIN.to_string().into_bytes());
    assert_eq!(to_long(150.9), 150);
    assert_eq!(to_long(-0.5), 0);
    assert_eq!(long_f(221.75), 221.0);
}

#[test]
fn the_games_box_draws_six_columns_the_third_with_the_seconds_rectangle() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let trace = run(
        |op, _a, b, c| match op {
            Op::GamesDrawCount => 1,
            Op::GamesLineHeight => 10,
            Op::GamesCopyRect => {
                // SAFETY: the op's out argument
                unsafe {
                    *(b as *mut EeWonFrontRect) = EeWonFrontRect { bottom: 0.0, left: 5.0, right: 0.0, top: 20.0 }
                };
                0
            }
            Op::GamesLineGet if c == Fd::Players as isize => 4,
            Op::GamesLineGet if c == Fd::Humans as isize => 2,
            Op::GamesWidthGet => {
                // SAFETY: the op's out argument
                unsafe { *(c as *mut f32) = 30.0 };
                0
            }
            _ => 0,
        },
        || {
            ee_won_front_call(EeWonFrontCall::GamesDrawText, std::ptr::null_mut(), 0);
        },
    );
    let draws: Vec<_> = trace.iter().filter(|t| t.0 == Op::GamesDrawClipped).collect();
    assert_eq!(draws.len(), 6);
    assert!(ops(&trace).contains(&Op::TextFontBold));
}

#[test]
fn the_server_list_selects_a_lone_server() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let trace = run(
        |op, _, _, _| match op {
            Op::ServerCount => 1,
            _ => 0,
        },
        || {
            ee_won_front_call(EeWonFrontCall::SsOpenScreen, std::ptr::null_mut(), 0);
        },
    );
    let o = ops(&trace);
    assert!(o.contains(&Op::ServerName) && !o.contains(&Op::ServerDisplayName));
    assert_eq!(o.last(), Some(&Op::ListSelectIndex));
}
