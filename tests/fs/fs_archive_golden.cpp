// Golden driver for FSArchive and FSPath: random sequences through the original units (fs_archive_orig.h: FSArchive.cpp
// and FSPath.cpp unmodified) against a real directory, emptied before each sequence: a source tree of random files is
// made, archives are built from it (the directory walk with patterns and recursion, or a list of names), opened,
// searched (case-insensitively), tagged, listed and extracted; search paths of archives and directories find files,
// list them and report where a file comes from. The Rust replay (rust/crates/ee_base/src/fs/golden_archive.rs) makes the
// same calls on Rust records in a directory of its own (its path the same length) and regenerates every line.
//
//   python -I build/lle5/cc.py build/lle5/fsag.exe tests/fs/fs_archive_golden.cpp -Itests/fs <relink library + libs>
//   build/lle5/fsag.exe <root directory, ending in a backslash> <sequences> [first seed] > golden.txt
// Relinked over the new units (-DFS_NEW: FSArchive.cpp and FSPath.cpp's forwarders) it must print the same.
//
// Texts are hex bytes ("-" for an empty one), the root's directory replaced by the byte 01. Every line ends
// " ne <the last native error>". Lines:
//   root <length of the root's directory>
//   seq <n>
//   file <relative name> <length> <seed>             a source file (bytes: see Bytes below)
//   <op> ... = <result> ...                           the calls (see the switch below)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include <windows.h>
#include "platform/PlatFile.h"

#define private public   // the destructors below (UString.cpp and UWideString.cpp include every engine header)
#define protected public  // FSPath's destructor, the specs' members
#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#ifdef FS_NEW
#include "lle/FSArchive.h"
#else
#include "fs_archive_orig_class.h"   // the original class (its private helpers)
#endif
#include "lle/FSPath.h"
#undef private
#undef protected
#include "rust/base/error.h"
#include "rust/base/ustring.h"
UString::~UString() { ee_ustring_free(&this->mState); }
UWideString::~UWideString() { ee_uwstring_free(&this->mState); }

UString gStartupVolume(256);
UString gStartupDirectory(256);
unsigned long ESRecordNativeError(unsigned long inError) { return ee_error_record_native(inError); }
unsigned long ESGetNativeError() { return ee_error_native(); }
void ESVerify(const char* inFile, unsigned long inLine, const char* inExpression)
{
    printf("VERIFY %lu %s\n", inLine, inExpression);
    fflush(stdout);
    exit(3);
}

#include "../../src/Low-Level Engine/File System/FSBasicFile.cpp"
#include "../../src/Low-Level Engine/File System/FSFileSpec.cpp"
#include "../../src/Low-Level Engine/File System/FSMemoryFile.cpp"
#ifdef FS_NEW
#include "../../src/Low-Level Engine/File System/FSArchive.cpp"
#include "../../src/Low-Level Engine/File System/FSPath.cpp"
#else
#include "fs_archive_orig.h"
#endif

static std::string gRootDir, gVolume, gRoot;
static uint64_t gRand;
static uint32_t R()
{
    gRand = gRand * 6364136223846793005ull + 1442695040888963407ull;
    return (uint32_t)(gRand >> 33);
}
static uint32_t R(uint32_t n) { return n ? R() % n : 0; }

static std::string Hex(const std::string& inText)
{
    std::string theText = inText;
    for (size_t p; (p = theText.find(gRootDir)) != std::string::npos;)
        theText.replace(p, gRootDir.size(), "\x01");
    if (theText.empty())
        return "-";
    std::string theOut;
    char theByte[3];
    for (unsigned char c : theText) {
        snprintf(theByte, sizeof theByte, "%02x", c);
        theOut += theByte;
    }
    return theOut;
}
static std::string Spec(const FSFileSpec& inSpec)
{
    return Hex((const char*)inSpec.mVolume) + "|" + Hex((const char*)inSpec.mDirectory) + "|" + Hex((const char*)inSpec.mFile);
}
static void Line(const std::string& inText) { printf("%s ne %lu\n", inText.c_str(), ESGetNativeError()); }
static std::string N(long inValue) { return std::to_string(inValue); }

// a source file's bytes: compressible runs or noise from the seed
static unsigned char Byte(uint32_t inSeed, uint32_t i)
{
    return (inSeed & 1) ? (unsigned char)("abcabcabdEE "[(i / 3 + inSeed) % 12]) : (unsigned char)((inSeed + i * 2654435761u) >> 13);
}
// FNV-1a of a memory file's contents (the member's bytes)
static uint32_t Digest(const unsigned char* inData, unsigned long inLength)
{
    uint32_t h = 2166136261u;
    for (unsigned long i = 0; i < inLength; i++)
        h = (h ^ inData[i]) * 16777619u;
    return h;
}

static void WipeTree(const std::string& inDirectory)
{
    WIN32_FIND_DATAA theData;
    HANDLE theFind = FindFirstFileA((inDirectory + "*").c_str(), &theData);
    if (theFind == INVALID_HANDLE_VALUE)
        return;
    do {
        std::string theName = theData.cFileName;
        if (theName == "." || theName == "..")
            continue;
        std::string thePath = inDirectory + theName;
        SetFileAttributesA(thePath.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (theData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            WipeTree(thePath + "\\");
            RemoveDirectoryA(thePath.c_str());
        } else
            DeleteFileA(thePath.c_str());
    } while (FindNextFileA(theFind, &theData));
    FindClose(theFind);
}

static const char* kDirs[] = {"src\\", "src\\Sub\\", "src\\Sub\\deep\\", "src\\Other\\", "alt\\"};
static const char* kNames[] = {"a.txt", "B.TXT", "c.dat", "Data.bin", "e", "f.txt", "G.dat", "readme.txt"};
static const char* kPatterns[] = {"*.*", "*", "*.txt", "*.dat", "a*", "?.*", "none"};
static const char* kListPatterns[] = {"*", "*.txt", "sub\\*", "*a*", "c.dat", "*.", "*sub*.dat", ""};
static const char* kTags[] = {"Name", "name", "Author", "Description", "Empty"};
static const char* kValues[] = {"", "x", "Campaign One", "\xc3\xa9t\xc3\xa9", "a longer value with spaces"};

static FSFileSpec Dir(const std::string& inRelative)   // a directory spec under the root
{
    FSFileSpec theSpec;
    theSpec.SetVolume(UString(gVolume.c_str()));
    theSpec.SetDirectory(UString((gRootDir + inRelative).c_str()));
    return theSpec;
}

static std::vector<std::string> gFiles;   // the sequence's source files (relative to the root)

static std::string Name()   // a member's name: a relative path (mostly a source file's), in any case
{
    std::string theName = std::string(kDirs[R(4)] + 4) + kNames[R(8)];
    if (R(10) < 7 && !gFiles.empty()) {
        theName = gFiles[R((uint32_t)gFiles.size())];
        size_t theCut = theName.find('\\');   // relative to its top directory, sometimes to the next one
        theName = theName.substr(theCut + 1);
        if (R(3) == 0 && (theCut = theName.find('\\')) != std::string::npos)
            theName = theName.substr(theCut + 1);
    }
    if (R(3) == 0)
        for (auto& c : theName)
            c = (char)(R(2) ? toupper((unsigned char)c) : tolower((unsigned char)c));
    return R(10) ? theName : std::string("missing.txt");
}

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    gRoot = argv[1];
    gVolume = gRoot.substr(0, 2);
    gRootDir = gRoot.substr(2);
    unsigned long theCount = strtoul(argv[2], NULL, 10), theFirst = argc > 3 ? strtoul(argv[3], NULL, 10) : 1;
    gStartupVolume = gVolume.c_str();
    gStartupDirectory = gRootDir.c_str();
    printf("root %zu\n", gRootDir.size());
    for (unsigned long n = theFirst; n < theFirst + theCount; n++) {
        CreateDirectoryA(gRoot.c_str(), NULL);
        WipeTree(gRoot);
        gRand = n * 0x9E3779B97F4A7C15ull + 11;
        gFiles.clear();
        bool thePaths = n % 2 == 0;   // the search path's calls only in even sequences (the replay skips those)
        printf("seq %lu%s\n", n, thePaths ? " p" : "");
        // the source tree
        for (const char* d : kDirs)
            CreateDirectoryA((gRoot + d).c_str(), NULL);
        int theFiles = 3 + (int)R(10);
        for (int i = 0; i < theFiles; i++) {
            std::string theName = std::string(kDirs[R(5)]) + kNames[R(8)];
            uint32_t theLength = R(4) ? R(3000) : R(4) ? 0 : R(20000), theSeed = R();
            std::vector<unsigned char> theData(theLength);
            for (uint32_t j = 0; j < theLength; j++)
                theData[j] = Byte(theSeed, j);
            FILE* f = fopen((gRoot + theName).c_str(), "wb");
            if (theLength)
                fwrite(theData.data(), 1, theLength, f);
            fclose(f);
            printf("file %s %u %u\n", Hex(theName).c_str(), theLength, theSeed);
            gFiles.push_back(theName);
        }
        FSArchive* a[3] = {};
        for (int k = 0; k < 3; k++) {
            FSFileSpec theFile = Dir("");
            theFile.SetFile(UString(("arc" + std::to_string(k) + ".ssa").c_str()));
            a[k] = new FSArchive(theFile);
        }
        FSPath* p = new FSPath();
        int theOps = 15 + (int)R(30);
        for (int o = 0; o < theOps; o++) {
            int k = (int)R(3), j = (int)R(3);
            std::string K = N(k), J = N(j);
            uint32_t theOp = R(20);
            if (!thePaths && theOp >= 15)
                theOp = 6;   // a find instead
            switch (theOp) {
            case 0: case 1: {   // Create from the walk
                int d = R(3) ? 0 : (int)R(4), pi = R(2) ? 0 : (int)R(7), rec = R(4) ? 1 : 0;   // mostly all of src
                unsigned long theLevel = R(3) ? 0x1000 : R(2) ? 0x400 : 0x800;   // the implode dictionary sizes
                long r = a[k]->Create(Dir(kDirs[d]), UString(kPatterns[pi]), theLevel, rec != 0);
                Line("create " + K + " " + N(d) + " " + N(pi) + " " + N((long)theLevel) + " " + N(rec) + " = " + N(r));
                if (R(4))
                    Line("open " + K + " = " + N(a[k]->Open()));   // (Create leaves it closed)
                break;
            }
            case 2: {   // Create from a list of names under a directory
                int d = (int)R(4), count = (int)R(4);
                std::vector<UString> theNames;
                std::string theText;
                for (int i = 0; i < count; i++) {
                    std::string theName = R(4) ? kNames[R(8)] : std::string("Sub\\") + kNames[R(8)];
                    theNames.push_back(UString(theName.c_str()));
                    theText += " " + Hex(theName);
                }
                FILE* f = fopen((gRoot + kDirs[d] + "probe").c_str(), "r");   // (only listed files that exist)
                bool theAll = true;
                for (auto& s : theNames) {
                    FILE* g = fopen((gRoot + kDirs[d] + (const char*)s).c_str(), "rb");
                    if (g) fclose(g); else theAll = false;
                }
                if (f)
                    fclose(f);
                if (!theAll)
                    break;   // a missing file's length is 0 but its write fails a verification
                long r = a[k]->Create(Dir(kDirs[d]), theNames, 0x1000);
                Line("createl " + K + " " + N(d) + " " + N(count) + theText + " = " + N(r));
                if (R(4))
                    Line("open " + K + " = " + N(a[k]->Open()));
                break;
            }
            case 3: case 4: Line("open " + K + " = " + N(a[k]->Open())); break;
            case 5: Line("close " + K + " = " + N(a[k]->Close())); break;
            case 6: case 7: case 8: {
                std::string theName = Name();
                FSMemoryFile* f = NULL;
                long r = a[k]->FindFile(UString(theName.c_str()), f);
                std::string theOut = "find " + K + " " + Hex(theName) + " = " + N(r);
                if (f) {
                    unsigned long theLength = 0;
                    f->GetLength(theLength);
                    theOut += " " + N((long)theLength) + " " + N((long)Digest(f->GetData(), theLength)) + " " + Spec(*f);
                    delete f;
                }
                Line(theOut);
                break;
            }
            case 9: {
                int t = (int)R(5), v = (int)R(5);
                long r = a[k]->SetTag(UWideString(kTags[t]), UWideString(kValues[v]));
                Line("settag " + K + " " + N(t) + " " + N(v) + " = " + N(r));
                break;
            }
            case 10: {
                int t = (int)R(5);
                UWideString theValue("unset");
                long r = a[k]->GetTag(UWideString(kTags[t]), theValue);
                Line("gettag " + K + " " + N(t) + " = " + N(r) + " " + Hex((const char*)theValue));
                break;
            }
            case 11: {
                int pi = (int)R(8);
                std::set<UString> theFiles;
                theFiles.insert(UString("stale"));
                int c = (int)R(2);
                long r = a[k]->ListFiles(UString(kListPatterns[pi]), theFiles, c != 0);
                std::string theOut = "list " + K + " " + N(pi) + " " + N(c) + " = " + N(r);
                for (auto& s : theFiles)
                    theOut += " " + Hex((const char*)s);
                Line(theOut);
                break;
            }
            case 12: {   // ExtractAll into out\ (its subdirectories made first, as a caller would)
                CreateDirectoryA((gRoot + "out\\").c_str(), NULL);
                int theSubs = (int)R(2);
                if (theSubs) {
                    CreateDirectoryA((gRoot + "out\\sub\\").c_str(), NULL);
                    CreateDirectoryA((gRoot + "out\\sub\\deep\\").c_str(), NULL);
                    CreateDirectoryA((gRoot + "out\\other\\").c_str(), NULL);
                }
                printf("extract %d %d\n", k, theSubs);
                fflush(stdout);
                long r = a[k]->ExtractAll(Dir("out\\"));
                fflush(stdout);
                Line("extracted " + K + " = " + N(r));
                break;
            }
            case 13: {
                if (j != k)
                    *a[k] = *a[j];
                Line("asg " + K + " " + J);
                break;
            }
            case 14: {
                FSFileSpec theFile = Dir("");
                theFile.SetFile(UString(("arc" + std::to_string(R(3)) + ".ssa").c_str()));
                Line("setarc " + K + " " + Spec(theFile) + " = " + N(a[k]->SetArchive(theFile)));
                break;
            }
            case 15: {   // the path: an archive or a directory added
                if (R(2)) {
                    int f = (int)R(2);
                    p->AddArchive(*a[k], f != 0);
                    Line("padda " + K + " " + N(f) + " " + N((long)p->GetArchiveCount()));
                } else {
                    int d = (int)R(5), f = (int)R(2);
                    p->AddDirectory(Dir(kDirs[d]), f != 0);
                    Line("paddd " + N(d) + " " + N(f) + " " + N((long)p->GetDirectoryCount()));
                }
                break;
            }
            case 16: case 17: {
                std::string theName = R(2) ? Name() : std::string(kNames[R(8)]);
                FSFileSpec* f = NULL;
                FSFileSpec theDirectory;
                long r = p->FindFile(UString(theName.c_str()), f, theDirectory);
                std::string theOut = "pfind " + Hex(theName) + " = " + N(r);
                if (r == kErrorOK) {
                    unsigned long theLength = 0;
                    bool theMemory = dynamic_cast<FSMemoryFile*>(f) != NULL;
                    if (theMemory)
                        f->GetLength(theLength);
                    theOut += " " + N(theMemory) + " " + N((long)theLength) + " " + Spec(*f) + " " + Spec(theDirectory);
                    delete f;
                }
                bool theArchived = false;
                bool theFound = p->GetFileArchiveStatus(UString(theName.c_str()), theArchived);
                Line(theOut + " status " + N(theFound) + " " + N(theArchived));
                break;
            }
            case 18: {
                int pi = (int)R(8);
                std::set<UString> theFiles;
                long r = p->ListFiles(UString(kListPatterns[pi]), theFiles);
                std::string theOut = "plist " + N(pi) + " = " + N(r);
                for (auto& s : theFiles)
                    theOut += " " + Hex((const char*)s);
                Line(theOut);
                break;
            }
            default: {
                if (R(2)) {
                    unsigned long i = R(3);
                    Line("prema " + N((long)i) + " = " + N(p->RemoveArchive(i)) + " " + N((long)p->GetArchiveCount()));
                } else if (R(2)) {
                    unsigned long i = R(3);
                    Line("premd " + N((long)i) + " = " + N(p->RemoveDirectory(i)) + " " + N((long)p->GetDirectoryCount()));
                } else {   // a copy of the path replaces it
                    FSPath* q = new FSPath(*p);
                    if (R(2)) {
                        *p = *q;
                        delete q;
                    } else {
                        delete p;
                        p = q;
                    }
                    Line("pcopy " + N((long)p->GetArchiveCount()) + " " + N((long)p->GetDirectoryCount()));
                }
                break;
            }
            }
        }
        delete p;
        for (int k = 0; k < 3; k++)
            delete a[k];
    }
    CreateDirectoryA(gRoot.c_str(), NULL);
    WipeTree(gRoot);
    return 0;
}
