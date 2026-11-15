// C:\EEX\Empire Earth\...\EEACalamity.h (RECONSTRUCTION; file path inferred). The cast-calamity
// action (RTTI EEACalamity > EEAction; vtable 0x438b88: ??_G 0x1dc5, Virtual4 ICF-folded into 0x28c4, Virtual8 the
// shared empty slot 0x1720c6; sizeof 0x2c). The main game form holds one at +0x518 (EEUIFMainGame 0x270c08 calls
// Check, then Cast). Names are provisional. Requires ee/EEAction.h, ee/EEMessage.h.
#ifndef EE_EEACALAMITY_H
#define EE_EEACALAMITY_H
#include "ee/EETechTreeTechID.h"

class EECalamityClass;

class EEACalamity : public EEAction {
public:
    // inline (0x2671df, EEUIFMainGame.cpp): +0x10 in the init list (stored before the vptr), the rest in the body
    EEACalamity() : mCalamityClass(NULL)
    {
        this->mMessage = NULL;
        this->mTechnology = kTechNone;
        this->mTargetID = 0;
        this->mCasterID = 0;
        this->mX = 0;
        this->mY = 0;
        this->mValid = 1;
    }
    virtual ~EEACalamity();
    virtual void Virtual4();                // +0x04 (ICF-folded into EEALasso::Virtual4 0x28c4)
    virtual void Virtual8();                // +0x08 (the shared empty slot 0x1720c6)
    void Cast(const GE2DPoint& inLocation);     // 0x1ab2: posts an EEMCastCalamity at inLocation
    long Check(const GE2DPoint& inLocation);    // 0x1c26: whether the selected units can cast at inLocation (mValid)
    // inline (EEUIFMainGame::StartCalamity 0x66662d: both stores through one load of mCalamity; provisional name)
    void SetCalamity(long inTechnology, EECalamityClass* inClass) { this->mTechnology = inTechnology; this->mCalamityClass = inClass; }
    void SetCasterID(long inCasterID);      // 0x1c1c (out of line: the main game form calls it)

    EEMRecord* mMessage;              // +0x0c
    EECalamityClass* mCalamityClass;        // +0x10
    long mTechnology;                       // +0x14 (0x1b6c: cast on the caster itself)
    long mTargetID;                         // +0x18
    long mCasterID;                         // +0x1c
    long mX;                                // +0x20
    long mY;                                // +0x24
    long mValid;                            // +0x28
};

#endif
