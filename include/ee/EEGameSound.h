// Game sound helpers (RECONSTRUCTION; file name inferred; all names are provisional). Requires lle/GE3DPoint.h.
#ifndef EE_EEGAMESOUND_H
#define EE_EEGAMESOUND_H

class SESoundManager;

// 0x2449ca (cdecl): volume, pan and pitch of a sound of inVolume played at inLocation, as heard from the camera
void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFalloff);

extern SESoundManager* gSoundManager;       // 0x5319c4

class GE3DPoint;
class EEGraphicObject;
class SESMInstanceNode;
// 0x1487a3 (cdecl; also declared in ee/EEEffect3D.h, whose include renumbers the 2D effect units' $E chains)
SESMInstanceNode* PlayAnimationSound(const GE3DPoint& inLocation, EEGraphicObject* inGraphicObject, long inAnimation,
                           SESMInstanceNode* inSound, unsigned long inFrame, bool inStart);

#endif
