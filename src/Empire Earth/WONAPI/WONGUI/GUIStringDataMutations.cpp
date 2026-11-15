// GUIStringData construction, assignment and editing
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <string.h>
#include <wchar.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/ee_won.h"
namespace WONAPI {
// Destroys the global empty GUIString at exit (once)
// (_$E31)
GUIString gEmptyGUIString;
// Clears the string data to empty with no text buffer
void GUIStringData::Init() { this->mLength=this->mCapacity=0; this->mText=NULL; }
// Creates empty string data with room for the given number of characters
GUIStringData::GUIStringData(unsigned short capacity) { this->Init(); this->resize(capacity,false); this->resize(0,false); }
// Copy constructor (emitted at 0x00727600)
GUIStringData::GUIStringData(const GUIStringData& other)
{ this->Init(); this->resize(other.mLength,false); memcpy(this->mText,other.mText,sizeof(wchar_t)*(this->mLength+1)); }
// Substring constructor (emitted at 0x007276f0); offset and length are not range-checked
GUIStringData::GUIStringData(const GUIStringData* other,unsigned short offset,unsigned short length)
{ this->Init(); this->resize(length,false); memcpy(this->mText,other->mText+offset,sizeof(wchar_t)*length); }
// Constructors from narrow, wide, std::string and std::wstring text (emitted at 0x007277e0..0x00727910)
GUIStringData::GUIStringData(const char* text) { this->Init(); this->AssignStr(text,strlen(text)); }
GUIStringData::GUIStringData(const wchar_t* text) { this->Init(); this->AssignWStr(text,wcslen(text)); }
GUIStringData::GUIStringData(const std::string& text) { this->Init(); this->AssignStr(text.c_str(),text.length()); }
GUIStringData::GUIStringData(const std::wstring& text) { this->Init(); this->AssignWStr(text.c_str(),text.length()); }
// The sink of ee_won_guistring_widen: the decoded units become the text
static void GUIStringDataAssignUnits(void* context, const void* units, size_t count)
{
    GUIStringData* data=static_cast<GUIStringData*>(context);
    data->resize((int)count,false);
    if(count!=0) memcpy(data->mText,units,count*sizeof(wchar_t));
}
// Replaces the text with narrow text of the given length, the game's UTF-8 (D6; the original widened each byte:
// Latin-1); a stray byte becomes U+FFFD (ee_won guistring.rs)
void GUIStringData::AssignStr(const char* text,int length)
{
    this->resize(0,false);
    if(length>0)
        ee_won_guistring_widen(reinterpret_cast<const uint8_t*>(text),(size_t)length,sizeof(wchar_t),
                               GUIStringDataAssignUnits,this);
}
// Replaces the text with wide text of the given length
void GUIStringData::AssignWStr(const wchar_t* text,int length)
{
    this->resize(length,false);
    this->mLength=length;
    for(int i=0;i<length;++i) this->mText[i]=text[i];
}
// Sets the length to offset+length and fills that range with one character
void GUIStringData::Fill(int character,unsigned short offset,unsigned short length)
{
    int end=offset+length;
    this->resize(end,false);
    for(int i=offset;i<end;++i) this->mText[i]=character;
}
// Replaces the text with a range of another string's text
void GUIStringData::assign(const GUIStringData* text,unsigned short offset,unsigned short length)
{
    int aLen=length;
    this->Reserve(aLen,false);
    this->resize(0,false);
    int aLength=this->mLength;
    this->resize(aLength+aLen,true);
    memcpy(this->mText+aLength,text->mText+offset,aLen*sizeof(wchar_t));
}
}

namespace WONAPI {
// Creates string data from narrow text of the given length (not identified in the original exe)
GUIStringData::GUIStringData(const char *theStr, unsigned short theLen)
{
	this->Init();
	this->AssignStr(theStr,theLen);
}

// Creates string data from wide text of the given length (not identified in the original exe)
GUIStringData::GUIStringData(const wchar_t *theStr, unsigned short theLen)
{
	this->Init();
	this->AssignWStr(theStr,theLen);
}
}

#include <string.h>
namespace WONAPI {
// Removes theLength characters (0 or past the end: the rest) from theStartChar; out-of-range starts
// do nothing (not identified in the original exe)
void GUIStringData::erase(int theStartChar, int theLength)
{
//	wstring aStr = GetWString();
//	aStr.erase(theStartChar,theLength);
//	AssignWStr(aStr.c_str(),aStr.length());
//	return;
	
	if (theStartChar>this->mLength || theStartChar < 0)
		return;

	int aEndChar = theStartChar+theLength;
	if(theLength==0 || aEndChar>this->mLength)
		aEndChar = this->mLength;

	if(aEndChar!=this->mLength)
		memmove(this->mText+theStartChar,this->mText+aEndChar,sizeof(wchar_t)*(this->mLength-aEndChar));
	this->resize(this->mLength-(aEndChar-theStartChar),false);
}

// Inserts another string's text at theStartChar, if within the length (not identified in the original exe)
void GUIStringData::insert(int theStartChar, const GUIStringData *theStr)
{
//	wstring aStr1 = GetWString();
//	wstring aStr2 = theStr->GetWString();
//	aStr1.insert(theStartChar,aStr2);
//	AssignWStr(aStr1.c_str(),aStr1.length());
//	return;

	if(theStartChar>this->mLength)
		return;

	int aOldLen = this->mLength;

	this->resize(this->mLength+theStr->mLength,false);
	if(theStartChar!=aOldLen)
		memmove(this->mText+theStartChar+theStr->mLength,this->mText+theStartChar,sizeof(wchar_t)*(aOldLen-theStartChar));
	memcpy(this->mText+theStartChar,theStr->mText,sizeof(wchar_t)*theStr->mLength);
}

// Sets the character at a position below the length; returns the character (not identified in the original exe)
int GUIStringData::setAt(int thePos, int theChar)
{
	if (thePos < this->mLength)
		this->mText[thePos] = (unsigned short)theChar;
	return theChar;
}
}
