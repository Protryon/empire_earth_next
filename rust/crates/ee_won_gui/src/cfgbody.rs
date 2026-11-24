//! The config subclasses' own bodies in Rust (R6 WON round 34): ButtonConfig's FITTEXT (`HandleFitText`),
//! ContainerConfig's PERCENTLAYOUT and COLORSCHEME groups (`HandleInstruction`), PercentLayoutConfig's and
//! ColorSchemeConfig's `PreParse`, FontConfig's `GetFont`, `SetFont` and `GetNamedValue`, ImageConfig's
//! `GetNamedValue` and ImageFilterConfig's `GetImage` (ConfigTableHost.cpp, ContainerConfigGroups.cpp,
//! ComponentLayoutConfigs.cpp, FontConfig.cpp, ImageConfig.cpp).
//!
//! Each C++ statement is a one-statement op of the host (include/ee/wonapi/WONCfgBody.inl); the C++ locals (the
//! texts, the child configs' `SmartPtr`s) are host objects released where the original's died. An op whose
//! statement can throw runs under the host's guard: the exception is kept, the body stops (releasing its locals as
//! the original's unwinding did) and the C++ forward rethrows it.

use std::ffi::c_void;

use crate::guiconf::*;

/// A body.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgBody {
    /// `ComponentConfig::HandleFitText()`: the button grown to the largest of the texts read, its text kept.
    FitText = 0,
    /// `ContainerConfig::HandleInstruction(a, the type b, c bytes)`.
    ContainerInstruction,
    /// `PercentLayoutConfig::PreParse(a)`.
    PercentPreParse,
    /// `ColorSchemeConfig::PreParse(a)`.
    SchemePreParse,
    /// `FontConfig::GetFont()`.
    FontGet,
    /// `FontConfig::SetFont(a)`.
    FontSet,
    /// `FontConfig::GetNamedValue(the name a, b bytes, *(int*)c)`.
    FontNamed,
    /// `ImageConfig::GetNamedValue(the name a, b bytes, *(int*)c)`.
    ImageNamed,
    /// `ImageFilterConfig::GetImage()`.
    FilterImage,
}

/// The host's ops; `k` below is the config the body runs on.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgBodyOp {
    /// The button's `Width()`.
    BtnWidth = 0,
    /// The button's `Height()`.
    BtnHeight,
    /// A new `GUIString` of the button's `GetText()`.
    BtnTextHold,
    /// A new `GUIString` of `k->ReadString(false)` (guarded).
    ReadTextHold,
    /// The button's `SetText(*(GUIString*)x, true)` (guarded).
    BtnSetText,
    /// The button's `GetDesiredSize` into the two ints at `x`.
    BtnDesired,
    /// `k->EndOfString()`.
    EndOfString,
    /// `k->EnsureComma()` (guarded).
    EnsureComma,
    /// The button's `SetSize(x, y)`.
    BtnSetSize,
    /// `GUIString` `x` deleted.
    TextFree,
    /// A new `SmartPtr<PercentLayoutConfig>` of a new PercentLayoutConfig.
    PercentNew,
    /// `k->ParseChild(*(ConfigParser*)y, percent holder x's config, false, true)` (guarded).
    PercentParse,
    /// Percent holder `x`'s `refs[y]`.
    PercentRef,
    /// Percent holder `x`'s `refs[0]` = `k`'s component.
    PercentRefSelf,
    /// The container's `AddChildLayout` of a new PercentLayout of holder `x`'s target and parameters, under
    /// `k`'s `mCurLayoutId`.
    PercentAdd,
    /// Percent holder `x` deleted.
    PercentFree,
    /// A new `SmartPtr<ColorSchemeConfig>` of a new ColorSchemeConfig.
    SchemeNew,
    /// `k->ParseChild(*(ConfigParser*)y, scheme holder x's config, false, true)` (guarded).
    SchemeParse,
    /// The container's `SetColorScheme` of scheme holder `x`'s scheme.
    SchemeSet,
    /// Scheme holder `x` deleted.
    SchemeFree,
    /// `k->ComponentConfig::HandleInstruction((ConfigParser*)x, the type y, z bytes)` (guarded).
    BaseInstruction,
    /// The percent config's `target` = its parent's `Fn386860(its name)`; the target.
    PercentFind,
    /// `((ConfigParser*)x)->SetError("Component not found: " + k's name, false)`.
    PercentNotFound,
    /// Config `x`'s parent (as a ComponentConfig*).
    CfgParent,
    /// Config `x`'s component.
    CfgComponent,
    /// Component `x`'s `GetOwnColorScheme()`.
    OwnScheme,
    /// The scheme config's colours = scheme `x`'s.
    CopyScheme,
    /// The window manager's colour scheme.
    DefaultScheme,
    /// The font config's `mNeedRecalcFont`.
    NeedRecalc,
    /// The font config's font.
    FontPtr,
    /// Whether the font config's font's descriptor differs from its own.
    FontStale,
    /// The font config's font = the default window's `GetFont(its descriptor)` (guarded).
    FontFetch,
    /// The font config's font = `x`.
    SetFontPtr,
    /// The font config's descriptor = font `x`'s.
    TakeDescriptor,
    /// The font config's `mNeedRecalcFont = false`.
    ClearRecalc,
    /// The font config's descriptor's size.
    DescSize,
    /// The font config's font's `GetHeight()`.
    FontHeight,
    /// The image config's image's `GetWidth()`.
    ImgWidth,
    /// The image config's image's `GetHeight()`.
    ImgHeight,
    /// The filter config's `mImageIsRawImage`.
    FilterIsRaw,
    /// The filter config's image = its raw image's native image on the default window (guarded).
    FilterNative,
    /// The filter config's `mImageIsRawImage = false`.
    FilterRawDone,
    /// The image config's `mImage.get()`.
    ImagePtr,
}

/// The host: `ok` cleared by an op that threw (its exception kept in the frame).
pub type EeWonCfgBodyHost = Option<
    unsafe extern "C" fn(
        frame: *mut c_void,
        op: EeWonCfgBodyOp,
        k: *mut c_void,
        x: isize,
        y: isize,
        z: isize,
        ok: *mut bool,
    ) -> isize,
>;

struct Thrown;
type R<T> = Result<T, Thrown>;

struct Ops {
    f: unsafe extern "C" fn(*mut c_void, EeWonCfgBodyOp, *mut c_void, isize, isize, isize, *mut bool) -> isize,
    frame: *mut c_void,
    k: *mut c_void,
}

impl Ops {
    fn op3(&self, op: EeWonCfgBodyOp, x: isize, y: isize, z: isize) -> R<isize> {
        let mut ok = true;
        // SAFETY: the host's contract
        let r = unsafe { (self.f)(self.frame, op, self.k, x, y, z, &mut ok) };
        if ok {
            Ok(r)
        } else {
            Err(Thrown)
        }
    }
    fn op(&self, op: EeWonCfgBodyOp, x: isize) -> R<isize> {
        self.op3(op, x, 0, 0)
    }
    /// An op that cannot throw.
    fn call(&self, op: EeWonCfgBodyOp, x: isize) -> isize {
        self.op(op, x).unwrap_or(0)
    }
    fn call2(&self, op: EeWonCfgBodyOp, x: isize, y: isize) -> isize {
        self.op3(op, x, y, 0).unwrap_or(0)
    }
}

unsafe fn text<'a>(p: isize, n: isize) -> &'a [u8] {
    if p == 0 || n <= 0 {
        &[]
    } else {
        // SAFETY: the caller's bytes
        unsafe { std::slice::from_raw_parts(p as *const u8, n as usize) }
    }
}

/// HandleFitText's loop: the texts read until the end of the value, the size grown to each one's desired size.
fn fit_text(o: &Ops, size: &mut [i32; 2]) -> R<()> {
    use EeWonCfgBodyOp as O;
    loop {
        let t = o.op(O::ReadTextHold, 0)?;
        let r = (|| -> R<bool> {
            o.op(O::BtnSetText, t)?;
            let mut d = [0i32; 2];
            o.call(O::BtnDesired, d.as_mut_ptr() as isize);
            size[0] = size[0].max(d[0]);
            size[1] = size[1].max(d[1]);
            if o.call(O::EndOfString, 0) != 0 {
                return Ok(true);
            }
            o.op(O::EnsureComma, 0)?;
            Ok(false)
        })();
        o.call(O::TextFree, t);
        if r? {
            return Ok(());
        }
    }
}

fn run(o: &Ops, which: EeWonCfgBody, a: isize, b: isize, c: isize) -> R<isize> {
    use EeWonCfgBody as W;
    use EeWonCfgBodyOp as O;
    Ok(match which {
        W::FitText => {
            let mut size = [o.call(O::BtnWidth, 0) as i32, o.call(O::BtnHeight, 0) as i32];
            let old = o.call(O::BtnTextHold, 0);
            let r = fit_text(o, &mut size).and_then(|()| {
                o.op(O::BtnSetText, old)?;
                o.call2(O::BtnSetSize, size[0] as isize, size[1] as isize);
                Ok(())
            });
            o.call(O::TextFree, old);
            r?;
            0
        }
        W::ContainerInstruction => {
            // SAFETY: the type's bytes (the forward's)
            let ty = unsafe { text(b, c) };
            match key_index(EE_WON_CFG_KEYS_CONTAINERCONFIG_HANDLEINSTRUCTION_TYPE, ty) {
                0 => {
                    let h = o.call(O::PercentNew, 0);
                    let r = o.op3(O::PercentParse, h, a, 0).map(|parsed| {
                        if parsed != 0 {
                            if (0..4).all(|i| o.call2(O::PercentRef, h, i) == 0) {
                                o.call(O::PercentRefSelf, h);
                            }
                            o.call(O::PercentAdd, h);
                        }
                    });
                    o.call(O::PercentFree, h);
                    r?;
                    1
                }
                1 => {
                    let h = o.call(O::SchemeNew, 0);
                    let r = o.op3(O::SchemeParse, h, a, 0).map(|parsed| {
                        if parsed != 0 {
                            o.call(O::SchemeSet, h);
                        }
                    });
                    o.call(O::SchemeFree, h);
                    r?;
                    1
                }
                _ => (o.op3(O::BaseInstruction, a, b, c)? != 0) as isize,
            }
        }
        W::PercentPreParse => {
            if o.op(O::PercentFind, 0)? == 0 {
                o.call(O::PercentNotFound, a);
                0
            } else {
                1
            }
        }
        W::SchemePreParse => {
            let mut parent = o.call(O::CfgParent, o.k as isize);
            while parent != 0 {
                let component = o.call(O::CfgComponent, parent);
                if component != 0 {
                    let scheme = o.call(O::OwnScheme, component);
                    if scheme != 0 {
                        o.call(O::CopyScheme, scheme);
                        return Ok(1);
                    }
                }
                parent = o.call(O::CfgParent, parent);
            }
            o.call(O::CopyScheme, o.call(O::DefaultScheme, 0));
            1
        }
        W::FontGet => {
            if o.call(O::NeedRecalc, 0) != 0 && (o.call(O::FontPtr, 0) == 0 || o.call(O::FontStale, 0) != 0) {
                o.op(O::FontFetch, 0)?;
            }
            o.call(O::FontPtr, 0)
        }
        W::FontSet => {
            o.call(O::SetFontPtr, a);
            if a != 0 {
                o.call(O::TakeDescriptor, a);
            }
            o.call(O::ClearRecalc, 0);
            0
        }
        W::FontNamed | W::ImageNamed => {
            // SAFETY: the name's bytes (the forward's)
            let name = unsafe { text(a, b) };
            let v = if which == W::FontNamed {
                let k = key_index(EE_WON_CFG_KEYS_FONTCONFIG_GETNAMEDVALUE_NAME, name);
                if o.call(O::FontPtr, 0) == 0 {
                    None
                } else {
                    match k {
                        0 => Some(o.call(O::DescSize, 0)),
                        1 => Some(o.call(O::FontHeight, 0)),
                        _ => None,
                    }
                }
            } else {
                match key_index(EE_WON_CFG_KEYS_IMAGECONFIG_GETNAMEDVALUE_INNAME, name) {
                    0 => Some(o.call(O::ImgWidth, 0)),
                    1 => Some(o.call(O::ImgHeight, 0)),
                    _ => None,
                }
            };
            match v {
                Some(v) => {
                    // SAFETY: the forward's int
                    unsafe { *(c as *mut i32) = v as i32 };
                    1
                }
                None => 0,
            }
        }
        W::FilterImage => {
            if o.call(O::FilterIsRaw, 0) != 0 {
                o.op(O::FilterNative, 0)?;
                o.call(O::FilterRawDone, 0);
            }
            o.call(O::ImagePtr, 0)
        }
    })
}

/// The body `which` of config `k` with the forward's arguments: 0 with its answer at `out`, 2 when an op threw
/// (the host rethrows the frame's exception).
///
/// # Safety
/// `host`, `frame` and `k` are the C++ end's; the arguments are what `which` says; `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_body(
    host: EeWonCfgBodyHost,
    frame: *mut c_void,
    k: *mut c_void,
    which: EeWonCfgBody,
    a: isize,
    b: isize,
    c: isize,
    out: *mut isize,
) -> i32 {
    let Some(f) = host else { return 2 };
    match run(&Ops { f, frame, k }, which, a, b, c) {
        Ok(v) => {
            if !out.is_null() {
                // SAFETY: the contract
                unsafe { *out = v };
            }
            0
        }
        Err(Thrown) => 2,
    }
}
