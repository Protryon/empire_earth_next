// GEOpenFormats: the game's models and textures are glTF and PNG files only (docs/RUST_PLAN.md D9, docs/MODEL_FORMATS.md).
// Rust finds them on the data path and decodes them into what the engine stores (ee_openformats find.rs:
// ee_of_find_model, ee_of_find_texture; GEOpenFormatsFile.cpp before R4.3 part 15); the path and its files are C++,
// reached through this host (GEModel.cpp).
#ifndef GE_OPENFORMATS_H
#define GE_OPENFORMATS_H

#include "rust/ee_openformats.h"

intptr_t GEOpenFormatsHost(EeOfFileOp inOp, void* inObj, intptr_t a, intptr_t b);

#endif
