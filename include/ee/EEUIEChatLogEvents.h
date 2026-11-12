// The chat log form's events and its form manager (RECONSTRUCTION; header and file names inferred, from the class
// prefix EEUIECLE). EEUIEChatLogEvents.cpp (0x1d321b..0x1d3a77) defines EEUIECLEFormManager (the form 0x1d's
// manager: the last 40 chat lines, saved with the game); its events are Rust's (ee_screens chat.rs). The UI thread adds the lines
// (EEUserInterface 0x2787fe, 0x27892d); EESaveLoad calls Load/Save (0x14c383, 0x14c50b). Names are provisional.
// Requires <deque>, lle/GEColor.h, lle/UIFormManager.h, lle/UIEvent.h.
#ifndef EE_EEUIECHATLOGEVENTS_H
#define EE_EEUIECHATLOGEVENTS_H

class FSFile;

// One chat line (sizeof 0x3c; implicit members, emitted here: ctor 0x1d340d, operator= 0x1d3520, copy ctor 0x1d3a10)
struct EEChatLogEntry {
    GEColor mColor;                         // +0x00
    UWideString mName;                      // +0x0c the sender (empty: a game message)
    UWideString mText;                      // +0x20
    unsigned long mType;                    // +0x34 (2: not logged, EEUserInterface 0x2787f7)
    unsigned long mTime;                    // +0x38 (provisional name)
};

class EEUIECLEFormManager : public UIFormManager {   // vtable 0x446290; sizeof 0x18
public:
    EEUIECLEFormManager();                  // 0x1d3269
    virtual ~EEUIECLEFormManager();         // 0x1d32a5
    static void AddEntry(const EEChatLogEntry& inEntry);   // 0x1d32bf (keeps the last 40)
    static EEUIECLEFormManager* Get();      // 0x1d32fe (form 0x1d's manager, created on first use)
    static void Clear();                    // 0x1d3345
    static unsigned long GetEntryCount();   // 0x1d3352
    static long Load(unsigned long inVersion, unsigned long inSize, FSFile& inFile);   // 0x1d335b
    static long Save(unsigned long inVersion, unsigned long inSize, FSFile& inFile);   // 0x1d3434

    std::deque<EEChatLogEntry> mEntries;    // +0x04
};

#endif
