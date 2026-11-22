//! The WON lobby's walk (TEST ONLY; game core round 10, from EETestFixture.cpp). The lobby owns the UI thread while it
//! is open and runs its own message loop, so its state is read and its controls are driven on that loop, by a
//! UserEvent the fixture posts ([`EeFixtureOp::FxLobbyPostWork`]; WindowManager::PostEvent is thread-safe): nothing
//! of the lobby's is touched from the fixture's thread. The snapshot (ui-state.txt's lobby lines) is
//!   "lobby shown <0|1> screen <t> lan <0|1> login <0|1> modal <0|1> game <0|1> host <0|1> clients <n> ready <0|1>
//!    allready <0|1> name <text>",
//!   "wonctl <controlId> <x> <y> <w> <h> <enabled> <text>" per visible component with a control id (root
//!   coordinates of the lobby window; a button's or label's text),
//!   "wonlist <k> items <n> sel <i>" per visible list area, in tree order, and "wontext <text>" per label of the
//!   modal dialog (a message box's message).
//! The commands: "<seq> wonclick <controlId> 0" (a mouse move, press and release at the component's centre through
//! the window's event queue; the modal dialog's components are searched first), "<seq> wonactivate <controlId> 0"
//! (Button::Activate), "<seq> wonselect <k> <item>" (the k-th list area's selection), "<seq> wonkey <code> 0" (one
//! typed character to the focus), "<seq> wonclickat <x> <y>" (a click at root coordinates). The verdict ("TEST UI
//! lobby-...") is printed by the lobby thread when the command ran.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

use super::host::{self, Call, EeFixtureOp as Op, EeFixtureSink};

struct Pending {
    snapshot: String, // the last snapshot the lobby thread wrote
    command: String,  // a pending "<op> <a> <b>" for the lobby thread (empty: none)
    seq: u64,
}

static STATE: Mutex<Pending> = Mutex::new(Pending { snapshot: String::new(), command: String::new(), seq: 0 });
/// A UserEvent is queued and not yet delivered.
static POSTED: AtomicBool = AtomicBool::new(false);

fn go(op: Op, h: u64, i: &[i64]) -> i64 {
    Call::new(op).h(h).i(i).go()
}

fn handle(op: Op, h: u64) -> u64 {
    go(op, h, &[]) as u64
}

/// The fixture's view of a GUI text: at most 60 characters, a space, a control character, a non-ASCII one or '|'
/// as '_', "-" when empty.
pub fn lobby_gui_text(chars: impl Iterator<Item = u16>) -> String {
    let text: String =
        chars.take(60).map(|c| if !(0x21..0x7f).contains(&c) || c == b'|' as u16 { '_' } else { c as u8 as char }).collect();
    if text.is_empty() {
        "-".into()
    } else {
        text
    }
}

/// A text through its length and character operations.
fn gui_text(length: Op, at: Op, h: u64) -> String {
    let n = go(length, h, &[]);
    lobby_gui_text((0..n).map(|k| go(at, h, &[k]) as u16))
}

/// `sscanf("%31s %ld %ld")`: the operation and up to two numbers (0 where one is missing or not a number).
pub fn lobby_command(command: &str) -> (String, i64, i64) {
    let mut words = command.split_ascii_whitespace();
    let op: String = words.next().unwrap_or("").chars().take(31).collect();
    let number = |w: Option<&str>| -> Option<i64> {
        let w = w?;
        let digits = w.strip_prefix(['-', '+']).unwrap_or(w);
        let end = digits.find(|c: char| !c.is_ascii_digit()).unwrap_or(digits.len());
        let value: i64 = digits[..end].parse().ok()?;
        Some(if w.starts_with('-') { -value } else { value })
    };
    let a = number(words.next());
    let b = if a.is_some() { number(words.next()) } else { None };
    (op, a.unwrap_or(0), b.unwrap_or(0))
}

/// The visible components with a control id and the list areas below `component`, in tree order (`modal`: the
/// modal dialog's component; `in_modal`: the walk is inside it).
fn walk(component: u64, modal: u64, in_modal: bool, text: &mut String, lists: &mut Vec<u64>) {
    if component == 0 || go(Op::FxCompVisible, component, &[]) == 0 {
        return;
    }
    let in_modal = in_modal || component == modal;
    let id = go(Op::FxCompControlId, component, &[]);
    if in_modal && id == 0 {
        // the modal dialog's texts (a message box's message)
        let label = handle(Op::FxAsLabel, component);
        if label != 0 && go(Op::FxLabelHasText, label, &[]) != 0 && go(Op::FxLabelTextLen, label, &[]) > 0 {
            text.push_str(&format!("wontext {}\n", gui_text(Op::FxLabelTextLen, Op::FxLabelTextAt, label)));
        }
    }
    if id != 0 {
        let at = Call::new(Op::FxCompLocalToRoot).h(component).run();
        let (x, y) = (at.i(0) as i32, at.i(1) as i32);
        let button = handle(Op::FxAsButton, component);
        let label = if button == 0 { handle(Op::FxAsLabel, component) } else { 0 };
        let shown = if button != 0 {
            gui_text(Op::FxButtonTextLen, Op::FxButtonTextAt, button)
        } else if label != 0 && go(Op::FxLabelHasText, label, &[]) != 0 {
            gui_text(Op::FxLabelTextLen, Op::FxLabelTextAt, label)
        } else {
            "-".into()
        };
        text.push_str(&format!(
            "wonctl {} {} {} {} {} {} {}\n",
            id as i32,
            x,
            y,
            go(Op::FxCompWidth, component, &[]) as i32,
            go(Op::FxCompHeight, component, &[]) as i32,
            if go(Op::FxCompDisabled, component, &[]) != 0 { 0 } else { 1 },
            shown
        ));
    }
    let list = handle(Op::FxAsListArea, component);
    if list != 0 {
        let selected = handle(Op::FxListSelItem, list);
        let position = if selected != 0 { Call::new(Op::FxListItemPos).h(list).h2(selected).go() as i32 } else { -1 };
        text.push_str(&format!(
            "wonlist {} items {} sel {}\n",
            lists.len(),
            go(Op::FxListItemCount, list, &[]) as i32,
            position
        ));
        lists.push(list);
    }
    let container = handle(Op::FxAsContainer, component);
    if container != 0 {
        let mut child = handle(Op::FxContainerFront, container);
        while child != 0 {
            walk(child, modal, in_modal, text, lists);
            child = handle(Op::FxCompNextSibling, child);
        }
    }
}

/// The first visible, enabled component with control id `id` below `component`.
fn find_by_id(component: u64, id: i64) -> u64 {
    if component == 0 || go(Op::FxCompVisible, component, &[]) == 0 {
        return 0;
    }
    if go(Op::FxCompControlId, component, &[]) == id && go(Op::FxCompDisabled, component, &[]) == 0 {
        return component;
    }
    let container = handle(Op::FxAsContainer, component);
    if container != 0 {
        let mut child = handle(Op::FxContainerFront, container);
        while child != 0 {
            let found = find_by_id(child, id);
            if found != 0 {
                return found;
            }
            child = handle(Op::FxCompNextSibling, child);
        }
    }
    0
}

/// A click (a mouse move, press and release) at root coordinates of a lobby window.
fn click(window: u64, x: i64, y: i64) {
    for kind in 0..3 {
        go(Op::FxWindowMouse, window, &[kind, x, y]);
    }
}

/// The command's verdict.
fn run_command(command: &str, root: u64, manager: u64, window: u64, modal: u64) -> &'static str {
    let (op, a, b) = lobby_command(command);
    let scope = if manager != 0 && modal != 0 { modal } else { root };
    match op.as_str() {
        "wonclick" | "wonactivate" => {
            let mut target = find_by_id(scope, a);
            if target == 0 {
                target = find_by_id(root, a);
            }
            if target == 0 {
                return "lobby-rejected";
            }
            if op == "wonclick" {
                let at = Call::new(Op::FxCompLocalToRoot)
                    .h(target)
                    .i(&[
                        (go(Op::FxCompWidth, target, &[]) as i32 / 2) as i64,
                        (go(Op::FxCompHeight, target, &[]) as i32 / 2) as i64,
                    ])
                    .run();
                let target_window = handle(Op::FxCompRootWindow, target);
                if target_window != 0 {
                    click(target_window, at.i(0) as i32 as i64, at.i(1) as i32 as i64);
                    return "lobby-clicked";
                }
            } else {
                let button = handle(Op::FxAsButton, target);
                if button != 0 {
                    go(Op::FxButtonActivate, button, &[]);
                    return "lobby-activated";
                }
            }
            "lobby-rejected"
        }
        "wonclickat" if window != 0 => {
            click(window, a as i32 as i64, b as i32 as i64);
            "lobby-clicked"
        }
        "wonkey" if window != 0 && a > 0 => {
            go(Op::FxWindowKeyChar, window, &[a as i32 as i64]);
            "lobby-typed"
        }
        "wonselect" => {
            let mut unused = String::new();
            let mut lists = Vec::new();
            walk(root, 0, false, &mut unused, &mut lists);
            if a >= 0 && (a as usize) < lists.len() && b >= 0 && b < go(Op::FxListItemCount, lists[a as usize], &[]) {
                go(Op::FxListSetSel, lists[a as usize], &[b]);
                return "lobby-selected";
            }
            "lobby-rejected"
        }
        _ => "lobby-rejected",
    }
}

/// On the lobby thread (a delivered UserEvent): runs the pending command, then takes the snapshot.
pub fn work() {
    let (command, seq) = {
        let mut s = STATE.lock().unwrap_or_else(|e| e.into_inner());
        (std::mem::take(&mut s.command), s.seq)
    };
    let window = handle(Op::FxLobbyRootWindow, 0);
    let manager = handle(Op::FxLobbyManager, 0);
    let root = if window != 0 { handle(Op::FxWindowRoot, window) } else { 0 };
    let modal = |m: u64| if m != 0 { handle(Op::FxManagerModal, m) } else { 0 };
    if !command.is_empty() {
        let verdict = run_command(&command, root, manager, window, modal(manager));
        let (op, a, b) = lobby_command(&command);
        host::stderr(&format!("TEST UI {verdict} {op} control {a} value {b} on form 0 (seq {seq}, top form 0)\n"));
    }
    let game = handle(Op::FxMyGame, 0);
    let client = handle(Op::FxMyClient, 0);
    let modal_now = modal(manager);
    let flag = |op: Op, h: u64| if h != 0 && go(op, h, &[]) != 0 { 1 } else { 0 };
    let mut text = format!(
        "lobby shown {} screen {} lan {} login {} modal {} game {} host {} clients {} ready {} allready {} name {}\n",
        flag(Op::FxLobbyShown, 1),
        go(Op::FxLobbyScreenType, 0, &[]) as i32,
        flag(Op::FxLobbyOnLan, 1),
        flag(Op::FxLobbyOnLogin, 1),
        if modal_now != 0 { 1 } else { 0 },
        if game != 0 { 1 } else { 0 },
        flag(Op::FxGameIsHost, game),
        if game != 0 { go(Op::FxGameClientCount, game, &[]) as i32 } else { 0 },
        flag(Op::FxClientReady, client),
        flag(Op::FxGameAllReady, game),
        if game != 0 { gui_text(Op::FxGameNameLen, Op::FxGameNameAt, game) } else { "-".into() }
    );
    let mut lists = Vec::new();
    walk(root, modal_now, false, &mut text, &mut lists);
    STATE.lock().unwrap_or_else(|e| e.into_inner()).snapshot = text;
    POSTED.store(false, Ordering::SeqCst);
}

/// Queues the lobby work on the lobby's message loop when none is queued (any thread).
fn post_work() {
    let manager = if go(Op::FxLobbyOpen, 0, &[]) != 0 { handle(Op::FxLobbyManager, 0) } else { 0 };
    if manager != 0 && !POSTED.swap(true, Ordering::SeqCst) {
        go(Op::FxLobbyPostWork, manager, &[]);
    }
}

/// [`Op::LobbyPost`]: the command `s` with sequence number i0 for the lobby thread -> 1, 0 the lobby is not open.
pub(super) fn post(command: &str, seq: u64) -> i64 {
    if go(Op::FxLobbyOpen, 0, &[]) == 0 {
        return 0;
    }
    {
        let mut s = STATE.lock().unwrap_or_else(|e| e.into_inner());
        s.command = command.to_string();
        s.seq = seq;
    }
    post_work();
    1
}

/// [`Op::LobbySnapshot`]: the last snapshot the lobby's own thread took into the sink, and a new one asked for -> 1,
/// 0 the lobby is not open.
pub(super) fn snapshot(sink: &mut EeFixtureSink) -> i64 {
    if go(Op::FxLobbyOpen, 0, &[]) == 0 {
        return 0;
    }
    let text = STATE.lock().unwrap_or_else(|e| e.into_inner()).snapshot.clone();
    sink.texts.push(text.into_bytes());
    post_work();
    1
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn gui_texts_are_ascii_without_spaces_or_bars_and_at_most_sixty() {
        let text = |s: &str| lobby_gui_text(s.encode_utf16());
        assert_eq!(text(""), "-");
        assert_eq!(text("Join Game"), "Join_Game");
        assert_eq!(text("a|b\u{e9}"), "a_b_");
        assert_eq!(text(&"x".repeat(70)).len(), 60);
    }

    #[test]
    fn commands_parse_as_sscanf_does() {
        assert_eq!(lobby_command("wonclick 12 0"), ("wonclick".into(), 12, 0));
        assert_eq!(lobby_command("wonclickat -5 7x"), ("wonclickat".into(), -5, 7));
        assert_eq!(lobby_command("wonkey"), ("wonkey".into(), 0, 0));
        assert_eq!(lobby_command("wonselect x 3"), ("wonselect".into(), 0, 0));
        assert_eq!(lobby_command(&format!("{} 1 2", "o".repeat(40))).0.len(), 31);
    }
}
