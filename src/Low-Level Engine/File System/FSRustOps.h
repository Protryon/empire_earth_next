// The FSFile seam of the Rust File System (rust/crates/ee_fs): a table of function pointers over a C++ FSFile&.
// Built per call by the facades (FSMemoryFile, FSChunk, FSArchive).
#ifndef FSRUSTOPS_H
#define FSRUSTOPS_H

#include "rust/ee_rust.h"

class FSFile;

void FSBuildFileOps(FSFile& inFile, EeFsFileOps& outOps);

#endif
