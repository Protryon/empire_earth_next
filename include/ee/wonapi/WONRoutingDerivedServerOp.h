// Derived routing server op (0x6c bytes) and its inline constructor
#ifndef EE_WONAPI_WONROUTINGDERIVEDSERVEROP_H
#define EE_WONAPI_WONROUTINGDERIVEDSERVEROP_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
namespace WONAPI {
class RoutingDerivedServerOp : public RoutingOp {
protected:
    unsigned char mMsgType; //64, natural65..67 alignment
    ByteBufferPtr mRecvMsg; //68, full6c
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //slot 0x10, 2c5260/180, ret 8
public:
    RoutingDerivedServerOp(RoutingConnection* connection)
        :RoutingOp(connection),mMsgType(0) { this->mIsDerivedServerOp=true; } //SDK inline, 2bc701..2bc719
    virtual RoutingOpType GetType() const { return (RoutingOpType)RoutingOp_DerivedServerOp; } //2bc780, 6 bytes
    unsigned char GetMsgType() { return this->mMsgType; } //SDK inline, no standalone address
    const ByteBuffer* GetRecvMsg() const { return this->mRecvMsg.get(); } //SDK borrowed const getter
    // Implicit destructor releases +68, then RoutingOp (0x64 bytes);
    // 2bc7b0/157
};
}
#endif
