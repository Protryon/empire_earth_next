// WON SDK Windows SocketThreadEx. No original address identified.
// No backend selection, base fields, or base virtual slots changed.
#ifndef __WON_SOCKETTHREADEX_WINDOWS_H__
#define __WON_SOCKETTHREADEX_WINDOWS_H__
// The base class comes from WONRouting.h.

#include <set>
#include <list>
#include <map>

#include <vector>
#include <string.h>
#include "ee/wonapi/WONRouting.h"

namespace WONAPI
{


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<class T> class GrowVector
{
private:
	T* mArray;
	int mSize;
	int mCapacity;

public:
	GrowVector() : mCapacity(0), mSize(0), mArray(NULL) {}
	~GrowVector() { delete [] this->mArray; }

	void push_back(const T& theElement)
	{
		if(this->mSize==this->mCapacity)
		{
			int aNewCapacity = this->mCapacity*2;
			if(aNewCapacity<32)
				aNewCapacity = 32;

			this->reserve(aNewCapacity);
		}

		this->mArray[this->mSize++] = theElement;
	}

	void reserve(int theCapacity)
	{
		if(theCapacity > this->mCapacity)
		{
			T* oldArray = this->mArray;
			this->mArray = new T[theCapacity];
			if(this->mSize!=0)
				memcpy(this->mArray, oldArray, sizeof(T)*this->mSize);		

			delete [] oldArray;
			this->mCapacity = theCapacity;
		}
	}

	void pop_back() { this->mSize--; }

	T* data() { return this->mArray; }
	int size() { return this->mSize; }
	int capacity() { return this->mCapacity; }
	void clear() { this->resize(0); }
	void resize(int theSize) { this->reserve(theSize); this->mSize = theSize; }

	T& operator [](int thePos) { return this->mArray[thePos]; }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class SocketThreadEx : public SocketThread
{
private:
	typedef std::map<SOCKET,SmartPtr<SocketOp> > OpMap;
	OpMap mOpMap[3];

	GrowVector<SOCKET> mSocketArray[3];
	GrowVector<SocketOp*> mOpArray[3];

	struct grow_fd_set
	{
		int mCapacity;
		char *mBuf;
		fd_set *mSet;

		grow_fd_set() : mBuf(NULL), mCapacity(0), mSet(NULL) {}
		~grow_fd_set() { delete this->mBuf; }

		void Copy(GrowVector<SOCKET> &theArray)
		{
			int aCapacity = theArray.capacity();
			if(this->mCapacity < aCapacity)
			{
				delete this->mBuf;
				this->mBuf = new char[sizeof(u_int) + aCapacity*sizeof(SOCKET)];
				this->mSet = (fd_set*)this->mBuf;

				this->mCapacity = aCapacity;
			}

			memcpy(this->mSet->fd_array,theArray.data(),theArray.size()*sizeof(SOCKET));
			this->mSet->fd_count = theArray.size();
		}
	};
		
	SmartPtr<AsyncSocket> mSignalSocket;
	grow_fd_set m_fdset[3];

protected:
	virtual void ThreadFunc();

public:
	SocketThreadEx();
	virtual ~SocketThreadEx();
	virtual void PurgeOps();

	virtual void Signal();

	virtual void AddSocketOp(SocketOp *theSocketOp);
	virtual void RemoveSocketOp(SocketOp *theSocketOp);
	void RemoveSocketOp(SocketOp *theSocketOp, int dontErase);
	virtual void Pump(DWORD theWaitTime);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}; // namespace WONAPI

#endif
