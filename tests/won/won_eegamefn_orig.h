// The EE lobby's network game functions R6 WONLobby round 30 moved to Rust (ee_won_lobby eegamefn.rs), as they
// were (copied by script from EELobbyGlueHost.cpp at 4d64743a6), for won_eewindow_test over won_eewindow_world.h's
// stand-ins (in the WinOrig namespace of won_eewindow_orig.h).
#ifndef WON_EEGAMEFN_ORIG_H
#define WON_EEGAMEFN_ORIG_H
namespace WinOrig {
static unsigned long sEELobbyStartTime;

UWideString& EEGetLocalAddress(NEProtocol* inProtocol, unsigned long inAdapter)
{
    LobbyGame* theGame = NULL;
    if (EEIsMultiplayerGame(true))
        theGame = GetMyLobbyGame();
    if (theGame == NULL)
        return inProtocol->GetLocalAddress(inAdapter);
    static UWideString theAddress;
    theAddress = theGame->GetAddress().GetHostString(true).c_str();
    return theAddress;
}

bool EELobbyHostGame()
{
    EEGameOptionsPlayer theMe;
    gPlayerName = EELobbyToWideString(GetMyLobbyClient()->GetName());
    Glue(GL::SyncNetworkAdapter);
    bool theFlag = LobbyIsBehindFirewall();
    if (gCommunications != NULL) {
        delete gCommunications;
        gCommunications = NULL;
    }
    gCommunications = new EEServer(UWideString("TestGame"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, false);
    static_cast<EEServer*>(gCommunications)->StartThreads();
    if (!static_cast<EEServer*>(gCommunications)->GetGameWasPublished()) {
        delete gCommunications;
        gCommunications = NULL;
        return false;
    }
    Glue(GL::FillGameOptions, (intptr_t)&theMe);
    theMe.mIsReady = true;
    theMe.mType = 1;
    gCommunications->ProcessLocalUserInput(
        new NEMRecord(kNetMessageClientConnectionRequest, {theFlag, gBinaryChecksum}, {gGameVersion, theMe.mName}), 0xff);
    return true;
}

void EELobbyStartHostedGame()
{
    EEGameOptionsPlayer thePlayer;
    unsigned char theHumans = 0;
    gGameOptions.mLock->ReadLock();
    sEELobbyStartTime = TMGetTime();
    EEServer* theServer = static_cast<EEServer*>(gCommunications);
    theServer->AllowNewPlayers(false);
    theServer->SetGameState(2);
    theServer->mNumLoadResponses = false;
    for (int i = 0; i < 16; i++) {
        thePlayer = gWorld.GetLobbyOptions().mPlayers[i];
        if (thePlayer.mType == 1 || thePlayer.mType == 2)
            theHumans++;
    }
    theServer->StopPing();
    theServer->ResetXfer();
    theServer->mLoadState = true;
    theServer->AllowNewPlayers(false);
    srand(TMGetTime());
    gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageStartLoading, {theHumans, rand()}), 0xff);
    gGamePreferences.SetLastGameOptions(gGameOptions);
    gGameOptions.mLock->ReadUnlock();
}

void EELobbyJoinGame()
{
    UString theAddress;
    UString theService;
    UWideString theGameName;
    gPlayerName = EELobbyToWideString(GetMyLobbyClient()->GetName());
    Glue(GL::SyncNetworkAdapter);
    bool theFlag = LobbyIsBehindFirewall();
    if (gCommunications == NULL) {
        gCommunications = new EEClient;
        static_cast<EEClient*>(gCommunications)->StartThreads();
    } else if (gCommunications->IsServer() == true) {
        delete gCommunications;
        gCommunications = new EEClient;
        dynamic_cast<EEClient*>(gCommunications)->StartThreads();
    }
    if (gCommunications) {
        gGameOptions.mLock->WriteLock();
        gGameOptions = gLobbyGameOptions;
        gGameOptions.mIsMultiplayer = true;
        gGameOptions.mCheatCodes = false;
        gGameOptions.mLock->WriteUnlock();
        theService = "EEService";
        theGameName = gEmptyString;
        theAddress = GetMyLobbyGame()->GetAddress().GetHostString(true).c_str();
        UWideString theName = EELobbyToWideString(GetMyLobbyClient()->GetName());
        if (static_cast<NESynchronousClient*>(gCommunications)->Connect(theAddress, theService, theGameName) == 0) {
            NEMessage* theRequest =
                new NEMRecord(kNetMessageClientConnectionRequest, {theFlag, gBinaryChecksum}, {gGameVersion, theName});
            static_cast<NESynchronousClient*>(gCommunications)->SendStartConnectMessage(theRequest);
            gCommunications->ProcessLocalUserInput(theRequest, 0xff);
        }
    }
}

void EELobbyRunSierraUpdate()
{
    FSFileSpec theFile;
    UWideString theMessage;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory + UString("Sierra Update\\"));
    theFile.SetFile("SierraUp.exe");
#ifdef _WIN32   // Sierra Update is a Windows program
    if (theFile.Open(1) == keseOK) {
        theFile.Close();
        HINSTANCE theResult = ShellExecuteA(GetDesktopWindow(), "open",
            theFile.GetVolume() + theFile.GetDirectory() + theFile.GetFile(), NULL,
            theFile.GetVolume() + theFile.GetDirectory(), SW_SHOWNORMAL);
        if ((INT_PTR)theResult > 32) {
            if (gUserInterface != NULL)   // PostQuitMessage: WM_QUIT to the UI thread's queue
                gUserInterface->GetThread()->Post(0x12, NULL, NULL, false);
            return;
        }
    }
#endif
    gResource->GetString(IDS_UNABLE_TO_LAUNCH_UPDATE_UTILITY, theMessage);
    gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
}
}
#endif
