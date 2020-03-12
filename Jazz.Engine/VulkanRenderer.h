#pragma once

#include "Types.h"

#include <vulkan/vulkan.h>

namespace Jazz {

	class Platform;

	class VulkanRenderer {
	public:
		VulkanRenderer(Platform* platform);
		~VulkanRenderer();

	private:
		VkPhysicalDevice selectPhysicalDevice();
		const bool physicalDeviceMeetsRequirements(VkPhysicalDevice physicalDevice);
		void detectQueueFamilyIndices(VkPhysicalDevice physicalDevice, I32* graphicsQueueIndex, I32* presentationQueueIndex);
	private:
		Platform* _platform;

		VkInstance _instance;

		VkDebugUtilsMessengerEXT _debugMessenger;

		VkPhysicalDevice _physicalDevice;
		VkDevice _device; // Logical device

		VkSurfaceKHR _surface;
	};
}