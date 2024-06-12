#include "application.hpp"
#include "stb_image.h"

void Application::init()
{
	env.create();
	swapchain.create(env);
	render_core.deferred_rt.create(env, swapchain);
	render_core.deferred_pipeline.create(env, swapchain, render_core.deferred_rt);

	render_core.composite_rt.create(env, swapchain);
	render_core.composite_pipeline.create(env, swapchain, render_core.composite_rt);
}

int main()
{
	try
	{
		initialize_glfw();
		Application app;
		app.init();
		terminate_glfw();
	}
	catch (std::exception e)
	{
		std::cerr << "Quitting..." << std::endl;
	}

	return 0;
}