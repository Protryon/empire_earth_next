// StringParser 64-bit read and sized integer formatting from the WON SDK
// No original address identified
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdio.h>
#include <ctype.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyParser.h"
namespace WONAPI {
// The SDK accumulates in an int even for __int64 output
template<class IntType>
static bool StringParser_ReadInt(StringParser *theParser, IntType &theInt)
{
	theParser->SkipWhitespace();
	int aNum = 0;
	int originalPos = theParser->GetPos();

	bool isNegative = false;
	if(theParser->GetChar(0)=='-')
	{
		isNegative = true;
		theParser->IncrementPos(1);
	}

	if(theParser->GetChar(0)=='0' && tolower(theParser->GetChar(1))=='x') // hex
	{
		theParser->IncrementPos(2);
		while(isxdigit(theParser->GetChar(0)))
		{
			aNum*=16;
			char aChar = toupper(theParser->GetChar(0));
			if(aChar>='0' && aChar<='9')
				aNum += aChar - '0';
			else if(aChar>='A' && aChar<='F')
				aNum += aChar - 'A' + 10;
			else
				break;

			theParser->IncrementPos(1);
		}
	}
	else // decimal
	{
		while(isdigit(theParser->GetChar(0)))
		{
			aNum*=10;
			aNum += theParser->GetChar(0) - '0';
			theParser->IncrementPos(1);
		}
	}

	if(isNegative)
		aNum*=-1;

	if(theParser->GetPos()!=originalPos)
	{
		theInt = aNum;
		return true;
	}
	else
		return false;
}

bool StringParser::ReadValue(__int64 &theInt64)
{
	return StringParser_ReadInt(this,theInt64);
}

void StringParser::ValueToString(const unsigned __int64 &theInt64, std::string &theStr)
{
	char aBuf[50];
	sprintf(aBuf,"%I64u",theInt64);
	theStr += aBuf;
}

void StringParser::ValueToString(const __int64 &theInt64, std::string &theStr)
{
	char aBuf[50];
	sprintf(aBuf,"%I64d",theInt64);
	theStr += aBuf;
}

void StringParser::ValueToString(const unsigned long &theLong, std::string &theStr)
{
	char aBuf[50];
	sprintf(aBuf,"%u",theLong);
	theStr += aBuf;
}

void StringParser::ValueToString(const long &theLong, std::string &theStr)
{
	char aBuf[50];
	sprintf(aBuf,"%d",theLong);
	theStr += aBuf;
}

void StringParser::ValueToString(const short &theShort, std::string &theStr)
{
	long aLong = theShort;
	StringParser::ValueToString(aLong, theStr);
}

void StringParser::ValueToString(const char &theByte, std::string &theStr)
{
	long aLong = theByte;
	StringParser::ValueToString(aLong, theStr);
}

void StringParser::ValueToString(const unsigned char &theByte, std::string &theStr)
{
	unsigned long aLong = theByte;
	StringParser::ValueToString(aLong, theStr);
}
}
