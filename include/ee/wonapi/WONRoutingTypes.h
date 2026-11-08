// ee/wonapi/WONRoutingTypes.h: the WON SDK's RoutingTypes constants the routing chat commands and the lobby's
// routing logic use (client ids, client flags, chat flags).
#ifndef EE_WONROUTINGTYPES_H
#define EE_WONROUTINGTYPES_H

enum RoutingSpecialId {
    RoutingId_Global = 0xffff,
};

enum RoutingClientFlag {
    RoutingClientFlag_IsModerator = 1,
    RoutingClientFlag_IsAway = 8,
};

enum RoutingChatFlag {
    RoutingChatFlag_IsEmote = 2,
    RoutingChatFlag_IsWhisper = 4,
};

#endif // EE_WONROUTINGTYPES_H
