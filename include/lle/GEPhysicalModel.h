// C:\EEX\Low-Level Engine\Graphics Engine\GEPhysicalModel.h (RECONSTRUCTION). A placed, scaled model.
// GEPhysicalModel.cpp (~0x10020e09) holds the real code; the forwarding Intersects and the setters are inline
// (GEBoundingBox.cpp emits them first, 0x10011c0a..).
#ifndef LLE_GEPHYSICALMODEL_H
#define LLE_GEPHYSICALMODEL_H

#include <stddef.h>
#include "lle/GERust.h"
#include "rust/render/ge.h"

#include "lle/LLEForward.h"

class LLE_API GEPhysicalModel {             // sizeof 0x4c
public:
    GEPhysicalModel() : mModel(NULL), mScale(1.0f), mFrame(0) {}
    // a model placed in the world at a location and scale, holding a reference to it, at frame 0
    GEPhysicalModel(GEModel* inModel, const GETransformation& inLocation, float inScale)
        : mLocation(inLocation), mModel(NULL), mScale(inScale), mFrame(0) { this->SetModel(inModel); }
    ~GEPhysicalModel() { this->SetModel(NULL); }   // releases its model
    // the model reference, location, scale and frame (ee_ge_pm_assign: the reference first, as the original)
    GEPhysicalModel& operator=(const GEPhysicalModel& inModel) { return ee_ge_pm_assign(this, &inModel), *this; }
    unsigned long GetFrame() const { return this->mFrame; }
    const GETransformation& GetLocation() const { return this->mLocation; }
    GEModel* GetModel() const { return this->mModel; }
    float GetScale() const { return this->mScale; }
    // New (port): where the scale and the frame sit (the world views' layout, Units/EEWorldLayout.cpp)
    static size_t ScaleOffset() { return offsetof(GEPhysicalModel, mScale); }
    static size_t FrameOffset() { return offsetof(GEPhysicalModel, mFrame); }
    bool Intersects(const GE3DLine& inLine, unsigned long inLOD, float& outDistance, bool inExact) const
        { return this->Intersects(this->mLocation, this->mModel, this->mFrame, inLine, inLOD, outDistance, inExact); }
    bool Intersects(const GEPhysicalModel* inModel, const GE3DLine& inLine, bool inExact, unsigned long inLOD,
                    float inX, float inY, float inZ) const
    {
        // through the accessors: GetScale() read once per product (0x10011c42 keeps two of them in memory temps)
        return this->Intersects(inModel->GetLocation(), inModel->GetModel(), inModel->GetFrame(), inLine, inExact, inLOD,
                          inModel->GetScale() * inX, inModel->GetScale() * inY, inModel->GetScale() * inZ);
    }
    bool Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame, float inScale,
                    const GE3DLine& inLine, bool inExact, unsigned long inLOD, float inX, float inY, float inZ) const
    {
        return this->Intersects(inLocation, inModel, inFrame, inLine, inExact, inLOD, inScale * inX, inScale * inY,
                          inScale * inZ);
    }
    bool Intersects(const GEViewport& inViewport, bool inOrthographic) const;
    void SetFrame(unsigned long inFrame) { this->mFrame = inFrame; }
    void SetLocation(const GETransformation& inLocation) { this->mLocation = inLocation; }
    void SetLocationAndScale(const GETransformation& inLocation, float inScale)
    {
        this->mLocation = inLocation;
        this->mScale = inScale;
    }
    void SetLocationXY(float inX, float inY)
    {
        this->mLocation.SetX(inX);
        this->mLocation.SetY(inY);
    }
    void SetLocationXYZ(const GE3DPoint& inPoint)
    {
        this->mLocation.SetX(inPoint.GetX());
        this->mLocation.SetY(inPoint.GetY());
        this->mLocation.SetZ(inPoint.GetZ());
    }
    // replaces the model, keeping a reference to the new one (the old one released first)
    void SetModel(GEModel* inModel) { ee_ge_pm_set_model(reinterpret_cast<void**>(&this->mModel), inModel); }
    void SetScale(float inScale) { this->mScale = inScale; }
    // The walks are Rust (ee_ge physical_walk.rs) over the game's table (ee_ge_physical_table).
    // A line against a model and its sub-models: bounding sphere, then box (LOD > 0), then triangles (LOD > 1); with
    // inExact the hit must lie on the segment (line_walk)
    bool Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                    const GE3DLine& inLine, unsigned long inLOD, float& outDistance, bool inExact) const
        { return ee_ge_physical_line_walk(ee_ge_physical_table(), this->mScale, GERust(inLocation), inModel, inFrame, GERust(inLine), inLOD, &outDistance, inExact); }
    // a model scaled per axis against this placed model, sub-models included; in exact mode a top model with no hit
    // retries as a uniformly scaled line test (scaled_walk)
    bool Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                    const GE3DLine& inLine, bool inExact, unsigned long inLOD, float inX, float inY, float inZ) const
        { return ee_ge_physical_scaled_walk(ee_ge_physical_table(), GERust(this->mLocation), this->mModel, this->mScale, this->mFrame, GERust(inLocation), inModel, inFrame, GERust(inLine), inExact, inLOD, inX, inY, inZ); }
    // model against model (0x1002179e): this model placed at inLocation against inOtherModel under inOtherLocation,
    // whose inverse (scale removed) is inOtherInverse: bounding spheres, then boxes (LOD > 0), then triangle boxes at a
    // reduced LOD (LOD > 1), recursing into this model's sub-models (model_walk)
    bool Intersects(const GETransformation& inLocation, const GEModel* inModel, const GETransformation& inOtherLocation,
                    const GETransformation& inOtherInverse, float inScale, const GEModel* inOtherModel,
                    unsigned long inFrame, unsigned long inLOD) const
        { return ee_ge_physical_model_walk(ee_ge_physical_table(), GERust(this->mLocation), this->mModel, this->mScale, this->mFrame, GERust(inLocation), inModel, GERust(inOtherLocation), GERust(inOtherInverse), inScale, inOtherModel, inFrame, inLOD); }

    GETransformation mLocation;             // +0x00
    GEModel* mModel;                        // +0x40
    float mScale;                           // +0x44
    unsigned long mFrame;                   // +0x48
};

#endif
