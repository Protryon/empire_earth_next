// C:\EEX\Low-Level Engine\File System\FSMemoryFile.h (file name inferred): a file held in memory.
// Vtable 0x10097634 (RTTI .?AVFSMemoryFile@@ > FSFileSpec > FSBasicFile > FSFile; one new slot, +0xd8).
// Requires lle/FSFileSpec.h. Inline members are the ones emitted with File System.cpp's inlines (0x4630..0x4724)
// or ICF-folded into one-instruction bodies elsewhere.
#ifndef LLE_FSMEMORYFILE_H
#define LLE_FSMEMORYFILE_H

#include "lle/FSFileSpec.h"  // the base class

struct EeFsMemFile;          // rust/crates/ee_base/src/fs: the contents and position

// The contents and the position are Rust's (rust/crates/ee_base/src/fs/memfile.rs): every member forwards to it but
// the stdio temporary file (Close, Copy, FOpen: File System.cpp).
class LLE_API FSMemoryFile : public FSFileSpec {
public:
    using FSFile::Read;    // the long/unsigned long adapters (lle/FSFile.h)
    using FSFile::Write;
    // an open, empty memory file with room for inBlockCount blocks, growing by blocks or by doubling (defaults proven
    // by the default-constructor closure ??_F 0x4715: push 1; push 1; push 1000)
    FSMemoryFile(unsigned long inBlockSize = 1000, bool inGrowByDoubling = true, unsigned long inBlockCount = 1)
        : mState(ee_fs_mem_new(inBlockSize, inGrowByDoubling, inBlockCount)), mOwnsState(true)
    {
        this->mTemporaryFileName = "";
    }
    // an open memory file holding the caller's buffer, which is copied and freed (the caller gave it up)
    FSMemoryFile(unsigned char* inData, unsigned long inLength, unsigned long inBlockSize, bool inGrowByDoubling)
        : mState(ee_fs_mem_new_over(inData, inLength, inBlockSize, inGrowByDoubling)), mOwnsState(true)
    {
        this->mTemporaryFileName = "";
        delete[] inData;
    }
    // over a state made by Rust (an archive member, a chunk's payload); inOwns: the destructor frees it
    FSMemoryFile(EeFsMemFile* inState, bool inOwns) : mState(inState), mOwnsState(inOwns)
    {
        this->mTemporaryFileName = "";
    }
    virtual ~FSMemoryFile()
    {
        if (this->mOwnsState) {
            this->Close();
            ee_fs_mem_free(this->mState);
        }
        this->mState = NULL;
    }
    // implicit copy ctor (0x466d) and operator= (0x46c4) dropped: they copied the raw buffer pointer (double delete)
    FSMemoryFile(const FSMemoryFile&) = delete;
    FSMemoryFile& operator=(const FSMemoryFile&) = delete;

    virtual long Create() { return this->GetData() ? kErrorOK : kErrorFailed; }   // +0x04 0x4644 (provisional names)
    virtual long Close();                                                       // +0x08 0x9fbe (and the temporary file)
    virtual long Copy(const UString& inDestination, bool inOverwrite, bool inKeepTimes);   // +0x0c 0xa176 (to a disk file)
    virtual long Delete() { return ee_fs_mem_delete(this->mState); }            // +0x10 0xa315 (empty and closed)
    virtual bool GetIsOpen() const { return true; }                             // +0x24 (folded 0x590dc)
    virtual long Open(unsigned long inAccess) { return ee_fs_mem_open(this->mState); }   // +0x30 0xa58b
    // inLength bytes from the position; kErrorEndOfFile (nothing read) if fewer are left
    virtual long Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead)   // +0x64 0xa5de
    {
        uint32_t theRead = (uint32_t)outRead;
        long theResult = ee_fs_mem_read(this->mState, outData, inLength, &theRead);
        outRead = theRead;
        return theResult;
    }
    virtual long Read(char* outData, unsigned long inLength, unsigned long& outRead)            // +0x60 0xa624
        { return this->Read(reinterpret_cast<unsigned char*>(outData), inLength, outRead); }
    virtual long Read(uint32_t& outValue) { return this->MemRead(outValue); }             // +0x5c (folded 0xa638)
    virtual long Read(int32_t& outValue) { return this->MemRead(outValue); }              // +0x58 0xa638
    virtual long Read(unsigned short& outValue) { return this->MemRead(outValue); }       // +0x54 (folded 0xa64d)
    virtual long Read(short& outValue) { return this->MemRead(outValue); }                // +0x50 0xa64d
    virtual long Read(unsigned char& outValue) { return this->MemRead(outValue); }        // +0x4c (folded 0xa662)
    virtual long Read(signed char& outValue) { return this->MemRead(outValue); }          // +0x48 0xa662
    virtual long Read(float& outValue) { return this->MemRead(outValue); }                // +0x44 (folded 0xa638)
    virtual long Read(double& outValue) { return this->MemRead(outValue); }               // +0x40 0xa677
    virtual long Read(bool& outValue) { return this->MemRead(outValue); }                 // +0x3c (folded 0xa662)
    // bytes at the position, growing the buffer by a block or by doubling; kErrorNotOpen when closed
    virtual long Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten)  // +0x9c 0xa754
    {
        uint32_t theWritten = 0;
        long theResult = ee_fs_mem_write(this->mState, inData, inLength, &theWritten);
        outWritten = theWritten;
        return theResult;
    }
    virtual long Write(const char* inData, unsigned long inLength, unsigned long& outWritten)           // +0x98 0xa7cc
        { return this->Write(reinterpret_cast<const unsigned char*>(inData), inLength, outWritten); }
    virtual long Write(const uint32_t& inValue) { return this->MemWrite(inValue); }       // +0x94 (folded 0xa7e3)
    virtual long Write(const int32_t& inValue) { return this->MemWrite(inValue); }        // +0x90 0xa7e3
    virtual long Write(const unsigned short& inValue) { return this->MemWrite(inValue); } // +0x8c (folded 0xa7fb)
    virtual long Write(const short& inValue) { return this->MemWrite(inValue); }          // +0x88 0xa7fb
    virtual long Write(const unsigned char& inValue) { return this->MemWrite(inValue); }  // +0x84 (folded 0xa813)
    virtual long Write(const signed char& inValue) { return this->MemWrite(inValue); }    // +0x80 0xa813
    virtual long Write(const float& inValue) { return this->MemWrite(inValue); }          // +0x7c (folded 0xa7e3)
    virtual long Write(const double& inValue) { return this->MemWrite(inValue); }         // +0x78 0xa82b
    virtual long Write(const bool& inValue) { return this->MemWrite(inValue); }           // +0x74 (folded 0xa813)

    virtual long DeleteDirectory() { return kErrorFailed; }                                // +0xa4 (0x4652)
    virtual long FOpen(char* inMode);                                           // +0xa8 0xa343 (a temporary disk copy)
    virtual long GetFirstFile(FSFileSpec& outFile) { return kErrorFailed; }                // +0xb0 (folded 0x3903e)
    virtual long GetFirstFile(const UString& inPattern, FSFileSpec& outFile) { return kErrorFailed; }   // +0xac (0x39110)
    virtual long GetLength(unsigned long& outLength) const                      // +0xb4 0xa45b (the written length)
    {
        outLength = ee_fs_mem_length(this->mState);
        return kErrorOK;
    }
    virtual long GetNextFile(FSFileSpec& outFile) { return kErrorFailed; }                 // +0xb8 (folded 0x3903e)
    virtual void GetParentDirectory(FSFileSpec& outDirectory) const {}          // +0xbc (folded 0x58f8e)
    virtual long GetPosition(long& outPosition) const                           // +0xc0 0xa46c (-1 closed or empty)
    {
        outPosition = ee_fs_mem_position(this->mState);
        return kErrorOK;
    }
    virtual bool IsDirectory() const { return false; }                          // +0xc4 (folded 0x5efa4)
    virtual long Lock() { return kErrorFailed; }                                           // +0xc8 (0x4652)
    virtual long SetLength(unsigned long& ioLength, bool inTruncate) { return kErrorFailed; }   // +0xcc (folded 0x39110)
    // within the written data (from the end it counts backwards); never past the end
    virtual long SetPosition(long inOffset, unsigned long inOrigin, bool inExtend)   // +0xd0 0xa6ec
        { return ee_fs_mem_set_position(this->mState, inOffset, inOrigin); }
    virtual long Unlock() { return kErrorFailed; }                                         // +0xd4 (0x4652)
    virtual void CloseSearchHandle() {}                                         // +0xd8 (folded 0x0c16a)

    const unsigned char* GetData() const { return ee_fs_mem_data(this->mState); }
    // shrinks the buffer to the written length and rewinds; kErrorNotOpen when closed
    long Compact() { return ee_fs_mem_compact(this->mState); }
    // compresses the contents in place (only above 12 bytes) and closes the file; an incompressible result (0x16)
    // keeps the data and succeeds
    long Compress(unsigned long inLevel, unsigned long& outLength)
    {
        uint32_t theLength = 0;
        long theResult = ee_fs_mem_compress(this->mState, (uint32_t)inLevel, &theLength);
        outLength = theLength;
        if (theResult == 0x15)
            ESVERIFY_AT(NULL == "PKWARE implode() error!", 140);
        return theResult;
    }
    // the contents from inLength bytes of another file (on failure empty and closed), or from a length-prefixed block
    long Load(FSFile& inFile, unsigned long inLength)
    {
        EeFsFileOps theOps = Ops(inFile);
        return ee_fs_mem_load(this->mState, &theOps, inLength);
    }
    long Load(FSFile& inFile)
    {
        EeFsFileOps theOps = Ops(inFile);
        return ee_fs_mem_load_prefixed(this->mState, &theOps);
    }
    // the first inLength bytes to another file, or the length and then the compacted contents (the second write's result)
    long Save(FSFile& outFile, unsigned long inLength)
    {
        EeFsFileOps theOps = Ops(outFile);
        return ee_fs_mem_save(this->mState, &theOps, inLength);
    }
    long Save(FSFile& outFile)
    {
        EeFsFileOps theOps = Ops(outFile);
        return ee_fs_mem_save_prefixed(this->mState, &theOps);
    }

    // the table of an FSFile's reads and writes that the Rust File System calls (also FSChunk's)
    static EeFsFileOps Ops(FSFile& inFile)
    {
        EeFsFileOps theOps;
        theOps.context = &inFile;
        theOps.read_u32 = [](void* c, uint32_t* v) -> int32_t { return (int32_t) static_cast<FSFile*>(c)->Read(*v); };
        theOps.read_bytes = [](void* c, uint8_t* d, size_t n, uint32_t* r) -> int32_t {
            unsigned long theRead = 0;
            int32_t theResult = (int32_t) static_cast<FSFile*>(c)->Read(d, (unsigned long)n, theRead);
            *r = (uint32_t)theRead;
            return theResult;
        };
        theOps.write_u32 = [](void* c, uint32_t v) -> int32_t { return (int32_t) static_cast<FSFile*>(c)->Write(v); };
        theOps.write_bytes = [](void* c, const uint8_t* d, size_t n, uint32_t* w) -> int32_t {
            unsigned long theWritten = 0;
            int32_t theResult = (int32_t) static_cast<FSFile*>(c)->Write(d, (unsigned long)n, theWritten);
            *w = (uint32_t)theWritten;
            return theResult;
        };
        return theOps;
    }

protected:
    template <typename T> long MemRead(T& outValue)
    {
        unsigned long theRead;
        return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
    }
    template <typename T> long MemWrite(const T& inValue)
    {
        unsigned long theWritten;
        return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
    }

    UString mTemporaryFileName;                       // +0x48
    EeFsMemFile* mState;                              // the contents (rust/crates/ee_base/src/fs/memfile.rs)
    bool mOwnsState;                                  // false for the view a chunk wraps around its payload
};

#endif
