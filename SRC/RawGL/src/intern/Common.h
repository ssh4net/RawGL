#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <array>

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <cassert>
#include <exception>

#include <algorithm>
#include <iterator>

#if 0
/* Is Windows OS? */
#define OS_WIN \
	defined _WINDOWS || defined WIN32 || defined _WIN32 || defined __WIN32__ || defined _WIN64

 // Is Mac OS?
#define OS_MAC \
	defined __APPLE__

 // Is Linux OS?
#define OS_LINUX \
	defined __linux__

// Is OS supported?
#define OS_SUPPORTED \
	OS_WIN /* || OS_MAC || OS_LINUX */

// Misc macro utility/
#define MINLINE static __forceinline

// Stringify code
#define STRINGIFY0(v) #v
#define STRINGIFY(v) STRINGIFY0(v)
#endif

extern const char* APP_NAME;
extern const int APP_VERSION[];
