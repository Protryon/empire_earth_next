// EEDataText: the db tables' text forms (CSV / strict YAML, written by tools/eedata) compiled back into the binary
// db file bytes the loaders read. Portable C++ (no platform or engine headers); the layouts come from
// EEDataTextSpecs.h, generated from the same specs as the Python converter.
// EEDbFindFile (EEDataTextFile.cpp) is the game's seam: it serves a table's text form, compiled, as an in-memory
// file in place of the .dat.
#ifndef EE_DATATEXT_H
#define EE_DATATEXT_H

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace EEDataText {

// The text file extension of a db table (".csv" or ".yaml"), or 0 when the table has no text form.
// inTable: the file stem, case-insensitive ("dbworld", "dbUnitBehavior"). The rules documents (kRuleDocuments) are
// ".yaml".
const char* TextExtension(const char* inTable);

// The rules documents: named settings that were literals in the code (docs/DATA_FORMATS.md 8.8), read from
// db\<name>.yaml next to the db tables; a missing one keeps the code's vanilla values (EERules.cpp).
extern const char* const kRuleDocuments[];
extern const unsigned long kRuleDocumentCount;

// The parsed text (the strict YAML subset of tools/eedata/textio.py): a scalar, a mapping (keys in file order) or a
// list.
struct Node {
    enum Kind { kNull, kScalar, kMap, kList };
    Kind mKind = kNull;
    bool mQuoted = false;
    long mLine = 0;
    std::string mText;                 // scalar
    std::vector<std::string> mKeys;    // map, in order
    std::vector<Node> mItems;          // map values / list items

    bool IsMissing() const
    {
        return this->mKind == kNull ||
               (this->mKind == kScalar && !this->mQuoted &&
                (this->mText.empty() || this->mText == "~" || this->mText == "null"));
    }
    const Node* Find(const std::string& inKey) const
    {
        for (size_t i = 0; i < this->mKeys.size(); i++)
            if (this->mKeys[i] == inKey)
                return &this->mItems[i];
        return nullptr;
    }
};

// A problem in a text, with its place ("combat line 12: minimum: 'x' is not an integer"). The text compiler throws it
// internally; Content::Compile returns it as a message. The document readers below throw it to their caller.
struct Failure {
    std::string mText;
};

// A document's scalars, read with the converter's grammar (correctly rounded floats); they throw Failure naming
// where + name. A missing value is an error.
long NodeInt(const Node* inValue, const std::string& inWhere, const char* inName);   // a signed 32-bit integer
float NodeFloat(const Node* inValue, const std::string& inWhere, const char* inName);
bool NodeBool(const Node* inValue, const std::string& inWhere, const char* inName);   // 0/1, true/false
const std::string& NodeScalar(const Node* inValue, const std::string& inWhere, const char* inName);   // the raw text
// A whole text in the YAML subset -> its tree; false with a message ("line 3: ...")
bool ParseDocument(const std::string& inText, Node& outDoc, std::string& outError);
// A string scalar's text as the game's CP-1252 bytes (throws Failure naming where + name)
std::string NodeText(const Node* inValue, const std::string& inWhere, const char* inName);
// The same without exceptions (for the units built without /EH): false with the message in outText
bool TryNodeText(const Node* inValue, const std::string& inWhere, const char* inName, std::string& outText);
// CP-1252 bytes -> a double-quoted YAML scalar (UTF-8): what NodeText reads back
std::string QuoteText(const std::string& inBytes);
// A dbworld value named `<id>.<F|F2|L|L2>` ("fanaticism.F2") -> its row and value (0 F .. 3 L2); the value must be
// one the row's readers use (the text carries only those). Throws Failure naming where + name.
void WorldValue(const std::string& inText, const std::string& inWhere, const char* inName, long& outRow,
                int& outValue);

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

    // A document of the data path parsed: a rules document ("combat") or a db table's text read for its text-only
    // keys ("epochs": music, citizen). False when the reader has no such text; throws Failure on a parse error.
    bool Document(const char* inName, Node& outDoc);
    // The key of the row of inTable that inID names (its row position; dbtechtree and epochs: the tech id); throws
    // Failure ("<where>: <field>: 'x' is not an id of dbobjects") when it is not an id of a listed row.
    long Key(const char* inTable, const Node* inID, const std::string& inWhere, const char* inField);
    // The number of listed rows of inTable (epochs: the epoch count); throws Failure.
    long Count(const char* inTable);
    // The ids of inTable's listed rows by key (its row position; dbtechtree: the tech id); false with a message when
    // the table has no text (e.g. a binary data tree) or does not parse. Never throws.
    bool Ids(const char* inTable, std::map<long, std::string>& outIds, std::string& outError);

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
// The row ids of dbobjects or dbtechtree by key (the unit class / the tech id), as the last load from text read them
// (EEDbFindFile); false and empty when that table was loaded from its .dat (no ids).
bool EEDbRowIds(const char* inTable, std::map<long, std::string>& outIds);

// Reads the rules documents (ee/EERules.h: db\combat.yaml ... and epochs.yaml's media) from the path into
// EEGetRules(), through the same Content as the db tables (references by id). A missing document keeps the
// defaults; a broken one stops the game with its message. EEInitializeDatabase calls it first.
void EELoadRules(FSPath* inPath);
// EE_DUMP_CONTENT: appends the rules' content dump (after the db tables are loaded: the binary load's songs come from
// dbmusic's flags)
void EEDumpRules();

#endif
