//! The load of a config file, `ComponentConfig::ReadConfig_Unsafe`, in Rust (R6 WON round 34).
//!
//! The original (ComponentConfigHost.cpp) bound the component, turned the window manager's image caching on, swapped
//! the parse's resource table in (the caller's or a new one), cleared the parse-time flag, parsed the file from the
//! resource folder with warnings as errors, glued the table's resources and initialised the components, showed the
//! parse time when a SHOWPARSETIME instruction asked, then put the flag, the caching and the table back. A failed
//! parse closed the file and kept the parser's abort reason as the config's error text; a glue or init that threw
//! a `ConfigObjectException` kept its text; both returned false leaving the table, the flag and the caching as they
//! were then. Any other exception went on to the caller.
//!
//! Each C++ statement is a one-statement op of the host (include/ee/wonapi/WONCfgLoad.inl). The C++ locals (the old
//! table's `SmartPtr`, the local `ConfigParser`) are host objects, released where the original's died. An op that
//! can throw runs under the host's guard: the exception is kept in the C++ frame and the Rust stops, releasing the
//! locals in the original's unwinding order, and the C++ entry rethrows it. The glue and the init catch a
//! `ConfigObjectException` themselves (`CAUGHT`), as the original's `catch` did.

use std::ffi::{c_char, c_void, CStr};

use crate::compcfg::resource_file;

/// The host's ops (one C++ statement each). `c` is the config (a `ComponentConfig*`) or the object the op names.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgLoadOp {
    /// `c->SetComponent(a)` (guarded).
    SetComponent = 0,
    /// The window manager's `SetCacheImages(a != 0)`.
    SetCacheImages,
    /// A new `SmartPtr<ResourceConfigTable>` holding `ComponentConfig::mResourceConfigTable` (the original's
    /// `oldTable` local).
    TableHold,
    /// A new `ResourceConfigTable` (no reference).
    TableNew,
    /// `ComponentConfig::mResourceConfigTable = (ResourceConfigTable*)a`.
    TableSet,
    /// `ComponentConfig::mResourceConfigTable = *(SmartPtr<ResourceConfigTable>*)c`.
    TableRestore,
    /// The `SmartPtr<ResourceConfigTable>` `c` deleted.
    TableDrop,
    /// `gShowParseTime`.
    ShowParseTime,
    /// `gShowParseTime = a != 0`.
    SetShowParseTime,
    /// A new `ConfigParser` (the original's `localParser`).
    ParserNew,
    /// The `ConfigParser` `c` deleted.
    ParserFree,
    /// `((ConfigParser*)c)->SetWarningsAsErrors(true)`.
    WarningsAsErrors,
    /// `c->ParseTopLevel(*(ConfigParser*)a, the path b, d bytes)` (guarded).
    ParseTopLevel,
    /// `((ConfigParser*)c)->CloseFile()`.
    CloseFile,
    /// `c->mConfigError = ((ConfigParser*)a)->GetAbortReason()`.
    AbortReason,
    /// `ComponentConfig::mResourceConfigTable->SafeGlueResources()` (guarded; `CAUGHT` for a ConfigObjectException,
    /// its text kept).
    Glue,
    /// `c->InitComponents()` (as `Glue`).
    InitComponents,
    /// `c->mConfigError` = the caught exception's text.
    CaughtError,
    /// `gLinesParsed`.
    LinesParsed,
    /// `*(double*)a` = `PerfTimer::GetElapsedTime()`.
    ElapsedTime,
    /// `*(double*)a` = `PerfTimer::GetCountedTime()`.
    CountedTime,
    /// `*(double*)a` = `PerfTimer::GetPercentTime()`.
    PercentTime,
    /// The "Config Parse Time" message box with the text `a`, `b` bytes (guarded).
    ParseTimeBox,
}

/// What `Glue` and `InitComponents` answer for a `ConfigObjectException` they caught.
pub const EE_WON_CFG_LOAD_CAUGHT: isize = 1;

/// The host: `frame` the C++ call's frame; `ok` cleared by an op that threw (its exception kept in the frame).
pub type EeWonCfgLoadHost = Option<
    unsafe extern "C" fn(
        frame: *mut c_void,
        op: EeWonCfgLoadOp,
        c: *mut c_void,
        a: isize,
        b: isize,
        d: isize,
        ok: *mut bool,
    ) -> isize,
>;

struct Host {
    f: unsafe extern "C" fn(*mut c_void, EeWonCfgLoadOp, *mut c_void, isize, isize, isize, *mut bool) -> isize,
    frame: *mut c_void,
}

/// An op threw: the frame holds the exception.
struct Thrown;

impl Host {
    fn op(&self, op: EeWonCfgLoadOp, c: *mut c_void, a: isize, b: isize, d: isize) -> Result<isize, Thrown> {
        let mut ok = true;
        // SAFETY: the host's contract (its objects are the C++ ones handed to us)
        let r = unsafe { (self.f)(self.frame, op, c, a, b, d, &mut ok) };
        if ok {
            Ok(r)
        } else {
            Err(Thrown)
        }
    }
    /// An op that cannot throw.
    fn call(&self, op: EeWonCfgLoadOp, c: *mut c_void, a: isize) -> isize {
        self.op(op, c, a, 0, 0).unwrap_or(0)
    }
    fn time(&self, op: EeWonCfgLoadOp) -> f64 {
        let mut v = 0f64;
        self.call(op, std::ptr::null_mut(), &mut v as *mut f64 as isize);
        v
    }
}

/// The parse-time report's text, as the original's `snprintf` into 512 bytes made it.
pub fn parse_time_text(lines: i32, elapsed: f64, counted: f64, percent: f64) -> Vec<u8> {
    // `(int)(percent * 100)`: x86's conversion, the integer indefinite for a value out of range or NaN
    let p = percent * 100.0;
    let whole = if p > -2_147_483_649.0 && p < 2_147_483_648.0 { p as i32 } else { i32::MIN };
    let mut t =
        format!("Lines parsed: {lines}\nConfig Parse Time: {elapsed:.2} ({counted:.2}) (= %{whole})").into_bytes();
    t.truncate(511);
    t
}

/// The body after the locals are made: whether the load succeeded.
fn body(
    h: &Host,
    cfg: *mut c_void,
    parser: *mut c_void,
    path: &[u8],
    old_table: *mut c_void,
    old_show: bool,
) -> Result<bool, Thrown> {
    use EeWonCfgLoadOp as O;
    let null = std::ptr::null_mut();
    if h.op(O::ParseTopLevel, cfg, parser as isize, path.as_ptr() as isize, path.len() as isize)? == 0 {
        h.call(O::CloseFile, parser, 0);
        h.call(O::AbortReason, cfg, parser as isize);
        return Ok(false);
    }
    for step in [O::Glue, O::InitComponents] {
        if h.op(step, cfg, 0, 0, 0)? == EE_WON_CFG_LOAD_CAUGHT {
            h.call(O::CaughtError, cfg, 0);
            return Ok(false);
        }
    }
    if h.call(O::ShowParseTime, null, 0) != 0 {
        // the original's snprintf arguments, evaluated as the game's compiler did (right to left)
        let percent = h.time(O::PercentTime);
        let counted = h.time(O::CountedTime);
        let elapsed = h.time(O::ElapsedTime);
        let lines = h.call(O::LinesParsed, null, 0) as i32;
        let text = parse_time_text(lines, elapsed, counted, percent);
        h.op(O::ParseTimeBox, null, text.as_ptr() as isize, text.len() as isize, 0)?;
    }
    h.call(O::SetShowParseTime, null, old_show as isize);
    h.call(O::SetCacheImages, null, 0);
    h.call(O::TableRestore, old_table, 0);
    Ok(true)
}

/// `ComponentConfig::ReadConfig_Unsafe(component, file, table, parser)` of config `cfg`: 1 true, 0 false, 2 an op
/// threw (the C++ rethrows the frame's exception).
///
/// # Safety
/// `host` and `frame` are the C++ end's; `file` is a NUL-terminated text or NULL; the pointers are the C++ objects
/// the call names (`table` and `parser` NULL for none).
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_load(
    host: EeWonCfgLoadHost,
    frame: *mut c_void,
    cfg: *mut c_void,
    component: *mut c_void,
    file: *const c_char,
    table: *mut c_void,
    parser: *mut c_void,
) -> i32 {
    use EeWonCfgLoadOp as O;
    let Some(f) = host else { return 2 };
    let h = Host { f, frame };
    let null = std::ptr::null_mut();
    if h.op(O::SetComponent, cfg, component as isize, 0, 0).is_err() {
        return 2;
    }
    h.call(O::SetCacheImages, null, 1);
    let old_table = h.call(O::TableHold, null, 0) as *mut c_void;
    let t = if table.is_null() { h.call(O::TableNew, null, 0) } else { table as isize };
    h.call(O::TableSet, null, t);
    let old_show = h.call(O::ShowParseTime, null, 0) != 0;
    h.call(O::SetShowParseTime, null, 0);
    let name: &[u8] = if file.is_null() {
        &[]
    } else {
        // SAFETY: the contract (a NUL-terminated text)
        unsafe { CStr::from_ptr(file) }.to_bytes()
    };
    let path = resource_file(name);
    let local = h.call(O::ParserNew, null, 0) as *mut c_void;
    let active = if parser.is_null() { local } else { parser };
    h.call(O::WarningsAsErrors, active, 0);
    let r = body(&h, cfg, active, &path, old_table, old_show);
    // the original's locals die in reverse order: the local parser, then the old table's SmartPtr
    h.call(O::ParserFree, local, 0);
    h.call(O::TableDrop, old_table, 0);
    match r {
        Ok(true) => 1,
        Ok(false) => 0,
        Err(Thrown) => 2,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_report_is_the_originals_format() {
        assert_eq!(
            parse_time_text(12, 1.234, 0.5, 0.4567),
            b"Lines parsed: 12\nConfig Parse Time: 1.23 (0.50) (= %45)".to_vec()
        );
        assert!(parse_time_text(0, 0.0, 0.0, 1e9).ends_with(b"(= %-2147483648)"));
        assert!(parse_time_text(0, 0.0, 0.0, f64::NAN).ends_with(b"(= %-2147483648)"));
    }
}
