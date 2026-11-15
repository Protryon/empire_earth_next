// C:\EEX\Empire Earth\User Interface Thread\EEUIMiniMap.h (RECONSTRUCTION; file name inferred).
// The mini-map control of the main game form and the scenario editor (RTTI EEUIMiniMap > UIControl; vtable 0x44b8b4,
// 34 slots; `new` at 0x274772 (the control factory) and 0x19d81d (the scenario editor's +0x238)). It draws the
// terrain and world textures of gMiniMap as two GEBitmaps, the camera's view as four lines and the flare of an
// EEMSendFlare as a blinking bitmap; a right click (or a left drag) posts the EEAMiniMap action (mode 1 / 0), a left
// click with the flare cursor (gMouse +0x98 == 0x16) sends the flare. EEUIMiniMap.cpp is 0x2753b1..0x276442 (the
// statics' `$E` chains, ctor, ??_G, dtor, then the members in address order, EEAMiniMap's inline ctor last).
// Layout from the ctor; provisional names. Requires lle/UIControl.h, lle/GELine.h, lle/GEBitmap.h.
#ifndef EE_EEUIMINIMAP_H
#define EE_EEUIMINIMAP_H

class EEAMiniMap;
class EEMiniMap;
class GEBitmap;

class EEUIMiniMap : public UIControl {
public:
    EEUIMiniMap(const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inId,
                unsigned long inBorderThickness);
    virtual ~EEUIMiniMap();
    virtual long CopyData();                // +0x14 0x275585
    virtual long DrawPolygons();            // +0x18 0x275ba1
    virtual void SetLocation(const GE2DPoint& inLocation);  // +0x6c 0x2761f6
    // 0x275a2d (Empire Earth.cpp 0x13fcc1): the three bitmaps and the flare texture (provisional name)
    static void LoadMedia();
    // 0x275b67 (0x140d23): releases them (provisional name)
    static void UnloadMedia();
    // 0x276236 (5 callers: EEMiniMap 0x11f0a1, EEWorld 0x13a308, the scenario editor): the bitmaps' textures, texture
    // coordinates and sBounds from inMiniMap's transformations, gMiniMapTransformation = its +0x64 (provisional name)
    static void Update(EEMiniMap* inMiniMap);
    static GERectangle sBounds;
    // the ee_screens mini_map.rs host of the left release and drag (EE_SCREENS_MM_OP_*; a member: the mouse state
    // and CenterCamera are protected)
    static int64_t HostOp(EEUIMiniMap* inMap, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC);             // 0x565318 the map's corners in mini-map units (Update; one guard bit)
protected:
    virtual long MouseLButtonDown();        // +0x34 (ICF-folded 0x25ae3b: CaptureMouse, 0)
    virtual long MouseLButtonUp();          // +0x38 0x275f72
    virtual long MouseMove(const GE2DPoint& inLocation);    // +0x40 0x2760af
    virtual long MouseRButtonDown();        // +0x44 0x275e45
    virtual long MouseRButtonUp();          // +0x48 0x275523
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}
    virtual long UpdateKeyboard() { return 0; }
    // 0x2760ec: centres the camera on the mini-map point under the mouse (EEAMiniMap mode 0) (provisional name)
    void CenterCamera();

    GE2DPoint mOrigin;                      // +0x0a0 the map's top-left (the location plus the border)
    GELine mLines[4];                       // +0x0a8 the camera view's edges
    EEAMiniMap* mAction;                    // +0x0d8 (new EEAMiniMap; dereferenced by the dtor)
    bool mDrawLine0;                        // +0x0dc
    bool mDrawLine1;                        // +0x0dd
    bool mDrawLine2;                        // +0x0de
    bool mDrawLine3;                        // +0x0df
    bool mShowFlare;                        // +0x0e0 gMiniMap's flare is on (and the camera is not free)
    bool mFlareBlink;                       // +0x0e1 toggles the flare colour each CopyData
};

#endif
