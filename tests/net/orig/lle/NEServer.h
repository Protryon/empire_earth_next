// ne_msgmgr_test stand-in: the protocol's server; a send is a world line and its result the world's
#pragma once
#include "net_world.h"
class NEServer {
public:
    long SendTo(unsigned long inIndex, const unsigned char* inData, unsigned long inLength)
        { return gWorld->Send("send", inIndex, inData, inLength); }
    long XferSendTo(unsigned long inIndex, const unsigned char* inData, unsigned long inLength)
        { return gWorld->Send("xsend", inIndex, inData, inLength); }
};
