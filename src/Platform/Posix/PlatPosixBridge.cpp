// Platform layer, POSIX: the seam between the C++ files still here (PlatFile.cpp, PlatThread.cpp, PlatSocket.cpp) and
// the part that moved to Rust (rust/crates/ee_plat_core: PlatPath, PlatTime, ...). They ask it for what PlatPosix.h
// declares about paths, and Rust records its failures in the thread's last error through the functions at the end.
// Goes away with those files (R1.3, docs/RUST_PLAN.md).
#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "platform/PlatPath.h"
#include "PlatPosix.h"

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
    char theBuffer[8192];   // a volume root (a native path) and a game path
    uint32_t theLength = PlatPathToNative(inPath, theBuffer, sizeof(theBuffer));
    if (theLength == 0)
        return false;
    outNative.assign(theBuffer, theLength);
    return true;
}

bool GameForm(const std::string& inNative, std::string& outPath)
{
    char theBuffer[8192];
    uint32_t theLength = PlatPathFromNative(inNative.c_str(), theBuffer, sizeof(theBuffer));
    if (theLength == 0)
        return false;
    outPath.assign(theBuffer, theLength);
    return true;
}

}   // namespace PlatPosix

extern "C" void ee_plat_posix_set_last_error(uint32_t inError)
{
    PlatPosix::SetLastError(inError);
}

extern "C" void ee_plat_posix_set_last_error_errno(int inError)
{
    PlatPosix::SetLastError(PlatPosix::ErrorFromErrno(inError));
}
