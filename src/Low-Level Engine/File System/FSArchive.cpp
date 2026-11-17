// C:\EEX\Low-Level Engine\File System\FSArchive.cpp (__FILE__ in the asserts; RTTI .?AVFSArchive@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Whole in Rust (rust/crates/ee_base/src/fs/archive_obj.rs, LLE lane round 5; the tables and the format: archive.rs): each
// member forwards the object, from its vtable pointer on (EeFsArchiveObj), to Rust. What stays here: the sinks that
// fill a UWideString and a std::set (GetTag, ListFiles), FindFile's new FSMemoryFile, Create's list of names.
#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSArchive.h"
#include "rust/ee_fs.h"

static_assert(sizeof(FSArchive) == sizeof(EeFsArchiveObj), "FSArchive is an EeFsArchiveObj");

static EeFsArchiveObj* Record(FSArchive* inArchive) { return reinterpret_cast<EeFsArchiveObj*>(inArchive); }
static const EeFsArchiveObj* Record(const FSArchive* inArchive) { return reinterpret_cast<const EeFsArchiveObj*>(inArchive); }
static EeFsSpec* Spec(FSFileSpec* inSpec) { return reinterpret_cast<EeFsSpec*>(inSpec); }
static const EeFsSpec* Spec(const FSFileSpec* inSpec) { return reinterpret_cast<const EeFsSpec*>(inSpec); }
static const EeFsString* Text(const UString* inText) { return reinterpret_cast<const EeFsString*>(inText); }

namespace {

void ListNameSink(void* inContext, const char* inName)
{
    static_cast<std::set<UString>*>(inContext)->insert(UString(inName));
}

void TagValueSink(void* inContext, const char* inValue)
{
    *static_cast<UWideString*>(inContext) = inValue;
}

}   // namespace

// Creates an archive with no file set and nothing mapped
FSArchive::FSArchive()
{
    static_assert(offsetof(FSArchive, mArchiveFile) == offsetof(EeFsArchiveObj, archive_file), "layout");
    static_assert(offsetof(FSArchive, mViewLength) == offsetof(EeFsArchiveObj, view_length), "layout");
    ee_fs_arch_init(Record(this));
}

// Creates an archive for the given archive file, not yet opened
FSArchive::FSArchive(const FSFileSpec& inFile)
{
    ee_fs_arch_init_file(Record(this), Spec(&inFile));
}

// Copies the file name and the tables; the view is shared (only closed archives are ever copied, FSPath::AddArchive)
FSArchive::FSArchive(const FSArchive& inArchive)
    : mArchiveFile(inArchive.mArchiveFile)
{
    ee_fs_arch_init_copy(Record(this), Record(&inArchive));
}

// Destroys the archive, unmapping and closing it
FSArchive::~FSArchive()
{
    ee_fs_arch_finish(Record(this));
}

// Closes the archive and takes over the other archive's file name (not its contents)
FSArchive& FSArchive::operator=(const FSArchive& inArchive)
{
    ee_fs_arch_assign(Record(this), Record(&inArchive));
    return *this;
}

long FSArchive::Close()
{
    return ee_fs_arch_close(Record(this));
}

// Builds an archive from the files under a root matching a pattern, writes it, reopens it and checks the offsets
long FSArchive::Create(const FSFileSpec& inRoot, const UString& inPattern, unsigned long inCompression, bool inRecursive)
{
    return ee_fs_arch_create_walk(Record(this), Spec(&inRoot), inPattern, inCompression, inRecursive,
                                  Text(&gStartupVolume), Text(&gStartupDirectory));
}

// Builds an archive from a list of files under a root, writes it, reopens it and checks the offsets
long FSArchive::Create(const FSFileSpec& inRoot, std::vector<UString>& inFiles, unsigned long inCompression)
{
    std::vector<const EeFsString*> theNames;
    for (unsigned long i = 0; i < inFiles.size(); i++)
        theNames.push_back(Text(&inFiles[i]));
    return ee_fs_arch_create_list(Record(this), Spec(&inRoot), theNames.data(), theNames.size(), inCompression);
}

// Extracts every archived file below a destination directory, creating directories as needed
long FSArchive::ExtractAll(const FSFileSpec& inDestination)
{
    fflush(stdout);   // (the progress lines are Rust's: after the C runtime's)
    return ee_fs_arch_extract_all(Record(this), Spec(&inDestination), Text(&gStartupVolume), Text(&gStartupDirectory));
}

// Finds a file by name (case-insensitive) and returns it decompressed as a memory file
long FSArchive::FindFile(const UString& inName, FSMemoryFile*& outFile)
{
    UString theName(inName);
    theName.ToLower();
    EeFsMemFile* theData = NULL;
    long theResult = ee_fs_arch_find(Record(this), theName, &theData);
    if (theData != NULL) {
        outFile = new FSMemoryFile(theData, true);
        ee_fs_arch_name_member(Record(this), Spec(outFile), Text(&theName));
    }
    return theResult;
}

// Returns the value of a named archive tag, or kErrorNotFound
long FSArchive::GetTag(const UWideString& inName, UWideString& outValue) const
{
    return ee_fs_archive_get_tag(this->mTable, (const char*)inName, TagValueSink, &outValue);
}

// Lists archived files matching a '*' pattern
long FSArchive::ListFiles(const UString& inPattern, std::set<UString>& outFiles, bool inClear)
{
    if (inClear)
        outFiles.clear();
    ee_fs_archive_list(this->mTable, (const char*)inPattern, ListNameSink, &outFiles);
    return kErrorOK;
}

// Opens the archive for reading: checks the header, reads the file and tag tables, maps the file into memory
long FSArchive::Open()
{
    return ee_fs_arch_open(Record(this));
}

// Closes the archive and points it at another archive file
long FSArchive::SetArchive(const FSFileSpec& inFile)
{
    return ee_fs_arch_set_archive(Record(this), Spec(&inFile));
}

// Sets a named tag's value; an empty value removes the tag
long FSArchive::SetTag(const UWideString& inName, const UWideString& inValue)
{
    ee_fs_archive_set_tag(this->mTable, (const char*)inName, (const char*)inValue);
    return kErrorOK;
}
