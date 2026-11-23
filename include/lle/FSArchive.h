// C:\EEX\Low-Level Engine\File System\FSArchive.h: an archive file ("ssar" header, a FAT of compressed files, a
// tag table) read through a file mapping. FSArchive.cpp 0x4e29..0x7af6; vtable 0x10097530 (dtor only).
// Requires lle/FSFileSpec.h.
#ifndef LLE_FSARCHIVE_H
#define LLE_FSARCHIVE_H

#include <map>
#include <set>
#include <vector>

#include <stdio.h>
#include "lle/FSMemoryFile.h"                // FindFile's new memory file
#include "rust/base/fs.h"                    // the members' Rust (rust/crates/ee_base/src/fs/archive_obj.rs)

typedef long int32;                         // Inferred home: the assert text "(int32)theCompressedSize" (0x100d8f64)

class LLE_API FSArchive {                   // sizeof 0x6c (FSPath's vector<FSArchive> stride)
public:
    struct ArchivedFile {                   // 0xc bytes; field names from the assert texts (0x100d8c4c..)
        int32 mStart;                       // +0x00 offset of the data in the archive
        int32 mEnd;                         // +0x04 last byte (mStart when empty)
        int32 mLength;                      // +0x08 compressed length
    };

    FSArchive() { ee_fs_arch_init(Rec(this)); }                                // no file set, nothing mapped
    FSArchive(const FSFileSpec& inFile) { ee_fs_arch_init_file(Rec(this), Spec(&inFile)); }   // not yet opened
    virtual ~FSArchive() { ee_fs_arch_finish(Rec(this)); }                     // unmaps and closes
    // 0x40cf: copies the file name and the tables; the view is shared (only closed archives are ever copied)
    FSArchive(const FSArchive& inArchive) : mArchiveFile(inArchive.mArchiveFile) { ee_fs_arch_init_copy(Rec(this), Rec(&inArchive)); }
    // 0x4ef0: closes and takes over the other archive's file name (not its contents)
    FSArchive& operator=(const FSArchive& inArchive)
    {
        ee_fs_arch_assign(Rec(this), Rec(&inArchive));
        return *this;
    }

    long Close() { return ee_fs_arch_close(Rec(this)); }
    // builds an archive from the files under a root matching a pattern (or from a list), writes it, reopens it and
    // checks the offsets
    long Create(const FSFileSpec& inRoot, const UString& inPattern, unsigned long inCompression, bool inRecursive)
    {
        return ee_fs_arch_create_walk(Rec(this), Spec(&inRoot), inPattern, inCompression, inRecursive,
                                      Text(&gStartupVolume), Text(&gStartupDirectory));
    }
    long Create(const FSFileSpec& inRoot, std::vector<UString>& inFiles, unsigned long inCompression)
        { return ee_fs_arch_create_list(Rec(this), Spec(&inRoot), Text(inFiles.data()), inFiles.size(), inCompression); }
    // every archived file below a destination directory, creating directories as needed
    long ExtractAll(const FSFileSpec& inDestination)   // (the progress lines are Rust's: after the C runtime's)
        { return ee_fs_arch_extract_all(Rec(this), Spec(&inDestination), Text(&gStartupVolume), Text(&gStartupDirectory)); }
    // a file by name (case-insensitive), decompressed as a new memory file
    long FindFile(const UString& inName, FSMemoryFile*& outFile)
        { return ee_fs_arch_find_file(Rec(this), Text(&inName), reinterpret_cast<void**>(&outFile)); }
    long FindFile(const UWideString& inName, FSMemoryFile*& outFile)
        { return this->FindFile(UString((const char*)inName), outFile); }
    const FSFileSpec& GetArchiveFile() const { return this->mArchiveFile; }    // folded 0x40cb (inline)
    // a named archive tag's value, or kErrorNotFound
    long GetTag(const UWideString& inName, UWideString& outValue) const
    {
        return ee_fs_archive_get_tag(this->mTable, (const char*)inName, AssignTag, &outValue);
    }
    // the archived files matching a '*' pattern
    long ListFiles(const UString& inPattern, std::set<UString>& outFiles, bool inClear)
        { return ee_fs_archive_list_files(this->mTable, (const char*)inPattern, inClear, ListName, &outFiles); }
    // checks the header, reads the file and tag tables, maps the file into memory
    long Open() { return ee_fs_arch_open(Rec(this)); }
    // closes the archive and points it at another archive file
    long SetArchive(const FSFileSpec& inFile) { return ee_fs_arch_set_archive(Rec(this), Spec(&inFile)); }
    // an empty value removes the tag
    long SetTag(const UWideString& inName, const UWideString& inValue)
        { return ee_fs_archive_set_tag(this->mTable, (const char*)inName, (const char*)inValue); }

private:
    static void AssignTag(void* inValue, const char* inText) { *static_cast<UWideString*>(inValue) = inText; }
    // a listed name into the set; null clears it (ee_fs_archive_list_files)
    static void ListName(void* inFiles, const char* inName)
        { inName ? (void)static_cast<std::set<UString>*>(inFiles)->insert(UString(inName)) : static_cast<std::set<UString>*>(inFiles)->clear(); }
    static EeFsArchiveObj* Rec(FSArchive* inArchive) { return reinterpret_cast<EeFsArchiveObj*>(inArchive); }
    static const EeFsArchiveObj* Rec(const FSArchive* inArchive) { return reinterpret_cast<const EeFsArchiveObj*>(inArchive); }
    static EeFsSpec* Spec(FSFileSpec* inSpec) { return reinterpret_cast<EeFsSpec*>(inSpec); }
    static const EeFsSpec* Spec(const FSFileSpec* inSpec) { return reinterpret_cast<const EeFsSpec*>(inSpec); }
    static const EeFsString* Text(const UString* inText) { return reinterpret_cast<const EeFsString*>(inText); }

    // the members' work is Rust's (rust/crates/ee_base/src/fs/archive_obj.rs): this object from its vtable on is an
    // EeFsArchiveObj
    FSFileSpec mArchiveFile;                            // +0x04
    EeFsArchive* mTable;                                // the FAT and the tags (rust/crates/ee_base/src/fs/archive.rs)
    PlatFileMapping* mMapping;                          // PlatFileMap (CreateFileMappingA in the original)
    const unsigned char* mView;                         // the mapped view
    unsigned long mViewLength;                          // the view's size (the file's length)
};

#endif
