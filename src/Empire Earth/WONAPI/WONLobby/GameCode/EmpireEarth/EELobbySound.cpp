// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbySound.cpp (file name PROVISIONAL: no __FILE__; the
// classes are WONAPI::EELobbyMusic / EELobbySound / EELobbySoundDecoder). The lobby's sounds, played through the
// engine's sound manager: 0x29ce06..0x29cfda (after EELobbyResource.cpp's GUIString COMDAT; the next unit's init
// chain starts at 0x29cfda). Unit flags: the exe's module set (r12-misc-h2, round 12).
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
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEGameSound.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/EELobbySound.h"
using namespace WONAPI;

// FUNCTION: EXE 0x69ce06
EELobbyMusic::EELobbyMusic(const SoundDescriptor& inDescriptor) : Sound(inDescriptor)
{
}

// TEMPLATE: EXE 0x69ce1e
// (compiler-generated; name in analysis/exe/names.txt)

// FUNCTION: EXE 0x69ce46
void EELobbyMusic::Play()
{
    if (gUIManager != NULL)
        gUIManager->SetMusicVolume(gGamePreferences.mSettings.GetMusicVolume());
}

// FUNCTION: EXE 0x69ce61
void EELobbyMusic::Stop()
{
    if (gUIManager != NULL)
        gUIManager->SetMusicVolume(0.0f);
}

// FUNCTION: EXE 0x69ce78
void EELobbyMusic::Virtual0C()
{
    Play();
}

// FUNCTION: EXE 0x69ce7d
EELobbySound::EELobbySound(const SoundDescriptor& inDescriptor, unsigned long inSampleID)
    : Sound(inDescriptor), mSampleID(inSampleID), mInstanceID(-1)
{
}

// TEMPLATE: EXE 0x69cea0
// (compiler-generated; name in analysis/exe/names.txt)

// FUNCTION: EXE 0x69cebc
EELobbySound::~EELobbySound()
{
    if (gSoundManager != NULL)
        gSoundManager->Unload(mSampleID);
}

// FUNCTION: EXE 0x69ceeb
void EELobbySound::Play()
{
    if (gSoundManager != NULL)
        mInstanceID = gSoundManager->Play(mSampleID, 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: EXE 0x69cf1c
void EELobbySound::Stop()
{
    if (gSoundManager != NULL && !UIsNone32(mInstanceID))
        gSoundManager->Stop(mInstanceID, false);
    mInstanceID = -1;
}

// FUNCTION: EXE 0x69cf40
SoundPtr EELobbySoundDecoder::Decode(const SoundDescriptor& inDescriptor)
{
    if (gSoundManager == NULL)
        return NULL;
    return new EELobbySound(inDescriptor, gSoundManager->Load(inDescriptor.mPath.c_str(), 0, 1));
}
