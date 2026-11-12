// WONInternal's TitleConfig and CommunityConfig (HandleInstruction's children and errors, CommunityConfig::PostParse's
// name-login mode, image path, hash string, lifespan map and community name, both ToString texts): the original's
// statements (copied from TitleConfig.cpp before the port, over stand-ins) against
// include/ee/wonapi/WONTitleConfigMsg.h over ee_won's titlecfg module, on generated configs. The exit code is the
// number of failed checks.
// `--print N` prints N cases in the golden's format (rust/crates/ee_won/src/golden_titlecfg.txt: the inputs and the
// original's results in hex; `titlecfg::tests::golden` replays them).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONTitleConfigMsg.h"
#include "rust/ee_rust.h"

using namespace WONAPI;
using std::string;
using std::wstring;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 4242;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static string Hex(const string& s)
{
    static const char kHex[] = "0123456789abcdef";
    string r;
    for (size_t i = 0; i < s.size(); i++) { r += kHex[(unsigned char)s[i] >> 4]; r += kHex[s[i] & 15]; }
    return r.empty() ? "-" : r;
}
static string FromSet(const char* inSet, int n)
{
    string s;
    size_t k = strlen(inSet);
    for (int i = 0; i < n; i++) s += inSet[Pick((int)k)];
    return s;
}

struct SParser {
    string mLog;
    void SetError(const string& s, bool b) { this->mLog += "error[" + s + "]" + (b ? "1" : "0") + ";"; }
};

// the members TitleConfig.cpp reads, with ConfigObject's name; ParseChild logged
struct SCfg {
    enum NameLogin { NameLogin_None = 0, NameLogin_Optional = 1, NameLogin_Required = 2, NameLogin_Unknown = 3 };
    string mName;
    string mNetwork;
    unsigned short mNetPort;
    bool mForceCDKey;
    string mImagePath, mHashString, mKeyPrefix;
    bool mRequireCrc, mAllowBetaKey, mRequireCDKey, mFailToGuest, mAllowGuest, mConcurrentUserLogin,
        mConcurrentCDKeyLogin, mGetUserData, mGetForgeableUserData, mCDKeyForInvite, mSaveSessionKey, mGetKeyID,
        mASCIIAccountNames;
    int mCommunityID;
    wstring mCommunityName;
    string mNameLoginStr;
    NameLogin mNameLogin;
    std::map<long, long> mTrustLevelLifespanMap;
    std::list<std::list<int> > mCertificateLifeSpanList;
    string mLog;
    bool mChildOk;
    bool ParseTitleChild(SParser&) { this->mLog += "title;"; return this->mChildOk; }
    bool ParseCommunityChild(SParser&, bool inNested) { this->mLog += inNested ? "community1;" : "community0;"; return this->mChildOk; }
    string Dump() const
    {
        char b[64];
        string s = Hex(this->mName) + " " + Hex(this->mImagePath) + " " + Hex(this->mHashString) + " " +
                   Hex(WStringToString(this->mCommunityName)) + " ";
        snprintf(b, sizeof(b), "%d %u ", (int)this->mNameLogin, (unsigned)this->mCommunityName.size());
        s += b;
        for (std::map<long, long>::const_iterator i = this->mTrustLevelLifespanMap.begin();
             i != this->mTrustLevelLifespanMap.end(); ++i) {
            snprintf(b, sizeof(b), "%ld:%ld,", i->first, i->second);
            s += b;
        }
        return s + " " + this->mLog;
    }
};

// ---- the original's statements (TitleConfig.cpp), over the stand-ins
namespace Orig {
static bool TitleHandleInstruction(SCfg& o, bool mAlreadyInTitle, SParser* theParser, const std::string& theTitleType)
{
    if (theTitleType == "TITLEDEF") {
        if (mAlreadyInTitle) {
            theParser->SetError("You can't nest Title definitions!", false);
            return false;
        }
        return o.ParseTitleChild(*theParser);
    } else if (theTitleType == "COMMUNITY") {
        return o.ParseCommunityChild(*theParser, false);
    } else
        return false;
}
static bool CommunityHandleInstruction(SCfg& o, bool mAlreadyInCommunity, SParser* theParser, const std::string& theCommunityType)
{
    if (theCommunityType == "TITLEDEF") {
        theParser->SetError("You can't have a title definition within a community!", false);
        return false;
    } else if (theCommunityType == "COMMUNITY") {
        if (mAlreadyInCommunity) {
            theParser->SetError("You can't nest communities!", false);
            return false;
        }
        return o.ParseCommunityChild(*theParser, true);
    } else
        return false;
}
static bool PostParse(SCfg& o, SCfg* anObjectP)
{
    string aNameLoginStr = StringToUpperCase(o.mNameLoginStr);
    if (aNameLoginStr == "REQUIRED")
        o.mNameLogin = SCfg::NameLogin_Required;
    else if (aNameLoginStr == "OPTIONAL")
        o.mNameLogin = SCfg::NameLogin_Optional;
    else if (aNameLoginStr == "NONE")
        o.mNameLogin = SCfg::NameLogin_None;
    else
        o.mNameLogin = SCfg::NameLogin_Unknown;

    if (o.mImagePath.empty()) {
        o.mImagePath = "./";
        if (anObjectP)
            o.mImagePath += anObjectP->mName;
        else
            o.mImagePath += o.mName;
    }
    if (o.mHashString.empty()) {
        if (anObjectP)
            o.mHashString = anObjectP->mName;
        else
            o.mHashString = o.mName;
    }
    long aTrustLevel, aDuration;
    o.mTrustLevelLifespanMap.clear();
    for (std::list<std::list<int> >::const_iterator anItr = o.mCertificateLifeSpanList.begin(); anItr != o.mCertificateLifeSpanList.end(); ++anItr) {
        std::list<int>::const_iterator anItr2 = anItr->begin();
        if (anItr2 == anItr->end())
            continue;
        aTrustLevel = *anItr2;
        ++anItr2;
        if (anItr2 != anItr->end()) {
            aDuration = *anItr2;
            o.mTrustLevelLifespanMap[aTrustLevel] = aDuration;
        }
    }
    if (o.mCommunityName.empty())
        o.mCommunityName = StringToWString(o.mName);
    return true;
}
static string TitleToString(const SCfg& o, bool includeName)
{
    string aStr;
    if (includeName)
        aStr += o.mName + "\r\n";
    aStr += string("\tNetwork: ") + (o.mNetwork.empty() ? string("EMPTY") : o.mNetwork) + "\r\n";
    char aBuffer[20];
    sprintf(aBuffer, "%d", o.mNetPort);
    aStr += string("\tNetPort: ") + string(aBuffer) + "\r\n";
    aStr += string("\tForceCDKey: ") + string((o.mForceCDKey) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tImagePath: ") + (o.mImagePath.empty() ? string("EMPTY") : o.mImagePath) + "\r\n";
    aStr += string("\tHashString: ") + (o.mHashString.empty() ? string("EMPTY") : o.mHashString) + "\r\n";
    aStr += string("\tKeyPrefix: ") + (o.mKeyPrefix.empty() ? string("EMPTY") : o.mKeyPrefix) + "\r\n";
    aStr += string("\tRequireCrc: ") + string((o.mRequireCrc) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tAllowBetaKey: ") + string((o.mAllowBetaKey) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tRequireCDKey: ") + string((o.mRequireCDKey) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tFailToGuest: ") + string((o.mFailToGuest) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tAllowGuest: ") + string((o.mAllowGuest) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tConcurrentUserLogin: ") + string((o.mConcurrentUserLogin) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tConcurrentCDKeyLogin: ") + string((o.mConcurrentCDKeyLogin) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tGetUserData: ") + string((o.mGetUserData) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tGetForgeableUserData: ") + string((o.mGetForgeableUserData) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tCDKeyForInvite: ") + string((o.mCDKeyForInvite) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tSaveSessionKey: ") + string((o.mSaveSessionKey) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tGetKeyID: ") + string((o.mGetKeyID) ? "TRUE" : "FALSE") + "\r\n";
    aStr += string("\tASCIIAccountNames: ") + string((o.mASCIIAccountNames) ? "TRUE" : "FALSE") + "\r\n";
    return aStr;
}
static string CommunityToString(const SCfg& o)
{
    string aStr = ((o.mCommunityName.empty()) ? o.mName : WStringToString(o.mCommunityName)) + "\r\n";
    char aBuffer[25];
    sprintf(aBuffer, "%d", o.mCommunityID);
    aStr += string("CommunityID: ") + string(aBuffer) + "\r\n";
    aStr += TitleToString(o, false);
    aStr += string("\tNameLogin: ") + o.mNameLoginStr + "\r\n";
    aStr += "\tTrust Level Lifespans: [";
    if (o.mTrustLevelLifespanMap.empty())
        aStr += "EMPTY";
    else {
        bool isFirst = true;
        for (std::map<long, long>::const_iterator anItr = o.mTrustLevelLifespanMap.begin(); anItr != o.mTrustLevelLifespanMap.end(); ++anItr) {
            if (isFirst)
                isFirst = false;
            else
                aStr += ",";
            sprintf(aBuffer, "(%d,%d)", anItr->first, anItr->second);
            aStr += string(aBuffer);
        }
    }
    aStr += "]\r\n";
    return aStr;
}
} // namespace Orig

static std::wstring ToWide(const std::string& s) { return StringToWString(s); }

static string RandText()
{
    switch (Pick(6)) {
    case 0: return "";
    case 1: return FromSet("abcXYZ._/ ", 1 + Pick(12));
    case 2: return FromSet("ab", 1);
    case 3: return "\xc3\xa9t\xc3\xa9";
    default: return FromSet("abcdefghijklmnopqrstuvwxyz0123456789", 1 + Pick(20));
    }
}
static string RandLogin()
{
    static const char* kWords[] = {"Required", "REQUIRED", "required", "Optional", "optional", "NONE", "none", "None",
                                   "", "req", "Requiredx", " none", "NoNe"};
    if (Pick(5) == 0) return FromSet("RQUIEDONPTALnrqe ", Pick(10));
    return kWords[Pick(13)];
}
static int RandInt()
{
    switch (Pick(5)) {
    case 0: return 0;
    case 1: return (int)(Next() << 8);
    case 2: return -(int)Pick(100);
    default: return Pick(10);
    }
}
static void RandCfg(SCfg& o)
{
    o.mName = RandText();
    o.mNetwork = RandText();
    o.mNetPort = (unsigned short)(Pick(3) == 0 ? 65535 - Pick(3) : Next());
    o.mForceCDKey = Pick(2) != 0;
    o.mImagePath = Pick(2) ? "" : RandText();
    o.mHashString = Pick(2) ? "" : RandText();
    o.mKeyPrefix = RandText();
    bool* aBools[] = {&o.mRequireCrc, &o.mAllowBetaKey, &o.mRequireCDKey, &o.mFailToGuest, &o.mAllowGuest,
                      &o.mConcurrentUserLogin, &o.mConcurrentCDKeyLogin, &o.mGetUserData, &o.mGetForgeableUserData,
                      &o.mCDKeyForInvite, &o.mSaveSessionKey, &o.mGetKeyID, &o.mASCIIAccountNames};
    for (int i = 0; i < 13; i++) *aBools[i] = Pick(2) != 0;
    o.mCommunityID = RandInt();
    o.mCommunityName = Pick(2) ? L"" : StringToWString(RandText());
    o.mNameLoginStr = RandLogin();
    o.mNameLogin = (SCfg::NameLogin)(Pick(4));
    o.mTrustLevelLifespanMap.clear();
    if (Pick(4) == 0) o.mTrustLevelLifespanMap[RandInt()] = RandInt();
    o.mCertificateLifeSpanList.clear();
    int n = Pick(4) == 0 ? 0 : Pick(6);
    for (int i = 0; i < n; i++) {
        std::list<int> l;
        int k = Pick(4);
        for (int j = 0; j < k; j++) l.push_back(Pick(3) == 0 ? RandInt() : Pick(4));
        o.mCertificateLifeSpanList.push_back(l);
    }
    o.mLog.clear();
    o.mChildOk = Pick(2) != 0;
}
static string RandType()
{
    static const char* kTypes[] = {"TITLEDEF", "COMMUNITY", "titledef", "Community", "", "TITLEDEFX", "COMMUNIT", "GROUPDEFINE"};
    return kTypes[Pick(8)];
}

static string Flat(const SCfg& o)
{
    string s;
    char b[24];
    for (std::list<std::list<int> >::const_iterator i = o.mCertificateLifeSpanList.begin(); i != o.mCertificateLifeSpanList.end(); ++i) {
        snprintf(b, sizeof(b), "%u", (unsigned)i->size());
        s += s.empty() ? "" : ",";
        s += b;
        for (std::list<int>::const_iterator j = i->begin(); j != i->end(); ++j) {
            snprintf(b, sizeof(b), ",%d", *j);
            s += b;
        }
    }
    return s.empty() ? "-" : s;
}

static void Run(int inCases, int inPrint)
{
    for (int c = 0; c < inCases; c++) {
        SCfg base;
        RandCfg(base);
        // instructions
        for (int k = 0; k < 4; k++) {
            string ty = RandType();
            bool already = Pick(2) != 0, community = Pick(2) != 0;
            SCfg a = base, b = base;
            SParser pa, pb;
            bool ra = community ? Orig::CommunityHandleInstruction(a, already, &pa, ty) : Orig::TitleHandleInstruction(a, already, &pa, ty);
            bool rb = TitleCfgMsg<SCfg>::Instruction(b, community, already, &pb, ty);
            CHECK(ra == rb && pa.mLog == pb.mLog && a.mLog == b.mLog);
            if (c < inPrint && k == 0)
                printf("instr %d %d %s %d %d %s\n", community, already, Hex(ty).c_str(), a.mChildOk, ra,
                       Hex(pa.mLog + a.mLog).c_str());
        }
        // PostParse with and without a parent
        SCfg parent;
        RandCfg(parent);
        bool hasParent = Pick(2) != 0;
        SCfg a = base, b = base;
        Orig::PostParse(a, hasParent ? &parent : NULL);
        TitleCfgMsg<SCfg>::PostParse(b, hasParent ? &parent.mName : NULL, ToWide);
        CHECK(a.Dump() == b.Dump());
        CHECK(a.mNameLogin == b.mNameLogin && a.mImagePath == b.mImagePath && a.mHashString == b.mHashString &&
              a.mTrustLevelLifespanMap == b.mTrustLevelLifespanMap && a.mCommunityName == b.mCommunityName);
        if (sFailures && sFailures < 4) printf("post: %s\n  / %s\n", a.Dump().c_str(), b.Dump().c_str());
        if (c < inPrint)
            printf("post %s %s %s %s %s %s %d %s %d %s\n", Hex(base.mName).c_str(), Hex(base.mImagePath).c_str(),
                   Hex(base.mHashString).c_str(), Hex(base.mNameLoginStr).c_str(), Flat(base).c_str(),
                   hasParent ? Hex(parent.mName).c_str() : "*", (int)a.mNameLogin, Hex(a.mImagePath).c_str() ,
                   (int)a.mTrustLevelLifespanMap.size(), Hex(a.mHashString).c_str());
        // the texts, of the generated config and of the parsed one
        for (int k = 0; k < 2; k++) {
            const SCfg& o = k ? a : base;
            bool inc = Pick(2) != 0;
            string ta = Orig::TitleToString(o, inc), tb = TitleCfgMsg<SCfg>::TitleText(o, inc);
            CHECK(ta == tb);
            string ca = Orig::CommunityToString(o), cb = TitleCfgMsg<SCfg>::CommunityText(o, WStringToString(o.mCommunityName));
            CHECK(ca == cb);
            if (ca != cb && sFailures < 4) printf("community:\n%s\n/\n%s\n", ca.c_str(), cb.c_str());
            if (c < inPrint && k == 1) {
                string bits, pairs;
                bool aFlags[] = {o.mForceCDKey, o.mRequireCrc, o.mAllowBetaKey, o.mRequireCDKey, o.mFailToGuest,
                                 o.mAllowGuest, o.mConcurrentUserLogin, o.mConcurrentCDKeyLogin, o.mGetUserData,
                                 o.mGetForgeableUserData, o.mCDKeyForInvite, o.mSaveSessionKey, o.mGetKeyID,
                                 o.mASCIIAccountNames};
                for (int f = 0; f < 14; f++) bits += aFlags[f] ? '1' : '0';
                char b[48];
                for (std::map<long, long>::const_iterator i = o.mTrustLevelLifespanMap.begin(); i != o.mTrustLevelLifespanMap.end(); ++i) {
                    snprintf(b, sizeof(b), "%s%ld,%ld", pairs.empty() ? "" : ",", i->first, i->second);
                    pairs += b;
                }
                string header = o.mCommunityName.empty() ? o.mName : WStringToString(o.mCommunityName);
                printf("text %d %s %s %u %s %s %s %s %s %d %s %s %s %s\n", inc, Hex(o.mName).c_str(), Hex(o.mNetwork).c_str(),
                       (unsigned)o.mNetPort, Hex(o.mImagePath).c_str(), Hex(o.mHashString).c_str(), Hex(o.mKeyPrefix).c_str(),
                       bits.c_str(), Hex(header).c_str(), o.mCommunityID, Hex(o.mNameLoginStr).c_str(),
                       pairs.empty() ? "-" : pairs.c_str(), Hex(ta).c_str(), Hex(ca).c_str());
            }
        }
    }
}

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    if (aPrint) {
        Run(aPrint, aPrint);
        return 0;
    }
    Run(60000, 0);
    printf("won_titlecfg_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
