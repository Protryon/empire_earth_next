// The game room's leave/dissolve and start/ready buttons
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyControls.h"
namespace WONAPI {
bool LobbyIAmReady();
// FUNCTION: WON 0x007bc740
// Creates the game room buttons panel, not in a game
GameStagingCtrl::GameStagingCtrl() : mInGame(false),mLastReady(false) { this->mRoomSpecFlags = 2; }
// The implicit destructor (0x7bc7a0) releases the start button (+0x88), then the leave button (+0x84), then the
// Container base
// FUNCTION: WON 0x007bc840
// Looks up the leave and start buttons from the layout, the start button disabled
void GameStagingCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mLeaveGameButton,"LeaveGameButton",__FILE__,33);
    SafeGetComponent(config,this->mStartGameButton,"StartGameButton",__FILE__,34);
    this->mStartGameButton->SetEnabled(false);
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007bc8b0
// Host: enables Start when the game can start; others: relabels Ready/Not Ready and enables it if allowed
void GameStagingCtrl::UpdateStartButton()
{
    if(!this->mInGame)
        return;
    LobbyGame* game=GetMyLobbyGame();
    if(game==NULL)
        return;
    bool enabled;
    if(game->mIsHost) enabled=game->CanStartGame();
    else {
        if(this->mLastReady!=LobbyIAmReady()) {
            this->mLastReady=LobbyIAmReady();
            this->mStartGameButton->SetText(this->mLastReady?::GameStagingCtrl_NotReady_String : ::GameStagingCtrl_Ready_String,true);
        }
        if(!this->mLastReady) enabled=game->CanReady();
        else enabled=true;
    }
    this->mStartGameButton->SetEnabled(enabled);
}
// FUNCTION: WON 0x007bc950
// Labels the buttons for host or player on entering a matching game room; another room leaves the state as is
void GameStagingCtrl::JoinedGame()
{
    LobbyGame* game=GetMyLobbyGame();
    if(game==NULL||!game->MatchesRoomSpec(this->mRoomSpecFlags))
        return;
    this->mInGame=true;
    if(game->mIsHost) {
        this->mLeaveGameButton->SetText(::GameStagingCtrl_DissolveGame_String,true);
        this->mStartGameButton->SetText(::GameStagingCtrl_StartGame_String,true);
    } else {
        this->mLeaveGameButton->SetText(::GameStagingCtrl_LeaveGame_String,true);
        this->mLastReady=LobbyIAmReady();
        this->mStartGameButton->SetText(this->mLastReady?::GameStagingCtrl_NotReady_String : ::GameStagingCtrl_Ready_String,true);
    }
    this->UpdateStartButton();
}
// FUNCTION: WON 0x007bca00
// Marks the panel as no longer in a game
void GameStagingCtrl::LeftGame() { this->mInGame=false; }
// FUNCTION: WON 0x007bca10
// Handles joining and leaving a game room, room filters and ready changes, then passes the event on
void GameStagingCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case 100029:this->JoinedGame();break;
    case 100032:this->LeftGame();break;
    case LobbyEvent_SetRoomSpecFlags:this->mRoomSpecFlags|=static_cast<SetRoomSpecFlagsEvent*>(inEvent)->mFlags;break;
    case 100061:this->UpdateStartButton();break;
    }
    Component::HandleComponentEvent(inEvent);
}
}


// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bc780
// (public: virtual void * __thiscall WONAPI::GameStagingCtrl::`scalar deleting destructor'(unsigned int)    [??_GGameStagingCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007bc7a0
// (public: virtual __thiscall WONAPI::GameStagingCtrl::~GameStagingCtrl(void)    [??1GameStagingCtrl@WONAPI@@UAE@XZ])
