// C:\EEX\Low-Level Engine\File System\FSPath.cpp (RTTI .?AVFSPath@@; 0x1000a843..0x1000bdb4)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// A search path of archives and directories (FindFile looks in the directories first). Names of the member
// functions come from the exports (local names provisional).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1000b1c0
// TEMPLATE: LLE 0x1000b217
// TEMPLATE: LLE 0x1000b304
// TEMPLATE: LLE 0x1000b35e
// TEMPLATE: LLE 0x1000b6f8
// TEMPLATE: LLE 0x1000bac1
// SYNTHETIC: LLE 0x1000a880
// TEMPLATE: LLE 0x1000b18d
// TEMPLATE: LLE 0x1000b2c6
// TEMPLATE: LLE 0x1000b40d
// TEMPLATE: LLE 0x1000b763
// TEMPLATE: LLE 0x1000b79a
// TEMPLATE: LLE 0x1000b7d3
// TEMPLATE: LLE 0x1000bb2c
// TEMPLATE: LLE 0x1000bb66
// TEMPLATE: LLE 0x1000bb9f
// TEMPLATE: LLE 0x1000bbca
// TEMPLATE: LLE 0x1000bbf5
// TEMPLATE: LLE 0x1000bc0c
// TEMPLATE: LLE 0x1000bc35
// TEMPLATE: LLE 0x1000bc5e
// TEMPLATE: LLE 0x1000bc8a
// TEMPLATE: LLE 0x1000bca7
// TEMPLATE: LLE 0x1000bcd3
// TEMPLATE: LLE 0x1000bcf7
// TEMPLATE: LLE 0x1000bd23
// TEMPLATE: LLE 0x1000bd40
// TEMPLATE: LLE 0x1000bd6c
// SYNTHETIC: LLE 0x1000bd90
// SYNTHETIC: LLE 0x1000bda2

// FUNCTION: LLE 0x1000a843
// Creates an empty search path
FSPath::FSPath()
{
}

// FUNCTION: LLE 0x1000a8da
// Creates a copy of a search path, adding (and reopening) each of its archives and directories in order
FSPath::FSPath(const FSPath& inPath)
{
    unsigned long i;
    for (i = 0; i < inPath.mArchives.size(); i++)
        this->AddArchive(inPath.mArchives[i], true);
    for (i = 0; i < inPath.mDirectories.size(); i++)
        this->AddDirectory(inPath.mDirectories[i], true);
}

// FUNCTION: LLE 0x1000a984
// Destroys the search path (members only)
FSPath::~FSPath()
{
}

// FUNCTION: LLE 0x1000a9a5
// Replaces the path's archives and directories with copies of another path's, in order
FSPath& FSPath::operator=(const FSPath& inPath)
{
    unsigned long i;
    this->mArchives.clear();
    this->mDirectories.clear();
    for (i = 0; i < inPath.mArchives.size(); i++)
        this->AddArchive(inPath.mArchives[i], true);
    for (i = 0; i < inPath.mDirectories.size(); i++)
        this->AddDirectory(inPath.mDirectories[i], true);
    return *this;
}

// FUNCTION: LLE 0x1000aa30
// Adds an archive (appended, else first): closes every archive, adds the new one, then reopens them all
void FSPath::AddArchive(const FSArchive& inArchive, bool inAppend)
{
    FSArchive theArchive;
    unsigned long i;
    for (i = 0; i < this->mArchives.size(); i++)
        this->mArchives[i].Close();
    if (inAppend) {
        this->mArchives.push_back(theArchive);
        this->mArchives.back() = inArchive;
    } else {
        if (this->mArchives.size() > 0)
            this->mArchives.insert(this->mArchives.begin(), theArchive);
        else
            this->mArchives.push_back(theArchive);
        this->mArchives.front() = inArchive;
    }
    for (i = 0; i < this->mArchives.size(); i++)
        this->mArchives[i].Open();
}

// FUNCTION: LLE 0x1000ab27
// Adds a directory to the path, appended or in front
void FSPath::AddDirectory(const FSFileSpec& inDirectory, bool inAppend)
{
    if (inAppend)
        this->mDirectories.push_back(inDirectory);
    else if (this->mDirectories.size() > 0)
        this->mDirectories.insert(this->mDirectories.begin(), inDirectory);
    else
        this->mDirectories.push_back(inDirectory);
}

// FUNCTION: LLE 0x1000ab85
// Returns whether the named file is on the path (kErrorOK), discarding the found file
long FSPath::FindFile(const UString& inName)
{
    FSFileSpec* theFile = NULL;
    long theResult = this->FindFile(UString(inName), theFile);
    if (theResult == kErrorOK)
        delete theFile;
    return theResult;
}

// FUNCTION: LLE 0x1000abcb
// Returns whether the named file (wide name) is on the path (kErrorOK), discarding the found file
long FSPath::FindFile(const UWideString& inName)
{
    FSFileSpec* theFile = NULL;
    long theResult = this->FindFile(UString((const char*)inName), theFile);
    if (theResult == kErrorOK)
        delete theFile;
    return theResult;
}

// FUNCTION: LLE 0x1000ac15
// Finds the named file on the path; returns it as a new file spec the caller deletes
long FSPath::FindFile(const UString& inName, FSFileSpec*& outFile)
{
    FSFileSpec theDirectory;
    return this->FindFile(inName, outFile, theDirectory);
}

// FUNCTION: LLE 0x1000ac48
// Finds a file in the directories, then the archives: returns a new file spec (an FSMemoryFile from an archive)
// and, in outDirectory, the found file itself or the archive file it came from
long FSPath::FindFile(const UString& inName, FSFileSpec*& outFile, FSFileSpec& outDirectory)
{
    FSMemoryFile* theMemoryFile = NULL;
    FSFileSpec theFile;
    long theResult = kErrorNotFound;
    unsigned long i;
    if (this->mDirectories.size() > 0)
        for (i = 0; i < this->mDirectories.size(); i++)
            if (this->mDirectories[i].GetFirstFile(inName, theFile) == kErrorOK) {
                outFile = new FSFileSpec(theFile);
                outDirectory = theFile;
                this->mDirectories[i].CloseSearchHandle();
                theResult = kErrorOK;
                break;
            }
    if (theResult != kErrorOK && this->mArchives.size() > 0)
        for (i = 0; i < this->mArchives.size(); i++)
            if (this->mArchives[i].FindFile(inName, theMemoryFile) == kErrorOK) {
                outFile = theMemoryFile;
                outDirectory = this->mArchives[i].GetArchiveFile();
                theResult = kErrorOK;
                break;
            }
    return theResult;
}

// FUNCTION: LLE 0x1000ad7a
// Returns the archive at an index (unchecked)
const FSArchive& FSPath::GetArchive(unsigned long inIndex) const
{
    return this->mArchives[inIndex];
}

// FUNCTION: LLE 0x1000ad89
// Copies the archive at an index to outArchive; kErrorBeyondEnd past the last one
long FSPath::GetArchive(unsigned long inIndex, FSArchive& outArchive) const
{
    if (inIndex < this->mArchives.size()) {
        outArchive = this->mArchives[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x1000adc1
// Copies the directory at an index to outDirectory; kErrorBeyondEnd past the last one
long FSPath::GetDirectory(unsigned long inIndex, FSFileSpec& outDirectory) const
{
    if (inIndex < this->mDirectories.size()) {
        outDirectory = this->mDirectories[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x1000adfa
// Sets outArchived to whether the named file comes from an archive rather than a directory; returns whether the
// file was found
bool FSPath::GetFileArchiveStatus(const UString& inName, bool& outArchived)
{
    FSFileSpec theDirectory;
    FSFileSpec* theFile = NULL;
    if (this->FindFile(inName, theFile, theDirectory) != kErrorOK)
        return false;
    outArchived = true;
    if (theFile->GetVolume() == theDirectory.GetVolume() &&
        theFile->GetDirectory() == theDirectory.GetDirectory() &&
        theFile->GetFile() == theDirectory.GetFile())
        outArchived = false;
    delete theFile;
    return true;
}

// FUNCTION: LLE 0x1000aea1
// Wide-name form of GetFileArchiveStatus
bool FSPath::GetFileArchiveStatus(const UWideString& inName, bool& outArchived)
{
    UString theName((const char*)inName);
    return this->GetFileArchiveStatus(theName, outArchived);
}

// FUNCTION: LLE 0x1000aed9
// Collects the names of the files matching a pattern from every archive and directory (relative to the directory);
// returns the last search's result, which is the end-of-search code when the path has directories
long FSPath::ListFiles(const UString& inPattern, std::set<UString>& outFiles)
{
    UString theRoot(MAX_PATH);
    UString thePath(MAX_PATH);
    UString theName(MAX_PATH);
    FSFileSpec theFile;
    long theResult = kErrorOK;
    unsigned long i;
    outFiles.clear();
    for (i = 0; i < this->mArchives.size(); i++)
        theResult = this->mArchives[i].ListFiles(inPattern, outFiles, false);
    for (i = 0; i < this->mDirectories.size(); i++) {
        theRoot = this->mDirectories[i].GetVolume() + this->mDirectories[i].GetDirectory();
        theResult = this->mDirectories[i].GetFirstFile(inPattern, theFile);
        while (theResult == kErrorOK) {
            thePath = theFile.GetVolume() + theFile.GetDirectory() + theFile.GetFile();
            // one GetLength of the root: a third inline site would leave the first operator[] of each loop too little
            // budget for _Ptrit::operator+ (the original expands both; 68 -> 86, 531 -> 499 bytes)
            unsigned long theStart = theRoot.GetLength();
            theName.Copy(thePath, theStart, thePath.GetLength() - theStart, 0, false);
            outFiles.insert(theName);
            theResult = this->mDirectories[i].GetNextFile(theFile);
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1000b0cd
// Closes and removes the archive at an index; kErrorBeyondEnd past the last one
long FSPath::RemoveArchive(unsigned long inIndex)
{
    if (inIndex < this->mArchives.size()) {
        this->mArchives[inIndex].Close();
        this->mArchives.erase(this->mArchives.begin() + inIndex);
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x1000b136
// Removes the directory at an index; kErrorBeyondEnd past the last one
long FSPath::RemoveDirectory(unsigned long inIndex)
{
    if (inIndex < this->mDirectories.size()) {
        this->mDirectories.erase(this->mDirectories.begin() + inIndex);
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// Vector COMDATs left ambiguous by match_templates (byte-identical across the two element types except for
// their callees):
// TEMPLATE: LLE 0x1000b194
// TEMPLATE: LLE 0x1000b28b
// TEMPLATE: LLE 0x1000b2d8
// TEMPLATE: LLE 0x1000b3d2
// TEMPLATE: LLE 0x1000b422
// TEMPLATE: LLE 0x1000b7e7
