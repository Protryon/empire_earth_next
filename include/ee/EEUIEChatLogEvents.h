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

// (the original's EEChatLogEntry, one chat line of sizeof 0x3c: colour, sender, text, type (2: not logged), time, is
// the Rust EeScreensChatEntry: ee_screens chat_log_ffi.rs, UI lane round 16)

// (EEUIECLEFormManager's statics AddEntry 0x1d32bf (keeps the last 40), Clear, Load and Save are the Rust log's
// ee_screens_chat_log_add, _clear, _load and _save: UI lane round 16)

#endif
