// ChatCommandParser: turns a line of lobby chat input into a broadcast, an emote or a slash command
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The original's functions run from 0x755750 to 0x757d67; the parser (0x54 bytes, RefCount-derived, vtable
// 0x851a9c) is allocated at 0x762aed. The LobbyCompletion functions follow. (file name inferred: no __FILE__)
// Behaviour of the original kept here: a one-character rest of line reads as empty, a name prefix shared by two
// clients is ambiguous, a duration needs a unit letter (D/H/M/S), and INFINITE keeps the previous duration.
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <cwctype>
#include <cctype>
#include <cstdlib>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "rust/won_lobby/chatobj.h"
#include "rust/won_net/chat.h"
#include <vector>
extern wchar_t gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss)
using namespace WONAPI;
// The parsing (the command word, targets by full name or unique prefix, texts, durations, the input's kind, the help
// lines a client is shown, the short names) is ee_won's chat module (rust/crates/ee_won_net/src/chat.rs); the parser's
// state (the command tables, the settings, what a parse leaves) is a Rust object since round 17 (chatobj.rs). This
// class keeps the client list and the target client and hands the clients' names over.
namespace {
EeWonChatText ChatText(const GUIString& inText)
{
    EeWonChatText aText;
    aText.text = inText.mData->mText;
    aText.length = inText.length();
    return aText;
}
GUIString ChatString(const uint32_t* inUnits, size_t inCount)
{
    std::wstring aText;
    aText.reserve(inCount);
    for (size_t i = 0; i < inCount; i++)
        aText += (wchar_t)inUnits[i];
    return GUIString(aText);
}
void ChatWordSink(void* inContext, const uint32_t* inUnits, size_t inCount)
{
    *static_cast<GUIString*>(inContext) = ChatString(inUnits, inCount);
}
// The texts a parse set
struct ChatTexts {
    bool mHasTargetName, mHasText;
    GUIString mTargetName, mText;
    ChatTexts() : mHasTargetName(false), mHasText(false) {}
};
void ChatTextSink(void* inContext, int32_t inWhich, const uint32_t* inUnits, size_t inCount)
{
    ChatTexts* theTexts = static_cast<ChatTexts*>(inContext);
    if (inWhich == EE_WON_CHAT_TARGET_NAME) {
        theTexts->mHasTargetName = true;
        theTexts->mTargetName = ChatString(inUnits, inCount);
    } else {
        theTexts->mHasText = true;
        theTexts->mText = ChatString(inUnits, inCount);
    }
}
// The room's clients and their names, in the list's order
struct ChatClients {
    std::vector<LobbyClient*> mClients;
    std::vector<EeWonChatText> mNames;
    ChatClients(LobbyClientList* inList)
    {
        if (inList == NULL)
            return;
        for (size_t i = 0; i < inList->GetCount(); ++i) {
            this->mClients.push_back(inList->ClientAt(i));
            this->mNames.push_back(ChatText(inList->ClientAt(i)->GetName()));
        }
    }
    const EeWonChatText* Names() const { return this->mNames.empty() ? NULL : &this->mNames[0]; }
};
}
// Creates the chat command parser: '/' prefix, unique name prefixes accepted, colon emotes off, no room flags
ChatCommandParser::ChatCommandParser() : mParser(ee_won_chatparser_new()) {}

// Destroys the chat command parser (members only)
ChatCommandParser::~ChatCommandParser() { ee_won_chatparser_free(this->mParser); }

GUIString ChatCommandParser::Text(EeWonChatParserText inText) const
{
 size_t n=0;
 const uint32_t* u=ee_won_chatparser_text(this->mParser,inText,&n);
 return ChatString(u,n);
}
static int64_t ChatNumber(const EeWonChatParser* p,EeWonChatParserNumber n) { return ee_won_chatparser_number(p,n); }
int ChatCommandParser::GetCommand() const { return (int)ChatNumber(this->mParser,EeWonChatParserNumber::Command); }
GUIString ChatCommandParser::GetTargetName() const { return this->Text(EeWonChatParserText::TargetName); }
int ChatCommandParser::GetPrefix() const { return (int)ChatNumber(this->mParser,EeWonChatParserNumber::Prefix); }
GUIString ChatCommandParser::GetText() const { return this->Text(EeWonChatParserText::Text); }
unsigned long ChatCommandParser::GetDuration() const
{ return (unsigned long)ChatNumber(this->mParser,EeWonChatParserNumber::Duration); }
unsigned long ChatCommandParser::GetRoomFlags() const
{ return (unsigned long)ChatNumber(this->mParser,EeWonChatParserNumber::RoomFlags); }
int ChatCommandParser::GetError() const { return (int)ChatNumber(this->mParser,EeWonChatParserNumber::Error); }
bool ChatCommandParser::GetInvitation() const { return ChatNumber(this->mParser,EeWonChatParserNumber::Invitation)!=0; }
void ChatCommandParser::SetCommand(int inCommand)
{ ee_won_chatparser_set_number(this->mParser,EeWonChatParserNumber::Command,inCommand); }
void ChatCommandParser::SetInput(const GUIString& inInput)
{ ee_won_chatparser_set_text(this->mParser,EeWonChatParserText::Input,inInput.mData->mText,inInput.length(),sizeof(wchar_t)); }
void ChatCommandParser::SetPrefix(int inPrefix)
{ ee_won_chatparser_set_number(this->mParser,EeWonChatParserNumber::Prefix,inPrefix); }
void ChatCommandParser::SetMatchThreshold(int inThreshold)
{ ee_won_chatparser_set_number(this->mParser,EeWonChatParserNumber::MatchThreshold,inThreshold); }
void ChatCommandParser::SetAllowColonEmote(bool inAllow)
{ ee_won_chatparser_set_number(this->mParser,EeWonChatParserNumber::AllowColonEmote,inAllow?1:0); }
void ChatCommandParser::SetText(const GUIString& inText)
{ ee_won_chatparser_set_text(this->mParser,EeWonChatParserText::Text,inText.mData->mText,inText.length(),sizeof(wchar_t)); }
void ChatCommandParser::SetInvitation(bool inInvitation)
{ ee_won_chatparser_set_number(this->mParser,EeWonChatParserNumber::Invitation,inInvitation?1:0); }

// Registers the localized names of every chat command (whisper, block, emote, ignore, ... alert, warn)
void ChatCommandParser::InitCommands()
{
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Whisper_String),LobbyChatCommand_Whisper);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Block_String),LobbyChatCommand_Block);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Emote_String),LobbyChatCommand_Emote);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Ignore_String),LobbyChatCommand_Ignore);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Away_String),LobbyChatCommand_Away);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Invite_String),LobbyChatCommand_Invite);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Uninvite_String),LobbyChatCommand_Uninvite);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Reply_String),LobbyChatCommand_Reply);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Clear_String),LobbyChatCommand_Clear);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Help_String),LobbyChatCommand_Help);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_ShowTeam_String),LobbyChatCommand_ShowTeam);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Moderator_String),LobbyChatCommand_Moderator);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_ServerMute_String),LobbyChatCommand_ServerMute);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_ServerBan_String),LobbyChatCommand_ServerBan);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Mute_String),LobbyChatCommand_Mute);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Ban_String),LobbyChatCommand_Ban);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Unmute_String),LobbyChatCommand_Unmute);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_ServerUnmute_String),LobbyChatCommand_ServerUnmute);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Unban_String),LobbyChatCommand_Unban);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_ServerUnban_String),LobbyChatCommand_ServerUnban);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Alert_String),LobbyChatCommand_Alert);
 this->AddCommands(gEmptyWideString,LobbyChatCommand_StartShutdown);
 this->AddCommands(gEmptyWideString,LobbyChatCommand_AbortShutdown);
 this->AddCommands(LOBBY_STRING(ChatCommandLogic_Warn_String),LobbyChatCommand_Warn);
}

// Sets the room's client list (used to resolve names) and the room flags (used for the help text)
void ChatCommandParser::SetClientList(LobbyClientList* clients,int roomFlags)
{
 this->mClients=clients;
 ee_won_chatparser_set_number(this->mParser,EeWonChatParserNumber::RoomFlags,(unsigned long)(long)roomFlags);
}

// Maps a command name to its command id; the first name added for an id becomes its display name
void ChatCommandParser::AddCommand(const GUIString& text,int command)
{
 ee_won_chatparser_add_command(this->mParser,text.mData->mText,text.length(),sizeof(wchar_t),command);
}

// Returns a command id's display name, else any name registered for it, else an empty string
GUIString ChatCommandParser::GetCommandString(int command)
{
 size_t n=0;
 const uint32_t* u=ee_won_chatparser_command_string(this->mParser,command,&n);
 return n!=0 ? ChatString(u,n) : GUIString("");
}

// Returns the built-in short name of a command id ('w', 'em', 'afk', ...); empty for an unknown id
const char* ChatCommandParser::GetCommandName(int command)
{
 return ee_won_chat_command_name(command);
}

// Registers every name in a list of command names; with no names, registers the built-in short name
void ChatCommandParser::AddCommands(const std::wstring& names,int command)
{
 StringParser parser; parser.Set(names.c_str());
 std::list<std::wstring> tokens;
 parser.ReadValue(tokens);
 if(names.empty() || tokens.empty()) {
  this->AddCommand(GUIString(this->GetCommandName(command)),command);
  return;
 } else {
  for(std::list<std::wstring>::iterator i=tokens.begin();i!=tokens.end();++i)
   this->AddCommand(GUIString(*i),command);
  return;
 }
}

// The original's CompareNames 0x756950, SkipWhitespace 0x7569f0, GetToken 0x756a40 and GetRestOfLine 0x756c90 are
// ee_won's chat module

// Finds a room client by full name, else by a unique name prefix; reports none, ambiguous, prefix or full match
LobbyClient* ChatCommandParser::FindClient(const GUIString& name,int* outMatch)
{
 ChatClients clients(this->mClients.get());
 int match=0;
 int index=ee_won_chat_find_client(ChatText(name),clients.Names(),clients.mNames.size(),sizeof(wchar_t),&match);
 if(outMatch) *outMatch=match;
 return index>=0 ? clients.mClients[index] : NULL;
}

// Parses a line of chat input as a slash command, a colon emote or a broadcast; false with an error code if bad
// (chatobj.rs handle_input; the target is the client at the position it returns)
bool ChatCommandParser::HandleInput(const GUIString& text)
{
 this->mTarget=NULL;
 ChatClients clients(this->mClients.get());
 int target=-1;
 bool ok=ee_won_chatparser_handle_input(this->mParser,ChatText(text),clients.Names(),clients.mNames.size(),
                                        this->mClients.get()!=NULL,sizeof(wchar_t),&target);
 if(target>=0) this->mTarget=clients.mClients[target];
 return ok;
}

// Appends one help line and a line break to the help text
void ChatCommandParser::AddHelpEntry(int command,const GUIString& entry,GUIString& out)
{
 out.append(entry);
 out.append(GUIString("<n>"));
}

// A command's help line (the resource's)
static const GUIString& ChatHelpString(int command)
{
 switch(command) {
 case LobbyChatCommand_Help: return LOBBY_STRING(ChatCommandLogic_HelpHelp_String);
 case LobbyChatCommand_Whisper: return LOBBY_STRING(ChatCommandLogic_WhisperHelp_String);
 case LobbyChatCommand_Reply: return LOBBY_STRING(ChatCommandLogic_ReplyHelp_String);
 case LobbyChatCommand_Emote: return LOBBY_STRING(ChatCommandLogic_EmoteHelp_String);
 case LobbyChatCommand_Ignore: return LOBBY_STRING(ChatCommandLogic_IgnoreHelp_String);
 case LobbyChatCommand_Clear: return LOBBY_STRING(ChatCommandLogic_ClearHelp_String);
 case LobbyChatCommand_ShowTeam: return LOBBY_STRING(ChatCommandLogic_ShowTeamHelp_String);
 case LobbyChatCommand_Block: return LOBBY_STRING(ChatCommandLogic_BlockHelp_String);
 case LobbyChatCommand_Away: return LOBBY_STRING(ChatCommandLogic_AwayHelp_String);
 case LobbyChatCommand_Invite: return LOBBY_STRING(ChatCommandLogic_InviteHelp_String);
 case LobbyChatCommand_Uninvite: return LOBBY_STRING(ChatCommandLogic_UninviteHelp_String);
 case LobbyChatCommand_Mute: return LOBBY_STRING(ChatCommandLogic_MuteHelp_String);
 case LobbyChatCommand_Unmute: return LOBBY_STRING(ChatCommandLogic_UnmuteHelp_String);
 case LobbyChatCommand_Ban: return LOBBY_STRING(ChatCommandLogic_BanHelp_String);
 case LobbyChatCommand_Unban: return LOBBY_STRING(ChatCommandLogic_UnbanHelp_String);
 case LobbyChatCommand_Moderator: return LOBBY_STRING(ChatCommandLogic_ModeratorHelp_String);
 case LobbyChatCommand_ServerMute: return LOBBY_STRING(ChatCommandLogic_ServerMuteHelp_String);
 case LobbyChatCommand_ServerUnmute: return LOBBY_STRING(ChatCommandLogic_ServerUnmuteHelp_String);
 case LobbyChatCommand_ServerBan: return LOBBY_STRING(ChatCommandLogic_ServerBanHelp_String);
 case LobbyChatCommand_ServerUnban: return LOBBY_STRING(ChatCommandLogic_ServerUnbanHelp_String);
 case LobbyChatCommand_Alert: return LOBBY_STRING(ChatCommandLogic_AlertHelp_String);
 default: return LOBBY_STRING(ChatCommandLogic_WarnHelp_String);
 }
}

// Builds a client's chat help; outside LAN adds block/away, invite if game captain, mute/ban if captain of this
// room or a moderator, server mute/ban and warn if a moderator, alert if an admin; team help in game rooms
GUIString ChatCommandParser::GetHelp(unsigned short clientId)
{
 bool isAdmin=false,isModerator=false,isRoomCaptain=false,isGameCaptain=false;
 if(this->mClients.get()!=NULL) {
  LobbyClient* client=this->mClients->FindClient(clientId);
  if(client!=NULL) {
   isModerator=client->IsModerator();
   isRoomCaptain=client->IsCaptain((this->GetRoomFlags()>>1)&1);
   isGameCaptain=client->IsCaptain(1);
   isAdmin=client->IsAdmin();
  }
 }
 GUIString out=LOBBY_STRING(ChatCommandLogic_HelpHeader_String);
 out.append(GUIString("<n>"));
 int32_t commands[32];
 size_t count=ee_won_chat_help((uint32_t)this->GetRoomFlags(),isModerator,isRoomCaptain,isGameCaptain,isAdmin,commands);
 for(size_t k=0;k<count;k++) this->AddHelpEntry(commands[k],ChatHelpString(commands[k]),out);
 return out;
}
