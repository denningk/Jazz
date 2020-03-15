#include <vector>
#include <fstream>

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

		// Create the surface
		_platform->CreateSurface(_instance, &_surface);
		
		// Select physical device
		_physicalDevice = selectPhysicalDevice();

		// Create logical device
		createLogicalDevice(requiredValidationLayers);

		// Shader creation
		createShader("main");
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
		detectQueueFamilyIndices(physicalDevice, &graphicsQueueIndex, &presentationQueueIndex);
		VulkanSwapchainSupportDetails swapchainSupport = querySwapchainSupport(physicalDevice);

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);

		bool supportsRequiredQueueFamilies = (graphicsQueueIndex != -1) && (presentationQueueIndex != -1);

		// Device extension support - Supported/Available extensions
		U32 extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

		// Required extensions
		std::vector<const char*> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	
		bool success = true;
		for (U64 i = 0; i < requiredExtensions.size(); ++i) {
			bool found = false;
			for (U64 j = 0; j < extensionCount; ++j) {
				if (strcmp(requiredExtensions[i], availableExtensions[j].extensionName) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				success = false;
				break;
			}
		}

		bool swapChainMeetsRequirements = false;
		if (supportsRequiredQueueFamilies) {
			swapChainMeetsRequirements = swapchainSupport.Formats.size() > 0 && swapchainSupport.PresentationModes.size() > 0;
		}

		// NOTE: Could also look for discrete GPU. We could score and rank them based on features and capabilities
		return supportsRequiredQueueFamilies && swapChainMeetsRequirements && features.samplerAnisotropy;
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
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, _surface, &supportsPresentation);
			if (supportsPresentation) {
				*presentationQueueIndex = i;
			}
		}
	}

	VulkanSwapchainSupportDetails VulkanRenderer::querySwapchainSupport(VkPhysicalDevice physicalDevice) {
		VulkanSwapchainSupportDetails details;

		// Capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, _surface, &details.Capabilities);

		// Surface formats
		U32 formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, _surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, _surface, &formatCount, details.Formats.data());
		}

		// Presentation modes
		U32 presentationModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, _surface, &presentationModeCount, nullptr);
		if (presentationModeCount != 0) {
			details.PresentationModes.resize(presentationModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, _surface, &presentationModeCount, details.PresentationModes.data());
		}

		return details;
	}

	void VulkanRenderer::createLogicalDevice(std::vector<const char*>& requiredValidationLayers) {
		I32 graphicsQueueIndex = -1;
		I32 presentationQueueIndex = -1;
		detectQueueFamilyIndices(_physicalDevice, &graphicsQueueIndex, &presentationQueueIndex);
		
		// If the queue indices are the same, only one queue needs to be created
		bool presentationSharesGraphicsQueue = graphicsQueueIndex == presentationQueueIndex;

		std::vector<U32> indices;
		indices.push_back(graphicsQueueIndex);
		if (!presentationSharesGraphicsQueue) {
			indices.push_back(presentationQueueIndex);
		}

		// Device queues
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(indices.size());
		for (U32 i = 0; i < (U32)indices.size(); ++i) {
			queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfos[i].queueFamilyIndex = indices[i];
			queueCreateInfos[i].queueCount = 1;
			queueCreateInfos[i].flags = 0;
			queueCreateInfos[i].pNext = nullptr;
			F32 queuePriority = 1.0f;
			queueCreateInfos[i].pQueuePriorities = &queuePriority;
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.samplerAnisotropy = VK_TRUE; // Request anistrophy
	
		VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		deviceCreateInfo.queueCreateInfoCount = (U32)indices.size();
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		deviceCreateInfo.enabledExtensionCount = 1;
		deviceCreateInfo.pNext = nullptr;
		const char* requiredExtensions[1] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};
		deviceCreateInfo.ppEnabledExtensionNames = requiredExtensions;

		// TODO: disable on realease builds
		deviceCreateInfo.enabledLayerCount = (U32)requiredValidationLayers.size();
		deviceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();
	
		// Create the device
		VK_CHECK(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device));
	
		// Save off the queue family indices
		_graphicsFamilyQueueIndex = graphicsQueueIndex;
		_presentationFamilyQueueIndex = presentationQueueIndex;

		// Create the queues
		vkGetDeviceQueue(_device, _graphicsFamilyQueueIndex, 0, &_graphicsQueue);
		vkGetDeviceQueue(_device, _presentationFamilyQueueIndex, 0, &_presentationQueue);
	}

	void VulkanRenderer::createShader(const char* name) {
		
		// Vertex shader
		U64 vertShaderSize;
		char* vertexShaderSource = readShaderFile(name, "vert", &vertShaderSize);
		VkShaderModuleCreateInfo vertexShaderCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		vertexShaderCreateInfo.codeSize = vertShaderSize;
		vertexShaderCreateInfo.pCode = (U32*)vertexShaderSource;
		VkShaderModule vertexShaderModule;
		VK_CHECK(vkCreateShaderModule(_device, &vertexShaderCreateInfo, nullptr, &vertexShaderModule));

		// Fragment shader
		U64 fragShaderSize;
		char* fragShaderSource = readShaderFile(name, "frag", &fragShaderSize);
		VkShaderModuleCreateInfo fragShaderCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		fragShaderCreateInfo.codeSize = fragShaderSize;
		fragShaderCreateInfo.pCode = (U32*)fragShaderSource;
		VkShaderModule fragmentShaderModule;
		VK_CHECK(vkCreateShaderModule(_device, &fragShaderCreateInfo, nullptr, &fragmentShaderModule));

		// Vertex shader stage
		VkPipelineShaderStageCreateInfo vertShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertexShaderModule;
		vertShaderStageInfo.pName = "main";

		// Vertex shader stage
		VkPipelineShaderStageCreateInfo fragShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = fragmentShaderModule;
		vertShaderStageInfo.pName = "main";

		_shaderStageCount = 2;
		_shaderStages.push_back(vertShaderStageInfo);
		_shaderStages.push_back(fragShaderStageInfo);

		delete vertexShaderSource;
		delete fragShaderSource;
	}

	char* VulkanRenderer::readShaderFile(const char* filename, const char* shaderType, U64* fileSize) {
		char buffer[256];
		I32 length = snprintf(buffer, 256, "shaders/%s.%s.spv", filename, shaderType);
		if (length < 0) {
			Logger::Fatal("Shader filename is too long.");
		}
		buffer[length] = '\0';

		std::ifstream file(buffer, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			Logger::Fatal("Shader unable to open file.");
		}

		*fileSize = (U64)file.tellg();
		char* fileBuffer = (char*)malloc(*fileSize);
		file.seekg(0);
		file.read(fileBuffer, *fileSize);
		file.close();

		return fileBuffer;
	}
}