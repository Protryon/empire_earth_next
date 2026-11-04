// C:\EEX\Empire Earth\Formations\EEFWedge.cpp (file and folder names inferred: no __FILE__). The wedge formation: 0xddf3a..0xde331 (ctor, Place).
// Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"

#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"

#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEFormation.h"
#include "ee/EEFWedge.h"

// NEAR: scheduling only (the init-list stores of +0x78/+0x80 sink below the six fldz stores in the original)
// FUNCTION: EXE 0x4ddf3a
// Creates a wedge formation: arms opening at inAngle with inSpacing between units, inCount units per
// wedge, the gap to the next wedge capped by the arm geometry
EEFWedge::EEFWedge(long inType, float inSpacing, float inAngle, unsigned long inCount)
    : EEFormation(inType), mSpacing(inSpacing), mCount(inCount)
{
    this->mRowDepth = inCount * inSpacing * 0.5f;
    if (inAngle != 0.0f) {
        float theLimit = inSpacing / UMTan(inAngle * 0.5f);
        if (theLimit < this->mRowDepth)
            this->mRowDepth = theLimit;
    }
    inAngle = inAngle * 0.5f;
    float theX = UMCos(1.57079637f - inAngle) * inSpacing;
    float theY = UMCos(inAngle) * inSpacing;
    this->mLeftArm.mX = -theX;
    this->mRightArm.mY = this->mLeftArm.mY = -theY;
    this->mRightArm.mX = theX;
}

// Destroys the wedge formation (ICF-folded into ~EEFCircle 0xdd1be)
EEFWedge::~EEFWedge()
{
}

// FUNCTION: EXE 0x4de019
// Places the units in wedges: a leader at the point, then pairs down the left and right arms, further
// wedges behind; arms are scaled by the largest collision radius (NaN when every radius is 0)
void EEFWedge::Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots)
{
    GE3DPoint theLeftPosition;
    GE3DPoint theRightPosition;
    GE3DPoint theCenter(this->mCenter);
    GEVector theStep;
    EEFormationSlot theSlot;
    GEVector theTransformedStep;
    GEVector theLeftStep;
    GEVector theRightStep;
    long theUnitCount = inUnits.size();
    outSlots.clear();
    float theRadius = 0.0f;
    for (long i = 0; i < theUnitCount; i++) {
        theRadius = max(inUnits[i]->mUnitClass->mOuterCollisionSphere, theRadius);
    }
    // scale copies of the arms: scaling the members and dividing back left 0/0 = NaN arms after a call with
    // no units (or radius 0), and let them drift by rounding on every call
    GEVector theLeftArm(this->mLeftArm);
    GEVector theRightArm(this->mRightArm);
    theLeftArm.mX *= theRadius;
    theLeftArm.mY *= theRadius;
    theRightArm.mX *= theRadius;
    theRightArm.mY *= theRadius;
    this->mOrientation.Apply(theLeftArm, theLeftStep);
    this->mOrientation.Apply(theRightArm, theRightStep);
    long thePlaced = 1;
    for (unsigned long j = 0; (long)j < theUnitCount; j++, thePlaced++) {
        if (j % this->mCount == 0) {
            if (thePlaced != 1) {
                theStep = GEVector(0.0f, -(theRadius * this->mRowDepth), 0.0f);
                this->mOrientation.Apply(theStep, theTransformedStep);
                theCenter += theTransformedStep;
            }
            theSlot.mPosition = theCenter;
            theSlot.mUnit = inUnits[j];
            theSlot.mHeading = this->mHeading;
            outSlots.push_back(theSlot);
            theLeftPosition = theCenter;
            theRightPosition = theCenter;
        } else {
            theLeftPosition += theLeftStep;
            theRightPosition += theRightStep;
            theSlot.mPosition = theLeftPosition;
            theSlot.mUnit = inUnits[j];
            theSlot.mHeading = this->mHeading;
            outSlots.push_back(theSlot);
            if (thePlaced < theUnitCount) {
                j++;
                thePlaced++;
                theSlot.mPosition = theRightPosition;
                theSlot.mUnit = inUnits[j];
                theSlot.mHeading = this->mHeading;
                outSlots.push_back(theSlot);
            }
        }
    }
}
