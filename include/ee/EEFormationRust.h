// New (port): the plain data of a formation's Place in Rust (ee_formations; rust/ee_rust.h, docs/RUST_PLAN.md R5b.1):
// the frame, the units' outer collision radii in input order, room for the slots (which name units by index).
// Requires ee/EEFormation.h and rust/ee_rust.h.
#ifndef EE_EEFORMATIONRUST_H
#define EE_EEFORMATIONRUST_H

struct EEFormationRadii {
    EEFormationRadii(const EEFormation* inFormation, std::vector<EEComplexUnit*>& inUnits);
    const float* Radii() const { return this->mRadii.empty() ? NULL : &this->mRadii[0]; }
    unsigned int Count() const { return (unsigned int)this->mRadii.size(); }
    EeFormationSlot* Slots() { return this->mSlots.empty() ? NULL : &this->mSlots[0]; }
    // the first inCount slots onto outSlots, their units from inUnits
    void Append(unsigned int inCount, std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots);

    EeFormationFrame mFrame;
    std::vector<float> mRadii;
    std::vector<EeFormationSlot> mSlots;
};

#endif
