// The game's UI as the WON network code (EEWonNet.cpp) uses it: the settings' save, the message boxes and the two
// arena screens' open events. EEUIEWonNetArenaRoomEvents.cpp fills the table at static initialization; the storage is
// EEGlobals.cpp's. Why: EEWonNet named the UI's units directly, and its 1,700 lines and the two arena screens' units
// sat in the C++ dependency cycle through those six names (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts").
#ifndef EE_EEWONNETUI_H
#define EE_EEWONNETUI_H

class UIForm;
class UIEvent;
class UWideString;

struct EEWonNetUI {
    long (*mSavePreferences)();             // gGamePreferences.Save()
    // gUIManager->DisplayMessage / DisplayCancel (ee/EEUIManager.h)
    void (*mDisplayMessage)(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                            void (*inCallback)(UIForm*), bool inFlag);
    void (*mDisplayCancel)(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                           void (*inCallback)(UIForm*), UIForm* inCancel);
    UIEvent* (*mNewRoomsOpenScreen)();      // new EEUIEWNAROpenScreen (the room list's open event)
    UIEvent* (*mNewGamesOpenScreen)();      // new EEUIEWNAGOpenScreen (the game list's open event)
};

extern EEWonNetUI gEEWonNetUI;

#endif
