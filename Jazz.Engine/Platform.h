#pragma once

#include "Types.h"

struct GLFWwindow;

namespace Jazz {

	class Engine;

	class Platform {
	public:
		Platform(Engine* engine, const char* applicationName);
		~Platform();

		GLFWwindow* GetWindow() { return _window; }

		void GetRequiredExtensions(U32* extensionCount, const char*** extensionNames);

		const bool StartGameLoop();

	private:
		Engine* _engine;
		GLFWwindow* _window;
	};
}