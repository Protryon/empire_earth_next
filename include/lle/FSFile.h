// C:\EEX\Low-Level Engine\File System\FSFile.h (file name inferred): the abstract file interface.
// Requires UString (lle/LowLevelEngine.h includes this after it).
//
// Vtable 0x10097390 (RTTI .?AVFSFile@@): every slot pure (_purecall) except Pop/PushIndentHint (an empty inline
// body, ICF-folded at 0x1000c16a). VC6 lays overloaded virtuals out in REVERSE declaration order
// (probe: tools/rtti.py + a compiled check), so each overload group below is declared in the reverse of its slot
// order: e.g. Read(unsigned char*,...) is declared first and sits last (+0x64).
//
// Value widths in a file are fixed on every data model (LP64 too, where long is 8 bytes): 4 bytes for the
// int32_t/uint32_t overloads and for long/unsigned long (non-virtual adapters below: a long is written as its low
// 32 bits and read back sign-extended, an unsigned long zero-extended), 2 for short, 1 for char and bool, 4/8 for
// float/double. Saved games, record files and network buffers are the same bytes on every platform.
#ifndef LLE_FSFILE_H
#define LLE_FSFILE_H

#include <stdint.h>

static_assert(sizeof(short) == 2 && sizeof(float) == 4 && sizeof(double) == 8 && sizeof(bool) == 1,
              "FSFile value widths");

// Open() access modes (names from NERecordPlayback.cpp's assert texts; values from its calls: Open(1) to read a
// playback file, Open(2) to write a record file; FSBasicFile +0x34 stores the mode)
enum FSFileAccess {
    kFileAccessRead = 1,
    kfsaRead = kFileAccessRead,  // spelled this way in the original's assert strings
    kFileAccessWrite = 2,
    kfsaWrite = kFileAccessWrite,  // spelled this way in the original's assert strings
    kFileAccessReadWrite = 3,  // FSFileSpec::Open maps modes other than 1/2/4 to GENERIC_READ|GENERIC_WRITE; Read/Write test the 1/2 bits; GESurface::SaveAsJPG2 reopens with 3 (provisional name)
    kFileAccessAppend = 4,   // FSFileSpec::Open: 4 -> GENERIC_WRITE, then SetFilePointer(FILE_END) when inAccess & 4; GESurface::SaveAsJPG opens with 4 (provisional name)
};

class LLE_API FSFile {
public:
    // user-declared (public, exported ??0FSFile@@QAE@XZ 0x1279): the first unit emits it BEFORE the dtor (0x1282), and
    // the implicit copy ctor (0x1289) after the declared members
    FSFile() {}
    virtual ~FSFile() {}                                                            // +0x00 (0x1282)
    virtual long Create() = 0;                                                      // +0x04
    virtual long Close() = 0;                                                       // +0x08
    virtual long Copy(const UString& inDestination, bool inOverwrite, bool inKeepTimes) = 0;   // +0x0c (provisional names)
    virtual long Delete() = 0;                                                      // +0x10
    virtual void GetDirectory(UString& outDirectory) const = 0;                     // +0x18
    virtual const UString& GetDirectory() const = 0;                                // +0x14
    virtual void GetFile(UString& outFile) const = 0;                               // +0x20
    virtual const UString& GetFile() const = 0;                                     // +0x1c
    virtual bool GetIsOpen() const = 0;                                             // +0x24
    virtual void GetVolume(UString& outVolume) const = 0;                           // +0x2c
    virtual const UString& GetVolume() const = 0;                                   // +0x28
    virtual long Open(unsigned long inAccess) = 0;                                  // +0x30
    virtual void PopIndentHint() {}                                                 // +0x34
    virtual void PushIndentHint() {}                                                // +0x38
    virtual long Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead) = 0;   // +0x64
    virtual long Read(char* outData, unsigned long inLength, unsigned long& outRead) = 0;            // +0x60
    virtual long Read(uint32_t& outValue) = 0;                                      // +0x5c
    virtual long Read(int32_t& outValue) = 0;                                       // +0x58
    virtual long Read(unsigned short& outValue) = 0;                                // +0x54
    virtual long Read(short& outValue) = 0;                                         // +0x50
    virtual long Read(unsigned char& outValue) = 0;                                 // +0x4c
    virtual long Read(signed char& outValue) = 0;                                   // +0x48
    virtual long Read(float& outValue) = 0;                                         // +0x44
    virtual long Read(double& outValue) = 0;                                        // +0x40
    virtual long Read(bool& outValue) = 0;                                          // +0x3c
    virtual void SetDirectory(const UString& inDirectory) = 0;                      // +0x68
    virtual void SetFile(const UString& inFile) = 0;                                // +0x6c
    virtual void SetVolume(const UString& inVolume) = 0;                            // +0x70
    virtual long Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten) = 0;  // +0x9c
    virtual long Write(const char* inData, unsigned long inLength, unsigned long& outWritten) = 0;           // +0x98
    virtual long Write(const uint32_t& inValue) = 0;                                // +0x94
    virtual long Write(const int32_t& inValue) = 0;                                 // +0x90
    virtual long Write(const unsigned short& inValue) = 0;                          // +0x8c
    virtual long Write(const short& inValue) = 0;                                   // +0x88
    virtual long Write(const unsigned char& inValue) = 0;                           // +0x84
    virtual long Write(const signed char& inValue) = 0;                             // +0x80
    virtual long Write(const float& inValue) = 0;                                   // +0x7c
    virtual long Write(const double& inValue) = 0;                                  // +0x78
    virtual long Write(const bool& inValue) = 0;                                    // +0x74

    // long/unsigned long: 4 bytes in the file on every data model (see the top). The value goes through a 32-bit
    // copy that starts as the old value and is stored back even when the read fails, so a short read leaves what
    // the direct 4-byte read left on Windows. Derived classes bring these in with `using FSFile::Read/Write`.
    long Read(long& outValue)
    {
        int32_t theValue = static_cast<int32_t>(outValue);
        long theResult = this->Read(theValue);
        outValue = theValue;
        return theResult;
    }
    long Read(unsigned long& outValue)
    {
        uint32_t theValue = static_cast<uint32_t>(outValue);
        long theResult = this->Read(theValue);
        outValue = theValue;
        return theResult;
    }
    long Write(const long& inValue) { return this->Write(static_cast<int32_t>(inValue)); }
    long Write(const unsigned long& inValue) { return this->Write(static_cast<uint32_t>(inValue)); }
};

// FSFile::SetPosition inOrigin: FSFileSpec::SetPosition maps it to the SetFilePointer move method
enum FSFileOrigin {
    kFileOriginBegin = 0,  // FSFileSpec::SetPosition -> FILE_BEGIN
    kFileOriginEnd = 1,    // FSFileSpec::SetPosition -> FILE_END
    kFileOriginCurrent = 2,  // FSFileSpec::SetPosition -> FILE_CURRENT
};

#endif
