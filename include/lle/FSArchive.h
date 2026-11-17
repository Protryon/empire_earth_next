// C:\EEX\Low-Level Engine\File System\FSArchive.h: an archive file ("ssar" header, a FAT of compressed files, a
// tag table) read through a file mapping. FSArchive.cpp 0x4e29..0x7af6; vtable 0x10097530 (dtor only).
// Requires <map>, <set>, <vector>, lle/FSFileSpec.h, lle/UDataCompression.h.
#ifndef LLE_FSARCHIVE_H
#define LLE_FSARCHIVE_H

#include <map>
#include <set>
#include <vector>

class FSMemoryFile;                         // only by pointer: File System.cpp emits FSMemoryFile's inlines after these
struct EeFsArchive;                         // rust/crates/ee_base/src/fs/archive.rs: the file and tag tables

typedef long int32;                         // Inferred home: the assert text "(int32)theCompressedSize" (0x100d8f64)

class LLE_API FSArchive {                   // sizeof 0x6c (FSPath's vector<FSArchive> stride)
public:
    struct ArchivedFile {                   // 0xc bytes; field names from the assert texts (0x100d8c4c..)
        int32 mStart;                       // +0x00 offset of the data in the archive
        int32 mEnd;                         // +0x04 last byte (mStart when empty)
        int32 mLength;                      // +0x08 compressed length
    };

    FSArchive();
    FSArchive(const FSFileSpec& inFile);
    virtual ~FSArchive();
    FSArchive(const FSArchive& inArchive);                                      // 0x40cf: copies the tables, shares the view
    FSArchive& operator=(const FSArchive& inArchive);                           // 0x4ef0 (the archive file only)

    long Close();
    long Create(const FSFileSpec& inRoot, const UString& inPattern, unsigned long inCompression,
                bool inRecursive);
    long Create(const FSFileSpec& inRoot, std::vector<UString>& inFiles, unsigned long inCompression);
    long ExtractAll(const FSFileSpec& inDestination);
    long FindFile(const UString& inName, FSMemoryFile*& outFile);
    long FindFile(const UWideString& inName, FSMemoryFile*& outFile)
        { UString theName((const char*)inName); return this->FindFile(theName, outFile); }
    const FSFileSpec& GetArchiveFile() const { return this->mArchiveFile; }    // folded 0x40cb (inline)
    long GetTag(const UWideString& inName, UWideString& outValue) const;
    long ListFiles(const UString& inPattern, std::set<UString>& outFiles, bool inClear);
    long Open();
    long SetArchive(const FSFileSpec& inFile);
    long SetTag(const UWideString& inName, const UWideString& inValue);

private:
    // the members' work is Rust's (rust/crates/ee_base/src/fs/archive_obj.rs): this object from its vtable on is an
    // EeFsArchiveObj
    FSFileSpec mArchiveFile;                            // +0x04
    EeFsArchive* mTable;                                // the FAT and the tags (rust/crates/ee_base/src/fs/archive.rs)
    PlatFileMapping* mMapping;                          // PlatFileMap (CreateFileMappingA in the original)
    const unsigned char* mView;                         // the mapped view
    unsigned long mViewLength;                          // the view's size (the file's length)
};

#endif
