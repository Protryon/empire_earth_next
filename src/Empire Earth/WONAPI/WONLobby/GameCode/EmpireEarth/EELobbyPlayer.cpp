// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyPlayer.cpp (file name inferred: no __FILE__; its
// classes EELobbyPlayerSlot and EELobbyPlayer come from RTTI). A lobby game's player slots and the game's lobby player:
// 0x699838..0x699b17 (the string/map functions at 0x69976f..0x699838 close the unit before; EELobbyPlayerOptionsCtrl.cpp
// follows).
#include <windows.h>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"

using namespace WONAPI;

// FUNCTION: EXE 0x699838
// Creates an open player slot numbered 0 with the default team, colour, citizen and transfer options
EELobbyPlayerSlot::EELobbyPlayerSlot()
{
    this->mIndex = 0;
    this->mOptions[0] = 0;
    this->Reset();
}

// FUNCTION: EXE 0x699880
// Resets the slot's team, colour, citizen, observer and transfer options to their defaults
void EELobbyPlayerSlot::Reset()
{
    this->mOptions[1] = 0;
    this->mOptions[2] = 0;
    this->mOptions[3] = 5;
    this->mOptions[4] = 0;
    this->mOptions[5] = 100;
}

// FUNCTION: EXE 0x699894
// Destroys the slot, detaching the player in it
EELobbyPlayerSlot::~EELobbyPlayerSlot()
{
    this->ClearPlayer();
}

// FUNCTION: EXE 0x6998c8
// Returns the name shown for the slot: its client's name, the slot's own name when named, else empty
const GUIString& EELobbyPlayerSlot::GetName()
{
    if (this->mClient.mPtr == NULL) {
        if (this->mOptions[0] == 4)
            return this->mName;
        return gEmptyGUIString;
    }
    return this->mClient->mName;
}

// FUNCTION: EXE 0x6998e3
// Sets whether the slot is open, computer, closed, a player or named; drops the player or name it no longer holds
void EELobbyPlayerSlot::SetState(int inState)
{
    this->mOptions[0] = inState;
    if (inState != 3)
        this->ClearPlayer();
    if (inState != 4)
        this->mName = GetEmptyGUIStringData();
}

// FUNCTION: EXE 0x699910
// Returns one of the slot's six options, or 0 for an option number out of range
unsigned char EELobbyPlayerSlot::GetOption(int inOption)
{
    if (inOption >= 0 && inOption < 6)
        return this->mOptions[inOption];
    return 0;
}

// FUNCTION: EXE 0x699928
// Sets one of the slot's options; option 0 changes the slot's state and ignores invalid states
void EELobbyPlayerSlot::SetOption(int inOption, unsigned char inValue)
{
    if (inOption == 0) {
        if (inValue < 5)
            this->SetState(inValue);
    } else
        this->mOptions[inOption] = inValue;
}

// FUNCTION: EXE 0x69994f
// Detaches the lobby player and client from the slot
// Both assignments call the ICF-shared SmartPtr<T>::operator=(T*) at 0x699ae3
void EELobbyPlayerSlot::ClearPlayer()
{
    if (this->mPlayer.mPtr != NULL) {
        this->mPlayer->mPlayerSlot = NULL;
        this->mPlayer = NULL;
    }
    this->mClient = NULL;
}

// FUNCTION: EXE 0x699973
// Opens the slot for a player to join
void EELobbyPlayerSlot::SetOpen()
{
    this->SetState(0);
}

// FUNCTION: EXE 0x69997b
// Closes the slot
void EELobbyPlayerSlot::SetClosed()
{
    this->SetState(2);
}

// FUNCTION: EXE 0x699983
// Gives the slot to a computer player
void EELobbyPlayerSlot::SetComputer()
{
    this->SetState(1);
}

// FUNCTION: EXE 0x69998b
// Marks the slot as named and stores the name shown for it
void EELobbyPlayerSlot::SetName(const GUIString& inName)
{
    this->SetState(4);
    this->mName = inName;
}

// FUNCTION: EXE 0x6999a7
// Puts a client's lobby player into the slot and tells the player its slot number; with no player the slot stays empty
// The assignments call the ICF-shared SmartPtr<T>::operator=(T*), as in ClearPlayer
void EELobbyPlayerSlot::SetPlayer(LobbyClient* inClient, EELobbyPlayer* inPlayer)
{
    this->ClearPlayer();
    if (inClient != NULL && inPlayer != NULL) {
        this->mClient = inClient;
        this->mPlayer = inPlayer;
        this->mPlayer->mPlayerSlot = this;
        this->mPlayer->mSlot = this->mIndex;
        this->SetState(3);
    }
}

// FUNCTION: EXE 0x6999f3
// Reads the slot's six options from a game message, detaching the player unless the slot holds one
void EELobbyPlayerSlot::ReadData(ReadBuffer& inBuffer)
{
    for (int i = 0; i < 6; i++)
        this->mOptions[i] = inBuffer.ReadByte();
    if (this->mOptions[0] != 3)
        this->ClearPlayer();
}

// FUNCTION: EXE 0x699a1e
// Writes the slot's six options to a game message
void EELobbyPlayerSlot::WriteData(WriteBuffer& outBuffer)
{
    for (int i = 0; i < 6; i++)
        outBuffer.AppendByte(this->mOptions[i]);
}

// FUNCTION: EXE 0x699a3f
// Creates a lobby game player in slot 0 with no slot attached
EELobbyPlayer::EELobbyPlayer()
{
    this->mSlot = 0;
    this->mPlayerSlot = NULL;
}

// FUNCTION: EXE 0x699a75
// Destroys the lobby game player (members only)
EELobbyPlayer::~EELobbyPlayer()
{
}

// FUNCTION: EXE 0x699a80
// Reads the player's slot number from a game message; always succeeds
bool EELobbyPlayer::ReadData(ReadBuffer& inBuffer)
{
    this->mSlot = inBuffer.ReadByte();
    return true;
}

// FUNCTION: EXE 0x699a95
// Writes the player's slot number to a game message
void EELobbyPlayer::WriteData(WriteBuffer& outBuffer)
{
    outBuffer.AppendByte(this->mSlot);
}

// FUNCTION: EXE 0x699aa7
// Returns whether another player is on the same team; team 0 (no team) has no teammates
bool EELobbyPlayer::IsTeammate(LobbyPlayer* inOther)
{
    unsigned char theTeam = static_cast<EELobbyPlayer*>(inOther)->GetTeam();
    unsigned char myTeam = this->GetTeam();
    return myTeam != 0 && myTeam == theTeam;
}

// FUNCTION: EXE 0x699ad1
// Returns the team of the player's slot, or 0 when the player has no slot
unsigned char EELobbyPlayer::GetTeam()
{
    if (this->mPlayerSlot != NULL)
        return this->mPlayerSlot->mOptions[1];
    return 0;
}

// FUNCTION: EXE 0x699adf
// Returns the player's slot number
unsigned char EELobbyPlayer::GetSlot()
{
    return this->mSlot;
}

// The unit's other compiler-generated functions:
// SYNTHETIC: EXE 0x699864
// (??_G EELobbyPlayerSlot)
// SYNTHETIC: EXE 0x699a59
// (??_G EELobbyPlayer)
// TEMPLATE: EXE 0x699ae3
// (SmartPtr<T>::operator=(T*), ICF-shared by every instantiation)
