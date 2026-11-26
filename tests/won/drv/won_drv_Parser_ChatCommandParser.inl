// the members only the drivers use, cut from ee/wonapi/WONLobbyParser.h's ChatCommandParser (R6 WON GUI round 40: the
// game's parser reads are lobbytext.rs's, in place): included in its class body when EE_WON_DRIVER is defined
// (tools/xbuild.py build_won_test)
    // a name for a command id; an id's first name is its display name
    void AddCommand(const GUIString& inCommand, int inId)
    { ee_won_chatparser_add_command(this->mParser, inCommand.mData->mText, inCommand.length(), sizeof(wchar_t), inId); }
    GUIString GetCommandString(int inId)
    {
        size_t n = 0;
        const uint32_t* u = ee_won_chatparser_command_string(this->mParser, inId, &n);
        return n != 0 ? Units(u, n) : GUIString("");
    }
    const char* GetCommandName(int inId) { return ee_won_chat_command_name(inId); } // 'w', 'em', 'afk', ...
    // a comma-separated list of names; none: the built-in short name
    void AddCommands(const std::wstring& inCommands, int inId)
    { ee_won_chatparser_add_commands(this->mParser, inCommands.c_str(), inCommands.length(), inId); }
    // by full name, else a unique prefix (no reference added)
    LobbyClient* FindClient(const GUIString& inName, int* outMatch)
    { return (LobbyClient*)ee_won_chatparser_find_client(this->mParser, inName.mData->mText, inName.length(), outMatch); }
    bool HandleInput(const GUIString& inInput) // with ParseCommand (0x756f40)
    { return ee_won_chatparser_handle_input_list(this->mParser, inInput.mData->mText, inInput.length()); }
    void AddHelpEntry(int inId, const GUIString& inCommand, GUIString& outHelp)
    {
        outHelp.append(inCommand);
        outHelp.append(GUIString("<n>"));
    }
    GUIString GetHelp(unsigned short inClientId)
    {
        size_t n = 0;
        const uint32_t* u = ee_won_chatparser_help(this->mParser, inClientId, &n);
        return Units(u, n);
    }

    int GetCommand() const { return (int)this->Number(EeWonChatParserNumber::Command); }
    GUIString GetTargetName() const { return this->Text(EeWonChatParserText::TargetName); } // the texts are copies
    int GetPrefix() const { return (int)this->Number(EeWonChatParserNumber::Prefix); }
    LobbyClient* GetTarget() const { return (LobbyClient*)ee_won_chatparser_object(this->mParser, 1); }
    GUIString GetText() const { return this->Text(EeWonChatParserText::Text); }
    unsigned long GetDuration() const { return (unsigned long)this->Number(EeWonChatParserNumber::Duration); }
    unsigned long GetRoomFlags() const { return (unsigned long)this->Number(EeWonChatParserNumber::RoomFlags); }
    LobbyClientList* GetClients() const { return (LobbyClientList*)ee_won_chatparser_object(this->mParser, 0); }
    int GetError() const { return (int)this->Number(EeWonChatParserNumber::Error); }
    bool GetInvitation() const { return this->Number(EeWonChatParserNumber::Invitation) != 0; }
    void SetCommand(int inCommand) { this->SetNumber(EeWonChatParserNumber::Command, inCommand); }
    void SetInput(const GUIString& inInput) { this->SetText(EeWonChatParserText::Input, inInput); } // the reply target's name
    void SetPrefix(int inPrefix) { this->SetNumber(EeWonChatParserNumber::Prefix, inPrefix); }
    void SetMatchThreshold(int inThreshold) { this->SetNumber(EeWonChatParserNumber::MatchThreshold, inThreshold); }
    void SetAllowColonEmote(bool inAllow) { this->SetNumber(EeWonChatParserNumber::AllowColonEmote, inAllow ? 1 : 0); }
    void SetText(const GUIString& inText) { this->SetText(EeWonChatParserText::Text, inText); }
    void SetInvitation(bool inInvitation) { this->SetNumber(EeWonChatParserNumber::Invitation, inInvitation ? 1 : 0); }
private:
    static GUIString Units(const uint32_t* inUnits, size_t inCount)
    {
        std::wstring aText;
        aText.reserve(inCount);
        for (size_t i = 0; i < inCount; i++)
            aText += (wchar_t)inUnits[i];
        return GUIString(aText);
    }
    GUIString Text(EeWonChatParserText inText) const
    {
        size_t n = 0;
        const uint32_t* u = ee_won_chatparser_text(this->mParser, inText, &n);
        return Units(u, n);
    }
    void SetText(EeWonChatParserText inWhich, const GUIString& inText)
    { ee_won_chatparser_set_text(this->mParser, inWhich, inText.mData->mText, inText.length(), sizeof(wchar_t)); }
    int64_t Number(EeWonChatParserNumber inNumber) const { return ee_won_chatparser_number(this->mParser, inNumber); }
    void SetNumber(EeWonChatParserNumber inNumber, int64_t inValue) { ee_won_chatparser_set_number(this->mParser, inNumber, inValue); }
