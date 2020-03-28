#include "VulkanUtils.h"

namespace Jazz {

	U32 VulkanUtils::getMemoryType(U32 typeBits, VkPhysicalDeviceMemoryProperties& memoryProperties, VkMemoryPropertyFlags properties, VkBool32* memTypeFound) {
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
		}
		else {
			Logger::Error("Could not find a matching memory type");
			return 0;
		}
	}
}