// Platform layer, Win32: files, directories and paths (include/platform/PlatFile.h). The ANSI file functions the
// engine called itself before, with the same arguments. A PlatFile* is the file HANDLE, a PlatFind* the search
// HANDLE, a PlatFileMapping* the mapping HANDLE.
#include <windows.h>
#include <string.h>

#include "platform/PlatFile.h"

static HANDLE FileHandle(PlatFile* inFile)
{
    return reinterpret_cast<HANDLE>(inFile);
}

static void CopyFindData(const WIN32_FIND_DATAA& inData, PlatFindData* outData)
{
    strncpy(outData->mName, inData.cFileName, PLAT_MAX_PATH - 1);
    outData->mName[PLAT_MAX_PATH - 1] = 0;
    outData->mAttributes = inData.dwFileAttributes;
}

extern "C" uint32_t PlatPathGetAttributes(const char* inPath)
{
    return GetFileAttributesA(inPath);
}

extern "C" int PlatPathSetAttributes(const char* inPath, uint32_t inAttributes)
{
    return SetFileAttributesA(inPath, inAttributes) ? 1 : 0;
}

extern "C" int PlatDeleteFile(const char* inPath)
{
    return DeleteFileA(inPath) ? 1 : 0;
}

extern "C" int PlatCopyFile(const char* inSource, const char* inDestination, int inFailIfExists)
{
    return CopyFileA(inSource, inDestination, inFailIfExists ? TRUE : FALSE) ? 1 : 0;
}

extern "C" PlatFileResult PlatCreateDirectory(const char* inPath)
{
    if (CreateDirectoryA(inPath, NULL))
        return PLAT_FILE_OK;
    return GetLastError() == ERROR_ALREADY_EXISTS ? PLAT_FILE_EXISTS : PLAT_FILE_NATIVE_ERROR;
}

extern "C" int PlatRemoveDirectory(const char* inPath)
{
    return RemoveDirectoryA(inPath) ? 1 : 0;
}

extern "C" uint32_t PlatGetCurrentDirectory(char* outPath, uint32_t inSize)
{
    return GetCurrentDirectoryA(inSize, outPath);
}

extern "C" uint32_t PlatGetTempDirectory(char* outPath, uint32_t inSize)
{
    return GetTempPathA(inSize, outPath);
}

extern "C" int PlatCreateTempFile(const char* inDirectory, const char* inPrefix, char* outPath)
{
    return GetTempFileNameA(inDirectory, inPrefix, 0, outPath) != 0 ? 1 : 0;
}

extern "C" PlatFile* PlatFileOpen(const char* inPath, uint32_t inAccess)
{
    DWORD theAccess = 0;
    if (inAccess & PLAT_FILE_READ)
        theAccess |= GENERIC_READ;
    if (inAccess & PLAT_FILE_WRITE)
        theAccess |= GENERIC_WRITE;
    HANDLE theHandle = CreateFileA(inPath, theAccess, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                                   NULL);
    return theHandle == INVALID_HANDLE_VALUE ? NULL : reinterpret_cast<PlatFile*>(theHandle);
}

extern "C" PlatFileResult PlatFileCreateNew(const char* inPath)
{
    HANDLE theHandle = CreateFileA(inPath, 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (theHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(theHandle);
        return PLAT_FILE_OK;
    }
    switch (GetLastError()) {
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_NAME:
        return PLAT_FILE_BAD_PATH;
    case ERROR_FILE_EXISTS:
        return PLAT_FILE_EXISTS;
    default:
        return PLAT_FILE_NATIVE_ERROR;
    }
}

extern "C" int PlatFileClose(PlatFile* inFile)
{
    return CloseHandle(FileHandle(inFile)) ? 1 : 0;
}

extern "C" int PlatFileRead(PlatFile* inFile, void* outData, uint32_t inLength, uint32_t* outRead)
{
    DWORD theRead = 0;
    BOOL theSucceeded = ReadFile(FileHandle(inFile), outData, inLength, &theRead, NULL);
    *outRead = theRead;
    return theSucceeded == TRUE ? 1 : 0;
}

extern "C" int PlatFileWrite(PlatFile* inFile, const void* inData, uint32_t inLength, uint32_t* outWritten)
{
    DWORD theWritten = 0;
    BOOL theSucceeded = WriteFile(FileHandle(inFile), inData, inLength, &theWritten, NULL);
    *outWritten = theWritten;
    return theSucceeded ? 1 : 0;
}

extern "C" uint32_t PlatFileGetSize(PlatFile* inFile)
{
    return GetFileSize(FileHandle(inFile), NULL);
}

extern "C" uint32_t PlatFileSeek(PlatFile* inFile, int32_t inOffset, PlatFileOrigin inOrigin)
{
    DWORD theMethod = inOrigin == PLAT_SEEK_BEGIN ? FILE_BEGIN : inOrigin == PLAT_SEEK_CURRENT ? FILE_CURRENT : FILE_END;
    return SetFilePointer(FileHandle(inFile), inOffset, NULL, theMethod);
}

extern "C" int PlatFileTruncate(PlatFile* inFile)
{
    return SetEndOfFile(FileHandle(inFile)) ? 1 : 0;
}

extern "C" int PlatFileGetWriteTime(PlatFile* inFile, uint64_t* outTime)
{
    FILETIME theTime;
    if (!GetFileTime(FileHandle(inFile), NULL, NULL, &theTime))
        return 0;
    *outTime = (static_cast<uint64_t>(theTime.dwHighDateTime) << 32) | theTime.dwLowDateTime;
    return 1;
}

extern "C" PlatFileMapping* PlatFileMap(PlatFile* inFile, const void** outView)
{
    HANDLE theMapping = CreateFileMappingA(FileHandle(inFile), NULL, PAGE_READONLY | SEC_COMMIT, 0, 0, NULL);
    if (theMapping == NULL)
        return NULL;
    const void* theView = MapViewOfFile(theMapping, FILE_MAP_READ, 0, 0, 0);
    if (theView == NULL) {
        DWORD theError = GetLastError();
        CloseHandle(theMapping);
        SetLastError(theError);
        return NULL;
    }
    *outView = theView;
    return reinterpret_cast<PlatFileMapping*>(theMapping);
}

extern "C" void PlatFileUnmap(PlatFileMapping* inMapping, const void* inView)
{
    UnmapViewOfFile(inView);
    CloseHandle(reinterpret_cast<HANDLE>(inMapping));
}

extern "C" PlatFind* PlatFindFirst(const char* inPattern, PlatFindData* outData)
{
    WIN32_FIND_DATAA theData = {0};
    HANDLE theHandle = FindFirstFileA(inPattern, &theData);
    if (theHandle == INVALID_HANDLE_VALUE)
        return NULL;
    CopyFindData(theData, outData);
    return reinterpret_cast<PlatFind*>(theHandle);
}

extern "C" PlatFileResult PlatFindNext(PlatFind* inFind, PlatFindData* outData)
{
    WIN32_FIND_DATAA theData = {0};
    if (FindNextFileA(reinterpret_cast<HANDLE>(inFind), &theData)) {
        CopyFindData(theData, outData);
        return PLAT_FILE_OK;
    }
    return GetLastError() == ERROR_NO_MORE_FILES ? PLAT_FILE_NO_MORE : PLAT_FILE_NATIVE_ERROR;
}

extern "C" void PlatFindClose(PlatFind* inFind)
{
    DWORD theError = GetLastError();   // a failed PlatFindNext's error survives the close
    FindClose(reinterpret_cast<HANDLE>(inFind));
    SetLastError(theError);
}
