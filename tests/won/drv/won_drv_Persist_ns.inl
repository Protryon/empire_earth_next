// the functions only the drivers use, cut from ee/wonapi/WONLobbyPersistence.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
// the saved login users, by name ignoring case (none without lobby data)
inline size_t LobbyGetUserCount() { return (size_t)LobbyPC(LobbyPC_::UserCount); }
inline long LobbyGetUserLastUseAt(size_t inPos) { return (long)LobbyPC(LobbyPC_::UserLastUseAt, (int64_t)inPos); }
inline LobbyUserInfo* LobbyGetCurrentUser() { return (LobbyUserInfo*)LobbyPC(LobbyPC_::CurrentUser); } // NULL: the LAN user, or no record
inline void LobbySetConnectionSpeed(unsigned short inSpeed) { LobbyPC(LobbyPC_::ChangeConnectionSpeed, inSpeed); }
inline void LobbyRestoreDefaults() { LobbyPC(LobbyPC_::RestoreDefaults); } // the user flag set, the global one on a change
// the friends, by user id (not owned)
inline size_t LobbyGetFriendCount() { return (size_t)LobbyPC(LobbyPC_::FriendCount); }
inline LobbyFriend* LobbyGetFriendAt(size_t inPos) { return (LobbyFriend*)LobbyPC(LobbyPC_::FriendAt, (int64_t)inPos); }
inline void LobbyRemoveFriend(unsigned long inId) { LobbyPC(LobbyPC_::RemoveFriend, (int64_t)inId); } // the user flag: whether one went
// the ignored names, ignoring case; the user flag set to whether the list changed
inline size_t LobbyGetIgnoredNameCount() { return (size_t)LobbyPC(LobbyPC_::IgnoredCount); }
inline size_t LobbyGetLanNameCount() { return (size_t)LobbyPC(LobbyPC_::LanNameCount); }
inline void LobbyRemoveLanName() { LobbyPC(LobbyPC_::RemoveLanName); } // the most recent other one becomes current
// the direct-connect addresses and their names, by address
inline size_t LobbyGetDirectConnectCount() { return (size_t)LobbyPC(LobbyPC_::DirectCount); }
inline void LobbyResetFilters() { LobbyPC(LobbyPC_::ResetFilters); } // the defaults copied, the user flag set
