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
    FSPath();                                                                   // 0xa843
    FSPath(const FSPath& inPath);                                               // 0xa8da
    FSPath& operator=(const FSPath& inPath);                                    // 0xa9a5

    void AddArchive(const FSArchive& inArchive, bool inFirst);                  // 0xaa30 (provisional names)
    void AddDirectory(const FSFileSpec& inDirectory, bool inFirst);             // 0xab27
    long FindFile(const UString& inName);                                       // 0xab85
    long FindFile(const UWideString& inName);                                   // 0xabcb
    long FindFile(const UString& inName, FSFileSpec*& outFile);                 // 0xac15
    long FindFile(const UString& inName, FSFileSpec*& outFile, FSFileSpec& outDirectory);   // 0xac48
    const FSArchive& GetArchive(unsigned long inIndex) const;                   // 0xad7a
    long GetArchive(unsigned long inIndex, FSArchive& outArchive) const;        // 0xad89
    unsigned long GetArchiveCount() const { return this->mArchives.size(); }    // 0x43dd (inline)
    long GetDirectory(unsigned long inIndex, FSFileSpec& outDirectory) const;   // 0xadc1
    unsigned long GetDirectoryCount() const { return this->mDirectories.size(); }  // 0x43f3 (inline)
    bool GetFileArchiveStatus(const UString& inName, bool& outArchived);        // 0xadfa
    bool GetFileArchiveStatus(const UWideString& inName, bool& outArchived);    // 0xaea1
    long ListFiles(const UString& inPattern, std::set<UString>& outFiles);      // 0xaed9
    long RemoveArchive(unsigned long inIndex);                                  // 0xb0cd
    long RemoveDirectory(unsigned long inIndex);                                // 0xb136 (exported as RemoveDirectoryA)

protected:
    virtual ~FSPath();                                                          // 0xa984

    std::vector<FSArchive> mArchives;       // +0x08
    std::vector<FSFileSpec> mDirectories;   // +0x18
};

#endif
