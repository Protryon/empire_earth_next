// C:\EEX\Low-Level Engine\File System\FSChunk.cpp (file name inferred: no __FILE__; RTTI .?AVFSChunk@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/TSScheduler.h"
#include "lle/TSReadWriteLock.h"
#include "lle/FSDebugFile.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "TSLockable.h"
#include "lle/FSPersistentList.h"
#include "lle/FSChunk.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

FSDebugFile* FSChunk::sDebugFile;
const UString FSChunk::sChunkSizeStringID("FSChunk-ChunkSize");
const UString FSChunk::sMajorVersionStringID("FSChunk-MajorVersion");
const UString FSChunk::sMinorVersionStringID("FSChunk-MinorVersion");
const UString FSChunk::sTagStringID("FSChunk-TagID");

// FUNCTION: LLE 0x10007db9
// Creates an empty chunk with no header or data loaded
FSChunk::FSChunk()
{
    this->mData = NULL;
    this->mTagID = 0;
    this->mChunkSize = 0;
    this->mMajorVersion = 0;
    this->mMinorVersion = 0;
    this->mDataLoaded = false;
    this->mHeaderLoaded = false;
    this->mDecompressed = false;
}

// FUNCTION: LLE 0x10007ddc
// Destroys the chunk, freeing its data
FSChunk::~FSChunk()
{
    if (this->mData)
        delete this->mData;
    this->mData = NULL;
}

// FUNCTION: LLE 0x10007df8
// Returns the file to read or write the chunk through: the debug file wrapping the data when one is set
FSFile* FSChunk::BuildFileToUse()
{
    FSFile* theFile = this->mData;
    if (FSChunk::sDebugFile) {
        FSChunk::sDebugFile->SetDelegate(this->mData);
        theFile = FSChunk::sDebugFile;
    }
    return theFile;
}

// FUNCTION: LLE 0x10007e11
// Detaches and clears the debug file used for chunk I/O
void FSChunk::ClearDebugFile()
{
    if (FSChunk::sDebugFile) {
        FSChunk::sDebugFile->SetDelegate(NULL);
        FSChunk::sDebugFile = NULL;
    }
}

// FUNCTION: LLE 0x10007e2a
// Returns the chunk's size in outSize once its header is loaded; false before that
bool FSChunk::GetChunkSize(unsigned long& outSize) const
{
    bool theResult = false;
    if (this->mHeaderLoaded) {
        outSize = this->mChunkSize;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: LLE 0x10007e3f
// Loads the chunk header if needed, then reads and decompresses the chunk data from a file
long FSChunk::LoadChunkFromFile(FSFile& inFile)
{
    long theResult = kErrorOK;
    unsigned long theLength = 0;
    if (!this->mHeaderLoaded)
        theResult = this->LoadChunkHeader(inFile);
    if (theResult != kErrorOK || this->mData != NULL)
        return theResult;
    this->mData = new FSMemoryFile(this->mChunkSize, false, 1);
    theResult = this->mData->Load(inFile, this->mChunkSize);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mData->Decompress(theLength);
    if (theResult != kErrorOK)
        return theResult;
    this->mDecompressed = true;
    this->mDataLoaded = true;
    this->mChunkSize = theLength;
    return theResult;
}

// FUNCTION: LLE 0x10007eb7
// Creates the persistent object registered for the chunk's tag and loads it from the chunk data
long FSChunk::LoadPersistentFromChunk(FSPersistent*& outPersistent)
{
    long theResult;
    if (this->mData) {
        outPersistent = FSPersistentList::Instance()->CreateDefaultPersistent(this->mTagID);
        if (outPersistent) {
            FSFile* theFile = this->BuildFileToUse();
            theResult = outPersistent->Load(this->mMajorVersion, this->mMinorVersion, *theFile);
        }
        else
            theResult = kErrorNotFound;
    } else
        theResult = kErrorNotSuspended;
    return theResult;
}

// FUNCTION: LLE 0x10007efd
// Reads the chunk header (versions, tag, size); a size of 2 GB or more fails
long FSChunk::LoadChunkHeader(FSFile& inFile)
{
    long theResult = inFile.Read(this->mMajorVersion);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(this->mMinorVersion);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mTagID);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mChunkSize);
                this->mHeaderLoaded = true;
                if (theResult == kErrorOK)
                    theResult = this->mChunkSize < 0x80000000 ? kErrorOK : kErrorFailed;
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10007f5a
// Writes the chunk header (versions, tag, size)
long FSChunk::SaveChunkHeader(FSFile& outFile)
{
    long theResult = outFile.Write(this->mMajorVersion);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mMinorVersion);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTagID);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mChunkSize);
    return theResult;
}

// FUNCTION: LLE 0x10007fab
// Writes the chunk header and its data to a file
long FSChunk::SaveChunkToFile(FSFile& outFile)
{
    long theResult = this->SaveChunkHeader(outFile);
    if (theResult == kErrorOK)
        theResult = this->mData->Save(outFile, this->mChunkSize);
    return theResult;
}

// FUNCTION: LLE 0x10007fce
// Saves a persistent object into new compressed chunk data; returns the save's error when it fails
long FSChunk::SavePersistentToChunk(unsigned long inMajorVersion, unsigned long inMinorVersion,
                                    FSPersistent* inPersistent)
{
    unsigned long theLength = 0;
    if (this->mData != NULL)
        return kErrorOK;
    this->mData = new FSMemoryFile(1000, true, 1);
    FSFile* theFile = this->BuildFileToUse();
    long theResult = inPersistent->Save(inMajorVersion, inMinorVersion, *theFile);
    if (theResult != kErrorOK)
        return theResult;   // Fixed: a failed save used to be reported as success
    this->mData->Compress(0x1000, theLength);
    this->mMajorVersion = inMajorVersion;
    this->mMinorVersion = inMinorVersion;
    this->mTagID = inPersistent->GetTagID();
    this->mChunkSize = theLength;
    return kErrorOK;
}

// FUNCTION: LLE 0x10008054
// Sets the debug file used for chunk I/O
void FSChunk::SetDebugFile(FSDebugFile* inFile)
{
    FSChunk::sDebugFile = inFile;
}

// FUNCTION: LLE 0x1000805e
// Skips over the chunk's data in a file, loading the header first if needed
long FSChunk::SkipChunk(FSFile& inFile)
{
    long theResult = kErrorOK;
    unsigned long theRead = 0;
    if (!this->mHeaderLoaded)
        theResult = this->LoadChunkHeader(inFile);
    if (theResult == kErrorOK && !this->mDataLoaded) {
        unsigned char* theBuffer = new unsigned char[this->mChunkSize];
        theResult = inFile.Read(theBuffer, this->mChunkSize, theRead);
        delete[] theBuffer;
        this->mDataLoaded = true;
    }
    return theResult;
}

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x10007cb5
// SYNTHETIC: LLE 0x10007cbf
// SYNTHETIC: LLE 0x10007ccf
// SYNTHETIC: LLE 0x10007cdb
// SYNTHETIC: LLE 0x10007cf6
// SYNTHETIC: LLE 0x10007d00
// SYNTHETIC: LLE 0x10007d10
// SYNTHETIC: LLE 0x10007d1c
// SYNTHETIC: LLE 0x10007d37
// SYNTHETIC: LLE 0x10007d41
// SYNTHETIC: LLE 0x10007d51
// SYNTHETIC: LLE 0x10007d5d
// SYNTHETIC: LLE 0x10007d78
// SYNTHETIC: LLE 0x10007d82
// SYNTHETIC: LLE 0x10007d92
// SYNTHETIC: LLE 0x10007d9e
