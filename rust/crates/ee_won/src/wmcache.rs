//! WindowManager's caches (R6 WON round 33, the second move): the named fonts, the default font, and the native,
//! raw and delay-load image caches with their caching count (the original's WindowManager.cpp).
//!
//! The maps are keyed by the file or font name compared as the original's `StringLessNoCase`
//! (`strcase::compare_no_case`), so "A.png" and "a.PNG" are one entry. Each value is a C++ object held as the
//! original's `SmartPtr` held it (`WmRef` ops); the decoders, the default window's font and the new delay-load
//! image are host ops that answer an object carrying one reference, which the Rust adopts. A pointer the Rust
//! answers to the C++ carries one reference for the caller (the C++ forwarder adopts it into the `SmartPtr` it
//! returns), except the fonts, which the original answered as raw pointers.

use std::cmp::Ordering;
use std::collections::BTreeMap;

use crate::guihost::EeWonGuiOp as Op;
use crate::strcase;
use crate::wmobj::EeWonWm;

/// A name compared as `StringLessNoCase`.
#[derive(Clone, Debug)]
pub struct NoCase(pub Vec<u8>);

impl PartialEq for NoCase {
    fn eq(&self, other: &Self) -> bool {
        self.cmp(other) == Ordering::Equal
    }
}
impl Eq for NoCase {}
impl PartialOrd for NoCase {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
impl Ord for NoCase {
    fn cmp(&self, other: &Self) -> Ordering {
        strcase::compare_no_case(&self.0, &other.0).cmp(&0)
    }
}

/// Which kind of object a reference op is on (`WmRef`'s `a`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWmRefKind {
    Font = 1,
    NativeImage,
    RawImage,
    /// a delay-load image (an `Image`)
    Image,
}

/// The caches' state.
#[derive(Debug, Default)]
pub struct Caches {
    pub fonts: BTreeMap<NoCase, isize>,
    pub default_font: isize,
    pub native: BTreeMap<NoCase, isize>,
    pub raw: BTreeMap<NoCase, isize>,
    pub delay: BTreeMap<NoCase, isize>,
    /// `mCacheImageCount` (the original left it unset; it starts at 0)
    pub count: i32,
}

impl Caches {
    pub fn map(&mut self, kind: EeWonWmRefKind) -> &mut BTreeMap<NoCase, isize> {
        match kind {
            EeWonWmRefKind::Font => &mut self.fonts,
            EeWonWmRefKind::NativeImage => &mut self.native,
            EeWonWmRefKind::RawImage => &mut self.raw,
            EeWonWmRefKind::Image => &mut self.delay,
        }
    }
}

impl EeWonWm {
    fn cache_ref(&self, kind: EeWonWmRefKind, p: isize, hold: bool) {
        if p != 0 {
            self.op(Op::WmRef, kind as isize, p, isize::from(hold), 0);
        }
    }

    /// `map[key] = value`: the entry made empty when missing, then the `SmartPtr`'s assignment (the old object
    /// released, the new one held; nothing when they are the same).
    fn cache_assign(&self, kind: EeWonWmRefKind, key: &[u8], value: isize) {
        let old = *self.caches.borrow_mut().map(kind).entry(NoCase(key.to_vec())).or_insert(0);
        if old == value {
            return;
        }
        self.cache_ref(kind, old, false);
        if let Some(v) = self.caches.borrow_mut().map(kind).get_mut(&NoCase(key.to_vec())) {
            *v = value;
        }
        self.cache_ref(kind, value, true);
    }

    /// `SetNamedFont`.
    pub fn set_named_font(&self, name: &[u8], font: isize) {
        self.cache_assign(EeWonWmRefKind::Font, name, font);
    }

    /// `GetNamedFont`: the font stored under the name (raw), or the default font.
    pub fn named_font(&self, name: &[u8]) -> isize {
        let found = self.caches.borrow().fonts.get(&NoCase(name.to_vec())).copied();
        match found {
            Some(f) => f,
            None => self.default_font(),
        }
    }

    /// `SetDefaultFont` (the header's inline `mDefaultFont = font`).
    pub fn set_default_font(&self, font: isize) {
        let old = self.caches.borrow().default_font;
        if old == font {
            return;
        }
        self.cache_ref(EeWonWmRefKind::Font, old, false);
        self.caches.borrow_mut().default_font = font;
        self.cache_ref(EeWonWmRefKind::Font, font, true);
    }

    /// `GetDefaultFont`: made from the default window (Tahoma 8) the first time; the window's answer is a
    /// temporary `SmartPtr` assigned to the member and then given back.
    pub fn default_font(&self) -> isize {
        if self.caches.borrow().default_font == 0 {
            let f = self.op(Op::WmDefaultWindowFont, 0, 0, 0, 0);
            self.set_default_font(f);
            self.cache_ref(EeWonWmRefKind::Font, f, false);
        }
        self.caches.borrow().default_font
    }

    /// `SetCacheImages`: one request more or less; at none the native and raw caches are emptied (their images
    /// given back in name order; the original's tree went in its own order).
    pub fn set_cache_images(&self, cache: bool) {
        let clear = {
            let mut c = self.caches.borrow_mut();
            if cache {
                c.count = c.count.wrapping_add(1);
                false
            } else {
                c.count = c.count.wrapping_sub(1);
                if c.count <= 0 {
                    c.count = 0;
                    true
                } else {
                    false
                }
            }
        };
        if clear {
            for kind in [EeWonWmRefKind::NativeImage, EeWonWmRefKind::RawImage] {
                let old = std::mem::take(self.caches.borrow_mut().map(kind));
                for (_, v) in old {
                    self.cache_ref(kind, v, false);
                }
            }
        }
    }

    /// The cached decode of `DecodeImage` and `DecodeImageRaw`: the entry made when missing, decoded when empty
    /// (the decoder's temporary assigned, then given back), its object answered with a reference for the caller.
    fn cached_decode(&self, kind: EeWonWmRefKind, path: &[u8], decode: &dyn Fn() -> isize) -> isize {
        if self.caches.borrow().count == 0 {
            return decode();
        }
        let have = *self.caches.borrow_mut().map(kind).entry(NoCase(path.to_vec())).or_insert(0);
        if have == 0 {
            let img = decode();
            self.cache_assign(kind, path, img);
            self.cache_ref(kind, img, false);
        }
        let v = self.caches.borrow_mut().map(kind).get(&NoCase(path.to_vec())).copied().unwrap_or(0);
        self.cache_ref(kind, v, true);
        v
    }

    /// `DecodeImage(DisplayContext*, path)`: NULL without a context.
    pub fn decode_image(&self, context: isize, path: &[u8]) -> isize {
        if context == 0 {
            return 0;
        }
        let p = path.as_ptr() as isize;
        let n = path.len() as isize;
        self.cached_decode(EeWonWmRefKind::NativeImage, path, &|| self.op(Op::WmDecode, context, p, n, 0))
    }

    /// `DecodeImageRaw`.
    pub fn decode_image_raw(&self, path: &[u8]) -> isize {
        let p = path.as_ptr() as isize;
        let n = path.len() as isize;
        self.cached_decode(EeWonWmRefKind::RawImage, path, &|| self.op(Op::WmDecodeRaw, p, n, 0, 0))
    }

    /// `DecodeDelayImage`: an image that loads its file on first use (NULL when the file's attributes cannot be
    /// read); cached by name while caching is on, made only when the entry is new. The new image's reference is
    /// the map's (the original assigned the new object to the entry).
    pub fn decode_delay_image(&self, path: &[u8]) -> isize {
        let p = path.as_ptr() as isize;
        let n = path.len() as isize;
        if self.caches.borrow().count == 0 {
            return self.op(Op::WmDelayImage, p, n, 0, 0);
        }
        let key = NoCase(path.to_vec());
        let inserted = {
            let mut c = self.caches.borrow_mut();
            if c.delay.contains_key(&key) {
                false
            } else {
                c.delay.insert(key.clone(), 0);
                true
            }
        };
        if inserted {
            let img = self.op(Op::WmDelayImage, p, n, 0, 0);
            if let Some(v) = self.caches.borrow_mut().delay.get_mut(&key) {
                *v = img;
            }
        }
        let v = self.caches.borrow().delay.get(&key).copied().unwrap_or(0);
        self.cache_ref(EeWonWmRefKind::Image, v, true);
        v
    }

    /// The caches given back as the original's members were destroyed (in reverse order: the delay-load, raw
    /// and native images, the named fonts, then the default font).
    pub fn release_caches(&self) {
        for kind in [EeWonWmRefKind::Image, EeWonWmRefKind::RawImage, EeWonWmRefKind::NativeImage, EeWonWmRefKind::Font]
        {
            let old = std::mem::take(self.caches.borrow_mut().map(kind));
            for (_, v) in old {
                self.cache_ref(kind, v, false);
            }
        }
        let f = std::mem::take(&mut self.caches.borrow_mut().default_font);
        self.cache_ref(EeWonWmRefKind::Font, f, false);
    }

    /// The caches as one line (the golden's trace): the count, the default font, then each map's entries in
    /// name order.
    pub fn cache_line(&self) -> String {
        let c = self.caches.borrow();
        let mut s = format!("cache {} {}", c.count, c.default_font);
        for (tag, m) in [("fonts", &c.fonts), ("native", &c.native), ("raw", &c.raw), ("delay", &c.delay)] {
            s += " ";
            s += tag;
            for (k, v) in m {
                s += &format!(" {}={}", String::from_utf8_lossy(&k.0), v);
            }
        }
        s
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names_compare_without_case() {
        let mut m = BTreeMap::new();
        m.insert(NoCase(b"Lobby.png".to_vec()), 1);
        assert_eq!(m.get(&NoCase(b"LOBBY.PNG".to_vec())), Some(&1));
        m.insert(NoCase(b"lobby.PNG".to_vec()), 2);
        assert_eq!(m.len(), 1);
        assert!(NoCase(b"a".to_vec()) < NoCase(b"B".to_vec()));
        assert!(NoCase(b"ab".to_vec()) > NoCase(b"A".to_vec()));
    }
}
