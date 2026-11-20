// WONInternal's TitleConfig and CommunityConfig whole in Rust (WON round 22): the C++ classes as they were
// (tests/won/won_titlecfg_orig.h, over the game's ConfigObject) against ee_won_net titleobj.rs (the objects, their
// typed values, hooks and texts over cfgobject.rs with a Rust host), on random title files: the same root (a title or
// a community, nested or not) parses the same files on its own parser, and the results are compared: the parse's
// result or exception, the parser's error, warnings, line and stop flag and the process-wide line count, then the
// whole tree (every member, every key's text and default flag, both ToString texts), then SetKeyValue,
// CopyKeyValsFrom and Reset on each and the tree again. Also the dummy community. Exit code: the mismatches.
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
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONConfigObject.h"
#include "rust/won_net/titleobj.h"
#include "won_titlecfg_orig.h"

using namespace WONAPI;

static unsigned long sSeed = 1;
static int Rand(int n) { sSeed = sSeed * 1103515245 + 12345; return (int)((sSeed >> 8) % (unsigned long)n); }
static std::string Int(long v) { char b[32]; snprintf(b, sizeof(b), "%ld", v); return b; }
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
static void Hex(std::string& o, const std::string& s)
{
    char b[4];
    for (size_t i = 0; i < s.length(); i++) { snprintf(b, sizeof(b), "%02x", (unsigned char)s[i]); o += b; }
    o += ";";
}
static void Units(std::string& o, const std::wstring& s)
{
    char b[16];
    for (size_t i = 0; i < s.length(); i++) { snprintf(b, sizeof(b), "%x,", (unsigned)s[i]); o += b; }
    o += ";";
}
static void Sink(void* c, const uint8_t* b, size_t n) { static_cast<std::string*>(c)->append((const char*)b, n); }

// ---- the original's side: ee_won_titleobj_dump's text for a C++ object
static std::string DumpOrig(ConfigObject* o)
{
    OrigCommunityConfig* c = dynamic_cast<OrigCommunityConfig*>(o);
    OrigTitleConfig* t = dynamic_cast<OrigTitleConfig*>(o);
    std::string s = "k" + Int(c ? 2 : t ? 1 : 0) + ";";
    Hex(s, o->mName);
    Hex(s, o->mBaseName);
    if (t == NULL) return s;
    s += "a" + Int(t->mAlreadyInTitle) + ";";
    Hex(s, t->mNetwork);
    s += "p" + Int(t->mNetPort) + ";";
    Hex(s, t->mImagePath);
    Hex(s, t->mHashString);
    Hex(s, t->mKeyPrefix);
    bool f[14] = {t->mForceCDKey, t->mRequireCrc, t->mAllowBetaKey, t->mRequireCDKey, t->mFailToGuest, t->mAllowGuest,
                  t->mConcurrentUserLogin, t->mConcurrentCDKeyLogin, t->mGetUserData, t->mGetForgeableUserData,
                  t->mCDKeyForInvite, t->mSaveSessionKey, t->mGetKeyID, t->mASCIIAccountNames};
    for (int i = 0; i < 14; i++) s += f[i] ? "1" : "0";
    s += ";";
    Units(s, t->mAdminCommunityName);
    if (c != NULL) {
        s += "c" + Int(c->mAlreadyInCommunity) + ";i" + Int(c->mCommunityID) + ";";
        Units(s, c->mCommunityName);
        Hex(s, c->mNameLoginStr);
        s += "n" + Int(c->mNameLogin) + ";";
        for (std::map<long, long>::const_iterator i = c->mTrustLevelLifespanMap.begin(); i != c->mTrustLevelLifespanMap.end(); ++i)
            s += Int(i->first) + ":" + Int(i->second) + ",";
        s += ";";
        for (std::list<std::list<int> >::const_iterator i = c->mCertificateLifeSpanList.begin(); i != c->mCertificateLifeSpanList.end(); ++i) {
            s += "(";
            for (std::list<int>::const_iterator j = i->begin(); j != i->end(); ++j) s += Int(*j) + ",";
            s += ")";
        }
        s += ";";
    }
    return s;
}

struct OrigSide {
    typedef ConfigObject* Obj;
    static Obj New(int inKind, bool inAlready)
    {
        ConfigObject* o = inKind == 2 ? (ConfigObject*)new OrigCommunityConfig(inAlready)
                        : inKind == 1 ? (ConfigObject*)new OrigTitleConfig(inAlready) : new ConfigObject;
        o->AddRef();
        return o;
    }
    static Obj NewDummy(const std::wstring& inName, unsigned long inId)
    {
        ConfigObject* o = new OrigCommunityConfig(inName, inId);
        o->AddRef();
        return o;
    }
    static void Release(Obj o) { o->Release(); }
    static int Parse(Obj o, ConfigParser& p, const char* inPath, std::string& outExc)
    {
        try {
            return o->ParseTopLevel(p, inPath) ? 1 : 0;
        } catch (FileReaderException& e) {
            outExc = e.mDescription;
            return -2;
        }
    }
    static std::string Node(Obj o)
    {
        std::string s = DumpOrig(o), v;
        o->GetAllKeyValues(v);
        s += "\n  keys " + Esc(v);
        if (OrigCommunityConfig* c = dynamic_cast<OrigCommunityConfig*>(o)) s += "\n  text " + Esc(c->ToString());
        else if (OrigTitleConfig* t = dynamic_cast<OrigTitleConfig*>(o))
            s += "\n  text " + Esc(t->ToString(true)) + "\n  bare " + Esc(t->ToString(false));
        return s;
    }
    static void Rewind(Obj o) { o->Rewind(); }
    static Obj Next(Obj o) { return o->NextChild(); }
    static int Set(Obj o, const std::string& k, const std::string& v) { StringParser sp(v.c_str()); return o->SetKeyValue(k, sp); }
    static void Copy(Obj o, Obj from) { o->CopyKeyValsFrom(from); }
    static void Reset(Obj o, bool b) { o->Reset(b); }
};

struct PortSide {
    typedef EeWonTitleObj* Obj;
    static Obj New(int inKind, bool inAlready) { return ee_won_titleobj_new((EeWonTitleObjKind)inKind, inAlready); }
    static Obj NewDummy(const std::wstring& inName, unsigned long inId)
    {
        return ee_won_titleobj_new_dummy(inName.data(), inName.length(), (uint32_t)inId);
    }
    static void Release(Obj o) { ee_won_titleobj_ref(o, false); }
    static int Parse(Obj o, ConfigParser& p, const char* inPath, std::string& outExc)
    {
        return ee_won_titleobj_parse(o, p.Obj(), (const uint8_t*)inPath, strlen(inPath), Sink, &outExc);
    }
    static std::string Node(Obj o)
    {
        std::string s, v;
        ee_won_titleobj_dump(o, Sink, &s);
        ee_won_cfgo_get_all_key_values(ee_won_titleobj_group(o), Sink, &v);
        s += "\n  keys " + Esc(v);
        std::string t, b;
        ee_won_titleobj_text(o, true, Sink, &t);
        EeWonTitleObjKind k = ee_won_titleobj_kind(o);
        if (k == EeWonTitleObjKind::Community) s += "\n  text " + Esc(t);
        else if (k == EeWonTitleObjKind::Title) {
            ee_won_titleobj_text(o, false, Sink, &b);
            s += "\n  text " + Esc(t) + "\n  bare " + Esc(b);
        }
        return s;
    }
    static void Rewind(Obj o) { ee_won_cfgo_rewind(ee_won_titleobj_group(o)); }
    static Obj Next(Obj o) { return (Obj)ee_won_cfgo_next_child(ee_won_titleobj_group(o)); }
    static int Set(Obj o, const std::string& k, const std::string& v)
    {
        StringParser sp(v.c_str());
        return ee_won_cfgo_set_key_value(ee_won_titleobj_group(o), (const uint8_t*)k.data(), k.length(), (EeWonCfgStringParser*)&sp);
    }
    static void Copy(Obj o, Obj from) { ee_won_cfgo_copy_key_vals_from(ee_won_titleobj_group(o), ee_won_titleobj_group(from)); }
    static void Reset(Obj o, bool b) { ee_won_cfgo_reset(ee_won_titleobj_group(o), b); }
};

template <class S> static std::string Tree(typename S::Obj o, const std::string& inIndent)
{
    std::string s = inIndent + S::Node(o) + "\n";
    S::Rewind(o);
    while (typename S::Obj c = S::Next(o)) s += Tree<S>(c, inIndent + "  ");
    return s;
}

static const char* const kKeys[] = {
    "ForceCDKey", "RequireCRC", "AllowBetaKey", "RequireCDKey", "FailToGuestLogin", "AllowGuestLogin",
    "ConcurrentUserLogin", "ConcurrentCDKeyLogin", "GetUserData", "GetForgeableUserData", "CDKeyForInvite",
    "SaveSessionKey", "GetKeyID", "ASCIIAccountNames", "NetPort", "Network", "ImagePath", "HashString", "KeyPrefix",
    "AdminCommunityName", "CommunityID", "CertificateLifeSpan", "NameLogin", "netport", "communityid", "Unknown",
};
static const int kKeyCount = sizeof(kKeys) / sizeof(kKeys[0]);
static const char* const kValues[] = {
    "yes", "no", "true", "off", "1", "maybe", "15", "0x20", "-3", "70000", "x", "", "\"quoted text\"", "L\"wide\"",
    "hw", "(1, 60), (2, 90)", "(3, 30), (1)", "(4, 5", "(x)", "(7, 8), (7, 9), (1, 2, 3)", "required", "OPTIONAL",
    "none", "Bogus", "a b c", "$N$", "\xc3\xa9t\xc3\xa9",
};
static const int kValueCount = sizeof(kValues) / sizeof(kValues[0]);
static const char* const kGroups[] = {
    "TITLEDEF T1 {\n", "TITLEDEF {\n", "COMMUNITY C1 {\n", "COMMUNITY C2 : base {\n", "COMMUNITY {\n", "}\n",
    "}\n", "}\n", "Other {\n", "GROUPDEFINE G {\nNetwork: g\n}\n", "INCLUDEGROUP: G\n", "INCLUDEFILE: cK.cfg\n",
    "INCLUDEFILE: none.cfg, optional\n", "DEFINE: N val\n", "IFDEFINED N {\n", "} else {\n", "Network: $N$\n",
    "titledef lower {\n", "community lower {\n",
};
static const int kGroupCount = sizeof(kGroups) / sizeof(kGroups[0]);

static std::string RandomFile(int inFile, int inMax)
{
    std::string t;
    int n = 1 + Rand(inMax), aDepth = 0;
    for (int i = 0; i < n; i++) {
        std::string p;
        if (Rand(3) != 0) p = std::string(kKeys[Rand(kKeyCount)]) + ": " + kValues[Rand(kValueCount)] + "\n";
        else {
            p = kGroups[Rand(kGroupCount)];
            if (p.find("INCLUDEGROUP") != std::string::npos && (aDepth != 0 || inFile != 0)) continue;
            size_t k = p.find("cK.cfg");
            if (k != std::string::npos) p.replace(k, 2, inFile >= 3 ? "none" : "c" + Int(inFile + 1 + Rand(3 - inFile)));
        }
        for (size_t j = 0; j < p.length(); j++) aDepth += p[j] == '{' ? 1 : p[j] == '}' ? -1 : 0;
        if (aDepth < 0) aDepth = 0;
        t += p;
    }
    while (aDepth-- > 0 && Rand(4) != 0) t += "}\n";
    return t;
}

// a well-formed title file: keys, then titles with keys and communities (the random pieces above mostly fail early)
static std::string Keys(int n)
{
    std::string t;
    for (int i = 0; i < n; i++) t += std::string(kKeys[Rand(kKeyCount)]) + ": " + kValues[Rand(kValueCount)] + "\n";
    return t;
}
static std::string FormedFile()
{
    std::string t = Keys(Rand(4));
    for (int i = Rand(3); i >= 0; i--) {
        bool aCommunity = Rand(3) == 0;
        t += std::string(aCommunity ? "COMMUNITY" : "TITLEDEF") + (Rand(4) ? " N" + Int(i) : std::string()) + " {\n" + Keys(Rand(6));
        for (int j = aCommunity ? 0 : Rand(3); j > 0; j--) {
            t += "COMMUNITY C" + Int(j) + (Rand(3) ? std::string() : " : B") + " {\n" + Keys(Rand(6));
            if (Rand(2)) t += "CertificateLifeSpan: " + std::string(kValues[15 + Rand(5)]) + "\n";
            if (Rand(2)) t += "NameLogin: " + std::string(kValues[20 + Rand(4)]) + "\n";
            if (Rand(2)) t += "CommunityID: " + Int(Rand(100) - 10) + "\n";
            if (Rand(8) == 0) t += "COMMUNITY Inner {\n" + Keys(2) + "}\n";
            t += "}\n";
        }
        t += "}\n";
    }
    return t;
}

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

template <class S> static std::string Run(unsigned long inSeed, int inKind, bool inAlready, bool inIgnore, bool inWarnErr)
{
    sSeed = inSeed;
    std::string o;
    ConfigParser::ResetLineCount();
    {
        ConfigParser aParser;
        aParser.SetIgnorePathOnIncludeFile(inIgnore);
        aParser.SetWarningsAsErrors(inWarnErr);
        typename S::Obj aRoot = S::New(inKind, inAlready);
        // the constructors leave the members as they are (uninitialised in C++) until a Reset: the parse's first
        // step, but a missing file fails before it, and the second object below is never parsed
        S::Reset(aRoot, false);
        std::string anExc;
        int r = S::Parse(aRoot, aParser, Rand(12) ? "c0.cfg" : "missing.cfg", anExc);
        o += "r " + Int(r) + " exc " + Esc(anExc) + "\n";
        if (r == 0) o += "reason " + Esc(aParser.GetAbortReason()) + "\n";
        o += "abort " + Esc(aParser.GetAbortText()) + "\nwarn " + Esc(aParser.GetWarnings()) + "\nline " +
             Int(aParser.GetLineNum()) + " count " + Int(ConfigParser::GetLineCount()) + " stopped " +
             Int(aParser.IsStopped()) + "\n";
        o += Tree<S>(aRoot, "");
        for (int i = 0; i < 3; i++) {
            std::string k = kKeys[Rand(kKeyCount)], v = kValues[Rand(kValueCount)];
            o += "set " + k + " " + Int(S::Set(aRoot, k, v)) + "\n";
        }
        typename S::Obj anOther = S::New(1 + Rand(2), Rand(2) != 0);
        S::Reset(anOther, false);
        o += "set other " + Int(S::Set(anOther, kKeys[Rand(kKeyCount)], kValues[Rand(kValueCount)])) + "\n";
        S::Copy(anOther, aRoot);
        o += Tree<S>(anOther, "copy ");
        S::Reset(aRoot, Rand(2) != 0);
        o += Tree<S>(aRoot, "reset ");
        S::Release(anOther);
        S::Release(aRoot);
    }
    return o;
}

template <class S> static std::string Dummy(const std::wstring& inName, unsigned long inId)
{
    typename S::Obj d = S::NewDummy(inName, inId);
    std::string s = Tree<S>(d, "dummy ");
    S::Release(d);
    return s;
}

int main(int argc, char** argv)
{
    int aSequences = argc > 1 ? atoi(argv[1]) : 2000;
    int aFailures = 0, aChecks = 0;
    unsigned long aMaster = 9191;
    static const wchar_t* const kNames[] = {L"", L"Guests", L"A\x00e9\x4e2d", L"x"};
    for (int i = 0; i < 4; i++) {
        std::string a = Dummy<OrigSide>(kNames[i], 7u * i), b = Dummy<PortSide>(kNames[i], 7u * i);
        aChecks++;
        if (a != b && ++aFailures <= 3) printf("MISMATCH dummy %d\n--- orig\n%s--- port\n%s", i, a.c_str(), b.c_str());
    }
    for (int s = 0; s < aSequences; s++) {
        aMaster = aMaster * 69069 + 1;
        sSeed = aMaster;
        for (int f = 0; f < 4; f++)
            WriteFile("c" + Int(f) + ".cfg", f == 0 && Rand(2) ? FormedFile() : RandomFile(f, f == 0 ? 40 : 10), Rand(8) == 0);
        int aKind = Rand(5) == 0 ? 2 : 1;
        bool anAlready = Rand(4) == 0, anIgnore = Rand(3) == 0, aWarnErr = Rand(10) == 0;
        unsigned long aSeed = aMaster ^ 0x3c3c;
        std::string a = Run<OrigSide>(aSeed, aKind, anAlready, anIgnore, aWarnErr);
        std::string b = Run<PortSide>(aSeed, aKind, anAlready, anIgnore, aWarnErr);
        aChecks++;
        if (argc > 2 && s < atoi(argv[2])) printf("--- sequence %d\n%s", s, a.c_str());
        if (a != b && ++aFailures <= 3) {
            printf("MISMATCH sequence %d\n--- orig\n%s--- port\n%s", s, a.c_str(), b.c_str());
            FILE* f = fopen("c0.cfg", "rb");
            char buf[4096];
            size_t n = f ? fread(buf, 1, sizeof(buf) - 1, f) : 0;
            if (f) fclose(f);
            buf[n] = 0;
            printf("--- c0.cfg\n%s\n", buf);
        }
    }
    printf("won_titleobj_test: %d checks, %d mismatches\n", aChecks, aFailures);
    return aFailures;
}
