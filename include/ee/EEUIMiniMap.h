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

#include "ee/EEUIRustControl.h"

class EEAMiniMap;
class EEMiniMap;
class GEBitmap;

class EEUIMiniMap : public EEUIRustControl {
public:
    EEUIMiniMap(const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inId,
                unsigned long inBorderThickness);
    virtual ~EEUIMiniMap();
    virtual long CopyData();                // +0x14 0x275585
    virtual long DrawPolygons();            // +0x18 0x275ba1
    // Moves the mini map control and its map origin
    virtual void SetLocation(const GE2DPoint& inLocation)
    {
        ee_screens_mm_set_location((uintptr_t)this, inLocation.GetX(), inLocation.GetY());
    }  // +0x6c 0x2761f6
    // 0x275a2d (Empire Earth.cpp 0x13fcc1): the three bitmaps and the flare texture (provisional name)
    // Creates the terrain, units and flare bitmaps and loads the flare texture
    static void LoadMedia() { ee_screens_mm_load_media(); }
    // 0x275b67 (0x140d23): releases them (provisional name)
    // Releases the terrain, units and flare bitmaps
    static void UnloadMedia() { ee_screens_mm_unload_media(); }
    // 0x276236 (5 callers: EEMiniMap 0x11f0a1, EEWorld 0x13a308, the scenario editor): the bitmaps' textures, texture
    // coordinates and sBounds from inMiniMap's transformations, gMiniMapTransformation = its +0x64 (provisional name)
    static void Update(EEMiniMap* inMiniMap);
    static GERectangle sBounds;
    // (public for the shims of ee_screens mini_map_calls.rs: the members, CenterCamera and the base's mouse)
    using UIControl::CaptureMouse;
    using UIControl::GetMouseLocation;
    // 0x2760ec: centres the camera on the mini-map point under the mouse (EEAMiniMap mode 0) (provisional name)
    // Centres the camera on the map point under the mouse, clamped to the map
    void CenterCamera() { ee_screens_mm_center_camera((uintptr_t)this); }
protected:
    // the ee_screens mini_map.rs host of the left release and drag (EE_SCREENS_MM_OP_*, EEUIRustControl's the
    // toolkit's)
    virtual int64_t RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF);
    virtual long MouseLButtonDown();        // +0x34 (ICF-folded 0x25ae3b: CaptureMouse, 0)
    // On a left release: in flare mode sends a flare at the clicked tile and leaves flare mode, otherwise centres
    // the camera there (ee_screens mini_map.rs)
    virtual long MouseLButtonUp() { return (long)ee_screens_mini_map_mouse_l_up(this); }          // +0x38 0x275f72
    // Keeps centring the camera while the left button drags over the mini map (not in flare mode) (ee_screens
    // mini_map.rs)
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)ee_screens_mini_map_mouse_move(this); }    // +0x40 0x2760af
    // On a right press, posts a mini map action (mode 1) at the clicked map point, clamped to the map
    virtual long MouseRButtonDown() { return (long)ee_screens_mm_mouse_r_down((uintptr_t)this); }        // +0x44 0x275e45
    virtual long MouseRButtonUp();          // +0x48 0x275523
public:
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
