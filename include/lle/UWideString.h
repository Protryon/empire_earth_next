// C:\EEX\Low-Level Engine\Utilities\UWideString.h (RECONSTRUCTION; UWideString.cpp 0x838f7..0x847e1). A multibyte
// (MBCS) string: lengths through _mbslen/_mbsnbcnt; UTF-8 since decision D6 (platform/PlatString.h). Capacity grows in blocks of sDefaultAllocationLength.
// Whole in Rust (rust/crates/ee_base/src/ustring wide.rs, LLE lane round 4): the class holds the Rust record by value (the
// original's buffer, byte length, length and capacity in their order: sizeof and every embedding class keep their
// layout), the buffer is Rust's, and every member forwards. sprintf stays a C++ formatter writing into the record.
#ifndef LLE_UWIDESTRING_H
#define LLE_UWIDESTRING_H
#include "platform/PlatString.h"
#include "rust/base/ustring.h"
#include "lle/UString.h"           // UFormat

#include <vector>
#include <string.h>
#include <mbstring.h>

class FSFile;
class ESChecksum;

class LLE_API UWideString {                // proven sizeof 0x14 (TCP NPIMGetStatus stack local)
public:
    UWideString() { ee_uwstring_init(&this->mState); }
    UWideString(const UWideString& inString) { ee_uwstring_init_copy(&this->mState, &inString.mState); }
    UWideString(const char* inString) { ee_uwstring_init_text(&this->mState, inString); }
    // capacity for inLength characters (2 * inLength + 2 bytes)
    UWideString(unsigned long inLength) { ee_uwstring_init_characters(&this->mState, inLength); }
    virtual ~UWideString() { ee_uwstring_free(&this->mState); }
    UWideString& operator=(const UWideString& inString) { return this->Do(ee_uwstring_assign, &inString.mState); }
    UWideString& operator=(const char* inString) { return this->Do(ee_uwstring_assign_text, inString); }
    // exported ??BUWideString@@QBEPBDXZ = `mov eax,[ecx+4]; ret` (ICF-folded with UString's); TCP 0x2539 strcpy
    operator const char*() const { return this->mState.string; }
    void CopyString(const UWideString& inString, unsigned long inCount)
        { ee_uwstring_copy(&this->mState, &inString.mState, inCount); }
    void CopyString(const char* inString, unsigned long inCount)
        { ee_uwstring_copy_text(&this->mState, inString, inCount); }
    char operator[](int inIndex) const { return ee_uwstring_at(&this->mState, inIndex); }
    UWideString& operator+=(const UWideString& inString) { return this->Do(ee_uwstring_append, (const char*)inString); }
    UWideString& operator+=(const char* inString) { return this->Do(ee_uwstring_append, inString); }
    void Export(unsigned char* outData) const { ee_uwstring_export(&this->mState, outData); }
    unsigned long Find(const UWideString& inString, unsigned long inStart) const
        { return ee_uwstring_find(&this->mState, &inString.mState, inStart); }
    unsigned long Find(const char* inString, unsigned long inStart) const
        { return ee_uwstring_find_text(&this->mState, inString, inStart); }
    unsigned long GetBytesUsedToSave() const { return ee_uwstring_bytes_to_save(&this->mState); }
    // adds a checksum of the characters (weighted by position, times the length) named after the text (ESChecksum.h)
    void GetChecksum(ESChecksum& ioChecksum);
    unsigned long GetExportSize() const { return ee_uwstring_export_size(&this->mState); }
    UWideString GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const
        { return UWideString(kUnset, ee_uwstring_indexed_substring, &this->mState, inFirst, inLast); }
    unsigned long Import(unsigned char* inData) { return ee_uwstring_import(&this->mState, inData); }
    void InsertString(const UWideString& inString, unsigned long inPosition, bool inReplace)
        { ee_uwstring_insert(&this->mState, inString, inPosition, inReplace); }
    void InsertString(const char* inString, unsigned long inPosition, bool inReplace)
        { ee_uwstring_insert(&this->mState, inString, inPosition, inReplace); }
    // the byte size and the bytes, both counting the terminator (lle/FSFile.h: on a failed read the string is left
    // empty and the error returned; an old Windows-1252 string is converted, D6)
    long Load(FSFile& inFile);
    long Save(FSFile& inFile) const;
    // splits at any of the separator characters, skipping empty tokens (ee_uwstring_tokens: the tokens' first and
    // last characters); always true
    bool Tokenize(const UWideString& inSeparators, std::vector<UWideString>& outTokens) const
        { return ee_uwstring_tokens(&this->mState, &inSeparators.mState, &outTokens, PushToken); }
    unsigned long GetLength() const { return this->mState.length; }
    unsigned long GetTCHARLength() const { return this->mState.byte_length; }
    unsigned long GetAllocatedLength() const { return this->mState.capacity; }
    void Clear() { ee_uwstring_clear(&this->mState); }
    bool Equals(const UWideString& inString) const { return PlatMbsCmp(Bytes(*this), Bytes(inString)) == 0; }
    bool Equals(const UWideString& inString, unsigned long inCount) const
        { return PlatMbsNBCmp(Bytes(*this), Bytes(inString), inCount) == 0; }
    bool Equals(const char* inString) const { return PlatMbsCmp(Bytes(*this), Bytes(inString)) == 0; }
    bool Equals(const char* inString, unsigned long inCount) const
        { return PlatMbsNBCmp(Bytes(*this), Bytes(inString), inCount) == 0; }
    bool EqualsIgnoreCase(const UWideString& inString) const { return PlatMbsICmp(Bytes(*this), Bytes(inString)) == 0; }
    bool EqualsIgnoreCase(const UWideString& inString, unsigned long inCount) const
        { return PlatMbsNBICmp(Bytes(*this), Bytes(inString), inCount) == 0; }
    bool EqualsIgnoreCase(const char* inString) const { return PlatMbsICmp(Bytes(*this), Bytes(inString)) == 0; }
    bool EqualsIgnoreCase(const char* inString, unsigned long inCount) const
        { return PlatMbsNBICmp(Bytes(*this), Bytes(inString), inCount) == 0; }
    void ToLower() { PlatMbsLwr(reinterpret_cast<unsigned char*>(this->mState.string)); }
    void ToUpper() { PlatMbsUpr(reinterpret_cast<unsigned char*>(this->mState.string)); }
    // Replaces the buffer with an empty one of inSize + 1 bytes. (The original dropped the fresh buffer and pointed
    // mString at a "" literal, which Import then overwrote and the destructor freed.)
    void Reallocate(unsigned long inSize) { ee_uwstring_reallocate(&this->mState, inSize); }
    static void SetDefaultAllocationLength(unsigned long inLength) { ee_uwstring_set_default_allocation(inLength); }
    static const unsigned char* Bytes(const char* inString) { return reinterpret_cast<const unsigned char*>(inString); }
private:
    friend UWideString operator+(const UWideString& inLeft, const UWideString& inRight);
    friend UWideString operator+(const UWideString& inLeft, const char* inRight);
    template <class... A> friend long sprintf(UWideString& outString, const char* inFormat, A... inArguments);   // sets the lengths

    enum Unset { kUnset };                  // a result the Rust side initialises
    explicit UWideString(Unset) {}
    // a result the Rust call inFill initialises (its first argument the record)
    template <class F, class... A> UWideString(Unset, F inFill, A... inArguments) { inFill(&this->mState, inArguments...); }
    // the Rust call inChange on the record, then the string itself (the assignments and appends)
    template <class F, class... A> UWideString& Do(F inChange, A... inArguments)
    {
        inChange(&this->mState, inArguments...);
        return *this;
    }
    // ee_uwstring_tokens' calls: the vector cleared (no record), then each token from its first to its last character
    static void PushToken(void* inTokens, const EeUWideString* inString, uint32_t inFirst, uint32_t inLast)
    {
        inString ? static_cast<std::vector<UWideString>*>(inTokens)->push_back(
                       UWideString(kUnset, ee_uwstring_indexed_substring, inString, inFirst, inLast))
                 : static_cast<std::vector<UWideString>*>(inTokens)->clear();
    }

    EeUWideString mState;                   // +0x04 mString, +0x08 mByteLength (_mbsnbcnt), +0x0c mLength
                                            // (_mbslen), +0x10 mCapacity (allocated bytes)
};

inline UWideString operator+(const UWideString& inLeft, const UWideString& inRight)
{
    return UWideString(UWideString::kUnset, ee_uwstring_concat, inLeft.GetLength() + inRight.GetLength() + 1,
                       &inLeft.mState, (const char*)inRight);
}
inline UWideString operator+(const UWideString& inLeft, const char* inRight)
{
    return UWideString(UWideString::kUnset, ee_uwstring_concat, inLeft.GetLength() + strlen(inRight) + 1,
                       &inLeft.mState, inRight);
}
// the comparisons: multibyte order
inline int UWCompare(const char* inLeft, const char* inRight)
{
    return PlatMbsCmp(UWideString::Bytes(inLeft), UWideString::Bytes(inRight));
}
inline bool operator==(const UWideString& inLeft, const UWideString& inRight) { return UWCompare(inLeft, inRight) == 0; }
inline bool operator==(const UWideString& inLeft, const char* inRight) { return UWCompare(inLeft, inRight) == 0; }
inline bool operator!=(const UWideString& inLeft, const UWideString& inRight) { return UWCompare(inLeft, inRight) != 0; }
inline bool operator!=(const UWideString& inLeft, const char* inRight) { return UWCompare(inLeft, inRight) != 0; }
inline bool operator<(const UWideString& inLeft, const UWideString& inRight) { return UWCompare(inLeft, inRight) < 0; }
inline bool operator<(const UWideString& inLeft, const char* inRight) { return UWCompare(inLeft, inRight) < 0; }
inline bool operator<=(const UWideString& inLeft, const UWideString& inRight) { return UWCompare(inLeft, inRight) <= 0; }
inline bool operator<=(const UWideString& inLeft, const char* inRight) { return UWCompare(inLeft, inRight) <= 0; }
inline bool operator>(const UWideString& inLeft, const UWideString& inRight) { return UWCompare(inLeft, inRight) > 0; }
inline bool operator>(const UWideString& inLeft, const char* inRight) { return UWCompare(inLeft, inRight) > 0; }
inline bool operator>=(const UWideString& inLeft, const UWideString& inRight) { return UWCompare(inLeft, inRight) >= 0; }
inline bool operator>=(const UWideString& inLeft, const char* inRight) { return UWCompare(inLeft, inRight) >= 0; }
// formats into the string's buffer, then measures it (lle/LowLevelEngineCore.h UFormat)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgcc-compat"
template <class... A> LLE_PRINTF_TEMPLATE(2, 3) inline long sprintf(UWideString& outString, const char* inFormat, A... inArguments)
    { return UFormat(&outString.mState, ee_uwstring_format_buffer, ee_uwstring_format_end, inFormat, inArguments...); }
#pragma clang diagnostic pop

#endif
