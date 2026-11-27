use super::*;

/// A stand-in for the C++ containers: every target is one of these, each op on its field.
#[derive(Default, Clone, Debug, PartialEq)]
struct Fake {
    s: Vec<u8>,
    w: Vec<WChar>,
    list: Vec<Vec<u8>>,
    ll: Vec<Vec<i32>>,
}

unsafe fn cstr(p: *const u8) -> Vec<u8> {
    let mut n = 0;
    while unsafe { *p.add(n) } != 0 {
        n += 1;
    }
    unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
}

unsafe fn wcstr(p: *const WChar) -> Vec<WChar> {
    let mut n = 0;
    while unsafe { *p.add(n) } != 0 {
        n += 1;
    }
    unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
}

unsafe extern "C" fn fake_host(op: EeWonCfgValOp, t: *mut c_void, a: isize, b: isize, c: isize) -> isize {
    use EeWonCfgValOp as O;
    let f = unsafe { &mut *(t as *mut Fake) };
    let src = || unsafe { (*(a as *const Fake)).clone() };
    let bytes = || unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) }.to_vec();
    match op {
        O::StrSet => f.s = bytes(),
        O::StrSetC => f.s = unsafe { cstr(a as *const u8) },
        O::StrErase => f.s.clear(),
        O::StrOut => unsafe {
            let sink: EeWonByteSink = std::mem::transmute(a);
            sink(b as *mut c_void, f.s.as_ptr(), f.s.len())
        },
        O::StrCopy => f.s = src().s,
        O::WStrSet => f.w = unsafe { std::slice::from_raw_parts(a as *const WChar, b as usize) }.to_vec(),
        O::WStrSetC => f.w = unsafe { wcstr(a as *const WChar) },
        O::WStrErase => f.w.clear(),
        O::WStrOut => unsafe {
            let sink: EeWonCfgValUnits = std::mem::transmute(a);
            sink(b as *mut c_void, f.w.as_ptr() as *const c_void, f.w.len())
        },
        O::WStrCopy => f.w = src().w,
        O::ListClear => f.list.clear(),
        O::ListPush => f.list.push(bytes()),
        O::ListCount => return f.list.len() as isize,
        O::ListOut => unsafe {
            let sink: EeWonByteSink = std::mem::transmute(b);
            let item = &f.list[a as usize];
            sink(c as *mut c_void, item.as_ptr(), item.len())
        },
        O::ListCopy => f.list = src().list,
        O::LlClear => f.ll.clear(),
        O::LlPushGroup => f.ll.push(Vec::new()),
        O::LlPushInt => f.ll.last_mut().unwrap().push(a as i32),
        O::LlCount => return f.ll.len() as isize,
        O::LlGroupCount => return f.ll[a as usize].len() as isize,
        O::LlGet => return f.ll[a as usize][b as usize] as isize,
        O::LlCopy => f.ll = src().ll,
    }
    0
}

fn setup() {
    ee_won_cfgval_set_host(Some(fake_host));
}

fn parser(text: &[u8]) -> (Vec<u8>, EeWonCfgStringParser) {
    let mut t = text.to_vec();
    t.push(0);
    let sp = EeWonCfgStringParser { text: std::ptr::null(), pos: 0, wide: false };
    (t, sp)
}

fn read(v: *mut EeWonCfgVal, text: &[u8]) -> (bool, i32) {
    let (t, mut sp) = parser(text);
    sp.text = t.as_ptr() as *const c_void;
    let r = unsafe { ee_won_cfgval_call(v, EeWonCfgValCall::Read, &mut sp as *mut _ as isize, 0) } != 0;
    (r, sp.pos)
}

fn write(v: *mut EeWonCfgVal) -> String {
    let mut out: Vec<u8> = Vec::new();
    unsafe {
        ee_won_cfgval_call(v, EeWonCfgValCall::Write, sink_vec as EeWonByteSink as usize as isize, ctx(&mut out))
    };
    String::from_utf8(out).unwrap()
}

fn call(v: *mut EeWonCfgVal, c: EeWonCfgValCall, a: isize) -> isize {
    unsafe { ee_won_cfgval_call(v, c, a, 0) }
}

fn wide(s: &str) -> Vec<WChar> {
    s.chars().map(|c| c as WChar).collect()
}

#[test]
fn scalars_init_read_and_write() {
    setup();
    let mut b = false;
    let v = unsafe { ee_won_cfgval_bool(&mut b, true) };
    call(v, EeWonCfgValCall::Init, 0);
    assert!(b);
    assert_eq!(read(v, b"off"), (true, 3));
    assert!(!b);
    assert_eq!(write(v), "False");
    // another word is consumed and leaves the value
    assert!(!read(v, b"maybe").0);
    assert!(!b);

    let mut u = 0u16;
    let v = unsafe { ee_won_cfgval_u16(&mut u, 7) };
    call(v, EeWonCfgValCall::Init, 0);
    assert_eq!(u, 7);
    assert!(read(v, b"0x10001").0);
    assert_eq!(u, 1); // the int cut to 16 bits
    assert!(!read(v, b"x").0);
    assert_eq!(u, 1); // kept after a failed read
    assert_eq!(write(v), "1");

    let mut i = 0i32;
    let v = unsafe { ee_won_cfgval_int(&mut i, 3) };
    call(v, EeWonCfgValCall::Init, 0);
    assert_eq!(i, 3);
    assert!(read(v, b"-42").0);
    assert_eq!(i, -42);
    assert_eq!(write(v), "-42");
}

#[test]
fn texts_and_their_defaults() {
    setup();
    let mut f = Fake { s: b"old".to_vec(), w: wide("old"), ..Default::default() };
    let s = unsafe { ee_won_cfgval_string(&mut f as *mut Fake as *mut c_void, c"def".as_ptr() as *const u8) };
    call(s, EeWonCfgValCall::Init, 0);
    assert_eq!(f.s, b"def");
    assert!(read(s, b"\"a b\" rest").0);
    assert_eq!(f.s, b"a b");
    assert_eq!(write(s), "a b");
    let none = unsafe { ee_won_cfgval_string(&mut f as *mut Fake as *mut c_void, std::ptr::null()) };
    call(none, EeWonCfgValCall::Init, 0);
    assert!(f.s.is_empty());

    let wdef: Vec<WChar> = wide("w\0");
    let w = unsafe { ee_won_cfgval_wstring(&mut f as *mut Fake as *mut c_void, wdef.as_ptr() as *const c_void) };
    call(w, EeWonCfgValCall::Init, 0);
    assert_eq!(f.w, wide("w"));
    assert!(read(w, b"wide").0);
    assert_eq!(f.w, wide("wide"));
    assert_eq!(write(w), "wide");
}

#[test]
fn lists_replace_or_add() {
    setup();
    let mut f = Fake::default();
    let t = &mut f as *mut Fake as *mut c_void;
    let add = unsafe { ee_won_cfgval_string_list(t, c"a, b".as_ptr() as *const c_void, false, true) };
    call(add, EeWonCfgValCall::Init, 0);
    assert_eq!(f.list, vec![b"a".to_vec(), b"b".to_vec()]);
    assert!(read(add, b"c").0);
    assert_eq!(f.list.len(), 3);
    assert_eq!(write(add), "a, b, c");
    let repl = unsafe { ee_won_cfgval_string_list(t, std::ptr::null(), false, false) };
    assert!(read(repl, b"x, y").0);
    assert_eq!(f.list, vec![b"x".to_vec(), b"y".to_vec()]);
    // the lobby's hash file list (a Rust list since R6 WON round 39)
    assert_eq!(crate::cfgval::read_string_list(c"h1, h2".as_ptr() as *const u8), vec![b"h1".to_vec(), b"h2".to_vec()]);

    let ll = unsafe { ee_won_cfgval_int_list_list(t, c"(1, 2)".as_ptr() as *const c_void, false, false) };
    call(ll, EeWonCfgValCall::Init, 0);
    assert_eq!(f.ll, vec![vec![1, 2]]);
    assert!(read(ll, b"(3), (4, 5)").0);
    assert_eq!(f.ll, vec![vec![3], vec![4, 5]]);
    assert_eq!(write(ll), "(3), (4, 5)");
    // a failed element or a missing ')' keeps what was read
    assert!(!read(ll, b"(6, x)").0);
    assert_eq!(f.ll, vec![vec![6]]);
    assert!(!read(ll, b"(7").0);
    assert_eq!(f.ll, vec![vec![7]]);
}

#[test]
fn copies_need_the_same_type_and_take_the_default_flag() {
    setup();
    let (mut a, mut b) = (Fake { s: b"from".to_vec(), ..Default::default() }, Fake::default());
    let from = unsafe { ee_won_cfgval_string(&mut a as *mut Fake as *mut c_void, std::ptr::null()) };
    let to = unsafe { ee_won_cfgval_string(&mut b as *mut Fake as *mut c_void, std::ptr::null()) };
    call(from, EeWonCfgValCall::SetDefault, 1);
    call(to, EeWonCfgValCall::Copy, from as isize);
    assert_eq!(b.s, b"from");
    assert_eq!(call(to, EeWonCfgValCall::IsDefault, 0), 1);
    let mut flag = false;
    let other = unsafe { ee_won_cfgval_bool(&mut flag, false) };
    call(to, EeWonCfgValCall::SetDefault, 0);
    call(to, EeWonCfgValCall::Copy, other as isize);
    assert_eq!(call(to, EeWonCfgValCall::IsDefault, 0), 0); // no copy: the flag stays
}

#[test]
fn the_last_release_frees() {
    let mut b = false;
    let v = unsafe { ee_won_cfgval_bool(&mut b, false) };
    call(v, EeWonCfgValCall::AddRef, 0);
    call(v, EeWonCfgValCall::AddRef, 0);
    call(v, EeWonCfgValCall::Release, 0);
    assert_eq!(unsafe { (*v).refs.get() }, 1);
    call(v, EeWonCfgValCall::Release, 0); // freed
}
