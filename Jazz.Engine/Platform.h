#pragma once

struct GLFWwindow;

namespace Jazz {

	class Platform {
	public:
		Platform();
		~Platform();

		GLFWwindow* GetWindow() { return _window; }

		const bool StartGameLoop();

	private:
		GLFWwindow* _window;
	};
}