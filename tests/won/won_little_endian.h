#ifndef EE_WONAPI_WONLITTLEENDIAN_H
#define EE_WONAPI_WONLITTLEENDIAN_H
namespace WONAPI {
inline bool IsLittleEndian()
{
	short aTest = 1;
	return ((*reinterpret_cast<char*>(&aTest))&1)?true:false;
}

inline unsigned long LongToLittleEndian(unsigned long theLong)
{
	if(IsLittleEndian())
		return theLong;
	else
		return (theLong>>24) | ((theLong>>8)&0xFF00) | ((theLong<<8)&0xFF0000) | (theLong<<24);
}

inline unsigned long LongFromLittleEndian(unsigned long theLong)
{
	return LongToLittleEndian(theLong);
}
}
#endif
