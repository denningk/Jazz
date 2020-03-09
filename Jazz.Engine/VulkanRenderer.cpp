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

		VkInstanceCreateInfo instanceCreateInfo;
	}

	VulkanRenderer::~VulkanRenderer() {

	}
}