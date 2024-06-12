#include "app.hpp"

using namespace vklib;

int main()
{
	// system("cls");

	check_success(initialize_glfw(), "Can't initialize GLFW!\n");

	Application app;
	tools::print("App Initializing");
	app.init();
	tools::print("App Running");
	app.run();
	tools::print("App Exiting");
	terminate_glfw();

	return EXIT_SUCCESS;
}
