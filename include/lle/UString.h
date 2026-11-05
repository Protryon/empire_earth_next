// C:\EEX\Low-Level Engine\Utilities\UString.h (RECONSTRUCTION; UString.cpp 0x82c43..0x837ad). An 8-bit string with
// a capacity (mCapacity counts the terminator). Requires <vector> (Tokenize) and windows.h-free declarations only.
#ifndef LLE_USTRING_H
#define LLE_USTRING_H
#include "platform/PlatString.h"

#include <vector>
#include <string.h>
#include <ctype.h>

class FSFile;

class LLE_API UString {                    // proven sizeof 0x10 (TCP DetectedGame: UStrings at +0x20, +0x30)
public:
    UString();
    UString(const UString& inString);
    UString(const char* inString);
    UString(unsigned long inCapacity);
    virtual ~UString();
    UString& operator=(const UString& inString);
    UString& operator=(const char* inString);
    // exported ??BUString@@QBEPBDXZ = `mov eax,[ecx+4]; ret`; inlined at call sites (TCP 0x188e)
    operator const char*() const { return this->mString; }
    // exported ?GetLength@UString@@QBEKXZ = `mov eax,[ecx+8]; ret`; inlined in TCP 0x287c
    unsigned long GetLength() const { return this->mLength; }
    // exported, ICF-folded (0x4495, with UWideString::GetLength)
    unsigned long GetAllocatedLength() const { return this->mCapacity; }
    // proven inline: exported COMDATs emitted by the first unit (Error System.cpp, 0x1140..0x1279), members pushed
    // straight from memory
    bool Equals(const UString& inString) const { return strcmp(this->mString, inString.mString) == 0; }
    bool Equals(const UString& inString, unsigned long inCount) const
        { return strncmp(this->mString, inString.mString, inCount) == 0; }
    bool Equals(const char* inString) const { return strcmp(this->mString, inString) == 0; }
    bool Equals(const char* inString, unsigned long inCount) const { return strncmp(this->mString, inString, inCount) == 0; }
    bool EqualsIgnoreCase(const UString& inString) const { return PlatStrICmp(this->mString, inString.mString) == 0; }
    bool EqualsIgnoreCase(const UString& inString, unsigned long inCount) const
        { return PlatStrNICmp(this->mString, inString.mString, inCount) == 0; }
    bool EqualsIgnoreCase(const char* inString) const { return PlatStrICmp(this->mString, inString) == 0; }
    bool EqualsIgnoreCase(const char* inString, unsigned long inCount) const
        { return PlatStrNICmp(this->mString, inString, inCount) == 0; }
#ifdef LOWLEVELENGINE_EXPORTS
    static void SetDefaultAllocationLength(unsigned long inLength) { UString::sDefaultAllocationLength = inLength; }
#else
    static void SetDefaultAllocationLength(unsigned long inLength);
#endif
    unsigned long SizeOf() { return this->mCapacity + 3 * sizeof(unsigned long); }  // 0x1220: mCapacity + 0xc
    void ToLower()
    {
        for (unsigned long i = 0; i < this->mLength; i++)
            this->mString[i] = tolower(this->mString[i]);
    }
    void ToUpper()
    {
        for (unsigned long i = 0; i < this->mLength; i++)
            this->mString[i] = toupper(this->mString[i]);
    }
    char operator[](int inIndex) const;
    UString& operator+=(const UString& inString);
    UString& operator+=(const char* inString);
    void Compact();
    UString& Copy(const UString& inString, unsigned long inStart, unsigned long inCount, unsigned long inDestination,
                  bool inResize);
    UString& Copy(const char* inString, unsigned long inStart, unsigned long inCount, unsigned long inDestination,
                  bool inResize);
    // Find/FindBackwards return GetLength() when not found
    unsigned long Find(const UString& inString, unsigned long inStart) const;
    unsigned long Find(const char* inString, unsigned long inStart) const;
    unsigned long FindBackwards(const UString& inString, unsigned long inStart) const;
    unsigned long FindBackwards(const char* inString, unsigned long inStart) const;
    unsigned long GetBytesUsedToSave() const;
    UString GetSubstring(unsigned long inStart, unsigned long inLength) const;
    UString GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const;
    long Load(FSFile& inFile);
    long Save(FSFile& inFile) const;
    bool Tokenize(const UString& inSeparators, std::vector<UString>& outTokens) const;
    bool NestedTokenize(const UString& inSeparators, const UString& inBrackets,
                        std::vector<UString>& outTokens) const;
private:
    // friends: the comparisons push mString straight from memory (an inline accessor would load it first)
    friend LLE_API bool operator==(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator==(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator!=(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator!=(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator<(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator<(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator<=(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator<=(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator>(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator>(const UString& inLeft, const char* inRight);
    friend LLE_API bool operator>=(const UString& inLeft, const UString& inRight);
    friend LLE_API bool operator>=(const UString& inLeft, const char* inRight);
    friend LLE_API long sprintf(UString& outString, const char* inFormat, ...);   // sets mLength

    char* mString;                          // +0x04
    unsigned long mLength;                  // +0x08
    unsigned long mCapacity;                // +0x0c allocated bytes (including the terminator)

    // Inferred engine-only visibility: see TSScheduler.h (a static data member consumes a VC6 `$E` number in
    // every unit that sees the class; the plug-ins' numbering shows they saw none)
#ifdef LOWLEVELENGINE_EXPORTS
    static unsigned long sDefaultAllocationLength;   // exported ?sDefaultAllocationLength@UString@@0KA (0x10da3c)
#endif
};

LLE_API UString operator+(const UString& inLeft, const UString& inRight);
LLE_API UString operator+(const UString& inLeft, const char* inRight);
LLE_API bool operator==(const UString& inLeft, const UString& inRight);
LLE_API bool operator==(const UString& inLeft, const char* inRight);
LLE_API bool operator!=(const UString& inLeft, const UString& inRight);
LLE_API bool operator!=(const UString& inLeft, const char* inRight);
LLE_API bool operator<(const UString& inLeft, const UString& inRight);
LLE_API bool operator<(const UString& inLeft, const char* inRight);
LLE_API bool operator<=(const UString& inLeft, const UString& inRight);
LLE_API bool operator<=(const UString& inLeft, const char* inRight);
LLE_API bool operator>(const UString& inLeft, const UString& inRight);
LLE_API bool operator>(const UString& inLeft, const char* inRight);
LLE_API bool operator>=(const UString& inLeft, const UString& inRight);
LLE_API bool operator>=(const UString& inLeft, const char* inRight);
LLE_API long sprintf(UString& outString, const char* inFormat, ...);

#endif
