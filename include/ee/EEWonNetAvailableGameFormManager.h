// C:\EEX\Empire Earth\User Interface Thread\EEWonNetAvailableGameFormManager.h (RECONSTRUCTION; file name
// inferred). The WON game room form's (0x20) manager (RTTI EEWonNetAvailableGameFormManager > UIFormManager;
// vtable 0x44ba84, ctor 0x27a37e: new TMPoll(3000, 0, false), sizeof 8). Get() 0x27a3eb creates it on first
// use. Requires lle/UIFormManager.h, lle/TMPoll.h.
#ifndef EE_EEWONNETAVAILABLEGAMEFORMMANAGER_H
#define EE_EEWONNETAVAILABLEGAMEFORMMANAGER_H

class EEWonNetAvailableGameFormManager : public UIFormManager {
public:
    EEWonNetAvailableGameFormManager();     // 0x27a37e
    virtual ~EEWonNetAvailableGameFormManager();   // +0x00 0x27a3cb (??_G 0x27a3af)
    static EEWonNetAvailableGameFormManager* Get();   // 0x27a3eb
    TMPoll* GetPoll() const { return this->mPoll; }

    TMPoll* mPoll;                          // +0x04 the game list refresh timer (3 s)
};

#endif
