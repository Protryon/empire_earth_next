// Golden driver for FSBasicFile and FSFileSpec (every member: the constructors, assignment, the names, Create,
// CreateDirectory, Delete, DeleteDirectory, Copy, Lock, Unlock, IsDirectory, Open, Close, FOpen, the byte and scalar
// Read/Write, GetLength, GetPosition, SetLength, SetPosition, the directory search, GetParentDirectory,
// GetLengthFromClosedFile, operator<): random sequences on four specs through the original classes
// (fs_spec_orig.h, the unmodified units) against a real directory, emptied before each sequence. The Rust replay
// (rust/crates/ee_base/src/fs/golden_spec.rs) makes the same calls on Rust records in a directory of its own whose path
// has the same length, and regenerates every line.
//
//   python -I build/lle5/cc.py build/lle5/fsg.exe tests/fs/fs_spec_golden.cpp -Itests/fs <relink library + system libs>
//   build/lle5/fsg.exe <root directory, ending in a backslash> <sequences> [first seed] > golden.txt
// Relinked over the new classes (-DFS_NEW: include/lle/FSBasicFile.h, FSFileSpec.h and the two units' forwarders,
// with the Rust crates as a static library) it must print the same.
//
// Texts are hex bytes ("-" for an empty one), the root's directory (after its volume) replaced by the byte 01 before
// encoding. A spec's state is "= <volume> <directory> <file> <capacities> <access> <version> <open> <handle> <stream>"
// (handle and stream: 0/1). Every line ends " ne <the last native error>". Lines:
//   root <length of the root's directory>          once, first
//   seq <n>                                        a sequence starts (the directory is empty, the slots fresh)
//   newd|newp|newc <k> [<path>|<j>] <state>        the constructors
//   asg <k> <j> <state>                            operator=
//   setf|setd|setv <k> <text> <state>              SetFile, SetDirectory, SetVolume
//   <op> <k> [args] = <result> <state>              the members (see the switch below; a write's bytes come from
//                                                  the seed it prints)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>
#include "platform/PlatFile.h"

#define private public   // the destructor below (UString.cpp, which includes every engine header)
#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#undef private
UString::~UString() { ee_ustring_free(&this->mState); }
#include "lle/FSFile.h"

UString gStartupVolume(256);
UString gStartupDirectory(256);

#ifdef FS_NEW
#include "rust/ee_error.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
unsigned long ESRecordNativeError(unsigned long inError) { return ee_error_record_native(inError); }
unsigned long ESGetNativeError() { return ee_error_native(); }
#include "../../src/Low-Level Engine/File System/FSBasicFile.cpp"
#include "../../src/Low-Level Engine/File System/FSFileSpec.cpp"
#else
static unsigned long sNative = 0;
unsigned long ESRecordNativeError(unsigned long inError)
{
    if (inError != 0)
        sNative = inError;
    return inError;
}
unsigned long ESGetNativeError() { return sNative; }
#include "fs_spec_orig.h"
#endif

// a spec whose protected members the driver prints
class Spec : public FSFileSpec {
public:
    using FSFileSpec::FSFileSpec;
    Spec(const Spec& inSpec) : FSFileSpec(inSpec) {}
    Spec& operator=(const Spec& inSpec) { FSFileSpec::operator=(inSpec); return *this; }
    unsigned long Access() const { return mFileAccess; }
    unsigned long Version() const { return GetVersion(); }
    bool HasHandle() const { return mHandle != NULL; }
    bool HasStream() const { return mStream != NULL; }
    const UString& V() const { return mVolume; }
    const UString& D() const { return mDirectory; }
    const UString& F() const { return mFile; }
};

static std::string gRootDir;   // the root's directory part ("\...\")
static std::string gVolume;    // "C:"
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
    for (size_t p; !gRootDir.empty() && (p = theText.find(gRootDir)) != std::string::npos;)
        theText.replace(p, gRootDir.size(), "\x01");
    // the root's parent (GetParentDirectory of the root): byte 02
    std::string theParent = gRootDir.substr(0, gRootDir.rfind('\\', gRootDir.size() - 2) + 1);
    for (size_t p; !gRootDir.empty() && (p = theText.find(theParent)) != std::string::npos;)
        theText.replace(p, theParent.size(), "\x02");
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
static std::string HexBytes(const unsigned char* inData, unsigned long inLength)
{
    return Hex(std::string(reinterpret_cast<const char*>(inData), inLength));
}

static std::string State(const Spec* inSpec)
{
    if (!inSpec)
        return "= none";
    char theTail[160];
    snprintf(theTail, sizeof theTail, " %lu/%lu/%lu %lu %lu %d %d %d", inSpec->V().GetAllocatedLength(),
             inSpec->D().GetAllocatedLength(), inSpec->F().GetAllocatedLength(), inSpec->Access(), inSpec->Version(),
             inSpec->GetIsOpen() ? 1 : 0, inSpec->HasHandle() ? 1 : 0, inSpec->HasStream() ? 1 : 0);
    return "= " + Hex((const char*)inSpec->V()) + " " + Hex((const char*)inSpec->D()) + " " +
           Hex((const char*)inSpec->F()) + theTail;
}

static void Line(const std::string& inText)
{
    printf("%s ne %lu\n", inText.c_str(), ESGetNativeError());
    if (getenv("FSG_FLUSH"))
        fflush(stdout);
}

// ---- the names the sequences use
static const char* kNames[] = {"a", "b.txt", "B.TXT", "c.dat", "sub", "Sub2", "x y.bin", "", " ", "  ", "bad?.txt",
                               "d:e", "q*", "a<b", "e|f", "dot.", ".hidden", "Z"};
static const char* kPatterns[] = {"*.*", "*", "*.txt", "a", "b.*", "s*", "?", "*.dat", "none*", "SUB*", "*."};

static std::string LongName(long inRoom)   // a name that brings the path to about PLAT_MAX_PATH
{
    long theLength = inRoom + (long)R(7) - 3;
    if (theLength < 1)
        theLength = 1;
    return std::string((size_t)theLength, (char)('m' + R(3)));
}
static std::string Name()
{
    if (R(12) == 0)
        return LongName((long)PLAT_MAX_PATH - (long)(gVolume.size() + gRootDir.size()));
    if (R(4))
        return kNames[R(7)];   // mostly the plain names, so files exist, open and get read
    return kNames[R(sizeof kNames / sizeof *kNames)];
}
static std::string SubDir()   // a directory under the root, with its trailing backslash
{
    switch (R(6)) {
    case 0: return gRootDir;
    case 1: return gRootDir + "sub\\";
    case 2: return gRootDir + "Sub2\\";
    case 3: return gRootDir + "sub\\deep\\";
    case 4: return gRootDir + "missing\\";
    default: return gRootDir + "sub";   // no trailing backslash (nothing outside the root: no "\\")
    }
}
static std::string Path()
{
    switch (R(9)) {
    case 0: return "";
    case 1: return "a";
    case 2: return "ab";
    case 3: return "C:";
    case 4: return "abc.txt";
    case 5: return gVolume + SubDir();
    case 6: return "Q:" + SubDir() + Name();
    default: return gVolume + SubDir() + Name();
    }
}

static void WipeTree(const std::string& inDirectory)   // inDirectory ends in a backslash
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

static void Wipe(const std::string& inRoot)   // empties the root (made if missing)
{
    CreateDirectoryA(inRoot.c_str(), NULL);
    WipeTree(inRoot);
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: fsg <root\\> <sequences> [first seed]\n");
        return 2;
    }
    std::string theRoot = argv[1];
    gVolume = theRoot.substr(0, 2);
    gRootDir = theRoot.substr(2);
    unsigned long theCount = strtoul(argv[2], NULL, 10);
    unsigned long theFirst = argc > 3 ? strtoul(argv[3], NULL, 10) : 1;
    gStartupVolume = gVolume.c_str();
    gStartupDirectory = gRootDir.c_str();
    printf("root %zu\n", gRootDir.size());
    for (unsigned long n = theFirst; n < theFirst + theCount; n++) {
        Wipe(theRoot);
        gRand = n * 0x9E3779B97F4A7C15ull + 7;
        printf("seq %lu\n", n);
        Spec* s[4] = {};
        for (int k = 0; k < 4; k++) {
            s[k] = new Spec();
            Line("newd " + std::to_string(k) + " " + State(s[k]));
            std::string t = Name();
            s[k]->SetFile(UString(t.c_str()));
            Line("setf " + std::to_string(k) + " " + Hex(t) + " " + State(s[k]));
        }
        int theOps = 30 + (int)R(70);
        for (int o = 0; o < theOps; o++) {
            int k = (int)R(4), j = (int)R(4);
            std::string K = std::to_string(k), J = std::to_string(j);
            Spec* x = s[k];
            uint32_t theOp = R(40);
            static const uint32_t kIO[] = {21, 23, 25, 26, 27, 28, 29, 30, 18, 31, 21, 25};   // read, write, scalars, length, position
            if (x->GetIsOpen() && R(3))
                theOp = kIO[R(12)];
            else if (!x->GetIsOpen() && x->HasHandle() && R(2))
                theOp = 34;   // the search goes on
            else if (x->F().GetLength() == 0 && R(4) == 0)
                theOp = 32;   // a search of the directory
            if (getenv("FSG_FLUSH"))
                fprintf(stderr, "op %u %d %d\n", theOp, k, j);
            switch (theOp) {
            case 0: {   // a constructor
                int theKind = (int)R(3);
                std::string thePath = Path();
                Spec* theNew = theKind == 0 ? new Spec() : theKind == 1 ? new Spec(UString(thePath.c_str())) : new Spec(*s[j]);
                delete s[k];
                s[k] = theNew;
                Line(std::string(theKind == 0 ? "newd " : theKind == 1 ? "newp " : "newc ") + K +
                     (theKind == 1 ? " " + Hex(thePath) : theKind == 2 ? " " + J : "") + " " + State(s[k]));
                break;
            }
            case 1:
                if (x->GetIsOpen() || x->HasHandle())
                    break;   // the original drops an open file's or a search's handle (never closed: a leak)
                if (j != k)
                    *s[k] = *s[j];
                Line("asg " + K + " " + J + " " + State(x));
                break;
            case 2: case 3: {
                std::string t = Name();
                x->SetFile(UString(t.c_str()));
                Line("setf " + K + " " + Hex(t) + " " + State(x));
                break;
            }
            case 4: {
                std::string t = SubDir();
                x->SetDirectory(UString(t.c_str()));
                Line("setd " + K + " " + Hex(t) + " " + State(x));
                break;
            }
            case 5: {
                // (no empty volume: a search splits its path at byte 2 inside the root, which the replay cannot map)
                std::string t = R(5) ? gVolume : R(2) ? "Q:" : "C:";
                x->SetVolume(UString(t.c_str()));
                Line("setv " + K + " " + Hex(t) + " " + State(x));
                break;
            }
            case 6: case 7: if (x->GetIsOpen()) break;   // the original drops an open file's handle (Close: fclose(NULL))
                { long r = (long)(x->Create()); Line("create " + K + " = " + std::to_string(r) + " " + State(x)); } break;
            case 8: { long r = (long)(x->CreateDirectory()); Line("mkdir " + K + " = " + std::to_string(r) + " " + State(x)); } break;
            case 9: { long r = (long)(x->Delete()); Line("del " + K + " = " + std::to_string(r) + " " + State(x)); } break;
            case 10: { long r = (long)(x->DeleteDirectory()); Line("rmdir " + K + " = " + std::to_string(r) + " " + State(x)); } break;
            case 11: {
                std::string d = R(8) ? gVolume + SubDir() + Name() : std::string("Q:\\x");   // never relative
                int f = (int)R(2), a = (int)R(2);
                long r = x->Copy(UString(d.c_str()), f != 0, a != 0);
                Line("copy " + K + " " + Hex(d) + " " + std::to_string(f) + " " + std::to_string(a) + " = " +
                     std::to_string(r) + " " + State(x));
                break;
            }
            case 12: { long r = (long)(x->Lock()); Line("lock " + K + " = " + std::to_string(r) + " " + State(x)); } break;
            case 13: { long r = (long)(x->Unlock()); Line("unlock " + K + " = " + std::to_string(r) + " " + State(x)); } break;
            case 14: { long r = (long)(x->IsDirectory() ? 1 : 0); Line("isdir " + K + " = " + std::to_string(r) + " " + State(x)); } break;
            case 15: case 16: case 17: {
                if (x->GetIsOpen())
                    break;   // the original leaks the old handle (or closes a search handle as a file later)
                if (x->HasHandle())
                    x->CloseSearchHandle();
                unsigned long a = R(2) ? 3 : R(6);   // mostly read and write
                long r = x->Open(a);
                Line("open " + K + " " + std::to_string(a) + " = " + std::to_string(r) + " " + State(x));
                break;
            }
            case 18: case 19: { long r = (long)(x->Close()); Line("close " + K + " = " + std::to_string(r) + " " + State(x)); } break;
            case 20: {
                if (x->GetIsOpen() || x->HasHandle())
                    break;   // FOpen keeps a search handle, which Close then closes as a file
                const char* kModes[] = {"r", "rb", "w", "wb", "a+", "r+b"};
                int m = (int)R(6);
                long r = x->FOpen((char*)kModes[m]);
                Line("fopen " + K + " " + std::to_string(m) + " = " + std::to_string(r) + " " + State(x));
                break;
            }
            case 21: case 22: {
                unsigned long n2 = R(4) ? R(24) : R(9000), theRead = 0xdead;
                std::vector<unsigned char> theData(n2 + 1, 0xcc);
                long r = x->Read(theData.data(), n2, theRead);
                Line("read " + K + " " + std::to_string(n2) + " = " + std::to_string(r) + " " + std::to_string(theRead) +
                     " " + (theRead <= n2 ? HexBytes(theData.data(), theRead < 64 ? theRead : 64) : "?") + " " + State(x));
                break;
            }
            case 23: case 24: {
                unsigned long n2 = R(4) ? R(24) : R(9000), theWritten = 0xdead;
                std::vector<unsigned char> theData(n2 + 1);
                uint32_t theSeed = R();   // the bytes: (seed + i * 2654435761) >> 13 (the replay makes the same)
                for (size_t i = 0; i < theData.size(); i++)
                    theData[i] = (unsigned char)((theSeed + (uint32_t)i * 2654435761u) >> 13);
                long r = x->Write(theData.data(), n2, theWritten);
                Line("write " + K + " " + std::to_string(n2) + " " + std::to_string(theSeed) + " = " +
                     std::to_string(r) + " " + std::to_string(theWritten) + " " + State(x));
                break;
            }
            case 25: {   // the scalar reads (the value's bytes start as 0xcc)
                int t = (int)R(9);
                unsigned char v[8];
                memset(v, 0xcc, 8);
                long r;
                switch (t) {
                case 0: r = x->Read(*(uint32_t*)v); break;
                case 1: r = x->Read(*(int32_t*)v); break;
                case 2: r = x->Read(*(unsigned short*)v); break;
                case 3: r = x->Read(*(short*)v); break;
                case 4: r = x->Read(*(unsigned char*)v); break;
                case 5: r = x->Read(*(signed char*)v); break;
                case 6: r = x->Read(*(float*)v); break;
                case 7: r = x->Read(*(double*)v); break;
                default: r = x->Read(*(bool*)v); break;
                }
                Line("rds " + K + " " + std::to_string(t) + " = " + std::to_string(r) + " " + HexBytes(v, 8) + " " + State(x));
                break;
            }
            case 26: {
                int t = (int)R(9);
                unsigned char v[8];
                for (auto& c : v)
                    c = (unsigned char)R(256);
                v[0] &= t == 8 ? 1 : 0xff;
                long r;
                switch (t) {
                case 0: r = x->Write(*(uint32_t*)v); break;
                case 1: r = x->Write(*(int32_t*)v); break;
                case 2: r = x->Write(*(unsigned short*)v); break;
                case 3: r = x->Write(*(short*)v); break;
                case 4: r = x->Write(*(unsigned char*)v); break;
                case 5: r = x->Write(*(signed char*)v); break;
                case 6: r = x->Write(*(float*)v); break;
                case 7: r = x->Write(*(double*)v); break;
                default: r = x->Write(*(bool*)v); break;
                }
                Line("wrs " + K + " " + std::to_string(t) + " " + HexBytes(v, 8) + " = " + std::to_string(r) + " " + State(x));
                break;
            }
            case 27: {
                unsigned long l = 0xdead;
                long r = x->GetLength(l);
                Line("len " + K + " = " + std::to_string(r) + " " + std::to_string(l) + " " + State(x));
                break;
            }
            case 28: {
                long p = 0xdead;
                long r = x->GetPosition(p);
                Line("pos " + K + " = " + std::to_string(r) + " " + std::to_string(p) + " " + State(x));
                break;
            }
            case 29: {
                unsigned long l = R(40);
                long r = x->SetLength(l, R(2) != 0);
                Line("setlen " + K + " " + std::to_string(l) + " = " + std::to_string(r) + " " + State(x));
                break;
            }
            case 30: case 31: {   // 31: a rewind
                long off = theOp == 31 ? 0 : R(3) ? (long)R(40) - 20 : (long)R(20000) - 10000;
                unsigned long origin = theOp == 31 ? 0 : R(4);
                int e = theOp == 31 ? 0 : (int)R(2);
                long r = x->SetPosition(off, origin, e != 0);
                Line("setpos " + K + " " + std::to_string(off) + " " + std::to_string(origin) + " " + std::to_string(e) +
                     " = " + std::to_string(r) + " " + State(x));
                break;
            }
            case 32: case 33: {   // a search (never on an open spec: the original overwrote its file handle)
                if (x->GetIsOpen() || j == k || s[j]->GetIsOpen() || s[j]->HasHandle())   // (the out spec's handle: dropped)
                    break;
                if (x->HasHandle())
                    x->CloseSearchHandle();
                int pi = (int)R(sizeof kPatterns / sizeof *kPatterns + 1);
                long r = pi == 0 ? x->GetFirstFile(*s[j]) : x->GetFirstFile(UString(kPatterns[pi - 1]), *s[j]);
                Line("first " + K + " " + std::to_string(pi) + " " + J + " = " + std::to_string(r) + " " + State(x) + " " +
                     State(s[j]));
                break;
            }
            case 34: case 35: {
                if (x->GetIsOpen() || j == k || s[j]->GetIsOpen() || s[j]->HasHandle())   // (the out spec's handle: dropped)
                    break;
                long r = x->GetNextFile(*s[j]);
                Line("next " + K + " " + J + " = " + std::to_string(r) + " " + State(x) + " " + State(s[j]));
                break;
            }
            case 36: {
                if (j == k || s[j]->GetIsOpen() || s[j]->HasHandle())
                    break;   // the original leaks an open file's handle (its copy resets it)
                if (x->D().GetLength() < 1)
                    break;   // FindBackwards from (unsigned long)-2 on an empty directory reads before the text
                x->GetParentDirectory(*s[j]);
                Line("parent " + K + " " + J + " " + State(x) + " " + State(s[j]));
                if (strncmp((const char*)s[j]->D(), gRootDir.c_str(), gRootDir.size()) != 0) {
                    // the root's parent: never searched or written (the replay resets the same way)
                    delete s[j];
                    s[j] = new Spec();
                }
                break;
            }
            case 37: {
                if (x->GetIsOpen() || x->HasHandle())
                    break;
                unsigned long l = 0xdead;
                long r = x->GetLengthFromClosedFile(l);
                Line("lenc " + K + " = " + std::to_string(r) + " " + std::to_string(l) + " " + State(x));
                break;
            }
            case 38: {   // operator< on one spec (two files' times can tie within the clock's step: not replayable)
                if (x->GetIsOpen() || x->HasHandle())
                    break;
                bool r = *x < *x;
                Line("less " + K + " = " + std::to_string(r ? 1 : 0) + " " + State(x));
                break;
            }
            default: {
                if (x->GetIsOpen() || !x->HasHandle())
                    break;
                x->CloseSearchHandle();
                Line("csh " + K + " " + State(x));
                break;
            }
            }
        }
        for (int k = 0; k < 4; k++) {
            if (getenv("FSG_FLUSH"))
                fprintf(stderr, "delete %d\n", k);
            delete s[k];
        }
    }
    Wipe(theRoot);
    return 0;
}
