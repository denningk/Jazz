#pragma once

#ifndef U32_MAX
#define U32_MAX 0xffffffffui32
#endif // !U32_max

#ifndef U64_MAX
#define U64_MAX 0xffffffffffffffffui64
#endif // !U32_max

#if _WIN32 || _WIN64
#define PLATFORM_WINDOWS
#else
#if __linux__
#define PLATFORM_LINUX
#elif __APPLE__
#define PLATFORM_MAC
#else
#error "Unable to determine platform!"
#endif
#endif

#ifdef PLATFORM_WINDOWS
#define FORCEINLINE __forceinline
#define FORCENOINLINE __declspec(noinline)
#ifdef JAZZ_BUILD_LIB
#define JAZZ_API __declspec(dllexport)
#else
#define JAZZ_API __declspec(dllimport)
#endif // PLATFORM_WINDOWS
#elif PLATFORM_LINUX || PLATFORM_MAC
#define FORCEINLINE inline
// TODO: Define JAZZ_API for these platforms
#endif

// Assertions
#define ASSERTIONS_ENABLED
#ifdef ASSERTIONS_ENABLED
#include <iostream>

#if _MSC_VER
#include <intrin.h>
#define debugBreak() __debugbreak();
#else
#define debugBreak() __asm { int 3 }
#endif

#define ASSERT(expr) { \
	if (expr) { \
	} else { \
		reportAssertionFailure(#expr, "", __FILE__, __LINE__); \
		debugBreak(); \
	} \
}

#define ASSERT_MSG(expr, message) { \
	if (expr) { \
	} else { \
		reportAssertionFailure(#expr, message, __FILE__, __LINE__); \
		debugBreak(); \
	} \
}

#ifdef _DEBUG
#define ASSERT_DEBUG(expr) { \
	if (expr) { \
	} else { \
		reportAssertionFailure(#expr, "", __FILE__, __LINE__); \
		debugBreak(); \
	} \
}
#else
#define ASSERT_DEBUG(expr) // Does nothing
#endif

FORCEINLINE void reportAssertionFailure(const char* expression, const char* message, const char* file, int line) {
	std::cerr << "Assertion Failure: " << expression << ", message: '" << message << "', file: " << file << ", line: " << line << "\n";
}

#else
#define ASSERT(expr) // Does nothing
#define ASSERT_MSG(expr, message) // Does nothing
#define ASSERT_DEBUG(expr) // Does nothing
#endif
