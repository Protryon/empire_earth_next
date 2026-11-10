// Platform layer: string functions the MSVC runtime has and standard C does not (see platform/Platform.h).
//
// Game text is UTF-8 on every system (docs/RUST_PLAN.md, decision D6; rust/crates/ee_plat_core, utf8.rs and
// string.rs): one implementation, no C runtime locale or code page behind it. Two groups:
//  - byte strings: case-insensitive compares (Unicode's simple lower-case folding, which for ASCII is MSVC
//    _stricmp's), integer formatting (_itoa), a malloc'ed copy (_strdup, free it with free());
//  - multibyte strings (the MSVC <mbstring.h> _mbs* contracts over UTF-8): a "character" is one code point, one to
//    four bytes; a byte that is not part of a well-formed sequence is one character of its own (never a step past the
//    terminator). "Characters" below are such characters, "bytes" are bytes. The game's UString/UWideString and the
//    UI's text editing are written on these. A character value passed to or returned by these functions
//    (PlatMbsChr, PlatIsMbcSpace, PlatUtf8Decode) is a code point.
// Same arguments and results as the MSVC functions they are named after.
#ifndef PLATFORM_PLATSTRING_H
#define PLATFORM_PLATSTRING_H

#include <stddef.h>
#include <stdint.h>

// printf-style argument checking for the engine's variadic formatters (format string argument f, first value a);
// a varargs value of the wrong class (an integer for %f, a pointer for %d) reads another register or slot on x64
#ifndef PLAT_PRINTF_FORMAT
#if defined(__clang__) || defined(__GNUC__)
#define PLAT_PRINTF_FORMAT(f, a) __attribute__((format(printf, f, a)))
#else
#define PLAT_PRINTF_FORMAT(f, a)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---- byte strings
int PlatStrICmp(const char* inLeft, const char* inRight);                       // _stricmp (the folded difference)
int PlatStrNICmp(const char* inLeft, const char* inRight, size_t inCount);      // _strnicmp (inCount bytes; a character started inside counts whole)
char* PlatIToA(int inValue, char* outBuffer, int inRadix);                      // _itoa (buffer of 33 bytes covers radix 2)
char* PlatStrDup(const char* inString);                                         // _strdup (NULL when out of memory)

// ---- multibyte strings
size_t PlatMbsLen(const unsigned char* inString);                                         // _mbslen: characters
size_t PlatMbsNBCnt(const unsigned char* inString, size_t inCharacters);                  // _mbsnbcnt: bytes of the first n characters
size_t PlatMbsNCCnt(const unsigned char* inString, size_t inBytes);                       // _mbsnccnt: characters in the first n bytes
unsigned char* PlatMbsInc(const unsigned char* inCurrent);                                // _mbsinc: the next character
unsigned char* PlatMbsNInc(const unsigned char* inCurrent, size_t inCharacters);          // _mbsninc: n characters on
unsigned char* PlatMbsDec(const unsigned char* inStart, const unsigned char* inCurrent);   // _mbsdec: the previous character
int PlatMbsCmp(const unsigned char* inLeft, const unsigned char* inRight);                // _mbscmp (-1, 0, 1: byte order, which is code point order)
int PlatMbsICmp(const unsigned char* inLeft, const unsigned char* inRight);               // _mbsicmp (the folded difference)
int PlatMbsNCmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inCharacters);    // _mbsncmp
int PlatMbsNBCmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inBytes);       // _mbsnbcmp
int PlatMbsNBICmp(const unsigned char* inLeft, const unsigned char* inRight, size_t inBytes);      // _mbsnbicmp
unsigned char* PlatMbsNCpy(unsigned char* outDest, const unsigned char* inSource, size_t inCharacters);  // _mbsncpy (a shorter source is padded with zeros to n bytes)
unsigned char* PlatMbsNBCat(unsigned char* ioDest, const unsigned char* inSource, size_t inBytes);       // _mbsnbcat (never a partial character)
unsigned char* PlatMbsChr(const unsigned char* inString, unsigned int inCharacter);       // _mbschr (a code point; 0 finds the terminator)
unsigned char* PlatMbsLwr(unsigned char* ioString);                                       // _mbslwr (in place; a mapping that changes the byte length is skipped)
unsigned char* PlatMbsUpr(unsigned char* ioString);                                       // _mbsupr (in place, as above)
int PlatIsMbcSpace(unsigned int inCharacter);                                             // _ismbcspace: the C locale's white space
int PlatIsMbcDigit(unsigned int inCharacter);                                             // _ismbcdigit: an ASCII digit
int PlatIsMbbLead(unsigned int inByte);                                                   // _ismbblead: the first byte of a multi-byte sequence

// ---- code points
// The character at inText (0 at its terminator or for NULL); outLength (may be NULL) gets its bytes (0 there). A
// stray byte decodes to 0xDC00 + the byte.
uint32_t PlatUtf8Decode(const char* inText, uint32_t* outLength);
// A character's bytes into outBuffer (room for 4, no terminator) -> their count. A surrogate or a value past
// U+10FFFF gives U+FFFD; a stray byte's character (0xDC80..0xDCFF) gives the byte back.
uint32_t PlatUtf8Encode(uint32_t inCharacter, char* outBuffer);
// The Windows-1252 byte of a character, -1 when the code page has none (GEFontBitmap's cells).
int PlatCp1252ByteOf(uint32_t inCharacter);
// Whether inLength bytes (PLAT_TEXT_TERMINATED: up to the terminator) are well-formed UTF-8: tells an old
// Windows-1252 input from the game's text where no file version says (UString::Load).
int PlatUtf8IsValid(const char* inText, uint32_t inLength);

// ---- UTF-8 <-> wide characters (UTF-16 code units on Windows, UTF-32 elsewhere), and the conversion of old
// Windows-1252 inputs (saves and files written before decision D6; the five bytes 1252 leaves undefined map to the
// C1 controls U+0081, U+008D, U+008F, U+0090, U+009D). A stray byte or a lone surrogate becomes U+FFFD.
// Converts inLength units of inText (PLAT_TEXT_TERMINATED: up to its terminating zero; zeros inside a counted text
// are converted as characters) into outText, at most inCapacity units, and terminates it when room is left ->
// the units written (not counting the terminator). outText NULL -> the units the whole text needs (no terminator).
#define PLAT_TEXT_TERMINATED 0xFFFFFFFFu
uint32_t PlatUtf8ToWide(const char* inText, uint32_t inLength, wchar_t* outText, uint32_t inCapacity);
uint32_t PlatWideToUtf8(const wchar_t* inText, uint32_t inLength, char* outText, uint32_t inCapacity);
uint32_t PlatCp1252ToUtf8(const char* inText, uint32_t inLength, char* outText, uint32_t inCapacity);

#ifdef __cplusplus
}
#endif

#endif
