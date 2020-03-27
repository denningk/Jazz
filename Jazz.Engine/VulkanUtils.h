#pragma once

#include <vulkan/vulkan.h>
#include "Defines.h"
#include "Logger.h"

#define VK_CHECK(expr) { \
	ASSERT(expr == VK_SUCCESS); \
}

namespace Jazz {
	U32 getMemoryType(U32 typeBits,VkPhysicalDeviceMemoryProperties &memoryProperties, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) {
		for (U32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
			if ((typeBits & 1) == 1) {
				if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					if (memTypeFound) {
						*memTypeFound = true;
					}
					return i;
				}
			}
			typeBits >>= 1;
		}

		if (memTypeFound) {
			*memTypeFound = false;
			return 0;
		} else {
			Logger::Error("Could not find a matching memory type");
		}
	}
}