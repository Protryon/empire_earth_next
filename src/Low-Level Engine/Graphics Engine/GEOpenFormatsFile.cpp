// GEFindModelFile / GEFindTextureFile / GEFindTargaFile: the game's models and textures on the data path (new unit,
// not in the original game; tools/eemodel writes the files). The models are glTF (.glb, .gltf) and the textures PNG
// only (docs/RUST_PLAN.md D9): Rust (rust/crates/ee_openformats) decodes them into what GEModel::StoreLoaded and
// GETexture::Load store; no .cem or .sst is read. The path's own order applies among the files found.
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
#include "rust/ee_rust.h"

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

// "models\x" -> "models\" (the directory part, with its separator)
static std::string GEDirectoryOf(const std::string& inName)
{
    size_t theSlash = inName.find_last_of("\\/");
    return theSlash == std::string::npos ? std::string() : inName.substr(0, theSlash + 1);
}

// Rust's message (bytes) -> a string, freeing the buffer
static std::string GETakeMessage(EeOfBytes& ioBytes)
{
    std::string theMessage((const char*)ioBytes.ptr, ioBytes.len);
    ee_of_bytes_free(ioBytes);
    return theMessage;
}

static long GEOpenFormatFailed(const std::string& inMessage)
{
    // a broken open-format file stops the game with the place of the problem
    ESVerify(__FILE__, __LINE__, inMessage.c_str());
    return kErrorFailed;
}

struct GEBufferResolver {
    FSPath* mPath;
    std::string mDirectory;
};

// A glTF's external buffer (its uri, relative to the .gltf), for Rust
static bool GEResolveBuffer(void* inUser, const uint8_t* inUri, size_t inLength, EeOfBytes* outBytes)
{
    GEBufferResolver* theResolver = static_cast<GEBufferResolver*>(inUser);
    std::string theName = theResolver->mDirectory + std::string((const char*)inUri, inLength);
    for (size_t i = 0; i < theName.size(); i++)
        if (theName[i] == '/')
            theName[i] = '\\';
    std::vector<unsigned char> theData;
    if (!GEReadFromPath(theResolver->mPath, theName, theData))
        return false;
    *outBytes = ee_of_bytes_copy(theData.empty() ? NULL : &theData[0], theData.size());
    return true;
}

long GEFindModelFile(FSPath* inPath, const UString& inName, const std::map<unsigned long, long>& inShared,
                     EeGeCemModel*& outModel)
{
    outModel = NULL;
    std::string theName = (const char*)inName;
    std::vector<EeGeSharedFrame> theShared;
    for (std::map<unsigned long, long>::const_iterator theIterator = inShared.begin(); theIterator != inShared.end();
         theIterator++) {
        EeGeSharedFrame theEntry;
        theEntry.frame = (uint32_t)theIterator->first;
        theEntry.offset = (int32_t)theIterator->second;
        theShared.push_back(theEntry);
    }
    static const char* const kExtensions[] = {".glb", ".gltf"};
    for (int i = 0; i < 2; i++) {
        std::string theFileName = theName + kExtensions[i];
        std::vector<unsigned char> theSource;
        if (!GEReadFromPath(inPath, theFileName, theSource))
            continue;
        GEBufferResolver theResolver = {inPath, GEDirectoryOf(theName)};
        EeOfBytes theError;
        outModel = ee_of_gltf_to_model(theSource.empty() ? NULL : &theSource[0], theSource.size(), GEResolveBuffer,
                                       &theResolver, theShared.empty() ? NULL : &theShared[0], theShared.size(),
                                       &theError);
        if (outModel == NULL)
            return GEOpenFormatFailed(theFileName + ": " + GETakeMessage(theError));
        return kErrorOK;
    }
    return kErrorNotFound;
}

long GEFindTextureFile(FSPath* inPath, const UString& inName, EeOfTexture*& outTexture)
{
    outTexture = NULL;
    std::string theName = (const char*)inName;
    std::vector<std::vector<unsigned char> > theFrames(1);
    if (!GEReadFromPath(inPath, theName + ".png", theFrames[0]))
        return kErrorNotFound;
    // further frames: <name>@1.png, <name>@2.png, ... up to the first missing one
    for (unsigned long i = 1;; i++) {
        char theSuffix[24];
        sprintf(theSuffix, "@%lu.png", i);
        std::vector<unsigned char> theFrame;
        if (!GEReadFromPath(inPath, theName + theSuffix, theFrame))
            break;
        theFrames.push_back(theFrame);
    }
    std::vector<EeOfSlice> theSlices(theFrames.size());
    for (size_t i = 0; i < theFrames.size(); i++) {
        theSlices[i].ptr = theFrames[i].empty() ? NULL : &theFrames[i][0];
        theSlices[i].len = theFrames[i].size();
    }
    EeOfBytes theError;
    outTexture = ee_of_png_to_texture(&theSlices[0], theSlices.size(), &theError);
    if (outTexture == NULL)
        return GEOpenFormatFailed(theName + ".png: " + GETakeMessage(theError));
    return kErrorOK;
}

// "textures\x.tga" -> "textures\", "x"
static void GESplitName(const UString& inName, std::string& outDirectory, std::string& outStem)
{
    std::string theName = (const char*)inName;
    outDirectory = GEDirectoryOf(theName);
    outStem = theName.substr(outDirectory.size());
    size_t theDot = outStem.rfind('.');
    if (theDot != std::string::npos)
        outStem.resize(theDot);
}

long GEFindTargaFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile)
{
    std::string theDirectory, theStem;
    GESplitName(inName, theDirectory, theStem);
    std::vector<unsigned char> theSource;
    if (GEReadFromPath(inPath, theDirectory + theStem + ".png", theSource)) {
        EeOfBytes theTarga, theError;
        if (!ee_of_png_to_tga(theSource.empty() ? NULL : &theSource[0], theSource.size(), &theTarga, &theError)) {
            outFile = NULL;
            return GEOpenFormatFailed(theDirectory + theStem + ".png: " + GETakeMessage(theError));
        }
        // the Targa as an in-memory file named like the one the caller asked for
        unsigned char* theBuffer = new unsigned char[theTarga.len ? theTarga.len : 1];
        if (theTarga.len)
            memcpy(theBuffer, theTarga.ptr, theTarga.len);
        FSMemoryFile* theFile = new FSMemoryFile(theBuffer, (unsigned long)theTarga.len, 1000, true);   // owns theBuffer
        ee_of_bytes_free(theTarga);
        theFile->SetFile(UString((theStem + ".tga").c_str()));
        outFile = theFile;
        return kErrorOK;
    }
    return inPath->FindFile(inName, outFile);
}
