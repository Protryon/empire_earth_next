// C:\EEX\Empire Earth\WONAPI\WONCommon\StringParser.cpp
// StringParser: a cursor over a narrow or wide string for config/resource scanning (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <set>
#include <windows.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <list>
#include <vector>
#include <ctype.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyParser.h"
namespace WONAPI {
// FUNCTION: WON 0x006a48c0
// Returns the character at the given offset from the cursor; wide characters above 255 read as 0xff
unsigned char StringParser::GetChar(int offset)
{
    if(this->mWide) {
        wchar_t ch=mWideText[this->mPos+offset];
        return ch<256 ? (unsigned char)ch : (unsigned char)0xff;
    }
    return (unsigned char)(mNarrow+this->mPos)[offset];
}
// FUNCTION: WON 0x006a4910
// Returns the wide character at the given offset from the cursor
wchar_t StringParser::GetWChar(int offset)
{ return this->mWide ? mWideText[this->mPos+offset] : (unsigned char)mNarrow[this->mPos+offset]; }
// FUNCTION: WON 0x006a4900
// Moves the cursor by the given amount
void StringParser::IncrementPos(int amount) { this->mPos+=amount; }
// FUNCTION: WON 0x006a4940
// Returns true when the cursor is on the terminator
bool StringParser::EndOfString()
{
    if(this->mWide) return mWideText[this->mPos]==0;
    return mNarrow[this->mPos]==0;
}
// FUNCTION: WON 0x006a4970
// Moves the cursor past whitespace
void StringParser::SkipWhitespace()
{
    while(!this->EndOfString()) {
        if(!isspace(this->GetChar(0))) break;
        ++this->mPos;
    }
}
// FUNCTION: WON 0x006a49b0
// Skips whitespace and consumes the next character if it matches; whitespace stays consumed on a mismatch
bool StringParser::CheckNextChar(int ch)
{
    this->SkipWhitespace();
    if(ch==this->GetChar(0)) {
        ++this->mPos;
        return true;
    }
    return false;
}
static int ScannerHexValue(char ch);
static void ScannerAppend(std::string& out,wchar_t ch);
static void ScannerAppend(std::string& out,char ch);
inline bool ScannerReadString(StringParser& p,std::string& out,bool stopAtWhitespace)
{
    out.erase();
    p.SkipWhitespace();
    bool quoted=false;
    bool wideLiteral=false;
    std::string::size_type keepLength=0;
    while(!p.EndOfString()) {
        unsigned char ch=p.GetChar(0);
        wchar_t wideChar;
        if(wideLiteral) wideChar=p.GetWChar(0);
        p.IncrementPos(1);
        if(stopAtWhitespace && isspace(ch) && !quoted) break;
        if(ch=='\\' && quoted) {
            unsigned char escaped=p.GetChar(0);
            p.IncrementPos(1);
            switch(escaped) {
            case 't':wideChar='\t';break;
            case 'n':wideChar='\n';break;
            case 'r':wideChar='\r';break;
            case '"':wideChar='"';break;
            case '$':wideChar='$';break;
            case '\\':wideChar='\\';break;
            case 'x': {
                int maxDigits=wideLiteral?4:2;
                wideChar=0;
                for(int digit=0;digit<maxDigits;++digit) {
                    if(!isxdigit(p.GetChar(0))) break;
                    int val=ScannerHexValue(p.GetChar(0));
                    wideChar=wideChar<<4;
                    wideChar=wideChar|val;
                    p.IncrementPos(1);
                }
                break;
            }
            default:p.IncrementPos(-1);wideChar='\\';break;
            }
            if(wideLiteral) ScannerAppend(out,wideChar);
            else ScannerAppend(out,(char)wideChar);
            keepLength=out.length();
        } else if(!quoted && ch=='L' && p.GetChar(0)=='"') {
            p.IncrementPos(1);
            wideLiteral=true;
            quoted=true;
        } else if(ch=='"') {
            if(quoted) {
                quoted=false;
                if(p.GetChar(0)!='+') break;
                p.IncrementPos(1);
                p.SkipWhitespace();
            } else {
                wideLiteral=false;
                quoted=true;
            }
        } else if(!quoted && (ch==',' || ch==')')) {
            p.IncrementPos(-1);
            break;
        } else {
            if(wideLiteral) ScannerAppend(out,wideChar);
            else ScannerAppend(out,(char)ch);
            if(quoted || !isspace(ch)) keepLength=out.length();
        }
    }
    out.resize(keepLength);
    return true;
}
// FUNCTION: WON 0x006a49f0
// Reads a bare or quoted (escapes, L"...", "a"+"b") string token, trimming trailing whitespace; always true
bool StringParser::ReadString(std::string& out,bool stopAtWhitespace)
{ return ScannerReadString(*this,out,stopAtWhitespace); }
static int ScannerHexValue(char ch)
{ return isdigit(ch) ? ch-'0' : toupper(ch)-'A'+10; }
// FUNCTION: WON 0x006a4da0
// Appends the two raw bytes of a wide character to a narrow string (not UTF-8)
static void ScannerAppend(std::string& out,wchar_t ch)
{
    wchar_t c=ch;
    out.append(reinterpret_cast<const char*>(&c),2);
}
static void ScannerAppend(std::string& out,char ch) { out+=ch; }
static void ScannerAppend(std::wstring& out,wchar_t ch);
inline bool ScannerReadString(StringParser& p,std::wstring& out,bool stopAtWhitespace)
{
    out.erase();
    p.SkipWhitespace();
    bool quoted=false;
    std::wstring::size_type keepLength=0;
    while(!p.EndOfString()) {
        unsigned char ch=p.GetChar(0);
        wchar_t wideChar=p.GetWChar(0);
        p.IncrementPos(1);
        if(stopAtWhitespace && isspace(ch) && !quoted) break;
        if(ch=='\\' && quoted) {
            unsigned char escaped=p.GetChar(0);
            p.IncrementPos(1);
            wchar_t value;
            switch(escaped) {
            case 't':value='\t';break;
            case 'n':value='\n';break;
            case 'r':value='\r';break;
            case '"':value='"';break;
            case '$':value='$';break;
            case '\\':value='\\';break;
            case 'x': {
                value=0;
                for(int digit=0;digit<4;++digit) {
                    if(!isxdigit(p.GetChar(0))) break;
                    int val=ScannerHexValue(p.GetChar(0));
                    value=value<<4;
                    value=value|val;
                    p.IncrementPos(1);
                }
                break;
            }
            default:p.IncrementPos(-1);value='\\';break;
            }
            ScannerAppend(out,value);
            keepLength=out.length();
        } else if(!quoted && ch=='L' && p.GetChar(0)=='"') {
            p.IncrementPos(1);
            quoted=true;
        } else if(ch=='"') {
            if(quoted) {
                quoted=false;
                if(p.GetChar(0)!='+') break;
                p.IncrementPos(1);
                p.SkipWhitespace();
            } else quoted=true;
        } else if(!quoted && (ch==',' || ch==')')) {
            p.IncrementPos(-1);
            break;
        } else {
            ScannerAppend(out,wideChar);
            if(quoted || !isspace(ch)) keepLength=out.length();
        }
    }
    out.resize(keepLength);
    return true;
}
// FUNCTION: WON 0x006a4ff0
// Reads a bare or quoted string token into a wide string, trimming trailing whitespace; always true
bool StringParser::ReadString(std::wstring& out,bool stopAtWhitespace)
{ return ScannerReadString(*this,out,stopAtWhitespace); }
static void ScannerAppend(std::wstring& out,wchar_t ch) { out+=ch; }
// FUNCTION: WON 0x006a53d0
// Reads a decimal or 0x hex integer with optional '-'; returns true when the cursor moved
// A lone '-' or '0x' without digits still counts as success
bool StringParser::ReadValue(int& out)
{
    this->SkipWhitespace();
    int start=this->mPos;
    int value=0;
    bool negative=false;
    if(this->GetChar(0)=='-') {negative=true;this->IncrementPos(1);}
    if(this->GetChar(0)=='0' && tolower(this->GetChar(1))=='x') {
        this->IncrementPos(2);
        while(isxdigit(this->GetChar(0))) {
            value<<=4;
            char ch=toupper(this->GetChar(0));
            if(ch>='0' && ch<='9') value+=ch-'0';
            else if(ch>='A' && ch<='F') value+=ch-'A'+10;
            else break;
            this->IncrementPos(1);
        }
    } else {
        while(isdigit(this->GetChar(0))) {
            value=value*10+this->GetChar(0)-'0';
            this->IncrementPos(1);
        }
    }
    if(negative) value=-value;
    if(this->mPos!=start) {
        out=value;
        return true;
    }
    return false;
}
// FUNCTION: WON 0x006a5550
// Reads a true/yes/on/y/t/1 or false/no/off/n/f/0 word; returns false for any other word
// An invalid word is consumed but leaves the output unchanged
bool StringParser::ReadValue(bool& out)
{
    this->SkipWhitespace();
    std::string token;
    while(!this->EndOfString()) {
        char ch=toupper(this->GetChar(0));
        if(!isalnum(ch)) break;
        token+=ch;
        ++this->mPos;
    }
    if(token=="TRUE" || token=="YES" || token=="ON" || token=="Y" || token=="T" || token=="1") out=true;
    else if(token=="FALSE" || token=="NO" || token=="OFF" || token=="N" || token=="F" || token=="0") out=false;
    else return false;
    return true;
}
// WStringToString is defined in StringUtil.cpp (0x6a6280)
std::string WStringToString(const std::wstring&);
// FUNCTION: WON 0x006a5820
// Appends True or False
void StringParser::ValueToString(const bool& value,std::string& out)
{
    if(value) out+="True";
    else out+="False";
}
// FUNCTION: WON 0x006a5a70
// Appends the integer in decimal
void StringParser::ValueToString(const int& value,std::string& out)
{
    char buffer[50];
    sprintf(buffer,"%d",value);
    out+=buffer;
}
// FUNCTION: WON 0x006a5c40
// Appends the 16-bit value in decimal
void StringParser::ValueToString(const unsigned short& value,std::string& out)
{
    int integer=value;
    StringParser::ValueToString(integer,out);
}
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
}

namespace WONAPI {
// No original address identified: WON SDK cursor matching
bool StringParser::StartsWith(const char *theStr)
{
    int originalPos = this->mPos;
    const unsigned char *p = reinterpret_cast<const unsigned char*>(theStr);
    while(*p!='\0' && !this->EndOfString())
    {
        if(*p!=this->GetChar(0))
        {
            this->mPos = originalPos;
            return false;
        }
        p++;
        this->mPos++;
    }
    if(this->EndOfString() && *p!='\0')
    {
        this->mPos = originalPos;
        return false;
    }
    return true;
}

// No original address identified; a matched prefix is not rewound when input follows it
bool StringParser::Is(const char *theStr)
{
    bool startsWith = this->StartsWith(theStr);
    return startsWith && this->EndOfString();
}

// No original address identified: WON SDK whitespace scanner
void StringParser::SkipNonWhitespace()
{
    while(!this->EndOfString() && !isspace(this->GetChar(0)))
        this->mPos++;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a4d70
// (int __cdecl WONAPI::ScannerHexValue(char)    [?ScannerHexValue@WONAPI@@YAHD@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a4f30
// (void __cdecl WONAPI::ScannerAppend(class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > &,char)    [?ScannerAppend@)
// TEMPLATE: WON 0x006a5310
// (void __cdecl WONAPI::ScannerAppend(class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short>)
