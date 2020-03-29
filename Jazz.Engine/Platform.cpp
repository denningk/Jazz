#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "Logger.h"
#include "Engine.h"
#include "VulkanUtils.h"
#include "VulkanRenderer.h"
#include "Platform.h"

namespace Jazz {
	Platform::Platform(Engine* engine, const char* applicationName) {
		Logger::Trace("Initializing platform layer...");
		_engine = engine;

		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		_window = glfwCreateWindow(1280, 720, applicationName, nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
	}

	Platform::~Platform() {
		if (_window) {
			glfwDestroyWindow(_window);
		}

		glfwTerminate();
	}

	Extent2D Platform::GetFrameBufferExtent() {
		Extent2D extent;
		glfwGetFramebufferSize(_window, &extent.Width, &extent.Height);
		return extent;
	}


	void Platform::GetRequiredExtensions(U32* extensionCount, const char*** extensionNames) {
		*extensionNames = glfwGetRequiredInstanceExtensions(extensionCount);
	}

	void Platform::CreateSurface(VkInstance instance, VkSurfaceKHR* surface) {
		VK_CHECK(glfwCreateWindowSurface(instance, _window, nullptr, surface));
	}

	const bool Platform::StartGameLoop() {
		while (!glfwWindowShouldClose(_window)) {
			glfwPollEvents();

			_engine->OnLoop(0);
		}

		_engine->DeviceWaitIdle();

		return true;
	}
}