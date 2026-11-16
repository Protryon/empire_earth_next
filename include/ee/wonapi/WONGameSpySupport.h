// WON SDK GameSpySupport over the GameSpy Query & Reporting SDK: whole in Rust (ee_won gamespy.rs EeWonGameSpy, net
// round 28: the SDK's query socket, replies and heartbeats, the key-value maps, the auto pump, the service lookup);
// this class holds it. GameSpySupport.cpp, GameSpySDK/gqueryreporting.c and GameSpySDK/nonport.c are deleted.
#ifndef EE_WONAPI_WONGAMESPYSUPPORT_H
#define EE_WONAPI_WONGAMESPYSUPPORT_H
#include "ee/wonapi/WONRouting.h"
#define EE_WON_GAMESPY_RUST 1 // (won_gamespy_test: the master server's host is set through the class)
namespace WONAPI
{
enum GameSpyQueryType { GameSpyQueryType_Info = 1, GameSpyQueryType_Rules = 2, GameSpyQueryType_Players = 3 };

class GameSpySupport : public RefCount
{
public:
    GameSpySupport(const char* aGameName, const char* aGameVersion, const char* aSecretKey, ServerContext* theDirServers)
        : mRust(ee_won_gamespy_new(aGameName, aGameVersion, aSecretKey, static_cast<RefCount*>(this),
                                   static_cast<RefCount*>(theDirServers), WonCore())) {}
    WONStatus Startup(const IPAddr& ServAddr) // qr_init on the address's host string and port
    {
        std::string aHost = ServAddr.GetHostString(true);
        return ee_won_gamespy_startup(mRust, reinterpret_cast<const uint8_t*>(aHost.data()), aHost.size(), ServAddr.GetPort());
    }
    void Shutdown() { this->Op(EE_WON_GAMESPY_SHUTDOWN); } // sends the "gamemode" "exiting" state change
    WONStatus GetStatus() { return (WONStatus)this->Op(EE_WON_GAMESPY_STATUS); }
    int GetGameSpyStatus() { return (int)this->Op(EE_WON_GAMESPY_GAMESPY_STATUS); }
    void Pump() { this->Op(EE_WON_GAMESPY_PUMP); }
    void SetAutoPumpTime(unsigned long theMilliseconds) { this->Op(EE_WON_GAMESPY_SET_AUTO_PUMP, theMilliseconds); }
    bool IsAutoPumping() { return this->Op(EE_WON_GAMESPY_IS_AUTO_PUMPING) != 0; }
    void SetKeyValue(GameSpyQueryType theType, const char* theKey, const char* theValue)
    { ee_won_gamespy_set(mRust, theType, theKey, theValue); }
    const char* GetKeyValue(GameSpyQueryType theType, const char* theKey) { return ee_won_gamespy_get(mRust, theType, theKey); }
    bool EraseKeyValue(GameSpyQueryType theType, const std::string& theKey)
    { return ee_won_gamespy_set(mRust, theType, theKey.c_str(), NULL); }
    void EraseAllKeyValues(GameSpyQueryType theType) { this->Op(EE_WON_GAMESPY_ERASE_ALL, theType); }
    // the key-value maps' lock held across several updates
    void LockKeyValues() { this->Op(EE_WON_GAMESPY_LOCK); }
    void UnlockKeyValues() { this->Op(EE_WON_GAMESPY_UNLOCK); }
    void SetGameMode(const char* aGameMode) { ee_won_gamespy_set(mRust, 0, NULL, aGameMode); }
    // the SDK's qr_hostname: the master server's host the next Startup resolves
    static void SetMasterHost(const char* inHost) { ee_won_gamespy_set_master_host(inHost); }

protected:
    ~GameSpySupport() { ee_won_gamespy_free(mRust); }

private:
    long long Op(int inWhich, long long inNum = 0) { return ee_won_gamespy_op(mRust, inWhich, inNum); }
    EeWonGameSpy* mRust; // owned
};

typedef SmartPtr<GameSpySupport> GameSpySupportPtr;
} // namespace WONAPI

#endif
