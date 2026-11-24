use super::*;
use std::cell::RefCell as Cell2;

thread_local! {
    static LOG: Cell2<Vec<(EeWonBrowserOp, isize)>> = const { Cell2::new(Vec::new()) };
    static TEXT: Cell2<Vec<u16>> = const { Cell2::new(Vec::new()) };
}

unsafe extern "C" fn host(_me: *mut c_void, op: EeWonBrowserOp, a: isize, b: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a)));
    match op {
        EeWonBrowserOp::NewScrollContainer => 77,
        EeWonBrowserOp::ViewAddText => {
            // SAFETY: b UTF-16 units at a (the 2-byte wchar_t this test uses)
            let s = unsafe { std::slice::from_raw_parts(a as *const u16, b as usize) };
            TEXT.with(|t| *t.borrow_mut() = s.to_vec());
            0
        }
        _ => 0,
    }
}

fn ops() -> Vec<EeWonBrowserOp> {
    LOG.with(|l| l.borrow_mut().drain(..).map(|(o, _)| o).collect())
}

fn utf16(s: &str) -> Vec<u32> {
    s.encode_utf16().map(u32::from).collect()
}

#[test]
fn the_constructor_makes_the_view_and_the_default_scroller_is_held_over_its_calls() {
    use EeWonBrowserOp::*;
    let b = EeWonBrowser::new(std::ptr::null_mut(), Some(host), 2);
    assert_eq!(ops(), [NewView, ViewNoScrollOnBottom]);
    assert_eq!(b.call(EeWonBrowserCall::FontSize, 0, 0, 0, 0, 0), EE_WON_BROWSER_FONT_MEDIUM as isize);
    b.added_to_parent();
    assert_eq!(
        ops(),
        [
            HasScroller,
            NewScrollContainer,
            Ref,
            ScrollbarsConditional,
            ScrollerAssign,
            ScrollerHasView,
            ScrollerSetView,
            SameSize,
            AddChild,
            Unref,
            BaseAddedToParent
        ]
    );
    b.set_transparent(true);
    assert_eq!(ops(), [ViewNoBackground, ViewGrabBg]);
    b.set_transparent(false);
    assert_eq!(ops(), [ViewGrabBg]);
}

#[test]
fn a_web_address_and_a_missing_file_are_shown_as_text_and_refresh_goes_again() {
    use EeWonBrowserOp::*;
    let b = EeWonBrowser::new(std::ptr::null_mut(), Some(host), 2);
    ops();
    b.go(&utf16("http://x/y"));
    assert_eq!(ops(), [ViewClear, ViewAddText, ViewTop, Invalidate]);
    let t = TEXT.with(|t| t.borrow().clone());
    assert_eq!(String::from_utf16(&t).unwrap(), "http://x/y\n\n(a web page: the lobby shows no web pages)");
    b.go(&utf16("no/such/page.html"));
    let t = TEXT.with(|t| t.borrow().clone());
    assert_eq!(String::from_utf16(&t).unwrap(), "(no page: no/such/page.html)");
    ops();
    b.refresh();
    assert_eq!(ops(), [ViewClear, ViewAddText, ViewTop, Invalidate]);
    b.reset();
    assert_eq!(ops(), [ViewClear, ViewTop, Invalidate]);
    b.refresh();
    assert!(ops().is_empty());
}

#[test]
fn show_files_joins_two_readable_pages_with_a_blank_line() {
    let dir = std::env::temp_dir().join(format!("ee_browserobj_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let (a, c) = (dir.join("a.txt"), dir.join("c.html"));
    std::fs::write(&a, "!first\r\nline").unwrap();
    std::fs::write(&c, "<html><body>second</body></html>").unwrap();
    let b = EeWonBrowser::new(std::ptr::null_mut(), Some(host), 2);
    let (pa, pc) = (a.to_str().unwrap().as_bytes().to_vec(), c.to_str().unwrap().as_bytes().to_vec());
    b.show_files(&pa, &pc, true);
    let t = String::from_utf16(&TEXT.with(|t| t.borrow().clone())).unwrap();
    assert!(t.starts_with("first\nline\n\n"), "{t:?}");
    assert!(t.contains("second"), "{t:?}");
    b.show_files(b"", &pa, false);
    let t = String::from_utf16(&TEXT.with(|t| t.borrow().clone())).unwrap();
    assert_eq!(t, "!first\nline");
    std::fs::remove_dir_all(&dir).ok();
}
