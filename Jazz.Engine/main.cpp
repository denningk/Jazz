#include "Types.h"
#include "Defines.h"
#include "Engine.h"
#include "Logger.h"

int main(int argc, const char** argv) {
	Jazz::Engine* engine = new Jazz::Engine("Jazz");
	engine->Run();
	delete engine;
	return 0;
}