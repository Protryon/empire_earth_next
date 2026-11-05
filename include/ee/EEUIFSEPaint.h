// ee/EEUIFSEPaint.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
#ifndef EE_EEUIFSEPAINT_H
#define EE_EEUIFSEPAINT_H

#include "lle/UIControl.h"
#include "lle/GE2DPoint.h"

class EEAScroll;
class EEUIFSEBrushGroup;
class EESEAPaint;

class EEUIFSEPaint : public UIControl {
public:
    // 0x1a3f10 (the arguments from EEUIFSEPaintElevation's ctor 0x1a4470 and the hint)
    EEUIFSEPaint(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes, unsigned long inId, const GE2DPoint& inLocation,
                 unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex);
    virtual ~EEUIFSEPaint();                // +0x00 (??_G)
    virtual long CopyData();                // +0x14
    virtual long MouseLButtonDown();        // +0x34
    virtual long MouseLButtonUp();          // +0x38
    virtual long MouseMove(const GE2DPoint& inLocation);   // +0x40
    virtual long MouseRButtonDown() { return 0; }   // +0x44 datacmp: 0x5ad67e (xor eax, eax)
    virtual long MouseRButtonUp() { return 0; }     // +0x48 (the same folded body)
    virtual void TabbedTo();                // +0x74
    virtual void TabbedFrom();              // +0x78
    virtual long UpdateKeyboard();          // +0x7c
    virtual EESEAPaint* CreatePaintAction() = 0;  // +0x88 (the new paint action; pure in the vtable)
    // 0x1a3ff7 (EESEAPaintElevation::Virtual4 calls it on the form's control 0xa1) (provisional name)
    void StoreMouseTile();

    EEUIFSEBrushGroup* mBrushes;            // +0xa0 (ctor: the second argument)
    EEAScroll* mScroll;                     // +0xa4 (ctor: the first argument; its +0x0c rectangle is reset)
    long mBrushRadius;                      // +0xa8
    EESEAPaint* mPac;                // +0xac
    long mLastTileX;                        // +0xb0 (ctor: 0)
    long mLastTileY;                        // +0xb4 (ctor: 0)
    bool mIsCentered;                       // +0xb8 (ctor: false; EEUIFSEPaintElevation::CreatePaintAction clears it)
};

#endif
