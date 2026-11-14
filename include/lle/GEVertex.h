// C:\EEX\Low-Level Engine\Graphics Engine\GEVertex.h (RECONSTRUCTION). All inline (GEBoundingBox.cpp emits the block
// first, 0x100111d8..).
#ifndef LLE_GEVERTEX_H
#define LLE_GEVERTEX_H

class LLE_API GEVertex {                    // sizeof 0x2c
public:
    GEVertex()
    {
        this->mPointIndex = 0;
        this->mSpecularShininess = 1.0f;
    }
    // 0x11210: members default-constructed, then copied
    GEVertex(const GEVertex& inVertex)
    {
        this->mDiffuseColor = inVertex.mDiffuseColor;
        this->mSpecularColor = inVertex.mSpecularColor;
        this->mPrelitDiffuseColor = inVertex.mPrelitDiffuseColor;
        this->mSpecularShininess = inVertex.mSpecularShininess;
        this->mPointIndex = inVertex.mPointIndex;
    }
    ~GEVertex() {}
private:

    GEColor mDiffuseColor;                  // +0x00
    GEColor mSpecularColor;                 // +0x0c
    GEColor mPrelitDiffuseColor;            // +0x18
    float mSpecularShininess;               // +0x24
    unsigned long mPointIndex;              // +0x28
};

#endif
