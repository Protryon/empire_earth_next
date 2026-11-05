// Platform layer: string functions the MSVC runtime has and standard C does not (see platform/Platform.h).
//
// Two groups:
//  - byte strings: case-insensitive compares (ASCII letters; MSVC _stricmp/_strnicmp in the "C" locale), integer
//    formatting (_itoa), a malloc'ed copy (_strdup, free it with free());
//  - multibyte strings (MSVC <mbstring.h>, _mbs*): text in the process's multibyte code page, where a character is
//    one byte or a lead byte plus a trail byte (the MSVC runtime: the system's ANSI code page, so double-byte on
//    Japanese, Chinese and Korean Windows). "Characters" below are such characters, "bytes" are bytes. The game's
//    UString/UWideString and the UI's text editing are written on these. Another implementation picks its own
//    encoding (single-byte, or UTF-8 with these functions stepping over UTF-8 sequences) and must agree with the
//    font and text input code; until then only Win32's behaviour is defined.
// Same arguments and results as the MSVC functions they are named after.
#ifndef PLATFORM_PLATSTRING_H
#define PLATFORM_PLATSTRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- byte strings
int PlatStrICmp(const char* inLeft, const char* inRight);                       // _stricmp
int PlatStrNICmp(const char* inLeft, const char* inRight, size_t inCount);      // _strnicmp
char* PlatIToA(int inValue, char* outBuffer, int inRadix);                      // _itoa (buffer of 33 bytes covers radix 2)
char* PlatStrDup(const char* inString);                                         // _strdup (NULL when out of memory)

// ---- multibyte strings
size_t PlatMbsLen(const unsigned char* inString);                                         // _mbslen: characters
size_t PlatMbsNBCnt(const unsigned char* inString, size_t inCharacters);                  // _mbsnbcnt: bytes of the first n characters
size_t PlatMbsNCCnt(const unsigned char* inString, size_t inBytes);                       // _mbsnccnt: characters in the first n bytes
unsigned char* PlatMbsInc(const unsigned char* inCurrent);                                // _mbsinc: the next character
unsigned char* PlatMbsNInc(const unsigned char* inCurrent, size_t inCharacters);          // _mbsninc: n characters on
unsigned char* PlatMbsDec(const unsigned char* inStart, const unsigned char* inCurrent);   // _mbsdec: the previous character
int PlatMbsCmp(const unsigned char* inLeft, const unsigned char* inRight);                // _mbscmp
int PlatMbsICmp(const unsigned char* inLeft, const unsigned char* inRight);               // _mbsicmp
int PlatMbsNCmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inCharacters);    // _mbsncmp
int PlatMbsNBCmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inBytes);       // _mbsnbcmp
int PlatMbsNBICmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inBytes);      // _mbsnbicmp
unsigned char* PlatMbsNCpy(unsigned char* outDest, const unsigned char* inSource, size_t inCharacters);  // _mbsncpy
unsigned char* PlatMbsNBCat(unsigned char* ioDest, const unsigned char* inSource, size_t inBytes);       // _mbsnbcat
unsigned char* PlatMbsChr(const unsigned char* inString, unsigned int inCharacter);       // _mbschr
unsigned char* PlatMbsLwr(unsigned char* ioString);                                       // _mbslwr (in place)
unsigned char* PlatMbsUpr(unsigned char* ioString);                                       // _mbsupr (in place)
int PlatIsMbcSpace(unsigned int inCharacter);                                             // _ismbcspace
int PlatIsMbcDigit(unsigned int inCharacter);                                             // _ismbcdigit
int PlatIsMbbLead(unsigned int inByte);                                                   // _ismbblead: a lead byte

#ifdef __cplusplus
}
#endif

#endif
