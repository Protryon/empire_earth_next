// C:\EEX\Empire Earth\User Interface Thread\EEUIBGenericBackground.h (RECONSTRUCTION; file name inferred). A bitmap
// background drawn tinted and scaled (RTTI EEUIBGenericBackground > UIBitmapBackground; vtable 0x449c84).
// EEUIBGenericBackground.cpp is 0x254911..0x255c2a. Only what its users need is declared
// so far. Layout from the ctor 0x254a05 (sizeof 0x98: EEUIFError's `new`); member names are provisional. Requires
// lle/UIBitmapBackground.h.
#ifndef EE_EEUIBGENERICBACKGROUND_H
#define EE_EEUIBGENERICBACKGROUND_H
#include "rust/game/screens.h"

class EEUIBGenericBackground : public UIBitmapBackground {
public:
    // 0x254a05: UIBitmapBackground(inID, inTextureFileName, inDescription, GERectangle(), inLayout, inForm,
    // inDrawPriority, inVisible); then +0x7c, +0x94 and the colour (+0x88: GEColor(), then inColor), +0x80, +0x84
    EEUIBGenericBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                           const GETextureDescription& inDescription, const UILayoutUtility& inLayout,
                           UIForm* inForm, unsigned long inDrawPriority, const GEColor& inColor, bool inVisible,
                           float inOpacity, float inScaleX, float inScaleY, unsigned long inFlags);
    // Destroys the form background
    virtual ~EEUIBGenericBackground() {}
    // +0x14 0x255131: the tinted rhombus, then the frame pieces mFrameFlags asks for (1 edges + small corners, 2 large
    // corners, 4 side ornaments; ee_screens frame.rs, with the original's DrawCorner 0x255463, DrawCorners 0x254acc,
    // DrawEdge 0x25528a, DrawEdges 0x254c5d, DrawFrameCorners 0x254e05 and DrawSides 0x254f98)
    // Draws the tinted background inside the side insets, then the frame pieces its flags ask for, each clipped to the
    // redraw rectangle (the original's DrawCorners, DrawEdges, DrawFrameCorners, DrawSides, DrawEdge and DrawCorner:
    // ee_screens frame.rs over backgrounds_calls.rs' fill and pieces)
    virtual long DrawBackground(const GERectangle& inRectangle)
    {
        return ee_screens_frame_draw(this, &this->mRectangle, this->mScaleX, this->mScaleY, this->mFrameFlags, &inRectangle);
    }
    // +0x08 0x255675: scales the frame to the game window (mScaleX/mScaleY = window / designed size)
    // Opens the background and scales its frame to the window size against the form's designed size
    // (backgrounds_calls.rs)
    virtual void Open() { ee_screens_gb_open((uintptr_t)this); }
    void SetScale(float inScaleX, float inScaleY)
    {
        this->mScaleX = inScaleX;
        this->mScaleY = inScaleY;
    }

    float mOpacity;                         // +0x7c (provisional name)
    float mScaleX;                          // +0x80 EEUIFError::Open: game window width / 1024
    float mScaleY;                          // +0x84 EEUIFError::Open: game window height / 768
    GEColor mColor;                         // +0x88
    unsigned long mFrameFlags;              // +0x94 (3 from EEUIFError)
};

#endif
