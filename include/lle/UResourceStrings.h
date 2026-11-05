// The game's language strings (tools/lang_strings.py generates them from Language.rc into
// src/Empire Earth/Language/LanguageStrings.cpp): what UResourceFile::GetString returns, on every system.
#ifndef LLE_URESOURCESTRINGS_H
#define LLE_URESOURCESTRINGS_H

struct UResourceString {
    unsigned short mID;
    const char* mText;                      // code page 1252 (what LoadStringA gave on a Western Windows)
};

extern const unsigned short gResourceStringsLanguage;   // the strings' Windows LANGID
extern const unsigned long gResourceStringCount;
extern const UResourceString gResourceStrings[];        // sorted by mID

#endif
