// Copyright (c) Isildur 2003, Jérémy Ansel 2014
// Licensed under the MIT license. See LICENSE.txt

#pragma once

#include <Vfw.h>

BOOL aviaudioPlay(PAVISTREAM pavi, LONG lStart, LONG lEnd, BOOL fWait);

void aviaudioMessage(unsigned msg, WPARAM wParam, LONG lParam);

void aviaudioStop(void);

LONG aviaudioTime(void);

void CALLBACK aviwaveOutProc(
	HWAVEOUT hwo,
	UINT uMsg,
	DWORD dwInstance,
	DWORD dwParam1,
	DWORD dwParam2
	);
