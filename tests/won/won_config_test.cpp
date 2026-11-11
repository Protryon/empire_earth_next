// WON's ConfigParser (the lobby's .cfg reader): characters with pushback and line counts, comments, tokens (narrow
// and wide, to the end of a line, skipped), $define$ expansion, define scopes, group defines with saved states,
// parents, errors and warnings, over a table of inputs written as plain and as UTF-16 files. One line per step,
// compared with rust/crates/ee_won/src/golden_config.txt (recorded over the C++ parser before its port; the Rust
// test `config_golden` replays the C-locale pass). `--print` prints the lines instead. A second pass runs under
// setlocale(LC_ALL, "English") as the game's UI thread sets it (isspace is the C runtime's). `--files DIR...`
// prints, instead, one line per .cfg file under the given directories: the narrow token stream's CRC-16 and count
// (to compare a build over the C++ with one over the port on the real lobby files). Exit code: the failures.
#include <windows.h>
#include <locale.h>
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
#include "platform/PlatFile.h"

using namespace WONAPI;

// the inputs, shared with config_golden.rs (escapes: \n \r \t \\ \" \xNN only)
static const char* const kInputs[] = {
    "a b c",
    "key: value with spaces\nnext: 2\n",
    "# comment\n// c2\n/* block\n comment */ tok /x /*y*/z//w\n",
    "\"quoted string\" \"with \\\" escape\" \"unterminated\nnext",
    "a;b;c}d}{e}",
    "line one \\\ncontinued\nkey: a \\\n b\n",
    "x\r\ny\rz\r\n\r\nw",
    "$A$ pre$A$post \"q$A$\" \"\\$A$\" \\$B$ $missing$ end",
    "$un closed$",
    "k: a b # c\nk2: \"x y\" ; z\n",
    "{ } }}",
    "",
    "   \n\n  ",
    "tok\xa0tok2 \xe9t\xe9 \x85x",
    "a/b /c",
    "k: x // c\nk2: y /* d */\n",
    "\"a\nb\"",
    "trailing\\",
    "k:\nv",
    "k:   \n",
    "x$",
    "a \"b c\"d e\"\" \"\" f",
    "$A$$B$ $C$",
    "k: \"multi \\\nline\" w\n",
    "\\\\x \\a \\\n",
    "}",
    "a}",
    "k: v1 \\\n\n v2\n",
    "/",
    "x/",
    "\t\v\f a",
};

static std::vector<std::string> sLines;

static void Line(const std::string& inLine) { sLines.push_back(inLine); }

static std::string Esc(const std::string& inText)
{
    std::string anOut;
    char aBuffer[8];
    for (size_t i = 0; i < inText.size(); i++) {
        unsigned char c = (unsigned char)inText[i];
        if (c >= 0x20 && c < 0x7f && c != '\\' && c != '|')
            anOut += (char)c;
        else {
            snprintf(aBuffer, sizeof(aBuffer), "\\x%02x", c);
            anOut += aBuffer;
        }
    }
    return "|" + anOut + "|";
}

static std::string Units(const std::wstring& inText)
{
    std::string anOut;
    char aBuffer[12];
    for (size_t i = 0; i < inText.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%s%x", i ? "." : "", (unsigned)inText[i]);
        anOut += aBuffer;
    }
    return "|" + anOut + "|";
}

static std::string Int(long long inValue)
{
    char aBuffer[32];
    snprintf(aBuffer, sizeof(aBuffer), "%lld", inValue);
    return aBuffer;
}

static std::string FileName(int inCase, bool inUnicode)
{
    return "won_cfg_" + Int(inCase) + (inUnicode ? "u" : "") + ".cfg";
}

// writes a case's file: its bytes, or FF FE and each byte as a UTF-16 unit (plus two units above 0xff after a
// leading "U": not used by the narrow table)
static void WriteCase(int inCase, bool inUnicode)
{
    FILE* aFile = fopen(FileName(inCase, inUnicode).c_str(), "wb");
    const char* p = kInputs[inCase];
    if (inUnicode) {
        fputc(0xff, aFile);
        fputc(0xfe, aFile);
    }
    for (; *p; p++) {
        fputc((unsigned char)*p, aFile);
        if (inUnicode)
            fputc(0, aFile);
    }
    fclose(aFile);
}

static void Defines(ConfigParser& ioParser)
{
    ioParser.AddDefine("A", "va", false);
    ioParser.AddDefine("a", "dup", false);
    ioParser.AddDefine("B", "v\xc3\xa9", true);
    ioParser.AddDefine("b", "vb", true);
    ioParser.AddDefine("b c", "x", false);
    ioParser.AddDefine("C", "", false);
}

static std::string End(ConfigParser& ioParser, bool inResult)
{
    return "end " + std::string(inResult ? "true" : "false") + " line " + Int(ioParser.mLineNum) + " count " +
           Int(ConfigParser::GetLineCount()) + " eof " + (ioParser.EndOfFile() ? "1" : "0") + " abort " +
           Esc(ioParser.mAbortReason) + " warn " + Esc(ioParser.mWarnings);
}

static void Open(ConfigParser& ioParser, int inCase, bool inUnicode)
{
    ConfigParser::ResetLineCount();
    bool anOk = ioParser.OpenFile(FileName(inCase, inUnicode));
    Line("open " + std::string(anOk ? "true" : "false") + " unicode " + (ioParser.mIsUnicode ? "1" : "0") +
         " path " + Esc(ioParser.mFilePath) + " line " + Int(ioParser.mLineNum));
}

static void RunCase(const char* inLocale, int inCase, bool inUnicode)
{
    std::string aHead = std::string(inLocale) + " " + Int(inCase) + (inUnicode ? "u" : "") + " ";
    for (int aPass = 0; aPass < 6; aPass++) {
        ConfigParser aParser;
        Defines(aParser);
        Open(aParser, inCase, inUnicode);
        bool aResult = false;
        int aCount = 0;
        if (aPass == 0 || aPass == 1 || aPass == 4) {
            // 0: tokens; 1: a token then the rest of its line, alternately; 4: skipped tokens, then to the line's end
            std::string aToken;
            while (aCount < 60) {
                bool aToEnd = aPass == 1 ? (aCount & 1) != 0 : aPass == 4 ? (aCount & 1) != 0 : false;
                aResult = aParser.ReadToken(aToken, aToEnd, aPass == 4);
                if (!aResult)
                    break;
                Line(aHead + "p" + Int(aPass) + " tok " + Esc(aToken) + " line " + Int(aParser.mLineNum));
                aCount++;
            }
        } else if (aPass == 2 || aPass == 3) {
            // wide tokens; 3: to the line's end alternately
            std::wstring aToken;
            while (aCount < 60) {
                aResult = aParser.ReadToken(aToken, aPass == 3 && (aCount & 1) != 0, false);
                if (!aResult)
                    break;
                Line(aHead + "p" + Int(aPass) + " wtok " + Units(aToken) + " line " + Int(aParser.mLineNum));
                aCount++;
            }
        } else {
            // characters: pushback first, then peek and get to the end, a newline pushed back once
            aParser.UngetStr(std::string("q\n\xe9"));
            std::string aChars;
            bool anUnget = false;
            while (aCount < 200) {
                int aPeek = aParser.PeekChar();
                if (aParser.EndOfFile())
                    break;
                int aChar = aParser.GetChar();
                aChars += Int(aPeek) + "/" + Int(aChar) + " ";
                if (aChar == '\n' && !anUnget) {
                    aParser.UngetChar(aChar);
                    aParser.UngetChar(-1);
                    anUnget = true;
                    aChars += "u" + Int(aParser.mLineNum) + " ";
                }
                aCount++;
            }
            int aLastPeek = aParser.PeekChar();
            int aLastGet = aParser.GetChar();
            aChars += "peek " + Int(aLastPeek) + " get " + Int(aLastGet);
            Line(aHead + "p5 chars " + aChars);
        }
        Line(aHead + "p" + Int(aPass) + " " + End(aParser, aResult));
    }
}

// (each value is computed before its line is built: the operands of a chain of + are not sequenced)
static std::string Found(bool inFound, const std::string& inValue) { return (inFound ? "1 " : "0 ") + Esc(inValue); }
static std::string B(bool inValue) { return inValue ? "1" : "0"; }

static void RunScenarios(const char* inLocale)
{
    std::string aHead = std::string(inLocale) + " s ";
    // define scopes, the narrow and the wide lookup
    {
        ConfigParser aParser;
        std::string aValue = "unset";
        std::wstring aWide = L"unset";
        bool f = aParser.GetDefine("A", aValue);
        Line(aHead + "get-empty " + Found(f, aValue));
        aParser.PopDefineStack();
        aParser.PushDefineStack();
        f = aParser.GetDefine("A", aValue);
        Line(aHead + "get-null-scope " + Found(f, aValue));
        Defines(aParser);
        aParser.PushDefineStack();
        aParser.AddDefine("X", "inner", false);
        aParser.AddDefine("A", "shadow", false);
        f = aParser.GetDefine("x", aValue);
        Line(aHead + "inner-x " + Found(f, aValue));
        f = aParser.GetDefine("a", aValue);
        Line(aHead + "inner-a " + Found(f, aValue));
        f = aParser.GetDefine("B", aWide);
        Line(aHead + "inner-b-wide " + B(f) + " " + Units(aWide));
        aParser.PushDefineStack();
        f = aParser.GetDefine("A", aValue);
        Line(aHead + "empty-scope-a " + Found(f, aValue));
        aParser.PopDefineStack();
        aParser.PopDefineStack();
        aValue = "unset";
        f = aParser.GetDefine("X", aValue);
        Line(aHead + "outer-x " + Found(f, aValue));
        f = aParser.GetDefine("A", aValue);
        Line(aHead + "outer-a " + Found(f, aValue));
        f = aParser.GetDefine("c", aValue);
        Line(aHead + "outer-c " + Found(f, aValue));
        f = aParser.GetDefine("b c", aValue);
        Line(aHead + "outer-bc " + Found(f, aValue));
        bool anAdded = aParser.AddDefine("\xe9", "1", false);
        f = aParser.GetDefine("\xc9", aValue);
        Line(aHead + "case-e9 " + B(anAdded) + " " + Found(f, aValue));
        aParser.PopDefineStack();
        aParser.PopDefineStack();
        f = aParser.GetDefine("A", aValue);
        Line(aHead + "popped " + Found(f, aValue) + " warn " + Esc(aParser.mWarnings));
    }
    // a child's defines and warnings go to the root
    {
        ConfigParser aRoot, aChild, aGrandChild;
        aRoot.mWarningsAsErrors = true;
        aChild.SetParent(&aRoot);
        aGrandChild.SetParent(&aChild);
        aGrandChild.AddDefine("G", "vg", false);
        aGrandChild.AddDefine("g", "dup", false);
        std::string aValue;
        bool f = aRoot.GetDefine("g", aValue);
        Line(aHead + "root-g " + Found(f, aValue) + " root-warn " + Esc(aRoot.mWarnings) + " root-abort " +
             Esc(aRoot.mAbortReason) + " child-warn " + Esc(aGrandChild.mWarnings) + " child-flags " +
             B(aGrandChild.mWarningsAsErrors));
        aChild.PushDefineStack();
        aGrandChild.AddDefine("H", "vh", false);
        aChild.PopDefineStack();
        f = aRoot.GetDefine("H", aValue);
        Line(aHead + "root-h " + Found(f, aValue));
    }
    // group defines and saved states, across files
    {
        ConfigParser aParser;
        Open(aParser, 1, false);
        std::string aToken;
        aParser.ReadToken(aToken, false, false);
        aParser.UngetChar('Z');
        bool a1 = aParser.AddGroupDefine("G", NULL);
        bool a2 = aParser.AddGroupDefine("g", NULL);
        bool a3 = aParser.AddGroupDefine("s p", NULL);
        Line(aHead + "group-add " + B(a1) + " " + B(a2) + " " + B(a3) + " warn " + Esc(aParser.mWarnings));
        std::string aRead;
        for (int i = 0; i < 4 && aParser.ReadToken(aToken, false, false); i++)
            aRead += Esc(aToken) + " ";
        Line(aHead + "group-read " + aRead + "line " + Int(aParser.mLineNum));
        ConfigParserState aState;
        bool f = aParser.GetGroupDefine("g", aState);
        Line(aHead + "group-get " + B(f) + " pos " + Int(aState.mFilePos) + " line " + Int(aState.mLineNum) +
             " back " + Units(aState.mBackStr) + " file " + Esc(aState.mFileNameAndPath));
        ConfigParserState aNone;
        f = aParser.GetGroupDefine("none", aNone);
        Line(aHead + "group-missing " + B(f));
        aParser.SetState(aState);
        aRead = "";
        for (int i = 0; i < 4 && aParser.ReadToken(aToken, false, false); i++)
            aRead += Esc(aToken) + " ";
        Line(aHead + "group-again " + aRead + "line " + Int(aParser.mLineNum));
        ConfigParserState anOther;
        Open(aParser, 0, false);
        aParser.ReadToken(aToken, false, false);
        aParser.GetState(anOther);
        Line(aHead + "state-0 pos " + Int(anOther.mFilePos) + " line " + Int(anOther.mLineNum) + " back " +
             Units(anOther.mBackStr));
        f = aParser.SetState(aState);
        Line(aHead + "set-1 " + B(f) + " path " + Esc(aParser.mFileNameAndPath));
        aRead = "";
        for (int i = 0; i < 4 && aParser.ReadToken(aToken, false, false); i++)
            aRead += Esc(aToken) + " ";
        Line(aHead + "set-1-read " + aRead + "line " + Int(aParser.mLineNum));
        ConfigParserState aMissing = aState;
        aMissing.mFileNameAndPath = "won_cfg_none.cfg";
        f = aParser.SetState(aMissing);
        Line(aHead + "set-missing " + B(f) + " open " + B(aParser.mFileReader.IsOpen()) + " line " +
             Int(aParser.mLineNum));
        ConfigParser aChild;
        aChild.SetParent(&aParser);
        Open(aChild, 2, false);
        aChild.ReadToken(aToken, false, false);
        bool anAdded = aChild.AddGroupDefine("K", NULL);
        ConfigParserState aK;
        f = aParser.GetGroupDefine("k", aK);
        Line(aHead + "child-group " + B(anAdded) + " root " + B(f) + " pos " + Int(aK.mFilePos) + " line " +
             Int(aK.mLineNum) + " file " + Esc(aK.mFileNameAndPath));
    }
    // errors, warnings, reset
    {
        ConfigParser aParser;
        std::string anAbort = aParser.GetAbortReason();
        Line(aHead + "abort-empty " + Esc(anAbort));
        Open(aParser, 1, false);
        aParser.AddWarning("w1");
        aParser.SetError("e1", false);
        aParser.SetError("e2", false);
        aParser.SetError("e3", true);
        aParser.AddWarning("w2");
        anAbort = aParser.GetAbortReason();
        Line(aHead + "errors " + Esc(anAbort) + " warn " + Esc(aParser.mWarnings));
        std::string aToken;
        aParser.ReadToken(aToken, false, false);
        aParser.ReadToken(aToken, false, false);
        aParser.ReadToken(aToken, false, false);
        aParser.Reset();
        bool anEof = aParser.EndOfFile();
        Line(aHead + "reset abort " + Esc(aParser.mAbortReason) + " line " + Int(aParser.mLineNum) + " eof " +
             B(anEof));
        aParser.ReadToken(aToken, false, false);
        Line(aHead + "reset-read " + Esc(aToken) + " line " + Int(aParser.mLineNum));
        aParser.CloseFile();
        anEof = aParser.EndOfFile();
        int aPeek = aParser.PeekChar();
        int aGet = aParser.GetChar();
        Line(aHead + "closed eof " + B(anEof) + " peek " + Int(aPeek) + " get " + Int(aGet));
        aParser.UngetChar('x');
        anEof = aParser.EndOfFile();
        aPeek = aParser.PeekChar();
        aGet = aParser.GetChar();
        bool aRead = aParser.ReadToken(aToken, false, false);
        Line(aHead + "closed-unget eof " + B(anEof) + " peek " + Int(aPeek) + " get " + Int(aGet) + " read " +
             B(aRead));
        ConfigParser aMissing;
        bool anOpen = aMissing.OpenFile("won_cfg_dir/none.cfg");
        Line(aHead + "open-missing " + B(anOpen) + " path " + Esc(aMissing.mFilePath) + " line " +
             Int(aMissing.mLineNum));
        anOpen = aMissing.OpenFile("a\\b\\won_cfg_0.cfg");
        Line(aHead + "open-backslash " + B(anOpen) + " path " + Esc(aMissing.mFilePath));
    }
}

static void RunAll(const char* inLocale)
{
    for (int i = 0; i < (int)(sizeof(kInputs) / sizeof(kInputs[0])); i++) {
        WriteCase(i, false);
        WriteCase(i, true);
        RunCase(inLocale, i, false);
        RunCase(inLocale, i, true);
    }
    // the files of case 1 stay for the scenarios
    RunScenarios(inLocale);
    for (int i = 0; i < (int)(sizeof(kInputs) / sizeof(kInputs[0])); i++) {
        remove(FileName(i, false).c_str());
        remove(FileName(i, true).c_str());
    }
}

static unsigned short Crc16(unsigned short inCrc, const std::string& inData)
{
    for (size_t i = 0; i < inData.size(); i++) {
        inCrc ^= (unsigned char)inData[i];
        for (int b = 0; b < 8; b++)
            inCrc = (inCrc & 1) ? (inCrc >> 1) ^ 0xa001 : inCrc >> 1;
    }
    return inCrc;
}

// every .cfg file under a directory: the token stream (a token, then the rest of its line when it ends in ':')
static void RunFiles(const std::string& inDir)
{
    std::vector<std::string> aDirs(1, inDir);
    while (!aDirs.empty()) {
        std::string aDir = aDirs.back();
        aDirs.pop_back();
        PlatFindData aData;
        PlatFind* aFind = PlatFindFirst((aDir + "/*").c_str(), &aData);
        if (aFind == NULL)
            continue;
        do {
            std::string aName = aData.mName;
            if (aName == "." || aName == "..")
                continue;
            std::string aPath = aDir + "/" + aName;
            if (aData.mAttributes & PLAT_ATTR_DIRECTORY) {
                aDirs.push_back(aPath);
                continue;
            }
            if (aName.size() < 4 || _stricmp(aName.c_str() + aName.size() - 4, ".cfg") != 0)
                continue;
            ConfigParser aParser;
            aParser.OpenFile(aPath);
            unsigned short aCrc = 0;
            int aCount = 0;
            bool aToEnd = false;
            std::string aToken;
            std::wstring aWide;
            while (aCount < 200000) {
                bool anOk;
                if (aParser.mIsUnicode) {
                    anOk = aParser.ReadToken(aWide, aToEnd, false);
                    aToken = Units(aWide);
                } else
                    anOk = aParser.ReadToken(aToken, aToEnd, false);
                if (!anOk)
                    break;
                aCrc = Crc16(aCrc, aToken + "\n");
                aToEnd = !aToEnd && !aToken.empty() && aToken[aToken.size() - 1] == ':';
                aCount++;
            }
            printf("%s crc %04x tokens %d line %d abort %s\n", aPath.c_str(), aCrc, aCount, aParser.mLineNum,
                   Esc(aParser.mAbortReason).c_str());
        } while (PlatFindNext(aFind, &aData) == PLAT_FILE_OK);
        PlatFindClose(aFind);
    }
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_config.txt";
}

int main(int argc, char** argv)
{
    if (argc > 2 && strcmp(argv[1], "--files") == 0) {
        setlocale(LC_ALL, "English");
        for (int i = 2; i < argc; i++)
            RunFiles(argv[i]);
        return 0;
    }
    RunAll("c");
    if (setlocale(LC_ALL, "English") != NULL)
        RunAll("english");
    else
        Line("english: no such locale");
    setlocale(LC_ALL, "C");
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
    char aBuffer[8192];
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
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, aExpect);
        }
    }
    printf("won_config_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
