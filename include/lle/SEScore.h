// C:\EEX\Low-Level Engine\Sound Engine\SEScore.h (RECONSTRUCTION). One music score: a DirectMusic segment (.sgt) as
// loaded by dmusic (src/ThirdParty/dmusic). The original also made a secondary score for every motif of the
// segment's style; dmusic has no motifs, and the game never plays them, so a score has no secondary scores.
#ifndef LLE_SESCORE_H
#define LLE_SESCORE_H

#include <vector>

struct DmSegment;

class LLE_API SEScore {
    friend class SEConductor;
public:
    bool GetIsSecondaryScore() const { return this->mIsSecondaryScore; }
    unsigned long GetRepeatCount();
    SEScore* GetSecondaryScore(unsigned long inIndex) const { return this->mSecondaryScores[inIndex]; }
    unsigned long GetSecondaryScoreCount() const { return this->mSecondaryScores.size(); }
    void SetRepeatCount(unsigned long inCount);
private:
    DmSegment* GetSegment() const { return this->mSegment; }
    SEScore(DmSegment* inSegment, bool inIsSecondaryScore)
    {
        this->mSegment = inSegment;
        this->mIsSecondaryScore = inIsSecondaryScore;
    }
    virtual ~SEScore();

    std::vector<SEScore*> mSecondaryScores; // always empty (see above)
    DmSegment* mSegment;                    // one reference, released by the destructor
    bool mIsSecondaryScore;
};

#endif
