// New (port): where a record base sits in its class as this build's compiler placed it, for the start-up check of
// the Rust heads (ee_base record_head.rs; docs/progress/headers.md "Record bases"). A list of EE_RECORD_BASE entries
// goes to the Rust check of the crate that declares the heads (ee_world_record_bases_check: ee_sim's).
#ifndef EE_EERECORDBASE_H
#define EE_EERECORDBASE_H

#include <stdint.h>
#include "rust/base/record_head.h"

// where Base sits in a Derived object (a cast of a made-up address: nothing is read)
template <class Derived, class Base> inline uint64_t EERecordBaseOffset()
{
    Derived* theObject = reinterpret_cast<Derived*>(static_cast<uintptr_t>(0x10000));
    return reinterpret_cast<uintptr_t>(static_cast<Base*>(theObject)) - 0x10000;
}

// one entry of the list: the class, the record (the names the Rust head gives them) and the record's offset
#define EE_RECORD_BASE(Class, Record) EeRecordBase{ #Class, #Record, EERecordBaseOffset<Class, Record>() }

#endif
