// C:\EEX\Low-Level Engine\File System\FSFile.h (file name inferred): the abstract file interface.
// Requires LLE_API (lle/LowLevelEngineCore.h).
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
#include "rust/base/lle_enums.h"
#include "rust/base/fs.h"

#include <stdint.h>
#include <stdio.h>
#include "platform/PlatFile.h"
#include "lle/UString.h"       // their Load and Save (below)
#include "lle/UWideString.h"

static_assert(sizeof(short) == 2 && sizeof(float) == 4 && sizeof(double) == 8 && sizeof(bool) == 1,
              "FSFile value widths");

// (Open()'s access modes are FSFileAccess, rust/base/lle_enums.h: kFileAccessRead 1, kFileAccessWrite 2; the original's
// assert texts spelled them kfsaRead and kfsaWrite.)

#include "lle/LLEForward.h"

// The file classes are one class (LLE round 12): FSFile holds what FSBasicFile (the names, access, version, open flag)
// and FSFileSpec (a file on disk: the platform handle and stream) had, its members whole in Rust
// (rust/crates/ee_base/src/fs/spec.rs) over the object from its vtable pointer on (EeFsSpec; the layout is pinned in
// File System.cpp). FSBasicFile and FSFileSpec keep their constructors; FSMemoryFile overrides the virtuals. The
// original's FSFile was abstract (every slot pure), its slot order the comments' offsets.
class LLE_API FSFile {
public:
    virtual ~FSFile() {}                                                            // +0x00 (0x1282)
    virtual long Delete() { return ee_fs_basic_delete(Rec()); }   // +0x10
    virtual void GetDirectory(UString& outDirectory) const { outDirectory = this->mDirectory; }   // +0x18
    virtual const UString& GetDirectory() const { return this->mDirectory; }                       // +0x14
    virtual void GetFile(UString& outFile) const { outFile = this->mFile; }                       // +0x20
    virtual const UString& GetFile() const { return this->mFile; }                                 // +0x1c
    virtual void GetVolume(UString& outVolume) const { outVolume = this->mVolume; }               // +0x2c
    virtual const UString& GetVolume() const { return this->mVolume; }                             // +0x28
    virtual void SetDirectory(const UString& inDirectory) { this->mDirectory = inDirectory; }     // +0x68
    virtual void SetFile(const UString& inFile) { this->mFile = inFile; }                         // +0x6c
    virtual void SetVolume(const UString& inVolume) { this->mVolume = inVolume; }                 // +0x70

    unsigned long GetFileAccess() const { return this->mFileAccess; }  // inline: GESurface/GETexture read +0x34 directly; export folded 0x4c365
    unsigned long GetVersion() const { return this->mVersion; }
    void SetVersion(unsigned long inVersion) { this->mVersion = inVersion; }

    // (the vtable slots are the original's)
    virtual long Close() { return ee_fs_spec_close(Rec()); }                    // +0x08
    void CloseSearchHandle() { ee_fs_spec_close_search(Rec()); }
    virtual long Copy(const UString& inDestination, bool inOverwrite, bool inKeepTimes)   // +0x0c
        { return ee_fs_spec_copy(Rec(), inDestination, inOverwrite, inKeepTimes); }
    virtual long Create() { return ee_fs_spec_create(Rec()); }                  // +0x04
    virtual long CreateDirectory() { return ee_fs_spec_create_directory(Rec()); }   // +0xa0
    virtual long DeleteDirectory() { return ee_fs_spec_delete_directory(Rec()); }   // +0xa4
    virtual long FOpen(char* inMode) { return ee_fs_spec_fopen(Rec(), inMode); }   // +0xa8
    void* GetFileHandle() const { return this->mHandle; }  // inline (folded 0x11cc9)
    virtual long GetFirstFile(FSFileSpec& outFile) { return this->GetFirstFile(UString("*.*"), outFile); }   // +0xb0 0x13e8 (inline)
    virtual long GetFirstFile(const UString& inPattern, FSFileSpec& outFile)   // +0xac
    {
        return ee_fs_spec_first_file(Rec(), inPattern, Spec(&outFile), Startup(&gStartupVolume),
                                     Startup(&gStartupDirectory));
    }
    virtual bool GetIsOpen() const { return this->mIsOpen; }                   // +0x24
    virtual long GetLength(unsigned long& outLength) const { return ee_fs_spec_get_length(Rec(), &outLength); }   // +0xb4
    virtual long GetNextFile(FSFileSpec& outFile) { return ee_fs_spec_next_file(Rec(), Spec(&outFile)); }   // +0xb8
    virtual void GetParentDirectory(FSFileSpec& outDirectory) const { ee_fs_spec_parent(Rec(), Spec(&outDirectory)); }   // +0xbc
    virtual long GetPosition(long& outPosition) const { return ee_fs_spec_get_position(Rec(), &outPosition); }   // +0xc0
    virtual bool IsDirectory() const { return ee_fs_spec_is_directory(Rec()); }   // +0xc4
    virtual long Lock() { return ee_fs_spec_lock(Rec()); }                      // +0xc8
    virtual long Open(unsigned long inAccess) { return ee_fs_spec_open(Rec(), inAccess); }   // +0x30
    virtual long Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead)   // +0x64
        { return ee_fs_spec_read(Rec(), outData, inLength, &outRead); }
    virtual long Read(char* outData, unsigned long inLength, unsigned long& outRead)             // +0x60 0x1420
        { return this->Read(reinterpret_cast<unsigned char*>(outData), inLength, outRead); }
    // the scalar reads: kErrorFailed on a short read
    virtual long Read(uint32_t& outValue) { return this->ReadValue(outValue); }          // +0x5c
    virtual long Read(int32_t& outValue) { return this->ReadValue(outValue); }           // +0x58
    virtual long Read(unsigned short& outValue) { return this->ReadValue(outValue); }    // +0x54
    virtual long Read(short& outValue) { return this->ReadValue(outValue); }             // +0x50
    virtual long Read(unsigned char& outValue) { return this->ReadValue(outValue); }     // +0x4c
    virtual long Read(signed char& outValue) { return this->ReadValue(outValue); }       // +0x48
    virtual long Read(float& outValue) { return this->ReadValue(outValue); }             // +0x44
    virtual long Read(double& outValue) { return this->ReadValue(outValue); }            // +0x40
    virtual long Read(bool& outValue) { return this->ReadValue(outValue); }              // +0x3c
    virtual long SetLength(unsigned long& ioLength, bool inTruncate) { return ee_fs_spec_set_length(Rec(), ioLength); }   // +0xcc
    virtual long SetPosition(long inOffset, unsigned long inOrigin, bool inExtend)   // +0xd0
        { return ee_fs_spec_set_position(Rec(), inOffset, inOrigin, inExtend); }
    virtual long Unlock() { return ee_fs_spec_unlock(Rec()); }                  // +0xd4
    virtual long Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten)   // +0x9c
        { return ee_fs_spec_write(Rec(), inData, inLength, &outWritten); }
    virtual long Write(const char* inData, unsigned long inLength, unsigned long& outWritten)           // +0x98 0x1434
        { return this->Write(reinterpret_cast<const unsigned char*>(inData), inLength, outWritten); }
    virtual long Write(const uint32_t& inValue) { return this->WriteValue(inValue); }          // +0x94
    virtual long Write(const int32_t& inValue) { return this->WriteValue(inValue); }           // +0x90
    virtual long Write(const unsigned short& inValue) { return this->WriteValue(inValue); }    // +0x8c
    virtual long Write(const short& inValue) { return this->WriteValue(inValue); }             // +0x88
    virtual long Write(const unsigned char& inValue) { return this->WriteValue(inValue); }     // +0x84
    virtual long Write(const signed char& inValue) { return this->WriteValue(inValue); }       // +0x80
    virtual long Write(const float& inValue) { return this->WriteValue(inValue); }             // +0x7c
    virtual long Write(const double& inValue) { return this->WriteValue(inValue); }            // +0x78
    virtual long Write(const bool& inValue) { return this->WriteValue(inValue); }              // +0x74
    operator FILE*() const { return this->mStream; }  // exported ??BFSFileSpec@@QBEPAU_iobuf@@XZ (inline; ESFilterDebugOutput)

    long Read(long& outValue) { return ee_fs_read_long(this, &outValue); }   // (ee_base fs cppfile.rs)
    long Read(unsigned long& outValue) { return ee_fs_read_ulong(this, &outValue); }
    long Write(const long& inValue) { return this->Write(static_cast<int32_t>(inValue)); }
    long Write(const unsigned long& inValue) { return this->Write(static_cast<uint32_t>(inValue)); }

    FSFile() : mVolume(256), mDirectory(256), mFile(256) { ee_fs_basic_init(Rec()); }
    // the names copied; access, version and the open flag reset; no handle or stream
    FSFile(const FSFile& inFile) : mHandle(NULL), mStream(NULL) { ee_fs_basic_init_copy(Rec(), inFile.Rec()); }
    EeFsSpec* Rec() { return reinterpret_cast<EeFsSpec*>(this); }
    const EeFsSpec* Rec() const { return reinterpret_cast<const EeFsSpec*>(this); }
    static EeFsSpec* Spec(FSFileSpec* inSpec) { return reinterpret_cast<EeFsSpec*>(inSpec); }
    static const EeFsString* Startup(const UString* inPath) { return reinterpret_cast<const EeFsString*>(inPath); }
    template <typename T> long ReadValue(T& outValue)
        { return ee_fs_spec_read_value(Rec(), reinterpret_cast<uint8_t*>(&outValue), sizeof(outValue)); }
    template <typename T> long WriteValue(const T& inValue)
        { return ee_fs_spec_write_value(Rec(), reinterpret_cast<const uint8_t*>(&inValue), sizeof(inValue)); }

    UString mVolume;                                  // +0x04
    UString mDirectory;                               // +0x14
    UString mFile;                                    // +0x24
    unsigned long mFileAccess;                        // +0x34: Open(inAccess) stores it; 1 = read
    unsigned long mVersion;                           // +0x38
    bool mIsOpen;                                     // +0x3c
    void* mHandle;                                    // +0x40: the open PlatFile*, or a search's PlatFind*
                                                      // (a Win32 HANDLE in the original)
    FILE* mStream;                                    // +0x44: FOpen's fopen stream (Close: fclose)
};


#endif
