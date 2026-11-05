// C:\EEX\Low-Level Engine\File System\FSMemoryFile.h (file name inferred): a file held in memory.
// Vtable 0x10097634 (RTTI .?AVFSMemoryFile@@ > FSFileSpec > FSBasicFile > FSFile; one new slot, +0xd8).
// Requires lle/FSFileSpec.h. Inline members are the ones emitted with File System.cpp's inlines (0x4630..0x4724)
// or ICF-folded into one-instruction bodies elsewhere.
#ifndef LLE_FSMEMORYFILE_H
#define LLE_FSMEMORYFILE_H

#include "lle/FSFileSpec.h"  // the base class

class LLE_API FSMemoryFile : public FSFileSpec {
public:
    using FSFile::Read;    // the long/unsigned long adapters (lle/FSFile.h)
    using FSFile::Write;
    // defaults proven by the default-constructor closure ??_F (0x4715: push 1; push 1; push 1000)
    FSMemoryFile(unsigned long inBlockSize = 1000, bool inGrowByDoubling = true, unsigned long inBlockCount = 1);   // 0x9ec5
    FSMemoryFile(unsigned char* inData, unsigned long inLength, unsigned long inBlockSize, bool inGrowByDoubling);   // 0x9f30
    virtual ~FSMemoryFile();                                                    // 0x9f91
    // implicit copy ctor (0x466d) and operator= (0x46c4)

    virtual long Create() { return this->mBuffer ? kErrorOK : kErrorFailed; }                     // +0x04 0x4644 (provisional names)
    virtual long Close();                                                       // +0x08 0x9fbe
    virtual long Copy(const UString& inDestination, bool inOverwrite, bool inKeepTimes);   // +0x0c 0xa176
    virtual long Delete();                                                      // +0x10 0xa315
    virtual bool GetIsOpen() const { return true; }                             // +0x24 (folded 0x590dc)
    virtual long Open(unsigned long inAccess);                                  // +0x30 0xa58b
    virtual long Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead);   // +0x64 0xa5de
    virtual long Read(char* outData, unsigned long inLength, unsigned long& outRead);            // +0x60 0xa624
    virtual long Read(uint32_t& outValue);                                      // +0x5c (folded 0xa638)
    virtual long Read(int32_t& outValue);                                       // +0x58 0xa638
    virtual long Read(unsigned short& outValue);                                // +0x54 (folded 0xa64d)
    virtual long Read(short& outValue);                                         // +0x50 0xa64d
    virtual long Read(unsigned char& outValue);                                 // +0x4c (folded 0xa662)
    virtual long Read(signed char& outValue);                                   // +0x48 0xa662
    virtual long Read(float& outValue);                                         // +0x44 (folded 0xa638)
    virtual long Read(double& outValue);                                        // +0x40 0xa677
    virtual long Read(bool& outValue);                                          // +0x3c (folded 0xa662)
    virtual long Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten);  // +0x9c 0xa754
    virtual long Write(const char* inData, unsigned long inLength, unsigned long& outWritten);           // +0x98 0xa7cc
    virtual long Write(const uint32_t& inValue);                                // +0x94 (folded 0xa7e3)
    virtual long Write(const int32_t& inValue);                                 // +0x90 0xa7e3
    virtual long Write(const unsigned short& inValue);                          // +0x8c (folded 0xa7fb)
    virtual long Write(const short& inValue);                                   // +0x88 0xa7fb
    virtual long Write(const unsigned char& inValue);                           // +0x84 (folded 0xa813)
    virtual long Write(const signed char& inValue);                             // +0x80 0xa813
    virtual long Write(const float& inValue);                                   // +0x7c (folded 0xa7e3)
    virtual long Write(const double& inValue);                                  // +0x78 0xa82b
    virtual long Write(const bool& inValue);                                    // +0x74 (folded 0xa813)

    virtual long DeleteDirectory() { return kErrorFailed; }                                // +0xa4 (0x4652)
    virtual long FOpen(char* inMode);                                           // +0xa8 0xa343
    virtual long GetFirstFile(FSFileSpec& outFile) { return kErrorFailed; }                // +0xb0 (folded 0x3903e)
    virtual long GetFirstFile(const UString& inPattern, FSFileSpec& outFile) { return kErrorFailed; }   // +0xac (0x39110)
    virtual long GetLength(unsigned long& outLength) const;                     // +0xb4 0xa45b
    virtual long GetNextFile(FSFileSpec& outFile) { return kErrorFailed; }                 // +0xb8 (folded 0x3903e)
    virtual void GetParentDirectory(FSFileSpec& outDirectory) const {}          // +0xbc (folded 0x58f8e)
    virtual long GetPosition(long& outPosition) const;                          // +0xc0 0xa46c
    virtual bool IsDirectory() const { return false; }                          // +0xc4 (folded 0x5efa4)
    virtual long Lock() { return kErrorFailed; }                                           // +0xc8 (0x4652)
    virtual long SetLength(unsigned long& ioLength, bool inTruncate) { return kErrorFailed; }   // +0xcc (folded 0x39110)
    virtual long SetPosition(long inOffset, unsigned long inOrigin, bool inExtend);   // +0xd0 0xa6ec
    virtual long Unlock() { return kErrorFailed; }                                         // +0xd4 (0x4652)
    virtual void CloseSearchHandle() {}                                         // +0xd8 (folded 0x0c16a)

    const unsigned char* GetData() const { return this->mBuffer; }             // 0x4630
    void GetTemporaryFileName(UString& outName) { outName = this->mTemporaryFileName; }  // 0x4634
    long Compact();                                                             // 0x9ff6
    long Compress(unsigned long inMethod, unsigned long& outLength);            // 0xa05e (provisional names)
    long Decompress(unsigned long& outLength);                                  // 0xa20b
    long Load(FSFile& inFile, unsigned long inLength);                          // 0xa4e9
    long Load(FSFile& inFile);                                                  // 0xa55c
    long Save(FSFile& outFile, unsigned long inLength);                         // 0xa68c
    long Save(FSFile& outFile);                                                 // 0xa6ab

protected:
    void ClearData()                                                            // 0x4656
    {
        if (this->mBuffer)
            delete[] this->mBuffer;
        this->mBuffer = NULL;
    }
    void IncreaseFileSize(unsigned long inLength);                              // 0xa492

    UString mTemporaryFileName;                       // +0x48
    unsigned char* mBuffer;                           // +0x58: the allocation (delete[]); GetData()
    unsigned char* mStart;                            // +0x5c: first data byte
    unsigned char* mEnd;                              // +0x60: one past the last data byte (length = end - start)
    unsigned char* mLimit;                            // +0x64: one past the allocation
    unsigned char* mCurrent;                          // +0x68: read/write position
    unsigned long mAllocatedLength;                   // +0x6c
    unsigned long mBlockSize;                         // +0x70 (0 -> 1)
    bool mGrowByDoubling;                             // +0x74: Write grows by max(allocation, 2 * length) instead of blocks
};

#endif
