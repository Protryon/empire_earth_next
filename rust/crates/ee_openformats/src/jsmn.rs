//! A literal port of the jsmn tokenizer inside cgltf 1.15 (non-strict mode, as cgltf builds it). The token stream is
//! what `cgltf.rs` walks, so that a file is accepted or refused exactly as the C++ loader did.

/// jsmn's token types. `Undefined` is the sentinel cgltf puts after the last token.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum TokenType {
    Undefined,
    Object,
    Array,
    String,
    Primitive,
}

/// One token: a value's type, its byte range in the text and (objects, arrays) its child count.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct Token {
    pub ty: TokenType,
    pub start: i32,
    pub end: i32,
    pub size: i32,
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
    fn at(&self, p: usize) -> u8 {
        self.js.get(p).copied().unwrap_or(0)
    }

    // the text ends at the buffer's end or the first NUL
    fn more(&self) -> bool {
        self.pos < self.js.len() && self.js[self.pos] != 0
    }

    fn parse_primitive(&mut self) -> Result<(), JsmnError> {
        let start = self.pos;
        while self.more() {
            match self.js[self.pos] {
                b':' | b'\t' | b'\r' | b'\n' | b' ' | b',' | b']' | b'}' => break,
                c if !(32..127).contains(&c) => {
                    self.pos = start;
                    return Err(JsmnError);
                }
                _ => {}
            }
            self.pos += 1;
        }
        // non-strict: the end of the text ends a primitive too
        self.toks.push(Token { ty: TokenType::Primitive, start: start as i32, end: self.pos as i32, size: 0 });
        self.pos -= 1;
        Ok(())
    }

    fn parse_string(&mut self) -> Result<(), JsmnError> {
        let start = self.pos;
        self.pos += 1;
        while self.more() {
            let c = self.js[self.pos];
            if c == b'"' {
                self.toks.push(Token { ty: TokenType::String, start: start as i32 + 1, end: self.pos as i32, size: 0 });
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

    fn close(&mut self, ty: TokenType) -> Result<(), JsmnError> {
        let mut i = self.toks.len() as i32 - 1;
        while i >= 0 {
            let t = self.toks[i as usize];
            if t.start != -1 && t.end == -1 {
                if t.ty != ty {
                    return Err(JsmnError);
                }
                self.toksuper = -1;
                self.toks[i as usize].end = self.pos as i32 + 1;
                break;
            }
            i -= 1;
        }
        if i == -1 {
            return Err(JsmnError);
        }
        while i >= 0 {
            let t = self.toks[i as usize];
            if t.start != -1 && t.end == -1 {
                self.toksuper = i;
                break;
            }
            i -= 1;
        }
        Ok(())
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
        let c = p.at(p.pos);
        match c {
            b'{' | b'[' => {
                let ty = if c == b'{' { TokenType::Object } else { TokenType::Array };
                p.parent_grew();
                p.toks.push(Token { ty, start: p.pos as i32, end: -1, size: 0 });
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
                    let mut i = p.toks.len() as i32 - 1;
                    while i >= 0 {
                        let t = p.toks[i as usize];
                        if matches!(t.ty, TokenType::Array | TokenType::Object) && t.start != -1 && t.end == -1 {
                            p.toksuper = i;
                            break;
                        }
                        i -= 1;
                    }
                }
            }
            _ => {
                p.parse_primitive()?;
                p.parent_grew();
            }
        }
        p.pos += 1;
    }
    if p.toks.iter().any(|t| t.start != -1 && t.end == -1) {
        return Err(JsmnError);
    }
    if p.toks.is_empty() {
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
        assert_eq!(tokenize(b"12").unwrap()[0].ty, TokenType::Primitive);
    }

    #[test]
    fn a_control_character_or_non_ascii_byte_breaks_a_primitive() {
        assert_eq!(tokenize(b"[1\x012]"), Err(JsmnError));
        assert_eq!(tokenize(b"[1\xc32]"), Err(JsmnError));
    }
}
