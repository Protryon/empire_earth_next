// C:\EEX\Low-Level Engine\File System\FSArchive.cpp (__FILE__ in the asserts; RTTI .?AVFSArchive@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The ESVERIFY_ATs carry their original __LINE__.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/UDataCompression.h"
#include "lle/FSArchive.h"

static const unsigned long kArchiveIdentifier = 'ssar';   // 0x10097710 (provisional names)
static const unsigned long kArchiveVersion = 1;            // 0x10097714
static const unsigned long kArchiveReserved = 0;           // 0x10097718

// FUNCTION: LLE 0x10004e29
// Creates an archive with no file set and nothing mapped
FSArchive::FSArchive()
    : mMapping(NULL), mView(NULL)
{
}

// FUNCTION: LLE 0x10004e71
// Creates an archive for the given archive file, not yet opened
FSArchive::FSArchive(const FSFileSpec& inFile)
{
    this->mArchiveFile = inFile;
    this->mMapping = NULL;
    this->mView = NULL;
}

// FUNCTION: LLE 0x10004ec8
// Destroys the archive, unmapping and closing it
FSArchive::~FSArchive()
{
    this->Close();
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
    UString theFilename(MAX_PATH);
    FSFileSpec theDirectory(inDirectory);
    FSFileSpec theFile;
    std::pair<UString, ArchivedFile> theFATPair;
    ArchivedFile theArchivedFile;
    theArchivedFile.mStart = 0;
    theArchivedFile.mEnd = 0;
    if (theDirectory.IsDirectory()) {
        for (theResult = theDirectory.GetFirstFile(inPattern, theFile); theResult == kErrorOK;
             theResult = theDirectory.GetNextFile(theFile)) {
            if (theFile.IsDirectory())
                continue;
            theArchivedFile.mLength = this->ComputeCompressedFileLength(theFile, inCompression);
            this->ComputeFATFilename(inRoot, theFile, theFilename);
            theFATPair.first = theFilename;
            theFATPair.first.ToLower();

            ESVERIFY_AT(theFATPair.first.GetLength() == theFilename.GetLength(), 137);
            theFATPair.second = theArchivedFile;
            this->mFAT.insert(theFATPair);
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
    UString theFilename(MAX_PATH);
    FSFileSpec theFile(inRoot);
    std::pair<UString, ArchivedFile> theFATPair;
    ArchivedFile theArchivedFile;
    theArchivedFile.mStart = 0;
    theArchivedFile.mEnd = 0;
    for (unsigned long i = 0; i < inFiles.size(); i++) {
        theFile.SetFile(inFiles[i]);
        theArchivedFile.mLength = this->ComputeCompressedFileLength(theFile, inCompression);
        this->ComputeFATFilename(inRoot, theFile, theFilename);
        theFATPair.first = theFilename;
        theFATPair.first.ToLower();

        ESVERIFY_AT(theFATPair.first.GetLength() == theFilename.GetLength(), 213);
        theFATPair.second = theArchivedFile;
        this->mFAT.insert(theFATPair);
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x100051fe
// Closes the archive, dropping its file table and tags
long FSArchive::Close()
{
    this->Reset();
    this->mTags.clear();
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

// FUNCTION: LLE 0x100053a6
// Returns the size of the saved file table in bytes
unsigned long FSArchive::ComputeFATSize()
{
    std::map<UString, ArchivedFile>::iterator theIter = this->mFAT.begin();
    unsigned long theSize = 4;
    for (; theIter != this->mFAT.end(); ++theIter)
        theSize += theIter->first.GetBytesUsedToSave() + 12;
    return theSize;
}

// FUNCTION: LLE 0x100053e2
// Returns the size of the saved tag table in bytes
unsigned long FSArchive::ComputeTagsSize()
{
    std::map<UWideString, UWideString>::iterator theIter = this->mTags.begin();
    unsigned long theSize = 4;
    for (; theIter != this->mTags.end(); ++theIter) {
        theSize += theIter->first.GetBytesUsedToSave();
        theSize += theIter->second.GetBytesUsedToSave();
    }
    return theSize;
}

// FUNCTION: LLE 0x10005429
// Builds an archive from the files under a root matching a pattern, writes it, reopens it and checks the offsets
long FSArchive::Create(const FSFileSpec& inRoot, const UString& inPattern, unsigned long inCompression, bool inRecursive)
{
    FSMemoryFile* theFile = NULL;
    std::map<UString, ArchivedFile>::iterator theIter;
    std::map<UString, ArchivedFile>::iterator theNextIter;
    long theResult;
    this->Reset();
    this->DumpFAT();
    theResult = this->BuildFAT(inRoot, inRoot, inPattern, inCompression, inRecursive);
    if (theResult != kErrorOK)
        return theResult;
    this->DumpFAT();
    this->SetFATFileOffsets(this->ComputeTagsSize() + this->ComputeFATSize() + 12);
    this->DumpFAT();
    this->mArchiveFile.Delete();
    this->mArchiveFile.Create();
    theResult = this->mArchiveFile.Open(kFileAccessWrite);
    if (theResult != kErrorOK)
        return theResult;
    if (this->WriteHeader() == kErrorOK && this->WriteFAT() == kErrorOK && this->WriteTags() == kErrorOK)
        this->WriteFATFiles(inRoot, inCompression);
    this->DumpFAT();
    this->Close();
    this->Open();
    for (theIter = this->mFAT.begin(); theIter != this->mFAT.end(); ++theIter) {
        if (theIter->second.mLength == 0) {
            ESVERIFY_AT(theIter->second.mEnd - theIter->second.mStart == theIter->second.mLength, 452);
        } else

            ESVERIFY_AT(theIter->second.mEnd - theIter->second.mStart + 1 == theIter->second.mLength, 456);
        theNextIter = theIter;
        ++theNextIter;
        if (theNextIter != this->mFAT.end()) {
            ESVERIFY_AT(theIter->second.mStart + theIter->second.mLength == theNextIter->second.mStart, 462);
            if (theIter->second.mLength == 0) {
                ESVERIFY_AT(theIter->second.mEnd == theNextIter->second.mStart, 465);
            } else

                ESVERIFY_AT(theIter->second.mEnd + 1 == theNextIter->second.mStart, 469);
        }
        this->ReadFATFile(theIter->second, theFile);

        ESVERIFY_AT(theFile != NULL, 481);
        delete theFile;
        theFile = NULL;
    }
    theResult = this->Close();
    return theResult;
}

// FUNCTION: LLE 0x10005618
// Builds an archive from a list of files under a root, writes it, reopens it and checks the offsets
long FSArchive::Create(const FSFileSpec& inRoot, std::vector<UString>& inFiles, unsigned long inCompression)
{
    FSMemoryFile* theFile = NULL;
    std::map<UString, ArchivedFile>::iterator theIter;
    std::map<UString, ArchivedFile>::iterator theNextIter;
    long theResult;
    this->Reset();
    this->DumpFAT();
    theResult = this->BuildFAT(inRoot, inFiles, inCompression);
    if (theResult != kErrorOK)
        return theResult;
    this->DumpFAT();
    this->SetFATFileOffsets(this->ComputeTagsSize() + this->ComputeFATSize() + 12);
    this->DumpFAT();
    this->mArchiveFile.Delete();
    this->mArchiveFile.Create();
    theResult = this->mArchiveFile.Open(kFileAccessWrite);
    if (theResult != kErrorOK)
        return theResult;
    if (this->WriteHeader() == kErrorOK && this->WriteFAT() == kErrorOK && this->WriteTags() == kErrorOK)
        this->WriteFATFiles(inRoot, inCompression);
    this->DumpFAT();
    this->Close();
    this->Open();
    for (theIter = this->mFAT.begin(); theIter != this->mFAT.end(); ++theIter) {
        if (theIter->second.mLength == 0) {
            ESVERIFY_AT(theIter->second.mEnd - theIter->second.mStart == theIter->second.mLength, 580);
        } else

            ESVERIFY_AT(theIter->second.mEnd - theIter->second.mStart + 1 == theIter->second.mLength, 584);
        theNextIter = theIter;
        ++theNextIter;
        if (theNextIter != this->mFAT.end()) {
            ESVERIFY_AT(theIter->second.mStart + theIter->second.mLength == theNextIter->second.mStart, 590);
            if (theIter->second.mLength == 0) {
                ESVERIFY_AT(theIter->second.mEnd == theNextIter->second.mStart, 593);
            } else

                ESVERIFY_AT(theIter->second.mEnd + 1 == theNextIter->second.mStart, 597);
        }
        this->ReadFATFile(theIter->second, theFile);

        ESVERIFY_AT(theFile != NULL, 609);
        delete theFile;
        theFile = NULL;
    }
    theResult = this->Close();
    return theResult;
}

void FSArchive::DumpFAT()
{
}

// FUNCTION: LLE 0x10005801
// Extracts every archived file below a destination directory, creating directories as needed
long FSArchive::ExtractAll(const FSFileSpec& inDestination)
{
    long theResult = kErrorOK;
    std::map<UString, ArchivedFile>::iterator theEnd = this->mFAT.end();
    std::map<UString, ArchivedFile>::iterator theIter = this->mFAT.begin();
    while (theIter != theEnd && theResult == kErrorOK) {
        FSMemoryFile* theFile;
        printf("Extracting file %s%s%s...", (const char*)inDestination.GetVolume(),
               (const char*)inDestination.GetDirectory(), (const char*)theIter->first);
        FSFileSpec theDirectory(inDestination.GetVolume() + inDestination.GetDirectory() + theIter->first);
        theDirectory.SetFile("");
        theResult = theDirectory.CreateDirectory();
        if (theResult == kErrorAlreadyExists)
            theResult = kErrorOK;
        if (theResult == kErrorOK) {
            theFile = NULL;
            theResult = this->ReadFATFile(theIter->second, theFile);
            if (theResult == kErrorOK && theFile != NULL) {
                FSFileSpec theOutput(inDestination.GetVolume() + inDestination.GetDirectory() + theIter->first);
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
        ++theIter;
    }
    return theResult;
}

// FUNCTION: LLE 0x10005a79
// Finds a file by name (case-insensitive) and returns it decompressed as a memory file
long FSArchive::FindFile(const UString& inName, FSMemoryFile*& outFile)
{
    long theResult;
    UString theName(inName);
    theName.ToLower();
    std::map<UString, ArchivedFile>::iterator theIter = this->mFAT.find(theName);
    if (theIter != this->mFAT.end()) {
        theResult = this->ReadFATFile(theIter->second, outFile);
        if (outFile) {
            outFile->SetVolume(this->mArchiveFile.GetVolume());
            outFile->SetDirectory(this->mArchiveFile.GetDirectory() + this->mArchiveFile.GetFile() + "\\");
            outFile->SetFile(theName);
        }
    } else
        theResult = kErrorNotFound;
    return theResult;
}

// FUNCTION: LLE 0x10005b4d
// Returns the value of a named archive tag, or kErrorNotFound
long FSArchive::GetTag(const UWideString& inName, UWideString& outValue) const
{
    long theResult;
    std::map<UWideString, UWideString>::const_iterator theIter = this->mTags.find(inName);
    if (theIter != this->mTags.end()) {
        outValue = theIter->second;
        theResult = kErrorOK;
    } else
        theResult = kErrorNotFound;
    return theResult;
}

// FUNCTION: LLE 0x10005b81
// Lists archived files matching a '*' pattern, but never advances its iterator (hangs on a non-empty archive)
long FSArchive::ListFiles(const UString& inPattern, std::set<UString>& outFiles, bool inClear)
{
    std::vector<UString> theTokens;
    std::map<UString, ArchivedFile>::iterator theIter = this->mFAT.begin();
    if (inClear)
        outFiles.clear();
    inPattern.Tokenize("*", theTokens);
    while (theIter != this->mFAT.end()) {
        bool theMatch = true;
        unsigned long theOffset = 0;
        for (unsigned long i = 0; i < theTokens.size(); i++) {
            if (theOffset >= theIter->first.GetLength()) {
                theMatch = false;
                break;
            }
            unsigned long theFound = theIter->first.Find(theTokens[i], theOffset);
            if (theFound >= theIter->first.GetLength()) {
                theMatch = false;
                break;
            }
            theOffset = theFound + theTokens[i].GetLength();
        }
        if (theMatch)
            outFiles.insert(theIter->first);
        ++theIter;  // Fixed: the iterator was never advanced (endless loop on a non-empty archive)
    }
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
            theResult = this->ReadHeader();
            if (theResult == kErrorOK)
                theResult = this->ReadFAT();
            if (theResult == kErrorOK)
                theResult = this->ReadTags();
            if (theResult == kErrorOK) {
                this->mMapping = CreateFileMappingA(this->mArchiveFile.GetFileHandle(), NULL, PAGE_READONLY | SEC_COMMIT, 0, 0,
                                              NULL);
                if (this->mMapping) {
                    this->mView = (const unsigned char*)MapViewOfFile(this->mMapping, FILE_MAP_READ, 0, 0, 0);
                    if (!this->mView) {
                        ESRecordNativeError(GetLastError());
                        ESGetNativeError();
                        theResult = kErrorNativeError;
                        CloseHandle(this->mMapping);
                        this->mMapping = NULL;
                    }
                } else {
                    ESRecordNativeError(GetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
            }
            if (theResult == kErrorOK)
                this->DumpFAT();
            else
                this->mArchiveFile.Close();
        }
    } else
        theResult = kErrorAlreadyExists;
    return theResult;
}

// FUNCTION: LLE 0x10005d30
// Reads the archive's file table (name, start, end and length per file)
long FSArchive::ReadFAT()
{
    std::map<UString, ArchivedFile>::iterator theIter;
    std::pair<UString, ArchivedFile> theFATPair;
    UString theFilename;
    ArchivedFile theArchivedFile;
    unsigned long theFATSize;
    unsigned long thePosition = 0;
    unsigned long theNextPosition;
    long theResult = this->mArchiveFile.Read(theFATSize);
    if (theResult != kErrorOK)
        return theResult;
    while (thePosition < theFATSize && theResult == kErrorOK) {
        theResult = theFilename.Load(this->mArchiveFile);
        if (theResult != kErrorOK)
            continue;
        theNextPosition = thePosition + theFilename.GetLength() + 5;
        theResult = this->mArchiveFile.Read(theArchivedFile.mStart);
        if (theResult == kErrorOK) {
            theNextPosition += 4;
            theResult = this->mArchiveFile.Read(theArchivedFile.mEnd);
            if (theResult == kErrorOK) {
                theNextPosition += 4;
                theResult = this->mArchiveFile.Read(theArchivedFile.mLength);
                if (theResult == kErrorOK) {
                    theNextPosition += 4;
                    theFATPair.first = theFilename;
                    theFATPair.second = theArchivedFile;
                    this->mFAT.insert(theFATPair);
                }
            }
        }
        thePosition = theNextPosition;
    }
    if (theResult == kErrorOK) {
        for (theIter = this->mFAT.begin(); theIter != this->mFAT.end(); ++theIter)
            ((UString&)theIter->first).Compact();
    }
    return theResult;
}

// FUNCTION: LLE 0x10005e83
// Returns one archived file as a new memory file, decompressing it when it is compressed
long FSArchive::ReadFATFile(const ArchivedFile& inFile, FSMemoryFile*& outFile)
{
    long theResult = kErrorOK;
    const char* theData = reinterpret_cast<const char*>(this->mView) + inFile.mStart;
    UDataCompression* theCompression = new UDataCompression;
    unsigned long theLength;
    unsigned long theUsed;
    unsigned char* theBuffer;
    if (theCompression->GetIsCompressed(theData, inFile.mLength)) {
        theCompression->GetDecompressedSize(theData, inFile.mLength, theLength);
        theBuffer = new unsigned char[theLength];
        theResult = theCompression->Decompress(theData, inFile.mLength, reinterpret_cast<char*>(theBuffer), theLength, theUsed);
    } else {
        theLength = inFile.mLength;
        theBuffer = new unsigned char[theLength];
        memcpy(theBuffer, theData, inFile.mLength);
    }
    outFile = new FSMemoryFile(theBuffer, theLength, 1000, true);
    delete theCompression;
    return theResult;
}

// FUNCTION: LLE 0x10005f51
// Reads and checks the archive header (identifier, version, reserved word)
long FSArchive::ReadHeader()
{
    unsigned long theIdentifier;
    unsigned long theVersion;
    unsigned long theReserved;
    long theResult = this->mArchiveFile.Read(theIdentifier);
    if (theResult != kErrorOK)
        return theResult;
    if (theIdentifier == kArchiveIdentifier) {
        theResult = this->mArchiveFile.Read(theVersion);
        if (theResult == kErrorOK) {
            if (theVersion == kArchiveVersion) {
                theResult = this->mArchiveFile.Read(theReserved);
                if (theResult == kErrorOK && theReserved != kArchiveReserved)
                    theResult = kErrorFailed;
            } else
                theResult = kErrorFailed;
        }
    } else
        theResult = kErrorFailed;
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
    delete theBuffer;
    return theResult;
}

// FUNCTION: LLE 0x10005ff9
// Reads the archive's tag table (name/value pairs)
long FSArchive::ReadTags()
{
    UWideString theName;
    UWideString theValue;
    unsigned long theCount;
    this->mTags.clear();
    long theResult = this->mArchiveFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
        theResult = theName.Load(this->mArchiveFile);
        if (theResult != kErrorOK)
            continue;
        theResult = theValue.Load(this->mArchiveFile);
        if (theResult == kErrorOK)
            theResult = this->SetTag(theName, theValue);
    }
    return theResult;
}

// FUNCTION: LLE 0x1000608e
// Unmaps the archive view, closes the archive file and empties the file table
void FSArchive::Reset()
{
    if (this->mView) {
        UnmapViewOfFile(this->mView);
        this->mView = NULL;
    }
    if (this->mMapping) {
        CloseHandle(this->mMapping);
        this->mMapping = NULL;
    }
    this->mArchiveFile.Close();
    this->mFAT.clear();
}

// FUNCTION: LLE 0x100060c8
// Closes the archive and points it at another archive file
long FSArchive::SetArchive(const FSFileSpec& inFile)
{
    this->Close();
    this->mArchiveFile = inFile;
    return kErrorOK;
}

// FUNCTION: LLE 0x100060e2
// Lays out the archived files one after another from the given offset, setting each start and end
void FSArchive::SetFATFileOffsets(unsigned long inOffset)
{
    std::map<UString, ArchivedFile>::iterator theIter = this->mFAT.begin();
    unsigned long theOffset = inOffset;
    for (; theIter != this->mFAT.end(); ++theIter) {
        theIter->second.mStart = theOffset;
        if (theIter->second.mLength > 0)
            theIter->second.mEnd = theOffset + theIter->second.mLength - 1;
        else
            theIter->second.mEnd = theIter->second.mStart;
        theOffset += theIter->second.mLength;
    }
}

// FUNCTION: LLE 0x1000612f
// Sets a named tag's value; an empty value removes the tag
long FSArchive::SetTag(const UWideString& inName, const UWideString& inValue)
{
    std::pair<UWideString, UWideString> theTag;
    std::map<UWideString, UWideString>::iterator theIter = this->mTags.find(inName);
    if (theIter != this->mTags.end())
        this->mTags.erase(theIter);
    if (inValue.GetLength() == 0)
        return kErrorOK;
    theTag.first = inName;
    theTag.second = inValue;
    this->mTags.insert(theTag);
    return kErrorOK;
}

// FUNCTION: LLE 0x100061d9
// Writes the file table; ignores write errors and always returns kErrorOK
long FSArchive::WriteFAT()
{
    std::map<UString, ArchivedFile>::iterator theIter = this->mFAT.begin();
    unsigned long theSize = this->ComputeFATSize() - 4;
    if (this->mArchiveFile.Write(theSize) != kErrorOK)
        return kErrorOK;
    for (; theIter != this->mFAT.end(); ++theIter) {
        if (theIter->first.Save(this->mArchiveFile) == kErrorOK)
            if (this->mArchiveFile.Write(theIter->second.mStart) == kErrorOK)
                if (this->mArchiveFile.Write(theIter->second.mEnd) == kErrorOK)
                    this->mArchiveFile.Write(theIter->second.mLength);
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1000626d
// Writes every listed file, compressed, at its planned offset in the archive
long FSArchive::WriteFATFiles(const FSFileSpec& inRoot, unsigned long inCompression)
{
    std::map<UString, ArchivedFile>::iterator theIter = this->mFAT.begin();
    FSFileSpec theFile(inRoot);
    long theResult = kErrorOK;
    for (; theIter != this->mFAT.end() && theResult == kErrorOK; ++theIter) {
        long theFilePosition;
        this->mArchiveFile.GetPosition(theFilePosition);

        ESVERIFY_AT(theFilePosition == theIter->second.mStart, 1435);
        theFile.SetFile(theIter->first);
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

                        ESVERIFY_AT((int32)theCompressedSize == theIter->second.mLength, 1459);
                        theResult = this->mArchiveFile.Write(theMemoryFile->GetData(), theCompressedSize, theAmountWrote);

                        ESVERIFY_AT(theAmountWrote == theCompressedSize, 1463);
                        this->mArchiveFile.GetPosition(theFilePosition);
                        if (theIter->second.mLength > 0) {
                            ESVERIFY_AT(theFilePosition == theIter->second.mEnd + 1, 1468);
                        } else {
                            ESVERIFY_AT(theFilePosition == theIter->second.mEnd, 1472);
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

// FUNCTION: LLE 0x10006448
// Writes the archive header (identifier, version, reserved word)
long FSArchive::WriteHeader()
{
    long theResult = this->mArchiveFile.Write(kArchiveIdentifier);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mArchiveFile.Write(kArchiveVersion);
    if (theResult == kErrorOK)
        theResult = this->mArchiveFile.Write(kArchiveReserved);
    return theResult;
}

// FUNCTION: LLE 0x10006484
// Writes the tag table; returns the last tag's write status
long FSArchive::WriteTags()
{
    std::map<UWideString, UWideString>::iterator theIter = this->mTags.begin();
    unsigned long theCount = this->mTags.size();
    long theResult = this->mArchiveFile.Write(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (; theIter != this->mTags.end(); ++theIter) {
        theResult = theIter->first.Save(this->mArchiveFile);
        if (theResult == kErrorOK)
            theResult = theIter->second.Save(this->mArchiveFile);
    }
    return theResult;
}

// Compiler-generated, continued:
// TEMPLATE: LLE 0x100064ec
// TEMPLATE: LLE 0x10006517
// TEMPLATE: LLE 0x100065dc
// TEMPLATE: LLE 0x100065f0
// TEMPLATE: LLE 0x10006633
// TEMPLATE: LLE 0x1000665e
// TEMPLATE: LLE 0x10006723
// TEMPLATE: LLE 0x100069db
// TEMPLATE: LLE 0x100069ef
// TEMPLATE: LLE 0x100075c5
// TEMPLATE: LLE 0x100076af

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x10006af7
// TEMPLATE: LLE 0x10006fd4
// TEMPLATE: LLE 0x10006a32
// TEMPLATE: LLE 0x10006b0b
// TEMPLATE: LLE 0x10006b7c
// TEMPLATE: LLE 0x10006d10
// TEMPLATE: LLE 0x10006d81
// TEMPLATE: LLE 0x10006f15
// TEMPLATE: LLE 0x10006f57
// TEMPLATE: LLE 0x10006f9b
// TEMPLATE: LLE 0x10007045
// TEMPLATE: LLE 0x100071d9
// TEMPLATE: LLE 0x1000748f
// TEMPLATE: LLE 0x100074c9
// TEMPLATE: LLE 0x100074fe
// TEMPLATE: LLE 0x10007540
// TEMPLATE: LLE 0x10007584
// TEMPLATE: LLE 0x100075fd
// TEMPLATE: LLE 0x10007639
// TEMPLATE: LLE 0x1000766e
// TEMPLATE: LLE 0x100076e7
// TEMPLATE: LLE 0x1000799f
// TEMPLATE: LLE 0x100079db
// TEMPLATE: LLE 0x10007a1d
// TEMPLATE: LLE 0x10007a61
// TEMPLATE: LLE 0x10007a99
// TEMPLATE: LLE 0x10007aaf
// TEMPLATE: LLE 0x10007ad1
