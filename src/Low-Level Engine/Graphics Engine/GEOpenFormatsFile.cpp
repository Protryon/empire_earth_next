// GEFindModelFile / GEFindTextureFile: the game's seam for the open model and texture formats (new unit, not in the
// original game; tools/eemodel writes the files). GEModelManager and GETextureManager look their files up through
// these instead of FSPath::FindFile: when a model's .glb/.gltf or a texture's .png is on the data path it is
// converted (GEOpenFormats.cpp) into the bytes of the .cem/.sst and served as an FSMemoryFile, so the loaders read
// the same stream as from the binary file; otherwise the binary file is found as before. An open-format file anywhere
// on the path wins over the binary one (the path's own order applies among the open-format files).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include "GEOpenFormats.h"

// Reads a whole file -> its bytes; false when it cannot be read
static bool GEReadWholeFile(FSFileSpec& inFile, std::vector<unsigned char>& outData)
{
    if (inFile.Open(kFileAccessRead) != kErrorOK)
        return false;
    unsigned long theLength = 0;
    bool theOK = inFile.GetLength(theLength) == kErrorOK;
    if (theOK) {
        outData.resize(theLength);
        unsigned long theRead = 0;
        theOK = theLength == 0 || (inFile.Read(&outData[0], theLength, theRead) == kErrorOK && theRead == theLength);
    }
    inFile.Close();
    return theOK;
}

// Finds a file on the path and reads it; false when it is not there
static bool GEReadFromPath(FSPath* inPath, const std::string& inName, std::vector<unsigned char>& outData)
{
    FSFileSpec* theFile = NULL;
    if (inPath->FindFile(UString(inName.c_str()), theFile) != kErrorOK || theFile == NULL)
        return false;
    bool theOK = GEReadWholeFile(*theFile, outData);
    delete theFile;
    return theOK;
}

// "models\x.cem" -> "models\", "x"
static void GESplitName(const UString& inName, std::string& outDirectory, std::string& outStem)
{
    std::string theName = (const char*)inName;
    size_t theSlash = theName.find_last_of("\\/");
    outDirectory = theSlash == std::string::npos ? std::string() : theName.substr(0, theSlash + 1);
    outStem = theName.substr(outDirectory.size());
    size_t theDot = outStem.rfind('.');
    if (theDot != std::string::npos)
        outStem.resize(theDot);
}

// The converted bytes as an in-memory file named like the binary file the loader asked for
static FSFileSpec* GEMemoryFile(const std::vector<unsigned char>& inData, const std::string& inFileName)
{
    unsigned char* theBuffer = new unsigned char[inData.empty() ? 1 : inData.size()];
    if (!inData.empty())
        memcpy(theBuffer, &inData[0], inData.size());
    FSMemoryFile* theFile = new FSMemoryFile(theBuffer, (unsigned long)inData.size(), 1000, true);   // owns theBuffer
    theFile->SetFile(UString(inFileName.c_str()));
    return theFile;
}

static long GEOpenFormatFailed(const std::string& inMessage, FSFileSpec*& outFile)
{
    // a broken open-format file stops the game with the place of the problem
    ESVerify(__FILE__, __LINE__, inMessage.c_str());
    outFile = NULL;
    return kErrorFailed;
}

struct GEBufferResolver {
    FSPath* mPath;
    std::string mDirectory;
};

static bool GEResolveBuffer(void* inUser, const std::string& inUri, std::vector<unsigned char>& outData)
{
    GEBufferResolver* theResolver = static_cast<GEBufferResolver*>(inUser);
    std::string theName = theResolver->mDirectory + inUri;
    for (size_t i = 0; i < theName.size(); i++)
        if (theName[i] == '/')
            theName[i] = '\\';
    return GEReadFromPath(theResolver->mPath, theName, outData);
}

long GEFindModelFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile)
{
    std::string theDirectory, theStem;
    GESplitName(inName, theDirectory, theStem);
    static const char* const kExtensions[] = {".glb", ".gltf"};
    for (int i = 0; i < 2; i++) {
        std::string theOpenName = theDirectory + theStem + kExtensions[i];
        std::vector<unsigned char> theSource;
        if (!GEReadFromPath(inPath, theOpenName, theSource))
            continue;
        GEBufferResolver theResolver = {inPath, theDirectory};
        std::vector<unsigned char> theModel;
        std::string theError;
        if (!GEOpenFormats::GltfToCem(theSource.empty() ? NULL : &theSource[0], (unsigned long)theSource.size(),
                                      GEResolveBuffer, &theResolver, theModel, theError))
            return GEOpenFormatFailed(theOpenName + ": " + theError, outFile);
        outFile = GEMemoryFile(theModel, theStem + ".cem");
        return kErrorOK;
    }
    return inPath->FindFile(inName, outFile);
}

long GEFindTextureFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile, bool& outIsOpenFormat)
{
    outIsOpenFormat = false;
    std::string theDirectory, theStem;
    GESplitName(inName, theDirectory, theStem);
    std::vector<std::vector<unsigned char> > theFrames(1);
    if (GEReadFromPath(inPath, theDirectory + theStem + ".png", theFrames[0])) {
        // further frames: <name>@1.png, <name>@2.png, ... up to the first missing one
        for (unsigned long i = 1;; i++) {
            char theSuffix[24];
            sprintf(theSuffix, "@%lu.png", i);
            std::vector<unsigned char> theFrame;
            if (!GEReadFromPath(inPath, theDirectory + theStem + theSuffix, theFrame))
                break;
            theFrames.push_back(theFrame);
        }
        std::vector<unsigned char> theTexture;
        std::string theError;
        if (!GEOpenFormats::PngToSst(theFrames, theTexture, theError))
            return GEOpenFormatFailed(theDirectory + theStem + ".png: " + theError, outFile);
        outFile = GEMemoryFile(theTexture, theStem + ".sst");
        outIsOpenFormat = true;
        return kErrorOK;
    }
    return inPath->FindFile(inName, outFile);
}

long GEFindTargaFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile)
{
    std::string theDirectory, theStem;
    GESplitName(inName, theDirectory, theStem);
    std::vector<std::vector<unsigned char> > theFrames(1);
    if (GEReadFromPath(inPath, theDirectory + theStem + ".png", theFrames[0])) {
        std::vector<unsigned char> theTexture;
        std::string theError;
        if (!GEOpenFormats::PngToSst(theFrames, theTexture, theError))
            return GEOpenFormatFailed(theDirectory + theStem + ".png: " + theError, outFile);
        // a one-frame .sst: the 14-byte header, the frame's type byte, then the frame's Targa
        theTexture.erase(theTexture.begin(), theTexture.begin() + 15);
        outFile = GEMemoryFile(theTexture, theStem + ".tga");
        return kErrorOK;
    }
    return inPath->FindFile(inName, outFile);
}
