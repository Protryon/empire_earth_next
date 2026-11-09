// C:\EEX\Low-Level Engine\File System\FSArchive.cpp (__FILE__ in the asserts; RTTI .?AVFSArchive@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The tables and the file format are Rust's (rust/crates/ee_fs/src/archive.rs); this file keeps the directory walk, the
// file specs, the mapping and the asserts. The ESVERIFY_ATs carry their original __LINE__.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSArchive.h"
#include "rust/ee_rust.h"
#include "FSRustOps.h"

namespace {

// The FAT in order (ee_fs_archive_for_each_fat), for the loops that need FSFileSpec or several entries at a time
struct FATEntry {
    UString mName;
    FSArchive::ArchivedFile mFile;
};

void CollectFAT(void* inContext, const char* inName, int32_t inStart, int32_t inEnd, int32_t inLength)
{
    FATEntry theEntry;
    theEntry.mName = inName;
    theEntry.mFile.mStart = inStart;
    theEntry.mFile.mEnd = inEnd;
    theEntry.mFile.mLength = inLength;
    static_cast<std::vector<FATEntry>*>(inContext)->push_back(theEntry);
}

void ListNameSink(void* inContext, const char* inName)
{
    static_cast<std::set<UString>*>(inContext)->insert(UString(inName));
}

void TagValueSink(void* inContext, const char* inValue)
{
    *static_cast<UWideString*>(inContext) = inValue;
}

}   // namespace

// FUNCTION: LLE 0x10004e29
// Creates an archive with no file set and nothing mapped
FSArchive::FSArchive()
    : mTable(ee_fs_archive_new()), mMapping(NULL), mView(NULL), mViewLength(0)
{
}

// FUNCTION: LLE 0x10004e71
// Creates an archive for the given archive file, not yet opened
FSArchive::FSArchive(const FSFileSpec& inFile)
    : mTable(ee_fs_archive_new()), mMapping(NULL), mView(NULL), mViewLength(0)
{
    this->mArchiveFile = inFile;
}

// Copies the file name and the tables; the view is shared (only closed archives are ever copied, FSPath::AddArchive)
FSArchive::FSArchive(const FSArchive& inArchive)
    : mArchiveFile(inArchive.mArchiveFile), mTable(ee_fs_archive_clone(inArchive.mTable)),
      mMapping(inArchive.mMapping), mView(inArchive.mView), mViewLength(inArchive.mViewLength)
{
}

// FUNCTION: LLE 0x10004ec8
// Destroys the archive, unmapping and closing it
FSArchive::~FSArchive()
{
    this->Close();
    ee_fs_archive_free(this->mTable);
    this->mTable = NULL;
}

// FUNCTION: LLE 0x10004ef0
// Closes the archive and takes over the other archive's file name (not its contents)
FSArchive& FSArchive::operator=(const FSArchive& inArchive)
{
    this->Close();
    this->mArchiveFile = inArchive.mArchiveFile;
    return *this;
}

// FUNCTION: LLE 0x10004f0e
// Adds every file under a directory that matches a pattern (recursing if asked) to the table with its packed length
long FSArchive::BuildFAT(const FSFileSpec& inRoot, const FSFileSpec& inDirectory, const UString& inPattern,
                         unsigned long inCompression, bool inRecursive)
{
    long theResult;
    UString theFilename(PLAT_MAX_PATH);
    FSFileSpec theDirectory(inDirectory);
    FSFileSpec theFile;
    if (theDirectory.IsDirectory()) {
        for (theResult = theDirectory.GetFirstFile(inPattern, theFile); theResult == kErrorOK;
             theResult = theDirectory.GetNextFile(theFile)) {
            if (theFile.IsDirectory())
                continue;
            unsigned long theLength = this->ComputeCompressedFileLength(theFile, inCompression);
            this->ComputeFATFilename(inRoot, theFile, theFilename);
            UString theKey(theFilename);
            theKey.ToLower();

            ESVERIFY_AT(theKey.GetLength() == theFilename.GetLength(), 137);
            ee_fs_archive_fat_insert(this->mTable, (const char*)theKey, (int32_t)theLength);
        }
        if (inRecursive) {
            for (theResult = theDirectory.GetFirstFile(theFile); theResult == kErrorOK;
                 theResult = theDirectory.GetNextFile(theFile)) {
                if (theFile.IsDirectory())
                    this->BuildFAT(inRoot, theFile, inPattern, inCompression, inRecursive);
            }
        }
        theResult = kErrorOK;
    } else
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100050c4
// Adds the listed files under a root to the file table with their compressed lengths
long FSArchive::BuildFAT(const FSFileSpec& inRoot, std::vector<UString>& inFiles, unsigned long inCompression)
{
    UString theFilename(PLAT_MAX_PATH);
    FSFileSpec theFile(inRoot);
    for (unsigned long i = 0; i < inFiles.size(); i++) {
        theFile.SetFile(inFiles[i]);
        unsigned long theLength = this->ComputeCompressedFileLength(theFile, inCompression);
        this->ComputeFATFilename(inRoot, theFile, theFilename);
        UString theKey(theFilename);
        theKey.ToLower();

        ESVERIFY_AT(theKey.GetLength() == theFilename.GetLength(), 213);
        ee_fs_archive_fat_insert(this->mTable, (const char*)theKey, (int32_t)theLength);
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x100051fe
// Closes the archive, dropping its file table and tags
long FSArchive::Close()
{
    this->Reset();
    ee_fs_archive_clear_tags(this->mTable);
    return kErrorOK;
}

// FUNCTION: LLE 0x10005212
// Returns the compressed length of a file (0 when it cannot be read)
unsigned long FSArchive::ComputeCompressedFileLength(const FSFileSpec& inFile, unsigned long inCompression)
{
    FSFileSpec theFile(inFile);
    FSMemoryFile theMemoryFile(1000, true, 1);
    unsigned long theCompressedLength = 0;
    long theResult = theMemoryFile.Open(kFileAccessWrite);
    if (theResult != kErrorOK && theResult != kErrorAlreadyExists)
        return theCompressedLength;
    if (theFile.Open(kFileAccessRead) != kErrorOK)
        return theCompressedLength;
    if (this->ReadIntoMemoryFile(theFile, theMemoryFile) == kErrorOK) {
        theResult = theMemoryFile.Compress(inCompression, theCompressedLength);

        ESVERIFY_AT(theResult == keseOK, 268);
    }
    theFile.Close();
    return theCompressedLength;
}

// FUNCTION: LLE 0x100052cd
// Returns a file's path relative to the archive root
void FSArchive::ComputeFATFilename(const FSFileSpec& inRoot, const FSFileSpec& inFile, UString& outName)
{
    UString theRoot = inRoot.GetVolume() + inRoot.GetDirectory();
    UString theFilename = inFile.GetVolume() + inFile.GetDirectory() + inFile.GetFile();
    unsigned long theOffset = theFilename.Find(theRoot, 0);

    ESVERIFY_AT(theOffset == 0, 296);
    theOffset += theRoot.GetLength();
    ESVERIFY_AT(theOffset < theFilename.GetLength(), 298);
    outName.Copy(theFilename, theOffset, theFilename.GetLength() - theOffset, 0, false);
}

// FUNCTION: LLE 0x10005429
// Builds an archive from the files under a root matching a pattern, writes it, reopens it and checks the offsets
long FSArchive::Create(const FSFileSpec& inRoot, const UString& inPattern, unsigned long inCompression, bool inRecursive)
{
    this->Reset();
    long theResult = this->BuildFAT(inRoot, inRoot, inPattern, inCompression, inRecursive);
    if (theResult != kErrorOK)
        return theResult;
    return this->FinishCreate(inRoot, inCompression);
}

// FUNCTION: LLE 0x10005618
// Builds an archive from a list of files under a root, writes it, reopens it and checks the offsets
long FSArchive::Create(const FSFileSpec& inRoot, std::vector<UString>& inFiles, unsigned long inCompression)
{
    this->Reset();
    long theResult = this->BuildFAT(inRoot, inFiles, inCompression);
    if (theResult != kErrorOK)
        return theResult;
    return this->FinishCreate(inRoot, inCompression);
}

// The common end of both Creates
long FSArchive::FinishCreate(const FSFileSpec& inRoot, unsigned long inCompression)
{
    ee_fs_archive_set_offsets(this->mTable,
                              ee_fs_archive_tags_size(this->mTable) + ee_fs_archive_fat_size(this->mTable) + 12);
    this->mArchiveFile.Delete();
    this->mArchiveFile.Create();
    long theResult = this->mArchiveFile.Open(kFileAccessWrite);
    if (theResult != kErrorOK)
        return theResult;
    EeFsFileOps theOps;
    FSBuildFileOps(this->mArchiveFile, theOps);
    if (ee_fs_archive_write_header(this->mTable, &theOps) == kErrorOK &&
        ee_fs_archive_write_fat(this->mTable, &theOps) == kErrorOK &&
        ee_fs_archive_write_tags(this->mTable, &theOps) == kErrorOK)
        this->WriteFATFiles(inRoot, inCompression);
    this->Close();
    this->Open();
    this->CheckFATAfterCreate();
    theResult = this->Close();
    return theResult;
}

// Checks the offsets of the archive just reopened and that every file reads back
void FSArchive::CheckFATAfterCreate()
{
    std::vector<FATEntry> theFAT;
    ee_fs_archive_for_each_fat(this->mTable, CollectFAT, &theFAT);
    for (unsigned long i = 0; i < theFAT.size(); i++) {
        ArchivedFile& theFile = theFAT[i].mFile;
        if (theFile.mLength == 0) {
            ESVERIFY_AT(theFile.mEnd - theFile.mStart == theFile.mLength, 452);
        } else
            ESVERIFY_AT(theFile.mEnd - theFile.mStart + 1 == theFile.mLength, 456);
        if (i + 1 < theFAT.size()) {
            ESVERIFY_AT(theFile.mStart + theFile.mLength == theFAT[i + 1].mFile.mStart, 462);
            if (theFile.mLength == 0) {
                ESVERIFY_AT(theFile.mEnd == theFAT[i + 1].mFile.mStart, 465);
            } else
                ESVERIFY_AT(theFile.mEnd + 1 == theFAT[i + 1].mFile.mStart, 469);
        }
        FSMemoryFile* theMemoryFile = NULL;
        this->ReadFATFile(theFile, theMemoryFile);
        ESVERIFY_AT(theMemoryFile != NULL, 481);
        delete theMemoryFile;
    }
}

// FUNCTION: LLE 0x10005801
// Extracts every archived file below a destination directory, creating directories as needed
long FSArchive::ExtractAll(const FSFileSpec& inDestination)
{
    long theResult = kErrorOK;
    std::vector<FATEntry> theFAT;
    ee_fs_archive_for_each_fat(this->mTable, CollectFAT, &theFAT);
    for (unsigned long i = 0; i < theFAT.size() && theResult == kErrorOK; i++) {
        FSMemoryFile* theFile;
        printf("Extracting file %s%s%s...", (const char*)inDestination.GetVolume(),
               (const char*)inDestination.GetDirectory(), (const char*)theFAT[i].mName);
        FSFileSpec theDirectory(inDestination.GetVolume() + inDestination.GetDirectory() + theFAT[i].mName);
        theDirectory.SetFile("");
        theResult = theDirectory.CreateDirectory();
        if (theResult == kErrorAlreadyExists)
            theResult = kErrorOK;
        if (theResult == kErrorOK) {
            theFile = NULL;
            theResult = this->ReadFATFile(theFAT[i].mFile, theFile);
            if (theResult == kErrorOK && theFile != NULL) {
                FSFileSpec theOutput(inDestination.GetVolume() + inDestination.GetDirectory() + theFAT[i].mName);
                theResult = theOutput.Create();
                if (theResult == kErrorAlreadyExists)
                    theResult = kErrorOK;
                if (theResult == kErrorOK) {
                    unsigned long theLength;
                    theResult = theOutput.Open(kFileAccessWrite);
                    if (theResult == kErrorOK && theFile->GetLength(theLength) == kErrorOK) {
                        unsigned long theWritten;
                        theResult = theOutput.Write(theFile->GetData(), theLength, theWritten);
                        if (theResult == kErrorOK) {
                            if (theWritten == theLength)
                                printf("success!\n");
                            else {
                                printf("extraction failed: unable to write all data to file\n");
                                theResult = kErrorFailed;
                            }
                        } else
                            printf("extraction failed: unable to write to file\n");
                        theOutput.Close();
                    } else
                        printf("extraction failed: unable to open file\n");
                } else
                    printf("extraction failed: unable to create file\n");
                delete theFile;
                theFile = NULL;
            } else
                printf("extraction failed: unable to read from archive\n");
        } else
            printf("extraction faild: unable to create directory %s%s\n", (const char*)theDirectory.GetVolume(),
                   (const char*)theDirectory.GetDirectory());
    }
    return theResult;
}

// FUNCTION: LLE 0x10005a79
// Finds a file by name (case-insensitive) and returns it decompressed as a memory file
long FSArchive::FindFile(const UString& inName, FSMemoryFile*& outFile)
{
    UString theName(inName);
    theName.ToLower();
    EeFsMemFile* theData = NULL;
    long theResult = ee_fs_archive_find_file(this->mTable, (const char*)theName, this->mView, this->mViewLength, &theData);
    if (theData != NULL) {
        outFile = new FSMemoryFile(theData, true);
        outFile->SetVolume(this->mArchiveFile.GetVolume());
        outFile->SetDirectory(this->mArchiveFile.GetDirectory() + this->mArchiveFile.GetFile() + "\\");
        outFile->SetFile(theName);
    }
    return theResult;
}

// FUNCTION: LLE 0x10005b4d
// Returns the value of a named archive tag, or kErrorNotFound
long FSArchive::GetTag(const UWideString& inName, UWideString& outValue) const
{
    return ee_fs_archive_get_tag(this->mTable, (const char*)inName, TagValueSink, &outValue);
}

// FUNCTION: LLE 0x10005b81
// Lists archived files matching a '*' pattern
long FSArchive::ListFiles(const UString& inPattern, std::set<UString>& outFiles, bool inClear)
{
    if (inClear)
        outFiles.clear();
    ee_fs_archive_list(this->mTable, (const char*)inPattern, ListNameSink, &outFiles);
    return kErrorOK;
}

// FUNCTION: LLE 0x10005c58
// Opens the archive for reading: checks the header, reads the file and tag tables, maps the file into memory
long FSArchive::Open()
{
    long theResult;
    if (!this->mArchiveFile.GetIsOpen()) {
        theResult = this->mArchiveFile.Open(kFileAccessRead);
        if (theResult == kErrorOK) {
            EeFsFileOps theOps;
            FSBuildFileOps(this->mArchiveFile, theOps);
            theResult = ee_fs_archive_read_header(this->mTable, &theOps);
            if (theResult == kErrorOK)
                theResult = ee_fs_archive_read_fat(this->mTable, &theOps);
            if (theResult == kErrorOK)
                theResult = ee_fs_archive_read_tags(this->mTable, &theOps);
            if (theResult == kErrorOK) {
                const void* theView = NULL;
                unsigned long theLength = 0;
                this->mArchiveFile.GetLength(theLength);
                this->mMapping = PlatFileMap(static_cast<PlatFile*>(this->mArchiveFile.GetFileHandle()), &theView);
                if (this->mMapping) {
                    this->mView = static_cast<const unsigned char*>(theView);
                    this->mViewLength = theLength;
                } else {   // the mapping or the view failed (PlatFileMap closes a mapping without a view)
                    ESRecordNativeError(PlatGetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
            }
            if (theResult != kErrorOK)
                this->mArchiveFile.Close();
        }
    } else
        theResult = kErrorAlreadyExists;
    return theResult;
}

// FUNCTION: LLE 0x10005e83
// Returns one archived file as a new memory file, decompressing it when it is compressed
long FSArchive::ReadFATFile(const ArchivedFile& inFile, FSMemoryFile*& outFile)
{
    EeFsMemFile* theData = NULL;
    long theResult = ee_fs_archive_read_member(this->mView, this->mViewLength, inFile.mStart, inFile.mLength, &theData);
    outFile = new FSMemoryFile(theData, true);
    return theResult;
}

// FUNCTION: LLE 0x10005fa3
// Copies a whole file into a memory file
long FSArchive::ReadIntoMemoryFile(FSFileSpec& inFile, FSMemoryFile& outFile)
{
    unsigned long theLength;
    unsigned long theRead;
    unsigned long theWritten;
    inFile.GetLength(theLength);
    unsigned char* theBuffer = new unsigned char[theLength];
    inFile.Read(theBuffer, theLength, theRead);
    long theResult = outFile.Write(theBuffer, theRead, theWritten);
    delete[] theBuffer;
    return theResult;
}

// FUNCTION: LLE 0x1000608e
// Unmaps the archive view, closes the archive file and empties the file table
void FSArchive::Reset()
{
    if (this->mMapping) {
        PlatFileUnmap(this->mMapping, this->mView);
        this->mMapping = NULL;
    }
    this->mView = NULL;
    this->mViewLength = 0;
    this->mArchiveFile.Close();
    ee_fs_archive_clear_fat(this->mTable);
}

// FUNCTION: LLE 0x100060c8
// Closes the archive and points it at another archive file
long FSArchive::SetArchive(const FSFileSpec& inFile)
{
    this->Close();
    this->mArchiveFile = inFile;
    return kErrorOK;
}

// FUNCTION: LLE 0x1000612f
// Sets a named tag's value; an empty value removes the tag
long FSArchive::SetTag(const UWideString& inName, const UWideString& inValue)
{
    ee_fs_archive_set_tag(this->mTable, (const char*)inName, (const char*)inValue);
    return kErrorOK;
}

// FUNCTION: LLE 0x1000626d
// Writes every listed file, compressed, at its planned offset in the archive
long FSArchive::WriteFATFiles(const FSFileSpec& inRoot, unsigned long inCompression)
{
    std::vector<FATEntry> theFAT;
    ee_fs_archive_for_each_fat(this->mTable, CollectFAT, &theFAT);
    FSFileSpec theFile(inRoot);
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < theFAT.size() && theResult == kErrorOK; i++) {
        ArchivedFile& theEntry = theFAT[i].mFile;
        long theFilePosition;
        this->mArchiveFile.GetPosition(theFilePosition);

        ESVERIFY_AT(theFilePosition == theEntry.mStart, 1435);
        theFile.SetFile(theFAT[i].mName);
        FSMemoryFile* theMemoryFile = new FSMemoryFile(1000, true, 1);
        theResult = theMemoryFile->Open(kFileAccessWrite);
        if (theResult == kErrorOK || theResult == kErrorAlreadyExists) {
            theResult = theFile.Open(kFileAccessRead);
            if (theResult == kErrorOK) {
                theResult = this->ReadIntoMemoryFile(theFile, *theMemoryFile);
                if (theResult == kErrorOK) {
                    unsigned long theCompressedSize;
                    theResult = theMemoryFile->Compress(inCompression, theCompressedSize);
                    if (theResult == kErrorOK) {
                        unsigned long theAmountWrote;

                        ESVERIFY_AT((int32)theCompressedSize == theEntry.mLength, 1459);
                        theResult = this->mArchiveFile.Write(theMemoryFile->GetData(), theCompressedSize, theAmountWrote);

                        ESVERIFY_AT(theAmountWrote == theCompressedSize, 1463);
                        this->mArchiveFile.GetPosition(theFilePosition);
                        if (theEntry.mLength > 0) {
                            ESVERIFY_AT(theFilePosition == theEntry.mEnd + 1, 1468);
                        } else {
                            ESVERIFY_AT(theFilePosition == theEntry.mEnd, 1472);
                        }
                    }
                }
                theFile.Close();
            }
        }
        delete theMemoryFile;
    }
    return theResult;
}
