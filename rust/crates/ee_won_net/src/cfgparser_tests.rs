//! The parser object's own rules: the error and warning texts (their `snprintf` cuts), the parent walk of the
//! defines and group defines, the states and the open's names. The characters and tokens are `config`'s
//! (config_golden.rs replays the C++ parser's golden over them; `won_config_test` replays it over this object).

use super::*;
use crate::config::config_golden::LOCK;

fn file(name: &str, data: &[u8]) -> Vec<u8> {
    let dir = std::env::temp_dir().join(format!("ee_cfgparser_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join(name);
    std::fs::write(&path, data).unwrap();
    path.to_str().unwrap().as_bytes().to_vec()
}

fn token(p: &mut Parser) -> Option<Vec<u8>> {
    let mut t = Vec::new();
    let (ok, _) = p.read_token(&mut Token::Narrow(&mut t), false, false);
    ok.then_some(t)
}

#[test]
fn errors_name_the_file_and_line_and_keep_the_first() {
    let mut p = Parser::new();
    p.set_error(b"e1", false);
    p.set_error(b"e2", false);
    assert_eq!(p.abort, b"e1 (:0)");
    p.set_error(b"e3", true);
    assert_eq!(p.abort, b"e1 (:0)e3 (:0)");
    p.add_warning(b"ignored");
    assert!(p.warnings.is_empty(), "no warning once aborted");
}

#[test]
fn abort_reason_defaults_to_parse_aborted() {
    let mut p = Parser::new();
    assert_eq!(p.abort_reason(), b"Parse aborted. (:0)");
}

#[test]
fn warnings_cut_at_their_nul_and_at_1023_bytes() {
    let mut p = Parser::new();
    p.file_name_and_path = b"dir/a\0b.cfg".to_vec();
    p.line = 7;
    p.add_warning(b"w\0hidden");
    assert_eq!(p.warnings, b"Warning (dir/a: 7) - w\n");
    let mut q = Parser::new();
    q.add_warning(&vec![b'x'; 2000]);
    assert_eq!(q.warnings.len(), 1023);
    assert!(q.warnings.starts_with(b"Warning (: 0) - xxx"));
    assert_eq!(*q.warnings.last().unwrap(), b'x', "the newline is past the cut");
    let mut r = Parser::new();
    r.file_name_and_path = vec![b'f'; 1100];
    r.set_error(b"e\0e", false);
    assert_eq!(&r.abort[..5], b"e\0e (");
    assert_eq!(r.abort.len(), 3 + 1023);
}

#[test]
fn warnings_as_errors_set_the_error_first() {
    let mut p = Parser::new();
    p.warnings_as_errors = true;
    p.add_warning(b"w");
    assert_eq!(p.abort, b"w (:0)");
    assert_eq!(p.warnings, b"Warning (: 0) - w\n");
}

#[test]
fn defines_live_on_the_root_and_warn_there() {
    let mut root = Parser::new();
    root.warnings_as_errors = false;
    root.ignore_path_on_include = true;
    let mut child = Parser::new();
    unsafe { child.set_parent(&mut root) };
    assert!(child.ignore_path_on_include);
    let mut grandchild = Parser::new();
    unsafe { grandchild.set_parent(&mut child) };
    grandchild.push_define_stack();
    assert!(grandchild.add_define(b"A", b"1", false));
    assert!(grandchild.add_define(b"a", b"2", false));
    assert!(grandchild.add_define(b"b c", b"3", false));
    assert_eq!(root.get_define(b"A"), Some(b"1".to_vec()));
    assert_eq!(child.get_define(b"a"), Some(b"1".to_vec()));
    assert_eq!(
        root.warnings,
        b"Warning (: 0) - Redefintion of a\nWarning (: 0) - Not allowed to have spaces in define names.\n"
    );
    assert!(grandchild.warnings.is_empty());
    grandchild.add_define(b"a", b"2", true);
    assert_eq!(root.get_define(b"A"), Some(b"2".to_vec()));
    grandchild.pop_define_stack();
    assert_eq!(root.get_define(b"A"), None);
}

#[test]
fn group_defines_warn_spaces_here_and_redefinitions_on_the_root() {
    let mut root = Parser::new();
    let mut child = Parser::new();
    unsafe { child.set_parent(&mut root) };
    let s = SavedState { pos: 3, path: b"x".to_vec(), line: 4, back: vec![0x41] };
    child.add_group_define(b"G", Some(&s));
    child.add_group_define(b"g", Some(&s));
    child.add_group_define(b"g h", None);
    assert_eq!(root.get_group_define(b"G"), Some(s));
    assert_eq!(root.warnings, b"Warning (: 0) - Redefintion of g\n");
    assert_eq!(child.warnings, b"Warning (: 0) - Not allowed to have spaces in define names.\n");
}

#[test]
fn open_notes_the_directory_and_reads_tokens() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let path = file("a.cfg", b"one two\nthree");
    let mut p = Parser::new();
    assert_eq!(p.open_file(&path), (true, 0));
    let sep = path.iter().rposition(|&b| b == b'\\' || b == b'/').unwrap();
    assert_eq!(p.file_path, path[..=sep].to_vec());
    assert_eq!(p.line, 1);
    assert_eq!(token(&mut p), Some(b"one".to_vec()));
    let state = p.state();
    assert_eq!(token(&mut p), Some(b"two".to_vec()));
    assert_eq!(token(&mut p), Some(b"three".to_vec()));
    assert_eq!(p.line, 2);
    assert_eq!(token(&mut p), None);
    assert_eq!(p.set_state(&state), (true, 0));
    assert_eq!(token(&mut p), Some(b"two".to_vec()));
    let missing = SavedState { path: b"no such file.cfg".to_vec(), ..state };
    assert_eq!(p.set_state(&missing), (false, 0));
    assert!(!p.reader.is_open());
    let mut q = Parser::new();
    assert_eq!(q.open_file(b"bare.cfg"), (false, 0));
    assert!(q.file_path.is_empty());
    assert_eq!(q.file_name_and_path, b"bare.cfg");
}

#[test]
fn token_errors_are_set_on_the_reading_parser() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let path = file("b.cfg", b"$missing$ \"open");
    let mut root = Parser::new();
    let mut p = Parser::new();
    unsafe { p.set_parent(&mut root) };
    p.open_file(&path);
    let mut t = Vec::new();
    assert_eq!(p.read_token(&mut Token::Narrow(&mut t), false, false), (false, true));
    assert!(p.abort.starts_with(b"Define not found: missing ("));
    assert!(root.abort.is_empty());
    p.reset();
    assert!(p.abort.is_empty());
    assert_eq!(p.line, 1);
    let mut t = Vec::new();
    assert_eq!(p.read_token(&mut Token::Narrow(&mut t), false, false), (false, true));
}

#[test]
fn the_c_abi_reads_fields_and_texts() {
    let p = ee_won_cfgp_new();
    unsafe {
        ee_won_cfgp_set_field(p, EeWonCfgpField::Stopped, 1);
        assert_eq!(ee_won_cfgp_field(p, EeWonCfgpField::Stopped), 1);
        assert_eq!(ee_won_cfgp_field(p, EeWonCfgpField::NoError), 1);
        ee_won_cfgp_set_field(p, EeWonCfgpField::NoError, 0);
        assert_eq!(ee_won_cfgp_field(p, EeWonCfgpField::NoError), 1);
        ee_won_cfgp_set_error(p, b"x".as_ptr(), 1, false);
        assert_eq!(ee_won_cfgp_field(p, EeWonCfgpField::NoError), 0);
        ee_won_cfgp_copy_warnings(p, p);
        ee_won_cfgp_free(p);
    }
}
