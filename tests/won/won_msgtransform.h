// The C++ MsgTransform the drivers keep (2ad880; vtable 44ca2c): the game's one transform, CryptTransform, is Rust's
// since net round 29 (ee_won cppcore.rs), so the class and AsyncSocket::AddMsgTransform live here, over the Rust
// socket's C++ transform (ee_won_core_add_transform).
#ifndef EE_TESTS_WON_MSGTRANSFORM_H
#define EE_TESTS_WON_MSGTRANSFORM_H
#include "rust/won_net/hostop.h"
namespace WONAPI {
class MsgTransform {
public:
    virtual ~MsgTransform() {}
    virtual WONStatus Send(ByteBufferPtr&) { return 0; }
    virtual WONStatus Recv(ByteBufferPtr&) { return 0; }
    static long Call(void* inTransform, void* inMessage, bool inSend)
    {
        ByteBufferPtr& aMessage = *static_cast<ByteBufferPtr*>(inMessage);
        MsgTransform* t = static_cast<MsgTransform*>(inTransform);
        return inSend ? t->Send(aMessage) : t->Recv(aMessage);
    }
    static void Free(void* inTransform) { delete static_cast<MsgTransform*>(inTransform); }
};
// socket->AddMsgTransform(transform): the socket owns it
inline void AddMsgTransform(AsyncSocket* inSocket, MsgTransform* inTransform)
{
    ee_won_core_add_transform(static_cast<RefCount*>(inSocket), inTransform, MsgTransform::Call, MsgTransform::Free);
}
}
#endif
