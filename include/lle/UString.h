// C:\EEX\Low-Level Engine\Utilities\UString.h (RECONSTRUCTION; UString.cpp 0x82c43..0x837ad). An 8-bit string with
// a capacity (mCapacity counts the terminator). Requires <vector> (Tokenize) and windows.h-free declarations only.
// Whole in Rust (rust/crates/ee_base/src/ustring, LLE lane round 4): the class holds the Rust record by value (the original's
// buffer, length and capacity in their order: sizeof and every embedding class keep their layout), the buffer is
// Rust's, and every member forwards. sprintf stays a C++ formatter writing into the record's buffer.
#ifndef LLE_USTRING_H
#define LLE_USTRING_H
#include "platform/PlatString.h"
#include "rust/base/ustring.h"

#include <vector>
#include <string.h>
#include <ctype.h>

class FSFile;

// port (decision D6): game text is UTF-8. A string a file gives UString::Load or UWideString::Load is converted from
// Windows-1252 when it is not well-formed UTF-8, or always while a file written before the switch loads (a save of
// version < 0.255: the loader that read the version sets the flag around the load).
inline void USetLegacyTextLoad(bool inLegacy) { ee_ustring_set_legacy_text_load(inLegacy); }

class LLE_API UString {                    // proven sizeof 0x10 (TCP DetectedGame: UStrings at +0x20, +0x30)
public:
    UString() { ee_ustring_init(&this->mState); }
    UString(const UString& inString) { ee_ustring_init_copy(&this->mState, &inString.mState); }
    UString(const char* inString) { ee_ustring_init_text(&this->mState, inString); }
    UString(unsigned long inCapacity) { ee_ustring_init_capacity(&this->mState, inCapacity); }
    virtual ~UString();
    UString& operator=(const UString& inString) { ee_ustring_assign(&this->mState, &inString.mState); return *this; }
    UString& operator=(const char* inString) { ee_ustring_assign_text(&this->mState, inString); return *this; }
    // exported ??BUString@@QBEPBDXZ = `mov eax,[ecx+4]; ret`; inlined at call sites (TCP 0x188e)
    operator const char*() const { return this->mState.string; }
    // exported ?GetLength@UString@@QBEKXZ = `mov eax,[ecx+8]; ret`; inlined in TCP 0x287c
    unsigned long GetLength() const { return this->mState.length; }
    unsigned long GetAllocatedLength() const { return this->mState.capacity; }
    bool Equals(const UString& inString) const { return ee_ustring_compare(*this, inString) == 0; }
    bool Equals(const UString& inString, unsigned long inCount) const
        { return ee_ustring_compare_count(*this, inString, inCount) == 0; }
    bool Equals(const char* inString) const { return ee_ustring_compare(*this, inString) == 0; }
    bool Equals(const char* inString, unsigned long inCount) const
        { return ee_ustring_compare_count(*this, inString, inCount) == 0; }
    bool EqualsIgnoreCase(const UString& inString) const
        { return ee_ustring_equals_ignore_case(*this, inString, false, 0); }
    bool EqualsIgnoreCase(const UString& inString, unsigned long inCount) const
        { return ee_ustring_equals_ignore_case(*this, inString, true, inCount); }
    bool EqualsIgnoreCase(const char* inString) const { return ee_ustring_equals_ignore_case(*this, inString, false, 0); }
    bool EqualsIgnoreCase(const char* inString, unsigned long inCount) const
        { return ee_ustring_equals_ignore_case(*this, inString, true, inCount); }
    static void SetDefaultAllocationLength(unsigned long inLength) { ee_ustring_set_default_allocation(inLength); }
    unsigned long SizeOf() { return ee_ustring_size_of(&this->mState); }   // 0x1220: mCapacity + 0xc
    // port: the text is UTF-8 (decision D6), so the case change is the platform's (the original's byte-wise tolower/
    // toupper would have broken a multi-byte character under a code-page locale)
    void ToLower() { ee_ustring_change_case(&this->mState, false); }
    void ToUpper() { ee_ustring_change_case(&this->mState, true); }
    char operator[](int inIndex) const { return ee_ustring_at(&this->mState, inIndex); }
    UString& operator+=(const UString& inString) { ee_ustring_append(&this->mState, &inString.mState); return *this; }
    UString& operator+=(const char* inString) { ee_ustring_append_text(&this->mState, inString); return *this; }
    void Compact() { ee_ustring_compact(&this->mState); }
    UString& Copy(const UString& inString, unsigned long inStart, unsigned long inCount, unsigned long inDestination,
                  bool inResize)
        { ee_ustring_copy(&this->mState, &inString.mState, inStart, inCount, inDestination, inResize); return *this; }
    UString& Copy(const char* inString, unsigned long inStart, unsigned long inCount, unsigned long inDestination,
                  bool inResize)
        { ee_ustring_copy_text(&this->mState, inString, inStart, inCount, inDestination, inResize); return *this; }
    // Find/FindBackwards return GetLength() when not found
    unsigned long Find(const UString& inString, unsigned long inStart) const
        { return ee_ustring_find(&this->mState, inString, inString.GetLength(), inStart, false); }
    unsigned long Find(const char* inString, unsigned long inStart) const
        { return ee_ustring_find(&this->mState, inString, strlen(inString), inStart, false); }
    unsigned long FindBackwards(const UString& inString, unsigned long inStart) const
        { return ee_ustring_find(&this->mState, inString, inString.GetLength(), inStart, true); }
    unsigned long FindBackwards(const char* inString, unsigned long inStart) const
        { return ee_ustring_find(&this->mState, inString, strlen(inString), inStart, true); }
    unsigned long GetBytesUsedToSave() const { return ee_ustring_bytes_to_save(&this->mState); }
    UString GetSubstring(unsigned long inStart, unsigned long inLength) const
    {
        UString theResult(kUnset);
        ee_ustring_substring(&theResult.mState, &this->mState, inStart, inLength);
        return theResult;
    }
    UString GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const
    {
        UString theResult(kUnset);
        ee_ustring_indexed_substring(&theResult.mState, &this->mState, inFirst, inLast);
        return theResult;
    }
    long Load(FSFile& inFile);
    long Save(FSFile& inFile) const;
    bool Tokenize(const UString& inSeparators, std::vector<UString>& outTokens) const
        { return this->Tokens(inSeparators, NULL, outTokens); }
    bool NestedTokenize(const UString& inSeparators, const UString& inBrackets,
                        std::vector<UString>& outTokens) const
        { return this->Tokens(inSeparators, &inBrackets, outTokens); }
private:
    friend UString operator+(const UString& inLeft, const UString& inRight);
    friend UString operator+(const UString& inLeft, const char* inRight);
    friend LLE_API long sprintf(UString& outString, const char* inFormat, ...);   // sets mLength

    enum Unset { kUnset };                  // a result the Rust side initialises
    explicit UString(Unset) {}
    bool Tokens(const UString& inSeparators, const UString* inBrackets, std::vector<UString>& outTokens) const;

    EeUString mState;                       // +0x04 mString, +0x08 mLength, +0x0c mCapacity (allocated bytes)
};

inline UString operator+(const UString& inLeft, const UString& inRight)
{
    UString theResult(UString::kUnset);
    ee_ustring_concat(&theResult.mState, &inLeft.mState, &inRight.mState);
    return theResult;
}
inline UString operator+(const UString& inLeft, const char* inRight)
{
    UString theResult(UString::kUnset);
    ee_ustring_concat_text(&theResult.mState, &inLeft.mState, inRight);
    return theResult;
}
// the comparisons: strcmp order
inline bool operator==(const UString& inLeft, const UString& inRight) { return ee_ustring_compare(inLeft, inRight) == 0; }
inline bool operator==(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) == 0; }
inline bool operator!=(const UString& inLeft, const UString& inRight) { return ee_ustring_compare(inLeft, inRight) != 0; }
inline bool operator!=(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) != 0; }
inline bool operator<(const UString& inLeft, const UString& inRight) { return ee_ustring_compare(inLeft, inRight) < 0; }
inline bool operator<(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) < 0; }
inline bool operator<=(const UString& inLeft, const UString& inRight) { return ee_ustring_compare(inLeft, inRight) <= 0; }
inline bool operator<=(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) <= 0; }
inline bool operator>(const UString& inLeft, const UString& inRight) { return ee_ustring_compare(inLeft, inRight) > 0; }
inline bool operator>(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) > 0; }
inline bool operator>=(const UString& inLeft, const UString& inRight) { return ee_ustring_compare(inLeft, inRight) >= 0; }
inline bool operator>=(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) >= 0; }
// formats into the string's buffer (grown when the text does not fit: the original wrote past it), then measures it
LLE_API long sprintf(UString& outString, const char* inFormat, ...) PLAT_PRINTF_FORMAT(2, 3);

#endif
