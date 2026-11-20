// The game's UI as the rest of the game calls it: the message boxes, the command buttons, the status box's and the
// mini map's media, the mini map's update and the UI manager's construction. The UI's units fill the table at static
// initialization, each its own entries; the storage is EEGlobals.cpp's. Why: the game's messages, server, world,
// mini map and main unit named the UI's units directly, and the whole User Interface Thread folder sat in the C++
// dependency cycle with the simulation through those names (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts, round
// 3"). The UI's own calls stay direct.
#ifndef EE_EEGAMEUI_H
#define EE_EEGAMEUI_H

class UIManager;
class UIForm;
class UWideString;
class UString;
class GERasterizer;
class EEUIManager;
class EEUIFMGMainGroup;
class EEUnit;
class EEMiniMap;

struct EEGameUI {
    // EEUIFErrorDisplay (EEUIFError.cpp; ee/EEUIManager.h's Display* members): a new message box
    void (*mErrorDisplay)(UIManager* inManager, unsigned long inKind, const UWideString& inMessage,
                          unsigned long inAlignment, void* inData, void (*inFirst)(UIForm*),
                          void (*inSecond)(UIForm*), UIForm* inCloseForm, bool inFlag);
    // EEUIFMGMainGroup::ShowCommands (EEUIFMGMainGroup.cpp): the command buttons of a unit
    void (*mShowCommands)(EEUIFMGMainGroup* inGroup, EEUnit* inUnit);
    // EEUIFMGStatusBox's LoadIcons, UnloadIcons, ReloadIcons (EEUIFMGStatusBox.cpp)
    void (*mLoadIcons)();
    void (*mUnloadIcons)();
    long (*mReloadIcons)();
    // EEUIMiniMap's LoadMedia, UnloadMedia and Update (EEUIMiniMap.cpp)
    void (*mMiniMapLoadMedia)();
    void (*mMiniMapUnloadMedia)();
    void (*mMiniMapUpdate)(EEMiniMap* inMiniMap);
    // new EEUIManager (EEUIManager.cpp)
    EEUIManager* (*mNewManager)(GERasterizer* inRasterizer, unsigned long inNumberOfSurfaces, unsigned long inScreen,
                                const UString& inUnused);
};

extern EEGameUI gEEGameUI;

#endif
