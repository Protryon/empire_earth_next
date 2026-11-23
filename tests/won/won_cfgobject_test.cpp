// WON's ConfigObject, the original statements against the port: tests/won/won_cfgobject_orig.h (ConfigObject.cpp
// as it was before WON round 21, renamed OrigConfigObject) and the game's ConfigObject (the forwards of
// ee/wonapi/WONConfigObject.h over ee_won_net cfgobject.rs) parse the same random config files through the same
// subclass (every key type, factory groups, a group slot, a fixed group; hooks that throw both kinds of
// ConfigObjectException, fail, stop the parser, parse and skip children, read group headers, set and query keys,
// include files and copy their parent's keys), and the results are compared: the trees (names, base names, every
// key's value and default flag, the children, the slots), the parser's error, warnings and line counts, what the
// hooks logged and any exception. Then a Reset and a CopyKeyValsFrom on each. Exit code: the mismatches.
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
#include "won_cfgobject_orig.h"

using namespace WONAPI;

static unsigned long sSeed = 1;
static int Rand(int n) { sSeed = sSeed * 1103515245 + 12345; return (int)((sSeed >> 8) % (unsigned long)n); }

static std::string Esc(const std::string& s)
{
    std::string o;
    char b[8];
    for (size_t i = 0; i < s.length(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x20 && c < 0x7f && c != '%') o += (char)c;
        else { snprintf(b, sizeof(b), "%%%02x", c); o += b; }
    }
    return o;
}
static std::string Int(long v) { char b[32]; snprintf(b, sizeof(b), "%ld", v); return b; }

template <class B> struct Side { static std::string sLog; };
template <class B> std::string Side<B>::sLog;

template <class B> class TGroup : public B {
public:
    std::string mText;
    unsigned short mCount;
    bool mFlag;
    std::wstring mWide;
    std::list<std::string> mItems, mRepl;
    int mInt;
    std::list<std::list<int> > mLL;
    SmartPtr<B> mSlot;
    SmartPtr<B> mFixed;
    static B* Make() { return new TGroup; }
    static B* MakePlain() { return new B; }
    TGroup() : mCount(0), mFlag(false), mInt(0) // KeyAdd leaves the members as they are
    {
        this->KeyAdd("Text", this->mText, "def");
        this->KeyAdd("Count", this->mCount, 7);
        this->KeyAdd("Flag", this->mFlag, false);
        this->KeyAdd("Wide", this->mWide, L"w");
        this->KeyAdd("Items", this->mItems, "a, b", true);
        this->KeyAdd("Repl", this->mRepl, NULL, false);
        this->KeyAdd("TEXT", this->mText, "dup"); // a second registration is dropped
        this->RegisterIntValue("Int", this->mInt, 3);
        this->RegisterIntListListValue("LL", this->mLL, "(1, 2)", Rand(2) != 0);
        this->RegisterKeyGroup("Child", &TGroup::Make);
        this->RegisterKeyGroup("Plain", &TGroup::MakePlain);
        this->RegisterKeyGroup("Slot", &TGroup::Make, &this->mSlot);
        this->mFixed = new B;
        this->RegisterKeyGroup("Fixed", this->mFixed.get());
        this->RegisterKeyGroup("child", &TGroup::MakePlain); // dropped
    }
    void Log(const std::string& s) { Side<B>::sLog += "[" + Esc(this->mName) + "] " + s + "\n"; }
    virtual bool HandleInstruction(ConfigParser* p, const std::string& i)
    {
        if (i == "THROWE") throw ConfigObjectException("thrown error", true);
        if (i == "THROWW") throw ConfigObjectException("thrown warning", false);
        if (i == "STOP") { p->Stop(); return true; }
        if (i == "SUB") { SmartPtr<B> c = new TGroup; return this->ParseChild(*p, c.get(), Rand(2) != 0, true); }
        if (i == "RGS") {
            std::string n = "n0", b = "b0";
            bool r = this->ReadGroupStart(*p, n, b);
            this->Log("rgs " + Int(r) + " " + Esc(n) + " " + Esc(b));
            return r ? this->SkipGroup(*p, true, false) : false;
        }
        bool r = B::HandleInstruction(p, i);
        this->Log("hi " + Esc(i) + " " + Int(r));
        return r;
    }
    virtual bool HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
    {
        if (k == "THROWKEY") throw ConfigObjectException("key thrown", v.CheckNextChar('e'));
        if (k == "SETKEY") {
            std::string a;
            v.ReadString(a, true);
            this->Log("set " + Esc(a) + " " + Int(this->SetKeyValue(a, v)));
            return true;
        }
        if (k == "GETKEY") {
            std::string a, out = "pre";
            v.ReadString(a, true);
            bool r = this->GetKeyValue(a, out);
            this->Log("get " + Esc(a) + " " + Int(r) + " " + Esc(out));
            return true;
        }
        if (k == "INC") {
            std::string f;
            v.ReadString(f, false);
            return this->IncludeFile(*p, f, false);
        }
        bool r = B::HandleValueInstruction(p, k, v);
        this->Log("hv " + Esc(k) + " " + Int(r) + " pos " + Int(v.GetPos()));
        return r;
    }
    virtual bool PreParse(ConfigParser* p)
    {
        if (this->mName == "PREFAIL") return false;
        if (this->mName == "PRETHROW") throw ConfigObjectException("pre thrown", false);
        TGroup* aParent = dynamic_cast<TGroup*>(this->mParent);
        if (aParent != NULL && this->mBaseName == "COPY") this->CopyKeyValsFrom(aParent);
        return B::PreParse(p);
    }
    virtual bool PostParse(ConfigParser* p)
    {
        if (this->mName == "POSTFAIL") return false;
        if (this->mName == "POSTTHROW") throw ConfigObjectException("post thrown", true);
        return B::PostParse(p);
    }
    virtual void Reset(bool r)
    {
        this->Log("reset " + Int(r));
        B::Reset(r);
    }
};

template <class B> static std::string Dump(const std::string& inPrefix, B* g, int inDepth = 0)
{
    if (inDepth > 12) return inPrefix + " ...\n";
    std::string aValues = "v:";
    g->GetAllKeyValues(aValues);
    std::string o = inPrefix + " name=" + Esc(g->mName) + " base=" + Esc(g->mBaseName) + " keys=" + Esc(aValues) + "\n";
    TGroup<B>* t = dynamic_cast<TGroup<B>*>(g);
    if (t != NULL && t->mSlot.get() != NULL) o += Dump<B>(inPrefix + ".slot", t->mSlot.get(), inDepth + 1);
    if (t != NULL) o += inPrefix + " fixed " + Esc(t->mFixed->mName) + " : " + Esc(t->mFixed->mBaseName) + "\n";
    g->Rewind();
    int i = 0;
    while (B* c = (B*)g->NextChild()) o += Dump<B>(inPrefix + "." + Int(i++), c, inDepth + 1);
    o += inPrefix + " after " + Int(g->NextChild() != NULL) + "\n";
    return o;
}

static const char* const kPieces[] = {
    "Text: v\n", "Count: 12\n", "Count: x\n", "Flag: yes\n", "Wide: L\"w\"\n", "Items: c, d\n", "Repl: q\n",
    "Wide: L\"wide text\"\n", "SETKEY: Wide abc\n", "GETKEY: Wide\n", "GETKEY: Items\n", "GETKEY: LL\n",
    "GETKEY: Flag\n", "GETKEY: Int\n", "GETKEY: Text\n",
    "Int: -5\n", "LL: (4, 5), (6)\n", "Unknown: 1\n", "DEFINE: N val\n", "REDEFINE: N v2\n", "DEFINE: N\n",
    "Text: $N$\n", "INCLUDEGROUP: G\n", "INCLUDEGROUP: G, optional\n", "INCLUDEGROUP: Q, bad\n", "INCLUDEGROUP: Q,\n",
    "INCLUDEFILE: cK.cfg\n", "INCLUDEFILE: dir/cK.cfg, optional\n", "INCLUDEFILE: none.cfg\n",
    "INCLUDEFILE: none.cfg, OPTIONAL\n", "INCLUDEFILE: cK.cfg\n", "THROWKEY: e\n", "THROWKEY: x\n",
    "SETKEY: Text newval\n", "SETKEY: count 9\n", "SETKEY: Count bad\n", "SETKEY: Nokey 1\n", "GETKEY: Count\n",
    "GETKEY: none\n", "INC: cK.cfg\n", "Child {\n", "Child N : B {\n", "Child X : COPY {\n", "Slot {\n", "Fixed {\n",
    "Plain {\n", "Nosuch {\n", "GROUPDEFINE G {\n", "GROUPDEFINE G2 : x {\n", "GROUPDEFINE {\n", "IFDEFINED N {\n",
    "IFDEFINED N : val {\n", "IFDEFINED N : V2 {\n", "IFNOTDEFINED N {\n", "} else {\n", "} ELSE {\n", "}\n", "}\n",
    "}\n", "THROWE\n", "THROWW\n", "STOP\n", "SUB {\n", "RGS a : b {\n", "RGS {\n", "RGS a x\n",
    "Child PREFAIL {\n", "Child PRETHROW {\n", "Child POSTFAIL {\n", "Child POSTTHROW {\n", "{\n", ":\n",
    "Text: \"quoted $N$\"\n", "key:\n", "\"str\" {\n", "Child { Text: in } Count: 1\n",
};
static const int kPieceCount = sizeof(kPieces) / sizeof(kPieces[0]);

static void WriteFile(const std::string& inName, const std::string& inText, bool inWide)
{
    FILE* f = fopen(inName.c_str(), "wb");
    if (f == NULL) return;
    if (inWide) {
        fputc(0xff, f); fputc(0xfe, f);
        for (size_t i = 0; i < inText.length(); i++) { fputc((unsigned char)inText[i], f); fputc(0, f); }
    } else fwrite(inText.data(), 1, inText.length(), f);
    fclose(f);
}
// a file names only later files (no include cycle) and includes a group define only outside every brace (no
// group including itself)
static std::string RandomFile(int inFile, int inMax)
{
    std::string t;
    int n = 1 + Rand(inMax), aDepth = 0;
    for (int i = 0; i < n; i++) {
        std::string p = kPieces[Rand(kPieceCount)];
        if (p.find("INCLUDEGROUP") != std::string::npos && (aDepth != 0 || inFile != 0)) continue;
        size_t k = p.find("cK.cfg");
        if (k != std::string::npos)
            p.replace(k, 2, inFile >= 4 ? "none" : "c" + Int(inFile + 1 + Rand(4 - inFile)));
        for (size_t j = 0; j < p.length(); j++) aDepth += p[j] == '{' ? 1 : p[j] == '}' ? -1 : 0;
        if (aDepth < 0) aDepth = 0;
        t += p;
    }
    return t;
}

static bool sFixed = false;
template <class B> static std::string Run(unsigned long inSeed, bool inIgnorePath)
{
    sSeed = inSeed;
    Side<B>::sLog.erase();
    std::string o;
    ConfigParser::ResetLineCount();
    {
        ConfigParser aParser;
        aParser.SetIgnorePathOnIncludeFile(inIgnorePath);
        aParser.SetWarningsAsErrors(Rand(8) == 0);
        SmartPtr<B> aRoot = new TGroup<B>;
        try {
            bool r = aRoot->ParseTopLevel(aParser, (Rand(10) || sFixed) ? "c0.cfg" : "missing.cfg");
            o += "r " + Int(r) + "\n";
        } catch (ConfigObjectException& e) {
            o += "threw config " + Esc(e.mText) + " " + Int(e.mFlag) + "\n";
        } catch (FileReaderException& e) {
            o += "threw file " + Esc(e.mDescription) + "\n";
        }
        o += "abort " + Esc(aParser.GetAbortText()) + "\nwarn " + Esc(aParser.GetWarnings()) + "\nline " +
             Int(aParser.GetLineNum()) + " count " + Int(ConfigParser::GetLineCount()) + " stopped " +
             Int(aParser.IsStopped()) + "\n";
        o += Dump<B>("root", aRoot.get());
        SmartPtr<B> anOther = new TGroup<B>;
        StringParser aValue("77");
        o += "set " + Int(anOther->SetKeyValue("count", aValue)) + "\n";
        anOther->CopyKeyValsFrom(aRoot.get());
        o += Dump<B>("copy", anOther.get());
        aRoot->Reset(Rand(2) != 0);
        o += Dump<B>("reset", aRoot.get());
    }
    return o + Side<B>::sLog;
}

int main(int argc, char** argv)
{
    int aSequences = argc > 1 ? atoi(argv[1]) : 1500;
    CreateDirectoryA("dir", NULL);
    int aFailures = 0, aChecks = 0;
    unsigned long aMaster = 4242;
    // fixed cases first: an included group's error, a fixed group's names reset by its second parse
    static const char* const kFixed[] = {
        "GROUPDEFINE G { THROWKEY: e }\nINCLUDEGROUP: G\nText: after\n",
        "GROUPDEFINE G { THROWKEY: x Count: 4 }\nINCLUDEGROUP: G\nText: after\n",
        "GROUPDEFINE G { Count: 3 }\nTHROWW\nINCLUDEGROUP: G\n",
        "Fixed F : FB {\n}\nText: x\n",
        "Fixed F : FB {\n}\nFixed {\n}\n",
        "Fixed F : FB {\n}\nFixed G {\n}\n",
    };
    const int aFixedCount = sizeof(kFixed) / sizeof(kFixed[0]);
    for (int s = -aFixedCount; s < aSequences; s++) {
        aMaster = aMaster * 69069 + 1;
        sSeed = aMaster;
        if (s < 0) {
            WriteFile("c0.cfg", kFixed[s + aFixedCount], false);
            sFixed = true;
            std::string a = Run<OrigConfigObject>(aMaster, false), b = Run<ConfigObject>(aMaster, false);
            aChecks++;
            if (argc > 2) printf("--- fixed %d\n%s", s + aFixedCount, a.c_str()); // `N show`: the fixed cases' results
            if (a != b && ++aFailures <= 3) printf("MISMATCH fixed %d\n--- orig\n%s--- port\n%s", s, a.c_str(), b.c_str());
            continue;
        }
        sFixed = false;
        for (int f = 0; f < 5; f++) {
            std::string aText = RandomFile(f, f == 0 ? 30 : 8);
            bool aWide = Rand(6) == 0;
            WriteFile("c" + Int(f) + ".cfg", aText, aWide);
            WriteFile("dir/c" + Int(f) + ".cfg", RandomFile(4, 4), false);
        }
        bool anIgnore = Rand(3) == 0;
        unsigned long aSeed = aMaster ^ 0x5a5a;
        std::string a = Run<OrigConfigObject>(aSeed, anIgnore);
        std::string b = Run<ConfigObject>(aSeed, anIgnore);
        aChecks++;
        if (a != b) {
            if (++aFailures <= 3) {
                printf("MISMATCH sequence %d\n--- orig\n%s--- port\n%s", s, a.c_str(), b.c_str());
                FILE* f = fopen("c0.cfg", "rb");
                char buf[4096];
                size_t n = f ? fread(buf, 1, sizeof(buf) - 1, f) : 0;
                if (f) fclose(f);
                buf[n] = 0;
                printf("--- c0.cfg\n%s\n", buf);
            }
        }
    }
    printf("won_cfgobject_test: %d sequences, %d mismatches\n", aChecks, aFailures);
    return aFailures;
}
