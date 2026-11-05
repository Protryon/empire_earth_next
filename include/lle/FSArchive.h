// C:\EEX\Low-Level Engine\File System\FSArchive.h: an archive file ("ssar" header, a FAT of compressed files, a
// tag table) read through a file mapping. FSArchive.cpp 0x4e29..0x7af6; vtable 0x10097530 (dtor only).
// Requires <map>, <set>, <vector>, lle/FSFileSpec.h, lle/UDataCompression.h.
#ifndef LLE_FSARCHIVE_H
#define LLE_FSARCHIVE_H

#include <map>
#include <set>
#include <vector>

class FSMemoryFile;                         // only by pointer: File System.cpp emits FSMemoryFile's inlines after these

typedef long int32;                         // Inferred home: the assert text "(int32)theCompressedSize" (0x100d8f64)

class LLE_API FSArchive {                   // sizeof 0x6c (FSPath's vector<FSArchive> stride)
public:
    struct ArchivedFile {                   // 0xc bytes; field names from the assert texts (0x100d8c4c..)
        int32 mStart;                       // +0x00 offset of the data in the archive
        int32 mEnd;                         // +0x04 last byte (mStart when empty)
        int32 mLength;                      // +0x08 compressed length
    };

    FSArchive();                                                                // 0x4e29
    FSArchive(const FSFileSpec& inFile);                                        // 0x4e71
    virtual ~FSArchive();                                                       // 0x4ec8
    // implicit copy ctor (0x40cf), emitted with File System.cpp's inlines
    FSArchive& operator=(const FSArchive& inArchive);                           // 0x4ef0 (the archive file only)

    long Close();                                                               // 0x51fe
    long Create(const FSFileSpec& inRoot, const UString& inPattern, unsigned long inCompression,
                bool inRecursive);                                              // 0x5429
    long Create(const FSFileSpec& inRoot, std::vector<UString>& inFiles, unsigned long inCompression);   // 0x5618
    long ExtractAll(const FSFileSpec& inDestination);                           // 0x5801
    long FindFile(const UString& inName, FSMemoryFile*& outFile);               // 0x5a79
    long FindFile(const UWideString& inName, FSMemoryFile*& outFile)
        { UString theName((const char*)inName); return this->FindFile(theName, outFile); }
    const FSFileSpec& GetArchiveFile() const { return this->mArchiveFile; }    // folded 0x40cb (inline)
    long GetTag(const UWideString& inName, UWideString& outValue) const;        // 0x5b4d
    long ListFiles(const UString& inPattern, std::set<UString>& outFiles, bool inClear);   // 0x5b81
    long Open();                                                                // 0x5c58
    long SetArchive(const FSFileSpec& inFile);                                  // 0x60c8
    long SetTag(const UWideString& inName, const UWideString& inValue);         // 0x612f

private:
    long BuildFAT(const FSFileSpec& inRoot, const FSFileSpec& inDirectory, const UString& inPattern,
                  unsigned long inCompression, bool inRecursive);               // 0x4f0e
    long BuildFAT(const FSFileSpec& inRoot, std::vector<UString>& inFiles, unsigned long inCompression);   // 0x50c4
    unsigned long ComputeCompressedFileLength(const FSFileSpec& inFile, unsigned long inCompression);   // 0x5212
    void ComputeFATFilename(const FSFileSpec& inRoot, const FSFileSpec& inFile, UString& outName);   // 0x52cd
    unsigned long ComputeFATSize();                                             // 0x53a6
    unsigned long ComputeTagsSize();                                            // 0x53e2
    void DumpFAT();                                                             // empty (folded 0x80c07)
    long ReadFAT();                                                             // 0x5d30
    long ReadFATFile(const ArchivedFile& inFile, FSMemoryFile*& outFile);       // 0x5e83
    long ReadHeader();                                                          // 0x5f51
    long ReadIntoMemoryFile(FSFileSpec& inFile, FSMemoryFile& outFile);         // 0x5fa3
    long ReadTags();                                                            // 0x5ff9
    void Reset();                                                               // 0x608e
    void SetFATFileOffsets(unsigned long inOffset);                             // 0x60e2
    long WriteFAT();                                                            // 0x61d9
    long WriteFATFiles(const FSFileSpec& inRoot, unsigned long inCompression);  // 0x626d
    long WriteHeader();                                                         // 0x6448
    long WriteTags();                                                           // 0x6484

    FSFileSpec mArchiveFile;                            // +0x04
    std::map<UString, ArchivedFile> mFAT;               // +0x4c (head +0x50, size +0x54); keys lower case
    std::map<UWideString, UWideString> mTags;           // +0x58 (head +0x5c, size +0x60)
    PlatFileMapping* mMapping;                          // +0x64 PlatFileMap (CreateFileMappingA in the original)
    const unsigned char* mView;                         // +0x68 the mapped view
};

#endif
