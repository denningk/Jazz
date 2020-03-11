#pragma once

#include "Types.h"

namespace Jazz {
	
	class Platform;
	class VulkanRenderer;

	class Engine {
	public:
		Engine(const char* applicationName);
		~Engine();
	
		void Run();

		void OnLoop(const F32 deltaTime);

	private:
		Platform* _platform;
		VulkanRenderer* _renderer;
	};
}