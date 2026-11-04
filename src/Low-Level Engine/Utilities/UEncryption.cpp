// C:\EEX\Low-Level Engine\Utilities\UEncryption.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005662f..0x10057218: the two file-static block helpers, then the exported functions in header order. __ftol
// (library) follows at 0x10057218.
//
// Shapes the binary proves:
// - the padding is an if/else chain (compare 1, 2, 3 in order), not a switch;
// - the 4-character pack is one expression; the chars are signed (movsx), so high-bit characters smear (as written);
// - UEEncryptUString appends through a wide string with the left operand cast to const char*: the original builds
//   UWideString(x), then a second UWideString from its buffer (implicit conversion for operator+(W, W)), then the
//   right operand; `UWideString(x) + y` alone picks operator+(W, const char*) or elides the second temporary.
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/UEncryption.h"

struct UEKey {
    unsigned long mShift;
    unsigned long mKey;
};

static const UEKey kKeys[4] = {
    { 3, 0xb7b226bd },
    { 5, 0x35013685 },
    { 11, 0x007445a5 },
    { 21, 0x5c46fcc5 },
};

// sum of the four bytes of a 32-bit value (the checksum term)
#define UEByteSum(x) (((x) & 0xff) + (((x) >> 8) & 0xff) + (((x) >> 16) & 0xff) + ((x) >> 24))

// FUNCTION: LLE 0x1005662f
// Encrypts a value with one of four rotate-and-xor keys into an 8-digit hex block
static void UEEncryptBlock(unsigned long inValue, unsigned long inKey, UString& outBlock)
{
    sprintf(outBlock, "%08X",
            ((inValue << kKeys[inKey].mShift) | (inValue >> (32 - kKeys[inKey].mShift))) ^ kKeys[inKey].mKey);
}

// FUNCTION: LLE 0x1005666c
// Decrypts an 8-digit hex block with one of four rotate-and-xor keys
static void UEDecryptBlock(UString& inBlock, unsigned long inKey, unsigned long& outValue)
{
    unsigned long theValue;
    sscanf(inBlock, "%X", &theValue);
    theValue ^= kKeys[inKey].mKey;
    outValue = (theValue >> kKeys[inKey].mShift) | (theValue << (32 - kKeys[inKey].mShift));
}

// FUNCTION: LLE 0x100566b6
// Encrypts a 16-digit hex authorization code as two key-2 blocks
void UEEncryptAuthorizationCode(UString& inCode, UString& outCode)
{
    UString theSubstring;
    UString theBlock;
    unsigned long theValue;

    theSubstring = inCode.GetSubstring(0, 8);
    sscanf(theSubstring, "%X", &theValue);
    UEEncryptBlock(theValue, 2, theBlock);
    outCode = theBlock;
    theSubstring = inCode.GetSubstring(8, 8);
    sscanf(theSubstring, "%X", &theValue);
    UEEncryptBlock(theValue, 2, theBlock);
    outCode += theBlock;
}

// FUNCTION: LLE 0x1005677d
// Decrypts a 16-digit authorization code into its two values
void UEDecryptAuthorizationCode(UString& inCode, unsigned long& outFirst, unsigned long& outSecond)
{
    UString theSubstring;

    theSubstring = inCode.GetSubstring(0, 8);
    UEDecryptBlock(theSubstring, 2, outFirst);
    theSubstring = inCode.GetSubstring(8, 8);
    UEDecryptBlock(theSubstring, 2, outSecond);
}

// FUNCTION: LLE 0x100567f9
// Encrypts two expiration values plus a byte-sum checksum as three key-3 blocks
void UEEncryptExpiration(unsigned long inFirst, unsigned long inSecond, UString& outString)
{
    UString theBlock;

    UEEncryptBlock(inFirst, 3, theBlock);
    outString = theBlock;
    UEEncryptBlock(inSecond, 3, theBlock);
    outString += theBlock;
    UEEncryptBlock(UEByteSum(inFirst) + UEByteSum(inSecond), 3, theBlock);
    outString += theBlock;
}

// FUNCTION: LLE 0x100568a1
// Decrypts two expiration values; returns whether the checksum block matches
bool UEDecryptExpiration(const UString& inString, unsigned long& outFirst, unsigned long& outSecond)
{
    UString theBlock;
    unsigned long theChecksum = 0;
    unsigned long theSum;

    theBlock = inString.GetSubstring(0, 8);
    UEDecryptBlock(theBlock, 3, outFirst);
    theBlock = inString.GetSubstring(8, 8);
    UEDecryptBlock(theBlock, 3, outSecond);
    theSum = UEByteSum(outFirst) + UEByteSum(outSecond);
    theBlock = inString.GetSubstring(16, 8);
    UEDecryptBlock(theBlock, 3, theChecksum);
    return theChecksum == theSum;
}

// FUNCTION: LLE 0x100569a1
// Encrypts a value and a GUID string (pads the caller's string with spaces to a multiple of 4) plus a checksum
void UEEncryptGUID(unsigned long inValue, UString& inGUID, unsigned long inKey, UString& outString)
{
    UString theBlock;

    if ((inGUID.GetLength() & 3) == 1)
        inGUID += "   ";
    else if ((inGUID.GetLength() & 3) == 2)
        inGUID += "  ";
    else if ((inGUID.GetLength() & 3) == 3)
        inGUID += " ";
    UEEncryptBlock(inValue, inKey, theBlock);
    outString = theBlock;
    UEEncryptBlock(inGUID.GetLength(), inKey, theBlock);
    outString = outString + theBlock;
    int i; for (i = 0; i < inGUID.GetLength(); i += 4) {
        unsigned long theValue =
            (((inGUID[i] << 8 | inGUID[i + 1]) << 8 | inGUID[i + 2]) << 8) | inGUID[i + 3];
        UEEncryptBlock(theValue, inKey, theBlock);
        outString += theBlock;
    }
    unsigned long theSum = UEByteSum(inValue) + UEByteSum(inGUID.GetLength());
    for (i = 0; i < inGUID.GetLength(); i++)
        theSum += inGUID[i];
    UEEncryptBlock(theSum, inKey, theBlock);
    outString += theBlock;
}

// FUNCTION: LLE 0x10056b2a
// Decrypts a value and a GUID string (appended to outGUID, padding kept); returns whether the checksum matches
bool UEDecryptGUID(const UString& inString, unsigned long inKey, unsigned long& outValue, UString& outGUID)
{
    UString theBlock;
    unsigned long theChecksum = 0;
    unsigned long theLength;
    unsigned long theValue;
    unsigned long theSum;

    theBlock = inString.GetSubstring(0, 8);
    UEDecryptBlock(theBlock, inKey, outValue);
    theBlock = inString.GetSubstring(8, 8);
    UEDecryptBlock(theBlock, inKey, theLength);
    int i; for (i = 16; i < theLength * 2 + 16; i += 8) {
        theBlock = inString.GetSubstring(i, 8);
        UEDecryptBlock(theBlock, inKey, theValue);
        sprintf(theBlock, "%c%c%c%c", (char)(theValue >> 24), (char)(theValue >> 16), (char)(theValue >> 8),
                (char)theValue);
        outGUID += theBlock;
    }
    theSum = UEByteSum(outValue) + UEByteSum(theLength);
    for (i = 0; i < outGUID.GetLength(); i++)
        theSum += outGUID[i];
    theBlock = inString.GetSubstring(theLength * 2 + 16, 8);
    UEDecryptBlock(theBlock, inKey, theChecksum);
    return theChecksum == theSum;
}

// FUNCTION: LLE 0x10056cd7
// Encrypts a string with key 0: its length, padded length, 4-character blocks and a checksum
void UEEncryptUString(const UString& inString, UString& outString)
{
    UString theString(inString);
    UString theBlock;

    UEEncryptBlock(inString.GetLength(), 0, theBlock);
    outString = theBlock;
    if ((inString.GetLength() & 3) == 1)
        theString = UString((const char*)UWideString(theString) + UWideString("   "));
    else if ((inString.GetLength() & 3) == 2)
        theString += UString("  ");
    else if ((inString.GetLength() & 3) == 3)
        theString += UString(" ");
    UEEncryptBlock(theString.GetLength(), 0, theBlock);
    outString = UString((const char*)UWideString(outString) + UWideString(theBlock));
    int i; for (i = 0; i < theString.GetLength(); i += 4) {
        unsigned long theValue =
            (((theString[i] << 8 | theString[i + 1]) << 8 | theString[i + 2]) << 8) | theString[i + 3];
        UEEncryptBlock(theValue, 0, theBlock);
        outString = UString((const char*)UWideString(outString) + UWideString(theBlock));
    }
    unsigned long theSum = UEByteSum(theString.GetLength()) + UEByteSum(inString.GetLength());
    for (i = 0; i < theString.GetLength(); i++)
        theSum += theString[i];
    UEEncryptBlock(theSum, 0, theBlock);
    outString = UString((const char*)UWideString(outString) + UWideString(theBlock));
}

// FUNCTION: LLE 0x1005703f
// Decrypts a string written by UEEncryptUString; returns whether the checksum matches
bool UEDecryptUString(const UString& inString, UString& outString)
{
    UString theBlock;
    UString theString;
    unsigned long theChecksum = 0;
    unsigned long theLength;
    unsigned long thePaddedLength;
    unsigned long theValue;
    unsigned long theSum;

    theBlock = inString.GetSubstring(0, 8);
    UEDecryptBlock(theBlock, 0, theLength);
    theBlock = inString.GetSubstring(8, 8);
    UEDecryptBlock(theBlock, 0, thePaddedLength);
    int i; for (i = 16; i < thePaddedLength * 2 + 16; i += 8) {
        theBlock = inString.GetSubstring(i, 8);
        UEDecryptBlock(theBlock, 0, theValue);
        sprintf(theBlock, "%c%c%c%c", (char)(theValue >> 24), (char)(theValue >> 16), (char)(theValue >> 8),
                (char)theValue);
        theString += theBlock;
    }
    outString = theString.GetSubstring(0, theLength);
    theSum = UEByteSum(thePaddedLength) + UEByteSum(theLength);
    for (i = 0; i < theString.GetLength(); i++)
        theSum += theString[i];
    theBlock = inString.GetSubstring(thePaddedLength * 2 + 16, 8);
    UEDecryptBlock(theBlock, 0, theChecksum);
    return theChecksum == theSum;
}
