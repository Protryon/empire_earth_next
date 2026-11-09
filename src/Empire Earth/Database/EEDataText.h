// EEDataText: the db tables' text forms (CSV / strict YAML, written by tools/eedata) compiled back into the binary
// db file bytes the loaders read. The compiler is Rust (rust/crates/ee_data, docs/RUST_PLAN.md R2): the C API in
// rust/ee_rust.h, wrapped here for C++. Portable C++ (no platform or engine headers).
// EEDbFindFile (EEDataTextFile.cpp) is the game's seam: it serves a table's text form, compiled, as an in-memory
// file in place of the .dat.
#ifndef EE_DATATEXT_H
#define EE_DATATEXT_H

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "rust/ee_rust.h"

namespace EEDataText {

inline std::string TakeBytes(EeDataBytes& ioBytes)
{
    std::string out(reinterpret_cast<const char*>(ioBytes.ptr), ioBytes.len);
    ee_data_bytes_free(ioBytes);
    ioBytes.ptr = nullptr;
    ioBytes.len = 0;
    return out;
}

// The text file extension of a db table (".csv" or ".yaml"), or 0 when the table has no text form.
// inTable: the file stem, case-insensitive ("dbworld", "dbUnitBehavior"). The rules documents are ".yaml".
inline const char* TextExtension(const char* inTable)
{
    return ee_data_text_extension(inTable);
}

// CP-1252 bytes -> a double-quoted YAML scalar (UTF-8): what the text forms read back
inline std::string QuoteText(const std::string& inBytes)
{
    EeDataBytes out{nullptr, 0};
    ee_data_quote_text(reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.size(), &out);
    return TakeBytes(out);
}

// The db tables' texts and the ids that tie them together. The reader returns a table's text (by file stem,
// "dbobjects"), false when the table has no text form. Each text is read once and parsed once; a referenced table
// is parsed before it is compiled.
class Content {
public:
    typedef std::function<bool(const char* inTable, std::string& outText)> Reader;

    explicit Content(const Reader& inRead) : mRead(inRead) { mHandle = ee_data_content_new(&ReadTrampoline, this); }
    ~Content() { ee_data_content_free(mHandle); }
    Content(const Content&) = delete;
    Content& operator=(const Content&) = delete;

    // Compiles a table into the db file bytes; false with a message ("dbobjects line 120 (id priest): mGraphic: 'x'
    // is not an id of dbgraphics") on the first problem.
    bool Compile(const char* inTable, std::vector<unsigned char>& outData, std::string& outError)
    {
        EeDataBytes out{nullptr, 0}, err{nullptr, 0};
        if (!ee_data_compile(mHandle, inTable, &out, &err)) {
            outError = TakeBytes(err);
            return false;
        }
        std::string bytes = TakeBytes(out);
        outData.assign(bytes.begin(), bytes.end());
        return true;
    }

    // The ids of inTable's listed rows by key (its row position; dbtechtree: the tech id); false with a message when
    // the table has no text or does not parse. Never throws.
    bool Ids(const char* inTable, std::map<long, std::string>& outIds, std::string& outError)
    {
        outIds.clear();
        EeDataBytes err{nullptr, 0};
        if (!ee_data_ids(mHandle, inTable, &IdTrampoline, &outIds, &err)) {
            outError = TakeBytes(err);
            return false;
        }
        return true;
    }

    EeDataContent* Handle() const { return mHandle; }

private:
    static bool ReadTrampoline(void* inCtx, const char* inName, EeDataSink* inSink)
    {
        std::string text;
        if (!static_cast<Content*>(inCtx)->mRead(inName, text))
            return false;
        ee_data_sink_write(inSink, reinterpret_cast<const uint8_t*>(text.data()), text.size());
        return true;
    }
    static void IdTrampoline(void* inCtx, int32_t inKey, const uint8_t* inId, size_t inLen)
    {
        (*static_cast<std::map<long, std::string>*>(inCtx))[(long)inKey] =
            std::string(reinterpret_cast<const char*>(inId), inLen);
    }

    Reader mRead;
    EeDataContent* mHandle;
};

// Appends `"<table>":[...]` to outJson: every row with every field the game reads, floats as their bit patterns,
// strings as their bytes (docs/MODDING_PLAN.md gate G1). False with a message when the bytes are not a whole db file.
inline bool DumpTable(const char* inTable, const unsigned char* inData, size_t inSize, std::string& outJson,
                      std::string& outError)
{
    EeDataBytes out{nullptr, 0}, err{nullptr, 0};
    if (!ee_data_dump_table(inTable, inData, inSize, &out, &err)) {
        outError = TakeBytes(err);
        return false;
    }
    outJson += TakeBytes(out);
    return true;
}

// One keyed upgrade of a .civ text: its key and line
struct CivKey {
    std::string mKey;
    long mLine;
};

// Reads a keyed .civ text: its name as CP-1252 bytes and its upgrades in order; false with the error (the parse
// error, "<where>: not a civilization", or a bad name).
inline bool ReadCivilization(const std::string& inText, const char* inWhere, std::string& outName,
                             std::vector<CivKey>& outKeys, std::string& outError)
{
    EeDataBytes name{nullptr, 0}, err{nullptr, 0};
    struct Visitor {
        static void Visit(void* inCtx, const uint8_t* inKey, size_t inLen, int32_t inLine)
        {
            static_cast<std::vector<CivKey>*>(inCtx)->push_back(
                CivKey{std::string(reinterpret_cast<const char*>(inKey), inLen), (long)inLine});
        }
    };
    if (!ee_data_civ_read(reinterpret_cast<const uint8_t*>(inText.data()), inText.size(), inWhere, &name, &Visitor::Visit,
                          &outKeys, &err)) {
        outError = TakeBytes(err);
        return false;
    }
    outName = TakeBytes(name);
    return true;
}

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
