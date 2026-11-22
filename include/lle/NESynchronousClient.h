// C:\EEX\Low-Level Engine\Network Engine\NESynchronousClient.h (RECONSTRUCTION; NESynchronousClient.cpp 0x45c00..0x47301).
// The lock-step multiplayer client: a main thread (connect retries, outgoing/incoming message dispatch) and a receive
// thread over one NEConnection, plus an xfer connection for file transfer; messages pass through the lag/loss
// simulator and the record/playback stage. Abstract: the game implements the pure virtual callbacks.
// Whole in Rust (ee_net client.rs; PORT_LEARNINGS "LLE Network Engine, round 3"): the class keeps the type callers
// hold, its virtual members and one-line forwarders; the original is tests/net/orig_c (the golden's reference).
#ifndef LLE_NESYNCHRONOUSCLIENT_H
#define LLE_NESYNCHRONOUSCLIENT_H

#include "lle/NESynchronousCommunications.h"
#include "lle/NEProtocol.h"
#include "rust/net/net.h"

class FSFile;
struct EeNetClient;                         // the client, whole in Rust (rust/ee_net.h, ee_net client.rs)
enum class EeNetClOp : uint32_t;            // its calls out
class NEMessage;

class LLE_API NESynchronousClient : public NESynchronousCommunications {
public:
    NESynchronousClient();
    virtual ~NESynchronousClient();

    NEConnection* GetConnection() const { return static_cast<NEConnection*>(ee_net_cl_connection(this->mRust)); }
    virtual unsigned long GetCurrentCommunicationsTick() const
    {
        return ee_net_cl_current_communications_tick(this->mRust);
    }
    unsigned long GetCurrentPhysicsTick() const { return ee_net_cl_current_physics_tick(this->mRust); }
    unsigned long GetDetectedGameCount() const { return ee_net_cl_detected_game_count(this->mRust); }
    unsigned char GetMySendToSlot() const { return ee_net_cl_my_send_to_slot(this->mRust); }
    unsigned long GetNumConnectionTrys() const { return ee_net_cl_num_connection_trys(this->mRust); }
    // the recorder's mode (NERecordPlaybackMode) and its playback speed (ee_net recplay.rs)
    unsigned char GetRecordPlaybackMode() const { return ee_net_cl_record_playback_mode(this->mRust); }
    void SetSlowPlayback() { ee_net_cl_set_slow_playback(this->mRust); }
    void SetNormalPlayback() { ee_net_cl_set_normal_playback(this->mRust); }
    void SendStartConnectMessage(NEMessage* inMessage) { ee_net_cl_send_start_connect_message(this->mRust, inMessage); }
    virtual void SetCurrentCommunicationsTick(unsigned long inTick)
    {
        ee_net_cl_set_current_communications_tick(this->mRust, (uint32_t)inTick);
    }
    void SetCurrentPhysicsTick(unsigned long& inTick)
    {
        ee_net_cl_set_current_physics_tick(this->mRust, (uint32_t)inTick);
    }
    void SetMaxNumConnectionTrys(unsigned long inMax)
    {
        ee_net_cl_set_max_num_connection_trys(this->mRust, (uint32_t)inMax);
    }
    void SetMySendToSlot(unsigned char inSlot) { ee_net_cl_set_my_send_to_slot(this->mRust, inSlot); }

    long Connect(const UString& inName, const UString& inHost, const UWideString& inAddress)
    {
        const char* theAddress = inAddress;
        EeNetClStrings theStrings = {reinterpret_cast<const uint8_t*>(static_cast<const char*>(inName)), inName.GetLength(),
                                     reinterpret_cast<const uint8_t*>(static_cast<const char*>(inHost)), inHost.GetLength(),
                                     reinterpret_cast<const uint8_t*>(theAddress), strlen(theAddress)};
        return ee_net_cl_connect(this->mRust, &theStrings);
    }
    long Disconnect() { return ee_net_cl_disconnect(this->mRust); }
    long GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress)
    {
        EeNetClSinks theSinks = {NEProtocol::SetTo<UString>, &outName, NEProtocol::SetTo<UString>, &outHost,
                                 NEProtocol::SetTo<UWideString>, &outAddress};
        return ee_net_cl_detected_game(this->mRust, (uint32_t)inIndex, &theSinks);
    }
    virtual long ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
    {
        return ee_net_cl_process_user_input(this->mRust, inMessage, inPlayer, true);
    }
    virtual long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
    {
        return ee_net_cl_process_user_input(this->mRust, inMessage, inPlayer, false);
    }
    virtual long SearchForGame(bool inStart) { return ee_net_cl_search_for_game(this->mRust, inStart); }

    // vtable 0x98994 slots 9..22: the client's own virtuals. StartThreads/StopThreads are implemented; the rest are
    // pure (the game implements them). Names and signatures of the pure ones inferred (slots by offset).
    virtual FSFile* CreateXferFile(const UWideString& inName, unsigned long inSize) = 0;  // +0x24 (Receive: opens the file of an incoming xfer)
    virtual bool IsPlaybackRecordingSession() = 0;                              // +0x28 (ProcessUserInputMessage)
    virtual unsigned long GetConnectTimeout() = 0;                              // +0x2c (Connect: the last argument)
    virtual unsigned char GetClientSlot() = 0;                                  // +0x30 (Receive: the slot in an NEMXferStatus)
    virtual void StartThreads()   // +0x34
    {
        ee_net_cl_start_threads(this->mRust);
    }
    virtual void StopThreads()   // +0x38
    {
        ee_net_cl_stop_threads(this->mRust);
    }
    virtual bool IgnoreConnectionTimeout() = 0;                                 // +0x3c (Receive: true skips the timeout disconnect)
    virtual void SetConnectionState(long inState) = 0;                          // +0x40 (1 connecting, 2 ended, 0)
    virtual void HandleMessage(NEMessage* inMessage) = 0;                       // +0x44 (unhandled messages)
    virtual void HandleUserInput(NEMessage* inMessage) = 0;                     // +0x48
    virtual void HandleConnectionLost() = 0;                                    // +0x4c (Receive: after a 75 s timeout)
    virtual unsigned long HandleStartGame(unsigned long inParameter) = 0;       // +0x50
    virtual void HandleStopGame() = 0;                                          // +0x54
    virtual void HandleConnectionTimeoutWarning(unsigned long inRemaining) = 0;  // +0x58 (Receive: ms left before the 75 s timeout)

private:
    NESynchronousClient(const NESynchronousClient&);
    NESynchronousClient& operator=(const NESynchronousClient&);
    // the client's calls out (ee_net's EeNetClOp): the protocol, the messages, the base class's fields, the virtuals
    static ptrdiff_t Host(void* inClient, EeNetClOp inOp, size_t a, size_t b, size_t c, size_t d);
    // the lag simulator's and the recorder's common calls out (ee_net's EeNetQOp)
    static ptrdiff_t QueueHost(void* inClient, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
        { return NESynchronousCommunications::QueueHostCommon(inOp, a, b, c, d); }

    EeNetClient* mRust;                     // NESynchronousClient's state and members (ee_net client.rs)
};

#endif
