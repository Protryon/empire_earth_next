// WONGUI's multi-column lists and tree rows (MultiListArea.cpp, MultiListHeader.cpp, TreeItem.cpp): the
// original's statements (copied from the C++ before the port, the columns and header buttons replaced by plain
// structs, their SetPos / SetPosSize calls by the values they received) against ee_won's guimulti module through
// its C ABI, over generated inputs (a fixed LCG). Prints one line per failed check; the exit code is the number of
// failed checks.
#include <stdio.h>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 4242;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Any()
{
    switch (Next() % 8) {
    case 0: return (int)Next() - (int)(Next() << 8);
    case 1: return 0x7fffffff - (int)(Next() % 3);
    case 2: return (int)0x80000000 + (int)(Next() % 3);
    default: return (int)(Next() % 300) - 30;
    }
}
static int Val(int n) { return n % 13 == 0 ? Any() : (int)(Next() % 200) - 10; }

struct ColumnInfo { int mx, mWidth; unsigned char mSizeMode; int mPartnerColumn; };

static void Columns()
{
    for (int n = 0; n < 300000; n++) {
        int count = (int)(Next() % 6);
        std::vector<ColumnInfo> cols(count);
        for (int i = 0; i < count; i++) {
            cols[i].mx = Val(n);
            cols[i].mWidth = Val(n);
            cols[i].mSizeMode = (unsigned char)(Next() % 3);
            cols[i].mPartnerColumn = (int)(Next() % 8) - 2;
        }
        // GetColumnAt
        {
            int x = Val(n), o = (int)cols.size()-1;
            for(int i=0;i<cols.size();++i) if(cols[i].mx+cols[i].mWidth>=x) { o=i; break; }
            std::vector<int> xs, ws;
            for (int i = 0; i < count; i++) { xs.push_back(cols[i].mx); ws.push_back(cols[i].mWidth); }
            CHECK(o == ee_won_gui_ml_column_at(count ? &xs[0] : NULL, count ? &ws[0] : NULL, count, x));
        }
        // SetColumnWidth
        {
            int theCol = (int)(Next() % 8) - 1, theWidth = Val(n), mMaxTotalColumnWidth = Next() % 3 ? 0 : Val(n);
            int mTotalColumnWidth = Val(n);
            bool fromSizeChanged = Next() % 4 == 0;
            std::vector<ColumnInfo> o = cols;
            int oTotal = mTotalColumnWidth;
            bool changed = false;
            if(!(theCol >= o.size())) {
                changed = true;
                int w = theWidth;
                if(w<0) w = 0;
                ColumnInfo &aCol = o[theCol];
                if(!fromSizeChanged && aCol.mSizeMode==2 && aCol.mPartnerColumn>=0 && aCol.mPartnerColumn<o.size()) {
                    ColumnInfo &aCol2 = o[aCol.mPartnerColumn];
                    int aDelta = w - aCol.mWidth;
                    if(aDelta>aCol2.mWidth) aDelta = aCol2.mWidth;
                    aCol.mWidth += aDelta;
                    aCol2.mWidth -= aDelta;
                } else {
                    int aDelta = w - aCol.mWidth;
                    if(mMaxTotalColumnWidth>0 && aDelta>0 && oTotal+aDelta>mMaxTotalColumnWidth) {
                        aDelta = mMaxTotalColumnWidth - oTotal;
                        if(aDelta<0) aDelta = 0;
                    }
                    aCol.mWidth += aDelta;
                    oTotal += aDelta;
                }
                int aTotalWidth = 0;
                for(int i=0; i<o.size(); i++) { o[i].mx = aTotalWidth; aTotalWidth += o[i].mWidth; }
            }
            std::vector<int> ws, xs, ps;
            std::vector<unsigned char> ms;
            for (int i = 0; i < count; i++) { ws.push_back(cols[i].mWidth); xs.push_back(cols[i].mx); ms.push_back(cols[i].mSizeMode); ps.push_back(cols[i].mPartnerColumn); }
            int rTotal = mTotalColumnWidth;
            bool rc = ee_won_gui_ml_set_width(count ? &ws[0] : NULL, count ? &ms[0] : NULL, count ? &ps[0] : NULL,
                                              count ? &xs[0] : NULL, count, &rTotal, mMaxTotalColumnWidth, theCol, theWidth,
                                              fromSizeChanged);
            bool same = rc == changed && rTotal == oTotal;
            for (int i = 0; i < count; i++) same = same && ws[i] == o[i].mWidth && xs[i] == o[i].mx;
            CHECK(same);
        }
        // Virtual08
        {
            int mDynamicSizeColumn = (int)(Next() % 8) - 2, viewW = Val(n), oldW = Val(n), total = Val(n);
            int o = 0;
            if ((mDynamicSizeColumn >= 0) && (mDynamicSizeColumn < (int)cols.size())) {
                int aSizeDelta = viewW - oldW;
                if(total + aSizeDelta > viewW) { aSizeDelta = viewW - total; if(aSizeDelta<0) aSizeDelta = 0; }
                o = aSizeDelta;
            }
            CHECK(o == ee_won_gui_ml_dynamic_delta(mDynamicSizeColumn, count, viewW, oldW, total));
        }
        // UpdateItem's grow
        {
            int itemW = Val(n), border = (int)(Next() % 5), colW = Val(n);
            int o = -1;
            if(!(itemW>0)) o = -1;
            else if(itemW + border > colW) o = itemW + border;
            CHECK(o == ee_won_gui_ml_grow(itemW, border, colW));
        }
        // SetColumnSizeMode's links
        {
            int anOldMode = (int)(Next() % 3), theMode = (int)(Next() % 3);
            CHECK(!(theMode!=2 && anOldMode!=2) == ee_won_gui_ml_relink((unsigned char)anOldMode, (unsigned char)theMode));
            std::vector<ColumnInfo> o = cols;
            int aPrevPartner = -1;
            for(int i=0; i<o.size(); i++) {
                ColumnInfo &anInfo = o[i];
                if(anInfo.mSizeMode!=2) continue;
                if(aPrevPartner>=0) o[aPrevPartner].mPartnerColumn = i;
                aPrevPartner = i;
            }
            std::vector<unsigned char> ms; std::vector<int> ps;
            for (int i = 0; i < count; i++) { ms.push_back(cols[i].mSizeMode); ps.push_back(cols[i].mPartnerColumn); }
            ee_won_gui_ml_link_partners(count ? &ms[0] : NULL, count ? &ps[0] : NULL, count);
            bool same = true;
            for (int i = 0; i < count; i++) same = same && ps[i] == o[i].mPartnerColumn;
            CHECK(same);
        }
        // the header: NotifyColumnChange
        {
            int size = (int)(Next() % 7), horz = Val(n);
            std::vector<char> has(size);
            for (int i = 0; i < size; i++) has[i] = Next() % 4 != 0;
            std::vector<EeWonHeaderCol> o;
            int x=-horz;
            for(int i=0;i<size;++i) {
                int width=-1;
                if(i<cols.size()) width=cols[i].mWidth;
                bool enabled = true;
                if(has[i]) {
                    if(width==-1) { enabled=false; width=0; } else enabled=true;
                } else enabled = width!=-1;
                EeWonHeaderCol c = { x, width, enabled };
                o.push_back(c);
                x+=width;
            }
            std::vector<int> ws;
            for (int i = 0; i < count; i++) ws.push_back(cols[i].mWidth);
            std::vector<EeWonHeaderCol> r(size + 1);
            ee_won_gui_mh_layout((const bool*)(size ? &has[0] : NULL), size, count ? &ws[0] : NULL, count, horz, &r[0]);
            bool same = true;
            // the original's SetEnabled / SetVisible / SetPosSize reach only the columns with a button
            for (int i = 0; i < size; i++)
                same = same && r[i].x == o[i].x && (has[i] ? (r[i].width == o[i].width && r[i].enabled == o[i].enabled) : true);
            CHECK(same);
        }
        // HandleComponentEvent's sort, GetSortType
        {
            int type = (int)(Next() % 3), cur = (int)(Next() % 5) - 1, column = (int)(Next() % 6) - 1, size = (int)(Next() % 5);
            int t = type;
            if(column==cur || t==2) t = t==0 ? 1 : 0;
            CHECK(t == ee_won_gui_mh_toggle(type, column==cur));
            int o = (column<0 || column>=size || column!=cur) ? 2 : type;
            CHECK(o == ee_won_gui_mh_sort_type(column, size, cur, type));
        }
        // GetDragComponent, MouseDrag
        {
            int size = (int)(Next() % 6);
            std::vector<EeWonHeaderButton> b(size);
            int left = (int)(Next() % 10) - 5;
            for (int i = 0; i < size; i++) {
                b[i].present = Next() % 5 != 0;
                b[i].left = left;
                b[i].width = n % 17 == 0 ? Any() : (int)(Next() % 60);
                b[i].right = b[i].left + b[i].width;
                left = b[i].right + (Next() % 4 == 0 ? (int)(Next() % 5) - 2 : 0);
            }
            int x = (int)(Next() % 250) - 10;
            int mDragColumn=-1, mDragX=0;
            for(int i=0;i<b.size();++i) {
                if(!b[i].present || !(x>=b[i].left) || !(x<=b[i].right)) continue;
                int width=b[i].width/4;
                if(width>5) width=5;
                if(x-b[i].left<width && i!=0) mDragColumn=i-1;
                else if(b[i].right-x<width && i!=b.size()) mDragColumn=i;
                else break;
                if(mDragColumn>=cols.size()) { mDragColumn=-1; break; }
                ColumnInfo& info=cols[mDragColumn];
                if(info.mSizeMode==0 || (info.mSizeMode==2 && (info.mPartnerColumn<0 || info.mPartnerColumn>=cols.size()))) { mDragColumn=-1; break; }
                mDragX=x-cols[mDragColumn].mWidth;
                break;
            }
            std::vector<int> ws, ps; std::vector<unsigned char> ms;
            for (int i = 0; i < count; i++) { ws.push_back(cols[i].mWidth); ms.push_back(cols[i].mSizeMode); ps.push_back(cols[i].mPartnerColumn); }
            int rx = 12345;
            int rc = ee_won_gui_mh_drag(x, size ? &b[0] : NULL, size, count ? &ws[0] : NULL, count ? &ms[0] : NULL,
                                        count ? &ps[0] : NULL, count, &rx);
            CHECK(rc == mDragColumn && (rc < 0 || rx == mDragX));
            int w = x - mDragX;
            if(w<0) w=0;
            CHECK(w == ee_won_gui_mh_drag_width(x, mDragX));
        }
        // TreeItem: the +/- box, InsertChild's indent and place
        {
            int x = Val(n), mX = Val(n);
            CHECK((x < mX && x > mX-14) == ee_won_gui_tree_box_hit(x, mX));
            bool root = (Next() & 1) != 0;
            int flags = (int)(Next() % 16), thePos = (int)(Next() % 10) - 2;
            size_t size = Next() % 6;
            int indent = (root && !(flags & 3)) ? 0 : mX + 15;
            int p = thePos;
            if(p<0 || p>size) p = (int)size;
            size_t rp = 99;
            int ri = ee_won_gui_tree_child(root, flags, mX, thePos, size, &rp);
            CHECK(ri == indent && rp == (size_t)p);
        }
    }
}

int main()
{
    Columns();
    printf("won_guimulti_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
