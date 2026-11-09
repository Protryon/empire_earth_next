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
#include "ee/EESimCapture.h"
#include "rust/ee_rust.h"
#include "ee/EEFormationRust.h"

// FUNCTION: EXE 0x4ddf3a
// Creates a wedge formation: arms opening at inAngle with inSpacing between units, inCount units per
// wedge, the gap to the next wedge capped by the arm geometry
EEFWedge::EEFWedge(long inType, float inSpacing, float inAngle, unsigned long inCount)
    : EEFormation(inType), mSpacing(inSpacing), mCount(inCount)
{
    // the geometry is Rust (ee_formations)
    EeFormationWedge theWedge = ee_formation_wedge_geometry(inSpacing, inAngle, (unsigned int)inCount);
    this->mRowDepth = theWedge.row_depth;
    this->mLeftArm = GEVector(theWedge.left_arm[0], theWedge.left_arm[1], theWedge.left_arm[2]);
    this->mRightArm = GEVector(theWedge.right_arm[0], theWedge.right_arm[1], theWedge.right_arm[2]);
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
    outSlots.clear();
    EEFormationRadii theRadii(this, inUnits);
    EeFormationWedge theWedge;
    theWedge.left_arm[0] = this->mLeftArm.mX;
    theWedge.left_arm[1] = this->mLeftArm.mY;
    theWedge.left_arm[2] = this->mLeftArm.mZ;
    theWedge.right_arm[0] = this->mRightArm.mX;
    theWedge.right_arm[1] = this->mRightArm.mY;
    theWedge.right_arm[2] = this->mRightArm.mZ;
    theWedge.spacing = this->mSpacing;
    theWedge.row_depth = this->mRowDepth;
    theWedge.count = (unsigned int)this->mCount;
    unsigned int theCount = ee_formation_wedge_place(&theRadii.mFrame, &theWedge, theRadii.Radii(), theRadii.Count(),
                                                     theRadii.Slots());
    theRadii.Append(theCount, inUnits, outSlots);
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(0x47574d46);   // "FMWG"
        EEFormationCaptureBegin(theRecord, this);
        theRecord.F32(this->mLeftArm.mX);
        theRecord.F32(this->mLeftArm.mY);
        theRecord.F32(this->mLeftArm.mZ);
        theRecord.F32(this->mRightArm.mX);
        theRecord.F32(this->mRightArm.mY);
        theRecord.F32(this->mRightArm.mZ);
        theRecord.F32(this->mSpacing);
        theRecord.F32(this->mRowDepth);
        theRecord.U32(this->mCount);
        EEFormationCaptureEnd(theRecord, inUnits, outSlots, 0);
    }
}
