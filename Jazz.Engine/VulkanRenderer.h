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
		void createSwapchain();
		void createSwapchainImagesAndViews();
		void createRenderPass();
		void createDepthStencil(VkFormat depthFormat);
		void createGraphicsPipeline();
		void createFramebuffers();
		void createCommandPool();
	private:
		Platform* _platform;

		VkInstance _instance;

		VkDebugUtilsMessengerEXT _debugMessenger;

		VkPhysicalDevice _physicalDevice;
		VkPhysicalDeviceMemoryProperties _physicalDeviceMemory;
		VkDevice _device; // Logical device
		I32 _graphicsFamilyQueueIndex;
		I32 _presentationFamilyQueueIndex;
		VkQueue _graphicsQueue;
		VkQueue _presentationQueue;

		VkSurfaceKHR _surface;

		U64 _shaderStageCount;
		std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

		VkSurfaceFormatKHR _swapchainImageFormat;
		VkExtent2D _swapchainExtent;
		VkSwapchainKHR _swapchain;

		std::vector<VkImage> _swapchainImages;
		std::vector<VkImageView> _swapchainImageViews;
		std::vector<VkFramebuffer> _swapChainFramebuffers;

		struct {
			VkImage image;
			VkDeviceMemory memory;
			VkImageView view;
		} _depthStencil;

		VkRenderPass _renderPass;
		VkPipelineLayout _pipelineLayout;
		VkPipeline _pipeline;
		VkCommandPool _commandPool;
	};
}