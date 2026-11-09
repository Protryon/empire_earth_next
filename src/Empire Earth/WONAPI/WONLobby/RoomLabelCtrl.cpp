// The label over a chat room or game: prefix, server, room name and user count
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdio.h>
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
namespace WONAPI {
// FUNCTION: WON 0x007c32b0
// Creates the room label showing the server name, for no room kind yet
RoomLabelCtrl::RoomLabelCtrl() : mRoomSpecFlags(0),mUserCt(0) { this->mShowServer=true; }
// No explicit destructor: the original's (0x7c33a0) only destroys the members and the base
// FUNCTION: WON 0x007c3480
// Finds the text label in the layout, fills it and listens to lobby broadcasts
void RoomLabelCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig,this->mLabel,"Label",__FILE__,29);
    this->CalcLabel();
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007c34c0
// Builds the label text: disconnected, or prefix, server and room name with the user count
void RoomLabelCtrl::CalcLabel()
{
    GUIString aFormat;
    char aCount[33] = "";
    if(this->mRoom.empty())
        aFormat = ::RoomLabelCtrl_LabelDisconnected_String;
    else
    {
        if(this->mPrefix.length()!=0) aFormat.append(::RoomLabelCtrl_LabelPrefix_String);
        if(this->mShowServer && this->mServer.length()!=0) aFormat.append(::RoomLabelCtrl_LabelServer_String);
        aFormat.append(this->mRoom);
        if(this->mUserCt>0)
        {
            sprintf(aCount,"%d",this->mUserCt);
            const GUIString& aSuffix = this->mUserCt==1 ? ::RoomLabelCtrl_LabelUser_String : ::RoomLabelCtrl_LabelUsers_String;
            aFormat.append(aSuffix);
        }
    }
    this->mLabel->SetText(LobbyFormatString3(aFormat,this->mPrefix,this->mServer,aCount),true);
}

// FUNCTION: WON 0x007c3720
// Tracks joining and leaving rooms of its kind and players coming and going, relabelling each time
void RoomLabelCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType)
    {
    case LobbyEvent_JoinedRoom:
        {
            JoinedRoomEvent* aEvent=static_cast<JoinedRoomEvent*>(inEvent);
            if((this->mRoomSpecFlags&aEvent->GetRoomSpec())!=aEvent->GetRoomSpec()) return;
            this->mServer=aEvent->mServerName;
            this->mRoom=aEvent->mGroupName;
            if(this->mUserCt>=0) this->mUserCt=aEvent->mClients->mClients.size();
            this->CalcLabel();
        }
        return;
    case LobbyEvent_LeftRoom:
        {
            LeftRoomEvent* aEvent=static_cast<LeftRoomEvent*>(inEvent);
            if((this->mRoomSpecFlags&aEvent->GetRoomSpec())!=aEvent->GetRoomSpec()) return;
            this->mServer.erase();
            this->mRoom.erase();
            if(this->mUserCt>0) this->mUserCt=0;
            this->CalcLabel();
        }
        return;
    case LobbyEvent_ClientChanged:
        {
            if(this->mUserCt<0) return;
            ClientChangedEvent* aEvent=static_cast<ClientChangedEvent*>(inEvent);
            if((this->mRoomSpecFlags&aEvent->GetRoomSpec())!=aEvent->GetRoomSpec()) return;
            if(aEvent->IsAdd()) { ++this->mUserCt; this->CalcLabel(); return; }
            if(this->mUserCt>0 && aEvent->IsDelete()) --this->mUserCt;
            this->CalcLabel();
        }
        return;
    case LobbyEvent_SetRoomSpecFlags:
        this->mRoomSpecFlags|=static_cast<SetRoomSpecFlagsEvent*>(inEvent)->mFlags;
        return;
    default:
        Component::HandleComponentEvent(inEvent);
    }
}
}


namespace WONAPI {
// FUNCTION: WON 0x007c3930
// Reads the PREFIX, GROUPTYPE (CHAT or GAME), SHOWUSERCOUNT and SHOWSERVER layout keys
bool RoomLabelCtrlConfig::Virtual24(const std::string& inKey)
{
    if(inKey=="PREFIX")
    {
        this->mRoomLabel->SetPrefix(this->ReadString(false));
    }
    else if(inKey=="GROUPTYPE")
    {
        GUIString aGroup=this->ReadString(false);
        if(aGroup.CompareNoCase("CHAT",0)==0) this->mRoomLabel->SetRoomSpecFlags(1);
        else if(aGroup.CompareNoCase("GAME",0)==0) this->mRoomLabel->SetRoomSpecFlags(2);
        else throw ConfigObjectException("Invalid Group Type: "+(std::string)aGroup);
    }
    else if(inKey=="SHOWUSERCOUNT")
    {
        this->mRoomLabel->SetShowUserCount(this->ReadBool());
    }
    else if(inKey=="SHOWSERVER")
    {
        this->mRoomLabel->SetShowServer(this->ReadBool());
    }
    else return ContainerConfig::Virtual24(inKey);
    return true;
}
// SetComponent has no address of its own: the linker folded it with the identical body at 0x79afb0
void RoomLabelCtrlConfig::SetComponent(Component* inComponent)
{
    this->mRoomLabel=static_cast<RoomLabelCtrl*>(inComponent);
    ContainerConfig::SetComponent(inComponent);
}


}

namespace WONAPI {
// FUNCTION: WON 0x007655e0
// Creates a config for a room label in the layout file
ComponentConfig* RoomLabelCtrlConfig::CfgFactory()
{
    return new RoomLabelCtrlConfig;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c33a0
// (public: virtual __thiscall WONAPI::RoomLabelCtrl::~RoomLabelCtrl(void)    [??1RoomLabelCtrl@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c3380
// (public: virtual void * __thiscall WONAPI::RoomLabelCtrl::`scalar deleting destructor'(unsigned int)    [??_GRoomLabelCtrl@WONAPI@@UAEPAXI@Z])
