// Golden driver for File System.cpp's startup paths (FSGetVolumeAndDirectory, FSInitialize; LLE lane round 6):
// random paths (drive and UNC volumes, separators, existing and missing directories, empty, near PLAT_MAX_PATH) over
// fresh and reused specs, printing each spec's volume and directory with their capacities, and the startup volume and
// directory FSInitialize leaves. The process runs in the root directory (the current directory FSInitialize falls
// back to), whose path is printed as byte 01.
//
//   python -I build/lle6/drvfs.py orig|new;   build/lle6/fsst_<orig|new>.exe <root\> <cases> > golden.txt
// Linked over the original File System.cpp (orig) and the forwarders over Rust (new): both must print the same.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/File System.h"

void FSGetVolumeAndDirectory(const UString& inPath, FSFileSpec& outSpec);   // File System.cpp

static std::string gRoot;   // "C:\...\" (no volume: from the backslash)

static uint64_t gRand;
static uint32_t R(uint32_t n)
{
    gRand = gRand * 6364136223846793005ull + 1442695040888963407ull;
    return n ? (uint32_t)(gRand >> 33) % n : 0;
}

static std::string Show(const char* inText)
{
    std::string t = inText;
    for (size_t p; !gRoot.empty() && (p = t.find(gRoot)) != std::string::npos;)
        t.replace(p, gRoot.size(), "\x01");
    std::string o;
    char b[3];
    for (unsigned char c : t) {
        snprintf(b, sizeof b, "%02x", c);
        o += b;
    }
    return o.empty() ? "-" : o;
}

static std::string Spec(FSFileSpec& s)
{
    char t[64];
    snprintf(t, sizeof t, " %lu/%lu", s.GetVolume().GetAllocatedLength(), s.GetDirectory().GetAllocatedLength());
    return Show(s.GetVolume()) + " " + Show(s.GetDirectory()) + t;
}

static std::string Piece()
{
    static const char* kPieces[] = {"C:", "c:", "\\\\", "\\", "server", "share\\", "dir", "dir\\", "a b", "..",
                                    "D:", "x", "\\\\?\\", "/", "Data\\"};
    switch (R(6)) {
    case 0: return gRoot;
    case 1: return std::string(R(250), 'q');
    default: return kPieces[R(15)];
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    std::string theRoot = argv[1];
    gRoot = theRoot.substr(2);
    SetCurrentDirectoryA(theRoot.c_str());
    unsigned long theCount = strtoul(argv[2], NULL, 10);
    FSFileSpec* theReused = new FSFileSpec;
    for (unsigned long n = 0; n < theCount; n++) {
        gRand = n * 0x9E3779B97F4A7C15ull + 3;
        std::string thePath;
        int k = R(5);
        if (k == 1)
            thePath = theRoot.substr(0, 2) + gRoot;
        else
            for (int i = (int)R(5); i > 0; i--)
                thePath += Piece();
        // a path shorter than its volume makes a substring of length -1 or -2 (a 4 GB string): never a real path
        if (thePath.size() < 2)
            thePath = "C:";
        UString thePathString(thePath.c_str());
        FSFileSpec theFresh;
        bool theFreshUsed = R(2) == 0;
        FSFileSpec& s = theFreshUsed ? theFresh : *theReused;
        FSGetVolumeAndDirectory(thePathString, s);
        printf("vd %s %d = %s\n", Show(thePath.c_str()).c_str(), theFreshUsed, Spec(s).c_str());
        // not with a UNC-like path (its directory test is a network lookup)
        if (R(3) == 0 && thePath.find("\\\\") == std::string::npos && thePath.find_first_of("/?") == std::string::npos) {
            gStartupVolume = R(2) ? "e:" : "";
            gStartupDirectory = R(2) ? "\\old\\" : "";
            FSInitialize(thePathString);
            printf("init %s = %s %s %lu/%lu\n", Show(thePath.c_str()).c_str(), Show(gStartupVolume).c_str(),
                   Show(gStartupDirectory).c_str(), gStartupVolume.GetAllocatedLength(),
                   gStartupDirectory.GetAllocatedLength());
        }
    }
    return 0;
}
