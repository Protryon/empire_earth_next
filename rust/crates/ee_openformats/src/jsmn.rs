//! A literal port of the jsmn tokenizer inside cgltf 1.15, in the configuration cgltf builds it: `JSMN_STRICT` and
//! `JSMN_PARENT_LINKS`. The token stream is what `cgltf.rs` walks, so that a file is accepted or refused exactly as the
//! C++ loader did. The parent links are behaviour, not an optimisation: after a malformed `"key":"value":,` a comma
//! returns to the value's parent (the key), which miscounts the object and makes cgltf refuse it.

/// jsmn's token types. `Undefined` is the sentinel cgltf puts after the last token.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum TokenType {
    Undefined,
    Object,
    Array,
    String,
    Primitive,
}

/// One token: a value's type, its byte range in the text, its child count (objects, arrays, keys) and its parent.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct Token {
    pub ty: TokenType,
    pub start: i32,
    pub end: i32,
    pub size: i32,
    pub parent: i32,
}

/// The tokenizer's failure. The kind does not matter: cgltf reports every one as "invalid JSON".
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) struct JsmnError;

struct Parser<'a> {
    js: &'a [u8],
    pos: usize,
    toks: Vec<Token>,
    toksuper: i32,
}

impl Parser<'_> {
    // the text ends at the buffer's end or the first NUL
    fn more(&self) -> bool {
        self.pos < self.js.len() && self.js[self.pos] != 0
    }

    fn parse_primitive(&mut self) -> Result<(), JsmnError> {
        let start = self.pos;
        let mut found = false;
        while self.more() {
            match self.js[self.pos] {
                b'\t' | b'\r' | b'\n' | b' ' | b',' | b']' | b'}' => {
                    found = true;
                    break;
                }
                c if !(32..127).contains(&c) => {
                    self.pos = start;
                    return Err(JsmnError);
                }
                _ => {}
            }
            self.pos += 1;
        }
        if !found {
            // strict: a primitive must be followed by a comma, a closing bracket or white space
            self.pos = start;
            return Err(JsmnError);
        }
        let parent = self.toksuper;
        self.toks.push(Token { ty: TokenType::Primitive, start: start as i32, end: self.pos as i32, size: 0, parent });
        self.pos -= 1;
        Ok(())
    }

    fn parse_string(&mut self) -> Result<(), JsmnError> {
        let start = self.pos;
        self.pos += 1;
        while self.more() {
            let c = self.js[self.pos];
            if c == b'"' {
                let parent = self.toksuper;
                self.toks.push(Token {
                    ty: TokenType::String,
                    start: start as i32 + 1,
                    end: self.pos as i32,
                    size: 0,
                    parent,
                });
                return Ok(());
            }
            if c == b'\\' && self.pos + 1 < self.js.len() {
                self.pos += 1;
                match self.js[self.pos] {
                    b'"' | b'/' | b'\\' | b'b' | b'f' | b'r' | b'n' | b't' => {}
                    b'u' => {
                        self.pos += 1;
                        let mut i = 0;
                        while i < 4 && self.more() {
                            let h = self.js[self.pos];
                            if !h.is_ascii_hexdigit() {
                                self.pos = start;
                                return Err(JsmnError);
                            }
                            self.pos += 1;
                            i += 1;
                        }
                        self.pos -= 1;
                    }
                    _ => {
                        self.pos = start;
                        return Err(JsmnError);
                    }
                }
            }
            self.pos += 1;
        }
        self.pos = start;
        Err(JsmnError)
    }

    // the closing-bracket walk up the parent links
    fn close(&mut self, ty: TokenType) -> Result<(), JsmnError> {
        if self.toks.is_empty() {
            return Err(JsmnError);
        }
        let mut i = self.toks.len() - 1;
        loop {
            let t = self.toks[i];
            if t.start != -1 && t.end == -1 {
                if t.ty != ty {
                    return Err(JsmnError);
                }
                self.toks[i].end = self.pos as i32 + 1;
                self.toksuper = t.parent;
                return Ok(());
            }
            if t.parent == -1 {
                if t.ty != ty || self.toksuper == -1 {
                    return Err(JsmnError);
                }
                return Ok(());
            }
            i = t.parent as usize;
        }
    }

    fn parent_grew(&mut self) {
        if self.toksuper != -1 {
            self.toks[self.toksuper as usize].size += 1;
        }
    }
}

/// Tokenizes `js`; `Err` for what jsmn (either of cgltf's two passes) rejects. The result is never empty on `Ok`.
pub(crate) fn tokenize(js: &[u8]) -> Result<Vec<Token>, JsmnError> {
    let mut p = Parser { js, pos: 0, toks: Vec::new(), toksuper: -1 };
    while p.more() {
        let c = p.js[p.pos];
        match c {
            b'{' | b'[' => {
                let ty = if c == b'{' { TokenType::Object } else { TokenType::Array };
                p.parent_grew();
                let parent = p.toksuper;
                p.toks.push(Token { ty, start: p.pos as i32, end: -1, size: 0, parent });
                p.toksuper = p.toks.len() as i32 - 1;
            }
            b'}' | b']' => {
                let ty = if c == b'}' { TokenType::Object } else { TokenType::Array };
                p.close(ty)?;
            }
            b'"' => {
                p.parse_string()?;
                p.parent_grew();
            }
            b'\t' | b'\r' | b'\n' | b' ' => {}
            b':' => p.toksuper = p.toks.len() as i32 - 1,
            b',' => {
                if p.toksuper != -1 && !matches!(p.toks[p.toksuper as usize].ty, TokenType::Array | TokenType::Object) {
                    p.toksuper = p.toks[p.toksuper as usize].parent;
                }
            }
            b'-' | b'0'..=b'9' | b't' | b'f' | b'n' => {
                // strict: a primitive is a value, never a key
                if p.toksuper != -1 {
                    let t = p.toks[p.toksuper as usize];
                    if t.ty == TokenType::Object || (t.ty == TokenType::String && t.size != 0) {
                        return Err(JsmnError);
                    }
                }
                p.parse_primitive()?;
                p.parent_grew();
            }
            _ => return Err(JsmnError),
        }
        p.pos += 1;
    }
    if p.toks.iter().any(|t| t.start != -1 && t.end == -1) || p.toks.is_empty() {
        return Err(JsmnError);
    }
    Ok(p.toks)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn kinds(s: &str) -> Vec<(TokenType, i32)> {
        tokenize(s.as_bytes()).unwrap().iter().map(|t| (t.ty, t.size)).collect()
    }

    #[test]
    fn objects_count_keys_and_keys_count_their_value() {
        use TokenType::*;
        assert_eq!(
            kinds(r#"{"a":1,"b":[true,null]}"#),
            vec![(Object, 2), (String, 1), (Primitive, 0), (String, 1), (Array, 2), (Primitive, 0), (Primitive, 0)]
        );
    }

    #[test]
    fn nesting_returns_to_the_enclosing_container() {
        use TokenType::*;
        assert_eq!(
            kinds(r#"[{"a":[1]},2]"#),
            vec![(Array, 2), (Object, 1), (String, 1), (Array, 1), (Primitive, 0), (Primitive, 0)]
        );
    }

    #[test]
    fn a_string_token_excludes_its_quotes_and_keeps_escapes() {
        let t = tokenize(br#""a\u0041\tb""#).unwrap();
        assert_eq!((t[0].start, t[0].end), (1, 11));
    }

    #[test]
    fn refusals() {
        for bad in [&b""[..], b"   ", b"{", b"[1", b"}", b"[1}", br#""abc"#, br#""a\q""#, br#""\uZZZZ""#, b"\x80"] {
            assert_eq!(tokenize(bad), Err(JsmnError), "{bad:?}");
        }
    }

    #[test]
    fn nul_ends_the_text_and_primitives_end_at_the_end() {
        assert_eq!(tokenize(b"[1,2]\0garbage").unwrap().len(), 3);
        assert_eq!(tokenize(b"12"), Err(JsmnError)); // strict: a primitive needs a terminator
        assert_eq!(tokenize(b"12 ").unwrap()[0].ty, TokenType::Primitive);
    }

    #[test]
    fn strict_mode_refuses_bare_words_and_primitive_keys() {
        assert_eq!(tokenize(b"[abc]"), Err(JsmnError));
        assert_eq!(tokenize(b"{1:2}"), Err(JsmnError));
        assert_eq!(tokenize(br#"{"a":1 2}"#), Err(JsmnError)); // the key already has its value
        assert_eq!(tokenize(b"[0x10,nan]").unwrap().len(), 3); // the first letter decides, cgltf reads the text
    }

    #[test]
    fn a_comma_goes_back_through_the_parent_link() {
        use TokenType::*;
        // the comma after the second string returns to its parent, the key "a": "c" then counts as the key's child
        assert_eq!(
            kinds(r#"{"a":"b":,"c":1}"#),
            vec![(Object, 1), (String, 2), (String, 0), (String, 1), (Primitive, 0)]
        );
    }

    #[test]
    fn a_control_character_or_non_ascii_byte_breaks_a_primitive() {
        assert_eq!(tokenize(b"[1\x012]"), Err(JsmnError));
        assert_eq!(tokenize(b"[1\xc32]"), Err(JsmnError));
    }
}
