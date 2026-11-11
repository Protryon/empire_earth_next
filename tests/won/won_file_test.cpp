// WON's FileReader and FileWriter (the lobby's persistent data, the config files, the image decoders' reader): a
// script of writes and reads over real files and memory buffers, one line per step, compared with
// rust/crates/ee_won/src/golden_file.txt (recorded over the C++ classes before their port; the Rust test
// `file_golden` replays the same script). `--print` prints the lines instead. The exit code is the number of failures.
// The script's files are A and B in the current directory (won_file_A.bin, won_file_B.bin); a missing file is N.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyPersistence.h"

using namespace WONAPI;

// The script: one step per line, "<object>.<op> [args]". w = the writer, r = the reader. Shared with file_golden.rs.
static const char* const kScript[] = {
    "r.isopen", "r.pos", "r.length", "r.eof", "r.tryb", "r.byte", "r.skip 1", "r.maxbytes 4", "r.setpos 3",
    "r.rewind", "w.pos", "w.byte 1", "w.setpos 5",
    "w.open -", "w.open N/x.bin 0", "w.open A 0", "w.pos", "w.byte 0x41", "w.short 0x4243", "w.long -2",
    "w.length 7 1", "w.length 0x1234 2", "w.length 0x89abcdef 4", "w.length 9 3", "w.string hello 1",
    "w.string ab 2", "w.string c 4", "w.string - 1", "w.rawstring xyz", "w.wstring 263a.41.e9 2",
    "w.wstring 7a 1", "w.rawwstring 31.32", "w.pos", "w.setpos 1", "w.byte 0x5a", "w.setpos 0x7fffffff",
    "w.pos", "w.setpos 2", "w.close", "w.pos", "w.close", "dump A",
    "r.open -", "r.open N", "r.isopen", "r.open A", "r.isopen", "r.pos", "r.eof", "r.byte", "r.byte", "r.short",
    "r.pos", "r.long", "r.rlen 1", "r.rlen 2", "r.rlen 4", "r.rlen 3", "r.pos",
    "r.setpos 14", "r.string 1", "r.string 2", "r.string 4", "r.string 1", "r.rawstring 3", "r.wstring 2",
    "r.wstring 1", "r.rawwstring 2", "r.pos", "r.setpos 15", "r.string 1", "r.string 2",
    "r.string 4", "r.string 1", "r.rawstring 3", "r.wstring 2", "r.wstring 1", "r.rawwstring 2", "r.pos",
    "r.length", "r.avail", "r.eof", "r.tryb", "r.eof", "r.byte", "r.setpos 1000", "r.pos", "r.eof", "r.tryb",
    "r.skip 5", "r.pos", "r.setpos 2", "r.maxbytes 3", "r.maxbytes 0", "r.pos", "r.rewind", "r.pos",
    "r.maxbytes 100", "r.pos", "r.eof", "r.rewind", "r.skip 4", "r.intomem", "r.pos", "r.length", "r.short",
    "r.long", "r.eof", "r.setpos 0", "r.byte", "r.setpos 48", "r.setpos 47", "r.byte", "r.eof", "r.tryb",
    "r.byte", "r.skip 1", "r.rewind", "r.skip 46", "r.skip 2", "r.pos", "r.skip 1", "r.eof", "r.maxbytes 9",
    "r.rewind", "r.maxbytes 2", "r.pos", "r.intomem", "r.pos", "r.close", "r.isopen", "r.pos", "r.length",
    "r.eof", "r.tryb",
    "r.open A", "r.length", "r.pos", "r.byte", "r.length", "r.pos", "r.setpos 45", "r.intomem", "r.pos",
    "r.maxbytes 8", "r.pos", "r.intomem", "r.close",
    "r.open A", "r.setpos 48", "r.intomem", "r.isopen", "r.pos", "r.length", "r.eof", "r.close",
    "r.open A", "r.setpos 49", "r.intomem", "r.isopen", "r.pos", "r.length", "r.close",
    "w.open B 0", "w.rawstring a", "w.byte 13", "w.byte 10", "w.rawstring b", "w.byte 13", "w.rawstring c",
    "w.byte 13", "w.byte 10", "w.close", "dump B",
    "r.opentext -", "r.opentext N", "r.opentext B", "r.length", "r.tryb", "r.tryb", "r.tryb", "r.pos", "r.tryb",
    "r.tryb", "r.tryb", "r.eof", "r.tryb", "r.eof", "r.rewind", "r.intomem", "r.length", "r.maxbytes 10",
    "r.close", "r.open B", "r.length", "r.maxbytes 10", "r.close",
    "w.open B 1", "w.close", "dump B", "r.open B", "r.length", "r.eof", "r.tryb", "r.eof", "r.intomem",
    "r.isopen", "r.length", "r.close",
    "r.mem 0102030405", "r.isopen", "r.length", "r.pos", "r.byte", "r.short", "r.avail", "r.skip 2", "r.eof",
    "r.tryb", "r.skip 0", "r.skip 1", "r.setpos 5", "r.setpos 4", "r.byte", "r.eof", "r.rewind", "r.rlen 4",
    "r.rewind", "r.maxbytes 9", "r.pos", "r.intomem", "r.rewind", "r.rawstring 6", "r.pos", "r.rewind",
    "r.rawwstring 3", "r.pos", "r.rewind", "r.string 1", "r.pos", "r.close", "r.isopen",
    "r.mem -", "r.isopen", "r.length", "r.eof", "r.tryb", "r.setpos 0", "r.byte", "r.maxbytes 1", "r.skip 0",
    "r.close",
};

static std::vector<std::string> sLines;

static std::string Hex(const void* inData, size_t inLength)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inLength; i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<const unsigned char*>(inData)[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

static std::string Units(const std::wstring& inText)
{
    std::string anOut;
    char aBuffer[12];
    for (size_t i = 0; i < inText.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%s%04x", i ? "." : "", (unsigned)inText[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

static std::string Path(const std::string& inName)
{
    if (inName == "-")
        return "";
    if (inName == "N")
        return "won_file_missing_dir/none.bin";
    if (inName.compare(0, 2, "N/") == 0)
        return "won_file_missing_dir/" + inName.substr(2);
    return "won_file_" + inName + ".bin";
}

static std::string Text(const std::string& inArg) { return inArg == "-" ? std::string() : inArg; }

static std::wstring WideUnits(const std::string& inArg)
{
    std::wstring anOut;
    const char* p = inArg.c_str();
    while (*p) {
        char* anEnd;
        anOut += (wchar_t)strtoul(p, &anEnd, 16);
        p = *anEnd == '.' ? anEnd + 1 : anEnd;
    }
    return anOut;
}

static long Number(const std::string& inArg) { return (long)strtoll(inArg.c_str(), NULL, 0); }

static std::vector<unsigned char> Bytes(const std::string& inArg)
{
    std::vector<unsigned char> anOut;
    for (size_t i = 0; inArg != "-" && i + 1 < inArg.size(); i += 2)
        anOut.push_back((unsigned char)strtoul(inArg.substr(i, 2).c_str(), NULL, 16));
    return anOut;
}

static FileReader sReader;
static FileWriter sWriter;
static std::vector<unsigned char> sMemory;

static std::string Step(const std::string& inOp, const std::string& inArg, const std::string& inArg2)
{
    char aBuffer[64];
    if (inOp == "dump") {
        FILE* aFile = fopen(Path(inArg).c_str(), "rb");
        if (aFile == NULL)
            return "none";
        std::string aData;
        int c;
        while ((c = fgetc(aFile)) != EOF)
            aData += (char)c;
        fclose(aFile);
        return Hex(aData.data(), aData.size());
    }
    if (inOp == "w.open") return sWriter.Open(Path(inArg).c_str(), Number(inArg2) != 0) ? "true" : "false";
    if (inOp == "w.close") { sWriter.Close(); return "ok"; }
    if (inOp == "w.pos") { snprintf(aBuffer, sizeof(aBuffer), "%lu", sWriter.pos()); return aBuffer; }
    if (inOp == "w.setpos") { sWriter.SetPos((unsigned long)Number(inArg)); return "ok"; }
    if (inOp == "w.byte") { sWriter.WriteByte((unsigned char)Number(inArg)); return "ok"; }
    if (inOp == "w.short") { sWriter.WriteShort((unsigned short)Number(inArg)); return "ok"; }
    if (inOp == "w.long") { sWriter.WriteLong((int32_t)Number(inArg)); return "ok"; }
    if (inOp == "w.length") {
        sWriter.WriteLength((unsigned long)Number(inArg), (unsigned char)Number(inArg2));
        return "ok";
    }
    if (inOp == "w.string") { sWriter.WriteString(Text(inArg), (unsigned char)Number(inArg2)); return "ok"; }
    if (inOp == "w.rawstring") { sWriter.WriteRawString(Text(inArg)); return "ok"; }
    if (inOp == "w.wstring") { sWriter.WriteWString(WideUnits(inArg), (unsigned char)Number(inArg2)); return "ok"; }
    if (inOp == "w.rawwstring") { sWriter.WriteRawWString(WideUnits(inArg)); return "ok"; }

    if (inOp == "r.open") return sReader.Open(Path(inArg).c_str()) ? "true" : "false";
    if (inOp == "r.opentext") return sReader.OpenText(Path(inArg).c_str()) ? "true" : "false";
    if (inOp == "r.mem") {
        sMemory = Bytes(inArg);
        sReader.Open(sMemory.empty() ? (const void*)"" : (const void*)&sMemory[0], (unsigned long)sMemory.size());
        return "ok";
    }
    if (inOp == "r.close") { sReader.Close(); return "ok"; }
    if (inOp == "r.isopen") return sReader.IsOpen() ? "true" : "false";
    if (inOp == "r.pos") { snprintf(aBuffer, sizeof(aBuffer), "%lu", sReader.pos()); return aBuffer; }
    if (inOp == "r.length") { snprintf(aBuffer, sizeof(aBuffer), "%lu", sReader.length()); return aBuffer; }
    if (inOp == "r.avail") { snprintf(aBuffer, sizeof(aBuffer), "%lu", sReader.Available()); return aBuffer; }
    if (inOp == "r.eof") return sReader.EndOfFile() ? "true" : "false";
    if (inOp == "r.tryb") { snprintf(aBuffer, sizeof(aBuffer), "%d", sReader.TryReadByte()); return aBuffer; }
    if (inOp == "r.setpos") { sReader.SetPos((unsigned long)Number(inArg)); return "ok"; }
    if (inOp == "r.skip") { sReader.SkipBytes((unsigned long)Number(inArg)); return "ok"; }
    if (inOp == "r.rewind") { sReader.Rewind(); return "ok"; }
    if (inOp == "r.intomem") { sReader.ReadIntoMemory(); return "ok"; }
    if (inOp == "r.maxbytes") {
        char aData[256];
        int aCount = sReader.ReadMaxBytes(aData, (int)Number(inArg));
        snprintf(aBuffer, sizeof(aBuffer), "%d ", aCount);
        return aBuffer + Hex(aData, aCount > 0 ? aCount : 0);
    }
    if (inOp == "r.byte") { snprintf(aBuffer, sizeof(aBuffer), "%u", sReader.ReadByte()); return aBuffer; }
    if (inOp == "r.short") { snprintf(aBuffer, sizeof(aBuffer), "%u", sReader.ReadShort()); return aBuffer; }
    if (inOp == "r.long") { snprintf(aBuffer, sizeof(aBuffer), "%d", (int)sReader.ReadLong()); return aBuffer; }
    if (inOp == "r.rlen") {
        snprintf(aBuffer, sizeof(aBuffer), "%d", sReader.ReadLength((int)Number(inArg)));
        return aBuffer;
    }
    std::string aString = "unset";
    std::wstring aWide = L"unset";
    try {
        if (inOp == "r.rawstring") { sReader.ReadRawString(aString, (int)Number(inArg)); return Hex(aString.data(), aString.size()); }
        if (inOp == "r.string") { sReader.ReadString(aString, (unsigned char)Number(inArg)); return Hex(aString.data(), aString.size()); }
        if (inOp == "r.rawwstring") { sReader.ReadRawWString(aWide, (int)Number(inArg)); return Units(aWide); }
        if (inOp == "r.wstring") { sReader.ReadWString(aWide, (unsigned char)Number(inArg)); return Units(aWide); }
    } catch (FileReaderException& anException) {
        // the partly read string stays in the output
        return "throw " + anException.mDescription + " / " + Hex(aString.data(), aString.size()) + " / " + Units(aWide);
    }
    return "unknown op";
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_file.txt";
}

int main(int argc, char** argv)
{
    remove(Path("A").c_str());
    remove(Path("B").c_str());
    for (size_t i = 0; i < sizeof(kScript) / sizeof(kScript[0]); i++) {
        std::string aStep = kScript[i], anOp, anArg, anArg2;
        size_t aSpace = aStep.find(' ');
        anOp = aStep.substr(0, aSpace);
        if (aSpace != std::string::npos) {
            std::string aRest = aStep.substr(aSpace + 1);
            size_t aSecond = aRest.find(' ');
            anArg = aRest.substr(0, aSecond);
            if (aSecond != std::string::npos)
                anArg2 = aRest.substr(aSecond + 1);
        }
        std::string aResult;
        try {
            aResult = Step(anOp, anArg, anArg2);
        } catch (FileReaderException& anException) {
            aResult = "throw " + anException.mDescription;
        } catch (FileWriterException& anException) {
            aResult = "throw " + anException.mDescription;
        }
        sLines.push_back(aStep + " -> " + aResult);
    }
    sReader.Close();
    sWriter.Close();
    remove(Path("A").c_str());
    remove(Path("B").c_str());
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
    char aBuffer[4096];
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
    printf("won_file_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
