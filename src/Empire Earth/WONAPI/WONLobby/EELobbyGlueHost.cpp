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
#include "lle/SEMixer.h"
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
#include "rust/won_lobby/rows.h"
#include "rust/won_lobby/eesound.h"

using namespace WONAPI;

extern EEGameOptions& gLobbyGameOptions;    // 0x93030c (the lobby defaults; as EEGameOptions.cpp declares it)
extern char gEmptyString[];                 // 0x8e5b98 (pooled ""; as EEWonNet.cpp declares it)

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
    case Op::ChooserRust: return (intptr_t)reinterpret_cast<EELobbyColorChooser*>(a)->mRust;
    case Op::SetDesiredSize: aComp->SetDesiredSize((int)b, (int)c); return 0;
    case Op::AddListener: aComp->AddListener(new ComponentListenerComp(reinterpret_cast<Component*>(b))); return 0;
    case Op::Parent: return (intptr_t)aComp->mParent;
    case Op::ClosePopup: aComp->GetRoot()->ClosePopup(reinterpret_cast<Component*>(b)); return 0;
    case Op::LocalToRoot: aComp->LocalToRoot(*reinterpret_cast<int*>(b), *reinterpret_cast<int*>(c)); return 0;
    case Op::FireColorChanged: aComp->Fn2F5380(100064, 0); return 0;
    case Op::BaseEvent: reinterpret_cast<Button*>(a)->Button::HandleComponentEvent(reinterpret_cast<ComponentEvent*>(b)); return 0;
    case Op::BaseAddedToParent: aComp->Component::AddedToParent(); return 0;
    case Op::BaseMouseExit: aComp->Component::MouseExit(); return 0;
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameRust: return (intptr_t) reinterpret_cast<LobbyGame*>(a)->mEE;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    // the statements of ChooserNew and OpenPopup (eecolor.rs composes them, R6 WON round 35)
    case Op::ChooserAlloc: return (intptr_t) new EELobbyColorChooser;
    case Op::ChooserAddRef: reinterpret_cast<EELobbyColorChooser*>(a)->AddRef(); return 0;
    case Op::PopupNew: return (intptr_t) new PopupParams(reinterpret_cast<Component*>(b), 0x23e7, aComp, (int)c, (int)d);
    case Op::PopupAvoidHeight: reinterpret_cast<PopupParams*>(b)->mAvoidRect.height = (int)c; return 0;
    case Op::PopupAvoidY: reinterpret_cast<PopupParams*>(b)->mAvoidRect.y = (int)c; return 0;
    case Op::PopupInput: reinterpret_cast<PopupParams*>(b)->mInputComponent = aComp; return 0;
    case Op::PopupOpen: aComp->GetRoot()->OpenPopup(*reinterpret_cast<PopupParams*>(b)); return 0;
    case Op::PopupFree: delete reinterpret_cast<PopupParams*>(b); return 0;
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
// (the locks are gluesplit.rs's LocksRelease and LocksRetake, one op per lock: R6 WON round 32)

static LobbyPlayer* EELobbyNewPlayer() { return new LobbyPlayer(EeWonModelKind::EePlayer); }
static ComponentConfig* EELobbyNewButtonConfig() { return CfgNewOfKind(EeWonCfgKind::Button); }
static ComponentConfig* EELobbyNewContainerConfig() { return new ContainerConfig; }
static ComponentConfig* EELobbyNewDialogConfig() { return CfgNewOfKind(EeWonCfgKind::Dialog); }
static Component* EELobbyNewGameOptionsCtrl() { return new LobbyRustCtrl(EeWonLobbyCtlKind::GameOptions); }
static Component* EELobbyNewPlayerOptionsCtrl() { return new LobbyRustCtrl(EeWonLobbyCtlKind::PlayerOptions); }
static Component* EELobbyNewColorButton() { return new EELobbyColorButton; }
static LobbyConfig* EELobbyNewConfig() { return new EELobbyConfig; }
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

static EEGameOptionsPlayer sEELobbyPlayer;
static unsigned long sEELobbyMapIndex; // OptGetMapTypeIndex's (EERMFindGenerator's index)
// the locals of the cases gluesplit.rs composes (R6 WON round 32)
static ResourceConfigTablePtr sEELobbyResources;
static GEColor sEELobbyColor;
static unsigned long sEELobbySaveLength;
static std::list<LobbyNetAddress> sEELobbyAddressList;
// a text with one reference the Rust gives back
static intptr_t HeldData(const GUIString& inText)
{
    inText.mData->AddRef();
    return (intptr_t)inText.mData.get();
}
static intptr_t FindHeldDialog(ComponentConfig* inConfig, EeWonLobbyDlgKind inKind, const char* inName, int inLine)
{
    SmartPtr<LobbyRustDialog> aPtr;
    SafeGetDialog(inConfig, aPtr, inKind, inName, "EELobbyInit.cpp", inLine);
    if (aPtr.get() != NULL) aPtr->AddRef();
    return (intptr_t)aPtr.get();
}  // the game player being made (EELobbyFillPlayers' local)

// the lobby's window and WONAPI core (the game's; eewindow.rs's functions over them, the renderer under the lock)
SmartPtr<PlatWindowManager> gEELobbyRootWindow;
SmartPtr<Window> gEELobbyPlatformWindow;
static WONAPICoreEx* gEELobbyWindowManager;
static bool gEELobbyOpen;
static bool gEELobbyShown;
// port: the lobby's screens are laid out for 1024x768 (the original switched the display mode); its surface keeps
// that size and RTSGraphics::RenderLobby shows it letterboxed in the window
static const int kEELobbySurfaceWidth = 1024;
static const int kEELobbySurfaceHeight = 768;
static unsigned long sEELobbyStartTime;
// the network game functions' locals (eegamefn.rs): the host's player, the join's strings and request, the start's
// player copy, the local address handed out
static EEGameOptionsPlayer sEELobbyHostMe;
static UString sEELobbyJoinAddress;
static UString sEELobbyJoinService;
static UWideString sEELobbyJoinGameName;
static UWideString sEELobbyJoinName;
static NEMessage* sEELobbyJoinRequest;
static EEGameOptionsPlayer sEELobbyStartPlayer;
static UWideString sEELocalAddress;
void EELobbyInit();

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
// port: one text made once (the original's function-local static was emptied and appended to on every call)
static const GUIString sEELobbySavedGames("data\\Saved Games\\");
const GUIString& EELobbyFn2988E9() { return sEELobbySavedGames; }

// Returns whether the game is multiplayer when the flag is set, true otherwise
bool EEIsMultiplayerGame(bool inFlag) { return inFlag ? gGameOptions.mIsMultiplayer : true; }

// (EELobbyGetVersion's two statements are EELobbyGameHost.cpp's VersionTextNew and VersionChecksum: eesplit.rs,
// R6 WON round 35)

// Returns the lobby game's host address in a multiplayer lobby game, else the protocol's local address
UWideString& EEGetLocalAddress(NEProtocol* inProtocol, unsigned long inAdapter)
{
    return *reinterpret_cast<UWideString*>(Glue(GL::LocalAddress, (intptr_t)inProtocol, (intptr_t)inAdapter));
}

// Hosts the lobby game: replaces the connection with a new game server and, once the game is published, sends the
// host's own connection request; returns whether it was published
bool EELobbyHostGame() { return Glue(GL::HostGame) != 0; }

// Posts a function for the user interface thread to call
// (its statements are GlueHost's ActionNew, UiPostAction and ActionDereference: eeglue.rs, R6 WON round 35)
void EELobbyPostAction(void (*inFunc)()) { Glue(GL::PostAction, (intptr_t)inFunc); }

// port: the game window's events the lobby passes on while it owns the event loop (close, quit, size, focus,
// exposure, the keys of its main window): handled as the game window's own (EEUserInterface::HandleEvent)
static void EELobbyForwardEvent(const PlatInputEvent& inEvent) { EEUserInterface::HandleEvent(inEvent); }

// Opens the external lobby: WONAPI core, window manager, the lobby window and the lobby screen in it; then shows it
// and runs it until it is left
// port: the window manager and window are the platform backend's (WONLobbyPlatform.h): the lobby window fills the
// lobby surface (the back buffer's size), so the original's title, icon and owner window are gone
void EELobbyOpen() { Glue(GL::Open); }

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
void EELobbyShow(bool inFlag) { Glue(GL::Show, inFlag); }

// Starts loading the hosted game: closes it to new players, counts the humans, sends the start message with a random
// seed and saves the options as the last game's
void EELobbyStartHostedGame() { Glue(GL::StartHostedGame); }

// Joins the lobby game: makes this a client, resets the game options and connects to the host's address
void EELobbyJoinGame() { Glue(GL::JoinGame); }

// Hides the lobby and gives the game back its window, resuming input and the game's cursor
// port: the reverse of EELobbyShow: the game's pointer goes where the system pointer is, the system pointer hides
// again (the game draws its own) with its default shape; typed text stays on (the game window always takes it)
void EELobbyHide() { Glue(GL::Hide); }

// Runs Sierra Update and quits the game, or tells the player it is missing
void EELobbyRunSierraUpdate() { Glue(GL::RunSierraUpdate); }

// Pushes the game form (form 30), hooking it to return to the lobby once the game is done
void EELobbyShowGameScreen() { Glue(GL::ShowGameScreen); }

void EELobbyReturnFromGame() { Glue(GL::ReturnFromGame); }
// Asks the user interface thread to return to the lobby
void EELobbyPostReturnFromGame() { EELobbyPostAction(EELobbyReturnFromGame); }

// Closes the external lobby, its windows and its WONAPI core, and restores the game's music volume
void EELobbyClose() { Glue(GL::Close); }

bool EEIsLobbyOpen() { return gEELobbyOpen; }
bool EELobbyIsShown() { return gEELobbyShown; }
void EESetQuitFlag() { Glue(GL::SetQuitFlag); }

// Asks the user interface thread to open the external lobby
void EEStartExternalLobby() { EELobbyPostAction(EELobbyOpen); }

static intptr_t GlueHost(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t)
{
    typedef EeWonEeGlueOp Op;
    EELobbySavedGame* aSave = reinterpret_cast<EELobbySavedGame*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    std::vector<LobbyNetAddress>* anAddresses = reinterpret_cast<std::vector<LobbyNetAddress>*>(a);
    FSFileSpec* aFile = reinterpret_cast<FSFileSpec*>(a);
    EEGameOptions* anOptions = reinterpret_cast<EEGameOptions*>(a);
    EEGameOptionsPlayer* aPlayer = reinterpret_cast<EEGameOptionsPlayer*>(a);
    switch (inOp) {
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameRust: return (intptr_t) reinterpret_cast<LobbyGame*>(a)->mEE;
    case Op::GameIsHost: return reinterpret_cast<LobbyGame*>(a)->GetIsHost() ? 1 : 0;
    case Op::ItemGame: return ee_won_lobby_rows_ctl_op(EeWonLobbyCtlOp::RowGame, a, 0, 0); // a game list row's (rows.rs)
    case Op::DialogRust: return (intptr_t)reinterpret_cast<LobbyRustDialog*>(a)->mLobbyDlg;
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::SetColor: g->SetColor((unsigned long)b); return 0;
    case Op::DrawText: g->DrawString(GlueText(b, c), (int)d, (int)e); return 0;
    case Op::FontHeight: return g->mFont->GetHeight();
    case Op::ListFontHeight: return reinterpret_cast<MultiListArea*>(a)->mFont->GetHeight();
    case Op::TextFind: return GlueText(b, c).findNoCase(*reinterpret_cast<const GUIString*>(d), 0);
    case Op::TextCompareNoCase: return GlueText(a, b).CompareNoCase(GlueText(c, d));
    // the column's handlers are srvlistctl.rs's calls into this glue (LobbyNetCtrlHost.cpp's EeColumn)
    case Op::SetColumnHandlers: ee_won_lobbyctl_ee_column((int)a); return 0;
    case Op::AddGameOption: LobbyAddGameOption(a ? "MapType" : "Epoch", GlueText(b, c), true); return 0;
    case Op::NumMapTypes: return (intptr_t)EERMGetNumberOfGenerators();
    case Op::SetProductName: LobbySetProductName(L"EEAOC", NULL); return 0;
    case Op::ComboClear: reinterpret_cast<ComboBox*>(a)->Clear(true); return 0;
    case Op::ComboAddMapType: reinterpret_cast<ComboBox*>(a)->AddString(GlueText((intptr_t)EeWonEeGlueText::MapType, b)); return 0;
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
    case Op::SaveOpen: return aSave->mSpec.Open(1) == keseOK ? 1 : 0;
    case Op::SaveClose: aSave->mSpec.Close(); return 0;
    case Op::SaveChecksum: return (intptr_t)aSave->mHeader.mChecksum;
    case Op::SaveOptions: aSave->mOptions = aSave->mHeader.mGameOptions; return (intptr_t)&aSave->mOptions;
    case Op::SaveFree: delete aSave; return 0;
    case Op::BoxReadError:
        LobbyMessageBox(EE_LOBBY_STRING(gEELobbyInterface_LoadGameError), EE_LOBBY_STRING(gEELobbyInterface_ErrorReadingFile));
        return 0;
    case Op::BoxNotInGame: LobbyMessageBox(EE_LOBBY_STRING(gEELobbyInterface_LoadGameError), LobbyFormatString(EE_LOBBY_STRING(gEELobbyInterface_PlayerNotInGame), EELobbyStr(a)));
        return 0;
    case Op::OptionsPlayerName: return HeldData(EELobbyToGUIString(reinterpret_cast<const EEGameOptions*>(a)->mPlayers[b].mName));
    case Op::FileName: return HeldData(GUIString(WONFile(reinterpret_cast<const char*>(a)).GetFileName()));
    case Op::FileSize: return (intptr_t)WONFile(reinterpret_cast<const char*>(a)).GetSize();
    case Op::ClientPlayer: return (intptr_t)reinterpret_cast<LobbyClient*>(a)->GetPlayer().mPtr;
    case Op::DefaultOptions: return (intptr_t)&gGamePreferences.GetSettings().mGameOptions;
    case Op::OptionMapTypeName: gGameOptions.mMapType = EELobbyToWideString(EELobbyStr(a)); return 0;
    case Op::OptionMapTypeGenerator: gGameOptions.mMapType = EERMGetGeneratorName((unsigned long)a); return 0;
    case Op::OptionsEnd: gGameOptions.mLock->WriteUnlock(); return 0;
    case Op::PlayerNew: sEELobbyPlayer = EEGameOptionsPlayer(); return 0;
    case Op::PlayerType: return (intptr_t)sEELobbyPlayer.mType;
    case Op::PlayerSetName: sEELobbyPlayer.mName = EELobbyToWideString(EELobbyStr(a)); return 0;
    case Op::PlayerCommit: gGameOptions.mPlayers[a] = sEELobbyPlayer; return 0;
    case Op::PlayerCopyTo: *reinterpret_cast<EEGameOptionsPlayer*>(a) = sEELobbyPlayer; return 0;
    case Op::LocalHost: return (intptr_t)WONAPI::IPAddr::GetLocalHost();
    case Op::AddressesCount: return (intptr_t)anAddresses->size();
    case Op::AddressHandle: return (intptr_t)(*anAddresses)[b].mHandle;
    case Op::AddressesFree: delete anAddresses; return 0;
    case Op::SetNetworkAdapter: gNetworkAdapter = (unsigned long)a; return 0;
    case Op::FormHide: reinterpret_cast<UIForm*>(a)->GetControl((long)b)->SetDisplayMode(0); return 0;
    case Op::IsLobbyOpen: return EEIsLobbyOpen() ? 1 : 0;
    case Op::IsShown: return EELobbyIsShown() ? 1 : 0;
    case Op::PostReturnFromGame: EELobbyPostReturnFromGame(); return 0;
    case Op::LobbyRun: return LobbyRun();
    case Op::PostStartHostedGame: EELobbyPostAction(EELobbyStartHostedGame); return 0;
    case Op::ShowGameScreen: EELobbyShowGameScreen(); return 0;
    case Op::LobbySuspend: LobbySuspend(); return 0;
    case Op::Hide: EELobbyHide(); return 0;
    case Op::JoinGame: EELobbyJoinGame(); return 0;
    case Op::SierraUpdate: EELobbyRunSierraUpdate(); return 0;
    case Op::Show: EELobbyShow(true); return 0;
    case Op::GameForm: return (intptr_t)gUIManager->GetFormByID(30);
    case Op::CurrentForm: return (intptr_t)gUIManager->GetCurrentForm();
    case Op::PopForm: gUIManager->Pop(); return 0;
    case Op::LobbyResume: LobbyResume(); return 0;
    // the statements of the lobby window's functions (R6 WONLobby round 29: eewindow.rs)
    case Op::SetOpenTrue: gEELobbyOpen = true;
        return 0;
    case Op::SetOpenFalse: gEELobbyOpen = false;
        return 0;
    case Op::CoreNew: gEELobbyWindowManager = new WONAPICoreEx;
        return 0;
    case Op::CoreSetPumpThread: gEELobbyWindowManager->SetDoPumpThread(true);
        return 0;
    case Op::CoreStartup: gEELobbyWindowManager->Startup();
        return 0;
    case Op::RootNew: gEELobbyRootWindow = new PlatWindowManager;
        return 0;
    case Op::RootNoPopupWindows: gEELobbyRootWindow->SetPopupsInOwnWindow(false);
        return 0;
    case Op::RootAddJpg: JPGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
        return 0;
    case Op::RootAddPng: PNGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
        return 0;
    case Op::RootSoundDecoder: gEELobbyRootWindow->SetSoundDecoder(new EELobbySoundDecoder);
        return 0;
    case Op::SecondarySurface: return (intptr_t)gRasterizer->GetSecondarySurface(gScreenIndex);
    case Op::RootSetSurfaceSize: gEELobbyRootWindow->SetSurfaceSize(kEELobbySurfaceWidth, kEELobbySurfaceHeight);
        return 0;
    case Op::RootSetEventForward: gEELobbyRootWindow->SetEventForward(EELobbyForwardEvent);
        return 0;
    case Op::WindowNew: gEELobbyPlatformWindow = CreateNativeWindow();
        return 0;
    case Op::WindowForwardKeys: static_cast<PlatGUIWindow*>(gEELobbyPlatformWindow.get())->SetForwardKeyMsgToParent(true);
        return 0;
    case Op::FullScreen: return gGamePreferences.mSettings.mFullScreen ? 1 : 0;
    case Op::WindowSetStyle: gEELobbyPlatformWindow->SetStyle((long)a);
        return 0;
    case Op::WindowCreate: gEELobbyPlatformWindow->Create(WONRectangle(0, 0, 100, 100));
        return 0;
    case Op::GlueInit: EELobbyInit();
        return 0;
    case Op::ScreenHold: return (intptr_t)LobbyGetContainer()->CreateRef();
    case Op::RootLayoutScreen: gEELobbyPlatformWindow->mRoot->Fn2F6140(reinterpret_cast<Container*>(a), 3, gEELobbyPlatformWindow->mRoot.get(), 0, 0, 0, 0, 0);
        return 0;
    case Op::RootAddScreen: gEELobbyPlatformWindow->mRoot->AddChild(reinterpret_cast<Container*>(a));
        return 0;
    case Op::LobbyStart: LobbyStart();
        return 0;
    case Op::WindowExists: return gEELobbyPlatformWindow.get() != NULL;
    case Op::GameOsWindow: return (intptr_t)gGameWindow->GetOSRepresentation();
    case Op::MouseExists: return gMouse != NULL;
    case Op::MousePosition: *reinterpret_cast<GE2DPoint*>(a) = gMouse->mPosition;
        return 0;
    case Op::MouseSetPointer: gMouse->mPointer = (long)a;
        return 0;
    case Op::SurfaceWidth: return (long)reinterpret_cast<GESurface*>(a)->GetWidth();
    case Op::SurfaceHeight: return (long)reinterpret_cast<GESurface*>(a)->GetHeight();
    case Op::WindowShow: gEELobbyPlatformWindow->Show(a != 0);
        return 0;
    case Op::LobbyLock: EELobbyLock();
        return 0;
    case Op::LobbyUnlock: EELobbyUnlock();
        return 0;
    case Op::SetShown: gEELobbyShown = a != 0;
        return 0;
    case Op::KeyboardExists: return gISKeyboard != NULL;
    case Op::KeyboardSuspend: gISKeyboard->Suspend();
        return 0;
    case Op::KeyboardResume: gISKeyboard->Resume();
        return 0;
    case Op::MouseInputExists: return gISMouse != NULL;
    case Op::MouseInputSuspend: gISMouse->Suspend();
        return 0;
    case Op::MouseInputResume: gISMouse->Resume();
        return 0;
    case Op::TextInputStart: PlatTextInputStart(reinterpret_cast<PlatWindow*>(a), 0, 0, 0, 0);
        return 0;
    case Op::CursorShow: PlatCursorShow((int)a);
        return 0;
    case Op::CursorSetDefault: PlatCursorSet(NULL);
        return 0;
    case Op::ClientSize: PlatWindowGetClientSize(reinterpret_cast<PlatWindow*>(a), &reinterpret_cast<int32_t*>(b)[0], &reinterpret_cast<int32_t*>(b)[1]);
        return 0;
    case Op::MouseWarp: PlatMouseWarp(reinterpret_cast<PlatWindow*>(a), (int32_t)b, (int32_t)c);
        return 0;
    case Op::MouseGetState: PlatMouseGetState(reinterpret_cast<PlatWindow*>(a), &reinterpret_cast<int32_t*>(b)[0], &reinterpret_cast<int32_t*>(b)[1]);
        return 0;
    case Op::WindowInvalidateRoot: gEELobbyPlatformWindow->InvalidateRoot();
        return 0;
    case Op::RedrawAll: gUIManager->RedrawAll();
        return 0;
    case Op::MouseSetPosition: gMouse->SetPosition(GE2DPoint(reinterpret_cast<const float*>(a)[0], reinterpret_cast<const float*>(a)[1]));
        return 0;
    case Op::RootCloseAll: gEELobbyRootWindow->CloseAllWindows();
        return 0;
    case Op::GlueLobbyShutdown: LobbyShutdown();
        return 0;
    case Op::CoreShutdown: gEELobbyWindowManager->Shutdown();
        return 0;
    case Op::CoreDelete: delete gEELobbyWindowManager;
        return 0;
    case Op::CoreClear: gEELobbyWindowManager = NULL;
        return 0;
    case Op::WindowClear: gEELobbyPlatformWindow = NULL;
        return 0;
    case Op::RootClear: gEELobbyRootWindow = NULL;
        return 0;
    case Op::HasUiManager: return gUIManager != NULL;
    case Op::MusicVolumeRestore: gUIManager->SetMusicVolume(gGamePreferences.GetSettings().GetMusicVolume());
        return 0;
    case Op::GameDoneEventNew: return (intptr_t)new EELobbyGameDoneEvent;
    case Op::GameFormSetOpenEvent: gUIManager->GetFormByID(30)->SetFormOpenEvent(reinterpret_cast<EELobbyGameDoneEvent*>(a));
        return 0;
    case Op::EventDereference: reinterpret_cast<EELobbyGameDoneEvent*>(a)->Dereference();
        return 0;
    case Op::PushGameForm: gUIManager->Push(gUIManager->GetFormByID(30));
        return 0;
    // the statements of the network game functions (R6 WONLobby round 30: eegamefn.rs)
    case Op::HostMeNew: sEELobbyHostMe = EEGameOptionsPlayer();
        return 0;
    case Op::HostMe: return (intptr_t)&sEELobbyHostMe;
    case Op::SetPlayerNameFromClient: gPlayerName = EELobbyToWideString(GetMyLobbyClient()->GetName());
        return 0;
    case Op::BehindFirewall: return LobbyIsBehindFirewall() ? 1 : 0;
    case Op::CommExists: return gCommunications != NULL;
    case Op::CommDelete: delete gCommunications;
        return 0;
    case Op::CommClear: gCommunications = NULL;
        return 0;
    case Op::CommIsServer: return gCommunications->IsServer() == true;
    case Op::ServerNew: gCommunications = new EEServer(UWideString("TestGame"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, false);
        return 0;
    case Op::ServerStartThreads: static_cast<EEServer*>(gCommunications)->StartThreads();
        return 0;
    case Op::ServerPublished: return static_cast<EEServer*>(gCommunications)->GetGameWasPublished() ? 1 : 0;
    case Op::HostMeReady: sEELobbyHostMe.mIsReady = true;
        return 0;
    case Op::HostMeHuman: sEELobbyHostMe.mType = 1;
        return 0;
    case Op::HostRequest: gCommunications->ProcessLocalUserInput(new NEMRecord(kNetMessageClientConnectionRequest, {a != 0, gBinaryChecksum}, {gGameVersion, sEELobbyHostMe.mName}), 0xff);
        return 0;
    case Op::ClientNew: gCommunications = new EEClient;
        return 0;
    case Op::ClientStartThreads: static_cast<EEClient*>(gCommunications)->StartThreads();
        return 0;
    case Op::ClientStartThreadsDynamic: dynamic_cast<EEClient*>(gCommunications)->StartThreads();
        return 0;
    case Op::JoinSetService: sEELobbyJoinService = "EEService";
        return 0;
    case Op::JoinSetGameName: sEELobbyJoinGameName = gEmptyString;
        return 0;
    case Op::JoinSetAddress: sEELobbyJoinAddress = GetMyLobbyGame()->GetAddress().GetHostString(true).c_str();
        return 0;
    case Op::JoinSetName: sEELobbyJoinName = EELobbyToWideString(GetMyLobbyClient()->GetName());
        return 0;
    case Op::JoinConnect: return static_cast<NESynchronousClient*>(gCommunications)->Connect(sEELobbyJoinAddress, sEELobbyJoinService, sEELobbyJoinGameName) == 0;
    case Op::JoinRequestNew: sEELobbyJoinRequest = new NEMRecord(kNetMessageClientConnectionRequest, {a != 0, gBinaryChecksum}, {gGameVersion, sEELobbyJoinName});
        return 0;
    case Op::JoinSendStart: static_cast<NESynchronousClient*>(gCommunications)->SendStartConnectMessage(sEELobbyJoinRequest);
        return 0;
    case Op::JoinProcessRequest: gCommunications->ProcessLocalUserInput(sEELobbyJoinRequest, 0xff);
        return 0;
    case Op::OptionsReadLock: gGameOptions.mLock->ReadLock();
        return 0;
    case Op::StartTimeSet: sEELobbyStartTime = TMGetTime();
        return 0;
    case Op::ServerAllowNewPlayersOff: static_cast<EEServer*>(gCommunications)->AllowNewPlayers(false);
        return 0;
    case Op::ServerSetLoadingState: static_cast<EEServer*>(gCommunications)->SetGameState(2);
        return 0;
    case Op::ServerClearLoadResponses: static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
        return 0;
    case Op::StartPlayerCopy: sEELobbyStartPlayer = gWorld.GetLobbyOptions().mPlayers[a];
        return 0;
    case Op::StartPlayerType: return (intptr_t)sEELobbyStartPlayer.mType;
    case Op::ServerStopPing: static_cast<EEServer*>(gCommunications)->StopPing();
        return 0;
    case Op::ServerResetXfer: static_cast<EEServer*>(gCommunications)->ResetXfer();
        return 0;
    case Op::ServerSetLoadState: static_cast<EEServer*>(gCommunications)->mLoadState = true;
        return 0;
    case Op::SeedRandom: srand(TMGetTime());
        return 0;
    case Op::StartLoading: gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageStartLoading, {(unsigned char)a, rand()}), 0xff);
        return 0;
    case Op::SetLastGameOptions: gGamePreferences.SetLastGameOptions(gGameOptions);
        return 0;
    case Op::OptionsReadUnlock: gGameOptions.mLock->ReadUnlock();
        return 0;
    case Op::SierraFileNew: return (intptr_t)new FSFileSpec;
    case Op::MessageNew: return (intptr_t)new UWideString;
    case Op::SierraSetVolume: aFile->SetVolume(gStartupVolume);
        return 0;
    case Op::SierraSetDirectory: aFile->SetDirectory(gStartupDirectory + UString("Sierra Update\\"));
        return 0;
    case Op::SierraSetFile: aFile->SetFile("SierraUp.exe");
        return 0;
#ifdef _WIN32   // Sierra Update is a Windows program (elsewhere the file is never opened)
    case Op::SierraOpen: return aFile->Open(1) == keseOK;
    case Op::SierraClose: aFile->Close();
        return 0;
    case Op::SierraLaunch: return (INT_PTR)ShellExecuteA(GetDesktopWindow(), "open", aFile->GetVolume() + aFile->GetDirectory() + aFile->GetFile(), NULL, aFile->GetVolume() + aFile->GetDirectory(), SW_SHOWNORMAL) > 32;
#endif
    case Op::HasUserInterface: return gUserInterface != NULL;
    case Op::PostQuit: gUserInterface->GetThread()->Post(0x12, NULL, NULL, false);
        return 0;
    case Op::SierraFailMessage: gResource->GetString(IDS_UNABLE_TO_LAUNCH_UPDATE_UTILITY, *reinterpret_cast<UWideString*>(a));
        return 0;
    case Op::MessageShow: gUIManager->DisplayMessage(*reinterpret_cast<UWideString*>(a), true, NULL, 0, true);
        return 0;
    case Op::MessageFree: delete reinterpret_cast<UWideString*>(a);
        return 0;
    case Op::SierraFileFree: delete aFile;
        return 0;
    case Op::IsMultiplayerGame: return EEIsMultiplayerGame(true) ? 1 : 0;
    case Op::ProtocolLocalAddress: return (intptr_t)&reinterpret_cast<NEProtocol*>(a)->GetLocalAddress((unsigned long)b);
    case Op::GameAddressSet: sEELocalAddress = reinterpret_cast<LobbyGame*>(a)->GetAddress().GetHostString(true).c_str();
        return 0;
    case Op::GameAddressRef: return (intptr_t)&sEELocalAddress;
    // the statements of the cases gluesplit.rs composes (R6 WON round 32)
    case Op::TextHeld: return HeldData(GlueText(a, b));
    case Op::GfxTextWidthHeld: return g->mFont->GetStringWidth(GUIString(reinterpret_cast<GUIStringData*>(b)), 0, reinterpret_cast<GUIStringData*>(b)->mLength);
    case Op::FactoryGame: LobbyGame::SetGameFactory(EELobbyNewGame); return 0;
    case Op::FactoryPlayer: LobbyPlayer::SetPlayerFactory(EELobbyNewPlayer); return 0;
    case Op::FactoryConfig: LobbyConfig::SetAllocator(EELobbyNewConfig); return 0;
    case Op::ResInit: sEELobbyResources = EELobbyResource_Init(NULL); return 0;
    case Op::ResAllocGameOptions: sEELobbyResources->AddAllocator("GameOptionsCtrl", EELobbyNewContainerConfig, EELobbyNewGameOptionsCtrl); return 0;
    case Op::ResAllocPlayerOptions: sEELobbyResources->AddAllocator("PlayerOptionsCtrl", EELobbyNewContainerConfig, EELobbyNewPlayerOptionsCtrl); return 0;
    case Op::ResAllocColorButton: sEELobbyResources->AddAllocator("EELobbyColorButton", EELobbyNewButtonConfig, EELobbyNewColorButton); return 0;
    case Op::ResAllocFileDialog: sEELobbyResources->AddAllocator("EELobbyFileDialog", EELobbyNewDialogConfig, NewLobbyDialog<EeWonLobbyDlgKind::File>); return 0;
    case Op::ResAllocConnectDialog: sEELobbyResources->AddAllocator("EELobbyConnectDialog", EELobbyNewDialogConfig, NewLobbyDialog<EeWonLobbyDlgKind::Connect>); return 0;
    // EELobbyAddResources: the LobbyGlobal section with the lobby music
    case Op::ResAddMusic: sEELobbyResources->AddConfig("LobbyGlobal", NULL)->AddSound("LobbyMusic", new EELobbyMusic(SoundDescriptor("", 2))); return 0;
    case Op::ResAddLoader: LobbyAddComponentLoader(sEELobbyResources.mPtr, EELobbyGetComponents); return 0;
    case Op::ResDone: sEELobbyResources = NULL; return 0;
    case Op::PlayerColorGet: sEELobbyColor = EEGetPlayerColor((long)a); return 0;
    case Op::PlayerColorRed: *reinterpret_cast<float*>(a) = sEELobbyColor.GetRed(); return 0;
    case Op::PlayerColorGreen: *reinterpret_cast<float*>(a) = sEELobbyColor.GetGreen(); return 0;
    case Op::PlayerColorBlue: *reinterpret_cast<float*>(a) = sEELobbyColor.GetBlue(); return 0;
    case Op::FindConnectDialog: return FindHeldDialog(reinterpret_cast<ComponentConfig*>(a), EeWonLobbyDlgKind::Connect, "EELobbyConnectDialog", 329);
    case Op::FindFileDialog: return FindHeldDialog(reinterpret_cast<ComponentConfig*>(a), EeWonLobbyDlgKind::File, "EELobbyFileDialog", 328);
    case Op::SaveAlloc: return (intptr_t)new EELobbySavedGame;
    case Op::SaveSetFile: aSave->mSpec.SetFile(reinterpret_cast<const char*>(b)); return 0;
    case Op::SaveLengthClear: sEELobbySaveLength = 0; return 0;
    case Op::SaveGetLength: aSave->mSpec.GetLength(sEELobbySaveLength); return 0;
    case Op::SaveLengthValue: return (intptr_t)sEELobbySaveLength;
    case Op::SaveSetMajor: aSave->mHeader.SetMajorVersion(kSaveGameMajorVersion); return 0;
    case Op::SaveSetMinor: aSave->mHeader.SetMinorVersion(kSaveGameMinorVersion); return 0;
    case Op::SaveLoadFile: return aSave->mHeader.LoadFromFile(aSave->mSpec) == keseOK ? 1 : 0;
    case Op::OptionsWriteLock: gGameOptions.mLock->WriteLock(); return 0;
    case Op::OptionsCopyLobby: gGameOptions = gLobbyGameOptions; return 0;
    case Op::OptionsSetMultiplayer: gGameOptions.mIsMultiplayer = true; return 0;
    case Op::OptionsClearCheats: gGameOptions.mCheatCodes = false; return 0;
    case Op::AddressListFill: LobbyGetNetAddresses(sEELobbyAddressList, false); return 0;
    case Op::AddressVectorNew: return (intptr_t)new std::vector<LobbyNetAddress>(sEELobbyAddressList.begin(), sEELobbyAddressList.end());
    case Op::AddressListClear: sEELobbyAddressList.clear(); return 0;
    case Op::WorldWriteUnlock: gWorld.mLock->WriteUnlock(); return 0;
    case Op::UiUnlockCopy: gUIManager->UnlockUIForCopyData(); return 0;
    case Op::UiLockCopy: gUIManager->LockUIForCopyData(); return 0;
    case Op::WorldWriteLock: gWorld.mLock->WriteLock(); return 0;
    // StringUnits' (gluesplit.rs composes it, R6 WON round 33)
    case Op::StringLength: return (intptr_t)reinterpret_cast<GUIStringData*>(a)->mLength;
    case Op::StringUnitAt: return (intptr_t)(uint32_t)reinterpret_cast<GUIStringData*>(a)->mText[b];
    case Op::ActionNew: return (intptr_t) new EEALobbyAction(reinterpret_cast<void (*)()>(a));
    case Op::UiPostAction: gUserInterface->PostAction(reinterpret_cast<EEALobbyAction*>(a)); return 0;
    case Op::ActionDereference: reinterpret_cast<EEALobbyAction*>(a)->Dereference(); return 0;
    case Op::BoxDissolved: LobbyMessageBox(LOBBY_STRING(RoutingLogic_GameDissolvedTitle_String), LOBBY_STRING(RoutingLogic_GameDissolvedDetails_String)); return 0;
    // the fields of the options and players (gluesplit.rs maps the field to its op, R6 WON round 35)
    case Op::OptGetMapSize: return (long)anOptions->mMapSize;
    case Op::OptGetResources: return (long)anOptions->mResources;
    case Op::OptGetEpochStart: return (long)anOptions->mEpochStart;
    case Op::OptGetEpochEnd: return (long)anOptions->mEpochEnd;
    case Op::OptGetVariant: return (long)anOptions->mVariant;
    case Op::OptGetDifficulty: return (long)anOptions->mDifficulty;
    case Op::OptGetGameSpeed: return (long)anOptions->mGameSpeed;
    case Op::OptGetUnitLimit: return (long)anOptions->mUnitLimit;
    case Op::OptGetWondersForVictory: return (long)anOptions->mWondersForVictory;
    case Op::OptGetRevealMap: return (long)anOptions->mRevealMap;
    case Op::OptGetAllowCustomCivs: return (long)anOptions->mAllowCustomCivs;
    case Op::OptGetLockTeams: return (long)anOptions->mLockTeams;
    case Op::OptGetLockSpeed: return (long)anOptions->mLockSpeed;
    case Op::OptGetVictory: return (long)anOptions->mVictory;
    case Op::MapIndexClear: sEELobbyMapIndex = 0; return 0;
    case Op::OptGetMapTypeIndex: EERMFindGenerator(anOptions->mMapType, sEELobbyMapIndex); return (long)sEELobbyMapIndex;
    case Op::OptSetLoadType: gGameOptions.mLoadType = (long)a; return 0;
    case Op::OptSetMapSize: gGameOptions.mMapSize = (long)a; return 0;
    case Op::OptSetResources: gGameOptions.mResources = (long)a; return 0;
    case Op::OptSetEpochStart: gGameOptions.mEpochStart = (long)a; return 0;
    case Op::OptSetEpochEnd: gGameOptions.mEpochEnd = (long)a; return 0;
    case Op::OptSetVariant: gGameOptions.mVariant = (long)a; return 0;
    case Op::OptSetDifficulty: gGameOptions.mDifficulty = (long)a; return 0;
    case Op::OptSetGameSpeed: gGameOptions.mGameSpeed = (long)a; return 0;
    case Op::OptSetUnitLimit: gGameOptions.mUnitLimit = (long)a; return 0;
    case Op::OptSetWondersForVictory: gGameOptions.mWondersForVictory = (long)a; return 0;
    case Op::OptSetRevealMap: gGameOptions.mRevealMap = a != 0; return 0;
    case Op::OptSetAllowCustomCivs: gGameOptions.mAllowCustomCivs = a != 0; return 0;
    case Op::OptSetLockTeams: gGameOptions.mLockTeams = a != 0; return 0;
    case Op::OptSetLockSpeed: gGameOptions.mLockSpeed = a != 0; return 0;
    case Op::OptSetVictory: gGameOptions.mVictory = a != 0; return 0;
    case Op::OptionsPlayerAt: return (intptr_t)&anOptions->mPlayers[b];
    case Op::PlayerGetType: return (long)aPlayer->mType;
    case Op::PlayerGetLoadedType: return (long)aPlayer->mLoadedType;
    case Op::PlayerGetSlot: return (long)aPlayer->mSlot;
    case Op::PlayerGetColor: return (long)aPlayer->mColor;
    case Op::PlayerGetTeam: return (long)aPlayer->mTeam;
    case Op::PlayerGetStartingCitizens: return (long)aPlayer->mStartingCitizens;
    case Op::PlayerGetIsLocal: return (long)aPlayer->mIsLocal;
    case Op::PlayerGetIsReady: return (long)aPlayer->mIsReady;
    case Op::PlayerGetB36: return (long)aPlayer->mB36;
    case Op::PlayerGetNameEmpty: return aPlayer->mName == "" ? 1 : 0;
    case Op::PlayerSetType: sEELobbyPlayer.mType = (long)a; return 0;
    case Op::PlayerSetLoadedType: sEELobbyPlayer.mLoadedType = (long)a; return 0;
    case Op::PlayerSetSlot: sEELobbyPlayer.mSlot = (long)a; return 0;
    case Op::PlayerSetColor: sEELobbyPlayer.mColor = (long)a; return 0;
    case Op::PlayerSetTeam: sEELobbyPlayer.mTeam = (long)a; return 0;
    case Op::PlayerSetStartingCitizens: sEELobbyPlayer.mStartingCitizens = (long)a; return 0;
    case Op::PlayerSetIsLocal: sEELobbyPlayer.mIsLocal = a != 0; return 0;
    case Op::PlayerSetIsReady: sEELobbyPlayer.mIsReady = a != 0; return 0;
    case Op::PlayerSetB36: sEELobbyPlayer.mB36 = a != 0; return 0;
    }
    return 0;
}

// the host from the start: EELobbyOpen's statements are the Rust's before EELobbyInit sets it again (R6 WONLobby round 29)
[[maybe_unused]] static const bool sGlueHostSet = (ee_won_eeglue_set_game_host(GlueHost), true);

// ---- the entry points (EELobbyInit.cpp's and EELobbyMisc.cpp's; the Rust's)

// Sets the lobby up for Empire Earth: the server list's epoch and map type columns, the factories, the game's
// components, resources, epoch and map type options, player colours and the product name
void EELobbyInit()
{
    ee_won_eeglue_set_game_host(GlueHost);
    Glue(GL::Init);
}
void EELobbyShutdown() { Glue(GL::Shutdown); }
unsigned short EELobbyGetFileTransferPort() { return (unsigned short)Glue(GL::FileTransferPort); }
LobbyRustDialog* EELobbyGetFileDialog() { return reinterpret_cast<LobbyRustDialog*>(Glue(GL::FileDialog)); }
bool EELobbyRunConnectDialog() { return Glue(GL::RunConnectDialog) != 0; }
void EELobbySetSlotConnected(unsigned char inSlot, bool inConnected) { Glue(GL::SetSlotConnected, inSlot, inConnected); }
void EELobbyCancelConnectDialog() { Glue(GL::CancelConnectDialog); }
void EELobbyPostConnectStatus(bool inConnected) { Glue(GL::PostConnectStatus, inConnected); }
bool EELobbyIsConnectDialogRunning() { return Glue(GL::IsConnectDialogRunning) != 0; }
bool EELobbyGetSavedGameInfo(const char* inFile, uint32_t& outLength, uint32_t& outChecksum)
{
    return Glue(GL::SavedGameInfo, (intptr_t)inFile, (intptr_t)&outLength, (intptr_t)&outChecksum) != 0;
}
bool EELobbyLoadSavedGame(const char* inFile) { return Glue(GL::LoadSavedGame, (intptr_t)inFile) != 0; }
void EELobbyFillMapTypeCombo(ComboBox* inCombo) { Glue(GL::FillMapTypeCombo, (intptr_t)inCombo); }
void EELobbySetDefaultGameOptions(LobbyGame* inGame) { Glue(GL::SetDefaultGameOptions, (intptr_t)inGame); }
unsigned long EELobbyGetNumMapTypes() { return EERMGetNumberOfGenerators(); }
