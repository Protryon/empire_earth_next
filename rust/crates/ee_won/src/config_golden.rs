//! `golden_config.txt`: the C++ ConfigParser's answers to `tests/won/won_config_test.cpp`'s inputs and scenarios,
//! recorded over the original parser before the port. This test reads the inputs out of that source, writes their
//! files into a temporary directory and replays the C-locale pass through `config` and `file`; the C++ glue that
//! stays (the file names, the error and warning texts, the parent walk, the states' copies) is modelled here as
//! `Parser`. The `english` pass needs the C runtime's locale and is checked by `won_config_test` itself, which runs
//! over the port.

use std::ffi::{c_ulong, CString};
use std::sync::Mutex;

use super::*;
use crate::parser::CLocale;

const GOLDEN: &str = include_str!("golden_config.txt");
const SOURCE: &str = include_str!("../../../../tests/won/won_config_test.cpp");

/// The line count is process-wide: the tests that read characters take turns.
pub(super) static LOCK: Mutex<()> = Mutex::new(());

/// The C string literals of `kInputs`, unescaped.
fn inputs() -> Vec<Vec<u8>> {
    let start = SOURCE.find("kInputs[] = {").expect("the inputs");
    let end = start + SOURCE[start..].find("\n};").expect("their end");
    let mut out = Vec::new();
    for line in SOURCE[start..end].lines().skip(1) {
        let line = line.trim();
        if !line.starts_with('"') {
            continue;
        }
        let body = &line[1..line.rfind('"').unwrap()];
        let b = body.as_bytes();
        let mut v = Vec::new();
        let mut i = 0;
        while i < b.len() {
            if b[i] == b'\\' {
                i += 1;
                match b[i] {
                    b'n' => v.push(b'\n'),
                    b'r' => v.push(b'\r'),
                    b't' => v.push(b'\t'),
                    b'v' => v.push(0x0b),
                    b'f' => v.push(0x0c),
                    b'\\' => v.push(b'\\'),
                    b'"' => v.push(b'"'),
                    b'x' => {
                        v.push(u8::from_str_radix(&body[i + 1..i + 3], 16).unwrap());
                        i += 2;
                    }
                    c => panic!("escape {c}"),
                }
            } else {
                v.push(b[i]);
            }
            i += 1;
        }
        out.push(v);
    }
    out
}

fn esc(t: &[u8]) -> String {
    let mut s = String::from("|");
    for &c in t {
        if (0x20..0x7f).contains(&c) && c != b'\\' && c != b'|' {
            s.push(c as char);
        } else {
            s += &format!("\\x{c:02x}");
        }
    }
    s + "|"
}

fn units(t: &[u32]) -> String {
    format!("|{}|", t.iter().map(|u| format!("{u:x}")).collect::<Vec<_>>().join("."))
}

/// The C++ `ConfigParser` around the Rust parts.
struct Parser {
    reader: EeWonFileReader,
    lexer: Lexer,
    line: i32,
    unicode: bool,
    abort: Vec<u8>,
    warnings: Vec<u8>,
    name: Vec<u8>,
    file_path: Vec<u8>,
    warnings_as_errors: bool,
    parent: Option<usize>,
}

struct World {
    dir: std::path::PathBuf,
    ps: Vec<Parser>,
}

impl World {
    fn new_parser(&mut self) -> usize {
        self.ps.push(Parser {
            reader: EeWonFileReader::new(),
            lexer: Lexer::new(),
            line: 0,
            unicode: false,
            abort: Vec::new(),
            warnings: Vec::new(),
            name: Vec::new(),
            file_path: Vec::new(),
            warnings_as_errors: false,
            parent: None,
        });
        self.ps.len() - 1
    }

    fn root(&self, mut p: usize) -> usize {
        while let Some(q) = self.ps[p].parent {
            p = q;
        }
        p
    }

    fn set_parent(&mut self, p: usize, parent: usize) {
        self.ps[p].parent = Some(parent);
        self.ps[p].warnings_as_errors = self.ps[parent].warnings_as_errors;
    }

    fn with<R>(&mut self, p: usize, f: impl FnOnce(&mut Lexer, &mut Io<EeWonFileReader>) -> R) -> R {
        let q = &mut self.ps[p];
        let mut io = Io { source: &mut q.reader, line: &mut q.line, unicode: q.unicode };
        f(&mut q.lexer, &mut io)
    }

    fn set_error(&mut self, p: usize, error: &[u8], append: bool) {
        let q = &mut self.ps[p];
        if !q.abort.is_empty() && !append {
            return;
        }
        q.abort.extend_from_slice(error);
        q.abort.extend_from_slice(b" (");
        q.abort.extend_from_slice(&q.name);
        q.abort.extend_from_slice(format!(":{})", q.line).as_bytes());
    }

    fn add_warning(&mut self, p: usize, warning: &[u8]) {
        if !self.ps[p].abort.is_empty() {
            return;
        }
        if self.ps[p].warnings_as_errors {
            self.set_error(p, warning, false);
        }
        let q = &mut self.ps[p];
        q.warnings.extend_from_slice(b"Warning (");
        q.warnings.extend_from_slice(&q.name);
        q.warnings.extend_from_slice(format!(": {}) - ", q.line).as_bytes());
        q.warnings.extend_from_slice(warning);
        q.warnings.push(b'\n');
    }

    fn abort_reason(&mut self, p: usize) -> Vec<u8> {
        if self.ps[p].abort.is_empty() {
            self.set_error(p, b"Parse aborted.", false);
        }
        self.ps[p].abort.clone()
    }

    fn open(&mut self, p: usize, name: &str) -> bool {
        let real = CString::new(self.dir.join(name).to_str().unwrap()).unwrap();
        let q = &mut self.ps[p];
        q.reader.close();
        q.line = 1;
        q.unicode = false;
        q.lexer.set_back(&[]);
        q.name = name.as_bytes().to_vec();
        let sep = name.rfind('/').or_else(|| name.rfind('\\'));
        q.file_path = sep.map(|s| name.as_bytes()[..s + 1].to_vec()).unwrap_or_default();
        let (open, status) = unsafe { open_file(&mut q.reader, &mut q.unicode, real.as_ptr()) };
        assert_eq!(status, WON_FILE_OK);
        open
    }

    fn add_define(&mut self, p: usize, name: &[u8], value: &[u8], redefine: bool) -> bool {
        let r = self.root(p);
        match self.ps[r].lexer.add_define(name, value, redefine, &CLocale) {
            DefineResult::Spaces => self.add_warning(r, b"Not allowed to have spaces in define names."),
            DefineResult::Redefinition => self.add_warning(r, &[b"Redefintion of ", name].concat()),
            DefineResult::Ok => {}
        }
        true
    }

    fn get_define(&self, p: usize, name: &[u8]) -> Option<Vec<u8>> {
        self.ps[self.root(p)].lexer.get_define(name, &CLocale).map(<[u8]>::to_vec)
    }

    fn get_define_wide(&self, p: usize, name: &[u8]) -> Option<Vec<u32>> {
        let v = self.get_define(p, name)?;
        let mut t = Vec::new();
        Token::Wide(&mut t, 2).push_value(&v);
        Some(t)
    }

    fn push(&mut self, p: usize) {
        let r = self.root(p);
        self.ps[r].lexer.push_scope();
    }

    fn pop(&mut self, p: usize) {
        let r = self.root(p);
        self.ps[r].lexer.pop_scope();
    }

    fn get_state(&self, p: usize) -> SavedState {
        let q = &self.ps[p];
        SavedState { pos: q.reader.pos() as c_long, path: q.name.clone(), line: q.line, back: q.lexer.back().to_vec() }
    }

    fn set_state(&mut self, p: usize, s: &SavedState) -> bool {
        if !self.ps[p].reader.is_open() || s.path != self.ps[p].name {
            let name = String::from_utf8(s.path.clone()).unwrap();
            if !self.open(p, &name) {
                return false;
            }
        }
        let q = &mut self.ps[p];
        assert_eq!(q.reader.set_pos(s.pos as c_ulong), WON_FILE_OK);
        q.line = s.line;
        q.lexer.set_back(&s.back);
        true
    }

    fn add_group(&mut self, p: usize, name: &[u8]) -> bool {
        let state = self.get_state(p);
        let r = self.root(p);
        match self.ps[r].lexer.add_group(name, &state, &CLocale) {
            DefineResult::Spaces => self.add_warning(p, b"Not allowed to have spaces in define names."),
            DefineResult::Redefinition => self.add_warning(r, &[b"Redefintion of ", name].concat()),
            DefineResult::Ok => {}
        }
        true
    }

    fn get_group(&self, p: usize, name: &[u8]) -> Option<SavedState> {
        self.ps[self.root(p)].lexer.get_group(name, &CLocale).cloned()
    }

    fn reset(&mut self, p: usize) {
        let q = &mut self.ps[p];
        q.lexer.reset();
        q.abort.clear();
        q.warnings.clear();
        q.line = 1;
        q.reader.rewind();
    }

    fn token(&mut self, p: usize, out: &mut Token, to_end: bool, skip: bool) -> bool {
        let r = self.root(p);
        let result = if r == p {
            self.with(p, |l, io| read_token(l, io, &CLocale, None, out, to_end, skip))
        } else {
            // the root's lexer is read while this parser's is changed: two distinct parsers
            let root_lexer = std::mem::take(&mut self.ps[r].lexer);
            let result = self.with(p, |l, io| read_token(l, io, &CLocale, Some(&root_lexer), out, to_end, skip));
            self.ps[r].lexer = root_lexer;
            result
        };
        match result {
            TokenResult::False => false,
            TokenResult::True => true,
            TokenResult::EndQuote => {
                self.set_error(p, b"Expecting end quote", false);
                false
            }
            TokenResult::ClosingDollar => {
                self.set_error(p, b"Expecting closing '$'", false);
                false
            }
            TokenResult::DefineNotFound(name) => {
                self.set_error(p, &[b"Define not found: ", &name[..]].concat(), false);
                false
            }
        }
    }

    fn narrow(&mut self, p: usize, to_end: bool, skip: bool) -> Option<Vec<u8>> {
        let mut t = Vec::new();
        self.token(p, &mut Token::Narrow(&mut t), to_end, skip).then_some(t)
    }

    fn eof(&mut self, p: usize) -> bool {
        self.with(p, |l, io| l.end_of_file(io))
    }

    fn peek(&mut self, p: usize) -> i32 {
        self.with(p, |l, io| l.peek_char(io))
    }

    fn get(&mut self, p: usize) -> i32 {
        self.with(p, |l, io| l.get_char(io))
    }

    fn unget(&mut self, p: usize, c: i32) {
        self.with(p, |l, io| l.unget_char(io, c))
    }

    fn defines(&mut self, p: usize) {
        self.add_define(p, b"A", b"va", false);
        self.add_define(p, b"a", b"dup", false);
        self.add_define(p, b"B", b"v\xc3\xa9", true);
        self.add_define(p, b"b", b"vb", true);
        self.add_define(p, b"b c", b"x", false);
        self.add_define(p, b"C", b"", false);
    }

    fn end(&mut self, p: usize, result: bool) -> String {
        let eof = self.eof(p);
        let q = &self.ps[p];
        format!(
            "end {result} line {} count {} eof {} abort {} warn {}",
            q.line,
            ee_won_config_line_count(),
            eof as i32,
            esc(&q.abort),
            esc(&q.warnings)
        )
    }

    fn open_line(&mut self, p: usize, name: &str, out: &mut Vec<String>) {
        ee_won_config_reset_line_count();
        let ok = self.open(p, name);
        let q = &self.ps[p];
        out.push(format!("open {ok} unicode {} path {} line {}", q.unicode as i32, esc(&q.file_path), q.line));
    }
}

fn file_name(case: usize, unicode: bool) -> String {
    format!("won_cfg_{case}{}.cfg", if unicode { "u" } else { "" })
}

fn run_case(w: &mut World, case: usize, unicode: bool, out: &mut Vec<String>) {
    let head = format!("c {case}{} ", if unicode { "u" } else { "" });
    for pass in 0..6 {
        let p = w.new_parser();
        w.defines(p);
        w.open_line(p, &file_name(case, unicode), out);
        let mut result = false;
        let mut count = 0;
        if pass == 0 || pass == 1 || pass == 4 {
            while count < 60 {
                let to_end = (pass == 1 || pass == 4) && count & 1 != 0;
                match w.narrow(p, to_end, pass == 4) {
                    Some(t) => {
                        result = true;
                        out.push(format!("{head}p{pass} tok {} line {}", esc(&t), w.ps[p].line));
                    }
                    None => {
                        result = false;
                        break;
                    }
                }
                count += 1;
            }
        } else if pass == 2 || pass == 3 {
            while count < 60 {
                let mut t = Vec::new();
                result = w.token(p, &mut Token::Wide(&mut t, 2), pass == 3 && count & 1 != 0, false);
                if !result {
                    break;
                }
                out.push(format!("{head}p{pass} wtok {} line {}", units(&t), w.ps[p].line));
                count += 1;
            }
        } else {
            w.with(p, |l, io| l.unget_narrow(io, b"q\n\xe9"));
            let mut chars = String::new();
            let mut unget = false;
            while count < 200 {
                let peek = w.peek(p);
                if w.eof(p) {
                    break;
                }
                let ch = w.get(p);
                chars += &format!("{peek}/{ch} ");
                if ch == 0x0a && !unget {
                    w.unget(p, ch);
                    w.unget(p, -1);
                    unget = true;
                    chars += &format!("u{} ", w.ps[p].line);
                }
                count += 1;
            }
            let peek = w.peek(p);
            let get = w.get(p);
            chars += &format!("peek {peek} get {get}");
            out.push(format!("{head}p5 chars {chars}"));
        }
        let end = w.end(p, result);
        out.push(format!("{head}p{pass} {end}"));
        w.ps[p].reader.close();
    }
}

fn found(v: &Option<Vec<u8>>) -> String {
    match v {
        Some(v) => format!("1 {}", esc(v)),
        None => format!("0 {}", esc(b"unset")),
    }
}

fn run_scenarios(w: &mut World, out: &mut Vec<String>) {
    let head = "c s ";
    // define scopes
    {
        let p = w.new_parser();
        let mut last = b"unset".to_vec();
        let look = |w: &World, name: &[u8], last: &mut Vec<u8>| -> String {
            match w.get_define(p, name) {
                Some(v) => {
                    *last = v.clone();
                    format!("1 {}", esc(&v))
                }
                None => format!("0 {}", esc(last)),
            }
        };
        out.push(format!("{head}get-empty {}", look(w, b"A", &mut last)));
        w.pop(p);
        w.push(p);
        out.push(format!("{head}get-null-scope {}", look(w, b"A", &mut last)));
        w.defines(p);
        w.push(p);
        w.add_define(p, b"X", b"inner", false);
        w.add_define(p, b"A", b"shadow", false);
        out.push(format!("{head}inner-x {}", look(w, b"x", &mut last)));
        out.push(format!("{head}inner-a {}", look(w, b"a", &mut last)));
        let wide = w.get_define_wide(p, b"B");
        out.push(format!("{head}inner-b-wide {}", wide.map(|u| format!("1 {}", units(&u))).unwrap()));
        w.push(p);
        out.push(format!("{head}empty-scope-a {}", look(w, b"A", &mut last)));
        w.pop(p);
        w.pop(p);
        last = b"unset".to_vec();
        out.push(format!("{head}outer-x {}", look(w, b"X", &mut last)));
        out.push(format!("{head}outer-a {}", look(w, b"A", &mut last)));
        out.push(format!("{head}outer-c {}", look(w, b"c", &mut last)));
        out.push(format!("{head}outer-bc {}", look(w, b"b c", &mut last)));
        let added = w.add_define(p, b"\xe9", b"1", false);
        let line = format!("{head}case-e9 {} {}", added as i32, look(w, b"\xc9", &mut last));
        out.push(line);
        w.pop(p);
        w.pop(p);
        let popped = look(w, b"A", &mut last);
        out.push(format!("{head}popped {popped} warn {}", esc(&w.ps[p].warnings)));
    }
    // a child's defines and warnings go to the root
    {
        let root = w.new_parser();
        let child = w.new_parser();
        let grand = w.new_parser();
        w.ps[root].warnings_as_errors = true;
        w.set_parent(child, root);
        w.set_parent(grand, child);
        w.add_define(grand, b"G", b"vg", false);
        w.add_define(grand, b"g", b"dup", false);
        let g = w.get_define(root, b"g");
        out.push(format!(
            "{head}root-g {} root-warn {} root-abort {} child-warn {} child-flags {}",
            found(&g).replace("0 |unset|", "0 ||"),
            esc(&w.ps[root].warnings),
            esc(&w.ps[root].abort),
            esc(&w.ps[grand].warnings),
            w.ps[grand].warnings_as_errors as i32
        ));
        w.push(child);
        w.add_define(grand, b"H", b"vh", false);
        w.pop(child);
        let h = w.get_define(root, b"H");
        out.push(format!(
            "{head}root-h {}",
            h.map(|v| format!("1 {}", esc(&v))).unwrap_or(format!("0 {}", esc(b"vg")))
        ));
    }
    // group defines and saved states
    {
        let p = w.new_parser();
        w.open_line(p, &file_name(1, false), out);
        w.narrow(p, false, false);
        w.unget(p, b'Z' as i32);
        let a = w.add_group(p, b"G");
        let b = w.add_group(p, b"g");
        let c = w.add_group(p, b"s p");
        out.push(format!("{head}group-add {} {} {} warn {}", a as i32, b as i32, c as i32, esc(&w.ps[p].warnings)));
        let mut read = String::new();
        for _ in 0..4 {
            match w.narrow(p, false, false) {
                Some(t) => read += &format!("{} ", esc(&t)),
                None => break,
            }
        }
        out.push(format!("{head}group-read {read}line {}", w.ps[p].line));
        let state = w.get_group(p, b"g").unwrap();
        let back: Vec<u32> = state.back.iter().map(|&u| u as u32).collect();
        out.push(format!(
            "{head}group-get 1 pos {} line {} back {} file {}",
            state.pos,
            state.line,
            units(&back),
            esc(&state.path)
        ));
        out.push(format!("{head}group-missing {}", w.get_group(p, b"none").is_some() as i32));
        w.set_state(p, &state);
        let mut read = String::new();
        for _ in 0..4 {
            match w.narrow(p, false, false) {
                Some(t) => read += &format!("{} ", esc(&t)),
                None => break,
            }
        }
        out.push(format!("{head}group-again {read}line {}", w.ps[p].line));
        w.open_line(p, &file_name(0, false), out);
        w.narrow(p, false, false);
        let other = w.get_state(p);
        let back: Vec<u32> = other.back.iter().map(|&u| u as u32).collect();
        out.push(format!("{head}state-0 pos {} line {} back {}", other.pos, other.line, units(&back)));
        let set = w.set_state(p, &state);
        out.push(format!("{head}set-1 {} path {}", set as i32, esc(&w.ps[p].name)));
        let mut read = String::new();
        for _ in 0..4 {
            match w.narrow(p, false, false) {
                Some(t) => read += &format!("{} ", esc(&t)),
                None => break,
            }
        }
        out.push(format!("{head}set-1-read {read}line {}", w.ps[p].line));
        let mut missing = state.clone();
        missing.path = b"won_cfg_none.cfg".to_vec();
        let set = w.set_state(p, &missing);
        out.push(format!(
            "{head}set-missing {} open {} line {}",
            set as i32,
            w.ps[p].reader.is_open() as i32,
            w.ps[p].line
        ));
        let child = w.new_parser();
        w.set_parent(child, p);
        w.open_line(child, &file_name(2, false), out);
        w.narrow(child, false, false);
        let added = w.add_group(child, b"K");
        let k = w.get_group(p, b"k").unwrap();
        out.push(format!(
            "{head}child-group {} root 1 pos {} line {} file {}",
            added as i32,
            k.pos,
            k.line,
            esc(&k.path)
        ));
        w.ps[p].reader.close();
        w.ps[child].reader.close();
    }
    // errors, warnings, reset
    {
        let p = w.new_parser();
        let a = w.abort_reason(p);
        out.push(format!("{head}abort-empty {}", esc(&a)));
        w.open_line(p, &file_name(1, false), out);
        w.add_warning(p, b"w1");
        w.set_error(p, b"e1", false);
        w.set_error(p, b"e2", false);
        w.set_error(p, b"e3", true);
        w.add_warning(p, b"w2");
        let a = w.abort_reason(p);
        out.push(format!("{head}errors {} warn {}", esc(&a), esc(&w.ps[p].warnings)));
        for _ in 0..3 {
            w.narrow(p, false, false);
        }
        w.reset(p);
        let eof = w.eof(p);
        out.push(format!("{head}reset abort {} line {} eof {}", esc(&w.ps[p].abort), w.ps[p].line, eof as i32));
        let t = w.narrow(p, false, false).unwrap();
        out.push(format!("{head}reset-read {} line {}", esc(&t), w.ps[p].line));
        w.ps[p].reader.close();
        let (eof, peek, get) = (w.eof(p), w.peek(p), w.get(p));
        out.push(format!("{head}closed eof {} peek {peek} get {get}", eof as i32));
        w.unget(p, b'x' as i32);
        let (eof, peek, get) = (w.eof(p), w.peek(p), w.get(p));
        let read = w.narrow(p, false, false).is_some();
        out.push(format!("{head}closed-unget eof {} peek {peek} get {get} read {}", eof as i32, read as i32));
        let m = w.new_parser();
        let ok = w.open(m, "won_cfg_dir/none.cfg");
        out.push(format!("{head}open-missing {} path {} line {}", ok as i32, esc(&w.ps[m].file_path), w.ps[m].line));
        let ok = w.open(m, "a\\b\\won_cfg_0.cfg");
        out.push(format!("{head}open-backslash {} path {}", ok as i32, esc(&w.ps[m].file_path)));
    }
}

#[test]
fn config_golden() {
    let _turn = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let dir = std::env::temp_dir().join(format!("ee_won_config_golden_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let mut w = World { dir: dir.clone(), ps: Vec::new() };
    let mut out = Vec::new();
    let inputs = inputs();
    for (i, input) in inputs.iter().enumerate() {
        std::fs::write(dir.join(file_name(i, false)), input).unwrap();
        let mut u = vec![0xff, 0xfe];
        for &b in input {
            u.extend_from_slice(&[b, 0]);
        }
        std::fs::write(dir.join(file_name(i, true)), u).unwrap();
        run_case(&mut w, i, false, &mut out);
        run_case(&mut w, i, true, &mut out);
    }
    run_scenarios(&mut w, &mut out);
    for p in &mut w.ps {
        p.reader.close();
    }
    let _ = std::fs::remove_dir_all(&dir);
    let want: Vec<&str> = GOLDEN.lines().filter(|l| l.starts_with("c ") || l.starts_with("open ")).collect();
    // the open lines of both passes interleave with the c lines; compare the C pass's prefix of the golden
    let c_part: Vec<&str> = GOLDEN.lines().take_while(|l| !l.starts_with("english ")).collect();
    let c_part: Vec<&str> = c_part.into_iter().filter(|l| !l.starts_with("open ") || true).collect();
    let _ = want;
    // the golden's english pass repeats the open lines; the C pass ends where the first english line starts,
    // less the open line printed just before it
    let mut expect = c_part;
    while expect.last().is_some_and(|l| l.starts_with("open ")) {
        expect.pop();
    }
    for (i, (g, e)) in out.iter().zip(&expect).enumerate() {
        assert_eq!(g, e, "line {}", i + 1);
    }
    assert_eq!(out.len(), expect.len());
}
