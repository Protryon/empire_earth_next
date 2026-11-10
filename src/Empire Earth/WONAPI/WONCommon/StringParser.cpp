// C:\EEX\Empire Earth\WONAPI\WONCommon\StringParser.cpp
// StringParser: a cursor over a narrow or wide string for config/resource scanning, ee_won's
// (rust/crates/ee_won/src/parser.rs); the object keeps its text, position and width. The scalar formats of the
// former StringParserNumbers.cpp are here too.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <set>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <algorithm>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "rust/ee_rust.h"
namespace WONAPI {
static const void* ParserText(const StringParser* inParser)
{
    return inParser->mWide ? static_cast<const void*>(inParser->mWideText) : static_cast<const void*>(inParser->mNarrow);
}
static uint32_t ParserUnit(const StringParser* inParser)
{
    return inParser->mWide ? (uint32_t)sizeof(wchar_t) : 1u;
}
static void AppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static void AppendUnits(void* inContext, const uint32_t* inUnits, size_t inCount)
{
    std::wstring* anOut = static_cast<std::wstring*>(inContext);
    for (size_t i = 0; i < inCount; i++)
        *anOut += (wchar_t)inUnits[i];
}
// FUNCTION: WON 0x006a48c0
// Returns the character at the given offset from the cursor; wide characters above 255 read as 0xff
unsigned char StringParser::GetChar(int offset)
{ return ee_won_parser_get_char(ParserText(this), ParserUnit(this), this->mPos, offset); }
// FUNCTION: WON 0x006a4910
// Returns the wide character at the given offset from the cursor
wchar_t StringParser::GetWChar(int offset)
{ return (wchar_t)ee_won_parser_get_wchar(ParserText(this), ParserUnit(this), this->mPos, offset); }
// FUNCTION: WON 0x006a4900
// Moves the cursor by the given amount
void StringParser::IncrementPos(int amount) { this->mPos+=amount; }
// FUNCTION: WON 0x006a4940
// Returns true when the cursor is on the terminator
bool StringParser::EndOfString()
{ return ee_won_parser_get_wchar(ParserText(this), ParserUnit(this), this->mPos, 0) == 0; }
// FUNCTION: WON 0x006a4970
// Moves the cursor past whitespace
void StringParser::SkipWhitespace()
{ ee_won_parser_skip_whitespace(ParserText(this), ParserUnit(this), &this->mPos); }
// FUNCTION: WON 0x006a49b0
// Skips whitespace and consumes the next character if it matches; whitespace stays consumed on a mismatch
bool StringParser::CheckNextChar(int ch)
{ return ee_won_parser_check_next_char(ParserText(this), ParserUnit(this), &this->mPos, ch); }
// FUNCTION: WON 0x006a49f0
// Reads a bare or quoted (escapes, L"...", "a"+"b") string token, trimming trailing whitespace; always true
bool StringParser::ReadString(std::string& out,bool stopAtWhitespace)
{
    out.erase();
    ee_won_parser_read_string(ParserText(this), ParserUnit(this), &this->mPos, stopAtWhitespace, AppendBytes, &out);
    return true;
}
// FUNCTION: WON 0x006a4ff0
// Reads a bare or quoted string token into a wide string, trimming trailing whitespace; always true
bool StringParser::ReadString(std::wstring& out,bool stopAtWhitespace)
{
    out.erase();
    ee_won_parser_read_wstring(ParserText(this), ParserUnit(this), &this->mPos, stopAtWhitespace, AppendUnits, &out);
    return true;
}
// FUNCTION: WON 0x006a53d0
// Reads a decimal or 0x hex integer with optional '-'; returns true when the cursor moved
// A lone '-' or '0x' without digits still counts as success
bool StringParser::ReadValue(int& out)
{
    int32_t aValue = 0;
    if (!ee_won_parser_read_int(ParserText(this), ParserUnit(this), &this->mPos, &aValue))
        return false;
    out = aValue;
    return true;
}
// FUNCTION: WON 0x006a5550
// Reads a true/yes/on/y/t/1 or false/no/off/n/f/0 word; returns false for any other word
// An invalid word is consumed but leaves the output unchanged
bool StringParser::ReadValue(bool& out)
{ return ee_won_parser_read_bool(ParserText(this), ParserUnit(this), &this->mPos, &out); }
// WStringToString is defined in StringUtil.cpp (0x6a6280)
std::string WStringToString(const std::wstring&);
// FUNCTION: WON 0x006a5820
// Appends True or False
void StringParser::ValueToString(const bool& value,std::string& out) { ee_won_parser_format_bool(value, AppendBytes, &out); }
// FUNCTION: WON 0x006a5a70
// Appends the integer in decimal
void StringParser::ValueToString(const int& value,std::string& out) { ee_won_parser_format_i64(value, AppendBytes, &out); }
// FUNCTION: WON 0x006a5c40
// Appends the 16-bit value in decimal
void StringParser::ValueToString(const unsigned short& value,std::string& out)
{ ee_won_parser_format_i64(value, AppendBytes, &out); }
// FUNCTION: WON 0x006a5c60
// Appends the string
void StringParser::ValueToString(const std::string& value,std::string& out) {out+=value;}
// FUNCTION: WON 0x006a5d70
// Appends the wide string converted to narrow
void StringParser::ValueToString(const std::wstring& value,std::string& out) {out+=WStringToString(value);}
// FUNCTION: WON 0x006a5e70
// Appends the strings separated by ", "
void StringParser::ValueToString(const std::list<std::string>& values,std::string& out)
{
    std::list<std::string>::const_iterator i=values.begin();
    while(i!=values.end()) {
        StringParser::ValueToString(*i,out);
        i++;
        if(i!=values.end()) out+=", ";
    }
}

// No original address identified: WON SDK cursor matching
bool StringParser::StartsWith(const char *theStr)
{ return ee_won_parser_starts_with(ParserText(this), ParserUnit(this), &this->mPos, theStr); }

// No original address identified; a matched prefix is not rewound when input follows it
bool StringParser::Is(const char *theStr)
{ return ee_won_parser_is(ParserText(this), ParserUnit(this), &this->mPos, theStr); }

// No original address identified: WON SDK whitespace scanner
void StringParser::SkipNonWhitespace()
{ ee_won_parser_skip_non_whitespace(ParserText(this), ParserUnit(this), &this->mPos); }

// No original address identified: the SDK's 64-bit read accumulates in an int, as ReadValue(int&)
bool StringParser::ReadValue(__int64 &theInt64)
{
    int32_t aValue = 0;
    if (!ee_won_parser_read_int(ParserText(this), ParserUnit(this), &this->mPos, &aValue))
        return false;
    theInt64 = aValue;
    return true;
}

// No original address identified: the SDK's sized formats (decimal; the original's "%d" / "%u" of a long)
void StringParser::ValueToString(const unsigned __int64 &theInt64, std::string &theStr)
{ ee_won_parser_format_u64(theInt64, AppendBytes, &theStr); }
void StringParser::ValueToString(const __int64 &theInt64, std::string &theStr)
{ ee_won_parser_format_i64(theInt64, AppendBytes, &theStr); }
void StringParser::ValueToString(const unsigned long &theLong, std::string &theStr)
{ ee_won_parser_format_u64(theLong, AppendBytes, &theStr); }
void StringParser::ValueToString(const long &theLong, std::string &theStr)
{ ee_won_parser_format_i64(theLong, AppendBytes, &theStr); }
void StringParser::ValueToString(const short &theShort, std::string &theStr)
{ ee_won_parser_format_i64(theShort, AppendBytes, &theStr); }
void StringParser::ValueToString(const char &theByte, std::string &theStr)
{ ee_won_parser_format_i64(theByte, AppendBytes, &theStr); }
void StringParser::ValueToString(const unsigned char &theByte, std::string &theStr)
{ ee_won_parser_format_u64(theByte, AppendBytes, &theStr); }
}
