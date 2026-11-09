//! `FSMemoryFile`: a file held in memory. Semantics pinned by `golden_fs.txt` (a dump of the C++ class):
//! - `buffer` is the C++ `mBuffer`; its length is the capacity (`mAllocatedLength`), `None` is a null `mBuffer`;
//! - `end`/`current` are `mEnd`/`mCurrent` as offsets (`mStart` was always `mBuffer`);
//! - reading never checks `mIsOpen`, writing does: a closed file keeps its data readable.

use ee_codec::compression;

use crate::file::{FsFile, FsWrite};
use crate::status;

const COMPRESS_MINIMUM: usize = 12;
const OUTPUT_TOO_SMALL: i32 = compression::CODE_OUTPUT_TOO_SMALL;

pub struct EeFsMemFile {
    buffer: Option<Box<[u8]>>,
    end: usize,
    current: usize,
    block_size: usize,
    grow_by_doubling: bool,
    is_open: bool,
}

impl EeFsMemFile {
    /// `FSMemoryFile(inBlockSize, inGrowByDoubling, inBlockCount)`: open, with `block * count` bytes of capacity.
    pub fn new(block_size: usize, grow_by_doubling: bool, block_count: usize) -> Self {
        let block_size = block_size.max(1);
        let mut file = EeFsMemFile { buffer: None, end: 0, current: 0, block_size, grow_by_doubling, is_open: false };
        file.increase(block_size.wrapping_mul(block_count));
        file.is_open = true;
        file
    }

    /// `FSMemoryFile(inData, inLength, inBlockSize, inGrowByDoubling)`: open at the start of `data`.
    pub fn over(data: Box<[u8]>, block_size: usize, grow_by_doubling: bool) -> Self {
        let end = data.len();
        EeFsMemFile {
            buffer: Some(data),
            end,
            current: 0,
            block_size: block_size.max(1),
            grow_by_doubling,
            is_open: true,
        }
    }

    fn capacity(&self) -> usize {
        self.buffer.as_ref().map_or(0, |b| b.len())
    }

    /// `FSMemoryFile::IncreaseFileSize`: grows the capacity by `by`, keeping contents and position.
    fn increase(&mut self, by: usize) {
        let capacity = self.capacity() + by;
        let mut bigger = vec![0u8; capacity].into_boxed_slice();
        if let Some(old) = &self.buffer {
            bigger[..old.len()].copy_from_slice(old);
        }
        self.buffer = Some(bigger);
    }

    /// `GetData()`: `None` for a null buffer, which `Delete` leaves and a zero-length allocation is not.
    pub fn data(&self) -> Option<*const u8> {
        self.buffer.as_ref().map(|b| b.as_ptr())
    }

    pub fn contents(&self) -> &[u8] {
        match &self.buffer {
            Some(b) => &b[..self.end],
            None => &[],
        }
    }

    pub fn length(&self) -> usize {
        self.end
    }

    /// `GetPosition`: `-1` when closed or without a buffer.
    pub fn position(&self) -> i64 {
        if self.is_open && self.buffer.is_some() {
            self.current as i64
        } else {
            -1
        }
    }

    pub fn is_open(&self) -> bool {
        self.is_open
    }

    pub fn set_open(&mut self, open: bool) {
        self.is_open = open;
    }

    /// `Close`: `NOT_OPEN` if it was closed; the data stays.
    pub fn close(&mut self) -> i32 {
        if self.is_open {
            self.is_open = false;
            status::OK
        } else {
            status::NOT_OPEN
        }
    }

    /// `Compact`: shrinks the buffer to the written length and rewinds.
    pub fn compact(&mut self) -> i32 {
        if !self.is_open {
            return status::NOT_OPEN;
        }
        let Some(buffer) = &self.buffer else {
            return status::FAILED;
        };
        if buffer.len() > self.end {
            self.buffer = Some(buffer[..self.end].to_vec().into_boxed_slice());
        }
        self.current = 0;
        status::OK
    }

    /// `Compress`: only above 12 bytes; an incompressible result leaves the data and succeeds; closes the file on
    /// success. Returns the status and the new length.
    pub fn compress(&mut self, level: u32) -> (i32, u32) {
        let result = self.compact();
        if result != status::OK {
            return (result, 0);
        }
        let length = self.length();
        if length <= COMPRESS_MINIMUM {
            self.close();
            return (status::OK, length as u32);
        }
        let mut output = vec![0u8; length];
        let (code, compressed) = compression::compress(self.contents(), &mut output, level);
        if code == status::OK {
            output.truncate(compressed as usize);
            self.buffer = Some(output.into_boxed_slice());
            self.end = compressed as usize;
            self.current = 0;
            self.close();
            (status::OK, compressed)
        } else if code == OUTPUT_TOO_SMALL {
            self.close();
            (status::OK, length as u32)
        } else {
            (code, 0)
        }
    }

    /// `Decompress`: in place when compressed (closing the file); an uncompressed file stays as it is and open.
    /// Returns the status and the length.
    pub fn decompress(&mut self) -> (i32, u32) {
        let length = self.length();
        let contents = self.contents();
        if !compression::is_compressed(contents) {
            return (status::OK, length as u32);
        }
        let (result, size) = compression::decompressed_size(contents);
        if result != status::OK {
            return (result, 0);
        }
        let mut output = vec![0u8; size as usize];
        let (code, actual) = compression::decompress(contents, &mut output);
        if code != status::OK {
            return (code, 0);
        }
        if actual != size {
            return (status::FAILED, 0);
        }
        self.buffer = Some(output.into_boxed_slice());
        self.end = size as usize;
        self.current = 0;
        self.close();
        (status::OK, size)
    }

    /// `Delete`: frees the buffer, closes.
    pub fn delete(&mut self) -> i32 {
        self.buffer = None;
        self.current = 0;
        self.end = 0;
        self.is_open = false;
        status::OK
    }

    /// `Open(inAccess)`: rewinds, or allocates one block when there is no buffer.
    pub fn open(&mut self) -> i32 {
        if self.is_open {
            return status::OK;
        }
        if self.buffer.is_some() {
            self.is_open = true;
            self.set_position(0, ORIGIN_BEGIN);
        } else {
            self.current = 0;
            self.end = 0;
            self.increase(self.block_size);
            self.is_open = true;
        }
        status::OK
    }

    /// `Read`: all or nothing. `None` output: no buffer (`NOT_OPEN`, the C++ `outRead` untouched).
    pub fn read(&mut self, out: &mut [u8]) -> (i32, Option<u32>) {
        let Some(buffer) = &self.buffer else {
            return (status::NOT_OPEN, None);
        };
        if self.current + out.len() <= self.end {
            out.copy_from_slice(&buffer[self.current..self.current + out.len()]);
            self.current += out.len();
            (status::OK, Some(out.len() as u32))
        } else {
            (status::END_OF_FILE, Some(0))
        }
    }

    /// `Write`: `(NOT_OPEN, 0)` when closed or without a buffer; grows by a block or by doubling.
    pub fn write(&mut self, data: &[u8]) -> (i32, u32) {
        if !self.is_open || self.buffer.is_none() {
            return (status::NOT_OPEN, 0);
        }
        if self.current + data.len() > self.capacity() {
            let increase = if self.grow_by_doubling {
                if self.capacity() < data.len() {
                    data.len() * 2
                } else {
                    self.capacity()
                }
            } else if self.block_size < data.len() {
                self.block_size + data.len()
            } else {
                self.block_size
            };
            self.increase(increase);
        }
        let buffer = self.buffer.as_mut().expect("checked above");
        buffer[self.current..self.current + data.len()].copy_from_slice(data);
        self.current += data.len();
        self.end = self.end.max(self.current);
        (status::OK, data.len() as u32)
    }

    /// `SetPosition(inOffset, inOrigin, ...)`.
    pub fn set_position(&mut self, offset: i64, origin: u32) -> i32 {
        if offset < 0 {
            return status::BEYOND_END;
        }
        let offset = offset as usize;
        match origin {
            ORIGIN_CURRENT => {
                if self.current + offset <= self.end {
                    self.current += offset;
                    status::OK
                } else {
                    status::BEYOND_END
                }
            }
            ORIGIN_END => {
                if offset <= self.end {
                    self.current = self.end - offset;
                    status::OK
                } else {
                    status::BEYOND_END
                }
            }
            ORIGIN_BEGIN => {
                if offset <= self.end {
                    self.current = offset;
                    status::OK
                } else {
                    status::BEYOND_END
                }
            }
            _ => status::FAILED,
        }
    }

    /// `Load(inFile, inLength)`: replaces the contents with `length` bytes of `file`; leaves the file empty and closed
    /// on failure.
    pub fn load(&mut self, file: &mut dyn FsFile, length: usize) -> i32 {
        if self.buffer.is_some() {
            self.delete();
        }
        let mut data = vec![0u8; length].into_boxed_slice();
        let (result, _) = file.read_bytes(&mut data);
        if result != status::OK {
            self.delete();
            return result;
        }
        self.buffer = Some(data);
        self.current = 0;
        self.end = length;
        self.is_open = true;
        result
    }

    /// `Load(inFile)`: a dword length, then those bytes.
    pub fn load_prefixed(&mut self, file: &mut dyn FsFile) -> i32 {
        let mut length = 0u32;
        let result = file.read_u32(&mut length);
        if result != status::OK {
            return result;
        }
        self.load(file, length as usize)
    }

    /// `Save(outFile, inLength)`: the first `length` bytes (`min` the data: C++ read past the buffer).
    pub fn save(&self, file: &mut dyn FsWrite, length: usize) -> i32 {
        let (result, _) = match &self.buffer {
            Some(b) => file.write_bytes(&b[..length.min(b.len())]),
            None => file.write_bytes(&[]),
        };
        result
    }

    /// `Save(outFile)`: compacts, then a dword length (its write result ignored, as C++) and the data.
    pub fn save_prefixed(&mut self, file: &mut dyn FsWrite) -> i32 {
        self.compact();
        let length = self.length();
        file.write_u32(length as u32);
        self.save(file, length)
    }
}

const ORIGIN_BEGIN: u32 = 0;
const ORIGIN_END: u32 = 1;
const ORIGIN_CURRENT: u32 = 2;

// Lets a memory file be the source or target of another (the C++ scenarios do exactly that through `FSFile&`).
impl FsFile for EeFsMemFile {
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        let mut bytes = [0u8; 4];
        let (result, _) = self.read(&mut bytes);
        if result == status::OK {
            *value = u32::from_le_bytes(bytes);
        }
        result
    }
    fn read_bytes(&mut self, out: &mut [u8]) -> (i32, u32) {
        let (result, count) = self.read(out);
        (result, count.unwrap_or(0))
    }
}

impl FsWrite for EeFsMemFile {
    fn write_u32(&mut self, value: u32) -> i32 {
        self.write(&value.to_le_bytes()).0
    }
    fn write_bytes(&mut self, data: &[u8]) -> (i32, u32) {
        self.write(data)
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::file::tests::ScriptFile;

    #[test]
    fn a_zero_block_size_is_one() {
        let file = EeFsMemFile::new(0, false, 1);
        assert_eq!(file.capacity(), 1);
    }

    #[test]
    fn delete_nulls_the_data() {
        let mut file = EeFsMemFile::new(8, true, 1);
        file.write(b"ab");
        assert!(file.data().is_some());
        file.delete();
        assert_eq!(file.data(), None);
        assert_eq!(file.position(), -1);
        assert_eq!(file.read(&mut [0; 1]).0, status::NOT_OPEN);
    }

    #[test]
    fn write_grows_by_doubling_and_by_blocks() {
        let mut file = EeFsMemFile::new(8, true, 1);
        file.write(&[1; 4]);
        file.write(&[2; 8]);
        assert_eq!(file.capacity(), 16);
        let mut file = EeFsMemFile::new(4, false, 1);
        file.write(&[1; 3]);
        file.write(&[2; 2]); // 5 > 4: capacity grows by one block
        assert_eq!(file.capacity(), 8);
        file.write(&[3; 10]); // a write above the block size grows by block + length
        assert_eq!(file.capacity(), 22);
        assert_eq!(file.length(), 15);
    }

    #[test]
    fn writing_while_positioned_keeps_the_rest_and_end() {
        let mut file = EeFsMemFile::new(8, true, 1);
        file.write(&[1, 2, 3, 4]);
        file.set_position(1, ORIGIN_BEGIN);
        file.write(&[9]);
        assert_eq!(file.length(), 4);
        assert_eq!(file.contents(), &[1, 9, 3, 4]);
    }

    #[test]
    fn set_position_edges_and_origins() {
        let mut file = EeFsMemFile::new(8, true, 1);
        file.write(&[1, 2, 3, 4]);
        assert_eq!(file.set_position(-1, ORIGIN_BEGIN), status::BEYOND_END);
        assert_eq!(file.set_position(5, ORIGIN_BEGIN), status::BEYOND_END);
        assert_eq!(file.set_position(1, ORIGIN_END), status::OK);
        assert_eq!(file.position(), 3);
        assert_eq!(file.set_position(1, ORIGIN_CURRENT), status::OK);
        assert_eq!(file.set_position(1, ORIGIN_CURRENT), status::BEYOND_END);
        assert_eq!(file.set_position(0, 3), status::FAILED);
    }

    #[test]
    fn open_closed_file_with_data_rewinds() {
        let mut file = EeFsMemFile::new(8, true, 1);
        file.write(&[1, 2]);
        file.close();
        assert_eq!(file.close(), status::NOT_OPEN);
        assert_eq!(file.write(&[3]), (status::NOT_OPEN, 0));
        assert_eq!(file.open(), status::OK);
        assert_eq!(file.position(), 0);
        assert_eq!(file.length(), 2);
    }

    #[test]
    fn load_error_empties_and_closes_the_file() {
        let mut file = EeFsMemFile::new(8, true, 1);
        file.write(&[1, 2, 3]);
        let mut source = ScriptFile::with(vec![1]);
        assert_eq!(file.load(&mut source, 4), status::END_OF_FILE);
        assert!(file.data().is_none());
        assert!(!file.is_open());
    }

    #[test]
    fn save_prefixed_writes_length_then_data_ignoring_the_length_result() {
        let mut file = EeFsMemFile::new(8, true, 1);
        file.write(&[5, 6]);
        let mut target = ScriptFile::failing_at(0);
        // the length write fails, the data write is still done and its result returned
        assert_eq!(file.save_prefixed(&mut target), status::OK);
        assert_eq!(target.data, vec![5, 6]);
    }

    #[test]
    fn compress_short_and_compressible_cases() {
        let mut short = EeFsMemFile::new(8, true, 1);
        short.write(&[1; 12]);
        assert_eq!(short.compress(0x1000), (status::OK, 12));
        assert!(!short.is_open());
        let mut long = EeFsMemFile::new(8, true, 1);
        long.write(&[7; 1000]);
        let (result, length) = long.compress(0x1000);
        assert_eq!(result, status::OK);
        assert!(length < 1000 && compression::is_compressed(long.contents()));
        assert!(!long.is_open());
        assert_eq!(long.open(), status::OK);
        let (result, length) = long.decompress();
        assert_eq!((result, length), (status::OK, 1000));
        assert_eq!(long.contents(), &[7; 1000][..]);
    }

    #[test]
    fn decompress_of_plain_data_stays_open_and_unmoved() {
        let mut file = EeFsMemFile::new(8, true, 1);
        file.write(&[1, 2, 3]);
        file.set_position(2, ORIGIN_BEGIN);
        assert_eq!(file.decompress(), (status::OK, 3));
        assert_eq!(file.position(), 2);
        assert!(file.is_open());
    }

    /// The interpreter of `golden_fs.txt`'s "mem" lines (the C++ driver has its twin).
    pub(crate) fn run_scenario(ops: &str) -> String {
        let t: Vec<&str> = ops.split_whitespace().collect();
        let mut f: Option<EeFsMemFile> = None;
        let mut o = EeFsMemFile::new(16, true, 1);
        let mut r = String::new();
        let mut i = 0;
        let hex = |s: &str| -> Vec<u8> {
            (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
        };
        let hexs = |d: &[u8]| d.iter().map(|b| format!("{b:02x}")).collect::<String>();
        let num = |s: &str| -> usize {
            if let Some(h) = s.strip_prefix("0x") {
                usize::from_str_radix(h, 16).unwrap()
            } else {
                s.parse().unwrap()
            }
        };
        while i < t.len() {
            let c = t[i];
            i += 1;
            let item = match c {
                "new" => {
                    f = Some(EeFsMemFile::new(num(t[i]), num(t[i + 1]) != 0, num(t[i + 2])));
                    i += 3;
                    "new".to_string()
                }
                "adopt" => {
                    let v = hex(t[i]);
                    f = Some(EeFsMemFile::over(v.into_boxed_slice(), num(t[i + 1]), num(t[i + 2]) != 0));
                    i += 3;
                    "adopt".to_string()
                }
                "w" => {
                    let v = hex(t[i]);
                    i += 1;
                    let (res, w) = f.as_mut().unwrap().write(&v);
                    format!("w={res}:{w}")
                }
                "wf" | "wu32" | "wu16" | "wu8" | "wd" | "wb" => {
                    let bytes: Vec<u8> = match c {
                        "wf" => (t[i].parse::<f32>().unwrap()).to_le_bytes().to_vec(),
                        "wu32" => (num(t[i]) as u32).to_le_bytes().to_vec(),
                        "wu16" => (num(t[i]) as u16).to_le_bytes().to_vec(),
                        "wu8" => vec![num(t[i]) as u8],
                        "wd" => (t[i].parse::<f64>().unwrap()).to_le_bytes().to_vec(),
                        _ => vec![(num(t[i]) != 0) as u8],
                    };
                    i += 1;
                    let (res, _) = f.as_mut().unwrap().write(&bytes);
                    format!("{c}={res}")
                }
                "r" => {
                    let n = num(t[i]);
                    i += 1;
                    let mut b = vec![0u8; n + 1];
                    let (res, got) = f.as_mut().unwrap().read(&mut b[..n]);
                    let g = got.unwrap_or(123);
                    let shown = if g as usize <= n { g as usize } else { 0 };
                    format!("r={res}:{g}:{}", hexs(&b[..shown]))
                }
                "ru32" | "ru16" | "ru8" | "rf" | "rd" | "rb" => {
                    let width = match c {
                        "ru32" | "rf" => 4,
                        "ru16" => 2,
                        "ru8" | "rb" => 1,
                        _ => 8,
                    };
                    let mut b = [0u8; 8];
                    let (res, _) = f.as_mut().unwrap().read(&mut b[..width]);
                    match c {
                        "ru32" => format!("ru32={res}:{}", u32::from_le_bytes(b[..4].try_into().unwrap())),
                        "ru16" => format!("ru16={res}:{}", u16::from_le_bytes(b[..2].try_into().unwrap())),
                        "ru8" => format!("ru8={res}:{}", b[0]),
                        "rb" => format!("rb={res}:{}", (b[0] != 0) as u32),
                        "rf" => format!("rf={res}:{:08x}", u32::from_le_bytes(b[..4].try_into().unwrap())),
                        _ => format!("rd={res}:{:016x}", u64::from_le_bytes(b[..8].try_into().unwrap())),
                    }
                }
                "sp" => {
                    let off: i64 = t[i].parse().unwrap();
                    let org = num(t[i + 1]) as u32;
                    i += 2;
                    format!("sp={}", f.as_mut().unwrap().set_position(off, org))
                }
                "len" => format!("len={}", f.as_ref().unwrap().length()),
                "pos" => format!("pos={}", f.as_ref().unwrap().position()),
                "close" => format!("close={}", f.as_mut().unwrap().close()),
                "open" => {
                    i += 1;
                    format!("open={}", f.as_mut().unwrap().open())
                }
                "compact" => format!("compact={}", f.as_mut().unwrap().compact()),
                "comp" => {
                    let lv = num(t[i]);
                    i += 1;
                    let (res, n) = f.as_mut().unwrap().compress(lv as u32);
                    format!("comp={res}:{n}")
                }
                "decomp" => {
                    let (res, n) = f.as_mut().unwrap().decompress();
                    format!("decomp={res}:{n}")
                }
                "del" => format!("del={}", f.as_mut().unwrap().delete()),
                "data" => {
                    let m = f.as_ref().unwrap();
                    format!("data={}", if m.data().is_some() { hexs(m.contents()) } else { "null".to_string() })
                }
                "free" => {
                    f = None;
                    "free".to_string()
                }
                "osp" => {
                    o.set_position(0, ORIGIN_BEGIN);
                    "osp".to_string()
                }
                "save" => {
                    let res = f.as_mut().unwrap().save_prefixed(&mut o);
                    format!("save={res}:{}", hexs(o.contents()))
                }
                "saven" => {
                    let n = num(t[i]);
                    i += 1;
                    let res = f.as_ref().unwrap().save(&mut o, n);
                    format!("saven={res}:{}", hexs(o.contents()))
                }
                "load" => format!("load={}", f.as_mut().unwrap().load_prefixed(&mut o)),
                "loadn" => {
                    let n = num(t[i]);
                    i += 1;
                    format!("loadn={}", f.as_mut().unwrap().load(&mut o, n))
                }
                "ow" => {
                    let v = hex(t[i]);
                    i += 1;
                    o.set_position(0, ORIGIN_BEGIN);
                    o.write(&v);
                    "ow".to_string()
                }
                other => panic!("bad op {other}"),
            };
            r.push_str(&item);
            r.push(' ');
        }
        r
    }

    #[test]
    fn golden_scenarios_equal_the_cpp_class() {
        let golden = include_str!("golden_fs.txt");
        let mut n = 0;
        for line in golden.lines() {
            let fields: Vec<&str> = line.split('\t').collect();
            if fields[0] != "mem" {
                continue;
            }
            n += 1;
            assert_eq!(run_scenario(fields[2]).trim_end(), fields[3].trim_end(), "scenario {}", fields[1]);
        }
        assert_eq!(n, 20);
    }
}
