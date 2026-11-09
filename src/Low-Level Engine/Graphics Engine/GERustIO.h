// A field stream over an FSFile for the Graphics Engine's loaders and savers in Rust (rust/crates/ee_ge io.rs,
// docs/RUST_PLAN.md R4.3): Rust decides the order, the checks and the error handling, and every field still goes
// through the file's own typed Read/Write, so the bytes, the per-class short-read rules and the position a failed
// load leaves are the C++'s. Include after lle/FSFile.h (and lle/FSFileSpec.h for GERustFieldIO(FSFileSpec&), which
// adds the position) and rust/ee_rust.h.
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
    static long ReadBytes(void* inFile, uint8_t* outData, uint32_t inLength, uint32_t* outRead)
    {
        unsigned long theRead = *outRead;
        long theResult = static_cast<FSFile*>(inFile)->Read(reinterpret_cast<char*>(outData), inLength, theRead);
        *outRead = (uint32_t)theRead;
        return theResult;
    }
    static long WriteBytes(void* inFile, const uint8_t* inData, uint32_t inLength, uint32_t* outWritten)
    {
        unsigned long theWritten = *outWritten;
        long theResult =
            static_cast<FSFile*>(inFile)->Write(reinterpret_cast<const char*>(inData), inLength, theWritten);
        *outWritten = (uint32_t)theWritten;
        return theResult;
    }
    // a plain FSFile has no position
    static long NoPosition(void*, long*) { return kErrorFailed; }
    static long NoSetPosition(void*, long) { return kErrorFailed; }
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
    theIO.read_bytes = GERustIO::ReadBytes;
    theIO.write_bytes = GERustIO::WriteBytes;
    theIO.get_position = GERustIO::NoPosition;
    theIO.set_position = GERustIO::NoSetPosition;
    return theIO;
}

#ifdef LLE_FSFILESPEC_H
// The stream's file is the FSFile part of an FSFileSpec (GERustFieldIO(FSFileSpec&) below).
struct GERustSpecIO {
    static long GetPosition(void* inFile, long* outPosition)
        { return static_cast<FSFileSpec*>(static_cast<FSFile*>(inFile))->GetPosition(*outPosition); }
    static long SetPosition(void* inFile, long inPosition)
    {
        return static_cast<FSFileSpec*>(static_cast<FSFile*>(inFile))->SetPosition(inPosition, kFileOriginBegin,
                                                                                    false);
    }
};

inline EeGeFieldIo GERustFieldIO(FSFileSpec& inFile)
{
    EeGeFieldIo theIO = GERustFieldIO(static_cast<FSFile&>(inFile));
    theIO.get_position = GERustSpecIO::GetPosition;
    theIO.set_position = GERustSpecIO::SetPosition;
    return theIO;
}
#endif

#endif
