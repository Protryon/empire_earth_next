// GEOpenFormats: the open forms of the game's models and textures (tools/eemodel; docs/MODEL_FORMATS.md) turned back
// into the binary streams the loaders read. Portable C++ (no platform or engine headers): glTF 2.0 through cgltf,
// PNG through stb_image. GEOpenFormatsFile.cpp is the game's seam: it serves a model's .gltf/.glb in place of its
// .cem and a texture's .png in place of its .tga/.sst, as in-memory files.
#ifndef GE_OPENFORMATS_H
#define GE_OPENFORMATS_H

#include <string>
#include <vector>

namespace GEOpenFormats {

// Reads an external file a glTF names (a buffer's uri, percent-decoded, relative to the .gltf) -> its bytes; false
// when it cannot be found.
typedef bool (*ResolveFunc)(void* inUser, const std::string& inUri, std::vector<unsigned char>& outData);

// A .gltf or .glb -> the bytes of the .cem (version 2.0) the game would load in its place (MODEL_FORMATS.md 4.3).
// Returns false with a message on the first problem.
bool GltfToCem(const unsigned char* inData, unsigned long inLength, ResolveFunc inResolve, void* inUser,
               std::vector<unsigned char>& outCem, std::string& outError);

// PNG files (frame 0, 1, ...) -> the bytes of the .sst (14-byte header, one type-0 Targa per frame) the game would
// load in their place (MODEL_FORMATS.md 5). Returns false with a message on the first problem.
bool PngToSst(const std::vector<std::vector<unsigned char> >& inFrames, std::vector<unsigned char>& outSst,
              std::string& outError);

}  // namespace GEOpenFormats

class FSPath;
class FSFileSpec;
class UString;

// GEModelManager::LoadModel's lookup of "<name>.cem": when "<name>.glb" or "<name>.gltf" is anywhere on the path (it
// wins over the .cem), returns it converted to the .cem bytes as a new FSMemoryFile named like the .cem; otherwise
// FSPath::FindFile. A glTF that does not convert stops the game with its error message.
long GEFindModelFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile);

// GETextureManager's lookup of "<name>.tga" or "<name>.sst": when "<name>.png" is on the path, returns it (and
// "<name>@1.png", "<name>@2.png", ... as further frames) converted to .sst bytes as a new FSMemoryFile named
// "<name>.sst"; otherwise FSPath::FindFile. outIsOpenFormat says which: the caller then keys the texture by the .sst name.
long GEFindTextureFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile, bool& outIsOpenFormat);

// The lookup of a picture read as a Targa through GESurface, around the texture manager (textures\ColorPalette.tga,
// textures\Volcano.tga, the mouse pointers): when "<name>.png" is on the path, returns it converted to the Targa bytes
// (as GEFindTextureFile's frame) as a new FSMemoryFile named "<name>.tga"; otherwise FSPath::FindFile. Declared for the
// game in lle/GETextureManager.h.
long GEFindTargaFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile);

#endif
