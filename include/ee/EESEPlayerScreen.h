// C:\EEX\Empire Earth\Scenario Editor\EESEPlayerScreen.h (RECONSTRUCTION; file name inferred). The editor's player
// tab (RTTI EESEPlayerScreen > EEUIFSEMainGroup; vtable 0x4430dc; ctor 0x173313): the editor's group 5. Whole in
// Rust (ee_screens editor_players.rs, with its player rows and game options page): the class holds the object and
// its members forward. Requires ee/EEUIFSEMainGroup.h.
#ifndef EE_EESEPLAYERSCREEN_H
#define EE_EESEPLAYERSCREEN_H

class EEPlayer;
class UIComboBox;
class UIForm;
class GEFont;
struct EeScreensSePlayers;

class EESEPlayerScreen : public EEUIFSEMainGroup {
public:
    EESEPlayerScreen(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont, UIComboBox* inPlayerSelection); //173313
    virtual ~EESEPlayerScreen(); //1733bf
    virtual void Leave();     //1737ac
    virtual void Load();      //175c0b
    virtual void Enter();     //175c48
    virtual void Store();     //175d7e
    // Actual vtable+24 points157269, independently disassembled ret4.
    virtual void MouseMove(const GE2DPoint& inLocation) {}
    void Refresh();                         // 0x175a3e
    // 0x175a1f: the max-players combo box's list selection + 2 (EESETSCondition::Activate)
    unsigned long GetPlayerCount();
    // 0x173653: the player rows and diplomacy grid for inCount players, the player combo refilled, an
    // EESEAChangeWorldPlayerCount(inCount + 1) posted (EESECBMaxPlayer::ChangeSelection)
    void SetPlayerCount(unsigned long inCount);
    void SelectPage(unsigned long inPage);  // 0x1761c8 (EESETab::Check: shows page 0..4)
    void FillTechLists();                   // 0x1760ad (EESERBUnitTypes::Check)
    void CopyFirstPlayerToAll();            // 0x175eb2 (EESEPSSetPlayers::ButtonUp)
    // 0x199ae9: the player of the player combo box (its selection + 1 indexes gWorld.mPlayers)
    EEPlayer* GetCurrentPlayer();
    // 0x175d58: a player's settings marked changed, and needing a full tech tree reload when inFlag is set
    void MarkPlayerChanged(unsigned long inPlayer, bool inFlag);
    void RebuildTechLists();                // 0x1766ed (EESEEDisplayEpoch::ProcessEvent)
    UIComboBox* GetPlayerSelection();       // the editor's player combo box (port)
    UIComboBox* GetRowEpochs(unsigned long inRow, bool inEnd);   // a player row's epoch combo box (port; the gate)

    EeScreensSePlayers* mRust;              // the tab (editor_players.rs)
};

#endif
