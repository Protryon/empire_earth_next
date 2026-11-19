// C:\EEX\Low-Level Engine\File System\FSFileSpec.h (file name inferred): a file on disk (platform/PlatFile.h).
// Vtable 0x10097720 (RTTI .?AVFSFileSpec@@ > FSBasicFile > FSFile); sizeof 0x48 (Default.dll stack local).
// Requires lle/FSBasicFile.h and <windows.h> first: CreateDirectory is exported as ?CreateDirectoryA@ (A/W macro).
// Whole in Rust (rust/crates/ee_base/src/fs/spec.rs, LLE lane round 5): each member hands the object, from its vtable
// pointer on (EeFsSpec; the layout is pinned in File System.cpp), to Rust, which makes the original's platform calls
// in its order. operator< and GetLengthFromClosedFile stay C++: they call the virtuals FSMemoryFile overrides.
#ifndef LLE_FSFILESPEC_H
#define LLE_FSFILESPEC_H

#include <stdio.h>
#include "platform/PlatFile.h"
#include "lle/FSBasicFile.h" // the base class

class LLE_API FSFileSpec : public FSBasicFile {
public:
    using FSFile::Read;    // the long/unsigned long adapters (lle/FSFile.h)
    using FSFile::Write;
    // closed, for the startup directory with no file name
    FSFileSpec() { ee_fs_spec_init(Rec(), Startup(&gStartupVolume), Startup(&gStartupDirectory)); }
    // the path copied; the copy is closed and has no search handle
    FSFileSpec(const FSFileSpec& inSpec) : FSBasicFile(inSpec), mHandle(NULL), mStream(NULL) {}
    // closed, from a full path split into volume, directory and file name
    FSFileSpec(const UString& inPath)
    {
        ee_fs_spec_init_path(Rec(), reinterpret_cast<const EeFsString*>(&inPath), Startup(&gStartupVolume),
                             Startup(&gStartupDirectory));
    }
    virtual ~FSFileSpec() { ee_fs_spec_finish(Rec()); }   // closes the file or an open directory search
    // the path copied; this one is left closed with no search handle
    FSFileSpec& operator=(const FSFileSpec& inSpec) { ee_fs_spec_assign(Rec(), inSpec.Rec()); return *this; }
    // whether this file was last written before the other one, opening either as needed (true on failure)
    bool operator<(FSFileSpec& inSpec)                // (non-const parameter, as exported)
    {
        bool theOpenedThis = this->mHandle == NULL && this->Open(1) == kErrorOK;
        bool theOpenedOther = inSpec.mHandle == NULL && inSpec.Open(1) == kErrorOK;
        bool theResult = true;
        if (this->mHandle && inSpec.mHandle) {
            uint64_t theThisTime = 0;
            uint64_t theOtherTime = 0;
            PlatFileGetWriteTime(static_cast<PlatFile*>(this->mHandle), &theThisTime);
            PlatFileGetWriteTime(static_cast<PlatFile*>(inSpec.mHandle), &theOtherTime);
            theResult = theThisTime < theOtherTime;
        }
        if (theOpenedThis)
            this->Close();
        if (theOpenedOther)
            inSpec.Close();
        return theResult;
    }

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
    virtual long GetFirstFile(FSFileSpec& outFile)                              // +0xb0 0x13e8 (inline)
    {
        UString thePattern("*.*");
        return this->GetFirstFile(thePattern, outFile);
    }
    virtual long GetFirstFile(const UString& inPattern, FSFileSpec& outFile)   // +0xac
    {
        return ee_fs_spec_first_file(Rec(), inPattern, outFile.Rec(), Startup(&gStartupVolume),
                                     Startup(&gStartupDirectory));
    }
    virtual bool GetIsOpen() const { return this->mIsOpen; }                   // +0x24
    virtual long GetLength(unsigned long& outLength) const { return ee_fs_spec_get_length(Rec(), &outLength); }   // +0xb4
    // opens the file just to read its length, then closes it; OK even when it cannot open it (length 0; fixed:
    // outLength used to be left unset there)
    long GetLengthFromClosedFile(unsigned long& outLength)
    {
        unsigned long theLength = 0;
        if (this->Open(kFileAccessRead) == kErrorOK) {
            this->GetLength(theLength);
            this->Close();
        }
        outLength = theLength;
        return kErrorOK;
    }
    virtual long GetNextFile(FSFileSpec& outFile) { return ee_fs_spec_next_file(Rec(), outFile.Rec()); }   // +0xb8
    virtual void GetParentDirectory(FSFileSpec& outDirectory) const { ee_fs_spec_parent(Rec(), outDirectory.Rec()); }   // +0xbc
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

protected:
    EeFsSpec* Rec() { return reinterpret_cast<EeFsSpec*>(this); }
    const EeFsSpec* Rec() const { return reinterpret_cast<const EeFsSpec*>(this); }
    static const EeFsString* Startup(const UString* inPath) { return reinterpret_cast<const EeFsString*>(inPath); }
    template <typename T> long ReadValue(T& outValue)
        { return ee_fs_spec_read_value(Rec(), reinterpret_cast<uint8_t*>(&outValue), sizeof(outValue)); }
    template <typename T> long WriteValue(const T& inValue)
        { return ee_fs_spec_write_value(Rec(), reinterpret_cast<const uint8_t*>(&inValue), sizeof(inValue)); }

    void* mHandle;                                    // +0x40: the open PlatFile*, or a search's PlatFind*
                                                      // (a Win32 HANDLE in the original)
    FILE* mStream;                                    // +0x44: FOpen's fopen stream (Close: fclose)
};

#endif
