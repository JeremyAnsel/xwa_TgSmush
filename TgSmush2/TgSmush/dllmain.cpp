// Copyright (c) Isildur 2003, Jérémy Ansel 2014
// Licensed under the MIT license. See LICENSE.txt

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <Windows.h>

#include <string>
#include <fstream>

bool IsXwaExe()
{
	char filename[4096];

	if (GetModuleFileNameA(nullptr, filename, sizeof(filename)) == 0)
	{
		return false;
	}

	int length = strlen(filename);

	if (length < 17)
	{
		return false;
	}

	return _stricmp(filename + length - 17, "xwingalliance.exe") == 0;
}

void VirtualProtectMemoryReadWrite()
{
	DWORD oldProtection;
	VirtualProtect((void*)0x401000, 0x1A7B40, PAGE_READWRITE, &oldProtection);
}

void VirtualProtectMemoryExecuteReadWrite()
{
	DWORD oldProtection;
	VirtualProtect((void*)0x401000, 0x1A7B40, PAGE_EXECUTE_READWRITE, &oldProtection);
}

int XwaPlayMovieHook(int esp)
{
	const int* params = &esp;

	const auto XwaGetCdDriveLetter = (char(*)())0x0052B100;

	const char* name = (char*)params[68];
	char* buffer = (char*)(params + 3);

	const char* extensions[] =
	{
		".avi",
		".wmv",
		".mp4",
		".znm",
		".snm"
	};

	for (const char* ext : extensions)
	{
		std::string filename = std::string("movies\\") + name + ext;

		if (std::ifstream(filename))
		{
			strcpy_s(buffer, 256, filename.c_str());
			return 1;
		}
	}

	for (const char* ext : extensions)
	{
		char driveLetter = XwaGetCdDriveLetter();

		if (driveLetter == 0)
		{
			break;
		}

		std::string filename = driveLetter + std::string(":\\movies\\") + name + ext;

		if (std::ifstream(filename))
		{
			strcpy_s(buffer, 256, filename.c_str());
			return 1;
		}
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		if (IsXwaExe())
		{
			VirtualProtectMemoryReadWrite();

			// XwaPlayMovieHook
			*(unsigned short*)(0x0055BC29 + 0x00) = 0xDB33;
			*(unsigned short*)(0x0055BC29 + 0x02) = 0x1D89;
			*(int*)(0x0055BC29 + 0x04) = 0x009F4B40;
			*(unsigned char*)(0x0055BC29 + 0x08) = 0xE8;
			*(int*)(0x0055BC29 + 0x09) = (int)XwaPlayMovieHook - (0x0055BC29 + 0x0D);
			*(unsigned short*)(0x0055BC29 + 0x0D) = 0x67EB;
			*(unsigned char*)(0x0055BCB0 + 0x00) = 0x90;
			*(unsigned int*)(0x0055BCB0 + 0x01) = 0x90909090;

			VirtualProtectMemoryExecuteReadWrite();
		}

		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}
