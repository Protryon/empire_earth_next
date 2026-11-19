// WON's ConfigParser, the original statements against the port: tests/won/won_cfgparser_orig.h (ConfigParser.cpp
// and ConfigToken.cpp as they were before WON round 21, over the lexer's C ABI) and the game's ConfigParser (the
// inline forwards of ee/wonapi/WONLobbyConfig.h over ee_won_net cfgparser.rs) run the same random sequences: a
// root, a child and a grandchild parser on each side, over files of random tokens (plain and UTF-16, missing ones,
// a name past the texts' 1,024-byte buffers), every public method with random arguments, and after each step
// every member compared (the line, the flags, the texts, the open file, the process-wide line count's change,
// each call's result and any exception). Exit code: the mismatches (the first ones are printed).
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
#include "won_cfgparser_orig.h"

using namespace WONAPI;

static unsigned long sSeed = 12345;
static int Rand(int n) { sSeed = sSeed * 1103515245 + 12345; return (int)((sSeed >> 8) % (unsigned long)n); }

static std::string Esc(const std::string& s)
{
    std::string o = "|";
    char b[8];
    for (size_t i = 0; i < s.length(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x20 && c < 0x7f && c != '\\' && c != '|') o += (char)c;
        else { snprintf(b, sizeof(b), "\\x%02x", c); o += b; }
    }
    return o + "|";
}
static std::string Units(const std::wstring& s)
{
    std::string o = "|";
    char b[16];
    for (size_t i = 0; i < s.length(); i++) { snprintf(b, sizeof(b), "%x.", (unsigned)s[i]); o += b; }
    return o + "|";
}
static std::string Int(long v) { char b[32]; snprintf(b, sizeof(b), "%ld", v); return b; }

static const char* const kPieces[] = {
    "a", "B", "key:", " ", "  ", "\t", "\n", "\r\n", "\r", "$A$", "$b$", "$C$", "$missing$", "$un closed", "x y",
    "\"", "\\", "\\\n", "{", "}", ";", "# c\n", "// c\n", "/* c */", "/", "*", "\xc3\xa9", "\xe9", "\xa0", "\x85",
    "value with spaces", "q$A$", "\\$A$", "tok2", "\"quoted $A$\"",
};
static const int kPieceCount = sizeof(kPieces) / sizeof(kPieces[0]);

static std::string RandomText(int inMax)
{
    std::string t;
    int n = Rand(inMax + 1);
    for (int i = 0; i < n; i++) t += kPieces[Rand(kPieceCount)];
    return t;
}

static std::string sDir;
static std::vector<std::string> sFiles;

static void WriteFiles()
{
    char aTemp[MAX_PATH];
    GetTempPathA(MAX_PATH, aTemp);
    sDir = std::string(aTemp) + "won_cfgparser_test\\";
    CreateDirectoryA(sDir.c_str(), NULL);
    for (int i = 0; i < 12; i++) {
        std::string aText = RandomText(40);
        std::string aName = sDir + "f" + Int(i) + ".cfg";
        FILE* f = fopen(aName.c_str(), "wb");
        if (i % 4 == 3) { // UTF-16 with its FF FE start
            fputc(0xff, f); fputc(0xfe, f);
            for (size_t k = 0; k < aText.length(); k++) { fputc((unsigned char)aText[k], f); fputc(k % 7 == 6 ? 1 : 0, f); }
        } else fwrite(aText.data(), 1, aText.length(), f);
        fclose(f);
        sFiles.push_back(aName);
    }
    sFiles.push_back(sDir + "missing.cfg");
    sFiles.push_back("bare_missing.cfg");
    sFiles.push_back(sDir + "sub/../f0.cfg");
    sFiles.push_back(std::string(1100, 'n') + ".cfg");
    sFiles.push_back("");
}

static std::string RandomName()
{
    static const char* const kNames[] = { "A", "a", "b", "B", "C", "c", "b c", "G", "g", "g h", "missing", "" };
    return kNames[Rand(12)];
}
static std::string RandomMessage()
{
    switch (Rand(6)) {
    case 0: return std::string(1500, 'm');
    case 1: return std::string("with\0nul", 8);
    case 2: return "";
    default: return RandomText(4);
    }
}

static int sChecks = 0, sFailures = 0;
static void Check(int inStep, const char* inWhat, const std::string& a, const std::string& b)
{
    sChecks++;
    if (a == b) return;
    if (++sFailures <= 20)
        printf("MISMATCH step %d %s\n  orig %s\n  port %s\n", inStep, inWhat, a.c_str(), b.c_str());
}

static std::string Members(OrigConfigParser& p)
{
    return "line " + Int(p.mLineNum) + " uni " + Int(p.mIsUnicode) + " stop " + Int(p.mStopped) + " wae " +
           Int(p.mWarningsAsErrors) + " ign " + Int(p.mIgnorePathOnIncludeFile) + " open " +
           Int(p.mFileReader.IsOpen()) + " abort " + Esc(p.mAbortReason) + " warn " + Esc(p.mWarnings) + " path " +
           Esc(p.mFilePath) + " name " + Esc(p.mFileNameAndPath);
}
static std::string Members(ConfigParser& p)
{
    return "line " + Int(p.GetLineNum()) + " uni " + Int(p.IsUnicode()) + " stop " + Int(p.IsStopped()) + " wae " +
           Int(p.GetWarningsAsErrors()) + " ign " + Int(p.GetIgnorePathOnIncludeFile()) + " open " +
           Int(p.IsFileOpen()) + " abort " + Esc(p.GetAbortText()) + " warn " + Esc(p.GetWarnings()) + " path " +
           Esc(p.GetFilePath()) + " name " + Esc(p.GetFileNameAndPath());
}
static std::string State(const ConfigParserState& s)
{
    return "pos " + Int(s.mFilePos) + " line " + Int(s.mLineNum) + " file " + Esc(s.mFileNameAndPath) + " back " +
           Units(s.mBackStr);
}

// one step on one side: the call's result as text (an exception's text included)
template <class P>
static std::string Step(int inOp, P* ps[3], int i, std::vector<ConfigParserState>& states, unsigned long inSeed)
{
    sSeed = inSeed; // both sides draw the same arguments
    P& p = *ps[i];
    std::string r;
    try {
        switch (inOp) {
        case 0: r = Int(p.OpenFile(sFiles[Rand((int)sFiles.size())])); break;
        case 1: { std::string t = "keep"; bool b = p.ReadToken(t, Rand(2) != 0, Rand(3) == 0); r = Int(b) + Esc(t); break; }
        case 2: { std::wstring t = L"keep"; bool b = p.ReadToken(t, Rand(2) != 0, Rand(3) == 0); r = Int(b) + Units(t); break; }
        case 3: r = Int(p.GetChar()); break;
        case 4: r = Int(p.PeekChar()); break;
        case 5: { static const int kChars[] = { -1, '\n', '\r', 'a', '}', 0xe9, -23, 0x1234, '$' }; p.UngetChar(kChars[Rand(9)]); break; }
        case 6: p.UngetStr(RandomText(3)); break;
        case 7: { std::wstring w; int n = Rand(4); for (int k = 0; k < n; k++) w += (wchar_t)(Rand(3) ? 'a' + Rand(26) : 0x100 + Rand(0xff00)); p.UngetStr(w); break; }
        case 8: p.SkipWhitespace(); break;
        case 9: r = Int(p.EndOfFile()); break;
        case 10: { std::string m = RandomMessage(); p.SetError(m, Rand(2) != 0); break; }
        case 11: p.AddWarning(RandomMessage()); break;
        case 12: p.CopyWarnings(*ps[Rand(3)]); break;
        case 13: r = Esc(p.GetAbortReason()); break;
        case 14: if (Rand(2)) p.PushDefineStack(); else p.PopDefineStack(); break;
        case 15: { std::string n = RandomName(); r = Int(p.AddDefine(n, RandomText(2), Rand(2) != 0)); break; }
        case 16: { std::string v = "keep"; bool b = p.GetDefine(RandomName(), v); r = Int(b) + Esc(v); break; }
        case 17: { std::wstring v = L"keep"; bool b = p.GetDefine(RandomName(), v); r = Int(b) + Units(v); break; }
        case 18: {
            std::string n = RandomName();
            if (!states.empty() && Rand(2)) r = Int(p.AddGroupDefine(n, &states[Rand((int)states.size())]));
            else r = Int(p.AddGroupDefine(n, NULL));
            break;
        }
        case 19: { ConfigParserState s; s.mFilePos = -7; s.mLineNum = -7; bool b = p.GetGroupDefine(RandomName(), s); r = Int(b) + State(s); break; }
        case 20: { ConfigParserState s; p.GetState(s); r = State(s); states.push_back(s); break; }
        case 21: {
            if (states.empty()) break;
            ConfigParserState s = states[Rand((int)states.size())];
            int k = Rand(4);
            if (k == 0) s.mFileNameAndPath = sFiles[Rand((int)sFiles.size())];
            if (k == 1) s.mFilePos += Rand(5) - 2;
            r = Int(p.SetState(s));
            break;
        }
        case 22: p.CloseFile(); break;
        case 23: p.Reset(); break;
        case 24: p.Stop(); break;
        case 25: break; // the flags (set by the caller on each side)
        case 26: if (i > 0) p.SetParent(ps[Rand(i)]); break;
        }
    } catch (FileReaderException& e) {
        r += " threw " + e.mDescription;
    }
    return r;
}

int main(int argc, char** argv)
{
    int aSequences = argc > 1 ? atoi(argv[1]) : 3000;
    WriteFiles();
    unsigned long aMaster = 777;
    for (int aSeq = 0; aSeq < aSequences; aSeq++) {
        OrigConfigParser o0, o1, o2;
        ConfigParser n0, n1, n2;
        OrigConfigParser* os[3] = { &o0, &o1, &o2 };
        ConfigParser* ns[3] = { &n0, &n1, &n2 };
        if (aSeq % 3 != 0) { o1.SetParent(&o0); n1.SetParent(&n0); }
        if (aSeq % 3 == 2) { o2.SetParent(&o1); n2.SetParent(&n1); }
        if (aSeq % 50 == 0) { // a warning with a NUL on a parser with no error yet (the text ends at the NUL)
            std::string w("w hidden", 8);
            o0.AddWarning(w); n0.AddWarning(w);
        }
        std::vector<ConfigParserState> anOrigStates, aNewStates;
        int aSteps = 10 + (int)(aMaster % 40);
        for (int s = 0; s < aSteps; s++) {
            aMaster = aMaster * 69069 + 1;
            unsigned long aSeed = aMaster;
            sSeed = aSeed;
            int anOp = Rand(27), i = Rand(3);
            if (anOp == 0 && Rand(4) != 0) anOp = 0; // opens often
            if (anOp == 25) {
                bool on = Rand(2) != 0, ign = Rand(4) == 0;
                os[i]->mWarningsAsErrors = on; ns[i]->SetWarningsAsErrors(on);
                if (ign) { os[i]->mIgnorePathOnIncludeFile = true; ns[i]->SetIgnorePathOnIncludeFile(true); }
            }
            if (anOp != 1 && anOp != 2 && anOp != 0 && Rand(3) == 0) { anOp = 1 + Rand(2); } // tokens most
            int aCount0 = ConfigParser::GetLineCount();
            std::string a = Step(anOp, os, i, anOrigStates, aSeed);
            int aCount1 = ConfigParser::GetLineCount();
            std::string b = Step(anOp, ns, i, aNewStates, aSeed);
            int aCount2 = ConfigParser::GetLineCount();
            std::string aWhat = "op " + Int(anOp) + " on " + Int(i);
            Check(aSeq * 100 + s, (aWhat + " result").c_str(), a + " count " + Int(aCount1 - aCount0),
                  b + " count " + Int(aCount2 - aCount1));
            for (int k = 0; k < 3; k++)
                Check(aSeq * 100 + s, (aWhat + " members of " + Int(k)).c_str(), Members(*os[k]), Members(*ns[k]));
        }
    }
    printf("won_cfgparser_test: %d sequences, %d checks, %d mismatches\n", aSequences, sChecks, sFailures);
    return sFailures;
}
