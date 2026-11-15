#ifndef EE_WONAPI_WONLOBBYCORE_H
#define EE_WONAPI_WONLOBBYCORE_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
namespace WONAPI {
class Component;
int LobbyDoDialog(Component* inDialog); // 0x758e50
WONStatus LobbyGetVersionStatus(); // 0x75b520 (status 70, or 100004 without a lobby)
int LobbyDoYesNoDialog(const GUIString& inTitle, const GUIString& inText, bool inPlaySound); // 0x758ef0 (callers pass true)
void LobbyExit(int inScreen);     // 0x7627f0
void LobbyShowCreateAccount(); // 0x7610d0
void LobbyShowLostPassword(); // 0x7610f0
void LobbyShowLostUsername(); // 0x761110
void LobbySetVersionStatus(WONStatus inStatus); // 0x75b500
ServerContext* LobbyGetAccountServers(); // 0x75b1f0
ServerContext* LobbyGetAuthServers(); // 0x75b250
ServerContext* LobbyGetFirewallDetectServers(); // 0x75b370
ServerContext* LobbyGetPatchServers(); // 0x75b3d0
void LobbyResetRoomDirServers(); // 0x75b430
class Container;
class ChatCommandParser;
class UDPManager;
class LobbyGame;
class LobbyClient;
Container* GetLobbyRoot();
void LobbyOpenPopup(Component* inPopup, int inFlags, int inX, int inY);
void LobbyPlaySound(Sound* inSound);
void LobbyPingGame(LobbyGame* inGame, bool inForce);
void LobbyQueryGameDetails(LobbyGame* inGame);
void LobbyKillPings(int inUnused);
void LobbyShowLoginScreen();
ChatCommandParser* LobbyGetChatCommandParser();
UDPManager* LobbyGetUDPManager();
AuthContext* GetLobbyAuthContext();
const std::string& LobbyGetSysTOUPath();
const std::string& LobbyGetGameTOUPath();
void LobbyGetTOUTimes(long& outSystemTime, long& outGameTime);
GUIString GetWONStatusString(WONStatus inStatus);
IPAddr LobbyGetPublicAddress();
bool LobbyOnLoginScreen();
bool LobbyOnLanScreen();
bool LobbyIsFirewallTestDone();
void LobbyRunOpWithStatus(AsyncOp* inOp, const GUIString& inTitle, const GUIString& inStatus, int inStatusMode, unsigned long inTimeout);
int LobbyGetNumPingsToDo();
void LobbyJoinGame(LobbyGame* inGame);
void LobbyHostGame();
class ComponentEvent;
void LobbySendEventTo(ComponentEvent* inEvent, Component* inTarget);
ChatCommandParser* LobbyGetRoomChatParser(int inRoomSpec);
void LobbyGameDisconnected(bool inShowError);
void LobbySetClientBlocked(LobbyClient* inClient, bool inBlocked, unsigned int inRoomSpec);
void LobbySetClientIgnored(LobbyClient* inClient, bool inIgnored, unsigned int inRoomSpec);
bool LobbyIsValidEmail(const std::string& inEmail) throw(); // STAND-IN (owner 2026-11-06): no unwind state around the temporary in the caller 0x7ca0a0
void LobbyKickClient(LobbyClient* inClient, bool inIsBan);
int LobbyGetGameScreenType();
class Window;
void LobbyDoErrorDialog(const GUIString& inTitle, WONStatus inStatus);
void LobbyDispatchEvent(ComponentEvent* inEvent);
bool LobbyDoCDKeyCheck();
bool LobbyIsCDKeyValid(); // 0x75b560 (a flag of the lobby's private state)
bool LobbyIsTOUNewer(long,long); // 0x75b670 (signed compare of the terms-of-use timestamps)
void LobbyKillFirewallTest();
void LobbyShowLanScreen();
Window* LobbyGetRootWindow();
void LobbyEnterInternetScreen(bool inJoinDefaultRoom);
void LobbyNetLogout();
bool LobbyNetHostGame(LobbyGame* inGame);
bool LobbyNetJoinGame(LobbyGame* inGame);
LobbyClient* LobbyGetNetMyClient();
IPAddr LobbyGetMyAddress(int inRoomSpec);
void LobbyHandleStagingMessage(const ByteBuffer* inData, LobbyClient* inClient);
void LobbyClientLeftGame(LobbyClient* inClient);
void LobbyClientKicked(LobbyClient* inClient, bool inIsBan);
void LobbyShowLobbyTab();
bool LobbyOnInternetScreen();
bool LobbyIsSuspended(); // the private state's byte at 0x54; false without a lobby
class ColorScheme;
ColorScheme* LobbyGetColorScheme(); // Component::GetColorScheme 0x6f5970
void LobbyAddLogicListener(Component* inComponent); // wraps AddListener 0x6f4b10
void LobbyHandleStagingReadBuffer(ReadBuffer& inData, LobbyClient* inClient); // staging logic 0x7efb50
unsigned short LobbyGetLobbyPort(); // the private state's 0xb8; 0 without a lobby
Component* LobbyGetCurrentScreen(); // 0x760ea0
void LobbySetNetworkAdapter(long inAddress); // 0x75b9c0 (a numeric IPv4 address)
ServerContext* LobbyGetDirServers(); // 0x75b2b0 (not owned; the private state's 0x60)
ServerContext* LobbyGetRoomDirServers(); // 0x75b310 (not owned; the private state's 0x64)
void LobbySetTOUTimes(long inSysTime, long inGameTime); // 0x75b5d0 (stores the private state's 0xbc / 0xc0)
void LobbyLoadGlobalPrefs(); // checks the lobby exists, then Load 0x75f7b0
void LobbyBindUDPPort(); // sets up the UDP port from the config
void LobbySetLobbyPort(unsigned short inPort); // 0x761420 (stores the private state's 0xb8; nothing without a lobby)
void LobbyShowInternetScreen();
void LobbyRemoveEventHooks(); // 0x759040 (removes the lobby's event and timer hooks)
bool LobbyUserCanceledLastStatusOp(); // 0x758f50 (the dialog logic's active-state byte)
bool LobbyLogout(bool);   // 0x761330 (forwards to StagingLogic's Logout)
struct LobbyUserInfo;
LobbyUserInfo* LobbyGetCurrentUser(); // not owned; NULL when no record matches the file ID
GUIString LobbyFormatString3(const GUIString& inFormat, const GUIString& inFirst,
    const GUIString& inSecond, const GUIString& inThird);

class LobbyServerList;
class LobbyGroup;
LobbyServerList* LobbyGetServerList(); // 0x75b6b0: the routing logic's mServerList (0x54)
LobbyGroup* LobbyGetChatGroup(); // 0x75b6d0: the routing logic's mChatGroup (0x6c)
GUIString LobbyFormatString2(const GUIString& inFormat, const GUIString& inFirst, const GUIString& inSecond);
void LobbyResetFirewallTest();
void LobbySetPublicAddress(const IPAddr& inAddress);
void LobbySetBehindFirewall(bool inBehindFirewall);
}
#endif
