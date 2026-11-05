// EEDataDump: the content dump (docs/MODDING_PLAN.md gate G1). New unit (not in the original game). Portable C++17.
// Decodes a db file's bytes (what the loaders read: a .dat, or a text table compiled by EEDataText) into canonical
// JSON: every field the game reads, by its spec name, row by row. Rows the loaders skip are written as skipped and
// fields no reader uses are left out, so the dump of the retail .dat and of the text tables compiled from it are
// byte-identical (tools/eedata/eedata.py dump runs it on both).
// UNIT-FLAGS: -EHsc
#include "EEDataDump.h"
#include "EEDataTextSpecs.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace EEDataText {
namespace {

const unsigned long kSize[] = {1, 1, 1, 2, 2, 4, 4, 4, 8, 1};

struct Reader {
    const unsigned char* mData;
    size_t mSize;
    size_t mPos = 0;
    bool mBad = false;

    const unsigned char* Take(size_t n)
    {
        if (this->mBad || this->mPos + n > this->mSize) {
            this->mBad = true;
            return nullptr;
        }
        const unsigned char* p = this->mData + this->mPos;
        this->mPos += n;
        return p;
    }
    uint32_t U32()
    {
        const unsigned char* p = this->Take(4);
        return p ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24 : 0;
    }
};

uint32_t LE(const unsigned char* p, unsigned long n)
{
    uint32_t v = 0;
    for (unsigned long k = 0; k < n; k++)
        v |= (uint32_t)p[k] << (8 * k);
    return v;
}

long long Int(const unsigned char* p, unsigned char type)
{
    uint32_t v = LE(p, kSize[type]);
    switch (type) {
    case kEEDT_s8: return (int8_t)v;
    case kEEDT_s16: return (int16_t)v;
    case kEEDT_s32: return (int32_t)v;
    default: return v;
    }
}

void Str(std::string& out, const unsigned char* p, size_t n)
{
    out += '"';
    for (size_t i = 0; i < n && p[i]; i++) {
        unsigned char c = p[i];
        char buf[8];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c < 0x20 || c >= 0x7f) {
            snprintf(buf, sizeof(buf), "\\u%04x", c);   // the byte (CP-1252), not decoded: exact and plain ASCII
            out += buf;
        } else {
            out += (char)c;
        }
    }
    out += '"';
}

void Value(std::string& out, const EEDTField& f, const unsigned char* p)
{
    char buf[32];
    if (f.mType == kEEDT_f32) {
        snprintf(buf, sizeof(buf), "\"0x%08x\"", (unsigned)LE(p, 4));   // the bit pattern: exact
        out += buf;
    } else {
        snprintf(buf, sizeof(buf), "%lld", Int(p, f.mType));
        out += buf;
    }
}

void Field(std::string& out, const EEDTField& f, const unsigned char* rec)
{
    out += '"';
    out += f.mName;
    out += "\":";
    const unsigned char* p = rec + f.mOffset;
    if (f.mType == kEEDT_str) {
        Str(out, p, f.mCount);
    } else if (f.mType == kEEDT_pair_f32) {
        out += '[';
        for (unsigned long k = 0; k < f.mCount; k++) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%s[\"0x%08x\",\"0x%08x\"]", k ? "," : "", (unsigned)LE(p + 8 * k, 4),
                     (unsigned)LE(p + 8 * k + 4, 4));
            out += buf;
        }
        out += ']';
    } else if (f.mCount == 1) {
        Value(out, f, p);
    } else {
        out += '[';
        for (unsigned long k = 0; k < f.mCount; k++) {
            if (k)
                out += ',';
            Value(out, f, p + kSize[f.mType] * k);
        }
        out += ']';
    }
}

const EEDTField* Find(const EEDTStruct& st, const char* inName)
{
    for (unsigned long i = 0; i < st.mFieldCount; i++)
        if (strcmp(st.mFields[i].mName, inName) == 0)
            return &st.mFields[i];
    return nullptr;
}

// the rows the loaders skip before reading their fields (docs/DATA_FORMATS.md 8.2): written as skipped
bool Skipped(const EEDTTable& t, const unsigned char* rec)
{
    const EEDTStruct& st = *t.mRecord;
    if (strcmp(t.mName, "dbobjects") == 0)
        return Int(rec + Find(st, "mIsEnabled")->mOffset, kEEDT_bool) == 0 ||
               Int(rec + Find(st, "mUnitType")->mOffset, kEEDT_s32) == -1;
    if (strcmp(t.mName, "dbuiforms") == 0)
        return Int(rec + Find(st, "mFormID")->mOffset, kEEDT_s32) <= 0;
    if (strcmp(t.mName, "dbterrain") == 0)
        return Int(rec + Find(st, "mID")->mOffset, kEEDT_s32) < 0;
    return false;
}

// dbworld: a row's readers use only these fields; a row with none has no reader (null: not dumped)
const char* RowFields(const EEDTTable& t, long inRow, bool& outAll)
{
    outAll = t.mRowNameCount == 0;
    for (unsigned long i = 0; i < t.mRowNameCount; i++)
        if (t.mRowNames[i].mRow == inRow)
            return t.mRowNames[i].mFields;
    return nullptr;
}

bool InList(const char* inList, const char* inName)
{
    size_t n = strlen(inName);
    for (const char* p = inList; (p = strstr(p, inName)) != nullptr; p += n)
        if ((p == inList || p[-1] == ' ') && (p[n] == 0 || p[n] == ' '))
            return true;
    return false;
}

void Row(std::string& out, const EEDTTable& t, long inRow, const unsigned char* rec,
         const std::vector<std::pair<const unsigned char*, uint32_t>>& lead, const std::vector<uint32_t>* list,
         bool inEpoch, long inEpochIndex = 0)
{
    bool all;
    const char* only = RowFields(t, inRow, all);
    if (!all && !only)
        return;
    char buf[32];
    snprintf(buf, sizeof(buf), "\n  {\"row\":%ld", inRow);
    out += buf;
    if (Skipped(t, rec)) {
        out += ",\"skipped\":true},";
        return;
    }
    const EEDTStruct& st = *t.mRecord;
    for (const std::pair<const unsigned char*, uint32_t>& s : lead) {
        out += ',';
        out += "\"lead\":";
        std::string tmp(reinterpret_cast<const char*>(s.first), s.second);
        Str(out, reinterpret_cast<const unsigned char*>(tmp.c_str()), tmp.size());
    }
    for (unsigned long i = 0; i < st.mFieldCount; i++) {
        const EEDTField& f = st.mFields[i];
        if (strcmp(f.mName, "label") == 0)
            continue;   // text only: the game never reads it
        if (only && !InList(only, f.mName))
            continue;
        if (!inEpoch && t.mContainer == kEEDTC_techtree &&
            (strcmp(f.mName, "mFirstNodeID") == 0 || strcmp(f.mName, "mTitleID") == 0))
            continue;   // read from epoch records only
        out += ',';
        if (inEpoch && strcmp(f.mName, "mTitleID") == 0 && Int(rec + f.mOffset, kEEDT_s32) == 0) {
            char buf[48];
            snprintf(buf, sizeof(buf), "\"mTitleID\":%ld", 30145 + inEpochIndex);   // what the game uses for 0
            out += buf;
            continue;
        }
        Field(out, f, rec);
    }
    if (list) {
        out += ",\"list\":[";
        for (size_t k = 0; k < list->size(); k++) {
            snprintf(buf, sizeof(buf), "%s%lld", k ? "," : "",
                     t.mListSigned ? (long long)(int32_t)(*list)[k] : (long long)(*list)[k]);
            out += buf;
        }
        out += ']';
    }
    out += "},";
}

unsigned long LeadCount(const EEDTTable& t)
{
    unsigned long total = 0;
    for (unsigned long i = 0; i < t.mLeadCount; i++) {
        const char* s = t.mLead[i];
        const char* br = strchr(s, '[');
        unsigned long n = br ? strtoul(br + 1, nullptr, 10) : 0;
        total += n == 0 ? 1 : (strchr(s, '|') ? 2 * n : n);
    }
    return total;
}

}  // namespace

bool DumpTable(const char* inTable, const unsigned char* inData, size_t inSize, std::string& outJson,
               std::string& outError)
{
    const EEDTTable* t = nullptr;
    for (unsigned long i = 0; i < kEEDTTableCount; i++)
        if (strcmp(kEEDTTables[i].mName, inTable) == 0)
            t = &kEEDTTables[i];
    if (!t || t->mTextOnly) {
        outError = std::string(inTable) + ": not a db table with a text form";
        return false;
    }
    unsigned long size = t->mRecord->mSize;
    Reader r{inData, inSize};
    std::string out = "\"" + std::string(t->mName) + "\":[";
    std::vector<std::pair<const unsigned char*, uint32_t>> noLead;
    std::vector<uint32_t> list;
    auto ReadList = [&]() {
        list.clear();
        uint32_t n = r.U32();
        for (uint32_t k = 0; k < n && !r.mBad; k++)
            list.push_back(r.U32());
    };
    if (t->mContainer == kEEDTC_techtree) {
        long epoch = 0;
        while (r.mPos < r.mSize && !r.mBad) {
            uint32_t n = r.U32();
            const unsigned char* rec = r.Take(size);
            ReadList();
            if (r.mBad)
                break;
            char buf[48];
            snprintf(buf, sizeof(buf), "\n {\"epoch\":%ld,\"rows\":[", epoch++);
            out += buf;
            Row(out, *t, -1, rec, noLead, &list, true, epoch - 1);
            for (uint32_t k = 0; k < n && !r.mBad; k++) {
                rec = r.Take(size);
                ReadList();
                if (!r.mBad)
                    Row(out, *t, (long)k, rec, noLead, &list, false);
            }
            if (out.back() == ',')
                out.pop_back();
            out += "]},";
        }
    } else if (t->mContainer == kEEDTC_fixed || t->mContainer == kEEDTC_events) {
        for (long i = 0; r.mPos < r.mSize && !r.mBad; i++) {
            const unsigned char* rec = r.Take(size);
            if (t->mContainer == kEEDTC_events)
                ReadList();
            if (!r.mBad)
                Row(out, *t, i, rec, noLead, t->mContainer == kEEDTC_events ? &list : nullptr, false);
        }
    } else {
        uint32_t n = r.U32();
        unsigned long leads = t->mContainer == kEEDTC_lead ? LeadCount(*t) : 0;
        for (uint32_t i = 0; i < n && !r.mBad; i++) {
            std::vector<std::pair<const unsigned char*, uint32_t>> lead;
            for (unsigned long k = 0; k < leads; k++) {
                uint32_t len = r.U32();
                const unsigned char* s = r.Take(len);
                lead.emplace_back(s, len);
            }
            const unsigned char* rec = r.Take(size);
            if (!r.mBad)
                Row(out, *t, (long)i, rec, lead, nullptr, false);
        }
    }
    if (r.mBad || r.mPos != r.mSize) {
        outError = std::string(inTable) + ": the db file is truncated or has bytes after the last record";
        return false;
    }
    if (out.back() == ',')
        out.pop_back();
    outJson += out + "\n]";
    return true;
}

}  // namespace EEDataText
