// CDKey's storage (the registry value under the key path, on config.yaml through RegKey, and the key file): a script
// run twice over a scratch configuration file ($EE_CONFIG = won_cdkeystore.yaml in the current directory), once over
// the original statements of AuthContext.cpp's LoadFromRegistry, SaveToRegistry, ClearFromRegistry, SaveToFile and
// LoadFromFile (copied here before their port, net round 29) and once over the CDKey methods (ee_won cdkeystore.rs),
// one line per step and the files' bytes after each step; the two runs must print the same lines. The encrypted key
// depends on the machine id, so the lines are compared run against run, not against a golden. `--print` prints the
// lines. The exit code is the number of differing lines.
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
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "platform/PlatFile.h"

using namespace WONAPI;

static const char* kConfig = "won_cdkeystore.yaml";
static const char* kKeyFile = "won_cdkeystore.key";

// ---- the original statements (AuthContext.cpp before net round 29), over the real RegKey and CDKey
static bool OrigLoadFromRegistry(CDKey& k, long inRoot, const char* inPath)
{
    if (k.mProduct.empty()) return false;
    RegKey aKey(inRoot, std::string(inPath), false, true);
    if (!aKey.IsOpen()) return false;
    ByteBufferPtr anEncrypted;
    if (aKey.QueryBinary(k.mProduct, anEncrypted) != 0) return false;
    return k.InitEncrypted(anEncrypted->data(), anEncrypted->length());
}
static bool OrigSaveToRegistry(CDKey& k, long inRoot, const char* inPath)
{
    if (!k.IsValid()) return false;
    RegKey aKey(inRoot, inPath, true, true);
    if (!aKey.IsOpen()) return false;
    ByteBufferPtr anEncrypted = k.GetEncrypted();
    if (anEncrypted.get() == NULL) return false;
    return aKey.SetValue(k.mProduct, anEncrypted->data(), anEncrypted->length());
}
static void OrigClearFromRegistry(CDKey& k, long inRoot, const char* inPath)
{
    if (k.mProduct.empty()) return;
    RegKey aKey(inRoot, inPath, false, true);
    aKey.DeleteValue(k.mProduct);
}
static bool OrigSaveToFile(CDKey& k, const std::string& inFileName)
{
    if (!k.IsValid()) return false;
    FILE* aFile = PlatFOpen(inFileName.c_str(), "wb");
    if (aFile == NULL) return false;
    ByteBufferPtr anEncrypted = k.GetEncrypted();
    if (anEncrypted.get() == NULL) return false;
    if (fwrite(anEncrypted->data(), 1, anEncrypted->length(), aFile) != anEncrypted->length()) return false;
    fclose(aFile);
    return true;
}
static bool OrigLoadFromFile(CDKey& k, const std::string& inFileName)
{
    FILE* aFile = PlatFOpen(inFileName.c_str(), "rb");
    if (aFile == NULL) return false;
    char aBuf[2048];
    int aReadLen = (int)fread(aBuf, 1, 2048, aFile);
    fclose(aFile);
    return k.InitEncrypted(aBuf, aReadLen);
}

// ---- the two sides
struct Side {
    bool mOrig;
    bool Load(CDKey& k, long r, const char* p) { return mOrig ? OrigLoadFromRegistry(k, r, p) : k.LoadFromRegistry(r, p); }
    bool Save(CDKey& k, long r, const char* p) { return mOrig ? OrigSaveToRegistry(k, r, p) : k.SaveToRegistry(r, p); }
    void Clear(CDKey& k, long r, const char* p) { if (mOrig) OrigClearFromRegistry(k, r, p); else k.ClearFromRegistry(r, p); }
    bool SaveFile(CDKey& k, const std::string& f) { return mOrig ? OrigSaveToFile(k, f) : k.SaveToFile(f); }
    bool LoadFile(CDKey& k, const std::string& f) { return mOrig ? OrigLoadFromFile(k, f) : k.LoadFromFile(f); }
};

static std::vector<std::string>* sOut;
static void Line(const std::string& inText) { sOut->push_back(inText); }
static std::string B(bool b) { return b ? "1" : "0"; }
static std::string KeyText(const CDKey& k) { return std::string(" valid=") + B(k.IsValid()) + " key=" + k.GetString(); }

static std::string FileBytes(const char* inPath)
{
    FILE* f = fopen(inPath, "rb");
    if (f == NULL) return "(none)";
    std::string s;
    char aBuf[4096];
    size_t n;
    while ((n = fread(aBuf, 1, sizeof(aBuf), f)) > 0) s.append(aBuf, n);
    fclose(f);
    return s;
}
static void Files(const char* inStep)
{
    std::string c = FileBytes(kConfig), k = FileBytes(kKeyFile);
    // the config file's lines (the key's value is machine-bound but the same in both runs)
    Line(std::string(inStep) + " config " + c);
    char aBuf[64];
    snprintf(aBuf, sizeof(aBuf), " keyfile %u bytes ", (unsigned)k.size());
    std::string aHex;
    for (size_t i = 0; i < k.size() && i < 64; i++) {
        char h[4];
        snprintf(h, sizeof(h), "%02x", (unsigned char)k[i]);
        aHex += h;
    }
    Line(std::string(inStep) + aBuf + aHex);
}

// a valid raw key (the check byte searched), from a seed
static void MakeValid(CDKey& k, unsigned inSeed)
{
    unsigned char raw[8];
    for (unsigned i = 0; i < 4096; i++) {
        unsigned x = inSeed * 2654435761u + i * 40503u;
        for (int j = 0; j < 8; j++) { x = x * 1103515245u + 12345u; raw[j] = (unsigned char)(x >> 16); }
        if (k.InitRaw(raw)) return;
    }
}

static void Run(Side s)
{
    remove(kConfig);
    remove(kKeyFile);
    const char* p = "SOFTWARE\\Sierra\\CDKeys";
    for (int product = 0; product < 2; product++) {
        std::string aProduct = product == 0 ? "EmpireEarthAOC" : "Other Product";
        CDKey none(aProduct);
        Line("load missing " + B(s.Load(none, 2, p)) + KeyText(none));
        Line("save invalid " + B(s.Save(none, 2, p)));
        Files("after invalid");
        CDKey k(aProduct);
        MakeValid(k, 7 + product);
        Line("made" + KeyText(k));
        Line("save " + B(s.Save(k, 2, p)));
        Files("after save");
        CDKey back(aProduct);
        Line("load " + B(s.Load(back, 2, p)) + KeyText(back));
        CDKey other(aProduct);
        Line("load other root " + B(s.Load(other, 1, p)) + KeyText(other));
        Line("load other path " + B(s.Load(other, 2, "SOFTWARE\\Elsewhere")) + KeyText(other));
        {
            RegKey aKey(2, p, true, true);
            aKey.SetValue(aProduct, std::string("zz"));
        }
        CDKey bad(aProduct);
        Line("load bad hex " + B(s.Load(bad, 2, p)) + KeyText(bad));
        {
            RegKey aKey(2, p, true, true);
            aKey.SetValue(aProduct, std::string(""));
        }
        Line("load empty " + B(s.Load(bad, 2, p)) + KeyText(bad));
        {
            RegKey aKey(2, p, true, true);
            aKey.SetValue(aProduct, "\x01\x02\x03\x04\x05\x06\x07\x08", 8);
        }
        Line("load junk " + B(s.Load(bad, 2, p)) + KeyText(bad));
        Line("save again " + B(s.Save(k, 2, p)));
        s.Clear(k, 2, p);
        Files("after clear");
        Line("load cleared " + B(s.Load(back, 2, p)) + KeyText(back));
        CDKey empty;
        s.Clear(empty, 2, p);
        Line("load no product " + B(s.Load(empty, 2, p)));
        // files
        Line("save file invalid " + B(s.SaveFile(none, kKeyFile)));
        Files("after file invalid");
        Line("save file " + B(s.SaveFile(k, kKeyFile)));
        Files("after file");
        CDKey fromFile(aProduct);
        Line("load file " + B(s.LoadFile(fromFile, kKeyFile)) + KeyText(fromFile));
        Line("load file missing " + B(s.LoadFile(fromFile, "won_cdkeystore.none")) + KeyText(fromFile));
        {
            FILE* f = fopen(kKeyFile, "wb");
            std::string aBig(3000, '\x5a');
            fwrite(aBig.data(), 1, aBig.size(), f);
            fclose(f);
        }
        Line("load file big " + B(s.LoadFile(fromFile, kKeyFile)) + KeyText(fromFile));
        {
            FILE* f = fopen(kKeyFile, "wb");
            fclose(f);
        }
        Line("load file empty " + B(s.LoadFile(fromFile, kKeyFile)) + KeyText(fromFile));
        Line("save file bad path " + B(s.SaveFile(k, "no_such_dir/won_cdkeystore.key")));
        remove(kKeyFile);
    }
    Files("end");
    remove(kConfig);
}

int main(int argc, char** argv)
{
#ifdef _WIN32
    _putenv("EE_CONFIG=won_cdkeystore.yaml");
#else
    setenv("EE_CONFIG", "won_cdkeystore.yaml", 1);
#endif
    std::vector<std::string> anOrig, aPort;
    sOut = &anOrig;
    Run(Side{true});
    sOut = &aPort;
    Run(Side{false});
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int aFailures = 0;
    size_t n = anOrig.size() > aPort.size() ? anOrig.size() : aPort.size();
    for (size_t i = 0; i < n; i++) {
        const char* o = i < anOrig.size() ? anOrig[i].c_str() : "(none)";
        const char* q = i < aPort.size() ? aPort[i].c_str() : "(none)";
        if (aPrint) printf("%s\n", o);
        if (strcmp(o, q) != 0 && aFailures++ < 20) printf("FAIL line %d\n  orig %s\n  port %s\n", (int)i + 1, o, q);
    }
    printf("won_cdkeystore_test: %d lines, %d failures\n", (int)anOrig.size(), aFailures);
    return aFailures;
}
