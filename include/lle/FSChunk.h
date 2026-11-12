// C:\EEX\Low-Level Engine\File System\FSChunk.h (file name inferred): one compressed FSPersistent in a file: a header
// (major/minor version, tag, size) and the data in an FSMemoryFile. FSChunk.cpp 0x7cb5..0x80b9; vtable 0x1009754c.
// Requires UString (the units that call into FSMemoryFile/FSDebugFile/FSPersistent include those).
#ifndef LLE_FSCHUNK_H
#define LLE_FSCHUNK_H

#include "rust/ee_fs.h"                   // EeFsChunk (rust/crates/ee_fs/src/chunk.rs), embedded below

class FSDebugFile;
class FSFile;
class FSPersistent;

class LLE_API FSChunk {                     // sizeof 0x1c
public:
    FSChunk();                                                                  // 0x7db9
    virtual ~FSChunk();                                                         // 0x7ddc
    // implicit copy ctor (0x41b2) and operator= (0x41f1), emitted with File System.cpp's inlines

    static void ClearDebugFile();                                               // 0x7e11
    bool GetChunkSize(unsigned long& outSize) const;                            // 0x7e2a
    long LoadChunkFromFile(FSFile& inFile);                                     // 0x7e3f
    long LoadPersistentFromChunk(FSPersistent*& outPersistent);                 // 0x7eb7
    long LoadChunkHeader(FSFile& inFile);                                       // 0x7efd
    long SaveChunkToFile(FSFile& outFile);                                      // 0x7fab
    long SavePersistentToChunk(unsigned long inMajorVersion, unsigned long inMinorVersion,
                               FSPersistent* inPersistent);                     // 0x7fce
    static void SetDebugFile(FSDebugFile* inFile);                              // 0x8054
    long SkipChunk(FSFile& inFile);                                             // 0x805e

private:
    FSFile* BuildFileToUse(FSFile* inFile);                                     // 0x7df8

    EeFsChunk mState;                       // the header fields and the payload (the C++ copy copies it as before)

    static FSDebugFile* sDebugFile;                     // 0x1010f768
    static const UString sChunkSizeStringID;            // 0x1010f758 "FSChunk-ChunkSize"
    static const UString sMajorVersionStringID;         // 0x1010f740 "FSChunk-MajorVersion"
    static const UString sMinorVersionStringID;         // 0x1010f730 "FSChunk-MinorVersion"
    static const UString sTagStringID;                  // 0x1010f720 "FSChunk-TagID"
};

#endif
