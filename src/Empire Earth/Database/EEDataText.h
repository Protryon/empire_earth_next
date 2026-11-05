// EEDataText: the db tables' text forms (CSV / strict YAML, written by tools/eedata) compiled back into the binary
// db file bytes the loaders read. Portable C++ (no platform or engine headers); the layouts come from
// EEDataTextSpecs.h, generated from the same specs as the Python converter.
// EEDbFindFile (EEDataTextFile.cpp) is the game's seam: it serves a table's text form, compiled, as an in-memory
// file in place of the .dat.
#ifndef EE_DATATEXT_H
#define EE_DATATEXT_H

#include <functional>
#include <string>
#include <vector>

namespace EEDataText {

// The text file extension of a db table (".csv" or ".yaml"), or 0 when the table has no text form.
// inTable: the file stem, case-insensitive ("dbworld", "dbUnitBehavior").
const char* TextExtension(const char* inTable);

// The db tables' texts and the ids that tie them together (tools/eedata tables.Content). Every listed row has an
// `id`; a reference to a row of another table is written as that row's id and compiled to its key (its row
// position; dbtechtree: its mID). The reader returns a table's text (by file stem, "dbobjects"), false when the
// table has no text form; each text is read once and parsed once, its ids are indexed on first use (a referenced
// table is parsed before it is compiled). This is the single place a later mod merge step feeds.
class Content {
public:
    typedef std::function<bool(const char* inTable, std::string& outText)> Reader;
    explicit Content(const Reader& inRead);
    ~Content();
    Content(const Content&) = delete;
    Content& operator=(const Content&) = delete;

    // Compiles a table into the db file bytes. Returns false with a message ("dbobjects line 120 (id priest):
    // mGraphic: 'x' is not an id of dbgraphics") on the first problem; outData is then undefined. A duplicate id
    // or a reference to an unknown id is an error.
    bool Compile(const char* inTable, std::vector<unsigned char>& outData, std::string& outError);

private:
    struct Impl;
    Impl* mImpl;
};

// One table on its own (no other table's text: a reference by id to another table is an error).
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
