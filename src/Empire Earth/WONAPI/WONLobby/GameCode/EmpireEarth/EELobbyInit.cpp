// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyInit.cpp (__FILE__ 0x8d0be8, passed by the component
// getters of lines 328/329). The game's set-up of the WON lobby: its factories, the server list's column handlers, the
// component types of the resource file, and the accessors of the lobby's dialogs: 0x694d62..0x696aaa (its two
// SmartPtr init chains first; EELobbyGameOptionsCtrl.cpp ends at 0x694d62).
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
using namespace WONAPI;

SmartPtr<EELobbyConnectDialog> gEELobbyConnectDialog;
SmartPtr<EELobbyFileDialog> gEELobbyFileDialog;

// FUNCTION: EXE 0x694dbe
// Creates the lobby config with the EEFileTransferPort key (default 8889)
EELobbyConfig::EELobbyConfig()
{
    this->KeyAdd("EEFileTransferPort", this->mFileTransferPort, 8889);
}

// FUNCTION: EXE 0x694ebc
// Returns the lobby string naming a game option's value (first string + value), or empty when out of range
GUIString EELobbyServerDetailsItem::GetOptionText(int inOption, int inFirstString, unsigned long inCount)
{
    unsigned long theValue = this->GetGame()->GetOption(inOption);
    if (theValue < inCount)
        return EELobbyResource_GetString(theValue + inFirstString);
    return gEmptyGUIString;
}

// FUNCTION: EXE 0x694f02
// Returns a game option's value as decimal text
GUIString EELobbyServerDetailsItem::GetNumberText(int inOption)
{
    char theBuffer[50];
    sprintf(theBuffer, "%ld", this->GetGame()->GetOption(inOption));
    return GUIString(theBuffer);
}

// FUNCTION: EXE 0x694f39
// Returns the number of wonders needed for victory as text, or "wonders off" when it is 0
GUIString EELobbyServerDetailsItem::GetWondersText()
{
    if (this->GetGame()->GetOption(10) == 0)
        return gEELobbyGameOptionsCtrl_WondersOff;
    return this->GetNumberText(10);
}

// FUNCTION: EXE 0x694f7f
// Returns the name of the game's starting or ending epoch (option 15 shows the 16th epoch name)
GUIString EELobbyServerDetailsItem::GetEpochText(bool inStart)
{
    int theOption = inStart ? 4 : 5;
    if (this->GetGame()->GetOption(theOption) == 15)
        return gEELobbyGameOptionsCtrl_Epoch16;
    return this->GetOptionText(theOption, 0, 16);
}

// FUNCTION: EXE 0x694fd7
// Returns "on" or "off" for a game option flag
GUIString EELobbyServerDetailsItem::GetOnOffText(int inOption)
{
    return this->GetGame()->GetOption(inOption) != 0 ? gEELobbyGameOptionsCtrl_OptionOn : gEELobbyGameOptionsCtrl_OptionOff;
}

// FUNCTION: EXE 0x695010
// Draws a listed game's details: two label/value columns of game options (type, map, epochs, speed, flags ...),
// then the players
void EELobbyServerDetailsItem::PaintDetailsHook(Graphics& inGraphics, MultiListArea* inList)
{
    EELobbyGame* theGame = this->GetGame();
    int theCounts[4] = { 8, 8, 7, 7 };
    GUIString theLabels1[8] = { gEELobbyGameOptionsCtrl_GameType, gEELobbyGameOptionsCtrl_MapSize,
        gEELobbyGameOptionsCtrl_MapType, gEELobbyGameOptionsCtrl_Resources, gEELobbyGameOptionsCtrl_StartingEpoch,
        gEELobbyGameOptionsCtrl_EndingEpoch, gEELobbyGameOptionsCtrl_GameUnitLimit, gEELobbyGameOptionsCtrl_GameVariant };
    GUIString theValues1[8] = { this->GetOptionText(0, 0x10, 3), this->GetOptionText(1, 0x13, 6),
        EELobbyGetMapTypeName(theGame->GetOption(2)), this->GetOptionText(3, 0x19, 5), this->GetEpochText(true),
        this->GetEpochText(false), this->GetNumberText(9), this->GetOptionText(6, 0x1e, 2) };
    GUIString theLabels2[7] = { gEELobbyGameOptionsCtrl_DifficultyLevel, gEELobbyGameOptionsCtrl_GameSpeed,
        gEELobbyGameOptionsCtrl_RevealMap, gEELobbyGameOptionsCtrl_UseCustomCivs, gEELobbyGameOptionsCtrl_LockTeams,
        gEELobbyGameOptionsCtrl_LockSpeed, gEELobbyGameOptionsCtrl_NumberOfWonders };
    GUIString theValues2[7] = { this->GetOptionText(7, 0x20, 3), this->GetOptionText(8, 0x23, 4), this->GetOnOffText(0xb),
        this->GetOnOffText(0xc), this->GetOnOffText(0xd), this->GetOnOffText(0xe), this->GetWondersText() };
    GUIString* theColumns[4] = { theLabels1, theValues1, theLabels2, theValues2 };
    int theGaps[4] = { 10, 50, 10, 50 };
    unsigned long theColors[4] = { 0x8000000f, 0x8000000e, 0x8000000f, 0x8000000e };
    Font* theFont = inGraphics.mFont.get(); int theX = 0, theY, i;
    for (i = 0; i < 4; i++) {
        theY = 0;
        int theWidth = 0;
        inGraphics.SetColor(theColors[i]);
        for (int j = 0; j < theCounts[i]; j++) {
            int theTextWidth = theFont->GetStringWidth(theColumns[i][j], 0, theColumns[i][j].length());
            if (theTextWidth > theWidth)
                theWidth = theTextWidth;
            inGraphics.DrawString(theColumns[i][j], theX, theY);
            theY += theFont->GetHeight();
        }
        theX += theGaps[i] + theWidth;
    }
    theY = 0;
    inGraphics.SetColor(0x8000000f);
    inGraphics.DrawString(gEELobbyInterface_Players, theX, 0);
    theX += theFont->GetStringWidth(gEELobbyInterface_Players, 0, gEELobbyInterface_Players.length()) + 10;
    inGraphics.SetColor(0x8000000e);
    int theNumFiles = theGame->GetNumFiles();
    for (i = 0; i < theNumFiles; i++) {
        const GUIString& theFile = theGame->GetFile(i);
        inGraphics.DrawString(theFile, theX, theY);
        theY += theFont->GetHeight();
    }
}

// FUNCTION: EXE 0x6954fc
// Returns the details area's height, nine lines of the list's font
int EELobbyServerDetailsItem::GetDetailsHeightHook(MultiListArea* inList)
{
    return inList->mFont->GetHeight() * 9;
}

static LobbyPlayer* EELobbyNewPlayer();
static LobbyGame* EELobbyNewGame();
static ComponentConfig* EELobbyNewButtonConfig();
static ComponentConfig* EELobbyNewContainerConfig();
static ComponentConfig* EELobbyNewDialogConfig();
static Component* EELobbyNewGameOptionsCtrl();
static Component* EELobbyNewPlayerOptionsCtrl();
static Component* EELobbyNewColorButton();
static Component* EELobbyNewFileDialog();
static Component* EELobbyNewConnectDialog();
static LobbyConfig* EELobbyNewConfig();
static ServerDetailsItem* EELobbyNewServerDetailsItem();
static void EELobbyEpochGetText(Graphics& inGraphics, ServerDetailsItem* inItem, ServerListCtrl*);
static int EELobbyEpochCompare(const ServerDetailsItem* inA, const ServerDetailsItem* inB, ServerListCtrl*);
static void EELobbyEpochFilter(const GUIString& inText, std::list<ServerDetailsItem*>& ioItems, ServerListCtrl*);
static void EELobbyMapTypeGetText(Graphics& inGraphics, ServerDetailsItem* inItem, ServerListCtrl*);
static int EELobbyMapTypeCompare(const ServerDetailsItem* inA, const ServerDetailsItem* inB, ServerListCtrl*);
static void EELobbyMapTypeFilter(const GUIString& inText, std::list<ServerDetailsItem*>& ioItems, ServerListCtrl*);
static void EELobbyGetComponents(ComponentConfig* inConfig);

// FUNCTION: EXE 0x695511
// Sets the lobby up for Empire Earth: epoch and map type server list columns, game/player factories, the game's
// dialog and control types, resources, epoch and map type options, player colours and the product name
void EELobbyInit()
{
    ServerListCtrl::ColumnInfo* theHandlers = ServerListCtrl::GetColumnInfo("Epoch");
    theHandlers->mPaintFunc = EELobbyEpochGetText;
    theHandlers->mCompareFunc = EELobbyEpochCompare;
    theHandlers->mFilterFunc = EELobbyEpochFilter;
    theHandlers = ServerListCtrl::GetColumnInfo("MapType");
    theHandlers->mPaintFunc = EELobbyMapTypeGetText;
    theHandlers->mCompareFunc = EELobbyMapTypeCompare;
    theHandlers->mFilterFunc = EELobbyMapTypeFilter;

    LobbyGame::SetGameFactory(EELobbyNewGame);
    LobbyPlayer::SetPlayerFactory(EELobbyNewPlayer);
    ServerDetailsItem::SetItemFactory(EELobbyNewServerDetailsItem);
    LobbyConfig::SetAllocator(EELobbyNewConfig);

    ResourceConfigTablePtr theCollection = EELobbyResource_Init(NULL);
    theCollection->AddAllocator("GameOptionsCtrl", EELobbyNewContainerConfig, EELobbyNewGameOptionsCtrl);
    theCollection->AddAllocator("PlayerOptionsCtrl", EELobbyNewContainerConfig, EELobbyNewPlayerOptionsCtrl);
    theCollection->AddAllocator("EELobbyColorButton", EELobbyNewButtonConfig, EELobbyNewColorButton);
    theCollection->AddAllocator("EELobbyFileDialog", EELobbyNewDialogConfig, EELobbyNewFileDialog);
    theCollection->AddAllocator("EELobbyConnectDialog", EELobbyNewDialogConfig, EELobbyNewConnectDialog);
    EELobbyAddResources(theCollection.mPtr);
    LobbyAddComponentLoader(theCollection.mPtr, EELobbyGetComponents);

    for (int i = 0; i < 16; i++)
        LobbyAddGameOption("Epoch", EELobbyResource_GetString(i), true);
    EELobbyAddMapTypeOptions();
    EELobbyGetPlayerColors(gEELobbyColors, 16);
    LobbyAddGameOption("MapType", gEELobbyGameOptionsCtrl_GameType2, true);
    LobbySetProductName(L"EEAOC", NULL);
}

// FUNCTION: EXE 0x6957fa
// Creates an Empire Earth lobby player
static LobbyPlayer* EELobbyNewPlayer()
{
    return new EELobbyPlayer;
}

// FUNCTION: EXE 0x695811
// Creates an Empire Earth lobby game
static LobbyGame* EELobbyNewGame()
{
    return new EELobbyGame;
}

// FUNCTION: EXE 0x69582b
// Creates a button config for the resource file
static ComponentConfig* EELobbyNewButtonConfig()
{
    return new ButtonConfig;
}

// FUNCTION: EXE 0x695842
// Creates a container config for the resource file
static ComponentConfig* EELobbyNewContainerConfig()
{
    return new ContainerConfig;
}

// FUNCTION: EXE 0x69585c
// Creates a dialog config for the resource file
static ComponentConfig* EELobbyNewDialogConfig()
{
    return new DialogConfig;
}

// FUNCTION: EXE 0x69589f
// Creates the game options control
static Component* EELobbyNewGameOptionsCtrl()
{
    return new EELobbyGameOptionsCtrl;
}

// FUNCTION: EXE 0x6958b9
// Creates the player options control
static Component* EELobbyNewPlayerOptionsCtrl()
{
    return new EELobbyPlayerOptionsCtrl;
}

// FUNCTION: EXE 0x6958d3
// Creates a player colour button
static Component* EELobbyNewColorButton()
{
    return new EELobbyColorButton;
}

// FUNCTION: EXE 0x6958ea
// Creates the saved game / scenario file dialog
static Component* EELobbyNewFileDialog()
{
    return new EELobbyFileDialog;
}

// FUNCTION: EXE 0x695904
// Creates the connect dialog shown while players connect to the game
static Component* EELobbyNewConnectDialog()
{
    return new EELobbyConnectDialog;
}

// FUNCTION: EXE 0x69591e
// Creates the Empire Earth lobby config
static LobbyConfig* EELobbyNewConfig()
{
    return new EELobbyConfig;
}

// FUNCTION: EXE 0x695938
// Creates a server list item with Empire Earth game details
static ServerDetailsItem* EELobbyNewServerDetailsItem()
{
    return new EELobbyServerDetailsItem;
}

static GUIString EELobbyGetEpochText(ServerDetailsItem* inItem, bool inStart);
static GUIString EELobbyGetMapTypeText(EELobbyGame* inGame);

// FUNCTION: EXE 0x695978
// Draws a listed game's starting epoch in the epoch column
static void EELobbyEpochGetText(Graphics& inGraphics, ServerDetailsItem* inItem, ServerListCtrl*)
{
    GUIString theText = EELobbyGetEpochText(inItem, true);
    inGraphics.DrawString(theText, 0, 0);
}

// FUNCTION: EXE 0x6959aa
// Returns a listed game's starting epoch name
// The flag is not passed on: every caller asks for the starting epoch
static GUIString EELobbyGetEpochText(ServerDetailsItem* inItem, bool inStart)
{
    return (static_cast<EELobbyServerDetailsItem*>(inItem))->GetEpochText(true);
}

// FUNCTION: EXE 0x6959be
// Orders two listed games by starting epoch
static int EELobbyEpochCompare(const ServerDetailsItem* inA, const ServerDetailsItem* inB, ServerListCtrl*)
{
    EELobbyGame* theA = static_cast<EELobbyGame*>(inA->mGame.get());
    long theB = (static_cast<EELobbyGame*>(inB->mGame.get()))->GetOption(4);
    return theA->GetOption(4) - theB;
}

// FUNCTION: EXE 0x6959e5
// Removes listed games whose starting epoch name does not contain the filter text (any case)
// GUIStringData::findNoCase (0x728340): a substring search
static void EELobbyEpochFilter(const GUIString& inText, std::list<ServerDetailsItem*>& ioItems, ServerListCtrl*)
{
    std::list<ServerDetailsItem*>::iterator theIterator = ioItems.begin();
    while (theIterator != ioItems.end()) {
        GUIString theText = EELobbyGetEpochText(*theIterator, true);
        if (theText.findNoCase(inText, 0) >= 0)
            ++theIterator;
        else
            ioItems.erase(theIterator++);
    }
}

// FUNCTION: EXE 0x695a59
// Draws a listed game's map type in the map type column
static void EELobbyMapTypeGetText(Graphics& inGraphics, ServerDetailsItem* inItem, ServerListCtrl*)
{
    GUIString theText = EELobbyGetMapTypeText(static_cast<EELobbyGame*>(inItem->mGame.get()));
    inGraphics.DrawString(theText, 0, 0);
}

// FUNCTION: EXE 0x695a8b
// Returns a game's random map type name, or the second game type's name for other game types
static GUIString EELobbyGetMapTypeText(EELobbyGame* inGame)
{
    if (inGame->GetOption(0) == 0)
        return EELobbyGetMapTypeName(inGame->GetOption(2));
    return gEELobbyGameOptionsCtrl_GameType2;
}

// FUNCTION: EXE 0x695ad1
// Orders two listed games by map type name, ignoring case
static int EELobbyMapTypeCompare(const ServerDetailsItem* inA, const ServerDetailsItem* inB, ServerListCtrl*)
{
    GUIString theA = EELobbyGetMapTypeText(static_cast<EELobbyGame*>(inA->mGame.get()));
    GUIString theB = EELobbyGetMapTypeText(static_cast<EELobbyGame*>(inB->mGame.get()));
    return theA.CompareNoCase(theB);
}

// FUNCTION: EXE 0x695b21
// Removes listed games whose map type name does not contain the filter text (any case)
static void EELobbyMapTypeFilter(const GUIString& inText, std::list<ServerDetailsItem*>& ioItems, ServerListCtrl*)
{
    std::list<ServerDetailsItem*>::iterator theIterator = ioItems.begin();
    while (theIterator != ioItems.end()) {
        GUIString theText = EELobbyGetMapTypeText(static_cast<EELobbyGame*>((*theIterator)->mGame.get()));
        if (theText.findNoCase(inText, 0) >= 0)
            ++theIterator;
        else
            ioItems.erase(theIterator++);
    }
}

// FUNCTION: EXE 0x695b95
// Fetches the file dialog and connect dialog from the loaded lobby resources
static void EELobbyGetComponents(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, gEELobbyFileDialog, "EELobbyFileDialog", __FILE__, 328);
    SafeGetComponent(inConfig, gEELobbyConnectDialog, "EELobbyConnectDialog", __FILE__, 329);
}

// FUNCTION: EXE 0x695bd2
// Shuts the lobby down: closes it, empties its strings and releases the file and connect dialogs
// Both assignments call the ICF-folded SmartPtr<T>::operator=(T*) at 0x699ae3 (named after its GUIStringData
// instance)
void EELobbyShutdown()
{
    EELobbyClose();
    EELobbyResource_Destroy();
    gEELobbyFileDialog = NULL;
    gEELobbyConnectDialog = NULL;
}

// FUNCTION: EXE 0x695bf5
// Returns the configured file transfer port (8889 without a lobby config)
unsigned short EELobbyGetFileTransferPort()
{
    EELobbyConfig* theConfig = static_cast<EELobbyConfig*>(GetLobbyConfig());
    if (theConfig == NULL)
        return 8889;
    return theConfig->mFileTransferPort;
}

// FUNCTION: EXE 0x695c0b
// Returns the lobby's file dialog
EELobbyFileDialog* EELobbyGetFileDialog()
{
    return gEELobbyFileDialog.mPtr;
}

// FUNCTION: EXE 0x695c11
// Returns the lobby's connect dialog
EELobbyConnectDialog* EELobbyGetConnectDialog()
{
    return gEELobbyConnectDialog.mPtr;
}

// FUNCTION: EXE 0x695c17
// Runs the connect dialog for the player's game; on failure the host cancels the launch and a client reports it
// and leaves the game (unless the dialog was ended for it); returns whether everyone connected
bool EELobbyRunConnectDialog()
{
    SmartPtr<EELobbyConnectDialog> theDialog = EELobbyGetConnectDialog();
    if (theDialog.mPtr == NULL)
        return false;
    EELobbyGame* theGame = static_cast<EELobbyGame*>(GetMyLobbyGame());
    if (theGame == NULL)
        return false;
    bool theFlag = theGame->mIsHost;
    bool theResult = theDialog->Run(theGame);
    theGame = static_cast<EELobbyGame*>(GetMyLobbyGame());
    if (theGame == NULL)
        return false;
    if (!theResult) {
        if (theFlag) {
            LobbySetGameInProgress(0);
            theGame->SendCancelConnect();
        } else if (!theDialog->mIsEnded) {
            theGame->SendConnectStatus(false);
            LobbyLeaveGame(false);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x695cab
// Marks a player slot connected or not in the connect dialog
void EELobbySetSlotConnected(unsigned char inSlot, bool inConnected)
{
    if (gEELobbyConnectDialog.mPtr != NULL)
        gEELobbyConnectDialog->SetConnected(inSlot, inConnected);
}

// FUNCTION: EXE 0x695cc3
// Closes the connect dialog, unless this player is the game's captain
void EELobbyCancelConnectDialog()
{
    if (gEELobbyConnectDialog.mPtr != NULL && !LobbyIAmCaptain())
        gEELobbyConnectDialog->End(0);
}

static void EELobbySendConnectStatus(int inConnected);

// FUNCTION: EXE 0x695ce3
// Posts this client's connect status to the lobby window's thread for sending
void EELobbyPostConnectStatus(bool inConnected)
{
    WindowManager* theWindow = GetLobbyWindow();
    if (theWindow != NULL)
        theWindow->PostEvent(new UserEvent(EELobbySendConnectStatus, inConnected != false), true);
}

// FUNCTION: EXE 0x695d38
// Sends this client's connect status for the player's lobby game
static void EELobbySendConnectStatus(int inConnected)
{
    EELobbyGame* theGame = static_cast<EELobbyGame*>(GetMyLobbyGame());
    if (theGame != NULL)
        theGame->SendConnectStatus(inConnected != 0);
}

// FUNCTION: EXE 0x695d52
// Returns whether the connect dialog is running
bool EELobbyIsConnectDialogRunning()
{
    if (gEELobbyConnectDialog.mPtr != NULL)
        return gEELobbyConnectDialog->mIsRunning;
    return false;
}

// SYNTHETIC: EXE 0x694d62
// SYNTHETIC: EXE 0x694d6c
// SYNTHETIC: EXE 0x694d74
// SYNTHETIC: EXE 0x694d80
// SYNTHETIC: EXE 0x694d90
// SYNTHETIC: EXE 0x694d9a
// SYNTHETIC: EXE 0x694da2
// SYNTHETIC: EXE 0x694dae

// The unit's other compiler-generated functions (the vtables' ??_G, inline virtuals, the getter templates):
// SYNTHETIC: EXE 0x694de8
// (??_G EELobbyConfig)
// SYNTHETIC: EXE 0x695883
// (??_G DialogConfig, emitted with EELobbyNewDialogConfig's inline ctor)
// SYNTHETIC: EXE 0x69595c
// (??_G EELobbyServerDetailsItem)
// SYNTHETIC: EXE 0x695d30
// (UserEvent::Deliver)
// TEMPLATE: EXE 0x696948
// (SafeGetComponent<EELobbyFileDialog>)
// TEMPLATE: EXE 0x6969c8
// (SafeGetComponent<EELobbyConnectDialog>)
// The implicit dtors of EELobbyConfig (LobbyConfig's, ICF) and ConfigObject, then the map/list
// COMDATs they instantiate:
// TEMPLATE: EXE 0x694e04
// TEMPLATE: EXE 0x694e83
// TEMPLATE: EXE 0x695d65
// TEMPLATE: EXE 0x695d90
// TEMPLATE: EXE 0x695dbb
// TEMPLATE: EXE 0x695de6
// TEMPLATE: EXE 0x695e57
// TEMPLATE: EXE 0x695ec8
// TEMPLATE: EXE 0x695f39
// TEMPLATE: EXE 0x6961f1
// TEMPLATE: EXE 0x69622d
// TEMPLATE: EXE 0x6964e3
// TEMPLATE: EXE 0x69651d
// TEMPLATE: EXE 0x6967d3
// TEMPLATE: EXE 0x69680d
// TEMPLATE: EXE 0x696848
// TEMPLATE: EXE 0x69688a
// TEMPLATE: EXE 0x6968ce
// TEMPLATE: EXE 0x696910
// TEMPLATE: EXE 0x696a48
// TEMPLATE: EXE 0x696a74
// TEMPLATE: EXE 0x696a8f
