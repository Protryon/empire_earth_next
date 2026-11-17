use super::*;
use std::collections::BTreeMap;

// ---- the ping op's reply rules, without a host (the elapsed time answers 0)

fn ping_op(id: u16, flags: u8) -> *mut EeWonPingOp {
    ee_won_pingop_new(std::ptr::null_mut(), None, id, flags)
}

fn check(p: *mut EeWonPingOp, bytes: &[u8]) -> i64 {
    let mut r = EeWonReadBuffer::new(bytes.as_ptr(), bytes.len() as _);
    // SAFETY: a live op and reader
    unsafe { ee_won_pingop_call(p, EeWonPingOpCall::CheckResponse, &mut r as *mut EeWonReadBuffer as isize) }
}

fn field(p: *mut EeWonPingOp, which: isize) -> i64 {
    // SAFETY: a live op
    unsafe { ee_won_pingop_call(p, EeWonPingOpCall::Field, which) }
}

#[test]
fn a_reply_is_its_type_and_request_id() {
    let p = ping_op(0x1234, 3);
    assert_eq!(check(p, &[9, 0x34, 0x12]), WS_NONE);
    assert_eq!(check(p, &[8, 0x35, 0x12]), WS_NONE);
    assert_eq!(check(p, &[8]), EE_WON_LOGIC_THREW);
    assert_eq!(field(p, 2), 3, "a refused reply keeps the request's flags");
    assert_eq!(check(p, &[8, 0x34, 0x12]), WS_SUCCESS);
    assert_eq!((field(p, 1), field(p, 2), field(p, 4)), (0, 0, -1), "no flags, no reply bytes");
    assert_eq!(check(p, &[8, 0x34, 0x12, 1, 0xAA, 0xBB]), WS_SUCCESS);
    assert_eq!((field(p, 2), field(p, 4)), (1, 2));
    // SAFETY: from ee_won_pingop_new
    unsafe { ee_won_pingop_free(p) };
}

#[test]
fn request_ids_from_0x8000_never_match() {
    // the original compared the signed ReadShort with the unsigned member
    let p = ping_op(0x8001, 0);
    assert_eq!(check(p, &[8, 0x01, 0x80]), WS_NONE);
    // SAFETY: from ee_won_pingop_new
    unsafe { ee_won_pingop_free(p) };
    let q = ping_op(0x7fff, 0);
    assert_eq!(check(q, &[8, 0xff, 0x7f]), WS_SUCCESS);
    // SAFETY: from ee_won_pingop_new
    unsafe { ee_won_pingop_free(q) };
}

// ---- the replay of golden_logics.txt (won_logics_test --print): the calls run on the Rust objects, every op the
// objects make must be the recorded one, and its answer is the recorded one.

#[cfg(windows)]
type WChar = u16;
#[cfg(not(windows))]
type WChar = i32;

#[derive(Default)]
struct Replay {
    lines: Vec<String>,
    at: usize,
    refs: BTreeMap<isize, i32>,
    records: BTreeMap<isize, usize>,
    games: BTreeMap<isize, isize>,
    ping_ops: Vec<isize>,
    msg: Vec<u8>,
    name: Vec<WChar>,
    depth: i32,
    ping: usize,
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

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn bytes_at(p: isize, n: isize) -> Vec<u8> {
    if n <= 0 {
        return Vec::new();
    }
    // SAFETY: the objects' `n` bytes at `p`
    unsafe { std::slice::from_raw_parts(p as *const u8, n as usize) }.to_vec()
}

fn add_ref(h: isize, by: i32) {
    R.with(|r| *r.borrow_mut().refs.entry(h).or_default() += by);
}

fn record(h: isize) -> *mut EeWonPingOp {
    R.with(|r| *r.borrow().records.get(&h).unwrap()) as *mut EeWonPingOp
}

const DOCS: [&[u8]; 5] =
    [b"link=http://ee/\nimage=http://ee/a.png\n\0", b"image\n\0", b"link x\nIMAGE  y\r\n\0", b"z\0", b"\0"];

unsafe extern "C" fn host(obj: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let o = obj as isize;
    let n = op as u32;
    match op {
        Op::AddRef => {
            add_ref(a, 1);
            0
        }
        Op::Release => {
            add_ref(a, -1);
            0
        }
        Op::BufAppend => {
            expect(&format!("op {n} {o} {a} '{}'", hex(&bytes_at(b, c))));
            0
        }
        Op::RequestSend => {
            expect(&format!("op {n} {o} '{}'", hex(&bytes_at(a, b))));
            0
        }
        Op::SetPromotionLink | Op::FetchPromotionImage => {
            expect(&format!("op {n} {o} '{}'", String::from_utf8_lossy(&bytes_at(a, b))));
            0
        }
        Op::RecvMsg => {
            let ans = expect(&format!("op {n} {o} {a} = msg "));
            let k: u32 = ans.split(' ').nth(1).unwrap().parse().unwrap();
            R.with(|r| {
                let r = r.borrow();
                // SAFETY: the object's out value
                unsafe { *(b as *mut usize) = if k == 0 { 0 } else { r.msg.len() } };
                if k == 0 {
                    0
                } else if r.msg.is_empty() {
                    1
                } else {
                    r.msg.as_ptr() as isize
                }
            })
        }
        Op::LoginName => {
            let len = R.with(|r| r.borrow().name.len());
            expect(&format!("op {n} {o} {a} = {len}"));
            // SAFETY: the object's out value
            unsafe { *(b as *mut usize) = len };
            R.with(|r| r.borrow().name.as_ptr() as isize)
        }
        Op::ReadSummary | Op::ReadDetails => {
            // SAFETY: the object's reader
            let reader = unsafe { &mut *(b as *mut EeWonReadBuffer) };
            let left = (reader.length as i64 - reader.pos as i64).max(0);
            let rest = bytes_at(reader.data as isize + reader.pos as isize, left as isize);
            let ans = expect(&format!("op {n} {o} {a} at {} of {} '{}' = ", reader.pos, reader.length, hex(&rest)));
            let mut v = ans.split(' ');
            let result: isize = v.next().unwrap().parse().unwrap();
            reader.pos = v.next().unwrap().parse().unwrap();
            result
        }
        Op::TOUTimes => {
            let ans = expect(&format!("op {n} {o} = "));
            let mut v = ans.split(' ').map(|x| x.parse::<i32>().unwrap());
            // SAFETY: the object's out values
            unsafe {
                *(a as *mut i32) = v.next().unwrap();
                *(b as *mut i32) = v.next().unwrap();
            }
            0
        }
        Op::DocBytes => {
            let ans = expect(&format!("op {n} {o} {a} {b} = doc "));
            let k: usize = ans[4..].parse().unwrap();
            if k == 5 {
                // SAFETY: the object's out value
                unsafe { *(c as *mut usize) = usize::MAX };
                return 0;
            }
            // SAFETY: the object's out value
            unsafe { *(c as *mut usize) = DOCS[k].len() };
            DOCS[k].as_ptr() as isize
        }
        Op::OpRecord => {
            expect(&format!("op {n} {o} {a}"));
            record(a) as isize
        }
        _ => {
            let ans: isize = expect(&format!("op {n} {o} {a} {b} {c} {d} = ")).parse().unwrap();
            match op {
                Op::LoginStripAt => R.with(|r| {
                    r.borrow_mut().name.remove(0);
                }),
                Op::NewPingOp => {
                    add_ref(ans, 1);
                    let p = ee_won_pingop_new(ans as *mut c_void, Some(host), b as u16, c as u8);
                    R.with(|r| {
                        let mut r = r.borrow_mut();
                        r.records.insert(ans, p as usize);
                        r.games.insert(ans, a);
                        r.ping_ops.push(ans);
                    });
                }
                Op::QueryAccount | Op::CreateAccount | Op::GetCert => add_ref(ans, 1),
                Op::RunTracked | Op::KillTracked => {
                    let depth = R.with(|r| r.borrow().depth);
                    let next = peek();
                    if depth == 0 && next.starts_with("reenter done ") {
                        let h: isize = next["reenter done ".len()..].parse().unwrap();
                        expect(&next);
                        R.with(|r| r.borrow_mut().depth += 1);
                        let ping = R.with(|r| r.borrow().ping) as *const EeWonLogic;
                        // SAFETY: the sequence's live ping logic
                        unsafe { ee_won_logic_call(ping, EeWonLogicCall::PingDone, h, 0, 0) };
                        R.with(|r| r.borrow_mut().depth -= 1);
                    }
                }
                _ => {}
            }
            ans
        }
    }
}

fn replay(lines: &[String]) {
    R.with(|r| *r.borrow_mut() = Replay { lines: lines.to_vec(), ..Replay::default() });
    let ping = ee_won_logic_new(EeWonLogicKind::Ping, 0x7100 as *mut c_void, Some(host));
    let acct = ee_won_logic_new(EeWonLogicKind::Account, 0x7200 as *mut c_void, Some(host));
    let init = ee_won_logic_new(EeWonLogicKind::Init, 0x7300 as *mut c_void, Some(host));
    R.with(|r| r.borrow_mut().ping = ping as usize);
    let logic = |call: u32| {
        if call < 20 {
            ping
        } else if call < 40 {
            acct
        } else {
            init
        }
    };
    loop {
        let line = peek();
        if line.starts_with("fields ") {
            break;
        }
        assert!(line.starts_with("call "), "{line}");
        expect("call ");
        // "call W C a b c 'hex' [name]"
        let q = line.find(" '").unwrap();
        let v: Vec<isize> = line[5..q].split(' ').map(|t| t.parse().unwrap()).collect();
        let rest = &line[q + 2..];
        let e = rest.find('\'').unwrap();
        let msg = unhex(&rest[..e]);
        let name = &rest[e + 3..rest.len() - 1];
        R.with(|r| {
            let mut r = r.borrow_mut();
            r.msg = msg.clone();
            r.name = name.chars().map(|ch| ch as WChar).collect();
        });
        let (what, call, a, b, c) = (v[0], v[1] as u32, v[2], v[3], v[4]);
        if what == 3 {
            // a ping op of any request id
            let line = peek();
            expect("op new ");
            let h: isize = line["op new ".len()..].parse().unwrap();
            let p = ee_won_pingop_new(h as *mut c_void, Some(host), a as u16, b as u8);
            R.with(|r| {
                let mut r = r.borrow_mut();
                r.records.insert(h, p as usize);
                r.games.insert(h, c);
                r.ping_ops.push(h);
            });
            expect("ret 0");
            continue;
        }
        let ops = R.with(|r| r.borrow().ping_ops.clone());
        let pick = |i: isize| if ops.is_empty() { 0 } else { ops[i as usize % ops.len()] };
        let needs_op = what != 0 || call == EeWonLogicCall::PingDone as u32;
        if needs_op && ops.is_empty() {
            expect("skip");
            continue;
        }
        let result: i64 = match what {
            0 => {
                let a = if call == EeWonLogicCall::PingDone as u32 { pick(a) } else { a };
                // SAFETY: the sequence's live objects; the call's discriminant is the recorded one
                unsafe {
                    ee_won_logic_call(logic(call), std::mem::transmute::<u32, EeWonLogicCall>(call), a, b, c) as i64
                }
            }
            1 => {
                let h = pick(a);
                expect(&format!("op hook {h}"));
                let game = R.with(|r| *r.borrow().games.get(&h).unwrap());
                // SAFETY: a live record
                unsafe { ee_won_pingop_call(record(h), EeWonPingOpCall::RunHook, game) }
            }
            _ => {
                let h = pick(a);
                expect(&format!("op check {h}"));
                let mut rd =
                    EeWonReadBuffer::new(if msg.is_empty() { std::ptr::null() } else { msg.as_ptr() }, msg.len() as _);
                // SAFETY: a live record and reader
                unsafe {
                    ee_won_pingop_call(
                        record(h),
                        EeWonPingOpCall::CheckResponse,
                        &mut rd as *mut EeWonReadBuffer as isize,
                    )
                }
            }
        };
        expect(&format!("ret {result}"));
    }
    let f = |p: *const EeWonLogic, call: EeWonLogicCall, a: isize| {
        // SAFETY: the sequence's live objects
        unsafe { ee_won_logic_call(p, call, a, 0, 0) }
    };
    expect(&format!(
        "fields {} {} {} {}",
        f(ping, EeWonLogicCall::PingNextId, 0),
        f(ping, EeWonLogicCall::PingPending, 0),
        f(acct, EeWonLogicCall::AcctNeedTOU, 0),
        f(acct, EeWonLogicCall::AcctLastNameLength, 0)
    ));
    expect(&format!("fields {} {}", f(init, EeWonLogicCall::InitField, 0), f(init, EeWonLogicCall::InitField, 1)));
    let ops = R.with(|r| r.borrow().ping_ops.clone());
    for h in ops {
        let p = record(h);
        let fields: Vec<String> = (0..5).map(|i| field(p, i).to_string()).collect();
        expect(&format!("pingop {}", fields.join(" ")));
        // SAFETY: from ee_won_pingop_new
        unsafe { ee_won_pingop_free(p) };
    }
    expect("free");
    // SAFETY: from ee_won_logic_new
    unsafe {
        ee_won_logic_free(ping);
        ee_won_logic_free(acct);
        ee_won_logic_free(init);
    }
    let refs: Vec<(isize, i32)> =
        R.with(|r| r.borrow().refs.iter().filter(|x| *x.1 != 0).map(|(&k, &v)| (k, v)).collect());
    for (h, count) in refs {
        expect(&format!("refs {h} {count}"));
    }
    assert_eq!(peek(), "", "the sequence's lines are all made");
}

#[test]
fn the_golden_logics_replays() {
    let text = include_str!("golden_logics.txt");
    let mut sequences = 0;
    let mut block: Vec<String> = Vec::new();
    for line in text.lines().chain(std::iter::once("seq end")) {
        if line.starts_with("seq ") {
            if !block.is_empty() {
                replay(&block);
                sequences += 1;
            }
            block.clear();
        } else {
            block.push(line.to_string());
        }
    }
    assert!(sequences >= 60, "{sequences} sequences");
}
