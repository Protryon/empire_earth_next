// C:\EEX\Low-Level Engine\File System\FSPath.cpp (RTTI .?AVFSPath@@; 0x1000a843..0x1000bdb4)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// A search path of archives and directories (FindFile looks in the directories first). The searches are Rust's
// (rust/crates/ee_fs/src/path.rs, LLE lane round 5) over the vectors' element arrays; the vectors (of C++ objects:
// GetArchive hands out a reference into one) and their plumbing stay here.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "rust/ee_fs.h"

static EeFsSpec* PathSpec(FSFileSpec* inSpec) { return reinterpret_cast<EeFsSpec*>(inSpec); }
static const EeFsSpec* PathSpec(const FSFileSpec* inSpec) { return reinterpret_cast<const EeFsSpec*>(inSpec); }
static const EeFsArchiveObj* PathArchive(const FSArchive* inArchive) { return reinterpret_cast<const EeFsArchiveObj*>(inArchive); }
static const EeFsString* PathText(const UString* inText) { return reinterpret_cast<const EeFsString*>(inText); }

static void NameSink(void* inContext, const char* inName)
{
    static_cast<std::set<UString>*>(inContext)->insert(UString(inName));
}

// Creates an empty search path
FSPath::FSPath()
{
}

// Creates a copy of a search path, adding (and reopening) each of its archives and directories in order
FSPath::FSPath(const FSPath& inPath)
{
    unsigned long i;
    for (i = 0; i < inPath.mArchives.size(); i++)
        this->AddArchive(inPath.mArchives[i], true);
    for (i = 0; i < inPath.mDirectories.size(); i++)
        this->AddDirectory(inPath.mDirectories[i], true);
}

// Destroys the search path (members only)
FSPath::~FSPath()
{
}

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

// Returns whether the named file is on the path (kErrorOK), discarding the found file
long FSPath::FindFile(const UString& inName)
{
    FSFileSpec* theFile = NULL;
    long theResult = this->FindFile(UString(inName), theFile);
    if (theResult == kErrorOK)
        delete theFile;
    return theResult;
}

// Returns whether the named file (wide name) is on the path (kErrorOK), discarding the found file
long FSPath::FindFile(const UWideString& inName)
{
    FSFileSpec* theFile = NULL;
    long theResult = this->FindFile(UString((const char*)inName), theFile);
    if (theResult == kErrorOK)
        delete theFile;
    return theResult;
}

// Finds the named file on the path; returns it as a new file spec the caller deletes
long FSPath::FindFile(const UString& inName, FSFileSpec*& outFile)
{
    FSFileSpec theDirectory;
    return this->FindFile(inName, outFile, theDirectory);
}

// Finds a file in the directories, then the archives (Rust's search): returns a new file spec (an FSMemoryFile from
// an archive) and, in outDirectory, the found file itself or the archive file it came from
long FSPath::FindFile(const UString& inName, FSFileSpec*& outFile, FSFileSpec& outDirectory)
{
    FSFileSpec theFile;
    EeFsMemFile* theMember = NULL;
    size_t theArchive = 0;
    long theResult = ee_fs_path_find(PathSpec(this->mDirectories.data()), this->mDirectories.size(),
                                     PathArchive(this->mArchives.data()), this->mArchives.size(), PathText(&inName),
                                     PathSpec(&theFile), PathSpec(&outDirectory), &theMember, &theArchive,
                                     PathText(&gStartupVolume), PathText(&gStartupDirectory));
    if (theResult == kErrorOK) {
        if (theMember != NULL) {   // FSArchive::FindFile's member and its names
            FSMemoryFile* theMemoryFile = new FSMemoryFile(theMember, true);
            UString theName(inName);
            theName.ToLower();
            ee_fs_arch_name_member(PathArchive(&this->mArchives[theArchive]), PathSpec(theMemoryFile), PathText(&theName));
            outFile = theMemoryFile;
        } else
            outFile = new FSFileSpec(theFile);
    }
    return theResult;
}

// Returns the archive at an index (unchecked)
const FSArchive& FSPath::GetArchive(unsigned long inIndex) const
{
    return this->mArchives[inIndex];
}

// Copies the archive at an index to outArchive; kErrorBeyondEnd past the last one
long FSPath::GetArchive(unsigned long inIndex, FSArchive& outArchive) const
{
    if (inIndex < this->mArchives.size()) {
        outArchive = this->mArchives[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// Copies the directory at an index to outDirectory; kErrorBeyondEnd past the last one
long FSPath::GetDirectory(unsigned long inIndex, FSFileSpec& outDirectory) const
{
    if (inIndex < this->mDirectories.size()) {
        outDirectory = this->mDirectories[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

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

// Wide-name form of GetFileArchiveStatus
bool FSPath::GetFileArchiveStatus(const UWideString& inName, bool& outArchived)
{
    UString theName((const char*)inName);
    return this->GetFileArchiveStatus(theName, outArchived);
}

// Collects the names of the files matching a pattern from every archive and directory (relative to the directory,
// Rust's walk); returns the last search's result, which is the end-of-search code when the path has directories
long FSPath::ListFiles(const UString& inPattern, std::set<UString>& outFiles)
{
    long theResult = kErrorOK;
    outFiles.clear();
    for (unsigned long i = 0; i < this->mArchives.size(); i++)
        theResult = this->mArchives[i].ListFiles(inPattern, outFiles, false);
    return ee_fs_path_list_directories(PathSpec(this->mDirectories.data()), this->mDirectories.size(), inPattern, theResult,
                                       NameSink, &outFiles, PathText(&gStartupVolume), PathText(&gStartupDirectory));
}

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

// Removes the directory at an index; kErrorBeyondEnd past the last one
long FSPath::RemoveDirectory(unsigned long inIndex)
{
    if (inIndex < this->mDirectories.size()) {
        this->mDirectories.erase(this->mDirectories.begin() + inIndex);
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}
