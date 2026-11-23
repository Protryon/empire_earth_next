// The converter's gate's C++ wrappers of ee_data's text compiler (rust/data/ffi.h): the db tables' Content, the
// content dump, the keyed .civ text, the rules' load, dump and binary songs. Were the game's (EEDataText.h, EERules.cpp)
// until the game's loads became Rust (ee_game gamecpp db_text.rs: game core lane, round 11); only the gate calls them.
#ifndef EE_DATATEXT_GATE_H
#define EE_DATATEXT_GATE_H

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "EEDataText.h"
#include "ee/EERules.h"

namespace EEDataText {

// The text file extension of a db table (".csv" or ".yaml"), or 0 when the table has no text form.
// inTable: the file stem, case-insensitive ("dbworld", "dbUnitBehavior"). The rules documents are ".yaml".
inline const char* TextExtension(const char* inTable)
{
    return ee_data_text_extension(inTable);
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

// Reads a keyed .civ text: its name as UTF-8 bytes and its upgrades in order; false with the error (the parse
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

// The rules documents and epochs.yaml's media read from the Content into inRules (references by id); false with a
// message on the first problem
inline bool LoadRules(EERules& inRules, Content& inContent, std::string& outError)
{
    EeDataBytes theError{nullptr, 0};
    if (!ee_data_rules_load(const_cast<EeDataRules*>(inRules.RustRules()), inContent.Handle(), &theError)) {
        outError = TakeBytes(theError);
        return false;
    }
    inRules.Refresh();
    return true;
}

// The songs from a whole dbmusic.dat (the binary load); false on a short file
inline bool RulesMusicFromDbmusicFile(EERules& inRules, const unsigned char* inData, size_t inSize)
{
    bool theOK = ee_data_rules_music_from_dbmusic(const_cast<EeDataRules*>(inRules.RustRules()), inData, inSize);
    inRules.Refresh();
    return theOK;
}

// The content dump of the rules (docs/MODDING_PLAN.md gate G1): `"rules.<doc>":{...}` lines, values resolved
inline void DumpRules(const EERules& inRules, std::string& outJson)
{
    EeDataBytes theBytes{nullptr, 0};
    ee_data_rules_dump(inRules.RustRules(), &theBytes);
    outJson += TakeBytes(theBytes);
}

}  // namespace EEDataText

#endif
