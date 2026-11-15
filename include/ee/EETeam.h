// C:\EEX\Empire Earth\Empire Earth\EETeam.h (RECONSTRUCTION; provisional names). A team of players (RTTI EETeam >
// FSRestorable; vtable 0x441fa0). sizeof 0x18 (vector<EETeam> strides 0x18 in EEPWorldHelper). The ctor
// 0x13d013 and the dtor 0x131b02 are out of line (vector<EETeam>::resize 0x14c5d9 builds its default argument with
// the ctor and destroys its by-value argument with the dtor). Slots: GetChecksum +0x00 0x156dd8, Load +0x04
// 0x136ca3, Save +0x0c 0x136d03.
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h (includes ee/EEVictoryCondition.h, ee/EEVCWonder.h).
#ifndef EE_EETEAM_H
#define EE_EETEAM_H

class EEVCWonder;                          // RTTI EEVCWonder > EEVictoryCondition (vtable 0x442064)

class EETeam : public FSRestorable {
public:
    EETeam();                                                                   // 0x13d013
    // user-defined: each copy news its own victory condition. Inline below the class (it needs EEVCWonder);
    // out-of-line COMDAT 0x13e831 in EEWorld.cpp's vector<EETeam> range (_Construct 0x13e955 calls it)
    EETeam(const EETeam& inTeam);
    ~EETeam();                                                                  // 0x131b02
    // user-defined: copies the players only (vector<EETeam> fill 0x13e8e3 / copy 0x13e2a7 expand it: one vector
    // operator=, mVictoryCondition untouched)
    EETeam& operator=(const EETeam& inTeam) { this->mPlayers = inTeam.mPlayers; return *this; }
    virtual void GetChecksum(ESChecksum& ioChecksum) const;                     // +0x00 0x156dd8
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x136ca3
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x136d03
    // 0x3fe37 (ret 4): whether player inPlayerID belongs to the team (EECPIntelligence 0x8123d) (provisional name)
    bool ContainsPlayer(unsigned long inPlayerID);
    // (provisional names)
    EETeam(const std::vector<unsigned long>& inPlayers);   // 0x13d0d3 (mU04 = inPlayers, then the victory condition)
    EETeam(long inPlayer1, long inPlayer2);       // 0x13d076 (AddPlayer both)
    void Merge(const EETeam& inTeam);       // 0x136b6d (AddPlayer for each of inTeam's players)
    void AddPlayer(unsigned long inPlayer); // 0x136ba1 (push_back unless present; pushes &inPlayer itself: the element type)
    void RemovePlayer(long inPlayer, std::vector<unsigned long>& outPlayers);   // 0x136bfc (outPlayers = the players without inPlayer)
    bool Contains(long inPlayer1, long inPlayer2);   // 0x136c64
    long GetVictoryState();                 // 0x136d5e (mVictoryCondition's slot +0x04)
    // inline (out of line through the ICF-folded COMDAT 0x91f11)
    unsigned long GetNumPlayers() const { return this->mPlayers.size(); }

    // Layout: the ctor 0x13d013 (a vector's `_Buy(0)`, then +0x14 = new EEVCWonder (0xc bytes (vtable 0x442064: +4
    // = 0, +8 = this)) and the dtor 0x131b02 (deletes +0x14 through slot 0, vector _Tidy).
    unsigned long GetPlayersAt(unsigned long inIndex) const { return this->mPlayers[inIndex]; }
    std::vector<unsigned long> mPlayers;    // +0x04 (Load 0x136ca3 / Save 0x136d03 use FSFile's unsigned long Read +0x5c / Write +0x94)
    EEVCWonder* mVictoryCondition;          // +0x14 (owned; the ctor news an EEVCWonder(this); Load calls its own slot +0x08; provisional name)
};

#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"

// 0x13e831: FSRestorable vptr, the vector's _Buy(0), vector operator= from inTeam.mU04, EETeam vptr, new EEVCWonder
inline EETeam::EETeam(const EETeam& inTeam)
{
    this->mPlayers = inTeam.mPlayers;
    this->mVictoryCondition = new EEVCWonder(this);
}

#endif
