// C:\EEX\Empire Earth\User Interface Thread\EEUIBColor.h (RECONSTRUCTION; file name inferred).
// A 24x24 button showing a player colour; left click steps to the next of the 16 colours, right click to the
// previous (RTTI EEUIBColor > UIButton, vtable 0x449bec; EEUIBColor.cpp 0x2547e0..0x254911). Names are provisional.
// Requires lle/UIButton.h, ee/EEGameOptions.h (EEGetPlayerColor).
#ifndef EE_EEUIBCOLOR_H
#define EE_EEUIBCOLOR_H

class EEUIBColor : public UIButton {
public:
    EEUIBColor(unsigned long inColor, unsigned long inId, const GE2DPoint& inLocation, UIForm* inForm,
               unsigned long inIndex);      // 0x2547e0
    virtual ~EEUIBColor() {}                // +0x00 (??_G 0x25486f: the vptr store, then UIButton's dtor)
    void SetColor(unsigned long inColor);   // 0x2548ce (wraps to 0..15)
    long GetColor() const { return this->mColor; }
protected:
    virtual long MouseLButtonUp();          // +0x38 0x254892
    virtual long MouseRButtonDown();        // +0x44 0x2548ac
    virtual long MouseRButtonUp();          // +0x48 0x2548b2

    long mColor;                            // +0x104 (signed: SetColor's jl/jge; sizeof 0x108)
};

#endif
