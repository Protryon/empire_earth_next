// The chat log (RECONSTRUCTION; header and file names inferred, from the class prefix EEUIECLE): the last 40 chat
// lines, saved with the game. The original's EEUIEChatLogEvents.cpp (0x1d321b..0x1d3a77) kept them in a form manager
// it attached to the chat log form 0x1d; the log is Rust's now (ee_screens chat_log.rs; these statics are its C++
// entry points, defined in UI Events/EEScreens.cpp) and there is no form manager. The chat log form's events are Rust's
// too (ee_screens chat.rs). The UI thread adds the lines (EEUserInterface 0x2787fe, 0x27892d); EESaveLoad calls
// Load/Save (0x14c383, 0x14c50b). Names are provisional. The entry points are inline over the Rust log: in
// EEScreens.cpp, the world's clear and the saved game's load and save named the screens' host (tools/cpp_deps.py).
// Requires lle/GEColor.h, lle/UWideString.h.
#ifndef EE_EEUIECHATLOGEVENTS_H
#define EE_EEUIECHATLOGEVENTS_H

#include "rust/game/screens.h"   // ee_screens_chat_log_*

class FSFile;

// One chat line (sizeof 0x3c in the original)
struct EEChatLogEntry {
    GEColor mColor;                         // +0x00
    UWideString mName;                      // +0x0c the sender (empty: a game message)
    UWideString mText;                      // +0x20
    unsigned long mType;                    // +0x34 (2: not logged, EEUserInterface 0x2787f7)
    unsigned long mTime;                    // +0x38 (provisional name)
};

class EEUIECLEFormManager {
public:
    static void AddEntry(const EEChatLogEntry& inEntry)   // 0x1d32bf (keeps the last 40)
    {
        EeScreensChatLogHead theHead = {{inEntry.mColor.mRed, inEntry.mColor.mGreen, inEntry.mColor.mBlue},
                                        (uint32_t)inEntry.mType, (uint32_t)inEntry.mTime};
        ee_screens_chat_log_add(&theHead, (const uint8_t*)(const char*)inEntry.mName, inEntry.mName.GetTCHARLength(),
                                (const uint8_t*)(const char*)inEntry.mText, inEntry.mText.GetTCHARLength());
    }
    static void Clear() { ee_screens_chat_log_clear(); }
    static unsigned long GetEntryCount() { return ee_screens_chat_log_count(); }
    static long Load(unsigned long inVersion, unsigned long inSize, FSFile& inFile)
    {
        return ee_screens_chat_log_load(&inFile);
    }
    static long Save(unsigned long inVersion, unsigned long inSize, FSFile& inFile)
    {
        return ee_screens_chat_log_save(&inFile);
    }
};

#endif
