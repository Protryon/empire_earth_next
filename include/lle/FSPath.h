// C:\EEX\Low-Level Engine\File System\FSPath.h: a search path of archives and directories (reference counted).
// FSPath.cpp 0xa843..0xbdb4; vtable 0x100977fc. Requires <vector>, <set>, lle/FSArchive.h.
#ifndef LLE_FSPATH_H
#define LLE_FSPATH_H

#include <set>
#include <vector>
// The base class. File System.cpp emits UReferenceCounter's inline block right before FSPath's (0x436c..0x43dd):
// it is first included here.
#include "lle/UReferenceCounter.h"

class LLE_API FSPath : public UReferenceCounter {   // sizeof 0x28 (vector deleting dtor stride)
public:
    FSPath();
    FSPath(const FSPath& inPath);
    FSPath& operator=(const FSPath& inPath);

    void AddArchive(const FSArchive& inArchive, bool inFirst);
    void AddDirectory(const FSFileSpec& inDirectory, bool inFirst);
    long FindFile(const UString& inName);
    long FindFile(const UWideString& inName);
    long FindFile(const UString& inName, FSFileSpec*& outFile);
    long FindFile(const UString& inName, FSFileSpec*& outFile, FSFileSpec& outDirectory);
    const FSArchive& GetArchive(unsigned long inIndex) const;
    long GetArchive(unsigned long inIndex, FSArchive& outArchive) const;
    unsigned long GetArchiveCount() const { return this->mArchives.size(); }
    long GetDirectory(unsigned long inIndex, FSFileSpec& outDirectory) const;
    unsigned long GetDirectoryCount() const { return this->mDirectories.size(); }
    bool GetFileArchiveStatus(const UString& inName, bool& outArchived);
    bool GetFileArchiveStatus(const UWideString& inName, bool& outArchived);
    long ListFiles(const UString& inPattern, std::set<UString>& outFiles);
    long RemoveArchive(unsigned long inIndex);
    long RemoveDirectory(unsigned long inIndex);                                // 0xb136 (exported as RemoveDirectoryA)

protected:
    virtual ~FSPath();

    std::vector<FSArchive> mArchives;       // +0x08
    std::vector<FSFileSpec> mDirectories;   // +0x18
};

#endif
