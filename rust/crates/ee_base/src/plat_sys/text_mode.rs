//! PlatFOpen's text mode on POSIX: the Windows CRT reads a file opened without 'b' with every CR LF turned into LF
//! (the game's text files, the random map scripts and the debug files, have CR LF lines; fgets/fscanf callers expect
//! LF). Pure parts here (tested on every system); the `FILE*` wrapping is in `file_posix.rs`.

/// The mode the C library gets, and whether this is a read-only text open: the CRT's mode letters 'b' (binary) and
/// 't' (text, the default); the C library takes neither 't' nor a text mode.
pub fn translate_mode(mode: &[u8]) -> (Vec<u8>, bool) {
    let mut out: Vec<u8> = mode.iter().copied().filter(|&c| c != b't').collect();
    let binary = mode.contains(&b'b');
    let text_read = !binary && out.contains(&b'r') && !out.contains(&b'+');
    if text_read {
        out.push(b'b');
    }
    (out, text_read)
}

/// The CR LF filter's state: a byte read past a lone CR.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct CrLfFilter {
    pub pending: Option<u8>,
}

impl CrLfFilter {
    /// Fills `out` from `next` (a byte source, `None` at the end) with CR LF collapsed to LF -> how many bytes.
    pub fn read(&mut self, out: &mut [u8], mut next: impl FnMut() -> Option<u8>) -> usize {
        let mut n = 0;
        while n < out.len() {
            let Some(mut c) = self.pending.take().or_else(&mut next) else {
                break;
            };
            if c == b'\r' {
                match next() {
                    Some(b'\n') => c = b'\n',
                    other => self.pending = other,
                }
            }
            out[n] = c;
            n += 1;
        }
        n
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn modes() {
        assert_eq!(translate_mode(b"r"), (b"rb".to_vec(), true));
        assert_eq!(translate_mode(b"rt"), (b"rb".to_vec(), true));
        assert_eq!(translate_mode(b"rb"), (b"rb".to_vec(), false));
        assert_eq!(translate_mode(b"r+"), (b"r+".to_vec(), false));
        assert_eq!(translate_mode(b"rt+"), (b"r+".to_vec(), false));
        assert_eq!(translate_mode(b"w"), (b"w".to_vec(), false));
        assert_eq!(translate_mode(b"wt"), (b"w".to_vec(), false));
        assert_eq!(translate_mode(b"a+b"), (b"a+b".to_vec(), false));
    }

    fn filtered(input: &[u8], chunk: usize) -> Vec<u8> {
        let mut iter = input.iter().copied();
        let mut f = CrLfFilter::default();
        let mut out = Vec::new();
        loop {
            let mut buf = vec![0u8; chunk];
            let n = f.read(&mut buf, || iter.next());
            if n == 0 {
                return out;
            }
            out.extend_from_slice(&buf[..n]);
        }
    }

    // Golden output of the C++ TextReader for the same inputs (src/Platform/Posix/PlatFile.cpp).
    #[test]
    fn crlf_golden() {
        assert_eq!(filtered(b"a\r\nb\r\n", 64), b"a\nb\n");
        assert_eq!(filtered(b"no newline", 64), b"no newline");
        assert_eq!(filtered(b"a\nb", 64), b"a\nb");
        // a lone CR stays; the byte after it is kept
        assert_eq!(filtered(b"a\rb\r\nc", 64), b"a\rb\nc");
        // CR at the end of the file
        assert_eq!(filtered(b"a\r", 64), b"a\r");
        // CR CR LF: the first CR is lone
        assert_eq!(filtered(b"\r\r\n", 64), b"\r\n");
        assert_eq!(filtered(b"", 64), b"");
    }

    #[test]
    fn crlf_across_small_reads() {
        let input = b"line1\r\nline2\rmore\r\n\r\nend\r";
        let whole = filtered(input, 1024);
        for chunk in 1..8 {
            assert_eq!(filtered(input, chunk), whole, "chunk {chunk}");
        }
        assert_eq!(whole, b"line1\nline2\rmore\n\nend\r");
    }
}
