// C:\EEX\Low-Level Engine\File System\FSPath.h: a search path of archives and directories (reference counted).
// Whole in Rust (LLE round 9: rust/crates/ee_base/src/fs path_obj.rs): the class keeps its vtable and reference count
// and the Rust object; the archives and directories are C++ objects the Rust path makes and deletes through
// FSPath.cpp's calls. Requires <set>, lle/FSArchive.h.
#ifndef LLE_FSPATH_H
#define LLE_FSPATH_H

#include <set>
// The base class. File System.cpp emits UReferenceCounter's inline block right before FSPath's (0x436c..0x43dd):
// it is first included here.
#include "lle/UReferenceCounter.h"
#include "rust/base/fs.h"

class LLE_API FSPath : public UReferenceCounter {
public:
    FSPath() { this->mRust = ee_fs_path_new(&kCalls); }
    FSPath(const FSPath& inPath) { this->mRust = ee_fs_path_new_copy(inPath.mRust); }
    FSPath& operator=(const FSPath& inPath) { ee_fs_path_assign(this->mRust, inPath.mRust); return *this; }

    void AddArchive(const FSArchive& inArchive, bool inAppend)
        { ee_fs_path_add_archive(this->mRust, reinterpret_cast<const EeFsArchiveObj*>(&inArchive), inAppend); }
    void AddDirectory(const FSFileSpec& inDirectory, bool inAppend)
        { ee_fs_path_add_directory(this->mRust, reinterpret_cast<const EeFsSpec*>(&inDirectory), inAppend); }
    long FindFile(const UString& inName) { return ee_fs_path_has_file(this->mRust, inName); }
    long FindFile(const UWideString& inName) { return ee_fs_path_has_file(this->mRust, inName); }
    long FindFile(const UString& inName, FSFileSpec*& outFile)
        { return ee_fs_path_find_file(this->mRust, Text(&inName), reinterpret_cast<EeFsSpec**>(&outFile), NULL); }
    long FindFile(const UString& inName, FSFileSpec*& outFile, FSFileSpec& outDirectory)
    {
        return ee_fs_path_find_file(this->mRust, Text(&inName), reinterpret_cast<EeFsSpec**>(&outFile),
                                    reinterpret_cast<EeFsSpec*>(&outDirectory));
    }
    const FSArchive& GetArchive(unsigned long inIndex) const
        { return *reinterpret_cast<const FSArchive*>(ee_fs_path_archive(this->mRust, inIndex)); }
    long GetArchive(unsigned long inIndex, FSArchive& outArchive) const
        { return ee_fs_path_get(this->mRust, false, inIndex, &outArchive); }
    unsigned long GetArchiveCount() const { return ee_fs_path_count(this->mRust, false); }
    long GetDirectory(unsigned long inIndex, FSFileSpec& outDirectory) const
        { return ee_fs_path_get(this->mRust, true, inIndex, &outDirectory); }
    unsigned long GetDirectoryCount() const { return ee_fs_path_count(this->mRust, true); }
    bool GetFileArchiveStatus(const UString& inName, bool& outArchived)
        { return ee_fs_path_archive_status(this->mRust, inName, &outArchived); }
    bool GetFileArchiveStatus(const UWideString& inName, bool& outArchived)
        { return ee_fs_path_archive_status(this->mRust, inName, &outArchived); }
    long ListFiles(const UString& inPattern, std::set<UString>& outFiles)
        { return ee_fs_path_list_files(this->mRust, inPattern, &outFiles); }
    long RemoveArchive(unsigned long inIndex) { return ee_fs_path_remove(this->mRust, false, inIndex); }
    long RemoveDirectory(unsigned long inIndex) { return ee_fs_path_remove(this->mRust, true, inIndex); }   // (RemoveDirectoryA)

protected:
    virtual ~FSPath() { ee_fs_path_free(this->mRust); }

private:
    static const EeFsString* Text(const UString* inText) { return reinterpret_cast<const EeFsString*>(inText); }
    static const EeFsPathCalls kCalls;      // FSPath.cpp: the archives' and directories' C++ objects
    EeFsPath* mRust;
};

#endif
