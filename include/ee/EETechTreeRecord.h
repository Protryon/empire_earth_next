// EETechTreeRecord.h (RECONSTRUCTION; file name inferred; field names are provisional). One 0x10c-byte
// record of the tech tree database as EETechTreeEpoch::Load 0x1ce10e and EETechTreeNode::Load 0x1cf370 read it
// (FSFile::Read(char*, 0x10c, ...) into a local). Fields by offset, named after the members they are copied to.
// Only the tech tree units include it (an array member costs every includer a VC6 `$E` number).
#ifndef EE_EETECHTREERECORD_H
#define EE_EETECHTREERECORD_H

#include "rust/sim/tech.h"

// the record is Rust's (ee_sim tech nodes.rs EeTechTreeRecord: the fields, their evidence and the size assert)
typedef EeTechTreeRecord EETechTreeRecord;

#endif
