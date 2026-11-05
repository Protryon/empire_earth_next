// Platform layer, POSIX: the MSVC runtime's string functions (include/platform/PlatString.h).
//
// The multibyte functions treat text as single-byte (every byte one character, no lead bytes): the game's data is
// Windows-1252 text, which is what the MSVC runtime does on a Western Windows too (a single-byte ANSI code page).
// Case mapping is ASCII only, as MSVC's in the "C" locale. A double-byte or UTF-8 encoding needs the fonts and the
// text input to agree first (PORT.md); until then this is the defined behaviour on POSIX.
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "platform/PlatString.h"

namespace {

inline int Lower(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

inline int Upper(unsigned char c)
{
    return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
}

}   // namespace

// ---- byte strings

extern "C" int PlatStrICmp(const char* inLeft, const char* inRight)
{
    // MSVC: both sides lower-cased, compared as unsigned bytes
    const unsigned char* a = reinterpret_cast<const unsigned char*>(inLeft);
    const unsigned char* b = reinterpret_cast<const unsigned char*>(inRight);
    for (;; a++, b++) {
        int x = Lower(*a), y = Lower(*b);
        if (x != y || x == 0)
            return x - y;
    }
}

extern "C" int PlatStrNICmp(const char* inLeft, const char* inRight, size_t inCount)
{
    const unsigned char* a = reinterpret_cast<const unsigned char*>(inLeft);
    const unsigned char* b = reinterpret_cast<const unsigned char*>(inRight);
    for (; inCount > 0; inCount--, a++, b++) {
        int x = Lower(*a), y = Lower(*b);
        if (x != y || x == 0)
            return x - y;
    }
    return 0;
}

extern "C" char* PlatIToA(int inValue, char* outBuffer, int inRadix)
{
    // MSVC _itoa: a minus sign only in radix 10; other radices format the value's 32-bit two's complement
    char theDigits[34];
    int n = 0;
    bool theNegative = inRadix == 10 && inValue < 0;
    unsigned int theValue = theNegative ? 0u - static_cast<unsigned int>(inValue) : static_cast<unsigned int>(inValue);
    if (inRadix < 2 || inRadix > 36) {
        outBuffer[0] = 0;
        return outBuffer;
    }
    do {
        unsigned int d = theValue % static_cast<unsigned int>(inRadix);
        theDigits[n++] = static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10);
        theValue /= static_cast<unsigned int>(inRadix);
    } while (theValue);
    char* p = outBuffer;
    if (theNegative)
        *p++ = '-';
    while (n > 0)
        *p++ = theDigits[--n];
    *p = 0;
    return outBuffer;
}

extern "C" char* PlatStrDup(const char* inString)
{
    return inString ? strdup(inString) : NULL;
}

// ---- multibyte strings (single-byte here)

extern "C" size_t PlatMbsLen(const unsigned char* inString)
{
    return strlen(reinterpret_cast<const char*>(inString));
}

extern "C" size_t PlatMbsNBCnt(const unsigned char* inString, size_t inCharacters)
{
    size_t n = 0;
    while (n < inCharacters && inString[n])
        n++;
    return n;
}

extern "C" size_t PlatMbsNCCnt(const unsigned char* inString, size_t inBytes)
{
    return PlatMbsNBCnt(inString, inBytes);
}

extern "C" unsigned char* PlatMbsInc(const unsigned char* inCurrent)
{
    return const_cast<unsigned char*>(inCurrent + 1);
}

extern "C" unsigned char* PlatMbsNInc(const unsigned char* inCurrent, size_t inCharacters)
{
    if (!inCurrent)
        return NULL;
    return const_cast<unsigned char*>(inCurrent + PlatMbsNBCnt(inCurrent, inCharacters));
}

extern "C" unsigned char* PlatMbsDec(const unsigned char* inStart, const unsigned char* inCurrent)
{
    if (inCurrent <= inStart)
        return NULL;
    return const_cast<unsigned char*>(inCurrent - 1);
}

extern "C" int PlatMbsCmp(const unsigned char* inLeft, const unsigned char* inRight)
{
    int c = strcmp(reinterpret_cast<const char*>(inLeft), reinterpret_cast<const char*>(inRight));
    return c < 0 ? -1 : c > 0 ? 1 : 0;
}

extern "C" int PlatMbsICmp(const unsigned char* inLeft, const unsigned char* inRight)
{
    // _mbsicmp folds to upper case (it differs from _stricmp for the bytes between 'Z' and 'a')
    for (;; inLeft++, inRight++) {
        int x = Upper(*inLeft), y = Upper(*inRight);
        if (x != y)
            return x < y ? -1 : 1;
        if (x == 0)
            return 0;
    }
}

extern "C" int PlatMbsNCmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inCharacters)
{
    return PlatMbsNBCmp(inLeft, inRight, inCharacters);
}

extern "C" int PlatMbsNBCmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inBytes)
{
    int c = strncmp(reinterpret_cast<const char*>(inLeft), reinterpret_cast<const char*>(inRight), inBytes);
    return c < 0 ? -1 : c > 0 ? 1 : 0;
}

extern "C" int PlatMbsNBICmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inBytes)
{
    for (; inBytes > 0; inBytes--, inLeft++, inRight++) {
        int x = Upper(*inLeft), y = Upper(*inRight);
        if (x != y)
            return x < y ? -1 : 1;
        if (x == 0)
            return 0;
    }
    return 0;
}

extern "C" unsigned char* PlatMbsNCpy(unsigned char* outDest, const unsigned char* inSource, size_t inCharacters)
{
    // as strncpy: pads with zeros when the source is shorter, no terminator when it is not
    size_t i = 0;
    for (; i < inCharacters && inSource[i]; i++)
        outDest[i] = inSource[i];
    for (; i < inCharacters; i++)
        outDest[i] = 0;
    return outDest;
}

extern "C" unsigned char* PlatMbsNBCat(unsigned char* ioDest, const unsigned char* inSource, size_t inBytes)
{
    unsigned char* p = ioDest + strlen(reinterpret_cast<char*>(ioDest));
    for (size_t i = 0; i < inBytes && inSource[i]; i++)
        *p++ = inSource[i];
    *p = 0;
    return ioDest;
}

extern "C" unsigned char* PlatMbsChr(const unsigned char* inString, unsigned int inCharacter)
{
    if (inCharacter > 0xFF)
        return NULL;
    return reinterpret_cast<unsigned char*>(
        const_cast<char*>(strchr(reinterpret_cast<const char*>(inString), static_cast<int>(inCharacter))));
}

extern "C" unsigned char* PlatMbsLwr(unsigned char* ioString)
{
    for (unsigned char* p = ioString; *p; p++)
        *p = static_cast<unsigned char>(Lower(*p));
    return ioString;
}

extern "C" unsigned char* PlatMbsUpr(unsigned char* ioString)
{
    for (unsigned char* p = ioString; *p; p++)
        *p = static_cast<unsigned char>(Upper(*p));
    return ioString;
}

extern "C" int PlatIsMbcSpace(unsigned int inCharacter)
{
    return inCharacter == ' ' || (inCharacter >= 0x09 && inCharacter <= 0x0D);
}

extern "C" int PlatIsMbcDigit(unsigned int inCharacter)
{
    return inCharacter >= '0' && inCharacter <= '9';
}

extern "C" int PlatIsMbbLead(unsigned int inByte)
{
    (void)inByte;
    return 0;
}
