// C:\EEX\Empire Earth\Scenario Editor\EESEPSSetPlayers.h (RECONSTRUCTION; file name inferred).
// The player tab's "set players" button (RTTI EESEPSSetPlayers > UIButton; vtable 0x442e54; ctor 0x172c4a, ??_G
// 0x172ceb with the inline dtor). Requires lle/UIButton.h.
#ifndef EE_EESEPSSETPLAYERS_H
#define EE_EESEPSSETPLAYERS_H

class EESEPSSetPlayers : public UIButton {
public:
    EESEPSSetPlayers(const GE2DPoint& inPosition, GEFont* inFont, UIForm* inForm, unsigned long inIndex);
    virtual ~EESEPSSetPlayers() {}          // +0x00 (inline: ??_G 0x172ceb)
protected:
    virtual void ButtonUp();                // +0x8c (0x172d0e)
public:
    // four guarded vector<long> class statics (`$E` chains 0x172b0f..0x172c4a, one guard byte 0x531fa8, bits 1/2/4/8;
    // read by EESEPlayerScreen 0x175eb2..; provisional owner class and names)
    static std::vector<long> sV531FB0;      // 0x531fb0
    static std::vector<long> sBuildingTechs;  // 0x531f98
    static std::vector<long> sV531F88;      // 0x531f88
    static std::vector<long> sTechnologyTechs;  // 0x531f78
};

#endif
