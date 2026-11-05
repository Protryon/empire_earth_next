// C:\EEX\Low-Level Engine\Sound Engine\SEScore.cpp
// A music score on dmusic (see lle/SEScore.h).
#include <windows.h>
#include <dmusic.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEScore.h"

// Destroys the score, releasing its segment
SEScore::~SEScore()
{
    for (unsigned long i = 0; i < this->mSecondaryScores.size(); i++)
        delete this->mSecondaryScores[i];
    DmSegment_release(this->mSegment);
}

// Returns how many times the score repeats
unsigned long SEScore::GetRepeatCount()
{
    return DmSegment_getRepeats(this->mSegment);
}

// Sets how many times the score repeats (the game loops a score with a large count)
void SEScore::SetRepeatCount(unsigned long inCount)
{
    DmSegment_setRepeats(this->mSegment, inCount);
}
