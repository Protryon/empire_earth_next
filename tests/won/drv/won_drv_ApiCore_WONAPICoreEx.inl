// WONAPICoreEx's members only the drivers use, cut from ee/wonapi/WONAPICore.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
// each before start-up only
void SetDefaultFileDirectory(const char* inDirectory) { ee_won_api_set_directory(this->mCore,inDirectory); }
// a socket thread of the core's table (the drivers' own tables only: the game sets none)
void SetSocketThread(void* inThread) { ee_won_api_set_socket_thread(this->mCore,inThread); }
void SetDoSocketAndTimerThreads(bool inOn) { ee_won_api_set(this->mCore,1,inOn); }
