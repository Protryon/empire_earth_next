// C:\EEX\Low-Level Engine\Input System\Input System.h (RECONSTRUCTION; Input System.cpp 0x38abf..0x39715). Input
// System entry points and the two system devices. Not reachable from LowLevelEngine.h (the plug-ins don't use it).
#ifndef LLE_INPUTSYSTEM_H
#define LLE_INPUTSYSTEM_H

#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"

class GEWindow;
class UResourceFile;

LLE_API long ISInitialize(HINSTANCE inInstance, GEWindow* inWindow, UResourceFile* inResources,
                          unsigned long inFirstStringID, unsigned long inDoublePressTimeDelta);

extern LLE_API ISKeyboard* gISKeyboard;
extern LLE_API ISMouse* gISMouse;

#endif
