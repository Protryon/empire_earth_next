//! The parsed text: a scalar, a mapping (keys in file order) or a list, with each scalar's text as raw bytes
//! (`EEDataText::Node`; docs/RUST_PLAN.md 2.3: text is Windows-1252 bytes, never `String`).

use std::rc::Rc;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Kind {
    #[default]
    Null,
    Scalar,
    Map,
    List,
}

/// One value of a parsed text. Children are reference counted so that a record can be held on to after the parser
/// is done (a placed record outlives the document walk).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Node {
    pub kind: Kind,
    pub quoted: bool,
    pub line: i64,
    pub text: Vec<u8>,
    pub keys: Vec<Vec<u8>>,
    pub items: Vec<Rc<Node>>,
}

impl Node {
    /// A scalar value.
    pub fn scalar(text: Vec<u8>, quoted: bool, line: i64) -> Node {
        Node { kind: Kind::Scalar, quoted, line, text, ..Node::default() }
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
    pub fn find(&self, key: &[u8]) -> Option<&Rc<Node>> {
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
        m.keys.push(b"a".to_vec());
        m.items.push(Rc::new(Node::scalar(b"1".to_vec(), false, 1)));
        m.keys.push(b"b".to_vec());
        m.items.push(Rc::new(Node::scalar(b"2".to_vec(), false, 2)));
        assert_eq!(m.find(b"b").unwrap().text, b"2");
        assert!(m.find(b"c").is_none());
    }
}
