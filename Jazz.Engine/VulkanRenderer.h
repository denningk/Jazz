#pragma once

#include "Types.h"

#include <vector>
#include <vulkan/vulkan.h>

namespace Jazz {

	struct VulkanSwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentationModes;
	};

	class Platform;

	class VulkanRenderer {
	public:
		VulkanRenderer(Platform* platform);
		~VulkanRenderer();

	private:
		VkPhysicalDevice selectPhysicalDevice();
		const bool physicalDeviceMeetsRequirements(VkPhysicalDevice physicalDevice);
		void detectQueueFamilyIndices(VkPhysicalDevice physicalDevice, I32* graphicsQueueIndex, I32* presentationQueueIndex);
		VulkanSwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice physicalDevice);
		void createLogicalDevice(std::vector<const char*>& requireValidationLayers);
		void createShader(const char* name);
		char* readShaderFile(const char* filename, const char* shaderType, U64* fileSize);
	private:
		Platform* _platform;

		VkInstance _instance;

		VkDebugUtilsMessengerEXT _debugMessenger;

		VkPhysicalDevice _physicalDevice;
		VkDevice _device; // Logical device
		I32 _graphicsFamilyQueueIndex;
		I32 _presentationFamilyQueueIndex;
		VkQueue _graphicsQueue;
		VkQueue _presentationQueue;

		VkSurfaceKHR _surface;

		U64 _shaderStageCount;
		std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	};
}