// C:\EEX\Low-Level Engine\Network Engine\NESynchronousClient.h (RECONSTRUCTION; NESynchronousClient.cpp 0x45c00..0x47301).
// The lock-step multiplayer client: a main thread (connect retries, outgoing/incoming message dispatch) and a receive
// thread over one NEConnection, plus an xfer connection for file transfer; messages pass through the lag/loss
// simulator and the record/playback stage. The game's subclass answers the callbacks (GameHost, LLE round 13).
// Whole in Rust (ee_net client.rs; PORT_LEARNINGS "LLE Network Engine, round 3"): the class keeps the type callers
// hold and one-line forwarders; the original is tests/net/orig_c (the golden's reference).
#ifndef LLE_NESYNCHRONOUSCLIENT_H
#define LLE_NESYNCHRONOUSCLIENT_H

#include "lle/NESynchronousCommunications.h"
#include "lle/NEProtocol.h"
#include "rust/net/net.h"


class LLE_API NESynchronousClient : public NESynchronousCommunications {
public:
    explicit NESynchronousClient(EeNetClHostFn inHost);   // the subclass's GameHost<Game> (below)
    virtual ~NESynchronousClient();

    NEConnection* GetConnection() const { return static_cast<NEConnection*>(ee_net_cl_connection(this->mClient)); }
    unsigned long GetCurrentPhysicsTick() const { return ee_net_cl_current_physics_tick(this->mClient); }
    unsigned long GetDetectedGameCount() const { return ee_net_cl_detected_game_count(this->mClient); }
    unsigned char GetMySendToSlot() const { return ee_net_cl_my_send_to_slot(this->mClient); }
    unsigned long GetNumConnectionTrys() const { return ee_net_cl_num_connection_trys(this->mClient); }
    // the recorder's mode (NERecordPlaybackMode) and its playback speed (ee_net recplay.rs)
    unsigned char GetRecordPlaybackMode() const { return ee_net_cl_record_playback_mode(this->mClient); }
    void SetSlowPlayback() { ee_net_cl_set_slow_playback(this->mClient); }
    void SetNormalPlayback() { ee_net_cl_set_normal_playback(this->mClient); }
    void SendStartConnectMessage(NEMessage* inMessage) { ee_net_cl_send_start_connect_message(this->mClient, inMessage); }
    void SetCurrentPhysicsTick(unsigned long& inTick)
    {
        ee_net_cl_set_current_physics_tick(this->mClient, (uint32_t)inTick);
    }
    void SetMaxNumConnectionTrys(unsigned long inMax)
    {
        ee_net_cl_set_max_num_connection_trys(this->mClient, (uint32_t)inMax);
    }
    void SetMySendToSlot(unsigned char inSlot) { ee_net_cl_set_my_send_to_slot(this->mClient, inSlot); }

    long Connect(const UString& inName, const UString& inHost, const UWideString& inAddress)
    {
        const char* theAddress = inAddress;
        EeNetClStrings theStrings = {reinterpret_cast<const uint8_t*>(static_cast<const char*>(inName)), inName.GetLength(),
                                     reinterpret_cast<const uint8_t*>(static_cast<const char*>(inHost)), inHost.GetLength(),
                                     reinterpret_cast<const uint8_t*>(theAddress), strlen(theAddress)};
        return ee_net_cl_connect(this->mClient, &theStrings);
    }
    long Disconnect() { return ee_net_cl_disconnect(this->mClient); }
    long GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress)
    {
        EeNetClSinks theSinks = {NEProtocol::SetTo<UString>, &outName, NEProtocol::SetTo<UString>, &outHost,
                                 NEProtocol::SetTo<UWideString>, &outAddress};
        return ee_net_cl_detected_game(this->mClient, (uint32_t)inIndex, &theSinks);
    }

    // (the original's vtable 0x98994 slots 9..22: StartThreads/StopThreads, then the game's callbacks)
    void StartThreads() { ee_net_cl_start_threads(this->mClient); }
    void StopThreads() { ee_net_cl_stop_threads(this->mClient); }

protected:
    // The client's calls out to the game (the original's pure virtuals, slots +0x24..+0x58; LLE round 13: the
    // subclass's members by name), then the engine's (Host): the subclass passes GameHost<itself> to the constructor
    template <class Game> static Game* As(void* inClient) { return static_cast<Game*>(static_cast<NESynchronousClient*>(inClient)); }
    template <class Game> static ptrdiff_t GameHost(void* inClient, EeNetClOp inOp, size_t a, size_t b, size_t c, size_t d)
    {
        switch (inOp) {
        case EeNetClOp::CreateXferFile: return (ptrdiff_t)As<Game>(inClient)->CreateXferFile(UWideString(reinterpret_cast<char*>(a)), (unsigned long)b);
        case EeNetClOp::IsPlaybackRecordingSession: return As<Game>(inClient)->IsPlaybackRecordingSession();
        case EeNetClOp::ConnectTimeout: return (ptrdiff_t)As<Game>(inClient)->GetConnectTimeout();
        case EeNetClOp::ClientSlot: return As<Game>(inClient)->GetClientSlot();
        case EeNetClOp::IgnoreConnectionTimeout: return As<Game>(inClient)->IgnoreConnectionTimeout();
        case EeNetClOp::SetConnectionState: As<Game>(inClient)->SetConnectionState((long)(int32_t)a); return 0;
        case EeNetClOp::HandleMessage: As<Game>(inClient)->HandleMessage(reinterpret_cast<NEMessage*>(a)); return 0;
        case EeNetClOp::HandleUserInput: As<Game>(inClient)->HandleUserInput(reinterpret_cast<NEMessage*>(a)); return 0;
        case EeNetClOp::HandleConnectionLost: As<Game>(inClient)->HandleConnectionLost(); return 0;
        case EeNetClOp::HandleStartGame: return (ptrdiff_t)(uint32_t)As<Game>(inClient)->HandleStartGame((unsigned long)a);
        case EeNetClOp::HandleStopGame: As<Game>(inClient)->HandleStopGame(); return 0;
        case EeNetClOp::HandleConnectionTimeoutWarning: As<Game>(inClient)->HandleConnectionTimeoutWarning((unsigned long)a); return 0;
        default: return NESynchronousClient::Host(inClient, inOp, a, b, c, d);
        }
    }
    // the client's calls out to the engine (ee_net's EeNetClOp): the protocol, the messages, the base class's fields
    static ptrdiff_t Host(void* inClient, EeNetClOp inOp, size_t a, size_t b, size_t c, size_t d);

private:
    // the lag simulator's and the recorder's common calls out (ee_net's EeNetQOp)
    static ptrdiff_t QueueHost(void* inClient, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
        { return NESynchronousCommunications::QueueHostCommon(inOp, a, b, c, d); }
};

#endif
