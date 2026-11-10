// GEOpenFormats: the game's models and textures are glTF and PNG files only (docs/RUST_PLAN.md D9, docs/MODEL_FORMATS.md).
// GEOpenFormatsFile.cpp finds them on the data path and has Rust (rust/crates/ee_openformats) decode them into what
// the engine stores: the model GEModel::StoreLoaded stores and the frames GETexture::Load stores. There is no .cem or
// .sst reader any more.
#ifndef GE_OPENFORMATS_H
#define GE_OPENFORMATS_H

#include <map>

class FSPath;
class FSFileSpec;
class UString;
struct EeGeCemModel;
struct EeOfTexture;

// GEModelManager::LoadModel's lookup of a model ("models\x"): "<name>.glb", else "<name>.gltf", decoded into the
// model GEModel::StoreLoaded stores (inShared: the frames the model shares, EEGraphicObject's map), which the caller
// frees with ee_ge_cem_free. kErrorNotFound when neither file is on the path. A file that does not convert stops
// the game with its error message.
long GEFindModelFile(FSPath* inPath, const UString& inName, const std::map<unsigned long, long>& inShared,
                     EeGeCemModel*& outModel);

// GETextureManager's lookup of a texture ("textures\x"): "<name>.png" and its further frames "<name>@1.png",
// "<name>@2.png", ... (up to the first missing one), decoded into the frames GETexture::Load stores, which the
// caller frees with ee_of_texture_free. kErrorNotFound when "<name>.png" is not on the path. A file that does not
// decode stops the game with its error message.
long GEFindTextureFile(FSPath* inPath, const UString& inName, EeOfTexture*& outTexture);

// The lookup of a picture read as a Targa through GESurface, around the texture manager (textures\ColorPalette.tga,
// textures\Volcano.tga, the mouse pointers): when "<name>.png" is on the path, returns it converted to the Targa bytes
// as a new FSMemoryFile named "<name>.tga"; otherwise FSPath::FindFile. Declared for the game in lle/GETextureManager.h.
long GEFindTargaFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile);

#endif
