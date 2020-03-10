#include <vector>

#include "Platform.h"
#include "Logger.h"
#include "Defines.h"
#include "VulkanRenderer.h"

namespace Jazz {
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
		std::vector<const char*> requireValidationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		
	}

	VulkanRenderer::~VulkanRenderer() {

	}
}