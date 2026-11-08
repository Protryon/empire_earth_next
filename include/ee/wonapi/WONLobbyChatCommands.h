// ee/wonapi/WONLobbyChatCommands.h. Named integer constants; names PROVISIONAL unless the comment says T1.
#ifndef EE_WONLOBBYCHATCOMMANDS_H
#define EE_WONLOBBYCHATCOMMANDS_H

// the chat parser's commands (ChatCommandParser::mCommand); names from the resource strings the parser registers per command id
enum LobbyChatCommand {
    LobbyChatCommand_None = 0,          // T3 PROVISIONAL: ParseCommand stores 0 for an unknown command (mError 1)
    LobbyChatCommand_Broadcast = 1,     // T3 PROVISIONAL: HandleInput stores 1 for text that is not a /command (plain chat)
    LobbyChatCommand_Whisper = 2,       // T1 resource string ChatCommandLogic_Whisper_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Emote = 3,         // T1 resource string ChatCommandLogic_Emote_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Block = 4,         // T1 resource string ChatCommandLogic_Block_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Ignore = 5,        // T1 resource string ChatCommandLogic_Ignore_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Away = 6,          // T1 resource string ChatCommandLogic_Away_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Moderator = 7,     // T1 resource string ChatCommandLogic_Moderator_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_ServerMute = 8,    // T1 resource string ChatCommandLogic_ServerMute_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_ServerBan = 9,     // T1 resource string ChatCommandLogic_ServerBan_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Mute = 10,         // T1 resource string ChatCommandLogic_Mute_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Ban = 11,          // T1 resource string ChatCommandLogic_Ban_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Unmute = 12,       // T1 resource string ChatCommandLogic_Unmute_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_ServerUnmute = 13,  // T1 resource string ChatCommandLogic_ServerUnmute_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Unban = 14,        // T1 resource string ChatCommandLogic_Unban_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_ServerUnban = 15,  // T1 resource string ChatCommandLogic_ServerUnban_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Invite = 16,       // T1 resource string ChatCommandLogic_Invite_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Uninvite = 17,     // T1 resource string ChatCommandLogic_Uninvite_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Reply = 18,        // T1 resource string ChatCommandLogic_Reply_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Clear = 19,        // T1 resource string ChatCommandLogic_Clear_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Help = 20,         // T1 resource string ChatCommandLogic_Help_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_ShowTeam = 21,     // T1 resource string ChatCommandLogic_ShowTeam_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_Alert = 22,        // T1 resource string ChatCommandLogic_Alert_String (ChatCommandParser ctor AddCommands)
    LobbyChatCommand_StartShutdown = 23,  // T1 GetCommandName 23 -> "startshutdown"
    LobbyChatCommand_AbortShutdown = 24,  // T1 GetCommandName 24 -> "abortshutdown"
    LobbyChatCommand_Warn = 25,         // T1 resource string ChatCommandLogic_Warn_String (ChatCommandParser ctor AddCommands)
};

#endif // EE_WONLOBBYCHATCOMMANDS_H
