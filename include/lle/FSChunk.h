// C:\EEX\Low-Level Engine\File System\FSChunk.h (file name inferred): one compressed FSPersistent in a file: a header
// (major/minor version, tag, size) and the data in an FSMemoryFile. FSChunk.cpp 0x7cb5..0x80b9; vtable 0x1009754c.
// Requires UString and lle/FSFileSpec.h.
#ifndef LLE_FSCHUNK_H
#define LLE_FSCHUNK_H

#include "rust/base/fs.h"                   // EeFsChunk (rust/crates/ee_base/src/fs/chunk.rs), embedded below
#include "lle/FSMemoryFile.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/FSPersistentList.h"

// The class forwards to Rust (rust/crates/ee_base/src/fs/chunk.rs, cppobj.rs: the persistent's virtual calls through
// File System.cpp's table).
class LLE_API FSChunk {                     // sizeof 0x1c
public:
    FSChunk() { ee_fs_chunk_init(&this->mState); }                // empty: no header or data loaded
    virtual ~FSChunk() { ee_fs_chunk_free(&this->mState); }
    // implicit copy ctor (0x41b2) and operator= (0x41f1), emitted with File System.cpp's inlines

    // the chunk's size once its header is loaded; false before that
    bool GetChunkSize(unsigned long& outSize) const { return ee_fs_chunk_size(&this->mState, &outSize); }
    // the header if needed, then the chunk data read and decompressed
    long LoadChunkFromFile(FSFile& inFile) { return ee_fs_chunk_load_from_file(&this->mState, &inFile); }
    // creates the persistent object registered for the chunk's tag and loads it from the chunk data
    long LoadPersistentFromChunk(FSPersistent*& outPersistent) { return ee_fs_chunk_load_persistent(&this->mState, reinterpret_cast<void**>(&outPersistent)); }
    // the chunk header (versions, tag, size); a size of 2 GB or more fails
    long LoadChunkHeader(FSFile& inFile) { return ee_fs_chunk_load_header(&this->mState, &inFile); }
    // the header and the data
    long SaveChunkToFile(FSFile& outFile) { return ee_fs_chunk_save_to_file(&this->mState, &outFile); }
    // saves a persistent object into new compressed chunk data; the save's error when it fails
    long SavePersistentToChunk(unsigned long inMajorVersion, unsigned long inMinorVersion, FSPersistent* inPersistent)
        { return ee_fs_chunk_save_persistent(&this->mState, inMajorVersion, inMinorVersion, inPersistent); }
    // over the chunk's data, loading the header first if needed
    long SkipChunk(FSFile& inFile) { return ee_fs_chunk_skip(&this->mState, &inFile); }

    // The debug-file hooks (a never-set static text mirror, FSDebugFile) and the four static UString labels only it
    // wrote are gone (LLE lane round 1)
    EeFsChunk mState;                       // the header fields and the payload (the C++ copy copies it as before)
};

#endif
