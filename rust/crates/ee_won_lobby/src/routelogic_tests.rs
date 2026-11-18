use super::*;
use std::cell::RefCell;
use std::collections::BTreeMap;

// ---- a scripted host for the unit tests: answers from a closure, every op recorded

type Answer = Box<dyn Fn(Op, [isize; 5]) -> isize>;

thread_local! {
    static SCRIPT: RefCell<Option<Answer>> = const { RefCell::new(None) };
    static MADE: RefCell<Vec<(Op, [isize; 5])>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn script_host(_obj: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize, e: isize) -> isize {
    MADE.with(|m| m.borrow_mut().push((op, [a, b, c, d, e])));
    SCRIPT.with(|s| s.borrow().as_ref().map_or(0, |f| f(op, [a, b, c, d, e])))
}

fn with_script(f: Answer) -> EeWonRouteLogic {
    SCRIPT.with(|s| *s.borrow_mut() = Some(f));
    MADE.with(|m| m.borrow_mut().clear());
    EeWonRouteLogic::new(std::ptr::null_mut(), script_host)
}

fn made(op: Op) -> Vec<[isize; 5]> {
    MADE.with(|m| m.borrow().iter().filter(|(o, _)| *o == op).map(|(_, v)| *v).collect())
}

fn nexts() -> isize {
    MADE.with(|m| m.borrow().iter().filter(|(o, x)| *o == Op::GameList && x[0] == 4).count() as isize)
}

#[test]
fn a_new_logic_holds_nothing() {
    let lg = with_script(Box::new(|_, _| 0));
    for i in 0..10 {
        assert_eq!(lg.call(Call::Field, i, 0), 0, "field {i}");
    }
    lg.release_all();
    assert!(MADE.with(|m| m.borrow().is_empty()), "nothing held, nothing given back");
}

#[test]
fn quick_play_ranks_are_cut_to_16_bits() {
    // the original's multimap<unsigned short, LobbyGame*>: a rank of 30000 + a ping of 40000 wraps below a rank of
    // 20000, and the first of equal ranks wins
    let lg = with_script(Box::new(|op, v| match op {
        Op::ConfigPingCeiling => 0xf000,
        Op::GameList if v[0] == 3 => (nexts() < 3) as isize,
        Op::GameList if v[0] == 4 => 0x5000 + 0x10 * nexts(),
        Op::GameNum => match (v[0], v[1]) {
            (_, GM_CAN_JOIN) => 1,
            (0x5010, GM_PING) => 100,
            (0x5010, GM_SKILL) => 4, // one below the wanted 5: 20000 + 100
            (0x5020, GM_PING) => 40000,
            (0x5020, GM_SKILL) => 6, // one above: 30000 + 40000 = 70000, as 16 bits 4464
            (0x5030, GM_PING) => 40000,
            (0x5030, GM_SKILL) => 6, // the same rank again: the first stays
            _ => 0,
        },
        _ => 0,
    }));
    assert_eq!(lg.select_game(5, 0xffff), 0x5020);
    assert!(made(Op::PingGame).is_empty());
}

#[test]
fn a_moved_reference_is_released_before_the_new_one_is_taken() {
    let lg = with_script(Box::new(|_, _| 0));
    lg.assign(&lg.current, 0x1000);
    lg.assign(&lg.current, 0x1000);
    lg.assign(&lg.current, 0x1010);
    let ops: Vec<(Op, isize)> = MADE.with(|m| m.borrow().iter().map(|(o, v)| (*o, v[0])).collect());
    assert_eq!(ops, [(Op::AddRef, 0x1000), (Op::Release, 0x1000), (Op::AddRef, 0x1010)]);
    lg.release_all();
}

// ---- the replay of golden_routelogic.txt (won_routelogic_test --print): the calls run on the Rust object, every op
// it makes must be the recorded one, and its answer is the recorded one.

#[derive(Default)]
struct Replay {
    lines: Vec<String>,
    at: usize,
    refs: BTreeMap<isize, i32>,
    depth: i32,
    logic: usize,
}

thread_local! {
    static R: RefCell<Replay> = RefCell::new(Replay::default());
}

fn peek() -> String {
    R.with(|r| {
        let r = r.borrow();
        r.lines.get(r.at).cloned().unwrap_or_default()
    })
}

/// The next recorded line, which must start with `head`; the text after " = ".
fn expect(head: &str) -> String {
    R.with(|r| {
        let mut r = r.borrow_mut();
        let line = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        assert!(line.starts_with(head), "line {}: expected '{line}', made '{head}'", r.at);
        line.split_once(" = ").map(|x| x.1.to_string()).unwrap_or_default()
    })
}

fn add_ref(h: isize, by: i32) {
    R.with(|r| *r.borrow_mut().refs.entry(h).or_default() += by);
}

fn numbers(s: &str) -> Vec<i64> {
    s.split(' ').filter(|t| !t.is_empty()).map(|t| t.parse().unwrap()).collect()
}

fn call_logic(c: u32, a: isize, b: isize) -> isize {
    let lg = R.with(|r| r.borrow().logic) as *const EeWonRouteLogic;
    // SAFETY: the sequence's live logic; the call's discriminant is the recorded one
    unsafe { ee_won_routelogic_call(lg, std::mem::transmute::<u32, Call>(c), a, b) }
}

/// One recorded "call" line made and its "ret" checked.
fn replay_call() {
    let line = peek();
    expect("call ");
    let v = numbers(&line[5..]);
    let r = call_logic(v[0] as u32, v[1] as isize, v[2] as isize);
    expect(&format!("ret {r}"));
}

fn ids<T: ToString + Copy>(p: isize, n: isize) -> String {
    if n == 0 {
        return String::new();
    }
    // SAFETY: the op's n values at p
    let v = unsafe { std::slice::from_raw_parts(p as *const T, n as usize) };
    v.iter().map(|x| format!(" {}", x.to_string())).collect()
}

unsafe extern "C" fn host(_obj: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize, e: isize) -> isize {
    let n = op as u32;
    let r = match op {
        Op::AddRef => {
            add_ref(a, 1);
            return 0;
        }
        Op::Release | Op::TextFree | Op::ListFree => {
            add_ref(a, -1);
            return 0;
        }
        Op::BadUserIds | Op::StatusOpMap => {
            let (sink, shown) = if op == Op::BadUserIds { (a, 0) } else { (b, a) };
            let ans = numbers(&expect(&format!("op {n} {shown} = ")));
            for v in &ans[1..] {
                // SAFETY: the op's sink
                unsafe { ee_won_routelogic_push(sink as *mut c_void, *v as u64) };
            }
            return 0;
        }
        Op::StatusOpSetMap => {
            expect(&format!("op {n} {a} 0 0 [{} ]", ids::<u32>(b, c)));
            return 0;
        }
        Op::ConnSendChatTo => {
            expect(&format!("op {n} {a} {b} {c} [{} ]", ids::<u16>(e, d)));
            return 0;
        }
        Op::GroupSetCaptain => {
            let v = numbers(&expect(&format!("op {n} {a} {b} = ")));
            // SAFETY: the op's out value
            unsafe { *(c as *mut isize) = v[1] as isize };
            return v[0] as isize;
        }
        Op::GameList if a == 8 => {
            let v = numbers(&expect(&format!("op {n} {a} {b} = ")));
            // SAFETY: the op's out value
            unsafe { *(c as *mut bool) = v[1] != 0 };
            return v[0] as isize;
        }
        Op::RDataObjects | Op::DirEntities => {
            let v = numbers(&expect(&format!("op {n} {a} = ")));
            // SAFETY: the op's out value
            unsafe { *(b as *mut isize) = v[1] as isize };
            add_ref(v[0] as isize, 1);
            return v[0] as isize;
        }
        Op::UserIdLookup => {
            let v = numbers(&expect(&format!("op {n} {a} {b} = ")));
            // SAFETY: the op's out value
            unsafe { *(c as *mut i32) = v[1] as i32 };
            return v[0] as isize;
        }
        _ => expect(&format!("op {n} {a} {b} {c} {d} {e} = ")).parse::<isize>().unwrap(),
    };
    let made = match op {
        Op::AsyncOpNew
        | Op::RNew
        | Op::ConnNew
        | Op::ConnectOpNew
        | Op::StatusOpNew
        | Op::GameCreate
        | Op::ServerRemoveGroup
        | Op::ServerRemoveClient
        | Op::ServerRemoveClientFromGroup
        | Op::RoomDirServers
        | Op::DirOpRun
        | Op::GameText
        | Op::DialogText
        | Op::ParserText
        | Op::IterNew => true,
        Op::GameList => a == 7,
        _ => false,
    };
    if made && r != 0 {
        add_ref(r, 1);
    }
    let modal = matches!(op, Op::DialogDo | Op::RunWithStatus | Op::YesNo | Op::MessageBox | Op::ErrorDialog);
    if modal && R.with(|x| x.borrow().depth) == 0 && peek() == "reenter" {
        expect("reenter");
        R.with(|x| x.borrow_mut().depth += 1);
        replay_call();
        R.with(|x| x.borrow_mut().depth -= 1);
    }
    r
}

fn replay(lines: &[String]) {
    R.with(|r| *r.borrow_mut() = Replay { lines: lines.to_vec(), ..Replay::default() });
    let lg = ee_won_routelogic_new(0x7700 as *mut c_void, host);
    R.with(|r| r.borrow_mut().logic = lg as usize);
    while peek().starts_with("call ") {
        replay_call();
    }
    let mut fields = String::from("fields");
    for i in 0..10 {
        fields += &format!(" {}", call_logic(Call::Field as u32, i, 0));
    }
    expect(&fields);
    expect("free");
    // SAFETY: from ee_won_routelogic_new
    unsafe { ee_won_routelogic_free(lg) };
    let refs = R.with(|r| r.borrow().refs.clone());
    for (h, n) in refs {
        if n != 0 {
            expect(&format!("refs {h} {n}"));
        }
    }
    let left = peek();
    assert!(left.is_empty(), "unreplayed: {left}");
}

#[test]
fn replays_the_golden() {
    let text = include_str!("golden_routelogic.txt");
    let mut seqs: Vec<Vec<String>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("seq ") {
            seqs.push(Vec::new());
        } else {
            seqs.last_mut().unwrap().push(line.to_string());
        }
    }
    assert!(seqs.len() > 100, "{} sequences", seqs.len());
    for s in &seqs {
        replay(s);
    }
}

/// won_routeconn_req_test's lines (the original RoutingLogic.cpp cases' requests, recorded over the C++ helpers)
/// replayed through `conn_request`: the same kind and values, or none for the host's ops.
#[test]
fn replays_the_connection_requests() {
    let text = include_str!("golden_routeconn.txt");
    let mut n = 0;
    for line in text.lines() {
        let field = |k: &str| -> &str {
            line.split(' ').find_map(|t| t.strip_prefix(k)).unwrap_or_else(|| panic!("{k} in {line}"))
        };
        let num = |k: &str| field(k).parse::<i64>().unwrap();
        let op_n = num("op=") as u32;
        // SAFETY: the driver prints ops of this enum
        let op: Op = unsafe { std::mem::transmute::<u32, Op>(op_n) };
        let texts: Vec<Vec<u32>> = field("texts=")
            .split(',')
            .map(|t| {
                t.as_bytes()
                    .chunks(4)
                    .map(|c| u32::from_str_radix(std::str::from_utf8(c).unwrap(), 16).unwrap())
                    .collect()
            })
            .collect();
        let ids: Vec<u16> = field("ids=").split(',').filter(|t| !t.is_empty()).map(|t| t.parse().unwrap()).collect();
        let mut g = [0x1000, num("b=") as isize, num("c=") as isize, num("d=") as isize, num("e=") as isize];
        if op == Op::ConnSendChatTo {
            g[3] = ids.len() as isize;
            g[4] = ids.as_ptr() as isize;
        }
        let read = |p: isize| utf16le(&texts[(p - 1) as usize]);
        let got = conn_request(op, g, &read);
        let made = num("made=") == 1;
        assert_eq!(got.is_some(), made, "{line}");
        let kind = num("kind=") as i32;
        let want: Vec<(i64, String)> = line
            .split(" kind=")
            .nth(1)
            .unwrap()
            .split(' ')
            .skip(1)
            .take_while(|t| !t.starts_with("made="))
            .map(|t| {
                let (a, b) = t.split_once(':').unwrap();
                (a.parse().unwrap(), b.to_string())
            })
            .collect();
        if let Some((k, values)) = got {
            assert_eq!(k, kind, "{line}");
            let have: Vec<(i64, String)> =
                values.iter().map(|v| (v.num, v.bytes.iter().map(|b| format!("{b:02x}")).collect())).collect();
            assert_eq!(have, want, "{line}");
        } else {
            assert_eq!(kind, -1, "{line}");
        }
        n += 1;
    }
    assert!(n > 60, "{n} lines");
}
