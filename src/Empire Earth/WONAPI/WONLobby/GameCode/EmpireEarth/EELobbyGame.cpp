// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyGame.cpp (file name inferred: no __FILE__; the
// class EELobbyGame's ctor opens the range, and it sorts between EELobbyFileTransfer and EELobbyGameOptionsCtrl). The
// lobby game: the sixteen game options and their wire types, the eight player slots, the saved game's file transfer,
// the players' sync events: 0x69222b..0x6942c8 (its template COMDATs close it).
#include <windows.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include <map>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/WONLobbyModel.h"

using namespace WONAPI;

extern GUIString gEELobbyGame_StartGameError;          // 0x9658b0 (EELobbyResource.cpp)
extern GUIString gEELobbyGame_EveryoneOnSameTeam;      // 0x9659ac (EELobbyResource.cpp)
extern GUIString gEELobbyGameStatus_NotInSavedGame;    // 0x9658cc (EELobbyResource.cpp)
extern GUIString gEELobbyGameStatus_InvalidVersion;    // 0x96591c (EELobbyResource.cpp)
extern GUIString gEELobbyGameStatus_CorruptBinaries;   // 0x9659b4 (EELobbyResource.cpp)

// each option's wire type (0: bool, 1: byte, 2: short) and limit (SetOption keeps a value below it), set once by
// EELobbyInitOptionTypes
static int sOptionTypes[16];                // 0x965774
static unsigned long sOptionLimits[16];     // 0x965734
static bool sOptionTypesInitialized;        // 0x9657b4

static void EELobbyInitOptionTypes();

// FUNCTION: EXE 0x69222b
// Creates a lobby game with every game option 0, its eight slots numbered and no saved game file here
EELobbyGame::EELobbyGame()
{
    EELobbyInitOptionTypes();
    int i;
    for (i = 0; i < 16; i++)
        this->mOptions[i] = 0;
    for (i = 0; i < 8; i++)
        this->mSlots[i].mIndex = i;
    this->mHasFile = false;
}

// FUNCTION: EXE 0x6922b2
// Destroys the lobby game, releasing its slots, player names and file transfer
EELobbyGame::~EELobbyGame()
{
}

// FUNCTION: EXE 0x6922f2
// Returns the number of player names read from the game's details
int EELobbyGame::GetNumFiles()
{
    return this->mFiles.size();
}

// FUNCTION: EXE 0x692305
// Returns the player name with the given index from the game's details, or an empty string
const GUIString& EELobbyGame::GetFile(int inIndex)
{
    if (inIndex >= 0 && inIndex < this->mFiles.size())
        return this->mFiles[inIndex];
    return gEmptyGUIString;
}

// FUNCTION: EXE 0x69232f
// Sets the saved game file to play (name, size, checksum) and its local path in the saved games directory; the host
// serves it to the joining players when asked
// Not identical: the original calls GUIString::append(const GUIString&) out of line (COMDAT 0x6923f3); ours
// expands it inline (an operator+= wrapper does not change that)
void EELobbyGame::SetGameFile(const GUIString& inName, unsigned long inSize, unsigned long inChecksum, bool inFlag)
{
    if (this->mFileTransfer.mPtr == NULL)
        this->mFileTransfer = new EELobbyFileTransfer;
    this->mFileTransfer->SetFileInfo(inName, inSize, inChecksum);
    GUIString thePath = EELobbyFn2988E9();
    thePath.append(this->GetGameFileName());
    this->mFileTransfer->SetPath(thePath);
    if (this->mIsHost && inFlag)
        this->mFileTransfer->Serve(EELobbyGetFileTransferPort());
}

// FUNCTION: EXE 0x692419
// Appends a range of another GUI string's characters, growing the buffer as needed
inline void GUIStringData::append(const GUIStringData* inText, unsigned short inStart, unsigned short inLength)
{
    int theLength = this->mLength;
    int theNewLength = theLength + inLength;
    this->Reserve(theNewLength, true);
    this->mLength = theNewLength;
    this->mText[this->mLength] = 0;
    memcpy(this->mText + theLength, inText->mText + inStart, inLength * sizeof(wchar_t));
}

// FUNCTION: EXE 0x69246d
// Returns the saved game file's name, or an empty string without a file transfer
const GUIString& EELobbyGame::GetGameFileName()
{
    if (this->mFileTransfer.mPtr != NULL)
        return this->mFileTransfer->mName;
    return gEmptyGUIString;
}

// FUNCTION: EXE 0x69247e
// Writes the saved game file's name, size, checksum and the file transfer port to a message
void EELobbyGame::WriteGameFile(WriteBuffer& outBuffer)
{
    outBuffer.AppendWString(this->GetGameFileName(), 2);
    outBuffer.AppendLong(this->mFileTransfer->mSize);
    outBuffer.AppendLong(this->mFileTransfer->mChecksum);
    outBuffer.AppendShort(EELobbyGetFileTransferPort());
}

// FUNCTION: EXE 0x6924e0
// Reads the saved game file's details from a message and points its download at the host's port
void EELobbyGame::ReadGameFile(ReadBuffer& inBuffer)
{
    std::wstring theName;
    inBuffer.ReadWString(theName, 2);
    unsigned long theSize = inBuffer.ReadLong();
    unsigned long theChecksum = inBuffer.ReadLong();
    unsigned short thePort = inBuffer.ReadShort();
    this->SetGameFile(theName, theSize, theChecksum, false);
    IPAddr theAddress(this->mAddress);
    theAddress.SetPort(thePort);
    this->mFileTransfer->SetHost(theAddress);
}

// FUNCTION: EXE 0x69258f
// Creates a GUI string holding a copy of the wide text
GUIString::GUIString(const std::wstring& inText) : mData(new GUIStringData(inText))
{
}

// FUNCTION: EXE 0x6925cb
// Stops the saved game file transfer, if any
void EELobbyGame::StopTransfer()
{
    if (this->mFileTransfer.mPtr != NULL)
        this->mFileTransfer->Stop();
}

// FUNCTION: EXE 0x6925d8
// Reports the saved game download's progress as the local player's transfer percent; when complete, marks the file
// here and updates the staging screen
void EELobbyGame::UpdateTransferProgress(int inDone, int inTotal)
{
    if (inDone == inTotal) {
        this->mHasFile = true;
        this->SetTransferPercent(100);
        LobbyNotifyStagingChange();
    } else
        this->SetTransferPercent((unsigned char)(int)((double)inDone / inTotal * 100.0));
}

// FUNCTION: EXE 0x692616
// Sets the local player's file transfer percent (player option 5)
void EELobbyGame::SetTransferPercent(unsigned char inPercent)
{
    this->ChangePlayerOption(this->GetMySlot(), 5, inPercent);
}

// FUNCTION: EXE 0x692630
// On a client, looks for the saved game file in the saved games directory and downloads it from the host when missing
// Not identical: as in SetGameFile (0x69232f), GUIString::append(const GUIString&) is out of line in the original
void EELobbyGame::StartTransfer()
{
    if (this->mIsHost)
        return;
    this->mFileTransfer->CloseClients();
    GUIString thePath = EELobbyFn2988E9();
    thePath.append(this->GetGameFileName());
    this->mFileTransfer->SetPath(thePath);
    this->mHasFile = this->mFileTransfer->IsFileHere();
    LobbyNotifyStagingChange();
    if (!this->mHasFile) {
        this->SetTransferPercent(0);
        this->mFileTransfer->Download();
    } else
        this->SetTransferPercent(100);
}

// FUNCTION: EXE 0x6926df
// Returns whether the local player may get ready: a saved game's file must be here first
bool EELobbyGame::CanReadyHook()
{
    if (this->HasGameFile() && !this->mHasFile)
        return false;
    return true;
}

// FUNCTION: EXE 0x6926f9
// Returns 1 when the game can start: at least one human, two players in all, and no saved-game slot still waiting
long EELobbyGame::CanStartGameHook()
{
    if (this->mNumHumanPlayers > 0 && this->mNumHumanPlayers + this->mNumComputerSlots > 1 && this->mNumReservedSlots == 0)
        return 1;
    return 0;
}

// FUNCTION: EXE 0x692719
// Starts the hosted game, unless every player is on the same team (team 0 = no team); stops the file transfer first
bool EELobbyGame::TryStartGameHook()
{
    bool theDifferent = false;
    unsigned char theTeam = 0xff;
    for (int i = 0; i < 8 && !theDifferent; i++) {
        EELobbyPlayerSlot* theSlot = this->GetPlayerSlot(i);
        if (theSlot != NULL) {
            unsigned char theState = theSlot->mOptions[0];
            if ((theState == 3 && theSlot->mOptions[4] == 0) || theState == 1) {
                if (theTeam == 0xff)
                    theTeam = theSlot->mOptions[1];
                else
                    theDifferent = theTeam != theSlot->mOptions[1];
            }
        }
    }
    if (!theDifferent && theTeam != 0) {
        LobbyMessageBox(gEELobbyGame_StartGameError, gEELobbyGame_EveryoneOnSameTeam);
        return false;
    }
    this->StopTransfer();
    return EELobbyHostGame();
}

// FUNCTION: EXE 0x692797
// Stops the saved game file transfer after leaving the game
void EELobbyGame::PostNetLeaveGameHook()
{
    this->StopTransfer();
}

void EELobbyGame::NotifyStartGameHook()
{
    this->StopTransfer();
}

// FUNCTION: EXE 0x69279c
// Opens all eight player slots with no team and 5 starting citizens, counting one human player
void EELobbyGame::ResetSlots()
{
    this->mNumComputerSlots = 0;
    this->mNumReservedSlots = 0;
    this->mNumOpenSlots = 8;
    this->mNumHumanPlayers = 1;
    for (int i = 0; i < 8; i++) {
        this->mSlots[i].SetOpen();
        this->mSlots[i].mOptions[1] = 0;
        this->mSlots[i].mOptions[3] = 5;
    }
}

// FUNCTION: EXE 0x6927d0
// Counts human, computer and waiting saved-game slots (captain only); re-advertises the game when the open slot
// count changed and asked to
void EELobbyGame::CountPlayers(bool inNotify)
{
    if (!LobbyIAmCaptain())
        return;
    int theOpen = 8;
    this->mNumHumanPlayers = 0;
    this->mNumComputerSlots = 0;
    this->mNumReservedSlots = 0;
    for (int i = 0; i < 8; i++) {
        int theState = this->mSlots[i].mOptions[0];
        if (theState == 1 || theState == 2)
            theOpen--;
        if (theState == 3 && this->mSlots[i].mOptions[4] == 0)
            this->mNumHumanPlayers++;
        if (theState == 1)
            this->mNumComputerSlots++;
        if (theState == 4 && this->mSlots[i].mOptions[4] == 0)
            this->mNumReservedSlots++;
    }
    if (theOpen != this->mNumOpenSlots) {
        this->mNumOpenSlots = theOpen;
        if (inNotify)
            LobbyNetAdvertiseGame();
    }
    LobbyNotifyStagingChange();
}

// FUNCTION: EXE 0x692847
// Returns the player slot with the given number, or NULL outside 0..7
EELobbyPlayerSlot* EELobbyGame::GetPlayerSlot(int inSlot)
{
    if (inSlot >= 0 && inSlot < 8)
        return &this->mSlots[inSlot];
    return NULL;
}

// FUNCTION: EXE 0x692862
// Returns whether the game type (option 0) is one played from a file, not a random map
bool EELobbyGame::HasGameFile()
{
    return this->GetOption(0) != 0;
}

// FUNCTION: EXE 0x692870
// Returns a game option's value, or 0 outside 0..15
long EELobbyGame::GetOption(int inOption)
{
    if (inOption >= 0 && inOption < 16)
        return this->mOptions[inOption];
    return 0;
}

// FUNCTION: EXE 0x69288b
// Writes a game option to a message as a bool, byte or short, by its wire type
void EELobbyGame::WriteOption(WriteBuffer& outBuffer, int inOption)
{
    switch (sOptionTypes[inOption]) {
    case 2:
        outBuffer.AppendShort((short)this->mOptions[inOption]);
        break;
    case 1:
        outBuffer.AppendByte((unsigned char)this->mOptions[inOption]);
        break;
    case 0:
        outBuffer.AppendBool(this->mOptions[inOption] != 0);
        break;
    }
}

// FUNCTION: EXE 0x6928e0
// Reads a game option from a message by its wire type and sets it (values over the limit are ignored)
void EELobbyGame::ReadOption(ReadBuffer& inBuffer, int inOption)
{
    long theValue;
    switch (sOptionTypes[inOption]) {
    case 2:
        theValue = inBuffer.ReadShort();
        break;
    case 1:
        theValue = (char)inBuffer.ReadByte();
        break;
    case 0:
        theValue = inBuffer.ReadBool() ? 1 : 0;
        break;
    default:
        return;
    }
    this->SetOption(inOption, theValue);
}

// FUNCTION: EXE 0x692933
// Sets each game option's wire type and value limit once (the map type limit is the number of map types)
static void EELobbyInitOptionTypes()
{
    if (sOptionTypesInitialized)
        return;
    sOptionTypesInitialized = true;
    sOptionTypes[0] = 1;
    sOptionTypes[1] = 1;
    sOptionTypes[2] = 1;
    sOptionTypes[3] = 1;
    sOptionTypes[4] = 1;
    sOptionTypes[5] = 1;
    sOptionTypes[6] = 1;
    sOptionTypes[7] = 1;
    sOptionTypes[8] = 1;
    sOptionTypes[9] = 2;
    sOptionTypes[10] = 1;
    sOptionTypes[11] = 0;
    sOptionTypes[12] = 0;
    sOptionTypes[13] = 0;
    sOptionTypes[14] = 0;
    sOptionTypes[15] = 0;
    sOptionLimits[0] = 3;
    sOptionLimits[1] = 6;
    sOptionLimits[2] = EELobbyGetNumMapTypes();
    sOptionLimits[3] = 5;
    sOptionLimits[4] = 16;
    sOptionLimits[5] = 16;
    sOptionLimits[6] = 2;
    sOptionLimits[7] = 3;
    sOptionLimits[8] = 4;
    sOptionLimits[9] = 1201;
    sOptionLimits[10] = 7;
    sOptionLimits[11] = 2;
    sOptionLimits[12] = 2;
    sOptionLimits[13] = 2;
    sOptionLimits[14] = 2;
    sOptionLimits[15] = 2;
}

// FUNCTION: EXE 0x692a23
// Sets up a newly hosted game: a file transfer, default options overridden by the host's game preferences, open
// slots and the host in slot 0
void EELobbyGame::HandleHostGameHook(LobbyClient* inClient)
{
    this->mFileTransfer = new EELobbyFileTransfer;
    this->mOptions[0] = 0;
    this->mOptions[1] = 1;
    this->mOptions[2] = 0;
    this->mOptions[3] = 0;
    this->mOptions[4] = 0;
    this->mOptions[5] = 0xe;
    this->mOptions[6] = 1;
    this->mOptions[7] = 1;
    this->mOptions[8] = 1;
    this->mOptions[9] = 0x1f4;
    this->mOptions[10] = 3;
    this->mOptions[11] = 0;
    this->mOptions[12] = 1;
    this->mOptions[13] = 0;
    this->mOptions[14] = 0;
    this->mOptions[15] = 1;
    EELobbySetDefaultGameOptions(this);
    this->ResetSlots();
    this->mSlots[0].SetPlayer(inClient, (EELobbyPlayer*)inClient->mPlayer.mPtr);
}

// FUNCTION: EXE 0x692ade
// Marks the colours taken by the other playing slots (a seated or reserved human, not observing)
// (a slot reference: the strength-reduced pointer is based on mOptions[0])
void EELobbyGame::GetTakenColors(bool* outTaken, int inExcept)
{
    int i;
    for (i = 0; i < 16; i++)
        outTaken[i] = false;
    for (i = 0; i < 8; i++) {
        if (i != inExcept) {
            EELobbyPlayerSlot& theSlot = this->mSlots[i];
            if ((theSlot.mOptions[0] == 3 || theSlot.mOptions[0] == 4) && theSlot.mOptions[4] == 0 && theSlot.mOptions[2] < 16)
                outTaken[theSlot.mOptions[2]] = true;
        }
    }
}

// FUNCTION: EXE 0x692b29
// Returns whether no playing slot has the given colour
bool EELobbyGame::IsColorFree(unsigned char inColor)
{
    bool theTaken[16];
    this->GetTakenColors(theTaken, -1);
    if (inColor < 16)
        return !theTaken[inColor];
    return false;
}

// FUNCTION: EXE 0x692b57
// Writes the game's summary for the game list: starting epoch, game type and map type
void EELobbyGame::WriteSummaryHook(WriteBuffer& outBuffer)
{
    outBuffer.AppendByte(this->GetOption(4));
    outBuffer.AppendByte(this->GetOption(0));
    outBuffer.AppendByte(this->GetOption(2));
}

// FUNCTION: EXE 0x692b95
// Reads a listed game's summary: starting epoch, game type and map type
bool EELobbyGame::ReadSummaryHook(ReadBuffer& inBuffer)
{
    this->SetOption(4, (char)inBuffer.ReadByte());
    this->SetOption(0, (char)inBuffer.ReadByte());
    this->SetOption(2, (char)inBuffer.ReadByte());
    return true;
}

// FUNCTION: EXE 0x692be0
// Writes the game's details for the game list: every option, then its players' client ids (game type 2) or names
void EELobbyGame::WriteDetailsHook(WriteBuffer& outBuffer)
{
    int i;
    for (i = 0; i < 16; i++)
        this->WriteOption(outBuffer, i);
    unsigned char theCount = 0;
    unsigned long thePos = outBuffer.mLength;
    outBuffer.AppendByte(0);
    for (i = 0; i < 8; i++) {
        EELobbyPlayerSlot* theSlot = this->GetPlayerSlot(i);
        if (theSlot != NULL && theSlot->mClient.mPtr != NULL) {
            theCount++;
            if (this->mGameType == 2)
                outBuffer.AppendShort(theSlot->mClient->mClientId);
            else
                outBuffer.AppendWString(theSlot->mClient->mName, 2);
        }
    }
    outBuffer.SetByte(thePos, theCount);
}

// FUNCTION: EXE 0x692c7e
// Reads a listed game's details: every option, then its player names (looked up by client id for game type 2)
bool EELobbyGame::ReadDetailsHook(ReadBuffer& inBuffer)
{
    this->mFiles.clear();
    int i;
    for (i = 0; i < 16; i++)
        this->ReadOption(inBuffer, i);
    if (inBuffer.mLength - inBuffer.mPos > 0) {
        unsigned char theCount = inBuffer.ReadByte();
        LobbyServer* theClients = LobbyGetCurrentServer();
        if (theClients == NULL && this->mGameType == 2)
            return true;
        this->mFiles.reserve(theCount);
        for (i = theCount; i > 0; i--) {
            if (this->mGameType == 2) {
                unsigned short theId = inBuffer.ReadShort();
                LobbyClient* theClient = theClients->GetClient(theId);
                if (theClient != NULL)
                    this->mFiles.push_back(theClient->mName);
            } else {
                std::wstring theName;
                inBuffer.ReadWString(theName, 2);
                GUIString theString(theName);
                this->mFiles.push_back(theString);
            }
        }
    }
    return true;
}

// FUNCTION: EXE 0x692d71
// Returns the wanted colour if no other slot has it, else the first free colour (0 when none)
unsigned char EELobbyGame::GetFreeColor(int inExcept, unsigned char inColor)
{
    bool theTaken[16];
    this->GetTakenColors(theTaken, inExcept);
    if (inColor < 16 && !theTaken[inColor])
        return inColor;
    for (int i = 0; i < 16; i++) {
        if (!theTaken[i])
            return i;
    }
    return 0;
}

// FUNCTION: EXE 0x692daa
// Gives each playing slot whose colour another slot already has the first free one
void EELobbyGame::MakeColorsDistinct()
{
    for (int i = 0; i < 8; i++) {
        EELobbyPlayerSlot* theSlot = this->GetPlayerSlot(i);
        if (theSlot != NULL && (theSlot->mOptions[0] == 3 || theSlot->mOptions[0] == 4) && theSlot->mOptions[4] == 0)
            theSlot->mOptions[2] = this->GetFreeColor(i, theSlot->mOptions[2]);
    }
}

// FUNCTION: EXE 0x692df3
// Returns the text for a refused join: not in the saved game, wrong version or modified game files
GUIString EELobbyGame::GetStatusStringHook(short inStatus)
{
    switch (inStatus) {
    case 10000:
        return gEELobbyGameStatus_NotInSavedGame;
    case 10001:
        return gEELobbyGameStatus_InvalidVersion;
    case 10002:
        return gEELobbyGameStatus_CorruptBinaries;
    }
    return gEmptyGUIString;
}

// FUNCTION: EXE 0x692e42
// Writes the game's version and binary checksum into a join request
void EELobbyGame::GetJoinGameRequestHook(WriteBuffer& outBuffer)
{
    GUIString theVersion;
    unsigned long theChecksum;
    EELobbyGetVersion(theVersion, theChecksum);
    outBuffer.AppendWString(theVersion, 2);
    outBuffer.AppendLong(theChecksum);
}

// FUNCTION: EXE 0x692e9f
// Seats a joining player in a free slot (saved game: the slot reserved under its name) if version and checksum match;
// returns 0 or the refusal code
// Refusals: 10001 wrong version, 10002 wrong checksum, 10000 not in the saved game, 2 no open slot
short EELobbyGame::HandleJoinGameRequestHook(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    GUIString theVersion(GetEmptyGUIStringData());
    unsigned long theChecksum;
    EELobbyGetVersion(theVersion, theChecksum);
    std::wstring theText;
    inBuffer.ReadWString(theText, 2);
    if (theVersion != GUIString(theText))
        return 10001;
    if (inBuffer.ReadLong() != theChecksum)
        return 10002;
    bool theSaved = this->HasGameFile();
    int i;
    for (i = 0; i < 8; i++) {
        if (!theSaved) {
            if (this->mSlots[i].mOptions[0] == 0)
                break;
        } else if (this->mSlots[i].mOptions[0] == 4 && inClient->mName == this->mSlots[i].GetName()) {
            break;
        }
    }
    if (i >= 8)
        return theSaved ? 10000 : 2;
    this->mSlots[i].SetPlayer(inClient, (EELobbyPlayer*)inClient->mPlayer.mPtr);
    if (!theSaved)
        this->mSlots[i].Reset();
    return 0;
}

// FUNCTION: EXE 0x693029
// Writes the game's state for a joining player: every option, every slot and the saved game file
void EELobbyGame::GetJoinGameReplyHook(LobbyClient* inClient, WriteBuffer& outBuffer)
{
    int i;
    for (i = 0; i < 16; i++)
        this->WriteOption(outBuffer, i);
    for (i = 0; i < 8; i++)
        this->mSlots[i].WriteData(outBuffer);
    if (this->HasGameFile())
        this->WriteGameFile(outBuffer);
}

// FUNCTION: EXE 0x693075
// Puts a client in the slot its lobby player names; returns the slot, or -1
int EELobbyGame::SeatClient(LobbyClient* inClient)
{
    if (inClient != NULL) {
        EELobbyPlayer* thePlayer = (EELobbyPlayer*)inClient->mPlayer.mPtr;
        if (thePlayer != NULL) {
            unsigned char theSlot = thePlayer->GetSlot();
            if (theSlot < 8) {
                this->mSlots[theSlot].SetPlayer(inClient, thePlayer);
                return theSlot;
            }
        }
    }
    return -1;
}

// FUNCTION: EXE 0x6930b5
// Takes over a joined game's state: options, slots, the saved game file, the clients in their slots; picks a free
// colour for the local player and starts the file download
void EELobbyGame::HandleJoinGameReplyHook(ReadBuffer& inBuffer)
{
    this->mFileTransfer = new EELobbyFileTransfer;
    int i;
    for (i = 0; i < 16; i++)
        this->ReadOption(inBuffer, i);
    for (i = 0; i < 8; i++)
        this->mSlots[i].ReadData(inBuffer);
    if (this->HasGameFile())
        this->ReadGameFile(inBuffer);
    LobbyClientSetIterator theIterator(this->mClients.mPtr);
    while (theIterator.HasMore()) {
        LobbyClient* theClient = NULL;
        theIterator.Next(&theClient);
        this->SeatClient(theClient);
    }
    EELobbyPlayerSlot* theSlot = this->GetPlayerSlot(this->GetMySlot());
    if (theSlot != NULL)
        theSlot->mOptions[2] = this->GetFreeColor(theSlot->GetIndex(), 0);
    if (this->HasGameFile())
        this->StartTransfer();
}

// FUNCTION: EXE 0x693195
// Seats a player who joined (a random map game resets the slot and picks a free colour), recounts and syncs the slot
void EELobbyGame::HandlePlayerJoinedHook(LobbyClient* inClient)
{
    int theSlot = this->SeatClient(inClient);
    if (theSlot < 0)
        return;
    EELobbyPlayerSlot* thePlayerSlot = this->GetPlayerSlot(theSlot);
    if (thePlayerSlot != NULL && !this->HasGameFile()) {
        thePlayerSlot->Reset();
        thePlayerSlot->mOptions[2] = this->GetFreeColor(thePlayerSlot->GetIndex(), 0);
    }
    this->CountPlayers(false);
    LobbyBroadcastEvent(new EELobbySyncPlayerEvent(theSlot), true);
}

// FUNCTION: EXE 0x69321e
// Frees a leaving player's slot (a saved game keeps it reserved under the name), recounts, syncs it and, as captain,
// reports it as disconnected
void EELobbyGame::HandlePlayerLeftHook(LobbyClient* inClient, LobbyPlayer* inPlayer)
{
    unsigned char theSlot = static_cast<EELobbyPlayer*>(inPlayer)->GetSlot();
    if (theSlot >= 8 || this->mSlots[theSlot].mClient.mPtr != inClient)
        return;
    if (this->HasGameFile()) {
        this->mSlots[theSlot].SetName(inClient->mName);
    } else {
        this->mSlots[theSlot].SetOpen();
        this->mSlots[theSlot].Reset();
    }
    this->CountPlayers(false);
    LobbyBroadcastEvent(new EELobbySyncPlayerEvent(theSlot), true);
    if (LobbyIAmCaptain())
        this->BroadcastConnectStatus(theSlot, false);
}

// FUNCTION: EXE 0x6932d9
// Dispatches an Empire Earth game room message (options, player options, game state, connect status)
void EELobbyGame::HandleGameMessageHook(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom)
{
    switch (inType) {
    case 0x64:
        this->HandleOptionChange(inBuffer, inFrom);
        break;
    case 0x65:
        this->HandlePlayerOptionChange(inBuffer, inFrom);
        break;
    case 0x66:
        this->HandlePlayerOptionRequest(inBuffer, inFrom);
        break;
    case 0x67:
        this->HandleGameState(inBuffer, inFrom);
        break;
    case 0x68:
        this->HandleConnectStatus(inBuffer, inFrom, true);
        break;
    case 0x69:
        this->HandleConnectStatus(inBuffer, inFrom, false);
        break;
    case 0x6a:
        this->HandleCancelConnect(inBuffer, inFrom);
        break;
    }
}

// FUNCTION: EXE 0x69334d
// Closes the connect dialog when the captain cancels the launch (message 0x6a)
void EELobbyGame::HandleCancelConnect(ReadBuffer& inBuffer, LobbyClient* inFrom)
{
    if (inFrom->IsCaptain(1))
        EELobbyCancelConnectDialog();
}

// FUNCTION: EXE 0x693364
// Handles a slot's connect status: from the slot's own client the host passes it on; from the captain it updates the
// connect dialog
// messages 0x68 (from the slot's client) / 0x69 (from the captain)
void EELobbyGame::HandleConnectStatus(ReadBuffer& inBuffer, LobbyClient* inFrom, bool inFromClient)
{
    unsigned char theSlot = inBuffer.ReadByte();
    bool theFlag = inBuffer.ReadBool();
    if (inFromClient) {
        EELobbyPlayerSlot* thePlayerSlot = this->GetPlayerSlot(theSlot);
        if (thePlayerSlot != NULL && thePlayerSlot->mClient.mPtr == inFrom)
            this->BroadcastConnectStatus(theSlot, theFlag);
    } else if (inFrom->IsCaptain(1)) {
        EELobbySetSlotConnected(theSlot, theFlag);
    }
}

// FUNCTION: EXE 0x6933ce
// Tells the game room a slot's connect status while the connect dialog runs (message 0x69)
void EELobbyGame::BroadcastConnectStatus(unsigned char inSlot, bool inConnected)
{
    if (!EELobbyIsConnectDialogRunning())
        return;
    WriteBuffer theBuffer(0, true);
    theBuffer.AppendByte(0x69);
    theBuffer.AppendByte(inSlot);
    theBuffer.AppendBool(inConnected);
    LobbyBroadcastToGame(theBuffer.ToByteBuffer(true).get());
}

// FUNCTION: EXE 0x693437
// Tells the captain whether the local player connected (message 0x68)
void EELobbyGame::SendConnectStatus(bool inConnected)
{
    WriteBuffer theBuffer(0, true);
    theBuffer.AppendByte(0x68);
    theBuffer.AppendByte(this->GetMySlot());
    theBuffer.AppendBool(inConnected);
    LobbySendToCaptain(theBuffer.ToByteBuffer(true).get());
}

// FUNCTION: EXE 0x6934a0
// Tells the game room the launch was cancelled (message 0x6a)
void EELobbyGame::SendCancelConnect()
{
    WriteBuffer theBuffer(0, true);
    theBuffer.AppendByte(0x6a);
    LobbyBroadcastToGame(theBuffer.ToByteBuffer(true).get());
}

// FUNCTION: EXE 0x6934e8
// On the host, handles a player's request to change its own team, colour, observing or transfer percent: accepted
// changes go to everyone, a refusal back to the player
// message 0x66
void EELobbyGame::HandlePlayerOptionRequest(ReadBuffer& inBuffer, LobbyClient* inFrom)
{
    if (!this->mIsHost)
        return;
    EELobbyPlayer* thePlayer = (EELobbyPlayer*)inFrom->mPlayer.mPtr;
    if (thePlayer == NULL)
        return;
    int theIndex = thePlayer->GetSlot();
    EELobbyPlayerSlot* theSlot = this->GetPlayerSlot(theIndex);
    if (theSlot == NULL || theSlot->mClient.mPtr != inFrom)
        return;
    int theWhat = inBuffer.ReadByte();
    unsigned char theValue = inBuffer.ReadByte();
    if (thePlayer->GetSlot() != theIndex)
        return;
    bool theOK = false;
    switch (theWhat) {
    case 1:
    case 4:
    case 5:
        theOK = true;
        break;
    case 2:
        theOK = this->IsColorFree(theValue);
        break;
    }
    WriteBuffer theBuffer(0, true);
    theBuffer.AppendByte(0x65);
    theBuffer.AppendByte(theIndex);
    theBuffer.AppendByte(theWhat);
    theBuffer.AppendByte(theOK ? theValue : theSlot->GetOption(theWhat));
    if (theOK)
        LobbyBroadcastToGame(theBuffer.ToByteBuffer(true).get());
    else
        LobbySendToClient(inFrom, theBuffer.ToByteBuffer(true).get());
}

// FUNCTION: EXE 0x693623
// Applies a slot's player option from the captain, announcing team changes that join or leave the local player's team
// message 0x65
void EELobbyGame::HandlePlayerOptionChange(ReadBuffer& inBuffer, LobbyClient* inFrom)
{
    if (!inFrom->IsCaptain(1))
        return;
    int theIndex = (char)inBuffer.ReadByte();
    int theWhat = inBuffer.ReadByte();
    unsigned char theValue = inBuffer.ReadByte();
    EELobbyPlayerSlot* theSlot = this->GetPlayerSlot(theIndex);
    if (theSlot != NULL) {
        if (theWhat == 1) {
            if (theSlot->mClient.mPtr != NULL && theIndex != this->GetMySlot()) {
                unsigned char theOld = theSlot->mOptions[1];
                unsigned char theTeam = this->GetMyTeam();
                if (theTeam != 0 && theOld != theValue) {
                    if (theOld == theTeam)
                        LobbyBroadcastEvent(new PlayerTeamChangedEvent(theSlot->mClient.get(), true), true);
                    else if (theValue == theTeam)
                        LobbyBroadcastEvent(new PlayerTeamChangedEvent(theSlot->mClient.get(), false), true);
                }
            }
        }
        if (theWhat == 4 && theValue == 0) {
            unsigned char theColor = theSlot->mOptions[2];
            theSlot->mOptions[2] = this->GetFreeColor(theSlot->GetIndex(), theColor);
        }
        theSlot->SetOption(theWhat, theValue);
        if (theWhat == 0 || theWhat == 4)
            this->CountPlayers(true);
    }
    LobbyBroadcastEvent(new EELobbySyncPlayerEvent(theIndex), true);
    if (theWhat == 3)
        LobbyResetReadyFlags();
}

// FUNCTION: EXE 0x69379c
// Applies a game option change from the captain and resets everyone's ready flag (message 0x64)
void EELobbyGame::HandleOptionChange(ReadBuffer& inBuffer, LobbyClient* inFrom)
{
    if (!inFrom->IsCaptain(1))
        return;
    int theOption = inBuffer.ReadByte();
    this->ReadOption(inBuffer, theOption);
    LobbyBroadcastEvent(new EELobbySyncOptionEvent(theOption), true);
    LobbyResetReadyFlags();
}

// SYNTHETIC: EXE 0x693804
// (EELobbySyncOptionEvent's scalar deleting destructor; ICF: EELobbySyncPlayerEvent's)

// FUNCTION: EXE 0x69382d
// Sends one game option's value to the game room (message 0x64)
void EELobbyGame::SendOption(int inOption)
{
    WriteBuffer theBuffer(0, true);
    theBuffer.AppendByte(0x64);
    theBuffer.AppendByte(inOption);
    this->WriteOption(theBuffer, inOption);
    LobbyBroadcastToGame(theBuffer.ToByteBuffer(true).get());
}

// FUNCTION: EXE 0x693894
// Sets a game option when the value is below its limit
void EELobbyGame::SetOption(int inOption, unsigned long inValue)
{
    if (inOption >= 0 && inOption < 16 && inValue < sOptionLimits[inOption])
        this->mOptions[inOption] = inValue;
}

// FUNCTION: EXE 0x6938b8
// Changes a game option from the controls, sending it when asked; keeps the starting epoch at or before the ending
// one and re-advertises the game when a listed option changes
// The epoch rule is skipped when the starting epoch is 15; an out-of-range value makes the controls resync the
// option
void EELobbyGame::ChangeOption(int inOption, unsigned long inValue, bool inFlag)
{
    if (inOption < 0 || inOption >= 16)
        return;
    if (inValue >= sOptionLimits[inOption]) {
        LobbyBroadcastEvent(new EELobbySyncOptionEvent(inOption), true);
        return;
    }
    if (this->mOptions[inOption] == inValue)
        return;
    this->mOptions[inOption] = inValue;
    if (inFlag)
        this->SendOption(inOption);
    if (inOption == 4) {
        if ((unsigned long)this->mOptions[5] < (unsigned long)this->mOptions[4] && this->mOptions[4] != 0xf)
            this->ChangeOption(5, this->mOptions[4], inFlag);
    } else if (inOption == 5) {
        if ((unsigned long)this->mOptions[5] < (unsigned long)this->mOptions[4] && this->mOptions[4] != 0xf)
            this->ChangeOption(4, this->mOptions[5], inFlag);
        return;
    } else if (inOption != 2) {
        return;
    }
    LobbyNetAdvertiseGame();
}

// FUNCTION: EXE 0x69398e
// Returns the local player's slot number, or -1
int EELobbyGame::GetMySlot()
{
    EELobbyPlayer* thePlayer = static_cast<EELobbyPlayer*>(GetMyLobbyPlayer());
    if (thePlayer != NULL)
        return thePlayer->GetSlot();
    return -1;
}

// FUNCTION: EXE 0x6939a6
// Returns the local player's team, or -1
int EELobbyGame::GetMyTeam()
{
    EELobbyPlayer* thePlayer = static_cast<EELobbyPlayer*>(GetMyLobbyPlayer());
    if (thePlayer != NULL)
        return thePlayer->GetTeam();
    return -1;
}

// FUNCTION: EXE 0x6939be
// Takes over the whole game state from the captain (options, slots with their clients, the saved game file) and
// resets the ready flags
// message 0x67
void EELobbyGame::HandleGameState(ReadBuffer& inBuffer, LobbyClient* inFrom)
{
    if (!inFrom->IsCaptain(1))
        return;
    int i;
    for (i = 0; i < 16; i++)
        this->ReadOption(inBuffer, i);
    for (i = 0; i < 8; i++) {
        this->mSlots[i].ReadData(inBuffer);
        if (this->mSlots[i].mOptions[0] == 3) {
            LobbyClient* theClient = this->mClients.mPtr->FindClient(inBuffer.ReadShort());
            if (theClient != NULL && theClient->mPlayer.mPtr != NULL)
                this->mSlots[i].SetPlayer(theClient, (EELobbyPlayer*)theClient->mPlayer.mPtr);
            else
                this->mSlots[i].SetOpen();
        }
    }
    if (this->HasGameFile()) {
        this->ReadGameFile(inBuffer);
        this->StartTransfer();
        this->MakeColorsDistinct();
    }
    LobbyBroadcastLobbyEvent(100065, true);
    LobbyResetReadyFlags();
    if (this->mIsHost) {
        this->CountPlayers(false);
        LobbyNetAdvertiseGame();
    }
}

// FUNCTION: EXE 0x693a8b
// Sends the whole game state to the game room: options, slots (with client ids) and the saved game file (message 0x67)
void EELobbyGame::SendGameState()
{
    WriteBuffer theBuffer(0, true);
    theBuffer.AppendByte(0x67);
    int i;
    for (i = 0; i < 16; i++)
        this->WriteOption(theBuffer, i);
    for (i = 0; i < 8; i++) {
        this->mSlots[i].WriteData(theBuffer);
        if (this->mSlots[i].mOptions[0] == 3)
            theBuffer.AppendShort(this->mSlots[i].mClient->mClientId);
    }
    if (this->HasGameFile())
        this->WriteGameFile(theBuffer);
    LobbyBroadcastToGame(theBuffer.ToByteBuffer(true).get());
}

// FUNCTION: EXE 0x693b36
// Changes a slot's player option from the controls: the host sends it to everyone, a player asks the host; a
// refused change resyncs the slot's controls
// messages 0x65 (the host, to everyone) / 0x66 (a player, to the host); a state change by the host kicks the
// slot's client (case 0 falls through to the host check)
void EELobbyGame::ChangePlayerOption(int inSlot, int inWhat, unsigned char inValue)
{
    EELobbyPlayerSlot* theSlot = this->GetPlayerSlot(inSlot);
    if (theSlot == NULL || theSlot->GetOption(inWhat) == inValue)
        return;
    bool theToHost = false;
    bool theOK = true;
    switch (inWhat) {
    case 1:
    case 2:
    case 4:
    case 5:
        theOK = inSlot == this->GetMySlot() || (this->mIsHost && theSlot->mOptions[0] != 3);
        theToHost = true;
        break;
    case 0:
        if (this->mIsHost && inSlot != 0)
            LobbyKickClient(theSlot->mClient.get(), false);
        else
            theOK = false;
    default:
        if (!this->mIsHost)
            theOK = false;
        break;
    }
    if (inWhat == 2 && theOK)
        theOK = this->IsColorFree(inValue);
    if (!theOK) {
        LobbyBroadcastEvent(new EELobbySyncPlayerEvent(inSlot), true);
        return;
    }
    if (this->mIsHost)
        theToHost = false;
    WriteBuffer theBuffer(0, true);
    if (theToHost) {
        theBuffer.AppendByte(0x66);
        theBuffer.AppendByte(inWhat);
        theBuffer.AppendByte(inValue);
        LobbySendToCaptain(theBuffer.ToByteBuffer(true).get());
    } else {
        theBuffer.AppendByte(0x65);
        theBuffer.AppendByte(inSlot);
        theBuffer.AppendByte(inWhat);
        theBuffer.AppendByte(inValue);
        LobbyBroadcastToGame(theBuffer.ToByteBuffer(true).get());
    }
}

// The unit's compiler-generated functions:
// SYNTHETIC: EXE 0x692296
// (??_G EELobbyGame)
// SYNTHETIC: EXE 0x6923f3
// (GUIString::append(const GUIString&), inline)

// TEMPLATE: EXE 0x693cd4
// (SmartPtr<GUIStringData>::SmartPtr(GUIStringData*))

// TEMPLATE: EXE 0x693cf7
// (vector<GUIString>::reserve)

// TEMPLATE: EXE 0x693dc8
// (vector<GUIString>::push_back)

// TEMPLATE: EXE 0x693df4
// (vector<GUIString>::clear)

// TEMPLATE: EXE 0x693e2f
// (vector<GUIString>::insert(iterator, const GUIString&))

// TEMPLATE: EXE 0x693e7f
// (vector<GUIString>::erase(iterator, iterator))

// TEMPLATE: EXE 0x693ee9
// (vector<GUIString>::_Tidy)

// TEMPLATE: EXE 0x693f21
// (vector<GUIString>::insert(iterator, size_type, const GUIString&))

// TEMPLATE: EXE 0x6941d4
// (SmartPtr<GUIStringData>::~SmartPtr)

// TEMPLATE: EXE 0x6941e0
// (vector<GUIString>::_Ufill)

// TEMPLATE: EXE 0x694207
// (vector<GUIString>::_Ucopy)

// TEMPLATE: EXE 0x69422f
// (std::copy for GUIString)

// TEMPLATE: EXE 0x694254
// (std::fill for GUIString)

// TEMPLATE: EXE 0x694273
// (std::copy_backward for GUIString)

// TEMPLATE: EXE 0x6942a1
// (std::_Construct for GUIString)
