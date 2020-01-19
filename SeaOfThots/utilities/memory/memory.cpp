#include "memory.hpp"
#include "..//..//log.h"

#define INRANGE(x,a,b)    (x >= a && x <= b) 
#define getBits( x )    (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define getByte( x )    (getBits(x[0]) << 4 | getBits(x[1]))

bool bCompare(const BYTE* Data, const BYTE* Mask, const char* szMask)
{
	for (; *szMask; ++szMask, ++Mask, ++Data)
		if (*szMask == 'x' && *Mask != *Data)
			return false;

	return (*szMask) == 0;
}


DWORD Utilities::Memory::WaitOnModuleHandle(std::string moduleName)
{
	DWORD ModuleHandle = NULL;

	while (!ModuleHandle)
	{
		ModuleHandle = (DWORD)GetModuleHandleA(moduleName.c_str());
		if (!ModuleHandle)
			Sleep(50);
	}

	return ModuleHandle;
}

PBYTE Utilities::Memory::FindPattern(PBYTE rangeStart, PBYTE rangeEnd, const char* pattern)
{
	const unsigned char* pat = reinterpret_cast<const unsigned char*>(pattern);
	PBYTE firstMatch = 0;
	for (PBYTE pCur = rangeStart; pCur < rangeEnd; ++pCur) {
		if (*(PBYTE)pat == (BYTE)'\?' || *pCur == getByte(pat)) {
			if (!firstMatch) {
				firstMatch = pCur;
			}
			pat += (*(PWORD)pat == (WORD)'\?\?' || *(PBYTE)pat != (BYTE)'\?') ? 3 : 2;
			if (!*pat) {
				return firstMatch;
			}
		}
		else if (firstMatch) {
			pCur = firstMatch;
			pat = reinterpret_cast<const unsigned char*>(pattern);
			firstMatch = 0;
		}
	}
	return NULL;
}
