// C:\EEX\Low-Level Engine\Graphics Engine\GEPolygonGroup.h (RECONSTRUCTION). All inline (GEBoundingBox.cpp emits the
// block first, 0x10010c99..0x10010d9d).
#ifndef LLE_GEPOLYGONGROUP_H
#define LLE_GEPOLYGONGROUP_H

#include <vector>

class GEPolygon;

class LLE_API GEPolygonGroup {              // sizeof 0x20
public:
    GEPolygonGroup() : mName(32) {}
    GEPolygonGroup(const UString& inName) : mName(32) { this->mName = inName; }
    ~GEPolygonGroup() {}
    bool Contains(GEPolygon* inPolygon)
    {
        bool theFound = false;
        for (unsigned long i = 0; i < this->mPolygons.size(); i++)
            if (this->mPolygons[i] == inPolygon) {
                theFound = true;
                break;
            }
        return theFound;
    }
    const UString& GetName() const { return this->mName; }
    unsigned long GetPolygonCount() const { return this->mPolygons.size(); }
    void SetName(const UString& inName) { this->mName = inName; }
private:
    std::vector<GEPolygon*> mPolygons;      // +0x00
    UString mName;                          // +0x10
};

#endif
