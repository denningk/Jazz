#include "Engine.h"
#include "Platform.h"
#include "VulkanRenderer.h"
#include "Logger.h"

namespace Jazz {

	Engine::Engine(const char* applicationName) {
		Jazz::Logger::Log("Initializing Jazz Engine: %d", 4);
		_platform = new Platform(this, applicationName);
		_renderer = new VulkanRenderer(_platform);
	}

	Engine::~Engine() {
		delete _renderer;
		delete _platform;
	}

	void Engine::Run() {
		_platform->StartGameLoop();
	}

	void Engine::OnLoop(const F32 deltaTime) {
		_renderer->drawFrame();
	}

	void Engine::DeviceWaitIdle() {
		_renderer->deviceWaitIdle();
	}
}