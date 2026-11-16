// The resource configs' table reader (R6 WON round 39): the 49 table-shaped config handlers as they were (their
// statements copied verbatim before the port: won_cfgtab_orig.h) against the port (ee_won's cfgtab.rs through the
// game's own host statements, include/ee/wonapi/WONCfgTable.inl), both over the same stand-in world
// (won_cfgtab_world.h: reads from a cursor over the value text, traced components, a skin, typed component checks).
//
// Inputs: every `Key: value` line of the lobby's resource files the game ships (the data tree's
// Data/WONLobby Resources, or the directory given as the first argument) on every one of the 49 configs, then
// 5,000 random instructions on every config (a key of the reader's tables or of any table, values built from numbers, booleans, every name of
// guiconf's name tables with and without +/-, components, images, junk, commas and a token that makes a read throw).
// For each, the two traces (every read and its value, every call with its arguments, every field write, the base
// calls, the result or exception message, the reader's position, the percent layout's arrays) must be identical.
// `--print` prints the port's host events (where the reader held the key) of the resource files' instructions and of the
// first 400 random instructions, on every config: rust/crates/ee_won/src/golden_cfgtab.txt, which cargo replays. Exit code: the number of mismatches.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <exception>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/ee_won.h"
#include "won_cfgtab_methods.h"
wchar_t gEmptyWideString[2]; // EEWonNet.cpp's (GetEmptyGUIStringData)

namespace Orig {
#include "won_cfgtab_world.h"
#include "won_cfgtab_orig.h"
}

namespace Shell {
#include "won_cfgtab_world.h"
static bool sPrint;
static std::string sGold;
static std::string Hex(const void* p, size_t n, size_t unit)
{
    std::string s; char b[8];
    for (size_t i = 0; i < n; i++) {
        unsigned v = unit == 1 ? ((const unsigned char*)p)[i] : unit == 2 ? ((const unsigned short*)p)[i] : ((const unsigned*)p)[i];
        snprintf(b, sizeof b, "%04x", v); s += b;
    }
    return s.empty() ? "-" : s;
}
static const EeWonCfgTabHost* H(void* c) { return (const EeWonCfgTabHost*)c; }
static std::string sEv;
static long sRuns[3]; // the reader's answers: not its key, handled, threw
static void Ev(const std::string& e) { if (sPrint) sEv += " " + e; }
static bool LRead(void* c, EeWonCfgRead k) { bool r = H(c)->read(H(c)->ctx, k); Ev("r" + P((long long)(int)k) + "=" + P(r)); return r; }
static bool LComma(void* c) { bool r = H(c)->comma(H(c)->ctx); Ev("c=" + P(r)); return r; }
static bool LEnd(void* c) { bool r = H(c)->end(H(c)->ctx); Ev("e=" + P(r)); return r; }
static const void* LText(void* c, int32_t s, uint16_t* len, uint32_t* unit)
{
    const void* p = H(c)->text(H(c)->ctx, s, len, unit);
    Ev("t" + P((long long)s) + "=" + Hex(p, *len, *unit)); return p;
}
static void LPush(void* c, int32_t v) { H(c)->push(H(c)->ctx, v); Ev("p" + P((long long)v)); }
static bool LApply(void* c, int32_t s) { bool r = H(c)->apply(H(c)->ctx, s); Ev("a" + P((long long)s) + "=" + P(r)); return r; }
static void LFail(void* c, const uint8_t* m, uintptr_t n, int32_t s) { H(c)->fail(H(c)->ctx, m, n, s); Ev("f" + P((long long)s) + ":" + Hex(m, n, 1)); }
static void LClear(void* c) { H(c)->clear(H(c)->ctx); Ev("x"); }
static int32_t LPeek(void* c, int32_t s) { int32_t v = H(c)->peek(H(c)->ctx, s); Ev("k" + P((long long)s) + "=" + P((long long)v)); return v; }
static int32_t LoggedRun(const EeWonCfgTabHost* h, int32_t t, const uint8_t* k, uintptr_t n)
{
    EeWonCfgTabHost w = {(void*)h, LRead, LComma, LEnd, LText, LPush, LApply, LFail, LClear, LPeek};
    sEv.clear();
    int32_t r = ::ee_won_cfg_tab_run(&w, t, k, n);
    sRuns[r + 1]++;
    if (sPrint && r != -1) sGold += P((long long)t) + "\t" + Hex(k, n, 1) + "\t" + P((long long)r) + "\t" + (sEv.empty() ? "-" : sEv.substr(1)) + "\n";
    return r;
}
#define ee_won_cfg_tab_run LoggedRun
#include "ee/wonapi/WONCfgTable.inl"
#undef ee_won_cfg_tab_run
}

static unsigned long long sSeed = 0x9e3779b97f4a7c15ull;
static unsigned Rand(unsigned n) { sSeed = sSeed * 6364136223846793005ull + 1442695040888963407ull; return (unsigned)((sSeed >> 33) % n); }

static int sFailures;
static int sChecks;
typedef std::vector<std::pair<std::string, std::string>> Steps;
static void CompareSeq(int cfg, const Steps& steps)
{
    std::string a = Orig::RunSeq(cfg, steps), b = Shell::RunSeq(cfg, steps);
    sChecks++;
    if (a != b) {
        sFailures++;
        if (sFailures < 8) {
            printf("MISMATCH config %d sequence:", cfg);
            for (const auto& s : steps) printf(" [%s: %s]", s.first.c_str(), s.second.c_str());
            printf("\n--- original\n%s--- port\n%s\n", a.c_str(), b.c_str());
        }
    }
}
static void Compare(int cfg, const std::string& key, const std::string& value)
{
    std::string a = Orig::RunOne(cfg, key, value), b = Shell::RunOne(cfg, key, value);
    sChecks++;
    if (a != b) {
        sFailures++;
        if (sFailures < 8) printf("MISMATCH config %d key '%s' value '%s'\n--- original\n%s--- port\n%s\n", cfg, key.c_str(), value.c_str(), a.c_str(), b.c_str());
    }
}

int main(int argc, char** argv)
{
    if (argc > 2 && !strcmp(argv[1], "--seq")) {
        // one sequence on ContainerConfig, both traces printed: --seq KEY VALUE [KEY VALUE ...]
        Steps steps;
        for (int i = 2; i + 1 < argc; i += 2) steps.push_back({argv[i], argv[i + 1]});
        printf("--- original\n%s--- port\n%s", Orig::RunSeq(46, steps).c_str(), Shell::RunSeq(46, steps).c_str());
        return 0;
    }
    bool print = argc > 1 && !strcmp(argv[1], "--print");
    std::string dir = (argc > 1 && !print) ? argv[1] : "C:/Users/max/Desktop/empire_earth_next/build/open_data/tree/Data/WONLobby Resources";
    int nConfigs = 0;
#define EE_COUNT(T) nConfigs++;
    EE_CFGTAB_CONFIGS(EE_COUNT)
#undef EE_COUNT
    // the keys and names of every guiconf table
    std::vector<std::string> keys = {"BASEKEY", "JUNK", ""}, names, readerKeys;
    for (int t = 0; t < ee_won_gui_cfg_table_count(); t++) {
        for (size_t i = 0;; i++) {
            char buf[128]; int64_t len = 0;
            int kind = ee_won_gui_cfg_table_name(t, i, (uint8_t*)buf, sizeof buf, &len);
            if (len < 0) break;
            (kind == 0 ? keys : names).push_back(std::string(buf, (size_t)len));
            if (kind == 0 && ee_won_cfg_tab_has(t)) readerKeys.push_back(std::string(buf, (size_t)len));
        }
    }
    Shell::sPrint = print;
    {
        // the shipped resource files
        int files = 0, lines = 0;
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
            if (e.path().extension() != ".cfg") continue;
            files++;
            std::ifstream in(e.path(), std::ios::binary);
            std::string line;
            while (std::getline(in, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
                size_t s = line.find_first_not_of(" \t");
                if (s == std::string::npos) continue;
                size_t colon = line.find(':', s);
                if (colon == std::string::npos) continue;
                std::string key = line.substr(s, colon - s);
                if (key.find_first_of(" \t{}") != std::string::npos) continue;
                for (char& ch : key) ch = (char)toupper((unsigned char)ch);
                std::string value = line.substr(colon + 1);
                size_t v = value.find_first_not_of(" \t");
                value = v == std::string::npos ? "" : value.substr(v);
                lines++;
                for (int c = 0; c < nConfigs; c++) {
                    if (print) Shell::RunOne(c, key, value);
                    else Compare(c, key, value);
                }
            }
        }
        if (!print) printf("resource files: %d files, %d instructions on %d configs, %d checks, %d mismatches (the reader: %ld handled, %ld threw, "
               "%ld passed on)\n", files, lines, nConfigs, sChecks, sFailures, Shell::sRuns[1], Shell::sRuns[2], Shell::sRuns[0]);
        Shell::sRuns[0] = Shell::sRuns[1] = Shell::sRuns[2] = 0;
        if (files == 0) { printf("no resource files in %s\n", dir.c_str()); return 1; }
    }
    std::vector<std::string> tokens = {"0", "1", "5", "-3", "12", "300", "true", "false", "TRUE", "maybe", "!x", "", "c1", "c2all",
        "c3", "null", "cont", "img", "imgS", "@res", "xType", "abc def", "a"};
    for (auto& n : names) { tokens.push_back(n); tokens.push_back("+" + n); tokens.push_back("-" + n); }
    int randomRuns = print ? 400 : 5000;
    Shell::sPrint = print;
    for (int r = 0; r < randomRuns; r++) {
        std::string key = Rand(5) ? readerKeys[Rand((unsigned)readerKeys.size())] : keys[Rand((unsigned)keys.size())], value;
        int n = (int)Rand(6);
        for (int i = 0; i < n; i++) {
            if (i) { unsigned sep = Rand(10); value += sep < 7 ? ", " : sep < 9 ? " " : ","; }
            value += tokens[Rand((unsigned)tokens.size())];
        }
        for (int cc = 0; cc < nConfigs; cc++) {
            if (print) Shell::RunOne(cc, key, value);
            else Compare(cc, key, value);
        }
    }
    // ContainerConfig's stateful instructions in sequences on one config (R6 WON round 41): LAYOUT and its chain
    // and repeat (the previous rule's reference, words and offsets), NOOVERLAPLAYOUT, NEWLAYOUTPOINT, ADD
    {
        const char* seqKeys[] = {"LAYOUT", "LAYOUT", "CHAINLAYOUT", "REPEATLAYOUT", "REPEATLAYOUT", "NOOVERLAPLAYOUT", "NEWLAYOUTPOINT", "ADD"};
        std::vector<std::string> words = {"Above", "below", "SAMEWIDTH", "SameHeight", "Right", "Left", "SameLeft", "SameRight",
            "SameTop", "SameBottom", "GrowToRight", "GrowToLeft", "GrowToTop", "GrowToBottom", "SameSize", "Sideways", "Above Left",
            "Right SameTop GrowToBottom", "!w", "above", "BELOW", "LEFT", "Right", "Left Below", "middle"};
        std::vector<std::string> seqTokens = {"c1", "c2all", "c3", "null", "0", "5", "-3", "12", "300", "!x", "name1", "pt", "", "abc def"};
        int containers[] = {46, 17, 21, 24, 7};   // ContainerConfig, ScreenContainerConfig, TabCtrlConfig, DialogConfig, MSEtchedFrameGroupConfig
        int seqRuns = print ? 300 : 6000;
        for (int r = 0; r < seqRuns; r++) {
            Steps steps;
            int n = 1 + (int)Rand(4);
            for (int s = 0; s < n; s++) {
                std::string key = seqKeys[Rand(8)], value;
                const char* comps[] = {"c1", "c2all", "c3", "null"};
                if (Rand(2)) {
                    // a well-formed rule: LAYOUT's target, words and reference, a chain's or repeat's target (and
                    // reference), then 0..5 offsets
                    value = comps[Rand(4)];
                    if (key == "LAYOUT") value += std::string(", ") + words[Rand((unsigned)words.size())] + ", " + comps[Rand(4)];
                    else if (key == "REPEATLAYOUT" && Rand(2)) value += std::string(", ") + comps[Rand(4)];
                    for (int k = (int)Rand(6); k > 0; k--) value += ", " + seqTokens[4 + Rand(5)];
                } else {
                    int parts = (int)Rand(7);
                    for (int i = 0; i < parts; i++) {
                        if (i) { unsigned sep = Rand(12); value += sep < 9 ? ", " : sep < 11 ? " " : ","; }
                        value += (i == 1 && Rand(3)) ? words[Rand((unsigned)words.size())] : seqTokens[Rand((unsigned)seqTokens.size())];
                    }
                }
                steps.push_back({key, value});
            }
            int cfg = containers[Rand(5)];
            if (print) Shell::RunSeq(cfg, steps);
            else CompareSeq(cfg, steps);
        }
        if (!print) printf("sequences: %d checks in all, %d mismatches\n", sChecks, sFailures);
    }
    if (print) { fputs(Shell::sGold.c_str(), stdout); return 0; }
    printf("%d checks, %d mismatches (the random instructions' reader: %ld handled, %ld threw, %ld passed on)\n", sChecks, sFailures,
           Shell::sRuns[1], Shell::sRuns[2], Shell::sRuns[0]);
    return sFailures;
}
