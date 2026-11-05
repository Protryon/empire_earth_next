// Platform layer, POSIX: the game's Windows paths on the native file system (include/platform/PlatPath.h).
//
// A game path is split into a base (a volume's native root, the native root "/" for a native absolute path, or
// the current directory) and components (separators '\' and '/'; "." dropped, ".." removes the previous component
// but never the base, trailing dots and spaces trimmed as Win32 does). Each component is looked up exactly first,
// then by an ASCII case-insensitive scan of its directory; from the first component that does not exist on, the
// rest is kept as written (a file or directory about to be created).
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "platform/PlatFile.h"
#include "platform/PlatPath.h"
#include "PlatPosix.h"

namespace {

std::mutex sVolumeLock;
std::string sVolumeRoot[26];   // empty: "/"

std::string VolumeRoot(char inVolume)
{
    std::lock_guard<std::mutex> theLock(sVolumeLock);
    const std::string& theRoot = sVolumeRoot[toupper(static_cast<unsigned char>(inVolume)) - 'A'];
    return theRoot.empty() ? std::string("/") : theRoot;
}

bool IsSeparator(char c)
{
    return c == '\\' || c == '/';
}

std::string Join(const std::string& inDirectory, const std::string& inName)
{
    if (!inDirectory.empty() && inDirectory.back() == '/')
        return inDirectory + inName;
    return inDirectory + "/" + inName;
}

// The entry of inDirectory whose name equals inName ignoring ASCII case -> false when there is none (an exact
// match is preferred when several differ only in case)
bool FindNoCase(const std::string& inDirectory, const std::string& inName, std::string& outName)
{
    DIR* theDir = opendir(inDirectory.c_str());
    if (!theDir)
        return false;
    bool theFound = false;
    while (struct dirent* theEntry = readdir(theDir)) {
        if (PlatPosix::CompareNoCase(theEntry->d_name, inName.c_str()) == 0) {
            outName = theEntry->d_name;
            theFound = true;
            if (outName == inName)
                break;
        }
    }
    closedir(theDir);
    return theFound;
}

void SplitInto(const char* inText, std::vector<std::string>& ioComponents)
{
    const char* p = inText;
    while (*p) {
        while (IsSeparator(*p))
            p++;
        if (!*p)
            break;
        const char* theStart = p;
        while (*p && !IsSeparator(*p))
            p++;
        std::string theComponent(theStart, p - theStart);
        if (theComponent == ".")
            continue;
        if (theComponent == "..") {
            if (!ioComponents.empty())
                ioComponents.pop_back();
            continue;
        }
        // Win32 drops trailing dots and spaces of a name ("file." is "file")
        while (!theComponent.empty() && (theComponent.back() == '.' || theComponent.back() == ' '))
            theComponent.pop_back();
        if (!theComponent.empty())
            ioComponents.push_back(theComponent);
    }
}

bool NativeDirectoryOfExecutable(std::string& outDirectory)
{
    char theBuffer[PATH_MAX + 1];
#if defined(__APPLE__)
    uint32_t theSize = sizeof(theBuffer);
    if (_NSGetExecutablePath(theBuffer, &theSize) != 0)
        return false;
    char theReal[PATH_MAX + 1];
    if (!realpath(theBuffer, theReal))
        return false;
    outDirectory = theReal;
#else
    ssize_t theLength = readlink("/proc/self/exe", theBuffer, sizeof(theBuffer) - 1);
    if (theLength <= 0)
        return false;
    theBuffer[theLength] = 0;
    outDirectory = theBuffer;
#endif
    size_t theSlash = outDirectory.rfind('/');
    if (theSlash == std::string::npos)
        return false;
    outDirectory.resize(theSlash == 0 ? 1 : theSlash);
#if defined(__APPLE__)
    // an app bundle keeps its data in Contents/Resources
    const char kMacOS[] = "/Contents/MacOS";
    size_t n = sizeof(kMacOS) - 1;
    if (outDirectory.size() > n && outDirectory.compare(outDirectory.size() - n, n, kMacOS) == 0) {
        std::string theResources = outDirectory.substr(0, outDirectory.size() - n) + "/Contents/Resources";
        struct stat theStat;
        if (stat(theResources.c_str(), &theStat) == 0 && S_ISDIR(theStat.st_mode))
            outDirectory = theResources;
    }
#endif
    return true;
}

bool NativeDataRoot(std::string& outRoot)
{
    const char* theEnv = getenv("EE_DATA_ROOT");
    if (theEnv && *theEnv) {
        char theReal[PATH_MAX + 1];
        outRoot = realpath(theEnv, theReal) ? theReal : theEnv;
        return true;
    }
    return NativeDirectoryOfExecutable(outRoot);
}

uint32_t CopyOut(const std::string& inText, char* outBuffer, uint32_t inSize)
{
    if (inText.size() + 1 > inSize) {
        PlatPosix::SetLastError(PlatPosix::kErrorFilenameTooLong);
        return 0;
    }
    memcpy(outBuffer, inText.c_str(), inText.size() + 1);
    return static_cast<uint32_t>(inText.size());
}

}   // namespace

namespace PlatPosix {

int CompareNoCase(const char* inLeft, const char* inRight)
{
    for (;; inLeft++, inRight++) {
        int a = toupper(static_cast<unsigned char>(*inLeft));
        int b = toupper(static_cast<unsigned char>(*inRight));
        if (a != b || a == 0)
            return a - b;
    }
}

bool WildcardMatch(const char* inPattern, const char* inName)
{
    if (strcmp(inPattern, "*.*") == 0 || strcmp(inPattern, "*") == 0)
        return true;
    // iterative * backtracking
    const char* theStar = NULL;
    const char* theResume = NULL;
    const char* p = inPattern;
    const char* n = inName;
    while (*n) {
        if (*p == '*') {
            theStar = p++;
            theResume = n;
        } else if (*p == '?' || toupper(static_cast<unsigned char>(*p)) == toupper(static_cast<unsigned char>(*n))) {
            p++;
            n++;
        } else if (theStar) {
            p = theStar + 1;
            n = ++theResume;
        } else {
            return false;
        }
    }
    // the name is used up: what is left of the pattern must match nothing (stars; "?" and a final "." match the end
    // as Win32's matcher does: "name.*" matches "name", "a?" matches "a")
    while (*p == '*' || *p == '?' || (*p == '.' && (p[1] == '*' || p[1] == 0)))
        p++;
    return *p == 0;
}

bool ResolvePath(const char* inPath, std::string& outNative)
{
    if (!inPath || !*inPath) {
        SetLastError(kErrorPathNotFound);
        return false;
    }
    if (strlen(inPath) >= PLAT_MAX_PATH) {
        SetLastError(kErrorFilenameTooLong);
        return false;
    }
    std::string theBase;
    std::vector<std::string> theComponents;
    const char* p = inPath;
    if (isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
        theBase = VolumeRoot(p[0]);
        p += 2;
    } else if (IsSeparator(p[0]) && IsSeparator(p[1])) {
        SetLastError(kErrorPathNotFound);   // UNC paths (\\server\share) have no native meaning here
        return false;
    } else if (p[0] == '/') {
        theBase = "/";
    } else if (p[0] == '\\') {
        theBase = VolumeRoot('C');
    } else {
        char theCurrent[PATH_MAX + 1];
        if (!getcwd(theCurrent, sizeof(theCurrent))) {
            FailErrno();
            return false;
        }
        theBase = "/";
        SplitInto(theCurrent, theComponents);
    }
    SplitInto(p, theComponents);
    std::string theCurrent = theBase;
    bool theMissing = false;
    for (size_t i = 0; i < theComponents.size(); i++) {
        std::string theCandidate = Join(theCurrent, theComponents[i]);
        struct stat theStat;
        if (theMissing || lstat(theCandidate.c_str(), &theStat) == 0) {
            theCurrent = theCandidate;
            continue;
        }
        std::string theName;
        if (FindNoCase(theCurrent, theComponents[i], theName)) {
            theCurrent = Join(theCurrent, theName);
        } else {
            theMissing = true;
            theCurrent = theCandidate;
        }
    }
    outNative = theCurrent;
    return true;
}

bool GameForm(const std::string& inNative, std::string& outPath)
{
    if (inNative.empty() || inNative[0] != '/') {
        SetLastError(kErrorInvalidParameter);
        return false;
    }
    // the volume whose root is the longest prefix (C: first among equals)
    int theBest = -1;
    size_t theBestLength = 0;
    for (int i = 0; i < 26; i++) {
        int theVolume = (i + 2) % 26;   // C, D, ..., Z, A, B
        std::string theRoot = VolumeRoot(static_cast<char>('A' + theVolume));
        if (theRoot.size() > 1 && theRoot.back() == '/')
            theRoot.pop_back();
        bool theMatches = theRoot == "/" || inNative == theRoot
            || (inNative.compare(0, theRoot.size(), theRoot) == 0 && inNative[theRoot.size()] == '/');
        size_t theLength = theRoot == "/" ? 0 : theRoot.size();
        if (theMatches && (theBest < 0 || theLength > theBestLength)) {
            theBest = theVolume;
            theBestLength = theLength;
        }
    }
    if (theBest < 0) {
        SetLastError(kErrorPathNotFound);
        return false;
    }
    outPath.assign(1, static_cast<char>('A' + theBest));
    outPath += ":";
    std::string theRest = inNative.substr(theBestLength);
    if (theRest.empty() || theRest[0] != '/')
        theRest.insert(theRest.begin(), '/');
    for (char& c : theRest)
        if (c == '/')
            c = '\\';
    outPath += theRest;
    return true;
}

}   // namespace PlatPosix

extern "C" uint32_t PlatPathToNative(const char* inPath, char* outNative, uint32_t inSize)
{
    std::string theNative;
    if (!PlatPosix::ResolvePath(inPath, theNative))
        return 0;
    return CopyOut(theNative, outNative, inSize);
}

extern "C" uint32_t PlatPathFromNative(const char* inNative, char* outPath, uint32_t inSize)
{
    std::string thePath;
    if (!inNative || !PlatPosix::GameForm(inNative, thePath))
        return 0;
    return CopyOut(thePath, outPath, inSize);
}

extern "C" int PlatPathSetVolumeRoot(char inVolume, const char* inNativeRoot)
{
    if (!isalpha(static_cast<unsigned char>(inVolume)) || (inNativeRoot && inNativeRoot[0] != '/'))
        return PlatPosix::Fail(PlatPosix::kErrorInvalidParameter);
    std::string theRoot = inNativeRoot ? inNativeRoot : "";
    while (theRoot.size() > 1 && theRoot.back() == '/')
        theRoot.pop_back();
    std::lock_guard<std::mutex> theLock(sVolumeLock);
    sVolumeRoot[toupper(static_cast<unsigned char>(inVolume)) - 'A'] = theRoot;
    return 1;
}

extern "C" uint32_t PlatPathGetDataRoot(char* outPath, uint32_t inSize)
{
    std::string theNative, thePath;
    if (!NativeDataRoot(theNative) || !PlatPosix::GameForm(theNative, thePath))
        return 0;
    if (thePath.back() != '\\')
        thePath += '\\';
    return CopyOut(thePath, outPath, inSize);
}

extern "C" int PlatPathEnterDataRoot(void)
{
    std::string theNative;
    if (!NativeDataRoot(theNative))
        return PlatPosix::Fail(PlatPosix::kErrorPathNotFound);
    if (chdir(theNative.c_str()) != 0)
        return PlatPosix::FailErrno();
    return 1;
}

extern "C" uint32_t PlatPathGetExecutableName(char* outName, uint32_t inSize)
{
    char theBuffer[PATH_MAX + 1];
#if defined(__APPLE__)
    uint32_t theSize = sizeof(theBuffer);
    if (_NSGetExecutablePath(theBuffer, &theSize) != 0)
        return 0;
#else
    ssize_t theLength = readlink("/proc/self/exe", theBuffer, sizeof(theBuffer) - 1);
    if (theLength <= 0)
        return 0;
    theBuffer[theLength] = 0;
#endif
    const char* theName = strrchr(theBuffer, '/');
    theName = theName ? theName + 1 : theBuffer;
    size_t n = strlen(theName);
    if (n + 1 > inSize)
        return 0;
    memcpy(outName, theName, n + 1);
    return static_cast<uint32_t>(n);
}
