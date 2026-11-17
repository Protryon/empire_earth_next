// WONInternal's RegAuth (the auth settings kept under Software\Sierra\MPTools\Auth): a script of writes and reads of
// both forms over a scratch configuration file ($EE_CONFIG = won_regauth.yaml in the current directory), one line
// per step and the file's lines at the end, compared with rust/crates/ee_won_net/src/golden_regauth.txt (recorded over
// the C++ RegAuth.cpp before its port: net round 27). `--print` prints the lines instead. The exit code is the
// number of failures.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "won_regauth.h"

using namespace WONAPI;

static std::vector<std::string> sLines;

static std::string Hex(const std::string& inText)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inText.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<unsigned char>(inText[i]));
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

static void Line(const std::string& inText) { sLines.push_back(inText); }

static void ReadFull(const char* inCase)
{
    std::string anAddr = "keep", aLogin = "keep", aCommunity = "keep", aPassword = "keep";
    unsigned short aPort = 7;
    bool ok = ReadAuthSettings(anAddr, aPort, aLogin, aCommunity, aPassword);
    char aBuffer[64];
    snprintf(aBuffer, sizeof(aBuffer), " ok=%d port=%u", ok ? 1 : 0, (unsigned)aPort);
    Line(std::string(inCase) + " read5" + aBuffer + " addr=" + Hex(anAddr) + " login=" + Hex(aLogin) + " community="
         + Hex(aCommunity) + " password=" + Hex(aPassword));
}

static void ReadShort(const char* inCase)
{
    std::string aLogin = "keep", aCommunity = "keep", aPassword = "keep";
    bool ok = ReadAuthSettings(aLogin, aCommunity, aPassword);
    Line(std::string(inCase) + " read3 ok=" + (ok ? "1" : "0") + " login=" + Hex(aLogin) + " community="
         + Hex(aCommunity) + " password=" + Hex(aPassword));
}

static void Run()
{
    ReadFull("empty");
    ReadShort("empty");
    Line(std::string("write5 ok=") + (WriteAuthSettings("10.1.2.3", 15201, "Player", "Empire", "secret pw") ? "1" : "0"));
    ReadFull("written");
    ReadShort("written");
    Line(std::string("write5 nopw ok=") + (WriteAuthSettings("host.example", 80, "Other", "Comm", "") ? "1" : "0"));
    ReadFull("nopw");
    Line(std::string("write3 ok=") + (WriteAuthSettings("Third", "Com3", std::string("p\0w", 3)) ? "1" : "0"));
    ReadFull("third");
    ReadShort("third");
    Line(std::string("write3 long ok=") + (WriteAuthSettings("L", "C", std::string(40, 'x')) ? "1" : "0"));
    ReadShort("long");
    // a password value that is not hex, then an odd one
    RegKey aKey(1, KEY_TOOLS_AUTH, true, true);
    aKey.SetValue(VALUE_PASSWORD, std::string("zz"));
    ReadShort("bad hex");
    aKey.SetValue(VALUE_PASSWORD, std::string("abc"));
    ReadShort("odd hex");
    aKey.SetValue(VALUE_PASSWORD, std::string("0011"));
    ReadShort("short cipher");
    aKey.SetValue(VALUE_AUTHSERVERPORT, std::string("notanumber"));
    ReadFull("bad port");
    aKey.DeleteValue(VALUE_AUTHSERVERPORT);
    ReadFull("no port");
}

static void Dump()
{
    FILE* aFile = fopen("won_regauth.yaml", "rb");
    if (aFile == NULL) {
        Line("file (none)");
        return;
    }
    char aBuffer[4096];
    while (fgets(aBuffer, sizeof(aBuffer), aFile)) {
        size_t n = strlen(aBuffer);
        while (n && (aBuffer[n - 1] == '\n' || aBuffer[n - 1] == '\r'))
            aBuffer[--n] = 0;
        Line(std::string("file ") + aBuffer);
    }
    fclose(aFile);
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won_net/src/golden_regauth.txt";
}

int main(int argc, char** argv)
{
    remove("won_regauth.yaml");
#ifdef _WIN32
    _putenv("EE_CONFIG=won_regauth.yaml");
#else
    setenv("EE_CONFIG", "won_regauth.yaml", 1);
#endif
    Run();
    Dump();
    remove("won_regauth.yaml");
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
        if (strcmp(aGot, anExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, anExpect);
        }
    }
    printf("won_regauth_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
