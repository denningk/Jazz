#pragma once

#include <vulkan/vulkan.h>
#include "Defines.h"
#include "Logger.h"
#include "Types.h"

#define VK_CHECK(expr) { \
	ASSERT(expr == VK_SUCCESS); \
}

namespace Jazz {

	class VulkanUtils {
	public:
		static U32 getMemoryType(U32 typeBits, VkPhysicalDeviceMemoryProperties &memoryProperties, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr);
	};
}