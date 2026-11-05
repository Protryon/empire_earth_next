// The C++ open-format gate (tools/eemodel/eemodel.py cppgate builds and runs it): converts every case of a folder
// with the game's loader code (src/Low-Level Engine/Graphics Engine/GEOpenFormats.cpp) and writes the result for
// cppgate.py to compare with the Python converter's.
//   eemodel_gate <dir>
// Each line of <dir>/cases.txt is "model <name>.gltf|.glb" or "texture <name>.png" (further frames: <name>@1.png,
// ...); the result goes to <dir>/<name>.out (the .cem / .sst bytes) or <dir>/<name>.err (the message).
#include <chrono>
#include <float.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "GEOpenFormats.h"

static bool ReadFile(const std::string& inPath, std::vector<unsigned char>& outData)
{
    FILE* f = fopen(inPath.c_str(), "rb");
    if (!f)
        return false;
    unsigned char buf[65536];
    size_t n;
    outData.clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        outData.insert(outData.end(), buf, buf + n);
    fclose(f);
    return true;
}

static void WriteFile(const std::string& inPath, const void* inData, size_t inSize)
{
    FILE* f = fopen(inPath.c_str(), "wb");
    if (f) {
        if (inSize)
            fwrite(inData, 1, inSize, f);
        fclose(f);
    }
}

static bool Resolve(void* inUser, const std::string& inUri, std::vector<unsigned char>& outData)
{
    return ReadFile(*static_cast<std::string*>(inUser) + "/" + inUri, outData);
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: eemodel_gate <dir>\n");
        return 2;
    }
#if defined(_M_IX86)
    unsigned int theControl;
    _controlfp_s(&theControl, 0, 0);
    printf("x87 precision control at start: %s\n", (theControl & _MCW_PC) == _PC_53 ? "53" : (theControl & _MCW_PC) == _PC_24 ? "24" : "64");
#endif
    std::string theDir = argv[1];
    FILE* theList = fopen((theDir + "/cases.txt").c_str(), "rb");
    if (!theList) {
        fprintf(stderr, "no cases.txt\n");
        return 2;
    }
    char theLine[1024];
    int theCount = 0;
    double theModelSeconds = 0, theTextureSeconds = 0;
    while (fgets(theLine, sizeof(theLine), theList)) {
        std::string s = theLine;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        size_t sp = s.find(' ');
        if (sp == std::string::npos)
            continue;
        std::string theKind = s.substr(0, sp), theFile = s.substr(sp + 1);
        std::string theStem = theFile.substr(0, theFile.rfind('.'));
        std::vector<unsigned char> theOut;
        std::string theError;
        bool ok;
        std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
        if (theKind == "model") {
            std::vector<unsigned char> theData;
            ok = ReadFile(theDir + "/" + theFile, theData);
            if (!ok)
                theError = "cannot read";
            else
                ok = GEOpenFormats::GltfToCem(theData.empty() ? NULL : &theData[0], (unsigned long)theData.size(),
                                              Resolve, &theDir, theOut, theError);
            theModelSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        } else {
            std::vector<std::vector<unsigned char> > theFrames(1);
            ok = ReadFile(theDir + "/" + theFile, theFrames[0]);
            for (int i = 1; ok; i++) {
                std::vector<unsigned char> theFrame;
                if (!ReadFile(theDir + "/" + theStem + "@" + std::to_string(i) + ".png", theFrame))
                    break;
                theFrames.push_back(theFrame);
            }
            if (!ok)
                theError = "cannot read";
            else
                ok = GEOpenFormats::PngToSst(theFrames, theOut, theError);
            theTextureSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        }
        if (ok)
            WriteFile(theDir + "/" + theStem + ".out", theOut.empty() ? NULL : &theOut[0], theOut.size());
        else
            WriteFile(theDir + "/" + theStem + ".err", theError.data(), theError.size());
        theCount++;
    }
    fclose(theList);
    printf("%d cases converted (models %.1f s, textures %.1f s)\n", theCount, theModelSeconds, theTextureSeconds);
    return 0;
}
