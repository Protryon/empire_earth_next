// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbySound.cpp (file name inferred: no __FILE__; the
// classes are WONAPI::EELobbyMusic / EELobbySound / EELobbySoundDecoder). The lobby's sounds, played through the
// engine's sound manager: 0x69ce06..0x69cfda (after EELobbyResource.cpp's GUIString function; the next unit's static
// initializers start at 0x69cfda).
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
// Creates the lobby music entry for a sound description
EELobbyMusic::EELobbyMusic(const SoundDescriptor& inDescriptor) : Sound(inDescriptor)
{
}

// TEMPLATE: EXE 0x69ce1e
// (compiler-generated; name in analysis/exe/names.txt)

// FUNCTION: EXE 0x69ce46
// Starts the lobby music by raising the game's music volume to the player's preference
void EELobbyMusic::PlayHook()
{
    if (gUIManager != NULL)
        gUIManager->SetMusicVolume(gGamePreferences.mSettings.GetMusicVolume());
}

// FUNCTION: EXE 0x69ce61
// Silences the lobby music by setting the game's music volume to 0
void EELobbyMusic::StopHook()
{
    if (gUIManager != NULL)
        gUIManager->SetMusicVolume(0.0f);
}

// FUNCTION: EXE 0x69ce78
// Resumes the lobby music at the player's music volume
void EELobbyMusic::ResumeHook()
{
    this->PlayHook();
}

// FUNCTION: EXE 0x69ce7d
// Creates a lobby sound effect for a loaded engine sample, not playing
EELobbySound::EELobbySound(const SoundDescriptor& inDescriptor, unsigned long inSampleID)
    : Sound(inDescriptor), mSampleID(inSampleID), mInstanceID(-1)
{
}

// TEMPLATE: EXE 0x69cea0
// (compiler-generated; name in analysis/exe/names.txt)

// FUNCTION: EXE 0x69cebc
// Destroys the sound effect, unloading its sample from the sound manager
EELobbySound::~EELobbySound()
{
    if (gSoundManager != NULL)
        gSoundManager->Unload(this->mSampleID);
}

// FUNCTION: EXE 0x69ceeb
// Plays the sound effect once at full volume, centred, remembering the playing instance
void EELobbySound::PlayHook()
{
    if (gSoundManager != NULL)
        this->mInstanceID = gSoundManager->Play(this->mSampleID, 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: EXE 0x69cf1c
// Stops the playing instance of the sound effect, if any
void EELobbySound::StopHook()
{
    if (gSoundManager != NULL && !UIsNone32(this->mInstanceID))
        gSoundManager->Stop(this->mInstanceID, false);
    this->mInstanceID = -1;
}

// FUNCTION: EXE 0x69cf40
// Loads a lobby sound file into the engine's sound manager; returns NULL without a sound manager
SoundPtr EELobbySoundDecoder::Decode(const SoundDescriptor& inDescriptor)
{
    if (gSoundManager == NULL)
        return NULL;
    return new EELobbySound(inDescriptor, gSoundManager->Load(inDescriptor.mPath.c_str(), 0, 1));
}
