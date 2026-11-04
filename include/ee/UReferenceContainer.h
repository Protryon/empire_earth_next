// UReferenceContainer.h (RECONSTRUCTION; file name inferred). A polymorphic vector of
// referenced pointers (RTTI UReferenceContainer<EEAreaEffect*> vtable 0x446e74, <EEComplexUnit*> vtable 0x4429d8;
// sizeof 0x14). <EEAreaEffect*>: dtor body 0x1ffd23 (re-stores its vptr, Clear(), then the vector's dtor), ??_G
// 0x1ffd44, Clear 0x1f5364 (Dereference every element, then clear()). Bodies inferred.
// Requires lle/LowLevelEngine.h (UReferenceCounter), <vector>.
#ifndef EE_UREFERENCECONTAINER_H
#define EE_UREFERENCECONTAINER_H

#include <algorithm>
#include <vector>

template <class T>
class UReferenceContainer {
public:
    // out of line (both instantiations call them: dtor 0x16cd30 / 0x1ffd23, Clear 0x16cfad / 0x1f5364);
    // defined below the class without `inline`, so VC6 /Ob1 never expands them
    virtual ~UReferenceContainer();
    void Clear();

    // out of line in the exe (<EEComplexUnit*>: 0x91f11, 0x27c820, 0x16cfd0, 0x16cd51; EEUGLoadContainer calls them);
    // declared only here (provisional names)
    unsigned long GetSize() const { return this->mItems.size(); }  // inline (out-of-line COMDAT 0x91f11, 67 callers)
    bool Contains(const T& inItem, std::vector<T>::iterator& outIterator);
    // out of line in the exe: 0x27c873 (end: [this+0xc]) and the begin() call Contains makes (ICF-folded into
    // vector<long>::end 0x1f5388: [ecx+8])
    std::vector<T>::iterator begin() { return this->mItems.begin(); }
    std::vector<T>::iterator end() { return this->mItems.end(); }
    // inline: EEUGLoadContainer::AddUnit 0x225ab4 (the iterator's frame slot is reused afterwards) (provisional name)
    bool Contains(const T& inItem)
    {
        std::vector<T>::iterator theIterator;
        return this->Contains(inItem, theIterator);
    }
    // inline: EEPlayer 0x16368f/0x1636cd copy the item into a frame slot, Reference() it inline, then push_back the copy
    void Add(T inItem)
    {
        inItem->Reference();
        this->mItems.push_back(inItem);
    }
    bool Erase(std::vector<T>::iterator& ioIterator);
    bool Remove(const T& inItem);
    // <EEComplexUnit*> 0x16cd7f (EEPlayer 0x1680cd): inOld's slot gets inNew (its reference moved) (provisional name)
    bool Replace(const T& inOld, const T& inNew);
    // 0x1ffd60 (<EEAreaEffect*>, ret 4; EEUnitAreaEffectList::Fn27C721 0x27c721): Clear, then a referenced copy of
    // each of inOther's items (provisional name)
    // void: the body never loads eax (`pop; ret 4` after the loop)
    void operator=(const UReferenceContainer& inOther);

    std::vector<T> mItems;                  // +0x04 (provisional name)
};

template <class T>
UReferenceContainer<T>::~UReferenceContainer()
{
    this->Clear();
    this->mItems.clear();  // both dtors 0x16cd30 / 0x1ffd23 call clear() before _Tidy
}

template <class T>
void UReferenceContainer<T>::Clear()
{
    for (std::vector<T>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator)
        (*theIterator)->Dereference();
    this->mItems.clear();
}

// out of line (<EEComplexUnit*>: Remove 0x16cd51, Replace 0x16cd7f, Erase 0x16cfd0 in EEPlayer.cpp's range, after
// their first user)
template <class T>
bool UReferenceContainer<T>::Contains(const T& inItem, std::vector<T>::iterator& outIterator)
{
    outIterator = std::find(this->begin(), this->end(), inItem);
    return outIterator != this->end();
}

template <class T>
bool UReferenceContainer<T>::Remove(const T& inItem)
{
    bool theResult = false;
    std::vector<T>::iterator theIterator;
    if (this->Contains(inItem, theIterator))
        theResult = this->Erase(theIterator);
    return theResult;
}

template <class T>
bool UReferenceContainer<T>::Replace(const T& inOld, const T& inNew)
{
    std::vector<T>::iterator theIterator;
    bool theResult = this->Contains(inOld, theIterator);
    if (theResult) {
        (*theIterator)->Dereference();
        *theIterator = inNew;
        (*theIterator)->Reference();
    }
    return theResult;
}

template <class T>
void UReferenceContainer<T>::operator=(const UReferenceContainer<T>& inOther)
{
    this->Clear();
    this->mItems.clear();
    for (std::vector<T>::const_iterator theIterator = inOther.mItems.begin(); theIterator != inOther.mItems.end();
         ++theIterator)
        this->Add(*theIterator);
}

template <class T>
bool UReferenceContainer<T>::Erase(std::vector<T>::iterator& ioIterator)
{
    bool theResult = false;
    if (ioIterator != this->mItems.end()) {
        (*ioIterator)->Dereference();
        theResult = true;
        this->mItems.erase(ioIterator);
    }
    return theResult;
}

#endif
