#include <vector>
#include <fstream>

#include "Platform.h"
#include "Logger.h"
#include "Defines.h"
#include "TMath.h"
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

		createSwapchain();
		createSwapchainImagesAndViews();

		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
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
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragmentShaderModule;
		fragShaderStageInfo.pName = "main";

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

	void VulkanRenderer::createSwapchain() {
		VulkanSwapchainSupportDetails swapchainSupport = querySwapchainSupport(_physicalDevice);
		VkSurfaceCapabilitiesKHR capabilities = swapchainSupport.Capabilities;

		// Choose a swap surface format
		bool found = false;
		for (auto format : swapchainSupport.Formats) {

			// Preferred formats
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				_swapchainImageFormat = format;
				found = true;
				break;
			}
		}

		if (!found) {
			_swapchainImageFormat = swapchainSupport.Formats[0];
		}

		// Choose a present mode
		VkPresentModeKHR presentMode;
		found = false;
		for (auto mode : swapchainSupport.PresentationModes) {
		
			// If preferred mode is available
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				presentMode = mode;
				found = true;
				break;
			}
		}

		if (!found) {
			presentMode = VK_PRESENT_MODE_FIFO_KHR;
		}

		// Swapchain extent
		if (capabilities.currentExtent.width != U32_MAX) {
			_swapchainExtent = capabilities.currentExtent;
		} else {
			Extent2D extent = _platform->GetFrameBufferExtent();
			_swapchainExtent = { (U32)extent.Width, (U32)extent.Height };

			// Clamp to a value allowed by the GPU
			_swapchainExtent.width = TMath::ClampU32(_swapchainExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			_swapchainExtent.height = TMath::ClampU32(_swapchainExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		U32 imageCount = capabilities.minImageCount + 1;

		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
			imageCount = capabilities.maxImageCount;
		}

		// Swapchain create info
		VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		swapchainCreateInfo.surface = _surface;
		swapchainCreateInfo.minImageCount = imageCount;
		swapchainCreateInfo.imageFormat = _swapchainImageFormat.format;
		swapchainCreateInfo.imageColorSpace = _swapchainImageFormat.colorSpace;
		swapchainCreateInfo.imageExtent = _swapchainExtent;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// Setup the queue family indices
		if (_graphicsFamilyQueueIndex != _presentationFamilyQueueIndex) {
			U32 queueFamilyIndices[] = { (U32)_graphicsFamilyQueueIndex, (U32)_presentationFamilyQueueIndex };
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = 2;
			swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchainCreateInfo.queueFamilyIndexCount = 0;
			swapchainCreateInfo.pQueueFamilyIndices = nullptr;
		}

		swapchainCreateInfo.preTransform = capabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = presentMode;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = nullptr;

		VK_CHECK(vkCreateSwapchainKHR(_device, &swapchainCreateInfo, nullptr, &_swapchain));
	}

	void VulkanRenderer::createSwapchainImagesAndViews() {

		// Images
		U32 swapchainImageCount = 0;
		vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, nullptr);
		_swapchainImages.resize(swapchainImageCount);
		_swapchainImageViews.resize(swapchainImageCount);
		vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, _swapchainImages.data());

		for (U32 i = 0; i < swapchainImageCount; ++i) {
			VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			viewInfo.image = _swapchainImages[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = _swapchainImageFormat.format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &_swapchainImageViews[i]));
		}

	}

	void VulkanRenderer::createRenderPass() {

		// Color attachment
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = _swapchainImageFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Color attachment reference
		VkAttachmentReference colorAttachmentReference = {};
		colorAttachmentReference.attachment = 0;
		colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Depth attachment

		// Find depth format
		const U64 candidateCount = 3;
		VkFormat candidates[candidateCount] = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT
		};
		
		U32 flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkFormat depthFormat;
		bool depthFormatFound = false;
		for (U64 i = 0; i < candidateCount; ++i) {
			VkFormatProperties properties;
			vkGetPhysicalDeviceFormatProperties(_physicalDevice, candidates[i], &properties);

			if ((properties.linearTilingFeatures && flags) == flags) {
				depthFormat = candidates[i];
				depthFormatFound = true;
				break;
			} else if ((properties.optimalTilingFeatures & flags) == flags) {
				depthFormat = candidates[i];
				depthFormatFound = true;
				break;
			}
		}

		if (!depthFormatFound) {
			Logger::Fatal("Unable to find a supported depth format");
		}

		// Depth attachment
		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Color attachment reference
		VkAttachmentReference depthAttachmentReference = {};
		depthAttachmentReference.attachment = 1;
		depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Subpass
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentReference;
		subpass.pDepthStencilAttachment = &depthAttachmentReference;

		// Render pass dependencies
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		const U32 attachmentCount = 2;
		VkAttachmentDescription attachments[attachmentCount] = {
			colorAttachment,
			depthAttachment
		};

		// Render pass create
		VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		renderPassCreateInfo.attachmentCount = attachmentCount;
		renderPassCreateInfo.pAttachments = attachments;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;
		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies = &dependency;
		VK_CHECK(vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &_renderPass));
	}

	void VulkanRenderer::createGraphicsPipeline() {

		// Viewport
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = (F32)_swapchainExtent.height;
		viewport.width = (F32)_swapchainExtent.width;
		viewport.height = -(F32)_swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Scissor
		VkRect2D scissor = {};
		scissor.offset = { 0,0 };
		scissor.extent = { _swapchainExtent.width, _swapchainExtent.height };

		// Viewport state
		VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizerCreateInfo.depthClampEnable = VK_FALSE;
		rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizerCreateInfo.lineWidth = 1.0f;
		rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
		rasterizerCreateInfo.depthBiasConstantFactor = 0.0f;
		rasterizerCreateInfo.depthBiasClamp = 0.0f;
		rasterizerCreateInfo.depthBiasSlopeFactor = 0.0f;

		// Multisampling
		VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
		multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisamplingCreateInfo.minSampleShading = 1.0f;
		multisamplingCreateInfo.pSampleMask = nullptr;
		multisamplingCreateInfo.alphaToCoverageEnable = VK_FALSE;
		multisamplingCreateInfo.alphaToOneEnable = VK_FALSE;

		// Depth and stencil testing
		VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
		colorBlendAttachmentState.blendEnable = VK_FALSE;
		colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
		colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendStateCreateInfo.attachmentCount = 1;
		colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
		colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
		colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
		colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
		colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

		// Dynamic state
		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicStateCreateInfo.dynamicStateCount = 2;
		dynamicStateCreateInfo.pDynamicStates = dynamicStates;

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;

		// Input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// Pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineLayoutCreateInfo.setLayoutCount = 0;
		pipelineLayoutCreateInfo.pSetLayouts = nullptr;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
		VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout));

		// Pipeline create
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineCreateInfo.stageCount = _shaderStageCount;
		pipelineCreateInfo.pStages = _shaderStages.data();
		pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &depthStencil;
		pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
		pipelineCreateInfo.pDynamicState = nullptr;

		pipelineCreateInfo.layout = _pipelineLayout;
		pipelineCreateInfo.renderPass = _renderPass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		VK_CHECK(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_pipeline));

		Logger::Log("Graphics pipeline created!");
	}

	void VulkanRenderer::createFramebuffers() {
		_swapChainFramebuffers.resize(_swapchainImageViews.size());

		for (U64 i = 0; i < _swapchainImageViews.size(); i++) {
			VkImageView attachments[] = {
				_swapchainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = _renderPass;
			framebufferCreateInfo.attachmentCount = 2;
			framebufferCreateInfo.pAttachments = attachments;
			framebufferCreateInfo.width = _swapchainExtent.width;
			framebufferCreateInfo.height = _swapchainExtent.height;
			framebufferCreateInfo.layers = 1;

			VK_CHECK(vkCreateFramebuffer(_device, &framebufferCreateInfo, nullptr, &_swapChainFramebuffers[i]));
		}
	}
}