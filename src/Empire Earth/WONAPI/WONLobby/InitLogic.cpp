// InitLogic: Internet lobby start-up (DNS, directory, version and firewall checks, terms of use, advert)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyAccountLogic.h"
#include "ee/wonapi/WONStatus.h"
using namespace WONAPI;
// Timeout for the firewall detect and accept operations, in milliseconds.
unsigned long WONAPI::gLobbyAcceptTimeout = 20000;

// The three controls the login screen disables while it initialises.
struct LoginControlIds : public ControlIdSet {
    void Add(value_type inA,value_type inB,value_type inC) { insert(inA); insert(inB); insert(inC); }
};

namespace WONAPI {
// FUNCTION: WON 0x7dd100
// Creates the lobby start-up logic with its own HTTP session
InitLogic::InitLogic()
{
    this->mHTTPSession = new HTTPSession;
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dd200
// Destroys the start-up logic (members only)
InitLogic::~InitLogic() {}
}

namespace WONAPI {
// FUNCTION: WON 0x7dd340
// Looks up the choose-network-adapter dialog from the lobby layout
void InitLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mNetworkAdapterDialog, "ChooseNetworkAdapterDialog", __FILE__, 49);
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dd370
// Shows the network adapter dialog and, on OK, uses the chosen adapter's address for the lobby
void InitLogic::ChooseNetworkAdapter()
{
    this->mNetworkAdapterDialog->Reset();
    if (LobbyDoDialog(this->mNetworkAdapterDialog.get()) == 100001)
        LobbySetNetworkAdapter(this->mNetworkAdapterDialog->GetSelectedAddress());
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dd3b0
// Keeps the start-up errors the player has not ignored and broadcasts an init-failed status; true if any remain
// (the player may ignore them only when they are no-Internet or account-server errors)
bool InitLogic::SetError(int inError, const GUIString& inDescription)
{
    this->mErrors = inError & ~mIgnoredErrors;
    if (this->mErrors != 0) {
        bool allowIgnore = (this->mErrors & 5) == this->mErrors;
        LobbyBroadcastEvent(new LoginInitStatusEvent(::LobbyContainer_InitFailed_String, inDescription, allowIgnore, 3), true);
        return true;
    }
    return false;
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dd570
// Starts the lobby checks: tests the Internet connection, clears the server lists on a first try, runs DNS
void InitLogic::Start(bool inRetry)
{
    LobbySetVersionStatus(WS_None);
    this->mErrors = 0;
    LobbyConfig* config = GetLobbyConfig();
    if (config != NULL && config->mCheckForInternet && !AsyncSocket::HasInternetConnection()) {
        if (this->SetError(1, ::LobbyContainer_NoConnection_String))
            return;
    }
    if (!inRetry) {
        ServerContext* account = LobbyGetAccountServers();
        ServerContext* auth = LobbyGetAuthServers();
        ServerContext* firewall = LobbyGetFirewallDetectServers();
        ServerContext* routing = LobbyGetPatchServers();
        if (account != NULL) account->Clear();
        if (auth != NULL) auth->Clear();
        if (firewall != NULL) firewall->Clear();
        if (routing != NULL) routing->Clear();
    }
    this->StartDNSTest();
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dd630
// Loads settings, binds the UDP port, disables the login buttons, starts or stops lobby music, starts the checks
void InitLogic::Init()
{
    this->mIgnoredErrors = 0;
    LobbyLoadGlobalPrefs();
    IPAddr::SetLocalHost(LobbyGetNetworkAdapter(), true);
    this->LoadTOUTimes();
    LobbyBindUDPPort();
    LobbyBroadcastLobbyEvent(100052, true);
    Container* aRoot = GetLobbyRoot();
    if(aRoot == NULL)
        return;

    LoginControlIds anIds;
    anIds.Add(200001,200002,200003);
    aRoot->EnableControlsById(anIds, false);
    ::LobbyGlobal_LobbyMusic_Sound->SetLooping(true);
    if(LobbyGetLobbyMusic())
        ::LobbyGlobal_LobbyMusic_Sound->Play();
    else
        ::LobbyGlobal_LobbyMusic_Sound->Stop();
    this->Start(false);
}

}

namespace WONAPI {
// Any of the four lookups may return NULL; nothing happens then
// FUNCTION: WON 0x7dd810
// After a good DNS test, fills the directory, room directory and auth server lists and starts the next queries
void InitLogic::DNSCompletion(AsyncOp* inOp)
{
    LobbyConfig* config = GetLobbyConfig();
    ServerContext* dir = LobbyGetDirServers();
    ServerContext* roomDir = LobbyGetRoomDirServers();
    ServerContext* auth = LobbyGetAuthServers();
    if (config == NULL || dir == NULL || roomDir == NULL || auth == NULL)
        return;
    if (inOp->GetStatus() == WS_Success) {
        // The original builds this address with mRememberHostString set and destroys it only after both
        // queries are started; it is separate from the per-entry addresses in the loops
        IPAddr address;
        address.mRememberHostString = true;
        dir->RemoveAllAddresses();
        std::list<std::string>::iterator it = config->mDirServers.begin();
        while (true) {
            if (it == config->mDirServers.end())
                break;
            {
                IPAddr anAddr(*it);
                dir->AddAddress(anAddr);
            }
            ++it;
        }
        it = config->mRoomDirServers.begin();
        while (true) {
            if (it == config->mRoomDirServers.end())
                break;
            {
                IPAddr anAddr(*it);
                roomDir->AddAddress(anAddr);
            }
            ++it;
        }
        it = config->mAuthServers.begin();
        while (true) {
            if (it == config->mAuthServers.end())
                break;
            {
                IPAddr anAddr(*it);
                auth->AddAddress(anAddr);
            }
            ++it;
        }
        if (roomDir->GetNumAddresses() < 1)
            LobbyResetRoomDirServers();
        this->StartHTTPDocuments();
        this->StartDirectoryQuery();
    } else {
        this->SetError(2, ::LobbyContainer_DNSLookupFailed_String);
    }
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dda10
// Decodes the downloaded advert image and broadcasts it with the advert's link
void InitLogic::CrossPromotionImageCompletion(AsyncOp* inOp)
{
    if (inOp->mStatus != WS_Success)
        return;
    HTTPGetOp* op = static_cast<HTTPGetOp*>(inOp);
    SmartPtr<NativeImage> image = Window::GetDefaultWindow()->DecodeImage(op->GetLocalPath().c_str());
    LobbyBroadcastEvent(new SetCrossPromotionEvent(image.get(),this->mCrossPromotion),true);
}
}

namespace WONAPI {
// FUNCTION: WON 0x7ddc00
// Reads the 'link' and 'image' lines of the advert document and downloads the image
void InitLogic::ParseCrossPromotion(const ByteBuffer* inDocument)
{
    WriteBuffer storage(0,true);
    storage.AppendBytes(inDocument != NULL ? inDocument->data() : NULL,
                        inDocument != NULL ? inDocument->length() : 0);
    storage.AppendByte(0);
    char* token = storage.mData;
    char delimiters[] = " \t\n=";
    std::string imageURL("");
    this->mCrossPromotion = "";
    token = strtok(token,delimiters);
    while (token != NULL) {
        if (_stricmp(token,"link") == 0) {
            token = strtok(NULL,"\n");
            if (token == NULL)
                break;
            this->mCrossPromotion = token;
        } else if (_stricmp(token,"image") == 0) {
            token = strtok(NULL,"\n");
            if (token == NULL)
                break;
            imageURL = token;
        }
        if (token == NULL)
            break;
        token = strtok(NULL,delimiters);
    }
    if (!imageURL.empty()) {
        SmartPtr<HTTPGetOp> op = new HTTPGetOp(imageURL);
        op->SetHTTPCache(HTTPCache::GetGlobalCache());
        op->mCompletion = new LogicCompletion<InitLogic>(&InitLogic::CrossPromotionImageCompletion,this);
        this->mHTTPSession->AddOp(op.get());
    }
}


}

namespace WONAPI {
// FUNCTION: WON 0x7ddef0
// Records the modification times of the saved system and game terms-of-use files
void InitLogic::LoadTOUTimes()
{
    WONFile file(LobbyGetSysTOUPath());
    long systemTime = file.GetModifyTime();
    file.SetFilePath(LobbyGetGameTOUPath());
    long gameTime = file.GetModifyTime();
    LobbySetTOUTimes(systemTime, gameTime);
}
}

namespace WONAPI {
// Each of the two file writes has its own FileWriterException handler
// FUNCTION: WON 0x7de090
// Saves newer system and game terms-of-use documents to disk and re-reads their times
void InitLogic::HandleTOU(GetHTTPDocumentOp* inOp)
{
    long systemTime;
    long gameTime;
    LobbyGetTOUTimes(systemTime, gameTime);
    bool changed = false;
    if (inOp->GetDocumentStatus(HTTPDocOwner_Sys) == 0 && inOp->GetModifiedTime(HTTPDocOwner_Sys) > systemTime) {
        FileWriter writer;
        try {
            if (writer.Open(LobbyGetSysTOUPath().c_str(), true)) {
                ByteBufferPtr document = inOp->GetDocument(HTTPDocOwner_Sys);
                if (document.get() != NULL && document->length() > 1)
                    writer.WriteBytes(document->data(), document->length() - 1);
            }
            // Original sets this even if Open returns false, but not on an exception.
            changed = true;
        } catch (FileWriterException&) {}
    }
    if (inOp->GetDocumentStatus(HTTPDocOwner_Game) == 0 && inOp->GetModifiedTime(HTTPDocOwner_Game) > gameTime) {
        FileWriter writer;
        try {
            if (writer.Open(LobbyGetGameTOUPath().c_str(), true)) {
                ByteBufferPtr document = inOp->GetDocument(HTTPDocOwner_Game);
                if (document.get() != NULL && document->length() > 1)
                    writer.WriteBytes(document->data(), document->length() - 1);
            }
            changed = true;
        } catch (FileWriterException&) {}
    }
    if (changed)
        this->LoadTOUTimes();
}
}

namespace WONAPI {
// The document event is always broadcast before the document type is looked at
// FUNCTION: WON 0x7de280
// Broadcasts a downloaded document, then saves terms of use or parses the advert (clearing an empty advert)
void InitLogic::HTTPDocumentCompletion(AsyncOp* inOp)
{
    GetHTTPDocumentOp* op = static_cast<GetHTTPDocumentOp*>(inOp);
    LobbyBroadcastEvent(new GotHTTPDocumentEvent(op), true);
    switch (op->mDocumentType) {
    case HTTPDocType_TOU:
        this->HandleTOU(op);
        break;
    case HTTPDocType_ADV:
        {
            ByteBufferPtr document = op->GetDocument(HTTPDocOwner_Game);
            if (document.get() != NULL && document->length() > 1) {
                this->ParseCrossPromotion(document.get());
                return;
            }
            LobbyBroadcastEvent(new SetCrossPromotionEvent(NULL, std::string("")), true);
            return;
        }
    }
}


#include "ee/wonapi/EELobbySound.h"
}

namespace WONAPI {
// FUNCTION: WON 0x007de4c0
// Fills the auth, account, firewall and version server lists from the directory; reports missing ones,
// else starts the version check and firewall test
void InitLogic::DirectoryCompletion(AsyncOp* inOp)
{
    ServerContext* account = LobbyGetAccountServers();
    ServerContext* auth = LobbyGetAuthServers();
    ServerContext* firewall = LobbyGetFirewallDetectServers();
    ServerContext* versions = LobbyGetPatchServers();
    ServerContext* directory = LobbyGetDirServers();
    LobbyConfig* config = GetLobbyConfig();
    if (config == NULL || account == NULL || auth == NULL ||
        firewall == NULL || directory == NULL)
        return;
    WONStatus status = inOp->GetStatus();
    if (status == WS_Success || status == WS_DirServ_MultiGetPartialFailure) {
    GetMultiDirOp* op = static_cast<GetMultiDirOp*>(inOp);
    const GetEntityRequestList& requests = op->GetRequestList();
    GetEntityRequestList::const_iterator i = requests.begin();
    int index = 0;
    for (; i != requests.end(); ++i,++index) {
        GetEntityRequest* request = i->get();
        if (request->GetStatus() != 0)
            continue;
        ServerContext* selected = NULL;
        switch (index) {
        case 0:
            if (config->mAuthServers.empty())
                selected = auth;
            break;
        case 1: selected = account; break;
        case 2: selected = firewall; break;
        case 3: selected = versions; break;
        }
        if (selected != NULL)
            selected->AddAddressesFromDir(request->GetDirEntityList(),NULL);
    }
    if (auth->GetNumAddresses() == 0 || account->GetNumAddresses() == 0 ||
        versions->GetNumAddresses() == 0) {
        directory->NotifyFailed(op->GetCurAddr());
        GUIString names;
        int errors = 0;
        if (auth->GetNumAddresses() == 0) {
            names.append(::LobbyContainer_AuthServers_String);
            errors = 8;
        }
        if (account->GetNumAddresses() == 0) {
            if (names.length() != 0)
                names.append(GUIString(", "));
            names.append(::LobbyContainer_AccountServers_String);
            errors |= 4;
        }
        if (versions->GetNumAddresses() == 0) {
            if (names.length() != 0)
                names.append(GUIString(", "));
            names.append(::LobbyContainer_VersionServers_String);
            errors |= 0x10;
        }
        GUIString description(::LobbyContainer_NoServers_String);
        description.append(names);
        if (this->SetError(errors,description))
            return;
    }
    this->StartVersionCheck();
    this->StartFirewallTest();
    }
    else {
        GUIString description = LobbyFormatString(::LobbyContainer_DirLookupFailure_String,GetWONStatusString(status));
        this->SetError(0x20,description);
    }
}


}

namespace WONAPI {
// FUNCTION: WON 0x007de8f0
// Records the version check result; re-enables the login buttons when the version is usable, else reports it
void InitLogic::VersionCompletion(AsyncOp* inOp)
{
    Container* root = GetLobbyRoot();
    if (root == NULL)
        return;
    WONStatus status = inOp->GetStatus();
    LobbySetVersionStatus(status);
    if (status != WS_Success && status != WS_DBProxyServ_ValidNotLatest && status != WS_DBProxyServ_OutOfDate) {
        if (status == WS_DBProxyServ_OutOfDateNoUpdate || status == WS_DBProxyServ_DBError) {
            ServerContext* versions = LobbyGetPatchServers();
            if (versions != NULL)
                versions->NotifyFailed(static_cast<CheckValidVersionOp*>(inOp)->GetCurAddr());
        }
        GUIString description = LobbyFormatString(::LobbyContainer_VersionCheckFailure_String,GetWONStatusString(status));
        this->SetError(0x40,description);
        return;
    }
    LoginControlIds ids;
    ids.Add(200001,200002,200003);
    root->EnableControlsById(ids,true);
    LobbyBroadcastEvent(new LoginInitStatusEvent(::LobbyContainer_InitComplete_String,gEmptyGUIString,false,2),true);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007deb50
// Ends the firewall test: no connection means behind a firewall, one from the firewall server means not
void InitLogic::AcceptCompletion(AsyncOp* inOp)
{
    AcceptOp* op = this->mAcceptOp.get();
    if (inOp != op)
        return;
    this->mAcceptOp = NULL;
    if (op->GetStatus() != WS_Success) {
        LobbySetBehindFirewall(true);
        LobbyBroadcastLobbyEvent(100055,true);
        return;
    }
    if (op->mAcceptedSocket->GetDestAddr().GetHost() == this->mFirewallAddress.GetHost()) {
        LobbySetBehindFirewall(false);
        LobbyBroadcastLobbyEvent(100055,true);
        return;
    }
    this->mAcceptOp = new AcceptOp(op->mSocket.get());
    this->RunOp(this->mAcceptOp.get(),new LogicCompletion<InitLogic>(&InitLogic::AcceptCompletion,this),
          gLobbyAcceptTimeout);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007ded20
// Records our public address and waits for the firewall server to connect back; failure means behind a firewall
void InitLogic::FirewallCompletion(AsyncOp* inOp)
{
    DetectFirewallOp* op = static_cast<DetectFirewallOp*>(inOp);
    if (op->mStatus == WS_Success) {
        LobbySetPublicAddress(op->GetPublicAddr());
        if (this->mFirewallSocket.get() != NULL) {
            this->mFirewallAddress = op->GetCurAddr();
            this->mAcceptOp = new AcceptOp(this->mFirewallSocket.get());
            this->RunOp(this->mAcceptOp.get(),new LogicCompletion<InitLogic>(&InitLogic::AcceptCompletion,this),
                  gLobbyAcceptTimeout);
            this->mFirewallSocket = NULL;
        }
    } else {
        LobbySetBehindFirewall(true);
    }
    LobbyBroadcastLobbyEvent(100055,true);
}
}

namespace WONAPI {
// FUNCTION: WON 0x7def30
// Broadcasts the start status and tests DNS on the first directory server
void InitLogic::StartDNSTest()
{
    LobbyConfig* config = GetLobbyConfig();
    if (config == NULL || config->mDirServers.empty())
        return;
    LobbyBroadcastEvent(new LoginInitStatusEvent(::LobbyContainer_InitStart_String, gEmptyGUIString, false, 1), true);
    SmartPtr<DNSTestOp> op = new DNSTestOp(config->mDirServers.front());
    this->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::DNSCompletion, this), 0);
}
}

namespace WONAPI {
// All three op pointers live until return, as in the original's cleanup order
// FUNCTION: WON 0x7df0c0
// Requests the message of the day, terms of use and advert documents for the product and language
void InitLogic::StartHTTPDocuments()
{
    LobbyConfig* config = GetLobbyConfig();
    if (config == NULL)
        return;
    SmartPtr<GetHTTPDocumentOp> motd = new GetHTTPDocumentOp(config->mProductName, HTTPDocType_MOTD);
    motd->SetExtraConfig(config->mLanguage);
    motd->SetHTTPSession(this->mHTTPSession.get());
    this->RunTrackedOp(motd.get(), new LogicCompletion<InitLogic>(&InitLogic::HTTPDocumentCompletion, this), 0);
    SmartPtr<GetHTTPDocumentOp> tou = new GetHTTPDocumentOp(config->mProductName, HTTPDocType_TOU);
    tou->mPath = config->mLanguage;
    tou->mSession = this->mHTTPSession.get();
    this->RunTrackedOp(tou.get(), new LogicCompletion<InitLogic>(&InitLogic::HTTPDocumentCompletion, this), 0);
    SmartPtr<GetHTTPDocumentOp> adv = new GetHTTPDocumentOp(config->mProductName, HTTPDocType_ADV);
    adv->SetExtraConfig(config->mLanguage);
    adv->mSession = this->mHTTPSession.get();
    this->RunTrackedOp(adv.get(), new LogicCompletion<InitLogic>(&InitLogic::HTTPDocumentCompletion, this), 0);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007df410
// Broadcasts 'querying servers' and asks the directory for the auth, database, firewall and patch servers
void InitLogic::StartDirectoryQuery()
{
    ServerContext* context = LobbyGetDirServers();
    if (context == NULL)
        return;
    LobbyBroadcastEvent(new LoginInitStatusEvent(::LobbyContainer_QueryingServers_String, gEmptyGUIString, false, 0), true);
    SmartPtr<GetMultiDirOp> op = new GetMultiDirOp(context);
    op->SetFlags(0x04000002);
    op->AddPath(L"/TitanServers/Auth");
    op->AddPath(L"/TitanServers/DBProxy");
    op->AddPath(L"/TitanServers/Firewall");
    LobbyConfig* config = GetLobbyConfig();
    if (config != NULL)
        op->AddPath(config->mProductDirectory + L"/Patch");
    this->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::DirectoryCompletion, this), 0);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007df6c0
// Broadcasts 'checking version' and asks the version servers whether this game version is valid
void InitLogic::StartVersionCheck()
{
    ServerContext* versions = LobbyGetPatchServers();
    LobbyConfig* config = GetLobbyConfig();
    if (versions == NULL || config == NULL)
        return;
    LobbyBroadcastEvent(new LoginInitStatusEvent(::LobbyContainer_CheckingVersion_String,gEmptyGUIString,false,0),true);
    SmartPtr<CheckValidVersionOp> op = new CheckValidVersionOp(config->mProductName,versions);
    op->SetConfigName(config->mLanguage);
    op->SetVersion(config->mVersion);
    this->RunTrackedOp(op.get(),new LogicCompletion<InitLogic>(&InitLogic::VersionCompletion,this),0);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007df880
// Starts firewall detection on the lobby port and opens a listening socket for the server's connect-back
void InitLogic::StartFirewallTest()
{
    ServerContext* firewall = LobbyGetFirewallDetectServers();
    if (firewall == NULL)
        return;
    this->KillFirewallTest();
    LobbyResetFirewallTest();
    SmartPtr<DetectFirewallOp> op = new DetectFirewallOp(firewall);
    op->SetListenPort(LobbyGetLobbyPort());
    op->SetDoListen(false);
    op->SetWaitForConnect(false);
    this->RunTrackedOp(op.get(),new LogicCompletion<InitLogic>(&InitLogic::FirewallCompletion,this),0);
    LobbyBroadcastLobbyEvent(100055,true);
    this->mFirewallSocket = new AsyncSocket(0);
    WONStatus aStatus = this->mFirewallSocket->Fn2B0D90(LobbyGetLobbyPort(),0);
    if (aStatus == 0) {
        aStatus = this->mFirewallSocket->Listen(1000);
        if (aStatus == 0)
            return;
    }
    this->mFirewallSocket = NULL;
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dfa80
// Stops waiting for a firewall connect-back and closes its sockets
void InitLogic::KillFirewallTest()
{
    if (this->mAcceptOp.get() != NULL) {
        this->mAcceptOp->Kill(true);
        if (this->mAcceptOp->mSocket.get() != NULL)
            this->mAcceptOp->mSocket->Close();
        this->mAcceptOp = NULL;
    }
    this->mFirewallSocket = NULL;
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dfb00
// Cancels every HTTP download and the firewall test
void InitLogic::KillHook()
{
    this->mHTTPSession->KillAll();
    this->KillFirewallTest();
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dfb20
// Handles the login screen's Back, Internet and choose-network-adapter buttons
void InitLogic::HandleControlEvent(int inControlId)
{
    switch (inControlId) {
    case ID_LoginScreenBack: LobbyExit(0); break;
    case ID_ShowInternetScreen: LobbyShowInternetScreen(); break;
    case ID_ChooseNetworkAdapter: this->ChooseNetworkAdapter(); break;
    }
}
}

namespace WONAPI {
// FUNCTION: WON 0x7dfb50
// Retries start-up (event 100045), or ignores the current errors and retries (event 100025)
void InitLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType != 100045) {
        if (inEvent->mType != 100025)
            return;
        this->mIgnoredErrors |= this->mErrors;
    }
    this->Start(true);
}
}

// TEMPLATE: WON 0x007f20a0

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dd1e0
// (public: virtual void * __thiscall WONAPI::InitLogic::`scalar deleting destructor'(unsigned int)    [??_GInitLogic@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007df3f0
// (public: __thiscall WONAPI::LogicCompletionBase::LogicCompletionBase(void)    [??0LogicCompletionBase@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007f36d0

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dd4a0
// (public: virtual void * __thiscall WONAPI::LoginInitStatusEvent::`scalar deleting destructor'(unsigned int)    [??_GLoginInitStatusEvent@WONAPI@@UAEPAX)
// TEMPLATE: WON 0x007dd4c0
// (public: virtual __thiscall WONAPI::LoginInitStatusEvent::~LoginInitStatusEvent(void)    [??1LoginInitStatusEvent@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007ddb50
// (public: virtual void * __thiscall WONAPI::SetCrossPromotionEvent::`scalar deleting destructor'(unsigned int)    [??_GSetCrossPromotionEvent@WONAPI@@UA)
// TEMPLATE: WON 0x007ddb70
// (public: virtual __thiscall WONAPI::SetCrossPromotionEvent::~SetCrossPromotionEvent(void)    [??1SetCrossPromotionEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1430
// (public: virtual void * __thiscall WONAPI::LogicCompletion<class WONAPI::InitLogic>::`scalar deleting destructor'(unsigned int)    [??_G?$LogicCompleti)
// TEMPLATE: WON 0x007f1510
// (public: virtual __thiscall WONAPI::LogicCompletion<class WONAPI::InitLogic>::~LogicCompletion<class WONAPI::InitLogic>(void)    [??1?$LogicCompletion@)
