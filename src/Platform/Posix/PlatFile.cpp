// Platform layer, POSIX: files, directories and paths (include/platform/PlatFile.h). Every path is a game path,
// resolved by PlatPosix::ResolvePath (PlatPath.cpp: volumes, separators, case-insensitive lookup). A PlatFile is a
// file descriptor, a PlatFind a sorted snapshot of the directory's matching entries (NTFS lists a directory in
// case-insensitive order; readdir has no order at all), a PlatFileMapping an mmap'ed view.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   // glibc: fopencookie (PlatFOpen's text mode)
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <new>
#include <string>
#include <vector>

#include "platform/PlatFile.h"
#include "platform/PlatPath.h"
#include "PlatPosix.h"

using PlatPosix::FailErrno;
using PlatPosix::Fail;

struct PlatFile {
    int mDescriptor;
};

struct PlatFileMapping {
    void* mAddress;
    size_t mLength;
};

struct PlatFind {
    std::vector<PlatFindData> mEntries;
    size_t mNext;
};

namespace {

uint32_t AttributesOf(const struct stat& inStat, const char* inName)
{
    uint32_t theAttributes = 0;
    if (S_ISDIR(inStat.st_mode))
        theAttributes |= PLAT_ATTR_DIRECTORY;
    else
        theAttributes |= PLAT_ATTR_ARCHIVE;   // what NTFS reports for a file nobody has backed up
    if (!(inStat.st_mode & S_IWUSR))
        theAttributes |= PLAT_ATTR_READONLY;
    if (inName && inName[0] == '.' && strcmp(inName, ".") != 0 && strcmp(inName, "..") != 0)
        theAttributes |= PLAT_ATTR_HIDDEN;
    return theAttributes;
}

const char* BaseName(const std::string& inPath)
{
    size_t theSlash = inPath.rfind('/');
    return inPath.c_str() + (theSlash == std::string::npos ? 0 : theSlash + 1);
}

bool ParentExists(const std::string& inNative)
{
    size_t theSlash = inNative.rfind('/');
    if (theSlash == std::string::npos || theSlash == 0)
        return true;
    struct stat theStat;
    return stat(inNative.substr(0, theSlash).c_str(), &theStat) == 0 && S_ISDIR(theStat.st_mode);
}

// errno of a failed open/create of inNative, with ENOENT split into "file" and "path" as Win32 reports them
int FailPath(const std::string& inNative)
{
    if (errno == ENOENT && !ParentExists(inNative))
        return Fail(PlatPosix::kErrorPathNotFound);
    return FailErrno();
}

uint64_t FileTimeOf(const struct stat& inStat)
{
#if defined(__APPLE__)
    int64_t theSeconds = inStat.st_mtimespec.tv_sec;
    int64_t theNanoseconds = inStat.st_mtimespec.tv_nsec;
#else
    int64_t theSeconds = inStat.st_mtim.tv_sec;
    int64_t theNanoseconds = inStat.st_mtim.tv_nsec;
#endif
    // FILETIME: 100 ns intervals since 1601-01-01 (11644473600 s before the Unix epoch)
    return static_cast<uint64_t>(theSeconds + 11644473600LL) * 10000000ULL
        + static_cast<uint64_t>(theNanoseconds / 100);
}

bool NativeOf(const char* inPath, std::string& outNative)
{
    return PlatPosix::ResolvePath(inPath, outNative);
}

}   // namespace

// ---- paths

extern "C" uint32_t PlatPathGetAttributes(const char* inPath)
{
    std::string theNative;
    struct stat theStat;
    if (!NativeOf(inPath, theNative))
        return PLAT_ATTR_INVALID;
    if (stat(theNative.c_str(), &theStat) != 0) {
        FailPath(theNative);
        return PLAT_ATTR_INVALID;
    }
    return AttributesOf(theStat, BaseName(theNative));
}

extern "C" int PlatPathSetAttributes(const char* inPath, uint32_t inAttributes)
{
    std::string theNative;
    struct stat theStat;
    if (!NativeOf(inPath, theNative))
        return 0;
    if (stat(theNative.c_str(), &theStat) != 0)
        return FailPath(theNative);
    // only the read-only bit has a native meaning: it is the owner's write permission
    mode_t theMode = theStat.st_mode & 07777;
    if (inAttributes & PLAT_ATTR_READONLY)
        theMode &= ~static_cast<mode_t>(S_IWUSR | S_IWGRP | S_IWOTH);
    else
        theMode |= S_IWUSR;
    if (chmod(theNative.c_str(), theMode) != 0)
        return FailErrno();
    return 1;
}

extern "C" int PlatDeleteFile(const char* inPath)
{
    std::string theNative;
    if (!NativeOf(inPath, theNative))
        return 0;
    struct stat theStat;
    if (lstat(theNative.c_str(), &theStat) != 0)
        return FailPath(theNative);
    if (S_ISDIR(theStat.st_mode))
        return Fail(PlatPosix::kErrorAccessDenied);   // DeleteFile on a directory
    if (!(theStat.st_mode & S_IWUSR))
        return Fail(PlatPosix::kErrorAccessDenied);   // Win32 refuses to delete a read-only file
    if (unlink(theNative.c_str()) != 0)
        return FailErrno();
    return 1;
}

extern "C" int PlatCopyFile(const char* inSource, const char* inDestination, int inFailIfExists)
{
    std::string theSource, theDestination;
    if (!NativeOf(inSource, theSource) || !NativeOf(inDestination, theDestination))
        return 0;
    int theIn = open(theSource.c_str(), O_RDONLY | O_CLOEXEC);
    if (theIn < 0)
        return FailPath(theSource);
    struct stat theStat;
    if (fstat(theIn, &theStat) != 0) {
        int theError = errno;
        close(theIn);
        errno = theError;
        return FailErrno();
    }
    if (S_ISDIR(theStat.st_mode)) {
        close(theIn);
        return Fail(PlatPosix::kErrorAccessDenied);
    }
    int theFlags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | (inFailIfExists ? O_EXCL : 0);
    int theOut = open(theDestination.c_str(), theFlags, theStat.st_mode & 0777);
    if (theOut < 0) {
        int theError = errno;
        close(theIn);
        errno = theError;
        if (errno == EEXIST)
            return Fail(PlatPosix::kErrorFileExists);
        return FailPath(theDestination);
    }
    char theBuffer[65536];
    for (;;) {
        ssize_t theRead = read(theIn, theBuffer, sizeof(theBuffer));
        if (theRead == 0)
            break;
        if (theRead < 0) {
            if (errno == EINTR)
                continue;
            int theError = errno;
            close(theIn);
            close(theOut);
            errno = theError;
            return FailErrno();
        }
        for (ssize_t theDone = 0; theDone < theRead;) {
            ssize_t theWritten = write(theOut, theBuffer + theDone, theRead - theDone);
            if (theWritten < 0) {
                if (errno == EINTR)
                    continue;
                int theError = errno;
                close(theIn);
                close(theOut);
                errno = theError;
                return FailErrno();
            }
            theDone += theWritten;
        }
    }
    // CopyFile keeps the source's write time (and its attributes: the mode was given to open)
    struct timespec theTimes[2];
#if defined(__APPLE__)
    theTimes[0] = theStat.st_atimespec;
    theTimes[1] = theStat.st_mtimespec;
#else
    theTimes[0] = theStat.st_atim;
    theTimes[1] = theStat.st_mtim;
#endif
    futimens(theOut, theTimes);
    fchmod(theOut, theStat.st_mode & 0777);
    close(theIn);
    if (close(theOut) != 0)
        return FailErrno();
    return 1;
}

extern "C" PlatFileResult PlatCreateDirectory(const char* inPath)
{
    std::string theNative;
    if (!NativeOf(inPath, theNative))
        return PLAT_FILE_NATIVE_ERROR;
    if (mkdir(theNative.c_str(), 0777) == 0)
        return PLAT_FILE_OK;
    if (errno == EEXIST) {
        PlatPosix::SetLastError(PlatPosix::kErrorAlreadyExists);
        return PLAT_FILE_EXISTS;
    }
    FailPath(theNative);
    return PLAT_FILE_NATIVE_ERROR;
}

extern "C" int PlatRemoveDirectory(const char* inPath)
{
    std::string theNative;
    if (!NativeOf(inPath, theNative))
        return 0;
    if (rmdir(theNative.c_str()) != 0)
        return errno == ENOTEMPTY || errno == EEXIST ? Fail(PlatPosix::kErrorDirNotEmpty) : FailPath(theNative);
    return 1;
}

extern "C" uint32_t PlatGetCurrentDirectory(char* outPath, uint32_t inSize)
{
    char theNative[4096];
    if (!getcwd(theNative, sizeof(theNative))) {
        FailErrno();
        return 0;
    }
    std::string thePath;
    if (!PlatPosix::GameForm(theNative, thePath))
        return 0;
    // GetCurrentDirectory: the length without the terminator; when the buffer is too small, the size it needs
    if (thePath.size() + 1 > inSize)
        return static_cast<uint32_t>(thePath.size() + 1);
    memcpy(outPath, thePath.c_str(), thePath.size() + 1);
    return static_cast<uint32_t>(thePath.size());
}

extern "C" uint32_t PlatGetTempDirectory(char* outPath, uint32_t inSize)
{
    const char* theTemp = getenv("TMPDIR");
    std::string theNative = theTemp && *theTemp ? theTemp : "/tmp";
    while (theNative.size() > 1 && theNative.back() == '/')
        theNative.pop_back();
    char theReal[4096];
    if (realpath(theNative.c_str(), theReal))
        theNative = theReal;
    std::string thePath;
    if (!PlatPosix::GameForm(theNative, thePath))
        return 0;
    if (thePath.back() != '\\')
        thePath += '\\';
    if (thePath.size() + 1 > inSize)
        return static_cast<uint32_t>(thePath.size() + 1);
    memcpy(outPath, thePath.c_str(), thePath.size() + 1);
    return static_cast<uint32_t>(thePath.size());
}

extern "C" int PlatCreateTempFile(const char* inDirectory, const char* inPrefix, char* outPath)
{
    // GetTempFileName(dir, prefix, 0): "<dir>\<first 3 of prefix><4 hex digits>.TMP", created empty
    std::string theDirectory = inDirectory ? inDirectory : "";
    if (!theDirectory.empty() && theDirectory.back() != '\\' && theDirectory.back() != '/')
        theDirectory += '\\';
    std::string thePrefix = inPrefix ? std::string(inPrefix).substr(0, 3) : std::string();
    static unsigned sCounter;
    unsigned theStart = (static_cast<unsigned>(getpid()) * 7919u + __atomic_add_fetch(&sCounter, 1, __ATOMIC_RELAXED))
        & 0xFFFFu;
    for (unsigned i = 0; i < 0x10000u; i++) {
        unsigned theUnique = (theStart + i) & 0xFFFFu;
        if (theUnique == 0)
            continue;
        char theName[32];
        snprintf(theName, sizeof(theName), "%s%04X.TMP", thePrefix.c_str(), theUnique);
        std::string thePath = theDirectory + theName;
        if (thePath.size() >= PLAT_MAX_PATH)
            return Fail(PlatPosix::kErrorFilenameTooLong);
        std::string theNative;
        if (!NativeOf(thePath.c_str(), theNative))
            return 0;
        int theFile = open(theNative.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        if (theFile >= 0) {
            close(theFile);
            memcpy(outPath, thePath.c_str(), thePath.size() + 1);
            return 1;
        }
        if (errno != EEXIST)
            return FailPath(theNative);
    }
    return Fail(PlatPosix::kErrorFileExists);
}

// ---- files

extern "C" PlatFile* PlatFileOpen(const char* inPath, uint32_t inAccess)
{
    std::string theNative;
    if (!NativeOf(inPath, theNative))
        return NULL;
    int theFlags = (inAccess & PLAT_FILE_WRITE) ? ((inAccess & PLAT_FILE_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
    int theFile = open(theNative.c_str(), theFlags | O_CLOEXEC);
    if (theFile < 0) {
        FailPath(theNative);
        return NULL;
    }
    struct stat theStat;
    if (fstat(theFile, &theStat) == 0 && S_ISDIR(theStat.st_mode)) {
        close(theFile);   // CreateFile without FILE_FLAG_BACKUP_SEMANTICS does not open a directory
        Fail(PlatPosix::kErrorAccessDenied);
        return NULL;
    }
    PlatFile* theHandle = new (std::nothrow) PlatFile;
    if (!theHandle) {
        close(theFile);
        Fail(PlatPosix::kErrorNotEnoughMemory);
        return NULL;
    }
    theHandle->mDescriptor = theFile;
    return theHandle;
}

// ---- PlatFOpen's text mode: the Windows CRT reads a file opened without 'b' with every CR LF turned into LF (the
// game's text files, the random map scripts and the debug files, have CR LF lines; fgets/fscanf callers expect LF).
// A read-only text open goes through this filter; writing is unchanged (LF lines, which the game reads back alike).
namespace {

struct TextReader {
    FILE* mBase;
    int mPending;           // a byte read past a lone CR, -1 when none
    long long mPosition;    // bytes delivered (ftell)
};

long ReadText(TextReader* inReader, char* outData, size_t inSize)
{
    size_t n = 0;
    while (n < inSize) {
        int c = inReader->mPending >= 0 ? inReader->mPending : getc(inReader->mBase);
        inReader->mPending = -1;
        if (c == EOF)
            break;
        if (c == '\r') {
            int d = getc(inReader->mBase);
            if (d == '\n')
                c = '\n';
            else
                inReader->mPending = d == EOF ? -1 : d;
        }
        outData[n++] = (char)c;
    }
    inReader->mPosition += (long long)n;
    return ferror(inReader->mBase) && n == 0 ? -1 : (long)n;
}

// Rewind (SEEK_SET 0) and ftell (SEEK_CUR 0) only: a text stream's other offsets have no portable meaning
long long SeekText(TextReader* inReader, long long inOffset, int inWhence)
{
    if (inWhence == SEEK_SET && inOffset == 0) {
        if (fseek(inReader->mBase, 0, SEEK_SET) != 0)
            return -1;
        inReader->mPending = -1;
        inReader->mPosition = 0;
        return 0;
    }
    if (inWhence == SEEK_CUR && inOffset == 0)
        return inReader->mPosition;
    errno = EINVAL;
    return -1;
}

int CloseText(TextReader* inReader)
{
    int theResult = fclose(inReader->mBase);
    delete inReader;
    return theResult;
}

#ifdef __APPLE__
int ReadTextFn(void* inCookie, char* outData, int inSize)
{
    return (int)ReadText(static_cast<TextReader*>(inCookie), outData, (size_t)inSize);
}
fpos_t SeekTextFn(void* inCookie, fpos_t inOffset, int inWhence)
{
    return (fpos_t)SeekText(static_cast<TextReader*>(inCookie), (long long)inOffset, inWhence);
}
int CloseTextFn(void* inCookie)
{
    return CloseText(static_cast<TextReader*>(inCookie));
}
#else
ssize_t ReadTextFn(void* inCookie, char* outData, size_t inSize)
{
    return (ssize_t)ReadText(static_cast<TextReader*>(inCookie), outData, inSize);
}
int SeekTextFn(void* inCookie, off64_t* ioOffset, int inWhence)
{
    long long theResult = SeekText(static_cast<TextReader*>(inCookie), (long long)*ioOffset, inWhence);
    if (theResult < 0)
        return -1;
    *ioOffset = (off64_t)theResult;
    return 0;
}
int CloseTextFn(void* inCookie)
{
    return CloseText(static_cast<TextReader*>(inCookie));
}
#endif

FILE* OpenTextReader(FILE* inBase)
{
    TextReader* theReader = new TextReader{inBase, -1, 0};
#ifdef __APPLE__
    FILE* theStream = funopen(theReader, ReadTextFn, NULL, SeekTextFn, CloseTextFn);
#else
    cookie_io_functions_t theFunctions = {ReadTextFn, NULL, SeekTextFn, CloseTextFn};
    FILE* theStream = fopencookie(theReader, "r", theFunctions);
#endif
    if (!theStream) {
        fclose(inBase);
        delete theReader;
    }
    return theStream;
}

}   // namespace

extern "C" FILE* PlatFOpen(const char* inPath, const char* inMode)
{
    std::string theNative;
    if (!inMode || !NativeOf(inPath, theNative))
        return NULL;
    // the CRT's mode letters: 'b' binary, 't' text (the default); the C library takes neither 't' nor a text mode
    std::string theMode;
    bool theBinary = false;
    for (const char* p = inMode; *p; p++) {
        if (*p == 'b')
            theBinary = true;
        if (*p != 't')
            theMode += *p;
    }
    bool theTextRead = !theBinary && theMode.find('r') != std::string::npos && theMode.find('+') == std::string::npos;
    if (theTextRead)
        theMode += 'b';
    FILE* theStream = fopen(theNative.c_str(), theMode.c_str());
    if (!theStream) {
        FailErrno();
        return NULL;
    }
    if (theTextRead)
        theStream = OpenTextReader(theStream);
    return theStream;
}

extern "C" PlatFileResult PlatFileCreateNew(const char* inPath)
{
    std::string theNative;
    if (!NativeOf(inPath, theNative))
        return PLAT_FILE_BAD_PATH;
    int theFile = open(theNative.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (theFile >= 0) {
        close(theFile);
        return PLAT_FILE_OK;
    }
    if (errno == EEXIST) {
        PlatPosix::SetLastError(PlatPosix::kErrorFileExists);
        return PLAT_FILE_EXISTS;
    }
    if (errno == ENOENT || errno == ENOTDIR || errno == ENAMETOOLONG) {
        PlatPosix::SetLastError(PlatPosix::kErrorPathNotFound);
        return PLAT_FILE_BAD_PATH;
    }
    FailErrno();
    return PLAT_FILE_NATIVE_ERROR;
}

extern "C" int PlatFileClose(PlatFile* inFile)
{
    if (!inFile)
        return Fail(PlatPosix::kErrorInvalidHandle);
    int theResult = close(inFile->mDescriptor);
    delete inFile;
    return theResult == 0 ? 1 : FailErrno();
}

extern "C" int PlatFileRead(PlatFile* inFile, void* outData, uint32_t inLength, uint32_t* outRead)
{
    *outRead = 0;
    uint32_t theDone = 0;
    while (theDone < inLength) {
        ssize_t theRead = read(inFile->mDescriptor, static_cast<char*>(outData) + theDone, inLength - theDone);
        if (theRead == 0)
            break;
        if (theRead < 0) {
            if (errno == EINTR)
                continue;
            *outRead = theDone;
            return FailErrno();
        }
        theDone += static_cast<uint32_t>(theRead);
    }
    *outRead = theDone;
    return 1;
}

extern "C" int PlatFileWrite(PlatFile* inFile, const void* inData, uint32_t inLength, uint32_t* outWritten)
{
    *outWritten = 0;
    uint32_t theDone = 0;
    while (theDone < inLength) {
        ssize_t theWritten = write(inFile->mDescriptor, static_cast<const char*>(inData) + theDone,
                                   inLength - theDone);
        if (theWritten < 0) {
            if (errno == EINTR)
                continue;
            *outWritten = theDone;
            return FailErrno();
        }
        theDone += static_cast<uint32_t>(theWritten);
    }
    *outWritten = theDone;
    return 1;
}

extern "C" uint32_t PlatFileGetSize(PlatFile* inFile)
{
    struct stat theStat;
    if (fstat(inFile->mDescriptor, &theStat) != 0) {
        FailErrno();
        return 0xFFFFFFFFu;
    }
    return static_cast<uint32_t>(theStat.st_size);
}

extern "C" uint32_t PlatFileSeek(PlatFile* inFile, int32_t inOffset, PlatFileOrigin inOrigin)
{
    int theWhence = inOrigin == PLAT_SEEK_BEGIN ? SEEK_SET : inOrigin == PLAT_SEEK_CURRENT ? SEEK_CUR : SEEK_END;
    off_t thePosition = lseek(inFile->mDescriptor, inOffset, theWhence);
    if (thePosition < 0) {
        if (errno == EINVAL)
            PlatPosix::SetLastError(131);   // ERROR_NEGATIVE_SEEK
        else
            FailErrno();
        return 0xFFFFFFFFu;
    }
    return static_cast<uint32_t>(thePosition);
}

extern "C" int PlatFileTruncate(PlatFile* inFile)
{
    off_t thePosition = lseek(inFile->mDescriptor, 0, SEEK_CUR);
    if (thePosition < 0 || ftruncate(inFile->mDescriptor, thePosition) != 0)
        return FailErrno();
    return 1;
}

extern "C" int PlatFileGetWriteTime(PlatFile* inFile, uint64_t* outTime)
{
    struct stat theStat;
    if (fstat(inFile->mDescriptor, &theStat) != 0)
        return FailErrno();
    *outTime = FileTimeOf(theStat);
    return 1;
}

extern "C" PlatFileMapping* PlatFileMap(PlatFile* inFile, const void** outView)
{
    struct stat theStat;
    if (fstat(inFile->mDescriptor, &theStat) != 0) {
        FailErrno();
        return NULL;
    }
    if (theStat.st_size == 0) {
        Fail(PlatPosix::kErrorFileInvalid);   // CreateFileMapping of an empty file: ERROR_FILE_INVALID
        return NULL;
    }
    size_t theLength = static_cast<size_t>(theStat.st_size);
    void* theView = mmap(NULL, theLength, PROT_READ, MAP_PRIVATE, inFile->mDescriptor, 0);
    if (theView == MAP_FAILED) {
        FailErrno();
        return NULL;
    }
    PlatFileMapping* theMapping = new (std::nothrow) PlatFileMapping;
    if (!theMapping) {
        munmap(theView, theLength);
        Fail(PlatPosix::kErrorNotEnoughMemory);
        return NULL;
    }
    theMapping->mAddress = theView;
    theMapping->mLength = theLength;
    *outView = theView;
    return theMapping;
}

extern "C" void PlatFileUnmap(PlatFileMapping* inMapping, const void* inView)
{
    (void)inView;
    if (!inMapping)
        return;
    munmap(inMapping->mAddress, inMapping->mLength);
    delete inMapping;
}

// ---- directory search

extern "C" PlatFind* PlatFindFirst(const char* inPattern, PlatFindData* outData)
{
    if (!inPattern || !*inPattern) {
        Fail(PlatPosix::kErrorFileNotFound);
        return NULL;
    }
    // split the pattern into its directory and its name pattern (the last component)
    std::string thePattern = inPattern;
    size_t theSeparator = thePattern.find_last_of("\\/");
    std::string theDirectory, theName;
    if (theSeparator == std::string::npos) {
        if (thePattern.size() >= 2 && thePattern[1] == ':') {
            theDirectory = thePattern.substr(0, 2) + "\\";
            theName = thePattern.substr(2);
        } else {
            theDirectory = ".";
            theName = thePattern;
        }
    } else {
        theDirectory = thePattern.substr(0, theSeparator + 1);
        theName = thePattern.substr(theSeparator + 1);
    }
    if (theName.empty()) {
        Fail(PlatPosix::kErrorFileNotFound);   // FindFirstFile("dir\") fails
        return NULL;
    }
    std::string theNative;
    if (!NativeOf(theDirectory.c_str(), theNative))
        return NULL;
    DIR* theDir = opendir(theNative.c_str());
    if (!theDir) {
        if (errno == ENOENT || errno == ENOTDIR)
            Fail(PlatPosix::kErrorPathNotFound);
        else
            FailErrno();
        return NULL;
    }
    bool theWild = theName.find_first_of("*?") != std::string::npos;
    PlatFind* theFind = new (std::nothrow) PlatFind;
    if (!theFind) {
        closedir(theDir);
        Fail(PlatPosix::kErrorNotEnoughMemory);
        return NULL;
    }
    theFind->mNext = 0;
    while (struct dirent* theEntry = readdir(theDir)) {
        bool theMatch = theWild ? PlatPosix::WildcardMatch(theName.c_str(), theEntry->d_name)
                                : PlatPosix::CompareNoCase(theName.c_str(), theEntry->d_name) == 0;
        if (!theMatch)
            continue;
        if (strlen(theEntry->d_name) >= PLAT_MAX_PATH)
            continue;
        struct stat theStat;
        std::string theEntryPath = theNative + "/" + theEntry->d_name;
        if (stat(theEntryPath.c_str(), &theStat) != 0)
            continue;   // a dangling link
        PlatFindData theData;
        strcpy(theData.mName, theEntry->d_name);
        theData.mAttributes = AttributesOf(theStat, theEntry->d_name);
        theFind->mEntries.push_back(theData);
    }
    closedir(theDir);
    std::sort(theFind->mEntries.begin(), theFind->mEntries.end(), [](const PlatFindData& a, const PlatFindData& b) {
        int c = PlatPosix::CompareNoCase(a.mName, b.mName);
        return c != 0 ? c < 0 : strcmp(a.mName, b.mName) < 0;
    });
    if (theFind->mEntries.empty()) {
        delete theFind;
        Fail(PlatPosix::kErrorFileNotFound);
        return NULL;
    }
    *outData = theFind->mEntries[theFind->mNext++];
    return theFind;
}

extern "C" PlatFileResult PlatFindNext(PlatFind* inFind, PlatFindData* outData)
{
    if (!inFind) {
        Fail(PlatPosix::kErrorInvalidHandle);
        return PLAT_FILE_NATIVE_ERROR;
    }
    if (inFind->mNext >= inFind->mEntries.size()) {
        PlatPosix::SetLastError(PlatPosix::kErrorNoMoreFiles);
        return PLAT_FILE_NO_MORE;
    }
    *outData = inFind->mEntries[inFind->mNext++];
    return PLAT_FILE_OK;
}

extern "C" void PlatFindClose(PlatFind* inFind)
{
    delete inFind;   // touches no error state: a failed PlatFindNext's error survives
}
