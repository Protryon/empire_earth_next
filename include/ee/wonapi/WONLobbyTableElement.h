#ifndef EE_WONAPI_WONLOBBYTABLEELEMENT_H
#define EE_WONAPI_WONLOBBYTABLEELEMENT_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// TableElement (0x8c bytes), vtable 0x0085014c. The default constructor (0x0070d7f0) is defined in
// TextLayoutConstructors.cpp; the destructor and copy are out of line. The ColumnInfo (0x18) and
// RowInfo (0x24) vectors grow at 0x0070d9c0 and 0x0070dd20.
class TableElement : public LineSegment {
public:
    TableElement(); // at 0x0070d7f0 (allocated at 0x0073b0ab)
    struct ColumnInfo {
        int mx,mWeight,mWidth,mMinWidth,mMaxWidth,mColumnNum; //18
        ColumnInfo(int number=0) : mWeight(0),mWidth(0),mMinWidth(0),mMaxWidth(0),mColumnNum(number) {} // inline (as at 0x0070d9fa); mx deliberately not set
        void SetWidthToMin() { this->mWidth=this->mMinWidth; }
        void SetWidthToMax() { this->mWidth=this->mMaxWidth; }
    };
    struct CellInfo {
        SmartPtr<MultiLineElement> mContents;
        int mColumnSpan,mRowSpan,mBGColor,mWidth,mHeight;
        char mHAlign,mVAlign; bool mNoWrap; //1c
        CellInfo() : mContents(new MultiLineElement),mColumnSpan(1),mRowSpan(1),mBGColor(-1),mWidth(0),mHeight(0),mHAlign(-1),mVAlign(-1),mNoWrap(false) {} // as at 0x0070dc3d..0x0070dca3; 0x1c bytes
    };
    typedef std::vector<CellInfo> CellRow;
    struct CellInfoEx {
        CellInfo* mCellInfo;
        int mBorderX,mBorderY,mBorderWidth,mBorderHeight,mCellX,mCellY; //1c POD
    };
    struct RowInfo {
        int my,mHeight,mRowNum,mBGColor;
        char mHAlign,mVAlign;
        CellRow mRow; // offset 0x14; class size 0x24
        RowInfo(int number=0) : mHeight(0),mRowNum(number),mBGColor(-1),mHAlign(0),mVAlign(1) {} // inline (as at 0x0070dd9a); my deliberately not set
    };
    typedef std::vector<ColumnInfo> ColumnVector;
    typedef std::vector<RowInfo> RowVector;
    struct RowSearchPred { bool operator()(const RowInfo& a,const RowInfo& b) { return a.my<b.my; } };
    virtual int GetWidth() { return this->mWidth; } // at 0x0070d8a0
    virtual int GetMinWidth(); // cached calculation (0x0070eca0)
    virtual int GetMaxWidth(); // cached calculation (0x0070ecc0)
    virtual int GetHeight() { return this->mHeight; } // at 0x0070d8b0
    virtual int GetEndSelIndex() { return this->mEndSelIndex; }
    virtual void GetSelText(GUIString&,ElementSelectionInfo&);
    virtual bool FindStr(ElementFindInfo&);
    virtual void SetSelIndex(); // at 0x0070f500
    virtual LineSegment* GetSegmentAt(int&,int&,bool);
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&);
    virtual void SetMaxWidth(int);
    virtual void SetViewport(ScrollAreaViewport*,int,int);
    CellInfo* GetCellInfoAt(RowInfo&,int);
    CellInfo* GetCellInfoAt(int,int);
    MultiLineElement* GetCellAt(int,int);
    MultiLineElement* GetCellAt(RowInfo&,int);
    int FindRowAt(int);
    bool GetCellInfoEx(CellInfoEx&,int,int,RowInfo&,bool=false,bool=false);
    void PaintCell(Graphics&,int,int,int,int,RowInfo&,ElementPaintInfo&,bool);
    void CalcMinMaxWidths();
    void CalcRowHeights();
    void CalcColumnWidths(int);
    void StartTraversal(); // at 0x0070dfe0
    void SetRowBGColor(int); // at 0x0070e240
    void SetRowAlign(int,int); // at 0x0070e280
    void NextRow(); // at 0x0070e000
    CellInfo* NextColumn(); //30e020
    void SetSize(int,int); // resets to a fixed grid
    void Rewind(); // resets the cell cursor
    void AddSegment(LineSegment*,unsigned short=0); // adds to the current cell
    MultiLineElement* NextCell(); // advances the cell cursor
    void SetCellBGColor(int);
    void SetCellAlign(int,int);
    HorzAlign GetCurRowAlign(); // at 0x0070e070 (returns the row's alignment)
    void SetColumnWidth(int); // at 0x0070e2d0 (updates the column weight)
    void SetCellSpan(int,int,bool=false); //30e080
    void EnsureNumCols(RowInfo&,int); // creates and grows the cells, at 0x0070da90
    void EnsureNumCols(int); // at 0x0070d9c0
    void EnsureNumRows(int); // at 0x0070dd20
    ColumnVector mColumnVector; //14..23
    RowVector mRowVector; //24..33
    int mEndSelIndex; //34
    RowInfo mSearchRow; //38..5b
    int mCurRow,mCurCol; MultiLineElement* mCurCell; //5c,60,64
    int mWidth,mHeight,mMinWidth,mMaxWidth; //68..77
    int mCellSpacing,mCellPadding,mBorder,mBGColor,mWidthSpec; //78..8b
};
}
#endif
