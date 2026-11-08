// StringParser list and nested-list reads and formatting from the WON SDK
// No original address identified
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyParser.h"

namespace WONAPI {
// String ReadValue forms forward to the existing ReadString scanners
bool StringParser::ReadValue(std::string &theStr, bool stopAtWhitespace)
{
    return this->ReadString(theStr,stopAtWhitespace);
}
bool StringParser::ReadValue(std::wstring &theStr, bool stopAtWhitespace)
{
    return this->ReadString(theStr,stopAtWhitespace);
}

// Generic comma-separated list reader (the string-only form is in StringParserLists.cpp)
template<class T>
static bool ReadList(StringParser *theParser, std::list<T> &theList)
{
    while(true)
    {
        theList.push_back(T());
        if(!theParser->ReadValue(theList.back()))
        {
            theList.pop_back();
            return false;
        }
        if(!theParser->CheckNextChar(','))
            break;
    }
    return true;
}

bool StringParser::ReadValue(std::list<bool> &theList) { return ReadList<bool>(this,theList); }
bool StringParser::ReadValue(std::list<int> &theList) { return ReadList<int>(this,theList); }
bool StringParser::ReadValue(std::list<std::string> &theList) { return ReadList<std::string>(this,theList); }
bool StringParser::ReadValue(std::list<std::wstring> &theList) { return ReadList<std::wstring>(this,theList); }

// Parenthesized list-of-lists reader; on a failed element or missing ')' the groups read so far stay
template<class T>
static bool ReadListList(StringParser *theParser, std::list<std::list<T> > &theList)
{
    while(true)
    {
        if(!theParser->CheckNextChar('('))
            break;
        theList.push_back(std::list<T>());
        if(!ReadList<T>(theParser,theList.back()))
            return false;
        if(!theParser->CheckNextChar(')'))
            return false;
        if(!theParser->CheckNextChar(','))
            break;
    }
    return true;
}

bool StringParser::ReadValue(std::list<std::list<bool> > &theList) { return ReadListList<bool>(this,theList); }
bool StringParser::ReadValue(std::list<std::list<int> > &theList) { return ReadListList<int>(this,theList); }
bool StringParser::ReadValue(std::list<std::list<std::string> > &theList) { return ReadListList<std::string>(this,theList); }
bool StringParser::ReadValue(std::list<std::list<std::wstring> > &theList) { return ReadListList<std::wstring>(this,theList); }

// Generic list formatter: values separated by ", "
template<class T>
static void ListToString(const std::list<T> &theList, std::string &theStr)
{
    std::list<T>::const_iterator anItr = theList.begin();
    while(anItr!=theList.end())
    {
        StringParser::ValueToString(*anItr,theStr);
        ++anItr;
        if(anItr!=theList.end())
            theStr += ", ";
    }
}

void StringParser::ValueToString(const std::list<bool> &theVal,std::string &theStr) { ListToString<bool>(theVal,theStr); }
void StringParser::ValueToString(const std::list<int> &theVal,std::string &theStr) { ListToString<int>(theVal,theStr); }
void StringParser::ValueToString(const std::list<std::wstring> &theVal,std::string &theStr) { ListToString<std::wstring>(theVal,theStr); }

// Nested-list formatter: "(a, b), (c)"
template<class T>
static void ListListToString(const std::list<std::list<T> > &theList,std::string &theStr)
{
    std::list<std::list<T> >::const_iterator anItr = theList.begin();
    while(anItr!=theList.end())
    {
        theStr += "(";
        ListToString<T>(*anItr,theStr);
        theStr += ")";
        ++anItr;
        if(anItr!=theList.end())
            theStr += ", ";
    }
}

void StringParser::ValueToString(const std::list<std::list<bool> > &theVal,std::string &theStr) { ListListToString<bool>(theVal,theStr); }
void StringParser::ValueToString(const std::list<std::list<int> > &theVal,std::string &theStr) { ListListToString<int>(theVal,theStr); }
void StringParser::ValueToString(const std::list<std::list<std::string> > &theVal,std::string &theStr) { ListListToString<std::string>(theVal,theStr); }
void StringParser::ValueToString(const std::list<std::list<std::wstring> > &theVal,std::string &theStr) { ListListToString<std::wstring>(theVal,theStr); }
}
