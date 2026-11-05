// Platform layer, POSIX (Linux, macOS): what the implementation files share. Not part of the interface.
#ifndef PLATFORM_POSIX_PLATPOSIX_H
#define PLATFORM_POSIX_PLATPOSIX_H

#include <stdint.h>
#include <string>

// ---- the last error (PlatGetLastError): one value per thread, set by every platform function that fails.
// The values are the Win32 error codes the engine already records and a few callers compare against
// (ERROR_FILE_NOT_FOUND 2, ERROR_PATH_NOT_FOUND 3, ERROR_ACCESS_DENIED 5, ERROR_ALREADY_EXISTS 183, ...), so a
// trace reads the same on every system. An errno without a Win32 twin is 0x20000000 | errno (the Win32
// "customer" bit: never a system code).
namespace PlatPosix {

enum : uint32_t {
    kErrorFileNotFound = 2,
    kErrorPathNotFound = 3,
    kErrorTooManyOpenFiles = 4,
    kErrorAccessDenied = 5,
    kErrorInvalidHandle = 6,
    kErrorNotEnoughMemory = 8,
    kErrorWriteProtect = 19,
    kErrorNotSameDevice = 17,
    kErrorNoMoreFiles = 18,
    kErrorFileExists = 80,
    kErrorInvalidParameter = 87,
    kErrorDiskFull = 112,
    kErrorDirNotEmpty = 145,
    kErrorBusy = 170,
    kErrorAlreadyExists = 183,
    kErrorFilenameTooLong = 206,
    kErrorMoreData = 234,
    kErrorNotSupported = 50,
    kErrorFileInvalid = 1006,
    kErrorTimeout = 1460,
};

void SetLastError(uint32_t inError);
uint32_t ErrorFromErrno(int inErrno);
// SetLastError(ErrorFromErrno(errno)) -> 0, so a failing function can `return FailErrno();`
int FailErrno();
int Fail(uint32_t inError);

// ---- paths (PlatPath.cpp): the game's Windows-form paths ("C:\Dir\File.ext", "Data\x.dat", any case) onto the
// native file system. Volumes map to native directories (every letter to "/" unless PlatPathSetVolumeRoot says
// otherwise), separators become '/', each existing component is looked up case-insensitively.
// Resolve -> the native path; the components that do not exist are kept as written (a file about to be created).
// false (with the last error set) when the path is empty or too long.
bool ResolvePath(const char* inPath, std::string& outNative);
// The game form of a native absolute path ("/home/u/ee" -> "C:\home\u\ee" when C: maps to "/") -> false when no
// volume root contains it.
bool GameForm(const std::string& inNative, std::string& outPath);
// Case-insensitive (ASCII) wildcard match of a name against a Windows pattern (* and ?; "*.*" matches everything).
bool WildcardMatch(const char* inPattern, const char* inName);
// ASCII case-insensitive compare (the NTFS order is close to an upper-case ordinal compare)
int CompareNoCase(const char* inLeft, const char* inRight);

}   // namespace PlatPosix

#endif
