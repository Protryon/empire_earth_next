// C:\EEX\Low-Level Engine\User Interface\UIBackgroundTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005c34a..0x1005c84e: UIBackgroundTable's inline members first (0x1005c34a..0x1005c575), then the
// sUIBackgroundArray init chain, the default ctor, Create and DeleteAll. No U2D key chains (the unit does not see the
// rasterizer header). UIBFull.cpp's inline block (UITextureBackground's members) starts at 0x1005c84e.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/UIBackgroundTable.h"

std::vector<UIBackgroundTable*> UIBackgroundTable::sUIBackgroundArray;

// FUNCTION: LLE 0x1005c5be
// Creates an empty background table entry (IDs -1, bitmap type, hidden, full opacity, draw priority 1)
UIBackgroundTable::UIBackgroundTable()
{
    this->mID = -1;
    this->mBackgroundID = -1;
    this->mFormID = -1;
    this->mTileHeightPercent = 1.0f;
    this->mTileWidthPercent = 1.0f;
    this->mBackgroundType = kBackgroundBitmap;
    this->mVisible = false;
    this->mAttribute1 = false;
    this->mOpacity = 1.0f;
    this->mAttribute2 = false;
    this->mAttribute3 = false;
    this->mDrawPriority = 1;
}

// FUNCTION: LLE 0x1005c631
// Loads every background table entry from the file (NULL for nameless records); returns the last read result
long UIBackgroundTable::Create(FSFileSpec& inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    long theResult;
    UIDbDataBackgrounds theData;
    inFile.Open(kFileAccessRead);
    theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        UIBackgroundTable::sUIBackgroundArray.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++) {
            UIBackgroundTable* theTable;
            theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theData), sizeof(theData), theRead);
            if (strlen(theData.mName) > 0) {
                theTable = new UIBackgroundTable;
                theTable->mID = theData.mID;
                theTable->mBackgroundID = theData.mBackgroundID;
                theTable->mFormID = theData.mFormID;
                theTable->mTextureFile.Copy(theData.mTextureFile, 0, strlen(theData.mTextureFile), 0, true);
                theTable->mTextureRect = GERectangle(long(theData.mTextureLeft), long(theData.mTextureTop), long(theData.mTextureRight),
                                                     long(theData.mTextureBottom));   // the long overload (int32_t fields)
                theTable->mBackgroundType = theData.mBackgroundType;
                theTable->mLayout.SetTop(theData.mTopOffsetType, theData.mTopOffset);
                theTable->mLayout.SetBottom(theData.mBottomOffsetType, theData.mBottomOffset);
                theTable->mLayout.SetLeft(theData.mLeftOffsetType, theData.mLeftOffset);
                theTable->mLayout.SetRight(theData.mRightOffsetType, theData.mRightOffset);
                theTable->mVisible = theData.mVisible;
                theTable->mHasAlpha = theData.mHasAlpha;
                theTable->mNumCols = theData.mNumCols;
                theTable->mNumRows = theData.mNumRows;
                theTable->mTileHeightPercent = theData.mTileHeightPercent;
                theTable->mTileWidthPercent = theData.mTileWidthPercent;
                theTable->mAttribute1 = theData.mAttribute1;
                theTable->mAttribute2 = theData.mAttribute2;
                theTable->mAttribute3 = theData.mAttribute3;
                theTable->mOpacity = theData.mOpacity;
                theTable->mColor = GEColor(theData.mRed * (1.0f / 255.0f), theData.mGreen * (1.0f / 255.0f),
                                           theData.mBlue * (1.0f / 255.0f));
                if (theData.mDrawPriority <= 1)
                    theTable->mDrawPriority = 1;
                else
                    theTable->mDrawPriority = theData.mDrawPriority;
            } else
                theTable = NULL;
            UIBackgroundTable::sUIBackgroundArray[i] = theTable;
        }
    }
    inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x1005c80e
// Deletes every background table entry and empties the table
void UIBackgroundTable::DeleteAll()
{
    for (unsigned long i = 0; i < UIBackgroundTable::sUIBackgroundArray.size(); i++)
        delete UIBackgroundTable::sUIBackgroundArray[i];
    UIBackgroundTable::sUIBackgroundArray.resize(0);
}

// Header-inline members first emitted here (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005c34a
// SYNTHETIC: LLE 0x1005c358
// SYNTHETIC: LLE 0x1005c370
// SYNTHETIC: LLE 0x1005c374
// SYNTHETIC: LLE 0x1005c378
// SYNTHETIC: LLE 0x1005c37c
// SYNTHETIC: LLE 0x1005c380
// SYNTHETIC: LLE 0x1005c384
// SYNTHETIC: LLE 0x1005c388
// SYNTHETIC: LLE 0x1005c38c
// SYNTHETIC: LLE 0x1005c390
// SYNTHETIC: LLE 0x1005c394
// SYNTHETIC: LLE 0x1005c398
// SYNTHETIC: LLE 0x1005c39c
// SYNTHETIC: LLE 0x1005c3aa
// SYNTHETIC: LLE 0x1005c3ae
// SYNTHETIC: LLE 0x1005c44b
// SYNTHETIC: LLE 0x1005c4f9

// Compiler-generated: the sUIBackgroundArray init chain (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005c575
// SYNTHETIC: LLE 0x1005c57f
// SYNTHETIC: LLE 0x1005c597
// SYNTHETIC: LLE 0x1005c5a3
