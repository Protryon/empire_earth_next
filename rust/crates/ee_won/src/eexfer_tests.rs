use super::*;
use crate::eesound::*;
use std::collections::BTreeMap;

// ---- the replay of golden_eexfer.txt (won_eexfer_test --print): the calls run on the Rust objects, every op the
// objects make must be the recorded one, and its answer is the recorded one.

#[derive(Default)]
struct Replay {
    lines: Vec<String>,
    at: usize,
    trace: Vec<String>,
    refs: BTreeMap<isize, i32>,
    depth: i32,
    xfer: usize,
}

thread_local! {
    static R: RefCell<Replay> = RefCell::new(Replay::default());
}

/// The next recorded line, which must start with `head`; the text after " = ".
fn expect(head: &str) -> String {
    R.with(|r| {
        let mut r = r.borrow_mut();
        let line = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        assert!(line.starts_with(head), "expected '{line}', made '{head}'");
        let answer = line.split(" = ").nth(1).unwrap_or("").to_string();
        r.trace.push(line);
        answer
    })
}

fn cstr(p: isize) -> String {
    // SAFETY: a NUL-terminated path the objects made
    unsafe { std::ffi::CStr::from_ptr(p as *const std::ffi::c_char) }.to_string_lossy().into_owned()
}

unsafe extern "C" fn host(_obj: *mut c_void, op: EeWonEeXferOp, a: isize, b: isize, c: isize) -> isize {
    match op {
        Op::AddRef => {
            R.with(|r| *r.borrow_mut().refs.entry(a).or_default() += 1);
            0
        }
        Op::Release => {
            R.with(|r| *r.borrow_mut().refs.entry(a).or_default() -= 1);
            0
        }
        Op::SavedGameInfo => {
            let ans = expect(&format!("op 1 path '{}' = ", cstr(a)));
            let mut v = ans.split(' ').map(|x| x.parse::<u64>().unwrap());
            // SAFETY: the objects' out values
            unsafe {
                *(b as *mut u32) = v.next().unwrap() as u32;
                *(c as *mut u32) = v.next().unwrap() as u32;
            }
            0
        }
        Op::QueueSend | Op::QueueDownload => {
            // SAFETY: `c` bytes of path at `b`
            let path = unsafe { std::slice::from_raw_parts(b as *const u8, c as usize) };
            expect(&format!("op {} {} path '{}'", op as u32, a, String::from_utf8_lossy(path)));
            0
        }
        Op::Open => expect(&format!("op 40 path '{}' = ", cstr(a))).parse().unwrap(),
        Op::Read => expect(&format!("op 41 {a} {c} = ")).parse().unwrap(),
        Op::Send => {
            let line = R.with(|r| {
                let r = r.borrow();
                r.lines[r.at].clone()
            });
            let sent: i32 = line.split(" sent ").nth(1).unwrap().split(' ').next().unwrap().parse().unwrap();
            let ans = expect(&format!("op 44 {b} sent {sent} = "));
            // SAFETY: the object's out value
            unsafe { *(c as *mut i32) = sent };
            ans.parse().unwrap()
        }
        _ => {
            let ans: isize = expect(&format!("op {} {a} {b} {c} = ", op as u32)).parse().unwrap();
            if op == Op::NewSocket {
                R.with(|r| *r.borrow_mut().refs.entry(ans).or_default() += 1);
            }
            if op == Op::Kill {
                let next = R.with(|r| {
                    let r = r.borrow();
                    r.lines.get(r.at).cloned().unwrap_or_default()
                });
                let depth = R.with(|r| r.borrow().depth);
                if depth == 0 && next.starts_with("reenter closed ") {
                    expect(&format!("reenter closed {a}"));
                    R.with(|r| r.borrow_mut().depth += 1);
                    let x = R.with(|r| r.borrow().xfer) as *const EeWonEeXfer;
                    // SAFETY: the sequence's live transfer
                    unsafe { ee_won_eexfer_call(x, EeWonEeXferCall::OnClientClosed, 0x0C00 + a, 0) };
                    R.with(|r| r.borrow_mut().depth -= 1);
                }
            }
            ans
        }
    }
}

unsafe extern "C" fn sound_host(obj: *mut c_void, op: EeWonEeSoundOp, a: isize) -> isize {
    if op == EeWonEeSoundOp::Load {
        return expect(&format!("sop 8 '{}' = ", cstr(a))).parse().unwrap();
    }
    expect(&format!("sop {} {} {a} = ", op as u32, obj as isize)).parse().unwrap()
}

fn replay(head: &str, lines: &[String]) {
    // "seq n seed path|sample"
    let rest: Vec<&str> = head.splitn(4, ' ').collect();
    let (send_path, sample) = rest[3].rsplit_once('|').unwrap();
    let sample: u64 = sample.parse().unwrap();
    R.with(|r| *r.borrow_mut() = Replay { lines: lines.to_vec(), ..Replay::default() });
    let x = ee_won_eexfer_new(0x7000 as *mut c_void, Some(host));
    R.with(|r| r.borrow_mut().xfer = x as usize);
    let s = unsafe { ee_won_eexfer_send_new(0x7100 as *mut c_void, Some(host), send_path.as_ptr(), send_path.len()) };
    let music = ee_won_eesound_new(0x5100 as *mut c_void, EeWonEeSoundKind::Music, Some(sound_host), 0);
    let effect = ee_won_eesound_new(0x5200 as *mut c_void, EeWonEeSoundKind::Effect, Some(sound_host), sample as u32);
    loop {
        let line = R.with(|r| {
            let r = r.borrow();
            r.lines.get(r.at).cloned().unwrap_or_default()
        });
        if line == "free" {
            expect("free");
            break;
        }
        assert!(line.starts_with("call "), "{line}");
        expect("call ");
        let head_end = line.find(" '").unwrap_or(line.len());
        let v: Vec<isize> = line[5..head_end].split(' ').map(|t| t.parse().unwrap()).collect();
        let text = if head_end < line.len() { line[head_end + 2..line.len() - 1].to_string() } else { String::new() };
        let (what, call, a, b) = (v[0], v[1] as u32, v[2], v[3]);
        // SAFETY: the sequence's live objects; the enums' discriminants are the recorded ones
        let r = unsafe {
            match what {
                0 if call == EeWonEeXferCall::SetPath as u32 => {
                    ee_won_eexfer_call(x, EeWonEeXferCall::SetPath, text.as_ptr() as isize, text.len() as isize)
                }
                0 => ee_won_eexfer_call(x, std::mem::transmute::<u32, EeWonEeXferCall>(call), a, b),
                1 => ee_won_eexfer_send_call(s, std::mem::transmute::<u32, EeWonEeXferSendCall>(call)) as isize,
                2 => ee_won_eesound_call(music, std::mem::transmute::<u32, EeWonEeSoundCall>(call), a),
                3 => ee_won_eesound_call(effect, std::mem::transmute::<u32, EeWonEeSoundCall>(call), a),
                _ => {
                    let p = format!("{text}\0");
                    ee_won_eesound_load(Some(sound_host), p.as_ptr()) as isize
                }
            }
        };
        assert_eq!(expect("ret "), "", "ret");
        let want = R.with(|rr| rr.borrow().trace.last().cloned().unwrap());
        assert_eq!(want, format!("ret {r}"));
    }
    unsafe {
        ee_won_eexfer_free(x);
        ee_won_eexfer_send_free(s);
        ee_won_eesound_free(music);
        ee_won_eesound_free(effect);
    }
    let refs: Vec<String> =
        R.with(|r| r.borrow().refs.iter().filter(|(_, n)| **n != 0).map(|(p, n)| format!("refs {p} {n}")).collect());
    for line in refs {
        assert_eq!(expect("refs "), "");
        let want = R.with(|r| r.borrow().trace.last().cloned().unwrap());
        assert_eq!(want, line);
    }
    let (at, len) = R.with(|r| (r.borrow().at, r.borrow().lines.len()));
    assert_eq!(at, len, "lines left over");
}

#[test]
fn the_golden_sequences_replay() {
    let text = include_str!("golden_eexfer.txt");
    let mut seqs: Vec<(String, Vec<String>)> = Vec::new();
    for line in text.lines() {
        if line.starts_with("seq ") {
            seqs.push((line.to_string(), Vec::new()));
        } else if let Some(s) = seqs.last_mut() {
            s.1.push(line.to_string());
        }
    }
    assert!(seqs.len() >= 60);
    for (head, lines) in &seqs {
        replay(head, lines);
    }
}

// ---- the rules

#[derive(Default)]
struct Script {
    log: Vec<(u32, isize)>,
    answers: BTreeMap<u32, Vec<isize>>,
}

thread_local! {
    static S: RefCell<Script> = RefCell::new(Script::default());
}

unsafe extern "C" fn script_host(_obj: *mut c_void, op: EeWonEeXferOp, a: isize, _b: isize, c: isize) -> isize {
    S.with(|s| {
        let mut s = s.borrow_mut();
        s.log.push((op as u32, a));
        let ans = s.answers.get_mut(&(op as u32)).and_then(|v| if v.is_empty() { None } else { Some(v.remove(0)) });
        if op == Op::Send {
            // SAFETY: the out value
            unsafe { *(c as *mut i32) = 100 };
        }
        ans.unwrap_or(0)
    })
}

fn answers(list: &[(Op, &[isize])]) {
    S.with(|s| {
        let mut s = s.borrow_mut();
        *s = Script::default();
        for (op, v) in list {
            s.answers.insert(*op as u32, v.to_vec());
        }
    });
}

fn ops() -> Vec<u32> {
    S.with(|s| s.borrow().log.iter().map(|(o, _)| *o).collect())
}

#[test]
fn serve_stops_first_and_stops_at_a_failed_bind() {
    answers(&[(Op::NewSocket, &[0x100, 0x200]), (Op::Bind, &[-5, 0])]);
    let x = EeWonEeXfer::new(std::ptr::null_mut(), Some(script_host));
    assert_eq!(x.call(EeWonEeXferCall::Serve, 8889, 0), -5);
    assert_eq!(x.call(EeWonEeXferCall::Field, 5, 0), 0x100);
    assert_eq!(x.call(EeWonEeXferCall::Serve, 8889, 0), 0);
    let k = |o: Op| o as u32;
    assert_eq!(
        ops(),
        vec![
            k(Op::NewSocket),
            k(Op::Bind),
            k(Op::Kill),
            k(Op::Release),
            k(Op::NewSocket),
            k(Op::Bind),
            k(Op::Listen),
            k(Op::RepeatAccept)
        ]
    );
}

#[test]
fn a_file_is_here_by_size_and_checksum() {
    answers(&[]);
    let x = EeWonEeXfer::new(std::ptr::null_mut(), Some(script_host));
    assert_eq!(x.call(EeWonEeXferCall::IsFileHere, 0, 0), 1);
    x.call(EeWonEeXferCall::SetFileInfo, 5, 6);
    assert_eq!(x.call(EeWonEeXferCall::IsFileHere, 0, 0), 0);
    // no path: no download, and the host's address is not asked
    x.call(EeWonEeXferCall::Download, 0, 0);
    assert!(ops().is_empty());
}

#[test]
fn the_send_op_sends_every_chunk() {
    answers(&[(Op::Open, &[0xF11E]), (Op::Read, &[150, 0])]);
    let s = EeWonEeXferSend::new(std::ptr::null_mut(), Some(script_host), b"a.ees");
    assert_eq!(s.call(EeWonEeXferSendCall::Start) as i64, WS_SUCCESS);
    let k = |o: Op| o as u32;
    // 150 bytes: two sends of 100 (the host's), then the end of the file closes it
    assert_eq!(
        ops(),
        vec![
            k(Op::Open),
            k(Op::Read),
            k(Op::WaitForWrite),
            k(Op::Send),
            k(Op::WaitForWrite),
            k(Op::Send),
            k(Op::Read),
            k(Op::Close)
        ]
    );
    let none = EeWonEeXferSend::new(std::ptr::null_mut(), Some(script_host), b"");
    assert_eq!(none.call(EeWonEeXferSendCall::Start) as i64, WS_FAILED_TO_OPEN_FILE);
}
