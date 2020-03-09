#pragma once

struct GLFWwindow;

namespace Jazz {

	class Engine;

	class Platform {
	public:
		Platform(Engine* engine, const char* applicationName);
		~Platform();

		GLFWwindow* GetWindow() { return _window; }

		const bool StartGameLoop();

	private:
		Engine* _engine;
		GLFWwindow* _window;
	};
}