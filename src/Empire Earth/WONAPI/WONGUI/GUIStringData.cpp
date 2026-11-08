// GUIStringData conversion, comparison and search
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <ctype.h>
#include <wctype.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
namespace WONAPI {
// FUNCTION: WON 0x00727980
// Destroys the string data, freeing its text
GUIStringData::~GUIStringData()
{
    delete [] this->mText;
}
// FUNCTION: WON 0x007279d0
// Uppercases the text in place; only characters below 256 are changed
void GUIStringData::toUpper()
{
    int length=this->mLength;
    for(int i=0;i<length;++i)
        if(this->mText[i]<256)
            this->mText[i]=toupper(this->mText[i]);
}
// FUNCTION: WON 0x00727d90
// Returns the text as a narrow string; characters above 255 become '?'
std::string GUIStringData::GetString() const
{
    std::string result;
    result.reserve(this->mLength);
    for(int i=0;i<this->mLength;++i)
    {
        wchar_t c=this->mText[i];
        if(c<256) result+=(char)c;
        else result+='?';
    }
    return result;
}

// FUNCTION: WON 0x00727f30
// Returns the text as a wide string
std::wstring GUIStringData::GetWString() const
{
    return std::wstring(this->mText,this->mLength);
}
// FUNCTION: WON 0x00727fa0
// Compares two strings by character; returns the first character difference, else -1/0/1 by
// length (not the length difference)
int GUIStringData::Compare(const GUIStringData* other) const
{
    unsigned short len1=this->mLength, len2=other->mLength;
    int a=len1;
    int b=len2;
    int count=b<a?b:a;
    for(int i=0;i<count;++i)
        if(this->mText[i]!=other->mText[i]) return this->mText[i]-other->mText[i];
    if(len1==len2) return 0;
    return len1<len2?-1:1;
}
// FUNCTION: WON 0x00728030
// Compares two strings ignoring case (towupper); returns the first difference, else -1/0/1
// by length
int GUIStringData::CompareNoCase(const GUIStringData* other) const
{
    int count=this->mLength;
    if(other->mLength<count) count=other->mLength;
    const wchar_t* a=this->mText;
    const wchar_t* b=other->mText;
    for(int i=0;i<count;++a,++b,++i)
    {
        int ca=towupper(*a);
        int cb=towupper(*b);
        if(ca!=cb) return ca-cb;
    }
    if(other->mLength==this->mLength) return 0;
    return this->mLength<other->mLength?-1:1;
}
// The offset is a signed int and narrow bytes widen unsigned; the counted wide text decides, not an embedded NUL
// FUNCTION: WON 0x00728150
// Compares the text from an offset with a narrow string ignoring case; returns 1 if the narrow
// string ends first, the first difference, else 0 or -1
int GUIStringData::CompareNoCase(const char* other, int offset) const
{
    const wchar_t* a=this->mText+offset;
    int count=this->mLength-offset;
    for(int i=0;i<count;++a,++other,++i)
    {
        int ca=towupper(*a);
        unsigned char byte=*other;
        int cb=towupper(byte);
        if(cb==0) return 1;
        if(ca!=cb) return ca-cb;
    }
    return *other==0?0:-1;
}
// No length check and no '+' sign; the index is 16 bits and wraps
// FUNCTION: WON 0x007281e0
// Parses a decimal integer from an offset, skipping leading spaces and allowing a '-' sign; stops
// at the first non-digit
int GUIStringData::ToInt(unsigned short offset) const
{
    int value=0;
    while(isspace(this->mText[offset])) ++offset;
    const wchar_t* text=this->mText;
    bool negative;
    if(text[offset]=='-') {
        negative=true;
        ++offset;
    } else negative=false;
    while(true)
    {
        int digit=text[offset]-'0';
        ++offset;
        if(digit<0 || digit>9) break;
        value=value*10+digit;
    }
    return negative?-value:value;
}
// The character and start are full ints; start is not range-checked
// FUNCTION: WON 0x00728280
// Returns the position of a character at or after start, or -1
int GUIStringData::find(int ch, int start) const
{
    int length=this->mLength;
    for(int i=start;i<length;++i)
        if(this->mText[i]==ch) return i;
    return -1;
}
// An empty substring matches only while start is below the length
// FUNCTION: WON 0x007282c0
// Returns the position of a substring at or after start, or -1
int GUIStringData::find(const GUIStringData* other, int start) const
{
    int candidate=start;
    int length=this->mLength;
    int needleLength=other->mLength;
    const wchar_t* needle=other->mText;
    for(;candidate<length;++candidate)
    {
        int pos=candidate,matched=0;
        while(pos<length && matched<needleLength && this->mText[pos]==needle[matched])
        {
            ++pos;
            ++matched;
        }
        if(matched==needleLength) return pos-needleLength;
    }
    return -1;
}
// Same start and empty-substring rules as find
// FUNCTION: WON 0x00728340
// Returns the position of a substring at or after start ignoring case (towupper), or -1
int GUIStringData::findNoCase(const GUIStringData* other, int start) const
{
    int candidate=start;
    int length=this->mLength;
    int needleLength=other->mLength;
    const wchar_t* needle=other->mText;
    for(;candidate<length;++candidate)
    {
        int pos=candidate,matched=0;
        while(pos<length && matched<needleLength && towupper(this->mText[pos])==towupper(needle[matched]))
        {
            ++pos;
            ++matched;
        }
        if(matched==needleLength) return pos-needleLength;
    }
    return -1;
}
}
namespace WONAPI {
// FUNCTION: WON 0x007280f0
// Compares the text from an offset with a narrow string; returns 1 if the narrow string ends first,
// the first difference, else 0 only if the narrow string continues with '0' (original bug), otherwise -1
int GUIStringData::compare(const char *theStr, int theOffset) const
{
    const wchar_t *p1 = this->mText + theOffset;
    const unsigned char *p2 = reinterpret_cast<const unsigned char*>(theStr);
    int aLength = this->mLength - theOffset;
    for(int i=0; i<aLength; i++)
    {
        int c1 = *p1;
        int c2 = *p2;
        if(c2==0)
            return 1;
        if(c1!=c2)
            return c1-c2;
        p1++; p2++;
    }
    return *p2=='0'?0:-1;
}
}

namespace WONAPI {
// Lowercases byte-range characters in place (not identified in the original exe)
void GUIStringData::toLowerCase()
{
	int aLength = this->mLength;
	for(int i=0; i<aLength; i++)
	{
		if(this->mText[i]<256)
			this->mText[i] = tolower(this->mText[i]);
	}	
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007274c0
// (public: virtual void * __thiscall WONAPI::GUIStringData::`scalar deleting destructor'(unsigned int)    [??_GGUIStringData@WONAPI@@UAEPAXI@Z])
