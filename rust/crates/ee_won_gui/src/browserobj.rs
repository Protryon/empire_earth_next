//! WONGUI's BrowserComponent as a whole Rust object (R6 WON round 34): the lobby's page view (the message of the
//! day, the terms of use, the help and news tabs). WON's HTML rendering is not ported (docs/RUST_PLAN.md D8): a page
//! is plain text in a TextArea, an HTML page with its tags dropped and entities decoded (ee_won_net's plaintext.rs);
//! a web address is shown as text (the WON servers are gone).
//!
//! The record holds the class's data (the font size a layout gives, the background flag, the current location) and
//! runs every method (the constructor's view, SetScroller, AddedToParent's default scroller, SetTransparent,
//! SetPageText, Reset, Go, ShowFiles, Refresh, reading a page file). The C++ `BrowserComponent` keeps its C++ objects
//! (the text view and the scroller, `SmartPtr`s) and the Container base; its methods are one-line forwarders and each
//! of its host's ops (`EeWonBrowserOp`, WONGUI/BrowserComponent.cpp) one of the original's statements.

use std::cell::RefCell;
use std::ffi::{c_char, c_void};

use crate::guistring::{narrow, widen};
use ee_won_net::http::{Url, EE_WON_URL_FILE, EE_WON_URL_FTP, EE_WON_URL_HTTP, EE_WON_URL_NONE};
use ee_won_net::wonfile::EeWonFile;

/// `HTMLFontSize_Medium`, the constructor's font size.
pub const EE_WON_BROWSER_FONT_MEDIUM: i32 = 2;

/// The inward calls (the C++ class's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonBrowserCall {
    /// `AddedToParent()`.
    AddedToParent = 1,
    /// `SetTransparent(a != 0)`.
    SetTransparent,
    /// `SetScroller((Scroller*)a)`.
    SetScroller,
    /// `SetFontSize((HTMLFontSize)a)`.
    SetFontSize,
    /// `Reset()`.
    Reset,
    /// `Refresh()`.
    Refresh,
    /// `Go(location)`: the location's `b` units of `wchar_t` at `a`.
    Go,
    /// `ShowFiles(first, second, e != 0)`: the first path's `b` bytes at `a`, the second's `d` at `c`.
    ShowFiles,
    /// the font size (`mFontSize`).
    FontSize,
    /// the background flag (`mHaveBackground`, 0 or 1).
    HaveBackground,
}

/// The host's calls (`t` the C++ `BrowserComponent*`; one statement each).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonBrowserOp {
    /// `t->mHTMLView = new TextArea`.
    NewView = 1,
    /// `t->mHTMLView->SetTextFlags(TextFlag_ScrollOnBottom, false)`.
    ViewNoScrollOnBottom,
    /// `t->mScroller = (Scroller*)a`.
    ScrollerAssign,
    /// `((Scroller*)a)->GetScrollArea() == t->mHTMLView.get()` (0 or 1).
    ScrollerHasView,
    /// `((Scroller*)a)->SetScrollArea(t->mHTMLView.get())`.
    ScrollerSetView,
    /// `t->Fn2F6140((Scroller*)a, 3, t, 0, 0, 0, 0, 0)` (CLI_SameSize: the scroller sized to the view's parent).
    SameSize,
    /// `t->AddChild((Scroller*)a)`.
    AddChild,
    /// `t->mScroller.get() != NULL` (0 or 1).
    HasScroller,
    /// a new MSScrollContainer kind around the view, as a `Scroller*` with no reference taken.
    NewScrollContainer,
    /// `((Scroller*)a)->AddRef()`.
    Ref,
    /// `((Scroller*)a)->Release()`.
    Unref,
    /// `static_cast<ScrollbarScroller*>((Scroller*)a)->SetScrollbarConditions(Conditional, Conditional)`.
    ScrollbarsConditional,
    /// `t->Container::AddedToParent()`.
    BaseAddedToParent,
    /// `t->mHTMLView->SetBackground(Background(-1, -1))`.
    ViewNoBackground,
    /// `t->mHTMLView->SetComponentFlags(ComponentFlag_GrabBG, a != 0)`.
    ViewGrabBg,
    /// `t->mHTMLView->Clear()`.
    ViewClear,
    /// `t->mHTMLView->AddFormatedText(GUIString(std::wstring((const wchar_t*)a, b)), false)`.
    ViewAddText,
    /// `t->mHTMLView->SetOffsets(0, 0)`.
    ViewTop,
    /// `t->Invalidate()`.
    Invalidate,
}

/// a void call's answer (the C++ case's `return 0`)
fn z(_: isize) -> isize {
    0
}

/// Where the game's BrowserComponent keeps its view (`mHTMLView`, a SmartPtr; 0 before GuiHost.cpp registers it).
static VIEW_AT: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);

/// Registers the view's offset (GuiHost.cpp at static initialisation).
#[no_mangle]
pub extern "C" fn ee_won_browser_view_register(offset: usize) {
    VIEW_AT.store(offset, std::sync::atomic::Ordering::Relaxed);
}

/// The host: `me` the C++ object, `op`, two arguments.
pub type EeWonBrowserHostFn =
    Option<unsafe extern "C" fn(me: *mut c_void, op: EeWonBrowserOp, a: isize, b: isize) -> isize>;

/// cbindgen:ignore
mod stdio {
    use std::ffi::{c_char, c_int, c_void};
    extern "C" {
        pub fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub fn fclose(file: *mut c_void) -> c_int;
        pub fn fread(data: *mut c_void, size: usize, count: usize, file: *mut c_void) -> usize;
    }
}

const KINDS: [i32; 4] = [EE_WON_URL_NONE, EE_WON_URL_HTTP, EE_WON_URL_FTP, EE_WON_URL_FILE];

#[derive(Debug, Default)]
struct State {
    font_size: i32,
    have_background: bool,
    /// `mCurLocation`'s units.
    location: Vec<u32>,
}

/// The Rust BrowserComponent.
pub struct EeWonBrowser {
    me: *mut c_void,
    host: EeWonBrowserHostFn,
    wchar_size: u32,
    st: RefCell<State>,
    /// the game's scroller (the C++ member `mScroller` until R6 WON GUI round 40), held; 0 for none
    scroller: std::cell::Cell<isize>,
}

impl Drop for EeWonBrowser {
    fn drop(&mut self) {
        let s = self.scroller.replace(0);
        if s != 0 {
            crate::gamewin::rf(s, false);
        }
    }
}

/// A page file's bytes (`PlatFOpen(path, "rb")`, read in 4,096-byte pieces to the end); None when it cannot be
/// opened. The path is a C string: it ends at its first NUL.
fn read_file(path: &[u8]) -> Option<Vec<u8>> {
    let end = path.iter().position(|&c| c == 0).unwrap_or(path.len());
    let mut p: Vec<u8> = path[..end].to_vec();
    p.push(0);
    // SAFETY: a NUL-terminated path and mode; the stream is read and closed here
    unsafe {
        let file = stdio::PlatFOpen(p.as_ptr() as *const c_char, c"rb".as_ptr());
        if file.is_null() {
            return None;
        }
        let mut bytes = Vec::new();
        let mut buffer = [0u8; 4096];
        loop {
            let n = stdio::fread(buffer.as_mut_ptr() as *mut c_void, 1, buffer.len(), file);
            if n == 0 {
                break;
            }
            bytes.extend_from_slice(&buffer[..n]);
        }
        stdio::fclose(file);
        Some(bytes)
    }
}

/// `ReadPage`: a page file's text (an HTML page converted) as `wchar_t` units; None when it cannot be read.
pub fn read_page(path: &[u8], skip_first_if_text: bool) -> Option<Vec<u32>> {
    let bytes = read_file(path)?;
    let page = ee_won_net::plaintext::page_text(&bytes, skip_first_if_text);
    Some(page.chars().map(|c| c as u32).collect())
}

/// `Go`'s text for a location (narrow, UTF-8): a web address as text, a file's text, or the no-page line, as
/// `wchar_t` units (UTF-16 ones for 2-byte `wchar_t`).
pub fn go_text(location: &[u8], wchar_size: u32) -> Vec<u32> {
    let url = Url::parse(location, KINDS);
    if url.kind == EE_WON_URL_HTTP || url.kind == EE_WON_URL_FTP {
        let mut s = location.to_vec();
        s.extend_from_slice(b"\n\n(a web page: the lobby shows no web pages)");
        return gui_text(&s, wchar_size);
    }
    let path = if url.kind == EE_WON_URL_FILE { EeWonFile::new(&url.path).path } else { location.to_vec() };
    match read_page(&path, false) {
        Some(text) => wide(&text, wchar_size),
        None => {
            let mut s = b"(no page: ".to_vec();
            s.extend_from_slice(location);
            s.push(b')');
            gui_text(&s, wchar_size)
        }
    }
}

/// `std::wstring(GUIString(narrow))`: the decoded units, as many as the GUIString's 16-bit length keeps.
fn gui_text(narrow: &[u8], wchar_size: u32) -> Vec<u32> {
    let mut v = widen(narrow, wchar_size);
    v.truncate(usize::from(v.len() as u16));
    v
}

/// Code points as `wchar_t` units: UTF-16 for 2-byte `wchar_t` (as ee_won_wide_from_utf16_sink gives them).
fn wide(text: &[u32], wchar_size: u32) -> Vec<u32> {
    if wchar_size != 2 {
        return text.to_vec();
    }
    let s: String = text.iter().filter_map(|&c| char::from_u32(c)).collect();
    s.encode_utf16().map(u32::from).collect()
}

impl EeWonBrowser {
    /// The constructor: an empty, word-wrapped, read-only text view (the host's `NewView`), medium font, no
    /// background.
    pub fn new(me: *mut c_void, host: EeWonBrowserHostFn, wchar_size: u32) -> Self {
        let b = EeWonBrowser {
            me,
            host,
            wchar_size,
            st: RefCell::new(State { font_size: EE_WON_BROWSER_FONT_MEDIUM, ..State::default() }),
            scroller: std::cell::Cell::new(0),
        };
        b.op(EeWonBrowserOp::NewView, 0, 0);
        b.op(EeWonBrowserOp::ViewNoScrollOnBottom, 0, 0);
        b
    }

    fn op(&self, o: EeWonBrowserOp, a: isize, b: isize) -> isize {
        if let Some(r) = self.game_op(o, a, b) {
            return r;
        }
        match self.host {
            // SAFETY: the host was given with this object; no borrow of the state is live across a call
            Some(h) => unsafe { h(self.me, o, a, b) },
            None => 0,
        }
    }

    /// The game's answers (R6 WON GUI round 40): the scroller held here, the references the game's, the component's
    /// Invalidate its own (no class overrides it). None in a driver's world and for the others.
    fn game_op(&self, o: EeWonBrowserOp, a: isize, b: isize) -> Option<isize> {
        use EeWonBrowserOp as B;
        if !crate::compobj::is_game() {
            return None;
        }
        if let Some(r) = self.view_op(o, a, b) {
            return Some(r);
        }
        if !matches!(o, B::ScrollerAssign | B::HasScroller | B::Ref | B::Unref | B::Invalidate) {
            return None;
        }
        Some(match o {
            B::ScrollerAssign => {
                if a != 0 {
                    crate::gamewin::rf(a, true);
                }
                let old = self.scroller.replace(a);
                if old != 0 {
                    crate::gamewin::rf(old, false);
                }
                0
            }
            B::HasScroller => isize::from(self.scroller.get() != 0),
            B::Ref => crate::gamewin::rf(a, true)?,
            B::Unref => crate::gamewin::rf(a, false)?,
            _ => {
                let k = crate::compobj::EeWonCompCall::Invalidate;
                // SAFETY: the game's live browser component; the call re-enters through Component's entry
                unsafe { crate::compobj::ee_won_comp_call(self.me, crate::compobj::host(), k, 0, 0, 0) };
                0
            }
        })
    }

    /// The game's calls on the view (`mHTMLView`, read in place) and on the scroller `a` (R6 WON GUI round 41:
    /// GuiHost.cpp's cases went): each the method's own Rust object, found by the widget in the registry.
    fn view_op(&self, o: EeWonBrowserOp, a: isize, b: isize) -> Option<isize> {
        use crate::registry::{get, EeWonGuiKind as K};
        use crate::scrollerobj::{EeWonScroller, EeWonScrollerCall as S};
        use crate::textobj::{EeWonText, EeWonTextCall as T};
        use EeWonBrowserOp as B;
        let off = VIEW_AT.load(std::sync::atomic::Ordering::Relaxed);
        if off == 0 || self.me.is_null() {
            return None;
        }
        // SAFETY: the game's live browser, its view SmartPtr at the registered offset
        let view = unsafe { *((self.me as usize + off) as *const isize) };
        // SAFETY (the look-ups): the game's live widgets hold their Rust objects
        let text = || unsafe { get::<EeWonText>(view, K::Text) };
        // SAFETY: as above
        let scroller = || unsafe { get::<EeWonScroller>(a, K::Scroller) };
        Some(match o {
            B::ViewNoScrollOnBottom => z(text()?.call(T::SetTextFlags, 4, 0, 0)),
            B::ViewClear => z(text()?.call(T::Reset, 0, 0, 0)),
            B::ScrollerHasView => isize::from(scroller()?.call(S::GetScrollArea, 0, 0, 0) == view),
            B::ScrollerSetView => z(scroller()?.call(S::SetScrollArea, view, 0, 0)),
            // `ScrollbarCondition_Conditional` both ways
            B::ScrollbarsConditional => z(scroller()?.call(S::SetScrollbarConditions, 2, 2, 0)),
            B::AddChild => {
                // SAFETY: as above
                let ct = unsafe { get::<crate::ctobj::EeWonContainer>(self.me as isize, K::Container) }?;
                z(ct.call(crate::ctobj::EeWonCtCall::AddChild, a, 0, 0))
            }
            B::ViewGrabBg => {
                // `SetComponentFlags(ComponentFlag_GrabBG, a != 0)`: Component's own
                let k = crate::compobj::EeWonCompCall::SetFlags;
                // SAFETY: the game's live view; the call re-enters through Component's entry
                unsafe { crate::compobj::ee_won_comp_call(view as *mut c_void, crate::compobj::host(), k, 0x100, a, 0) };
                0
            }
            // `ScrollArea::SetOffsets(0, 0)`: the view's scroll pane
            B::ViewTop => {
                // SAFETY: as above
                let p = unsafe { get::<crate::paneobj::EeWonScrollPane>(view, K::Pane) }?;
                z(p.call(crate::paneobj::EeWonPaneCall::SetOffsets, 0, 0, 0))
            }
            // `AddFormatedText(GUIString(units), false)` (the original's local GUIString, Rust's)
            B::ViewAddText => {
                let n = usize::try_from(b).ok()?;
                let units: Vec<u32> = if self.wchar_size == 2 {
                    // SAFETY: `b` u16 units at `a` (the op's contract)
                    unsafe { std::slice::from_raw_parts(a as *const u16, n) }.iter().map(|&c| u32::from(c)).collect()
                } else {
                    // SAFETY: `b` u32 units at `a`
                    unsafe { std::slice::from_raw_parts(a as *const u32, n) }.to_vec()
                };
                let mut g = crate::guistrobj::GText::new(&units);
                // SAFETY: the game's live text area and its game host; the text lives for the call
                unsafe {
                    crate::guitext::ee_won_gui_add_formatted(
                        view as *mut c_void,
                        Some(crate::guisplit::ee_won_gui_text_game),
                        g.arg() as *const c_void,
                        a as *const c_void,
                        n as u16,
                        self.wchar_size,
                        false,
                    )
                };
                0
            }
            _ => return None,
        })
    }

    /// `SetScroller`: the scroller held, around the view, sized to it and added as a child.
    pub fn set_scroller(&self, s: isize) {
        self.op(EeWonBrowserOp::ScrollerAssign, s, 0);
        if self.op(EeWonBrowserOp::ScrollerHasView, s, 0) == 0 {
            self.op(EeWonBrowserOp::ScrollerSetView, s, 0);
        }
        self.op(EeWonBrowserOp::SameSize, s, 0);
        self.op(EeWonBrowserOp::AddChild, s, 0);
    }

    /// `AddedToParent`: a default scroller (scroll bars when needed) if the layout gave none, then the container's.
    pub fn added_to_parent(&self) {
        if self.op(EeWonBrowserOp::HasScroller, 0, 0) == 0 {
            // the original's SmartPtr local: a reference over the calls
            let s = self.op(EeWonBrowserOp::NewScrollContainer, 0, 0);
            self.op(EeWonBrowserOp::Ref, s, 0);
            self.op(EeWonBrowserOp::ScrollbarsConditional, s, 0);
            self.set_scroller(s);
            self.op(EeWonBrowserOp::Unref, s, 0);
        }
        self.op(EeWonBrowserOp::BaseAddedToParent, 0, 0);
    }

    /// `SetTransparent`: the parent's background shows through the text.
    pub fn set_transparent(&self, transparent: bool) {
        self.st.borrow_mut().have_background = transparent;
        if transparent {
            self.op(EeWonBrowserOp::ViewNoBackground, 0, 0);
        }
        let have = self.st.borrow().have_background;
        self.op(EeWonBrowserOp::ViewGrabBg, have as isize, 0);
    }

    /// `SetPageText`: the text shown from the top.
    pub fn set_page_text(&self, text: &[u32]) {
        self.op(EeWonBrowserOp::ViewClear, 0, 0);
        if !text.is_empty() {
            if self.wchar_size == 2 {
                let u: Vec<u16> = text.iter().map(|&c| c as u16).collect();
                self.op(EeWonBrowserOp::ViewAddText, u.as_ptr() as isize, u.len() as isize);
            } else {
                self.op(EeWonBrowserOp::ViewAddText, text.as_ptr() as isize, text.len() as isize);
            }
        }
        self.op(EeWonBrowserOp::ViewTop, 0, 0);
        self.op(EeWonBrowserOp::Invalidate, 0, 0);
    }

    /// `Reset`: the view emptied.
    pub fn reset(&self) {
        self.st.borrow_mut().location.clear();
        self.set_page_text(&[]);
    }

    /// `Go`: the page at a location (`wchar_t` units).
    pub fn go(&self, location: &[u32]) {
        self.st.borrow_mut().location = location.to_vec();
        let text = go_text(&narrow(location, self.wchar_size), self.wchar_size);
        self.set_page_text(&text);
    }

    /// `ShowFiles`: the readable ones of up to two page files, one after the other with a blank line between them.
    pub fn show_files(&self, first: &[u8], second: &[u8], skip_first_if_text: bool) {
        let read = |p: &[u8]| if p.is_empty() { None } else { read_page(p, skip_first_if_text) };
        let (a, b) = (read(first), read(second));
        let (ha, hb) = (a.is_some(), b.is_some());
        let (a, b) = (a.unwrap_or_default(), b.unwrap_or_default());
        let mut text = a.clone();
        if ha && hb && !a.is_empty() && !b.is_empty() {
            text.extend([u32::from(b'\n'), u32::from(b'\n')]);
        }
        text.extend_from_slice(&b);
        self.set_page_text(&wide(&text, self.wchar_size));
    }

    /// `Refresh`: the current location again.
    pub fn refresh(&self) {
        let location = self.st.borrow().location.clone();
        if !location.is_empty() {
            self.go(&location);
        }
    }

    fn call(&self, c: EeWonBrowserCall, a: isize, b: isize, cc: isize, d: isize, e: isize) -> isize {
        match c {
            EeWonBrowserCall::AddedToParent => self.added_to_parent(),
            EeWonBrowserCall::SetTransparent => self.set_transparent(a != 0),
            EeWonBrowserCall::SetScroller => self.set_scroller(a),
            EeWonBrowserCall::SetFontSize => self.st.borrow_mut().font_size = a as i32,
            EeWonBrowserCall::Reset => self.reset(),
            EeWonBrowserCall::Refresh => self.refresh(),
            EeWonBrowserCall::Go => {
                // SAFETY: the call's contract
                let units = unsafe { units(a as *const c_void, b as usize, self.wchar_size) };
                self.go(&units);
            }
            EeWonBrowserCall::ShowFiles => {
                // SAFETY: the call's contract
                let (f, s) = unsafe { (bytes(a, b), bytes(cc, d)) };
                self.show_files(&f, &s, e != 0);
            }
            EeWonBrowserCall::FontSize => return self.st.borrow().font_size as isize,
            EeWonBrowserCall::HaveBackground => return self.st.borrow().have_background as isize,
        }
        0
    }
}

unsafe fn units(p: *const c_void, n: usize, wchar_size: u32) -> Vec<u32> {
    if p.is_null() || n == 0 {
        return Vec::new();
    }
    // SAFETY: `n` units of `wchar_size` bytes at `p` (the caller's contract; unaligned reads)
    unsafe {
        if wchar_size == 2 {
            (0..n).map(|i| u32::from((p as *const u16).add(i).read_unaligned())).collect()
        } else {
            (0..n).map(|i| (p as *const u32).add(i).read_unaligned()).collect()
        }
    }
}

unsafe fn bytes(p: isize, n: isize) -> Vec<u8> {
    if p == 0 || n <= 0 {
        return Vec::new();
    }
    // SAFETY: `n` bytes at `p` (the caller's contract)
    unsafe { std::slice::from_raw_parts(p as *const u8, n as usize) }.to_vec()
}

/// The constructor (`BrowserComponent::BrowserComponent`): the record and the view the host makes.
#[no_mangle]
pub extern "C" fn ee_won_browser_new(me: *mut c_void, host: EeWonBrowserHostFn, wchar_size: u32) -> *mut EeWonBrowser {
    Box::into_raw(Box::new(EeWonBrowser::new(me, host, wchar_size)))
}

/// # Safety
/// `b` is null or from `ee_won_browser_new`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_browser_free(b: *mut EeWonBrowser) {
    if !b.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(b) });
    }
}

/// One of the class's methods (`EeWonBrowserCall`).
///
/// # Safety
/// `b` is from `ee_won_browser_new`; the arguments as the call says.
#[no_mangle]
pub unsafe extern "C" fn ee_won_browser_call(
    b: *const EeWonBrowser,
    c: EeWonBrowserCall,
    a: isize,
    bb: isize,
    cc: isize,
    d: isize,
    e: isize,
) -> isize {
    // SAFETY: the contract
    unsafe { (*b).call(c, a, bb, cc, d, e) }
}

#[cfg(test)]
#[path = "browserobj_tests.rs"]
mod tests;
