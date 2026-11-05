// UFromChars: std::from_chars for float and double on every standard library. The MSVC STL has it; libstdc++ added
// the floating-point overloads in GCC 11 and libc++ only for macOS 13.3+, so elsewhere it is strtof/strtod on a
// terminated copy (correctly rounded on glibc and macOS; the game never changes the "C" locale). Accepts what
// from_chars' general format accepts for the callers' inputs (decimal with an optional exponent; the callers check
// the syntax first). Out-of-range values report std::errc::result_out_of_range.
#ifndef LLE_UFROMCHARS_H
#define LLE_UFROMCHARS_H

#include <charconv>
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <system_error>

template <class T> inline std::from_chars_result UFromChars(const char* inFirst, const char* inLast, T& outValue)
{
#ifdef _MSC_VER
    return std::from_chars(inFirst, inLast, outValue);
#else
    std::string theCopy(inFirst, inLast);
    char* theEnd = nullptr;
    errno = 0;
    T theValue = sizeof(T) == sizeof(float) ? (T)strtof(theCopy.c_str(), &theEnd) : (T)strtod(theCopy.c_str(), &theEnd);
    std::from_chars_result theResult;
    theResult.ptr = inFirst + (theEnd - theCopy.c_str());
    if (theEnd == theCopy.c_str()) {
        theResult.ptr = inFirst;
        theResult.ec = std::errc::invalid_argument;
    } else if (errno == ERANGE) {
        theResult.ec = std::errc::result_out_of_range;
    } else {
        theResult.ec = std::errc();
        outValue = theValue;
    }
    return theResult;
#endif
}

#endif
