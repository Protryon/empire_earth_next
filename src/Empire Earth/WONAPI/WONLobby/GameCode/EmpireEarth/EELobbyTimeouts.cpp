// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyTimeouts.cpp (file name inferred: no __FILE__)
// The last game-code unit before the WON libraries: one global settings object at 0x9659d8 (no code references it)
// with its init chain, out-of-line destructor and constructor (0x69cfda..0x69d080). Every name here is inferred
// (no RTTI).
#include <windows.h>
#include <mbstring.h>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"

// 0x5c bytes: three counts/times (3, 2000, 45000), two vectors and a name (provisional names)
struct EELobbyTimeouts {
    EELobbyTimeouts();
    ~EELobbyTimeouts();

    long mL00;                              // +0x00 (3)
    long mL04;                              // +0x04 (2000)
    long mL08;                              // +0x08 (45000)
    std::vector<long> mV0C;                 // +0x0c
    short mS1C;                             // +0x1c (1)
    std::vector<long> mV20;                 // +0x20
    long mL30;                              // +0x30
    char _gap34[0x10];
    bool mB44;                              // +0x44
    UString mName;                          // +0x48
    long mL58;                              // +0x58
};

EELobbyTimeouts gEELobbyTimeouts;

// FUNCTION: EXE 0x69d004
// Destroys the global lobby timeout settings, releasing their name and both vectors
EELobbyTimeouts::~EELobbyTimeouts()
{
}

// FUNCTION: EXE 0x69d022
// Creates the lobby timeout settings with defaults 3, 2000 and 45000 and empty vectors
EELobbyTimeouts::EELobbyTimeouts()
    : mL00(3), mL04(2000), mL08(45000), mS1C(1), mL30(0), mB44(false), mL58(0)
{
}

// SYNTHETIC: EXE 0x69cfda
// SYNTHETIC: EXE 0x69cfe4
// SYNTHETIC: EXE 0x69cfee
// SYNTHETIC: EXE 0x69cffa
