// C:\EEX\Empire Earth\Scenario Editor\EEUIFSERBBrush.h (RECONSTRUCTION; file name inferred). A
// brush size radio button (RTTI EEUIFSERBBrush > UIRadioButton; vtable 0x445620, sizeof 0x11c; ctor 0x1a4c00, ??_G
// 0x1a4c98 with the inline dtor): Check stores its size into the group's +0x28. Requires lle/UIRadioButton.h,
// ee/EEUIFSEBrushGroup.h.
#ifndef EE_EEUIFSERBBRUSH_H
#define EE_EEUIFSERBBRUSH_H

class EEUIFSERBBrush : public UIRadioButton {
public:
    // 0x1a4c00: the 100x20 button at inPosition, background opacity 0.4
    EEUIFSERBBrush(EEUIFSEBrushGroup* inGroup, long inBrushSize, unsigned long inId, const UWideString& inText,
                   GEFont* inFont, const GE2DPoint& inPosition, UIForm* inForm, unsigned long inIndex,
                   bool inIsChecked);
    virtual ~EEUIFSERBBrush() {}            // inline (??_G 0x1a4c98)
    virtual void Check();                   // 0x1a4c83

    EEUIFSEBrushGroup* mGroup;              // +0x114
    long mBrushSize;                        // +0x118
};

#endif
