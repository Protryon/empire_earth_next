// EEDataText: the db tables' text forms (CSV / strict YAML, written by tools/eedata) compiled back into the binary
// db file bytes the loaders read. Portable C++ (no platform or engine headers); the layouts come from
// EEDataTextSpecs.h, generated from the same specs as the Python converter.
// EEDbFindFile (EEDataTextFile.cpp) is the game's seam: it serves a table's text form, compiled, as an in-memory
// file in place of the .dat.
#ifndef EE_DATATEXT_H
#define EE_DATATEXT_H

#include <string>
#include <vector>

namespace EEDataText {

// The text file extension of a db table (".csv" or ".yaml"), or 0 when the table has no text form.
// inTable: the file stem, case-insensitive ("dbworld", "dbUnitBehavior").
const char* TextExtension(const char* inTable);

// Compiles a table's text into the db file bytes. Returns false with a message ("dbworld row 3: mIndex: ...")
// on the first problem; outData is then undefined.
bool Compile(const char* inTable, const char* inText, unsigned long inLength, std::vector<unsigned char>& outData,
             std::string& outError);

}  // namespace EEDataText

class FSPath;
class FSFileSpec;
class UString;

// FSPath::FindFile for a db table: when the table's text form (the same name with .csv/.yaml) is on the path,
// returns that text compiled to the binary db file as a new FSMemoryFile; otherwise the .dat as FindFile does.
// A text form that does not compile stops the game with its error message.
long EEDbFindFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile);

#endif
