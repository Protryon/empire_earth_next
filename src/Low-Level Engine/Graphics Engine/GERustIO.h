// A field stream over an FSFile for the Graphics Engine's loaders and savers in Rust (rust/crates/ee_ge io.rs,
// docs/RUST_PLAN.md R4.3): Rust decides the order, the checks and the error handling, and every field still goes
// through the file's own typed Read/Write, so the bytes, the per-class short-read rules and the position a failed
// load leaves are the C++'s. Include after lle/FSFile.h and rust/ee_rust.h.
#ifndef GERUSTIO_H
#define GERUSTIO_H

struct GERustIO {
    static long ReadU8(void* inFile, uint8_t* outValue) { return static_cast<FSFile*>(inFile)->Read(*outValue); }
    static long ReadU16(void* inFile, uint16_t* outValue) { return static_cast<FSFile*>(inFile)->Read(*outValue); }
    static long ReadU32(void* inFile, uint32_t* outValue) { return static_cast<FSFile*>(inFile)->Read(*outValue); }
    static long ReadF32(void* inFile, float* outValue) { return static_cast<FSFile*>(inFile)->Read(*outValue); }
    static long WriteU8(void* inFile, uint8_t inValue) { return static_cast<FSFile*>(inFile)->Write(inValue); }
    static long WriteU16(void* inFile, uint16_t inValue) { return static_cast<FSFile*>(inFile)->Write(inValue); }
    static long WriteU32(void* inFile, uint32_t inValue) { return static_cast<FSFile*>(inFile)->Write(inValue); }
    static long WriteF32(void* inFile, float inValue) { return static_cast<FSFile*>(inFile)->Write(inValue); }
};

inline EeGeFieldIo GERustFieldIO(FSFile& inFile)
{
    EeGeFieldIo theIO;
    theIO.file = &inFile;
    theIO.read_u8 = GERustIO::ReadU8;
    theIO.read_u16 = GERustIO::ReadU16;
    theIO.read_u32 = GERustIO::ReadU32;
    theIO.read_f32 = GERustIO::ReadF32;
    theIO.write_u8 = GERustIO::WriteU8;
    theIO.write_u16 = GERustIO::WriteU16;
    theIO.write_u32 = GERustIO::WriteU32;
    theIO.write_f32 = GERustIO::WriteF32;
    return theIO;
}

#endif
