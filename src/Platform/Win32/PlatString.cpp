// Platform layer, Win32: the MSVC runtime's string functions (include/platform/PlatString.h), called as before.
#include <mbctype.h>
#include <mbstring.h>
#include <stdlib.h>
#include <string.h>

#include "platform/PlatString.h"

extern "C" int PlatStrICmp(const char* inLeft, const char* inRight)
{
    return _stricmp(inLeft, inRight);
}

extern "C" int PlatStrNICmp(const char* inLeft, const char* inRight, size_t inCount)
{
    return _strnicmp(inLeft, inRight, inCount);
}

extern "C" char* PlatIToA(int inValue, char* outBuffer, int inRadix)
{
    return _itoa(inValue, outBuffer, inRadix);
}

extern "C" char* PlatStrDup(const char* inString)
{
    return _strdup(inString);
}

extern "C" size_t PlatMbsLen(const unsigned char* inString)
{
    return _mbslen(inString);
}

extern "C" size_t PlatMbsNBCnt(const unsigned char* inString, size_t inCharacters)
{
    return _mbsnbcnt(inString, inCharacters);
}

extern "C" size_t PlatMbsNCCnt(const unsigned char* inString, size_t inBytes)
{
    return _mbsnccnt(inString, inBytes);
}

extern "C" unsigned char* PlatMbsInc(const unsigned char* inCurrent)
{
    return _mbsinc(inCurrent);
}

extern "C" unsigned char* PlatMbsNInc(const unsigned char* inCurrent, size_t inCharacters)
{
    return _mbsninc(inCurrent, inCharacters);
}

extern "C" unsigned char* PlatMbsDec(const unsigned char* inStart, const unsigned char* inCurrent)
{
    return _mbsdec(inStart, inCurrent);
}

extern "C" int PlatMbsCmp(const unsigned char* inLeft, const unsigned char* inRight)
{
    return _mbscmp(inLeft, inRight);
}

extern "C" int PlatMbsICmp(const unsigned char* inLeft, const unsigned char* inRight)
{
    return _mbsicmp(inLeft, inRight);
}

extern "C" int PlatMbsNCmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inCharacters)
{
    return _mbsncmp(inLeft, inRight, inCharacters);
}

extern "C" int PlatMbsNBCmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inBytes)
{
    return _mbsnbcmp(inLeft, inRight, inBytes);
}

extern "C" int PlatMbsNBICmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inBytes)
{
    return _mbsnbicmp(inLeft, inRight, inBytes);
}

extern "C" unsigned char* PlatMbsNCpy(unsigned char* outDest, const unsigned char* inSource, size_t inCharacters)
{
    return _mbsncpy(outDest, inSource, inCharacters);
}

extern "C" unsigned char* PlatMbsNBCat(unsigned char* ioDest, const unsigned char* inSource, size_t inBytes)
{
    return _mbsnbcat(ioDest, inSource, inBytes);
}

extern "C" unsigned char* PlatMbsChr(const unsigned char* inString, unsigned int inCharacter)
{
    return const_cast<unsigned char*>(_mbschr(inString, inCharacter));
}

extern "C" unsigned char* PlatMbsLwr(unsigned char* ioString)
{
    return _mbslwr(ioString);
}

extern "C" unsigned char* PlatMbsUpr(unsigned char* ioString)
{
    return _mbsupr(ioString);
}

extern "C" int PlatIsMbcSpace(unsigned int inCharacter)
{
    return _ismbcspace(inCharacter);
}

extern "C" int PlatIsMbcDigit(unsigned int inCharacter)
{
    return _ismbcdigit(inCharacter);
}

extern "C" int PlatIsMbbLead(unsigned int inByte)
{
    return _ismbblead(inByte);
}
