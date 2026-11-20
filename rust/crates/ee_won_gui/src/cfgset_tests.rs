//! The setters made in Rust: their descriptions over a scripted frame (the method and its arguments, the forward's
//! calls), the argument conversions, and a call reaching a real object through the registry.

use std::cell::RefCell;
use std::ffi::c_void;

use super::*;
use crate::cfgtab::EeWonCfgRead;

/// a traced setter: the method, its shown arguments, the forward's calls
type Seen = (String, Vec<(EeWonCfgDisp, isize)>, Vec<EeWonCfgSetCall>);

thread_local! {
    static FRAME: RefCell<Vec<i32>> = const { RefCell::new(Vec::new()) };
    static SEEN: RefCell<Vec<Seen>> = const { RefCell::new(Vec::new()) };
}

const COMP: isize = 0x5000;
/// the trace hook is global: one test at a time installs it or applies a setter
static LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());

extern "C" fn read(_: *mut c_void, _: EeWonCfgRead) -> bool {
    true
}
extern "C" fn yes(_: *mut c_void) -> bool {
    true
}
extern "C" fn text(_: *mut c_void, _: i32, len: *mut u16, unit: *mut u32) -> *const c_void {
    // SAFETY: the out-parameters
    unsafe {
        *len = 0;
        *unit = 2;
    }
    std::ptr::null()
}
extern "C" fn push(_: *mut c_void, _: i32) {}
extern "C" fn apply(_: *mut c_void, _: i32) -> bool {
    false
}
extern "C" fn fail(_: *mut c_void, _: *const u8, _: usize, _: i32) {}
extern "C" fn clear(_: *mut c_void) {}
extern "C" fn peek(_: *mut c_void, s: i32) -> i32 {
    FRAME.with(|f| f.borrow().get(s as usize).copied().unwrap_or(0))
}
extern "C" fn arg(_: *mut c_void, what: EeWonCfgArg, s: i32) -> isize {
    match what {
        EeWonCfgArg::Int => peek(std::ptr::null_mut(), s) as isize,
        EeWonCfgArg::IntCount => FRAME.with(|f| f.borrow().len()) as isize,
        EeWonCfgArg::Component => COMP,
        EeWonCfgArg::Comp => 0x7000 + s as isize,
        EeWonCfgArg::StrPtr => 0x8000 + s as isize,
        EeWonCfgArg::Image => 0x9000,
        EeWonCfgArg::Font => 0xa000,
        EeWonCfgArg::FontCount => 1,
        EeWonCfgArg::CompCount => 2,
        EeWonCfgArg::StrUnits => 0xb000,
        EeWonCfgArg::StrLen => 3,
    }
}

fn host() -> EeWonCfgTabHost {
    EeWonCfgTabHost { ctx: std::ptr::null_mut(), read, comma: yes, end: yes, text, push, apply, fail, clear, peek, arg }
}

extern "C" fn hook(_: *mut c_void, comp: *mut c_void, _: i32, t: *const EeWonCfgSetTrace) {
    assert_eq!(comp as isize, COMP);
    // SAFETY: the trace lives for the call
    let t = unsafe { &*t };
    // SAFETY: name_len bytes of a &'static str
    let name = unsafe { std::str::from_utf8_unchecked(std::slice::from_raw_parts(t.name, t.name_len)) }.to_string();
    let args = (0..t.nargs as usize).map(|k| (t.disp[k], t.args[k])).collect();
    let calls = t.calls[..t.ncalls as usize].to_vec();
    SEEN.with(|s| s.borrow_mut().push((name, args, calls)));
}

/// The trace of `setter` over the ints (one hook at a time: the tests that trace take this lock)
fn traced(setter: i32, ints: &[i32]) -> Option<Seen> {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    FRAME.with(|f| *f.borrow_mut() = ints.to_vec());
    SEEN.with(|s| s.borrow_mut().clear());
    ee_won_cfg_set_trace(Some(hook), std::ptr::null_mut());
    let h = host();
    let made = super::apply(&h, setter);
    ee_won_cfg_set_trace(None, std::ptr::null_mut());
    let seen = SEEN.with(|s| s.borrow_mut().pop());
    assert_eq!(made, seen.is_some());
    seen
}

fn id(t: i32, n: i32) -> i32 {
    t << 8 | n
}

#[test]
fn the_flag_setters_convert_as_the_forwards_did() {
    // Button::SetButtonFlags(i[1], i[0]!=0): the int as an unsigned long, then an intptr_t
    let (name, args, calls) = traced(id(EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, 0), &[5, -2]).unwrap();
    assert_eq!(name, "SetButtonFlags");
    assert_eq!(args.iter().map(|a| a.1).collect::<Vec<_>>(), vec![-2, 1]);
    assert_eq!(calls.len(), 1);
    assert_eq!(calls[0].fun, EeWonCfgFun::Btn);
    assert_eq!(calls[0].op, EeWonBtnCall::SetButtonFlags as u32);
    assert_eq!(calls[0].args[0], (-2i32) as std::ffi::c_ulong as isize);
    assert_eq!(calls[0].args[1], 1);
    // ListArea::SetListFlags((unsigned long)i[1], ...): shown widened, passed back as an int
    let (_, args, calls) = traced(id(EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION, 0), &[0, -3]).unwrap();
    assert_eq!(args[0].1, (-3i32) as std::ffi::c_ulong as isize);
    assert_eq!(calls[0].args[0], -3);
    assert_eq!(calls[0].args[1], 0);
}

#[test]
fn the_narrowing_setters_keep_the_low_byte() {
    let (_, args, calls) = traced(id(EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION, 2), &[0x1ff]).unwrap();
    assert_eq!(args[0].1, 0xff);
    assert_eq!(calls[0].args[0], 0xff);
    let (_, args, calls) = traced(id(EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, 4), &[0x102]).unwrap();
    assert_eq!(args[0].1, 0x102);
    assert_eq!(calls[0].args, [EeWonWbField::VAlign as isize, 2, 0, 0, 0]);
    let (_, _, calls) = traced(id(EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 1), &[0x1ff]).unwrap();
    assert_eq!(calls[0].args[0], (0x1ffi32 as std::ffi::c_char) as isize);
}

#[test]
fn the_padding_setters_make_four_calls_in_order() {
    let (name, _, calls) =
        traced(id(EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, 10), &[1, 2, 3, 4]).unwrap();
    assert_eq!(name, "SetTextPadding");
    let fields: Vec<isize> = calls.iter().map(|c| c.args[0]).collect();
    assert_eq!(
        fields,
        vec![
            EeWonWbField::LeftTextPad as isize,
            EeWonWbField::TopTextPad as isize,
            EeWonWbField::RightTextPad as isize,
            EeWonWbField::BottomTextPad as isize
        ]
    );
    assert_eq!(calls.iter().map(|c| c.args[1]).collect::<Vec<_>>(), vec![1, 2, 3, 4]);
}

#[test]
fn the_optional_arguments_follow_the_counts() {
    // SetCursorBlinkRate(i[0], ni>1 ? i[1] : i[0])
    let (_, args, _) = traced(id(EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 14), &[300]).unwrap();
    assert_eq!(args.iter().map(|a| a.1).collect::<Vec<_>>(), vec![300, 300]);
    let (_, args, _) = traced(id(EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 14), &[300, 7]).unwrap();
    assert_eq!(args.iter().map(|a| a.1).collect::<Vec<_>>(), vec![300, 7]);
    // AddInvisibleSplitter(i[0], ni>1 ? i[1] : 10)
    let (_, args, _) = traced(id(EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION, 2), &[5]).unwrap();
    assert_eq!(args[1].1, 10);
    // SetButtonState: the state's low byte, the checked bit, the colour -1 without a second int
    let (_, _, calls) = traced(id(EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, 15), &[0x103]).unwrap();
    assert_eq!(calls[0].fun, EeWonCfgFun::WonBtnState);
    assert_eq!(calls[0].args, [3, 0x9000, -1, 0xa000, 1]);
}

#[test]
fn the_host_keeps_the_setters_on_cpp_members() {
    // Label::SetFont (the wrapped text's), Label::SetBackground, ImageComponent::mTile, the container's
    for s in [
        id(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 1),
        id(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 3),
        id(EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY, 1),
        id(EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION, 0),
        id(EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, 1),
    ] {
        assert!(traced(s, &[1]).is_none(), "{s:#x}");
    }
}

#[test]
fn a_setter_reaches_the_components_object_through_the_registry() {
    unsafe extern "C" fn quiet(
        _: *mut c_void,
        _: crate::guihost::EeWonGuiOp,
        _: isize,
        _: isize,
        _: isize,
        _: isize,
    ) -> isize {
        0
    }
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let comp = Box::into_raw(Box::new(0u64)) as *mut c_void;
    let label = crate::labelobj::ee_won_label_new(comp, Some(quiet));
    struct Ctx(isize);
    extern "C" fn carg(ctx: *mut c_void, what: EeWonCfgArg, s: i32) -> isize {
        // SAFETY: the test's context
        let c = unsafe { &*(ctx as *const Ctx) };
        match what {
            EeWonCfgArg::Component => c.0,
            EeWonCfgArg::Int if s == 0 => 0x123456,
            _ => 0,
        }
    }
    let ctx = Ctx(comp as isize);
    let mut h = host();
    h.ctx = &ctx as *const Ctx as *mut c_void;
    h.arg = carg;
    assert!(super::apply(&h, id(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 2)));
    // SAFETY: the live label
    let color = unsafe { crate::labelobj::ee_won_label_call(label, EeWonLabelCall::TextColor, 0, 0, 0) };
    assert_eq!(color, 0x123456);
    // SAFETY: made above, not used again
    unsafe {
        crate::labelobj::ee_won_label_free(label);
        drop(Box::from_raw(comp as *mut u64));
    }
    // a component with no object of the kind: nothing is called
    assert!(super::apply(&h, id(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 2)));
}
