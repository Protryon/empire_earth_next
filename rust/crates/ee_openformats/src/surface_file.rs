//! `GESurface`'s image files (R4.3, fifteenth part): `Load`, `Save` and `GetImageFileAttributes` with every
//! per-format member of GESurface.cpp (the BMP writer, the JPEG and JPG2 readers and writers, the PNG reader), in the
//! original's statements. The rows are `ee_ge`'s (`surface_px.rs`), the PNG decoder this crate's (`png.rs`).
//!
//! `GESurface` stays the C++ abstract class the renderer implements (`DX7Surface`); its image members are one-line
//! forwarders. Every call the original made on another object is one host operation (`EeOfSurfOp`,
//! `GESurface.cpp`'s `GESurfaceHost`): the surface's virtuals, the file's (an `FSFileSpec`, whose virtuals a memory
//! file overrides) and libjpeg's (its structures stay C: a decoder or encoder is a handle the host owns). The C
//! stream's own calls (`fseek`, `ftell`, `_getw`/`_putw`) are made here, on the `FILE*` the file's `FOpen` opened.

#![allow(clippy::unnecessary_cast)] // `c_ulong` is `u32` on Windows, `u64` on Linux and macOS

use std::ffi::{c_int, c_long, c_ulong, c_void};

use ee_ge::surface_px::{ee_ge_surf_png_store, ee_ge_surf_row_in, ee_ge_surf_row_out, EeGeSurfRow};

use crate::png::png_to_picture;

/// The calls `GESurface`'s image members make on other objects. `obj` is the object called (the surface, the file,
/// or a libjpeg handle); `a`, `b`, `c` as each says. Results are `long`s unless said otherwise.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeOfSurfOp {
    /// `surface->GetPixelFormat()`.
    Format = 1,
    /// `surface->GetPixelDepth()`.
    Depth,
    /// `surface->GetBytesPerRow()`.
    BytesPerRow,
    /// `surface->GetMemoryAddress()`.
    Memory,
    /// `surface->GetPixelWidth()`.
    PixelWidth,
    /// `surface->GetPixelHeight()`.
    PixelHeight,
    /// `surface->GetSurfaceDescription().mNoAlpha`.
    NoAlpha,
    /// `surface->Lock()`.
    Lock,
    /// `surface->Unlock()`.
    Unlock,
    /// `file->GetIsOpen()`.
    FileIsOpen,
    /// `file->GetFileAccess()`.
    FileAccess,
    /// `file->Open(a)`.
    FileOpen,
    /// `file->Close()`.
    FileClose,
    /// `file->FOpen((char*)a)`.
    FileFOpen,
    /// `(FILE*)*file`.
    FileStream,
    /// `file->GetPosition(*(long*)a)`.
    FilePosition,
    /// `file->SetPosition(a, b, c != 0)`.
    FileSetPosition,
    /// `file->Write((const unsigned char*)a, b, written)` (the count written is not read).
    FileWrite,
    /// `file->Write((unsigned short)a)`.
    FileWriteU16,
    /// `file->Write((long)a)`.
    FileWriteLong,
    /// `file->Read((unsigned char*)a, b, *(unsigned long*)c)`.
    FileRead,
    /// `file->GetLength(*(unsigned long*)a)`.
    FileLength,
    /// A new decompressor (`jpeg_std_error`, `jpeg_create_decompress`) -> its handle.
    JpegDecNew,
    /// `jpeg_stdio_src(obj, (FILE*)a)`, `jpeg_read_header`, `jpeg_start_decompress`, the row buffer allocated;
    /// `b` (three `u32`) gets the output width, height and components -> the row's length.
    JpegDecStart,
    /// The next scanline while `output_scanline < output_height` and `jpeg_read_scanlines` reads one -> the row, or
    /// NULL.
    JpegDecRow,
    /// `jpeg_finish_decompress(obj)`.
    JpegDecFinish,
    /// `jpeg_destroy_decompress(obj)`; the handle freed.
    JpegDecDestroy,
    /// A new quality-80 RGB compressor of `b` x `c` on stream `a`, started, with its row buffer of `b * 3 + b % 4`
    /// bytes -> its handle.
    JpegEncNew,
    /// The compressor's row buffer.
    JpegEncBuffer,
    /// `jpeg_write_scanlines(obj, buffer, 1)`.
    JpegEncRow,
    /// `jpeg_finish_compress(obj)`, `jpeg_destroy_compress(obj)`; the handle freed.
    JpegEncEnd,
}

/// The host. `obj` as `EeOfSurfOp` says.
pub type EeOfSurfHostFn = unsafe extern "C" fn(op: EeOfSurfOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize;

const K_ERROR_OK: c_long = 0;
const K_ERROR_FAILED: c_long = 9;
const K_FILE_ACCESS_READ: isize = 1;
const K_FILE_ACCESS_WRITE: isize = 2;
const K_FILE_ACCESS_READ_WRITE: isize = 3;
const K_FILE_ACCESS_APPEND: isize = 4;
const K_FILE_ORIGIN_BEGIN: isize = 0;

/// `GEImageFileType`.
const K_IMAGE_FILE_BMP: u8 = 1;
const K_IMAGE_FILE_JPG: u8 = 2;
const K_IMAGE_FILE_JPG2: u8 = 3;
const K_IMAGE_FILE_PNG: u8 = 4;

const SEEK_SET: c_int = 0;
const EOF: c_int = -1;

/// The C runtime's stream calls on the file's `FILE*`.
/// cbindgen:ignore
mod crt {
    use std::ffi::{c_int, c_long, c_void};

    extern "C" {
        pub fn fseek(stream: *mut c_void, offset: c_long, origin: c_int) -> c_int;
        pub fn ftell(stream: *mut c_void) -> c_long;
        pub fn fread(buffer: *mut c_void, size: usize, count: usize, stream: *mut c_void) -> usize;
        pub fn fwrite(buffer: *const c_void, size: usize, count: usize, stream: *mut c_void) -> usize;
    }
}
use crt::{fread, fseek, ftell, fwrite};

/// `_getw`: a 4-byte int in the file's (little-endian) order; `EOF` on a short read.
///
/// # Safety
/// `stream` is an open C stream.
unsafe fn get_word(stream: *mut c_void) -> c_int {
    let mut word = [0u8; 4];
    // SAFETY: the caller's contract
    if unsafe { fread(word.as_mut_ptr().cast(), 4, 1, stream) } == 1 {
        i32::from_le_bytes(word)
    } else {
        EOF
    }
}

/// `_putw`.
///
/// # Safety
/// `stream` is an open C stream.
unsafe fn put_word(word: c_int, stream: *mut c_void) {
    let bytes = word.to_le_bytes();
    // SAFETY: the caller's contract
    unsafe { fwrite(bytes.as_ptr().cast(), 4, 1, stream) };
}

/// One image operation's surface and file, with the host.
struct Io {
    host: EeOfSurfHostFn,
    surface: *mut c_void,
    file: *mut c_void,
}

impl Io {
    fn call(&self, op: EeOfSurfOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize {
        // SAFETY: the host's contract (the pointers are the caller's live objects)
        unsafe { (self.host)(op, obj, a, b, c) }
    }
    fn s(&self, op: EeOfSurfOp) -> isize {
        self.call(op, self.surface, 0, 0, 0)
    }
    fn f(&self, op: EeOfSurfOp, a: isize, b: isize, c: isize) -> c_long {
        self.call(op, self.file, a, b, c) as c_long
    }
    fn format(&self) -> c_ulong {
        self.s(EeOfSurfOp::Format) as c_ulong
    }
    /// `GetPixelDepth() == 32 ? 32 : 16`.
    fn depth(&self) -> c_ulong {
        if self.s(EeOfSurfOp::Depth) as c_ulong == 32 {
            32
        } else {
            16
        }
    }
    fn memory(&self) -> *mut u8 {
        self.s(EeOfSurfOp::Memory) as *mut u8
    }
    fn lock(&self) -> c_long {
        self.s(EeOfSurfOp::Lock) as c_long
    }
    fn unlock(&self) -> c_long {
        self.s(EeOfSurfOp::Unlock) as c_long
    }
    fn is_open(&self) -> bool {
        self.f(EeOfSurfOp::FileIsOpen, 0, 0, 0) != 0
    }
    fn open(&self, access: isize) -> c_long {
        self.f(EeOfSurfOp::FileOpen, access, 0, 0)
    }
    fn close(&self) -> c_long {
        self.f(EeOfSurfOp::FileClose, 0, 0, 0)
    }
    fn fopen(&self, mode: &'static [u8]) -> c_long {
        self.f(EeOfSurfOp::FileFOpen, mode.as_ptr() as isize, 0, 0)
    }
    fn stream(&self) -> *mut c_void {
        self.call(EeOfSurfOp::FileStream, self.file, 0, 0, 0) as *mut c_void
    }
    fn position(&self, out: &mut c_long) -> c_long {
        self.f(EeOfSurfOp::FilePosition, out as *mut c_long as isize, 0, 0)
    }
    fn set_position(&self, position: c_long, extend: bool) -> c_long {
        self.f(EeOfSurfOp::FileSetPosition, position as isize, K_FILE_ORIGIN_BEGIN, extend as isize)
    }
    fn write(&self, data: &[u8]) -> c_long {
        self.f(EeOfSurfOp::FileWrite, data.as_ptr() as isize, data.len() as isize, 0)
    }
    /// The skip after each row: the row's pixels past the width (`(GetBytesPerRow() - GetPixelWidth() * bytes) /
    /// bytes`, its operands left to right).
    fn skip(&self, depth: c_ulong) -> c_ulong {
        let bytes = depth / 8;
        let bytes_per_row = self.s(EeOfSurfOp::BytesPerRow) as c_ulong;
        let width = self.s(EeOfSurfOp::PixelWidth) as c_ulong;
        bytes_per_row.wrapping_sub(width.wrapping_mul(bytes)) / bytes
    }
}

/// The BMP and JPEG rows' size: `width * 3 + width % 4` (the original's padding).
fn row_size(width: c_ulong) -> c_ulong {
    width.wrapping_mul(3).wrapping_add(width % 4)
}

/// `SaveAsBMP`: a bottom-up 24-bit BMP (the file must be open for writing, or is opened and closed), the two
/// trailing bytes after the pixels.
fn save_bmp(io: &Io) -> c_long {
    let format = io.format();
    let width = io.s(EeOfSurfOp::PixelWidth) as c_ulong;
    let height = io.s(EeOfSurfOp::PixelHeight) as c_ulong;
    let was_open = io.is_open();
    let mut result = if was_open {
        if io.f(EeOfSurfOp::FileAccess, 0, 0, 0) as isize != K_FILE_ACCESS_WRITE {
            K_ERROR_FAILED
        } else {
            K_ERROR_OK
        }
    } else {
        io.open(K_FILE_ACCESS_WRITE)
    };
    if result == K_ERROR_OK {
        let size = row_size(width);
        let mut buffer = vec![0u8; size as usize];
        let mut header = Vec::with_capacity(14);
        header.extend_from_slice(&0x4d42u16.to_le_bytes());
        header.extend_from_slice(&(size.wrapping_mul(height).wrapping_add(14 + 40 + 2) as u32).to_le_bytes());
        header.extend_from_slice(&[0, 0, 0, 0]);
        header.extend_from_slice(&54u32.to_le_bytes());
        result = io.write(&header);
        if result == K_ERROR_OK {
            let mut info = Vec::with_capacity(40);
            for v in [40u32, width as u32, height as u32] {
                info.extend_from_slice(&v.to_le_bytes());
            }
            info.extend_from_slice(&1u16.to_le_bytes());
            info.extend_from_slice(&24u16.to_le_bytes());
            for v in [0u32, 0, 2834, 2834, 0, 0] {
                info.extend_from_slice(&v.to_le_bytes());
            }
            result = io.write(&info);
            if result == K_ERROR_OK {
                result = io.lock();
                if result == K_ERROR_OK {
                    // bottom row first, each as blue, green, red
                    let bytes_per_row = io.s(EeOfSurfOp::BytesPerRow) as c_ulong;
                    let depth = io.depth();
                    let pitch = bytes_per_row / (depth / 8) * (depth / 8);
                    let last = height.wrapping_sub(1).wrapping_mul(pitch) as u32 as usize;
                    let mut row = io.memory().wrapping_add(last);
                    for _ in 0..height {
                        // SAFETY: the locked surface holds `height` rows of `pitch` bytes
                        unsafe { ee_ge_surf_row_out(format, depth, row, width, EeGeSurfRow::Bgr, buffer.as_mut_ptr()) };
                        io.write(&buffer);
                        row = row.wrapping_sub(pitch as usize);
                    }
                    io.unlock();
                    result = io.f(EeOfSurfOp::FileWriteU16, 0, 0, 0);
                }
            }
        }
        drop(buffer);
        if !was_open {
            result = io.close();
        }
    }
    result
}

/// The readers' open: a file that was open has its position taken and is closed (the C stream reopens it).
fn close_at(io: &Io, position: &mut c_long) -> bool {
    let was_open = io.is_open();
    if was_open && io.position(position) == K_ERROR_OK {
        io.close();
    }
    was_open
}

/// A decoded JPEG's scanlines into the locked surface from its first pixel: opaque colour, or (`alpha`) the alpha
/// merged into the pixels there; the decompressor finished and destroyed.
fn read_jpeg(io: &Io, format: c_ulong, decoder: *mut c_void, stream: *mut c_void, alpha: bool) {
    let mut dims = [0u32; 3];
    let stride = io.call(EeOfSurfOp::JpegDecStart, decoder, stream as isize, dims.as_mut_ptr() as isize, 0) as c_ulong;
    let depth = io.depth();
    let skip = io.skip(depth);
    let mut pixel = io.memory();
    loop {
        let row = io.call(EeOfSurfOp::JpegDecRow, decoder, 0, 0, 0) as *const u8;
        if row.is_null() {
            break;
        }
        // SAFETY: the locked surface holds the image's rows; `row` holds `stride` bytes
        pixel = unsafe { ee_ge_surf_row_in(format, depth, row, stride, alpha, pixel, skip) };
    }
    io.call(EeOfSurfOp::JpegDecFinish, decoder, 0, 0, 0);
    io.call(EeOfSurfOp::JpegDecDestroy, decoder, 0, 0, 0);
}

/// A new decompressor.
fn decoder(io: &Io) -> *mut c_void {
    io.call(EeOfSurfOp::JpegDecNew, std::ptr::null_mut(), 0, 0, 0) as *mut c_void
}

/// `GetJPGFileAttributes`: the width, height and components times 8 of a JPEG (the file opened and closed).
fn jpg_attributes(io: &Io, out: &mut [c_ulong; 3]) -> c_long {
    let result = io.fopen(b"rb\0");
    if result != K_ERROR_OK {
        return result;
    }
    let decoder = decoder(io);
    let stream = io.stream();
    if !stream.is_null() {
        let mut dims = [0u32; 3];
        io.call(EeOfSurfOp::JpegDecStart, decoder, stream as isize, dims.as_mut_ptr() as isize, 0);
        *out = [dims[0] as c_ulong, dims[1] as c_ulong, dims[2].wrapping_mul(8) as c_ulong];
        io.call(EeOfSurfOp::JpegDecDestroy, decoder, 0, 0, 0);
    }
    io.close()
}

/// After a read through the C stream: a file that was open is reopened for reading at `position`, else closed.
fn reopen_read(io: &Io, was_open: bool, position: c_long) -> c_long {
    if !was_open {
        return io.close();
    }
    let mut result = io.close();
    if result == K_ERROR_OK {
        result = io.open(K_FILE_ACCESS_READ);
        if result == K_ERROR_OK {
            result = io.set_position(position, false);
        }
    }
    result
}

/// `LoadJPG`: a JPEG from the file's position (a file that was open is read from its position and reopened after the
/// image), opaque.
fn load_jpg(io: &Io) -> c_long {
    let format = io.format();
    let mut position: c_long = 0;
    let was_open = close_at(io, &mut position);
    let result = io.fopen(b"rb\0");
    if result != K_ERROR_OK {
        return result;
    }
    let stream = io.stream();
    if !stream.is_null() {
        // SAFETY: the file's open C stream
        unsafe { fseek(stream, position, SEEK_SET) };
        let decoder = decoder(io);
        if io.lock() == K_ERROR_OK {
            read_jpeg(io, format, decoder, stream, false);
            io.unlock();
        } else {
            // the lock failed: still release the decompressor's memory pools
            io.call(EeOfSurfOp::JpegDecDestroy, decoder, 0, 0, 0);
        }
    }
    if was_open {
        // SAFETY: as above
        position = unsafe { ftell(stream) };
    }
    reopen_read(io, was_open, position)
}

/// `LoadJPG2`: the colour JPEG, then the alpha JPEG at the offset of the two-word header, merged into the pixels'
/// alpha; a file that was open is reopened at the end offset.
fn load_jpg2(io: &Io) -> c_long {
    let format = io.format();
    let mut position: c_long = 0;
    let mut end: c_long = 0;
    let was_open = close_at(io, &mut position);
    let result = io.fopen(b"rb\0");
    if result != K_ERROR_OK {
        return result;
    }
    let stream = io.stream();
    if !stream.is_null() {
        // SAFETY: the file's open C stream
        let alpha_position = unsafe {
            fseek(stream, position, SEEK_SET);
            let alpha = get_word(stream);
            end = get_word(stream);
            alpha
        };
        let first = decoder(io);
        if io.lock() == K_ERROR_OK {
            read_jpeg(io, format, first, stream, false);
            // SAFETY: as above
            unsafe { fseek(stream, alpha_position, SEEK_SET) };
            let second = decoder(io);
            read_jpeg(io, format, second, stream, true);
            io.unlock();
        } else {
            io.call(EeOfSurfOp::JpegDecDestroy, first, 0, 0, 0);
        }
    }
    reopen_read(io, was_open, end)
}

/// The JPEG writers' open: a file that was open appended to (its position taken first), else created.
fn open_for_jpeg(io: &Io, position: &mut c_long) -> (bool, c_long) {
    let was_open = io.is_open();
    if !was_open {
        return (false, io.fopen(b"wb\0"));
    }
    let mut result = io.position(position);
    if result == K_ERROR_OK {
        result = io.close();
    }
    if result == K_ERROR_OK {
        result = io.fopen(b"ab\0");
    }
    (true, result)
}

/// A new started quality-80 compressor on the stream and its row buffer.
fn encoder(io: &Io, stream: *mut c_void, width: c_ulong, height: c_ulong) -> (*mut c_void, *mut u8) {
    let encoder =
        io.call(EeOfSurfOp::JpegEncNew, std::ptr::null_mut(), stream as isize, width as isize, height as isize)
            as *mut c_void;
    let buffer = io.call(EeOfSurfOp::JpegEncBuffer, encoder, 0, 0, 0) as *mut u8;
    (encoder, buffer)
}

/// One JPEG's rows: the locked surface's (`kind`: colour or alpha), or blank rows when the lock failed (libjpeg
/// aborts on an image with missing rows).
#[allow(clippy::too_many_arguments)]
fn jpeg_rows(
    io: &Io,
    encoder: *mut c_void,
    buffer: *mut u8,
    format: c_ulong,
    width: c_ulong,
    height: c_ulong,
    locked: bool,
    kind: EeGeSurfRow,
) {
    if locked {
        let bytes_per_row = io.s(EeOfSurfOp::BytesPerRow) as c_ulong;
        let depth = io.depth();
        for y in 0..height {
            let offset = y.wrapping_mul(bytes_per_row / (depth / 8)).wrapping_mul(depth / 8);
            let row = io.memory().wrapping_add(offset as usize);
            // SAFETY: the locked surface holds the row; the buffer holds `width * 3` bytes
            unsafe { ee_ge_surf_row_out(format, depth, row, width, kind, buffer) };
            io.call(EeOfSurfOp::JpegEncRow, encoder, 0, 0, 0);
        }
    } else {
        // SAFETY: the buffer holds `row_size(width)` bytes
        unsafe { std::ptr::write_bytes(buffer, 0, row_size(width) as usize) };
        for _ in 0..height {
            io.call(EeOfSurfOp::JpegEncRow, encoder, 0, 0, 0);
        }
    }
}

/// `SaveAsJPG`: a quality-80 JPEG, written at the position of a file that was open (reopened for appending after).
fn save_jpg(io: &Io) -> c_long {
    let format = io.format();
    let width = io.s(EeOfSurfOp::PixelWidth) as c_ulong;
    let height = io.s(EeOfSurfOp::PixelHeight) as c_ulong;
    let mut position: c_long = 0;
    let (was_open, result) = open_for_jpeg(io, &mut position);
    if result != K_ERROR_OK {
        return result;
    }
    let stream = io.stream();
    if !stream.is_null() {
        // SAFETY: the file's open C stream
        unsafe { fseek(stream, position, SEEK_SET) };
        let (jpeg, buffer) = encoder(io, stream, width, height);
        if io.lock() == K_ERROR_OK {
            jpeg_rows(io, jpeg, buffer, format, width, height, true, EeGeSurfRow::Rgb);
            io.unlock();
        } else {
            jpeg_rows(io, jpeg, buffer, format, width, height, false, EeGeSurfRow::Rgb);
        }
        io.call(EeOfSurfOp::JpegEncEnd, jpeg, 0, 0, 0);
    }
    if !was_open {
        return io.close();
    }
    // SAFETY: as above
    let position = unsafe { ftell(stream) };
    let mut result = io.close();
    if result == K_ERROR_OK {
        result = io.open(K_FILE_ACCESS_APPEND);
        if result == K_ERROR_OK {
            result = io.set_position(position, false);
        }
    }
    result
}

/// `SaveAsJPG2`: placeholder offsets, the colour JPEG and the alpha JPEG (the surface locked over both), then the
/// two offsets patched in through the file reopened for reading and writing.
fn save_jpg2(io: &Io) -> c_long {
    let format = io.format();
    let width = io.s(EeOfSurfOp::PixelWidth) as c_ulong;
    let height = io.s(EeOfSurfOp::PixelHeight) as c_ulong;
    let mut position: c_long = 0;
    let mut alpha: c_long = 0;
    let mut end: c_long = 0;
    let (was_open, result) = open_for_jpeg(io, &mut position);
    if result != K_ERROR_OK {
        return result;
    }
    let stream = io.stream();
    if !stream.is_null() {
        // SAFETY: the file's open C stream
        unsafe {
            put_word(alpha, stream);
            put_word(end, stream);
        }
        let (jpeg, buffer) = encoder(io, stream, width, height);
        let locked = io.lock() == K_ERROR_OK;
        jpeg_rows(io, jpeg, buffer, format, width, height, locked, EeGeSurfRow::Rgb);
        io.call(EeOfSurfOp::JpegEncEnd, jpeg, 0, 0, 0);
        // SAFETY: as above
        alpha = unsafe { ftell(stream) };
        let (jpeg, buffer) = encoder(io, stream, width, height);
        jpeg_rows(io, jpeg, buffer, format, width, height, locked, EeGeSurfRow::Alpha);
        if locked {
            io.unlock();
        }
        io.call(EeOfSurfOp::JpegEncEnd, jpeg, 0, 0, 0);
        // SAFETY: as above
        end = unsafe { ftell(stream) };
    }
    let mut result = io.close();
    if result == K_ERROR_OK {
        result = io.open(K_FILE_ACCESS_READ_WRITE);
        if result == K_ERROR_OK {
            result = io.set_position(position, false);
            if result == K_ERROR_OK {
                result = io.f(EeOfSurfOp::FileWriteLong, alpha as isize, 0, 0);
                if result == K_ERROR_OK {
                    result = io.f(EeOfSurfOp::FileWriteLong, end as isize, 0, 0);
                    if was_open && result == K_ERROR_OK {
                        result = io.set_position(end, false);
                    }
                }
            }
            if !was_open {
                result = io.close();
            }
        }
    }
    result
}

/// A PNG file's bytes, read whole from its start (an open file must be open for reading and stays open; a closed one
/// is opened and closed again).
fn read_png_file(io: &Io, out: &mut Vec<u8>) -> c_long {
    let was_open = io.is_open();
    let mut result = if was_open {
        if io.f(EeOfSurfOp::FileAccess, 0, 0, 0) as isize != K_FILE_ACCESS_READ {
            K_ERROR_FAILED
        } else {
            io.set_position(0, true)
        }
    } else {
        io.open(K_FILE_ACCESS_READ)
    };
    if result != K_ERROR_OK {
        return result;
    }
    let mut length: c_ulong = 0;
    result = io.f(EeOfSurfOp::FileLength, &mut length as *mut c_ulong as isize, 0, 0);
    if result == K_ERROR_OK {
        out.resize(length as usize, 0);
        let mut read: c_ulong = 0;
        if length != 0 {
            result = io.f(
                EeOfSurfOp::FileRead,
                out.as_mut_ptr() as isize,
                length as isize,
                &mut read as *mut c_ulong as isize,
            );
            if result == K_ERROR_OK && read != length {
                result = K_ERROR_FAILED;
            }
        }
    }
    if !was_open {
        let close = io.close();
        if result == K_ERROR_OK {
            result = close;
        }
    }
    result
}

/// A PNG file decoded (`png.rs`: B, G, R, A per pixel, top row first): the width, height, depth (32 with alpha, else
/// 24) and the pixels.
fn decode_png(io: &Io) -> Result<(c_ulong, c_ulong, c_ulong, Vec<u8>), c_long> {
    let mut data = Vec::new();
    let result = read_png_file(io, &mut data);
    if result != K_ERROR_OK {
        return Err(result);
    }
    match png_to_picture(&data) {
        Ok(p) => Ok((p.width as c_ulong, p.height as c_ulong, p.depth as c_ulong, p.bgra)),
        Err(_) => Err(K_ERROR_FAILED),
    }
}

/// `LoadPNG`: the picture into the surface's top left corner (its alpha, or 0xff without one or when the surface has
/// no alpha), packed to the surface's format; a picture larger than the surface is refused.
fn load_png(io: &Io) -> c_long {
    let format = io.format();
    let no_alpha = io.s(EeOfSurfOp::NoAlpha) != 0;
    let (width, height, _, pixels) = match decode_png(io) {
        Ok(p) => p,
        Err(result) => return result,
    };
    if width > io.s(EeOfSurfOp::PixelWidth) as c_ulong || height > io.s(EeOfSurfOp::PixelHeight) as c_ulong {
        return K_ERROR_FAILED;
    }
    let result = io.lock();
    if result != K_ERROR_OK {
        return result;
    }
    // the store's arguments right to left, as clang-cl's MSVC ABI evaluates them
    let bytes_per_row = io.s(EeOfSurfOp::BytesPerRow) as c_ulong;
    let memory = io.memory();
    let depth = io.depth();
    let data = if pixels.is_empty() { std::ptr::null() } else { pixels.as_ptr() };
    // SAFETY: the locked surface holds the picture (checked above); `data` holds its pixels
    unsafe { ee_ge_surf_png_store(format, depth, no_alpha, data, width, height, memory, bytes_per_row) };
    io.unlock()
}

/// `GESurface::Load(file, type)` (`surface` the `GESurface*`, `file` the `FSFileSpec*`): BMP fails (never read),
/// JPG, JPG2, PNG; any other type fails (TGA: no longer read, D9).
///
/// # Safety
/// `host` serves the surface and the file; both are live.
#[no_mangle]
pub unsafe extern "C" fn ee_of_surf_load(
    host: EeOfSurfHostFn,
    surface: *mut c_void,
    file: *mut c_void,
    kind: u8,
) -> c_long {
    let io = Io { host, surface, file };
    match kind {
        K_IMAGE_FILE_JPG => load_jpg(&io),
        K_IMAGE_FILE_JPG2 => load_jpg2(&io),
        K_IMAGE_FILE_PNG => load_png(&io),
        _ => K_ERROR_FAILED,
    }
}

/// `GESurface::Save(file, type)`: BMP, JPG, JPG2; any other type fails.
///
/// # Safety
/// As `ee_of_surf_load`.
#[no_mangle]
pub unsafe extern "C" fn ee_of_surf_save(
    host: EeOfSurfHostFn,
    surface: *mut c_void,
    file: *mut c_void,
    kind: u8,
) -> c_long {
    let io = Io { host, surface, file };
    match kind {
        K_IMAGE_FILE_BMP => save_bmp(&io),
        K_IMAGE_FILE_JPG => save_jpg(&io),
        K_IMAGE_FILE_JPG2 => save_jpg2(&io),
        _ => K_ERROR_FAILED,
    }
}

/// `GESurface::GetImageFileAttributes(file, type, ...)`: `out` the width, height and depth (written only as the
/// original wrote them). BMP fails, JPG, JPG2 (its colour image's size, 32 bits), PNG; any other type fails.
///
/// # Safety
/// `host` serves the file; `out` holds three values.
#[no_mangle]
pub unsafe extern "C" fn ee_of_surf_attributes(
    host: EeOfSurfHostFn,
    file: *mut c_void,
    kind: u8,
    out: *mut [c_ulong; 3],
) -> c_long {
    let io = Io { host, surface: std::ptr::null_mut(), file };
    // SAFETY: the caller's contract
    let out = unsafe { &mut *out };
    match kind {
        K_IMAGE_FILE_JPG => jpg_attributes(&io, out),
        K_IMAGE_FILE_JPG2 => {
            let result = jpg_attributes(&io, out);
            out[2] = 32;
            result
        }
        K_IMAGE_FILE_PNG => match decode_png(&io) {
            Ok((width, height, depth, _)) => {
                *out = [width, height, depth];
                K_ERROR_OK
            }
            Err(result) => result,
        },
        _ => K_ERROR_FAILED,
    }
}

#[cfg(test)]
#[path = "surface_file_tests.rs"]
mod tests;
