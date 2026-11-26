//! The lobby hosts' text and event cases answered in Rust in the game (R6 WON GUI round 40): every case that only
//! made, read, edited or released a lobby text (a `GUIString`, whose text is ee_won_gui's guistrobj.rs `EeWonGstr`
//! since round 39) or read a WONGUI event's members (compobj.rs `EeWonCompEventRec`, read in place) is answered here
//! before the C++ host, which lost the case. Reached from the controls' (lobbyctl.rs, rows.rs), the dialogs'
//! (lobbydlg.rs), the popups' (lobbypopup.rs), the logics' (logics.rs) and RoutingLogic's (routelogic.rs) game
//! dispatches, only while the WON GUI is the game's (`compobj::is_game`): the drivers' scripted worlds answer the ops
//! as before.
//!
//! The texts' conventions are the hosts': a text argument is a `GUIStringData*` (NULL reads as the empty text, the
//! hosts' `Txt`), a held text carries one reference, and a `GUIString*` is one pointer to its text (a heap
//! `GUIString` is a `Box` of one: [`gstring_new`]). The C++ statics the compound cases shared (the text being
//! built, the formatted text's strings, the column text, the item text's bytes) are this module's slots.

use std::cell::UnsafeCell;
use std::ffi::c_void;

use ee_won_gui::compobj::{event_rec, is_game, EeWonCompEventRec};
use ee_won_gui::guistrobj::{
    self as gs, data_units, ee_won_gstr_edit, ee_won_gstr_empty, ee_won_gstr_new, ee_won_gstr_read, gstr_ref,
    gstr_release, EeWonGstr, EeWonGstrOp as GOp, EeWonGstrRead as GRead,
};

use crate::lobbyctl::EeWonLobbyCtlOp as Op;

/// `offsetof(TextListItem, mText)` (LobbyNetCtrlHost.cpp asserts it): the item's text pointer, then `mColor`.
pub const EE_WON_TEXT_ITEM_TEXT_AT: usize = 32;
/// `offsetof(TextListItem, mColor)` (a C `long`).
pub const EE_WON_TEXT_ITEM_COLOR_AT: usize = 40;
/// `offsetof(ChatCommandParser, mParser)` (LobbyNetCtrlHost.cpp asserts it): the parser's Rust object.
pub const EE_WON_CHAT_PARSER_AT: usize = 16;

// ---- the texts

/// A text argument: NULL is the empty text.
fn t(p: isize) -> *mut EeWonGstr {
    if p == 0 {
        ee_won_gstr_empty()
    } else {
        p as *mut EeWonGstr
    }
}

/// A text's units (NULL: none).
pub(crate) fn units(p: isize) -> Vec<u32> {
    // SAFETY: a live text or NULL (the hosts' arguments)
    unsafe { data_units(p as *const c_void) }
}

/// A text's length (`mLength`; NULL: 0).
pub(crate) fn len(p: isize) -> isize {
    if p == 0 {
        return 0;
    }
    // SAFETY: a live text
    unsafe { (*(p as *const EeWonGstr)).buf.length as isize }
}

/// One of `GUIString`'s reads of `a` against the text `b` (or the read's other argument) and `x`, `y`.
fn read(op: GRead, a: isize, other: isize, x: i32, y: i32) -> isize {
    // SAFETY: live texts (NULL reads as empty)
    unsafe { ee_won_gstr_read(op, a as *const EeWonGstr, other as *const c_void, x, y) as isize }
}

/// Resource string `row`'s text (`gLobbyResources[row].mString`, read in place).
pub(crate) fn res(row: isize) -> isize {
    crate::resource::string_data(row as usize)
}

/// `p` with one reference more.
pub(crate) fn held(p: *mut EeWonGstr) -> isize {
    // SAFETY: a live text
    unsafe { gstr_ref(p) };
    p as isize
}

/// One reference fewer (the text freed at none).
pub(crate) fn release(p: isize) {
    if p != 0 {
        // SAFETY: a held text
        unsafe { gstr_release(p as *const EeWonGstr) };
    }
}

/// A new text of `n` bytes of UTF-8 at `bytes` (`GUIString(const char*, n)`), no reference.
pub(crate) fn narrow_new(bytes: isize, n: usize) -> *mut EeWonGstr {
    // SAFETY: `n` bytes at `bytes` (the callers' contract)
    unsafe { ee_won_gstr_new(GOp::AssignNarrow, bytes as *const c_void, n as u16 as i32, 0) }
}

/// A new text of the units, one reference.
pub(crate) fn held_units(u: &[u32]) -> isize {
    held(gs::gstr_from_units(u))
}

/// `slot->append(text, 0, text.length())` (copied first when shared).
///
/// # Safety
/// `slot` holds a live text with its reference.
unsafe fn append(slot: *mut *mut EeWonGstr, text: isize) {
    let other: *const EeWonGstr = t(text);
    // SAFETY: the contract; `other` a live text
    unsafe { ee_won_gstr_edit(GOp::Append, slot, (&other as *const *const EeWonGstr).cast(), 0, len(text) as i32, 0) };
}

/// `slot->append(ch)`.
///
/// # Safety
/// As [`append`].
unsafe fn append_unit(slot: *mut *mut EeWonGstr, ch: i32) {
    // SAFETY: the contract
    unsafe { ee_won_gstr_edit(GOp::AppendUnit, slot, std::ptr::null(), ch, 0, 0) };
}

/// `slot->append(GUIString(s))` of a narrow literal.
///
/// # Safety
/// As [`append`].
unsafe fn append_str(slot: *mut *mut EeWonGstr, s: &str) {
    let n = held(narrow_new(s.as_ptr() as isize, s.len()));
    // SAFETY: the contract
    unsafe { append(slot, n) };
    release(n);
}

/// A `GUIString` the C++ reads in place: one text pointer with its reference (a C++ static's stand-in).
pub(crate) struct Slot(UnsafeCell<*mut EeWonGstr>);

// SAFETY: the lobby's texts are its thread's (as the C++ statics were)
unsafe impl Sync for Slot {}

impl Slot {
    pub(crate) const fn new() -> Self {
        Slot(UnsafeCell::new(std::ptr::null_mut()))
    }
    /// The `GUIString*`.
    pub(crate) fn ptr(&self) -> *mut *mut EeWonGstr {
        self.0.get()
    }
    /// `slot = GUIString(p)` (NULL: the empty text).
    pub(crate) fn set(&self, p: isize) {
        // SAFETY: the slot's own pointer
        unsafe { assign(self.ptr(), p) }
    }
}

/// `*slot = GUIString(p)`: the new text referenced, the old one released (nothing for the same text; NULL: empty).
///
/// # Safety
/// `slot` is a `GUIString` (NULL or a held text).
pub(crate) unsafe fn assign(slot: *mut *mut EeWonGstr, p: isize) {
    let new = t(p);
    // SAFETY: the contract
    let old = unsafe { *slot };
    if old != new {
        // SAFETY: a live text
        unsafe { gstr_ref(new) };
        // SAFETY: the contract
        unsafe { *slot = new };
        release(old as isize);
    }
}

/// `new GUIString(p)` (NULL: the empty text): a heap `GUIString`, one pointer holding its reference.
pub(crate) fn gstring_new(p: isize) -> isize {
    let text = t(p);
    // SAFETY: a live text
    unsafe { gstr_ref(text) };
    Box::into_raw(Box::new(text)) as isize
}

/// `delete (GUIString*)g` of one [`gstring_new`] made.
pub(crate) fn gstring_free(g: isize) {
    if g != 0 {
        // SAFETY: the contract: a Box this module made
        let b = unsafe { Box::from_raw(g as *mut *mut EeWonGstr) };
        release(*b as isize);
    }
}

/// A `GUIString*`'s text (NULL `GUIString*`: none).
pub(crate) fn of(g: isize) -> isize {
    if g == 0 {
        return 0;
    }
    // SAFETY: a live GUIString (one text pointer)
    unsafe { *(g as *const isize) }
}

/// `LobbyFormatString(format, a)`: a new text with one reference (both arguments texts).
pub(crate) fn format(format: isize, a: isize) -> isize {
    let (f, x): (*const EeWonGstr, *const EeWonGstr) = (t(format), t(a));
    // SAFETY: two GUIStrings (each one text pointer)
    let r = unsafe {
        ee_won_gui::localize::ee_won_gui_localize(
            (&f as *const *const EeWonGstr).cast(),
            (&x as *const *const EeWonGstr).cast(),
            std::ptr::null(),
            std::ptr::null(),
            1,
        )
    };
    held(r)
}

/// The Rust object of the C++ `ChatCommandParser*` `p` (its `mParser`).
fn parser(p: isize) -> *const crate::chatobj::EeWonChatParser {
    of(p + EE_WON_CHAT_PARSER_AT as isize) as *const crate::chatobj::EeWonChatParser
}

/// `n` units at `u` (none for 0).
fn unit_slice<'a>(u: *const u32, n: usize) -> &'a [u32] {
    if n == 0 || u.is_null() {
        &[]
    } else {
        // SAFETY: the parser's `n` units, valid until it changes
        unsafe { std::slice::from_raw_parts(u, n) }
    }
}

// ---- the events

/// A game event's record (`None` outside the game: the host's world answers).
fn ev(e: isize) -> Option<EeWonCompEventRec> {
    // SAFETY: a live ComponentEvent (the hosts' argument)
    event_rec(e).map(|r| unsafe { *r })
}

/// `((ComponentEvent*)e)->GetControlId()`: the event's component's control id (compmem.rs, read in place).
pub(crate) fn event_control_id(e: isize) -> Option<isize> {
    Some(ee_won_gui::compmem::control_id(ev(e)?.comp) as isize)
}

// ---- the list items (TextListItem's text and colour, ListItem's members: read in place)

fn item_text(item: isize) -> *mut *mut EeWonGstr {
    (item as usize + EE_WON_TEXT_ITEM_TEXT_AT) as *mut *mut EeWonGstr
}

fn item_rec(item: isize) -> &'static mut ee_won_gui::leafpaint::EeWonListItemRec {
    // SAFETY: a live game list item (the hosts' argument)
    unsafe { &mut *ee_won_gui::leafpaint::item_rec(item) }
}

// ---- the C++ statics of the compound cases

/// ChatAddFormatted's strings and the array of their `GUIString*`s (`sEELobbyFmtStrings`, `sEELobbyFmtText`).
static FMT: [Slot; 2] = [Slot::new(), Slot::new()];
struct FmtTexts([*const *mut EeWonGstr; 2]);
// SAFETY: two pointers to the statics above, never written
unsafe impl Sync for FmtTexts {}
static FMT_TEXTS: FmtTexts = FmtTexts([FMT[0].0.get().cast_const(), FMT[1].0.get().cast_const()]);
/// EeColumn's text (`sEELobbyColumnText`).
static COLUMN: Slot = Slot::new();

/// The controls' text and event ops (lobbyctl.rs and rows.rs, in the game; `None`: the host's).
pub(crate) fn ctl_op(op: Op, a: isize, b: isize, c: isize) -> Option<isize> {
    if !is_game() {
        return None;
    }
    let r = match op {
        Op::ResText => res(a),
        Op::TextFind => read(GRead::Find, a, b, 0, 0),
        Op::TextToInt => read(GRead::ToInt, a, 0, 0, 0),
        Op::TextCompareNoCase => read(GRead::CompareNoCase, a, b, 0, 0),
        Op::TextFindNoCase => read(GRead::FindNoCase, a, b, 0, 0),
        Op::TextFindRes => read(GRead::FindNoCase, a, res(b), 0, 0),
        Op::TextLen => len(a),
        Op::TextAt => units(a).get(b as usize).map_or(0, |&u| u as i32 as isize),
        Op::TextEquals => isize::from(read(GRead::Compare, a, b, 0, 0) == 0),
        Op::TextAlloc => narrow_new(a, b as usize) as isize,
        // the text's narrow bytes (UTF-8) into the buffer `b` of `c` bytes (as many as fit); their count
        Op::TextBytes => {
            let bytes = ee_won_gui::guistring::narrow(&units(a), gs::EE_WON_GSTR_WCHAR as u32);
            let n = bytes.len().min(c as usize);
            if n > 0 {
                // SAFETY: the caller's buffer of `c` bytes
                unsafe { std::ptr::copy_nonoverlapping(bytes.as_ptr(), b as *mut u8, n) };
            }
            bytes.len() as isize
        }
        // the held text `a` (its reference now the result's) with the text `b`, or the bytes `b` of `c`, after it
        Op::TextAppend | Op::TextAppendBytes => {
            let mut s = a as *mut EeWonGstr;
            if op == Op::TextAppend {
                // SAFETY: `s` holds `a`'s reference
                unsafe { append(&mut s, b) };
            } else {
                let n = held(narrow_new(b, c as usize));
                // SAFETY: as above
                unsafe { append(&mut s, n) };
                release(n);
            }
            s as isize
        }
        // the chat parser's members (its Rust object read from the C++ parser)
        Op::ParserNumber => {
            use crate::chatobj::EeWonChatParserNumber as N;
            let n = match b {
                0 => N::Prefix,
                1 => N::Error,
                _ => N::Command,
            };
            // SAFETY: the parser's live Rust object
            unsafe { crate::chatobj::ee_won_chatparser_number(parser(a), n) as i32 as isize }
        }
        Op::ParserText => {
            use crate::chatobj::EeWonChatParserText as T;
            let mut n = 0usize;
            // SAFETY: as above
            let u = unsafe {
                crate::chatobj::ee_won_chatparser_text(parser(a), if b == 0 { T::TargetName } else { T::Text }, &mut n)
            };
            held_units(unit_slice(u, n))
        }
        Op::ParserHelp => {
            let mut n = 0usize;
            // SAFETY: as above
            let u = unsafe { crate::chatobj::ee_won_chatparser_help(parser(a).cast_mut(), b as u16, &mut n) };
            held_units(unit_slice(u, n))
        }
        // SAFETY: as above
        Op::ParserClients => unsafe { crate::chatobj::ee_won_chatparser_object(parser(a), 0) as isize },
        Op::ParserHandleInput => {
            let input = t(b);
            // SAFETY: as above; the input's units
            let r = unsafe {
                crate::chatobj::ee_won_chatparser_handle_input_list(
                    parser(a).cast_mut(),
                    (*input).buf.text,
                    (*input).buf.length as usize,
                )
            };
            isize::from(r)
        }
        // `Held(LobbyFormatString(LOBBY_STRING(a), ((ChatCommandParser*)b)->GetTargetName()))`
        Op::FormatTarget => {
            let mut n = 0usize;
            // SAFETY: as above
            let u = unsafe {
                crate::chatobj::ee_won_chatparser_text(
                    parser(b),
                    crate::chatobj::EeWonChatParserText::TargetName,
                    &mut n,
                )
            };
            let target = held_units(unit_slice(u, n));
            let r = format(res(a), target);
            release(target);
            r
        }
        // the parser's prefix and command `b`, then the quoted name (`c`, else the target's), one reference
        Op::ParserCommandText => {
            let p = parser(a);
            let mut s = held(ee_won_gstr_empty()) as *mut EeWonGstr;
            // SAFETY: the parser's Rust object (the C++ parser's `mParser`); `s` the built text's reference
            unsafe {
                let prefix = crate::chatobj::ee_won_chatparser_number(p, crate::chatobj::EeWonChatParserNumber::Prefix);
                append_unit(&mut s, prefix as i32);
                let mut n = 0usize;
                let u = crate::chatobj::ee_won_chatparser_command_string(p, b as i32, &mut n);
                let command = held_units(if n != 0 { std::slice::from_raw_parts(u, n) } else { &[] });
                append(&mut s, command);
                release(command);
                append_str(&mut s, " \"");
                if c != 0 {
                    append(&mut s, c);
                } else {
                    let mut n = 0usize;
                    let u = crate::chatobj::ee_won_chatparser_text(
                        p,
                        crate::chatobj::EeWonChatParserText::TargetName,
                        &mut n,
                    );
                    let target = held_units(if n != 0 { std::slice::from_raw_parts(u, n) } else { &[] });
                    append(&mut s, target);
                    release(target);
                }
                append_str(&mut s, "\" ");
            }
            s as isize
        }
        Op::FmtSetString => {
            FMT.get(a as usize)?.set(b);
            0
        }
        Op::FmtTexts => FMT_TEXTS.0.as_ptr() as isize,
        Op::ColumnTextSet => {
            COLUMN.set(a);
            0
        }
        Op::ColumnText => COLUMN.ptr() as isize,
        // the events' members
        Op::EvType => ev(a)?.kind as isize,
        Op::EvComboItem | Op::EvTreeItem | Op::EvListItem => ev(a)?.item,
        Op::EventX => ev(a)?.x as isize,
        Op::EventY => ev(a)?.y as isize,
        Op::EventActiveSel | Op::EventDoubleClick => ev(a)?.flag as isize,
        Op::EventButton => ev(a)?.button as isize,
        Op::EventControlId => event_control_id(a)?,
        // a text row's text and colour
        Op::ItemSetText => {
            // SAFETY: a live text row's text
            unsafe { assign(item_text(a), b) };
            0
        }
        Op::ItemSetTextRes => {
            // SAFETY: as above
            unsafe { assign(item_text(a), res(b)) };
            0
        }
        Op::ItemTextErase => {
            // SAFETY: as above
            unsafe { assign(item_text(a), 0) };
            0
        }
        Op::ItemTextAppend => {
            // SAFETY: as above
            unsafe { append(item_text(a), b) };
            0
        }
        // SAFETY: two live text rows' texts
        Op::ItemTextCompare => {
            read(GRead::CompareNoCase, unsafe { *item_text(a) } as isize, unsafe { *item_text(b) } as isize, 0, 0)
        }
        // SAFETY: a live text row's text
        Op::TextItemIs => isize::from(read(GRead::Compare, unsafe { *item_text(a) } as isize, b, 0, 0) == 0),
        // SAFETY: a live text row's colour
        // SAFETY: a live text row's colour
        Op::ItemGetColor => {
            (unsafe { *((a as usize + EE_WON_TEXT_ITEM_COLOR_AT) as *const std::ffi::c_long) }) as isize
        }
        Op::ItemSetColor => {
            // SAFETY: as above
            unsafe { *((a as usize + EE_WON_TEXT_ITEM_COLOR_AT) as *mut std::ffi::c_long) = b as std::ffi::c_long };
            0
        }
        Op::ItemGetHeight => item_rec(a).height as isize,
        Op::ItemGetTop => item_rec(a).top as isize,
        Op::ItemGetWidth => item_rec(a).width as isize,
        Op::ItemGetSelected => item_rec(a).selected as isize,
        Op::ItemGetSelectable => item_rec(a).selectable as isize,
        Op::ItemSetHeight => {
            item_rec(a).height = b as i32;
            0
        }
        Op::ItemSetTop => {
            item_rec(a).top = b as i32;
            0
        }
        Op::ItemSetWidth => {
            item_rec(a).width = b as i32;
            0
        }
        Op::ItemSetSelected => {
            item_rec(a).selected = u8::from(b != 0);
            0
        }
        Op::ItemSetSelectable => {
            item_rec(a).selectable = u8::from(b != 0);
            0
        }
        _ => return None,
    };
    Some(r)
}

/// A wide text of `count` UTF-16 units at `p` (`WonWideSink`: as they are with a 2-byte `wchar_t`, else UTF-32).
pub(crate) fn wide_units(p: isize, count: usize) -> Vec<u32> {
    if count == 0 {
        return Vec::new();
    }
    // SAFETY: `count` units at `p` (the callers' contract)
    let u = unsafe { std::slice::from_raw_parts(p as *const u16, count) };
    if gs::EE_WON_GSTR_WCHAR == 2 {
        u.iter().map(|&x| x as u32).collect()
    } else {
        ee_won_net::text::utf32_from_utf16(u)
    }
}

/// `GetWONStatusString(status)` (WONStatusString.cpp until R6 WON GUI round 40): the status's resource text, or the
/// general one with the status's description, with one reference.
pub(crate) fn status_string(status: i32) -> isize {
    let (row, general) = crate::statusstr::status_text(status);
    let text = res(row as isize);
    if !general {
        return held(t(text));
    }
    // SAFETY: a static string or this thread's latest unnamed one, NUL-terminated
    let d = unsafe { std::ffi::CStr::from_ptr(ee_won_net::ffi_net::ee_won_status_text(status as std::ffi::c_long)) };
    let desc = held(narrow_new(d.as_ptr() as isize, d.to_bytes().len()));
    let r = format(text, desc);
    release(desc);
    r
}

/// `GetWONStatusString` for the C++ (WONLobbyCore.h's inline): a text with one reference.
#[no_mangle]
pub extern "C" fn ee_won_lobby_status_string(status: i32) -> *mut EeWonGstr {
    status_string(status) as *mut EeWonGstr
}

/// The item text's bytes ItemText gave (`sDlgItemText`: valid until the next).
static ITEM_BYTES: std::sync::Mutex<Vec<u8>> = std::sync::Mutex::new(Vec::new());

/// The dialogs' text and event ops (lobbydlg.rs and dlgsplit.rs, in the game; `None`: the host's).
pub(crate) fn dlg_op(op: crate::lobbydlg::EeWonLobbyDlgOp, a: isize, b: isize) -> Option<isize> {
    use crate::lobbydlg::EeWonLobbyDlgOp as D;
    if !is_game() {
        return None;
    }
    let r = match op {
        D::EventDoubleClick => ev(a)?.flag as isize,
        D::EventButton => ev(a)?.button as isize,
        D::TextCompare => read(GRead::Compare, a, b, 0, 0),
        D::TextRecord => held_units(&wide_units(a, b as usize)),
        D::TextFormatRes => format(res(a), b),
        D::TitleNew => gstring_new(res(a)),
        D::TitleAppendSpace => {
            // SAFETY: a GUIString TitleNew made
            unsafe { append_str(a as *mut *mut EeWonGstr, " ") };
            0
        }
        D::TitleFree => {
            gstring_free(a);
            0
        }
        // `GUIString((const char*)a).length()`
        D::NameLength => {
            // SAFETY: a NUL-terminated string
            let n = unsafe { std::ffi::CStr::from_ptr(a as *const std::ffi::c_char) }.to_bytes().len();
            let s = held(narrow_new(a, n));
            let l = len(s);
            release(s);
            l
        }
        // the text row's text as narrow bytes, valid until the next
        D::ItemText => {
            // SAFETY: a live text row's text
            let text = unsafe { *item_text(a) } as isize;
            let mut v = ee_won_gui::guistring::narrow(&units(text), gs::EE_WON_GSTR_WCHAR as u32);
            v.push(0);
            let mut g = ITEM_BYTES.lock().unwrap_or_else(|e| e.into_inner());
            *g = v;
            g.as_ptr() as isize
        }
        _ => return None,
    };
    Some(r)
}

/// The popups' event ops (lobbypopup.rs, in the game).
pub(crate) fn popup_op(op: crate::lobbypopup::EeWonLobbyPopupOp, a: isize) -> Option<isize> {
    use crate::lobbypopup::EeWonLobbyPopupOp as P;
    match op {
        P::EventItem => Some(ev(a)?.item),
        P::EventButton => Some(ev(a)?.button as isize),
        _ => None,
    }
}

/// The logics' text ops (InitLogic's: logics.rs, in the game). Its texts are heap `GUIString`s.
pub(crate) fn logic_op(op: crate::logics::EeWonLogicOp, a: isize, b: isize) -> Option<isize> {
    use crate::logics::EeWonLogicOp as L;
    if !is_game() {
        return None;
    }
    let slot = a as *mut *mut EeWonGstr;
    let r = match op {
        L::TextNew => gstring_new(if a >= 0 { res(a) } else { 0 }),
        L::TextAppendRes => {
            // SAFETY: a GUIString TextNew made
            unsafe { append(slot, res(b)) };
            0
        }
        L::TextAppendComma => {
            // SAFETY: as above
            unsafe { append_str(slot, ", ") };
            0
        }
        L::TextAppendText => {
            // SAFETY: as above
            unsafe { append(slot, of(b)) };
            0
        }
        L::TextLength => len(of(a)),
        L::TextFree => {
            gstring_free(a);
            0
        }
        // AccountLogic's saved users of a lobby event's name (and password): `LobbyAddUser(name, password, false)`,
        // `LobbyFindUser(name)`
        L::AddUserNamed | L::AddUserNoPassword | L::AddUserWithPassword | L::FindUserOfEvent => {
            use crate::lobbyevt::EeWonLobbyEvtField as F;
            use crate::persistobj::EeWonPersistCall as P;
            // SAFETY: a bound event lives until its shell's destructor unbinds it; the call holds the shell
            let e = unsafe { &*crate::lobbyevt::of(a)? };
            let password: &[u8] = if op == L::AddUserWithPassword { e.bytes(F::Password) } else { &[] };
            let call = if op == L::FindUserOfEvent { P::FindUser } else { P::AddUser };
            crate::persistobj::game_text_call(call, e.bytes(F::UserName), password, 0)?
        }
        // `new GUIString(LobbyFormatString(LOBBY_STRING(a), GetWONStatusString(b)))`
        L::TextStatus => {
            let s = status_string(b as i32);
            let f = format(res(a), s);
            release(s);
            let g = gstring_new(f);
            release(f);
            g
        }
        _ => return None,
    };
    Some(r)
}

/// RoutingLogic's texts and event members (routelogic.rs, in the game, for the shell `logic`): its heap texts
/// (`new GUIString(..)` of a game's name or password, the parser's text and the dialogs' input texts) are
/// [`gstring_new`]'s, which TextFree gives back.
pub(crate) fn route_op(op: crate::routelogic::EeWonRouteLogicOp, logic: isize, a: isize, b: isize) -> Option<isize> {
    use crate::lobbydlg::EeWonLobbyDlgCall as C;
    use crate::modelobj::EeWonModelField as F;
    use crate::routelogic::EeWonRouteLogicOp as R;
    if !is_game() {
        return None;
    }
    // a held text as a new heap GUIString
    let boxed = |t: isize| {
        let g = gstring_new(t);
        release(t);
        g
    };
    let r = match op {
        R::TextFree => {
            gstring_free(a);
            0
        }
        // SAFETY: a live LobbyGame
        R::GameText => gstring_new(unsafe { crate::modelobj::shell_get(a, if b == 0 { F::GameName } else { F::GamePassword }) }),
        R::ParserText => {
            let mut n = 0;
            // SAFETY: a live ChatCommandParser
            let u = unsafe { crate::chatobj::ee_won_chatparser_text(parser(a), crate::chatobj::EeWonChatParserText::Text, &mut n) };
            boxed(held_units(unit_slice(u, n)))
        }
        // (the routing dialogs' slots: 0 add friend, 1 admin action, 3 create room, 5 password)
        R::DialogText => gstring_new(match a {
            1 => dialog_input(logic, 3, 0)?,
            2 => input_text(dlg_call(logic_dialog(logic, 3), C::GetPassword, 0, 0, 0)?),
            3 => dialog_input(logic, 5, 0)?,
            4 => dialog_input(logic, 1, 2)?,
            5 => dialog_input(logic, 0, 0)?,
            _ => return None,
        }),
        R::TextLength => len(of(a)),
        // the text's units (to `b` when not 0); 0 is the empty text
        R::TextUnits => {
            let u = units(of(a));
            if b != 0 {
                // SAFETY: the caller's buffer of the text's length
                unsafe { std::ptr::copy_nonoverlapping(u.as_ptr(), b as *mut u32, u.len()) };
            }
            u.len() as isize
        }
        R::EventComponent => ev(a)?.comp,
        R::EventType => ev(a)?.kind as isize,
        _ => return None,
    };
    Some(r)
}

/// `offsetof(LobbyRustDialog, mLobbyDlg)` (DialogLogic.cpp registers it; 0 in a driver's world).
static DLG_AT: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);
/// `offsetof(RustLogic, mDialog)`, the shell's dialog slots (`SmartPtr<LobbyRustDialog>[6]`).
static LOGIC_DIALOGS_AT: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);
/// `offsetof(InputBox, mRealText)`, the text `GetText()` answers.
static INPUT_TEXT_AT: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);

/// Registers `offsetof(LobbyRustDialog, mLobbyDlg)`, `offsetof(RustLogic, mDialog)` and `offsetof(InputBox,
/// mRealText)` (the game, at static initialisation).
#[no_mangle]
pub extern "C" fn ee_won_lobbytext_dialog_at(at: usize, logic_dialogs: usize, input_text: usize) {
    use std::sync::atomic::Ordering::Release;
    LOGIC_DIALOGS_AT.store(logic_dialogs, Release);
    INPUT_TEXT_AT.store(input_text, Release);
    DLG_AT.store(at, Release);
}

/// The shell `logic`'s dialog in slot `k` (`L->mDialog[k].get()`; 0 outside the game).
fn logic_dialog(logic: isize, k: usize) -> isize {
    let at = LOGIC_DIALOGS_AT.load(std::sync::atomic::Ordering::Acquire);
    if at == 0 || logic == 0 {
        return 0;
    }
    // SAFETY: a live RustLogic: its dialog slots (one pointer each) at the registered offset
    unsafe { *((logic as usize + at + k * std::mem::size_of::<usize>()) as *const isize) }
}

/// An input box's `GetText()` (its `mRealText`'s text; the empty text for NULL).
fn input_text(input: isize) -> isize {
    let at = INPUT_TEXT_AT.load(std::sync::atomic::Ordering::Acquire);
    if input == 0 || at == 0 {
        return ee_won_gstr_empty() as isize;
    }
    // SAFETY: a live InputBox: its GUIString at the registered offset
    of(input + at as isize)
}

/// `L->mDialog[k]->InputText(member)` (`None` outside the game).
fn dialog_input(logic: isize, k: usize, member: isize) -> Option<isize> {
    let input = dlg_call(logic_dialog(logic, k), crate::lobbydlg::EeWonLobbyDlgCall::Slot, member, 0, 0)?;
    Some(input_text(input))
}

/// `((LobbyRustDialog*)d)->LCall(call, a, b, c)` on the game's dialog `d` (`None` outside the game).
fn dlg_call(d: isize, call: crate::lobbydlg::EeWonLobbyDlgCall, a: isize, b: isize, c: isize) -> Option<isize> {
    let at = DLG_AT.load(std::sync::atomic::Ordering::Acquire);
    if at == 0 || d == 0 || !is_game() {
        return None;
    }
    // SAFETY: a live game dialog: its Rust object at the registered offset
    let dlg = unsafe { *((d as usize + at) as *const *const crate::lobbydlg::EeWonLobbyDlg) };
    // SAFETY: the dialog's live Rust object
    Some(unsafe { crate::lobbydlg::ee_won_lobbydlg_call(dlg, call, a, b, c) })
}

/// DialogLogic's dialog texts (dlglogic.rs, in the game): the status dialog's texts and status, the OK/Cancel
/// dialog's texts and buttons.
pub(crate) fn dlglogic_op(
    op: crate::dlglogic::EeWonDlgLogicOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> Option<isize> {
    use crate::dlglogic::EeWonDlgLogicOp as L;
    use crate::lobbydlg::EeWonLobbyDlgCall as C;
    use crate::resource::EeWonLobbyRes as Res;
    match op {
        L::StatusSetText => dlg_call(a, C::SetText, b, c, isize::from(d != 0)).map(|_| 0),
        // `GUIString aStatus = GetWONStatusString(b); a->SetStatus(aStatus, false)`
        L::StatusSetStatus => {
            if DLG_AT.load(std::sync::atomic::Ordering::Acquire) == 0 || a == 0 || !is_game() {
                return None;
            }
            let status = status_string(b as i32);
            let slot = status;
            dlg_call(a, C::SetStatus, &slot as *const isize as isize, 0, 0);
            release(status);
            Some(0)
        }
        // `const GUIString* aButtons[2] = {&(Yes or OK), &(No or Cancel)}; a->SetText(b, c, aButtons)`
        L::OkCancelSetText => {
            let (first, second) = if d != 0 {
                (Res::LobbyGlobal_Yes_String, Res::LobbyGlobal_No_String)
            } else {
                (Res::LobbyGlobal_OK_String, Res::LobbyGlobal_Cancel_String)
            };
            let buttons = [crate::resource::string_slot(first as usize), crate::resource::string_slot(second as usize)];
            dlg_call(a, C::SetText, b, c, buttons.as_ptr() as isize).map(|_| 0)
        }
        _ => None,
    }
}

/// The EE game's text comparison (eegame.rs, in the game).
pub(crate) fn eegame_op(op: crate::eegame::EeWonEeGameOp, a: isize, b: isize) -> Option<isize> {
    use crate::eegame::EeWonEeGameOp as G;
    if !is_game() {
        return None;
    }
    match op {
        G::StringsEqual => Some(isize::from(read(GRead::Compare, a, b, 0, 0) == 0)),
        _ => None,
    }
}

/// The LAN logic's event member and its dialogs' texts (lanobj.rs, in the game, for the shell `logic`; its dialog
/// slots: 0 direct connect, 1 LAN name, 3 password).
pub(crate) fn lobby_op(op: crate::lobbyhost::EeWonLobbyOp, logic: isize, a: isize, b: isize, c: isize) -> Option<isize> {
    use crate::lobbyhost::EeWonLobbyOp as L;
    use crate::persistobj::EeWonPersistCall as P;
    // an object argument, read only in the game (a driver's world passes made-up values)
    let game = |p: isize| is_game().then_some(p);
    // `Take(b, bytes)`: the bytes given to the sink `b`
    let take = |bytes: Vec<u8>| {
        // SAFETY: the op's sink (a live Vec<u8>, or NULL)
        unsafe { crate::lobbyhost::ee_won_lobby_sink(b as *mut c_void, bytes.as_ptr(), bytes.len()) };
        0
    };
    let r = match op {
        L::EventComponent => ev(a)?.comp,
        // `l->mDialog[kLanName]->InputText(0).length()`
        L::LanNameDialogLength => len(dialog_input(logic, 1, 0)?),
        L::LanDirectInputLength => len(dialog_input(logic, 0, 0)?),
        // `Take(b, LanUtf16(((LobbyClient*)a)->GetName()))`
        // SAFETY: a live LobbyClient (in the game)
        L::ClientNameTake => take(crate::routelogic::utf16le(&units(unsafe {
            crate::modelobj::shell_get(game(a)?, crate::modelobj::EeWonModelField::ClientName)
        }))),
        // `Take(b, LanUtf16(((ChatCommandParser*)a)->GetText()))`
        L::ParserTextTake => {
            let mut n = 0;
            // SAFETY: a live ChatCommandParser
            let u = unsafe { crate::chatobj::ee_won_chatparser_text(parser(game(a)?), crate::chatobj::EeWonChatParserText::Text, &mut n) };
            take(crate::routelogic::utf16le(unit_slice(u, n)))
        }
        // the parser's (`a`) `GetRoomFlags()`, `GetTarget()`, `GetCommand()`
        L::ParserRoomFlags | L::ParserTarget | L::ParserCommand => {
            use crate::chatobj::EeWonChatParserNumber as N;
            let p = parser(game(a)?);
            // SAFETY: a live ChatCommandParser's Rust object
            unsafe {
                match op {
                    L::ParserRoomFlags => crate::chatobj::ee_won_chatparser_number(p, N::RoomFlags) as std::ffi::c_ulong as isize,
                    L::ParserTarget => crate::chatobj::ee_won_chatparser_object(p, 1) as isize,
                    _ => crate::chatobj::ee_won_chatparser_number(p, N::Command) as i32 as isize,
                }
            }
        }
        // `((ChatCommandParser*)a)->SetInput(((LobbyClient*)b)->GetName())`
        L::ParserSetInputName => {
            // SAFETY: a live LobbyClient
            let name = units(unsafe { crate::modelobj::shell_get(game(b)?, crate::modelobj::EeWonModelField::ClientName) });
            // SAFETY: a live ChatCommandParser's Rust object; the units live for the call
            unsafe {
                crate::chatobj::ee_won_chatparser_set_text(
                    parser(a).cast_mut(),
                    crate::chatobj::EeWonChatParserText::Input,
                    name.as_ptr().cast(),
                    name.len(),
                    4,
                )
            };
            0
        }
        // `Take(b, LanUtf16(the password dialog's InputText(0)))`
        L::LanPasswordTake => take(crate::routelogic::utf16le(&units(dialog_input(logic, 3, 0)?))),
        // `Take(b, std::string(the direct connect dialog's InputText(1)))`
        L::LanDirectHostTake => {
            take(ee_won_gui::guistring::narrow(&units(dialog_input(logic, 0, 1)?), gs::EE_WON_GSTR_WCHAR as u32))
        }
        // `((LobbyGame*)a)->SetName(the direct connect dialog's InputText(c))`
        L::GameSetDirectName => {
            let name = dialog_input(logic, 0, c)?;
            // SAFETY: a live LobbyGame
            unsafe { crate::modelobj::shell_set(a, crate::modelobj::EeWonModelField::GameName, name) };
            0
        }
        // `LobbySetLanName(the LAN name dialog's InputText(0))`
        L::LanSetLanNameFromDialog => crate::persistobj::game_call_text(P::SetLanName, dialog_input(logic, 1, 0)?, 0)?,
        // `LobbyAddDirectConnect(Addr(b), the direct connect dialog's InputText(c))`
        L::AddDirectConnectNamed => crate::persistobj::game_call_text(P::AddDirect, dialog_input(logic, 0, c)?, b as i64)?,
        _ => return None,
    };
    Some(r)
}

/// A record's name: a new text of `count` UTF-16 units at `units16`, held (modelobj.rs's and modelbuf.rs's
/// TextOfUnits, in the game).
pub(crate) fn model_text(units16: isize, count: isize) -> Option<isize> {
    is_game().then(|| held_units(&wide_units(units16, count as usize)))
}

/// The game buffers' text ops (modelbuf.rs, in the game).
pub(crate) fn buf_op(op: crate::modelbuf::EeWonModelBufOp, a: isize, b: isize) -> Option<isize> {
    use crate::modelbuf::EeWonModelBufOp as B;
    use crate::resource::EeWonLobbyRes as Res;
    if !is_game() {
        return None;
    }
    let status = |r: Res| Some(held(t(res(r as isize))));
    match op {
        B::TextOfUnits => model_text(a, b),
        // `NewGUIStringData((const char*)a)` held
        B::TextOfBytes => {
            // SAFETY: a NUL-terminated string
            let n = unsafe { std::ffi::CStr::from_ptr(a as *const std::ffi::c_char) }.to_bytes().len();
            Some(held(narrow_new(a, n)))
        }
        // the empty text, no reference added
        B::TextEmpty => Some(ee_won_gstr_empty() as isize),
        B::TextLength => Some(len(a)),
        B::StatusSuccess => status(Res::LobbyGameStatus_Success_String),
        B::StatusGameFull => status(Res::LobbyGameStatus_GameFull_String),
        B::StatusGameInProgress => status(Res::LobbyGameStatus_GameInProgress_String),
        B::StatusUnpackFailure => status(Res::LobbyGameStatus_UnpackFailure_String),
        B::StatusNotInvited => status(Res::LobbyGameStatus_NotInvited_String),
        B::StatusCaptainRejectedYou => status(Res::LobbyGameStatus_CaptainRejectedYou_String),
        B::StatusInvalidPassword => status(Res::LobbyGameStatus_InvalidPassword_String),
        B::StatusDuplicateName => status(Res::LobbyGameStatus_DuplicateName_String),
        _ => None,
    }
}
