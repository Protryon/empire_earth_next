// C:\EEX\Low-Level Engine\File System\FSChunk.h (file name inferred): one compressed FSPersistent in a file: a header
// (major/minor version, tag, size) and the data in an FSMemoryFile. FSChunk.cpp 0x7cb5..0x80b9; vtable 0x1009754c.
// Requires UString (the units that call into FSMemoryFile/FSPersistent include those).
#ifndef LLE_FSCHUNK_H
#define LLE_FSCHUNK_H

#include "rust/ee_fs.h"                   // EeFsChunk (rust/crates/ee_fs/src/chunk.rs), embedded below

class FSFile;
class FSPersistent;

class LLE_API FSChunk {                     // sizeof 0x1c
public:
    FSChunk();                                                                  // 0x7db9
    virtual ~FSChunk();                                                         // 0x7ddc
    // implicit copy ctor (0x41b2) and operator= (0x41f1), emitted with File System.cpp's inlines

    bool GetChunkSize(unsigned long& outSize) const;                            // 0x7e2a
    long LoadChunkFromFile(FSFile& inFile);                                     // 0x7e3f
    long LoadPersistentFromChunk(FSPersistent*& outPersistent);                 // 0x7eb7
    long LoadChunkHeader(FSFile& inFile);                                       // 0x7efd
    long SaveChunkToFile(FSFile& outFile);                                      // 0x7fab
    long SavePersistentToChunk(unsigned long inMajorVersion, unsigned long inMinorVersion,
                               FSPersistent* inPersistent);                     // 0x7fce
    long SkipChunk(FSFile& inFile);                                             // 0x805e

private:
    // The debug-file hooks (a never-set static text mirror, FSDebugFile) and the four static UString labels only it
    // wrote are gone (LLE lane round 1)
    EeFsChunk mState;                       // the header fields and the payload (the C++ copy copies it as before)
};

#endif
