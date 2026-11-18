// The Empire Earth lobby's glue (R6 WONLobby round 6). The colour button and its chooser (EELobbyColorButton.cpp)
// are Rust objects (rust/crates/ee_won_lobby/src/eecolor.rs), as are the glue's decisions (eeglue.rs: the original
// EELobbyInit.cpp and EELobbyMisc.cpp, and EELobbyInterface.cpp's options, players, adapter, run loop and return
// from a game). This unit is the classes the lobby's factories make (their virtuals forward), one function per entry
// point the game and the lobby call, the two hosts (each op the original's statements on a game object, a lobby
// object, a string or a global, with no decision), and what the game owns: the lobby's window and its frames for the
// renderer (EELobbyOpen, Show, Hide, Close), the game's network objects (EELobbyHostGame, EELobbyJoinGame,
// EELobbyStartHostedGame), Sierra Update and the lobby lock.
// port: the lobby no longer has windows of its own. Its windows are rectangles of one lobby surface
// (ee/wonapi/WONLobbyPlatform.h) the size of the back buffer; the graphics thread shows its frames in the game's
// window while the lobby is shown (RTSGraphics::RenderLobby), on every system.
#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/UResourceFile.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEGameSound.h"
#include "lle/FSRestorable.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEMouse.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAction.h"
#include "ee/EEMessage.h"
#include "ee/EEStringIDs.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "ee/RTSGraphics.h"
#include "platform/PlatWindow.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "rust/won_lobby/eecolor.h"
#include "rust/won_lobby/eeglue.h"
#include "rust/won_lobby/eesound.h"

using namespace WONAPI;

extern EEGameOptions& gLobbyGameOptions;    // 0x93030c (the lobby defaults; as EEGameOptions.cpp declares it)
extern char gEmptyString[];                 // 0x8e5b98 (pooled ""; as EEWonNet.cpp declares it)
void EELobbyShow(bool inFlag);
void EELobbyHide();
void EELobbyJoinGame();
void EELobbyStartHostedGame();
void EELobbyShowGameScreen();
void EELobbyRunSierraUpdate();
void EELobbyPostAction(void (*inFunc)());
void EELobbyOpen();
void EELobbyPostReturnFromGame();
bool EEIsLobbyOpen();
bool EELobbyIsShown();
UWideString EELobbyToWideString(const GUIString& inText);

typedef EeWonEeGlueCall GL;
static intptr_t Glue(GL inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return ee_won_eeglue_call(inCall, a, b, c); }

// ---- the colour button and chooser (EELobbyColorButton.cpp's classes; the Rust objects' shells)

typedef EeWonEeColorCall CC;
static intptr_t ColorHost(void*, EeWonEeColorOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e,
                          intptr_t f);

EELobbyColorButton::EELobbyColorButton() : mRust(ee_won_eecolor_new(this, EeWonEeColorKind::Button, ColorHost))
{
    this->SetButtonFlags(0x40, true);
}
EELobbyColorButton::~EELobbyColorButton() { ee_won_eecolor_free(this->mRust); }
bool EELobbyColorButton::SetColor(int inColor) { return ee_won_eecolor_call(this->mRust, CC::SetColor, inColor, 0, 0) != 0; }
int EELobbyColorButton::GetColor() { return (int)ee_won_eecolor_call(this->mRust, CC::GetColor, 0, 0, 0); }
void EELobbyColorButton::Paint(Graphics& inGraphics) { ee_won_eecolor_call(this->mRust, CC::Paint, (intptr_t)&inGraphics, 0, 0); }
bool EELobbyColorButton::HandleActivate() { return ee_won_eecolor_call(this->mRust, CC::Activate, 0, 0, 0) != 0; }
void EELobbyColorButton::HandleComponentEvent(ComponentEvent* inEvent)
{
    ee_won_eecolor_call(this->mRust, CC::Event, (intptr_t)inEvent, inEvent->mType == ComponentEvent_EndPopup, 0);
}

EELobbyColorChooser::EELobbyColorChooser() : mRust(ee_won_eecolor_new(this, EeWonEeColorKind::Chooser, ColorHost)) {}
EELobbyColorChooser::~EELobbyColorChooser() { ee_won_eecolor_free(this->mRust); }
// Returns the chooser's fixed size of 120 x 120 pixels
void EELobbyColorChooser::GetDesiredSize(int& outWidth, int& outHeight) { outWidth = outHeight = 120; }
void EELobbyColorChooser::Paint(Graphics& inGraphics) { ee_won_eecolor_call(this->mRust, CC::Paint, (intptr_t)&inGraphics, 0, 0); }
void EELobbyColorChooser::MouseExit() { ee_won_eecolor_call(this->mRust, CC::MouseExit, 0, 0, 0); }
void EELobbyColorChooser::MouseDown(int inA, int inB, int inC) { ee_won_eecolor_call(this->mRust, CC::MouseDown, inA, inB, inC); }
void EELobbyColorChooser::MouseMove(int inX, int inY) { ee_won_eecolor_call(this->mRust, CC::MouseMove, inX, inY, 0); }
void EELobbyColorChooser::AddedToParent() { ee_won_eecolor_call(this->mRust, CC::AddedToParent, 0, 0, 0); }

static intptr_t ColorHost(void*, EeWonEeColorOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e,
                          intptr_t f)
{
    typedef EeWonEeColorOp Op;
    Component* aComp = reinterpret_cast<Component*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    switch (inOp) {
    case Op::Invalidate: aComp->Invalidate(); return 0;
    case Op::Width: return aComp->Width();
    case Op::Height: return aComp->Height();
    case Op::IsDisabled: return reinterpret_cast<Button*>(a)->IsDisabled() ? 1 : 0;
    case Op::IsPressed: return reinterpret_cast<Button*>(a)->IsPressed() ? 1 : 0;
    case Op::BasePaint: aComp->Component::Paint(*reinterpret_cast<Graphics*>(b)); return 0;
    case Op::SetColor: g->SetColor((unsigned long)b); return 0;
    case Op::SetMenuBackColor: g->SetSysColor(StandardColor_MenuBack); return 0;
    case Op::FillRect: g->FillRect((int)b, (int)c, (int)d, (int)e); return 0;
    case Op::DrawRect: g->DrawRect((int)b, (int)c, (int)d, (int)e); return 0;
    case Op::Draw3DFrame: LobbyDraw3DFrame(*g, (int)b, (int)c, (int)d, (int)e, f != 0); return 0;
    case Op::DrawFrame: LobbyDrawFrame(*g, (int)b, (int)c, (int)d, (int)e, f != 0); return 0;
    case Op::ChooserNew: {
        EELobbyColorChooser* aNew = new EELobbyColorChooser;
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case Op::ChooserRust: return (intptr_t)reinterpret_cast<EELobbyColorChooser*>(a)->mRust;
    case Op::SetDesiredSize: aComp->SetDesiredSize((int)b, (int)c); return 0;
    case Op::AddListener: aComp->AddListener(new ComponentListenerComp(reinterpret_cast<Component*>(b))); return 0;
    case Op::Parent: return (intptr_t)aComp->mParent;
    case Op::ClosePopup: aComp->GetRoot()->ClosePopup(reinterpret_cast<Component*>(b)); return 0;
    case Op::LocalToRoot: aComp->LocalToRoot(*reinterpret_cast<int*>(b), *reinterpret_cast<int*>(c)); return 0;
    case Op::OpenPopup: {
        PopupParams theParams(reinterpret_cast<Component*>(b), 0x23e7, aComp, (int)c, (int)d);
        theParams.mAvoidRect.height = aComp->Height();
        theParams.mAvoidRect.y = (int)d - aComp->Height();
        theParams.mInputComponent = aComp;
        aComp->GetRoot()->OpenPopup(theParams);
        return 0;
    }
    case Op::FireColorChanged: aComp->Fn2F5380(100064, 0); return 0;
    case Op::BaseEvent: reinterpret_cast<Button*>(a)->Button::HandleComponentEvent(reinterpret_cast<ComponentEvent*>(b)); return 0;
    case Op::BaseAddedToParent: aComp->Component::AddedToParent(); return 0;
    case Op::BaseMouseExit: aComp->Component::MouseExit(); return 0;
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameRust: return (intptr_t) static_cast<EELobbyGame*>(reinterpret_cast<LobbyGame*>(a))->mRust;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    }
    return 0;
}

// ---- the classes the lobby's factories make

// ---- the lobby's sounds (EELobbySound.cpp: ee_won eesound.rs objects; each op one call on the game's managers)

static intptr_t EELobbySoundHost(void*, EeWonEeSoundOp inOp, intptr_t a)
{
    typedef EeWonEeSoundOp Op;
    switch (inOp) {
    case Op::HasUiManager: return gUIManager != NULL;
    case Op::MusicVolumeOn: gUIManager->SetMusicVolume(gGamePreferences.mSettings.GetMusicVolume()); return 0;
    case Op::MusicVolumeOff: gUIManager->SetMusicVolume(0.0f); return 0;
    case Op::HasSoundManager: return gSoundManager != NULL;
    case Op::Unload: gSoundManager->Unload((unsigned long)a); return 0;
    case Op::Play: return (intptr_t)(uint32_t)gSoundManager->Play((unsigned long)a, 1.0f, 0.0f, 1.0f, false, NULL);
    case Op::Stop: gSoundManager->Stop((unsigned long)a, false); return 0;
    case Op::Load: return (intptr_t)(uint32_t)gSoundManager->Load(reinterpret_cast<const char*>(a), 0, 1);
    }
    return 0;
}

EELobbyMusic::EELobbyMusic(const SoundDescriptor& inDescriptor)
    : Sound(inDescriptor), mRust(ee_won_eesound_new(this, EeWonEeSoundKind::Music, EELobbySoundHost, 0)) {}
EELobbyMusic::~EELobbyMusic() { ee_won_eesound_free(this->mRust); }
void EELobbyMusic::PlayHook() { ee_won_eesound_call(this->mRust, EeWonEeSoundCall::Play, 0); }
void EELobbyMusic::StopHook() { ee_won_eesound_call(this->mRust, EeWonEeSoundCall::Stop, 0); }
void EELobbyMusic::ResumeHook() { ee_won_eesound_call(this->mRust, EeWonEeSoundCall::Resume, 0); }
EELobbySound::EELobbySound(const SoundDescriptor& inDescriptor, unsigned long inSampleID)
    : Sound(inDescriptor),
      mRust(ee_won_eesound_new(this, EeWonEeSoundKind::Effect, EELobbySoundHost, (uint32_t)inSampleID)) {}
EELobbySound::~EELobbySound() { ee_won_eesound_free(this->mRust); }
void EELobbySound::PlayHook() { ee_won_eesound_call(this->mRust, EeWonEeSoundCall::Play, 0); }
void EELobbySound::StopHook() { ee_won_eesound_call(this->mRust, EeWonEeSoundCall::Stop, 0); }
// Loads a lobby sound file into the engine's sound manager; returns NULL without a sound manager
SoundPtr EELobbySoundDecoder::Decode(const SoundDescriptor& inDescriptor)
{
    int64_t aSample = ee_won_eesound_load(EELobbySoundHost, reinterpret_cast<const uint8_t*>(inDescriptor.mPath.c_str()));
    if (aSample < 0)
        return NULL;
    return new EELobbySound(inDescriptor, (unsigned long)aSample);
}

// Creates the lobby config with the EEFileTransferPort key (default 8889)
EELobbyConfig::EELobbyConfig() { this->KeyAdd("EEFileTransferPort", this->mFileTransferPort, 8889); }

void EELobbyServerDetailsItem::PaintDetailsHook(Graphics& inGraphics, MultiListArea*)
{
    Glue(GL::PaintDetails, (intptr_t)this, (intptr_t)&inGraphics);
}
int EELobbyServerDetailsItem::GetDetailsHeightHook(MultiListArea* inList) { return (int)Glue(GL::DetailsHeight, (intptr_t)inList); }

// the game form's open event once a game is done (vtable 0x84c6e8; ??_G ICF-folded 0x5e60e7)
class EELobbyGameDoneEvent : public UIEvent {
public:
    EELobbyGameDoneEvent() : mCount(0) {}
    virtual ~EELobbyGameDoneEvent() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity*) { Glue(GL::GameDone, (intptr_t)inForm, (intptr_t)&this->mCount); return keseOK; }
    int32_t mCount;                         // +0x0c
};

// the game's actions the lobby posts to the user interface: one function to call (vtable 0x84c6c4)
class EEALobbyAction : public EEAction {
public:
    EEALobbyAction(void (*inFunc)()) : mFunc(inFunc) {}
    virtual void Virtual4() { this->mFunc(); }  // +0x04 0x2972c3
    virtual void Virtual8() {}              // +0x08 (ICF-folded 0x1720c6)
    void (*mFunc)();                        // +0x0c
};

// port: EEUserInterface::ProcessActions holds the UI manager's copy lock and the world's write lock around every
// action, and the lobby runs inside one; the graphics thread presents the lobby (RTSGraphics::RenderLobby), so the run
// loop releases them while the lobby runs and takes them back, in ProcessActions' order, before the action returns
static void EELobbyLocks(bool inRelease)
{
    if (inRelease) {
        gWorld.mLock->WriteUnlock();
        gUIManager->UnlockUIForCopyData();
    } else {
        gUIManager->LockUIForCopyData();
        gWorld.mLock->WriteLock();
    }
}

static LobbyPlayer* EELobbyNewPlayer() { return new EELobbyPlayer; }
static LobbyGame* EELobbyNewGame() { return new EELobbyGame; }
static ComponentConfig* EELobbyNewButtonConfig() { return new ButtonConfig; }
static ComponentConfig* EELobbyNewContainerConfig() { return new ContainerConfig; }
static ComponentConfig* EELobbyNewDialogConfig() { return new DialogConfig; }
static Component* EELobbyNewGameOptionsCtrl() { return new EELobbyGameOptionsCtrl; }
static Component* EELobbyNewPlayerOptionsCtrl() { return new EELobbyPlayerOptionsCtrl; }
static Component* EELobbyNewColorButton() { return new EELobbyColorButton; }
static Component* EELobbyNewFileDialog() { return new EELobbyFileDialog; }
static Component* EELobbyNewConnectDialog() { return new EELobbyConnectDialog; }
static LobbyConfig* EELobbyNewConfig() { return new EELobbyConfig; }
static ServerDetailsItem* EELobbyNewServerDetailsItem() { return new EELobbyServerDetailsItem; }
static void EELobbyGetComponents(ComponentConfig* inConfig) { Glue(GL::GetComponents, (intptr_t)inConfig); }
static void EELobbySendConnectStatus(int inConnected) { Glue(GL::SendConnectStatus, inConnected); }

// a lobby text (eeglue.rs's EeWonEeGlueText) as a GUIString
static GUIString GlueText(intptr_t inKind, intptr_t inValue)
{
    char aBuffer[50];
    switch ((EeWonEeGlueText)inKind) {
    case EeWonEeGlueText::Resource: return EELobbyResource_GetString((int)inValue);
    case EeWonEeGlueText::Number: sprintf(aBuffer, "%ld", (long)inValue); return GUIString(aBuffer);
    case EeWonEeGlueText::MapType: return EELobbyToGUIString(EERMGetGeneratorDisplayName((unsigned long)inValue));
    case EeWonEeGlueText::Data: return EELobbyStr(inValue);
    case EeWonEeGlueText::Empty: break;
    }
    return gEmptyGUIString;
}

// a saved game's header, options and file (EELobbyGetSavedGameInfo's and EELobbyLoadSavedGame's locals)
struct EELobbySavedGame {
    EEPSaveGameHeader mHeader;
    EEGameOptions mOptions;
    FSFileSpec mSpec;
};

static long OptionsField(const EEGameOptions& o, EeWonEeGlueField inField)
{
    typedef EeWonEeGlueField F;
    unsigned long aIndex = 0;
    switch (inField) {
    case F::MapTypeIndex: EERMFindGenerator(o.mMapType, aIndex); return (long)aIndex;
    case F::MapSize: return o.mMapSize;
    case F::Resources: return o.mResources;
    case F::EpochStart: return o.mEpochStart;
    case F::EpochEnd: return o.mEpochEnd;
    case F::Variant: return o.mVariant;
    case F::Difficulty: return o.mDifficulty;
    case F::GameSpeed: return o.mGameSpeed;
    case F::UnitLimit: return o.mUnitLimit;
    case F::WondersForVictory: return o.mWondersForVictory;
    case F::RevealMap: return o.mRevealMap;
    case F::AllowCustomCivs: return o.mAllowCustomCivs;
    case F::LockTeams: return o.mLockTeams;
    case F::LockSpeed: return o.mLockSpeed;
    case F::Victory: return o.mVictory;
    default: return 0;
    }
}

static void OptionSet(EeWonEeGlueField inField, long v)
{
    typedef EeWonEeGlueField F;
    EEGameOptions& o = gGameOptions;
    switch (inField) {
    case F::LoadType: o.mLoadType = v; return;
    case F::MapSize: o.mMapSize = v; return;
    case F::Resources: o.mResources = v; return;
    case F::EpochStart: o.mEpochStart = v; return;
    case F::EpochEnd: o.mEpochEnd = v; return;
    case F::Variant: o.mVariant = v; return;
    case F::Difficulty: o.mDifficulty = v; return;
    case F::GameSpeed: o.mGameSpeed = v; return;
    case F::UnitLimit: o.mUnitLimit = v; return;
    case F::WondersForVictory: o.mWondersForVictory = v; return;
    case F::RevealMap: o.mRevealMap = v != 0; return;
    case F::AllowCustomCivs: o.mAllowCustomCivs = v != 0; return;
    case F::LockTeams: o.mLockTeams = v != 0; return;
    case F::LockSpeed: o.mLockSpeed = v != 0; return;
    case F::Victory: o.mVictory = v != 0; return;
    default: return;
    }
}

static long PlayerField(EEGameOptionsPlayer& p, EeWonEeGlueField inField, bool inSet, long v)
{
    typedef EeWonEeGlueField F;
    switch (inField) {
    case F::Type: if (inSet) p.mType = v; return (long)p.mType;
    case F::LoadedType: if (inSet) p.mLoadedType = v; return p.mLoadedType;
    case F::Slot: if (inSet) p.mSlot = v; return (long)p.mSlot;
    case F::Color: if (inSet) p.mColor = v; return (long)p.mColor;
    case F::Team: if (inSet) p.mTeam = v; return (long)p.mTeam;
    case F::StartingCitizens: if (inSet) p.mStartingCitizens = v; return (long)p.mStartingCitizens;
    case F::IsLocal: if (inSet) p.mIsLocal = v != 0; return p.mIsLocal;
    case F::IsReady: if (inSet) p.mIsReady = v != 0; return p.mIsReady;
    case F::B36: if (inSet) p.mB36 = v != 0; return p.mB36;
    case F::NameEmpty: return p.mName == "" ? 1 : 0;
    default: return 0;
    }
}

static EEGameOptionsPlayer sEELobbyPlayer;  // the game player being made (EELobbyFillPlayers' local)

static intptr_t GlueHost(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t)
{
    typedef EeWonEeGlueOp Op;
    EELobbySavedGame* aSave = reinterpret_cast<EELobbySavedGame*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    std::vector<LobbyNetAddress>* anAddresses = reinterpret_cast<std::vector<LobbyNetAddress>*>(a);
    switch (inOp) {
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameRust: return (intptr_t) static_cast<EELobbyGame*>(reinterpret_cast<LobbyGame*>(a))->mRust;
    case Op::GameIsHost: return reinterpret_cast<LobbyGame*>(a)->GetIsHost() ? 1 : 0;
    case Op::ItemGame: return (intptr_t)reinterpret_cast<ServerDetailsItem*>(a)->mGame.get();
    case Op::DialogRust: return (intptr_t)reinterpret_cast<LobbyRustDialog*>(a)->mLobbyDlg;
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::SetColor: g->SetColor((unsigned long)b); return 0;
    case Op::DrawText: g->DrawString(GlueText(b, c), (int)d, (int)e); return 0;
    case Op::TextWidth: {
        GUIString aText = GlueText(b, c);
        return g->mFont->GetStringWidth(aText, 0, aText.length());
    }
    case Op::FontHeight: return g->mFont->GetHeight();
    case Op::ListFontHeight: return reinterpret_cast<MultiListArea*>(a)->mFont->GetHeight();
    case Op::TextFind: return GlueText(b, c).findNoCase(*reinterpret_cast<const GUIString*>(d), 0);
    case Op::TextCompareNoCase: return GlueText(a, b).CompareNoCase(GlueText(c, d));
    case Op::StringUnits: {
        GUIStringData* aData = reinterpret_cast<GUIStringData*>(a);
        if (b != 0)
            for (unsigned i = 0; i < aData->mLength; i++) reinterpret_cast<uint32_t*>(b)[i] = (uint32_t)aData->mText[i];
        return aData->mLength;
    }
    // the column's handlers are srvlistctl.rs's calls into this glue (LobbyNetCtrlHost.cpp's EeColumn)
    case Op::SetColumnHandlers: ee_won_lobbyctl_ee_column((int)a); return 0;
    case Op::SetFactories:
        LobbyGame::SetGameFactory(EELobbyNewGame);
        LobbyPlayer::SetPlayerFactory(EELobbyNewPlayer);
        ServerDetailsItem::SetItemFactory(EELobbyNewServerDetailsItem);
        LobbyConfig::SetAllocator(EELobbyNewConfig);
        return 0;
    case Op::AddResources: {
        ResourceConfigTablePtr theCollection = EELobbyResource_Init(NULL);
        theCollection->AddAllocator("GameOptionsCtrl", EELobbyNewContainerConfig, EELobbyNewGameOptionsCtrl);
        theCollection->AddAllocator("PlayerOptionsCtrl", EELobbyNewContainerConfig, EELobbyNewPlayerOptionsCtrl);
        theCollection->AddAllocator("EELobbyColorButton", EELobbyNewButtonConfig, EELobbyNewColorButton);
        theCollection->AddAllocator("EELobbyFileDialog", EELobbyNewDialogConfig, EELobbyNewFileDialog);
        theCollection->AddAllocator("EELobbyConnectDialog", EELobbyNewDialogConfig, EELobbyNewConnectDialog);
        // EELobbyAddResources: the LobbyGlobal section with the lobby music
        theCollection->AddConfig("LobbyGlobal", NULL)->AddSound("LobbyMusic", new EELobbyMusic(SoundDescriptor("", 2)));
        LobbyAddComponentLoader(theCollection.mPtr, EELobbyGetComponents);
        return 0;
    }
    case Op::AddGameOption: LobbyAddGameOption(a ? "MapType" : "Epoch", GlueText(b, c), true); return 0;
    case Op::NumMapTypes: return (intptr_t)EERMGetNumberOfGenerators();
    case Op::PlayerColor: {
        GEColor theColor = EEGetPlayerColor((long)a);
        float* aRgb = reinterpret_cast<float*>(b);
        aRgb[0] = theColor.GetRed();
        aRgb[1] = theColor.GetGreen();
        aRgb[2] = theColor.GetBlue();
        return 0;
    }
    case Op::SetProductName: LobbySetProductName(L"EEAOC", NULL); return 0;
    case Op::ComboClear: reinterpret_cast<ComboBox*>(a)->Clear(true); return 0;
    case Op::ComboAddMapType: reinterpret_cast<ComboBox*>(a)->AddString(GlueText((intptr_t)EeWonEeGlueText::MapType, b)); return 0;
    case Op::FindDialog: {
        // EELobbyInit.cpp's lines 328/329
        Component* aDialog;
        if (b) {
            SmartPtr<EELobbyConnectDialog> aPtr;
            SafeGetComponent(reinterpret_cast<ComponentConfig*>(a), aPtr, "EELobbyConnectDialog", "EELobbyInit.cpp", 329);
            aDialog = aPtr.get();
        } else {
            SmartPtr<EELobbyFileDialog> aPtr;
            SafeGetComponent(reinterpret_cast<ComponentConfig*>(a), aPtr, "EELobbyFileDialog", "EELobbyInit.cpp", 328);
            aDialog = aPtr.get();
        }
        if (aDialog != NULL) aDialog->AddRef();
        return (intptr_t)aDialog;
    }
    case Op::LobbyClose: EELobbyClose(); return 0;
    case Op::ResourceDestroy: EELobbyResource_Destroy(); return 0;
    case Op::Config: return (intptr_t)GetLobbyConfig();
    case Op::ConfigPort: return static_cast<EELobbyConfig*>(reinterpret_cast<LobbyConfig*>(a))->mFileTransferPort;
    case Op::SetGameInProgress: LobbySetGameInProgress((int)a); return 0;
    case Op::LeaveGame: LobbyLeaveGame(a != 0); return 0;
    case Op::IAmCaptain: return LobbyIAmCaptain() ? 1 : 0;
    case Op::LobbyWindow: return (intptr_t)GetLobbyWindow();
    case Op::PostSendConnectStatus:
        reinterpret_cast<WindowManager*>(a)->PostEvent(new UserEvent(EELobbySendConnectStatus, (int)b), true);
        return 0;
    case Op::SaveNew: {
        EELobbySavedGame* aNew = new EELobbySavedGame;
        aNew->mSpec.SetFile(reinterpret_cast<const char*>(a));
        return (intptr_t)aNew;
    }
    case Op::SaveOpen: return aSave->mSpec.Open(1) == keseOK ? 1 : 0;
    case Op::SaveLength: {
        unsigned long theLength = 0;
        aSave->mSpec.GetLength(theLength);
        return (intptr_t)theLength;
    }
    case Op::SaveClose: aSave->mSpec.Close(); return 0;
    case Op::SaveLoadHeader:
        aSave->mHeader.SetMajorVersion(kSaveGameMajorVersion);
        aSave->mHeader.SetMinorVersion(kSaveGameMinorVersion);
        return aSave->mHeader.LoadFromFile(aSave->mSpec) == keseOK ? 1 : 0;
    case Op::SaveChecksum: return (intptr_t)aSave->mHeader.mChecksum;
    case Op::SaveOptions: aSave->mOptions = aSave->mHeader.mGameOptions; return (intptr_t)&aSave->mOptions;
    case Op::SaveFree: delete aSave; return 0;
    case Op::BoxReadError:
        LobbyMessageBox(EE_LOBBY_STRING(gEELobbyInterface_LoadGameError), EE_LOBBY_STRING(gEELobbyInterface_ErrorReadingFile));
        return 0;
    case Op::BoxNotInGame:
        LobbyMessageBox(EE_LOBBY_STRING(gEELobbyInterface_LoadGameError),
                        LobbyFormatString(EE_LOBBY_STRING(gEELobbyInterface_PlayerNotInGame), EELobbyStr(a)));
        return 0;
    case Op::OptionsField: return OptionsField(*reinterpret_cast<const EEGameOptions*>(a), (EeWonEeGlueField)b);
    case Op::OptionsPlayer:
        return PlayerField(reinterpret_cast<EEGameOptions*>(a)->mPlayers[b], (EeWonEeGlueField)c, false, 0);
    case Op::OptionsPlayerName: {
        GUIStringData* aData = EELobbyToGUIString(reinterpret_cast<const EEGameOptions*>(a)->mPlayers[b].mName).mData.get();
        aData->AddRef();
        return (intptr_t)aData;
    }
    case Op::FileName: {
        WONFile theFile(reinterpret_cast<const char*>(a));
        GUIStringData* aData = GUIString(theFile.GetFileName()).mData.get();
        aData->AddRef();
        return (intptr_t)aData;
    }
    case Op::FileSize: return (intptr_t)WONFile(reinterpret_cast<const char*>(a)).GetSize();
    case Op::ClientPlayer: return (intptr_t)(EELobbyPlayer*)reinterpret_cast<LobbyClient*>(a)->GetPlayer().mPtr;
    case Op::DefaultOptions: return (intptr_t)&gGamePreferences.GetSettings().mGameOptions;
    case Op::OptionsBegin:
        gGameOptions.mLock->WriteLock();
        gGameOptions = gLobbyGameOptions;
        gGameOptions.mIsMultiplayer = true;
        gGameOptions.mCheatCodes = false;
        return 0;
    case Op::OptionSet: OptionSet((EeWonEeGlueField)a, (long)b); return 0;
    case Op::OptionMapTypeName: gGameOptions.mMapType = EELobbyToWideString(EELobbyStr(a)); return 0;
    case Op::OptionMapTypeGenerator: gGameOptions.mMapType = EERMGetGeneratorName((unsigned long)a); return 0;
    case Op::OptionsEnd: gGameOptions.mLock->WriteUnlock(); return 0;
    case Op::PlayerNew: sEELobbyPlayer = EEGameOptionsPlayer(); return 0;
    case Op::PlayerSet: PlayerField(sEELobbyPlayer, (EeWonEeGlueField)a, true, (long)b); return 0;
    case Op::PlayerType: return (intptr_t)sEELobbyPlayer.mType;
    case Op::PlayerSetName: sEELobbyPlayer.mName = EELobbyToWideString(EELobbyStr(a)); return 0;
    case Op::PlayerCommit: gGameOptions.mPlayers[a] = sEELobbyPlayer; return 0;
    case Op::PlayerCopyTo: *reinterpret_cast<EEGameOptionsPlayer*>(a) = sEELobbyPlayer; return 0;
    case Op::LocalHost: return (intptr_t)WONAPI::IPAddr::GetLocalHost();
    case Op::AddressesGet: {
        std::list<LobbyNetAddress> theAddresses;
        LobbyGetNetAddresses(theAddresses, false);
        return (intptr_t)new std::vector<LobbyNetAddress>(theAddresses.begin(), theAddresses.end());
    }
    case Op::AddressesCount: return (intptr_t)anAddresses->size();
    case Op::AddressHandle: return (intptr_t)(*anAddresses)[b].mHandle;
    case Op::AddressesFree: delete anAddresses; return 0;
    case Op::SetNetworkAdapter: gNetworkAdapter = (unsigned long)a; return 0;
    case Op::FormHide: reinterpret_cast<UIForm*>(a)->GetControl((long)b)->SetDisplayMode(0); return 0;
    case Op::IsLobbyOpen: return EEIsLobbyOpen() ? 1 : 0;
    case Op::IsShown: return EELobbyIsShown() ? 1 : 0;
    case Op::PostReturnFromGame: EELobbyPostReturnFromGame(); return 0;
    case Op::LocksRelease: EELobbyLocks(true); return 0;
    case Op::LocksRetake: EELobbyLocks(false); return 0;
    case Op::LobbyRun: return LobbyRun();
    case Op::PostStartHostedGame: EELobbyPostAction(EELobbyStartHostedGame); return 0;
    case Op::ShowGameScreen: EELobbyShowGameScreen(); return 0;
    case Op::LobbySuspend: LobbySuspend(); return 0;
    case Op::Hide: EELobbyHide(); return 0;
    case Op::JoinGame: EELobbyJoinGame(); return 0;
    case Op::SierraUpdate: EELobbyRunSierraUpdate(); return 0;
    case Op::DeleteCommunications:
        delete gCommunications;
        gCommunications = NULL;
        return 0;
    case Op::Show: EELobbyShow(true); return 0;
    case Op::GameForm: return (intptr_t)gUIManager->GetFormByID(30);
    case Op::CurrentForm: return (intptr_t)gUIManager->GetCurrentForm();
    case Op::PopForm: gUIManager->Pop(); return 0;
    case Op::LobbyResume: LobbyResume(); return 0;
    case Op::BoxDissolved: LobbyMessageBox(LOBBY_STRING(RoutingLogic_GameDissolvedTitle_String), LOBBY_STRING(RoutingLogic_GameDissolvedDetails_String)); return 0;
    }
    return 0;
}

// ---- the entry points (EELobbyInit.cpp's and EELobbyMisc.cpp's; the Rust's)

// Sets the lobby up for Empire Earth: the server list's epoch and map type columns, the factories, the game's
// components, resources, epoch and map type options, player colours and the product name
void EELobbyInit()
{
    ee_won_eeglue_set_host(GlueHost);
    Glue(GL::Init);
}
void EELobbyShutdown() { Glue(GL::Shutdown); }
unsigned short EELobbyGetFileTransferPort() { return (unsigned short)Glue(GL::FileTransferPort); }
EELobbyFileDialog* EELobbyGetFileDialog() { return reinterpret_cast<EELobbyFileDialog*>(Glue(GL::FileDialog)); }
bool EELobbyRunConnectDialog() { return Glue(GL::RunConnectDialog) != 0; }
void EELobbySetSlotConnected(unsigned char inSlot, bool inConnected) { Glue(GL::SetSlotConnected, inSlot, inConnected); }
void EELobbyCancelConnectDialog() { Glue(GL::CancelConnectDialog); }
void EELobbyPostConnectStatus(bool inConnected) { Glue(GL::PostConnectStatus, inConnected); }
bool EELobbyIsConnectDialogRunning() { return Glue(GL::IsConnectDialogRunning) != 0; }
bool EELobbyGetSavedGameInfo(const char* inFile, unsigned long& outLength, unsigned long& outChecksum)
{
    uint32_t aLength = (uint32_t)outLength, aChecksum = (uint32_t)outChecksum;
    bool aResult = Glue(GL::SavedGameInfo, (intptr_t)inFile, (intptr_t)&aLength, (intptr_t)&aChecksum) != 0;
    outLength = aLength;
    outChecksum = aChecksum;
    return aResult;
}
bool EELobbyLoadSavedGame(const char* inFile) { return Glue(GL::LoadSavedGame, (intptr_t)inFile) != 0; }
void EELobbyFillMapTypeCombo(ComboBox* inCombo) { Glue(GL::FillMapTypeCombo, (intptr_t)inCombo); }
void EELobbySetDefaultGameOptions(EELobbyGame* inGame) { Glue(GL::SetDefaultGameOptions, (intptr_t)inGame); }
unsigned long EELobbyGetNumMapTypes() { return EERMGetNumberOfGenerators(); }

// ---- what the game owns (EELobbyMisc.cpp's and EELobbyInterface.cpp's)

static CriticalSection gEELobbyCriticalSection;

// Enters the lobby's critical section (shared with the renderer)
void EELobbyLock() { gEELobbyCriticalSection.Enter(); }
// Leaves the lobby's critical section
void EELobbyUnlock() { gEELobbyCriticalSection.Leave(); }

// Converts a game wide string (player or map name) to a lobby GUI string
GUIString EELobbyToGUIString(const UWideString& inText) { return GUIString(StringToWString(std::string(inText), -1)); }
// Converts a lobby GUI string to a game wide string
UWideString EELobbyToWideString(const GUIString& inText) { return UWideString(WStringToString(inText, -1).c_str()); }

// Returns the saved games directory (data\Saved Games)
const GUIString& EELobbyFn2988E9()
{
    static GUIString theDirectory;
    theDirectory = GetEmptyGUIStringData();
    theDirectory.append("data\\");
    theDirectory.append("Saved Games\\");
    return theDirectory;
}

// Returns whether the game is multiplayer when the flag is set, true otherwise
bool EEIsMultiplayerGame(bool inFlag) { return inFlag ? gGameOptions.mIsMultiplayer : true; }

// Returns the game's version string and binary checksum
void EELobbyGetVersion(GUIString& outVersion, unsigned long& outChecksum)
{
    outVersion = EELobbyToGUIString(gGameVersion);
    outChecksum = gBinaryChecksum;
}

SmartPtr<PlatWindowManager> gEELobbyRootWindow;
SmartPtr<Window> gEELobbyPlatformWindow;
static WONAPICoreEx* gEELobbyWindowManager;
static bool gEELobbyOpen;
static bool gEELobbyShown;
static long sEELobbyHeight;
static long sEELobbyWidth;
// port: the lobby's screens are laid out for 1024x768 (the original switched the display mode); its surface keeps
// that size and RTSGraphics::RenderLobby shows it letterboxed in the window
static const int kEELobbySurfaceWidth = 1024;
static const int kEELobbySurfaceHeight = 768;
static unsigned long sEELobbyStartTime;

// Returns the lobby game's host address in a multiplayer lobby game, else the protocol's local address
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

// Hosts the lobby game: replaces the connection with a new game server and, once the game is published, sends the
// host's own connection request; returns whether it was published
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

// Asks the user interface thread to open the external lobby
void EEStartExternalLobby() { EELobbyPostAction(EELobbyOpen); }

// Posts a function for the user interface thread to call
void EELobbyPostAction(void (*inFunc)())
{
    EEALobbyAction* theAction = new EEALobbyAction(inFunc);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
}

// port: the game window's events the lobby passes on while it owns the event loop (close, quit, size, focus,
// exposure, the keys of its main window): handled as the game window's own (EEUserInterface::HandleEvent)
static void EELobbyForwardEvent(const PlatInputEvent& inEvent) { EEUserInterface::HandleEvent(inEvent); }

// Opens the external lobby: WONAPI core, window manager, the lobby window and the lobby screen in it; then shows it
// and runs it until it is left
// port: the window manager and window are the platform backend's (WONLobbyPlatform.h): the lobby window fills the
// lobby surface (the back buffer's size), so the original's title, icon and owner window are gone
void EELobbyOpen()
{
    if (gEELobbyOpen)
        return;
    gEELobbyOpen = true;
    gEELobbyWindowManager = new WONAPICoreEx;
    gEELobbyWindowManager->SetDoPumpThread(true);
    gEELobbyWindowManager->Startup();
    gEELobbyRootWindow = new PlatWindowManager;
    gEELobbyRootWindow->SetPopupsInOwnWindow(false);
    JPGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
    PNGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
    gEELobbyRootWindow->SetSoundDecoder(new EELobbySoundDecoder);
    if (gRasterizer->GetSecondarySurface(gScreenIndex) != NULL)
        gEELobbyRootWindow->SetSurfaceSize(kEELobbySurfaceWidth, kEELobbySurfaceHeight);
    gEELobbyRootWindow->SetEventForward(EELobbyForwardEvent);
    gEELobbyPlatformWindow = CreateNativeWindow();
    static_cast<PlatGUIWindow*>(gEELobbyPlatformWindow.get())->SetForwardKeyMsgToParent(true);
    long theStyle = 2;
    if (gGamePreferences.mSettings.mFullScreen)
        theStyle = 0x22;
    gEELobbyPlatformWindow->SetStyle(theStyle);
    WONRectangle theRect(0, 0, 100, 100);
    gEELobbyPlatformWindow->Create(theRect);
    RootContainer* theRoot = gEELobbyPlatformWindow->mRoot.get();
    EELobbyInit();
    SmartPtr<Container> theScreen = LobbyGetContainer();
    theRoot->Fn2F6140(theScreen.get(), 3, theRoot, 0, 0, 0, 0, 0);
    theRoot->AddChild(theScreen.get());
    EELobbyShow(true);
    LobbyStart();
    Glue(GL::RunLoop);
}

// port: the lobby's frame for RTSGraphics::RenderLobby (RTSGraphics.h). The caller holds EELobbyLock, under which
// EELobbyShow and EELobbyHide change gEELobbyShown (and EELobbyClose hides before it lets the manager go), so the
// manager outlives every locked frame.
bool EELobbyLockFrame(const uint32_t*& outPixels, int& outWidth, int& outHeight, int& outPitch, int& outLogicalWidth,
                      int& outLogicalHeight, uint32_t& outSerial)
{
    if (!gEELobbyShown || gEELobbyRootWindow.get() == NULL)
        return false;
    return gEELobbyRootWindow->LockFrame(outPixels, outWidth, outHeight, outPitch, outLogicalWidth, outLogicalHeight,
                                         outSerial);
}

// port: RenderLobby's pixel scale (the lobby at the window's pixels; the caller holds EELobbyLock)
void EELobbySetPresentation(float inScale, float inLeft, float inTop, float inWidth, float inHeight)
{
    if (gEELobbyShown && gEELobbyRootWindow.get() != NULL) {
        gEELobbyRootWindow->SetPresentRect(inLeft, inTop, inWidth, inHeight);
        gEELobbyRootWindow->SetPixelScale(inScale);
    }
}

void EELobbyGetSurfaceSize(int& outWidth, int& outHeight)
{
    outWidth = kEELobbySurfaceWidth;
    outHeight = kEELobbySurfaceHeight;
}

void EELobbyUnlockFrame()
{
    if (gEELobbyRootWindow.get() != NULL)
        gEELobbyRootWindow->UnlockFrame();
}

// Shows the lobby in the game's place, keeping the cursor where it was, and suspends the game's input
// port: the lobby is drawn into the game's window (RTSGraphics::RenderLobby), so the game window stays; its input is
// suspended in both modes (the mouse leaves relative mode), the system pointer shows and the lobby gets typed text.
// inFlag asked the original to flip to the GDI surface (full screen), which has no meaning here.
void EELobbyShow(bool inFlag)
{
    (void)inFlag;
    if (gEELobbyShown || gEELobbyPlatformWindow.get() == NULL)
        return;
    PlatWindow* theWindow = gGameWindow->GetOSRepresentation();
    GE2DPoint theCursor(0.0f, 0.0f);
    if (gMouse != NULL) {
        theCursor = gMouse->mPosition;
        gMouse->mPointer = 6;
    }
    if (GESurface* theSecondary = gRasterizer->GetSecondarySurface(gScreenIndex)) {
        sEELobbyWidth = (long)theSecondary->GetWidth();
        sEELobbyHeight = (long)theSecondary->GetHeight();
        gEELobbyRootWindow->SetSurfaceSize(kEELobbySurfaceWidth, kEELobbySurfaceHeight);
    }
    gEELobbyPlatformWindow->Show(true);
    EELobbyLock();
    gEELobbyShown = true;
    if (gISKeyboard != NULL)
        gISKeyboard->Suspend();
    if (gISMouse != NULL)
        gISMouse->Suspend();
    EELobbyUnlock();
    PlatTextInputStart(theWindow, 0, 0, 0, 0);
    PlatCursorShow(1);
    if (gMouse != NULL && theWindow != NULL && sEELobbyWidth > 0 && sEELobbyHeight > 0) {
        // the game's pointer is in back buffer pixels, the window's in client pixels
        int32_t theWidth = 0, theHeight = 0;
        PlatWindowGetClientSize(theWindow, &theWidth, &theHeight);
        PlatMouseWarp(theWindow, (int32_t)(theCursor.mX * theWidth / sEELobbyWidth),
                      (int32_t)(theCursor.mY * theHeight / sEELobbyHeight));
    }
    gEELobbyPlatformWindow->InvalidateRoot();
}

// Starts loading the hosted game: closes it to new players, counts the humans, sends the start message with a random
// seed and saves the options as the last game's
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

// Joins the lobby game: makes this a client, resets the game options and connects to the host's address
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

// Hides the lobby and gives the game back its window, resuming input and the game's cursor
// port: the reverse of EELobbyShow: the game's pointer goes where the system pointer is, the system pointer hides
// again (the game draws its own) with its default shape; typed text stays on (the game window always takes it)
void EELobbyHide()
{
    if (!gEELobbyShown || gEELobbyPlatformWindow.get() == NULL)
        return;
    PlatWindow* theWindow = gGameWindow->GetOSRepresentation();
    int32_t thePointerX = 0, thePointerY = 0, theWidth = 0, theHeight = 0;
    if (theWindow != NULL) {
        PlatMouseGetState(theWindow, &thePointerX, &thePointerY);
        PlatWindowGetClientSize(theWindow, &theWidth, &theHeight);
    }
    EELobbyLock();
    gEELobbyShown = false;
    EELobbyUnlock();
    gUIManager->RedrawAll();
    if (gISKeyboard != NULL)
        gISKeyboard->Resume();
    if (gISMouse != NULL)
        gISMouse->Resume();
    PlatCursorSet(NULL);
    PlatCursorShow(0);
    if (gMouse != NULL) {
        float theX = 0.0f, theY = 0.0f;
        if (theWidth > 0 && theHeight > 0) {
            theX = (float)thePointerX * sEELobbyWidth / theWidth;
            theY = (float)thePointerY * sEELobbyHeight / theHeight;
        }
        gMouse->SetPosition(GE2DPoint(theX, theY));
        gMouse->mPointer = 0;
    }
    gEELobbyPlatformWindow->Show(false);
}

// Runs Sierra Update and quits the game, or tells the player it is missing
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
                gUserInterface->mThread->Post(0x12, NULL, NULL, false);
            return;
        }
    }
#endif
    gResource->GetString(IDS_UNABLE_TO_LAUNCH_UPDATE_UTILITY, theMessage);
    gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
}

// Pushes the game form (form 30), hooking it to return to the lobby once the game is done
void EELobbyShowGameScreen()
{
    if (gUIManager != NULL) {
        EELobbyGameDoneEvent* theEvent = new EELobbyGameDoneEvent;
        gUIManager->GetFormByID(30)->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        gUIManager->Push(gUIManager->GetFormByID(30));
    }
}

void EELobbyReturnFromGame() { Glue(GL::ReturnFromGame); }
// Asks the user interface thread to return to the lobby
void EELobbyPostReturnFromGame() { EELobbyPostAction(EELobbyReturnFromGame); }

// Closes the external lobby, its windows and its WONAPI core, and restores the game's music volume
void EELobbyClose()
{
    if (gEELobbyOpen) {
        gEELobbyOpen = false;
        EELobbyHide();
        gEELobbyRootWindow->CloseAllWindows();
        LobbyShutdown();
        gEELobbyWindowManager->Shutdown();
        delete gEELobbyWindowManager;
        gEELobbyWindowManager = NULL;
        gEELobbyPlatformWindow = NULL;
        gEELobbyRootWindow = NULL;
        PlatCursorShow(0);   // port: closing the lobby's windows shows the system pointer; the game draws its own
        if (gUIManager != NULL)
            gUIManager->SetMusicVolume(gGamePreferences.GetSettings().GetMusicVolume());
    }
}

bool EEIsLobbyOpen() { return gEELobbyOpen; }
bool EELobbyIsShown() { return gEELobbyShown; }
void EESetQuitFlag() { Glue(GL::SetQuitFlag); }
