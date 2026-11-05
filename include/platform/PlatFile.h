// Platform layer: files, directories and paths (see platform/Platform.h).
//
// Paths are byte strings in the game's own form: a volume ("C:"), a directory with backslash separators and a
// trailing backslash, a file name (lle/FSFileSpec.h). The Win32 implementation hands them to the ANSI file functions
// unchanged; another implementation maps them (separators, the volume, case-insensitive lookup of the game's data).
// Lengths and positions are 32-bit, as the game's file classes are.
//
// Results: PlatFileResult codes, or a documented value. After PLAT_FILE_NATIVE_ERROR (and after a NULL handle or a
// "failed" value) PlatGetLastError (platform/PlatThread.h) is the error the engine records.
#ifndef PLATFORM_PLATFILE_H
#define PLATFORM_PLATFILE_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// The longest path the game builds (Win32 MAX_PATH, terminator included)
#define PLAT_MAX_PATH 260

typedef enum PlatFileResult {
    PLAT_FILE_OK = 0,
    PLAT_FILE_EXISTS = 1,                   // the file or directory to create is there already
    PLAT_FILE_BAD_PATH = 2,                 // a directory on the path is missing, or the name is invalid
    PLAT_FILE_NO_MORE = 3,                  // a directory search has no more entries
    PLAT_FILE_NATIVE_ERROR = 4              // anything else: see PlatGetLastError
} PlatFileResult;

// Attribute bits (the Win32 FILE_ATTRIBUTE_* values; another system keeps at least DIRECTORY and READONLY)
#define PLAT_ATTR_READONLY 0x01u
#define PLAT_ATTR_HIDDEN 0x02u
#define PLAT_ATTR_SYSTEM 0x04u
#define PLAT_ATTR_DIRECTORY 0x10u
#define PLAT_ATTR_ARCHIVE 0x20u
#define PLAT_ATTR_NORMAL 0x80u              // no other bit: what a plain file gets when its attributes are reset
#define PLAT_ATTR_INVALID 0xFFFFFFFFu       // PlatPathGetAttributes: no such file or directory

// ---- paths
uint32_t PlatPathGetAttributes(const char* inPath);                    // PLAT_ATTR_* bits or PLAT_ATTR_INVALID
int PlatPathSetAttributes(const char* inPath, uint32_t inAttributes);  // replaces them -> nonzero on success
int PlatDeleteFile(const char* inPath);                                // -> nonzero on success
int PlatCopyFile(const char* inSource, const char* inDestination, int inFailIfExists);   // -> nonzero on success
PlatFileResult PlatCreateDirectory(const char* inPath);                // OK, EXISTS or NATIVE_ERROR
int PlatRemoveDirectory(const char* inPath);                           // an empty one -> nonzero on success
// The current directory into outPath (PLAT_MAX_PATH bytes) -> its length, 0 on failure
uint32_t PlatGetCurrentDirectory(char* outPath, uint32_t inSize);
// The directory for temporary files, with a trailing separator -> its length, 0 on failure
uint32_t PlatGetTempDirectory(char* outPath, uint32_t inSize);
// Creates an empty file with a new name in inDirectory (a name starting with inPrefix) and writes its full path to
// outPath (PLAT_MAX_PATH bytes) -> nonzero on success
int PlatCreateTempFile(const char* inDirectory, const char* inPrefix, char* outPath);

// ---- files
typedef struct PlatFile PlatFile;
#define PLAT_FILE_READ 0x1u
#define PLAT_FILE_WRITE 0x2u
// Opens an existing file for PLAT_FILE_READ and/or PLAT_FILE_WRITE at position 0; other processes may read it
// meanwhile -> NULL on failure.
PlatFile* PlatFileOpen(const char* inPath, uint32_t inAccess);
// Creates a new, empty file (failing when it exists) and closes it again -> OK, EXISTS, BAD_PATH or NATIVE_ERROR.
PlatFileResult PlatFileCreateNew(const char* inPath);
// fopen of a game path (the engine's stdio streams: FSFileSpec::FOpen, the mixer's wave files, the music loader)
// -> the stream, NULL on failure (PlatGetLastError set). Win32: fopen unchanged; POSIX: the path resolved first, and
// a read-only text open (no 'b', no '+') reads CR LF as LF, as the Windows CRT's text mode.
FILE* PlatFOpen(const char* inPath, const char* inMode);
int PlatFileClose(PlatFile* inFile);                                   // -> nonzero on success
// Reads up to inLength bytes -> nonzero on success (*outRead 0 at the end of the file)
int PlatFileRead(PlatFile* inFile, void* outData, uint32_t inLength, uint32_t* outRead);
// Writes inLength bytes -> nonzero on success (*outWritten: the bytes written)
int PlatFileWrite(PlatFile* inFile, const void* inData, uint32_t inLength, uint32_t* outWritten);
// The file's length -> 0xFFFFFFFF on failure
uint32_t PlatFileGetSize(PlatFile* inFile);
// Moves the position by inOffset from the start, the current position or the end -> the new position, 0xFFFFFFFF on
// failure
typedef enum PlatFileOrigin { PLAT_SEEK_BEGIN = 0, PLAT_SEEK_CURRENT = 1, PLAT_SEEK_END = 2 } PlatFileOrigin;
uint32_t PlatFileSeek(PlatFile* inFile, int32_t inOffset, PlatFileOrigin inOrigin);
int PlatFileTruncate(PlatFile* inFile);                                // ends the file at the position -> nonzero ok
// The time of the last write, in a unit that orders correctly (Win32: FILETIME, 100 ns since 1601) -> nonzero ok
int PlatFileGetWriteTime(PlatFile* inFile, uint64_t* outTime);
// A read-only view of the whole file in memory -> its address, NULL on failure; unmap with PlatFileUnmap.
typedef struct PlatFileMapping PlatFileMapping;
PlatFileMapping* PlatFileMap(PlatFile* inFile, const void** outView);
void PlatFileUnmap(PlatFileMapping* inMapping, const void* inView);

// ---- directory search
typedef struct PlatFind PlatFind;
typedef struct PlatFindData {
    char mName[PLAT_MAX_PATH];              // the entry's name (no directory)
    uint32_t mAttributes;                   // PLAT_ATTR_* bits
} PlatFindData;
// Starts a search for inPattern (a directory plus a name with * and ? wildcards); the first entry goes to outData
// (it can be "." or "..") -> NULL when nothing matches or on failure.
PlatFind* PlatFindFirst(const char* inPattern, PlatFindData* outData);
// The next entry -> OK, NO_MORE or NATIVE_ERROR
PlatFileResult PlatFindNext(PlatFind* inFind, PlatFindData* outData);
void PlatFindClose(PlatFind* inFind);

#ifdef __cplusplus
}
#endif

#endif
