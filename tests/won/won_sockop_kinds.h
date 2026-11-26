// The socket op classes the drivers make (CloseOp .. RecvBytesFromOp): since R6 WONLobby round 28 they are kinds of
// the one SocketOp (include/ee/wonapi/WONRouting.h; sockops.rs's init_kind, duplicate). These are the original
// constructors' signatures over the kinds, so a driver's `new RecvMsgOp(s)` makes the op the game's `new
// SocketOp(EeWonCoreSocketOp::RecvMsg, s)` does; an op Rust makes (a Duplicate, a queue socket's own) is a plain
// SocketOp of the kind: ask its kind (WonSocketOpIs), not its C++ class. Include after ee/wonapi/WONRouting.h.
#ifndef WON_SOCKOP_KINDS_H
#define WON_SOCKOP_KINDS_H
#include "rust/won_net/hostop.h"
#include "rust/won_net/sockops.h"
namespace WONAPI {
typedef EeWonCoreSocketOp WonOpKind;
inline bool WonSocketOpIs(AsyncOp* inOp, WonOpKind inKind)
{
    SocketOp* s = dynamic_cast<SocketOp*>(inOp);
    return s != NULL && s->GetKind() == (int)inKind;
}
// the original class's name of an op (a kind's), or NULL for a C++ class of its own
inline const char* WonSocketOpClass(const AsyncOp* inOp)
{
    static const char* const kNames[] = {"CloseOp", "QueueCloseOp", "RecvBytesFromOp", "SendBytesToOp", "RecvMsgOp",
                                         "SendMsgOp", "ConnectOp", "SendBytesOp", "RecvBytesOp", "RecvCRMsgOp", "AcceptOp"};
    const SocketOp* s = dynamic_cast<const SocketOp*>(inOp);
    int k = s != NULL ? s->GetKind() : -1;
    return k >= 0 && k < 11 ? kNames[k] : NULL;
}
struct CloseOp : SocketOp { CloseOp(AsyncSocket* s) : SocketOp(WonOpKind::Close, s) {} };
struct AcceptOp : SocketOp { AcceptOp(AsyncSocket* s) : SocketOp(WonOpKind::Accept, s) {} };
struct ConnectOp : SocketOp
{ ConnectOp(const IPAddr& a, AsyncSocket* s = NULL) : SocketOp(WonOpKind::Connect, s, NULL, a.GetValue()) {} };
struct SendBytesOp : SocketOp { SendBytesOp(const ByteBuffer* b, AsyncSocket* s = NULL) : SocketOp(WonOpKind::SendBytes, s, b) {} };
struct SendMsgOp : SocketOp { SendMsgOp(const ByteBuffer* b, AsyncSocket* s = NULL) : SocketOp(WonOpKind::SendMsg, s, b) {} };
struct SendBytesToOp : SocketOp
{ SendBytesToOp(const ByteBuffer* b, const IPAddr& a, AsyncSocket* s = NULL) : SocketOp(WonOpKind::SendTo, s, b, a.GetValue()) {} };
struct RecvMsgOp : SocketOp { RecvMsgOp(AsyncSocket* s = NULL) : SocketOp(WonOpKind::RecvMsg, s) {} };
struct RecvCRMsgOp : SocketOp { RecvCRMsgOp(AsyncSocket* s = NULL) : SocketOp(WonOpKind::RecvCrMsg, s) {} };
struct RecvBytesFromOp : SocketOp
{ RecvBytesFromOp(unsigned short n, AsyncSocket* s = NULL) : SocketOp(WonOpKind::RecvFrom, s, NULL, NULL, n) {} };
struct RecvBytesOp : SocketOp {
    RecvBytesOp(unsigned long n, AsyncSocket* s = NULL) : SocketOp(WonOpKind::RecvBytes, s, NULL, NULL, n) {}
    // the file setters the game no longer calls
    void SetAppendFile(bool v) { ee_won_socketop_set_flag(this->GetSocketOpObject(), 0, v); }
    void SetBinaryFile(bool v) { ee_won_socketop_set_flag(this->GetSocketOpObject(), 1, v); }
    void SetSkipBytes(bool v) { ee_won_socketop_set_flag(this->GetSocketOpObject(), 2, v); }
    void SetFileModifyTime(time_t t) { ee_won_socketop_set_modify_time(this->GetSocketOpObject(), (int64_t)t); }
    std::string GetFilePath() const
    {
        std::string aPath;
        ee_won_socketop_path(this->GetSocketOpObject(), [](void* c, const uint8_t* b, size_t n) {
            static_cast<std::string*>(c)->assign(reinterpret_cast<const char*>(b), n);
        }, &aPath);
        return aPath;
    }
};
}
#endif
