#include <vector>

#include "Platform.h"
#include "Logger.h"
#include "Defines.h"
#include "VulkanUtils.h"
#include "VulkanRenderer.h"

namespace Jazz {

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		switch (messageSeverity) {
		default:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			Logger::Error(pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			Logger::Warn(pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			Logger::Log(pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			Logger::Trace(pCallbackData->pMessage);
			break;
		}

		return VK_FALSE;
	}

	VulkanRenderer::VulkanRenderer(Platform* platform) {
		_platform = platform;
		Logger::Trace("Initializing Vulkan renderer...");

		VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		appInfo.apiVersion = VK_API_VERSION_1_2;
		appInfo.pApplicationName = "Jazz";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

		VkInstanceCreateInfo instanceCreateInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
		instanceCreateInfo.pApplicationInfo = &appInfo;

		// Extensions
		const char** pfe = nullptr;
		U32 count = 0;
		_platform->GetRequiredExtensions(&count, &pfe);
		std::vector<const char*> platformExtensions;
		for (U32 i = 0; i < count; i++) {
			platformExtensions.push_back(pfe[i]);
		}

		platformExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		instanceCreateInfo.enabledExtensionCount = (U32)platformExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = platformExtensions.data();

		// Validation Layers
		std::vector<const char*> requiredValidationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		// Get available layers
		U32 availableLayerCount = 0;
		VK_CHECK(vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr));
		std::vector<VkLayerProperties> availableLayers(availableLayerCount);
		VK_CHECK(vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data()));

		// Verify that all required layers are available
		bool success = true;
		for (U32 i = 0; i < (U32)requiredValidationLayers.size(); ++i) {
			bool found = false;
			for (U32 j = 0; j < availableLayerCount; ++j) {
				if (strcmp(requiredValidationLayers[i], availableLayers[j].layerName) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				success = false;
				Logger::Fatal("Required validation layer is missing: %s", requiredValidationLayers[i]);
				break;
			}
		}

		instanceCreateInfo.enabledLayerCount = (U32)requiredValidationLayers.size();
		instanceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();

		// Create instance
		VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &_instance));

		// Create debugger
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback;
		debugCreateInfo.pUserData = this;

		PFN_vkCreateDebugUtilsMessengerEXT debugMessengerFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
		ASSERT_MSG(debugMessengerFunc, "Failed to create debug messenger!");
		debugMessengerFunc(_instance, &debugCreateInfo, nullptr, &_debugMessenger);

		_platform->CreateSurface(_instance, &_surface);
	}

	VulkanRenderer::~VulkanRenderer() {

		if (_debugMessenger) {
			PFN_vkDestroyDebugUtilsMessengerEXT debugMessengerFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
			// ASSERT_MSG(createDebugMessenger, "Failed to destroy debug messenger!");
			debugMessengerFunc(_instance, _debugMessenger, nullptr);
		}

		vkDestroyInstance(_instance, nullptr);
	}

	VkPhysicalDevice VulkanRenderer::selectPhysicalDevice() {
		U32 deviceCount = 0;
		vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			Logger::Fatal("No supported physical devices were found.");
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

		for (U32 i = 0; i < deviceCount; ++i) {
			if (physicalDeviceMeetsRequirements(devices[i])) {
				return devices[i];
			}
		}

		Logger::Fatal("No devices found which meet application requirements");
		return nullptr;
	}

	const bool VulkanRenderer::physicalDeviceMeetsRequirements(VkPhysicalDevice physicalDevice) {
		I32 graphicsQueueIndex = -1;
		I32 presentationQueueIndex = -1;
		
	}

	void VulkanRenderer::detectQueueFamilyIndices(VkPhysicalDevice physicalDevice, I32* graphicsQueueIndex, I32* presentationQueueIndex) {
		U32 queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> familyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, familyProperties.data());

		for (U32 i = 0; i < queueFamilyCount; ++i) {

			// Does it support the graphics queue?
			if (familyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				*graphicsQueueIndex = i;
			}

			VkBool32 supportsPresentation = VK_FALSE;
		}
	}
}