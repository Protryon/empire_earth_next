// GUIStringData conversion, comparison and search
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/ee_won.h"
// The comparisons, searches, case maps, number read and narrowing are ee_won's guistring module
// (rust/crates/ee_won/src/guistring.rs); this class keeps the buffer, its length and capacity.
namespace WONAPI {
// Destroys the string data, freeing its text
GUIStringData::~GUIStringData()
{
    delete [] this->mText;
}
// Uppercases the text in place; only characters below 256 are changed
void GUIStringData::toUpper()
{
    ee_won_guistring_change_case(this->mText,this->mLength,true,sizeof(wchar_t));
}
static void GUIStringDataNarrowSink(void* context, const uint8_t* bytes, size_t length)
{
    static_cast<std::string*>(context)->assign(reinterpret_cast<const char*>(bytes),length);
}
// Returns the text as the game's narrow text, UTF-8 (D6; the original narrowed a character below 256 to its byte and
// any other to '?')
std::string GUIStringData::GetString() const
{
    std::string result;
    if(this->mLength!=0)
        ee_won_guistring_narrow(this->mText,this->mLength,sizeof(wchar_t),GUIStringDataNarrowSink,&result);
    return result;
}

// Returns the text as a wide string
std::wstring GUIStringData::GetWString() const
{
    return std::wstring(this->mText,this->mLength);
}
// Compares two strings by character; returns the first character difference, else -1/0/1 by
// length (not the length difference)
int GUIStringData::Compare(const GUIStringData* other) const
{
    return ee_won_guistring_op(EeWonGuiStringOp::Compare,this->mText,this->mLength,other->mText,other->mLength,0,sizeof(wchar_t));
}
// Compares two strings ignoring case (towupper); returns the first difference, else -1/0/1
// by length
int GUIStringData::CompareNoCase(const GUIStringData* other) const
{
    return ee_won_guistring_op(EeWonGuiStringOp::CompareNoCase,this->mText,this->mLength,other->mText,other->mLength,0,
                               sizeof(wchar_t));
}
// The offset is a signed int and the narrow text is UTF-8 (decoded first); the counted wide text decides, not an
// embedded NUL
// Compares the text from an offset with a narrow string ignoring case; returns 1 if the narrow
// string ends first, the first difference, else 0 or -1
int GUIStringData::CompareNoCase(const char* other, int offset) const
{
    return ee_won_guistring_compare_narrow(this->mText,this->mLength,offset,reinterpret_cast<const uint8_t*>(other),true,
                                           sizeof(wchar_t));
}
// No length check and no '+' sign; the index is 16 bits and wraps
// Parses a decimal integer from an offset, skipping leading spaces and allowing a '-' sign; stops
// at the first non-digit
int GUIStringData::ToInt(unsigned short offset) const
{
    return ee_won_guistring_to_int(this->mText,this->mLength,offset,sizeof(wchar_t));
}
// The character and start are full ints; start is not range-checked
// Returns the position of a character at or after start, or -1
int GUIStringData::find(int ch, int start) const
{
    return ee_won_guistring_find_char(this->mText,this->mLength,ch,start,false,sizeof(wchar_t));
}
// An empty substring matches only while start is below the length
// Returns the position of a substring at or after start, or -1
int GUIStringData::find(const GUIStringData* other, int start) const
{
    return ee_won_guistring_op(EeWonGuiStringOp::Find,this->mText,this->mLength,other->mText,other->mLength,start,sizeof(wchar_t));
}
// Same start and empty-substring rules as find
// Returns the position of a substring at or after start ignoring case (towupper), or -1
int GUIStringData::findNoCase(const GUIStringData* other, int start) const
{
    return ee_won_guistring_op(EeWonGuiStringOp::FindNoCase,this->mText,this->mLength,other->mText,other->mLength,start,
                               sizeof(wchar_t));
}
}
namespace WONAPI {
// Compares the text from an offset with a narrow string; returns 1 if the narrow string ends first,
// the first difference, else 0 only if the narrow string continues with '0' (original bug), otherwise -1
int GUIStringData::compare(const char *theStr, int theOffset) const
{
    return ee_won_guistring_compare_narrow(this->mText,this->mLength,theOffset,reinterpret_cast<const uint8_t*>(theStr),
                                           false,sizeof(wchar_t));
}
}

namespace WONAPI {
// Lowercases byte-range characters in place (not identified in the original exe)
void GUIStringData::toLowerCase()
{
    ee_won_guistring_change_case(this->mText,this->mLength,false,sizeof(wchar_t));
}
}
