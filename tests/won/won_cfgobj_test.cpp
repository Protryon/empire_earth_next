// WON's config object model (ConfigObject, ConfigValue: the grammar of the lobby's .cfg files over the ConfigParser
// lexer): a table of snippets, each written to a file in the current directory and parsed into a test group with
// typed keys and three kinds of child group, one block of lines per snippet (the result, the error, the warnings and
// every group's keys), compared with rust/crates/ee_won/src/golden_cfgobj.txt (recorded over the C++ before its port;
// the Rust test `cfgobj_golden` checks the decisions against it). `--print` prints the lines instead;
// `--files DIR` parses each .cfg file of DIR into a plain ConfigObject and prints its result and warnings' CRC.
// The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONConfigObject.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "platform/PlatFile.h"

using namespace WONAPI;

static std::vector<std::string> sLines;

static std::string Escape(const std::string& inText)
{
    std::string anOut;
    char aBuffer[8];
    for (size_t i = 0; i < inText.size(); i++) {
        unsigned char c = (unsigned char)inText[i];
        if (c >= 0x20 && c < 0x7f && c != '%') {
            anOut += (char)c;
        } else {
            snprintf(aBuffer, sizeof(aBuffer), "%%%02x", c);
            anOut += aBuffer;
        }
    }
    return anOut;
}

class TestGroup : public ConfigObject {
public:
    std::string mText;
    unsigned short mCount;
    bool mFlag;
    std::wstring mWide;
    std::list<std::string> mItems, mRepl;
    SmartPtr<ConfigObject> mSlot;
    SmartPtr<ConfigObject> mFixed;
    static ConfigObject* Make() { return new TestGroup; }
    TestGroup()
    {
        this->KeyAdd("Text", this->mText, "def");
        this->KeyAdd("Count", this->mCount, 7);
        this->KeyAdd("Flag", this->mFlag, false);
        this->KeyAdd("Wide", this->mWide, L"w");
        this->KeyAdd("Items", this->mItems, "a, b", true);
        this->KeyAdd("Repl", this->mRepl, NULL, false);
        this->RegisterKeyGroup("Child", &TestGroup::Make);
        this->RegisterKeyGroup("Slot", &TestGroup::Make, &this->mSlot);
        this->mFixed = new ConfigObject;
        this->RegisterKeyGroup("Fixed", this->mFixed.get());
    }
};

static void Dump(const std::string& inPrefix, ConfigObject* inGroup)
{
    std::string aValues;
    inGroup->GetAllKeyValues(aValues);
    sLines.push_back(inPrefix + " name=" + Escape(inGroup->mName) + " base=" + Escape(inGroup->mBaseName) +
                     " keys=" + Escape(aValues));
    TestGroup* aTest = dynamic_cast<TestGroup*>(inGroup);
    if (aTest != NULL && aTest->mSlot.get() != NULL)
        Dump(inPrefix + ".slot", aTest->mSlot.get());
    inGroup->Rewind();
    int i = 0;
    while (ConfigObject* aChild = inGroup->NextChild()) {
        char aBuffer[16];
        snprintf(aBuffer, sizeof(aBuffer), ".%d", i++);
        Dump(inPrefix + aBuffer, aChild);
    }
}

static void WriteFile(const char* inName, const char* inText)
{
    FILE* f = fopen(inName, "wb");
    fwrite(inText, 1, strlen(inText), f);
    fclose(f);
}

// Shared with cfgobj_golden.rs: the snippets (the include file is won_cfgobj_inc.cfg)
static const char* const kInclude = "Text: included\nItems: inc\nDEFINE: IncDef 5\n";
static const char* const kSnippets[] = {
    "",
    "Text: hello\nCount: 12\nFlag: true\nWide: L\"wide\"\n",
    "text: lower case key\nCOUNT: 3\n",
    "Items: c, d\nItems: e\nRepl: x\nRepl: y, z\n",
    "Count: notanumber\nFlag: maybe\n",
    "Unknown: 1\nText: after\n",
    "Child { Text: one }\nChild { Count: 2 }\n",
    "Child Named { Text: n }\nChild : Base { }\nChild Named2 : Base2 { Flag: true }\n",
    "Slot { Text: slotted }\nSlot { Text: second }\n",
    "Fixed { }\nFixed { Text: x }\n",
    "Nosuch { Text: skipped Child { } }\nText: kept\n",
    "}\n",
    "Child { Text: open\n",
    "Child Text: x\n",
    "Child : { }\n",
    "DEFINE: Name value here\nText: $Name$\n",
    "DEFINE: A 1\nREDEFINE: A 2\nText: $A$\nDEFINE: A 3\nCount: $A$\n",
    "DEFINE:\nText: x\n",
    "DEFINE: Flag yes\nIFDEFINED Flag { Text: defined } else { Text: undefined }\n",
    "IFDEFINED Nope { Text: defined } else { Text: undefined }\n",
    "IFDEFINED Nope { Text: defined }\nCount: 4\n",
    "DEFINE: Mode fast\nIFDEFINED Mode : FAST { Text: fast } ELSE { Text: other }\n",
    "DEFINE: Mode fast\nIFDEFINED Mode : slow { Text: slow } else { Text: other }\n",
    "IFNOTDEFINED Nope { Text: notdefined } else { Text: defined }\n",
    "DEFINE: X 1\nIFNOTDEFINED X { Text: notdefined } else { Text: defined }\n",
    "IFDEFINED Nope { Text: a } else Text: b\n",
    "GROUPDEFINE Common { Text: common Count: 9 }\nChild { INCLUDEGROUP: Common }\nINCLUDEGROUP: Common\n",
    "INCLUDEGROUP: Missing\nINCLUDEGROUP: Missing2, optional\nINCLUDEGROUP: M3, other\nINCLUDEGROUP: M4,\n",
    "INCLUDEFILE: won_cfgobj_inc.cfg\nText: $IncDef$\n",
    "INCLUDEFILE: dir/won_cfgobj_inc.cfg\n",
    "INCLUDEFILE: nofile.cfg, optional\nText: fine\n",
    "INCLUDEFILE: nofile.cfg\nText: notreached\n",
    "INCLUDEFILE: nofile.cfg, bogus\n",
    "Child { DEFINE: Inner 1 }\nText: $Inner$\n",
    "Child { Child { Child { Text: deep } } }\n",
    "// comment\nText: a // not a comment?\n/* block */ Count: 5\n",
    "Text: \"quoted value\"\nItems: \"x y\", z\n",
    "Wide: plain\n",
    "Count: 70000\n",
    "Text: line \\\n continued\n",
    "Child\n{\nText: split\n}\n",
    "GROUPDEFINE Common { Text: common Count: 9 }\nChild { INCLUDEGROUP: Common }\nINCLUDEGROUP: Common\nFlag: true\n",
    "GROUPDEFINE G { Text: g }\nGROUPDEFINE G { Text: h }\nINCLUDEGROUP: G\nCount: 1\n",
    "GROUPDEFINE Bad Name { }\nCount: 1\n",
};

static void RunSnippet(int inIndex, const char* inText, bool inIgnorePath)
{
    WriteFile("won_cfgobj_case.cfg", inText);
    fprintf(stderr, "snippet %d\n", inIndex);
    fflush(stderr);
    TestGroup aRoot;
    ConfigParser aParser;
    aParser.mIgnorePathOnIncludeFile = inIgnorePath;
    bool r = false;
    std::string aThrown;
    try {
        r = aRoot.ParseTopLevel(aParser, "won_cfgobj_case.cfg");
    } catch (FileReaderException& e) {
        aThrown = " thrown=" + Escape(e.mDescription);
    }
    char aCase[32];
    snprintf(aCase, sizeof(aCase), "s%02d%s", inIndex, inIgnorePath ? "i" : "");
    sLines.push_back(std::string(aCase) + " r=" + (r ? "1" : "0") + aThrown + " error=" + Escape(aParser.mAbortReason) +
                     " warnings=" + Escape(aParser.mWarnings));
    Dump(std::string(aCase) + " root", &aRoot);
}

static unsigned long Crc(const std::string& inText)
{
    unsigned long c = 0xffffffffu;
    for (size_t i = 0; i < inText.size(); i++) {
        c ^= (unsigned char)inText[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return c ^ 0xffffffffu;
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_cfgobj.txt";
}

int main(int argc, char** argv)
{
    if (argc > 2 && strcmp(argv[1], "--files") == 0) {
        WIN32_FIND_DATAA aData;
        HANDLE h = FindFirstFileA((std::string(argv[2]) + "\\*.cfg").c_str(), &aData);
        if (h == INVALID_HANDLE_VALUE)
            return 1;
        do {
            std::string aPath = std::string(argv[2]) + "\\" + aData.cFileName;
            ConfigObject aRoot;
            ConfigParser aParser;
            bool r = aRoot.ParseTopLevel(aParser, aPath.c_str());
            std::string aValues;
            aRoot.GetAllKeyValues(aValues);
            printf("%s r=%d error=%s warnings=%08lx children=%d\n", aData.cFileName, r ? 1 : 0,
                   Escape(aParser.mAbortReason).c_str(), Crc(aParser.mWarnings), (int)aRoot.mChildList.size());
        } while (FindNextFileA(h, &aData));
        FindClose(h);
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "--one") == 0) {
        // one snippet from the command line ("\n" as a real line break)
        std::string aText;
        for (const char* p = argv[2]; *p; p++) {
            if (p[0] == '\\' && p[1] == 'n') {
                aText += '\n';
                p++;
            } else {
                aText += *p;
            }
        }
        RunSnippet(0, aText.c_str(), false);
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    WriteFile("won_cfgobj_inc.cfg", kInclude);
    PlatCreateDirectory("dir");
    WriteFile("dir\\won_cfgobj_inc.cfg", "Text: from dir\n");
    for (int i = 0; i < (int)(sizeof(kSnippets) / sizeof(kSnippets[0])); i++)
        RunSnippet(i, kSnippets[i], false);
    RunSnippet(29, kSnippets[29], true);
    remove("won_cfgobj_case.cfg");
    remove("won_cfgobj_inc.cfg");
    remove("dir\\won_cfgobj_inc.cfg");
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
    static char aBuffer[65536];
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
    printf("won_cfgobj_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
