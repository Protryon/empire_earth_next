//! The RIFF reader of the DirectMusic files (dmusic's `Riff.c`): chunks with a FOURCC, a length and, for `RIFF` and
//! `LIST`, a type; reads past a chunk's end give zeros and leave the position where it was, as dmusic's did. Unlike
//! dmusic's (which trusted a chunk's length), every read is bounded by the file.

/// A FOURCC as dmusic compares it (the four bytes little-endian).
pub const fn fourcc(s: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*s)
}

pub const RIFF: u32 = fourcc(b"RIFF");
pub const LIST: u32 = fourcc(b"LIST");

/// One chunk being read: its bytes (`mem[..len]`), the read position, its id and type (0 when not a list).
#[derive(Clone, Copy)]
pub struct Riff<'a> {
    mem: &'a [u8],
    len: u32,
    pub pos: u32,
    pub id: u32,
    pub typ: u32,
}

impl<'a> Riff<'a> {
    /// The file's outer chunk (`DmRiff_init`): None when the buffer is shorter than its header or its length.
    pub fn file(buf: &'a [u8]) -> Option<Riff<'a>> {
        if buf.len() < 12 {
            return None;
        }
        let word = |i: usize| u32::from_le_bytes([buf[i], buf[i + 1], buf[i + 2], buf[i + 3]]);
        let (id, len, typ) = (word(0), word(4), word(8));
        if ((buf.len() - 8) as u64) < u64::from(len) {
            return None;
        }
        Some(Riff { mem: &buf[12..], len: len.wrapping_sub(4), pos: 0, id, typ })
    }

    /// A chunk over bytes of its own (tests).
    #[cfg(test)]
    pub fn over(mem: &'a [u8]) -> Riff<'a> {
        Riff { mem, len: mem.len() as u32, pos: 0, id: 0, typ: 0 }
    }

    pub fn len(&self) -> u32 {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    pub fn is(&self, id: u32, typ: u32) -> bool {
        self.id == id && self.typ == typ
    }

    /// The next sub-chunk (`DmRiff_readChunk`), the position moved past it (and its pad byte).
    pub fn chunk(&mut self) -> Option<Riff<'a>> {
        if self.pos > self.len {
            return None;
        }
        if self.len - self.pos < 8 {
            return None;
        }
        let id = self.dword();
        let mut len = self.dword();
        let mut typ = 0;
        if id == RIFF || id == LIST {
            typ = self.dword();
            len = len.wrapping_sub(4);
        }
        let start = (self.pos as usize).min(self.mem.len());
        let end = start.saturating_add(len as usize).min(self.mem.len());
        let child = Riff { mem: &self.mem[start..end], len, pos: 0, id, typ };
        self.pos = self.pos.wrapping_add(len.wrapping_add(len % 2));
        Some(child)
    }

    /// The number of sub-chunks (`DmRiff_chunks`), the position kept.
    pub fn count(&mut self) -> u32 {
        let pos = self.pos;
        self.pos = 0;
        let mut n = 0;
        while self.chunk().is_some() {
            n += 1;
        }
        self.pos = pos;
        n
    }

    /// `n` bytes, or zeros (and the position unmoved) past the chunk's end.
    pub fn read<const N: usize>(&mut self) -> [u8; N] {
        let mut out = [0u8; N];
        let pos = self.pos as usize;
        if pos + N > self.len as usize {
            return out;
        }
        for (i, b) in out.iter_mut().enumerate() {
            *b = self.mem.get(pos + i).copied().unwrap_or(0);
        }
        self.pos += N as u32;
        out
    }

    pub fn byte(&mut self) -> u8 {
        self.read::<1>()[0]
    }
    pub fn word(&mut self) -> u16 {
        u16::from_le_bytes(self.read())
    }
    pub fn short(&mut self) -> i16 {
        i16::from_le_bytes(self.read())
    }
    pub fn dword(&mut self) -> u32 {
        u32::from_le_bytes(self.read())
    }
    pub fn double(&mut self) -> f64 {
        f64::from_le_bytes(self.read())
    }
    pub fn guid(&mut self) -> [u8; 16] {
        self.read()
    }

    /// A NUL-terminated UTF-16 string from the chunk's start (`DmRiff_readStringUtf`, as dmusic uses it).
    pub fn utf16(&mut self) -> String {
        let mut units = Vec::new();
        let mut i = 0usize;
        while i + 1 < self.mem.len() && i + 1 < self.len as usize {
            let u = u16::from_le_bytes([self.mem[i], self.mem[i + 1]]);
            if u == 0 {
                break;
            }
            units.push(u);
            i += 2;
        }
        String::from_utf16_lossy(&units)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn chunk(id: &[u8; 4], typ: Option<&[u8; 4]>, body: &[u8]) -> Vec<u8> {
        let mut v = id.to_vec();
        let len = body.len() + if typ.is_some() { 4 } else { 0 };
        v.extend_from_slice(&(len as u32).to_le_bytes());
        if let Some(t) = typ {
            v.extend_from_slice(t);
        }
        v.extend_from_slice(body);
        if body.len() % 2 == 1 {
            v.push(0);
        }
        v
    }

    #[test]
    fn chunks_lists_pads_and_reads_past_the_end() {
        let mut body = chunk(b"abcd", None, &[1, 2, 3]);
        body.extend(chunk(b"LIST", Some(b"UNFO"), &chunk(b"UNAM", None, &[b'H', 0, b'i', 0, 0, 0])));
        let file = chunk(b"RIFF", Some(b"DMSG"), &body);
        let mut r = Riff::file(&file).unwrap();
        assert!(r.is(RIFF, fourcc(b"DMSG")));
        assert_eq!(r.count(), 2);
        let mut a = r.chunk().unwrap();
        assert_eq!((a.id, a.len()), (fourcc(b"abcd"), 3));
        assert_eq!(a.word(), 0x0201);
        assert_eq!(a.word(), 0, "past the end: zeros");
        assert_eq!(a.byte(), 3, "the position did not move");
        let mut l = r.chunk().unwrap();
        assert!(l.is(LIST, fourcc(b"UNFO")));
        let mut n = l.chunk().unwrap();
        assert_eq!(n.utf16(), "Hi");
        assert!(r.chunk().is_none());
        assert!(Riff::file(&file[..file.len() - 1]).is_none());
        assert_eq!(Riff::over(&[9, 0, 0, 0]).dword(), 9);
    }
}
