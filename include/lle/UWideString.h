// C:\EEX\Low-Level Engine\Utilities\UWideString.h (RECONSTRUCTION; UWideString.cpp 0x838f7..0x847e1). A multibyte
// (MBCS) string: lengths through _mbslen/_mbsnbcnt; UTF-8 since decision D6 (platform/PlatString.h). Capacity grows in blocks of sDefaultAllocationLength.
#ifndef LLE_UWIDESTRING_H
#define LLE_UWIDESTRING_H
#include "platform/PlatString.h"

#include <vector>
#include <mbstring.h>

class FSFile;
class ESChecksum;

class LLE_API UWideString {                // proven sizeof 0x14 (TCP NPIMGetStatus stack local)
public:
    UWideString();
    UWideString(const UWideString& inString);
    UWideString(const char* inString);
    UWideString(unsigned long inLength);    // capacity for inLength characters (2 * inLength + 2 bytes)
    virtual ~UWideString();
    UWideString& operator=(const UWideString& inString);
    UWideString& operator=(const char* inString);
    // exported ??BUWideString@@QBEPBDXZ = `mov eax,[ecx+4]; ret` (ICF-folded with UString's); TCP 0x2539 strcpy
    operator const char*() const { return this->mString; }
    void CopyString(const UWideString& inString, unsigned long inCount);
    void CopyString(const char* inString, unsigned long inCount);
    char operator[](int inIndex) const;
    UWideString& operator+=(const UWideString& inString);
    UWideString& operator+=(const char* inString);
    void Export(unsigned char* outData) const;
    unsigned long Find(const UWideString& inString, unsigned long inStart) const;
    unsigned long Find(const char* inString, unsigned long inStart) const;
    unsigned long GetBytesUsedToSave() const;
    void GetChecksum(ESChecksum& ioChecksum);
    unsigned long GetExportSize() const;
    UWideString GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const;
    unsigned long Import(unsigned char* inData);
    void InsertString(const UWideString& inString, unsigned long inPosition, bool inReplace);
    void InsertString(const char* inString, unsigned long inPosition, bool inReplace);
    long Load(FSFile& inFile);
    long Save(FSFile& inFile) const;
    bool Tokenize(const UWideString& inSeparators, std::vector<UWideString>& outTokens) const;
    // proven inline (exported COMDATs emitted by other units: 0x1000..0x1140 in Error System.cpp's range, the getters
    // ICF-folded at 0x4495 / 0x44c71 / 0x4012b)
    unsigned long GetLength() const { return this->mLength; }
    unsigned long GetTCHARLength() const { return this->mByteLength; }
    unsigned long GetAllocatedLength() const { return this->mCapacity; }
    void Clear() { this->mString[0] = 0; this->mLength = 0; this->mByteLength = 0; }
    bool Equals(const UWideString& inString) const
        { return PlatMbsCmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>((const char*)inString)) == 0; }
    bool Equals(const UWideString& inString, unsigned long inCount) const
        { return PlatMbsNBCmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>((const char*)inString),
                           inCount) == 0; }
    bool Equals(const char* inString) const
        { return PlatMbsCmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>(inString)) == 0; }
    bool Equals(const char* inString, unsigned long inCount) const
        { return PlatMbsNBCmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>(inString), inCount) == 0; }
    bool EqualsIgnoreCase(const UWideString& inString) const
        { return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>((const char*)inString)) == 0; }
    bool EqualsIgnoreCase(const UWideString& inString, unsigned long inCount) const
        { return PlatMbsNBICmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>((const char*)inString),
                            inCount) == 0; }
    bool EqualsIgnoreCase(const char* inString) const
        { return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>(inString)) == 0; }
    bool EqualsIgnoreCase(const char* inString, unsigned long inCount) const
        { return PlatMbsNBICmp(reinterpret_cast<const unsigned char*>((const char*)*this), reinterpret_cast<const unsigned char*>(inString), inCount) == 0; }
    void ToLower() { PlatMbsLwr(reinterpret_cast<unsigned char*>(this->mString)); }
    void ToUpper() { PlatMbsUpr(reinterpret_cast<unsigned char*>(this->mString)); }
    // Replaces the buffer with an empty one of inSize + 1 bytes. (The original dropped the fresh buffer and pointed
    // mString at a "" literal, which Import then overwrote and the destructor freed.)
    void Reallocate(unsigned long inSize)
    {
        delete[] this->mString;
        this->mLength = 0;
        this->mByteLength = 0;
        this->mCapacity = inSize + 1;
        this->mString = new char[this->mCapacity];
        this->mString[0] = '\0';
    }
    // after Reallocate: the first unit emits it last (0x10001136)
#ifdef LOWLEVELENGINE_EXPORTS
    static void SetDefaultAllocationLength(unsigned long inLength) { UWideString::sDefaultAllocationLength = inLength; }
#else
    static void SetDefaultAllocationLength(unsigned long inLength);
#endif
private:
    friend LLE_API UWideString operator+(const UWideString& inLeft, const UWideString& inRight);
    friend LLE_API UWideString operator+(const UWideString& inLeft, const char* inRight);
    friend LLE_API long sprintf(UWideString& outString, const char* inFormat, ...);   // sets the lengths

    char* mString;                          // +0x04
    unsigned long mByteLength;              // +0x08 bytes (_mbsnbcnt)
    unsigned long mLength;                  // +0x0c characters (_mbslen)
    unsigned long mCapacity;                // +0x10 allocated bytes

    // Inferred engine-only visibility: see TSScheduler.h ($E numbering of the plug-ins)
#ifdef LOWLEVELENGINE_EXPORTS
    static unsigned long sDefaultAllocationLength;   // exported ?sDefaultAllocationLength@UWideString@@0KA (0x10da58)
#endif
};

LLE_API UWideString operator+(const UWideString& inLeft, const UWideString& inRight);
LLE_API UWideString operator+(const UWideString& inLeft, const char* inRight);
LLE_API bool operator==(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator==(const UWideString& inLeft, const char* inRight);
LLE_API bool operator!=(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator!=(const UWideString& inLeft, const char* inRight);
LLE_API bool operator<(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator<(const UWideString& inLeft, const char* inRight);
LLE_API bool operator<=(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator<=(const UWideString& inLeft, const char* inRight);
LLE_API bool operator>(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator>(const UWideString& inLeft, const char* inRight);
LLE_API bool operator>=(const UWideString& inLeft, const UWideString& inRight);
LLE_API bool operator>=(const UWideString& inLeft, const char* inRight);
LLE_API long sprintf(UWideString& outString, const char* inFormat, ...) PLAT_PRINTF_FORMAT(2, 3);

#endif
