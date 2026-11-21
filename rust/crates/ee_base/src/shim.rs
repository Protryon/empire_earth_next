//! One-statement C++ shims for C++ bodies moved to Rust ("calling C++ from Rust is fine", the owner's decision of
//! 2026-11-20; the UI lane's `ee_game::screens::game_cpp` is the first user of the pattern, this is its shared form).
//!
//! A moved body is generic over a trait of the C++ calls it makes; [`cpp_shims!`] makes from one list of
//! `fn name(args) -> ret = symbol;` lines the trait, its FFI implementation (an `extern "C"` block over the C++ shims,
//! absent from a test binary) and its implementation on [`ShimFake`], which logs every call and answers from
//! per-shim queues, so that a unit test asserts the C++ calls a body makes, in order.
//!
//! Each C++ shim is an `extern "C"` function of ONE statement (a call on a C++ object, a field read or write): no
//! choice and no loop, those are the moved body's.

use std::collections::BTreeMap;
use std::fmt;

/// A shim's argument: a number, a pointer as `usize`, or an out pointer the fake writes.
pub trait ShimArg: Copy + fmt::Debug {
    /// The fake's part of the call (an out argument gets the shim's next out value).
    fn fake(self, _name: &'static str, _fake: &mut ShimFake) {}
}

macro_rules! plain_args {
    ($($t:ty),*) => { $(impl ShimArg for $t {})* };
}
plain_args!(u8, u16, u32, i8, i16, i32, u64, i64, usize, isize, f32, f64, bool);

impl<T> ShimArg for *const T {}

/// An out argument (`T*` / `T&` of the C++ call): the fake writes the shim's next out value into it.
#[repr(transparent)]
pub struct ShimOut<T>(pub *mut T);

impl<T> Clone for ShimOut<T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T> Copy for ShimOut<T> {}
impl<T> fmt::Debug for ShimOut<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("out")
    }
}
impl<T> ShimOut<T> {
    pub fn of(out: &mut T) -> Self {
        ShimOut(out)
    }
}

/// A value the fake can write through a [`ShimOut`].
pub trait ShimFromFake {
    fn from_fake(v: i64) -> Self;
}
macro_rules! from_fake {
    ($($t:ty),*) => { $(impl ShimFromFake for $t { fn from_fake(v: i64) -> Self { v as $t } })* };
}
from_fake!(u8, u16, u32, i8, i16, i32, u64, i64, usize, isize, f32, f64);
impl ShimFromFake for bool {
    fn from_fake(v: i64) -> Self {
        v != 0
    }
}
impl ShimFromFake for () {
    fn from_fake(_: i64) -> Self {}
}
impl<T> ShimFromFake for Option<T> {
    fn from_fake(_: i64) -> Self {
        None
    }
}
impl<T> ShimFromFake for *mut T {
    fn from_fake(v: i64) -> Self {
        v as usize as *mut T
    }
}
impl<T> ShimFromFake for *const T {
    fn from_fake(v: i64) -> Self {
        v as usize as *const T
    }
}

impl<T: ShimFromFake> ShimArg for ShimOut<T> {
    fn fake(self, name: &'static str, fake: &mut ShimFake) {
        let v = fake.outs.get_mut(name).and_then(|q| if q.is_empty() { None } else { Some(q.remove(0)) });
        if !self.0.is_null() {
            // SAFETY: the out argument of the call
            unsafe { *self.0 = T::from_fake(v.unwrap_or(0)) };
        }
    }
}

impl<T> ShimArg for *mut T {}

/// The recording fake: every call logged as `name(args)`; answers and out values from per-shim queues (0 once a
/// queue is empty).
#[derive(Default)]
pub struct ShimFake {
    pub log: Vec<String>,
    pub answers: BTreeMap<&'static str, Vec<i64>>,
    pub outs: BTreeMap<&'static str, Vec<i64>>,
}

impl ShimFake {
    /// The shim `name` answers `values`, one per call.
    pub fn answer(&mut self, name: &'static str, values: &[i64]) -> &mut Self {
        self.answers.entry(name).or_default().extend_from_slice(values);
        self
    }
    /// The shim `name`'s out value is `value` (one per call).
    pub fn out(&mut self, name: &'static str, value: i64) -> &mut Self {
        self.outs.entry(name).or_default().push(value);
        self
    }
    /// The next answer of `name` (0 once its queue is empty).
    pub fn next(&mut self, name: &'static str) -> i64 {
        match self.answers.get_mut(name) {
            Some(q) if !q.is_empty() => q.remove(0),
            _ => 0,
        }
    }
    /// The log, each call on its own line.
    pub fn calls(&self) -> String {
        self.log.join("\n")
    }
    /// The calls of the log whose name is `name`.
    pub fn count(&self, name: &str) -> usize {
        self.log.iter().filter(|c| c.split('(').next() == Some(name)).count()
    }
}

/// The FFI implementation of a [`cpp_shims!`] trait: the C++ shims themselves.
pub struct ShimFfi;

/// `cpp_shims! { trait Name; fn name(args) -> ret = symbol; ... }`: the trait `Name`, its implementation on
/// [`ShimFfi`] (`#[cfg(not(test))]` in the invoking crate: a test binary has no C++) and on [`ShimFake`]
/// (`#[cfg(test)]`).
#[macro_export]
macro_rules! cpp_shims {
    (trait $trait:ident; $( fn $name:ident ( $($arg:ident : $ty:ty),* ) $(-> $ret:ty)? = $sym:ident ; )*) => {
        /// The C++ shims (one statement each).
        #[allow(clippy::too_many_arguments, dead_code)]
        pub trait $trait {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)?; )*
        }

        #[cfg(not(test))]
        #[allow(clippy::too_many_arguments)]
        impl $trait for $crate::shim::ShimFfi {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)? {
                extern "C" {
                    fn $sym($($arg: $ty),*) $(-> $ret)?;
                }
                // SAFETY: the shim's own contract: the pointers the moved body hands it are the C++ object's live
                // members and objects, as the C++ body had them
                unsafe { $sym($($arg),*) }
            } )*
        }

        #[cfg(test)]
        #[allow(clippy::too_many_arguments, clippy::unused_unit)]
        impl $trait for $crate::shim::ShimFake {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)? {
                let args: Vec<String> = vec![$(format!("{:?}", $arg)),*];
                $( $crate::shim::ShimArg::fake($arg, stringify!($name), self); )*
                self.log.push(format!("{}({})", stringify!($name), args.join(", ")));
                let v = self.next(stringify!($name));
                $crate::shim::ShimFromFake::from_fake(v)
            } )*
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    crate::cpp_shims! {
        trait Demo;
        fn open(file: usize, mode: u32) -> i32 = ee_shim_demo_open;
        fn size(file: usize, out: ShimOut<u32>) = ee_shim_demo_size;
        fn close(file: usize) = ee_shim_demo_close;
    }

    fn body<C: Demo>(c: &mut C, file: usize) -> u32 {
        if c.open(file, 1) != 0 {
            return 0;
        }
        let mut n = 0u32;
        c.size(file, ShimOut::of(&mut n));
        c.close(file);
        n
    }

    #[test]
    fn the_fake_logs_answers_and_writes_outs() {
        let mut f = ShimFake::default();
        f.out("size", 42);
        assert_eq!(body(&mut f, 7), 42);
        assert_eq!(f.calls(), "open(7, 1)\nsize(7, out)\nclose(7)");
        let mut g = ShimFake::default();
        g.answer("open", &[5]);
        assert_eq!(body(&mut g, 3), 0);
        assert_eq!(g.calls(), "open(3, 1)");
        assert_eq!(g.count("open"), 1);
    }
}
