// WON's WONFile path logic (the lobby's file paths: its persistent data, images, the file dialog): every path
// accessor over a table of paths, the normalisation, the absolute path and the relative path, one line per case,
// compared with rust/crates/ee_won/src/golden_path.txt (recorded over the C++ WONFile before its port; the Rust test
// `path_golden` replays the same table). `--print` prints the lines instead. The exit code is the number of failures.
// The absolute paths are made in a directory the test creates (<T>\a\b, <T> = won_path_cwd under the current
// directory) and printed with <T> for it and <V> for its volume; "%v" in an input is that volume in lower case.
// They are Windows paths: elsewhere the "full" lines are printed but not compared.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "platform/PlatFile.h"

using namespace WONAPI;

// Shared with path_golden.rs
static const char* const kPaths[] = {
    "", "a", "a.b", "dir/file.txt", "C:\\dir\\file.tar.gz", "\\\\server\\share\\x", "..\\x", "a\\..\\b",
    "a\\.\\b", "a\\b\\..\\..\\..\\c", "\\a\\..", ".\\", "a\\.", "a\\..b\\c", "a\\...\\c", "a\\.b", "dir.d\\file",
    "trailing\\", "C:", "%vrel\\x", "Q:rel", "Q:\\abs\\..\\y", "\\rooted\\p", "/fw/slash/./x", "x\\\\y",
    "\\..\\..\\a", "\xc3\xa9\\\xc3\xbc.txt", "a.\\b", ".hidden", "a\\b.", "..", ".", "\\", "\\\\", "a/../../b",
    "%v", "%v..\\..\\z","x\\..\\..", "a\\b\\.\\.\\c\\..\\d",
};

static const char* const kRelative[][2] = {
    {"C:\\a\\b\\f.txt", "C:\\a\\b\\c\\d"}, {"c:\\A\\B\\f", "C:\\a\\b\\x"}, {"a\\f", "a\\"}, {"a\\f", "a"},
    {"a\\f", "ab\\x"}, {"f", "\\x"}, {"f", "x"}, {"\xc3\xa9\\f", "\xc3\x89\\x"}, {"a\\f", "a\\x\\y"},
    {"a/b/f", "a\\b\\g"}, {"a\\b\\f", "a/b/g"}, {"", "x"}, {"", "\\"}, {"x\\f", "X\\"},
};

static std::vector<std::string> sLines;
static std::string sBase, sVolume;

static std::string Hex(const std::string& inBytes)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inBytes.size(); i++) {
        unsigned char c = (unsigned char)inBytes[i];
        if (c > 0x20 && c < 0x7f && c != '%') {
            anOut += (char)c;
        } else {
            snprintf(aBuffer, sizeof(aBuffer), "%%%02x", c);
            anOut += aBuffer;
        }
    }
    return anOut.empty() ? "-" : anOut;
}

static std::string Expand(const char* inPath)
{
    std::string anOut;
    std::string aLower = sVolume;
    for (size_t i = 0; i < aLower.size(); i++)
        if (aLower[i] >= 'A' && aLower[i] <= 'Z')
            aLower[i] = (char)(aLower[i] + 32);
    for (const char* p = inPath; *p; p++) {
        if (p[0] == '%' && p[1] == 'v') {
            anOut += aLower;
            p++;
        } else {
            anOut += *p;
        }
    }
    return anOut;
}

static std::string Collapse(const std::string& inPath)
{
    std::string anOut = inPath;
    if (!sBase.empty() && anOut.compare(0, sBase.size(), sBase) == 0)
        return "<T>" + anOut.substr(sBase.size());
    if (!sVolume.empty() && anOut.compare(0, sVolume.size(), sVolume) == 0)
        return "<V>" + anOut.substr(sVolume.size());
    return anOut;
}

static void Run()
{
    for (size_t i = 0; i < sizeof(kPaths) / sizeof(kPaths[0]); i++) {
        std::string anIn = Expand(kPaths[i]);
        char aCase[8];
        snprintf(aCase, sizeof(aCase), "p%02d", (int)i);
        try {
            WONFile f(anIn);
            std::string aPath = f.GetFilePath();
            std::string aDir = f.GetFileDirectory();
            std::string aName = f.GetFileName();
            std::string anExt = f.GetFileExtension();
            std::string aNoExt = f.mFileNameNoExtension;
            sLines.push_back(std::string(aCase) + " path=" + Hex(aPath) + " dir=" + Hex(aDir) + " name=" +
                             Hex(aName) + " ext=" + Hex(anExt) + " noext=" + Hex(aNoExt));
        } catch (const std::exception&) {
            sLines.push_back(std::string(aCase) + " path" + " throw");
        }
        try {
            WONFile f(anIn);
            bool r = f.NormalizePath();
            std::string aPath = f.GetFilePath();
            std::string aDir = f.GetFileDirectory();
            std::string aName = f.GetFileName();
            sLines.push_back(std::string(aCase) + " norm r=" + (r ? "1" : "0") + " path=" + Hex(aPath) + " dir=" +
                             Hex(aDir) + " name=" + Hex(aName));
        } catch (const std::exception&) {
            sLines.push_back(std::string(aCase) + " norm" + " throw");
        }
        try {
            // the name cached before the normalisation stays; the directory is recomputed
            WONFile f(anIn);
            std::string aName = f.GetFileName();
            f.NormalizePath();
            std::string aName2 = f.GetFileName();
            std::string aDir = f.GetFileDirectory();
            std::string aName3 = f.GetFileName();
            sLines.push_back(std::string(aCase) + " cached name=" + Hex(aName) + " after=" + Hex(aName2) + " dir=" +
                             Hex(aDir) + " name=" + Hex(aName3));
        } catch (const std::exception&) {
            sLines.push_back(std::string(aCase) + " cached" + " throw");
        }
        try {
            WONFile f(anIn);
            std::string aFull = Collapse(f.GetFullPath());
            sLines.push_back("full " + std::string(aCase) + " " + Hex(aFull));
        } catch (const std::exception&) {
            sLines.push_back("full " + std::string(aCase) + " throw");
        }
    }
    for (size_t i = 0; i < sizeof(kRelative) / sizeof(kRelative[0]); i++) {
        WONFile f(kRelative[i][0]);
        WONFile aSub(kRelative[i][1]);
        std::string aRel = f.GetRelativePath(aSub);
        char aCase[8];
        snprintf(aCase, sizeof(aCase), "r%02d", (int)i);
        sLines.push_back(std::string(aCase) + " rel=" + Hex(aRel));
    }
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_path.txt";
}

int main(int argc, char** argv)
{
    char aDir[PLAT_MAX_PATH];
    uint32_t aLength = PlatGetCurrentDirectory(aDir, sizeof(aDir));
    std::string aCwd(aDir, aLength);
    if (!aCwd.empty() && aCwd[aCwd.size() - 1] != '\\' && aCwd[aCwd.size() - 1] != '/')
        aCwd += '\\';
    sBase = aCwd + "won_path_cwd";
    PlatCreateDirectory(sBase.c_str());
    PlatCreateDirectory((sBase + "\\a").c_str());
    PlatCreateDirectory((sBase + "\\a\\b").c_str());
#ifdef _WIN32
    bool aMoved = SetCurrentDirectoryA((sBase + "\\a\\b").c_str()) != 0;
#else
    bool aMoved = chdir((sBase + "\\a\\b").c_str()) == 0;
#endif
    if (!aMoved) {
        printf("FAIL: cannot enter %s\\a\\b\n", sBase.c_str());
        return 1;
    }
    sVolume = sBase.size() >= 2 && sBase[1] == ':' ? sBase.substr(0, 2) : std::string();
    bool aWindowsPaths = !sVolume.empty();
    try {
        Run();
    } catch (const std::exception& e) {
        printf("FAIL: exception %s after %d lines\n", e.what(), (int)sLines.size());
        return 1;
    }
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", GoldenPath().c_str());
        return 1;
    }
    std::vector<std::string> aWant;
    char aBuffer[4096];
    while (fgets(aBuffer, sizeof(aBuffer), aFile)) {
        size_t n = strlen(aBuffer);
        while (n && (aBuffer[n - 1] == '\n' || aBuffer[n - 1] == '\r'))
            aBuffer[--n] = 0;
        aWant.push_back(aBuffer);
    }
    fclose(aFile);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* anExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (!aWindowsPaths && strncmp(anExpect, "full ", 5) == 0)
            continue;
        if (strcmp(aGot, anExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, anExpect);
        }
    }
    printf("won_path_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
