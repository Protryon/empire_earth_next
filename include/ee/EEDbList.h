// A db table held by ee_data (port; rust/crates/ee_data tables.rs EeDataList): the entries are the records'
// members, Rust's boxes, and the class (the members' struct as its only base, nothing of its own) reads them as the
// std::vector<T*> it had (operator[], size). A slot without an entry, or one past the end, is NULL.
#ifndef EE_EEDBLIST_H
#define EE_EEDBLIST_H

#include <stddef.h>

#include "rust/data/tables.h"

template <class T, EeDataList K> struct EEDbList {
    // the class's object is its base's (reinterpret_cast: T may be incomplete where a reader indexes it)
    T* operator[](size_t inIndex) const { return reinterpret_cast<T*>(ee_data_list_get(K, inIndex)); }
    size_t size() const { return ee_data_list_count(K); }
    void clear() const { ee_data_list_clear(K); }
};

#endif
