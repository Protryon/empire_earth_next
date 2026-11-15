// C:\EEX\Empire Earth\Actions\EEAScroll.cpp (file name inferred: no __FILE__; RTTI class EEAScroll, vtable
// 0x43907c). The edge-scroll action: 0x470f..0x4a0b (EEASaveGame's dtor ends at 0x470f, EEAServerMigration's ctor
// starts at 0x4a0b). ctor, ??_G and the implicit dtor (first users: the ctor's vtable), Virtual4, Update.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GERectangle.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/TMPoll.h"

#include "ee/EmpireEarth.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAction.h"
#include "ee/EEAScroll.h"

// the camera's scroll velocity, kept between the actions (0x4e5be4 / 0x4e5be0; provisional names)
static float sVelocityX;
static float sVelocityY;

static const float kMinimumSpeedFactor = 0.1f;      // 0x439070
static const float kScaleFactor = 0.1f;             // 0x43906c
static const float kAccelerationFactor = 0.01f;     // 0x439074

// FUNCTION: EXE 0x40470f
// Creates the edge-scroll action, not scrolling, with its 10 ms poll timer
EEAScroll::EEAScroll()
    : EEAction(false), mPoll(10, 0, false)
{
    this->mScrollLeft = false;
    this->mScrollRight = false;
    this->mScrollUp = false;
    this->mScrollDown = false;
    this->mElapsed = 0;
    this->mAccelerate = true;
}

// SYNTHETIC: EXE 0x404764
// (the scalar deleting destructor; ICF: also EEAServerMigration's)

// FUNCTION: EXE 0x404780
// Destroys the edge-scroll action
EEAScroll::~EEAScroll()
{
}

// FUNCTION: EXE 0x40478e
// Scrolls the camera toward the active screen edges, accelerating up to the side-scroll speed setting
// the scroll speed comes through EEGameSettings::GetSideScrollSpeed() (inline)
void EEAScroll::Virtual4()
{
    float theMaxSpeed = gGamePreferences.mSettings.GetSideScrollSpeed() + 0.5f;
    float theMinSpeed = kMinimumSpeedFactor * theMaxSpeed;
    float theScale = (::gWorld.mCameraControl.mZoom + 0.5f) * this->mElapsed * kScaleFactor;
    float theAcceleration = this->mAccelerate ? this->mElapsed * kAccelerationFactor * theMaxSpeed : theMaxSpeed + theMaxSpeed;
    float theDX = 0.0f;
    float theDY = 0.0f;
    if (this->mScrollLeft) {
        if (sVelocityX > -theMinSpeed)
            sVelocityX = -theMinSpeed;
        sVelocityX -= theAcceleration;
        if (sVelocityX < -theMaxSpeed)
            sVelocityX = -theMaxSpeed;
        theDX = sVelocityX * theScale;
    } else if (this->mScrollRight) {
        if (sVelocityX < theMinSpeed)
            sVelocityX = theMinSpeed;
        sVelocityX += theAcceleration;
        if (sVelocityX > theMaxSpeed)
            sVelocityX = theMaxSpeed;
        theDX = sVelocityX * theScale;
    }
    if (this->mScrollDown) {
        if (sVelocityY > -theMinSpeed)
            sVelocityY = -theMinSpeed;
        sVelocityY -= theAcceleration;
        if (sVelocityY < -theMaxSpeed)
            sVelocityY = -theMaxSpeed;
        theDY = sVelocityY * theScale;
    } else if (this->mScrollUp) {
        if (sVelocityY < theMinSpeed)
            sVelocityY = theMinSpeed;
        sVelocityY += theAcceleration;
        if (sVelocityY > theMaxSpeed)
            sVelocityY = theMaxSpeed;
        theDY = sVelocityY * theScale;
    }
    if (theDX != 0.0f || theDY != 0.0f)
        ::gWorld.mCameraControl.Scroll(theDX, theDY);
}

// FUNCTION: EXE 0x404935
// Updates which screen edges the mouse touches and the elapsed time; returns whether any edge scrolls
bool EEAScroll::Update(const GE2DPoint& inPoint)
{
    bool theScrolling = false;
    unsigned long theElapsed = 1;
    if (this->mPoll.GetIsRunning())
        theElapsed = this->mPoll.Poll();
    if (!gMouse->mIsFrozen) {
        if (inPoint.GetX() <= this->mEdges.GetLeft()) {
            theScrolling = true;
            this->mScrollLeft = true;
        } else {
            this->mScrollLeft = false;
        }
        if (inPoint.GetY() <= this->mEdges.GetTop()) {
            theScrolling = true;
            this->mScrollUp = true;
        } else {
            this->mScrollUp = false;
        }
        if (inPoint.GetX() >= this->mEdges.GetRight()) {
            theScrolling = true;
            this->mScrollRight = true;
        } else {
            this->mScrollRight = false;
        }
        if (inPoint.GetY() >= this->mEdges.GetBottom()) {
            theScrolling = true;
            this->mScrollDown = true;
        } else {
            this->mScrollDown = false;
        }
    }
    if (theScrolling) {
        if (theElapsed > 0) {
            this->mElapsed = theElapsed;
            gUserInterface->PostAction(this);
        }
        if (!this->mPoll.GetIsRunning())
            this->mPoll.Start();
    } else if (this->mPoll.GetIsRunning()) {
        this->mPoll.Stop();
        sVelocityY = 0.0f;
        sVelocityX = 0.0f;
    }
    return theScrolling;
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAScroll::Virtual8()
{
}
