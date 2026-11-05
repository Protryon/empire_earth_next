// EEDataDump: the content dump (docs/MODDING_PLAN.md gate G1): a db file's bytes, as the loaders read them, as
// canonical JSON. Portable C++ (no platform or engine headers).
#ifndef EE_DATADUMP_H
#define EE_DATADUMP_H

#include <cstddef>
#include <string>

namespace EEDataText {

// Appends `"<table>":[...]` to outJson: every row (dbtechtree: every epoch block) with every field the game reads,
// floats as their bit patterns, strings as their bytes; rows the loaders skip as {"row":n,"skipped":true}. False
// with a message when the bytes are not a whole db file of that table.
bool DumpTable(const char* inTable, const unsigned char* inData, size_t inSize, std::string& outJson,
               std::string& outError);

}  // namespace EEDataText

#endif
