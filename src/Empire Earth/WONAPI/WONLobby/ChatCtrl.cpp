// ChatCtrl: a lobby chat panel (output, input line, Send button) and the clickable links it prints
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233

#include <windows.h>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <set>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyRichText.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyControls.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
using namespace WONAPI;
// A clickable link in the chat output (a client, friend, room or Accept/Reject). The original's RTTI names it in
// the global namespace (vtable 0x852df8, 0x8c bytes).
class ChatLink : public WONAPI::TextLink {
public:
    ChatLink(WONAPI::LobbyClient* inClient,WONAPI::Font* inFont,int inColor) : mKind(3)
    {
        this->mClient=inClient;
        this->mText=inClient->GetName();
        this->mColor=inColor;
        this->mFont=inFont;
        this->mEnabled=true;
        this->mPrevious=NULL;
    }
    ChatLink(const WONAPI::GUIString& inText,unsigned short inId,WONAPI::LobbyServer* inServer,
        WONAPI::Font* inFont,int inColor) : mKind(0)
    {
        this->mText=inText;
        this->mGroupId=inId;
        this->mColor=inColor;
        this->mFont=inFont;
        this->mEnabled=true;
        this->mPrevious=NULL;
        if(inServer!=NULL) this->mAddress=inServer->GetAddress();
    }
    ChatLink(WONAPI::LobbyFriend* inFriend,WONAPI::Font* inFont,int inColor) : mKind(4)
    {
        this->mFriend=inFriend;
        this->mText=inFriend->GetName();
        this->mColor=inColor;
        this->mFont=inFont;
        this->mEnabled=true;
        this->mPrevious=NULL;
    }
    ChatLink(WONAPI::LobbyFriend* inFriend,const WONAPI::GUIString& inText,
        const WONAPI::IPAddr& inAddress,WONAPI::Font* inFont,int inColor) : mKind(5)
    {
        this->mFriend=inFriend;
        this->mAddress=inAddress;
        this->mText=inText;
        this->mColor=inColor;
        this->mFont=inFont;
        this->mEnabled=true;
        this->mPrevious=NULL;
    }
    ChatLink(int inKind,WONAPI::LobbyClient* inClient,const WONAPI::GUIString& inText,
        WONAPI::Font* inFont,int inColor) : mKind(inKind)
    {
        this->mClient=inClient;
        this->mText=inText;
        this->mColor=inColor;
        this->mFont=inFont;
        this->mEnabled=true;
        this->mPrevious=NULL;
    }
    // implicit destructor (0x7b7190): the original stores no vptr of its own
    virtual bool IsLink(); // 0x7b7160
    int mKind; // 0x48: kind (0 room, 1 accept, 2 reject, 3 client, 4 friend, 5 friend's location)
    WONAPI::IPAddr mAddress; // 0x4c..0x74: server or friend's address
    unsigned short mGroupId; // 0x74: room (group) id
    WONAPI::SmartPtr<WONAPI::LobbyClient> mClient; // 0x78
    WONAPI::SmartPtr<WONAPI::LobbyFriend> mFriend; // 0x7c
    bool mEnabled; // 0x80: still clickable
    WONAPI::SmartPtr<ChatLink> mNext; // 0x84: next link in the chain (counted reference)
    ChatLink* mPrevious; // 0x88: previous link (not counted, never released)
};

// FUNCTION: WON 0x007b7160
// Returns whether the link can still be clicked
bool ChatLink::IsLink() { return this->mEnabled; }

namespace WONAPI {
// FUNCTION: WON 0x007b6b80
// Creates a chat panel with its default text colors, no room joined and no room kind set
ChatCtrl::ChatCtrl() : mRoomName(GetEmptyGUIStringData())
{
    this->mDefaultColor=0;
    this->mAcceptColor=0;
    this->mRoomSpecFlags=0;
    this->mErrorColor=0xff0000;
    this->mHelpColor=0x404040;
    this->mUserColor=0x800000;
    this->mEmoteColor=0x800080;
    this->mWhisperColor=0x8000;
    this->mWarningColor=0xff8000;
    this->mGroupColor=0x80;
    this->mMessageColor=0xff;
    this->mRejectColor=0xa0a0a0;
    this->mClientId=0xffff;
}
ChatCtrl::~ChatCtrl() {} // explicit: the original destructor (0x7b6ca0) stores the ChatCtrl vptr at 0x7b6cbf
// FUNCTION: WON 0x007b6f00
// Returns whether this is a team chat panel (room flag 0x10, set by GROUPTYPE TEAM)
bool ChatCtrl::IsTeamRoom() { return (this->mRoomSpecFlags&16)!=0 ? true : false; }

// FUNCTION: WON 0x007b6f10
// Appends formatted text to the chat output, filling in strings and links with the panel's colors and fonts
void ChatCtrl::AddFormattedText(const GUIString& inFormat,const GUIString*const* inStrings,int inNumStrings,
    TextSegmentBase*const* inSegments,int inNumSegments,bool inLineBreak)
{
    int aColors[9]={this->mDefaultColor,this->mUserColor,this->mWhisperColor,this->mEmoteColor,this->mWarningColor,
        this->mErrorColor,this->mHelpColor,this->mGroupColor,this->mMessageColor};
    Font* aFonts[3]={this->mChatOutput->mFont.get(),this->mBoldFont.get(),this->mItalicFont.get()};
    LobbyAppendFormattedText(this->mChatOutput.get(),inFormat,inStrings,inNumStrings,aColors,9,aFonts,3,
        inSegments,inNumSegments,inLineBreak);
}
// FUNCTION: WON 0x007b6fd0
// Appends formatted text with one string argument to the chat output
void ChatCtrl::AddTextLine(const GUIString& inFormat,const GUIString& inText,bool inLineBreak)
{
    const GUIString* aText=&inText;
    this->AddFormattedText(inFormat,&aText,1,NULL,0,inLineBreak);
}
// FUNCTION: WON 0x007b7000
// Appends formatted text with one link argument to the chat output
void ChatCtrl::AddLinkLine(const GUIString& inFormat,TextSegmentBase* inSegment,bool inLineBreak)
{
    TextSegmentBase* aSegment=inSegment;
    this->AddFormattedText(inFormat,NULL,0,&aSegment,1,inLineBreak);
}
// FUNCTION: WON 0x007b7030
// Creates a clickable link for a client's name in the user color
SmartPtr<ChatLink> ChatCtrl::MakeClientLink(LobbyClient* inClient)
{
    return new ChatLink(inClient,this->mChatOutput->mFont.get(),this->mUserColor);
}
// FUNCTION: WON 0x007b98a0
// Disables a clicked link and every link chained to it, recoloring them, and redraws the chat output
void ChatCtrl::DisableLinkChain(ChatLink* inLink,int inColor,int inLinkedColor)
{
    inLink->mColor=inColor;
    inLink->mEnabled=false;
    ChatLink* aLink=inLink->mNext.get();
    while(aLink!=NULL) {
        aLink->mEnabled=false;
        aLink->mColor=inLinkedColor;
        aLink=aLink->mNext.get();
    }
    aLink=inLink->mPrevious;
    while(aLink!=NULL) {
        aLink->mEnabled=false;
        aLink->mColor=inLinkedColor;
        aLink=aLink->mPrevious;
    }
    this->mChatOutput->Invalidate();
}
// The component types passed to SafeGetComponent are the ones the original's RTTI names
// FUNCTION: WON 0x007b6dd0
// Looks up the chat input, Send button and chat output, makes bold and italic fonts, and listens to broadcasts
void ChatCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig,this->mChatInput,"ChatInput",__FILE__,160);
    SafeGetComponent(aConfig,this->mSendButton,"SendButton",__FILE__,161);
    SafeGetComponent(aConfig,this->mChatOutput,"ChatOutput",__FILE__,162);
    this->mBoldFont=this->GetFontMod(this->mChatOutput->mFont.get(),1,0);
    this->mItalicFont=this->GetFontMod(this->mChatOutput->mFont.get(),2,0);
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007b72e0
// Prints where a client is (anonymous, in a game and/or chat room, or not on the server) with links to join
void ChatCtrl::HandleAnnounceClientLocation(AnnounceClientLocationEvent* inEvent)
{
    if(inEvent->mHandled || (this->mRoomSpecFlags&inEvent->mRoomId)!=inEvent->mRoomId) return;
    LobbyClient* initialClient=inEvent->mClient.get(); // borrowed; the original reads it once (0x7b731e)
    if(initialClient==NULL || !this->IsRootVisible()) return;
    inEvent->mHandled=true;
    LobbyServer* aServer=LobbyGetCurrentServer();
    SmartPtr<ChatLink> aClientLink=this->MakeClientLink(inEvent->mClient.get());
    SmartPtr<ChatLink> aRoomLink=new ChatLink(inEvent->mRoom,initialClient->GetGameGroupId(),
        aServer,this->mChatOutput->mLinkFont.get(),this->mGroupColor);
    SmartPtr<ChatLink> aServerLink=new ChatLink(inEvent->mServer,initialClient->GetChatGroupId(),
        aServer,this->mChatOutput->mLinkFont.get(),this->mGroupColor);
    TextSegmentBase* aLinks[3]={aClientLink.get(),aRoomLink.get(),aServerLink.get()};
    GUIString aFormat(GetEmptyGUIStringData());
    bool hasRoom=inEvent->mRoom.length()!=0;
    bool hasServer=inEvent->mServer.length()!=0;
    if(initialClient->IsAnonymous()) aFormat=::ChatCtrl_ClientIsAnonymous_String;
    else if(hasRoom && hasServer) aFormat=::ChatCtrl_ClientInGameAndChat_String;
    else if(hasServer) aFormat=::ChatCtrl_ClientInChat_String;
    else if(hasRoom) aFormat=::ChatCtrl_ClientInGame_String;
    else aFormat=::ChatCtrl_ClientNotOnServer_String;
    this->AddFormattedText(aFormat,NULL,0,aLinks,3,true);
}

// FUNCTION: WON 0x007b7660
// Prints a friend joining or leaving, or where a friend is (chatting, staging, playing, on, offline) with links
void ChatCtrl::HandleAnnounceFriend(AnnounceFriendEvent* inEvent)
{
    if((this->mRoomSpecFlags&inEvent->mLocation)!=inEvent->mLocation || inEvent->mFriend.get()==NULL) return;
    LobbyFriend* aFriend=inEvent->mFriend.get();
    if(inEvent->IsAdd()) {
        SmartPtr<ChatLink> aLink=new ChatLink(aFriend,this->mChatOutput->mFont.get(),this->mUserColor);
        this->AddLinkLine(::ChatCtrl_FriendJoined_String,aLink.get(),true);
        return;
    }
    if(inEvent->IsDelete()) {
        SmartPtr<ChatLink> aLink=new ChatLink(aFriend,this->mChatOutput->mFont.get(),this->mUserColor);
        this->AddLinkLine(::ChatCtrl_FriendLeft_String,aLink.get(),true);
        return;
    }
    if(inEvent->mHandled || !this->IsRootVisible()) return;
    inEvent->mHandled=true;
    IPAddr aAddress=aFriend->GetAddress();
    GUIString aLocation=aFriend->GetLocation();
    bool hasLocation=(aFriend->GetFlags()&3)!=0 && aAddress.IsValid() && aLocation.length()!=0;
    SmartPtr<ChatLink> aFriendLink=new ChatLink(aFriend,this->mChatOutput->mFont.get(),this->mUserColor);
    SmartPtr<ChatLink> aLocationLink;
    if(hasLocation) aLocationLink=new ChatLink(aFriend,aLocation,aAddress,this->mChatOutput->mFont.get(),this->mUserColor);
    TextSegmentBase* aLinks[2]={aFriendLink.get(),aLocationLink.get()};
    GUIString aFormat(GetEmptyGUIStringData());
    if(!hasLocation) aFormat=::ChatCtrl_FriendIsNotOnline_String;
    else if((aFriend->GetFlags()&1)!=0) {
        if((aFriend->GetFlags()&2)!=0) aFormat=::ChatCtrl_FriendIsInGameStaging_String;
        else aFormat=::ChatCtrl_FriendIsChatting_String;
    } else if((aFriend->GetFlags()&2)!=0) aFormat=::ChatCtrl_FriendIsPlaying_String;
    else aFormat=::ChatCtrl_FriendIsOn_String;
    this->AddFormattedText(aFormat,NULL,0,aLinks,2,true);
}

// The switch's jump table (0x7b9b7c, 24 bytes) follows the code
// FUNCTION: WON 0x007b9910
// Acts on a clicked chat link: client or friend menu, join a friend's room, jump to a room, accept or reject
void ChatCtrl::HandleTextLinkActivated(TextLinkActivatedEvent* inEvent)
{
    ChatLink* aLink=dynamic_cast<ChatLink*>(inEvent->mLink.get());
    if(aLink==NULL) return;
    ComponentEvent* anEvent;
    switch(aLink->mKind) {
    case 3:
        anEvent=new ClientActionEvent(18,this->mRoomSpecFlags,aLink->mClient.get());
        break;
    case 4:
        anEvent=new FriendActionEvent(5,aLink->mFriend.get(),this->mRoomSpecFlags);
        break;
    case 5:
        anEvent=new JoinFriendsRoomEvent(aLink->mFriend.get());
        break;
    case 0:
        this->DeliverEvent(new JumpToRoomEvent(aLink->mText,aLink->mGroupId,aLink->mAddress),false);
        return;
    case 1:
        this->DisableLinkChain(aLink,this->mAcceptColor,this->mRejectColor);
        anEvent=new AcceptClientEvent(aLink->mClient.get(),true);
        break;
    case 2:
        this->DisableLinkChain(aLink,this->mAcceptColor,this->mRejectColor);
        anEvent=new AcceptClientEvent(aLink->mClient.get(),false);
        break;
    default: return;
    }
    this->DeliverEvent(anEvent,false);
}

// Each branch's link temporary lives until the line is printed, then is released
// FUNCTION: WON 0x007b81f0
// Prints a teammate joining or leaving the team (team panels only)
void ChatCtrl::HandlePlayerTeamChanged(PlayerTeamChangedEvent* inEvent)
{
    if(!this->IsTeamRoom()) return;
    if(inEvent->IsAdd()) this->AddLinkLine(::ChatCtrl_TeamMemberJoined_String,this->MakeClientLink(inEvent->mClient.get()).get(),true);
    else if(inEvent->IsDelete()) this->AddLinkLine(::ChatCtrl_TeamMemberLeft_String,this->MakeClientLink(inEvent->mClient.get()).get(),true);
}
// The original calls the event's flag tests again at each use (no cached results).
// FUNCTION: WON 0x007b85b0
// Prints a received chat line as normal, team, emote, whisper or warning text with sender and target links;
// a team panel shows only whispers, warnings and team chat
void ChatCtrl::HandleRecvChat(RecvChatEvent* inEvent)
{
    if(inEvent->mSender.get()==NULL || this->mClientId==0xffff) return;
    if(inEvent->IsTeam()) {
        if(((this->mRoomSpecFlags|16)&inEvent->mRoomSpec)!=inEvent->mRoomSpec) return;
    } else {
        if((this->mRoomSpecFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec) return;
        if(this->IsTeamRoom() && !inEvent->IsWhisper() && !inEvent->IsWarning()) return;
    }
    bool crossRoom=inEvent->IsTeam() && !this->IsTeamRoom();
    SmartPtr<ChatLink> aSender=this->MakeClientLink(inEvent->mSender.get());
    SmartPtr<ChatLink> aTarget;
    TextSegmentBase* aLinks[2]={aSender.get(),NULL};
    int count=1;
    if(inEvent->mTarget.get()!=NULL) {
        aLinks[1]=(aTarget=this->MakeClientLink(inEvent->mTarget.get())).get();
        count=2;
    }
    const GUIString* aText=&inEvent->mText;
    GUIString aFormat(GetEmptyGUIStringData());
    if(inEvent->IsEmote()) aFormat=crossRoom?::ChatCtrl_TeamEmote_String : ::ChatCtrl_Emote_String;
    else if(inEvent->IsWhisper() && inEvent->mTarget.get()!=NULL) {
        if(inEvent->mSender->GetClientId()==this->mClientId)
            if(inEvent->mTarget->GetClientId()==this->mClientId) aFormat=::ChatCtrl_YouWhisperToYourself_String;
            else aFormat=::ChatCtrl_YouWhisper_String;
        else aFormat=::ChatCtrl_SomeoneWhispersToYou_String;
    } else if(inEvent->IsWarning() && inEvent->mTarget.get()!=NULL) {
        if(inEvent->mSender->GetClientId()==this->mClientId)
            if(inEvent->mTarget->GetClientId()==this->mClientId) aFormat=::ChatCtrl_YouWarnYourself_String;
            else aFormat=::ChatCtrl_YouWarnSomeone_String;
        else if(inEvent->mTarget->GetClientId()==this->mClientId) aFormat=::ChatCtrl_SomeoneWarnsYou_String;
        else aFormat=::ChatCtrl_SomeoneWarnsSomeone_String;
    } else aFormat=crossRoom?::ChatCtrl_TeamChat_String : ::ChatCtrl_NormalChat_String;
    if(aFormat.length()!=0) this->AddFormattedText(aFormat,&aText,1,aLinks,count,true);
}
// FUNCTION: WON 0x007b88f0
// Records the joined room and our client id, enables Send and prints the entering-room line
void ChatCtrl::HandleJoinedRoom(JoinedRoomEvent* inEvent)
{
    if((this->mRoomSpecFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec)
        return;
    this->mClientId=inEvent->mClientId;
    this->mRoomName=inEvent->mGroupName;
    this->mSendButton->SetEnabled(true);
    this->AddTextLine(::ChatCtrl_EnteringRoom_String,inEvent->mGroupName,true);
    this->mChatOutput->LineBreak(10);
}
// FUNCTION: WON 0x007b89a0
// Forgets our client id, disables Send and, when asked, prints a leaving-room or disconnected line
void ChatCtrl::HandleLeftRoom(LeftRoomEvent* inEvent)
{
    if((this->mRoomSpecFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec)
        return;
    this->mClientId=0xffff;
    this->mSendButton->SetEnabled(false);
    if(!inEvent->mFlag)
        return;
    if(inEvent->mForced) this->AddTextLine(::ChatCtrl_Disconnected_String,this->mRoomName,true);
    else this->AddTextLine(::ChatCtrl_LeavingRoom_String,this->mRoomName,true);
    this->mChatOutput->LineBreak(10);
}
// FUNCTION: WON 0x007b8bd0
// Prints a chat command error in the error color
void ChatCtrl::HandleChatCommandError(ChatCommandErrorEvent* inEvent)
{
    this->mChatOutput->AddSegment(inEvent->mText,this->mErrorColor,true);
}

// FUNCTION: WON 0x007b94f0
// Splits seconds into a count and a unit (0/1 minute(s), 2/3 hour(s), 4/5 day(s), 6 none), rounding up or down
unsigned char LobbySplitDuration(unsigned long inSeconds,bool inRoundUp,unsigned long* outValue)
{
    if(inSeconds==0) { *outValue=0;return 6; }
    if(inRoundUp) {
        if(inSeconds<=60) { *outValue=1;return 0; }
        if(inSeconds<=3540) {
            *outValue=inSeconds/60;
            if(inSeconds%60>0) ++*outValue;
            return 1;
        }
        if(inSeconds<=3600) { *outValue=1;return 2; }
        if(inSeconds<=82800) {
            *outValue=inSeconds/3600;
            if(inSeconds%3600>0) ++*outValue;
            return 3;
        }
        if(inSeconds<=86400) { *outValue=1;return 4; }
        *outValue=inSeconds/86400;
        if(inSeconds%86400>0) ++*outValue;
        return 5;
    }
    if(inSeconds<120) { *outValue=1;return 0; }
    if(inSeconds<3600) { *outValue=inSeconds/60;return 1; }
    if(inSeconds<7200) { *outValue=1;return 2; }
    if(inSeconds<86400) { *outValue=inSeconds/3600;return 3; }
    if(inSeconds<172800) { *outValue=1;return 4; }
    *outValue=inSeconds/86400;
    return 5;
}
// AlertEvent's field names are neutral (inferred): GetValue is the alert kind, mSeverity the seconds
// FUNCTION: WON 0x007b9680
// Prints a server alert, a shutdown countdown or a shutdown-aborted notice, then the alert's comment
void ChatCtrl::HandleAlert(AlertEvent* inEvent)
{
    if((this->mRoomSpecFlags&inEvent->GetKind())!=inEvent->GetKind()) return;
    GUIString aFormat;
    const GUIString& aTextRef=inEvent->GetText();
    const GUIString* aText=&aTextRef;
    switch(inEvent->GetValue()) {
    case 0:
        aFormat=::ChatCtrl_ServerAlert_String;
        break;
    case 1: {
        unsigned long value=0;
        aFormat=::LobbyResource_GetString((LobbyResourceId)(LobbySplitDuration(inEvent->mSeverity,false,&value)+0x86));
        if(value!=0) {
            char text[20];
            sprintf(text,"%lu",value);
            GUIString number(text);
            const GUIString* arg=&number;
            this->AddFormattedText(aFormat,&arg,1,NULL,0,true);
            aFormat=::ChatCtrl_AlertComment_String;
        }
        break;
    }
    default:
        aFormat=::ChatCtrl_ServerShutdownAborted_String;
        this->AddFormattedText(aFormat,NULL,0,NULL,0,true);
        aFormat=::ChatCtrl_AlertComment_String;
        break;
    }
    if(!aTextRef.empty()) this->AddFormattedText(aFormat,&aText,1,NULL,0,true);
}

// The event is marked handled before the parser lookup, so a later failure still consumes it
// FUNCTION: WON 0x007b82f0
// Fills the chat input with a command addressed to a name (prefix, command, quoted name), keeping a typed message
void ChatCtrl::HandleSetChatInput(SetChatInputEvent* inEvent)
{
    if(inEvent->mHandled || (this->mRoomSpecFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec || !this->IsRootVisible()) return;
    inEvent->mHandled=true;
    ChatCommandParser* parser=LobbyGetChatCommandParser();
    if(parser==NULL) return;
    this->mChatInput->RequestFocus();
    GUIString input(GetEmptyGUIStringData());
    input.append(parser->GetPrefix());
    input.append(parser->GetCommandString(inEvent->mCommand));
    input.append(GUIString(" \""));
    input.append(inEvent->mText);
    input.append(GUIString(new GUIStringData("\" ")));
    this->mChatInput->SetText(input,false);
    this->mChatInput->SelectRange(-1,-1);
    if(this->mChatInput->GetText().find(input,0)!=-1) return;
    if(this->mChatInput->GetText()[0]!=parser->GetPrefix())
        input.append(this->mChatInput->GetText());
    this->mChatInput->SetText(input,false);
    this->mChatInput->SelectRange(-1,-1);
}

// The room link's constructor arguments are evaluated after the allocation, inside its cleanup guard
// FUNCTION: WON 0x007b7c60
// Prints a game invitation (or its withdrawal) with links to the client and the game, plus any comment
void ChatCtrl::HandleGroupInvitation(GroupInvitationEvent* inEvent)
{
    if((this->mRoomSpecFlags&inEvent->mKind)!=inEvent->mKind) return;
    SmartPtr<ChatLink> aClient=this->MakeClientLink(inEvent->mClient.get());
    SmartPtr<ChatLink> aRoom=new ChatLink(inEvent->mGameName,inEvent->mGameId,
        LobbyGetCurrentServer(),this->mChatOutput->mLinkFont.get(),this->mGroupColor);
    TextSegmentBase* aLinks[2]={aClient.get(),aRoom.get()};
    const GUIString* aText[2]={&inEvent->mGameName,&inEvent->mComment};
    int id=!inEvent->mFlag?(inEvent->mInvite?0x8d:0x8e):(inEvent->mInvite?0x8f:0x90);
    if(inEvent->mComment.length()!=0) id+=4;
    GUIString aFormat(::LobbyResource_GetString((LobbyResourceId)(id)));
    this->AddFormattedText(aFormat,aText,2,aLinks,2,true);
}
// The Reject link holds the Accept link (mNext, counted); Accept points back without a reference
// FUNCTION: WON 0x007b7ec0
// Prints a client's request to join our game with Accept and Reject links (not in team panels)
void ChatCtrl::HandleGroupJoinAttempt(GroupJoinAttemptEvent* inEvent)
{
    if((this->mRoomSpecFlags&inEvent->mKind)!=inEvent->mKind || this->IsTeamRoom()) return;
    SmartPtr<ChatLink> aClient=this->MakeClientLink(inEvent->mClient.get());
    SmartPtr<ChatLink> aAccept=new ChatLink(1,inEvent->mClient.get(),::ChatCtrl_Accept_String,
        this->mChatOutput->mLinkFont.get(),this->mMessageColor);
    SmartPtr<ChatLink> aDecline=new ChatLink(2,inEvent->mClient.get(),::ChatCtrl_Reject_String,
        this->mChatOutput->mLinkFont.get(),this->mMessageColor);
    aAccept->mPrevious=aDecline.get();
    aDecline->mNext=aAccept.get();
    const GUIString* aText=&inEvent->mComment;
    TextSegmentBase* aLinks[3]={aClient.get(),aAccept.get(),aDecline.get()};
    GUIString aFormat(inEvent->mComment.length()==0?::ChatCtrl_SomeoneAsksToJoin_String : ::ChatCtrl_SomeoneAsksToJoinExpl_String);
    this->AddFormattedText(aFormat,&aText,1,aLinks,3,true);
}

// The switch's 7-entry table (0x7b8ba8..0x7b8bc4) follows the code
// FUNCTION: WON 0x007b8a20
// Prints the parser's error (unknown or ambiguous name, bad command, name required, bad time) or a generic one
void ChatCtrl::ShowParserError(ChatCommandParser* inParser)
{
    GUIString error(GetEmptyGUIStringData());
    switch(inParser->GetError()) {
    case 3:error=LobbyFormatString(::RoutingLogic_ClientNotFound_String,inParser->GetTargetName());break;
    case 4:error=LobbyFormatString(::RoutingLogic_ClientAmbiguous_String,inParser->GetTargetName());break;
    case 1:error=::RoutingLogic_InvalidCommand_String;break;
    case 2:error=::RoutingLogic_RequireName_String;break;
    case 7:error=::RoutingLogic_BadTimeSpec_String;break;
    }
    if(error.length()==0) error=::RoutingLogic_Error_String;
    this->mChatOutput->AddSegment(error,this->mErrorColor,true);
}
// The number string dies before the comment is printed; the format string lives across both
// FUNCTION: WON 0x007b9240
// Prints that we or another client were muted or banned (with duration), unmuted or kicked, plus the comment
void ChatCtrl::HandleClientModerated(ClientModeratedEvent* inEvent)
{
    if((this->mRoomSpecFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec) return;
    GUIString format(GetEmptyGUIStringData());
    SmartPtr<ChatLink> client=this->MakeClientLink(inEvent->mClient.get());
    TextSegmentBase* link=client.get();
    unsigned long value=0;
    if(inEvent->mFlag) {
        switch(inEvent->mModerationKind) {
        case 0:format=::LobbyResource_GetString((LobbyResourceId)(LobbySplitDuration(inEvent->mDuration,true,&value)+0x64));break;
        case 2:format=::LobbyResource_GetString((LobbyResourceId)(LobbySplitDuration(inEvent->mDuration,true,&value)+0x74));break;
        case 3:format=::ChatCtrl_YouWereUnmuted_String;break;
        default:format=::ChatCtrl_YouWereKicked_String;break;
        }
    } else {
        switch(inEvent->mModerationKind) {
        case 0:format=::LobbyResource_GetString((LobbyResourceId)(LobbySplitDuration(inEvent->mDuration,true,&value)+0x6c));break;
        case 2:format=::LobbyResource_GetString((LobbyResourceId)(LobbySplitDuration(inEvent->mDuration,true,&value)+0x7c));break;
        case 3:format=::ChatCtrl_SomeoneWasUnmuted_String;break;
        default:format=::ChatCtrl_SomeoneWasKicked_String;break;
        }
    }
    if(value==0) {
        const GUIString* text=&inEvent->mRoomName;
        this->AddFormattedText(format,&text,1,&link,1,true);
    } else {
        char text[20];
        sprintf(text,"%lu",value);
        GUIString number(text);
        const GUIString* args[2]={&inEvent->mRoomName,&number};
        this->AddFormattedText(format,args,2,&link,1,true);
    }
    const GUIString& aComment=inEvent->mComment;
    if(aComment.length()!=0) {
        const GUIString* text=&aComment;
        format=::ChatCtrl_ModeratorComment_String;
        this->AddFormattedText(format,&text,1,NULL,0,true);
    }
}

// The switch's tables (0x7b920c..0x7b9235) follow the code
// FUNCTION: WON 0x007b8bf0
// Handles the typed line: help, clear, reply, team and ignore lists run locally; anything else goes to the lobby
void ChatCtrl::SendInput()
{
    if(this->mChatInput->GetText().empty() || this->mSendButton->IsDisabled()) return;
    ChatCommandParser* parser=LobbyGetRoomChatParser(this->mRoomSpecFlags);
    if(parser==NULL) return;
    GUIString input=this->mChatInput->GetText();
    this->mChatInput->AddInputHistory(input,true);
    this->mChatInput->Clear(true);
    if(!parser->HandleInput(input)) {
        this->ShowParserError(parser);
        return;
    }
    switch(parser->GetCommand()) {
    case LobbyChatCommand_Help:
        this->AddFormattedText(parser->GetHelp(this->mClientId),NULL,0,NULL,0,true);
        return;
    case LobbyChatCommand_Clear:
        this->mChatOutput->Reset(false);
        return;
    case LobbyChatCommand_Reply:
        if(parser->GetText().length()!=0) break;
        {
            GUIString command;
            command.append(parser->GetPrefix());
            command.append(parser->GetCommandString(LobbyChatCommand_Whisper));
            command.append(GUIString(" \""));
            command.append(parser->GetTargetName());
            command.append(GUIString("\" "));
            this->mChatInput->SetText(command,false);
            this->mChatInput->SelectRange(-1,-1);
        }
        return;
    case LobbyChatCommand_ShowTeam: {
        LobbyClientIterator iterator(parser->GetClients());
        LobbyClient* me=GetMyLobbyClient();
        if(me!=NULL) {
            GUIString names;
            while(iterator.HasMore()) {
                LobbyClient* client=iterator.Next();
                if(me->IsTeammate(client)) {
                    if(names.length()!=0) names.append(GUIString(", "));
                    names.append(client->GetName());
                }
            }
            this->AddTextLine(::ChatCtrl_TeamMembers_String,names,true);
        }
        return;
    }
    case LobbyChatCommand_Ignore:
        if(parser->GetTargetName().length()!=0) break;
        {
            GUIString names;
            for(size_t i=0;i<LobbyGetIgnoredNameCount();++i) {
                if(names.length()!=0) names.append(GUIString(", "));
                names.append(LobbyGetIgnoredNameAt(i));
            }
            this->AddTextLine(::ChatCtrl_IgnoreList_String,names,true);
        }
        return;
    }
    this->Fn2F5380(100012,1);
}


// The switch's tables follow the code, up to 0x7b9e0c
// FUNCTION: WON 0x007b9c80
// Routes Send, Enter, link clicks and lobby events (chat, rooms, friends, alerts, moderation) to the handlers
void ChatCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case ComponentEvent_TextLinkActivated: this->HandleTextLinkActivated(static_cast<TextLinkActivatedEvent*>(inEvent)); return;
    case ComponentEvent_ButtonPressed:
        if(inEvent->mComponent==this->mSendButton.get()) { this->SendInput(); return; }
        break;
    case ComponentEvent_InputReturn:
        if(inEvent->mComponent==this->mChatInput.get()) { this->SendInput(); return; }
        break;
    case LobbyEvent_Alert: this->HandleAlert(static_cast<AlertEvent*>(inEvent)); return;
    case LobbyEvent_AnnounceFriend: this->HandleAnnounceFriend(static_cast<AnnounceFriendEvent*>(inEvent)); return;
    case LobbyEvent_AnnounceClientLocation: this->HandleAnnounceClientLocation(static_cast<AnnounceClientLocationEvent*>(inEvent)); return;
    case LobbyEvent_ClientModerated: this->HandleClientModerated(static_cast<ClientModeratedEvent*>(inEvent)); return;
    case LobbyEvent_ChatCommandError: this->HandleChatCommandError(static_cast<ChatCommandErrorEvent*>(inEvent)); return;
    case LobbyEvent_GroupInvitation: this->HandleGroupInvitation(static_cast<GroupInvitationEvent*>(inEvent)); return;
    case LobbyEvent_GroupJoinAttempt: this->HandleGroupJoinAttempt(static_cast<GroupJoinAttemptEvent*>(inEvent)); return;
    case LobbyEvent_JoinedRoom: this->HandleJoinedRoom(static_cast<JoinedRoomEvent*>(inEvent)); return;
    case LobbyEvent_LeftRoom: this->HandleLeftRoom(static_cast<LeftRoomEvent*>(inEvent)); return;
    case LobbyEvent_PlayerTeamChanged: this->HandlePlayerTeamChanged(static_cast<PlayerTeamChangedEvent*>(inEvent)); return;
    case LobbyEvent_RecvChat: this->HandleRecvChat(static_cast<RecvChatEvent*>(inEvent)); return;
    case LobbyEvent_SetChatInput: this->HandleSetChatInput(static_cast<SetChatInputEvent*>(inEvent)); return;
    case LobbyEvent_SetRoomSpecFlags: this->mRoomSpecFlags|=static_cast<SetRoomSpecFlagsEvent*>(inEvent)->mFlags; return;
    }
    Component::HandleComponentEvent(inEvent);
}

}



namespace WONAPI {
// ChatCtrlConfig is declared in WONLobbyControlConfig.h (0x9c bytes, vtable 0x851c38; its base ContainerConfig
// is 0x98 bytes).
// FUNCTION: WON 0x007b9e10
// Reads a chat panel layout key: GROUPTYPE (CHAT, GAME or TEAM) or a text color; false for an unknown key
bool ChatCtrlConfig::Virtual24(const std::string& inKey)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_CHATCTRLCONFIG_VIRTUAL24_INKEY,inKey.data(),inKey.size());
    if(ContainerConfig::Virtual24(inKey))
        return true;
    if(aKeyIdx==0) {
        GUIString group=this->ReadString(false);
        const int aNameIdx_group=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_CHATCTRLCONFIG_VIRTUAL24_GROUP,group.mData->mText,group.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_group==0) {
            ChatCtrl* aCtrl=this->mChatCtrl; aCtrl->mRoomSpecFlags=1; }
        else if(aNameIdx_group==1) {
            ChatCtrl* aCtrl=this->mChatCtrl; aCtrl->mRoomSpecFlags=2; }
        else if(aNameIdx_group==2) {
            ChatCtrl* aCtrl=this->mChatCtrl; aCtrl->mRoomSpecFlags=0x12; }
        else {
            throw ConfigObjectException("Invalid Group Type: "+(std::string)group);
        }
    }
    else if(aKeyIdx==1)
        ;
    else if(aKeyIdx==2) this->mChatCtrl->mDefaultColor=this->ReadColor();
    else if(aKeyIdx==3) this->mChatCtrl->mErrorColor=this->ReadColor();
    else if(aKeyIdx==4) this->mChatCtrl->mHelpColor=this->ReadColor();
    else if(aKeyIdx==5) this->mChatCtrl->mUserColor=this->ReadColor();
    else if(aKeyIdx==6) this->mChatCtrl->mEmoteColor=this->ReadColor();
    else if(aKeyIdx==7) this->mChatCtrl->mWhisperColor=this->ReadColor();
    else if(aKeyIdx==8) this->mChatCtrl->mWarningColor=this->ReadColor();
    else if(aKeyIdx==9) this->mChatCtrl->mGroupColor=this->ReadColor();
    else if(aKeyIdx==10) this->mChatCtrl->mMessageColor=this->ReadColor();
    else if(aKeyIdx==11) this->mChatCtrl->mAcceptColor=this->ReadColor();
    else if(aKeyIdx==12) this->mChatCtrl->mRejectColor=this->ReadColor();
    else return false;
    return true;
}
// Remembers the configured component as the chat panel (the original's body is at 0x7c8380; no marker)
void ChatCtrlConfig::SetComponent(Component* inComponent)
{
    ContainerConfig::SetComponent(inComponent);
    this->mChatCtrl=static_cast<ChatCtrl*>(inComponent);
}
} //namespace WONAPI

namespace WONAPI {
// FUNCTION: WON 0x007651e0
// Creates the layout reader for a chat panel
ComponentConfig* ChatCtrlConfig::CfgFactory()
{
    return new ChatCtrlConfig;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007b6ca0
// (public: virtual __thiscall WONAPI::ChatCtrl::~ChatCtrl(void)    [??1ChatCtrl@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007b7190
// (public: virtual __thiscall ChatLink::~ChatLink(void)    [??1ChatLink@@UAE@XZ])
// TEMPLATE: WON 0x007b9bc0
// (public: virtual __thiscall WONAPI::JumpToRoomEvent::~JumpToRoomEvent(void)    [??1JumpToRoomEvent@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007cd8a0
// TEMPLATE: WON 0x007cda20

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007b6c80
// (public: virtual void * __thiscall WONAPI::ChatCtrl::`scalar deleting destructor'(unsigned int)    [??_GChatCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007b9ba0
// (public: virtual void * __thiscall WONAPI::JumpToRoomEvent::`scalar deleting destructor'(unsigned int)    [??_GJumpToRoomEvent@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007b7170
// (public: virtual void * __thiscall ChatLink::`scalar deleting destructor'(unsigned int)    [??_GChatLink@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00765240
// (public: virtual void * __thiscall WONAPI::ChatCtrlConfig::`scalar deleting destructor'(unsigned int))
