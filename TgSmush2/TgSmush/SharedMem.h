#pragma once

struct SharedMemData
{
	int videoFrameIndex;
	int videoFrameWidth;
	int videoFrameHeight;
	int videoDataLength;
	char* videoDataPtr;
};

SharedMemData* GetTgSmushVideoSharedMem();
