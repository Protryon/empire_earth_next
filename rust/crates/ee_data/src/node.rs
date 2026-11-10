//! The parsed text: a scalar, a mapping (keys in file order) or a list, with each scalar's text as raw bytes
//! (`EEDataText::Node`; docs/RUST_PLAN.md 2.3: text is UTF-8 bytes, never `String`).

use std::borrow::Borrow;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::ops::Deref;
use std::rc::Rc;

/// A text's bytes: a slice of the source it was parsed from (shared, no copy), or static. Parsing a table makes no
/// allocation per scalar or key: a node holds its source and a range (docs: PORT_LEARNINGS "R2 performance").
#[derive(Clone)]
pub struct Bytes(Repr);

#[derive(Clone)]
enum Repr {
    Static(&'static [u8]),
    Shared(Rc<[u8]>, u32, u32),
}

impl Bytes {
    /// `src[start..end]`, sharing `src`.
    pub fn slice(src: &Rc<[u8]>, start: usize, end: usize) -> Bytes {
        debug_assert!(start <= end && end <= src.len());
        if start == end {
            return Bytes::default();
        }
        Bytes(Repr::Shared(src.clone(), start as u32, end as u32))
    }

    /// A slice `part` of `src`'s bytes (a subslice of `&src[..]`), sharing `src`.
    pub fn within(src: &Rc<[u8]>, part: &[u8]) -> Bytes {
        let start = part.as_ptr() as usize - src.as_ptr() as usize;
        debug_assert!(start + part.len() <= src.len());
        Bytes::slice(src, start, start + part.len())
    }

    /// A copy of `bytes` (one allocation; none when empty).
    pub fn copy_of(bytes: &[u8]) -> Bytes {
        if bytes.is_empty() {
            return Bytes::default();
        }
        Bytes(Repr::Shared(Rc::from(bytes), 0, bytes.len() as u32))
    }

    pub fn as_slice(&self) -> &[u8] {
        match &self.0 {
            Repr::Static(s) => s,
            Repr::Shared(src, a, b) => &src[*a as usize..*b as usize],
        }
    }
}

impl Default for Bytes {
    fn default() -> Self {
        Bytes(Repr::Static(b""))
    }
}

impl From<Vec<u8>> for Bytes {
    fn from(v: Vec<u8>) -> Self {
        if v.is_empty() {
            return Bytes::default();
        }
        let n = v.len();
        Bytes(Repr::Shared(Rc::from(v), 0, n as u32))
    }
}

impl From<&'static [u8]> for Bytes {
    fn from(s: &'static [u8]) -> Self {
        Bytes(Repr::Static(s))
    }
}

impl<const N: usize> From<&'static [u8; N]> for Bytes {
    fn from(s: &'static [u8; N]) -> Self {
        Bytes(Repr::Static(s))
    }
}

impl Deref for Bytes {
    type Target = [u8];
    fn deref(&self) -> &[u8] {
        self.as_slice()
    }
}

impl AsRef<[u8]> for Bytes {
    fn as_ref(&self) -> &[u8] {
        self.as_slice()
    }
}

impl Borrow<[u8]> for Bytes {
    fn borrow(&self) -> &[u8] {
        self.as_slice()
    }
}

impl Hash for Bytes {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.as_slice().hash(state)
    }
}

impl fmt::Debug for Bytes {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "b\"{}\"", self.as_slice().escape_ascii())
    }
}

impl PartialEq for Bytes {
    fn eq(&self, o: &Bytes) -> bool {
        self.as_slice() == o.as_slice()
    }
}
impl Eq for Bytes {}
impl PartialEq<[u8]> for Bytes {
    fn eq(&self, o: &[u8]) -> bool {
        self.as_slice() == o
    }
}
impl PartialEq<&[u8]> for Bytes {
    fn eq(&self, o: &&[u8]) -> bool {
        self.as_slice() == *o
    }
}
impl<const N: usize> PartialEq<[u8; N]> for Bytes {
    fn eq(&self, o: &[u8; N]) -> bool {
        self.as_slice() == o
    }
}
impl<const N: usize> PartialEq<&[u8; N]> for Bytes {
    fn eq(&self, o: &&[u8; N]) -> bool {
        self.as_slice() == *o
    }
}
impl PartialEq<Vec<u8>> for Bytes {
    fn eq(&self, o: &Vec<u8>) -> bool {
        self.as_slice() == o.as_slice()
    }
}
impl PartialEq<Bytes> for Vec<u8> {
    fn eq(&self, o: &Bytes) -> bool {
        self.as_slice() == o.as_slice()
    }
}
impl PartialEq<Bytes> for [u8] {
    fn eq(&self, o: &Bytes) -> bool {
        self == o.as_slice()
    }
}
impl PartialEq<Bytes> for &[u8] {
    fn eq(&self, o: &Bytes) -> bool {
        *self == o.as_slice()
    }
}

impl crate::msg::Part for Bytes {
    fn put(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(self.as_slice());
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Kind {
    #[default]
    Null,
    Scalar,
    Map,
    List,
}

/// One value of a parsed text. A placed record is held on to as its list (reference counted) and its index there.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Node {
    pub kind: Kind,
    pub quoted: bool,
    pub line: i64,
    pub text: Bytes,
    pub keys: Vec<Bytes>,
    pub items: Vec<Node>,
}

impl Node {
    /// A scalar value.
    pub fn scalar(text: impl Into<Bytes>, quoted: bool, line: i64) -> Node {
        Node { kind: Kind::Scalar, quoted, line, text: text.into(), ..Node::default() }
    }

    /// An empty mapping or list.
    pub fn collection(kind: Kind, line: i64) -> Node {
        Node { kind, line, ..Node::default() }
    }

    /// Null, an empty plain scalar, or a plain `~` / `null`.
    pub fn is_missing(&self) -> bool {
        self.kind == Kind::Null
            || (self.kind == Kind::Scalar
                && !self.quoted
                && (self.text.is_empty() || self.text == b"~" || self.text == b"null"))
    }

    /// The value of a key of a mapping (a linear search: mappings are small).
    pub fn find(&self, key: &[u8]) -> Option<&Node> {
        self.keys.iter().position(|k| k == key).map(|i| &self.items[i])
    }
}

/// A key's value is present and not missing (`Present` in the C++: quoted scalars count even when empty; empty and
/// `~`/`null` plain scalars do not).
pub fn present(v: Option<&Node>) -> bool {
    match v {
        None => false,
        Some(n) => {
            !(n.kind == Kind::Scalar && (n.text.is_empty() || (!n.quoted && (n.text == b"~" || n.text == b"null"))))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn missing_scalars() {
        assert!(Node::default().is_missing());
        assert!(Node::scalar(b"".to_vec(), false, 1).is_missing());
        assert!(Node::scalar(b"~".to_vec(), false, 1).is_missing());
        assert!(Node::scalar(b"null".to_vec(), false, 1).is_missing());
        assert!(!Node::scalar(b"~".to_vec(), true, 1).is_missing());
        assert!(!Node::scalar(b"x".to_vec(), false, 1).is_missing());
    }

    #[test]
    fn present_counts_quoted_empty_as_missing() {
        // the C++ Present: a quoted empty scalar is not present, a quoted '~' is
        assert!(!present(None));
        assert!(!present(Some(&Node::scalar(b"".to_vec(), true, 1))));
        assert!(present(Some(&Node::scalar(b"~".to_vec(), true, 1))));
        assert!(!present(Some(&Node::scalar(b"null".to_vec(), false, 1))));
        assert!(present(Some(&Node::collection(Kind::Map, 1))));
    }

    #[test]
    fn find_in_order() {
        let mut m = Node::collection(Kind::Map, 1);
        m.keys.push(b"a".to_vec().into());
        m.items.push(Node::scalar(b"1".to_vec(), false, 1));
        m.keys.push(b"b".into());
        m.items.push(Node::scalar(b"2".to_vec(), false, 2));
        assert_eq!(m.find(b"b").unwrap().text, b"2");
        assert!(m.find(b"c").is_none());
    }

    #[test]
    fn bytes_share_their_source() {
        let src: Rc<[u8]> = Rc::from(&b"key: value"[..]);
        let b = Bytes::slice(&src, 5, 10);
        assert_eq!(b, b"value");
        assert_eq!(Bytes::within(&src, &src[0..3]), b"key".to_vec());
        assert_eq!(Rc::strong_count(&src), 2);
        assert!(Bytes::slice(&src, 4, 4).is_empty());
        assert_eq!(Rc::strong_count(&src), 2); // an empty slice keeps nothing
        assert_eq!(Bytes::copy_of(b"yz"), b"yz");
        assert_eq!(Bytes::copy_of(b""), Bytes::default());
        let owned = Bytes::from(b"x".to_vec());
        assert_eq!(&*owned, b"x");
        assert_eq!(Bytes::from(Vec::new()), Bytes::default());
        let mut set = std::collections::HashMap::new();
        set.insert(b.clone(), 1);
        assert_eq!(set.get(&b"value"[..]), Some(&1));
        assert_eq!(crate::msg!(&b, b'!'), b"value!".to_vec());
        assert_eq!(format!("{b:?}"), "b\"value\"");
    }
}
