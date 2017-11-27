// Copyright (c) Isildur 2003, Jérémy Ansel 2014
// Licensed under the MIT license. See LICENSE.txt

#pragma once

#include <sdkddkver.h>

#undef NTDDI_VERSION
#undef _WIN32_WINNT
#undef WINVER

#define NTDDI_VERSION		NTDDI_WINXPSP2
#define _WIN32_WINNT		_WIN32_WINNT_WINXP
#define WINVER				_WIN32_WINNT
