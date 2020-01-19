#include "../common.hpp"
#include "../../log.h"
#include <Psapi.h>

#include <locale>
#include <codecvt>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <sstream>

namespace Utilities
{
	namespace Memory
	{
		PBYTE FindPattern(PBYTE rangeStart, PBYTE rangeEnd, const char* pattern);
		DWORD WaitOnModuleHandle(std::string moduleName);
	}
}