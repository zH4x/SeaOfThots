// dllmain.cpp
#include "SeaOfThieves.h"

HANDLE mainThread;

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		SeaOfThieves::SetModule(hModule);
		//SeaOfThieves::Initialise();
		mainThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&SeaOfThieves::Initialise, NULL, NULL, NULL);
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		SeaOfThieves::UnHookRender();
		//TerminateThread(mainThread, 0);
		break;
	}
	return TRUE;
}