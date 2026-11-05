// The C++ text-loader gate (tools/eedata/eedata.py cppgate builds and runs it): compiles every db table's text
// form with the game's loader code (src/Empire Earth/Database/EEDataText.cpp) and compares the result with the
// expected db file bytes.
//   eedata_gate <text dir> <expected .dat dir>
// Prints one line per table; exits 0 when every table present compiles to identical bytes.
//   eedata_gate --batch <dir> <text dir>
// Compiles every <case>__<table>.<ext> in dir to <case>.out (the bytes) or <case>.err (the message): the parity
// cases, compared with the Python importer by cppgate.py (the other tables, which references name, from text dir).
//   eedata_gate --dump-text <text dir> <out.json>     the content dump of every table compiled from its text
//   eedata_gate --dump-dat <db dir> <out.json>        the content dump of every .dat (the binary load)
// (EEDataDump.cpp: canonical JSON of what the loaders read; the two must be identical.)
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "EEDataDump.h"
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

static EEDataText::Content::Reader TextReader(const std::string& inDir)
{
    return [inDir](const char* inTable, std::string& outText) {
        const char* ext = EEDataText::TextExtension(inTable);
        return ext && ReadFile(inDir + "/" + inTable + ext, outText);
    };
}

static int Dump(bool inText, const char* inDir, const char* inOut)
{
    EEDataText::Content c(TextReader(inDir));
    std::string json = "{";
    for (unsigned long i = 0; i < kEEDTTableCount; i++) {
        const EEDTTable& t = kEEDTTables[i];
        std::vector<unsigned char> data;
        std::string err, raw;
        if (inText) {
            if (!c.Compile(t.mName, data, err)) {
                fprintf(stderr, "%s\n", err.c_str());
                return 1;
            }
        } else {
            if (!ReadFile(std::string(inDir) + "/" + t.mName + ".dat", raw)) {
                fprintf(stderr, "%s.dat: cannot be read\n", t.mName);
                return 1;
            }
            data.assign(raw.begin(), raw.end());
        }
        if (i)
            json += ",\n";
        if (!EEDataText::DumpTable(t.mName, data.empty() ? nullptr : &data[0], data.size(), json, err)) {
            fprintf(stderr, "%s\n", err.c_str());
            return 1;
        }
    }
    json += "}\n";
    FILE* f = fopen(inOut, "wb");
    if (!f)
        return 2;
    fwrite(json.data(), 1, json.size(), f);
    fclose(f);
    printf("%lu tables -> %s (%zu bytes)\n", kEEDTTableCount, inOut, json.size());
    return 0;
}

static int Batch(const char* inDir, const char* inTextDir)
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
        EEDataText::Content::Reader others = TextReader(inTextDir);
        EEDataText::Content c([&](const char* inTable, std::string& outText) {
            if (table == inTable) {
                outText = text;
                return true;
            }
            return others(inTable, outText);
        });
        bool ok = c.Compile(table.c_str(), got, err);
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
    if (argc == 4 && strcmp(argv[1], "--batch") == 0)
        return Batch(argv[2], argv[3]);
    if (argc == 4 && (strcmp(argv[1], "--dump-text") == 0 || strcmp(argv[1], "--dump-dat") == 0))
        return Dump(strcmp(argv[1], "--dump-text") == 0, argv[2], argv[3]);
    if (argc != 3) {
        fprintf(stderr, "usage: eedata_gate <text dir> <expected .dat dir>\n");
        return 2;
    }
    int same = 0, bad = 0, missing = 0;
    double total = 0;
    EEDataText::Content c(TextReader(argv[1]));
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
        bool ok = c.Compile(t.mName, got, err);
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
