// C:\EEX\Empire Earth\Graphics Thread\EEROUnits.h (RECONSTRUCTION; file name inferred). The units' render object (RTTI
// EEROUnits > EERenderObject > RTSRenderObject, vtable 0x441bbc; EEROUnits.cpp 0x114296..0x118741, __FILE__ 0x4be760):
// the models, rings, bars and labels of the
// units in EERenderLists. No members of its own. Names are provisional. Requires ee/EERenderObject.h.
#ifndef EE_EEROUNITS_H
#define EE_EEROUNITS_H

class EEROUnits : public EERenderObject {
public:
    EEROUnits();                            // 0x1144e8
    virtual ~EEROUnits();                   // +0x10 dtor 0x114516 (??_G 0x1144fa)
    virtual void FrameStarting();           // +0x00 0x114521: EERenderObject::Fn10EABE
    virtual void SceneStarting();           // +0x04 0xe123d trivial: ret (ICF)
    virtual void RenderScene();             // +0x08 0x118203
    virtual void FrameDone();               // +0x0c 0x11852f

    static void LoadMedia();                // 0x116a97 (0x13fccb calls it; asserts) (provisional name)
    static void ReleaseMedia();             // 0x1185b5 (provisional name)

private:
    // the passes (thiscall; Virtual08 passes `this`; provisional names)
    void DrawSelectionRings();              // 0x114526 the selection rings (class flag bit 0)
    void DrawRings();                       // 0x11461b the rings of mUnits0
    void DrawBar(EERenderUnit* inUnit, const GEColor& inColor, const GEColor& inBackColor, float inFraction,
                  long inRow);              // 0x1147f8 one bar of a unit
    void DrawPlacement(EERenderUnitRecord& inRecord);  // 0x114cbc a placed building's model and footprint grid
    void DrawSelection();                   // 0x115488 the selection models and rings
    void DrawObjectModel(GETransformation& ioTransformation, EERenderShadow* inObject);  // 0x115674 an object's model
    void DrawObjectBars(EERenderShadow* inObject);  // 0x1157b6 an object's bars
    void DrawObjectModels(EERenderShadowMap& inObjects);  // 0x115a50 the objects' models
    void DrawObjectSelection();             // 0x115b63 the objects' selection models
    long DrawUnitBars(EERenderUnit* inUnit);  // 0x115c3a a unit's bars (returns their height)
    void DrawUnitModel(EERenderUnit* inUnit);  // 0x115cc7 a unit's model
    void DrawUnitModels(EERenderUnitMap& inUnits);  // 0x116299 the units' models
    void DrawBars();                        // 0x116500 the bars
    void DrawLabels(EERenderUnit* inUnits, unsigned long inCount);  // 0x116692 the units' labels
};


// 0x118676 (cdecl): inPoint's screen position through the main viewport (EEE3DGoal) (provisional name)
void GetScreenPoint(const GE3DPoint& inPoint, GE2DPoint& outPoint);

#endif
