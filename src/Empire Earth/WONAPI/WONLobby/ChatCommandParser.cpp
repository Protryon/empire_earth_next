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
extern unsigned short gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss)
using namespace WONAPI;
namespace WONAPI {
}
// FUNCTION: WON 0x755750
// Creates the chat command parser: '/' prefix, unique name prefixes accepted, colon emotes off, no room flags
ChatCommandParser::ChatCommandParser()
{
 this->mB2C=false;
 this->mAllowColonEmote=false;
 this->mMatchThreshold=2;
 this->mDuration=0;
 this->mPrefix='/';
 this->mRoomFlags=0;
}

// FUNCTION: WON 0x7559f0
// Destroys the chat command parser (members only)
ChatCommandParser::~ChatCommandParser() {}

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
 this->mRoomFlags=roomFlags;
}

// FUNCTION: WON 0x756220
// Maps a command name to its command id; the first name added for an id becomes its display name
void ChatCommandParser::AddCommand(const GUIString& text,int command)
{
 if(text.empty()) return;
 GUIString key=text;
 this->mCommands[key]=command;
 std::map<int,std::wstring>::iterator i=this->mCommandStrings.find(command);
 if(i==this->mCommandStrings.end())
  this->mCommandStrings.insert(std::map<int,std::wstring>::value_type(command,text));
}

// FUNCTION: WON 0x7564b0
// Returns a command id's display name, else any name registered for it, else an empty string
GUIString ChatCommandParser::GetCommandString(int command)
{
 std::map<int,std::wstring>::iterator i=this->mCommandStrings.find(command);
 if(i!=this->mCommandStrings.end()) return GUIString(i->second);
 ChatCommandParser::CommandMap::iterator j=this->mCommands.begin();
 while(true) {
  if(j==this->mCommands.end()) break;
  if(j->second==command) return GUIString(j->first);
  ++j;
 }
 return GUIString("");
}

// FUNCTION: WON 0x756620
// Returns the built-in short name of a command id ('w', 'em', 'afk', ...); empty for an unknown id
const char* ChatCommandParser::GetCommandName(int command)
{
 switch(command) {
 case LobbyChatCommand_Whisper:return "w";case LobbyChatCommand_Emote:return "em";case LobbyChatCommand_Block:return "b";case LobbyChatCommand_Ignore:return "i";
 case LobbyChatCommand_Away:return "afk";case LobbyChatCommand_Moderator:return "mod";case LobbyChatCommand_ServerMute:return "smute";case LobbyChatCommand_ServerBan:return "sban";
 case LobbyChatCommand_Mute:return "mute";case LobbyChatCommand_Ban:return "ban";case LobbyChatCommand_Unmute:return "unmute";case LobbyChatCommand_ServerUnmute:return "sunmute";
 case LobbyChatCommand_Unban:return "unban";case LobbyChatCommand_ServerUnban:return "sunban";case LobbyChatCommand_Invite:return "invite";case LobbyChatCommand_Uninvite:return "uninvite";
 case LobbyChatCommand_Reply:return "r";case LobbyChatCommand_Clear:return "cls";case LobbyChatCommand_Help:return "?";case LobbyChatCommand_ShowTeam:return "team";
 case LobbyChatCommand_Alert:return "alert";case LobbyChatCommand_StartShutdown:return "startshutdown";case LobbyChatCommand_AbortShutdown:return "abortshutdown";case LobbyChatCommand_Warn:return "warn";
 default:return "";
 }
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

// FUNCTION: WON 0x756950
// Compares two names ignoring case: true when equal; also reports whether the first is a prefix of the second
static bool CompareNames(const GUIString& a,const GUIString& b,bool& prefix)
{
 prefix=false;
 int na=a.length(),nb=b.length();
 for(int i=0;i<na && i<nb;i++) {
  if(towupper(a[i])!=towupper(b[i])) return false;
 }
 if(na<=nb) prefix=true;
 return na==nb;
}

// FUNCTION: WON 0x7569f0
// Advances a position past whitespace in a string
static void SkipWhitespace(const GUIString& text,int& pos)
{
 while(pos<text.length() && iswctype(text.at(pos),_SPACE)) ++pos;
}

// FUNCTION: WON 0x756a40
// Reads the next space-delimited word (or quoted string), optionally upper-cased, and advances the position
static GUIString GetToken(const GUIString& text,int& pos,bool upper,bool quotes)
{
 SkipWhitespace(text,pos);
 if(pos>=text.length()) return GUIString("");
 GUIString token;
 if(quotes && text[pos]=='"') {
  int end=text.find('"',pos+1);
  if(end>=0) {
   token=text.substr(pos+1,end-pos-1);
   pos=end+1;
   if(upper) token.toUpper();
   return token;
  }
 }
 while(pos<text.length()) {
  wchar_t ch=text[pos];
  if(upper) ch=towupper(ch);
  if(ch==' ') return token;
  token.append(ch);
  ++pos;
 }
 return token;
}

// FUNCTION: WON 0x756c90
// Returns the rest of the line (or a quoted string) without trailing spaces; a single character returns empty
static GUIString GetRestOfLine(const GUIString& text,int pos)
{
 SkipWhitespace(text,pos);
 if(pos>=text.length()) return GUIString("");
 GUIString result;
 if(text[pos]=='"') {
  int end=text.find('"',pos+1);
  if(end>=0) { result=text.substr(pos+1,end-pos-1); return result.mData.get(); }
 }
 int end=text.length()-1;
 while(end>pos && text[end]==' ') --end;
 if(end>pos) return text.substr(pos,end-pos+1);
 else return GUIString("");
}

// FUNCTION: WON 0x756e90
// Finds a room client by full name, else by a unique name prefix; reports none, ambiguous, prefix or full match
LobbyClient* ChatCommandParser::FindClient(const GUIString& name,int* outMatch)
{
 LobbyClientMap::iterator i;
 int match=0;
 LobbyClient* found=NULL;
 LobbyClientList* clients=this->mClients.get();
 if(clients!=NULL) {
  for(i=clients->mClients.begin();i!=clients->mClients.end();++i) {
   LobbyClient* client=i->second.get();
   bool prefix=false;
   if(CompareNames(name,client->mName,prefix)) {
    if(match==3) { match=1;found=NULL;break; }
    match=3;found=client;
   } else if(prefix) {
    if(match==0) { match=2;found=i->second.get(); }
    else if(match==2) { match=1;found=NULL;break; }
   }
  }
 }
 if(match==0 || match==1) found=NULL;
 if(outMatch) *outMatch=match;
 return found;
}

// FUNCTION: WON 0x756f40
// Parses a slash command: looks it up, resolves its target client, reads its text or duration; sets an error code
bool ChatCommandParser::ParseCommand(const GUIString& text)
{
 int pos=1;
 GUIString command=GetToken(text,pos,true,false);
 ChatCommandParser::CommandMap::iterator i=this->mCommands.find(command);
 if(i==this->mCommands.end()) { this->mCommand=LobbyChatCommand_None;this->mError=1;return false; }
 this->mCommand=i->second;
 bool needTarget=false;
 switch(this->mCommand) {
 case LobbyChatCommand_Reply: this->mTargetName=this->mInput; needTarget=true; break;
 case LobbyChatCommand_Ignore:
  this->mTargetName=GetRestOfLine(text,pos);
  needTarget=!this->mTargetName.empty();
  break;
 case LobbyChatCommand_Block: this->mTargetName=GetRestOfLine(text,pos); needTarget=true; break;
 case LobbyChatCommand_Unmute:
 case LobbyChatCommand_ServerUnmute:
  this->mTargetName=GetRestOfLine(text,pos);
  if(this->mTargetName.length()!=0) needTarget=true;
  break;
 case LobbyChatCommand_Whisper:case LobbyChatCommand_ServerMute:case LobbyChatCommand_ServerBan:case LobbyChatCommand_Mute:case LobbyChatCommand_Ban:case LobbyChatCommand_Invite:case LobbyChatCommand_Uninvite:case LobbyChatCommand_Warn:
  this->mTargetName=GetToken(text,pos,false,true);
  needTarget=true;
  break;
 }
 if(needTarget) {
  if(this->mTargetName.length()==0) { this->mError=2;return false; }
  int match;
  this->mTarget=this->FindClient(this->mTargetName,&match);
  if(match<this->mMatchThreshold) {
   if(match==1) this->mError=4;
   else this->mError=3;
   return false;
  }
 }
 SkipWhitespace(text,pos);
 switch(this->mCommand) {
 case LobbyChatCommand_Whisper:case LobbyChatCommand_Emote:case LobbyChatCommand_Alert:case LobbyChatCommand_Warn:
  if(pos>=text.length()) { this->mError=5;return false; }
  this->mText=text.substr(pos);
  return true;
 case LobbyChatCommand_Reply:
  this->mText=text.substr(pos);
  if(this->mText.length()!=0) this->mCommand=LobbyChatCommand_Whisper;
  return true;
 case LobbyChatCommand_Invite:case LobbyChatCommand_Uninvite:
  this->mText=text.substr(pos);
  this->mInvitation=this->mCommand==LobbyChatCommand_Invite;
  return true;
 case LobbyChatCommand_Ignore:case LobbyChatCommand_Away:case LobbyChatCommand_Moderator:case LobbyChatCommand_Help:case LobbyChatCommand_AbortShutdown:
  return true;
 case LobbyChatCommand_ServerMute:case LobbyChatCommand_ServerBan:case LobbyChatCommand_Mute:case LobbyChatCommand_Ban:case LobbyChatCommand_StartShutdown:
  {
   std::string duration=GetToken(text,pos,true,false);
   if(!(duration=="INFINITE") && duration.length()>0) {
    char unit=duration[duration.length()-1];
    if(isalpha(unit)) duration=duration.substr(0,duration.length()-1);
    int value=atoi(duration.c_str());
    switch(unit) {
    case 'D': value*=24;
    case 'H': value*=60;
    case 'M': value*=60;
    case 'S': break;
    default: value=0;
    }
    if(value==0) { this->mError=7;return false; }
    this->mDuration=value;
   }
   this->mText=GetRestOfLine(text,pos);
  }
  return true;
 case LobbyChatCommand_Unmute:case LobbyChatCommand_ServerUnmute: this->mText=GetRestOfLine(text,pos); return true;
 }
 return true;
}

// FUNCTION: WON 0x757630
// Parses a line of chat input as a slash command, a colon emote or a broadcast; false with an error code if bad
bool ChatCommandParser::HandleInput(const GUIString& text)
{
 this->mError=0;
 this->mTarget=NULL;
 this->mTargetName=GetEmptyGUIStringData();
 if(this->mClients.get()==NULL) { this->mError=6;return false; }
 if(text[0]==this->mPrefix) return this->ParseCommand(text);
 if(this->mAllowColonEmote && text[0]==':') {
  this->mCommand=LobbyChatCommand_Emote;
  this->mText=text.substr(1);
  int pos=0;SkipWhitespace(this->mText,pos);
  if(pos==this->mText.length()) { this->mError=5;return false; }
 } else {
  this->mCommand=LobbyChatCommand_Broadcast;
  int pos=0;SkipWhitespace(text,pos);
  if(pos==text.length()) { this->mError=5;return false; }
  this->mText=text;
 }
 return true;
}

// FUNCTION: WON 0x757800
// Appends one help line and a line break to the help text
void ChatCommandParser::AddHelpEntry(int command,const GUIString& entry,GUIString& out)
{
 out.append(entry);
 out.append(GUIString("<n>"));
}

// FUNCTION: WON 0x7579c0
// Builds a client's chat help; outside LAN adds block/away, invite if game captain, mute/ban if captain of this
// room or a moderator, server mute/ban and warn if a moderator, alert if an admin; team help in game rooms
GUIString ChatCommandParser::GetHelp(unsigned short clientId)
{
 bool lan=(this->mRoomFlags>>3)&1;
 bool isAdmin=false,isModerator=false,isRoomCaptain=false,isGameCaptain=false;
 if(this->mClients.get()!=NULL) {
  LobbyClient* client=this->mClients->FindClient(clientId);
  if(client!=NULL) {
   isModerator=client->IsModerator();
   isRoomCaptain=client->IsCaptain((this->mRoomFlags>>1)&1);
   isGameCaptain=client->IsCaptain(1);
   isAdmin=client->IsAdmin();
  }
 }
 GUIString out=::ChatCommandLogic_HelpHeader_String;
 out.append(GUIString("<n>"));
 this->AddHelpEntry(LobbyChatCommand_Help,::ChatCommandLogic_HelpHelp_String,out);
 this->AddHelpEntry(LobbyChatCommand_Whisper,::ChatCommandLogic_WhisperHelp_String,out);
 this->AddHelpEntry(LobbyChatCommand_Reply,::ChatCommandLogic_ReplyHelp_String,out);
 this->AddHelpEntry(LobbyChatCommand_Emote,::ChatCommandLogic_EmoteHelp_String,out);
 this->AddHelpEntry(LobbyChatCommand_Ignore,::ChatCommandLogic_IgnoreHelp_String,out);
 this->AddHelpEntry(LobbyChatCommand_Clear,::ChatCommandLogic_ClearHelp_String,out);
 if(this->mRoomFlags&2) this->AddHelpEntry(LobbyChatCommand_ShowTeam,::ChatCommandLogic_ShowTeamHelp_String,out);
 if(!lan) {
  this->AddHelpEntry(LobbyChatCommand_Block,::ChatCommandLogic_BlockHelp_String,out);
  this->AddHelpEntry(LobbyChatCommand_Away,::ChatCommandLogic_AwayHelp_String,out);
  if(isGameCaptain) {
   this->AddHelpEntry(LobbyChatCommand_Invite,::ChatCommandLogic_InviteHelp_String,out);
   this->AddHelpEntry(LobbyChatCommand_Uninvite,::ChatCommandLogic_UninviteHelp_String,out);
  }
  if(isRoomCaptain||isModerator) {
   this->AddHelpEntry(LobbyChatCommand_Mute,::ChatCommandLogic_MuteHelp_String,out);
   this->AddHelpEntry(LobbyChatCommand_Unmute,::ChatCommandLogic_UnmuteHelp_String,out);
   this->AddHelpEntry(LobbyChatCommand_Ban,::ChatCommandLogic_BanHelp_String,out);
   this->AddHelpEntry(LobbyChatCommand_Unban,::ChatCommandLogic_UnbanHelp_String,out);
   if(isModerator) {
    this->AddHelpEntry(LobbyChatCommand_Moderator,::ChatCommandLogic_ModeratorHelp_String,out);
    this->AddHelpEntry(LobbyChatCommand_ServerMute,::ChatCommandLogic_ServerMuteHelp_String,out);
    this->AddHelpEntry(LobbyChatCommand_ServerUnmute,::ChatCommandLogic_ServerUnmuteHelp_String,out);
    this->AddHelpEntry(LobbyChatCommand_ServerBan,::ChatCommandLogic_ServerBanHelp_String,out);
    this->AddHelpEntry(LobbyChatCommand_ServerUnban,::ChatCommandLogic_ServerUnbanHelp_String,out);
    this->AddHelpEntry(LobbyChatCommand_Warn,::ChatCommandLogic_WarnHelp_String,out);
   }
  }
  if(isAdmin) this->AddHelpEntry(LobbyChatCommand_Alert,::ChatCommandLogic_AlertHelp_String,out);
 }
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
