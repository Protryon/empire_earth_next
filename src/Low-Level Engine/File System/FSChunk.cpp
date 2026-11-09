// C:\EEX\Low-Level Engine\File System\FSChunk.cpp (file name inferred: no __FILE__; RTTI .?AVFSChunk@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class forwards to Rust (rust/crates/ee_fs/src/chunk.rs); what stays here is the persistent's virtual Load/Save
// and the debug file around them.
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
#include "FSRustOps.h"

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
    ee_fs_chunk_init(&this->mState);
}

// FUNCTION: LLE 0x10007ddc
// Destroys the chunk, freeing its data
FSChunk::~FSChunk()
{
    ee_fs_chunk_free(&this->mState);
}

// FUNCTION: LLE 0x10007df8
// Returns the file to read or write the chunk through: the debug file wrapping the given payload view when one is set
FSFile* FSChunk::BuildFileToUse(FSFile* inFile)
{
    FSFile* theFile = inFile;
    if (FSChunk::sDebugFile) {
        FSChunk::sDebugFile->SetDelegate(inFile);
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
    uint32_t theSize = 0;
    if (!ee_fs_chunk_size(&this->mState, &theSize))
        return false;
    outSize = theSize;
    return true;
}

// FUNCTION: LLE 0x10007e3f
// Loads the chunk header if needed, then reads and decompresses the chunk data from a file
long FSChunk::LoadChunkFromFile(FSFile& inFile)
{
    EeFsFileOps theOps;
    FSBuildFileOps(inFile, theOps);
    return ee_fs_chunk_load_from_file(&this->mState, &theOps);
}

// FUNCTION: LLE 0x10007eb7
// Creates the persistent object registered for the chunk's tag and loads it from the chunk data
long FSChunk::LoadPersistentFromChunk(FSPersistent*& outPersistent)
{
    if (this->mState.data == NULL)
        return kErrorNotSuspended;
    outPersistent = FSPersistentList::Instance()->CreateDefaultPersistent(this->mState.tag);
    if (outPersistent == NULL)
        return kErrorNotFound;
    FSMemoryFile theData(this->mState.data, false);
    FSFile* theFile = this->BuildFileToUse(&theData);
    return outPersistent->Load(this->mState.major, this->mState.minor, *theFile);
}

// FUNCTION: LLE 0x10007efd
// Reads the chunk header (versions, tag, size); a size of 2 GB or more fails
long FSChunk::LoadChunkHeader(FSFile& inFile)
{
    EeFsFileOps theOps;
    FSBuildFileOps(inFile, theOps);
    return ee_fs_chunk_load_header(&this->mState, &theOps);
}

// FUNCTION: LLE 0x10007fab
// Writes the chunk header and its data to a file
long FSChunk::SaveChunkToFile(FSFile& outFile)
{
    EeFsFileOps theOps;
    FSBuildFileOps(outFile, theOps);
    return ee_fs_chunk_save_to_file(&this->mState, &theOps);
}

// FUNCTION: LLE 0x10007fce
// Saves a persistent object into new compressed chunk data; returns the save's error when it fails
long FSChunk::SavePersistentToChunk(unsigned long inMajorVersion, unsigned long inMinorVersion,
                                    FSPersistent* inPersistent)
{
    EeFsMemFile* theData = ee_fs_chunk_begin_save(&this->mState);
    if (theData == NULL)
        return kErrorOK;   // a payload exists already
    FSMemoryFile theFile(theData, false);
    FSFile* theTarget = this->BuildFileToUse(&theFile);
    long theResult = inPersistent->Save(inMajorVersion, inMinorVersion, *theTarget);
    if (theResult != kErrorOK)
        return theResult;   // Fixed: a failed save used to be reported as success
    ee_fs_chunk_finish_save(&this->mState, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                            (int32_t)inPersistent->GetTagID());
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
    EeFsFileOps theOps;
    FSBuildFileOps(inFile, theOps);
    return ee_fs_chunk_skip(&this->mState, &theOps);
}
