#include "application.hpp"
#include "vklib.h"

int main(int argc, const char** argv)
{
	vklib::initialize_glfw();

	std::string model_path, img_path;

	if (argc == 3)
	{
		model_path = std::string(argv[1]);
		img_path   = std::string(argv[2]);
	}
	else
	{
		std::string model, img;
		std::cout << "model: ";
		std::getline(std::cin, model);
		std::cout << "texture: ";
		std::getline(std::cin, img);

		if (model.empty() || img.empty())
		{
			model_path = "model/model.bin";
			img_path   = "model/texture.png";
		}
		else
		{
			model_path = model;
			img_path   = img;
		}
	}

	try
	{
		Obj_app app;
		app.init(model_path, img_path);
		app.loop();
		vklib::terminate_glfw();
	}
	catch (std::runtime_error e)
	{
		std::cerr << "ERR: " << e.what() << std::endl;
		return 0;
	}

	return 0;
}