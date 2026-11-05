// WON (World Opponent Network) library stand-ins. The WONAPI library (online lobby, routing server, the lobby GUI
// toolkit) is not part of the source reconstruction, and its servers no longer exist. The game's own lobby code
// (src/Empire Earth/WONAPI/WONLobby/GameCode, Communications/EEWonNet.cpp) is compiled and calls into it; this unit
// defines every library function and global it references, declared by include/ee/wonapi/*.h and ee/EEWonNet.h:
//   - graceful stand-ins: log their name once ("EE-WON-STUB <signature>", as a debug string), then return 0, false,
//     NULL or an empty object (a constructor leaves its members default-constructed);
//   - 13 functions that return or take a WON class by value cannot be answered gracefully: they stop the game with a
//     fatal error naming the function ("EE-WON-FATAL <signature>", then abort()).
// No game path outside the online lobby reaches either kind. The vtable slots the original's ICF had pointed at
// unrelated game functions (ListItem::Virtual20 ran EEUGAttackGround::ReplaceUnit's `ret`, ...) are stand-ins here too.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/EELobbyGameCode.h"

#if defined(_MSC_VER)
#define WON_SIGNATURE __FUNCSIG__
#else
#define WON_SIGNATURE __PRETTY_FUNCTION__
#endif

static void WONReport(const char* inKind, const char* inSignature)
{
    char theText[1024];
    snprintf(theText, sizeof(theText), "EE-WON-%s %s\n", inKind, inSignature);
#ifdef _WIN32
    OutputDebugStringA(theText);
#else
    fputs(theText, stderr);
#endif
}

// a graceful stand-in: report the first call
#define WON_STUB() do { static bool sReported = false; if (!sReported) { sReported = true; \
    WONReport("STUB", WON_SIGNATURE); } } while (0)
// no graceful answer (a WON class by value): a fatal error
#define WON_TRAP() do { WONReport("FATAL", WON_SIGNATURE); abort(); } while (0)

// ---- the library's globals ----
namespace WONAPI {
bool gB565F08;
const char* gCDKeyRegistryKey = NULL;
GUIString gEmptyGUIString;
GUIString gLobbyGameDoneText;
GUIString gLobbyGameDoneTitle;
SmartPtr<Font> gLobbyFont5665C0;
SmartPtr<Font> gLobbyFont5666BC;
SmartPtr<Font> gLobbyFont5668AC;
void* gLobbyNetHandle;
LobbyConfig* (*gLobbyConfigFactory)() = NULL;
LobbyGame* (*gLobbyGameFactory)() = NULL;
RefCount* (*gLobbyPlayerFactory)() = NULL;
ServerDetailsItem* (*gServerDetailsItemFactory)() = NULL;
}

// ---- the library's functions ----

long WONAPI::AcceptOp::Virtual10()
{
    WON_STUB();
    return 0;
}

WONAPI::AsyncOp::AsyncOp()
{
    WON_STUB();
}

WONAPI::AsyncOp::~AsyncOp()
{
}

unsigned long WONAPI::AsyncOp::TimeLeft()
{
    WON_STUB();
    return 0;
}

void WONAPI::AsyncOp::Kill(bool)
{
    WON_STUB();
}

void WONAPI::AsyncOp::Run(long)
{
    WON_STUB();
}

void WONAPI::AsyncOp::SetFlag3A(bool)
{
    WON_STUB();
}

void WONAPI::AsyncOp::Virtual04()
{
    WON_STUB();
}

void WONAPI::AsyncOp::Virtual08()
{
    WON_STUB();
}

WONAPI::AsyncOpTrackerMaps::AsyncOpTrackerMaps(bool, bool)
{
    WON_STUB();
}

WONAPI::AsyncSocket::AsyncSocket(int)
{
    WON_STUB();
}

long WONAPI::AsyncSocket::Fn2B0D90(unsigned short, int)
{
    WON_STUB();
    return 0;
}

long WONAPI::AsyncSocket::Fn2B0FC0(int)
{
    WON_STUB();
    return 0;
}

long WONAPI::AsyncSocket::Send(void const*, unsigned long, unsigned long*)
{
    WON_STUB();
    return 0;
}

void WONAPI::AsyncSocket::Close()
{
    WON_STUB();
}

void WONAPI::AsyncSocket::Fn2B1360(unsigned long)
{
    WON_STUB();
}

void WONAPI::AsyncSocket::Fn2B1AD0()
{
    WON_STUB();
}

void WONAPI::AsyncSocket::Fn2B3BF0(WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::AsyncSocket::Fn2B4500(WONAPI::AsyncOp*)
{
    WON_STUB();
}

void WONAPI::AsyncSocket::Fn2B4740(WONAPI::AsyncOp*, long)
{
    WON_STUB();
}

void WONAPI::AsyncSocket::SetCloseCompletion(WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

WONAPI::Button::Button()
{
    WON_STUB();
}

WONAPI::Button::~Button()
{
}

bool WONAPI::Button::VirtualAC()
{
    WON_STUB();
    return false;
}

void WONAPI::Button::Fn2F3370(unsigned long, bool)
{
    WON_STUB();
}

void WONAPI::Button::SetCheck(bool, bool)
{
    WON_STUB();
}

void WONAPI::Button::SetText(WONAPI::GUIString const&, bool)
{
    WON_STUB();
}

void WONAPI::Button::VirtualB0(int)
{
    WON_STUB();
}

WONAPI::ButtonConfig::ButtonConfig()
{
    WON_STUB();
}

WONAPI::ByteBuffer::ByteBuffer(void const*, unsigned long, bool)
{
    WON_STUB();
}

WONAPI::CDKey::CDKey(std::string const&)
{
    WON_STUB();
}

bool WONAPI::CDKey::LoadFromRegistry(long, char const*)
{
    WON_STUB();
    return false;
}

void WONAPI::CDKey::SaveToRegistry(long, char const*)
{
    WON_STUB();
}

int WONAPI::ComboBox::GetSelItem()
{
    WON_STUB();
    return 0;
}

void WONAPI::ComboBox::AddString(WONAPI::GUIString const&, int)
{
    WON_STUB();
}

void WONAPI::ComboBox::Clear(bool)
{
    WON_STUB();
}

void WONAPI::ComboBox::SetSelItem(int, bool)
{
    WON_STUB();
}

void WONAPI::ComboBox::SetText(WONAPI::GUIString const&, bool)
{
    WON_STUB();
}

WONAPI::Component::Component()
{
    WON_STUB();
}

WONAPI::Component::~Component()
{
}

WONAPI::RootContainer* WONAPI::Component::Fn2F5940()
{
    WON_STUB();
    return NULL;
}

WONAPI::RootContainer* WONAPI::Component::GetRoot()
{
    WON_STUB();
    return NULL;
}

void WONAPI::Component::AddListener(WONAPI::ComponentListener*)
{
    WON_STUB();
}

void WONAPI::Component::Fn2F4E90(int, int)
{
    WON_STUB();
}

void WONAPI::Component::Fn2F5380(int, int)
{
    WON_STUB();
}

void WONAPI::Component::Fn2F56D0(int&, int&)
{
    WON_STUB();
}

void WONAPI::Component::Fn2F5890()
{
    WON_STUB();
}

void WONAPI::Component::HandleComponentEvent(WONAPI::ComponentEvent*)
{
    WON_STUB();
}

void WONAPI::Component::InitComponent(WONAPI::ComponentInit&)
{
    WON_STUB();
}

void WONAPI::Component::Invalidate()
{
    WON_STUB();
}

void WONAPI::Component::Paint(WONAPI::Graphics&)
{
    WON_STUB();
}

void WONAPI::Component::SetEnabled(bool)
{
    WON_STUB();
}

void WONAPI::Component::SetVisible(bool)
{
    WON_STUB();
}

void WONAPI::Component::Virtual04()
{
    WON_STUB();
}

void WONAPI::Component::Virtual08()
{
    WON_STUB();
}

void WONAPI::Component::Virtual0C(int&, int&)
{
    WON_STUB();
}

void WONAPI::Component::Virtual10()
{
    WON_STUB();
}

void WONAPI::Component::Virtual14()
{
    WON_STUB();
}

void WONAPI::Component::Virtual20()
{
    WON_STUB();
}

void WONAPI::Component::Virtual24(int, int)
{
    WON_STUB();
}

void WONAPI::Component::Virtual28()
{
    WON_STUB();
}

void WONAPI::Component::Virtual2C(int, int, int)
{
    WON_STUB();
}

void WONAPI::Component::Virtual34(int, int)
{
    WON_STUB();
}

void WONAPI::Component::Virtual38()
{
    WON_STUB();
}

void WONAPI::Component::Virtual40()
{
    WON_STUB();
}

void WONAPI::Component::Virtual44()
{
    WON_STUB();
}

void WONAPI::Component::Virtual48()
{
    WON_STUB();
}

void WONAPI::Component::Virtual4C()
{
    WON_STUB();
}

void WONAPI::Component::Virtual5C()
{
    WON_STUB();
}

void WONAPI::Component::Virtual60()
{
    WON_STUB();
}

void WONAPI::Component::Virtual68()
{
    WON_STUB();
}

void WONAPI::Component::Virtual6C()
{
    WON_STUB();
}

void WONAPI::Component::Virtual70()
{
    WON_STUB();
}

void WONAPI::Component::Virtual74(bool)
{
    WON_STUB();
}

void WONAPI::Component::Virtual78()
{
    WON_STUB();
}

void WONAPI::Component::Virtual80()
{
    WON_STUB();
}

void WONAPI::Component::Virtual84()
{
    WON_STUB();
}

void WONAPI::Component::Virtual88()
{
    WON_STUB();
}

void WONAPI::Component::Virtual8C()
{
    WON_STUB();
}

void WONAPI::Component::Virtual98()
{
    WON_STUB();
}

void WONAPI::Component::Virtual9C()
{
    WON_STUB();
}

void WONAPI::Component::VirtualA0()
{
    WON_STUB();
}

WONAPI::ComponentEvent::ComponentEvent(WONAPI::Component*, int)
{
    WON_STUB();
}

int WONAPI::ComponentEvent::GetControlId() const
{
    WON_STUB();
    return 0;
}

void WONAPI::ComponentListener::Virtual04()
{
    WON_STUB();
}

void WONAPI::ComponentListenerComp::Virtual04()
{
    WON_STUB();
}

void WONAPI::ConfigObject::KeyAdd(char const*, unsigned short&, unsigned short)
{
    WON_STUB();
}

long WONAPI::ConnectOp::Virtual0C()
{
    WON_STUB();
    return 0;
}

long WONAPI::ConnectOp::Virtual10()
{
    WON_STUB();
    return 0;
}

WONAPI::Container::Container()
{
    WON_STUB();
}

WONAPI::Container::~Container()
{
}

void WONAPI::Container::Fn2F5360(int)
{
    WON_STUB();
}

void WONAPI::Container::Fn2F6140(WONAPI::Component*, int, WONAPI::Component*, int, int, int, int, int)
{
    WON_STUB();
}

void WONAPI::Container::Fn2F6A60(WONAPI::Component*)
{
    WON_STUB();
}

void WONAPI::Container::VirtualAC(WONAPI::Component*, int)
{
    WON_STUB();
}

void WONAPI::Container::VirtualB4()
{
    WON_STUB();
}

void WONAPI::Container::VirtualB8()
{
    WON_STUB();
}

void WONAPI::Container::VirtualBC(WONAPI::Component*)
{
    WON_STUB();
}

void WONAPI::Container::VirtualC0()
{
    WON_STUB();
}

void WONAPI::Container::VirtualC4()
{
    WON_STUB();
}

void WONAPI::Container::VirtualC8()
{
    WON_STUB();
}

WONAPI::ContainerConfig::ContainerConfig()
{
    WON_STUB();
}

WONAPI::ContainerConfig::~ContainerConfig()
{
}

WONAPI::Dialog::Dialog()
{
    WON_STUB();
}

void WONAPI::Dialog::HandleComponentEvent(WONAPI::ComponentEvent*)
{
    WON_STUB();
}

WONAPI::IPAddr WONAPI::DirEntity::GetNetAddrAsIP() const
{
    WON_TRAP();
}

WONAPI::GUIString::GUIString()
{
    WON_STUB();
}

WONAPI::GUIStringData::GUIStringData(WONAPI::GUIStringData const&)
{
    WON_STUB();
}

WONAPI::GUIStringData::GUIStringData(char const*)
{
    WON_STUB();
}

WONAPI::GUIStringData::GUIStringData(std::string const&)
{
    WON_STUB();
}

WONAPI::GUIStringData::GUIStringData(std::wstring const&)
{
    WON_STUB();
}

int WONAPI::GUIStringData::Compare(WONAPI::GUIStringData const*) const
{
    WON_STUB();
    return 0;
}

int WONAPI::GUIStringData::Compare(WONAPI::GUIStringData const*, bool) const
{
    WON_STUB();
    return 0;
}

int WONAPI::GUIStringData::CompareNoCase(WONAPI::GUIStringData const*) const
{
    WON_STUB();
    return 0;
}

std::string WONAPI::GUIStringData::GetString() const
{
    WON_TRAP();
}

std::wstring WONAPI::GUIStringData::GetWString() const
{
    WON_TRAP();
}

WONAPI::GetCertOp::GetCertOp(EEWonNetUser*)
{
    WON_STUB();
}

WONAPI::GetDirOp::GetDirOp(WONAPI::ServerContext*)
{
    WON_STUB();
}

void WONAPI::GetDirOp::AddPath(std::wstring const&)
{
    WON_STUB();
}

WONAPI::GetHTTPDocumentOp::GetHTTPDocumentOp(std::string const&, WONAPI::ServerContext*)
{
    WON_STUB();
}

WONAPI::SmartPtr<WONAPI::ByteBuffer> WONAPI::GetMOTDOp::GetMOTD(bool)
{
    WON_TRAP();
}

void WONAPI::Graphics::DrawRect(int, int, int, int)
{
    WON_STUB();
}

void WONAPI::Graphics::DrawString(WONAPI::GUIString const&, int, int)
{
    WON_STUB();
}

void WONAPI::Graphics::FillRect(int, int, int, int)
{
    WON_STUB();
}

void WONAPI::Graphics::SetColor(unsigned long)
{
    WON_STUB();
}

void WONAPI::Graphics::SetSysColor(int)
{
    WON_STUB();
}

WONAPI::IPAddr::IPAddr()
{
    WON_STUB();
}

WONAPI::IPAddr::IPAddr(std::string const&)
{
    WON_STUB();
}

bool WONAPI::IPAddr::IsValid() const
{
    WON_STUB();
    return false;
}

std::string WONAPI::IPAddr::GetHostString(bool) const
{
    WON_TRAP();
}

void WONAPI::IPAddr::SetPortA(unsigned short)
{
    WON_STUB();
}

void WONAPI::Label::SetText(WONAPI::GUIString const&, bool)
{
    WON_STUB();
}

WONAPI::ListItem* WONAPI::ListArea::GetItem(int)
{
    WON_STUB();
    return NULL;
}

void WONAPI::ListArea::Fn2FAA00()
{
    WON_STUB();
}

void WONAPI::ListArea::Fn2FAA10()
{
    WON_STUB();
}

void WONAPI::ListArea::Fn2FAA50(bool)
{
    WON_STUB();
}

void WONAPI::ListArea::Fn2FC730(WONAPI::ListItem*)
{
    WON_STUB();
}

void WONAPI::ListArea::InsertItem(WONAPI::ListItem*, int)
{
    WON_STUB();
}

void WONAPI::ListArea::InsertString(WONAPI::GUIString const&, int)
{
    WON_STUB();
}

void WONAPI::ListArea::RemoveItem(WONAPI::ListItem*)
{
    WON_STUB();
}

WONAPI::ListItem::ListItem()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual04()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual08()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual0C()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual10()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual20()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual24()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual28()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual2C()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual30()
{
    WON_STUB();
}

void WONAPI::ListItem::Virtual34()
{
    WON_STUB();
}

bool WONAPI::LobbyClient::Fn3A54E0(int)
{
    WON_STUB();
    return false;
}

WONAPI::LobbyClient* WONAPI::LobbyClientSet::FindClient(unsigned short)
{
    WON_STUB();
    return NULL;
}

WONAPI::LobbyClientSetIterator::LobbyClientSetIterator(WONAPI::LobbyClientSet*)
{
    WON_STUB();
}

bool WONAPI::LobbyClientSetIterator::HasMore()
{
    WON_STUB();
    return false;
}

bool WONAPI::LobbyClientSetIterator::Next(WONAPI::LobbyClient**)
{
    WON_STUB();
    return false;
}

void WONAPI::LobbyCompletionBase::Complete(WONAPI::SmartPtr<WONAPI::AsyncOp>)
{
    WON_TRAP();
}

WONAPI::LobbyConfig::LobbyConfig()
{
    WON_STUB();
}

bool WONAPI::LobbyEvent::Fn359470()
{
    WON_STUB();
    return false;
}

WONAPI::GUIString WONAPI::LobbyGame::Virtual64(short)
{
    WON_STUB();
    return WONAPI::GUIString();
}

WONAPI::LobbyGame::LobbyGame()
{
    WON_STUB();
}

WONAPI::LobbyGame::~LobbyGame()
{
}

bool WONAPI::LobbyGame::Fn3A6D40(long)
{
    WON_STUB();
    return false;
}

bool WONAPI::LobbyGame::Virtual20()
{
    WON_STUB();
    return false;
}

bool WONAPI::LobbyGame::Virtual28()
{
    WON_STUB();
    return false;
}

bool WONAPI::LobbyGame::Virtual58(WONAPI::ReadBuffer&)
{
    WON_STUB();
    return false;
}

bool WONAPI::LobbyGame::Virtual5C(WONAPI::ReadBuffer&)
{
    WON_STUB();
    return false;
}

long WONAPI::LobbyGame::Virtual24()
{
    WON_STUB();
    return 0;
}

short WONAPI::LobbyGame::Virtual38(WONAPI::ReadBuffer&, WONAPI::LobbyClient*)
{
    WON_STUB();
    return 0;
}

void WONAPI::LobbyGame::Virtual04()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual08()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual0C()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual10()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual14()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual18()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual1C()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual2C()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual30(WONAPI::LobbyClient*)
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual34(WONAPI::WriteBuffer&)
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual3C(WONAPI::LobbyClient*, WONAPI::WriteBuffer&)
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual40()
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual44(WONAPI::ReadBuffer&)
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual48(WONAPI::LobbyClient*, WONAPI::LobbyPlayer*)
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual4C(WONAPI::LobbyClient*)
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual50(WONAPI::WriteBuffer&)
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual54(WONAPI::WriteBuffer&)
{
    WON_STUB();
}

void WONAPI::LobbyGame::Virtual60(unsigned char, WONAPI::ReadBuffer&, WONAPI::LobbyClient*)
{
    WON_STUB();
}

WONAPI::LobbyPlayer::LobbyPlayer()
{
    WON_STUB();
}

WONAPI::LobbyPlayer::~LobbyPlayer()
{
}

bool WONAPI::LobbyPlayer::IsTeammate(WONAPI::LobbyPlayer*)
{
    WON_STUB();
    return false;
}

bool WONAPI::LobbyPlayer::ReadData(WONAPI::ReadBuffer&)
{
    WON_STUB();
    return false;
}

void WONAPI::LobbyPlayer::WriteData(WONAPI::WriteBuffer&)
{
    WON_STUB();
}

WONAPI::LobbyClient* WONAPI::LobbyRoomClients::GetClient(int)
{
    WON_STUB();
    return NULL;
}

WONAPI::MSNativeIcon::~MSNativeIcon()
{
}

WONAPI::MSWindow::MSWindow()
{
    WON_STUB();
}

WONAPI::MSWindowManager::MSWindowManager(HINSTANCE__*)
{
    WON_STUB();
}

WONAPI::NativeIcon::NativeIcon(WONAPI::DisplayContext*)
{
    WON_STUB();
}

WONAPI::PlayerTeamChangedEvent::PlayerTeamChangedEvent(WONAPI::LobbyClient*, bool) : LobbyChangeEvent(0, 0)
{
    WON_STUB();
}

WONAPI::PopupParams::PopupParams(WONAPI::Component*, int, WONAPI::Component*, int, int)
{
    WON_STUB();
}

WONAPI::ReadBuffer::ReadBuffer(void const*, unsigned long)
{
    WON_STUB();
}

bool WONAPI::ReadBuffer::ReadBool()
{
    WON_STUB();
    return false;
}

long WONAPI::ReadBuffer::ReadLong()
{
    WON_STUB();
    return 0;
}

short WONAPI::ReadBuffer::ReadShort()
{
    WON_STUB();
    return 0;
}

unsigned char WONAPI::ReadBuffer::ReadByte()
{
    WON_STUB();
    return 0;
}

void WONAPI::ReadBuffer::ReadWString(std::wstring&, int)
{
    WON_STUB();
}

void WONAPI::ReadBuffer::SetData(void const*, unsigned long)
{
    WON_STUB();
}

void const* WONAPI::ReadBuffer::ReadBytes(unsigned long)
{
    WON_STUB();
    return NULL;
}

WONAPI::RecvBytesOp::RecvBytesOp(unsigned long, WONAPI::AsyncSocket* inSocket) : SocketOp(inSocket)
{
    WON_STUB();
}

long WONAPI::RecvBytesOp::Virtual0C()
{
    WON_STUB();
    return 0;
}

long WONAPI::RecvBytesOp::Virtual10()
{
    WON_STUB();
    return 0;
}

void WONAPI::RecvBytesOp::Fn2B4F90(WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*, long)
{
    WON_STUB();
}

void WONAPI::RecvBytesOp::Virtual08()
{
    WON_STUB();
}

WONAPI::ResourceCollection::ResourceCollection()
{
    WON_STUB();
}

WONAPI::ResourceConfigTable* WONAPI::ResourceCollection::GetTable(std::string const&, bool)
{
    WON_STUB();
    return NULL;
}

void WONAPI::ResourceCollection::AddComponentType(std::string const&, WONAPI::RefCount* (*)(), WONAPI::RefCount* (*)())
{
    WON_STUB();
}

void WONAPI::ResourceCollection::AddLoadFunc(void (*)(WONAPI::ResourceConfigTable*))
{
    WON_STUB();
}

WONAPI::GUIString const& WONAPI::ResourceConfigTable::GetString(std::string const&)
{
    WON_STUB();
    static WONAPI::GUIString sNone;
    return sNone;
}

WONAPI::ResourceConfigTable* WONAPI::ResourceConfigTable::GetTable(std::string const&)
{
    WON_STUB();
    return NULL;
}

void WONAPI::ResourceConfigTable::AddSound(std::string const&, WONAPI::Sound*)
{
    WON_STUB();
}

void WONAPI::RootContainer::ClosePopup(WONAPI::Component*)
{
    WON_STUB();
}

void WONAPI::RootContainer::Fn32B0A0(int, WONAPI::Component*)
{
    WON_STUB();
}

void WONAPI::RootContainer::OpenPopup(WONAPI::PopupParams&)
{
    WON_STUB();
}

WONAPI::RoutingConnection::RoutingConnection()
{
    WON_STUB();
}

WONAPI::SmartPtr<WONAPI::RoutingClientInfo> WONAPI::RoutingConnection::GetClient(unsigned short)
{
    WON_TRAP();
}

WONAPI::SmartPtr<WONAPI::RoutingGroupInfo> WONAPI::RoutingConnection::GetGroup(unsigned short)
{
    WON_TRAP();
}

void WONAPI::RoutingConnection::CancelJoinGroup(unsigned short, bool, bool)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::Close()
{
    WON_STUB();
}

void WONAPI::RoutingConnection::Connect(WONAPI::IPAddr const&, long)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::CreateDataObject(bool, unsigned short, std::string const&, std::wstring const&, WONAPI::ByteBuffer*, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::CreateGroup(unsigned short, std::wstring const&, std::wstring const&, unsigned short, unsigned long, bool, unsigned long, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::FindClient(std::wstring const&, WONAPI::SmartPtr<WONAPI::RoutingClientInfo>&)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::Fn2AB0A0(void*, long)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::Fn2BF0C0(unsigned short, std::string const&, bool, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::Fn2BF2E0(unsigned short, std::string const&, bool, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::GetClientList(WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::GetGroupList(unsigned char, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::GetMembersOfGroup(unsigned short, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::IgnoreClient(WONAPI::SmartPtr<WONAPI::RoutingClientInfo>&, bool)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::JoinGroup(unsigned short, std::wstring const&, std::wstring const&, bool, bool, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::LeaveGroup(unsigned short, bool)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::ModifyDataObject(unsigned short, std::string const&, std::wstring const&, unsigned long, bool, WONAPI::ByteBuffer*, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::ReadDataObject(unsigned short, std::string const&, bool, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::Register(std::wstring const&, std::wstring const&, unsigned long, unsigned long, unsigned long, unsigned long, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::ReplaceDataObject(unsigned short, std::string const&, std::wstring const&, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::SendChat(std::wstring const&, unsigned long, unsigned short, WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::SendChatTo(unsigned long, std::wstring const&, std::list<unsigned short,std::allocator<unsigned short> > const&, bool)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::SetCloseCompletion(WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::SetConnectCompletion(WONAPI::CompletionBase<WONAPI::SmartPtr<WONAPI::AsyncOp> >*)
{
    WON_STUB();
}

void WONAPI::RoutingConnection::SetProductName(std::string const&)
{
    WON_STUB();
}

void WONAPI::ServerContext::AddAddress(WONAPI::IPAddr const&)
{
    WON_STUB();
}

WONAPI::ServerDetailsItem::ServerDetailsItem()
{
    WON_STUB();
}

WONAPI::ServerDetailsItem::~ServerDetailsItem()
{
}

void WONAPI::SocketOp::Virtual08()
{
    WON_STUB();
}

WONAPI::Sound::Sound(WONAPI::SoundDescriptor const&)
{
    WON_STUB();
}

void WONAPI::Sound::Play()
{
    WON_STUB();
}

void WONAPI::Sound::Stop()
{
    WON_STUB();
}

void WONAPI::Sound::Virtual0C()
{
    WON_STUB();
}

void WONAPI::Sound::Virtual10()
{
    WON_STUB();
}

WONAPI::SoundDecoder::SoundDecoder()
{
    WON_STUB();
}

WONAPI::SoundDecoder::~SoundDecoder()
{
}

WONAPI::SoundDescriptor::SoundDescriptor(std::string const&, long)
{
    WON_STUB();
}

void WONAPI::TextListItem::Virtual04()
{
    WON_STUB();
}

void WONAPI::TextListItem::Virtual08()
{
    WON_STUB();
}

void WONAPI::TextListItem::Virtual10()
{
    WON_STUB();
}

WONAPI::WONAPICore::~WONAPICore()
{
}

void WONAPI::WONAPICore::Virtual04()
{
    WON_STUB();
}

void WONAPI::WONAPICore::Virtual08()
{
    WON_STUB();
}

WONAPI::WONAPICoreEx::WONAPICoreEx()
{
    WON_STUB();
}

void WONAPI::WONAPICoreEx::Fn2F2F90(bool)
{
    WON_STUB();
}

bool WONAPI::WONAccountData::Fn2CDAE0(bool)
{
    WON_STUB();
    return false;
}

void WONAPI::WONAccountData::Fn2CD9E0(bool, bool)
{
    WON_STUB();
}

void WONAPI::WONAccountData::Fn2CDC20()
{
    WON_STUB();
}

WONAPI::WONAccountOp::WONAccountOp(WONAPI::ServerContext*, EEWonNetUser*)
{
    WON_STUB();
}

WONAPI::WONFile::WONFile(std::string const&)
{
    WON_STUB();
}

bool WONAPI::WONFile::Delete(bool)
{
    WON_STUB();
    return false;
}

std::string const& WONAPI::WONFile::GetFileName()
{
    WON_STUB();
    static std::string sNone;
    return sNone;
}

unsigned long WONAPI::WONFile::GetSize()
{
    WON_STUB();
    return 0;
}

WONAPI::WONGetAccountOp::WONGetAccountOp(WONAPI::ServerContext*, EEWonNetUser*)
{
    WON_STUB();
}

void WONAPI::WONLobbyHost::Fn2F2D20(long, long)
{
    WON_STUB();
}

void WONAPI::WONLobbyWindow::Close(bool)
{
    WON_STUB();
}

WONAPI::WONRectangle::WONRectangle()
{
    WON_STUB();
}

WONAPI::WONRectangle::WONRectangle(int, int, int, int)
{
    WON_STUB();
}

bool WONAPI::Window::Virtual4C()
{
    WON_STUB();
    return false;
}

void WONAPI::Window::Create(WONAPI::WONRectangle const&)
{
    WON_STUB();
}

void WONAPI::Window::Fn32A1E0()
{
    WON_STUB();
}

void WONAPI::Window::GetRect(WONAPI::WONRectangle&, bool)
{
    WON_STUB();
}

void WONAPI::Window::PostEvent(WONAPI::WindowEventBase*, bool)
{
    WON_STUB();
}

void WONAPI::Window::SetIcon(WONAPI::NativeIcon*)
{
    WON_STUB();
}

void WONAPI::Window::SetPosSize(int, int, int, int)
{
    WON_STUB();
}

void WONAPI::Window::SetTitle(WONAPI::GUIString const&)
{
    WON_STUB();
}

void WONAPI::Window::Show(bool)
{
    WON_STUB();
}

void WONAPI::Window::Virtual04()
{
    WON_STUB();
}

void WONAPI::Window::Virtual08()
{
    WON_STUB();
}

void WONAPI::Window::Virtual0C()
{
    WON_STUB();
}

void WONAPI::Window::Virtual10()
{
    WON_STUB();
}

void WONAPI::Window::Virtual14()
{
    WON_STUB();
}

void WONAPI::Window::Virtual18()
{
    WON_STUB();
}

void WONAPI::Window::Virtual1C()
{
    WON_STUB();
}

void WONAPI::Window::Virtual20()
{
    WON_STUB();
}

void WONAPI::Window::Virtual24()
{
    WON_STUB();
}

void WONAPI::Window::Virtual28()
{
    WON_STUB();
}

void WONAPI::Window::Virtual2C()
{
    WON_STUB();
}

void WONAPI::Window::Virtual3C()
{
    WON_STUB();
}

void WONAPI::Window::Virtual44()
{
    WON_STUB();
}

void WONAPI::Window::Virtual48()
{
    WON_STUB();
}

void WONAPI::WindowManager::Fn32D5B0()
{
    WON_STUB();
}

void WONAPI::WindowManager::SetSoundDecoder(WONAPI::SoundDecoder*)
{
    WON_STUB();
}

WONAPI::SmartPtr<WONAPI::ByteBuffer> WONAPI::WriteBuffer::ToByteBuffer(bool)
{
    WON_TRAP();
}

WONAPI::WriteBuffer::WriteBuffer(unsigned char, bool)
{
    WON_STUB();
}

WONAPI::WriteBuffer::~WriteBuffer()
{
}

void WONAPI::WriteBuffer::AppendBool(bool)
{
    WON_STUB();
}

void WONAPI::WriteBuffer::AppendByte(unsigned char)
{
    WON_STUB();
}

void WONAPI::WriteBuffer::AppendLong(long)
{
    WON_STUB();
}

void WONAPI::WriteBuffer::AppendShort(short)
{
    WON_STUB();
}

void WONAPI::WriteBuffer::AppendWString(std::wstring const&, int)
{
    WON_STUB();
}

void WONAPI::WriteBuffer::SetByte(unsigned long, unsigned char)
{
    WON_STUB();
}

WONAPI::Component* WONAPI::ComponentConfigGet(WONAPI::ComponentConfig*, char const*)
{
    WON_STUB();
    return NULL;
}

WONAPI::Container* WONAPI::LobbyFn362250()
{
    WON_STUB();
    return NULL;
}

WONAPI::GUIString WONAPI::LobbyFormatString(WONAPI::GUIString const&, WONAPI::GUIString const&)
{
    WON_TRAP();
}

WONAPI::GUIStringData* WONAPI::GetEmptyGUIStringData()
{
    WON_STUB();
    return NULL;
}

WONAPI::LobbyClient* WONAPI::LobbyFn361370()
{
    WON_STUB();
    return NULL;
}

WONAPI::LobbyConfig* WONAPI::GetLobbyConfig()
{
    WON_STUB();
    return NULL;
}

WONAPI::LobbyGame* WONAPI::GetMyLobbyGame()
{
    WON_STUB();
    return NULL;
}

WONAPI::LobbyPlayer* WONAPI::GetMyLobbyPlayer()
{
    WON_STUB();
    return NULL;
}

WONAPI::LobbyRoomClients* WONAPI::LobbyFn35B6F0()
{
    WON_STUB();
    return NULL;
}

WONAPI::ServerColumnHandlers* WONAPI::GetServerColumnHandlers(WONAPI::GUIString const&)
{
    WON_STUB();
    return NULL;
}

WONAPI::Window* WONAPI::GetLobbyWindow()
{
    WON_STUB();
    return NULL;
}

bool WONAPI::LobbyFn3613C0()
{
    WON_STUB();
    return false;
}

bool WONAPI::LobbyFn361B20()
{
    WON_STUB();
    return false;
}

char const* WONAPI::WONStatusToString(long)
{
    WON_STUB();
    return NULL;
}

int WONAPI::LobbyDoDialog(WONAPI::Dialog*)
{
    WON_STUB();
    return 0;
}

int WONAPI::LobbyFn362760()
{
    WON_STUB();
    return 0;
}

int WONAPI::LobbyMessageBoxYesNo(WONAPI::GUIString const&, WONAPI::GUIString const&, bool)
{
    WON_STUB();
    return 0;
}

std::string WONAPI::WStringToString(std::wstring const&)
{
    WON_TRAP();
}

std::string WONAPI::WStringToString(std::wstring const&, int)
{
    WON_TRAP();
}

std::wstring WONAPI::StringToWString(std::string const&, int)
{
    WON_TRAP();
}

void WONAPI::ComponentConfigMissing(std::string const&, char const*, int)
{
    WON_STUB();
}

void WONAPI::LobbyAddComponentLoader(WONAPI::ResourceCollection*, void (*)(WONAPI::ComponentConfig*))
{
    WON_STUB();
}

void WONAPI::LobbyAddGameOption(WONAPI::GUIString const&, WONAPI::GUIString const&, bool)
{
    WON_STUB();
}

void WONAPI::LobbyDraw3DFrame(WONAPI::Graphics&, int, int, int, int, bool)
{
    WON_STUB();
}

void WONAPI::LobbyDrawFrame(WONAPI::Graphics&, int, int, int, int, bool)
{
    WON_STUB();
}

void WONAPI::LobbyFn359100(WONAPI::ComponentEvent*, bool)
{
    WON_STUB();
}

void WONAPI::LobbyFn359120(int, bool)
{
    WON_STUB();
}

void WONAPI::LobbyFn361260()
{
    WON_STUB();
}

void WONAPI::LobbyFn361290()
{
    WON_STUB();
}

void WONAPI::LobbyFn3612B0(int)
{
    WON_STUB();
}

void WONAPI::LobbyFn361310(int)
{
    WON_STUB();
}

void WONAPI::LobbyFn3613E0()
{
    WON_STUB();
}

void WONAPI::LobbyFn361B60(WONAPI::ByteBuffer*)
{
    WON_STUB();
}

void WONAPI::LobbyFn361B80(WONAPI::ByteBuffer*)
{
    WON_STUB();
}

void WONAPI::LobbyFn361BA0(WONAPI::LobbyClient*, WONAPI::ByteBuffer*)
{
    WON_STUB();
}

void WONAPI::LobbyFn361BC0(WONAPI::LobbyClient*, bool)
{
    WON_STUB();
}

void WONAPI::LobbyFn362170()
{
    WON_STUB();
}

void WONAPI::LobbyFn362740()
{
    WON_STUB();
}

void WONAPI::LobbyFn362830()
{
    WON_STUB();
}

void WONAPI::LobbyFn362880()
{
    WON_STUB();
}

void WONAPI::LobbyFn3D08C0(HWND__*)
{
    WON_STUB();
}

void WONAPI::LobbyFn3D24E0(WONAPI::MultiImageDecoder*)
{
    WON_STUB();
}

void WONAPI::LobbyFn3D2A20(WONAPI::MultiImageDecoder*)
{
    WON_STUB();
}

void WONAPI::LobbyGetNetAddresses(std::list<WONAPI::LobbyNetAddress,std::allocator<WONAPI::LobbyNetAddress> >&, bool)
{
    WON_STUB();
}

void WONAPI::LobbyMessageBox(WONAPI::GUIString const&, WONAPI::GUIString const&)
{
    WON_STUB();
}

void WONAPI::LobbySetProductName(std::wstring const&, int)
{
    WON_STUB();
}
