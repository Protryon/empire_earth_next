// GameSpySupport's members only the drivers use, cut from ee/wonapi/WONGameSpySupport.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
int GetGameSpyStatus() { return (int)this->Op(EE_WON_GAMESPY_GAMESPY_STATUS); }
void SetAutoPumpTime(unsigned long theMilliseconds) { this->Op(EE_WON_GAMESPY_SET_AUTO_PUMP, theMilliseconds); }
bool IsAutoPumping() { return this->Op(EE_WON_GAMESPY_IS_AUTO_PUMPING) != 0; }
bool EraseKeyValue(GameSpyQueryType theType, const std::string& theKey)
{ return ee_won_gamespy_set(mRust, theType, theKey.c_str(), NULL); }
void EraseAllKeyValues(GameSpyQueryType theType) { this->Op(EE_WON_GAMESPY_ERASE_ALL, theType); }
// the key-value maps' lock held across several updates
void LockKeyValues() { this->Op(EE_WON_GAMESPY_LOCK); }
void UnlockKeyValues() { this->Op(EE_WON_GAMESPY_UNLOCK); }
void SetGameMode(const char* aGameMode) { ee_won_gamespy_set(mRust, 0, NULL, aGameMode); }
// the SDK's qr_hostname: the master server's host the next Startup resolves
static void SetMasterHost(const char* inHost) { ee_won_gamespy_set_master_host(inHost); }
