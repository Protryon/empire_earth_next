//! `FSArchive`: the ".ssa" package: `"ssar"`, version, reserved dword, a FAT (name, start, end, length per file, sorted),
//! the tag table, the compressed files. The directory walk, `FSFileSpec` handling and the file mapping stay in
//! `FSArchive.cpp`.

use std::collections::BTreeMap;

use ee_codec::compression;
use ee_plat_core::string::{PlatMbsCmp, PlatMbsLen, PlatMbsNBCnt};

use crate::file::{FsFile, FsWrite};
use crate::memfile::EeFsMemFile;
use crate::status;

pub const ARCHIVE_IDENTIFIER: u32 = 0x7373_6172; // 'ssar'
pub const ARCHIVE_VERSION: u32 = 1;
pub const ARCHIVE_RESERVED: u32 = 0;

/// An `FSArchive::ArchivedFile` (`mStart`, `mEnd`, `mLength`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FatEntry {
    pub start: i32,
    pub end: i32,
    pub length: i32,
}

/// A tag's key: `UWideString` bytes with the terminator kept for the platform's multibyte compares and sizes.
#[derive(Clone, Debug)]
pub struct TagKey(Vec<u8>);

impl TagKey {
    fn new(bytes: &[u8]) -> Self {
        let mut key = bytes.to_vec();
        key.push(0);
        TagKey(key)
    }
    fn as_ptr(&self) -> *const u8 {
        self.0.as_ptr()
    }
}

impl PartialEq for TagKey {
    fn eq(&self, other: &Self) -> bool {
        // `operator<` is PlatMbsCmp, as in `std::map<UWideString, ...>`
        mbs_cmp(self, other) == 0
    }
}
impl Eq for TagKey {}
impl PartialOrd for TagKey {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}
impl Ord for TagKey {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        mbs_cmp(self, other).cmp(&0)
    }
}

fn mbs_cmp(a: &TagKey, b: &TagKey) -> i32 {
    // SAFETY: both are NUL-terminated
    unsafe { PlatMbsCmp(a.as_ptr(), b.as_ptr()) }
}

/// `GetBytesUsedToSave` of a `UWideString`: the byte count of its characters + a dword + the terminator.
fn tag_save_size(key: &TagKey) -> usize {
    // SAFETY: NUL-terminated
    unsafe { PlatMbsNBCnt(key.as_ptr(), PlatMbsLen(key.as_ptr())) + 5 }
}

/// The bytes a `UWideString::Save` writes after its size dword.
fn tag_bytes(key: &TagKey) -> &[u8] {
    // SAFETY: NUL-terminated
    let characters = unsafe { PlatMbsLen(key.as_ptr()) };
    // SAFETY: as above
    let length = unsafe { PlatMbsNBCnt(key.as_ptr(), characters) } + 1;
    &key.0[..length.min(key.0.len())]
}

fn write_tag<F: FsWrite + ?Sized>(file: &mut F, key: &TagKey) -> i32 {
    let bytes = tag_bytes(key);
    let result = file.write_u32(bytes.len() as u32);
    if result != status::OK {
        return result;
    }
    let (result, written) = file.write_bytes(bytes);
    if result == status::OK && (written as usize) < bytes.len() {
        return status::FAILED;
    }
    result
}

/// `UWideString::Load`: a size, then that many bytes, kept up to the first NUL. Reads into `out`; failure leaves
/// it empty.
fn read_tag<F: FsFile + ?Sized>(file: &mut F, out: &mut Vec<u8>) -> i32 {
    out.clear();
    let mut size = 0u32;
    let mut result = file.read_u32(&mut size);
    if result == status::OK {
        let mut bytes = vec![0u8; size as usize];
        let (read_result, read) = file.read_bytes(&mut bytes);
        result = read_result;
        if result == status::OK {
            let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
            out.extend_from_slice(&bytes[..end]);
            if read != size {
                result = status::FAILED;
            }
        }
    }
    if result != status::OK {
        out.clear();
    }
    result
}

/// `UString::Load`: a length (0 reads nothing and gives an empty string), then that many bytes up to the first NUL.
fn read_ustring<F: FsFile + ?Sized>(file: &mut F, out: &mut Vec<u8>) -> i32 {
    out.clear();
    let mut length = 0u32;
    let mut result = file.read_u32(&mut length);
    if result == status::OK && length != 0 {
        let mut bytes = vec![0u8; length as usize];
        let (read_result, read) = file.read_bytes(&mut bytes);
        result = read_result;
        if result == status::OK && read == length {
            let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
            out.extend_from_slice(&bytes[..end]);
        } else if result == status::OK {
            result = status::FAILED;
        }
    }
    result
}

/// `UString::Tokenize("*")` on bytes: splits at `*`, skipping empty tokens.
pub fn tokenize_star(pattern: &[u8]) -> Vec<Vec<u8>> {
    let mut tokens = Vec::new();
    let mut start = None;
    for i in 0..=pattern.len() {
        let separator = i == pattern.len() || pattern[i] == b'*';
        match (start, separator) {
            (None, false) => start = Some(i),
            (Some(s), true) => {
                tokens.push(pattern[s..i].to_vec());
                start = None;
            }
            _ => {}
        }
    }
    tokens
}

/// `UString::Find`: the first position at or after `start` where `needle` occurs, `haystack.len()` when not found.
fn find_from(haystack: &[u8], needle: &[u8], start: usize) -> usize {
    if needle.len() <= haystack.len().wrapping_sub(start) {
        for i in start..=haystack.len() - needle.len() {
            if haystack[i..].starts_with(needle) {
                return i;
            }
        }
    }
    haystack.len()
}

/// `FSArchive::ListFiles`' test of one file name against the `*` tokens.
fn name_matches(name: &[u8], tokens: &[Vec<u8>]) -> bool {
    let mut offset = 0;
    for token in tokens {
        if offset >= name.len() {
            return false;
        }
        let found = find_from(name, token, offset);
        if found >= name.len() {
            return false;
        }
        offset = found + token.len();
    }
    true
}

/// `FSArchive`'s tables.
#[derive(Clone, Default)]
pub struct Archive {
    fat: BTreeMap<Vec<u8>, FatEntry>,
    tags: BTreeMap<TagKey, Vec<u8>>,
}

impl Archive {
    pub fn new() -> Self {
        Archive::default()
    }

    /// `Reset`'s part: empties the file table (the facade unmaps and closes the file).
    pub fn clear_fat(&mut self) {
        self.fat.clear();
    }

    pub fn clear_tags(&mut self) {
        self.tags.clear();
    }

    /// `std::map::insert`: an existing name keeps its entry.
    pub fn fat_insert(&mut self, name: &[u8], entry: FatEntry) {
        self.fat.entry(name.to_vec()).or_insert(entry);
    }

    pub fn fat_find(&self, name: &[u8]) -> Option<FatEntry> {
        self.fat.get(name).copied()
    }

    pub fn fat_len(&self) -> usize {
        self.fat.len()
    }

    /// Visits the entries in name order.
    pub fn for_each_fat(&self, mut visit: impl FnMut(&[u8], FatEntry)) {
        for (name, entry) in &self.fat {
            visit(name, *entry);
        }
    }

    /// `ComputeFATSize`: a dword plus, per file, the `UString` save size and three dwords.
    pub fn fat_size(&self) -> u32 {
        let mut size: u32 = 4;
        for name in self.fat.keys() {
            size += name.len() as u32 + 5 + 12;
        }
        size
    }

    /// `ComputeTagsSize`.
    pub fn tags_size(&self) -> u32 {
        let mut size: u32 = 4;
        for (key, value) in &self.tags {
            size += tag_save_size(key) as u32 + tag_save_size(&TagKey::new(value)) as u32;
        }
        size
    }

    /// `SetFATFileOffsets`: lays the files out one after another from `offset`.
    pub fn set_file_offsets(&mut self, offset: u32) {
        let mut offset = offset;
        for entry in self.fat.values_mut() {
            entry.start = offset as i32;
            entry.end = if entry.length > 0 { offset as i32 + entry.length - 1 } else { offset as i32 };
            offset = offset.wrapping_add(entry.length as u32);
        }
    }

    /// `WriteHeader`.
    pub fn write_header<F: FsWrite + ?Sized>(&self, file: &mut F) -> i32 {
        let mut result = file.write_u32(ARCHIVE_IDENTIFIER);
        if result != status::OK {
            return result;
        }
        result = file.write_u32(ARCHIVE_VERSION);
        if result == status::OK {
            result = file.write_u32(ARCHIVE_RESERVED);
        }
        result
    }

    /// `ReadHeader`.
    pub fn read_header<F: FsFile + ?Sized>(&self, file: &mut F) -> i32 {
        let mut value = 0u32;
        let mut result = file.read_u32(&mut value);
        if result != status::OK {
            return result;
        }
        if value != ARCHIVE_IDENTIFIER {
            return status::FAILED;
        }
        result = file.read_u32(&mut value);
        if result != status::OK {
            return result;
        }
        if value != ARCHIVE_VERSION {
            return status::FAILED;
        }
        result = file.read_u32(&mut value);
        if result == status::OK && value != ARCHIVE_RESERVED {
            result = status::FAILED;
        }
        result
    }

    /// `WriteFAT`: returns the first write error.
    pub fn write_fat<F: FsWrite + ?Sized>(&self, file: &mut F) -> i32 {
        let mut result = file.write_u32(self.fat_size() - 4);
        for (name, entry) in &self.fat {
            if result != status::OK {
                break;
            }
            // `UString::Save`: length with the terminator, then the bytes and the terminator
            let mut bytes = name.clone();
            bytes.push(0);
            result = file.write_u32(bytes.len() as u32);
            if result == status::OK {
                let (r, written) = file.write_bytes(&bytes);
                result = r;
                if result == status::OK && (written as usize) < bytes.len() {
                    result = status::FAILED;
                }
            }
            if result == status::OK {
                result = file.write_u32(entry.start as u32);
            }
            if result == status::OK {
                result = file.write_u32(entry.end as u32);
            }
            if result == status::OK {
                result = file.write_u32(entry.length as u32);
            }
        }
        result
    }

    /// `ReadFAT`.
    pub fn read_fat<F: FsFile + ?Sized>(&mut self, file: &mut F) -> i32 {
        let mut fat_size = 0u32;
        let mut result = file.read_u32(&mut fat_size);
        if result != status::OK {
            return result;
        }
        let mut position = 0u32;
        let mut name = Vec::new();
        while position < fat_size && result == status::OK {
            result = read_ustring(file, &mut name);
            if result != status::OK {
                continue;
            }
            // the original's count: the name's strlen + 5 (not the stored length)
            let mut next = position.wrapping_add(name.len() as u32).wrapping_add(5);
            let mut entry = FatEntry { start: 0, end: 0, length: 0 };
            let mut value = 0u32;
            result = file.read_u32(&mut value);
            entry.start = value as i32;
            if result == status::OK {
                next = next.wrapping_add(4);
                result = file.read_u32(&mut value);
                entry.end = value as i32;
                if result == status::OK {
                    next = next.wrapping_add(4);
                    result = file.read_u32(&mut value);
                    entry.length = value as i32;
                    if result == status::OK {
                        next = next.wrapping_add(4);
                        self.fat.entry(name.clone()).or_insert(entry);
                    }
                }
            }
            position = next;
        }
        result
    }

    /// `WriteTags`: the last tag's write status.
    pub fn write_tags<F: FsWrite + ?Sized>(&self, file: &mut F) -> i32 {
        let mut result = file.write_u32(self.tags.len() as u32);
        if result != status::OK {
            return result;
        }
        for (key, value) in &self.tags {
            result = write_tag(file, key);
            if result == status::OK {
                result = write_tag(file, &TagKey::new(value));
            }
        }
        result
    }

    /// `ReadTags`.
    pub fn read_tags<F: FsFile + ?Sized>(&mut self, file: &mut F) -> i32 {
        self.tags.clear();
        let mut count = 0u32;
        let mut result = file.read_u32(&mut count);
        if result != status::OK {
            return result;
        }
        let mut name = Vec::new();
        let mut value = Vec::new();
        let mut i = 0;
        while i < count && result == status::OK {
            i += 1;
            result = read_tag(file, &mut name);
            if result != status::OK {
                continue;
            }
            result = read_tag(file, &mut value);
            if result == status::OK {
                self.set_tag(&name, &value);
            }
        }
        result
    }

    /// `GetTag`: the value, `None` when absent.
    pub fn get_tag(&self, name: &[u8]) -> Option<&[u8]> {
        self.tags.get(&TagKey::new(name)).map(Vec::as_slice)
    }

    /// `SetTag`: an empty value removes the tag.
    pub fn set_tag(&mut self, name: &[u8], value: &[u8]) {
        let key = TagKey::new(name);
        self.tags.remove(&key);
        if !value.is_empty() {
            self.tags.insert(key, value.to_vec());
        }
    }

    /// `ReadFATFile`: a memory file for one archive member, decompressed when compressed. An entry outside the view
    /// fails with an empty file (C++ read past the mapping); the status of a failed decompression is returned with
    /// the (partial) file, as C++ did.
    pub fn read_member(view: &[u8], entry: FatEntry) -> (i32, EeFsMemFile) {
        let start = entry.start as u32 as usize;
        let length = entry.length as u32 as usize;
        let Some(data) = start.checked_add(length).and_then(|end| view.get(start..end)) else {
            return (status::FAILED, EeFsMemFile::over(Vec::new().into_boxed_slice(), 1000, true));
        };
        let (result, buffer) = if compression::is_compressed(data) {
            let (_, size) = compression::decompressed_size(data);
            let mut buffer = vec![0u8; size as usize].into_boxed_slice();
            let (code, _) = compression::decompress(data, &mut buffer);
            (code, buffer)
        } else {
            (status::OK, data.to_vec().into_boxed_slice())
        };
        (result, EeFsMemFile::over(buffer, 1000, true))
    }

    /// `ListFiles`' selection: the names matching a `*` pattern, in order.
    pub fn list_matching(&self, pattern: &[u8]) -> Vec<Vec<u8>> {
        let tokens = tokenize_star(pattern);
        self.fat.keys().filter(|name| name_matches(name, &tokens)).cloned().collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::file::tests::ScriptFile;

    fn hex(s: &str) -> Vec<u8> {
        (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
    }

    fn golden(kind: &str, name: &str) -> Vec<Vec<String>> {
        include_str!("golden_fs.txt")
            .lines()
            .filter_map(|l| {
                let f: Vec<String> = l.split('\t').map(str::to_string).collect();
                (f[0] == kind && (name.is_empty() || f[1] == name)).then_some(f)
            })
            .collect()
    }

    /// The C++ driver's files: the same patterns as the bytes in `golden_fs.txt`.
    fn pat(n: u32, a: u32) -> Vec<u8> {
        (0..n).map(|i| ((i * a + (i >> 3)) & 0xff) as u8).collect()
    }

    type Case<'a> = (&'a str, Vec<(&'a str, Vec<u8>)>, u32);

    #[allow(clippy::type_complexity)]
    fn image(files: &[(&str, Vec<u8>)], level: u32) -> (Archive, Vec<u8>) {
        let mut archive = Archive::new();
        archive.set_tag(b"Name", b"Foo Bar");
        archive.set_tag(b"Empty", b"");
        archive.set_tag(b"Zeta", b"9");
        for (name, data) in files {
            let mut f = EeFsMemFile::new(1000, true, 1);
            f.write(data);
            let (r, length) = f.compress(level);
            assert_eq!(r, 0);
            archive.fat_insert(name.as_bytes(), FatEntry { start: 0, end: 0, length: length as i32 });
        }
        archive.set_file_offsets(archive.tags_size() + archive.fat_size() + 12);
        let mut out = EeFsMemFile::new(1000, true, 1);
        assert_eq!(archive.write_header(&mut out), 0);
        assert_eq!(archive.write_fat(&mut out), 0);
        assert_eq!(archive.write_tags(&mut out), 0);
        for (name, data) in files {
            let mut f = EeFsMemFile::new(1000, true, 1);
            f.write(data);
            let (r, length) = f.compress(level);
            assert_eq!(r, 0);
            assert_eq!(length as i32, archive.fat_find(name.as_bytes()).unwrap().length);
            out.write(f.contents());
        }
        (archive, out.contents().to_vec())
    }

    #[test]
    fn archive_bytes_equal_the_cpp_create() {
        let cases: [Case; 3] = [
            (
                "rec4k",
                vec![
                    ("alpha.txt", pat(40, 1)),
                    ("beta.bin", pat(2000, 5)),
                    ("empty.txt", Vec::new()),
                    ("sub\\one.dat", pat(9, 7)),
                ],
                0x1000,
            ),
            ("flat1k", vec![("alpha.txt", pat(40, 1)), ("beta.bin", pat(2000, 5)), ("empty.txt", Vec::new())], 0x400),
            ("txt2k", vec![("alpha.txt", pat(40, 1)), ("empty.txt", Vec::new())], 0x800),
        ];
        for (name, files, level) in cases {
            let (_, bytes) = image(&files, level);
            assert_eq!(hex(&golden("arch-file", name)[0][2]), bytes, "{name}");
        }
    }

    #[test]
    fn open_finds_files_tags_and_lists_like_the_cpp() {
        for (name, g) in [("rec4k", 3usize), ("flat1k", 2), ("txt2k", 2)] {
            let bytes = hex(&golden("arch-file", name)[0][2]);
            let mut archive = Archive::new();
            let mut f = EeFsMemFile::over(bytes.clone().into_boxed_slice(), 1000, true);
            assert_eq!(archive.read_header(&mut f), 0);
            assert_eq!(archive.read_fat(&mut f), 0);
            assert_eq!(archive.read_tags(&mut f), 0);
            assert_eq!(archive.get_tag(b"Name"), Some(&b"Foo Bar"[..]));
            assert_eq!(golden("arch-tag", name)[0][3], "Foo Bar");
            assert_eq!(archive.get_tag(b"Empty"), None);
            assert!(archive.fat_find(b"alpha.txt").is_some() && g > 1);
            let listed: Vec<String> =
                archive.list_matching(b"*.txt").iter().map(|n| String::from_utf8_lossy(n).into_owned()).collect();
            let expected = golden("arch-list", name)[0][2..].to_vec();
            assert_eq!(listed, expected);
            let listed2: Vec<String> =
                archive.list_matching(b"sub*one*").iter().map(|n| String::from_utf8_lossy(n).into_owned()).collect();
            assert_eq!(listed2, golden("arch-list2", name)[0][2..].to_vec());
            assert!(archive.list_matching(b"ALPHA").is_empty());
            for line in golden("arch-find", name) {
                let name = line[2].to_lowercase();
                match archive.fat_find(name.as_bytes()) {
                    Some(entry) => {
                        let (code, file) = Archive::read_member(&bytes, entry);
                        assert_eq!(code.to_string(), line[3]);
                        assert_eq!(file.length().to_string(), line[4]);
                        assert_eq!(hex(&line[5]), file.contents());
                    }
                    None => assert_eq!(line[3], status::NOT_FOUND.to_string()),
                }
            }
        }
    }

    #[test]
    fn a_wrong_identifier_or_version_fails_like_the_cpp() {
        let bytes = hex(&golden("arch-file", "txt2k")[0][2]);
        let mut wrong = bytes.clone();
        wrong[0] = b'X';
        let archive = Archive::new();
        assert_eq!(archive.read_header(&mut ScriptFile::with(wrong)).to_string(), golden("arch-badmagic", "")[0][1]);
        let mut wrong = bytes;
        wrong[4] = 2;
        assert_eq!(archive.read_header(&mut ScriptFile::with(wrong)).to_string(), golden("arch-badversion", "")[0][1]);
    }

    #[test]
    fn fat_insert_keeps_the_first_entry() {
        let mut a = Archive::new();
        a.fat_insert(b"x", FatEntry { start: 1, end: 1, length: 1 });
        a.fat_insert(b"x", FatEntry { start: 2, end: 2, length: 2 });
        assert_eq!(a.fat_find(b"x").unwrap().start, 1);
    }

    #[test]
    fn tags_set_replace_and_remove() {
        let mut a = Archive::new();
        a.set_tag(b"k", b"1");
        a.set_tag(b"k", b"2");
        assert_eq!(a.get_tag(b"k"), Some(&b"2"[..]));
        a.set_tag(b"k", b"");
        assert_eq!(a.get_tag(b"k"), None);
    }

    #[test]
    fn empty_entries_and_offsets_end_at_their_start() {
        let mut a = Archive::new();
        a.fat_insert(b"a", FatEntry { start: 0, end: 0, length: 0 });
        a.fat_insert(b"b", FatEntry { start: 0, end: 0, length: 3 });
        a.set_file_offsets(100);
        assert_eq!(a.fat_find(b"a").unwrap(), FatEntry { start: 100, end: 100, length: 0 });
        assert_eq!(a.fat_find(b"b").unwrap(), FatEntry { start: 100, end: 102, length: 3 });
    }

    #[test]
    fn read_member_rejects_entries_outside_the_view() {
        let (code, file) = Archive::read_member(&[0, 1, 2], FatEntry { start: 10, end: 11, length: 2 });
        assert_eq!(code, status::FAILED);
        assert_eq!(file.length(), 0);
    }

    #[test]
    fn tokenize_star_equals_the_cpp_rules() {
        assert_eq!(tokenize_star(b"*.txt"), vec![b".txt".to_vec()]);
        assert_eq!(tokenize_star(b"a*b**c"), vec![b"a".to_vec(), b"b".to_vec(), b"c".to_vec()]);
        assert!(tokenize_star(b"***").is_empty());
        assert!(tokenize_star(b"").is_empty());
    }

    #[test]
    fn name_matching_rules() {
        let t = tokenize_star(b"a*c");
        assert!(name_matches(b"abc", &t));
        assert!(!name_matches(b"ab", &t));
        assert!(!name_matches(b"xbc", &[b"a".to_vec()]));
        // no tokens match everything
        assert!(name_matches(b"anything", &[]));
    }

    #[test]
    fn read_fat_stops_at_the_first_error_and_at_the_table_end() {
        let mut archive = Archive::new();
        // FAT of 21 bytes: one entry "a" (5 + 1 + 12 = 18... the original's count is name + 5 + 12) then a second that is
        // never reached because position >= size
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&18u32.to_le_bytes());
        bytes.extend_from_slice(&2u32.to_le_bytes());
        bytes.extend_from_slice(b"a\0");
        for v in [1u32, 2, 3] {
            bytes.extend_from_slice(&v.to_le_bytes());
        }
        // garbage that would be read if the size bound failed
        bytes.extend_from_slice(&999u32.to_le_bytes());
        let mut f = ScriptFile::with(bytes);
        assert_eq!(archive.read_fat(&mut f), 0);
        assert_eq!(archive.fat_len(), 1);
        assert_eq!(archive.fat_find(b"a").unwrap(), FatEntry { start: 1, end: 2, length: 3 });
        // a failing name read ends the loop with its error
        let mut archive = Archive::new();
        let mut f = ScriptFile::with(vec![100, 0, 0, 0]);
        assert_eq!(archive.read_fat(&mut f), status::END_OF_FILE);
        assert_eq!(archive.fat_len(), 0);
    }

    #[test]
    fn write_fat_and_tags_pass_errors_through() {
        let mut a = Archive::new();
        a.fat_insert(b"a", FatEntry { start: 1, end: 2, length: 3 });
        for failing in 0..5 {
            let mut f = ScriptFile::failing_at(failing);
            assert_eq!(a.write_fat(&mut f), status::FAILED, "op {failing}");
        }
        let mut a = Archive::new();
        a.set_tag(b"a", b"b");
        let mut f = ScriptFile::failing_at(0);
        assert_eq!(a.write_tags(&mut f), status::FAILED);
    }

    #[test]
    fn clone_is_independent() {
        let mut a = Archive::new();
        a.fat_insert(b"a", FatEntry { start: 1, end: 2, length: 3 });
        let b = a.clone();
        a.clear_fat();
        assert_eq!(b.fat_len(), 1);
    }
}
