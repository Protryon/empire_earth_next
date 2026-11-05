// stb_image 2.30 (public domain / MIT, Sean Barrett; include/stb_image/stb_image.h, unmodified, commit
// 013ac3beddff3dbffafd5177e7972067cd2b5083, sha256 594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3):
// the implementation unit, PNG only. Decodes the PNG textures the game loads in place of .sst/.tga files
// (GEOpenFormats.cpp, tools/eemodel).
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image/stb_image.h"
