// Platform layer, Win32: paths (include/platform/PlatPath.h). The game's paths are native here: the conversions
// copy, volumes are real, the data root is the executable's directory.
#include <windows.h>
#include <string.h>

#include "platform/PlatPath.h"

static uint32_t CopyPath(const char* inPath, char* outPath, uint32_t inSize)
{
    if (!inPath || !outPath) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    size_t theLength = strlen(inPath);
    if (theLength == 0 || theLength + 1 > inSize) {
        SetLastError(theLength == 0 ? ERROR_PATH_NOT_FOUND : ERROR_FILENAME_EXCED_RANGE);
        return 0;
    }
    memcpy(outPath, inPath, theLength + 1);
    return static_cast<uint32_t>(theLength);
}

extern "C" uint32_t PlatPathToNative(const char* inPath, char* outNative, uint32_t inSize)
{
    return CopyPath(inPath, outNative, inSize);
}

extern "C" uint32_t PlatPathFromNative(const char* inNative, char* outPath, uint32_t inSize)
{
    return CopyPath(inNative, outPath, inSize);
}

extern "C" int PlatPathSetVolumeRoot(char inVolume, const char* inNativeRoot)
{
    (void)inVolume;
    (void)inNativeRoot;
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}

// The executable's directory with a trailing backslash -> its length, 0 on failure
static uint32_t ExecutableDirectory(char* outPath, uint32_t inSize)
{
    char theModule[MAX_PATH];
    DWORD theLength = GetModuleFileNameA(NULL, theModule, MAX_PATH);
    if (theLength == 0 || theLength >= MAX_PATH)
        return 0;
    char* theSlash = strrchr(theModule, '\\');
    if (!theSlash)
        return 0;
    theSlash[1] = 0;
    return CopyPath(theModule, outPath, inSize);
}

extern "C" uint32_t PlatPathGetDataRoot(char* outPath, uint32_t inSize)
{
    char theEnv[MAX_PATH];
    DWORD theLength = GetEnvironmentVariableA("EE_DATA_ROOT", theEnv, MAX_PATH);
    if (theLength > 0 && theLength < MAX_PATH - 1) {
        if (theEnv[theLength - 1] != '\\' && theEnv[theLength - 1] != '/') {
            theEnv[theLength] = '\\';
            theEnv[theLength + 1] = 0;
        }
        return CopyPath(theEnv, outPath, inSize);
    }
    return ExecutableDirectory(outPath, inSize);
}

extern "C" int PlatPathEnterDataRoot(void)
{
    char theRoot[MAX_PATH];
    if (!PlatPathGetDataRoot(theRoot, MAX_PATH))
        return 0;
    return SetCurrentDirectoryA(theRoot) ? 1 : 0;
}

extern "C" uint32_t PlatPathGetExecutableName(char* outName, uint32_t inSize)
{
    char theModule[MAX_PATH];
    DWORD theLength = GetModuleFileNameA(NULL, theModule, MAX_PATH);
    if (theLength == 0 || theLength >= MAX_PATH)
        return 0;
    const char* theName = strrchr(theModule, '\\');
    theName = theName ? theName + 1 : theModule;
    size_t n = strlen(theName);
    if (n + 1 > inSize)
        return 0;
    memcpy(outName, theName, n + 1);
    return (uint32_t)n;
}
