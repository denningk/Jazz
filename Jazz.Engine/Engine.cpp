#include "Engine.h"
#include "Platform.h"
#include "Logger.h"

namespace Jazz {

	Engine::Engine(const char* applicationName) {
		Jazz::Logger::Log("Initializing Jazz Engine: %d", 4);
		_platform = new Platform(this, applicationName);
	}

	Engine::~Engine() {

	}

	void Engine::Run() {
		_platform->StartGameLoop();
	}

	void Engine::OnLoop(const F32 deltaTime) {

	}
}