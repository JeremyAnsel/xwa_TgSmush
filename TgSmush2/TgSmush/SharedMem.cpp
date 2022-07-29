#include "targetver.h"
#include "SharedMem.h"
#include <Windows.h>

template<class T>
class SharedMem
{
public:
	SharedMem(LPCWSTR name, bool openOrCreate);
	~SharedMem();

	T* GetMemoryPointer();

private:
	HANDLE hMapFile;
	T* pSharedData;
};

template<class T>
SharedMem<T>::SharedMem(LPCWSTR name, bool openOrCreate)
{
	pSharedData = nullptr;

	if (openOrCreate)
	{
		hMapFile = CreateFileMapping(
			INVALID_HANDLE_VALUE,    // use paging file
			NULL,                    // default security
			PAGE_READWRITE,          // read/write access
			0,                       // maximum object size (high-order DWORD)
			sizeof(T),         // maximum object size (low-order DWORD)
			name);        // name of mapping object
	}
	else
	{
		hMapFile = OpenFileMapping(
			FILE_MAP_ALL_ACCESS,   // read/write access
			FALSE,                 // do not inherit the name
			name);      // name of mapping object
	}

	if (!hMapFile)
	{
		return;
	}

	pSharedData = (SharedMemData*)MapViewOfFile(
		hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		sizeof(T));
}

template<class T>
SharedMem<T>::~SharedMem()
{
	UnmapViewOfFile(pSharedData);
	CloseHandle(hMapFile);
}

template<class T>
T* SharedMem<T>::GetMemoryPointer()
{
	return pSharedData;
}

SharedMemData* GetTgSmushVideoSharedMem()
{
	static SharedMem<SharedMemData> g_tgSmushVideoSharedMem(L"Local\\TgSmushVideo", true);
	return g_tgSmushVideoSharedMem.GetMemoryPointer();
}
