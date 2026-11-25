// The network library's moves of R6 WON lobby and network round 37 that no other driver covers, against the original
// statements: InitLogic's SaveTOU (InitLogic.cpp's InitSaveTOU over a FileWriter, now ee_won_lobby flow.rs
// ee_won_init_save_tou). The terms of use come only from the Internet's patch servers, so no LAN game reaches it
// ("WON goldens by reach").
//
// Each case prepares the same file state twice, runs the original (Orig) on one and the Rust on the other, and logs
// the result, the file's bytes and its read-only flag after; the two lines must be equal. The cases: documents of
// 0, 1, 2 and 6 bytes (the last byte, the terminator, is not written), an existing file overwritten, a read-only
// file (made writable and written), a path in a missing directory and an empty path (neither opens: counted as
// written), a directory as the path. `--print` prints the lines. The exit code is the number of mismatches.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "won_file37_orig.h" // FileWriter
#include "platform/PlatFile.h"
#include "rust/won_lobby/flow.h"

using namespace WONAPI;

namespace Orig {
// InitLogic.cpp's InitSaveTOU as it was (the document is GetHttpDocument(owner)'s: a buffer of the op's bytes)
static bool InitSaveTOU(const std::string& inPath, const std::string& inDocument)
{
    FileWriter writer;
    try {
        if (writer.Open(inPath.c_str(), true)) {
            ByteBufferPtr document = new ByteBuffer(inDocument.data(), (unsigned long)inDocument.size(), false);
            size_t aBytes = document.get() != NULL ? ee_won_init_document_bytes(document->length()) : 0;
            if (aBytes != 0)
                writer.WriteBytes(document->data(), aBytes);
        }
        return true;
    } catch (FileWriterException&) {}
    return false;
}
}

static std::string ReadAll(const std::string& inPath)
{
    FILE* f = fopen(inPath.c_str(), "rb");
    if (f == NULL) return "(none)";
    std::string s;
    int c;
    while ((c = fgetc(f)) != EOF) s += (char)c;
    fclose(f);
    return "'" + s + "'";
}
static std::string State(const std::string& inPath)
{
    if (inPath.empty()) return "no path";
    uint32_t a = PlatPathGetAttributes(inPath.c_str());
    return ReadAll(inPath) + (a == PLAT_ATTR_INVALID ? " missing" : (a & PLAT_ATTR_READONLY) ? " read-only" : " writable");
}
// the file state a case starts from: 0 none, 1 an old file, 2 an old read-only file
static void Prepare(const std::string& inPath, int inState)
{
    if (inPath.empty()) return;
    WONFile(inPath).SetReadOnly(false);
    remove(inPath.c_str());
    if (inState == 0) return;
    FILE* f = fopen(inPath.c_str(), "wb");
    if (f != NULL) {
        fputs("old contents", f);
        fclose(f);
    }
    if (inState == 2) WONFile(inPath).SetReadOnly(true);
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    PlatCreateDirectory("won_net37_dir");
    struct Case { const char* mName; std::string mPath; int mState; std::string mDoc; };
    const Case kCases[] = {
        {"empty document", "won_net37_a.tou", 0, std::string()},
        {"one byte", "won_net37_a.tou", 0, std::string("x")},
        {"two bytes", "won_net37_a.tou", 0, std::string("ab")},
        {"terms", "won_net37_a.tou", 0, std::string("terms\0", 6)},
        {"over an old file", "won_net37_a.tou", 1, std::string("new terms!")},
        {"one byte over an old file", "won_net37_a.tou", 1, std::string("z")},
        {"read-only file", "won_net37_a.tou", 2, std::string("unlocked")},
        {"missing directory", "won_net37_missing/a.tou", 0, std::string("lost")},
        {"empty path", "", 0, std::string("none")},
        {"a directory", "won_net37_dir", 0, std::string("dir")},
    };
    int aMismatches = 0;
    for (const Case& c : kCases) {
        std::string aLines[2];
        for (int aSide = 0; aSide < 2; aSide++) {
            Prepare(c.mPath, c.mState);
            bool r = aSide == 0 ? Orig::InitSaveTOU(c.mPath, c.mDoc)
                                : ee_won_init_save_tou(c.mPath.c_str(), reinterpret_cast<const uint8_t*>(c.mDoc.data()), c.mDoc.size());
            aLines[aSide] = std::string(c.mName) + ": " + (r ? "saved" : "failed") + " " + State(c.mPath);
        }
        Prepare(c.mPath, 0);
        if (aPrint) printf("%s\n", aLines[1].c_str());
        if (aLines[0] != aLines[1]) {
            aMismatches++;
            printf("MISMATCH\n  orig %s\n  rust %s\n", aLines[0].c_str(), aLines[1].c_str());
        }
    }
    printf("won_net37_test: %d cases, %d mismatches\n", (int)(sizeof(kCases) / sizeof(kCases[0])), aMismatches);
    return aMismatches;
}
