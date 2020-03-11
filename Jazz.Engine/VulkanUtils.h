#pragma once

#include <vulkan/vulkan.h>
#include "Defines.h"

#define VK_CHECK(expr) { \
	ASSERT(expr == VK_SUCCESS); \
}