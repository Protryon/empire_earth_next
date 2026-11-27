// C:\EEX\Low-Level Engine\Utilities\UString.h (RECONSTRUCTION; UString.cpp 0x82c43..0x837ad). An 8-bit string with
// a capacity (mCapacity counts the terminator). Requires windows.h-free declarations only.
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

#include "lle/LLEForward.h"

class LLE_API UString {                    // proven sizeof 0x10 (TCP DetectedGame: UStrings at +0x20, +0x30)
public:
    UString() { ee_ustring_init(&this->mState); }
    UString(const UString& inString) { ee_ustring_init_copy(&this->mState, &inString.mState); }
    UString(const char* inString) { ee_ustring_init_text(&this->mState, inString); }
    UString(unsigned long inCapacity) { ee_ustring_init_capacity(&this->mState, inCapacity); }
    virtual ~UString() { ee_ustring_free(&this->mState); }
    UString& operator=(const UString& inString) { return this->Do(ee_ustring_assign, &inString.mState); }
    UString& operator=(const char* inString) { return this->Do(ee_ustring_assign_text, inString); }
    // exported ??BUString@@QBEPBDXZ = `mov eax,[ecx+4]; ret`; inlined at call sites (TCP 0x188e)
    operator const char*() const { return this->mState.string; }
    // exported ?GetLength@UString@@QBEKXZ = `mov eax,[ecx+8]; ret`; inlined in TCP 0x287c
    unsigned long GetLength() const { return this->mState.length; }
    bool Equals(const char* inString) const { return ee_ustring_compare(*this, inString) == 0; }
    bool Equals(const char* inString, unsigned long inCount) const { return ee_ustring_compare_count(*this, inString, inCount) == 0; }
    bool EqualsIgnoreCase(const char* inString) const { return ee_ustring_equals_ignore_case(*this, inString, false, 0); }
    bool EqualsIgnoreCase(const char* inString, unsigned long inCount) const { return ee_ustring_equals_ignore_case(*this, inString, true, inCount); }
    unsigned long SizeOf() { return ee_ustring_size_of(&this->mState); }   // 0x1220: mCapacity + 0xc
    // port: the text is UTF-8 (decision D6), so the case change is the platform's (the original's byte-wise tolower/
    // toupper would have broken a multi-byte character under a code-page locale)
    void ToUpper() { ee_ustring_change_case(&this->mState, true); }
    char operator[](int inIndex) const { return ee_ustring_at(&this->mState, inIndex); }
    UString& operator+=(const UString& inString) { return this->Do(ee_ustring_append, &inString.mState); }
    UString& operator+=(const char* inString) { return this->Do(ee_ustring_append_text, inString); }
    UString& Copy(const UString& inString, unsigned long inStart, unsigned long inCount, unsigned long inDestination,
                  bool inResize)
        { return this->Do(ee_ustring_copy, &inString.mState, inStart, inCount, inDestination, inResize); }
    UString& Copy(const char* inString, unsigned long inStart, unsigned long inCount, unsigned long inDestination,
                  bool inResize)
        { return this->Do(ee_ustring_copy_text, inString, inStart, inCount, inDestination, inResize); }
    // Find/FindBackwards return GetLength() when not found
    unsigned long Find(const UString& inString, unsigned long inStart) const
        { return ee_ustring_find(&this->mState, inString, inString.GetLength(), inStart, false); }
    unsigned long Find(const char* inString, unsigned long inStart) const { return ee_ustring_find(&this->mState, inString, strlen(inString), inStart, false); }
    UString GetSubstring(unsigned long inStart, unsigned long inLength) const
        { return UString(kUnset, ee_ustring_substring, &this->mState, inStart, inLength); }
    UString GetIndexedSubstring(unsigned long inFirst, unsigned long inLast) const
        { return UString(kUnset, ee_ustring_indexed_substring, &this->mState, inFirst, inLast); }
    // the length and the characters, both counting the terminator (lle/FSFile.h: on a failed read the string is
    // left empty and the error returned)
    long Load(FSFile& inFile) { return ee_ustring_load_file(&this->mState, &inFile); }
    long Save(FSFile& inFile) const { return ee_ustring_save_file(&this->mState, &inFile); }

    enum Unset { kUnset };                  // a result the Rust side initialises
    explicit UString(Unset) {}
    // a result the Rust call inFill initialises (its first argument the record)
    template <class F, class... A> UString(Unset, F inFill, A... inArguments) { inFill(&this->mState, inArguments...); }
    // the Rust call inChange on the record, then the string itself (the assignments, appends and copies)
    template <class F, class... A> UString& Do(F inChange, A... inArguments)
    {
        inChange(&this->mState, inArguments...);
        return *this;
    }

    EeUString mState;                       // +0x04 mString, +0x08 mLength, +0x0c mCapacity (allocated bytes)
};

inline UString operator+(const UString& inLeft, const UString& inRight) { return UString(UString::kUnset, ee_ustring_concat, &inLeft.mState, &inRight.mState); }
inline UString operator+(const UString& inLeft, const char* inRight) { return UString(UString::kUnset, ee_ustring_concat_text, &inLeft.mState, inRight); }
// the comparisons: strcmp order (a UString on the right converts to its text)
inline bool operator==(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) == 0; }
inline bool operator!=(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) != 0; }
inline bool operator<(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) < 0; }
inline bool operator<=(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) <= 0; }
inline bool operator>(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) > 0; }
inline bool operator>=(const UString& inLeft, const char* inRight) { return ee_ustring_compare(inLeft, inRight) >= 0; }
// formats into the string (lle/LowLevelEngineCore.h UFormat); vsprintf's result
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgcc-compat"
template <class... A> LLE_PRINTF_TEMPLATE(2, 3) inline long sprintf(UString& outString, const char* inFormat, A... inArguments)
    { return UFormat(&outString.mState, ee_ustring_format_buffer, ee_ustring_format_end, inFormat, inArguments...); }
#pragma clang diagnostic pop

#endif
