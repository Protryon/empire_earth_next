// C:\EEX\Low-Level Engine\File System\File System.h (RECONSTRUCTION; File System.cpp 0x4020..0x4e29): the file
// system's entry point, for File System.cpp, which does not include the umbrella. lle/LowLevelEngine.h repeats the
// declaration instead of including this file: with the include, Default.dll's `_Tree::_Dec` canary (0x12ee6) flipped
// (that canary also flips with the include path alone: a byte-identical copy of include/ elsewhere compiles it
// exact). Requires LLE_API (lle/LowLevelEngineCore.h).
#ifndef LLE_FILESYSTEM_H
#define LLE_FILESYSTEM_H

#include "lle/LLEForward.h"

LLE_API void FSInitialize(const UString& inPath);     // proven ?FSInitialize@@YAXABVUString@@@Z (exe import)

#endif
