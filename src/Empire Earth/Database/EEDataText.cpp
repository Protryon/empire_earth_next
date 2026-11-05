// EEDataText: compiles the text form of a db table (tools/eedata: CSV or the strict YAML subset) back into the
// binary db file. New unit (not in the original game). Portable C++17: no platform or engine headers.
// UNIT-FLAGS: -EHsc
// The grammar and every rule mirror tools/eedata (textio.py, scalars.py, tables.py, binfmt.py); keep them in step.
// The gate is tools/eedata/eedata.py cppgate: every exported AoC table compiles here to the Python importer's bytes.
#include "EEDataText.h"
#include "EEDataTextSpecs.h"

#include <charconv>
#include "lle/UFromChars.h"
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace EEDataText {
namespace {

struct Failure {
    std::string mText;
};

[[noreturn]] void Fail(const std::string& inText)
{
    throw Failure{inText};
}

std::string Num(long long inValue)
{
    return std::to_string(inValue);
}

// ------------------------------------------------------------------------------------------------ the data model

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

Node Scalar(const std::string& inText, bool inQuoted, long inLine)
{
    Node n;
    n.mKind = Node::kScalar;
    n.mText = inText;
    n.mQuoted = inQuoted;
    n.mLine = inLine;
    return n;
}

// ------------------------------------------------------------------------------------------------ YAML subset

struct Line {
    long mIndent;
    std::string mContent;
    long mNumber;
};

std::string StripComment(const std::string& s)
{
    if (!s.empty() && s[0] == '#')
        return std::string();
    char q = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (q == '"') {
            if (c == '\\')
                i++;
            else if (c == '"')
                q = 0;
        } else if (q == '\'') {
            if (c == '\'')
                q = 0;
        } else if ((c == '"' || c == '\'') && (i == 0 || strchr(" [,:-", s[i - 1]))) {
            q = c;
        } else if (c == '#' && i > 0 && (s[i - 1] == ' ' || s[i - 1] == '\t')) {
            return s.substr(0, i);
        }
    }
    return s;
}

std::string RTrim(std::string s, const char* inChars = " \t")
{
    size_t e = s.find_last_not_of(inChars);
    return e == std::string::npos ? std::string() : s.substr(0, e + 1);
}

std::string Trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos)
        return std::string();
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

std::vector<Line> SplitLines(const std::string& inText)
{
    std::vector<Line> out;
    size_t p = 0;
    long no = 0;
    if (inText.compare(0, 3, "\xEF\xBB\xBF") == 0)
        p = 3;
    while (p <= inText.size()) {
        size_t e = inText.find('\n', p);
        if (e == std::string::npos)
            e = inText.size();
        std::string raw = inText.substr(p, e - p);
        p = e + 1;
        no++;
        if (!raw.empty() && raw.back() == '\r')
            raw.pop_back();
        size_t ind = raw.find_first_not_of(' ');
        if (ind == std::string::npos)
            continue;
        if (raw[ind] == '\t')
            Fail("line " + Num(no) + ": tab in indentation");
        std::string content = RTrim(StripComment(raw.substr(ind)));
        if (content.empty())
            continue;
        if (content == "---" && out.empty())
            continue;
        if (content == "---" || content == "...")
            Fail("line " + Num(no) + ": only one document per file");
        out.push_back(Line{(long)ind, content, no});
        if (e == inText.size())
            break;
    }
    return out;
}

void AppendUtf8(std::string& out, unsigned long cp)
{
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xc0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
        out += (char)(0xe0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3f));
        out += (char)(0x80 | (cp & 0x3f));
    } else {
        out += (char)(0xf0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3f));
        out += (char)(0x80 | ((cp >> 6) & 0x3f));
        out += (char)(0x80 | (cp & 0x3f));
    }
}

int HexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// s[i] is a quote -> the text; i is left after the closing quote
std::string ParseQuoted(const std::string& s, size_t& i, long no)
{
    char q = s[i++];
    std::string out;
    for (;;) {
        if (i >= s.size())
            Fail("line " + Num(no) + ": unterminated string");
        char c = s[i];
        if (q == '\'') {
            if (c == '\'') {
                if (i + 1 < s.size() && s[i + 1] == '\'') {
                    out += '\'';
                    i += 2;
                    continue;
                }
                i++;
                return out;
            }
            out += c;
            i++;
            continue;
        }
        if (c == '"') {
            i++;
            return out;
        }
        if (c == '\\') {
            char e = i + 1 < s.size() ? s[i + 1] : 0;
            const char* simple = "\\\"/0abtnvfre ";
            const char* value = "\\\"/\0\a\b\t\n\v\f\r\x1b ";
            const char* hit = e ? strchr(simple, e) : nullptr;
            if (hit) {
                out += value[hit - simple];
                i += 2;
            } else if (e == 'x' || e == 'u' || e == 'U') {
                size_t n = e == 'x' ? 2 : e == 'u' ? 4 : 8;
                unsigned long cp = 0;
                if (i + 2 + n > s.size())
                    Fail("line " + Num(no) + ": bad \\" + std::string(1, e) + " escape");
                for (size_t k = 0; k < n; k++) {
                    int d = HexDigit(s[i + 2 + k]);
                    if (d < 0)
                        Fail("line " + Num(no) + ": bad \\" + std::string(1, e) + " escape");
                    cp = cp * 16 + d;
                }
                if (cp > 0x10ffff)
                    Fail("line " + Num(no) + ": escape out of range");
                AppendUtf8(out, cp);
                i += 2 + n;
            } else {
                Fail("line " + Num(no) + ": unknown escape \\" + std::string(1, e));
            }
            continue;
        }
        out += c;
        i++;
    }
}

void CheckPlain(const std::string& t, long no)
{
    if (t.empty())
        return;
    if (strchr("&*!|>%@`{}", t[0]))
        Fail("line " + Num(no) + ": " + t.substr(0, 20) +
             ": anchors, aliases, tags, block scalars and flow mappings are not allowed");
    if (t.find(": ") != std::string::npos || t.back() == ':')
        Fail("line " + Num(no) + ": " + t.substr(0, 30) + ": a mapping inside a value (quote the string)");
}

Node ParseFlow(const std::string& s, size_t& i, long no)
{
    Node out;
    out.mKind = Node::kList;
    out.mLine = no;
    i++;
    bool expectItem = true;
    for (;;) {
        while (i < s.size() && s[i] == ' ')
            i++;
        if (i >= s.size())
            Fail("line " + Num(no) + ": unterminated [ (flow sequences must close on their line)");
        char c = s[i];
        if (c == ']') {
            if (expectItem && !out.mItems.empty())
                Fail("line " + Num(no) + ": trailing comma in a flow sequence");
            i++;
            return out;
        }
        if (!expectItem) {
            if (c != ',')
                Fail("line " + Num(no) + ": expected , or ] in a flow sequence");
            i++;
            expectItem = true;
            continue;
        }
        if (c == '[') {
            out.mItems.push_back(ParseFlow(s, i, no));
        } else if (c == '"' || c == '\'') {
            std::string t = ParseQuoted(s, i, no);
            out.mItems.push_back(Scalar(t, true, no));
        } else {
            size_t j = i;
            while (j < s.size() && s[j] != ',' && s[j] != ']')
                j++;
            std::string t = Trim(s.substr(i, j - i));
            if (t.empty())
                Fail("line " + Num(no) + ": empty item in a flow sequence");
            if (t[0] == '{' || t[0] == '[')
                Fail("line " + Num(no) + ": flow mappings are not allowed");
            CheckPlain(t, no);
            out.mItems.push_back(Scalar(t, false, no));
            i = j;
        }
        expectItem = false;
    }
}

Node ParseValue(const std::string& s, long no)
{
    if (s == "{}") {
        Node n;
        n.mKind = Node::kMap;
        n.mLine = no;
        return n;
    }
    size_t i = 0;
    if (s[0] == '[') {
        Node v = ParseFlow(s, i, no);
        if (!Trim(s.substr(i)).empty())
            Fail("line " + Num(no) + ": text after ]");
        return v;
    }
    if (s[0] == '"' || s[0] == '\'') {
        std::string t = ParseQuoted(s, i, no);
        if (!Trim(s.substr(i)).empty())
            Fail("line " + Num(no) + ": text after the closing quote");
        return Scalar(t, true, no);
    }
    CheckPlain(s, no);
    return Scalar(s, false, no);
}

// "key: rest" / "key:" -> true with the key and the rest; false when s is not a mapping entry
bool SplitKey(const std::string& s, std::string& outKey, std::string& outRest, long no)
{
    if (s[0] == '"' || s[0] == '\'') {
        size_t i = 0;
        std::string k = ParseQuoted(s, i, no);
        if (i >= s.size() || s[i] != ':' || (i + 1 < s.size() && s[i + 1] != ' '))
            return false;
        outKey = k;
        outRest = Trim(s.substr(i + 1));
        return true;
    }
    if (s[0] == '[' || s[0] == '{' || (s[0] == '-' && !(s.size() > 1 && s[1] != ' ')))
        return false;
    // ^([^:#]*?):(?: (.*))?$
    size_t c = s.find(':');
    if (c == std::string::npos || s.find('#') < c)
        return false;
    if (c + 1 < s.size() && s[c + 1] != ' ')
        return false;
    std::string k = Trim(s.substr(0, c));
    if (k.empty())
        return false;
    outKey = k;
    outRest = c + 1 < s.size() ? Trim(s.substr(c + 2)) : std::string();
    return true;
}

bool IsSeq(const std::string& c)
{
    return c == "-" || c.compare(0, 2, "- ") == 0;
}

struct YamlParser {
    const std::vector<Line>& mItems;
    explicit YamlParser(const std::vector<Line>& inItems) : mItems(inItems) {}

    Node Block(size_t& i, long indent)
    {
        if (IsSeq(this->mItems[i].mContent))
            return this->Seq(i, indent);
        return this->Map(i, indent, nullptr);
    }

    Node Map(size_t& i, long indent, const std::string* first)
    {
        Node out;
        out.mKind = Node::kMap;
        out.mLine = i < this->mItems.size() ? this->mItems[i].mNumber : 0;
        while (i < this->mItems.size()) {
            const Line& ln = this->mItems[i];
            long ind = first ? indent : ln.mIndent;
            const std::string& content = first ? *first : ln.mContent;
            long no = ln.mNumber;
            if (!first && ind < indent)
                break;
            if (ind != indent)
                Fail("line " + Num(no) + ": unexpected indentation");
            if (!first && IsSeq(content))
                break;
            first = nullptr;
            std::string k, rest;
            if (!SplitKey(content, k, rest, no))
                Fail("line " + Num(no) + ": expected \"key: value\"");
            if (out.Find(k))
                Fail("line " + Num(no) + ": duplicate key '" + k + "'");
            i++;
            out.mKeys.push_back(k);
            if (!rest.empty()) {
                out.mItems.push_back(ParseValue(rest, no));
            } else if (i < this->mItems.size() &&
                       (this->mItems[i].mIndent > indent ||
                        (this->mItems[i].mIndent == indent && IsSeq(this->mItems[i].mContent)))) {
                long sub = this->mItems[i].mIndent;
                out.mItems.push_back(this->Block(i, sub));
            } else {
                out.mItems.push_back(Scalar("", false, no));
            }
        }
        return out;
    }

    Node Seq(size_t& i, long indent)
    {
        Node out;
        out.mKind = Node::kList;
        out.mLine = this->mItems[i].mNumber;
        while (i < this->mItems.size()) {
            const Line& ln = this->mItems[i];
            if (ln.mIndent < indent || (ln.mIndent == indent && !IsSeq(ln.mContent)))
                break;
            if (ln.mIndent != indent)
                Fail("line " + Num(ln.mNumber) + ": unexpected indentation");
            size_t r = ln.mContent.find_first_not_of(' ', 1);
            std::string rest = r == std::string::npos ? std::string() : ln.mContent.substr(r);
            if (rest.empty()) {
                long no = ln.mNumber;
                i++;
                if (i < this->mItems.size() && this->mItems[i].mIndent > indent) {
                    long sub = this->mItems[i].mIndent;
                    out.mItems.push_back(this->Block(i, sub));
                } else {
                    out.mItems.push_back(Scalar("", false, no));
                }
                continue;
            }
            long sub = indent + (long)(ln.mContent.size() - rest.size());
            if (IsSeq(rest))
                Fail("line " + Num(ln.mNumber) + ": nested \"- -\" sequences are not allowed");
            std::string k, v;
            if (SplitKey(rest, k, v, ln.mNumber)) {
                std::string first = rest;
                out.mItems.push_back(this->Map(i, sub, &first));
                continue;
            }
            out.mItems.push_back(ParseValue(rest, ln.mNumber));
            i++;
        }
        return out;
    }
};

Node ParseYaml(const std::string& inText)
{
    std::vector<Line> items = SplitLines(inText);
    if (items.empty()) {
        Node n;
        n.mKind = Node::kMap;
        return n;
    }
    YamlParser p(items);
    size_t i = 0;
    Node n = p.Block(i, items[0].mIndent);
    if (i != items.size())
        Fail("line " + Num(items[i].mNumber) + ": unexpected indentation");
    return n;
}

// ------------------------------------------------------------------------------------------------ CSV

struct Csv {
    std::map<std::string, std::string> mDirectives;
    std::vector<std::string> mHeader;
    std::vector<std::vector<std::string>> mRows;
    std::vector<long> mRowLines;   // the line each row starts on
};

Csv ParseCsv(const std::string& inText)
{
    Csv out;
    size_t p = inText.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    long no = 0;
    // directives and blank lines before the header
    while (p < inText.size()) {
        size_t e = inText.find('\n', p);
        if (e == std::string::npos)
            e = inText.size();
        std::string s = RTrim(inText.substr(p, e - p), "\r");
        bool blank = Trim(s).empty();
        if (!blank && s[0] != '#')
            break;
        no++;
        if (!blank && s.size() > 1 && s[1] != ' ' && s[1] != '#') {
            size_t sp = s.find(' ');
            std::string name = s.substr(1, sp == std::string::npos ? std::string::npos : sp - 1);
            std::string value = sp == std::string::npos ? std::string() : Trim(s.substr(sp + 1));
            if (out.mDirectives.count(name))
                Fail("line " + Num(no) + ": directive #" + name + " twice");
            out.mDirectives[name] = value;
        }
        p = e + 1;
    }
    // RFC 4180 records (as Python's csv module reads them: a quote inside an unquoted field is a character)
    std::vector<std::vector<std::string>> rows;
    std::vector<long> starts;
    long line = no + 1;
    while (p < inText.size()) {
        long start = line;
        std::vector<std::string> row;
        std::string cell;
        bool any = false;
        for (;;) {
            if (p < inText.size() && inText[p] == '"') {
                any = true;
                p++;
                for (;;) {
                    if (p >= inText.size())
                        Fail("CSV: unterminated quoted field");
                    if (inText[p] == '"') {
                        if (p + 1 < inText.size() && inText[p + 1] == '"') {
                            cell += '"';
                            p += 2;
                            continue;
                        }
                        p++;
                        break;
                    }
                    if (inText[p] == '\n')
                        line++;
                    cell += inText[p++];
                }
                if (p < inText.size() && inText[p] != ',' && inText[p] != '\n' &&
                    !(inText[p] == '\r' && p + 1 < inText.size() && inText[p + 1] == '\n') && inText[p] != '\r')
                    Fail("CSV: text after a closing quote");
            } else {
                while (p < inText.size() && inText[p] != ',' && inText[p] != '\n' &&
                       !(inText[p] == '\r' && (p + 1 >= inText.size() || inText[p + 1] == '\n'))) {
                    cell += inText[p++];
                    any = true;
                }
            }
            row.push_back(cell);
            cell.clear();
            if (p < inText.size() && inText[p] == ',') {
                p++;
                any = true;
                continue;
            }
            if (p < inText.size() && inText[p] == '\r')
                p++;
            if (p < inText.size() && inText[p] == '\n') {
                p++;
                line++;
            }
            break;
        }
        if (any) {
            rows.push_back(row);
            starts.push_back(start);
        }
    }
    if (rows.empty())
        Fail("CSV: no header row");
    out.mHeader = rows[0];
    for (size_t i = 0; i < out.mHeader.size(); i++)
        for (size_t j = 0; j < i; j++)
            if (out.mHeader[i] == out.mHeader[j])
                Fail("CSV: duplicate column names");
    for (size_t i = 1; i < rows.size(); i++) {
        if (rows[i].size() != out.mHeader.size())
            Fail("line " + Num(starts[i]) + ": " + Num(rows[i].size()) + " cells, the header has " +
                 Num(out.mHeader.size()));
        out.mRows.push_back(rows[i]);
        out.mRowLines.push_back(starts[i]);
    }
    return out;
}

// ------------------------------------------------------------------------------------------------ scalars

const unsigned long kTypeSize[] = {1, 1, 1, 2, 2, 4, 4, 4, 8, 1};

void IntRange(unsigned char t, long long& lo, long long& hi, int& width)
{
    switch (t) {
    case kEEDT_s8: lo = -128; hi = 127; width = 8; break;
    case kEEDT_u8: case kEEDT_bool: lo = 0; hi = 255; width = 8; break;
    case kEEDT_s16: lo = -32768; hi = 32767; width = 16; break;
    case kEEDT_u16: lo = 0; hi = 65535; width = 16; break;
    case kEEDT_s32: lo = -2147483648LL; hi = 2147483647LL; width = 32; break;
    default: lo = 0; hi = 4294967295LL; width = 32; break;
    }
}

bool IsHexInt(const std::string& s)
{
    if (s.size() < 3 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
        return false;
    for (size_t i = 2; i < s.size(); i++)
        if (HexDigit(s[i]) < 0)
            return false;
    return true;
}

long long ParseInt(const std::string& inText, unsigned char t)
{
    std::string s = Trim(inText);
    long long lo, hi;
    int width;
    IntRange(t, lo, hi, width);
    if (t == kEEDT_bool && (s == "true" || s == "false"))
        return s == "true";
    if (IsHexInt(s)) {
        unsigned long long v = 0;
        for (size_t i = 2; i < s.size(); i++) {
            v = v * 16 + HexDigit(s[i]);
            if (v >= (1ULL << width))
                Fail("'" + inText + "' does not fit " + "the field");
        }
        long long r = (long long)v;
        if (lo < 0 && r > hi)
            r -= (1LL << width);
        return r;
    }
    size_t i = 0;
    bool neg = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
        neg = s[i++] == '-';
    if (i >= s.size())
        Fail("'" + inText + "' is not an integer");
    long long v = 0;
    for (; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9')
            Fail("'" + inText + "' is not an integer");
        v = v * 10 + (s[i] - '0');
        if (v > 5000000000LL)
            Fail("'" + inText + "' out of range");
    }
    if (neg)
        v = -v;
    if (v < lo || v > hi)
        Fail("'" + inText + "' out of range");
    return v;
}

bool IsDecimal(const std::string& s)
{
    // ^[+-]?([0-9]+\.?[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?$
    size_t i = 0, n = s.size();
    if (i < n && (s[i] == '+' || s[i] == '-'))
        i++;
    size_t d0 = i;
    while (i < n && s[i] >= '0' && s[i] <= '9')
        i++;
    bool intDigits = i > d0;
    if (i < n && s[i] == '.') {
        i++;
        size_t f0 = i;
        while (i < n && s[i] >= '0' && s[i] <= '9')
            i++;
        if (!intDigits && i == f0)
            return false;
    } else if (!intDigits) {
        return false;
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < n && (s[i] == '+' || s[i] == '-'))
            i++;
        size_t e0 = i;
        while (i < n && s[i] >= '0' && s[i] <= '9')
            i++;
        if (i == e0)
            return false;
    }
    return i == n;
}

uint32_t ParseF32(const std::string& inText)
{
    std::string s = Trim(inText);
    std::string low;
    for (char c : s)
        low += (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
    if (low == ".inf" || low == "+.inf" || low == "inf" || low == "+inf" || low == "infinity")
        return 0x7f800000u;
    if (low == "-.inf" || low == "-inf" || low == "-infinity")
        return 0xff800000u;
    if (low == ".nan" || low == "nan")
        return 0x7fc00000u;
    if (IsHexInt(s)) {
        unsigned long long v = 0;
        for (size_t i = 2; i < s.size(); i++) {
            v = v * 16 + HexDigit(s[i]);
            if (v >> 32)
                Fail("'" + inText + "': more than 32 bits");
        }
        return (uint32_t)v;
    }
    if (!IsDecimal(s))
        Fail("'" + inText + "' is not a number");
    bool neg = s[0] == '-';
    std::string body = (s[0] == '+' || s[0] == '-') ? s.substr(1) : s;
    float f = 0.0f;
    std::from_chars_result r = UFromChars(body.data(), body.data() + body.size(), f);
    if (r.ec == std::errc::result_out_of_range)
        Fail("'" + inText + "' is outside the float range");
    if (r.ec != std::errc() || r.ptr != body.data() + body.size())
        Fail("'" + inText + "' is not a number");
    uint32_t bits;
    memcpy(&bits, &f, 4);
    if (bits == 0x7f800000u)
        Fail("'" + inText + "' is outside the float range");
    if (bits == 0) {
        for (char c : body) {
            if (c >= '1' && c <= '9')
                Fail("'" + inText + "' is outside the float range");
            if (c == 'e' || c == 'E')
                break;
        }
    }
    return neg ? bits | 0x80000000u : bits;
}

// UTF-8 text -> CP-1252 bytes (EEDataTextSpecs.h's code page, inverted)
std::string EncodeText(const std::string& inText)
{
    static std::map<unsigned long, unsigned char> sInverse;
    if (sInverse.empty())
        for (int b = 0; b < 256; b++)
            sInverse[kEEDTCodePage[b]] = (unsigned char)b;
    std::string out;
    for (size_t i = 0; i < inText.size();) {
        unsigned char c = (unsigned char)inText[i];
        unsigned long cp;
        int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; n = 2; }
        else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; n = 3; }
        else if ((c & 0xf8) == 0xf0) { cp = c & 0x07; n = 4; }
        else Fail("not UTF-8 text");
        if (i + n > inText.size())
            Fail("not UTF-8 text");
        for (int k = 1; k < n; k++) {
            unsigned char d = (unsigned char)inText[i + k];
            if ((d & 0xc0) != 0x80)
                Fail("not UTF-8 text");
            cp = (cp << 6) | (d & 0x3f);
        }
        i += n;
        std::map<unsigned long, unsigned char>::const_iterator it = sInverse.find(cp);
        if (it == sInverse.end())
            Fail("character U+" + Num((long long)cp) + " has no CP-1252 byte");
        out += (char)it->second;
    }
    return out;
}

// ------------------------------------------------------------------------------------------------ records

struct Getter {
    const Node& mMap;
    std::string mWhere;
    std::vector<bool> mUsed;
    size_t mCursor = 0;
    Getter(const Node& inMap, const std::string& inWhere) : mMap(inMap), mWhere(inWhere), mUsed(inMap.mKeys.size()) {}

    // the value, or nullptr when the key is absent or holds a null
    const Node* Get(const std::string& inKey)
    {
        size_t n = this->mMap.mKeys.size();
        for (size_t k = 0; k < n; k++) {
            size_t i = (this->mCursor + k) % n;
            if (this->mMap.mKeys[i] == inKey) {
                this->mUsed[i] = true;
                this->mCursor = i + 1;
                const Node& v = this->mMap.mItems[i];
                return v.IsMissing() ? nullptr : &v;
            }
        }
        return nullptr;
    }
    void Leftover()
    {
        std::string extra;
        int count = 0;
        for (size_t i = 0; i < this->mUsed.size(); i++)
            if (!this->mUsed[i] && count++ < 5)
                extra += (extra.empty() ? "" : ", ") + this->mMap.mKeys[i];
        if (count)
            Fail(this->mWhere + ": unknown key(s) " + extra);
    }
};

// an array value -> n element pointers (nullptr = missing)
std::vector<const Node*> Elems(const Node* v, size_t n, const std::string& where, const char* name)
{
    std::vector<const Node*> out(n, nullptr);
    if (!v)
        return out;
    if (v->mKind != Node::kList)
        Fail(where + ": " + name + " must be a list");
    if (v->mItems.size() > n)
        Fail(where + ": " + name + " has " + Num(v->mItems.size()) + " elements, the field has " + Num(n));
    for (size_t i = 0; i < v->mItems.size(); i++)
        out[i] = v->mItems[i].IsMissing() ? nullptr : &v->mItems[i];
    return out;
}

const std::string& ScalarText(const Node* v, const std::string& where, const char* name, const char* what)
{
    if (v->mKind != Node::kScalar)
        Fail(where + ": " + name + ": a list where " + what + " belongs");
    return v->mText;
}

void PutLE(std::vector<unsigned char>& buf, size_t off, uint32_t v, unsigned long size)
{
    for (unsigned long k = 0; k < size; k++)
        buf[off + k] = (unsigned char)(v >> (8 * k));
}

uint32_t Number(const EEDTField& f, unsigned char type, const Node* v, const std::string& where)
{
    if (!v)
        return 0;
    try {
        const std::string& t = ScalarText(v, where, f.mName, "a number");
        if (type == kEEDT_f32)
            return ParseF32(t);
        return (uint32_t)ParseInt(t, type);
    } catch (Failure& e) {
        if (e.mText.compare(0, where.size(), where) == 0)
            throw;
        Fail(where + ": " + f.mName + ": " + e.mText);
    }
}

std::string Text(const Node* v, const std::string& where, const char* name)
{
    if (!v)
        return std::string();
    const std::string& t = ScalarText(v, where, name, "a string");
    try {
        return EncodeText(t);
    } catch (Failure& e) {
        Fail(where + ": " + name + ": " + e.mText);
    }
}

// a record's bytes from the spec (0: all zero)
std::vector<unsigned char> Record(const char* inBytes, unsigned long inSize)
{
    std::vector<unsigned char> out(inSize, 0);
    if (inBytes)
        memcpy(out.data(), inBytes, inSize);
    return out;
}

// an id: a lowercase letter, then a-z, 0-9 or _ (tools/eedata/ids.py ID_RE)
bool IsId(const std::string& s)
{
    if (s.empty() || s[0] < 'a' || s[0] > 'z')
        return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

const EEDTTable* FindTable(const char* inTable)
{
    std::string want;
    for (const char* p = inTable; *p; p++)
        want += (char)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
    for (unsigned long i = 0; i < kEEDTTableCount; i++)
        if (want == kEEDTTables[i].mName)
            return &kEEDTTables[i];
    return nullptr;
}

const EEDTRef* FindRef(const EEDTTable& t, const char* inField)
{
    for (unsigned long i = 0; i < t.mRefCount; i++)
        if (strcmp(t.mRefs[i].mField, inField) == 0)
            return &t.mRefs[i];
    return nullptr;
}

const EEDTField* FindField(const EEDTStruct& st, const char* inName)
{
    for (unsigned long i = 0; i < st.mFieldCount; i++)
        if (strcmp(st.mFields[i].mName, inName) == 0)
            return &st.mFields[i];
    return nullptr;
}

// ------------------------------------------------------------------------------------------------ the parsed text

// a listed record: its position (the row's key, or its slot in an epoch block), its mapping and line
struct Placed {
    long mPos;
    const Node* mMap;
    long mLine;
};

// one epoch of the tech tree: its epochs.yaml entry and the dbtechtree.yaml techs placed in its block's slots
struct EpochBlock {
    const Node* mEpoch;
    long mLine;
    long mSlots;
    std::vector<Placed> mNodes;
};

// a table's text, parsed once: the records placed at their positions (dbtechtree and epochs: placed together, by
// Resolver::GetLayout)
struct Doc {
    Node mDoc;
    Node mCsvRecords;
    long mTotal = 0;
    bool mTechtree = false;
    std::vector<Placed> mRows;
};

// the ids of a table: id -> key, and which keys are listed
struct Index {
    std::map<std::string, long> mKeys;
    std::map<long, std::string> mIds;
};

bool Present(const Node* v)
{
    return v && !(v->mKind == Node::kScalar && (v->mText.empty() || (!v->mQuoted && (v->mText == "~" ||
                                                                                       v->mText == "null"))));
}

long HeadInt(const Node* v, const char* what, const std::string& where)
{
    if (v->mKind != Node::kScalar)
        Fail(where + ": " + what + ": not a number");
    try {
        return (long)(uint32_t)ParseInt(v->mText, kEEDT_u32);
    } catch (Failure& e) {
        Fail(where + ": " + what + ": " + e.mText);
    }
}

// the records of a list -> positions in file order: an explicit row must be after the previous explicit one, a
// record without one takes the next free position above every explicit row and the count (new rows go above)
std::vector<Placed> PlaceList(const std::vector<const Node*>& items, const std::vector<long>& lines, long total,
                              const std::string& where)
{
    std::vector<Placed> out;
    std::vector<long> pos(items.size(), -1);
    long next = 0;
    for (size_t k = 0; k < items.size(); k++) {
        const Node& m = *items[k];
        if (m.mKind != Node::kMap)
            Fail(where + " item " + Num(k) + ": not a mapping");
        long line = lines[k];
        const Node* v = m.Find("row");
        if (!v || v->IsMissing())
            continue;
        if (v->mKind != Node::kScalar)
            Fail(where + " line " + Num(line) + ": row: not a number");
        try {
            pos[k] = (long)ParseInt(v->mText, kEEDT_s32);
        } catch (Failure& e) {
            Fail(where + " line " + Num(line) + ": row: " + e.mText);
        }
        if (pos[k] < next)
            Fail(where + " line " + Num(line) + ": row " + Num(pos[k]) + " is not after the previous row (" +
                 Num(next - 1) + ")");
        next = pos[k] + 1;
    }
    long free = total > next ? total : next;
    for (size_t k = 0; k < items.size(); k++) {
        long p = pos[k];
        if (p < 0)
            p = free++;
        out.push_back(Placed{p, items[k], lines[k]});
    }
    return out;
}

std::vector<Placed> PlaceRecords(const Node* records, long total, const std::string& where,
                                 const std::vector<long>* lines)
{
    if (!records)
        return std::vector<Placed>();
    if (records->mKind != Node::kList)
        Fail(where + ": must be a list");
    std::vector<const Node*> items;
    std::vector<long> theLines;
    for (size_t k = 0; k < records->mItems.size(); k++) {
        items.push_back(&records->mItems[k]);
        theLines.push_back(lines ? (*lines)[k] : records->mItems[k].mLine);
    }
    return PlaceList(items, theLines, total, where);
}

// CSV columns the exporter can write for a table (tables.csv_columns)
std::vector<std::string> CsvColumns(const EEDTTable& t);
bool SplitColumn(const std::string& c, std::string& base, std::vector<unsigned long>& idx);
Node CsvRecord(const std::vector<std::string>& bases, const std::vector<std::vector<unsigned long>>& idx,
               const std::vector<std::string>& cells, long line);

void ParseDoc(const EEDTTable& t, const std::string& inText, Doc& d)
{
    std::string name = t.mName;
    const Node* head[3] = {};   // eedata format rows
    static const char* const kHead[3] = {"eedata", "format", "rows"};
    std::vector<Node> headNodes;
    std::vector<long> csvLines;
    if (t.mYaml) {
        d.mDoc = ParseYaml(inText);
        if (d.mDoc.mKind != Node::kMap)
            Fail(name + ": the document is not a mapping");
        for (const std::string& k : d.mDoc.mKeys) {
            bool ok = k == "records" || k == "epochs";
            for (const char* h : kHead)
                ok = ok || k == h;
            if (!ok)
                Fail(name + ": unknown top-level key " + k);
        }
        for (int i = 0; i < 3; i++)
            head[i] = d.mDoc.Find(kHead[i]);
    } else {
        Csv csv = ParseCsv(inText);
        for (const std::pair<const std::string, std::string>& dir : csv.mDirectives) {
            bool ok = false;
            for (const char* h : kHead)
                ok = ok || dir.first == h;
            if (!ok)
                Fail(name + ": unknown directive " + dir.first);
        }
        headNodes.resize(3);
        for (int i = 0; i < 3; i++) {
            std::map<std::string, std::string>::const_iterator it = csv.mDirectives.find(kHead[i]);
            if (it != csv.mDirectives.end()) {
                headNodes[i] = Scalar(it->second, true, 0);
                head[i] = &headNodes[i];
            }
        }
        std::vector<std::string> known = CsvColumns(t);
        std::vector<std::string> bases;
        std::vector<std::vector<unsigned long>> idx;
        for (const std::string& c : csv.mHeader) {
            bool ok = false;
            for (const std::string& k : known)
                ok = ok || k == c;
            std::string base;
            std::vector<unsigned long> ix;
            if (!ok || !SplitColumn(c, base, ix))
                Fail(name + ": unknown column " + c);
            bases.push_back(base);
            idx.push_back(ix);
        }
        d.mCsvRecords.mKind = Node::kList;
        for (size_t r = 0; r < csv.mRows.size(); r++)
            d.mCsvRecords.mItems.push_back(CsvRecord(bases, idx, csv.mRows[r], csv.mRowLines[r]));
        csvLines = csv.mRowLines;
    }
    if (head[0] && head[0]->mKind == Node::kScalar && head[0]->mText != name)
        Fail(name + ": the file says it is '" + head[0]->mText + "'");
    if (head[1] && (head[1]->mKind != Node::kScalar || head[1]->mText != Num(EEDT_FORMAT_VERSION)))
        Fail(name + ": format " + (head[1]->mKind == Node::kScalar ? head[1]->mText : std::string("?")) +
             ", this loader reads format " + Num(EEDT_FORMAT_VERSION));
    d.mTotal = Present(head[2]) ? HeadInt(head[2], "rows", name) : 0;
    if (t.mContainer == kEEDTC_techtree || t.mContainer == kEEDTC_epochs) {
        d.mTechtree = true;   // placed together with the other text (Resolver::GetLayout)
        for (const std::string& k : d.mDoc.mKeys)
            if (k == "epochs" || k == "rows")
                Fail(name + ": unknown top-level key " + k);
        const Node* records = d.mDoc.Find("records");
        if (records && !records->IsMissing() && records->mKind != Node::kList)
            Fail(name + ": records must be a list");
        if (records && records->mKind == Node::kList)
            for (const Node& m : records->mItems)
                if (m.mKind != Node::kMap)
                    Fail(name + ": records must be a list of mappings");
        return;
    }
    const Node* records = t.mYaml ? d.mDoc.Find("records") : &d.mCsvRecords;
    if (records && records->IsMissing())
        records = nullptr;
    d.mRows = PlaceRecords(records, d.mTotal, name, t.mYaml ? nullptr : &csvLines);
}

// ------------------------------------------------------------------------------------------------ records

struct Row {
    std::vector<unsigned char> mRec;
    std::vector<std::string> mLead;
    std::vector<uint32_t> mList;
    bool mHole = true;
};

class Resolver;

// the record: the struct's base (zero + the defaults of the fields the game never reads), then every field the
// text carries (a missing one is zero; a missing self field is the row's key + offset; a reference is resolved)
std::vector<unsigned char> BuildRecord(const EEDTTable& t, Getter& g, Resolver& r, bool inHasKey, long inKey);

// "A|B[n]" -> A, B, n (B empty for "A[n]"; n 0 for a single string)
void LeadSpec(const char* inSpec, std::string& a, std::string& b, unsigned long& n)
{
    std::string s = inSpec;
    size_t bar = s.find('|'), br = s.find('[');
    n = br == std::string::npos ? 0 : strtoul(s.c_str() + br + 1, nullptr, 10);
    a = s.substr(0, bar != std::string::npos ? bar : br);
    b = bar != std::string::npos ? s.substr(bar + 1, br - bar - 1) : std::string();
}

unsigned long LeadCount(const EEDTTable& t)
{
    unsigned long total = 0;
    for (unsigned long i = 0; i < t.mLeadCount; i++) {
        std::string a, b;
        unsigned long n;
        LeadSpec(t.mLead[i], a, b, n);
        total += n == 0 ? 1 : (b.empty() ? n : 2 * n);
    }
    return total;
}

std::vector<std::string> BuildLead(const EEDTTable& t, Getter& g)
{
    std::vector<std::string> out;
    for (unsigned long i = 0; i < t.mLeadCount; i++) {
        std::string a, b;
        unsigned long n;
        LeadSpec(t.mLead[i], a, b, n);
        if (n == 0) {
            out.push_back(Text(g.Get(a), g.mWhere, a.c_str()));
        } else if (b.empty()) {
            std::vector<const Node*> e = Elems(g.Get(a), n, g.mWhere, a.c_str());
            for (unsigned long k = 0; k < n; k++)
                out.push_back(Text(e[k], g.mWhere, a.c_str()));
        } else {
            std::vector<const Node*> ea = Elems(g.Get(a), n, g.mWhere, a.c_str());
            std::vector<const Node*> eb = Elems(g.Get(b), n, g.mWhere, b.c_str());
            for (unsigned long k = 0; k < n; k++) {
                out.push_back(Text(ea[k], g.mWhere, a.c_str()));
                out.push_back(Text(eb[k], g.mWhere, b.c_str()));
            }
        }
    }
    return out;
}

void Put32(std::vector<unsigned char>& out, uint32_t v)
{
    for (int k = 0; k < 4; k++)
        out.push_back((unsigned char)(v >> (8 * k)));
}

struct Writer {
    const EEDTTable& mTable;
    const std::vector<unsigned char>& mHole;
    std::vector<unsigned char>& mOut;
    unsigned long mLeadCount;

    void Record(const Row& r)
    {
        const std::vector<unsigned char>& rec = r.mHole ? this->mHole : r.mRec;
        this->mOut.insert(this->mOut.end(), rec.begin(), rec.end());
    }
    void List(const Row& r)
    {
        Put32(this->mOut, (uint32_t)r.mList.size());
        for (uint32_t v : r.mList)
            Put32(this->mOut, v);
    }
    void Lead(const Row& r)
    {
        for (unsigned long k = 0; k < this->mLeadCount; k++) {
            const std::string empty;
            const std::string& s = r.mHole ? empty : r.mLead[k];
            Put32(this->mOut, (uint32_t)s.size());
            this->mOut.insert(this->mOut.end(), s.begin(), s.end());
        }
    }
};

// CSV columns the exporter can write for a table (tables.csv_columns)
std::vector<std::string> CsvColumns(const EEDTTable& t)
{
    std::vector<std::string> cols{"row", "id"};
    for (unsigned long i = 0; i < t.mLeadCount; i++) {
        std::string a, b;
        unsigned long n;
        LeadSpec(t.mLead[i], a, b, n);
        if (n == 0)
            cols.push_back(a);
        for (unsigned long k = 0; k < n; k++)
            cols.push_back(a + "[" + Num(k) + "]");
        if (!b.empty())
            for (unsigned long k = 0; k < n; k++)
                cols.push_back(b + "[" + Num(k) + "]");
    }
    const EEDTStruct& st = *t.mRecord;
    for (unsigned long fi = 0; fi < st.mFieldCount; fi++) {
        const EEDTField& f = st.mFields[fi];
        std::string name = f.mName;
        if (f.mType == kEEDT_str || f.mCount == 1) {
            cols.push_back(name);
        } else if (f.mType == kEEDT_pair_f32) {
            for (unsigned long k = 0; k < f.mCount; k++)
                for (int j = 0; j < 2; j++)
                    cols.push_back(name + "[" + Num(k) + "][" + Num(j) + "]");
        } else {
            for (unsigned long k = 0; k < f.mCount; k++)
                cols.push_back(name + "[" + Num(k) + "]");
        }
    }
    return cols;
}

// a CSV column name -> base and indices ("mDamage[3]" -> mDamage, {3})
bool SplitColumn(const std::string& c, std::string& base, std::vector<unsigned long>& idx)
{
    size_t b = c.find('[');
    base = c.substr(0, b);
    if (base.empty() || base.find(']') != std::string::npos)
        return false;
    idx.clear();
    while (b != std::string::npos && b < c.size()) {
        size_t e = c.find(']', b);
        if (c[b] != '[' || e == std::string::npos || e == b + 1)
            return false;
        for (size_t k = b + 1; k < e; k++)
            if (c[k] < '0' || c[k] > '9')
                return false;
        idx.push_back(strtoul(c.c_str() + b + 1, nullptr, 10));
        b = e + 1 < c.size() ? e + 1 : std::string::npos;
    }
    return true;
}

// one CSV row -> the same mapping the YAML side gives (arrays as lists with null gaps)
Node CsvRecord(const std::vector<std::string>& bases, const std::vector<std::vector<unsigned long>>& idx,
               const std::vector<std::string>& cells, long line)
{
    Node m;
    m.mKind = Node::kMap;
    m.mLine = line;
    for (size_t c = 0; c < cells.size(); c++) {
        if (cells[c].empty())
            continue;
        Node* slot = nullptr;
        for (size_t k = 0; k < m.mKeys.size(); k++)
            if (m.mKeys[k] == bases[c])
                slot = &m.mItems[k];
        if (!slot) {
            m.mKeys.push_back(bases[c]);
            m.mItems.push_back(Node());
            slot = &m.mItems.back();
        }
        for (unsigned long i : idx[c]) {
            slot->mKind = Node::kList;
            if (slot->mItems.size() <= i)
                slot->mItems.resize(i + 1);
            slot = &slot->mItems[i];
        }
        *slot = Scalar(cells[c], true, line);
    }
    return m;
}

// ------------------------------------------------------------------------------------------------ the resolver

// Every table's parsed text, the ids of each and the references between them (tools/eedata tables.Content). The
// single place where tables meet: a later mod layer feeds it merged entries instead of files.
class Resolver {
public:
    explicit Resolver(const std::function<bool(const char*, std::string&)>& inRead) : mRead(inRead) {}

    Doc& GetDoc(const EEDTTable& t)
    {
        std::map<std::string, std::unique_ptr<Doc>>::iterator it = this->mDocs.find(t.mName);
        if (it != this->mDocs.end())
            return *it->second;
        std::string text;
        if (!this->mRead(t.mName, text))
            Fail(std::string(t.mName) + ": referenced, but the table has no text form here");
        std::unique_ptr<Doc> d(new Doc);
        ParseDoc(t, text, *d);
        Doc& out = *d;
        this->mDocs[t.mName] = std::move(d);
        return out;
    }

    // a record's key: its position, or its key field (dbtechtree mID)
    long Key(const EEDTTable& t, const Placed& p, const std::string& where)
    {
        if (!t.mKeyField)
            return p.mPos;
        const EEDTField* f = FindField(*t.mRecord, t.mKeyField);
        const Node* v = p.mMap->Find(t.mKeyField);
        return (long)(int32_t)Number(*f, f->mType, Present(v) ? v : nullptr, where);
    }

    // epochs.yaml and dbtechtree.yaml placed: every epoch (its position is its key) with its techs in its block
    std::vector<EpochBlock>& GetLayout()
    {
        if (this->mHaveLayout)
            return this->mLayout;
        const EEDTTable& te = *FindTable("epochs");
        const EEDTTable& tt = *FindTable("dbtechtree");
        Doc& de = this->GetDoc(te);
        Doc& dt = this->GetDoc(tt);
        const Node* eps = de.mDoc.Find("records");
        const Node* nodes = dt.mDoc.Find("records");
        std::map<std::string, long> theIndex;
        this->mLayout.clear();
        if (eps && eps->mKind == Node::kList) {
            for (size_t k = 0; k < eps->mItems.size(); k++) {
                const Node& m = eps->mItems[k];
                std::string where = "epochs line " + Num(m.mLine);
                if (m.Find("row"))
                    Fail(where + ": an epoch has no row (its place in the file is its position)");
                const Node* slots = m.Find("slots");
                EpochBlock b;
                b.mEpoch = &m;
                b.mLine = m.mLine;
                b.mSlots = Present(slots) ? HeadInt(slots, "slots", where) : 0;
                const Node* id = m.Find("id");
                if (Present(id) && id->mKind == Node::kScalar && !theIndex.count(id->mText))
                    theIndex[id->mText] = (long)k;
                this->mLayout.push_back(b);
            }
        }
        std::vector<std::vector<const Node*>> groups(this->mLayout.size());
        std::vector<std::vector<long>> lines(this->mLayout.size());
        if (nodes && nodes->mKind == Node::kList) {
            for (const Node& m : nodes->mItems) {
                const Node* e = m.Find("epoch");
                std::map<std::string, long>::const_iterator it =
                    Present(e) && e->mKind == Node::kScalar ? theIndex.find(e->mText) : theIndex.end();
                if (it == theIndex.end())
                    Fail("dbtechtree line " + Num(m.mLine) + ": epoch '" + (Present(e) && e->mKind == Node::kScalar ?
                         e->mText : std::string()) + "' is not an id of epochs");
                groups[it->second].push_back(&m);
                lines[it->second].push_back(m.mLine);
            }
        }
        for (size_t k = 0; k < this->mLayout.size(); k++)
            this->mLayout[k].mNodes = PlaceList(groups[k], lines[k], this->mLayout[k].mSlots,
                                                "dbtechtree epoch " + Num((long long)k));
        this->mHaveLayout = true;
        return this->mLayout;
    }

    // a tech's key: its mID, or (epoch + 1) * 1000 + slot
    long NodeKey(const Placed& p, long inEpoch, const std::string& where)
    {
        const Node* v = p.mMap->Find("mID");
        if (!Present(v))
            return (inEpoch + 1) * 1000 + p.mPos;
        const EEDTField* f = FindField(*FindTable("dbtechtree")->mRecord, "mID");
        return (long)(int32_t)Number(*f, f->mType, v, where);
    }

    const Index& GetIndex(const EEDTTable& t)
    {
        std::map<std::string, Index>::iterator it = this->mIndexes.find(t.mName);
        if (it != this->mIndexes.end())
            return it->second;
        std::vector<Placed> all;
        std::vector<long> keys;
        std::vector<std::string> files;
        if (t.mContainer == kEEDTC_techtree || t.mContainer == kEEDTC_epochs) {
            std::vector<EpochBlock>& layout = this->GetLayout();
            for (size_t k = 0; k < layout.size(); k++) {
                all.push_back(Placed{(long)k, layout[k].mEpoch, layout[k].mLine});
                keys.push_back((long)k);
                files.push_back("epochs");
            }
            if (t.mContainer == kEEDTC_techtree) {
                for (size_t k = 0; k < layout.size(); k++)
                    for (const Placed& p : layout[k].mNodes) {
                        all.push_back(p);
                        keys.push_back(this->NodeKey(p, (long)k, "dbtechtree line " + Num(p.mLine)));
                        files.push_back("dbtechtree");
                    }
            }
        } else {
            Doc& d = this->GetDoc(t);
            all = d.mRows;
            for (const Placed& p : all) {
                keys.push_back(this->Key(t, p, std::string(t.mName) + " line " + Num(p.mLine)));
                files.push_back(t.mName);
            }
        }
        Index idx;
        std::map<std::string, std::string> lineOf;
        for (size_t n = 0; n < all.size(); n++) {
            const Placed& p = all[n];
            std::string where = files[n] + " line " + Num(p.mLine);
            long key = keys[n];
            const Node* v = p.mMap->Find("id");
            if (!Present(v) || v->mKind != Node::kScalar)
                Fail(where + ": no id");
            if (!IsId(v->mText))
                Fail(where + ": id '" + v->mText + "': an id is a lowercase letter, then a-z, 0-9 or _");
            if (idx.mKeys.count(v->mText))
                Fail(where + ": duplicate id '" + v->mText + "' (also " + lineOf[v->mText] + ")");
            if (idx.mIds.count(key))
                Fail(where + ": id '" + v->mText + "': key " + Num(key) + " is also '" + idx.mIds[key] + "'");
            for (unsigned long i = 0; i < t.mRowNameCount; i++)
                if (t.mRowNames[i].mRow == key && v->mText != t.mRowNames[i].mName)
                    Fail(where + ": id '" + v->mText + "' does not match row " + Num(key) + " ('" +
                         t.mRowNames[i].mName + "')");
            idx.mKeys[v->mText] = key;
            idx.mIds[key] = v->mText;
            lineOf[v->mText] = where;
        }
        return this->mIndexes[t.mName] = std::move(idx);
    }

    // a reference cell -> the integer the record holds: an id is the target row's key; an integer must name no
    // listed row of the target (or be one of the field's none values)
    uint32_t Resolve(const EEDTRef& ref, unsigned char type, const Node* v, const std::string& where,
                     const char* field)
    {
        if (!v)
            return 0;
        if (v->mKind != Node::kScalar)
            Fail(where + ": " + field + ": a list where a reference belongs");
        const EEDTTable* target = FindTable(ref.mTable);
        if (ref.mNever && v->mText == "never")
            return (uint32_t)this->GetIndex(*target).mKeys.size();   // the target's count: no row
        if (IsId(v->mText)) {
            const Index& idx = this->GetIndex(*target);
            std::map<std::string, long>::const_iterator it = idx.mKeys.find(v->mText);
            if (it == idx.mKeys.end())
                Fail(where + ": " + field + ": '" + v->mText + "' is not an id of " + ref.mTable);
            long long lo, hi;
            int width;
            IntRange(type, lo, hi, width);
            if (it->second < lo || it->second > hi)
                Fail(where + ": " + field + ": '" + v->mText + "' is key " + Num(it->second) + ", out of range");
            return (uint32_t)it->second;
        }
        long long n;
        try {
            n = ParseInt(v->mText, type);
        } catch (Failure& e) {
            Fail(where + ": " + field + ": " + e.mText);
        }
        bool none = false;
        for (unsigned long i = 0; i < ref.mNoneCount; i++)
            none = none || ref.mNone[i] == n;
        const Index& idx = this->GetIndex(*target);
        if (!none && n >= -2147483648LL && n <= 2147483647LL && idx.mIds.count((long)n))
            Fail(where + ": " + field + ": " + v->mText + " names a row of " + ref.mTable + ": write its id");
        return (uint32_t)n;
    }

    Row BuildRow(const EEDTTable& t, const Placed& p, bool inHasKey, const char* const* inTextKeys = nullptr)
    {
        const Node& m = *p.mMap;
        std::string where = std::string(t.mContainer == kEEDTC_epochs ? "epochs" : t.mName) + " line " + Num(p.mLine);
        const Node* id = m.Find("id");
        if (Present(id) && id->mKind == Node::kScalar)
            where += " (id " + id->mText + ")";
        Getter g(m, where);
        g.Get("row");
        g.Get("id");
        for (const char* const* k = inTextKeys; k && *k; k++)
            g.Get(*k);
        Row r;
        r.mHole = false;
        r.mRec = BuildRecord(t, g, *this, inHasKey, p.mPos);
        r.mLead = BuildLead(t, g);
        if (t.mListName)
            r.mList = this->BuildList(t, g);
        g.Leftover();
        return r;
    }

    std::vector<uint32_t> BuildList(const EEDTTable& t, Getter& g)
    {
        std::vector<uint32_t> out;
        const Node* v = g.Get(t.mListName);
        if (!v)
            return out;
        if (v->mKind != Node::kList)
            Fail(g.mWhere + ": " + t.mListName + " must be a list");
        const EEDTRef* ref = FindRef(t, "@list");
        unsigned char type = t.mListSigned ? kEEDT_s32 : kEEDT_u32;
        for (const Node& x : v->mItems) {
            if (x.mKind != Node::kScalar)
                Fail(g.mWhere + ": " + t.mListName + ": a list where a number belongs");
            if (ref) {
                out.push_back(this->Resolve(*ref, type, &x, g.mWhere, t.mListName));
                continue;
            }
            try {
                out.push_back((uint32_t)ParseInt(x.mText, type));
            } catch (Failure& e) {
                Fail(g.mWhere + ": " + t.mListName + ": " + e.mText);
            }
        }
        return out;
    }

    void Build(const EEDTTable& t, std::vector<unsigned char>& out)
    {
        if (t.mTextOnly)
            Fail(std::string(t.mName) + ": a text-only table (it compiles into dbtechtree)");
        this->GetIndex(t);   // the ids are checked even where nothing references the table
        std::vector<unsigned char> hole = Record(t.mHole, t.mRecord->mSize);
        Writer w{t, hole, out, LeadCount(t)};
        out.clear();
        if (t.mContainer == kEEDTC_techtree) {
            const EEDTTable& te = *FindTable("epochs");
            this->GetIndex(te);
            static const char* const kEpochKeys[] = {"slots", "music", "citizen_models", nullptr};
            static const char* const kNodeKeys[] = {"epoch", nullptr};
            const EEDTField* theID = FindField(*t.mRecord, "mID");
            std::vector<EpochBlock>& layout = this->GetLayout();
            for (size_t k = 0; k < layout.size(); k++) {
                const EpochBlock& b = layout[k];
                Row ep = this->BuildRow(te, Placed{(long)k, b.mEpoch, b.mLine}, false, kEpochKeys);
                for (unsigned long i = 0; i < te.mDerivedCount; i++) {   // what follows from the position
                    const EEDTDerived& dv = te.mDerived[i];
                    if (!Present(b.mEpoch->Find(dv.mField)))
                        PutLE(ep.mRec, FindField(*te.mRecord, dv.mField)->mOffset, (uint32_t)(dv.mA * (long)k + dv.mB), 4);
                }
                long n = b.mSlots;
                for (const Placed& p : b.mNodes)
                    n = p.mPos + 1 > n ? p.mPos + 1 : n;
                std::vector<Row> rows(n);
                for (const Placed& p : b.mNodes) {
                    rows[p.mPos] = this->BuildRow(t, p, false, kNodeKeys);
                    if (!Present(p.mMap->Find("mID")))
                        PutLE(rows[p.mPos].mRec, theID->mOffset, (uint32_t)(((long)k + 1) * 1000 + p.mPos), 4);
                }
                Put32(out, (uint32_t)rows.size());
                w.Record(ep);
                w.List(ep);
                for (const Row& r : rows) {
                    w.Record(r);
                    w.List(r);
                }
            }
            this->mDocs.erase("epochs");
            this->mHaveLayout = false;
            this->mLayout.clear();
        } else {
            Doc& d = this->GetDoc(t);
            long n = d.mTotal;
            for (const Placed& p : d.mRows)
                n = p.mPos + 1 > n ? p.mPos + 1 : n;
            std::vector<Row> rows(n);
            for (const Placed& p : d.mRows)
                rows[p.mPos] = this->BuildRow(t, p, true);
            if (t.mContainer == kEEDTC_count || t.mContainer == kEEDTC_lead)
                Put32(out, (uint32_t)rows.size());
            for (const Row& r : rows) {
                if (t.mContainer == kEEDTC_lead)
                    w.Lead(r);
                w.Record(r);
                if (t.mContainer == kEEDTC_events)
                    w.List(r);
            }
        }
        // the parsed text is no longer needed once the table is built (its ids stay indexed)
        this->mDocs.erase(t.mName);
    }

private:
    std::function<bool(const char*, std::string&)> mRead;
    std::map<std::string, std::unique_ptr<Doc>> mDocs;
    std::map<std::string, Index> mIndexes;
    std::vector<EpochBlock> mLayout;
    bool mHaveLayout = false;
};

std::vector<unsigned char> BuildRecord(const EEDTTable& t, Getter& g, Resolver& r, bool inHasKey, long inKey)
{
    const EEDTStruct& st = *t.mRecord;
    std::vector<unsigned char> buf = Record(st.mBase, st.mSize);
    for (unsigned long fi = 0; fi < st.mFieldCount; fi++) {
        const EEDTField& f = st.mFields[fi];
        std::string name = f.mName;
        const EEDTRef* ref = FindRef(t, f.mName);
        if (f.mType == kEEDT_str) {
            std::string text = Text(g.Get(name), g.mWhere, f.mName);
            if (text.size() >= f.mCount)
                Fail(g.mWhere + ": " + name + ": " + Num(text.size()) + " bytes, the field holds " +
                     Num(f.mCount - 1) + " (+ NUL)");
            memset(&buf[f.mOffset], 0, f.mCount);
            memcpy(&buf[f.mOffset], text.data(), text.size());
        } else if (f.mType == kEEDT_pair_f32) {
            std::vector<const Node*> e = Elems(g.Get(name), f.mCount, g.mWhere, f.mName);
            for (size_t k = 0; k < f.mCount; k++) {
                std::vector<const Node*> p = Elems(e[k], 2, g.mWhere, f.mName);
                PutLE(buf, f.mOffset + 8 * k, Number(f, kEEDT_f32, p[0], g.mWhere), 4);
                PutLE(buf, f.mOffset + 8 * k + 4, Number(f, kEEDT_f32, p[1], g.mWhere), 4);
            }
        } else {
            unsigned long size = kTypeSize[f.mType];
            if (f.mCount == 1) {
                const Node* v = g.Get(name);
                if (v && v->mKind == Node::kList)
                    Fail(g.mWhere + ": " + name + ": a list where a number belongs");
                uint32_t x;
                if (!v && inHasKey && t.mSelfField && strcmp(t.mSelfField, f.mName) == 0)
                    x = (uint32_t)(inKey + t.mSelfOffset);
                else
                    x = ref ? r.Resolve(*ref, f.mType, v, g.mWhere, f.mName) : Number(f, f.mType, v, g.mWhere);
                PutLE(buf, f.mOffset, x, size);
            } else {
                std::vector<const Node*> e = Elems(g.Get(name), f.mCount, g.mWhere, f.mName);
                for (size_t k = 0; k < f.mCount; k++) {
                    if (e[k] && e[k]->mKind == Node::kList)
                        Fail(g.mWhere + ": " + name + ": a list where a number belongs");
                    uint32_t x = ref ? r.Resolve(*ref, f.mType, e[k], g.mWhere, f.mName)
                                     : Number(f, f.mType, e[k], g.mWhere);
                    PutLE(buf, f.mOffset + size * k, x, size);
                }
            }
        }
    }
    return buf;
}

bool Run(const std::function<void()>& inBody, const char* inTable, std::string& outError)
{
    try {
        inBody();
    } catch (Failure& e) {
        outError = e.mText;
        return false;
    } catch (std::exception& e) {
        outError = std::string(inTable) + ": " + e.what();
        return false;
    }
    return true;
}

}  // namespace

const char* TextExtension(const char* inTable)
{
    const EEDTTable* t = FindTable(inTable);
    return t ? (t->mYaml ? ".yaml" : ".csv") : nullptr;
}

struct Content::Impl {
    Resolver mResolver;
    explicit Impl(const Reader& inRead) : mResolver(inRead) {}
};

Content::Content(const Reader& inRead) : mImpl(new Impl(inRead)) {}

Content::~Content()
{
    delete this->mImpl;
}

bool Content::Compile(const char* inTable, std::vector<unsigned char>& outData, std::string& outError)
{
    const EEDTTable* t = FindTable(inTable);
    if (!t) {
        outError = std::string(inTable) + ": not a db table with a text form";
        return false;
    }
    return Run([&] { this->mImpl->mResolver.Build(*t, outData); }, inTable, outError);
}

bool Compile(const char* inTable, const char* inText, unsigned long inLength, std::vector<unsigned char>& outData,
             std::string& outError)
{
    const EEDTTable* t = FindTable(inTable);
    std::string text(inText, inLength);
    Content c([&](const char* inName, std::string& outText) {
        if (!t || strcmp(inName, t->mName) != 0)
            return false;
        outText = text;
        return true;
    });
    return c.Compile(inTable, outData, outError);
}

}  // namespace EEDataText
