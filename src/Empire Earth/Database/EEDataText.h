// EEDataText: the db tables' text forms (CSV / strict YAML, written by tools/eedata) compiled back into the binary
// db file bytes the loaders read. The compiler is Rust (rust/crates/ee_data, docs/RUST_PLAN.md R2): the C API in
// rust/ee_rust.h, wrapped here for C++. Portable C++ (no platform or engine headers). The game's own loads are Rust
// (ee_game gamecpp db_text.rs); the converter's gate wraps the rest of the C API (tools/eedata/cppgate/eedata_gate_text.h).
// EEDbFindFile (EEDataTextFile.cpp) is the game's seam: it serves a table's text form, compiled, as an in-memory
// file in place of the .dat.
#ifndef EE_DATATEXT_H
#define EE_DATATEXT_H

#include <map>
#include <string>

#include "rust/data/ffi.h"

namespace EEDataText {

inline std::string TakeBytes(EeDataBytes& ioBytes)
{
    std::string out(reinterpret_cast<const char*>(ioBytes.ptr), ioBytes.len);
    ee_data_bytes_free(ioBytes);
    ioBytes.ptr = nullptr;
    ioBytes.len = 0;
    return out;
}

// The game's text (UTF-8) -> a double-quoted YAML scalar: what the text forms read back
inline std::string QuoteText(const std::string& inBytes)
{
    EeDataBytes out{nullptr, 0};
    ee_data_quote_text(reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.size(), &out);
    return TakeBytes(out);
}

}  // namespace EEDataText

class FSPath;
class FSFileSpec;
class UString;

// FSPath::FindFile for a db table ("db\dbworld.dat" names the table dbworld): returns the table's text form (the same
// name with .csv/.yaml) compiled to the table's record stream as a new FSMemoryFile. No binary db file is read (RUST_PLAN
// D9): a missing text table, a table with no text form or a text that does not compile stops the game with its message.
long EEDbFindFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile);

// Reads the rules documents (ee/EERules.h: db\combat.yaml ... and epochs.yaml's media) from the path into
// EEGetRules(), through the same Content as the db tables (references by id). A missing document keeps the
// defaults; a broken one stops the game with its message. EEInitializeDatabase calls it first.
void EELoadRules(FSPath* inPath);

#endif
