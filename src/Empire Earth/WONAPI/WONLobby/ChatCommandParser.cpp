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
#include "rust/ee_rust.h"
#include <vector>
extern wchar_t gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss)
using namespace WONAPI;
// The parsing (the command word, targets by full name or unique prefix, texts, durations, the input's kind, the help
// lines a client is shown, the short names) is ee_won's chat module (rust/crates/ee_won/src/chat.rs); the parser's
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
            this->mNames.push_back(ChatText(inList->ClientAt(i)->mName));
        }
    }
    const EeWonChatText* Names() const { return this->mNames.empty() ? NULL : &this->mNames[0]; }
};
}
// FUNCTION: WON 0x755750
// Creates the chat command parser: '/' prefix, unique name prefixes accepted, colon emotes off, no room flags
ChatCommandParser::ChatCommandParser() : mParser(ee_won_chatparser_new()) {}

// FUNCTION: WON 0x7559f0
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

// FUNCTION: WON 0x755b50
// Registers the localized names of every chat command (whisper, block, emote, ignore, ... alert, warn)
void ChatCommandParser::InitCommands()
{
 this->AddCommands(::ChatCommandLogic_Whisper_String,LobbyChatCommand_Whisper);
 this->AddCommands(::ChatCommandLogic_Block_String,LobbyChatCommand_Block);
 this->AddCommands(::ChatCommandLogic_Emote_String,LobbyChatCommand_Emote);
 this->AddCommands(::ChatCommandLogic_Ignore_String,LobbyChatCommand_Ignore);
 this->AddCommands(::ChatCommandLogic_Away_String,LobbyChatCommand_Away);
 this->AddCommands(::ChatCommandLogic_Invite_String,LobbyChatCommand_Invite);
 this->AddCommands(::ChatCommandLogic_Uninvite_String,LobbyChatCommand_Uninvite);
 this->AddCommands(::ChatCommandLogic_Reply_String,LobbyChatCommand_Reply);
 this->AddCommands(::ChatCommandLogic_Clear_String,LobbyChatCommand_Clear);
 this->AddCommands(::ChatCommandLogic_Help_String,LobbyChatCommand_Help);
 this->AddCommands(::ChatCommandLogic_ShowTeam_String,LobbyChatCommand_ShowTeam);
 this->AddCommands(::ChatCommandLogic_Moderator_String,LobbyChatCommand_Moderator);
 this->AddCommands(::ChatCommandLogic_ServerMute_String,LobbyChatCommand_ServerMute);
 this->AddCommands(::ChatCommandLogic_ServerBan_String,LobbyChatCommand_ServerBan);
 this->AddCommands(::ChatCommandLogic_Mute_String,LobbyChatCommand_Mute);
 this->AddCommands(::ChatCommandLogic_Ban_String,LobbyChatCommand_Ban);
 this->AddCommands(::ChatCommandLogic_Unmute_String,LobbyChatCommand_Unmute);
 this->AddCommands(::ChatCommandLogic_ServerUnmute_String,LobbyChatCommand_ServerUnmute);
 this->AddCommands(::ChatCommandLogic_Unban_String,LobbyChatCommand_Unban);
 this->AddCommands(::ChatCommandLogic_ServerUnban_String,LobbyChatCommand_ServerUnban);
 this->AddCommands(::ChatCommandLogic_Alert_String,LobbyChatCommand_Alert);
 this->AddCommands(gEmptyWideString,LobbyChatCommand_StartShutdown);
 this->AddCommands(gEmptyWideString,LobbyChatCommand_AbortShutdown);
 this->AddCommands(::ChatCommandLogic_Warn_String,LobbyChatCommand_Warn);
}

// FUNCTION: WON 0x7561b0
// Sets the room's client list (used to resolve names) and the room flags (used for the help text)
void ChatCommandParser::SetClientList(LobbyClientList* clients,int roomFlags)
{
 this->mClients=clients;
 ee_won_chatparser_set_number(this->mParser,EeWonChatParserNumber::RoomFlags,(unsigned long)(long)roomFlags);
}

// FUNCTION: WON 0x756220
// Maps a command name to its command id; the first name added for an id becomes its display name
void ChatCommandParser::AddCommand(const GUIString& text,int command)
{
 ee_won_chatparser_add_command(this->mParser,text.mData->mText,text.length(),sizeof(wchar_t),command);
}

// FUNCTION: WON 0x7564b0
// Returns a command id's display name, else any name registered for it, else an empty string
GUIString ChatCommandParser::GetCommandString(int command)
{
 size_t n=0;
 const uint32_t* u=ee_won_chatparser_command_string(this->mParser,command,&n);
 return n!=0 ? ChatString(u,n) : GUIString("");
}

// FUNCTION: WON 0x756620
// Returns the built-in short name of a command id ('w', 'em', 'afk', ...); empty for an unknown id
const char* ChatCommandParser::GetCommandName(int command)
{
 return ee_won_chat_command_name(command);
}

// FUNCTION: WON 0x756760
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

// FUNCTION: WON 0x756e90
// Finds a room client by full name, else by a unique name prefix; reports none, ambiguous, prefix or full match
LobbyClient* ChatCommandParser::FindClient(const GUIString& name,int* outMatch)
{
 ChatClients clients(this->mClients.get());
 int match=0;
 int index=ee_won_chat_find_client(ChatText(name),clients.Names(),clients.mNames.size(),sizeof(wchar_t),&match);
 if(outMatch) *outMatch=match;
 return index>=0 ? clients.mClients[index] : NULL;
}

// FUNCTION: WON 0x756f40 (ParseCommand: a slash command looked up, its target, text or duration read) and
// FUNCTION: WON 0x757630
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

// FUNCTION: WON 0x757800
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
 case LobbyChatCommand_Help: return ::ChatCommandLogic_HelpHelp_String;
 case LobbyChatCommand_Whisper: return ::ChatCommandLogic_WhisperHelp_String;
 case LobbyChatCommand_Reply: return ::ChatCommandLogic_ReplyHelp_String;
 case LobbyChatCommand_Emote: return ::ChatCommandLogic_EmoteHelp_String;
 case LobbyChatCommand_Ignore: return ::ChatCommandLogic_IgnoreHelp_String;
 case LobbyChatCommand_Clear: return ::ChatCommandLogic_ClearHelp_String;
 case LobbyChatCommand_ShowTeam: return ::ChatCommandLogic_ShowTeamHelp_String;
 case LobbyChatCommand_Block: return ::ChatCommandLogic_BlockHelp_String;
 case LobbyChatCommand_Away: return ::ChatCommandLogic_AwayHelp_String;
 case LobbyChatCommand_Invite: return ::ChatCommandLogic_InviteHelp_String;
 case LobbyChatCommand_Uninvite: return ::ChatCommandLogic_UninviteHelp_String;
 case LobbyChatCommand_Mute: return ::ChatCommandLogic_MuteHelp_String;
 case LobbyChatCommand_Unmute: return ::ChatCommandLogic_UnmuteHelp_String;
 case LobbyChatCommand_Ban: return ::ChatCommandLogic_BanHelp_String;
 case LobbyChatCommand_Unban: return ::ChatCommandLogic_UnbanHelp_String;
 case LobbyChatCommand_Moderator: return ::ChatCommandLogic_ModeratorHelp_String;
 case LobbyChatCommand_ServerMute: return ::ChatCommandLogic_ServerMuteHelp_String;
 case LobbyChatCommand_ServerUnmute: return ::ChatCommandLogic_ServerUnmuteHelp_String;
 case LobbyChatCommand_ServerBan: return ::ChatCommandLogic_ServerBanHelp_String;
 case LobbyChatCommand_ServerUnban: return ::ChatCommandLogic_ServerUnbanHelp_String;
 case LobbyChatCommand_Alert: return ::ChatCommandLogic_AlertHelp_String;
 default: return ::ChatCommandLogic_WarnHelp_String;
 }
}

// FUNCTION: WON 0x7579c0
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
 GUIString out=::ChatCommandLogic_HelpHeader_String;
 out.append(GUIString("<n>"));
 int32_t commands[32];
 size_t count=ee_won_chat_help((uint32_t)this->GetRoomFlags(),isModerator,isRoomCaptain,isGameCaptain,isAdmin,commands);
 for(size_t k=0;k<count;k++) this->AddHelpEntry(commands[k],ChatHelpString(commands[k]),out);
 return out;
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aa250
// (std::list<std::wstring>::~list)
// TEMPLATE: WON 0x00755850
// (public: virtual void * __thiscall WONAPI::ChatCommandParser::`scalar deleting destructor'(unsigned int)    [??_GChatCommandParser@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00756190
// (public: __thiscall WONAPI::GUIString::operator class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<uns)
// TEMPLATE: WON 0x00756470
// (public: __thiscall std::pair<int const ,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned s)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00755870
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,int,)
// TEMPLATE: WON 0x00755930
// (public: __thiscall std::map<int,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00768ab0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x00768de0
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x00768e90
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x00769040
// (public: class std::_Tree<int,struct std::pair<int const ,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::all)
// TEMPLATE: WON 0x007690f0
// (protected: class std::_Tree<int,struct std::pair<int const ,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::)
// TEMPLATE: WON 0x0076b7f0
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x0076b830
// (protected: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,stru)
// TEMPLATE: WON 0x0076c010
// (protected: void __thiscall std::_Tree<int,struct std::pair<int const ,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,c)
// TEMPLATE: WON 0x0076c050
// (protected: struct std::_Tree<int,struct std::pair<int const ,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std:)
// TEMPLATE: WON 0x00770010
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)
// TEMPLATE: WON 0x00770030
// (public: void * __thiscall std::pair<int const ,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<uns)
// TEMPLATE: WON 0x0076b310
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x0076bb30
// (public: class std::_Tree<int,struct std::pair<int const ,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::all)
// TEMPLATE: WON 0x0076f890
// (void __cdecl std::_Construct(struct std::pair<int const ,class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::all)
// TEMPLATE: WON 0x0076fc30
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsi)
// TEMPLATE: WON 0x00768ce0
// (public: struct std::pair<class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned)
// TEMPLATE: WON 0x00768ee0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x007694f0
// (public: class std::list<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,class st)
