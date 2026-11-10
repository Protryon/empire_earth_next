//! Encoding and decoding by a class's layout (`schema::MsgClass`): plain values in, bytes out, and back.
//!
//! The decoder reports where each member lies (a `Span`) rather than copying it, so the caller decides where the bytes
//! go: the C ABI (`ffi`) copies fixed members into the C++ object and hands strings to `UWideString::Import`, which
//! keeps its own semantics. Decoding goes through a `Source`: a bounds-checked slice here and in the tests, the
//! received buffer as the C++ trusted it in `ffi` (the original's imports have no length).

use crate::schema::{Kind, MsgClass, HEADER_SIZE};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    /// the bytes end inside this member
    Short(&'static str),
    /// a value does not fit its member's kind (the wrong byte count for a fixed member, or a record count that
    /// disagrees with the bytes)
    Value(&'static str),
    /// more or fewer values than the class has members
    Arity,
    /// the output buffer is smaller than the message
    Output,
}

/// A member's value for `encode`.
#[derive(Clone, Copy, Debug)]
pub enum Value<'a> {
    /// a fixed member's bytes (exactly its kind's size)
    Fixed(&'a [u8]),
    /// a string's bytes, without a terminator
    Str(&'a [u8]),
    /// `count` records, `bytes` holding all of them
    Records { count: usize, bytes: &'a [u8] },
    /// a blob's bytes
    Blob(&'a [u8]),
    /// the players' inputs, in list order (only the first `(u8)len()` travel)
    Inputs(&'a [Input<'a>]),
}

/// One nested message of a players' inputs list: its wire size (its own `ExportSize`) and, to encode, its bytes.
#[derive(Clone, Copy, Debug)]
pub struct Input<'a> {
    pub size: usize,
    pub bytes: Option<&'a [u8]>,
}

/// How many of a list's inputs travel: the C++ `(unsigned char)mPlayerInputs.size()`, for the count byte and the loop.
pub fn inputs_travelling(len: usize) -> usize {
    len as u8 as usize
}

/// `NEMPlayerInput::ExportSize`: the count byte and each travelling input's size.
pub fn inputs_size(inputs: &[Input]) -> usize {
    inputs[..inputs_travelling(inputs.len())].iter().fold(1usize, |n, i| n.wrapping_add(i.size))
}

/// `NEMPlayerInput::ExportMessage`: the count byte, then each travelling input's bytes; returns the byte count.
pub fn encode_inputs(inputs: &[Input], out: &mut [u8]) -> Result<usize, Error> {
    let size = inputs_size(inputs);
    if out.len() < size {
        return Err(Error::Output);
    }
    let travelling = &inputs[..inputs_travelling(inputs.len())];
    out[0] = travelling.len() as u8;
    let mut at = 1;
    for i in travelling {
        match i.bytes {
            Some(b) if b.len() == i.size => {
                out[at..at + b.len()].copy_from_slice(b);
                at += b.len();
            }
            _ => return Err(Error::Value("mPlayerInputs")),
        }
    }
    Ok(at)
}

/// `NEMPlayerInput::ImportMessage`'s walk: `count` times, `build` makes the message at that offset from the list's
/// start and says its size (its `ExportSize`), or `None` for a type nobody builds, which the original skips without
/// advancing (so every later input is tried at the same place). Returns the offset after the last input built.
pub fn walk_inputs(count: u8, mut build: impl FnMut(usize) -> Option<usize>) -> usize {
    let mut at = 1usize;
    for _ in 0..count {
        if let Some(n) = build(at) {
            at = at.wrapping_add(n);
        }
    }
    at
}

/// Where a decoded member lies in the message.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct Span {
    /// fixed and blob: the first byte; string: its u32; records: the first record
    pub offset: usize,
    /// fixed and blob: the byte count; string: the u32's value (bytes + 2); records: count x size
    pub len: usize,
    /// records: how many; blob: its bytes; otherwise 0
    pub count: usize,
}

fn record_size(kind: Kind) -> usize {
    match kind {
        Kind::Records8(n) | Kind::Records32(n) => n as usize,
        _ => 0,
    }
}

/// The message's size with these values (`ExportSize`).
pub fn encoded_size(class: &MsgClass, values: &[Value]) -> Result<usize, Error> {
    if values.len() != class.fields.len() {
        return Err(Error::Arity);
    }
    let mut size = HEADER_SIZE;
    for (f, v) in class.fields.iter().zip(values) {
        size += match (f.kind, v) {
            (k, Value::Fixed(b)) if k.fixed_size() == Some(b.len()) => b.len(),
            (Kind::WStr, Value::Str(s)) => 4 + s.len() + 2,
            (Kind::Records8(_), Value::Records { count, bytes }) => 1 + check_records(f.kind, *count, bytes, f.name)?,
            (Kind::Records32(_), Value::Records { count, bytes }) => 4 + check_records(f.kind, *count, bytes, f.name)?,
            (Kind::Blob, Value::Blob(b)) => b.len(),
            (Kind::Inputs, Value::Inputs(i)) => inputs_size(i),
            _ => return Err(Error::Value(f.name)),
        };
    }
    Ok(size)
}

fn check_records(kind: Kind, count: usize, bytes: &[u8], name: &'static str) -> Result<usize, Error> {
    let size = record_size(kind);
    if count.checked_mul(size) != Some(bytes.len()) {
        return Err(Error::Value(name));
    }
    Ok(bytes.len())
}

/// Writes the header and the members into `out` (`ExportMessage`); returns the byte count.
pub fn encode(class: &MsgClass, header: [u8; HEADER_SIZE], values: &[Value], out: &mut [u8]) -> Result<usize, Error> {
    let size = encoded_size(class, values)?;
    if out.len() < size {
        return Err(Error::Output);
    }
    out[..HEADER_SIZE].copy_from_slice(&header);
    let mut at = HEADER_SIZE;
    let mut put = |bytes: &[u8]| {
        out[at..at + bytes.len()].copy_from_slice(bytes);
        at += bytes.len();
    };
    for (f, v) in class.fields.iter().zip(values) {
        match (f.kind, *v) {
            (Kind::WStr, Value::Str(s)) => {
                // the C++ stores strlen + 2 in a uint32_t
                put(&(s.len() as u32).wrapping_add(2).to_le_bytes());
                put(s);
                put(&[0, 0]);
            }
            (Kind::Records8(_), Value::Records { count, bytes }) => {
                put(&[count as u8]); // `(unsigned char)size()`: wraps, every record still follows
                put(bytes);
            }
            (Kind::Records32(_), Value::Records { count, bytes }) => {
                put(&(count as u32).to_le_bytes());
                put(bytes);
            }
            (_, Value::Fixed(b)) | (_, Value::Blob(b)) => put(b),
            (Kind::Inputs, Value::Inputs(i)) => {
                let mut part = vec![0u8; inputs_size(i)];
                let n = encode_inputs(i, &mut part)?;
                put(&part[..n]);
            }
            _ => unreachable!("encoded_size checked the kinds"),
        }
    }
    Ok(size)
}

/// Where the decoder reads its bytes from.
pub trait Source {
    /// `len` bytes at `offset`
    fn bytes(&self, offset: usize, len: usize, member: &'static str) -> Result<&[u8], Error>;
}

impl Source for [u8] {
    fn bytes(&self, offset: usize, len: usize, member: &'static str) -> Result<&[u8], Error> {
        let end = offset.checked_add(len).ok_or(Error::Short(member))?;
        self.get(offset..end).ok_or(Error::Short(member))
    }
}

/// Reads the header and the spans of the members the import reads (`import_fields`), in order (`ImportMessage`).
/// A string's u32 says where the next member is (the original's `theLocation += Import(...)`); records and blobs
/// are read where their counts say.
pub fn decode<S: Source + ?Sized>(class: &MsgClass, src: &S, out: &mut Vec<Span>) -> Result<[u8; HEADER_SIZE], Error> {
    out.clear();
    let mut header = [0u8; HEADER_SIZE];
    header.copy_from_slice(src.bytes(0, HEADER_SIZE, "header")?);
    let mut at = HEADER_SIZE;
    let mut previous_u8 = 0usize;
    for f in &class.fields[..class.import_fields] {
        let span = match f.kind {
            Kind::WStr => {
                let n = u32::from_le_bytes(src.bytes(at, 4, f.name)?.try_into().unwrap()) as usize;
                Span { offset: at, len: n, count: 0 }
            }
            Kind::Records8(_) => {
                let count = src.bytes(at, 1, f.name)?[0] as usize;
                Span { offset: at + 1, len: count * record_size(f.kind), count }
            }
            Kind::Records32(_) => {
                let count = u32::from_le_bytes(src.bytes(at, 4, f.name)?.try_into().unwrap()) as usize;
                let len = count.checked_mul(record_size(f.kind)).ok_or(Error::Short(f.name))?;
                Span { offset: at + 4, len, count }
            }
            Kind::Blob => Span { offset: at, len: previous_u8, count: previous_u8 },
            // the count byte; the inputs' own bytes are read by their own imports (`walk_inputs`)
            Kind::Inputs => Span { offset: at, len: 1, count: src.bytes(at, 1, f.name)?[0] as usize },
            k => {
                let n = k.fixed_size().unwrap();
                let b = src.bytes(at, n, f.name)?;
                previous_u8 = b[0] as usize;
                Span { offset: at, len: n, count: 0 }
            }
        };
        // the next member: after the string's u32 and its counted bytes, after the records, after the bytes
        at = match f.kind {
            Kind::WStr => span.offset.checked_add(4 + span.len).ok_or(Error::Short(f.name))?,
            _ => span.offset.checked_add(span.len).ok_or(Error::Short(f.name))?,
        };
        if !matches!(f.kind, Kind::WStr) {
            // the variable members' bytes must be there too (a string's are read by its own Import)
            src.bytes(span.offset, span.len, f.name)?;
        }
        out.push(span);
    }
    Ok(header)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::schema::{Field, Kind::*};

    static FIXED: MsgClass = MsgClass {
        name: "Fixed",
        fields: &[Field::new("sel", U8), Field::new("id", I32), Field::new("q", Bool), Field::new("p", Point2)],
        import_fields: 4,
    };
    static MIXED: MsgClass = MsgClass {
        name: "Mixed",
        fields: &[Field::new("sel", U8), Field::new("name", WStr), Field::new("ping", F32)],
        import_fields: 3,
    };
    static LISTS: MsgClass = MsgClass {
        name: "Lists",
        fields: &[Field::new("sel", U8), Field::new("ups", Records8(4)), Field::new("ids", Records32(4))],
        import_fields: 3,
    };
    static BLOB: MsgClass = MsgClass {
        name: "Blob",
        fields: &[Field::new("sel", U8), Field::new("n", U8), Field::new("buf", Blob)],
        import_fields: 3,
    };
    static SHORT_IMPORT: MsgClass = MsgClass {
        name: "ShortImport",
        fields: &[Field::new("sel", U8), Field::new("t", I32), Field::new("b", Bool)],
        import_fields: 2,
    };

    #[test]
    fn fixed_members_are_their_bytes_after_the_header() {
        let id = (-2i32).to_le_bytes();
        let p = [1.5f32.to_le_bytes(), f32::NAN.to_le_bytes()].concat();
        let values = [Value::Fixed(&[7]), Value::Fixed(&id), Value::Fixed(&[1]), Value::Fixed(&p)];
        let mut out = [0u8; 32];
        let n = encode(&FIXED, [0, 0x4c, 0xff, 2], &values, &mut out).unwrap();
        assert_eq!(n, 18);
        assert_eq!(&out[..n], &[0, 0x4c, 0xff, 2, 7, 0xfe, 0xff, 0xff, 0xff, 1, 0, 0, 0xc0, 0x3f, 0, 0, 0xc0, 0x7f]);
        let mut spans = Vec::new();
        let header = decode(&FIXED, &out[..n], &mut spans).unwrap();
        assert_eq!(header, [0, 0x4c, 0xff, 2]);
        let offsets: Vec<_> = spans.iter().map(|s| (s.offset, s.len)).collect();
        assert_eq!(offsets, [(4, 1), (5, 4), (9, 1), (10, 8)]);
    }

    #[test]
    fn a_string_is_its_length_plus_two_then_the_bytes_then_two_zeros() {
        let ping = 0.25f32.to_le_bytes();
        let values = [Value::Fixed(&[1]), Value::Str(b"ab\xc3\xa9"), Value::Fixed(&ping)];
        assert_eq!(encoded_size(&MIXED, &values), Ok(4 + 1 + 4 + 4 + 2 + 4));
        let mut out = [0u8; 64];
        let n = encode(&MIXED, [0, 0x93, 1, 2], &values, &mut out).unwrap();
        assert_eq!(&out[5..15], &[6, 0, 0, 0, b'a', b'b', 0xc3, 0xa9, 0, 0]);
        let mut spans = Vec::new();
        decode(&MIXED, &out[..n], &mut spans).unwrap();
        assert_eq!(spans[1], Span { offset: 5, len: 6, count: 0 });
        // the member after the string is found through the string's u32
        assert_eq!(spans[2], Span { offset: 15, len: 4, count: 0 });
        assert_eq!(&out[15..19], &ping);
    }

    #[test]
    fn an_empty_string_is_two_zero_bytes() {
        let values = [Value::Fixed(&[0]), Value::Str(b""), Value::Fixed(&[0; 4])];
        let mut out = [0u8; 32];
        let n = encode(&MIXED, [0; 4], &values, &mut out).unwrap();
        assert_eq!(n, 4 + 1 + 6 + 4);
        assert_eq!(&out[5..11], &[2, 0, 0, 0, 0, 0]);
    }

    #[test]
    fn record_lists_carry_their_count() {
        let ups = [1u8, 0, 2, 0, 3, 0, 4, 0];
        let ids = [0xffu8; 12];
        let values =
            [Value::Fixed(&[9]), Value::Records { count: 2, bytes: &ups }, Value::Records { count: 3, bytes: &ids }];
        let mut out = [0u8; 64];
        let n = encode(&LISTS, [0; 4], &values, &mut out).unwrap();
        assert_eq!(n, 4 + 1 + 1 + 8 + 4 + 12);
        assert_eq!(out[5], 2);
        assert_eq!(&out[14..18], &[3, 0, 0, 0]);
        let mut spans = Vec::new();
        decode(&LISTS, &out[..n], &mut spans).unwrap();
        assert_eq!(spans[1], Span { offset: 6, len: 8, count: 2 });
        assert_eq!(spans[2], Span { offset: 18, len: 12, count: 3 });
        // a count that disagrees with the bytes is refused
        let bad =
            [Value::Fixed(&[9]), Value::Records { count: 3, bytes: &ups }, Value::Records { count: 0, bytes: &[] }];
        assert_eq!(encoded_size(&LISTS, &bad), Err(Error::Value("ups")));
    }

    #[test]
    fn a_u8_count_wraps_but_every_record_follows() {
        let ups = vec![0xabu8; 4 * 257];
        let values =
            [Value::Fixed(&[9]), Value::Records { count: 257, bytes: &ups }, Value::Records { count: 0, bytes: &[] }];
        let mut out = vec![0u8; 2000];
        let n = encode(&LISTS, [0; 4], &values, &mut out).unwrap();
        assert_eq!(n, 4 + 1 + 1 + 4 * 257 + 4);
        assert_eq!(out[5], 1);
    }

    #[test]
    fn a_blob_is_as_long_as_the_member_before_it() {
        let values = [Value::Fixed(&[3]), Value::Fixed(&[5]), Value::Blob(&[1, 2, 3, 4, 5])];
        let mut out = [0u8; 32];
        let n = encode(&BLOB, [0; 4], &values, &mut out).unwrap();
        assert_eq!(n, 11);
        let mut spans = Vec::new();
        decode(&BLOB, &out[..n], &mut spans).unwrap();
        assert_eq!(spans[2], Span { offset: 6, len: 5, count: 5 });
        assert_eq!(decode(&BLOB, &out[..10], &mut spans), Err(Error::Short("buf")));
    }

    #[test]
    fn the_import_may_read_fewer_members_than_the_export_writes() {
        let t = 5i32.to_le_bytes();
        let values = [Value::Fixed(&[1]), Value::Fixed(&t), Value::Fixed(&[1])];
        let mut out = [0u8; 16];
        let n = encode(&SHORT_IMPORT, [0; 4], &values, &mut out).unwrap();
        assert_eq!(n, 10);
        let mut spans = Vec::new();
        decode(&SHORT_IMPORT, &out[..9], &mut spans).unwrap();
        assert_eq!(spans.len(), 2);
    }

    static TICK: MsgClass = MsgClass {
        name: "Tick",
        fields: &[Field::new("tick", U32), Field::new("raw", Raw(3)), Field::new("inputs", Inputs)],
        import_fields: 3,
    };

    #[test]
    fn inputs_are_a_count_then_each_message() {
        let a = [0u8, 0x4c, 1, 2, 9];
        let b = [0u8, 0x3a, 0xff, 0xfe];
        let inputs = [Input { size: 5, bytes: Some(&a) }, Input { size: 4, bytes: Some(&b) }];
        let tick = 7u32.to_le_bytes();
        let values = [Value::Fixed(&tick), Value::Fixed(&[1, 2, 3]), Value::Inputs(&inputs)];
        assert_eq!(encoded_size(&TICK, &values), Ok(4 + 4 + 3 + 1 + 9));
        let mut out = [0u8; 32];
        let n = encode(&TICK, [0, 0x26, 0xff, 1], &values, &mut out).unwrap();
        assert_eq!(&out[..n], &[0, 0x26, 0xff, 1, 7, 0, 0, 0, 1, 2, 3, 2, 0, 0x4c, 1, 2, 9, 0, 0x3a, 0xff, 0xfe]);
        let mut spans = Vec::new();
        decode(&TICK, &out[..n], &mut spans).unwrap();
        assert_eq!(spans[1], Span { offset: 8, len: 3, count: 0 });
        assert_eq!(spans[2], Span { offset: 11, len: 1, count: 2 });
        // the sizes alone are enough for the size, the bytes are needed to encode
        let sized = [Input { size: 5, bytes: None }];
        assert_eq!(inputs_size(&sized), 6);
        assert_eq!(encode_inputs(&sized, &mut out), Err(Error::Value("mPlayerInputs")));
        let wrong = [Input { size: 4, bytes: Some(&a) }];
        assert_eq!(encode_inputs(&wrong, &mut out), Err(Error::Value("mPlayerInputs")));
        assert_eq!(encode_inputs(&inputs, &mut out[..9]), Err(Error::Output));
    }

    #[test]
    fn only_the_u8_count_of_inputs_travels() {
        let b = [0u8, 0x3a, 0, 0];
        let many = vec![Input { size: 4, bytes: Some(&b) }; 257];
        assert_eq!(inputs_travelling(257), 1);
        assert_eq!(inputs_size(&many), 5);
        let mut out = [0u8; 16];
        assert_eq!(encode_inputs(&many, &mut out), Ok(5));
        assert_eq!(out[0], 1);
        assert_eq!(inputs_size(&many[..256]), 1);
        assert_eq!(inputs_size(&many[..255]), 1 + 4 * 255);
        assert_eq!(inputs_size(&[]), 1);
    }

    #[test]
    fn the_walk_skips_an_unbuilt_input_without_advancing() {
        // sizes by offset: 1 -> 5, 6 -> nothing builds there
        let mut seen = Vec::new();
        let end = walk_inputs(3, |at| {
            seen.push(at);
            if at == 1 {
                Some(5)
            } else {
                None
            }
        });
        assert_eq!(end, 6);
        assert_eq!(seen, [1, 6, 6]);
        assert_eq!(walk_inputs(0, |_| panic!("no inputs")), 1);
        assert_eq!(walk_inputs(255, |_| Some(4)), 1 + 255 * 4);
    }

    #[test]
    fn errors() {
        let mut out = [0u8; 4];
        let values = [Value::Fixed(&[1]), Value::Fixed(&[0; 4]), Value::Fixed(&[1]), Value::Fixed(&[0; 8])];
        assert_eq!(encode(&FIXED, [0; 4], &values, &mut out), Err(Error::Output));
        assert_eq!(encoded_size(&FIXED, &values[..3]), Err(Error::Arity));
        let wrong = [Value::Fixed(&[1]), Value::Fixed(&[0; 2]), Value::Fixed(&[1]), Value::Fixed(&[0; 8])];
        assert_eq!(encoded_size(&FIXED, &wrong), Err(Error::Value("id")));
        let mut spans = Vec::new();
        assert_eq!(decode(&FIXED, &[0u8; 3][..], &mut spans), Err(Error::Short("header")));
        assert_eq!(decode(&FIXED, &[0u8; 8][..], &mut spans), Err(Error::Short("id")));
        // a string whose length runs past the bytes: the next member is not there
        let s = [0u8, 0, 0, 0, 1, 0xff, 0xff, 0xff, 0x7f];
        assert_eq!(decode(&MIXED, &s[..], &mut spans), Err(Error::Short("ping")));
    }
}
