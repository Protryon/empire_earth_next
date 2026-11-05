// C:\EEX\Low-Level Engine\File System\FSFileSpec.h (file name inferred): a file on disk (platform/PlatFile.h).
// Vtable 0x10097720 (RTTI .?AVFSFileSpec@@ > FSBasicFile > FSFile); sizeof 0x48 (Default.dll stack local).
// Requires lle/FSBasicFile.h and <windows.h> first: CreateDirectory is exported as ?CreateDirectoryA@ (A/W macro).
#ifndef LLE_FSFILESPEC_H
#define LLE_FSFILESPEC_H

#include <stdio.h>
#include "platform/PlatFile.h"
#include "lle/FSBasicFile.h" // the base class

class LLE_API FSFileSpec : public FSBasicFile {
public:
    using FSFile::Read;    // the long/unsigned long adapters (lle/FSFile.h)
    using FSFile::Write;
    FSFileSpec();                                     // 0x8d15: names from gStartupVolume/gStartupDirectory
    FSFileSpec(const FSFileSpec& inSpec);             // 0x8dad
    FSFileSpec(const UString& inPath);                // 0x8dcd
    virtual ~FSFileSpec();                            // 0x8efe
    FSFileSpec& operator=(const FSFileSpec& inSpec);  // 0x8f2f
    bool operator<(FSFileSpec& inSpec);               // 0x8f74 (non-const parameter, as exported)

    // Members in alphabetical order, as the first unit's emission order shows (CloseSearchHandle 0x13d6, GetFirstFile
    // 0x13e8, Read 0x1420, Write 0x1434); the new virtuals keep their relative order (vtable slots unchanged).
    virtual long Close();                                                       // +0x08 0x91cd
    void CloseSearchHandle()                          // 0x13d6 (inline)
    {
        PlatFindClose(static_cast<PlatFind*>(this->mHandle));
        this->mHandle = NULL;
    }
    virtual long Copy(const UString& inDestination, bool inOverwrite, bool inKeepTimes);   // +0x0c 0x921c
    virtual long Create();                                                      // +0x04 0x901a
    virtual long CreateDirectory();                                             // +0xa0 0x9142
    virtual long DeleteDirectory();                                             // +0xa4 0x92ca
    virtual long FOpen(char* inMode);                                           // +0xa8 0x935a
    void* GetFileHandle() const { return this->mHandle; }  // inline (folded 0x11cc9)
    virtual long GetFirstFile(FSFileSpec& outFile)                              // +0xb0 0x13e8 (inline)
    {
        UString thePattern("*.*");
        return this->GetFirstFile(thePattern, outFile);
    }
    virtual long GetFirstFile(const UString& inPattern, FSFileSpec& outFile);  // +0xac 0x9406
    virtual bool GetIsOpen() const;                                             // +0x24 (folded 0x4b4d9)
    virtual long GetLength(unsigned long& outLength) const;                     // +0xb4 0x95e3
    long GetLengthFromClosedFile(unsigned long& outLength);   // 0x9627
    virtual long GetNextFile(FSFileSpec& outFile);                              // +0xb8 0x9661
    virtual void GetParentDirectory(FSFileSpec& outDirectory) const;            // +0xbc 0x9816
    virtual long GetPosition(long& outPosition) const;                          // +0xc0 0x98d8
    virtual bool IsDirectory() const;                                           // +0xc4 0x991f
    virtual long Lock();                                                        // +0xc8 0x9992
    virtual long Open(unsigned long inAccess);                                  // +0x30 0x9a41
    virtual long Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead);   // +0x64 0x9b48
    virtual long Read(char* outData, unsigned long inLength, unsigned long& outRead)             // +0x60 0x1420
        { return this->Read(reinterpret_cast<unsigned char*>(outData), inLength, outRead); }
    virtual long Read(uint32_t& outValue);                                      // +0x5c (folded 0x9ba5)
    virtual long Read(int32_t& outValue);                                       // +0x58 0x9ba5
    virtual long Read(unsigned short& outValue);                                // +0x54 (folded 0x9bcc)
    virtual long Read(short& outValue);                                         // +0x50 0x9bcc
    virtual long Read(unsigned char& outValue);                                 // +0x4c (folded 0x9c1a)
    virtual long Read(signed char& outValue);                                   // +0x48 0x9c1a
    virtual long Read(float& outValue);                                         // +0x44 (folded 0x9ba5)
    virtual long Read(double& outValue);                                        // +0x40 0x9bf3
    virtual long Read(bool& outValue);                                          // +0x3c (folded 0x9c1a)
    virtual long SetLength(unsigned long& ioLength, bool inTruncate);           // +0xcc 0x9c41 (provisional names)
    virtual long SetPosition(long inOffset, unsigned long inOrigin, bool inExtend);   // +0xd0 0x9c9f (provisional names)
    virtual long Unlock();                                                      // +0xd4 0x9d44
    virtual long Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten);  // +0x9c 0x9df6
    virtual long Write(const char* inData, unsigned long inLength, unsigned long& outWritten)           // +0x98 0x1434
        { return this->Write(reinterpret_cast<const unsigned char*>(inData), inLength, outWritten); }
    virtual long Write(const uint32_t& inValue);                                // +0x94 (folded 0x9e51)
    virtual long Write(const int32_t& inValue);                                 // +0x90 0x9e51
    virtual long Write(const unsigned short& inValue);                          // +0x8c (folded 0x9e6e)
    virtual long Write(const short& inValue);                                   // +0x88 0x9e6e
    virtual long Write(const unsigned char& inValue);                           // +0x84 (folded 0x9e8b)
    virtual long Write(const signed char& inValue);                             // +0x80 0x9e8b
    virtual long Write(const float& inValue);                                   // +0x7c (folded 0x9e51)
    virtual long Write(const double& inValue);                                  // +0x78 0x9ea8
    virtual long Write(const bool& inValue);                                    // +0x74 (folded 0x9e8b)
    operator FILE*() const { return this->mStream; }  // exported ??BFSFileSpec@@QBEPAU_iobuf@@XZ (inline; ESFilterDebugOutput)

protected:
    void* mHandle;                                    // +0x40: the open PlatFile*, or a search's PlatFind*
                                                      // (a Win32 HANDLE in the original)
    FILE* mStream;                                    // +0x44: FOpen's fopen stream (Close: fclose)
};

#endif
