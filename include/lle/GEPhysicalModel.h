// C:\EEX\Low-Level Engine\Graphics Engine\GEPhysicalModel.h (RECONSTRUCTION). A placed, scaled model.
// GEPhysicalModel.cpp (~0x10020e09) holds the real code; the forwarding Intersects and the setters are inline
// (GEBoundingBox.cpp emits them first, 0x10011c0a..).
#ifndef LLE_GEPHYSICALMODEL_H
#define LLE_GEPHYSICALMODEL_H

class GEModel;

class LLE_API GEPhysicalModel {             // sizeof 0x4c
public:
    GEPhysicalModel()
    {
        this->mModel = NULL;
        this->mScale = 1.0f;
        this->mFrame = 0;
    }
    GEPhysicalModel(GEModel* inModel, const GETransformation& inLocation, float inScale);
    ~GEPhysicalModel();
    GEPhysicalModel& operator=(const GEPhysicalModel& inModel);
    unsigned long GetFrame() const { return this->mFrame; }
    const GETransformation& GetLocation() const { return this->mLocation; }
    GEModel* GetModel() const { return this->mModel; }
    float GetScale() const { return this->mScale; }
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
    void SetModel(GEModel* inModel);
    void SetScale(float inScale) { this->mScale = inScale; }
    friend void EEGfxObjectLayoutRegister();   // the effects' reads of the scale and the frame (ee_gfx effect_reads.rs)
private:
    bool Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                    const GE3DLine& inLine, unsigned long inLOD, float& outDistance, bool inExact) const;
    bool Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                    const GE3DLine& inLine, bool inExact, unsigned long inLOD, float inX, float inY, float inZ) const;
    // model against model (0x1002179e): this model placed at inLocation against inOtherModel under inOtherLocation,
    // whose inverse (scale removed) is inOtherInverse
    bool Intersects(const GETransformation& inLocation, const GEModel* inModel, const GETransformation& inOtherLocation,
                    const GETransformation& inOtherInverse, float inScale, const GEModel* inOtherModel,
                    unsigned long inFrame, unsigned long inLOD) const;

    GETransformation mLocation;             // +0x00
    GEModel* mModel;                        // +0x40
    float mScale;                           // +0x44
    unsigned long mFrame;                   // +0x48
};

#endif
