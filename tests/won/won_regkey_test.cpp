// WON's RegKey on config.yaml (the lobby's UseLocalIP, the CD key's store): a script of opens, writes, reads and
// deletes over a scratch configuration file ($EE_CONFIG = won_regkey.yaml in the current directory), one line per
// step and the file's lines at the end, compared with rust/crates/ee_won/src/golden_regkey.txt (recorded over the C++
// RegKey before its port; the Rust test `regkey_golden` replays the names and values). `--print` prints the lines
// instead. The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONBuffer.h"

using namespace WONAPI;

static std::vector<std::string> sLines;

static void Line(const char* inFormat, ...)
{
    char aBuffer[2048];
    va_list anArgs;
    va_start(anArgs, inFormat);
    vsnprintf(aBuffer, sizeof(aBuffer), inFormat, anArgs);
    va_end(anArgs);
    sLines.push_back(aBuffer);
}

static std::string Hex(const void* inData, size_t inLength)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inLength; i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<const unsigned char*>(inData)[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

static std::string Hex(const std::string& inText)
{
    return Hex(inText.data(), inText.size());
}

static void Key(const char* inCase, RegKey& inKey)
{
    Line("%s open=%d block=%s path=%s", inCase, inKey.IsOpen() ? 1 : 0, Hex(inKey.mBlock).c_str(),
         Hex(inKey.mPath).c_str());
}

static void Query(const char* inCase, RegKey& inKey, const char* inName)
{
    DWORD aValue = 12345;
    int r = inKey.QueryDWORD(inName, aValue);
    std::string aText = "unchanged";
    RegKey::GetResult g = inKey.GetValue(inName, aText);
    ConstSmartPtr<ByteBuffer> aBinary;
    int b = inKey.QueryBinary(inName, aBinary);
    std::string aBytes = aBinary.get() != NULL ? Hex(aBinary->data(), aBinary->length()) : std::string("none");
    Line("%s %s dword=%d/%lu value=%d/%s binary=%d/%s", inCase, Hex(std::string(inName)).c_str(), r,
         (unsigned long)aValue, (int)g, Hex(aText).c_str(), b, aBytes.c_str());
}

// The scratch file's lines, as text (the keys' names are printable)
static void Dump()
{
    FILE* aConfig = fopen("won_regkey.yaml", "rb");
    if (aConfig == NULL) {
        Line("yaml none");
        return;
    }
    char aBuffer[4096];
    while (fgets(aBuffer, sizeof(aBuffer), aConfig)) {
        size_t n = strlen(aBuffer);
        while (n && (aBuffer[n - 1] == '\n' || aBuffer[n - 1] == '\r'))
            aBuffer[--n] = 0;
        if (aBuffer[0] != '#')
            Line("yaml %s", aBuffer);
    }
    fclose(aConfig);
}

// Shared with regkey.rs: the names a value or a key path is given
static const char* const kNames[] = {
    "", " ", "Plain", "  padded  ", "dot.ted", "co:lon", "tab\there", "\x01\x1f", "Sierra\\WONLobby", "a/b",
    "\xc3\xa9t\xc3\xa9", "UseLocalIP", "x . y",
};

static void Run()
{
    {
        RegKey k(2, "SOFTWARE\\Sierra\\WONLobby", false, true);
        Key("open", k);
        Query("missing", k, "UseLocalIP");
        Line("set dword=%d", k.SetValue("UseLocalIP", 1ul) ? 1 : 0);
        Query("dword", k, "UseLocalIP");
        Line("set big=%d", k.SetValue("Big", 0xfffffffful) ? 1 : 0);
        Query("big", k, "Big");
        Line("set wide=%d", k.SetValue("Wide", 0x123456789ul) ? 1 : 0);
        Query("wide", k, "Wide");
        Line("set text=%d", k.SetValue("Text", std::string("hello world")) ? 1 : 0);
        Query("text", k, "Text");
        Line("set hextext=%d", k.SetValue("HexText", std::string("0a1B")) ? 1 : 0);
        Query("hextext", k, "HexText");
        Line("set oddtext=%d", k.SetValue("OddText", std::string("abc")) ? 1 : 0);
        Query("oddtext", k, "OddText");
        Line("set badhex=%d", k.SetValue("BadHex", std::string("zz")) ? 1 : 0);
        Query("badhex", k, "BadHex");
        Line("set numtext=%d", k.SetValue("NumText", std::string("42")) ? 1 : 0);
        Query("numtext", k, "NumText");
        unsigned char aBytes[] = {0x00, 0x01, 0x7f, 0x80, 0xff};
        Line("set binary=%d", k.SetValue("Bin", aBytes, sizeof(aBytes)) ? 1 : 0);
        Query("binary", k, "Bin");
        Line("set empty=%d", k.SetValue("Empty", aBytes, 0) ? 1 : 0);
        Query("empty", k, "Empty");
        Line("set emptytext=%d", k.SetValue("EmptyText", std::string("")) ? 1 : 0);
        Query("emptytext", k, "EmptyText");
        for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
            char aCase[16];
            snprintf(aCase, sizeof(aCase), "name%02d", (int)i);
            int s = k.SetValue(kNames[i], std::string(aCase)) ? 1 : 0;
            std::string aText;
            RegKey::GetResult g = k.GetValue(kNames[i], aText);
            Line("%s set=%d get=%d/%s", aCase, s, (int)g, Hex(aText).c_str());
        }
        Dump();
        Line("delete=%d", k.DeleteValue("Text") ? 1 : 0);
        Line("delete again=%d", k.DeleteValue("Text") ? 1 : 0);
        Query("deleted", k, "Text");
        RegKey aSub(0, "x", false, true);
        Line("getsub=%d", k.GetSubKey("Sub\\Key", aSub) ? 1 : 0);
        Key("sub", aSub);
        Line("sub set=%d", aSub.SetValue("V", 7ul) ? 1 : 0);
        Line("getsub empty=%d", k.GetSubKey("", aSub) ? 1 : 0);
        Key("sub after empty", aSub);
        RegKey::DataType aType;
        std::string aName;
        Line("enum sub=%d values=%d", (int)k.GetFirstSubKey(aSub), (int)k.GetFirstValueName(aName, aType));
        Line("opensub=%d", k.OpenSubKey("Deeper/Still") ? 1 : 0);
        Key("opened", k);
        Line("opensub empty=%d", k.OpenSubKey("") ? 1 : 0);
        Line("deeper set=%d", k.SetValue("D", std::string("d")) ? 1 : 0);
    }
    {
        RegKey k(7, "SOFTWARE\\Bad", false, true);
        Key("root7", k);
        Query("root7", k, "UseLocalIP");
        Line("root7 set=%d delete=%d", k.SetValue("X", 1ul) ? 1 : 0, k.DeleteValue("X") ? 1 : 0);
        RegKey k2(-1, "SOFTWARE\\Bad", false, true);
        Key("root-1", k2);
        RegKey k3(0, "", false, true);
        Key("emptypath", k3);
        RegKey k4(1, "\\\\lead\\\\\\trail\\", false, true);
        Key("separators", k4);
        RegKey k5(6, "SOFTWARE/Sierra/WONLobby", false, true);
        Key("slashes", k5);
        Query("slashes", k5, "UseLocalIP");
        Line("reopen=%d", k5.OpenNewKey(3, "Other", true, true) ? 1 : 0);
        Key("reopened", k5);
        Line("reopen empty=%d", k5.OpenNewKey(3, "", true, true) ? 1 : 0);
        Key("reopened empty", k5);
    }
    {
        RegKey k(2, "SOFTWARE\\Sierra", false, true);
        Line("deletesub missing=%d", k.DeleteSubKey(std::string("Nope")) ? 1 : 0);
        Line("deletesub empty=%d", k.DeleteSubKey(std::string("")) ? 1 : 0);
        RegKey aChild(2, "WONLobby\\Sub", false, true);
        Line("deletesub child=%d", k.DeleteSubKey(aChild) ? 1 : 0);
        Key("child", aChild);
        Line("deletesub lobby=%d", k.DeleteSubKey(std::string("WONLobby")) ? 1 : 0);
        RegKey aLobby(2, "SOFTWARE\\Sierra\\WONLobby", false, true);
        Query("after delete", aLobby, "UseLocalIP");
        RegKey aClosed(9, "x", false, true);
        Line("closed deletesub=%d", aClosed.DeleteSubKey(aChild) ? 1 : 0);
    }
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_regkey.txt";
}

int main(int argc, char** argv)
{
    remove("won_regkey.yaml");
#ifdef _WIN32
    _putenv("EE_CONFIG=won_regkey.yaml");
#else
    setenv("EE_CONFIG", "won_regkey.yaml", 1);
#endif
    Run();
    Dump();
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
    printf("won_regkey_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
