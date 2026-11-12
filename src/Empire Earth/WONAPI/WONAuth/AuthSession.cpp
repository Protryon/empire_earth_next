// AuthSession: Blowfish session encryption of server messages with sequence numbers; the session is ee_won's object
// (rust/crates/ee_won/src/peerauth.rs: its type, id, key, sequence numbers, last use and both framings), this class
// a RefCount shim owning it
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include <time.h>
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006e27b0
// Creates a session with its type, id, Blowfish key and length-field size; sequences start at 0
AuthSession::AuthSession(AuthType theType, unsigned short theId, Blowfish theKey,
						 unsigned char theLengthFieldSize)
	: mImpl(ee_won_auth_session_new(theType, theId, reinterpret_cast<const EeWonBlowfish*>(&theKey),
	                                theLengthFieldSize, static_cast<int64_t>(time(NULL))))
{
}

// FUNCTION: WON 0x006e2a80
// Destroys the session
AuthSession::~AuthSession()
{
	ee_won_auth_session_free(this->mImpl);
}

// the message replaced by the session's framing when the Rust side made a new one
static WONStatus SessionCrypt(EeWonAuthSession* inSession, bool inDecrypt, ByteBufferPtr& theMsg)
{
	ByteBufferPtr anOut;
	bool replaced = false;
	WONStatus aStatus = ee_won_auth_session_crypt(inSession, inDecrypt, reinterpret_cast<const uint8_t*>(theMsg->data()),
	                                             theMsg->length(), static_cast<int64_t>(time(NULL)), WonByteBufferSink,
	                                             &anOut, &replaced);
	if(replaced)
		theMsg = anOut;
	return aStatus;
}

// FUNCTION: WON 0x006e2b40
// Replaces the message with its encrypted form (session id and sequence number when sessioned)
WONStatus AuthSession::Encrypt(ByteBufferPtr &theMsg)
{
	return SessionCrypt(this->mImpl, false, theMsg);
}

// FUNCTION: WON 0x006e2d60
// Decrypts an encrypted service message in place, checking session id and sequence; returns the status
WONStatus AuthSession::Decrypt(ByteBufferPtr &theMsg)
{
	return SessionCrypt(this->mImpl, true, theMsg);
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e2a60
// (protected: virtual void * __thiscall WONAPI::AuthSession::`scalar deleting destructor'(unsigned int)    [??_GAuthSession@WONAPI@@MAEPAXI@Z])
