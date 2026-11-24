//! The action over a recording host: each type's statements in the original's order, the refill brackets and the
//! statics between actions. (The golden against the original C++ is tests/won/won_action37a_test.cpp.)

use super::*;
use std::cell::RefCell;
use std::ffi::CString;

struct World {
    form: isize,
    lines: isize,
    display: isize,
    selection: Option<c_ulong>,
    bar: bool,
    max: isize,
    value: isize,
}

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    /// The world: the current form, a list's lines, its display line, the selection (or none), the scroll bar.
    static WORLD: RefCell<World> = const {
        RefCell::new(World { form: 0, lines: 0, display: 0, selection: None, bar: false, max: 0, value: 0 })
    };
    static TEXTS: RefCell<Vec<CString>> = const { RefCell::new(Vec::new()) };
}

unsafe fn s(p: isize) -> String {
    CStr::from_ptr(p as *const c_char).to_string_lossy().into_owned()
}

fn keep(text: String) -> isize {
    let t = CString::new(text).unwrap();
    let p = t.as_ptr() as isize;
    TEXTS.with(|x| x.borrow_mut().push(t));
    p
}

unsafe extern "C" fn host(op: EeWonActionOp, a: isize, b: isize) -> isize {
    use EeWonActionOp as Op;
    let (line, r) = WORLD.with(|w| {
        let w = w.borrow();
        match op {
            Op::CurrentFormIs => (format!("form is {a}"), (w.form == a) as isize),
            Op::CurrentControl => (format!("current control {a}"), 100 + a),
            Op::FormControl => (format!("form {a} control {b}"), 1000 * a + b),
            Op::TextNew => ("text new".into(), 77),
            Op::TextBytes => ("text bytes".into(), keep("players".into())),
            Op::TextDelete => ("text delete".into(), 0),
            Op::ResString => (format!("res {a} {b}"), 0),
            Op::LabelSetText => (format!("label {a} set {}", s(b)), 0),
            Op::LabelHasScrollBar => (format!("label {a} bar"), w.bar as isize),
            Op::LabelSliderMax => (format!("label {a} max"), w.max),
            Op::LabelSliderValue => (format!("label {a} value"), w.value),
            Op::LabelAppendText => (format!("label {a} append {}", s(b)), 0),
            Op::LabelSetLine => (format!("label {a} line {b}"), 0),
            Op::ListDisplayLine => (format!("list {a} display"), w.display),
            Op::ListDeleteAll => (format!("list {a} delete all"), 0),
            Op::ListLineCount => (format!("list {a} count"), w.lines),
            Op::ListSetLine => (format!("list {a} line {b}"), 0),
            Op::ListAppend => (format!("list {a} append {}", s(b)), 0),
            Op::GamesBox => ("games box".into(), 500),
            Op::GamesLineCount => ("games count".into(), w.lines),
            Op::GamesDisplayLine => ("games display".into(), w.display),
            Op::GamesSelection => match w.selection {
                Some(n) => {
                    *(b as *mut c_ulong) = n;
                    ("games selection".into(), 0)
                }
                None => ("games selection".into(), 1),
            },
            Op::GamesName => (format!("games name {b}"), keep(format!("game{b}"))),
            Op::GamesDeleteAll => ("games delete all".into(), 0),
            Op::GamesSetLine => (format!("games line {b}"), 0),
            Op::GamesSelect => (format!("games select {b}"), 0),
            Op::GamesAppend => {
                let l = &*(b as *const EeWonActionLine);
                let flags = format!("{}{}{}", l.has_password as u8, l.is_private as u8, l.is_in_progress as u8);
                (format!("games append {} {} {} {flags}", s(l.name as isize), l.humans, l.players), 0)
            }
            Op::RoomsBox => ("rooms box".into(), 600),
            Op::RoomsDeleteAll => ("rooms delete all".into(), 0),
            Op::RoomsAppend => {
                let l = &*(b as *const EeWonActionLine);
                let texts = format!("{}|{}", s(l.name as isize), s(l.description as isize));
                (format!("rooms append {texts}|{}|{}", l.humans, l.players), 0)
            }
        }
    });
    LOG.with(|l| l.borrow_mut().push(line));
    r
}

fn run(mut a: EeWonAction) -> Vec<String> {
    LOG.with(|l| l.borrow_mut().clear());
    unsafe { a.run(host) };
    LOG.with(|l| l.borrow().clone())
}

fn world(f: impl FnOnce(&mut World)) {
    WORLD.with(|w| f(&mut w.borrow_mut()));
}

fn statics(reset: bool) -> String {
    let mut out = [0 as c_char; 256];
    unsafe {
        ee_won_action_statics(out.as_mut_ptr(), out.len(), reset);
        CStr::from_ptr(out.as_ptr()).to_string_lossy().into_owned()
    }
}

fn game(text: &[u8]) -> EeWonAction {
    EeWonAction::new_game(3, text, 0, 0, false, false, false)
}

/// One test: the statics are the process's.
#[test]
fn the_action_runs_as_the_original() {
    statics(true);
    // a game line on another form does nothing but ask
    world(|w| w.form = 31);
    assert_eq!(run(EeWonAction::new_game(3, b"a", 1, 2, false, false, false)), ["form is 32"]);

    // a refill of the games box: the bracket keeps the display line and the selected name
    world(|w| {
        w.form = 32;
        w.lines = 3;
        w.display = 2;
        w.selection = Some(1);
    });
    assert_eq!(
        run(game(b"@@@@")),
        [
            "form is 32",
            "games box",
            "games count",
            "games display",
            "games selection",
            "games name 1",
            "games delete all"
        ]
    );
    assert_eq!(statics(false), "selected=game1 count=0 selection=0 games_line=2 list_line=0");
    assert_eq!(
        run(EeWonAction::new_game(3, b"game0", 1, 2, true, false, true)),
        ["form is 32", "games box", "games append game0 1 2 101"]
    );
    assert_eq!(
        run(EeWonAction::new_game(3, b"game1", 3, 4, false, true, false)),
        ["form is 32", "games box", "games append game1 3 4 010"]
    );
    assert_eq!(statics(false), "selected=game1 count=2 selection=1 games_line=2 list_line=0");
    assert_eq!(run(game(b"@@@")), ["form is 32", "games box", "games count", "games line 2", "games select 1"]);
    assert_eq!(statics(false), "selected=game1 count=0 selection=0 games_line=2 list_line=0");

    // no selection: the name is cleared; an empty box: nothing kept, every line appended selects line 0
    world(|w| w.selection = None);
    assert_eq!(
        run(game(b"@@@@")),
        ["form is 32", "games box", "games count", "games display", "games selection", "games delete all"]
    );
    world(|w| w.lines = 0);
    assert_eq!(run(game(b"@@@@")), ["form is 32", "games box", "games count"]);
    assert_eq!(run(game(b"x")), ["form is 32", "games box", "games append x 0 0 000", "games select 0"]);
    assert_eq!(run(game(b"@@@")), ["form is 32", "games box", "games count"]);

    // the player list
    world(|w| {
        w.lines = 5;
        w.display = 4;
    });
    assert_eq!(
        run(EeWonAction::new_list(4, 0, b"@@@@", false)),
        ["form is 32", "current control 8", "list 108 display", "list 108 delete all"]
    );
    assert_eq!(
        run(EeWonAction::new_list(4, 0, b"bob", false)),
        ["form is 32", "current control 8", "list 108 append bob"]
    );
    assert_eq!(
        run(EeWonAction::new_list(4, 0, b"@@@", false)),
        ["form is 32", "current control 8", "list 108 count", "list 108 line 4"]
    );
    world(|w| w.form = 26);
    assert_eq!(run(EeWonAction::new_list(4, 0, b"bob", false)), ["form is 32"]);

    // chat: a reader scrolled back is left alone, their own line scrolls to the end
    world(|w| {
        w.bar = true;
        w.max = 9;
        w.value = 3;
    });
    assert_eq!(
        run(EeWonAction::new_chat(6, 15, b"al: ", b"hi", false)),
        ["label 15 bar", "label 15 max", "label 15 value", "label 15 append al: hi"]
    );
    assert_eq!(
        run(EeWonAction::new_chat(6, 15, b"me: ", b"hi", true)),
        ["label 15 append me: hi", "label 15 bar", "label 15 max", "label 15 line 9"]
    );
    world(|w| w.value = 9);
    assert_eq!(
        run(EeWonAction::new_chat(7, 0, b"al: ", b"yo", false)),
        [
            "form is 26",
            "current control 8",
            "label 108 bar",
            "label 108 max",
            "label 108 value",
            "label 108 append al: yo",
            "label 108 bar",
            "label 108 max",
            "label 108 line 9"
        ]
    );
    world(|w| w.bar = false);
    assert_eq!(
        run(EeWonAction::new_chat(6, 15, b"", b"z", false)),
        ["label 15 bar", "label 15 append z", "label 15 bar"]
    );
    world(|w| w.form = 32);
    assert_eq!(run(EeWonAction::new_chat(7, 0, b"", b"z", false)), ["form is 26"]);

    // the room list's types (no constructor makes them; the members set by hand)
    world(|w| w.form = 31);
    let mut a = EeWonAction::new_list(8, 0, b"", false);
    a.server_name = b"srv".to_vec();
    a.l8c = 7;
    a.players = -2;
    assert_eq!(
        run(a),
        [
            "form is 31",
            "text new",
            "res 7206 77",
            "text bytes",
            "text delete",
            "form 31 control 14",
            "label 31014 set 7  (-2 players)",
            "form 31 control 16",
            "label 31016 set srv"
        ]
    );
    assert_eq!(run(EeWonAction::new_list(2, 0, b"d", false)), ["form is 31", "rooms box", "rooms delete all"]);
    let mut a = EeWonAction::new_list(2, 0, b"desc", false);
    a.room_name = b"room".to_vec();
    a.humans = 3;
    a.l8c = 4;
    assert_eq!(run(a), ["form is 31", "rooms box", "rooms append room|desc|3|4"]);
    assert_eq!(run(EeWonAction::new_list(5, 0, b"x", false)), Vec::<String>::new());
    statics(true);
}

#[test]
fn describes_itself() {
    assert_eq!(
        EeWonAction::new_game(3, b"g", 1, 2, true, false, true).describe(),
        "3 list=0 chat=0 prefix= text=g humans=1 players=2 flags=1010"
    );
    assert_eq!(
        EeWonAction::new_chat(6, 9, b"p", b"t", true).describe(),
        "6 list=0 chat=1 prefix=p text=t humans=0 players=0 flags=0100"
    );
}
