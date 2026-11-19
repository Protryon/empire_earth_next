// won_chatparser_test's namespace Orig (by tools/won_chatparser_orig.py from 030204242): ChatCommandParser's
// class and ChatCommandParser.cpp's statements as they were before R6 WONLobby round 17 moved them to chatobj.rs.
// Do not edit: regenerate.
#ifndef WON_CHATPARSER_ORIG_H
#define WON_CHATPARSER_ORIG_H
extern wchar_t gEmptyWideString[];
namespace Orig {
using namespace WONAPI;
class ChatCommandParser : public RefCount {
public:
    ChatCommandParser();
    virtual ~ChatCommandParser();         // 0x3559f0; only virtual slot
    void InitCommands();
    void SetClientList(LobbyClientList* inClients, int inMatchThreshold);
    void AddCommand(const GUIString& inCommand, int inId);
    GUIString GetCommandString(int inId);
    const char* GetCommandName(int inId);
    void AddCommands(const std::wstring& inCommands, int inId);
    LobbyClient* FindClient(const GUIString& inName, int* outMatch);
    bool HandleInput(const GUIString& inInput); // with ParseCommand (0x756f40), chatobj.rs's
    void AddHelpEntry(int inId, const GUIString& inCommand, GUIString& outHelp);
    GUIString GetHelp(unsigned short inClientId);

    int GetCommand() const;
    GUIString GetTargetName() const; // the texts are copies (the original returned references to the members)
    int GetPrefix() const;
    LobbyClient* GetTarget() const { return this->mTarget.get(); }
    GUIString GetText() const;
    unsigned long GetDuration() const;
    unsigned long GetRoomFlags() const;
    LobbyClientList* GetClients() const { return this->mClients.get(); }
    int GetError() const;
    bool GetInvitation() const;
    void SetCommand(int inCommand);
    void SetInput(const GUIString& inInput); // the reply target's name
    void SetPrefix(int inPrefix);
    void SetMatchThreshold(int inThreshold);
    void SetAllowColonEmote(bool inAllow);
    void SetText(const GUIString& inText);
    void SetInvitation(bool inInvitation);
private:
    ChatCommandParser(const ChatCommandParser&);
    ChatCommandParser& operator=(const ChatCommandParser&);
    GUIString Text(EeWonChatParserText inText) const;
    SmartPtr<LobbyClientList> mClients;    // +0x08
    SmartPtr<LobbyClient> mTarget;         // +0x38
    EeWonChatParser* mParser;              // the original's +0x0c..+0x50 but these two
};

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
 int command=0;
 for(size_t i=0;;i++) {
  int row=ee_won_chatparser_init_command(i,&command); // chatobj.rs's table, the original's order
  if(row==-2) break;
  this->AddCommands(row>=0 ? (std::wstring)gLobbyResources[row].mString : std::wstring(gEmptyWideString),command);
 }
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
 for(size_t k=0;k<count;k++) this->AddHelpEntry(commands[k],gLobbyResources[ee_won_chat_help_row(commands[k])].mString,out);
 return out;
}

} // namespace Orig
#endif
