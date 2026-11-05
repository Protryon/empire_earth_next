// The C++ text-loader gate (tools/eedata/eedata.py cppgate builds and runs it): compiles every db table's text
// form with the game's loader code (src/Empire Earth/Database/EEDataText.cpp) and compares the result with the
// expected db file bytes.
//   eedata_gate <text dir> <expected .dat dir>
// Prints one line per table; exits 0 when every table present compiles to identical bytes.
//   eedata_gate --batch <dir>
// Compiles every <case>__<table>.<ext> in dir to <case>.out (the bytes) or <case>.err (the message): the parity
// cases, compared with the Python importer by cppgate.py.
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "EEDataText.h"
#include "EEDataTextSpecs.h"

static bool ReadFile(const std::string& inPath, std::string& outData)
{
    FILE* f = fopen(inPath.c_str(), "rb");
    if (!f)
        return false;
    char buf[65536];
    size_t n;
    outData.clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        outData.append(buf, n);
    fclose(f);
    return true;
}

static int Batch(const char* inDir)
{
    int n = 0;
    for (const std::filesystem::directory_entry& e : std::filesystem::directory_iterator(inDir)) {
        std::string file = e.path().filename().string();
        size_t sep = file.find("__"), dot = file.rfind('.');
        if (sep == std::string::npos || dot == std::string::npos || dot < sep)
            continue;
        std::string name = file.substr(0, sep), table = file.substr(sep + 2, dot - sep - 2), text;
        if (!ReadFile(e.path().string(), text))
            return 2;
        std::vector<unsigned char> got;
        std::string err;
        bool ok = EEDataText::Compile(table.c_str(), text.data(), (unsigned long)text.size(), got, err);
        std::string out = (std::filesystem::path(inDir) / (name + (ok ? ".out" : ".err"))).string();
        FILE* f = fopen(out.c_str(), "wb");
        if (!f)
            return 2;
        if (ok && !got.empty())
            fwrite(&got[0], 1, got.size(), f);
        else if (!ok)
            fwrite(err.data(), 1, err.size(), f);
        fclose(f);
        n++;
    }
    printf("%d parity cases compiled\n", n);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 3 && strcmp(argv[1], "--batch") == 0)
        return Batch(argv[2]);
    if (argc != 3) {
        fprintf(stderr, "usage: eedata_gate <text dir> <expected .dat dir>\n");
        return 2;
    }
    int same = 0, bad = 0, missing = 0;
    double total = 0;
    for (unsigned long i = 0; i < kEEDTTableCount; i++) {
        const EEDTTable& t = kEEDTTables[i];
        std::string text, want;
        std::string tp = std::string(argv[1]) + "/" + t.mName + EEDataText::TextExtension(t.mName);
        if (!ReadFile(tp, text) || !ReadFile(std::string(argv[2]) + "/" + t.mName + ".dat", want)) {
            printf("  %-24s MISSING\n", t.mName);
            missing++;
            continue;
        }
        std::vector<unsigned char> got;
        std::string err;
        auto t0 = std::chrono::steady_clock::now();
        bool ok = EEDataText::Compile(t.mName, text.data(), (unsigned long)text.size(), got, err);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        total += ms;
        if (!ok) {
            printf("  %-24s ERROR %s\n", t.mName, err.c_str());
            bad++;
        } else if (got.size() != want.size() || (!got.empty() && memcmp(&got[0], want.data(), got.size()))) {
            size_t k = 0;
            while (k < got.size() && k < want.size() && got[k] == (unsigned char)want[k])
                k++;
            printf("  %-24s DIFF %zu bytes for %zu, first difference at %zu\n", t.mName, got.size(), want.size(), k);
            bad++;
        } else {
            printf("  %-24s %9zu B identical (%.0f ms)\n", t.mName, got.size(), ms);
            same++;
        }
    }
    printf("%d/%lu tables compile to identical bytes (%d differ or fail, %d missing; %.0f ms in all)\n", same,
           kEEDTTableCount, bad, missing, total);
    return bad || missing ? 1 : 0;
}
