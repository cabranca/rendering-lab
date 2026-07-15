#pragma once

#include "Logger.h"

#ifdef __DEBUG__
	#define CBK_ENABLE_ASSERTS
	#if defined(_WIN32) && defined(_WIN64)
		#define CBK_DEBUG_BREAK __debugbreak()
	#elif defined(__linux__) || defined(__APPLE__)
		#include <signal.h>
		#define CBK_DEBUG_BREAK raise(SIGTRAP)
	#endif
#endif

#ifdef CBK_ENABLE_ASSERTS
	#define CBK_ASSERT(x, ...) { if(!(x)) { CBK_ERROR("Assertion failed: {0}", __VA_ARGS__); CBK_DEBUG_BREAK; } }
#else
	#define CBK_ASSERT(x, ...)
#endif
