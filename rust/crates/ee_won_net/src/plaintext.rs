//! Plain text for the lobby's pages (the owner's decision D8: WON's HTML rendering is not ported). Where the lobby
//! showed an HTML page (the message of the day, the terms of use, the help pages) it shows the page's text: tags
//! dropped, line breaks kept from the block tags, entities decoded, white space collapsed as a browser would.
//!
//! Pages are files of Windows-1252 bytes, HTML when their first non-blank character is `<` (as the SDK's
//! `HTMLDocumentGen` decided); anything else is shown as it is.

/// Windows-1252's 0x80..=0x9f (0 for the five undefined bytes, shown as U+FFFD).
const CP1252_HIGH: [u16; 32] = [
    0x20ac, 0, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017d, 0, 0,
    0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0, 0x017e, 0x0178,
];

/// Windows-1252 bytes as text.
pub fn from_cp1252(bytes: &[u8]) -> String {
    bytes
        .iter()
        .map(|&b| match b {
            0x80..=0x9f => char::from_u32(u32::from(CP1252_HIGH[usize::from(b - 0x80)])).filter(|&c| c != '\0'),
            _ => Some(char::from(b)),
        })
        .map(|c| c.unwrap_or('\u{fffd}'))
        .collect()
}

/// Whether a page is HTML: its first non-blank character is `<`.
pub fn is_html(text: &str) -> bool {
    text.trim_start().starts_with('<')
}

fn entity(name: &str) -> Option<char> {
    if let Some(num) = name.strip_prefix('#') {
        let v = match num.strip_prefix(['x', 'X']) {
            Some(hex) => u32::from_str_radix(hex, 16).ok()?,
            None => num.parse::<u32>().ok()?,
        };
        // numeric references in 128..160 are Windows-1252 in practice (as browsers read them)
        if (0x80..0xa0).contains(&v) {
            return char::from_u32(u32::from(CP1252_HIGH[(v - 0x80) as usize])).filter(|&c| c != '\0');
        }
        return char::from_u32(v).filter(|&c| c != '\0');
    }
    Some(match name {
        "amp" => '&',
        "lt" => '<',
        "gt" => '>',
        "quot" => '"',
        "apos" => '\'',
        "nbsp" => '\u{a0}',
        "copy" => '\u{a9}',
        "reg" => '\u{ae}',
        "trade" => '\u{2122}',
        "mdash" => '\u{2014}',
        "ndash" => '\u{2013}',
        "hellip" => '\u{2026}',
        "laquo" => '\u{ab}',
        "raquo" => '\u{bb}',
        "lsquo" => '\u{2018}',
        "rsquo" => '\u{2019}',
        "ldquo" => '\u{201c}',
        "rdquo" => '\u{201d}',
        "bull" => '\u{2022}',
        "middot" => '\u{b7}',
        "deg" => '\u{b0}',
        "eacute" => '\u{e9}',
        "egrave" => '\u{e8}',
        "agrave" => '\u{e0}',
        "auml" => '\u{e4}',
        "ouml" => '\u{f6}',
        "uuml" => '\u{fc}',
        "szlig" => '\u{df}',
        "ccedil" => '\u{e7}',
        _ => return None,
    })
}

/// Decodes the entity at the start of `s` (which starts with `&`): the character and the bytes it used.
fn decode_entity(s: &str) -> Option<(char, usize)> {
    let end = s[1..].find(|c: char| !(c.is_ascii_alphanumeric() || c == '#'))? + 1;
    if !(2..=12).contains(&end) {
        return None;
    }
    let c = entity(&s[1..end])?;
    // the semicolon is optional, as browsers allow
    let used = if s[end..].starts_with(';') { end + 1 } else { end };
    Some((c, used))
}

/// Builds the text: collapses white space and keeps at most one blank line.
struct Out {
    text: String,
    /// a space is due before the next word
    space: bool,
    /// line breaks due before the next word (2 = a blank line)
    breaks: usize,
}

impl Out {
    fn word(&mut self, c: char) {
        if self.breaks > 0 {
            if !self.text.is_empty() {
                for _ in 0..self.breaks {
                    self.text.push('\n');
                }
            }
            self.breaks = 0;
            self.space = false;
        } else if self.space && !self.text.is_empty() {
            self.text.push(' ');
        }
        self.space = false;
        self.text.push(c);
    }

    /// One line break (they add up to a blank line), or a paragraph break (`n` 2: a blank line).
    fn line_break(&mut self, n: usize) {
        self.breaks = if n == 1 { (self.breaks + 1).min(2) } else { 2 };
    }
}

/// The text of an HTML page.
pub fn html_to_text(html: &str) -> String {
    let mut out = Out { text: String::new(), space: false, breaks: 0 };
    let mut skip_until: Option<&'static str> = None;
    let mut i = 0;
    let bytes = html.as_bytes();
    while i < html.len() {
        let rest = &html[i..];
        if rest.starts_with("<!--") {
            i += rest.find("-->").map_or(rest.len(), |e| e + 3);
            continue;
        }
        if bytes[i] == b'<' {
            let Some(end) = rest.find('>') else {
                break;
            };
            let tag = rest[1..end].trim();
            i += end + 1;
            let closing = tag.starts_with('/');
            let name: String = tag
                .trim_start_matches('/')
                .chars()
                .take_while(|c| c.is_ascii_alphanumeric())
                .collect::<String>()
                .to_ascii_lowercase();
            if let Some(until) = skip_until {
                if closing && name == until {
                    skip_until = None;
                }
                continue;
            }
            match name.as_str() {
                "script" | "style" | "head" | "title" if !closing => {
                    skip_until = Some(match name.as_str() {
                        "script" => "script",
                        "style" => "style",
                        "head" => "head",
                        _ => "title",
                    })
                }
                "br" => out.line_break(1),
                "p" | "div" | "h1" | "h2" | "h3" | "h4" | "h5" | "h6" | "table" | "ul" | "ol" | "blockquote" | "hr"
                | "pre" | "center" | "dl" => out.line_break(2),
                "tr" | "dt" | "dd" if !closing => out.line_break(1),
                "li" if !closing => {
                    out.line_break(1);
                    out.word('-');
                    out.space = true;
                }
                "td" | "th" if !closing => out.space = true,
                _ => {}
            }
            continue;
        }
        if skip_until.is_some() {
            i += rest.chars().next().map_or(1, char::len_utf8);
            continue;
        }
        let (c, used) = if bytes[i] == b'&' {
            decode_entity(rest).unwrap_or(('&', 1))
        } else {
            let c = rest.chars().next().unwrap_or(' ');
            (c, c.len_utf8())
        };
        i += used;
        if c.is_whitespace() && c != '\u{a0}' {
            out.space = true;
        } else {
            out.word(if c == '\u{a0}' { ' ' } else { c });
        }
    }
    out.text
}

/// A page's text: HTML pages converted, others as they are (line ends made `\n`). `skip_first_if_text` drops the
/// first character of a non-HTML page (the MOTD files' "is new" flag, the SDK's `SkipFirstCharIfNotHTML`). A page
/// is UTF-8 (decision D6; tools/open_data.py converts the retail pages); one that is not is an old Windows-1252
/// page and is read as such.
pub fn page_text(bytes: &[u8], skip_first_if_text: bool) -> String {
    let text = match std::str::from_utf8(bytes) {
        Ok(s) => s.to_owned(),
        Err(_) => from_cp1252(bytes),
    };
    if is_html(&text) {
        return html_to_text(&text);
    }
    let mut chars = text.chars();
    if skip_first_if_text {
        chars.next();
    }
    chars.as_str().replace("\r\n", "\n").replace('\r', "\n")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tags_go_and_blocks_break_lines() {
        let html = "<html><head><title>T</title><style>p{}</style></head><body><h1>Help</h1><p>First   para\n\
                    graph</p><p>Second<br>line two</p><ul><li>one</li><li>two</li></ul>end</body></html>";
        assert_eq!(html_to_text(html), "Help\n\nFirst para graph\n\nSecond\nline two\n\n- one\n- two\n\nend");
    }

    #[test]
    fn entities_decode() {
        assert_eq!(html_to_text("<p>a &amp; b &lt;c&gt; &quot;d&quot; &copy;&nbsp;2001 &#233;&#x41;&#150;</p>"), {
            "a & b <c> \"d\" \u{a9} 2001 \u{e9}A\u{2013}"
        });
        // unknown or unterminated: kept as written
        assert_eq!(html_to_text("<b>AT&T &bogus; &</b>"), "AT&T &bogus; &");
        assert_eq!(html_to_text("a &amp b&amp;c"), "a & b&c");
    }

    #[test]
    fn comments_scripts_and_links() {
        assert_eq!(html_to_text("<!-- hidden <b>x</b> -->Shown<script>var a='<p>';</script>"), "Shown");
        assert_eq!(html_to_text("Visit <a href=\"http://x\">our site</a>."), "Visit our site.");
        assert_eq!(html_to_text("<table><tr><td>a</td><td>b</td></tr><tr><td>c</td></tr></table>"), "a b\nc");
        assert_eq!(html_to_text("broken <b"), "broken");
    }

    #[test]
    fn plain_pages() {
        assert!(is_html("  \n<body>"));
        assert!(!is_html("Hello <b>"));
        assert_eq!(page_text(b"1Welcome\r\nback", true), "Welcome\nback");
        assert_eq!(page_text(b"Welcome", false), "Welcome");
        assert_eq!(page_text(b"<p>x</p>", true), "x");
        // an old Windows-1252 page (not UTF-8) is read as such; a UTF-8 page as it is
        assert_eq!(page_text(b"\x80 \x93q\x94 \x81", false), "\u{20ac} \u{201c}q\u{201d} \u{fffd}");
        assert_eq!(
            page_text("\u{20ac} \u{201c}q\u{201d} \u{4e2d}".as_bytes(), false),
            "\u{20ac} \u{201c}q\u{201d} \u{4e2d}"
        );
        assert_eq!(page_text(b"", true), "");
    }
}
