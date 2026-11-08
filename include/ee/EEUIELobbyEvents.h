// C:\EEX\Empire Earth\UI Events\EEUIELobbyEvents.h (RECONSTRUCTION; header and file names inferred, from the class
// prefix EEUIELE). The game lobby's events. ProcessEvents:
// EEUIELobbyEvents.cpp (0x1d9dcf..0x1e0e1b). Declaration order = the order of the ??_G VC6 emits after each creator
// (CancelLoad .. LoadScenarioGame after the two load screens, IncChangeColor .. ChangeReady after 0x1db7a7, the game
// option events after 0x1dbad8, ChatKeyboardFocus .. Start after ActivateMultiPlayerServer, the two Open* after
// GameType). The ActivateMultiPlayer* ??_G are elsewhere (0x2713/0x273e), ActivateSinglePlayer's at 0x1e8e52.
// Requires lle/UIEvent.h.
#ifndef EE_EEUIELOBBYEVENTS_H
#define EE_EEUIELOBBYEVENTS_H

class EEUIELECancelLoad : public UIEvent {              // vtable 0x4464a8; ??_G 0x1da0c5; ProcessEvent 0x1d9dcf
public:
    virtual ~EEUIELECancelLoad() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    void SetLoadType(long inLoadType) { this->mLoadType = inLoadType; }  // GameType: the argument is loaded first

    long mLoadType;                         // +0x0c not set by the (implicit) ctor; CancelLoad restores it
};

class EEUIELELoadSavedGame : public UIEvent {           // vtable 0x446494; ??_G 0x1da0f0; ProcessEvent 0x1d9e19
public:
    virtual ~EEUIELELoadSavedGame() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELELoadScenarioGame : public UIEvent {        // vtable 0x4464bc; ??_G 0x1da3ff; ProcessEvent 0x1da11b
public:
    virtual ~EEUIELELoadScenarioGame() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEIncChangeColor : public UIEvent {          // vtable 0x446540; ??_G 0x1db91a; ProcessEvent 0x1dad59
public:
    EEUIELEIncChangeColor(long inSlot) { this->mSlot = inSlot; }
    void SetSlot(unsigned long inSlot) { this->mSlot = inSlot; }
    virtual ~EEUIELEIncChangeColor() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    unsigned long mSlot;                    // +0x0c the colour index 0..15
};

class EEUIELEDecChangeColor : public UIEvent {          // vtable 0x44652c; ??_G 0x1db945; ProcessEvent 0x1dae32
public:
    EEUIELEDecChangeColor(long inSlot) { this->mSlot = inSlot; }
    void SetSlot(unsigned long inSlot) { this->mSlot = inSlot; }
    virtual ~EEUIELEDecChangeColor() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    unsigned long mSlot;                    // +0x0c the colour index 0..15
};

class EEUIELEChangeTeam : public UIEvent {              // vtable 0x446518; ??_G 0x1db970; ProcessEvent 0x1daf10
public:
    virtual ~EEUIELEChangeTeam() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEChangePlayerType : public UIEvent {        // vtable 0x446554; ??_G 0x1db99b; ProcessEvent 0x1dafb8
public:
    virtual ~EEUIELEChangePlayerType() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEChangeReady : public UIEvent {             // vtable 0x446504; ??_G 0x1db9c6; ProcessEvent 0x1db358
public:
    virtual ~EEUIELEChangeReady() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEChangeCitizenBonus : public UIEvent {      // vtable 0x446568; ??_G 0x1dbaad; ProcessEvent 0x1da42a
public:
    virtual ~EEUIELEChangeCitizenBonus() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// port: the host sets a computer slot's engine from its engine box (kLobbyEngineCombo + slot; ee/EECPEngine.h)
class EEUIELEChangeEngine : public UIEvent {
public:
    virtual ~EEUIELEChangeEngine() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameType : public UIEvent {                // vtable 0x4466d0; ??_G 0x1dbfe8; ProcessEvent 0x1dfff4
public:
    virtual ~EEUIELEGameType() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameMapSize : public UIEvent {             // vtable 0x4466bc; ??_G 0x1dc013; ProcessEvent 0x1dff58
public:
    virtual ~EEUIELEGameMapSize() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameMapType : public UIEvent {             // vtable 0x4466a8; ??_G 0x1dc03e; ProcessEvent 0x1e0831
public:
    virtual ~EEUIELEGameMapType() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameResources : public UIEvent {           // vtable 0x446694; ??_G 0x1dc069; ProcessEvent 0x1e0783
public:
    virtual ~EEUIELEGameResources() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameEpochStart : public UIEvent {          // vtable 0x446680; ??_G 0x1dc094; ProcessEvent 0x1e066a
public:
    virtual ~EEUIELEGameEpochStart() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEEpochEnd : public UIEvent {                // vtable 0x44666c; ??_G 0x1dc0bf; ProcessEvent 0x1e05be
public:
    virtual ~EEUIELEEpochEnd() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameUnitLimit : public UIEvent {           // vtable 0x446658; ??_G 0x1dc0ea; ProcessEvent 0x1e04ea
public:
    virtual ~EEUIELEGameUnitLimit() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameWondersForVictory : public UIEvent {   // vtable 0x446644; ??_G 0x1dc115; ProcessEvent 0x1e044e
public:
    virtual ~EEUIELEGameWondersForVictory() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameVariant : public UIEvent {             // vtable 0x446630; ??_G 0x1dc140; ProcessEvent 0x1e0391
public:
    virtual ~EEUIELEGameVariant() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameDifficulty : public UIEvent {          // vtable 0x44661c; ??_G 0x1dc16b; ProcessEvent 0x1e02e5
public:
    virtual ~EEUIELEGameDifficulty() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameSpeed : public UIEvent {               // vtable 0x446608; ??_G 0x1dc196; ProcessEvent 0x1e0249
public:
    virtual ~EEUIELEGameSpeed() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameRecordGames : public UIEvent {         // vtable 0x4465f4; ??_G 0x1dc1c1; ProcessEvent 0x1dfc98
public:
    virtual ~EEUIELEGameRecordGames() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameTeamIslands : public UIEvent {         // vtable 0x4465cc; ??_G 0x1dc1ec; ProcessEvent 0x1dfbe8
public:
    virtual ~EEUIELEGameTeamIslands() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameRevealMap : public UIEvent {           // vtable 0x4465e0; ??_G 0x1dc217; ProcessEvent 0x1dfb38
public:
    virtual ~EEUIELEGameRevealMap() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameLockTeams : public UIEvent {           // vtable 0x4465b8; ??_G 0x1dc242; ProcessEvent 0x1dfd48
public:
    virtual ~EEUIELEGameLockTeams() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameLockSpeed : public UIEvent {           // vtable 0x4465a4; ??_G 0x1dc26d; ProcessEvent 0x1dfea8
public:
    virtual ~EEUIELEGameLockSpeed() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEGameCheatCodes : public UIEvent {          // vtable 0x446590; ??_G 0x1dc298; ProcessEvent 0x1dfdf8
public:
    virtual ~EEUIELEGameCheatCodes() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEChangeVictory : public UIEvent {           // vtable 0x44657c; ??_G 0x1dc2c3; ProcessEvent 0x1db63c
public:
    virtual ~EEUIELEChangeVictory() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEChatKeyboardFocus : public UIEvent {       // vtable 0x4466e4; ??_G 0x1df3a8; ProcessEvent 0x1df38e
public:
    virtual ~EEUIELEChatKeyboardFocus() {}
    // inline: emitted with the ??_G after EEUIELEActivateMultiPlayerServer (0x1df38e)
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        if (!inForm->GetIsKeyboardCaptured())
            inForm->SetTabStop(9);
        return 0;
    }
};

class EEUIELECancel : public UIEvent {                  // vtable 0x44670c; ??_G 0x1df3d3; ProcessEvent 0x1da4db
public:
    virtual ~EEUIELECancel() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEChat : public UIEvent {                    // vtable 0x446720; ??_G 0x1df42d; ProcessEvent 0x1da5a2
public:
    EEUIELEChat() {}                        // out of line 0x1df3fe where ActivateMultiPlayerServer refused it
    virtual ~EEUIELEChat() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    UWideString mPrefix;                    // +0x0c
};

class EEUIELEStart : public UIEvent {                   // vtable 0x4466f8; ??_G 0x1df461; ProcessEvent 0x1da6b3
public:
    virtual ~EEUIELEStart() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEOpenLoadSavedGame : public UIEvent {       // vtable 0x446734; ??_G 0x1e01f3; ProcessEvent 0x1d9e82
public:
    virtual ~EEUIELEOpenLoadSavedGame() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEOpenLoadScenarioGame : public UIEvent {    // vtable 0x446748; ??_G 0x1e021e; ProcessEvent 0x1da184
public:
    virtual ~EEUIELEOpenLoadScenarioGame() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEActivateMultiPlayerServer : public UIEvent {// vtable 0x438c18; ??_G 0x2713; ProcessEvent 0x1df1cc
public:
    virtual ~EEUIELEActivateMultiPlayerServer() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEActivateMultiPlayerClient : public UIEvent {// vtable 0x438c2c; ??_G 0x273e; ProcessEvent 0x1df874
public:
    virtual ~EEUIELEActivateMultiPlayerClient() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIELEActivateSinglePlayer : public UIEvent {    // vtable 0x446ab4; ??_G 0x1e8e52; ProcessEvent 0x1df9e8
public:
    virtual ~EEUIELEActivateSinglePlayer() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

#endif
