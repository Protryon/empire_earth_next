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
#include <map>
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
    while (p < inText.size()) {
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
            if (p < inText.size() && inText[p] == '\n')
                p++;
            break;
        }
        if (any)
            rows.push_back(row);
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
            Fail("CSV row " + Num(i) + ": " + Num(rows[i].size()) + " cells, the header has " +
                 Num(out.mHeader.size()));
        out.mRows.push_back(rows[i]);
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

// the record: the struct's base (zero + the defaults of the fields the game never reads), then every field the
// text carries (a missing one is zero)
std::vector<unsigned char> BuildRecord(const EEDTStruct& st, Getter& g)
{
    std::vector<unsigned char> buf = Record(st.mBase, st.mSize);
    for (unsigned long fi = 0; fi < st.mFieldCount; fi++) {
        const EEDTField& f = st.mFields[fi];
        std::string name = f.mName;
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
                PutLE(buf, f.mOffset, Number(f, f.mType, v, g.mWhere), size);
            } else {
                std::vector<const Node*> e = Elems(g.Get(name), f.mCount, g.mWhere, f.mName);
                for (size_t k = 0; k < f.mCount; k++) {
                    if (e[k] && e[k]->mKind == Node::kList)
                        Fail(g.mWhere + ": " + name + ": a list where a number belongs");
                    PutLE(buf, f.mOffset + size * k, Number(f, f.mType, e[k], g.mWhere), size);
                }
            }
        }
    }
    return buf;
}

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

struct Row {
    std::vector<unsigned char> mRec;
    std::vector<std::string> mLead;
    std::vector<uint32_t> mList;
    bool mHole = true;
};

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

std::vector<uint32_t> BuildList(const EEDTTable& t, Getter& g)
{
    std::vector<uint32_t> out;
    const Node* v = g.Get(t.mListName);
    if (!v)
        return out;
    if (v->mKind != Node::kList)
        Fail(g.mWhere + ": " + t.mListName + " must be a list");
    for (const Node& x : v->mItems) {
        if (x.mKind != Node::kScalar)
            Fail(g.mWhere + ": " + t.mListName + ": a list where a number belongs");
        try {
            out.push_back((uint32_t)ParseInt(x.mText, t.mListSigned ? kEEDT_s32 : kEEDT_u32));
        } catch (Failure& e) {
            Fail(g.mWhere + ": " + t.mListName + ": " + e.mText);
        }
    }
    return out;
}

Row BuildRow(const EEDTTable& t, const Node& m, const std::string& where, long pos)
{
    if (m.mKind != Node::kMap)
        Fail(where + ": not a mapping");
    Getter g(m, where);
    g.Get("row");
    const Node* label = g.Get("label");
    if (label && label->mKind == Node::kScalar && !label->mText.empty() && pos >= 0) {
        for (unsigned long i = 0; i < t.mRowNameCount; i++)
            if (t.mRowNames[i].mRow == pos && label->mText != t.mRowNames[i].mName)
                Fail(where + ": label '" + label->mText + "' does not match row " + Num(pos) + " ('" +
                     t.mRowNames[i].mName + "')");
    }
    Row r;
    r.mHole = false;
    r.mRec = BuildRecord(*t.mRecord, g);
    r.mLead = BuildLead(t, g);
    if (t.mListName)
        r.mList = BuildList(t, g);
    g.Leftover();
    return r;
}

// the records of a list -> rows placed at their positions (missing ones stay holes)
std::vector<Row> PlaceRows(const EEDTTable& t, const Node* records, long total, const std::string& where)
{
    std::vector<std::pair<long, Row>> placed;
    long next = 0;
    if (records && records->mKind != Node::kList)
        Fail(where + ": must be a list");
    if (records) {
        for (size_t k = 0; k < records->mItems.size(); k++) {
            const Node& m = records->mItems[k];
            if (m.mKind != Node::kMap)
                Fail(where + " item " + Num(k) + ": not a mapping");
            const Node* v = m.Find("row");
            long pos = next;
            if (v && !v->IsMissing()) {
                if (v->mKind != Node::kScalar)
                    Fail(where + " item " + Num(k) + ": row: not a number");
                try {
                    pos = (long)ParseInt(v->mText, kEEDT_s32);
                } catch (Failure& e) {
                    Fail(where + " item " + Num(k) + ": row: " + e.mText);
                }
                if (pos < next)
                    Fail(where + " item " + Num(k) + ": row " + Num(pos) + " is not after the previous row (" +
                         Num(next - 1) + ")");
            }
            placed.emplace_back(pos, BuildRow(t, m, where + " row " + Num(pos), pos));
            next = pos + 1;
        }
    }
    long n = total;
    if (next > n)
        n = next;
    std::vector<Row> rows(n);
    for (std::pair<long, Row>& p : placed)
        rows[p.first] = std::move(p.second);
    return rows;
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

bool Present(const Node* v)
{
    return v && !(v->mKind == Node::kScalar && (v->mText.empty() || (!v->mQuoted && (v->mText == "~" ||
                                                                                       v->mText == "null"))));
}

// CSV columns the exporter can write for a table (tables.csv_columns)
std::vector<std::string> CsvColumns(const EEDTTable& t)
{
    std::vector<std::string> cols{"row"};
    if (t.mRowNameCount)
        cols.push_back("label");
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
               const std::vector<std::string>& cells)
{
    Node m;
    m.mKind = Node::kMap;
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
        *slot = Scalar(cells[c], true, 0);
    }
    return m;
}

void CompileTable(const EEDTTable& t, const std::string& inText, std::vector<unsigned char>& out)
{
    const EEDTStruct& st = *t.mRecord;
    std::string name = t.mName;
    Node doc;
    Node csvRecords;
    std::map<std::string, std::string> directives;
    const Node* head[3] = {};   // eedata format rows
    static const char* const kHead[3] = {"eedata", "format", "rows"};
    std::vector<Node> headNodes;
    if (t.mYaml) {
        doc = ParseYaml(inText);
        if (doc.mKind != Node::kMap)
            Fail(name + ": the document is not a mapping");
        for (const std::string& k : doc.mKeys) {
            bool ok = k == "records" || k == "epochs";
            for (const char* h : kHead)
                ok = ok || k == h;
            if (!ok)
                Fail(name + ": unknown top-level key " + k);
        }
        for (int i = 0; i < 3; i++)
            head[i] = doc.Find(kHead[i]);
    } else {
        Csv csv = ParseCsv(inText);
        for (const std::pair<const std::string, std::string>& d : csv.mDirectives) {
            bool ok = false;
            for (const char* h : kHead)
                ok = ok || d.first == h;
            if (!ok)
                Fail(name + ": unknown directive " + d.first);
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
        csvRecords.mKind = Node::kList;
        for (const std::vector<std::string>& r : csv.mRows)
            csvRecords.mItems.push_back(CsvRecord(bases, idx, r));
    }
    if (head[0] && head[0]->mKind == Node::kScalar && head[0]->mText != name)
        Fail(name + ": the file says it is '" + head[0]->mText + "'");
    if (head[1] && (head[1]->mKind != Node::kScalar || head[1]->mText != Num(EEDT_FORMAT_VERSION)))
        Fail(name + ": format " + (head[1]->mKind == Node::kScalar ? head[1]->mText : std::string("?")) +
             ", this loader reads format " + Num(EEDT_FORMAT_VERSION));
    long total = Present(head[2]) ? HeadInt(head[2], "rows", name) : 0;
    std::vector<unsigned char> hole = Record(t.mHole, st.mSize);
    Writer w{t, hole, out, LeadCount(t)};
    out.clear();
    if (t.mContainer == kEEDTC_techtree) {
        const Node* eps = doc.Find("epochs");
        if (!eps || eps->mKind != Node::kList)
            Fail(name + ": epochs must be a list");
        for (size_t k = 0; k < eps->mItems.size(); k++) {
            const Node& e = eps->mItems[k];
            std::string where = name + " epoch " + Num(k);
            const Node* epoch = e.mKind == Node::kMap ? e.Find("epoch") : nullptr;
            if (!epoch || epoch->mKind != Node::kMap)
                Fail(where + ": needs an epoch: mapping");
            for (const std::string& key : e.mKeys)
                if (key != "slots" && key != "epoch" && key != "nodes")
                    Fail(where + ": unknown key " + key);
            Row ep = BuildRow(t, *epoch, where, -1);
            const Node* nodes = e.Find("nodes");
            if (nodes && nodes->IsMissing())
                nodes = nullptr;
            const Node* slots = e.Find("slots");
            long n = Present(slots) ? HeadInt(slots, "slots", where) : 0;
            std::vector<Row> rows = PlaceRows(t, nodes, n, where);
            Put32(out, (uint32_t)rows.size());
            w.Record(ep);
            w.List(ep);
            for (const Row& r : rows) {
                w.Record(r);
                w.List(r);
            }
        }
        return;
    }
    const Node* records = t.mYaml ? doc.Find("records") : &csvRecords;
    if (records && records->IsMissing())
        records = nullptr;
    std::vector<Row> rows = PlaceRows(t, records, total, name);
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

}  // namespace

const char* TextExtension(const char* inTable)
{
    const EEDTTable* t = FindTable(inTable);
    return t ? (t->mYaml ? ".yaml" : ".csv") : nullptr;
}

bool Compile(const char* inTable, const char* inText, unsigned long inLength, std::vector<unsigned char>& outData,
             std::string& outError)
{
    const EEDTTable* t = FindTable(inTable);
    if (!t) {
        outError = std::string(inTable) + ": not a db table with a text form";
        return false;
    }
    try {
        CompileTable(*t, std::string(inText, inLength), outData);
    } catch (Failure& e) {
        outError = e.mText;
        return false;
    } catch (std::exception& e) {
        outError = std::string(inTable) + ": " + e.what();
        return false;
    }
    return true;
}

}  // namespace EEDataText
