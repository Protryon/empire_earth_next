// C:\EEX\Low-Level Engine\File System\FSDebugFile.h (file name inferred): a text mirror of a binary file. Every
// Read/Write forwards to the delegate file; ReadInfo/WriteInfo also read/write a "name: value" text line on the
// debug file's own stdio stream ("rt"/"wt"). FSDebugFile.cpp 0x80b9..0x8d15.
// Vtable 0x10097554 (RTTI .?AVFSDebugFile@@ > FSBasicFile > FSFile; one new slot, +0xa0 IsDirectory).
// Requires lle/FSBasicFile.h and <stdio.h>.
#ifndef LLE_FSDEBUGFILE_H
#define LLE_FSDEBUGFILE_H

#include <stdio.h>

// Inline/implicit members are emitted with File System.cpp's inlines: WriteText(const char*, ...) 0x4285, implicit
// copy ctor 0x4299 and operator= 0x42d8, vector deleting dtor 0x4311.
class LLE_API FSDebugFile : public FSBasicFile {
public:
    using FSFile::Read;    // the long/unsigned long adapters (lle/FSFile.h)
    using FSFile::Write;
    FSDebugFile(FSFile* inDelegate);                                            // 0x80b9
    virtual ~FSDebugFile();                                                     // 0x80e7

    virtual long Create() { return kErrorOK; }                                    // +0x04 (folded 0x32e79)
    virtual long Close();                                                       // +0x08 0x810c
    virtual long Copy(const UString& inDestination, bool inOverwrite, bool inKeepTimes);   // +0x0c 0x8154
    virtual long Open(unsigned long inAccess);                                  // +0x30 0x815a
    virtual void PopIndentHint();                                               // +0x34 0x829b
    virtual void PushIndentHint();                                              // +0x38 0x82df
    virtual long Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead);   // +0x64 0x84ef
    virtual long Read(char* outData, unsigned long inLength, unsigned long& outRead);            // +0x60 0x8506
    virtual long Read(uint32_t& outValue);                                      // +0x5c 0x851d
    virtual long Read(int32_t& outValue);                                       // +0x58 0x852c
    virtual long Read(unsigned short& outValue);                                // +0x54 0x853b
    virtual long Read(short& outValue);                                         // +0x50 0x854a
    virtual long Read(unsigned char& outValue);                                 // +0x4c 0x8559
    virtual long Read(signed char& outValue);                                   // +0x48 0x8568
    virtual long Read(float& outValue);                                         // +0x44 0x8577
    virtual long Read(double& outValue);                                        // +0x40 0x8586
    virtual long Read(bool& outValue);                                          // +0x3c 0x8595
    virtual long Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten);  // +0x9c 0x8978
    virtual long Write(const char* inData, unsigned long inLength, unsigned long& outWritten);           // +0x98 0x8992
    virtual long Write(const uint32_t& inValue);                                // +0x94 0x89ac
    virtual long Write(const int32_t& inValue);                                 // +0x90 0x89be
    virtual long Write(const unsigned short& inValue);                          // +0x8c 0x89d0
    virtual long Write(const short& inValue);                                   // +0x88 0x89e2
    virtual long Write(const unsigned char& inValue);                           // +0x84 0x89f4
    virtual long Write(const signed char& inValue);                             // +0x80 0x8a06
    virtual long Write(const float& inValue);                                   // +0x7c 0x8a18
    virtual long Write(const double& inValue);                                  // +0x78 0x8a27
    virtual long Write(const bool& inValue);                                    // +0x74 0x8a36
    virtual bool IsDirectory() const;                                           // +0xa0 (folded with FSFileSpec's 0x991f)

    long ReadInfo(UString& outName, unsigned long& outValue);                   // 0x85a4
    long ReadInfo(UString& outName, long& outValue);                            // 0x85d7
    long ReadInfo(UString& outName, unsigned short& outValue);                  // 0x860a
    long ReadInfo(UString& outName, short& outValue);                           // 0x863d
    long ReadInfo(UString& outName, unsigned char& outValue);                   // 0x8670
    long ReadInfo(UString& outName, signed char& outValue);                     // 0x86a3
    long ReadInfo(UString& outName, float& outValue);                           // 0x86d6
    long ReadInfo(UString& outName, double& outValue);                          // 0x8709
    long ReadInfo(UString& outName, bool& outValue);                            // 0x873c
    void SetDelegate(FSFile* inDelegate);                                       // 0x876f
    long WriteInfo(const UString& inName, const unsigned long& inValue);        // 0x8a45
    long WriteInfo(const UString& inName, const long& inValue);                 // 0x8a7b
    long WriteInfo(const UString& inName, const unsigned short& inValue);       // 0x8ab1
    long WriteInfo(const UString& inName, const short& inValue);                // 0x8ae7
    long WriteInfo(const UString& inName, const unsigned char& inValue);        // 0x8b1d
    long WriteInfo(const UString& inName, const signed char& inValue);          // 0x8b53
    long WriteInfo(const UString& inName, const float& inValue);                // 0x8b89
    long WriteInfo(const UString& inName, const double& inValue);               // 0x8bbc
    long WriteInfo(const UString& inName, const bool& inValue);                 // 0x8bef

private:
    long ReadText(char* outData, unsigned long inLength, unsigned long& outRead);            // ICF-folded (0x8315)
    long ReadText(unsigned char* outData, unsigned long inLength, unsigned long& outRead);   // 0x8315
    long ReadText(unsigned long& outValue);                                     // 0x8354
    long ReadText(long& outValue);                                              // 0x837a
    long ReadText(unsigned short& outValue);                                    // 0x83a0
    long ReadText(short& outValue);                                             // 0x83c6
    long ReadText(unsigned char& outValue);                                     // 0x83ec
    long ReadText(signed char& outValue);                                       // 0x8421
    long ReadText(float& outValue);                                             // 0x8459
    long ReadText(double& outValue);                                            // 0x847f
    long ReadText(bool& outValue);                                              // 0x84a5
    long WriteText(const char* inData, unsigned long inLength, unsigned long& outWritten)   // 0x4285 (inline)
        { return this->WriteText(reinterpret_cast<const unsigned char*>(inData), inLength, outWritten); }
    long WriteText(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten);   // 0x8779
    long WriteText(const unsigned long& inValue);                               // 0x87ab
    long WriteText(const long& inValue);                                        // 0x87dc
    long WriteText(const unsigned short& inValue);                              // 0x880d
    long WriteText(const short& inValue);                                       // 0x8840
    long WriteText(const unsigned char& inValue);                               // 0x8873
    long WriteText(const signed char& inValue);                                 // 0x88a6
    long WriteText(const float& inValue);                                       // 0x88d9
    long WriteText(const double& inValue);                                      // 0x890f
    long WriteText(const bool& inValue);                                        // 0x8945
    long ReadPossibleInfoString(UString& outName);                              // 0x8c22
    long ReadPossibleEOL();                                                     // 0x8c87
    long WritePossibleInfoString(const UString& inName);                        // 0x8cb2
    long WritePossibleEOL();                                                    // 0x8cea

    FSFile* mDelegate;                      // +0x40
    FILE* mStream;                          // +0x44
    bool mIsTextOutput;                     // +0x48: opened "wt" (provisional names)
    bool mIsTextInput;                      // +0x49: opened "rt"
    UString mIndent;                        // +0x4c: Push/PopIndentHint add/remove two spaces
};

#endif
